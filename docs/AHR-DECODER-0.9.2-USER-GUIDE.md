# AHR-Decoder 0.9.2-beta — Windows installation and operating guide

**Alaska Ham Radio · KL5NE · 12 November 2026**

AHR-Decoder is an experimental WWV/WWVH time-code receiver plugin for the **official SDRangel 8.0.0 Windows x64 release**. It decodes the 100 Hz time-code subcarrier and displays UTC, date, day of year, DUT1, flags, and signal diagnostics. **It never sets or adjusts your computer clock.** This version includes the Palmer 15 MHz weak-signal fix and a safety guard that hides unverified predicted UTC in HOLDOVER.

**Release status:** 0.9.2-beta passed CI, nine automated Windows installer acceptance checks, and live HAMBEAST reception on 12 November 2026. This remains an unsigned beta for the exact supported host, not a universal SDRangel plugin. See [HAMBEAST acceptance report](HAMBEAST-WINDOWS-TEST.md).

## Part 1 — Install, upgrade, and uninstall

### Requirements

- Windows 11 x64, with official **SDRangel v8.0.0 x64** installed (tested with Qt 6.9.3); other SDRangel releases are intentionally refused. A custom installation folder is supported if the user selects the actual official installation directory.
- A receive-capable SDR and antenna. HAMBEAST was tested with SDRplay RSP2pro.
- Administrator permission for the installer and access to GitHub Actions release-candidate artifacts.
- **Close SDRangel** before installing, upgrading, or uninstalling.

### Verify what you downloaded

Get **AHR-Decoder-0.9.2-beta-SDRangel-8.0.0-x64-Setup.exe**, its accompanying `SHA256SUMS.txt` and `PROVENANCE.json` from the **same artifact** at [approved installer run 43119319819](https://github.com/KL5NE/AHR-WWV-Decoder/actions/runs/43119319819). The run also offers a manual ZIP; the installer is preferable for hash-checked ownership and safe upgrades.

In PowerShell, from the directory containing the files:

```powershell
Get-FileHash .\AHR-Decoder-0.9.2-beta-SDRangel-8.0.0-x64-Setup.exe -Algorithm SHA256
Get-Content .\SHA256SUMS.txt
```

Compare the full setup EXE hash against the matching filename in `SHA256SUMS.txt` before running it. **The DLL SHA-256 is a separate value:** `87e8b179e38f1f0bada44d5f0e6e66ed9dc4a05dd5068479f6fd4a3da65cc9b7`. Do not confuse it with the installer hash. The binary is unsigned; Windows SmartScreen may warn. Only proceed when the file source and hash are verified.

### Installation

1. Exit SDRangel and ensure `sdrangel.exe` has stopped.
2. Run the verified Setup EXE as administrator.
3. Select the folder containing the official SDRangel 8.0.0 application (typically `C:\Program Files\SDRangel`), not the `plugins` subfolder.
4. Finish Setup. The installer checks the official host fingerprint and the payload DLL hash before writing.
5. Start SDRangel; add **AHR WWV/WWVH Time Decoder** from receive channels. It should appear exactly once.

The installer places `ahrwwvtimedecoder.dll` in SDRangel's `plugins` directory. The plugin on HAMBEAST was at `C:\Program Files\SDRangel\plugins\ahrwwvtimedecoder.dll`. Installer ownership metadata is recorded in `%ProgramData%\AHR-Decoder\installed.json`. It does not replace SDRangel's other plugin files, executable, or Qt dependencies.

### Safe upgrades and uninstall

The installer uses a **hash-checked ownership record** to distinguish a previously installed AHR DLL from an unknown or modified file. Recognized upgrades are backed up with rollback protection; unrelated plugins are left alone. Uninstall removes only the recognized AHR-owned DLL and associated AHR files/registration. An unknown/modified DLL is deliberately not overwritten or removed.

**If an old DLL was copied manually**, the installer can refuse with *unrecognized same-name DLL*. Exit SDRangel, back up the old `ahrwwvtimedecoder.dll` to a folder **outside** the `plugins` directory, remove that old copy from `plugins`, and then rerun Setup. Never delete an unknown file before making a backup. If an AHR-managed DLL has been modified, investigate rather than bypassing the protection. Uninstall via Windows **Installed apps → AHR-Decoder**.

**Common refusals:** *SDRangel running* — exit the application completely; *unsupported host / host fingerprint* — use the exact official 8.0.0 Windows release; *unrecognized existing plugin* — back up and remove manually copied same-name DLL; *modified owned DLL* — stop and inspect the DLL and ownership record before taking action. Do not replace Qt/system DLLs to work around a load failure.

### Verify installed DLL

```powershell
Get-FileHash 'C:\Program Files\SDRangel\plugins\ahrwwvtimedecoder.dll' -Algorithm SHA256
Get-Content "$env:ProgramData\AHR-Decoder\installed.json"
```

Expected DLL SHA-256: `87e8b179e38f1f0bada44d5f0e6e66ed9dc4a05dd5068479f6fd4a3da65cc9b7`; expected version `0.9.2-beta`; Windows DLL build run `43104513833`. Adjust the DLL path if SDRangel is installed elsewhere.

## Part 2 — Operating the decoder

### Quick start

1. In SDRangel, start a receive device with a suitable WWV/WWVH antenna. Start with **10 MHz**; try **5 or 15 MHz** as propagation permits. WWV/WWVH broadcast on shared channels; reception and station identification depend on propagation. WWV also has other frequencies.
2. Set the receiver **centre 5 kHz below** the wanted carrier, e.g. **9.995 MHz** for WWV/WWVH **10.000 MHz**. This avoids the centre DC spike.
3. Add **AHR WWV/WWVH Time Decoder** and set its **Δf to +5000 Hz** (the channel marker must land on the carrier). Select **Station = Auto** initially.
4. Ensure **Decode** is enabled. Wait for seconds synchronization and then independent good minute frames. Weak HF signal conditions may require several minutes; on HAMBEAST VERIFIED took roughly 9–11 minutes.
5. Read the UTC badge, time-code cells, signal SNR/lock, and frame acceptance/rejection reason. Never treat an unverified time estimate as trustworthy.

Tested HAMBEAST settings: RSP2pro, Ant A / 80 m loop, 6 MS/s, centre 9.995 MHz, Δf +5000 Hz, AGC on, IF −40 dB, LNA 0. These are **observations**, not required universal gain settings.

### What the state badges mean

| State | Meaning | Trust |
| --- | --- | --- |
| SEARCHING / SECOND SYNC / FRAME SYNC | Searching for the tone or frame alignment | No verified UTC; `--:--:--` |
| DECODED (unverified) | A candidate frame, not yet independently confirmed | Not trusted |
| VERIFIED | Decoder has enough agreeing frame evidence to expose trusted UTC | Verified display/PC comparison available |
| **HOLDOVER m:ss** | Previously verified, but fresh reliable frame evidence has been lost | **Clock shows `--:--:--`; PC clock comparison unavailable. Predicted internal time is NOT exposed as verified UTC.** |
| NO DATA / DISABLED | No samples or decoder disabled | No verified UTC |

On live HAMBEAST, HOLDOVER appeared when receive gain was lowered to remove the signal without retuning. The clock and PC offset remained blank until new valid frames re-verified. After signal returned, a new accepted frame could restore VERIFIED when consistent with preserved prior evidence; it does **not** mean the first new frame has been independently verified in isolation.

### How to read the data

- The **60-second time code** appears as six rows of ten seconds: `0`, `1`, `M` for markers, `·` at second zero, and `?` for unreadable seconds. Color tint identifies year, time, day of year, DUT1, and flags.
- **DUT1, DST, leap-second and station** fields come from the received broadcast/data or station identification; weak data can be rejected with reasons such as *day of year too weak to read*.
- **WWV+WWVH** means both stations were detected on a shared channel; the station indicator can change as one fades. Path-delay estimates may depend on the selected station, so the PC-clock comparison is approximate when both are present.
- **100 Hz SNR** and second-lock strength describe signal metrics, not a guarantee of correct UTC. In this beta the SNR may look artificially high with no real signal during HOLDOVER; follow the verified state instead.
- **PC clock comparison** is read-only, approximate, and depends on propagation delay, receive path and SDRangel buffering. Optional **Latency corr.** is for a known receive-chain delay; **My Position** affects path-delay estimation.

### Reset and retune behavior

Changing Δf, centre frequency or sample rate restarts acquisition; it should not preserve stale trusted UTC. **Do not use a retune to test HOLDOVER:** it resets the decoder. To test signal loss, leave frequency/sample rate fixed and attenuate or remove the *received* signal safely. Do not transmit or alter Windows system time.

### Limitations and support

- Only the approved SDRangel 8.0.0 Windows x64 build is supported by the 0.9.2-beta installer.
- The plugin has **no REST/web status API** and no supported headless/server build. Confirm its state through the GUI.
- It never sets the PC clock. It can reject many frames under fading rather than guess.
- Seconds-event UTC labelling in HOLDOVER is covered by core CI; the GUI cannot display those events directly.
- No live SDRangel GUI startup test was performed by the disposable GitHub VM; that was confirmed separately on HAMBEAST.
- Report bugs with version, SDRangel release, frequency/offset, signal conditions, timestamps in UTC, screenshot, and steps to reproduce. Do not upload private recordings or credentials.
- [GitHub issues](https://github.com/KL5NE/AHR-WWV-Decoder/issues) · [Alaska Ham Radio](https://alaskahamradio.com) · [HAMBEAST live acceptance](HAMBEAST-WINDOWS-TEST.md)

**73 de KL5NE · Palmer, Alaska**
