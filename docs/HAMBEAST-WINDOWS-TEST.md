# HAMBEAST — Windows SDRangel v8.0.0 test checklist

**Not started.** The owner has not executed this plugin, installed it, or tested a live receiver on HAMBEAST. Complete this checklist only after GitHub Actions produces a successful, tested Windows DLL artifact.

## Before copying a DLL
- Confirm installed SDRangel is version **8.0.0** and note the Qt version in Help/About. Target build uses Qt **6.9.3**, MSVC 2022 x64.
- Download only the CI artifact for the reviewed GitHub commit; check the SHA-256 hash in `SHA256SUMS.txt`.
- Close SDRangel and confirm its process has exited.
- Locate the actual SDRangel executable and its `plugins` directory. **Do not assume an install path** if customized.
- Back up the plugins directory before changes. Check no `ahrwwvtimedecoder.dll` is already present.
- This build has not been code-signed; a successful compilation does not prove compatibility with an existing installation.

## Initial plugin load test
1. Copy **only** `ahrwwvtimedecoder.dll` into the application's plugins folder (typically `C:\Program Files\SDRangel\plugins\`).
2. Start SDRangel 8.0.0.
3. Look for **AHR WWV/WWVH Time Decoder** in RX channel list. Do **not** alter Windows system time.
4. Add the channel and verify the GUI shows **Searching**, not a fabricated UTC time.
5. Open the SDRangel log; capture any `PluginManager::loadPluginsDir` or DLL dependency errors.
6. Delete and re-add the channel; verify stable behavior and no crash.

If plugin fails to load, remove **only** the added DLL and relaunch SDRangel. Share the log. Do not replace SDRangel's original DLLs, Qt files, or executable.

## Synthetic replay test (recommended before radio)
1. Build or obtain `wwv_make_sdriq` output from the verified M3 core using a separate, known test vector.
2. In SDRangel, choose File Input; open a valid `.sdriq` recording at its declared center frequency and sample rate.
3. Add AHR channel, enable Decode, set channel frequency offset correctly.
4. Play in real time; observe the 60-second frame display, Searching -> Decoded -> Verified.
5. Compare each decoded UTC minute and day of year against the simulator's embedded reference.
6. Stop playback and verify Holdover/unverified indications. Do not use the PC offset reading to measure precision from simulated replay.

## Live RSP2pro test (only after the SDR/USB interference issue is resolved)
1. Record the antenna, sample rate, gain, AGC, frequency, date/time and SDRangel version.
2. Tune WWV 5 MHz or 10 MHz, then try 15 MHz when propagation permits. Observe the 100 Hz subcarrier in addition to the usual audible audio ticks.
3. Log frame counts, confidence, received signal, recovered UTC, and any missing-sample restarts.
4. Change sample rate and retune, verifying the decoder reacquires without confidently showing stale time.
5. Test long enough for multiple frames; capture SDRangel logs and a short video/screenshot.
6. Record which station was heard (WWV, WWVH or both); do not infer from the digital code alone.

## Result record

| Field | Result |
|---|---|
| Plugin build GitHub run URL | Pending |
| CI source commit SHA | Pending |
| Plugin SHA-256 | Pending |
| SDRangel version and Qt version | Pending |
| Windows DLL loads | Not tested |
| Synthetic replay inside stock SDRangel | Not tested |
| Live WWV/WWVH decode | Not tested |
| CPU and stability | Not tested |
| Observed errors/log excerpt | Pending |

**Release gate:** Do not call this a stable installer until stock Windows SDRangel loadability, correct frame decoding, an uninstall path, dependency licensing, and real-air reception have been verified.
