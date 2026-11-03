# AHR WWV/WWVH Time Decoder: Milestone 1 Architecture

Target: SDRangel 8.0.0, Windows 64-bit, Qt 6.9.3. This matches the user's About box.

## 1. What SDRangel already has

- **Radio Clock** (`plugins/channelrx/radioclock`) decodes LF time stations only: MSF, DCF77, TDF, WWVB, JJY, RBU and PCSK-225.
- Nothing in SDRangel 8.0.0 decodes the WWV/WWVH HF 100 Hz time code, so this plugin does not duplicate existing work.
- Radio Clock is the closest model to copy. Its layout is:
  - `*plugin` (descriptor and factory)
  - `*settings`
  - `*baseband` (thread plus decimation)
  - `*sink` (DSP)
  - the main channel object
  - `*webapiadapter`
  - `*gui` + `.ui`
- WWVB on 60 kHz (a possible v2) is already covered by Radio Clock, so v2 can probably be skipped.

## 2. How SDRangel loads plugins (checked in the 8.0.0 source)

- `sdrbase/plugin/pluginmanager.cpp` scans `<application dir>/plugins` with `QPluginLoader`. On Windows that is `C:\Program Files\SDRangel\plugins\`.
- Each plugin is a Qt plugin. Its IID is `sdrangel.channel.<name>` and it exposes a `PluginDescriptor`. The version string is `"8.0.0"`.
- In CMake it is built with `qt_add_plugin(<target> CLASS_NAME <Class>)` and links `Qt::Core`, `Qt::Widgets`, `sdrbase` and `sdrgui`.
- The plugin is registered from `plugins/channelrx/CMakeLists.txt` with an `ENABLE_CHANNELRX_<NAME>` option and `add_subdirectory(...)`.

## 3. Can we ship a drop-in DLL?

Yes, provided the DLL is binary-compatible with the installed SDRangel. A plugin calls C++ classes exported by `sdrbase.dll` and `sdrgui.dll` (ChannelAPI, DeviceAPI, the GUI base classes). C++ has no stable ABI across compilers or library versions, so the DLL must be built with:

| Item | Must match | Source |
|---|---|---|
| SDRangel source | tag `v8.0.0` | user's About box |
| Qt | 6.9.3, `msvc2022_64` | About box, `.github/workflows/windows.yml` |
| Compiler | MSVC 2022 (v143), x64, Release | windows.yml |
| Generator / flags | Ninja; `-DCMAKE_BUILD_TYPE=Release -DENABLE_QT6=ON -DARCH_OPT=SSE4_2 -DDEBUG_OUTPUT=ON -DENABLE_MIRISDR=OFF -DBUILD_SERVER=OFF` | windows.yml |
| Boost | 1.73.0 (headers) | windows.yml |

Risks:

- If the official 8.0.0 installer was built from a slightly different commit or toolchain than the tag's workflow, the plugin may fail to load. The usual symptoms are "procedure entry point not found" or the plugin missing from the channel list.
- The fallback is to ship a full SDRangel build that includes the plugin. The same CI produces it with `--target package`.
- The decoder engine itself (`core/`) is plain C++17. It does not depend on Qt or SDRangel, so the ABI risk comes only from the thin plugin wrapper.

## 4. Recommendation: separate repository, CI builds against SDRangel v8.0.0

**Option chosen:** a separate repository named something like `ahr-wwv-decoder`. It contains:

```
core/            portable decoder engine (no Qt)           <- done, Milestone 2
sim/, tests/     simulator and tests (Linux and Windows)   <- done
tools/           WAV decode / generate command-line tools  <- done
plugin/          SDRangel channel plugin wrapper           <- Milestone 3
ci/              Windows build script                      <- ChatGPT
```

The CI does the following:

1. Check out SDRangel at `v8.0.0`.
2. Copy `plugin/` and `core/` into `plugins/channelrx/ahrwwv/`.
3. Append the `add_subdirectory` line.
4. Configure with the official flags.
5. Build only the plugin target (`cmake --build . --target ahrwwvtimedecoder`). This still compiles sdrbase and sdrgui, which it needs.
6. Publish the DLL as `ahrwwvtimedecoder.dll` in a zip with a README.

Why not a fork of SDRangel:

- A fork means tracking upstream for no benefit.
- Our code is a few files.
- A separate repository keeps the user's code clearly theirs.
- If the plugin is ever offered upstream, the `plugin/` folder drops straight into a pull request.

## 5. Plugin design (Milestone 3, not built yet)

```
SDRangel device (SDRplay, 2 MS/s)
   └─ AHRWWVBaseband   decimate to ~12 kHz around the WWV carrier (SDRangel DownChannelizer)
        └─ AHRWWVSink  feeds complex samples to ahrwwv::WwvDecoder::processIQ()
                       stamps the block with QDateTime::currentMSecsSinceEpoch() -> setNextSampleTimestamp()
        └─ main channel object  posts DecoderStatus/events to the GUI through the message queue
             └─ AHRWWVGUI  big clock + state badge, bit strip, signal meters, optional diagnostics
```

- Channel bandwidth: about ±1.6 kHz. The engine also filters internally.
- Settings to persist:
  - frequency offset
  - station selection (WWV / WWVH / auto)
  - receiver location (for the skywave delay)
  - display options
- The web API adapter is optional, but nice to have for the MCP tools.
- Never set the PC clock. The decoder only displays and compares.

## 6. Decoder engine (Milestone 2, built and tested)

Signal path:

1. AM envelope `|IQ|`. Audio input is also accepted.
2. Mix with 100 Hz, then 1 ms boxcar decimation.
3. 4th-order 25 Hz low-pass.
4. Divide by the carrier level, passed through the same low-pass, to cancel flat fading.
5. Fold modulo 1 s to find the pulse leading edge. This gives second lock.
6. Lock to the 100 Hz subcarrier phase and use coherent projection. Noise then averages to zero instead of biasing the levels.
7. Per second, measure the levels in three windows: 60–180, 240–480 and 540–780 ms after the tick. Score None/0/1/Marker. Weight each window by its measured noise (from the quadrature component) and by carrier fade depth.
8. Find the frame position with seconds 9/19/29/39/49/59 as markers, second 0 empty, and the unused bits at 0.
9. Maximum-likelihood decode of each field over its legal values only. Reject the frame if the weakest field margin is low or the frame structure is broken.
10. Show a time only after one strong frame (Decoded). Mark it Verified when two independent frames agree on the same sample-clock offset. A frame that disagrees never silently replaces a verified time.
11. Fine timing: 50 % edge crossings from quiet seconds only, Huber-clamped, in a slow loop aided by the measured sample-clock ppm.
12. Holdover after a verified time: the sample clock carries time, and the uncertainty grows by ppm × age.
13. Station ID from the second ticks: 1000 Hz is WWV, 1200 Hz is WWVH. The 800/1400 Hz noise reference sits at spectral nulls of both ticks.

PC clock offset = PC timestamp of the boundary − (UTC + propagation delay). It is reported with a 1σ uncertainty that includes:

- propagation (default 5 ms)
- host timestamp latency (default 20 ms; this is SDRangel's buffering, and it dominates)

## 7. Honest limits

- The real recordings available (30 s, SDRangel AM audio) are too short for a 60 s frame. They are also dominated by the ~10 Hz pulsing artifact found earlier in the SDR/USB chain. The decoder correctly refuses to lock on them.
- Real-signal validation happens in Milestone 3 inside SDRangel, or with a new recording of at least 3 minutes, made with the artifact fixed.
- Leap seconds (a 61-second minute) are not handled specially. The frame lock re-acquires afterwards.
- WWV's voice may contain energy near 100 Hz. The simulator includes a deliberately harsh voice model in seconds 45–58, and the decoder handles it.
- The absolute PC offset will be limited by SDRangel's sample-to-timestamp latency, which we can't measure from inside the plugin. Expect roughly ±20–50 ms unless the latency is calibrated.
