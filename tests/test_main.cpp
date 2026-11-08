// AHR WWV/WWVH Time Decoder - unit and scenario tests (no framework needed).
#include "ahrwwv/decoder.h"
#include "ahrwwv/timecode.h"
#include "wwvsim.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <string>

using namespace ahrwwv;

static int g_fail = 0, g_checks = 0;
#define CHECK(cond, ...) do { g_checks++; if (!(cond)) { g_fail++; std::printf("  FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::string frameString(const Frame& f)
{
    std::string s;
    for (auto x : f) s += symbolChar(x);
    return s;
}

// ------------------------------------------------------------------ time code
static void testEncodeKnown()
{
    std::printf("[timecode] known frame 2026-11-02 (day 306) 15:17 UTC\n");
    TimeCode tc;
    tc.year = 2026; tc.dayOfYear = 306; tc.hour = 15; tc.minute = 17;
    tc.dut1Tenths = 0;
    Frame f = encodeFrame(tc);
    // Built by hand from NIST SP 250-67 bit assignments (LSB first):
    //  sec: 0         1         2         3         4         5
    //       0123456789012345678901234567890123456789012345678901234567890
    const char* expect =
        "_000"     // 0 ref, 1 unused, 2 DST2, 3 LSW
        "0110"     // 4-7  year units 6
        "0M"       // 8 unused, 9 P1
        "1110"     // 10-13 minute units 7
        "0"        // 14 unused
        "100"      // 15-17 minute tens 1
        "0M"       // 18 unused, 19 P2
        "1010"     // 20-23 hour units 5
        "0"        // 24 unused
        "10"       // 25-26 hour tens 1
        "00M"      // 27-28 unused, 29 P3
        "0110"     // 30-33 day units 6
        "0"        // 34 unused
        "0000M"    // 35-38 day tens 0, 39 P4
        "11"       // 40-41 day hundreds 3
        "0000000M" // 42-48 unused, 49 P5
        "1"        // 50 UT1 sign (+)
        "0100"     // 51-54 year tens 2
        "0"        // 55 DST1
        "000M";    // 56-58 DUT1 0, 59 P0
    CHECK(std::strlen(expect) == 60, "expect length %zu", std::strlen(expect));
    CHECK(frameString(f) == expect, "\n    got    %s\n    expect %s", frameString(f).c_str(), expect);
    DecodeResult r = decodeFrame(f);
    CHECK(r.valid && r.tc.year == 2026 && r.tc.dayOfYear == 306 && r.tc.hour == 15 && r.tc.minute == 17, "decode");
}

static void testRoundTrip()
{
    std::printf("[timecode] 20000 random encode/decode round trips\n");
    std::mt19937 rng(7);
    int bad = 0;
    for (int i = 0; i < 20000; i++) {
        TimeCode tc;
        tc.year = 2000 + int(rng() % 100);
        tc.dayOfYear = 1 + int(rng() % (isLeapYear(tc.year) ? 366 : 365));
        tc.hour = int(rng() % 24); tc.minute = int(rng() % 60);
        tc.dut1Tenths = int(rng() % 15) - 7;
        tc.leapSecondWarning = rng() & 1; tc.dst1 = rng() & 1; tc.dst2 = rng() & 1;
        Frame f = encodeFrame(tc);
        DecodeResult r = decodeFrame(f);
        SoftFrame sf;
        for (int p = 0; p < 60; p++)
            for (int h = 0; h < 4; h++) sf[p][h] = (h == int(f[p])) ? 0.0 : 1.0;
        DecodeResult rs = decodeFrameSoft(sf);
        auto same = [&](const TimeCode& a) {
            return a.year == tc.year && a.dayOfYear == tc.dayOfYear && a.hour == tc.hour && a.minute == tc.minute
                && a.dut1Tenths == tc.dut1Tenths && a.leapSecondWarning == tc.leapSecondWarning
                && a.dst1 == tc.dst1 && a.dst2 == tc.dst2;
        };
        if (!r.valid || !same(r.tc) || !rs.valid || !same(rs.tc)) bad++;
    }
    CHECK(bad == 0, "%d round trips failed", bad);
}

static void testRejects()
{
    std::printf("[timecode] corrupted frames are rejected\n");
    TimeCode tc; tc.year = 2026; tc.dayOfYear = 306; tc.hour = 15; tc.minute = 17;
    Frame good = encodeFrame(tc);
    struct Case { const char* name; std::function<void(Frame&)> mut; } cases[] = {
        {"missing marker 29", [](Frame& f) { f[29] = Symbol::One; }},
        {"pulse in second 0", [](Frame& f) { f[0] = Symbol::Zero; }},
        {"minute tens 6", [](Frame& f) { f[15] = Symbol::Zero; f[16] = Symbol::One; f[17] = Symbol::One; }},
        {"minute units 12", [](Frame& f) { f[10] = Symbol::Zero; f[11] = Symbol::Zero; f[12] = Symbol::One; f[13] = Symbol::One; }},
        {"hour 25", [](Frame& f) { f[20] = Symbol::One; f[21] = Symbol::Zero; f[22] = Symbol::One; f[23] = Symbol::Zero; f[25] = Symbol::Zero; f[26] = Symbol::One; }},
        {"day 0", [](Frame& f) { for (int p : {30, 31, 32, 33, 35, 36, 37, 38, 40, 41}) f[p] = Symbol::Zero; }},
        {"unreadable second", [](Frame& f) { f[22] = Symbol::Unknown; }},
        {"extra marker", [](Frame& f) { f[12] = Symbol::Marker; }},
    };
    for (auto& c : cases) {
        Frame f = good;
        c.mut(f);
        DecodeResult r = decodeFrame(f);
        CHECK(!r.valid, "%s accepted", c.name);
    }
    // NIST lists 42-48 as unused, but WWVH sends 1 at second 47 (live
    // 15 MHz, 2026-11-04): an unused bit must not reject the frame.
    {
        Frame f = good;
        f[47] = Symbol::One;
        CHECK(decodeFrame(f).valid, "WWVH-style frame with bit 47 set rejected");
    }
    // day 366 is only legal in leap years
    TimeCode t2 = tc; t2.year = 2026; t2.dayOfYear = 366;
    CHECK(!decodeFrame(encodeFrame(t2)).valid, "day 366 in 2026 accepted");
    t2.year = 2028;
    CHECK(decodeFrame(encodeFrame(t2)).valid, "day 366 in 2028 rejected");
}

static void testSoft()
{
    std::printf("[timecode] soft decoding with noise and erasures\n");
    TimeCode tc; tc.year = 2027; tc.dayOfYear = 45; tc.hour = 3; tc.minute = 59; tc.dut1Tenths = -2;
    Frame f = encodeFrame(tc);
    const double ex[4][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}};
    std::mt19937 rng(3);
    std::normal_distribution<double> n(0, 1);
    auto make = [&](double sigma, int erase) {
        SoftFrame sf;
        for (int p = 0; p < 60; p++) {
            double lv[3];
            for (int i = 0; i < 3; i++) lv[i] = ex[int(f[p])][i] + sigma * n(rng);
            if (p < erase && p % 10 != 9 && p) for (double& v : lv) v = 0.5; // erased: no information
            for (int h = 0; h < 4; h++) {
                double c = 0;
                for (int i = 0; i < 3; i++) c += (lv[i] - ex[h][i]) * (lv[i] - ex[h][i]);
                sf[p][h] = c;
            }
        }
        return sf;
    };
    int ok = 0, wrong = 0;
    for (int i = 0; i < 200; i++) {
        DecodeResult r = decodeFrameSoft(make(0.15, 0));
        if (r.valid) { ok++; if (r.tc.minute != 59 || r.tc.dayOfYear != 45 || r.tc.hour != 3 || r.tc.dut1Tenths != -2) wrong++; }
    }
    CHECK(ok >= 130 && wrong == 0, "sigma 0.15: %d/200 valid, %d wrong", ok, wrong);
    std::printf("  sigma 0.15: %d/200 decoded, %d wrong\n", ok, wrong);
    ok = 0; wrong = 0;
    for (int i = 0; i < 200; i++) {
        DecodeResult r = decodeFrameSoft(make(0.35, 0));
        if (r.valid) { ok++; if (r.tc.minute != 59 || r.tc.dayOfYear != 45 || r.tc.hour != 3 || r.tc.dut1Tenths != -2) wrong++; }
    }
    std::printf("  sigma 0.35: %d/200 decoded, %d wrong\n", ok, wrong);
    CHECK(wrong * 10 <= ok, "sigma 0.35 wrong %d of %d", wrong, ok);
    // erased year bits must be reported as too weak, not guessed
    DecodeResult r = decodeFrameSoft(make(0.05, 8));
    CHECK(!r.valid, "frame with erased year accepted (weakest %s %.2f)", r.weakestField.c_str(), r.minMargin);
    // pure noise must not decode
    int falseOk = 0;
    for (int i = 0; i < 500; i++) {
        SoftFrame sf;
        for (int p = 0; p < 60; p++) {
            double lv[3] = {0.5 + 0.5 * n(rng), 0.5 + 0.5 * n(rng), 0.5 + 0.5 * n(rng)};
            for (int h = 0; h < 4; h++) {
                double c = 0;
                for (int k = 0; k < 3; k++) c += (lv[k] - ex[h][k]) * (lv[k] - ex[h][k]);
                sf[p][h] = c;
            }
        }
        if (decodeFrameSoft(sf).valid) falseOk++;
    }
    CHECK(falseOk == 0, "%d/500 noise frames decoded", falseOk);
}

// Faded seconds are erasures, not wrong-kind pulses.
static void testFadedSecondsAreErasures()
{
    std::printf("[timecode] faded seconds are erasures, wrong-kind pulses are not\n");
    TimeCode tc; tc.year = 2026; tc.dayOfYear = 309; tc.hour = 0; tc.minute = 19; tc.dut1Tenths = -1;
    Frame f = encodeFrame(tc);
    const double ex[4][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}};
    auto costOf = [&](const double lv[3], std::array<double, 4>& c) {
        for (int h = 0; h < 4; h++) {
            c[h] = 0;
            for (int i = 0; i < 3; i++) c[h] += (lv[i] - ex[h][i]) * (lv[i] - ex[h][i]);
        }
    };
    const int faded[7] = {1, 8, 14, 18, 24, 28, 34};
    SoftFrame fade, wrong;
    for (int p = 0; p < 60; p++) {
        double lv[3] = {ex[int(f[p])][0], ex[int(f[p])][1], ex[int(f[p])][2]};
        costOf(lv, fade[p]);
        costOf(lv, wrong[p]);
    }
    for (int p : faded) {
        double gone[3] = {0, 0, 0};
        costOf(gone, fade[p]);
        double marker[3] = {1, 1, 1};
        costOf(marker, wrong[p]);
    }
    DecodeResult a = decodeFrameSoft(fade);
    CHECK(a.structureErrors == 0, "faded seconds counted as %d structure errors", a.structureErrors);
    CHECK(a.valid && a.tc.minute == 19 && a.tc.dayOfYear == 309, "faded-seconds frame rejected (%s)",
          a.problems.empty() ? "" : a.problems[0].c_str());
    DecodeResult b = decodeFrameSoft(wrong);
    CHECK(b.structureErrors == 7 && !b.valid, "wrong-kind pulses: %d structure errors, valid %d", b.structureErrors, b.valid);
    DecodeResult j = decodeFramesSoft({fade, fade});
    CHECK(j.structureErrors == 0, "joint decode counted faded seconds (%d)", j.structureErrors);
}

// Joint decode of consecutive minutes: a fade that wipes a different field in
// each minute must still give the right time, across hour, midnight and new
// year carries, and noise must never decode.
static void testJoint()
{
    std::printf("[timecode] joint decoding of consecutive faded minutes\n");
    const double ex[4][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}};
    std::mt19937 rng(11);
    std::normal_distribution<double> n(0, 1);
    auto soft = [&](int64_t minuteUnix, int fadeLo, int fadeHi, double sigma) {
        TimeCode tc = unixToTimeCode(minuteUnix);
        tc.dut1Tenths = -1;
        Frame f = encodeFrame(tc);
        SoftFrame sf;
        for (int p = 0; p < 60; p++) {
            double lv[3];
            for (int i = 0; i < 3; i++) lv[i] = ex[int(f[p])][i] + sigma * n(rng);
            if (p >= fadeLo && p <= fadeHi && p % 10 != 9) for (double& v : lv) v = 0.5; // faded: no information
            for (int h = 0; h < 4; h++) {
                double c = 0;
                for (int i = 0; i < 3; i++) c += (lv[i] - ex[h][i]) * (lv[i] - ex[h][i]);
                sf[p][h] = c;
            }
        }
        return sf;
    };
    struct Case { const char* name; int64_t start; } cases[] = {
        {"mid-day", 1793755620},                  // 2026-11-04 01:27
        {"hour carry", 1793755620 + 31 * 60},     // 01:58 -> 01:59 -> 02:00
        {"midnight", 1793750400 - 120},           // 23:58 -> 00:00 (day 307 -> 308)
        {"new year", 1798761600 - 120},           // 2026-12-31 23:58 -> 2027-01-01 00:00
    };
    for (auto& c : cases) {
        // minute k loses a different field: hour, minute units, day of year
        std::vector<SoftFrame> fs = {soft(c.start, 20, 26, 0.1), soft(c.start + 60, 10, 13, 0.1),
                                     soft(c.start + 120, 30, 41, 0.1)};
        int singles = 0;
        for (auto& f : fs) if (decodeFrameSoft(f).valid) singles++;
        DecodeResult r = decodeFramesSoft(fs);
        int64_t got = timeCodeToUnix(r.tc);
        CHECK(singles == 0, "%s: %d faded single frame(s) accepted", c.name, singles);
        CHECK(r.valid && got == c.start + 120 && r.tc.dut1Tenths == -1 && r.framesUsed == 3,
              "%s: joint decode valid=%d %s (%s %.2f)", c.name, r.valid, formatUnix(double(got), false).c_str(),
              r.weakestField.c_str(), r.minMargin);
        std::printf("  %-10s single frames valid %d/3, joint -> %s, weakest %s %.2f\n", c.name, singles,
                    formatUnix(double(got), false).c_str(), r.weakestField.c_str(), r.minMargin);
    }
    // the same field faded in every minute cannot be recovered and must not be guessed
    {
        std::vector<SoftFrame> fs = {soft(1793755620, 30, 41, 0.1), soft(1793755680, 30, 41, 0.1)};
        DecodeResult r = decodeFramesSoft(fs);
        CHECK(!r.valid, "day of year guessed with no information (%s %.2f)", r.weakestField.c_str(), r.minMargin);
    }
    // noise frames, joined in pairs and triples, must never decode
    int falseOk = 0;
    for (int i = 0; i < 300; i++) {
        std::vector<SoftFrame> fs;
        for (int k = 0; k < 2 + i % 2; k++) {
            SoftFrame sf;
            for (int p = 0; p < 60; p++) {
                double lv[3] = {0.5 + 0.5 * n(rng), 0.5 + 0.5 * n(rng), 0.5 + 0.5 * n(rng)};
                for (int h = 0; h < 4; h++) {
                    double c = 0;
                    for (int j = 0; j < 3; j++) c += (lv[j] - ex[h][j]) * (lv[j] - ex[h][j]);
                    sf[p][h] = c;
                }
            }
            fs.push_back(sf);
        }
        if (decodeFramesSoft(fs).valid) falseOk++;
    }
    CHECK(falseOk == 0, "%d/300 joined noise frame sets decoded", falseOk);
}

static void testCalendar()
{
    std::printf("[timecode] calendar helpers\n");
    CHECK(timeCodeToUnix(unixToTimeCode(1793577600)) == 1793577600, "round trip");
    TimeCode t = unixToTimeCode(1793577600); // 2026-11-02 00:00:00 UTC
    CHECK(t.year == 2026 && t.dayOfYear == 306 && t.hour == 0 && t.minute == 0, "2026-11-02 -> %d/%d", t.year, t.dayOfYear);
    int m, d;
    dayOfYearToDate(2028, 60, m, d); CHECK(m == 2 && d == 29, "2028 day 60 = %d/%d", m, d);
    dayOfYearToDate(2026, 60, m, d); CHECK(m == 3 && d == 1, "2026 day 60 = %d/%d", m, d);
    dayOfYearToDate(2028, 366, m, d); CHECK(m == 12 && d == 31, "2028 day 366");
    CHECK(dateToDayOfYear(2026, 12, 31) == 365 && dateToDayOfYear(2000, 12, 31) == 366, "doy");
    CHECK(formatUnix(1793577600.0 + 15 * 3600 + 17 * 60 + 30.25) == "2026-11-02 15:17:30.250", "%s",
          formatUnix(1793577600.0 + 15 * 3600 + 17 * 60 + 30.25).c_str());
    TimeCode a; a.dst1 = true; a.dst2 = false;
    CHECK(dstState(a) == DstState::BeginsToday, "dst begins");
    a.dst1 = false; a.dst2 = true;
    CHECK(dstState(a) == DstState::EndsToday, "dst ends");
}

static void testSkywave()
{
    std::printf("[engine] skywave delay estimate, Palmer AK\n");
    int hw, hh;
    double dw = estimateSkywaveDelay(61.6, -149.1, kWwvLat, kWwvLon, &hw);
    double dh = estimateSkywaveDelay(61.6, -149.1, kWwvhLat, kWwvhLon, &hh);
    std::printf("  WWV  %.1f ms (%d hops)   WWVH %.1f ms (%d hops)\n", dw * 1e3, hw, dh * 1e3, hh);
    CHECK(dw > 0.0125 && dw < 0.0145, "WWV %.4f", dw);
    CHECK(dh > 0.0150 && dh < 0.0175, "WWVH %.4f", dh);
}

// ------------------------------------------------------------------ scenarios
struct RunResult {
    double tDecoded = -1, tVerified = -1;
    int wrongSeconds = 0, labelledSeconds = 0, frameOk = 0, frameBad = 0;
    double maxBoundaryErr = 0;   // vs. truth, after Verified (s)
    int holdoverSeconds = 0;
    int provisionalSeen = 0, provisionalPublicUtcLeaks = 0, provisionalClockOffsetLeaks = 0;
    int holdoverPublicUtcLeaks = 0, holdoverClockOffsetLeaks = 0;
    DecoderStatus final;
    double finalUtcErr = 1e9;
};

// truthDelay: delay of the station used as truth for boundary error.
static RunResult runScenario(const SimConfig& sc, double seconds, bool audio = false,
                             DecoderConfig dc = DecoderConfig(), bool pcStamps = false, double pcOffset = 0.0,
                             std::function<void(double, const DecoderStatus&)> perSecond = nullptr)
{
    dc.sampleRate = sc.sampleRate;
    WwvSim sim(sc);
    WwvDecoder dec(dc);
    RunResult rr;
    const SimStation& ts = sc.st[0].enabled ? sc.st[0] : sc.st[1];
    std::vector<std::complex<float>> buf;
    std::vector<float> aud;
    const size_t block = size_t(sc.sampleRate / 20);
    size_t blocks = size_t(seconds * 20);
    double nextReport = 1.0;
    for (size_t k = 0; k < blocks; k++) {
        uint64_t first = sim.samplesGenerated();
        sim.generate(buf, block);
        if (pcStamps) dec.setNextSampleTimestamp(sim.trueUtc(first) + pcOffset);
        if (audio) {
            // AM demodulated audio as SDRangel would give it: |iq|, DC removed, arbitrary gain
            aud.resize(block);
            for (size_t i = 0; i < block; i++) aud[i] = 0.3f * (std::abs(buf[i]) - 1.0f);
            dec.processAudio(aud.data(), aud.size());
        } else {
            dec.processIQ(buf.data(), buf.size());
        }
        for (auto& e : dec.takeEvents()) {
            // AHRWWV_TEST_FRAMES=1 prints every frame decision (debugging aid).
            static const bool showFrames = std::getenv("AHRWWV_TEST_FRAMES") != nullptr;
            if (showFrames && (e.type == DecoderEvent::Type::FrameDecoded || e.type == DecoderEvent::Type::FrameRejected)) {
                double tTrue = sc.startUnix + dec.currentSampleTime() - ts.delay;
                std::printf("      %6.1f s %s %s weakest %s %.2f frames %d struct %d | %s | true %s\n", dec.currentSampleTime(),
                            e.type == DecoderEvent::Type::FrameDecoded ? "OK " : "BAD",
                            formatUnix(double(timeCodeToUnix(e.decode.tc)), false).c_str(), e.decode.weakestField.c_str(),
                            e.decode.minMargin, e.decode.framesUsed, e.decode.structureErrors,
                            e.decode.problems.empty() ? "" : e.decode.problems.back().c_str(), formatUnix(tTrue, false).c_str());
            }
            if (e.type == DecoderEvent::Type::FrameDecoded) rr.frameOk++;
            if (e.type == DecoderEvent::Type::FrameRejected) rr.frameBad++;
            if (e.type == DecoderEvent::Type::Second && e.haveUtc) {
                rr.labelledSeconds++;
                // transmit-time truth of this boundary
                double tTrue = sc.startUnix + e.second.boundaryTime / (1.0 + sc.ppm * 1e-6) - ts.delay;
                if (std::fabs(e.utcAtBoundary - tTrue) > 0.5) rr.wrongSeconds++;
                if (dec.status().state == DecoderState::Verified) {
                    double u; dec.utcAt(e.second.boundaryTime, u);
                    rr.maxBoundaryErr = std::max(rr.maxBoundaryErr, std::fabs(u - tTrue));
                }
            }
        }
        double t = dec.currentSampleTime();
        const DecoderStatus& s = dec.status();
        if (s.state == DecoderState::Decoded) {
            rr.provisionalSeen++;
            double tentativeUtc;
            if (dec.currentUtc(tentativeUtc)) rr.provisionalPublicUtcLeaks++;
            if (s.haveClockOffset) rr.provisionalClockOffsetLeaks++;
        }
        // HOLDOVER is explicitly not trusted UTC: the public API and offset
        // readiness flag must both be closed even if a predicted time remains.
        if (s.state == DecoderState::Holdover) {
            double staleUtc;
            if (dec.currentUtc(staleUtc)) rr.holdoverPublicUtcLeaks++;
            if (s.haveClockOffset) rr.holdoverClockOffsetLeaks++;
        }
        if (rr.tDecoded < 0 && (s.state == DecoderState::Decoded || s.state == DecoderState::Verified)) rr.tDecoded = t;
        if (rr.tVerified < 0 && s.state == DecoderState::Verified) rr.tVerified = t;
        if (t >= nextReport) {
            if (s.state == DecoderState::Holdover) rr.holdoverSeconds++;
            if (perSecond) perSecond(t, s);
            nextReport += 1.0;
        }
    }
    rr.final = dec.status();
    double u;
    if (dec.currentUtc(u)) {
        double truth = sim.trueUtc(sim.samplesGenerated() - 1) - ts.delay;
        rr.finalUtcErr = u - truth;
    }
    return rr;
}

static void report(const char* name, const RunResult& r)
{
    std::printf("  %-34s state=%-11s decoded@%5.0fs verified@%5.0fs frames ok/bad %d/%d wrong-sec %d/%d "
                "max-err %.2f ms snr %.1f dB jitter %.2f ms station %s\n",
                name, decoderStateName(r.final.state), r.tDecoded, r.tVerified, r.frameOk, r.frameBad,
                r.wrongSeconds, r.labelledSeconds, r.maxBoundaryErr * 1e3, r.final.subcarrierSnrDb,
                r.final.timingJitter * 1e3, stationName(r.final.station));
}

static SimConfig baseSim(double start, double fs = 12000)
{
    SimConfig sc;
    sc.sampleRate = fs;
    sc.startUnix = start;
    sc.snrDb = 30;
    sc.st[0].delay = 0.0134;
    return sc;
}

static const double kNov2 = 1793577600.0; // 2026-11-02 00:00:00 UTC

static void testClean()
{
    std::printf("[decoder] clean signal, IQ input, start mid-minute\n");
    for (double off : {17.25, 44.6, 59.3}) {
        SimConfig sc = baseSim(kNov2 + 15 * 3600 + 17 * 60 + off);
        RunResult r = runScenario(sc, 200);
        char nm[64]; std::snprintf(nm, sizeof nm, "start at :%04.1f", off);
        report(nm, r);
        CHECK(r.final.state == DecoderState::Verified, "not verified");
        CHECK(r.tVerified > 0 && r.tVerified < 190, "verify took %.0f s", r.tVerified);
        CHECK(r.wrongSeconds == 0, "%d wrong labelled seconds (including provisional DECODED)", r.wrongSeconds);
        CHECK(r.provisionalSeen > 0 && r.provisionalPublicUtcLeaks == 0 && r.provisionalClockOffsetLeaks == 0,
              "provisional time leaked: states %d, UTC %d, PC offset %d",
              r.provisionalSeen, r.provisionalPublicUtcLeaks, r.provisionalClockOffsetLeaks);
        CHECK(r.final.confirmationPreviousLastFrameId > 0
              && r.final.confirmationPreviousLastFrameId < r.final.confirmationCurrentFirstFrameId,
              "VERIFIED did not use disjoint raw minutes: %llu / %llu",
              (unsigned long long) r.final.confirmationPreviousLastFrameId,
              (unsigned long long) r.final.confirmationCurrentFirstFrameId);
        CHECK(r.maxBoundaryErr < 0.001, "boundary error %.4f", r.maxBoundaryErr);
        CHECK(r.final.station == StationId::WWV, "station %s", stationName(r.final.station));
    }
}

static void testAudioAndRates()
{
    std::printf("[decoder] AM audio input and other sample rates / sample clock errors\n");
    {
        SimConfig sc = baseSim(kNov2 + 7 * 3600 + 3 * 60 + 12.0, 48000);
        RunResult r = runScenario(sc, 200, true);
        report("audio 48 kHz", r);
        CHECK(r.final.state == DecoderState::Verified && r.wrongSeconds == 0, "audio path");
    }
    struct { double fs, ppm; } cases[] = {{8000, 40}, {44100, -25}, {24000, 0}};
    for (auto c : cases) {
        SimConfig sc = baseSim(kNov2 + 22 * 3600 + 41 * 60 + 33.3, c.fs);
        sc.ppm = c.ppm;
        RunResult r = runScenario(sc, 240);
        char nm[64]; std::snprintf(nm, sizeof nm, "IQ %.0f Hz, %+.0f ppm", c.fs, c.ppm);
        report(nm, r);
        std::printf("    measured sample clock %+.1f ppm (valid %d)\n", r.final.sampleClockPpm, r.final.sampleClockPpmValid);
        CHECK(r.final.state == DecoderState::Verified && r.wrongSeconds == 0, "rate case");
        CHECK(r.maxBoundaryErr < 0.0015, "boundary err %.4f", r.maxBoundaryErr);
        CHECK(r.final.sampleClockPpmValid && std::fabs(r.final.sampleClockPpm - c.ppm) < 5, "ppm %.1f", r.final.sampleClockPpm);
    }
}

static void testNoiseSweep()
{
    std::printf("[decoder] carrier-to-noise sweep (3 kHz bandwidth), 6 minutes each\n");
    for (double snr : {20.0, 10.0, 6.0, 3.0, 0.0, -3.0}) {
        SimConfig sc = baseSim(kNov2 + 12 * 3600 + 30 * 60 + 20.0);
        sc.snrDb = snr;
        sc.seed = 11;
        RunResult r = runScenario(sc, 360);
        char nm[64]; std::snprintf(nm, sizeof nm, "C/N %+.0f dB", snr);
        report(nm, r);
        CHECK(r.wrongSeconds == 0, "wrong seconds at %.0f dB", snr);
        if (snr >= 6) CHECK(r.final.state == DecoderState::Verified, "not verified at %.0f dB", snr);
    }
}

static void testFadingAndWwvh()
{
    std::printf("[decoder] fading, WWV+WWVH on the same frequency\n");
    {
        SimConfig sc = baseSim(kNov2 + 3 * 3600 + 5 * 60 + 40.0);
        sc.snrDb = 15; sc.st[0].fadeDepth = 0.9; sc.st[0].fadeRateHz = 0.3; sc.seed = 5;
        RunResult r = runScenario(sc, 360);
        report("Rayleigh-like fading, C/N 15", r);
        CHECK(r.wrongSeconds == 0, "wrong");
        CHECK(r.final.haveTime, "no time");
    }
    {
        SimConfig sc = baseSim(kNov2 + 9 * 3600 + 59 * 60 + 2.0);
        sc.snrDb = 20;
        sc.st[0].delay = 0.0134; sc.st[0].fadeDepth = 0.6; sc.st[0].fadeRateHz = 0.15;
        sc.st[1].enabled = true; sc.st[1].delay = 0.0162; sc.st[1].amplitude = 0.8;
        sc.st[1].fadeDepth = 0.6; sc.st[1].fadeRateHz = 0.2; sc.seed = 9;
        RunResult r = runScenario(sc, 300);
        report("WWV + WWVH (2.8 ms apart)", r);
        CHECK(r.final.state == DecoderState::Verified && r.wrongSeconds == 0, "dual");
        CHECK(r.final.station == StationId::Both, "station %s (f1000 %.2f f1200 %.2f)", stationName(r.final.station),
              r.final.tick1000, r.final.tick1200);
    }
    {
        SimConfig sc = baseSim(kNov2 + 9 * 3600 + 20 * 60 + 2.0);
        sc.st[0].enabled = false;
        sc.st[1].enabled = true; sc.st[1].delay = 0.0162;
        RunResult r = runScenario(sc, 200);
        report("WWVH only", r);
        CHECK(r.final.state == DecoderState::Verified && r.wrongSeconds == 0, "wwvh");
        CHECK(r.final.station == StationId::WWVH, "station %s", stationName(r.final.station));
    }
}

static void testMultipathAndVoice()
{
    std::printf("[decoder] two-hop multipath and harsh in-band voice\n");
    {
        // Two hops 1.8 ms apart independently fade, rotating 100 Hz phase.
        SimConfig sc = baseSim(kNov2 + 0 * 3600 + 8 * 60 + 16.0);
        sc.snrDb = 20;
        sc.st[0].fadeDepth = 0.9; sc.st[0].fadeRateHz = 0.6;
        sc.st[1].enabled = true; sc.st[1].tickHz = 1000.0; sc.st[1].delay = sc.st[0].delay + 0.0018;
        sc.st[1].amplitude = 0.9; sc.st[1].fadeDepth = 0.9; sc.st[1].fadeRateHz = 0.7;
        sc.st[1].carrierPhase = 2.0; sc.seed = 21;
        RunResult r = runScenario(sc, 420);
        report("WWV two-hop multipath, 1.8 ms", r);
        CHECK(r.final.haveTime && r.wrongSeconds == 0, "multipath: time %d wrong %d", r.final.haveTime, r.wrongSeconds);
    }
    {
        // Intentionally strong energy near 100 Hz: do not show wrong time.
        SimConfig sc = baseSim(kNov2 + 13 * 3600 + 41 * 60 + 5.0);
        sc.st[0].voiceCentreHz = 150.0; sc.st[0].voiceSwingHz = 100.0;
        RunResult r = runScenario(sc, 360);
        report("harsh in-band voice (worst case)", r);
        CHECK(r.wrongSeconds == 0, "harsh voice produced %d wrong seconds", r.wrongSeconds);
    }
}

// High-latitude two-path flutter regression, recorded configuration.
static void testHighLatitudeFlutter()
{
    std::printf("[decoder] high-latitude flutter: two paths, fast deep fading\n");
    for (int sd : {101, 103, 104}) {
        SimConfig sc = baseSim(kNov2 + 3600 * (sd - 100) + 17 * 60 + 9.0);
        sc.snrDb = 15; sc.seed = unsigned(sd);
        sc.st[0].fadeDepth = 1.0; sc.st[0].fadeRateHz = 0.6;
        sc.st[1].enabled = true; sc.st[1].tickHz = 1000.0; sc.st[1].delay = sc.st[0].delay + 0.004;
        sc.st[1].amplitude = 1.0; sc.st[1].fadeDepth = 1.0; sc.st[1].fadeRateHz = 0.8; sc.st[1].carrierPhase = 2.0;
        // Calibrated likelihoods: 8 seeds all verify within 780 s, none wrong
        // (seeds 101 and 103 take 771 and 711 s; the raw-cost rule took 531
        // and 591 s but was faster to give up evidence in noisy minutes).
        RunResult r = runScenario(sc, 900);
        char name[48];
        std::snprintf(name, sizeof name, "flutter 0.6/0.8 Hz, 4 ms, seed %d", sd);
        report(name, r);
        // Verified, or holding over a verified time through a late fade.
        CHECK(r.tVerified > 0 && (r.final.state == DecoderState::Verified || r.final.state == DecoderState::Holdover),
              "seed %d: %s", sd, decoderStateName(r.final.state));
        CHECK(r.wrongSeconds == 0, "seed %d: %d wrong seconds", sd, r.wrongSeconds);
    }
}

// The live false DECODED (5 MHz evening, Palmer AK, 2026-11-05: 05:04 shown
// while the true time was 05:08) came from deep two-station fading plus
// static. Recordings stay private, so this reproduces the conditions
// synthetically. Any wrong candidate minute must stay provisional: never
// VERIFIED, never published as UTC or as a PC clock offset.
static void testFalseDecodedRegression()
{
    std::printf("[decoder] false-DECODED regression: WWV+WWVH deep fading with static crashes\n");
    // Same draw as tests/stress.cpp, "WWV+WWVH deep fade + static", trials
    // 5 and 0-3. Trial 5 produces a confidently wrong candidate minute
    // (2022-10-12 17:28 for 2026-10-12 17:28): it must be rejected by
    // independent confirmation, not shown as time.
    for (int k : {5, 0, 1, 2, 3}) {
        std::mt19937 rng(1000 + k);
        SimConfig sc;
        sc.sampleRate = 12000;
        sc.startUnix = 1767225600.0 + double(rng() % 31536000) + (rng() % 1000) / 1000.0;
        sc.snrDb = 12; sc.seed = 77 + k;
        sc.st[0].delay = 0.0134; sc.st[0].fadeDepth = 0.95; sc.st[0].fadeRateHz = 0.1 + 0.3 * (rng() % 100) / 100.0;
        sc.dut1Tenths = int(rng() % 15) - 7;
        sc.impulseRate = 0.25;
        sc.st[1].enabled = true; sc.st[1].delay = 0.0162; sc.st[1].amplitude = 0.5 + (rng() % 100) / 100.0;
        sc.st[1].fadeDepth = 0.95; sc.st[1].fadeRateHz = 0.25;
        RunResult r = runScenario(sc, 600);
        char name[48];
        std::snprintf(name, sizeof name, "deep fade + static, draw %d", k);
        report(name, r);
        CHECK(r.wrongSeconds == 0, "draw %d: %d wrong labelled seconds", k, r.wrongSeconds);
        CHECK(r.provisionalPublicUtcLeaks == 0 && r.provisionalClockOffsetLeaks == 0,
              "draw %d: provisional time leaked (UTC %d, PC offset %d)", k,
              r.provisionalPublicUtcLeaks, r.provisionalClockOffsetLeaks);
        CHECK(r.tVerified > 0, "draw %d: never verified", k);
    }
}

static void testDropoutHoldover()
{
    std::printf("[decoder] 3-minute signal loss after lock: holdover and recovery\n");
    SimConfig sc = baseSim(kNov2 + 18 * 3600 + 0 * 60 + 10.0);
    sc.dropouts.push_back({200.0, 380.0});
    bool sawHoldover = false;
    double worstHoldErr = 0, maxUnc = 0;
    DecoderConfig dc;
    RunResult r = runScenario(sc, 560, false, dc, false, 0, [&](double t, const DecoderStatus& s) {
        if (s.state == DecoderState::Holdover) {
            sawHoldover = true;
            maxUnc = std::max(maxUnc, s.timeUncertainty);
        }
        (void)t;
    });
    report("dropout 200-380 s", r);
    std::printf("    holdover seconds %d, max reported uncertainty %.1f ms\n", r.holdoverSeconds, maxUnc * 1e3);
    CHECK(sawHoldover, "never entered holdover");
    CHECK(r.holdoverPublicUtcLeaks == 0 && r.holdoverClockOffsetLeaks == 0,
          "holdover leaked trusted UTC %d times or clock offset %d times",
          r.holdoverPublicUtcLeaks, r.holdoverClockOffsetLeaks);
    CHECK(r.final.state == DecoderState::Verified, "did not recover (%s)", decoderStateName(r.final.state));
    CHECK(r.wrongSeconds == 0, "wrong seconds %d", r.wrongSeconds);
    CHECK(std::fabs(r.finalUtcErr) < 0.002, "final err %.4f", r.finalUtcErr);
    (void)worstHoldErr;
}

static void testNoFalseLock()
{
    std::printf("[decoder] false-lock checks (must never produce a time)\n");
    {
        SimConfig sc = baseSim(kNov2);
        sc.st[0].enabled = false; // noise only
        sc.snrDb = 0;
        RunResult r = runScenario(sc, 900);
        report("noise only, 15 min", r);
        CHECK(!r.final.haveTime && r.frameOk == 0 && r.tDecoded < 0, "noise produced a time");
    }
    {
        SimConfig sc = baseSim(kNov2 + 3600);
        sc.st[0].subcarrierMod = 0.0; // carrier, ticks, tones, voice but no time code
        RunResult r = runScenario(sc, 600);
        report("AM station without time code", r);
        CHECK(!r.final.haveTime && r.frameOk == 0, "no-timecode signal produced a time");
    }
    {
        SimConfig sc = baseSim(kNov2 + 7200);
        sc.st[0].enabled = false;
        sc.snrDb = 25;
        RunResult r = runScenario(sc, 600, true);
        report("audio noise only, 10 min", r);
        CHECK(!r.final.haveTime && r.frameOk == 0, "audio noise produced a time");
    }
}

static void testFlagsAndRollover()
{
    std::printf("[decoder] flags, year rollover, leap day\n");
    {
        SimConfig sc = baseSim(kNov2 + 6 * 3600 + 30.0);
        sc.dut1Tenths = -3; sc.lsw = true; sc.dst1 = true; sc.dst2 = false;
        RunResult r = runScenario(sc, 200);
        report("DUT1 -0.3, LSW, DST begins", r);
        const TimeCode& t = r.final.lastTimeCode;
        CHECK(r.final.haveTime && t.dut1Tenths == -3 && t.leapSecondWarning && t.dst1 && !t.dst2, "flags dut1 %d lsw %d dst %d%d",
              t.dut1Tenths, t.leapSecondWarning, t.dst1, t.dst2);
    }
    {
        double start = double(daysFromCivil(2026, 12, 31)) * 86400 + 23 * 3600 + 57 * 60 + 20;
        SimConfig sc = baseSim(start);
        RunResult r = runScenario(sc, 260);
        report("across 2027 new year", r);
        const TimeCode& t = r.final.lastTimeCode;
        CHECK(r.final.state == DecoderState::Verified && t.year == 2027 && t.dayOfYear == 1 && r.wrongSeconds == 0,
              "rollover %d/%d %02d:%02d", t.year, t.dayOfYear, t.hour, t.minute);
    }
    {
        double start = double(daysFromCivil(2028, 12, 31)) * 86400 + 12 * 3600 + 40;
        SimConfig sc = baseSim(start);
        RunResult r = runScenario(sc, 200);
        report("2028 day 366", r);
        CHECK(r.final.haveTime && r.final.lastTimeCode.dayOfYear == 366 && r.wrongSeconds == 0, "day 366");
    }
}

// Verification evidence is never reusable: each raw-minute span must be
// disjoint, and no shifted/gapped joint window can be passed to the decoder.
static void testRawMinuteEvidenceSafety()
{
    std::printf("[decoder] raw-minute identity, alignment and independence guards\n");
    CHECK(consecutiveRawMinutes({10, 11, 12}, {59.1, 119.1, 179.1}), "valid consecutive window rejected");
    CHECK(!consecutiveRawMinutes({10, 10, 11}, {59, 119, 179}), "duplicate raw-minute ID accepted");
    CHECK(!consecutiveRawMinutes({10, 12}, {59, 179}), "missing minute / 120 s jump accepted");
    CHECK(!consecutiveRawMinutes({10, 11}, {59, 129}), "shifted 70-second window accepted");
    CHECK(!consecutiveRawMinutes({10, 11}, {59, 118}), "shifted 59-second window accepted");
    CHECK(!consecutiveRawMinutes({}, {}), "empty evidence window accepted");
    CHECK(disjointRawMinuteWindows(1, 3, 4, 6), "non-overlapping windows rejected");
    CHECK(!disjointRawMinuteWindows(1, 3, 3, 5), "overlap accepted as independent");
    CHECK(!disjointRawMinuteWindows(1, 5, 2, 3), "nested overlap accepted as independent");
    CHECK(!disjointRawMinuteWindows(0, 0, 3, 4), "uninitialized frame ID accepted");
}

// Calibrated likelihoods: independent of the raw cost scale, best hypothesis
// at 0, and no single second worth more than the outlier cap.
static void testSoftCalibration()
{
    std::printf("[timecode] calibrated log-likelihoods\n");
    std::mt19937 rng(7);
    std::normal_distribution<double> n(0.0, 0.25);
    SoftFrame a{};
    for (int p = 0; p < 60; p++)
        for (int h = 0; h < 4; h++) a[p][h] = (h == 1 ? 0.0 : 1.0) + n(rng) * n(rng);
    a[33][2] = 40.0; // one wild second
    SoftFrame b = a;
    for (auto& s : b) for (auto& v : s) v *= 3.0; // same frame, 3x the noise power
    SoftFrame ca = calibrateSoftFrame(a), cb = calibrateSoftFrame(b);
    double maxDiff = 0, maxV = 0, minBest = 1e9;
    for (int p = 0; p < 60; p++) {
        double best = 1e9;
        for (int h = 0; h < 4; h++) {
            maxDiff = std::max(maxDiff, std::fabs(ca[p][h] - cb[p][h]));
            maxV = std::max(maxV, ca[p][h]);
            best = std::min(best, ca[p][h]);
        }
        minBest = std::min(minBest, best);
        CHECK(std::fabs(best) < 1e-9, "second %d: best hypothesis %.3f, expected 0", p, best);
    }
    CHECK(maxDiff < 1e-9, "calibration depends on raw scale (%.3g)", maxDiff);
    CHECK(maxV <= -std::log(kOutlierProb) + 1e-9, "a second counts %.2f nats, cap %.2f", maxV, -std::log(kOutlierProb));
}

static void testClockOffset()
{
    std::printf("[decoder] PC clock offset measurement\n");
    SimConfig sc = baseSim(kNov2 + 20 * 3600 + 15.5);
    DecoderConfig dc;
    dc.propagationDelay = 0.0134; // matches the simulated path
    RunResult r = runScenario(sc, 220, false, dc, true, 0.250);
    report("PC clock 250 ms fast", r);
    std::printf("    measured offset %+.2f ms +/- %.1f ms (1 sigma)\n", r.final.clockOffset * 1e3, r.final.clockOffsetUncertainty * 1e3);
    CHECK(r.final.haveClockOffset && std::fabs(r.final.clockOffset - 0.250) < 0.002, "offset %.4f", r.final.clockOffset);
}

static void testEnvelopeTap()
{
    std::printf("[engine] diagnostic envelope tap (used by the plugin GUI)\n");
    SimConfig sc = baseSim(kNov2 + 5 * 3600 + 12.5);
    DecoderConfig dc;
    dc.sampleRate = sc.sampleRate;
    WwvSim sim(sc);
    WwvDecoder dec(dc);
    struct Acc { long n = 0; double maxv = -1e9; double sumHigh = 0; long nHigh = 0; } acc;
    dec.setEnvelopeTap([](void* u, double, float v) {
        Acc* a = static_cast<Acc*>(u);
        a->n++;
        a->maxv = std::max(a->maxv, double(v));
        if (v > 0.5) { a->sumHigh += v; a->nHigh++; }
    }, &acc);
    std::vector<std::complex<float>> buf;
    for (int k = 0; k < 60 * 20; k++) { sim.generate(buf, 600); dec.processIQ(buf.data(), buf.size()); }
    dec.reset(); // the tap must survive reset()
    for (int k = 0; k < 20; k++) { sim.generate(buf, 600); dec.processIQ(buf.data(), buf.size()); }
    double meanHigh = acc.nHigh ? acc.sumHigh / acc.nHigh : 0;
    std::printf("  %ld samples in 61 s, mean pulse level %.2f\n", acc.n, meanHigh);
    CHECK(acc.n > 60500 && acc.n < 61500, "tap called %ld times", acc.n);
    CHECK(meanHigh > 0.8 && meanHigh < 1.25, "pulse level %.2f", meanHigh);
}

int main(int argc, char** argv)
{
    bool quick = argc > 1 && std::string(argv[1]) == "--quick";
    testEncodeKnown();
    testRoundTrip();
    testRejects();
    testSoft();
    testFadedSecondsAreErasures();
    testJoint();
    testCalendar();
    testSkywave();
    {
        WwvDecoder d;
        std::printf("[engine] measured detector delay at 48 kHz: %.2f ms\n", d.detectorDelay() * 1e3);
    }
    testEnvelopeTap();
    testRawMinuteEvidenceSafety();
    testSoftCalibration();
    testClean();
    if (!quick) {
        testAudioAndRates();
        testNoiseSweep();
        testFadingAndWwvh();
        testMultipathAndVoice();
        testHighLatitudeFlutter();
        testFalseDecodedRegression();
        testDropoutHoldover();
        testNoFalseLock();
        testFlagsAndRollover();
        testClockOffset();
    }
    std::printf("\n%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
