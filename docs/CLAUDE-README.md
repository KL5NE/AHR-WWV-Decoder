# AHR WWV/WWVH Time Decoder

This project decodes the 100 Hz time code that NIST stations WWV (Fort Collins) and WWVH (Kauai) send on HF, and turns it into a UTC date and time. It is built for SDRangel 8.0.0 on Windows. Alaska HamRadio / KL5NE.

**Status:**

- Milestone 1 (architecture): done.
- Milestone 2 (standalone decoder engine plus tests): done.
- The SDRangel plugin wrapper and GUI (Milestones 3–6) are next.

The decoder only displays the time. It never sets the PC clock.

## What's here

| Path | What |
|---|---|
| `core/` | Decoder engine. Portable C++17 with no Qt. The plugin will link this. |
| `sim/` | WWV/WWVH signal simulator (noise, fading, WWVH overlap, dropouts, voice, sample-clock error) |
| `tests/test_main.cpp` | Unit and scenario tests (81 checks) |
| `tests/stress.cpp` | Monte Carlo stress test: 120 random runs |
| `tools/decode_wav.cpp` | `wwv_decode_wav file.wav`: decode a recording (mono = AM audio, stereo = I/Q) |
| `tools/make_wav.cpp` | `wwv_make_wav out.wav`: make a synthetic test recording |
| `docs/ARCHITECTURE.md` | Milestone 1: SDRangel plugin findings and design |
| `docs/BUILD_BRIEF_CHATGPT.md` | Instructions for the repository/CI side |
| `docs/test-results.txt`, `docs/stress-results.txt`, `docs/real-recordings.txt` | Actual outputs from the runs below |

## Build and test (any OS with CMake and a C++17 compiler)

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## What was actually tested (Linux, g++ 13.3, 2026-11-02)

- **81/81 checks pass.** They cover:
  - The time code encoder against a hand-built NIST SP 250-67 frame.
  - 20,000 random encode/decode round trips.
  - Rejection of corrupted frames.
  - Soft decoding: no false decodes from 500 noise frames.
  - Calendar handling: leap years and day 366.
  - Decoding end to end from IQ and from AM audio at 8, 12, 24, 44.1 and 48 kHz, with sample clocks off by ±40 ppm (measured within 0.5 ppm).
  - Fading, WWV and WWVH on the same frequency, and WWVH alone.
  - A 3-minute dropout: holdover, then recovery.
  - DUT1, leap second warning and DST bits.
  - The 2027 new-year rollover.
  - PC clock offset: measured +250.3 ms against a true +250 ms.
  - 25 minutes of noise or non-time-code AM: no false time.
- **Stress, 120 random runs:**
  - Zero wrong seconds were ever labelled.
  - Steady signal: verified in 20/20 runs down to 0 dB carrier-to-noise (3 kHz bandwidth).
  - Deep Rayleigh-like fading at 15 dB: time obtained in 16/20 runs.
  - At 8 dB with deep fading, the decoder mostly waits rather than guessing (time obtained in 6/20 runs).
- **Timing:** after verification, boundary error is typically under 1 ms against the simulator truth. This is receiver-side only; HF path variation is extra.
- **Real recordings:** the existing 30 s SDRangel captures cannot be decoded. They are shorter than one 60 s frame, and the 100 Hz band is swamped by the ~10 Hz pulsing artifact from the SDR/USB chain. The decoder correctly stays in "Searching" on them.
- **Not yet tested:** live signal in SDRangel, MSVC/Windows compile (ChatGPT's CI), and the plugin/GUI.
