# Milestone 3: native SDRangel plugin (handoff)

Status of the plugin, kept separate by kind of evidence:

| What | Status | Evidence |
|---|---|---|
| Plugin source (channel, DSP, GUI, settings) | **Done** | `plugin/` |
| Compiles against SDRangel **v8.0.0** source | **Done on Linux** (GCC 13.3, Qt 6.4.2) | Built in-tree in a v8.0.0 checkout (commit `0282a596`) |
| Compiles with MSVC 2022 / Qt 6.9.3 | **Pending.** ChatGPT CI. | n/a |
| Loads in SDRangel 8.0.0 | **Done on Linux** (dev build) | Log: `loaded plugin libahrwwvtimedecoder.so`; channel listed and created through the REST API |
| Decodes inside running SDRangel | **Done on Linux** with a replayed synthetic recording (File Input device) | `docs/m3-results/sdrangel-replay-test.txt`, screenshot |
| Integration tests (real plugin classes + SDRangel DSP) | **80/80 pass on Linux** | `docs/m3-results/plugin-integration-tests.txt` |
| Core tests (Milestone 2 plus envelope tap) | **83/83 pass on Linux** | `docs/m3-results/core-tests.txt` |
| Drop-in DLL into a stock Windows SDRangel 8.0.0 | **Unverified.** Expected to work if built as below (section 2). | Must be tried on HAMBEAST |
| Live reception from the RSP2pro | **Not tested** | Final milestone |

## 1. Files

### Added

```
plugin/CMakeLists.txt            in-tree CMake (qt_add_plugin, links sdrbase + sdrgui), tests option
plugin/ahrwwvplugin.h/.cpp       PluginInterface: descriptor, registration, factory
plugin/ahrwwv.h/.cpp             channel (BasebandSampleSink + ChannelAPI), messages, settings relay
plugin/ahrwwvbaseband.h/.cpp     worker in its own QThread: SampleSinkFifo -> DownChannelizer -> sink
plugin/ahrwwvsink.h/.cpp         NCO + interpolator to 12 kHz, feeds the core decoder, sample-loss detector, reports
plugin/ahrwwvreport.h            plain-data snapshot passed DSP thread -> GUI thread
plugin/ahrwwvsettings.h/.cpp     settings + SimpleSerializer (presets / configurations)
plugin/ahrwwvgui.h/.cpp/.ui      ChannelGUI: settings, UTC clock, time-code monitor, signal, diagnostics
plugin/ahrwwvwidgets.h/.cpp      custom widgets: 60-second code strip, envelope plot
plugin/readme.md                 user documentation (opened by the panel's help button)
plugin/LICENSE-HEADER.txt        GPL-3.0 header used in the plugin files
plugin/tests/test_plugin_integration.cpp   integration tests (see section 5)
plugin/tests/run_sdrangel_replay_test.sh   end-to-end test inside a running SDRangel
sdrangel-integration/apply_integration.cmake   adds the hook to SDRangel (idempotent)
sdrangel-integration/channelrx-hook.cmake      the 8 lines that get appended
tools/make_sdriq.cpp             writes simulated WWV I/Q as SDRangel .sdriq (File Input)
docs/M3-PLUGIN.md                this file
docs/m3-results/*                test outputs and screenshot
```

### Modified

| File | Change |
|---|---|
| `core/include/ahrwwv/decoder.h`, `core/src/decoder.cpp` | Added `setEnvelopeTap()`: an optional read-only callback for the GUI's envelope plot. It survives `reset()`. Decoding behaviour is unchanged: all Milestone 2 tests and expected outputs are the same. |
| `tests/test_main.cpp` | Added a test for the envelope tap. |
| `CMakeLists.txt` | Builds `wwv_make_sdriq`. |
| `README.md`, `.gitignore` | Status updated; `*.sdriq` ignored. |

### SDRangel upstream changes

- **None to SDRangel's code.** `plugins/channelrx/CMakeLists.txt` gets one guarded `add_subdirectory()`. It is applied by `sdrangel-integration/apply_integration.cmake` and does nothing unless `-DAHRWWV_PLUGIN_DIR` is set.
- No swagger, web API or sdrbase changes. That is deliberate: it is what keeps a drop-in DLL possible.

## 2. Drop-in DLL or custom SDRangel build? (source evidence, SDRangel v8.0.0)

**Conclusion:** a plugin-only DLL can work with stock SDRangel 8.0.0 on Windows. It must be compiled against the v8.0.0 sources with the same compiler, Qt and sample-size options as the official release. Building it still requires building SDRangel's `sdrbase` and `sdrgui` from source, because that is the only way to get their import libraries. Only the plugin DLL is shipped. A full SDRangel build is the fallback.

Evidence:

1. **Discovery.**
   - `sdrbase/plugin/pluginmanager.cpp` `loadPluginsPart()`: on Windows it scans `<applicationDir>/plugins` (`#elif defined(_WIN32)` branch) with the filter `"*"`.
   - `loadPluginsDir()` loads every library with `QPluginLoader` and accepts it if `qobject_cast<PluginInterface*>` succeeds.
   - No list of allowed plugins, no signature check. A new DLL in `C:\Program Files\SDRangel\plugins\` is picked up.
2. **Interface identity.** `sdrbase/plugin/plugininterface.h`: `Q_DECLARE_INTERFACE(PluginInterface, "SDRangel.PluginInterface/0.1")`. Our plugin uses the same header, so the IID matches. Verified on Linux, where the plugin loaded in SDRangel.
3. **Web API is optional.**
   - `PluginInterface::createChannelWebAPIAdapter()` defaults to `nullptr`.
   - `sdrbase/webapi/webapiadapterbase.cpp` checks for `nullptr` (lines 183-185, 454-455).
   - `ChannelAPI::webapi*` default to "not implemented".
   - So the plugin needs **no** swagger classes. Swagger classes would require rebuilding `libswagger`/`sdrbase`, which would make drop-in impossible.
4. **Binary interface.** The plugin calls C++ classes exported from `sdrbase.dll` and `sdrgui.dll`: ChannelAPI, BasebandSampleSink, DeviceAPI, DownChannelizer, SampleSinkFifo, Interpolator, NCO, ChannelGUI, RollupContents, ValueDialZ, LevelMeterSignalDB, ChannelMarker, MainCore, BasicChannelSettingsDialog. Their layout must match the installed DLLs:
   - **Same source:** tag `v8.0.0` (commit `0282a596caf195591d989edcc1e05a8f75361cf9`), cloned with submodules (`external/windows`).
   - **Same sample size:** `RX_SAMPLE_24BIT` left at its default ON (defines `SDR_RX_SAMPLE_24BIT`, CMakeLists.txt:813). This changes `Sample`/`FixReal` (`sdrbase/dsp/dsptypes.h`) and `DownChannelizer`.
     - The official build does not change it.
     - SDRangel reports `"dspRxBits": 24`. Our Linux build matched.
   - **Other build-dependent macros in the headers we use:**
     - `USE_SSE2` (interpolator.h) and `NCO_64_BIT` (nco.h) are not defined anywhere in the tree, so they are the same everywhere.
     - `ARCH_OPT` only sets compiler `/arch` flags and does not change layouts. Still, use `SSE4_2` like the release.
   - **Same compiler and runtime:** MSVC 2022 (v143), x64, Release, `/MD` (CMakeLists.txt:7: `MultiThreaded…DLL`).
   - **Same Qt:** 6.9.3 `msvc2022_64`. `QPluginLoader` rejects plugins built against a newer Qt. The user's About box shows Qt 6.9.3.
5. **Import libraries.** The NSIS packaging removes `*.lib` (`cmake/cpack/CMakeLists.txt:330`), so the installer has no import libraries to link against. CI must build `sdrbase`/`sdrgui` (and what they need) from the v8.0.0 sources. That is what the `--target ahrwwvtimedecoder` build below does.
6. **Official release provenance.** `.github/workflows/windows.yml` at v8.0.0 builds the signed installer on tag push. It uses:
   - `windows-latest` with MSVC via `ilammy/msvc-dev-cmd`
   - Qt 6.9.3 `win64_msvc2022_64` (`jurplel/install-qt-action@v4`)
   - Boost 1.73.0
   - Ninja
   - `cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_QT6=ON -DARCH_OPT=SSE4_2 -DDEBUG_OUTPUT=ON -DENABLE_MIRISDR=OFF -DBUILD_SERVER=OFF`

**Risks still open:**

- Small MSVC or Qt patch-level differences between ChatGPT's runner and the release runner should not change the class layouts, but only a test on HAMBEAST proves it.
- If the DLL fails to load, SDRangel logs `PluginManager::loadPluginsDir: <reason>`.
- The fallback is the full package: the same CI with `--target package`, which builds SDRangel plus this plugin into one installer.

## 3. Build: Windows (for ChatGPT's CI)

```
# prerequisites, as in SDRangel's windows.yml at v8.0.0:
#   MSVC 2022 x64 dev environment, CMake, Ninja, 7-Zip, patch (choco install patch)
#   Qt 6.9.3 win64_msvc2022_64 with modules: qtcharts qtscxml qt5compat qtlocation qtmultimedia
#       qtpositioning qtserialport qtspeech qtwebsockets qtwebengine qtshadertools qtwebchannel
#   Boost 1.73.0 headers in C:\Libraries\boost_1_73_0

git clone --branch v8.0.0 --depth 1 --recurse-submodules --shallow-submodules https://github.com/f4exb/sdrangel.git
cmake -DSDRANGEL_SOURCE_DIR=sdrangel -P AHR-WWV-Decoder/sdrangel-integration/apply_integration.cmake

cmake -S sdrangel -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_QT6=ON -DARCH_OPT=SSE4_2 ^
      -DDEBUG_OUTPUT=ON -DENABLE_MIRISDR=OFF -DBUILD_SERVER=OFF ^
      -DCMAKE_PREFIX_PATH="C:\Qt\6.9.3\msvc2022_64;C:\Libraries\boost_1_73_0" ^
      -DAHRWWV_PLUGIN_DIR=%CD%\AHR-WWV-Decoder\plugin ^
      -DAHRWWV_BUILD_PLUGIN_TESTS=ON

cmake --build build --target ahrwwvtimedecoder ahrwwv_plugin_tests
```

`AHRWWV_PLUGIN_DIR` must be an absolute path.

Expected outputs. On Windows with Qt 6, plugin libraries go to `bin/plugins` (`plugins/CMakeLists.txt`, QTBUG-124589 branch):

| Output | Path |
|---|---|
| Plugin DLL (the deliverable) | `build/bin/plugins/ahrwwvtimedecoder.dll` |
| Integration test | `build/bin/ahrwwv_plugin_tests.exe` |
| SDRangel dev build (optional, `--target sdrangel` plus device plugins) | `build/bin/sdrangel.exe` |

Run the integration tests from `build/bin` so that `sdrbase.dll` and the Qt DLLs are found (or add them to PATH):

```
cd build\bin
ahrwwv_plugin_tests.exe plugins\ahrwwvtimedecoder.dll
```

Notes:

- The memory check in that test uses `getrusage` and is skipped off Linux. Everything else runs on Windows.
- Expect the first configure and build to take a long time. SDRangel's Windows build compiles its external libraries (`ENABLE_EXTERNAL_LIBRARIES` is forced ON on WIN32).

**Install on a stock SDRangel 8.0.0 (manual test on HAMBEAST):**

1. Close SDRangel.
2. Copy `ahrwwvtimedecoder.dll` into `C:\Program Files\SDRangel\plugins\`.
3. Start SDRangel. The channel list should show **AHR WWV/WWVH Time Decoder**.

If the channel is missing, check SDRangel's log for `PluginManager::loadPluginsDir` lines naming the DLL. Uninstall by deleting the DLL.

## 4. Build: Linux dev build (what was done here)

```
sudo apt install qt6-base-dev qt6-base-private-dev qt6-websockets-dev qt6-multimedia-dev \
     qt6-positioning-dev qt6-charts-dev qt6-serialport-dev qt6-declarative-dev qt6-svg-dev \
     qt6-scxml-dev qt6-speech-dev libqt6opengl6-dev libgl-dev libboost-dev libfftw3-dev \
     libusb-1.0-0-dev libopus-dev libflac-dev zlib1g-dev pkg-config
git clone --branch v8.0.0 --depth 1 https://github.com/f4exb/sdrangel.git
cmake -DSDRANGEL_SOURCE_DIR=sdrangel -P AHR-WWV-Decoder/sdrangel-integration/apply_integration.cmake
cmake -S sdrangel -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_QT6=ON \
      -DBUILD_SERVER=OFF -DBUILD_BENCH=OFF -DENABLE_EXTERNAL_LIBRARIES=OFF -DDEBUG_OUTPUT=ON \
      -DENABLE_CHANNELTX=OFF -DENABLE_CHANNELMIMO=OFF -DENABLE_FEATURE=OFF \
      -DAHRWWV_PLUGIN_DIR=$PWD/AHR-WWV-Decoder/plugin -DAHRWWV_BUILD_PLUGIN_TESTS=ON
# (hardware drivers and other channel plugins were switched off with their ENABLE_* options to save time)
ninja -C build sdrangel inputfileinput ahrwwvtimedecoder ahrwwv_plugin_tests
```

| Output | Path |
|---|---|
| Plugin | `build/lib/plugins/libahrwwvtimedecoder.so` |
| Integration tests | `build/ahrwwv_plugin_tests` |
| SDRangel | `build/sdrangel` |

## 5. Tests

### Core (unchanged procedure)

`ctest` in the AHR repository: 83 checks. This adds the envelope-tap test to Milestone 2's 81.

### Plugin integration tests (`ahrwwv_plugin_tests [plugin library] [--long]`)

These link the real plugin classes and SDRangel's `sdrbase`. Simulated WWV I/Q is fed into `AHRWWVBaseband`, which runs in its own QThread through SDRangel's FIFO, DownChannelizer, NCO and Interpolator:

| Test | What it proves |
|---|---|
| settings round trip / corrupt data | SDRangel preset serialization; bad data resets to defaults |
| QPluginLoader ×20 | The built library loads, exposes `sdrangel.channel.ahrwwvtimedecoder`, descriptor `AHRWWVTimeDecoder` / 8.0.0, no web API adapter, unloads cleanly |
| decode through channel, 48 kS/s, +5 kHz | Full DSP path to a Verified time; zero wrong seconds; reports reach the GUI queue |
| 2 MS/s −250 kHz, 192 kS/s −20 kHz, 62.5 kS/s 0 Hz | Variable device rates, negative and zero offsets |
| device rate change mid-stream | Decoder restarts and re-verifies |
| retune off the carrier and back | Offset changes restart the decoder; no time is shown off the carrier; recovers |
| disable / enable / Reset button message | Decoder idles, restarts, clears |
| 3-minute signal loss | Holdover with <1 ms drift, then Verified again |
| real-time feed with 1.5 s of samples lost | The sample-loss detector restarts the decoder (no false alarm in 22 s of steady feed) |
| 50 s of signal then silence | An incomplete frame never produces a time |
| 15-minute run (60 min with `--long`), fading, C/N 12 dB | No wrong seconds, no memory growth |

Result: **80/80 checks pass** (Linux; with `--long`, see `docs/m3-results/plugin-integration-tests.txt`).

### End-to-end inside SDRangel (`plugin/tests/run_sdrangel_replay_test.sh`)

The script:

1. Generates a synthetic recording with `wwv_make_sdriq`.
2. Starts SDRangel.
3. Through the REST API: creates a File Input device set, adds the AHR channel, and starts playback in real time.
4. Checks SDRangel's log. The plugin writes one `AHRWWV: frame accepted …` line per minute, and each decoded minute must match the recording's start time.

Result on Linux: PASS. Plugin loaded, channel created, 3 frames accepted and 0 rejected, states Decoded → Verified → Verified, all minutes correct.

The same procedure works by hand on Windows. Make a recording with `wwv_make_sdriq.exe` (built by the AHR CMake), then in SDRangel:

1. Add a File Input device and open the `.sdriq` file.
2. Add the channel.
3. Press play.

Channel lifecycle: the channel was deleted and re-created 5 times through the REST API while running, with no crash or leak in the log. CPU cost of the channel: about 6 % of one core in that build (decoder plus GUI repaint). Measured with software OpenGL under Xvfb, so only indicative.

## 6. Design notes

- **Signal path:** device I/Q → SampleSinkFifo → DownChannelizer (power-of-two decimation around Δf) → NCO + polyphase Interpolator to exactly 12 kHz (±2 kHz) → `WwvDecoder::processIQ()`. The core then does:
  1. ±1.6 kHz channel filter
  2. AM envelope
  3. 100 Hz coherent detection, then frame decode and verification
- **Threads:**
  - The DSP and decoder run in the channel's own `AHRWWVBB` thread.
  - Every 250 ms of wall clock, and on every frame, the sink posts an `AHRWWVReport` by value through the message queues to the channel (main thread), which forwards it to the GUI.
  - The GUI never touches the decoder.
  - The clock display extrapolates from `(utcAtRef, pcAtRef)` every 50 ms.
- **Restarts:** the decoder starts from scratch when:
  - Δf changes
  - the device centre frequency or sample rate changes
  - the device restarts
  - the decoder is re-enabled
  - Reset is pressed
  - the sample-loss detector fires: PC time minus sample time steps up by more than 0.25 s for 10 s, which means samples were lost rather than delayed

  Each of these breaks the link between sample count and real time, and a wrong time is worse than a short re-acquisition.
- **Station:**
  - Auto shows the tick-tone detector result (1000 Hz WWV, 1200 Hz WWVH, both). This was verified on simulated signals in Milestone 2; not yet on air.
  - Manual WWV/WWVH is a label and selects the path delay for the PC comparison.
- **PC clock comparison:** PC timestamp minus (decoded UTC + skywave path delay + user latency correction).
  - The path delay comes from SDRangel's *My Position*.
  - The 1σ uncertainty includes 5 ms propagation and 20 ms host latency.
  - Offsets above 5 s are reported as a recording replay or a badly set clock.
  - It never sets the clock.

## 7. Known limitations

- No web API or REST settings for this channel. They would need SDRangel swagger changes and so a custom SDRangel build. The channel offset can't be set by script; set it in the GUI or save it in a preset.
- No server (headless) build of the plugin.
- Help button opens the GitHub readme (`plugin/readme.md`) rather than a bundled page.
- Leap seconds are not handled specially; re-acquisition follows.
- The PC offset includes SDRangel's internal buffering, which the plugin can't measure; there is a manual latency correction.
- Under faster-than-real-time playback (File Input acceleration), decoding works, but the GUI clock and PC comparison are meaningless.
- Very deep fading at low C/N may take many minutes to verify (see Milestone 2 stress results). The decoder prefers no time over a wrong one.

## 8. Remaining blockers / decisions

1. **Windows MSVC build** of `ahrwwvtimedecoder` and `ahrwwv_plugin_tests` (ChatGPT CI).
2. **Load test on stock SDRangel 8.0.0 on HAMBEAST.** This decides drop-in DLL versus full package.
3. **Licence:**
   - The plugin follows SDRangel's Radio Clock structure and links GPL-3.0 libraries, so its files carry a GPL-3.0-or-later header.
   - The repository has no LICENSE file yet.
   - Distributing the DLL means GPL-3.0 terms apply. Sean to decide; adding GPL-3.0 as the repository licence is the simplest path.
4. **Live reception test** with the RSP2pro, after the 10 Hz USB/SDR artifact found earlier is resolved.
