// wwv_make_sdriq: write a synthetic WWV/WWVH I/Q recording in SDRangel's
// .sdriq format, for replay with SDRangel's "File input" device. This lets the
// plugin be exercised inside SDRangel without radio hardware.
//
// Usage: wwv_make_sdriq out.sdriq [--seconds N] [--rate S/s] [--centre Hz]
//                       [--offset Hz] [--snr dB] [--fade depth] [--wwvh] [--start unix]
//
// The WWV carrier is placed at centre + offset. Samples are 16-bit I/Q
// (header sampleSize = 16), which SDRangel 8.0.0 reads in either sample-size build.
#include "wwvsim.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

using namespace ahrwwv;

static const double kPi = 3.14159265358979323846;

static uint32_t crc32(const uint8_t* p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

#pragma pack(push, 1)
struct SdriqHeader {          // matches FileRecord::Header in SDRangel sdrbase/dsp/filerecord.h
    uint32_t sampleRate;
    uint64_t centerFrequency;
    uint64_t startTimeStamp;  // ms since the Unix epoch
    uint32_t sampleSize;      // 16 or 24
    uint32_t filler;
    uint32_t crc32;           // CRC-32 of the first 28 bytes
};
#pragma pack(pop)
static_assert(sizeof(SdriqHeader) == 32, "sdriq header must be 32 bytes");

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s out.sdriq [--seconds N] [--rate S/s] [--centre Hz] [--offset Hz] [--snr dB] [--fade depth] [--wwvh] [--start unix]\n", argv[0]);
        return 2;
    }
    SimConfig sc;
    double secs = 240, centre = 10000000, offset = 0;
    sc.sampleRate = 48000;
    sc.startUnix = std::floor(double(std::time(nullptr)));
    sc.snrDb = 25;
    sc.st[0].delay = 0.0134;
    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::atof(argv[++i]) : 0.0; };
        if (a == "--seconds") secs = next();
        else if (a == "--rate") sc.sampleRate = next();
        else if (a == "--centre" || a == "--center") centre = next();
        else if (a == "--offset") offset = next();
        else if (a == "--snr") sc.snrDb = next();
        else if (a == "--fade") { sc.st[0].fadeDepth = next(); sc.st[1].fadeDepth = sc.st[0].fadeDepth; }
        else if (a == "--wwvh") { sc.st[1].enabled = true; sc.st[1].amplitude = 0.7; }
        else if (a == "--start") sc.startUnix = next();
    }
    FILE* f = std::fopen(argv[1], "wb");
    if (!f) { std::fprintf(stderr, "cannot write %s\n", argv[1]); return 1; }
    SdriqHeader h{};
    h.sampleRate = uint32_t(sc.sampleRate);
    h.centerFrequency = uint64_t(centre);
    h.startTimeStamp = uint64_t(sc.startUnix * 1000.0);
    h.sampleSize = 16;
    h.filler = 0;
    h.crc32 = crc32(reinterpret_cast<const uint8_t*>(&h), 28);
    std::fwrite(&h, sizeof h, 1, f);

    WwvSim sim(sc);
    std::vector<std::complex<float>> buf;
    std::vector<int16_t> out;
    size_t total = size_t(secs * sc.sampleRate);
    double ph = 0, dph = 2 * kPi * offset / sc.sampleRate;
    while (sim.samplesGenerated() < total) {
        sim.generate(buf, std::min<size_t>(size_t(sc.sampleRate), total - sim.samplesGenerated()));
        out.resize(2 * buf.size());
        for (size_t i = 0; i < buf.size(); i++) {
            std::complex<double> z(buf[i].real(), buf[i].imag());
            z *= std::polar(1.0, ph);
            ph += dph;
            if (ph > 2 * kPi) ph -= 2 * kPi;
            double s = 0.15 * 32767.0;
            out[2 * i] = int16_t(std::lround(std::max(-32767.0, std::min(32767.0, z.real() * s))));
            out[2 * i + 1] = int16_t(std::lround(std::max(-32767.0, std::min(32767.0, z.imag() * s))));
        }
        std::fwrite(out.data(), sizeof(int16_t), out.size(), f);
    }
    std::fclose(f);
    std::printf("wrote %s: %.0f s at %.0f S/s, centre %.0f Hz, carrier at %+.0f Hz, C/N %.0f dB, starts %s UTC\n",
                argv[1], secs, sc.sampleRate, centre, offset, sc.snrDb, formatUnix(sc.startUnix).c_str());
    return 0;
}
