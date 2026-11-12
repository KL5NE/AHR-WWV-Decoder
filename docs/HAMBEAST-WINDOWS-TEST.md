# AHR-Decoder 0.9.2-beta — HAMBEAST Windows live acceptance

**GO — tested 12 November 2026 UTC, 02:08–02:46.** This supersedes the previous pre-release checklist. Acceptance performed by Sean KL5NE on HAMBEAST, assisted by Claude using receive-side SDRangel control. Panel state was confirmed from screenshots and PowerShell output; SDRangel log-to-file was not enabled. **No incorrect trusted UTC was observed.**

## Tested system and provenance

| Item | Observation |
| --- | --- |
| Windows | HAMBEAST, Windows 11 25H2 x64 |
| Host | Official SDRangel 8.0.0 (Qt 6.9.3) |
| SDR | SDRplay RSP2pro; Ant A, 80 m loop; 6 MS/s |
| Frequency | 10.000 MHz (SDR centre 9.995 MHz, decoder offset +5000 Hz), WWV and WWVH |
| RF gain | IF AGC on, IF −40 dB, LNA 0 baseline |
| Decoder DLL | `C:\Program Files\SDRangel\plugins\ahrwwvtimedecoder.dll`, 284,672 bytes, only copy |
| DLL SHA-256 | `87e8b179e38f1f0bada44d5f0e6e66ed9dc4a05dd5068479f6fd4a3da65cc9b7` |
| Windows plugin CI | [43104513833](https://github.com/KL5NE/AHR-WWV-Decoder/actions/runs/43104513833) — PASS |
| Core CI Linux + Windows | [43104513923](https://github.com/KL5NE/AHR-WWV-Decoder/actions/runs/43104513923) — PASS |
| C API CI | [43104514184](https://github.com/KL5NE/AHR-WWV-Decoder/actions/runs/43104514184) — PASS |
| NSIS installer + VM | [43119319819](https://github.com/KL5NE/AHR-WWV-Decoder/actions/runs/43119319819) — PASS, nine automated checks |
| Recorded DLL source | `52517ba4f82bc7a18ddc3ab3b0d8a567c60e694d` (PR #17 merge ref) |
| Installer metadata | `0.9.2-beta`; buildRun `43104513833`; `%ProgramData%\AHR-Decoder\installed.json` |

The recorded PR merge source includes the 15 MHz fading fix and HOLDOVER UTC guard. The core and plugin trees match the merged release source. Installation loaded the plugin exactly once, and the panel rendered correctly. Nine Windows VM cases passed: official host custom path, clean install, recognized upgrade, owned-only uninstall, unrelated plugins unaffected, rejection of unknown existing DLL, rejection of unsupported host, refusal while SDRangel running, and refusal to uninstall modified owned DLL. VM **did not** interactively launch SDRangel, force a copy-failure rollback, or test alternative host versions; HAMBEAST independently confirmed interactive plugin loading.

## Live reception

The only usable channel at test time was 10 MHz. Reception ranged roughly −46 to −62 dB, 100 Hz SNR 25–29 dB. Decoder acquired SECOND SYNC within seconds and had three VERIFIED acquisitions: first at **02:17 UTC**, second **02:30:43 UTC**, and recovery after HOLDOVER **02:46:02 UTC**. Both WWV and WWVH were detected, later WWVH alone. Observed UTC and date (2026-11-12, Thursday, day 316), DUT1 (−0.1 seconds), standard-time status, and absence of a leap-second warning were consistent with reference data. PC clock read ~44–49 ms fast before propagation correction, plausible for this receive chain.

Frames with insufficient day-of-year evidence were rejected rather than accepted incorrectly. A retune around 02:18–02:19 reset the decoder to SECOND SYNC without showing stale UTC; retuning is **not** a HOLDOVER test.

## Live HOLDOVER safety — PASS

At **02:31:10 UTC**, signal was removed **without retuning**: IF AGC off, IF gain −59 dB, LNA 8 (carrier to ~−96 dB). At 02:32:00 and 02:34:00 HOLDOVER screenshots, the UTC display was **`--:--:--`**, PC clock displayed **no comparison (no VERIFIED time)**, and the badge was orange HOLDOVER. Second lock and frame trust dropped; noise frames were rejected. Baseline gain was restored 02:34:19. At 02:35:29 the decoder remained in HOLDOVER despite new samples. At 02:41:10 it still withheld time after weak frames; it reacquired VERIFIED by 02:46:02 on a fresh accepted 02:45 frame, with retained prior evidence. No incorrect trusted UTC was observed.

**Limitation:** Per-second UTC event labelling could not be directly viewed in the plugin UI and was covered by regression tests in core CI run 43104513923, not observed live. The GUI provides no REST/web status API.

## Disposition and next version

**Overall GO for 0.9.2-beta release.** The following six non-blocking improvements are deferred rather than quietly changing the field-tested DLL:

1. Add read-only status/report API for automated acceptance.
2. Suppress misleading 100 Hz SNR (47–50 dB) when no signal/lock in HOLDOVER.
3. Correct stale HOLDOVER README and unreachable GUI orange prediction path.
4. Document re-verification after HOLDOVER using prior compatible evidence.
5. Monitor rare lost-samples restart and timestamp future occurrences.
6. Clarify mixed WWV/WWVH station identity and ambiguous propagation-delay calculations.

The older recorded source SHA `52517ba` was an ephemeral PR merge ref; the release should anchor to a durable branch commit with identical `core/` and `plugin/` source.

For users: [0.9.2-beta installation and operations guide](AHR-DECODER-0.9.2-USER-GUIDE.md). This is an experimental open-source SDRangel 8.0.0 Windows beta; it never sets the PC clock.
