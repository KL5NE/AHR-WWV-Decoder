// Monte Carlo stress test: many random starts/seeds per condition.
// Reports how often a time is obtained and whether any wrong second is ever labelled.
#include "ahrwwv/decoder.h"
#include "wwvsim.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <cstring>
using namespace ahrwwv;

struct Cond { const char* name; double snr; double fade; bool wwvh; };

int main(int argc, char** argv)
{
    int trials = argc > 1 ? std::atoi(argv[1]) : 20;
    double secs = argc > 2 ? std::atof(argv[2]) : 420;
    Cond conds[] = {
        {"C/N 10 dB steady", 10, 0, false},
        {"C/N 3 dB steady", 3, 0, false},
        {"C/N 0 dB steady", 0, 0, false},
        {"C/N 15 dB, deep fading", 15, 0.9, false},
        {"C/N 8 dB, deep fading", 8, 0.9, false},
        {"C/N 12 dB, WWV+WWVH fading", 12, 0.7, true},
    };
    int totalWrong = 0;
    const char* only = std::getenv("ONLY");
    int verboseTrial = std::getenv("TRIAL") ? std::atoi(std::getenv("TRIAL")) : -1;
    for (auto& c : conds) {
        if (only && !std::strstr(c.name, only)) continue;
        int got = 0, verified = 0, wrongTrials = 0, wrongSecs = 0;
        double tSum = 0;
        for (int k = 0; k < trials; k++) {
            if (verboseTrial >= 0 && k != verboseTrial) continue;
            std::mt19937 rng(1000 + k);
            SimConfig sc;
            sc.sampleRate = 12000;
            sc.startUnix = 1767225600.0 + double(rng() % 31536000) + (rng() % 1000) / 1000.0; // random 2026 time
            sc.snrDb = c.snr; sc.seed = 77 + k;
            sc.st[0].delay = 0.0134; sc.st[0].fadeDepth = c.fade; sc.st[0].fadeRateHz = 0.1 + 0.3 * (rng() % 100) / 100.0;
            sc.dut1Tenths = int(rng() % 15) - 7;
            if (c.wwvh) {
                sc.st[1].enabled = true; sc.st[1].delay = 0.0162; sc.st[1].amplitude = 0.5 + (rng() % 100) / 100.0;
                sc.st[1].fadeDepth = c.fade; sc.st[1].fadeRateHz = 0.25;
            }
            DecoderConfig dc; dc.sampleRate = sc.sampleRate;
            WwvSim sim(sc); WwvDecoder dec(dc);
            std::vector<std::complex<float>> buf;
            int wrong = 0; double tFirst = -1; bool ver = false;
            for (int b = 0; b < int(secs * 10); b++) {
                sim.generate(buf, 1200);
                dec.processIQ(buf.data(), buf.size());
                for (auto& e : dec.takeEvents()) {
                    if (verboseTrial >= 0 && (e.type == DecoderEvent::Type::FrameDecoded || e.type == DecoderEvent::Type::FrameRejected)) {
                        double tt = sc.startUnix + dec.currentSampleTime() - 0.0134;
                        std::printf("  %6.1f %s %s margin %.2f %s struct %d | %s | true %s\n", dec.currentSampleTime(),
                            e.decode.valid ? "OK " : "BAD", formatUnix(double(timeCodeToUnix(e.decode.tc))).c_str(), e.decode.minMargin,
                            e.decode.weakestField.c_str(), e.decode.structureErrors, e.decode.problems.empty() ? "" : e.decode.problems.back().c_str(),
                            formatUnix(tt).c_str());
                    }
                    if (verboseTrial >= 0 && std::getenv("DUMP") && e.type == DecoderEvent::Type::Second) {
                        double tt = sc.startUnix + e.second.boundaryTime - 0.0134;
                        int64_t sec = int64_t(std::llround(tt));
                        TimeCode tc = unixToTimeCode(sec - (sec % 60)); tc.dut1Tenths = sc.dut1Tenths;
                        Frame tf = encodeFrame(tc);
                        Symbol truth = tf[sec % 60];
                        std::printf("    t=%6.1f pos %2d sym %c truth %c conf %.2f lv %.2f %.2f %.2f fade %.2f %s\n", e.second.boundaryTime, e.second.framePosition,
                            symbolChar(e.second.symbol), symbolChar(truth), e.second.confidence, e.second.levels[0], e.second.levels[1], e.second.levels[2],
                            0.0, (e.second.symbol != truth && e.second.symbol != Symbol::Unknown) ? "<<<" : "");
                    }
                    if (e.type == DecoderEvent::Type::Second && e.haveUtc) {
                        double tt = sc.startUnix + e.second.boundaryTime - 0.0134;
                        if (std::fabs(e.utcAtBoundary - tt) > 0.5) wrong++;
                        if (tFirst < 0) tFirst = e.second.boundaryTime;
                    }
                }
                if (dec.status().state == DecoderState::Verified) ver = true;
            }
            if (tFirst >= 0) { got++; tSum += tFirst; }
            if (ver) verified++;
            if (wrong) { wrongTrials++; wrongSecs += wrong; std::printf("    trial %d: %d wrong seconds\n", k, wrong); }
        }
        totalWrong += wrongSecs;
        std::printf("%-30s time obtained %2d/%d (avg %.0f s), verified %2d/%d, trials with a wrong second: %d (%d s)\n",
                    c.name, got, trials, got ? tSum / got : 0.0, verified, trials, wrongTrials, wrongSecs);
        std::fflush(stdout);
    }
    return totalWrong ? 1 : 0;
}
