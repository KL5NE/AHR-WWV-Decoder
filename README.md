# AHR WWV/WWVH Time Decoder

**Current status: Milestone 3 source delivered — native SDRangel 8.0.0 receive-channel plugin (`plugin/`). Built, loaded and decoding inside SDRangel 8.0.0 on Linux with replayed synthetic I/Q; Windows/MSVC build, stock-install load test and live-radio test still pending. See [docs/M3-PLUGIN.md](docs/M3-PLUGIN.md).**

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
| `tools/` | WAV-file decoder, synthetic WAV and SDRangel `.sdriq` generators |
| `plugin/` | SDRangel 8.0.0 receive-channel plugin: DSP, Qt GUI, integration tests |
| `sdrangel-integration/` | One-line hook that adds the plugin to an SDRangel v8.0.0 source tree |
| `docs/M3-PLUGIN.md` | Plugin build, install, test procedure, drop-in DLL analysis, limitations |
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

## SDRangel plugin (Milestone 3)

The plugin is built inside an SDRangel **v8.0.0** source tree (exact toolchain matters for a drop-in DLL):

```shell
git clone --branch v8.0.0 --depth 1 --recurse-submodules https://github.com/f4exb/sdrangel.git
cmake -DSDRANGEL_SOURCE_DIR=sdrangel -P AHR-WWV-Decoder/sdrangel-integration/apply_integration.cmake
cmake -S sdrangel -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_QT6=ON <official flags> \
      -DAHRWWV_PLUGIN_DIR=<absolute path>/AHR-WWV-Decoder/plugin -DAHRWWV_BUILD_PLUGIN_TESTS=ON
cmake --build build --target ahrwwvtimedecoder ahrwwv_plugin_tests
```

Full Windows instructions, outputs and the test procedure are in [docs/M3-PLUGIN.md](docs/M3-PLUGIN.md); user guide in [plugin/readme.md](plugin/readme.md).

Remaining: MSVC build in CI, load test in a stock Windows SDRangel 8.0.0 (decides drop-in DLL vs full package), live WWV reception on HAMBEAST.

**Clock operation:** decoded UTC display and estimated PC-clock offset only. Never set or adjust the operating-system clock.

## References and license notes

- [SDRangel upstream](https://github.com/f4exb/sdrangel)
- [SDRangel Radio Clock example](https://github.com/f4exb/sdrangel/tree/master/plugins/channelrx/radioclock)
- [NIST WWV overview](https://www.nist.gov/pml/time-and-frequency-division/time-distribution/radio-station-wwv)

This repository is **public**. Before distributing derived SDRangel source, binaries, or an installer, confirm the appropriate license and dependency notices. Do not commit secrets or private RF recordings.
