// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 KL5NE, Alaska HamRadio, and AHR-Decoder contributors
// wwv_make_wav: write a synthetic WWV/WWVH recording for testing.
// Usage: wwv_make_wav out.wav [--iq] [--seconds N] [--snr dB] [--fade depth] [--wwvh] [--rate Hz]
#include "wavio.h"
#include "wwvsim.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
using namespace ahrwwv;

int main(int argc, char** argv)
{
    if (argc < 2) { std::fprintf(stderr, "usage: %s out.wav [--iq] [--seconds N] [--snr dB] [--fade depth] [--wwvh] [--rate Hz]\n", argv[0]); return 2; }
    SimConfig sc;
    bool iq = false;
    double secs = 180;
    sc.sampleRate = 48000;
    sc.startUnix = double(std::time(nullptr));
    sc.snrDb = 25;
    sc.st[0].delay = 0.0134;
    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::atof(argv[++i]) : 0.0; };
        if (a == "--iq") iq = true;
        else if (a == "--seconds") secs = next();
        else if (a == "--snr") sc.snrDb = next();
        else if (a == "--fade") { sc.st[0].fadeDepth = next(); sc.st[1].fadeDepth = sc.st[0].fadeDepth; }
        else if (a == "--wwvh") { sc.st[1].enabled = true; sc.st[1].amplitude = 0.7; }
        else if (a == "--rate") sc.sampleRate = next();
        else if (a == "--start") sc.startUnix = next();
    }
    WwvSim sim(sc);
    std::vector<std::complex<float>> buf;
    Wav w;
    w.sampleRate = int(sc.sampleRate);
    w.channels = iq ? 2 : 1;
    size_t total = size_t(secs * sc.sampleRate);
    while (sim.samplesGenerated() < total) {
        sim.generate(buf, std::min<size_t>(size_t(sc.sampleRate), total - sim.samplesGenerated()));
        for (auto& z : buf) {
            if (iq) { w.data.push_back(0.4f * z.real()); w.data.push_back(0.4f * z.imag()); }
            else w.data.push_back(0.3f * (std::abs(z) - 1.0f));
        }
    }
    if (!writeWav16(argv[1], w)) { std::fprintf(stderr, "cannot write %s\n", argv[1]); return 1; }
    std::printf("wrote %s: %.0f s, %s, %d Hz, starts %s UTC\n", argv[1], secs, iq ? "I/Q" : "AM audio", w.sampleRate,
                formatUnix(sc.startUnix).c_str());
    return 0;
}
