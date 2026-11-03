# AHR WWV/WWVH Time Decoder

**Current status: Milestone 2 complete — portable C++17 decoder core and automated tests. The native SDRangel plugin and Windows installer are not built yet.**

An Alaska HamRadio (AHR / KL5NE) project to decode the NIST WWV/WWVH 100 Hz audio subcarrier and its modified IRIG-H time-code frames.

## Tested portable decoder

Source was supplied by Claude and independently built and tested on Linux with GCC 14.2, CMake and Ninja. GitHub Actions has **successfully built and run the CTest suite on both Linux and Windows (MSVC 2022)**:

- [Verified Linux + Windows build / test run](https://github.com/KL5NE/AHR-WWV-Decoder/actions/runs/41655659226)
- [Merged Milestone 2 pull request](https://github.com/KL5NE/AHR-WWV-Decoder/pull/2)
- [Claude's detailed test report](docs/test-results.txt) and [stress results](docs/stress-results.txt)

These results validate the portable core, not native SDRangel integration or reception from a live radio.

## Contents

| Path | Purpose |
| --- | --- |
| `core/` | Portable, platform-independent C++17 WWV/WWVH decoder engine |
| `sim/` | Signal simulator for repeatable DSP tests |
| `tests/` | C++ unit and stress tests |
| `tools/` | WAV-file decoder and synthetic WAV generator |
| `docs/ARCHITECTURE.md` | SDRangel 8.0.0 plugin design and integration notes |
| `docs/BUILD_BRIEF_CHATGPT.md` | Claude's build and packaging handoff |
| `docs/INSTALLER-PLAN.md` | Planned Windows packaging and compatibility checks |
| `.github/workflows/core-ci.yml` | Linux/Windows compile and CTest jobs |

## Build and test locally

```shell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Optional stress test:

```shell
./build/ahrwwv_stress 10 420
```

On Windows use `build\\ahrwwv_stress.exe`.

## Next: Milestone 3

Build the actual **native SDRangel 8.0.0 receive-channel plugin and GUI**, validate loadability on a Windows 11 x64 stock installation with SDRplay RSP2pro, and then establish an installer workflow. A drop-in DLL may depend on the precise MSVC/Qt/SDRangel binary ABI; a full bundled SDRangel installer might be required.

**Clock operation:** decoded UTC display and estimated PC-clock offset only. Never set or adjust the operating-system clock.

## References and license notes

- [SDRangel upstream](https://github.com/f4exb/sdrangel)
- [SDRangel Radio Clock example](https://github.com/f4exb/sdrangel/tree/master/plugins/channelrx/radioclock)
- [NIST WWV overview](https://www.nist.gov/pml/time-and-frequency-division/time-distribution/radio-station-wwv)

This repository is **public**. Before distributing derived SDRangel source, binaries, or an installer, confirm the appropriate license and dependency notices. Do not commit secrets or private RF recordings.
