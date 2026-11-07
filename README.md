<div align="center">

# AHR-Decoder
### Atomic time from the airwaves — built and tested in Alaska

**Decode NIST WWV / WWVH time signals with software-defined radio.**

[![Core CI](https://github.com/KL5NE/AHR-WWV-Decoder/actions/workflows/core-ci.yml/badge.svg)](https://github.com/KL5NE/AHR-WWV-Decoder/actions/workflows/core-ci.yml)
[![Scaffold checks](https://github.com/KL5NE/AHR-WWV-Decoder/actions/workflows/scaffold-check.yml/badge.svg)](https://github.com/KL5NE/AHR-WWV-Decoder/actions/workflows/scaffold-check.yml)

[Alaska Ham Radio](https://alaskahamradio.com) · [AHR Insights](https://alaskahamradio.com/ahr-insights/) · [Issues](https://github.com/KL5NE/AHR-WWV-Decoder/issues) · [Development PRs](https://github.com/KL5NE/AHR-WWV-Decoder/pulls)

</div>

---

## What we're building

**AHR-Decoder** is an experimental open-source time-signal decoder from **Alaska Ham Radio (KL5NE)**. The project explores decoding WWV/WWVH broadcast time information with an SDR, including the challenges presented by fading and high-latitude radio propagation.

The work includes a portable C++ decoder core and experimental native SDRangel integration, with a separately developed interface for possible integration into [Nexus](https://github.com/KL5NE/Nexus).

> **Release status:** This is active engineering work, not a supported public installer. The `main` branch is the tested portable-core baseline. SDRangel plugin, C API, fading improvements, and Windows installer work live on development branches and draft pull requests until reviewed and deliberately integrated. A green CI run alone is not certification for installation on your machine.

## Capabilities and roadmap

| Area | Status | Where to follow |
| --- | --- | --- |
| Portable C++17 WWV/WWVH decoder and synthetic test signals | Tested baseline on `main` | [Core CI](https://github.com/KL5NE/AHR-WWV-Decoder/actions/workflows/core-ci.yml) |
| High-latitude fading tolerance and station identification | Experimental, tested in development | [Pull requests](https://github.com/KL5NE/AHR-WWV-Decoder/pulls) |
| Native SDRangel 8.0.0 receive-channel plugin and GUI | Development builds; not released | [Plugin work](https://github.com/KL5NE/AHR-WWV-Decoder/pull/4) |
| C API for embedding the decoder in other programs | Development branch and checks | [C1 integration](https://github.com/KL5NE/AHR-WWV-Decoder/pull/11) |
| Windows plugin-only beta installer | CI packaging and VM validation in progress; **not approved for general installation** | [Installer review](https://github.com/KL5NE/AHR-WWV-Decoder/pull/12) |
| Clock synchronization | **Display/measurement only**; no automatic system-clock changes | Design and test documentation |

## Why Alaska matters

Skywave propagation at high latitudes brings fast fading, auroral effects, and polar-path complications. We use these conditions as a demanding real-world test environment rather than assuming signals will look like clean lab waveforms.

The engineering goal is traceable, testable decoding: clear confidence indicators, reproducible recordings, and careful separation between a decoded time estimate and anything that might alter the computer's clock.

## Building the portable core

For developers with CMake, a C++17 toolchain, and Ninja:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Windows builds can use MSVC; see the CI workflow for exact tested tooling. **These commands build the portable core, not a production SDRangel installer.**

## Project map

- `core/` — platform-independent decoding logic.
- `sim/` and `tests/` — synthetic signals, regression and stress tests.
- `tools/` — WAV and signal utilities.
- `docs/` — architecture notes, experiments, CI diagnostics and build handoffs.
- Development branches / pull requests — experimental plugin, C API, and installer work.

## Safety, licenses, and contributions

We welcome technically grounded bug reports, reproducible sample descriptions, and proposed tests via [Issues](https://github.com/KL5NE/AHR-WWV-Decoder/issues). Please **do not post private recordings, secrets, personal information, or unlicensed third-party content**.

Before redistribution of SDRangel-derived components or binaries, validate the applicable upstream license and dependency notices. **Do not treat draft CI artifacts as official releases.** We do not automatically set or adjust your PC clock.

## Part of Alaska Ham Radio

[AlaskaHamRadio.com](https://alaskahamradio.com) is the home of AHR field reports, experimentation, operating stories, and technical articles from Alaska. Explore related experiments in [KL5NE's public repositories](https://github.com/KL5NE?tab=repositories).

**73 de KL5NE · Palmer, Alaska**

---

<sub>NIST and WWV/WWVH are referenced for technical interoperability; this independent amateur-radio project is not affiliated with or endorsed by NIST. [NIST WWV reference](https://www.nist.gov/pml/time-and-frequency-division/time-distribution/radio-station-wwv).</sub>
