# Windows Installer / GitHub Release Plan

## Current status
**No decoder or installer has been built.** A repository scaffold and lightweight documentation CI are in place only.

## Preferred packaging path
Use the SDRangel Windows build toolchain, CMake/CPack and NSIS as supported by the *matching* SDRangel source revision:
https://github.com/f4exb/sdrangel/wiki/Compile-in-Windows

A full customized SDRangel installer may be safer than a generic plugin-only installer because native plugins can depend on the exact Qt ABI, MSVC toolchain, and SDRangel build revision.

## Plugin-only installer alternative
Consider only after proving that the plugin DLL loads into a pristine, unmodified Windows x64 SDRangel 8.0.0 installation. Verify plugin search path, transitive dependencies and architecture. Detect incompatible installs, never overwrite unrelated files, and implement uninstall safely.

## Build pipeline requirements — after Claude delivers compilable source
1. Pin exact source revisions and dependency versions.
2. Create a reproducible Windows build/test job with versioned runner and toolchain.
3. Compile decoder/tests and run tests; reject failures.
4. Package with upstream CPack/NSIS when appropriate, or a validated plugin-only installer.
5. Upload the generated installer and SHA-256 checksum as CI artifacts.
6. Publish tagged releases only after a tested installer is approved.
7. Verify fresh install, uninstall, receiver function, and live time-frame decoding on Windows 11.

## Inputs required from Claude
- Source and tested build command, plugin integration location, exact SDRangel source and Qt/MSVC versions
- Test suite and results, executable/DLL output paths, installer generation command
- Full dependency and license disclosures; whether runtime requires full SDRangel rebuild

This document is a plan, **not a claim of installer functionality**.
