# REAPER integration test

Verified on Joe's M1 (REAPER 7.84, macOS 26.6) and MINI-PC (REAPER 7.83, Windows 11 25H2) on 2026-10-30, using the RC1b product binaries. This is a host integration test; the separate HostTest golden tests remain the cross-platform DSP regression check.

## Run

Build the **HostTest target only** from this branch with `WLA_TESTS=ON`. Older CI copies of HostTest do not understand the new reference options. Do not rebuild or replace the product plug-in to investigate a release binary.

```text
python plugin/tests/reaper/run_reaper_test.py <new-HostTest> --vst3 <installed-Tertiary.vst3> --reaper-config <isolated-reaper.ini> --prefix reaper_vst3
python plugin/tests/reaper/run_reaper_test.py <new-HostTest> --fx AU:Tertiary --reaper-config <isolated-reaper.ini> --prefix reaper_au
```

`--reaper` overrides the application path. `--vst3` defaults to the installed bundle (Windows Common Files; Mac user Library, then system Library). Configure the isolated REAPER profile to scan **that same binary**. A fresh profile needs an audio device configured or its startup dialog blocks the script. Windows test profile:

```ini
[reaper]
vstpath64=<directory containing the RC1b VST3 bundle>
renderclosewhendone=4
[audioconfig]
mode=4
dummy_srate=48000
dummy_blocksize=512
```

On the M1 the isolated profile requested CoreAudio 48000 Hz / 512 samples. SSH runs must enter the logged-in user's Aqua session (AU discovery needs it), for example `sudo -n launchctl asuser $(id -u) sudo -n -u $(id -un) python3 ...`. Do not use the old Mac-day wrapper that substitutes its archived Lua script. Run this driver's current Lua directly. Windows uses its normal startup window so the evaluation countdown can complete; each owned test instance quits on completion.

## Windows quit investigation (Codex, 2026-10-30)

The 20:53 run's failure was reproduced with the same binary: all audio assertions
passed, but the process reached its 90-second watchdog with **About REAPER
v7.83/win64 rev 6eea47 (Oct 24 2026)** still showing disabled **Buy Me [4]**.
**REAPER New Version Notification** had taken focus after About was activated;
the driver then released the Lua render after a fixed delay while About was still
modal. The screenshot also shows an unrelated `electron.exe` error overlapping
About. The driver never acts on that other process. This is a harness startup
dialog/quit failure, not a failed plug-in audio comparison.

`tests/lib/reaper_windows.py` now restores focus to the owned About dialog,
waits for its enabled **Still Evaluating** button, and closes only that named
button and the notification's **Close** button. The driver releases the render
handshake only after dialogs close, then requests native **File: Quit** after
the saved-project marker. It requires exit zero; a forced cleanup still fails.
Unknown dialogs are recorded rather than accepted. Dependencies are `pywinauto`
and its Windows dependencies (`pywin32` and Pillow). Timestamped `windows.log`
and window-rendered PNGs accompany every run; screenshot capture has its own
three-second bound and cannot prevent cleanup of an owned timed-out process.

Evidence is under `tests/results/investigation-2026-10-30/`: `reaper-before`
failed after 90.2 seconds; `reaper-after-1` and `reaper-after-2` exited normally
in 29.65 and 29.85 seconds. `reaper-final-verified`, with the final window-rendered
capture and main-window startup gate, exited normally in **27.21 seconds**:
Still Evaluating at 23.922 s, render released at 24.953 s, native Quit at 26.063 s.
The clear dialog screenshot is `reaper-final-verified/reaper-artifacts/vst3/reaper-still-evaluating.png`.
Every corrected run retained maximum audio error **0** over all
120000 frames, repeat error **0**, valid automation and saved projects. The
tested module SHA256 stayed
`5b8db0b2461338c5ffa0e371aced3c4a9e51f1df7cbb9992c1d412e337a89311`.
No plug-in source or binary was changed. This follow-up did not use the Mac.

## Matched reference

1. `HostTest --write-reaper-input` writes 24000 silent frames followed by the original 96000-frame deterministic noise/sweep/click signal: 2.5 s, stereo, 48000 Hz, float WAV.
2. `HostTest --vst3 <path> --write-reaper-reference <wav>` (or `--au`) processes that same input with a fresh instance, defaults, 512-sample blocks, offline mode and an explicit **playing** clock: 120 BPM, sample/time/PPQ starting at zero and advancing every block.
3. REAPER processes the same timeline and input. The 0.5 s silent lead lets both the 50 ms gain ramps and the 10 ms transport phase crossfade settle. Item fades, dither and render postprocessing are disabled. Bounds are exactly 0 to 2.5 s, stereo, 48000 Hz, 32-bit float, with no tail. Render properties follow the [official ReaScript API](https://www.reaper.fm/sdk/reascript/reascripthelp.html#GetSetProjectInfo).
4. Compare **every sample**, including silence, without alignment, cropping or gain fitting. Require exactly 120000 frames, matching rate/channels, finite samples and maximum absolute error <= **1e-5** (unchanged tolerance). Render the same default project a second time and compare it too.
5. Automate Depth Low Band from 0 to 100% over the audible two seconds; require finite, bounded audio that differs from defaults. Save both default and automated projects.

Outputs live under `out/<prefix>_*`: independent HostTest reference, first/repeat/automation renders, logs and projects. A missing or incompatible HostTest fixture fails before launching REAPER. This does **not** create or bless a platform-specific golden. Run the original HostTest suite against `plugin/tests/golden` as well; an on-demand integration reference alone cannot detect DSP changes shared by both hosts.

## RC1b failure investigation, 2026-10-30

**Conclusion: mismatched startup and transport conditions, not a Mac-specific sound regression.** Product code, product binaries, the existing golden WAVs and the 1e-5 comparison tolerance were unchanged.

### Original golden and comparison

`plugin/tests/golden/default.wav` was added in commit `0a19f9e` on October 27. `CLAUDE_CHANGES.md` records generation by **HostTest --make-golden on Windows**, not REAPER. Surrounding results identify the local Windows 11 / MSVC 2022 / JUCE 8.0.8 setup and mini PC runs; there is no per-file provenance manifest recording the exact historical hostname or Windows build. A REAPER version does not apply to this golden. SHA256: `7c5ff76068d2e3b371b4f58a34a5e1083cca8c5e304dfb9ee5cc0d7d9cba8067`.

The reference is 48000 Hz, stereo, 96000 frames of float audio, processed in 512-frame blocks **after 256 silent frames**, with no external playhead. JUCE's fallback state is 120 BPM, stopped, PPQ zero; the synced LFO consequently free-runs. The old Python test compared interleaved samples from frame zero through the shorter file, without alignment, using maximum absolute difference <= 1e-5 and allowing up to 4096 frames of length difference. It did not verify sample rate or channels. The actual original files all had identical 48000 Hz/stereo/96000-frame dimensions.

### Measurements

Current test machines: M1 / macOS 26.6 (25G72) / native REAPER 7.84; MINI-PC / Windows 11 Pro 25H2 (26200.9711) / REAPER 7.83 x64. Mac plug-ins are the original CI artifacts from f24dc90; Windows RC build reports 4ee4fc0 (the f24dc90 delta changes UI tests only). Product binaries were hashed before/after and unchanged.

| Comparison | Maximum absolute difference |
|---|---:|
| Original Mac-day VST3 vs old golden | 0.285875860 |
| Original Mac-day AU vs old golden | 0.244381833 |
| Original harness rerun on Windows vs old golden | 0.173396597 (fails too) |
| Fresh Mac HostTest VST3 or AU vs old golden | 2.384185791e-7 (-132.5 dBFS) |
| Original Mac VST3 repeated in fresh REAPER instances | 0.020618618, confined to first 480 frames; exactly 0 afterward |
| Original Mac AU repeated | 0 |
| Original Mac VST3 vs AU reruns | 0.020618618, confined to first 480 frames; exactly 0 afterward |
| Original Windows repeated | 0.001829892, confined to first 480 frames; exactly 0 afterward |
| Original Windows vs Mac, after first 480 frames | 2.384185791e-7 |
| Original REAPER vs golden after first 2400 frames, either OS | 0.015491247 |
| Mac with phase-matched start, vs golden after 2400 frames | 2.384185791e-7 |
| Corrected REAPER vs matched HostTest reference, Windows / Mac VST3 / Mac AU | **0**, all 120000 frames |
| Corrected same-project repeat, each format/platform | **0**, all 120000 frames |
| Corrected Mac VST3 vs AU, and separate fresh VST3 run | **0**, all 120000 frames |
| Corrected Windows vs Mac (either format) | 2.384185791e-7; RMS 2.013618221e-8 |

The original golden comparison's best audio correlation lag was **0 samples** (searched +/-4096). Fitting a constant gain of 1.004620046 left a 0.285861577 peak error, so neither an audio offset nor a constant level correction explains it. All files were float 48k stereo; original RPPs show no item fades and `RENDER_DITHER 0`.

Two source-backed causes account for the discrepancy:

- **Startup state:** HostTest's 256 silent samples leave 2144 frames of the 50 ms input/output gain ramps still rising. REAPER's stopped processing has already settled those ramps. Stopped processing also free-runs the LFO; starting transport fades from that arbitrary phase to host PPQ over 10 ms. This explains the first 480 samples varying between identical runs, on both platforms. `PluginProcessor.cpp`: prepareToPlay gain ramp setup, updateState gain targets, forceSynchronization/play latch and relockToHost; `LFO.h`: jumpTo.
- **LFO phase:** HostTest's extra 256 samples advance the default 2 Hz LFO by 5.333 ms, or 3.84 degrees. REAPER follows playing PPQ and tempo, resets on play and relocks after position jumps; the old reference has no playing clock. In a controlled Mac experiment the input started at 48256/48000 s (two full LFO cycles plus 256 samples). Cropping only that known silent lead, the remaining periodic error fell from 0.015491247 to 2.384185791e-7 after the startup ramp. The audio itself was never time-shifted against the reference. The first 50 ms still differed, confirming that phase and gain startup are separate causes.

Peak difference envelope, time measured from the original signal start (not from the experimental silent lead):

| Interval | Original Mac VST3 | Original Mac AU | Phase-matched Mac VST3 |
|---|---:|---:|---:|
| 0-10 ms | 0.285875860 | 0.244381833 | 0.266510847 |
| 10-50 ms | 0.171902984 | 0.171902984 | 0.179549754 |
| 50-100 ms | 0.005799562 | 0.005799562 | 2.086162567e-7 |
| 100-2000 ms | 0.015491247 | 0.015491247 | 2.384185791e-7 |

The rebuilt test helper also passed the original golden checks: Windows default/varied/free bit-identical; Mac default 2.384e-7, varied 3.278e-7, free 2.086e-7. The existing M1 VST3 HostTest suite remained 58 passed, 0 failed, 1 Windows-only skip. Automation checks passed on all three integrations; they assert sanity/change, not cross-host sample equivalence of automation scheduling.

Raw artifacts, scripts, 10 ms CSV envelope and plot are preserved at `C:\Users\Joe\.wla-test\macday-rc1b\investigation\` and `~/macday/investigation` on the M1; corrected Mac renders also remain in `~/macday/in/src/plugin/tests/reaper/out`. Original failures remain in the original Mac-day result directory. No Logic, reboot, Tailscale or sharing changes were made.
