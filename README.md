# AHR-Decoder — WWV/WWVH Time Decoder

**Current status: 0.9.2-beta has passed Windows SDRangel 8.0.0 CI and live HAMBEAST acceptance (12 Nov 2026 UTC).** The held-over estimated clock is deliberately hidden until VERIFIED, and the software never sets the system clock.

**[0.9.2-beta Windows installation and operations guide](docs/AHR-DECODER-0.9.2-USER-GUIDE.md)** · [HAMBEAST acceptance results](docs/HAMBEAST-WINDOWS-TEST.md) · [Validated installer build / artifact](https://github.com/KL5NE/AHR-WWV-Decoder/actions/runs/43119319819) · [Source code](https://github.com/KL5NE/AHR-WWV-Decoder)

**Supported beta host:** official SDRangel **8.0.0 x64 on Windows 11**; other versions aren't validated. The DLL is available only as an unsigned beta and should be installed with the official AHR installer where practical.

**Validated 0.9.2-beta DLL SHA-256:** `87e8b179e38f1f0bada44d5f0e6e66ed9dc4a05dd5068479f6fd4a3da65cc9b7`; Windows plugin run `43104513833`; core `43104513923`; C API `43104514184`; installer `43119319819`.

**Live test:** HAMBEAST (Windows 11, SDRangel 8.0.0, SDRplay RSP2pro) had three successful 10 MHz WWV/WWVH VERIFIED acquisitions. During signal-loss HOLDOVER the UTC clock and PC offset remained blank; live reception re-verified after recovery. The automated Windows installation suite passed nine cases; interactive GUI launch was subsequently confirmed on HAMBEAST.

## Project layout

- `core/`: portable C++17 decoding engine and tests; this is the reusable technology for applications including Nexus.
- `plugin/`: SDRangel receive-channel GUI and integration tests; **this DLL is specific to SDRangel**, not a Nexus DLL.
- `packaging/`: Windows installer, safe upgrade/uninstall scripts and VM acceptance.
- `docs/`: architecture, acceptance evidence, install/operations guide, engineering references.

The software is open-source, experimental, and intended for community use, modification and contributions under applicable repository and upstream licenses. File issues at [GitHub](https://github.com/KL5NE/AHR-WWV-Decoder/issues). Neither NIST nor SDRangel endorses this independent amateur-radio project.

**73 de KL5NE — Alaska Ham Radio**
