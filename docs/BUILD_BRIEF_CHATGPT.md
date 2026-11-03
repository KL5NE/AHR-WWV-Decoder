# Build brief for ChatGPT (repository and Windows CI)

Hand this file to ChatGPT. Claude writes the code; ChatGPT owns the repository, the GitHub Actions workflow and the Windows packaging.

## Scope now (Milestone 2)

1. Create a new **separate** repository for this project. Do not fork or modify SDRangel or any of Sean's existing repositories.
2. Commit the contents of this folder as they are.
3. Add CI that builds and runs the portable core tests on:
   - `ubuntu-latest`: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build --output-on-failure`
   - `windows-latest` (MSVC 2022), the same commands. This proves the engine compiles and passes with MSVC before any SDRangel work.
   - Optional: run `build/ahrwwv_stress 10 420`. It exits non-zero if any wrong second is ever labelled.
4. Upload `ahrwwv_tests` output as an artifact.

## Scope later (Milestone 3, once Claude delivers `plugin/`)

Windows job that builds the SDRangel plugin DLL against the exact 8.0.0 toolchain:

- Check out `f4exb/sdrangel` at tag `v8.0.0`.
- Copy the following into `sdrangel/plugins/channelrx/ahrwwv/`:
  - `plugin/*`
  - `core/include`
  - `core/src`
- Append to `sdrangel/plugins/channelrx/CMakeLists.txt`:
  `add_subdirectory(ahrwwv)`
- Mirror `.github/workflows/windows.yml` from the 8.0.0 tag:
  - Qt 6.9.3 `win64_msvc2022_64` via `jurplel/install-qt-action@v4`, with the same modules list
  - Boost 1.73.0
  - Ninja, MSVC dev command prompt
- Configure:
  `cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_QT6=ON -DARCH_OPT=SSE4_2 -DDEBUG_OUTPUT=ON -DENABLE_MIRISDR=OFF -DBUILD_SERVER=OFF -DCMAKE_PREFIX_PATH=<Qt dir>;<boost dir>`
- Build just the plugin target: `cmake --build . --target ahrwwvtimedecoder`. Fall back to the full build if dependencies require it.
- Package `ahrwwvtimedecoder.dll` with `README-install.txt`:
  - Close SDRangel.
  - Copy the DLL into `C:\Program Files\SDRangel\plugins\`.
  - Restart SDRangel.
  - Add the channel "AHR WWV/WWVH Time Decoder".
- Also produce a full SDRangel package with the plugin (`--target package`) as a fallback, in case the drop-in DLL fails to load against the official installer (ABI mismatch).

## Rules

- No secrets, no code signing unless Sean asks.
- Do not claim the plugin loads in SDRangel until Sean has tried it on his PC.
