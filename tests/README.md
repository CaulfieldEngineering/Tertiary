# Release test scripts

<!-- [Codex 2026-10-30] Joe's individually runnable scripts and complete release report. -->

AI writes these scripts and reviews their evidence. The scripts run the tests and decide PASS/FAIL. Existing
DSP, UI and DAW harnesses stay under `plugin/tests`; this folder selects the repo's build and collects their results.

```text
tests/
  sequence.json               ordered release checklist, shared by both operating systems
  win/                        Test-*.ps1, Run-All.ps1, portable Cubase helpers
  mac/                        test-*.sh, run-all.sh, Logic bounce and Mac-day screenshots
  web/                        web suite, iPhone/manual, staging-preview and post-release wrappers
  lib/                        shared runner, result format, build selection and host cleanup
  results/YYYY-MM-DD_HHMM/     <name>.json, <name>-artifacts/, summary.md, summary.json
```

## Run

Requires Python 3.9 or later (`python` on Windows, `python3` on Mac), Windows PowerShell 5.1 or later / Bash,
and a local build. Launch from any working directory. `WLA_TEST_PYTHON` selects another Python interpreter.

```powershell
# Windows: one check, or the full ordered sequence
powershell -NoProfile -ExecutionPolicy Bypass -File tests/win/Test-Reaper.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/win/Run-All.ps1
# Optional: a different build INSIDE this repo and a result destination
powershell -NoProfile -ExecutionPolicy Bypass -File tests/win/Test-Suite.ps1 -BuildDir build-rc -ResultsDir tests/results/my-run
```

```bash
# Mac: one check, or everything (do not open Logic for this verification)
bash tests/mac/test-reaper.sh
bash tests/mac/run-all.sh --no-logic
bash tests/mac/test-suite.sh --build-dir build-ci --results-dir tests/results/my-run
```

There are no host downloads, host installations, reboots or privacy-setting changes. Missing installed hosts
are SKIP. On SSH, the Mac wrapper uses the already logged-in desktop's Aqua session through passwordless
`sudo launchctl asuser`; without a desktop session GUI checks are SKIP. It never performs Logic's first launch.
`WLA_NO_LOGIC=1` also disables opening a future Logic harness. Existing Cubase, REAPER, standalone or Logic
sessions are left alone; the corresponding test reports SKIP. A machine-wide OS lock prevents overlapping
release scripts from temporarily replacing the same plugin.

Build with the repository's normal CMake setup, `WLA_TESTS=ON`, `WLA_REVIEW_RENDER=ON` and
`WLA_COPY_PLUGIN_AFTER_BUILD=OFF`. Windows targets are `<product>_VST3`, `<product>_Standalone`, `HostTest`,
`<product>Tests`, `ReviewRender`, `UiTest` and `PerfTest` when supplied by the product. Mac additionally builds
`<product>_AU` when AU is in `wFORMATS`. Product-specific test targets absent from a new template are explicit
coverage gaps; the wrapper does not create synthetic passes for them.

Default build paths (all relative to this repo, read from `config.cmake`):

```text
build-tests/plugin/<product>_artefacts/Release/VST3/<product>.vst3
build-tests/plugin/<product>_artefacts/Release/AU/<product>.component
build-tests/plugin/<product>_artefacts/Release/Standalone/<product>.exe or <product>.app
build-tests/plugin/<test-target>_artefacts/Release/<test-target>[.exe]
```

CI bundles can instead be copied into a flat **repo-local** build folder containing `<product>.vst3`,
`<product>.component`, `<product>.app`, `HostTest`, `UiTest`, etc. Preserve a provenance note with the source
CI run/commit. Scripts always print the tested file and its embedded build ID, when present, plus its SHA256.
The checkout's current commit is never passed off as a previously compiled binary's identity. Build paths
outside this repo, including symlinks to another worktree, are rejected. Installed binaries are never fallback builds.

## Checklist and prerequisites

Execution order is in `sequence.json`. Reports display failing scripts/checks first while retaining each script's
checklist index. Adding a future test takes one sequence entry and an independently executable wrapper.

| Script / row | What runs or is intended | Installed tools / fixtures |
|---|---|---|
| Suite | Windows wraps `plugin/tests/run_all.ps1`: unit, HostTest, pluginval, render matrix, UI robot; product LFO scripts follow. Mac runs the Mac-day auval, pluginval with and without GUI, VST3/AU HostTest, unit, UI robot, renders, screenshots and LFO phases. | Built test programs; pluginval; Python. Windows retains the existing pinned pluginval download into the build folder. Mac: `WLA_PLUGINVAL`, `/Applications/pluginval.app` or `build-tests/pluginval/pluginval.app`. Screenshots need command-line tools and screen capture permission. |
| Standalone | Windows reuses the pywinauto control smoke test; Mac starts the built app, waits, captures it and quits. Settings are restored. | Python `pywinauto`, Pillow on Windows; built app; product control names in config. |
| TwoInstance | Reuses HostTest section 11 from `fix-instance-isolation`; a binary without the section's completion marker is SKIP. | Updated HostTest and VST3. A template without this product check reports PLANNED. |
| ASan | Uses a repo-local `build-asan` with `WLA_UI_TEST_ASAN:BOOL=ON`; otherwise PLANNED. Sanitizer errors fail. | Instrumented UiTest. The script does not silently rebuild the normal plugin. |
| Performance | Existing PerfTest DSP, 120 seconds each of open/closed/idle, then 30 instances for 60 seconds. Stores measurements; an agreed budget/sign-off remains PLANNED. | PerfTest where the product supplies it; roughly 8 minutes. |
| Cubase | Existing export automation, machine golden comparison, normal Quit. | Cubase Pro 12, interactive console, pywinauto, psutil, NumPy, SciPy, Pillow, product `.cpr` and media builder, working audio output, golden `harness.wav`. `WLA_CUBASE` overrides host location. |
| REAPER | Existing Lua/Python harness, matched transport render, repeatability, automation and saved projects; VST3 plus AU on Mac. | REAPER (`WLA_REAPER` override), updated HostTest, psutil on Windows. Product harness path from config. |
| Logic | PLANNED: `no Logic harness project yet`. Once present: stage AU, auval, copy/open harness, System Events bounce, compare golden, quit. `--no-logic` skips opening even when a project exists. | Logic already set up by Joe; configured `tests/mac/logic/<product> harness.logicx`, default Cmd+B binding, saved cycle/PCM/WAVE/48 kHz/32-bit float/offline settings, golden `logic/harness.wav`, UI scripting permission. The bounce adapter is unverified until that project exists. |
| FLStudio, Ableton, Bitwig, StudioOne | PLANNED: local-build scan, tempo, automation, save/reopen and golden bounce. | Future product harnesses and installed hosts. |
| Installer (each OS) | PLANNED: signed staging installer, files/build ID, upgrade/uninstall; Mac also notarisation/Gatekeeper. | Future disposable installation check; normal runs do not change installed products. |
| Microphone (Mac) | PLANNED: V-08 permission prompt and actual input. | First-launch fixture and input source; no TCC reset here. |
| Web | `npm test -- --no-ai` in `plugin/tests/web`, importing its report and screenshots. Missing suite: SKIP `web suite not present`. Missing dependencies/build: SKIP. | Node/npm, suite dependencies and Playwright browsers, repo web build made with that suite's `npm run build`. Does not build or alter `web-tests`. |
| Safari (Mac) | PLANNED: real Safari via safaridriver. Playwright WebKit is a separate engine test. | Future real Safari adapter; no Remote Automation settings changed here. |
| iPhone Safari | PLANNED/manual: physical-device look/listen with build ID and reviewer. | Joe's later review. |
| Staging preview | PLANNED: private preview serves this exact build ID. | Future configured preview URL and identity check. |
| Post-release | PLANNED: call `npm run post-release` in `wla-site-tests` **after** an approved tag. | Deliberately not run by this pre-release sequence. |

The web wrapper accepts the concurrent suite's Node/Playwright `out/report.json` contract (`rows` containing
`id`, `status`, `title`/`error`) and copies `out/` to this run's artifacts. Future multi-browser reports should
include `browsers: ["chromium", "firefox", "webkit"]` only for engines actually executed. Until those engines
are implemented, their rows remain PLANNED. The existing desktop Chromium and mobile WebKit rows are recognized
from their completed checks and runner; an
unknown report never certifies all three engines. AI review is disabled during execution and can be done on
the completed reports afterwards.

## Staging and cleanup

REAPER gets its own throwaway ini/profile with the repo build's VST path. The harness inspects REAPER's mapped
module while the plugin is loaded, checking the VST3 path and SHA256; AU checks the staged binary's hash. A
render without proof of the loaded build fails. The small Lua defer is for this inspection; the original audio
comparison is unchanged. REAPER's type identifier is also available through its
[documented ReaScript API](https://www.reaper.fm/sdk/reascript/reascripthelp.html#TrackFX_GetNamedConfigParm).

The profile supplies both Intel and ARM VST path keys and keeps REAPER's literal standard-folder tokens
before the repo path, so REAPER does not append a duplicate installed-product folder after it. Windows
reuses only unrelated plugin scan metadata to shorten initial scanning. Update/evaluation dialogs are
handled through their named Close/Still Evaluating buttons or About window's own Close after its countdown;
no licence file is copied or changed. A host
that renders successfully but fails to exit still reports FAIL, with the audio assertions retained.

Cubase uses a temporary per-user VST3 copy. If the common VST3 slot also contains this product, the same build
is staged there during the run to prevent shadowing. Every previous file/bundle is renamed to a unique backup
beside its original location, restored in `finally`, and never discarded on a failed restore. A restore journal
is kept in the result artifacts. Permission errors fail before Cubase starts. Goldens are only read, never updated.
The existing menu/window/wait/screenshot helpers were brought in from Joe's production pipeline tools;
the product export and comparison remain in `plugin/tests/daw/cubase`.
The moved-project prompt selects the new harness directory. A recognized Cubase 12 Safe Mode prompt keeps
current preferences; it never disables plugins or deletes preferences. Unknown dialogs stop the test with
their window text and a screenshot. The configured audio output must be available for the Cubase harness.
The export adapter replaces the CPR's saved absolute export directory with this run's directory, verifies
the native edit's value, and refuses to export if it cannot verify that path.

AU uses the user's Components slot, with the same backup/restore discipline and AudioComponentRegistrar reset
before validation and after restoration. Standalone runs directly from the repo build. The screenshot phase
was moved from `HQ/scripts/mac-day-build-checks.sh`: four scales, actual window pixels, a temporary Retina display
when needed, settings restored and the temporary display released. It does not certify visual design approval.

Every child has a timeout. Screenshots and logs accompany failures. Cubase normally quits through its own
Ctrl+Q, answering only the copied harness's exact Save / Don't Save / Cancel prompt. If that fails, the wrapper
tries normal Quit for 30 seconds, captures the dialog, and **only then** terminates the PID it started. This is
the documented last-resort fallback; it makes the test FAIL. It never kills a Cubase session found open at
preflight. REAPER and standalone similarly clean up only their own test processes. A hard power loss cannot
run `finally`; use any un-restored journal's backup path to recover the previous slot before testing again.

## Results

Each script writes the same schema and exits `0=PASS`, `1=FAIL`, `2=SKIP`, `3=PLANNED`:

```json
{
  "name": "reaper", "platform": "win", "status": "PASS",
  "started": "2026-10-30T20:00:00-04:00", "finished": "2026-10-30T20:00:20-04:00",
  "build": {"path": ".../VST3/Product.vst3", "id": "embedded build id; sha256:..."},
  "host": {"name": "REAPER", "version": "7.x"},
  "checks": [{"id": "reaper-vst3", "status": "PASS", "detail": "..."}],
  "artifacts": ["reaper-artifacts/reaper-vst3.log"]
}
```

PLANNED means the intended check is not written/provisioned yet and prints `Future placeholder: ...`.
SKIP means an implemented check cannot run here, with its reason. PASS certifies only the checks named.
Mixed script precedence is FAIL, PLANNED, SKIP, PASS; the individual check rows preserve partial success.
The meta-script continues past failures, missing reports and result/exit mismatches, writes both summaries
after every script, and exits 1 if any check failed (otherwise 0, even with gaps). Coverage counts the named
checks, including staging/restoration checks; it does not equate the number of rows with release approval.
Artifacts use paths relative to the result folder so copied reports retain working links. Results and binary
artifacts are ignored by Git. Repeated runs in the same minute receive `-01`, `-02`, etc.

To retry one script, supply the original `ResultsDir`/`--results-dir`; its previous artifacts are retained
under `history/`. Then rebuild the complete summary from all the current per-script results:

```text
python tests/lib/run.py summarize --results-dir tests/results/my-run
```

This requires every row for that operating system to have a result; it does not fill absent tests with passes.

## New plugins and runner checks

The same skeleton is carried in NewPluginTemplate. Product name, codes, build directory, golden directory,
settings and smoke-control names come from `config.cmake`. Empty product-specific harness/control settings
remain PLANNED until the new plugin supplies them. Do not copy another plugin's audio golden or DAW project
and call it a test of the new plugin. Existing `plugin/tests` suites remain the owners of their assertions.

```powershell
python -m unittest discover -s tests/lib -p test_runner.py -v
```

These tests exercise failed staging/restoration, build-path containment, all four statuses and coverage,
and meta-script continuation after a child dies without a report. They do not open any host or touch an
installed plugin.
