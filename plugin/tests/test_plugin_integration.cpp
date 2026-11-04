// AHR WWV/WWVH Time Decoder - plugin integration tests.
//
// Built inside the SDRangel tree (-DAHRWWV_BUILD_PLUGIN_TESTS=ON) and linked
// against SDRangel's own sdrbase, so these exercise the real plugin classes:
// AHRWWVBaseband in its own QThread, SDRangel's SampleSinkFifo, DownChannelizer,
// NCO and Interpolator, AHRWWVSink, the core decoder, the report messages, and
// the plugin library itself through QPluginLoader. No radio hardware needed:
// samples come from the WWV/WWVH simulator.
//
// Usage: ahrwwv_plugin_tests [path to the built plugin library] [--long]

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>

#ifdef __linux__
#include <sys/resource.h>
#endif

#include <QCoreApplication>
#include <QLibrary>
#include <QPluginLoader>
#include <QThread>
#include <QElapsedTimer>

#include "dsp/dsptypes.h"
#include "dsp/dspcommands.h"
#include "plugin/plugininterface.h"
#include "util/messagequeue.h"

#include "ahrwwv.h"
#include "ahrwwvbaseband.h"
#include "ahrwwvsettings.h"
#include "wwvsim.h"

using namespace ahrwwv;

static const double kPi = 3.14159265358979323846;

static int g_checks = 0, g_fail = 0;
#define CHECK(cond, ...) do { g_checks++; if (!(cond)) { g_fail++; std::printf("  FAIL line %d: %s  ", __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static const double kStart = 1793577600.0 + 15 * 3600 + 17 * 60 + 23.4; // 2026-11-02 15:17:23.4 UTC
static const double kPathDelay = 0.0134;

// --------------------------------------------------------------------------
// Harness: one baseband worker in its own thread, fed from the simulator.
struct Harness
{
    AHRWWVBaseband *bb = nullptr;
    QThread thread;
    MessageQueue toChannel;     // where the sink posts AHRWWV::MsgReport
    WwvSim *sim = nullptr;
    SimConfig sc;
    double deviceRate = 48000;
    double carrierOffset = 5000; // Hz, where the simulated carrier sits in the device band
    double ncoPhase = 0;
    uint64_t devSamples = 0;     // device samples fed
    double simTimeFed = 0;       // seconds of signal fed
    double start0 = 0;           // true UTC of the very first sample
    double base = 0;             // signal time at which the decoder last started from scratch

    // collected
    std::vector<AHRWWVReport> reports;
    int wrongSeconds = 0, labelledSeconds = 0, accepted = 0, rejected = 0, resets = 0;
    bool sawVerified = false, sawHoldover = false;
    double firstVerifiedAt = -1;
    double maxUtcErr = 0;
    std::function<void(const AHRWWVReport&)> onReport;

    Harness(double rate, double offset, const SimConfig& cfg) : sc(cfg), deviceRate(rate), carrierOffset(offset)
    {
        sc.sampleRate = rate;
        start0 = sc.startUnix;
        sim = new WwvSim(sc);
        bb = new AHRWWVBaseband();
        bb->setMessageQueueToChannel(&toChannel);
        bb->moveToThread(&thread);
        bb->reset();
        bb->startWork();
        thread.start();
        notify(rate, 10000000);
        AHRWWVSettings s;
        s.m_inputFrequencyOffset = (qint32) offset;
        configure(QStringList(), s, true);
    }

    ~Harness()
    {
        bb->stopWork();
        thread.quit();
        thread.wait();
        delete bb;
        delete sim;
    }

    void notify(double rate, qint64 centre)
    {
        bb->getInputMessageQueue()->push(new DSPSignalNotification((int) rate, centre));
    }

    void configure(const QStringList& keys, const AHRWWVSettings& s, bool force = false)
    {
        bb->getInputMessageQueue()->push(AHRWWVBaseband::MsgConfigureAHRWWVBaseband::create(keys, s, force));
        mark();
    }

    // The FIFO is idle when a reset-causing message is sent, so the decoder's
    // sample 0 is the next sample fed.
    void mark() { waitIdle(); base = simTimeFed; }

    double truthAt(quint64 samplesProcessed) const
    {
        return start0 + base + (samplesProcessed - 1) / 12000.0 - kPathDelay;
    }

    void waitIdle()
    {
        for (int i = 0; i < 20000 && (bb->getFifoFill() > 0 || bb->getInputMessageQueue()->size() > 0); i++) {
            QThread::usleep(100);
        }
        drain();
    }

    void drain()
    {
        Message *m;
        while ((m = toChannel.pop()) != nullptr)
        {
            if (AHRWWV::MsgReport::match(*m))
            {
                const AHRWWVReport& r = ((AHRWWV::MsgReport*) m)->getReport();
                reports.push_back(r);
                if (r.resetHappened) resets++;
                for (auto& f : r.frames) { if (f.accepted) accepted++; else rejected++; }
                for (auto& s : r.newSeconds) (void) s;
                if (r.status.state == DecoderState::Verified && !sawVerified) { sawVerified = true; firstVerifiedAt = simTimeFed; }
                if (r.status.state == DecoderState::Holdover) sawHoldover = true;
                if (r.haveUtc && r.status.haveTime)
                {
                    // utcAtRef refers to the last 12 kHz sample handed to the decoder; compare with the
                    // simulator truth for the same moment (transmitter time = arrival - path delay).
                    double truth = truthAt(r.samplesProcessed);
                    double err = std::fabs(r.utcAtRef - truth);
                    // allow the channelizer/interpolator group delay (a few ms) plus decoder accuracy
                    if (r.status.state == DecoderState::Verified || r.status.state == DecoderState::Decoded) {
                        labelledSeconds++;
                        if (err > 0.5) wrongSeconds++;
                        if (r.status.state == DecoderState::Verified) maxUtcErr = std::max(maxUtcErr, err);
                    }
                }
                if (onReport) onReport(r);
            }
            delete m;
        }
    }

    // Feed `seconds` of simulated signal in 50 ms chunks. realtime: pace the
    // feed to the wall clock like a real receiver. skip: generate but do not
    // feed (samples lost before the plugin).
    QElapsedTimer wall;
    double wallBase = -1;

    void run(double seconds, bool silence = false, bool realtime = false, bool skip = false)
    {
        if (realtime && wallBase < 0) { wall.start(); wallBase = simTimeFed; }
        const size_t chunk = (size_t) (deviceRate / 20);
        std::vector<std::complex<float>> buf;
        SampleVector sv(chunk);
        size_t chunks = (size_t) std::llround(seconds * 20);
        double dphi = 2 * kPi * carrierOffset / deviceRate;

        for (size_t k = 0; k < chunks; k++)
        {
            sim->generate(buf, chunk);

            for (size_t i = 0; i < chunk; i++)
            {
                std::complex<double> z(buf[i].real(), buf[i].imag());
                if (silence) z = 0;
                z *= std::polar(1.0, ncoPhase);
                ncoPhase += dphi;
                if (ncoPhase > 2 * kPi) ncoPhase -= 2 * kPi;
                double sc = 0.2 * SDR_RX_SCALEF;
                sv[i].setReal((FixReal) std::lround(z.real() * sc));
                sv[i].setImag((FixReal) std::lround(z.imag() * sc));
            }

            if (!skip) {
                bb->feed(sv.begin(), sv.end());
                devSamples += chunk;
            }
            simTimeFed += chunk / deviceRate;

            if (realtime) {
                while (wall.elapsed() / 1000.0 < simTimeFed - wallBase) QThread::usleep(500);
            }

            waitIdle();
        }
    }

    // Change the device sample rate mid-stream: the simulator must follow.
    void changeRate(double rate)
    {
        double t = sim->trueUtc(sim->samplesGenerated());
        delete sim;
        sc.sampleRate = rate;
        sc.startUnix = t;
        sim = new WwvSim(sc);
        deviceRate = rate;
        notify(rate, 10000000);
        mark();
    }

    // Keep feeding until a new report arrives (reports are paced by the wall clock).
    void runUntilReport(double maxSeconds = 60)
    {
        size_t n = reports.size();
        for (double t = 0; t < maxSeconds && reports.size() == n; t += 0.5) {
            run(0.5);
            QThread::msleep(30);
            drain();
        }
    }

    const AHRWWVReport& last() const { static AHRWWVReport empty; return reports.empty() ? empty : reports.back(); }
};

static SimConfig baseSim(double start = kStart)
{
    SimConfig sc;
    sc.startUnix = start;
    sc.snrDb = 25;
    sc.st[0].delay = kPathDelay;
    return sc;
}

static void summary(const char *name, const Harness& h)
{
    const auto& r = h.last();
    std::printf("  %-44s state=%-11s verified@%5.0fs frames %d/%d resets %d wrong %d/%d maxerr %.1f ms\n", name,
                decoderStateName(r.status.state), h.firstVerifiedAt, h.accepted, h.rejected, h.resets,
                h.wrongSeconds, h.labelledSeconds, h.maxUtcErr * 1e3);
}

// --------------------------------------------------------------------------
static void testSettingsSerialization()
{
    std::printf("[settings] serialize / deserialize round trip and corrupt data\n");
    AHRWWVSettings a;
    a.m_inputFrequencyOffset = -12345;
    a.m_decoderEnabled = false;
    a.m_station = AHRWWVSettings::StationWWVH;
    a.m_showDiagnostics = true;
    a.m_latencyCorrectionMs = 37.5f;
    a.m_rxLatitude = 61.6f;
    a.m_rxLongitude = -149.1f;
    a.m_useStationLocation = false;
    a.m_title = "WWV 10 MHz";
    a.m_workspaceIndex = 2;
    AHRWWVSettings b;
    CHECK(b.deserialize(a.serialize()), "deserialize failed");
    CHECK(b.m_inputFrequencyOffset == -12345 && !b.m_decoderEnabled && b.m_station == AHRWWVSettings::StationWWVH
          && b.m_showDiagnostics && b.m_latencyCorrectionMs == 37.5f && b.m_rxLatitude == 61.6f && b.m_rxLongitude == -149.1f
          && !b.m_useStationLocation && b.m_title == "WWV 10 MHz" && b.m_workspaceIndex == 2, "round trip mismatch");
    AHRWWVSettings c;
    c.m_inputFrequencyOffset = 99;
    CHECK(!c.deserialize(QByteArray("garbage")) && c.m_inputFrequencyOffset == 0 && c.m_decoderEnabled, "corrupt data not reset to defaults");
}

static void testDecodeThroughChannel()
{
    std::printf("[baseband] live-style decode through SDRangel channelizer (48 kS/s, carrier +5 kHz)\n");
    Harness h(48000, 5000, baseSim());
    h.run(200);
    summary("48 kS/s, offset +5000 Hz", h);
    CHECK(h.sawVerified, "never verified");
    CHECK(h.wrongSeconds == 0, "%d wrong", h.wrongSeconds);
    CHECK(h.maxUtcErr < 0.010, "max error %.4f s", h.maxUtcErr);
    CHECK(h.last().status.station == StationId::WWV, "station %s", stationName(h.last().status.station));
    size_t env = 0;
    for (auto& r : h.reports) env += r.envelope.size();
    // Reports are paced by wall time and the GUI caps its envelope buffer,
    // so the sample count depends on test host load.
    CHECK(env > 100 * 100 && env <= 100 * 201, "envelope samples %zu for 200 s", env);
    // Reports are paced by the PC clock (4 per second) for the GUI; this test
    // runs ~20x faster than real time, so fewer arrive.
    CHECK(h.reports.size() >= 10, "only %zu reports", h.reports.size());
}

static void testOtherRatesAndNegativeOffset()
{
    std::printf("[baseband] other device rates and a negative offset\n");
    struct { double rate, off; } cases[] = {{2000000, -250000}, {192000, -20000}, {62500, 0}};
    for (auto c : cases)
    {
        Harness h(c.rate, c.off, baseSim(kStart + 31));
        h.run(c.rate > 1e6 ? 190 : 200);
        char nm[80]; std::snprintf(nm, sizeof nm, "%.0f S/s, offset %+.0f Hz", c.rate, c.off);
        summary(nm, h);
        CHECK(h.sawVerified && h.wrongSeconds == 0, "rate %.0f", c.rate);
    }
}

static void testRateChange()
{
    std::printf("[baseband] device sample-rate change mid-stream\n");
    Harness h(48000, 5000, baseSim());
    h.run(170);
    bool verifiedBefore = h.sawVerified;
    int resetsBefore = h.resets;
    h.changeRate(96000);
    h.sawVerified = false;
    h.run(200);
    summary("48k -> 96k after 170 s", h);
    CHECK(verifiedBefore, "not verified before the change");
    CHECK(h.resets > resetsBefore, "decoder not reset on rate change");
    CHECK(h.sawVerified && h.wrongSeconds == 0, "did not re-verify cleanly");
}

static void testOffsetChange()
{
    std::printf("[baseband] retune the channel off the carrier and back\n");
    Harness h(48000, 5000, baseSim());
    h.run(170);
    bool verified = h.sawVerified;
    AHRWWVSettings s;
    s.m_inputFrequencyOffset = 9000; // nothing there
    int resetsBefore = h.resets;
    h.configure(QStringList({"inputFrequencyOffset"}), s);
    h.run(60);
    DecoderState away = h.last().status.state;
    bool timeAway = h.last().status.haveTime;
    s.m_inputFrequencyOffset = 5000;
    h.configure(QStringList({"inputFrequencyOffset"}), s);
    h.sawVerified = false;
    h.run(200);
    summary("5000 -> 9000 -> 5000 Hz", h);
    CHECK(verified, "not verified first");
    CHECK(h.resets >= resetsBefore + 2, "offset changes did not reset the decoder (%d)", h.resets - resetsBefore);
    CHECK(!timeAway && away == DecoderState::Searching, "off-carrier state %s, time %d", decoderStateName(away), timeAway);
    CHECK(h.sawVerified && h.wrongSeconds == 0, "did not recover");
}

static void testEnableDisableReset()
{
    std::printf("[baseband] decoder disable / enable / reset message\n");
    Harness h(48000, 5000, baseSim());
    h.run(170);
    CHECK(h.sawVerified, "not verified");
    AHRWWVSettings s;
    s.m_inputFrequencyOffset = 5000;
    s.m_decoderEnabled = false;
    h.configure(QStringList({"decoderEnabled"}), s);
    size_t n0 = h.reports.size();
    h.run(10);
    CHECK(h.reports.size() == n0, "reports kept coming while disabled (%zu)", h.reports.size() - n0);
    s.m_decoderEnabled = true;
    int r0 = h.resets;
    h.configure(QStringList({"decoderEnabled"}), s);
    h.runUntilReport();
    CHECK(h.resets > r0, "no reset after re-enable");
    CHECK(!h.last().status.haveTime, "time survived disable/enable");
    h.sawVerified = false;
    h.run(170);
    CHECK(h.sawVerified, "not verified after re-enable");
    r0 = h.resets;
    h.bb->getInputMessageQueue()->push(AHRWWVBaseband::MsgResetDecoder::create());
    h.mark();
    h.runUntilReport();
    CHECK(h.resets > r0 && !h.last().status.haveTime && h.last().framesAccepted == 0, "reset message ignored");
    summary("disable/enable/reset", h);
    CHECK(h.wrongSeconds == 0, "wrong %d", h.wrongSeconds);
}

static void testSignalLoss()
{
    std::printf("[baseband] signal loss for 3 minutes and reacquisition\n");
    SimConfig sc = baseSim();
    sc.dropouts.push_back({200.0, 380.0});
    Harness h(48000, 5000, sc);
    double worstHoldover = 0;
    h.onReport = [&](const AHRWWVReport& r) {
        if (r.status.state == DecoderState::Holdover && r.haveUtc) {
            double truth = h.truthAt(r.samplesProcessed);
            worstHoldover = std::max(worstHoldover, std::fabs(r.utcAtRef - truth));
        }
    };
    h.run(560);
    summary("dropout 200-380 s", h);
    std::printf("    worst holdover error %.1f ms\n", worstHoldover * 1e3);
    CHECK(h.sawHoldover, "no holdover");
    CHECK(worstHoldover < 0.01, "holdover drifted %.4f", worstHoldover);
    CHECK(h.last().status.state == DecoderState::Verified, "not verified at end (%s)", decoderStateName(h.last().status.state));
    CHECK(h.wrongSeconds == 0, "wrong %d", h.wrongSeconds);
}

static void testIncompleteFrames()
{
    std::printf("[baseband] incomplete frames: 50 s of signal, then silence\n");
    Harness h(48000, 5000, baseSim(kStart + 20)); // starts at :43, so no complete minute within 50 s
    h.run(50);
    h.run(30, true);
    summary("50 s signal + 30 s silence", h);
    CHECK(h.accepted == 0 && !h.last().status.haveTime, "time from an incomplete frame");
    CHECK(h.last().status.state == DecoderState::Searching || h.last().status.state == DecoderState::SecondLock
          || h.last().status.state == DecoderState::FrameLock, "state %s", decoderStateName(h.last().status.state));
}

static void testSampleLossDetection()
{
    std::printf("[baseband] real-time feed with 1.5 s of samples lost (FIFO overflow / USB drop)\n");
    Harness h(48000, 5000, baseSim());
    h.run(22, false, true);
    quint32 before = h.last().discontinuities;
    int resetsBefore = h.resets;
    h.run(1.5, false, true, true);   // lost: never reaches the plugin
    double lossAt = h.simTimeFed;
    h.run(18, false, true);
    h.runUntilReport(2);
    std::printf("    restarts caused by sample loss: before %u, after %u (resets %d -> %d)\n",
                before, h.last().discontinuities, resetsBefore, h.resets);
    CHECK(before == 0, "false detection in steady real-time feed");
    CHECK(h.last().discontinuities >= 1 && h.resets > resetsBefore, "1.5 s sample loss not detected");
    (void) lossAt;
}

static long rssKb() // peak resident memory; 0 where not available (memory check then skipped)
{
#ifdef __linux__
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_maxrss;
#else
    return 0;
#endif
}

static void testLongRun(double minutes)
{
    std::printf("[baseband] long run: %.0f minutes at 24 kS/s, memory check\n", minutes);
    SimConfig sc = baseSim();
    sc.snrDb = 12;
    sc.st[0].fadeDepth = 0.5;
    Harness h(24000, -3000, sc);
    h.run(300);
    long rss5 = rssKb();
    h.run(minutes * 60 - 300);
    long rssEnd = rssKb();
    summary("long run", h);
    std::printf("    max RSS after 5 min %ld kB, at end %ld kB\n", rss5, rssEnd);
    CHECK(h.wrongSeconds == 0, "wrong %d", h.wrongSeconds);
    CHECK(rssEnd - rss5 < 8192, "memory grew %ld kB", rssEnd - rss5);
    CHECK(h.accepted >= (int) (minutes * 0.6), "only %d frames in %.0f min", h.accepted, minutes);
}

static void testPluginLibrary(const char *path)
{
    std::printf("[plugin] load the built plugin library with QPluginLoader (%s)\n", path);
#ifdef _WIN32
    // Isolate SDRangel's GUI/FFmpeg runtime loading from plugin code.
    // If this fails, the Windows deployment is broken before our plugin runs.
    {
        QLibrary sdrgui("sdrgui.dll");
        bool ok = sdrgui.load();
        std::printf("    SDRangel sdrgui.dll alone: %s%s\n", ok ? "loads" : "FAILS: ",
                    ok ? "" : qPrintable(sdrgui.errorString()));
        CHECK(ok, "SDRangel sdrgui.dll failed to load: %s", qPrintable(sdrgui.errorString()));
    }
#endif
    for (int i = 0; i < 20; i++)
    {
        QPluginLoader loader(QString::fromLocal8Bit(path));
        bool ok = loader.load();
        if (!ok) {
            CHECK(ok, "load failed: %s", qPrintable(loader.errorString()));
            return;
        }
        QJsonObject meta = loader.metaData();
        PluginInterface *pi = qobject_cast<PluginInterface*>(loader.instance());
        CHECK(pi != nullptr, "no PluginInterface instance");
        if (i == 0 && pi)
        {
            const PluginDescriptor& d = pi->getPluginDescriptor();
            std::printf("    IID %s | id %s | name '%s' | version %s\n", qPrintable(meta.value("IID").toString()),
                        qPrintable(d.hardwareId), qPrintable(d.displayedName), qPrintable(d.version));
            CHECK(meta.value("IID").toString() == "sdrangel.channel.ahrwwvtimedecoder", "IID");
            CHECK(d.hardwareId == "AHRWWVTimeDecoder" && d.version == "8.0.0", "descriptor");
            CHECK(pi->createChannelWebAPIAdapter() == nullptr, "web API adapter should be absent");
        }
        CHECK(loader.unload() || i > 0, "unload failed: %s", qPrintable(loader.errorString()));
    }
    std::printf("    loaded/unloaded 20 times\n");
}

int main(int argc, char **argv)
{
    // CI diagnostic: Windows may terminate an executable before buffered stdout is flushed.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    std::fprintf(stderr, "[ahrwwv] integration-test process started\n");
    QCoreApplication app(argc, argv);
    std::fprintf(stderr, "[ahrwwv] QCoreApplication initialized\n");
    const char *plugin = nullptr;
    bool longRun = false;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--long")) longRun = true;
        else plugin = argv[i];
    }
    QElapsedTimer t;
    t.start();

    testSettingsSerialization();
    if (plugin) testPluginLibrary(plugin);
    testDecodeThroughChannel();
    testOtherRatesAndNegativeOffset();
    testRateChange();
    testOffsetChange();
    testEnableDisableReset();
    testSignalLoss();
    testSampleLossDetection();
    testIncompleteFrames();
    testLongRun(longRun ? 60 : 15);

    std::printf("\n%d checks, %d failed (%.0f s)\n", g_checks, g_fail, t.elapsed() / 1000.0);
    return g_fail ? 1 : 0;
}
