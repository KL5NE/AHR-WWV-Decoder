# Claude development handoff

Repository: https://github.com/KL5NE/AHR-WWV-Decoder

**Status:** Initial scaffold, not working decoder software. Repository is currently **public**; do not push secrets, unlicensed third-party code, raw private captures, or binaries without release review.

Suggested code areas:
- `decoder/` — modular WWV/WWVH 100 Hz subcarrier processing and modified IRIG-H time-frame decoding.
- `tests/` — synthetic NIST-compliant frames, invalid cases, and reproducible regression tests.
- `sdrangel-plugin/` — SDRangel native RX channel and user interface.
- `docs/INSTALLER-PLAN.md` — Windows compatibility and release requirements.

Target: Windows 11 x64, SDRangel 8.0.0, SDRplay RSP2pro.
Clock comparison is read-only; NEVER set the user's system clock.
Verify whether native RX plugin installation requires a matching custom SDRangel build; do not assume drop-in DLL ABI compatibility.

**For release engineering, supply:**
1. Exact source branch/commit, dependencies, toolchain, Qt version, SDRangel version/commit.
2. Reproducible Windows build commands.
3. DLL and executable output paths, packaging method, CMake/CPack configuration.
4. Test vectors, test command, recorded signal validation and results.
5. Any redistributed dependency DLLs and their licenses.
6. Installer/upgrade/uninstall behavior and clean-machine validation.

Do not assert an installer or end-to-end integration works unless it has been built and tested.
