# AHR WWV/WWVH Time Decoder

**Status: project scaffold only.** No working decoder, SDRangel plugin, or Windows installer has been built or tested yet.

A planned **native SDRangel RX channel plugin** to recover NIST WWV/WWVH digital time broadcasts using the **100 Hz audio subcarrier** and modified IRIG-H format. The independent DSP engine should be validated before building the SDRangel user interface.

## Target environment
- Windows 11 x64 and SDRangel **8.0.0** (exact source/toolchain compatibility still to validate)
- SDRplay RSP2pro, WWV/WWVH HF reception
- Decode UTC/date/day-of-year, DUT1, DST/leap-second indicators, confidence and frame bits
- Read-only **estimated** computer-clock comparison; never set Windows time

## Project layout

| Path | Purpose |
| --- | --- |
| `decoder/` | Reusable C++ DSP and frame decoder (pending Claude) |
| `tests/` | Synthetic and real-signal regression tests (pending Claude) |
| `sdrangel-plugin/` | SDRangel native RX channel and GUI (pending Claude) |
| `docs/CLAUDE-HANDOFF.md` | Code-delivery and compatibility requirements |
| `docs/INSTALLER-PLAN.md` | Windows packaging and release criteria |
| `.github/workflows/scaffold-check.yml` | Documentation-only starter CI (not a software build) |

## Milestones
1. **Architecture:** Verify SDRangel 8.0.0 RX plugin implementation, ABI constraints, Windows build toolchain.
2. **Decoder:** Implement and test 100 Hz pulse extraction, framing, and time decoding.
3. **Integration:** Build native plugin against compatible SDRangel source and dependencies.
4. **Validation:** Verify real RF/audio recordings and live SDRplay reception.
5. **Installer:** Reproducible Windows build, test, packaging, SHA-256, clean install/uninstall.
6. **Release:** Publish only after verified results.

A standalone plugin installer must **not** be assumed compatible with a stock SDRangel 8.0.0 binary. A complete SDRangel+plugin installer may be necessary.

## Technical references
- [SDRangel upstream](https://github.com/f4exb/sdrangel)
- [SDRangel Radio Clock example](https://github.com/f4exb/sdrangel/tree/master/plugins/channelrx/radioclock)
- [SDRangel Windows compilation](https://github.com/f4exb/sdrangel/wiki/Compile-in-Windows)
- [NIST WWV](https://www.nist.gov/pml/time-and-frequency-division/time-distribution/radio-station-wwv)

## Licensing and public visibility
This repository is currently **public**. Choose a compatible open-source license and preserve third-party notices before incorporating or distributing any SDRangel-derived source or binaries. Do not commit secrets or private recordings.
