<h1>AHR WWV/WWVH Time Decoder plugin</h1>

<h2>Introduction</h2>

This receive channel decodes the 100 Hz binary time code broadcast by NIST stations WWV (Fort Collins, Colorado: 2.5, 5, 10, 15, 20, 25 MHz) and WWVH (Kauai, Hawaii: 2.5, 5, 10, 15 MHz). It shows UTC date and time, day of year, DUT1, daylight-saving and leap-second flags, the 60-second code as it arrives, signal quality, and an approximate comparison with the computer clock.

The plugin only **displays** time. It never changes the computer's clock.

<h2>Quick start</h2>

1. Tune the receiver so a WWV/WWVH frequency is inside the band. Keep it away from the centre DC spike, e.g. centre 9.995 MHz for 10 MHz.
2. Add the channel **AHR WWV/WWVH Time Decoder**.
3. Set Δf so the channel marker sits on the carrier (+5000 Hz in the example above).
4. Wait. The decoder needs about 5 s to find the seconds and 1–3 minutes to read and confirm a minute frame.

Any receiver sample rate works. The channel resamples to 12 kHz internally.

<h2>Interface</h2>

<h3>Settings</h3>

- **Δf**: channel offset from the receiver centre, in Hz. Changing it restarts the decoder.
- **Decode**: switch the decoder on or off. Switching it back on starts from scratch.
- **Station**:
  - **Auto** shows what the seconds-tick tone says: 1000 Hz is WWV, 1200 Hz is WWVH, "WWV+WWVH" when both are heard on a shared frequency.
  - **WWV** or **WWVH** labels the signal yourself.
  - The choice also selects which path delay is removed in the PC clock comparison.
- **Reset**: clear everything and search again.
- **Diagnostics**: show the envelope panel.
- **Latency corr.**: the delay between the antenna and this plugin, if you know it (SDR and SDRangel buffering). It is only used in the PC clock comparison.
- **Path delay**: estimated skywave delay from the station to *My Position* (Preferences > My Position).

<h3>UTC Clock</h3>

The big clock shows decoded UTC, as transmitted. Your receive delay (about 10–20 ms) is not added. The badge says how far to trust it:

| Badge | Meaning |
|---|---|
| SEARCHING / SECOND SYNC / FRAME SYNC | No time yet. The clock shows `--:--:--`. |
| DECODED (unverified) | One good minute frame. Shown in yellow until a second frame agrees. |
| VERIFIED | Two independent frames agree. Shown in green. |
| HOLDOVER m:ss | The signal faded after a verified time. The internal decoder may predict time, but the **public clock is blank (`--:--:--`) and PC-clock comparison is unavailable** until fresh verification. Shown in orange. |
| NO DATA / DISABLED | No samples arriving (device stopped) or decoder off. |

**PC clock** is a read-only estimate of the computer clock against decoded UTC. It removes the path delay and your latency correction. It is approximate: SDR and SDRangel buffering can add tens of milliseconds that the plugin cannot measure.

<h3>Time Code</h3>

The 60-second code is shown as 6 rows of 10 seconds:

- **0** and **1** are data bits.
- **M** is a position marker (seconds 9, 19, … 59).
- **·** is second 0, which carries no pulse.
- **?** is a second that could not be read.

Cells are tinted by field: year, minute, hour, day of year, DUT1, flags. The current second is outlined. Hover a cell for its meaning.

Below the code are DUT1, DST, the leap-second warning, the frame count ("3 / 32 decoded (9 %)" since the decoder last started), the confidence of the last frame, and why the last frame was rejected (if it was).

<h3>Signal</h3>

- 100 Hz subcarrier SNR.
- Second-lock strength.
- Sync state and position in the minute.
- Station heard.
- Whether samples and the time code are arriving.
- Timing: edge jitter, the measured SDR sample-clock error in ppm, and the time uncertainty.

<h3>Diagnostics</h3>

- The recovered 100 Hz level over the last 4 seconds. A pulse is about 1.0 and a gap about 0. The dashed line marks 0.5.
- The three measurement-window levels (A, B, C) and the decision for each recent second:

| Symbol | A | B | C |
|---|---|---|---|
| 0 | high | low | low |
| 1 | high | high | low |
| M | high | high | high |
| second 0 | low | low | low |

<h2>Limitations</h2>

- No REST/web API for this channel. SDRangel answers "not implemented" for its settings and report endpoints, because adding them needs changes inside SDRangel.
- No server (headless) build.
- Leap seconds are not handled specially. The decoder re-acquires after one.
- When fading spoils one minute, the decoder reads it together with the previous one or two minutes. It still needs two independent readings to show VERIFIED.
- The PC comparison cannot see SDRangel's internal buffering. Calibrate with **Latency corr.** if you know your setup.
