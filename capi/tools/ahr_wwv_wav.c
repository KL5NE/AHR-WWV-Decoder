/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 KL5NE, Alaska HamRadio, and AHR-Decoder contributors */
/* ahr_wwv_wav: run the AHR-Decoder C API over a mono WAV of rig audio.
 * Also a minimal example of the API for host developers (plain C99).
 *
 * usage: ahr_wwv_wav file.wav [--carrier HZ] [--start-unix T] [--am]
 *   --carrier     tone frequency of the WWV carrier in the audio (default 1500)
 *   --start-unix  PC clock time of the first sample, to report a clock offset
 *   --am          the audio is already AM (envelope) demodulated
 * Reads 16-bit PCM or 32-bit float WAV (first channel if more than one).
 */
#include "ahr_wwv.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const unsigned char *p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const unsigned char *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

int main(int argc, char **argv)
{
    const char *path = NULL;
    double carrier = 1500.0, start = NAN;
    int am = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--carrier") && i + 1 < argc) carrier = atof(argv[++i]);
        else if (!strcmp(argv[i], "--start-unix") && i + 1 < argc) start = atof(argv[++i]);
        else if (!strcmp(argv[i], "--am")) am = 1;
        else path = argv[i];
    }
    if (!path) { fprintf(stderr, "usage: %s file.wav [--carrier HZ] [--start-unix T] [--am]\n", argv[0]); return 2; }
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 1; }

    /* Minimal RIFF/WAVE parser */
    unsigned char hdr[12], ch[8];
    uint16_t fmtTag = 0, chans = 0, bits = 0;
    uint32_t rate = 0, dataLen = 0;
    if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) { fprintf(stderr, "not a WAV\n"); return 1; }
    for (;;) {
        if (fread(ch, 1, 8, f) != 8) { fprintf(stderr, "no data chunk\n"); return 1; }
        uint32_t len = rd32(ch + 4);
        if (!memcmp(ch, "fmt ", 4)) {
            unsigned char b[40] = {0};
            if (len > sizeof b || fread(b, 1, len, f) != len) { fprintf(stderr, "bad fmt\n"); return 1; }
            fmtTag = rd16(b); chans = rd16(b + 2); rate = rd32(b + 4); bits = rd16(b + 14);
            if (fmtTag == 0xFFFE && len >= 26) fmtTag = rd16(b + 24); /* WAVE_FORMAT_EXTENSIBLE */
        } else if (!memcmp(ch, "data", 4)) { dataLen = len; break; }
        else fseek(f, (long)(len + (len & 1)), SEEK_CUR);
    }
    if (!((fmtTag == 1 && bits == 16) || (fmtTag == 3 && bits == 32)) || !chans) { fprintf(stderr, "need 16-bit PCM or float32\n"); return 1; }
    printf("%s: %u Hz, %u ch, %s, %.1f s\n", path, rate, chans, fmtTag == 3 ? "float" : "16-bit",
           (double)dataLen / (bits / 8) / chans / rate);

    ahr_wwv_config cfg;
    ahr_wwv_config_default(&cfg);
    cfg.input = am ? AHR_WWV_INPUT_AM_AUDIO : AHR_WWV_INPUT_SSB_AUDIO;
    cfg.sample_rate_hz = rate;
    cfg.carrier_hz = carrier;
    ahr_wwv_handle *h = ahr_wwv_create(&cfg);
    if (!h) { fprintf(stderr, "ahr_wwv_create rejected the configuration\n"); return 1; }

    const size_t block = rate / 10; /* 100 ms, as a sound card callback might deliver */
    const size_t frameBytes = (size_t)(bits / 8) * chans;
    unsigned char *raw = malloc(block * frameBytes);
    float *mono = malloc(block * sizeof(float));
    uint64_t done = 0;
    int lastState = -1;
    ahr_wwv_status st;
    while (raw && mono) {
        size_t got = fread(raw, frameBytes, block, f);
        if (!got) break;
        for (size_t i = 0; i < got; i++) {
            const unsigned char *p = raw + i * frameBytes;
            if (fmtTag == 3) { float v; memcpy(&v, p, 4); mono[i] = v; }
            else mono[i] = (int16_t)rd16(p) / 32768.0f;
        }
        double ts = isnan(start) ? NAN : start + (double)done / rate;
        ahr_wwv_push_audio(h, mono, got, ts);
        done += got;
        st.struct_size = sizeof st;
        ahr_wwv_poll(h, &st);
        if (st.state != lastState) {
            printf("%8.1f s  %s", (double)done / rate, ahr_wwv_state_name(st.state));
            if (st.candidate_valid) printf("  (candidate %04d day %03d %02d:%02d UTC)", st.year, st.day_of_year, st.hour, st.minute);
            if (st.utc_valid) printf("  %04d day %03d %02d:%02d UTC", st.year, st.day_of_year, st.hour, st.minute);
            printf("\n");
            lastState = st.state;
        }
    }
    printf("final: %s, station %s, SNR %.1f dB, jitter %.1f ms, frames verified %d\n", ahr_wwv_state_name(st.state),
           ahr_wwv_station_name(st.station), st.subcarrier_snr_db, st.timing_jitter_s * 1e3, st.frames_verified);
    if (st.utc_valid) printf("UTC at end: %.3f (+/- %.1f ms)\n", st.utc_unix_s, st.time_uncertainty_s * 1e3);
    if (st.flags_confirmed) printf("DUT1 %+.1f s, leap warning %d, DST %d%d\n", st.dut1_tenths / 10.0, st.leap_second_warning, st.dst1, st.dst2);
    if (st.clock_offset_valid) printf("PC clock offset %+.4f s (+/- %.1f ms)\n", st.clock_offset_s, st.clock_offset_uncertainty_s * 1e3);
    printf("sample clock %+.2f +/- %.2f ppm (%s)\n", st.sample_clock_ppm, st.sample_clock_ppm_sigma, st.sample_clock_settled ? "settled" : "measuring");
    ahr_wwv_destroy(h);
    free(raw); free(mono); fclose(f);
    return 0;
}
