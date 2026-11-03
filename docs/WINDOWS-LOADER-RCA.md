# Windows SDRangel 8.0.0 plugin-loader diagnosis

## Symptom
The Windows integration run builds `ahrwwvtimedecoder.dll`, and 37 of 38 original checks pass, but the `QPluginLoader` check fails with "The specified procedure could not be found" (0xC0000139).

## Root cause identified by Claude
The CI build invokes only the plugin and integration-test targets, skipping SDRangel's normal `copy_*` deployment targets. Meanwhile, `sdrgui`'s Qt deployment places Qt Multimedia's FFmpeg DLLs into `build/bin`. The CI had manually supplied missing `avfilter-10.dll` and `postproc-58.dll` from SDRangel's pinned FFmpeg set. That mixes two FFmpeg builds with identical DLL names but different private exports.

Probable failing binding:

```
ahrwwvtimedecoder.dll -> sdrgui.dll -> avfilter-10.dll
-> avutil-59.dll (Qt's build): avpriv_fopen_utf8 not exported
```

The diagnostic PE scan on the previous CI run found 0 missing DLLs but reported this specific unresolved import; it also produced unrelated false positives because its search model differed from the Windows loader.

## CI fix
1. Invoke SDRangel's upstream `copy_*` targets *after* the Qt deployment, staging a complete, consistent pinned Windows runtime.
2. Require byte-for-byte SHA-256 matches for each FFmpeg DLL against `sdrangel/external/windows/ffmpeg/bin`.
3. Put `build/bin` ahead of Qt's `bin` in PATH.
4. Test loading `sdrgui.dll` separately, then preserve the original `QPluginLoader` plugin test.
5. Record loader diagnostics and failed-build artifacts without bypassing the integration test.

This document records a root-cause hypothesis independently developed by Claude, not yet Windows-CI verified. Do not distribute or install the experimental DLL until integration tests pass.

## Later build optimization
Cache a version-keyed SDRangel build tree with restored source timestamps, or use sccache, after integration tests pass. Avoid making build-caching changes during the loader fix.
