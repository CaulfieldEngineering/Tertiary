# REAPER test (prepared, not run)

Written by Claude, 2026-10-27. REAPER isn't installed on the mini PC; Joe hasn't decided whether to (it is a free 60-day evaluation that keeps working, then a $60 licence). Nothing here has been run yet, so expect a first-run fix or two: the REAPER command-line options and the render-format string are marked "unverified" in the files.

## What it does
1. `HostTest --write-input` writes the golden renders' input (2 s, 48 kHz) to `input.wav`.
2. REAPER starts and runs `tertiary_reaper_test.lua`, which builds the project from scratch: one track with `input.wav`, Tertiary (VST3) on it, renders the whole project, then adds an automation envelope on "Depth Low Band" (0 to 100 % over 2 s), renders again, saves `tertiary_test.rpp` and quits.
3. `run_reaper_test.py` compares `out/reaper_default.wav` with `../golden/default.wav` (the same input and settings rendered by HostTest), within 1e-5, and checks the automation render is sane and different.

The project is built by the script rather than written by hand because a REAPER project stores the plug-in's state as a machine-specific blob; the script saves `tertiary_test.rpp` on its first run (with Tertiary and the automation), and that file can be committed and opened by hand afterwards.

## Run it on the mini PC
1. Install REAPER, and install Tertiary (or copy the built `Tertiary.vst3` to `C:\Program Files\Common Files\VST3`). Open REAPER once so it scans plug-ins.
2. `python plugin\tests\reaper\run_reaper_test.py <build>\plugin\HostTest_artefacts\Release\HostTest.exe`

It prints PASS/FAIL lines. `plugin\tests\run_all.ps1` could call it as a last step once REAPER is installed.

## Run it in CI
GitHub's runners don't have REAPER. A job could download the portable REAPER build from reaper.fm, install Tertiary from the build, and run the same script (on Windows it can run without a visible desktop session; on macOS the runners have one). Two things to decide first:
- REAPER's licence: the evaluation is for evaluating; using it in automated CI is a question for Cockos, or buy the licence.
- It adds a few minutes per platform.
