# Cubase harness for Tertiary

The Cubase part of the per-DAW release test (HQ `plans/tertiary-daw-test-plan.md`, Cubase section). It was built
2026-10-29 in Joe's music folder and moved here the same day. Joe: "the plug-in specific test harnesses should
probably be stored in their respective Plug-In repo project folders".

```
plugin/tests/daw/cubase/
  README.md
  tertiary_harness_build.py    builds the harness's parts from files, no Cubase needed
  tertiary_harness_run.ps1     one unattended Cubase run: install, open, export, quit, compare with the golden
  Tertiary harness/
    Tertiary harness.cpr       the project (Cubase Pro 12): one audio track, Tertiary in insert 1, tempo map, automation
    .gitignore
    Misc/                      harness-tempo.smt, harness-track.xml (generated) and learn-1..3.xml (Cubase's own
                               Selected Tracks exports; learn-3.xml is the template the archive is written from)
    Audio/                     NOT in git: harness-input.wav, rebuilt by the build script (below)
    Images/ Mixdown/ Runs/ ... NOT in git: peak caches, exports, run logs
```

## The song

It is 4/4, bars 1 to 13, about 24.8 s: 120 BPM, a ramp to 90, 90, then a jump to 140. Six Low-band and crossover
lanes are automated. The details are in the build script's docstring. The song is kept short because an unactivated
Tertiary bypasses itself after 50 s of processing.

## Rebuilding

From this folder:

```
python tertiary_harness_build.py audio     # Audio/harness-input.wav from HostTest --write-input (needs a HostTest build;
                                           # finds build-tests/ in this repo, or pass its path). Byte-identical to the
                                           # file the .cpr was made with (checked 2026-10-29).
python tertiary_harness_build.py tempo     # Misc/harness-tempo.smt
python tertiary_harness_build.py archive   # Misc/learn-3.xml -> Misc/harness-track.xml
```

To build the .cpr itself from these parts in Cubase, follow the recipe in `CUBASE-PLAYBOOK.md` ("the Tertiary
harness project, built from files"). Run `audio` once after cloning: the run script stops if the WAV is missing.

The .cpr stores absolute media paths. Opened from this folder on 2026-10-29, Cubase found
`Audio/harness-input.wav` relative to the project. There was no Resolve Missing Files prompt, no project-moved prompt
and no save prompt on Ctrl+Q.

## Running

```
powershell -NoProfile -ExecutionPolicy Bypass -File plugin\tests\daw\cubase\tertiary_harness_run.ps1 [-MakeGolden]
```

- Cubase must be closed. The script never closes or kills Cubase except with Ctrl+Q on its own run.
- Cubase needs an audio output that is always there (see "Audio output" below), or the export does nothing.
- **Goldens are per machine**, so they are not in git. The default place is
  `%USERPROFILE%\.wla-test\goldens\tertiary\cubase\harness.wav`; use `-Golden <folder>` to change it.
- Each run leaves `Tertiary harness\Runs\<date-time>\` and a line in `Runs\results.log`, both git-ignored.
- Renders are compared by `plugin/tests/regression/compare_renders.py` in this repo.

### Depends on Joe's generic Cubase helpers (not in this repo)

The run script drives Cubase through four generic helpers. They serve every Cubase project, so they stay in Joe's
pipeline tools:

| Helper | Does |
|---|---|
| `cubase_windows.ps1` | lists Cubase's windows (title, class, rect, front marker) |
| `cubase_wait.ps1` | waits for the project window and answers only known-safe prompts |
| `cubase_menu.ps1` | runs a menu item by path (`File>Export>Audio Mixdown...`) |
| `screen.ps1` | focus, click, key and screenshot |

The default folder is `%USERPROFILE%\Dropbox\Joe's Music\_Production\_PIPELINE\tools`. Override it with `-Tools <folder>`
or `$env:WLA_CUBASE_TOOLS`. The script checks that all four helpers are there before it starts.

## Audio output (the fix that made export work, 2026-10-29)

Cubase needs a working audio device for its engine to run. Without one, audio tracks do not play and
**File > Export > Audio Mixdown silently does nothing** (the button looks pressed, no progress window, no file),
in every project. On the mini PC, Stereo Out used to go through the Generic Low Latency ASIO Driver to the TV's
HDMI audio ("2 - D40f-J09 (AMD High Definition Audio Device)"), which disappears when the TV is off. Joe,
2026-10-29: "can we just set up a dummy driver or something? we'll never play cubase out of the tv".

Fix: **VB-Audio Virtual Cable** (free, vb-audio.com, Driver Pack 45) is installed, and Cubase plays into it:

- Generic Low Latency ASIO Driver, Output = **CABLE Input (VB-Audio Virtual Cable)**. The setting is in the registry,
  `HKCU\Software\ASIO\Generic Low Latency ASIO Driver`, value `Output` = the endpoint ID; it can be changed there
  with Cubase closed (Studio Setup > Generic Low Latency ASIO Driver does the same by hand).
- Stereo Out Left/Right = `CABLE Input (VB-Audio Virtua 1/2` (saved in `Tertiary harness.cpr`).
- Cubase's Missing Ports dialog has **Map Always** set from the TV's two ports to the cable's, so older projects
  open on the cable too (`%APPDATA%\Steinberg\Cubase 12_64\Port Setup.xml`, list `Mappings`).

The cable is always present, needs nothing plugged in, and nobody hears it. Windows' default playback device was
not changed. To hear Cubase, pick another output in Studio Setup (the TV still shows there when it is on).

## Status (2026-10-29, night)

- Install, launch, open, export, quit and compare all pass unattended: about 30 s per run.
- **Golden:** `%USERPROFILE%\.wla-test\goldens\tertiary\cubase\harness.wav`, made 2026-10-29 23:45 from build
  `84FE898419E6D05A`. Marked "first run, Joe to listen" in `golden-info.txt` beside it. A second run matched it bit
  for bit (PASS). L peak -4.52 dBFS, RMS -18.20 dBFS; R peak -5.66 dBFS, RMS -20.06 dBFS; 24.773 s.
- The export makes Cubase mark the project changed, so Ctrl+Q asks to save. The run script answers **Don't Save**,
  only when the buttons are exactly Save / Don't Save / Cancel and the harness is the only project open.
- **Export Range:** the list under the Locators tab is always empty; that list is only for the Cycle Markers tab.
  Locators mode exports bar 1 to 13 as it is. No cycle marker is needed.
- The export dialog remembers its format across projects. On 2026-10-29 it had been left on AIFF, 16 kHz, 16 bit
  and was set back to Wave, 48 kHz, 32 bit float. If a run ever fails on format, check the dialog first.
- The harness track is not record-armed in the saved project.
