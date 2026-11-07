// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 KL5NE, Alaska HamRadio, and AHR-Decoder contributors
// On-air tests of the C API with rig-style USB audio built from the
// simulator: carrier as a tone near 1500 Hz, upper sideband only, rig
// passband 300-2700 Hz, at sound-card rates, with capture timestamps from a
// PC clock that is deliberately off.
#include "ahr_wwv.h"
#include "wwvsim.h"

#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

extern "C" int ahr_wwv_c_header_checks(void);

using namespace ahrwwv;

static int g_checks = 0, g_fails = 0;
#define CHECK(cond, ...) do { g_checks++; if (!(cond)) { g_fails++; std::printf("  FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond); \
    std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

namespace {
constexpr double kPi = 3.14159265358979323846;

// Complex FIR low-pass used to keep only 0..+2700 Hz of the simulator's
// double-sideband baseband (shift down 1350 Hz, low-pass +/-1350, shift up):
// a rig in USB passes the carrier and the upper sideband only.
struct UsbRig {
    double fsSim, fsAudio, carrier;
    std::vector<double> h;
    std::vector<std::complex<double>> hist;
    size_t pos = 0;
    double ph1 = 0, ph2 = 0, t = 0;
    double resampAcc = 0; // simple linear resampler state
    std::complex<double> prev{0, 0};
    bool havePrev = false;

    UsbRig(double fsSim_, double fsAudio_, double carrier_) : fsSim(fsSim_), fsAudio(fsAudio_), carrier(carrier_)
    {
        const int n = 401;
        const double cut = 1350.0 / fsSim;
        double sum = 0;
        for (int i = 0; i < n; i++) {
            double m = i - (n - 1) / 2.0;
            double v = (m == 0 ? 2 * cut : std::sin(2 * kPi * cut * m) / (kPi * m)) * (0.54 - 0.46 * std::cos(2 * kPi * i / (n - 1)));
            h.push_back(v); sum += v;
        }
        for (auto& v : h) v /= sum;
        hist.assign(h.size(), 0.0);
    }

    double groupDelay() const { return (double(h.size()) - 1.0) / 2.0 / fsSim; }

    // Upper sideband of one simulator sample (complex baseband, carrier at 0).
    std::complex<double> usb(std::complex<double> z)
    {
        z *= std::polar(1.0, -ph1);
        ph1 += 2 * kPi * 1350.0 / fsSim;
        hist[pos] = z; pos = (pos + 1) % h.size();
        std::complex<double> acc = 0;
        size_t p = pos;
        for (size_t k = 0; k < h.size(); k++) { acc += hist[p] * h[k]; if (++p == h.size()) p = 0; }
        return acc * std::polar(1.0, ph1 - 2 * kPi * 1350.0 / fsSim); // back up
    }

    // Convert a block of simulator IQ into real audio at fsAudio with the
    // carrier at `carrier` Hz.
    void block(const std::vector<std::complex<float>>& iq, std::vector<float>& audio)
    {
        audio.clear();
        const double ratio = fsAudio / fsSim;
        for (auto& s : iq) {
            std::complex<double> u = usb(std::complex<double>(s.real(), s.imag()));
            if (!havePrev) { prev = u; havePrev = true; }
            // emit audio samples falling between prev and u
            while (resampAcc < 1.0) {
                std::complex<double> v = prev + (u - prev) * resampAcc;
                audio.push_back(float(std::real(v * std::polar(1.0, ph2))));
                ph2 += 2 * kPi * carrier / fsAudio;
                if (ph2 > kPi) ph2 -= 2 * kPi;
                resampAcc += 1.0 / ratio;
            }
            resampAcc -= 1.0;
            prev = u;
        }
    }
};

struct Result { bool verified = false; double tVerified = -1; int wrongSeconds = 0; ahr_wwv_status last{}; };

Result run(double fsAudio, double carrierTone, double snrDb, double pcOffset, double latency, double seconds,
           double startUtc, bool fading = false, double ppm = 0.0)
{
    SimConfig sc;
    sc.sampleRate = 12000;
    sc.startUnix = startUtc;
    sc.snrDb = snrDb;
    sc.st[0].delay = 0.0134;
    sc.dut1Tenths = -1;
    if (fading) { sc.st[0].fadeDepth = 0.9; sc.st[0].fadeRateHz = 0.2; }
    sc.ppm = ppm;
    WwvSim sim(sc);
    UsbRig rig(sc.sampleRate, fsAudio, carrierTone);

    ahr_wwv_config c;
    ahr_wwv_config_default(&c);
    c.sample_rate_hz = fsAudio;
    c.carrier_hz = 1500.0;            // the rig may be off by a few tens of Hz
    c.propagation_delay_s = 0.0134;   // as simulated
    // A calibration measures the whole rig delay: here the stated latency plus
    // this test rig's own USB filter (its FIR group delay).
    c.capture_latency_s = latency + rig.groupDelay();
    ahr_wwv_handle* h = ahr_wwv_create(&c);
    Result r;
    if (!h) return r;
    std::vector<std::complex<float>> iq;
    std::vector<float> audio;
    ahr_wwv_status st;
    double audioSamples = 0; // count of audio samples delivered so far
    const size_t blk = size_t(sc.sampleRate / 10);
    for (int b = 0; b < int(seconds * 10); b++) {
        sim.generate(iq, blk);
        rig.block(iq, audio);
        // PC timestamp of the first audio sample: true capture time (the
        // simulator's UTC at the antenna plus the rig's latency) + PC error.
        double tCapture = sc.startUnix + audioSamples / fsAudio / (1.0 + ppm * 1e-6) + latency; // real time
        ahr_wwv_push_audio(h, audio.data(), audio.size(), tCapture + pcOffset);
        audioSamples += double(audio.size());
        st.struct_size = sizeof st;
        ahr_wwv_poll(h, &st);
        double tNow = sc.startUnix + audioSamples / fsAudio / (1.0 + ppm * 1e-6) - sc.st[0].delay; // transmitter UTC now
        if (st.utc_valid && std::fabs(st.utc_unix_s - tNow) > 0.5) r.wrongSeconds++;
        if (!r.verified && st.state == AHR_WWV_VERIFIED) { r.verified = true; r.tVerified = audioSamples / fsAudio; }
    }
    r.last = st;
    ahr_wwv_destroy(h);
    return r;
}

} // namespace

int main()
{
    std::printf("[capi] C header compiled as C, argument checks\n");
    int cf = ahr_wwv_c_header_checks();
    g_checks += 10; g_fails += cf;

    const double kNov6 = 1793923200.0 + 22 * 3600 + 22 * 60 + 13.7; // 2026-11-06 22:22:13.7 UTC
    struct Case { const char* name; double fs, tone, snr, pcOff, lat; bool fade; double ppm; };
    Case cases[] = {
        {"48 kHz, tone 1500 Hz, PC 250 ms fast", 48000, 1500.0, 30, 0.250, 0.035, false, 0.0},
        {"44.1 kHz, rig 12 Hz high (1512 Hz), PC 1.7 s slow", 44100, 1512.0, 25, -1.700, 0.035, false, 0.0},
        {"48 kHz, rig 25 Hz low, deep fading", 48000, 1475.0, 18, 0.080, 0.050, true, 0.0},
        {"48 kHz, sound card 150 ppm fast", 48000, 1500.0, 25, -0.400, 0.035, false, 150.0},
    };
    for (auto& c : cases) {
        Result r = run(c.fs, c.tone, c.snr, c.pcOff, c.lat, 600, kNov6, c.fade, c.ppm);
        std::printf("  %-48s %s verified@%4.0f s, PC offset %+.4f s (+/- %.3f), wrong %d, SNR %.1f dB, ppm %+.1f +/- %.1f\n",
                    c.name, ahr_wwv_state_name(r.last.state), r.tVerified, r.last.clock_offset_s,
                    r.last.clock_offset_uncertainty_s, r.wrongSeconds, r.last.subcarrier_snr_db,
                    r.last.sample_clock_ppm, r.last.sample_clock_ppm_sigma);
        CHECK(r.verified, "%s: never verified", c.name);
        CHECK(r.wrongSeconds == 0, "%s: %d wrong UTC seconds", c.name, r.wrongSeconds);
        CHECK(r.last.clock_offset_valid && std::fabs(r.last.clock_offset_s - c.pcOff) < 0.010,
              "%s: offset %.4f, expected %.3f", c.name, r.last.clock_offset_s, c.pcOff);
        CHECK(r.last.flags_confirmed && r.last.dut1_tenths == -1, "%s: DUT1 %d confirmed %d", c.name,
              r.last.dut1_tenths, r.last.flags_confirmed);
        if (c.ppm != 0.0)
            CHECK(r.last.sample_clock_settled && std::fabs(r.last.sample_clock_ppm - c.ppm) < 3 * r.last.sample_clock_ppm_sigma + 0.5,
                  "%s: sample clock %+.1f +/- %.1f", c.name, r.last.sample_clock_ppm, r.last.sample_clock_ppm_sigma);
        CHECK(r.last.year == 2026 && r.last.day_of_year == 310, "%s: date %d/%d", c.name, r.last.year, r.last.day_of_year);
    }
    std::printf("\n%d checks, %d failed\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
