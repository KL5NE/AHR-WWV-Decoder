# AHR WWV/WWVH — Windows plugin installer plan

Status: **Design only. No installer has been built or validated.** Owner: ChatGPT / Windows packaging. Decoder/DSP owner: Claude. Live-radio acceptance owner: KL5NE. This is a separate planning branch; do not disturb the working decoder/build branch.

## Decision: NSIS plugin-only .exe

Ship an NSIS `.exe` which installs **only** `ahrwwvtimedecoder.dll` into a compatible existing SDRangel installation. SDRangel itself uses CPack/NSIS for its Windows `.exe` distribution. Do **not** replace SDRangel binaries, Qt, FFmpeg, SDR hardware drivers, preferences or unrelated plugins.

First supported target: **official SDRangel v8.0.0, Windows x64**, matching our native MSVC 2022 + Qt 6.9.3 + SDRangel v8.0.0 build. The plugin was compiled against SDRangel's v8.0.0 headers and its 24-bit DSP defaults. Custom SDRangel builds and later releases are unsupported until tested.

An MSI/WiX installer is unnecessary for the initial community release. Reconsider only if enterprise deployment requires it.

Proposed name (placeholder until version assigned):
`AHR-WWV-Decoder-<version>-SDRangel-8.0.0-x64-Setup.exe`.

## Minimum preflight and install behavior

1. Require Windows x64 and an install location with `sdrangel.exe`, `sdrbase.dll`, `sdrgui.dll` and `plugins/`. Support a manually selected installation location if automatic discovery fails.
2. Discover candidates from SDRangel/NSIS-related installer/uninstall registry entries (both registry views / supported hives) and standard application directories. Do not assume `C:\Program Files\SDRangel` or trust registry paths without checking actual files.
3. **Fail closed on host version.** Check official SDRangel installer metadata if present and validate the installed application's actual identity and x64 architecture. The v8.0.0 `sdrgui/resources/sdrangel.rc` only defines an icon, so a PE FileVersion resource on `sdrangel.exe` is not guaranteed. Do not claim that `GetDLLVersion` solves version detection until verified on the official release. Investigate authenticated release fingerprints (e.g. approved binaries' hashes) and/or a verified version-reporting endpoint. Never accept an unsupported host merely because the user clicked through a warning.
4. Verify expected plugin location, architecture, dependency compatibility and checksum of the payload. Do not deploy additional runtime DLLs, particularly no Qt/FFmpeg files.
5. Require SDRangel to close before installing/uninstalling. Do not forcibly terminate a running application or modify its open files.
6. On first installation, copy `ahrwwvtimedecoder.dll` only; record the installed DLL SHA-256, version/build ID, destination path and ownership in the plug-in installer's own uninstaller metadata.
7. On upgrade, replace only a previously recognized AHR plugin. Preserve the previous DLL for rollback; if an unrecognized same-name DLL exists, **abort** instead of silently overwriting it.
8. On copy/verification failure, restore the prior DLL and report a useful actionable error; never leave a partial plugin.
9. Support **uninstall** from Windows Apps / Installed apps. Delete only the exact file installed by AHR when it still matches the recorded hash. If the file changed, do not delete; explain why. Do not remove `plugins/`, SDRangel, other plugins, device drivers, or SDRangel preferences.
10. Produce install/uninstall logs, an easily readable compatibility-error message, a build provenance record and a release SHA-256 manifest. Make privileges explicit; request elevation only to write into a protected installation directory.

## CI/CD workflow after hardware acceptance

The `.github/workflows/windows-plugin.yml` job already builds the DLL and runs Windows integration tests. Extend packaging as a **dependent stage** only after compilation, FFmpeg-hash validation and plugin tests have passed. Fail if any required check fails. Build with pinned NSIS version; attach the unsigned experimental `.exe` and SHA-256 file as CI artifacts.

Do not publish a stable GitHub Release yet. Ship a labelled experimental installer first for controlled testing. Code signing is strongly preferred before public distribution; if unsigned, clearly explain Windows SmartScreen warnings and do not recommend bypassing protections casually.

No RF captures, private test recordings, or station-specific files may be uploaded to the public repository or artifact.

## Acceptance test matrix (must pass before stable release)

| Case | Expected |
| --- | --- |
| Official clean SDRangel 8.0.0 x64 install | Plugin installs and appears in receive-channel list |
| SDRangel version 7.x or later untested release | Installation blocked without changes |
| Non-SDRangel folder or malformed installation | Blocked |
| Custom install path, including spaces | Detected or explicitly selected, correct plugin folder |
| SDRangel actively running | Clean close prompt and no modification until closed |
| Existing recognized AHR plugin | Backed up, upgraded, rollback supported |
| Existing unknown same-name DLL | Blocked; original untouched |
| Installer crash/file copy error/permission denied | Original state preserved or restored |
| Uninstall from Windows Settings | Only owned AHR DLL removed; original SDRangel intact |
| Other SDRangel plugins/preferences/presets | Untouched throughout installation/uninstall |
| DLL corruption/hash mismatch | Blocked and error logged |
| Decoder live test | On-air WWV/WWVH achieves correct DECODED then VERIFIED where signal permits |

The test harness should also test silent install/uninstall **only after** interactive safety checks are verified. Perform clean install/uninstall/upgrade testing on disposable Windows environments rather than HAMBEAST first.

## Open technical verification

- Identify authoritative version metadata or fingerprints for the **actual upstream SDRangel v8.0.0 Windows installer**; do not rely on the icon-only `.rc` resource as version evidence.
- Confirm registry keys and install layouts for default and custom installations.
- Confirm installer privileges and behavior with an x64 application installed in `Program Files`.
- Confirm upgrade/uninstall behavior with a manually installed prior AHR DLL.
- Review GPL/license text and redistribution obligations before publication.
- Record the decoder commit, CI run URL, payload SHA-256 and test matrix results for every candidate installer.

## Gate

**Do not ship until the current fading/joint-minute DLL has passed the next real-radio test on HAMBEAST**. Implementation can be prepared separately, but the installer must package exactly that approved, pinned build, not a moving branch HEAD.
