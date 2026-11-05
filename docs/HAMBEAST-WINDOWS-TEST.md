# HAMBEAST — Windows SDRangel 8.0.0 live test and pinned DLL

## Provenance: the exact DLL tested on HAMBEAST (5 MHz, November 5, 2026 UTC)

This is a **source-and-artifact pin**, not a claim that subsequent safety changes are part of the installed DLL. The DLL below was produced by GitHub Actions from the source commit identified here. The owner reports installing that DLL on HAMBEAST and observing live VERIFIED. The artifact's SHA-256 was independently calculated from the CI archive and matches its `SHA256SUMS.txt`.

| Field | Pinned value |
| --- | --- |
| Source commit (exact tested source) | `c40f823a1b3971d48b0bd2d2b06ae62f43d04960` |
| Source branch at build | `milestone-3-high-latitude-fading-fixes` |
| Windows GitHub Actions run | https://github.com/KL5NE/AHR-WWV-Decoder/actions/runs/41967213296 |
| CI artifact | `AHR-WWV-v8.0.0-win64-DLL-test` (artifact ID `13388464261`) |
| DLL filename | `ahrwwvtimedecoder.dll` |
| DLL SHA-256 | `b5c11123f9ecacb4e3d498c7b77cbd064f6a154306da448294b1f09c95923832` |
| Build target | SDRangel v8.0.0; Qt 6.9.3; MSVC 2022 x64 |
| Windows plugin CI | PASS: 81 integration checks; zero failed; FFmpeg dependency checks passed |
| Core CI | PASS: 105 checks, zero failed on Windows and on Linux |
| Installer status | **On hold**; this is an experimental DLL, not a released installer |

Verify the SHA-256 against the **DLL**, not against either the outer GitHub artifact ZIP or inner packaged ZIP. Back up the old DLL and close SDRangel before any replacement.

## Reported HAMBEAST live test (not independently reproduced in CI)

- **Receiver:** SDRplay RSP2pro, Yagi antenna oriented 180°, SDRangel 8.0.0, 5 MHz.
- **UTC test window:** 2026-11-05 05:02–05:17 UTC.
- **Live result:** first live **VERIFIED**; WWVH identified; 9 / 13 frames decoded; DUT1 −0.1 s; standard time.
- **Offline replay of that same private I/Q recording using new pinned core:** 10 consecutive frames accepted from 05:06 through 05:15 UTC; VERIFIED at about 761 s; 538 labelled seconds, **0 wrong**. End time within ~80 ms of the PC clock including receiver buffering and WWVH path delay.
- **Old core comparator:** branch `milestone-3-fading-joint-wwvh`, commit `199415908557cb93c61e8174f5efe2870aa75eaf` accepted a **false unconfirmed DECODED** time: 05:04 UTC when truth was 05:08 UTC. It incorrectly labelled 60 seconds by four minutes, then dropped the hypothesis; it never reached VERIFIED.
- The pinned new core did **not** show the false-DECODED failure in that recording, but its design did not mathematically rule out the issue.

These measurements are reported by KL5NE and Claude. The private recordings are in HAMBEAST's `Documents\\SDRangel` and **must not enter the GitHub repository or public CI artifacts**.

## Release-blocking safety requirements

1. **Only VERIFIED may be presented as an authoritative UTC clock or used in the PC-clock comparison.** DECODED is explicitly provisional/unconfirmed; no seconds should be labelled as trusted UTC before independent confirmation.
2. **Acceptance criterion: zero wrong labelled seconds, including during DECODED.** A false unconfirmed hypothesis must not generate an authoritative per-second UTC label, API output, or PC-offset comparison. Add a synthetic reproduction of the observed 4-minute misdecode when Claude supplies one; the private recording stays local.
3. Independent confirmation needs **explicit raw-minute frame IDs** and disjoint underlying evidence spans. Two overlapping joint-window decodes do not constitute independent readings.
4. A joint decode must use **consecutive, correctly aligned** 60-second frames. Reject shifted, duplicated, or gapped raw-minute windows; verify correct time delta versus sample clock across confirmation.
5. Re-run Linux/Windows core CI and native Windows plugin/integration suite after safety changes. Any changed DLL **has a new hash and is NOT the pinned DLL listed above**.
6. Run the same final candidate live on **5 MHz at night** and **10 or 15 MHz in daytime** before producing a full installer. Installation, rollback, version checking, and uninstall require separate testing.

## Earlier experimental setup checklist

- Locate the actual SDRangel application and plugins directory; verify version 8.0.0 and matching x64 architecture.
- Close the application completely, back up `ahrwwvtimedecoder.dll`, and install **only** our plugin DLL; do not replace SDRangel's Qt, FFmpeg, SDR driver, or core files.
- Confirm AHR channel loads, second/frame sync works, and only verified UTC is treated as trusted.
- Confirm shutdown and restart, changing receiver settings, sample loss, and loss of signal do not cause stale trusted timestamps.
- Preserve build and capture metadata, including frequency, antenna, gain, sample rate and SDRangel version.
- Keep the full Windows installer paused. The provenance of a future installer release must be pinned to its own fully tested source SHA and binary hash.
