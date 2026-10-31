# Release test scripts

<!-- [Codex 2026-10-30] Joe's individually runnable scripts and complete release report. -->

AI writes these scripts and reviews their evidence. The scripts run the tests and decide PASS/FAIL. Existing
DSP, UI and DAW harnesses stay under `plugin/tests`; this folder selects the repo's build and collects their results.

```text
tests/
  sequence.json               ordered release checklist, shared by both operating systems
  win/                        universal Test-*.ps1, Run-All.ps1, Run-All-Product-Tests.ps1
    product/                  this plugin's Test-*.ps1 scripts
  mac/                        universal test-*.sh, run-all.sh, run-all-product-tests.sh
    product/                  this plugin's test-*.sh scripts
  web/                        web suite, iPhone/manual, staging-preview and post-release wrappers
  lib/                        shared runner, result format, build selection and host cleanup
  results/YYYY-MM-DD_HHMM/     <ID>.json, <ID>-artifacts/, summary.md/json, product-summary.md/json
```

## Run

Requires Python 3.9 or later (`python` on Windows, `python3` on Mac), Windows PowerShell 5.1 or later / Bash,
and a local build. Launch from any working directory. `WLA_TEST_PYTHON` selects another Python interpreter.

```powershell
# Windows: one check, or the full ordered sequence
powershell -NoProfile -ExecutionPolicy Bypass -File tests/win/Test-Reaper.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/win/Run-All.ps1
# Optional: a different build INSIDE this repo and a result destination
powershell -NoProfile -ExecutionPolicy Bypass -File tests/win/Test-HostTest.ps1 -BuildDir build-rc -ResultsDir tests/results/my-run
```

```bash
# Mac: one check, or everything (do not open Logic for this verification)
bash tests/mac/test-reaper.sh
bash tests/mac/run-all.sh --no-logic
bash tests/mac/test-hosttest.sh --build-dir build-ci --results-dir tests/results/my-run
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

The 35 Blocks Tests IDs and Suite relation IDs are recorded in `sequence.json`, plus the retained
`T2-03` performance test. `ci_ok` mirrors the GitHub checkbox. One row owns one script per supported
platform; the old combined Test-Suite wrappers have been removed. Reports use the registry ID in every
script row and every check (`test_id`); internal assertion IDs remain separate.

Product runners live in `win/` and `mac/` and discover all `Test-*.ps1` / `test-*.sh` files directly in that
platform's `product/` folder. There is no top-level `tests/product/` and no second runner list. Register
metadata in `sequence.json`; locally an unregistered file is still executed, then explicitly fails the
result contract. In CI it fails without execution because it has no registered GitHub checkbox.
Main Run-All invokes the product runner once and merges its results. Each has its own summary.
Product checks run together first; the final report follows registry order, with failures first.

Tertiary product scripts: W-01 unit, W-05 LFO, W-06 Windows UI robot and M-04 Mac UI robot. Mac unit/LFO
counterparts retain W-01/W-05, and the existing Mac render/isolation/ASan checks retain W-04/W-14/W-15:
these are the same checks on another platform, not new IDs invented outside Blocks. `suite_ids` preserves
the database relation; the summary also names the platform suite actually invoked.

NewPluginTemplate's two product folders contain only a README. Its product runners say **no product tests
yet**. The existing generic processor-invariant unit binary still runs through W-01 at the platform root
(scope `universal` in the template); its five real generic sections and existing empty product section
are retained. Actual product scenarios and their wrappers are written in situ when a plugin is developed.

```powershell
powershell -NoProfile -File tests/win/Run-All-Product-Tests.ps1
pwsh tests/win/Run-All.ps1 -Profile ci -BuildDir build -ResultsDir tests/results/ci -ExpectDemo
```

```bash
bash tests/mac/run-all-product-tests.sh
bash tests/mac/run-all.sh --profile ci --build-dir out --results-dir tests/results/ci --expect-demo
bash tests/web/test-web.sh --profile ci --engine chromium --results-dir tests/results/ci
```

The `ci` profile selects checked rows, excludes DAWs/humans/Mac-day-only rows, and delegates WEB-01 to the
separate web job. M-01 also requires AU in `config.cmake`; a VST3-only template records it as not applicable. M-05/06/07 retain checked GitHub flags but need a real Mac desktop/Retina/microphone session;
they are explicitly listed as excluded, not silently marked passed. T2-03 is local-only as before and its
original 30-instance deadline failure is not suppressed. ASan without an instrumented build remains
PLANNED; it has never been part of the prior CI baseline. Missing prerequisites for selected implemented
CI checks fail the run. Template product scenarios not written yet are listed as absent.

CI and local wrappers execute the same assertions. `ci-baseline.json` retains pre-layout counts, and result
metrics compare them with the current unit/UI/HostTest/render counts. Mac M-03 also preserves architecture
and minimum-OS checks from the old CI script. CI uses the existing quick render matrix (36 Tertiary files);
local runs use the full matrix. Set `WLA_TEST_REFERENCE` to an approved release-render directory for the
W-04 previous-release comparison; without it that comparison is PLANNED, never an implied sound match.

Both workflows print `summary.md` before artifact uploads and upload `tests/results/`. Native tests run
before the universal-build upload, so a quota error cannot prevent the first Mac's tests. Every test/staging
artifact has seven-day retention; release/tag installer retention stays at its previous repository default.
Cross-machine comparisons still require the transferred artifacts; a quota failure is not a green CI run.

| Script / row | What runs or is intended | Installed tools / fixtures |
|---|---|---|
| W-01 Unit | Existing unit binary, with assertion-section count. | Built `<product>Tests`. |
| W-02 / M-03 HostTest | Full VST3 host suite; AU too on Mac. | Built HostTest; `-ExpectDemo` / `--expect-demo` for unlicensed CI. |
| W-03 / M-02 pluginval | Strictness 10; all GUI completion markers required. Mac local also runs the no-GUI pass. | `WLA_PLUGINVAL` or pluginval in the build folder. |
| W-04 Renders | Existing render matrix; optional approved-release comparison. Also on Mac with the same ID. | ReviewRender, sample audio. |
| W-05 LFO | This repo's configured regression scripts. | ReviewRender. |
| W-06 / M-04 UI robot | All existing assertions, snapshots, private per-run temp/settings directory. | UiTest. |
| M-01 auval | Strict Apple AU validation of the staged repo build. | AU build, logged-in Mac session. |
| M-05 Screenshots | All four scales and Retina from the existing Mac-day script. | Real Mac display and screen-capture permission. |
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
  "id": "W-09", "name": "W-09", "title": "REAPER",
  "suite_ids": ["SU-01"], "profile": "local", "platform": "win", "status": "PASS",
  "started": "2026-10-30T20:00:00-04:00", "finished": "2026-10-30T20:00:20-04:00",
  "build": {"path": ".../VST3/Product.vst3", "id": "embedded build id; sha256:..."},
  "host": {"name": "REAPER", "version": "7.x"},
  "checks": [{"test_id": "W-09", "id": "reaper-vst3", "status": "PASS", "detail": "..."}],
  "artifacts": ["W-09-artifacts/reaper-vst3.log"]
}
```

PLANNED means the intended check is not written/provisioned yet and records its reason.
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

This reads only result files present in that run; it never fills absent tests with passes. The original full-run summary records profile exclusions.

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
