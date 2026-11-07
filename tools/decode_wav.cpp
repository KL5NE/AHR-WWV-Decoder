// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 KL5NE, Alaska HamRadio, and AHR-Decoder contributors
// wwv_decode_wav: run the decoder over a WAV recording and print what it sees.
//   mono WAV   = AM-demodulated audio (e.g. SDRangel AM demod recording)
//   stereo WAV = complex I/Q with the WWV carrier near 0 Hz
// Usage: wwv_decode_wav file.wav [--start "YYYY-MM-DD HH:MM:SS"] [--quiet]
#include "ahrwwv/decoder.h"
#include "wavio.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
using namespace ahrwwv;

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s file.wav [--quiet]\n", argv[0]);
        return 2;
    }
    bool quiet = false;
    for (int i = 2; i < argc; i++) if (!std::strcmp(argv[i], "--quiet")) quiet = true;
    Wav w;
    std::string err;
    if (!readWav(argv[1], w, err)) { std::fprintf(stderr, "%s: %s\n", argv[1], err.c_str()); return 1; }
    bool iq = w.channels == 2;
    size_t frames = w.data.size() / w.channels;
    std::printf("%s: %d Hz, %s, %.1f s\n", argv[1], w.sampleRate, iq ? "I/Q (stereo)" : "audio (mono)", double(frames) / w.sampleRate);
    DecoderConfig cfg;
    cfg.sampleRate = w.sampleRate;
    WwvDecoder dec(cfg);
    const size_t block = size_t(w.sampleRate / 10);
    std::string line;
    for (size_t pos = 0; pos < frames; pos += block) {
        size_t n = std::min(block, frames - pos);
        if (iq) {
            std::vector<std::complex<float>> b(n);
            for (size_t i = 0; i < n; i++) b[i] = {w.data[2 * (pos + i)], w.data[2 * (pos + i) + 1]};
            dec.processIQ(b.data(), n);
        } else {
            dec.processAudio(&w.data[pos], n);
        }
        for (auto& e : dec.takeEvents()) {
            switch (e.type) {
            case DecoderEvent::Type::StateChanged:
                std::printf("%7.2f s  state -> %s\n", dec.currentSampleTime(), decoderStateName(e.state));
                break;
            case DecoderEvent::Type::Second:
                if (!quiet)
                    std::printf("%7.2f s  second %2d  %c  conf %.2f  levels %5.2f %5.2f %5.2f%s%s\n", e.second.boundaryTime,
                                e.second.framePosition, symbolChar(e.second.symbol), e.second.confidence, e.second.levels[0],
                                e.second.levels[1], e.second.levels[2], e.haveUtc ? "  UTC " : "",
                                e.haveUtc ? formatUnix(e.utcAtBoundary, false).c_str() : "");
                line += symbolChar(e.second.symbol);
                break;
            case DecoderEvent::Type::FrameDecoded:
            case DecoderEvent::Type::FrameRejected: {
                const DecodeResult& r = e.decode;
                int mon, day;
                dayOfYearToDate(r.tc.year, r.tc.dayOfYear, mon, day);
                std::printf("%7.2f s  FRAME %s  %04d-%02d-%02d (day %03d) %02d:%02d UTC  DUT1 %+.1f  LSW %d  %s  weakest %s %.2f  frames %d%s%s\n",
                            dec.currentSampleTime(), r.valid ? "accepted" : "rejected", r.tc.year, mon, day, r.tc.dayOfYear,
                            r.tc.hour, r.tc.minute, r.tc.dut1Tenths / 10.0, r.tc.leapSecondWarning,
                            dstStateName(dstState(r.tc)), r.weakestField.c_str(), r.minMargin, r.framesUsed,
                            r.problems.empty() ? "" : "  : ", r.problems.empty() ? "" : r.problems.back().c_str());
                break;
            }
            }
        }
    }
    const DecoderStatus& s = dec.status();
    std::printf("\nsymbols: %s\n", line.c_str());
    std::printf("final state %s | 100 Hz SNR %.1f dB | second lock quality %.2f | edge jitter %.2f ms | station %s (tick 1000 %.2f, 1200 %.2f)\n",
                decoderStateName(s.state), s.subcarrierSnrDb, s.secondLockQuality, s.timingJitter * 1e3,
                stationName(s.station), s.tick1000, s.tick1200);
    if (s.sampleClockPpmValid)
        std::printf("sample clock %+.2f +/- %.2f ppm (%s)\n", s.sampleClockPpmMeasured, s.sampleClockPpmSigma,
                    s.sampleClockPpmSettled ? "settled" : "measuring");
    double u;
    if (dec.currentUtc(u))
        std::printf("UTC at end of file (transmitter time): %s  +/- %.1f ms\n", formatUnix(u).c_str(), s.timeUncertainty * 1e3);
    return 0;
}
