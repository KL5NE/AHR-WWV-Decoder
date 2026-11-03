// Minimal WAV reader/writer for the test tools (PCM 16-bit and 32-bit float).
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct Wav {
    int channels = 1;
    int sampleRate = 48000;
    std::vector<float> data; // interleaved, -1..1
};

inline bool readWav(const std::string& path, Wav& w, std::string& err)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { err = "cannot open"; return false; }
    char riff[12];
    if (std::fread(riff, 1, 12, f) != 12 || std::memcmp(riff, "RIFF", 4) || std::memcmp(riff + 8, "WAVE", 4)) {
        err = "not a RIFF/WAVE file"; std::fclose(f); return false;
    }
    int fmt = 0, bits = 0;
    bool haveFmt = false;
    for (;;) {
        char id[4]; uint32_t len;
        if (std::fread(id, 1, 4, f) != 4 || std::fread(&len, 4, 1, f) != 1) break;
        if (!std::memcmp(id, "fmt ", 4)) {
            std::vector<uint8_t> b(len);
            if (std::fread(b.data(), 1, len, f) != len) break;
            fmt = b[0] | (b[1] << 8);
            w.channels = b[2] | (b[3] << 8);
            w.sampleRate = int(b[4] | (b[5] << 8) | (b[6] << 16) | (uint32_t(b[7]) << 24));
            bits = b[14] | (b[15] << 8);
            if (fmt == 0xFFFE && len >= 26) fmt = b[24] | (b[25] << 8); // extensible
            haveFmt = true;
        } else if (!std::memcmp(id, "data", 4)) {
            if (!haveFmt) break;
            std::vector<uint8_t> b(len);
            size_t got = std::fread(b.data(), 1, len, f);
            if (fmt == 1 && bits == 16) {
                w.data.resize(got / 2);
                for (size_t i = 0; i < w.data.size(); i++) { int16_t v; std::memcpy(&v, &b[2 * i], 2); w.data[i] = v / 32768.0f; }
            } else if (fmt == 3 && bits == 32) {
                w.data.resize(got / 4);
                std::memcpy(w.data.data(), b.data(), w.data.size() * 4);
            } else { err = "unsupported format (need 16-bit PCM or 32-bit float)"; std::fclose(f); return false; }
            std::fclose(f);
            return true;
        } else {
            std::fseek(f, long(len + (len & 1)), SEEK_CUR);
        }
    }
    err = "no data chunk";
    std::fclose(f);
    return false;
}

inline bool writeWav16(const std::string& path, const Wav& w)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    uint32_t dataLen = uint32_t(w.data.size() * 2);
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + dataLen); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(uint16_t(w.channels)); u32(uint32_t(w.sampleRate));
    u32(uint32_t(w.sampleRate * w.channels * 2)); u16(uint16_t(w.channels * 2)); u16(16);
    std::fwrite("data", 1, 4, f); u32(dataLen);
    for (float x : w.data) {
        float c = x > 1 ? 1 : x < -1 ? -1 : x;
        int16_t v = int16_t(c * 32767.0f);
        std::fwrite(&v, 2, 1, f);
    }
    std::fclose(f);
    return true;
}
