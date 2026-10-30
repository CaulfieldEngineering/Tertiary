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

## Status (2026-10-29)

- Install, launch and open all pass unattended.
- **Export does not work yet by automation.** The Export Audio Mixdown dialog opens with the right settings, but
  the synthetic click on **Export Audio** does nothing: no progress window and no file. Everything that was tried is
  listed in `CUBASE-PLAYBOOK.md` under "Tertiary harness run, and where it stopped". The next step is for Joe to
  press it once by hand.
- There is no golden yet, so every run before that ends `NOGOLD` or `STOPPED`.
