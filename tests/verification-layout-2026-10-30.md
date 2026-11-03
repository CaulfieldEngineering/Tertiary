# Test-layout follow-up verification, 2026-10-30

Joe authorized this follow-up while a separate run fixes first-block performance on
`fix-first-block`. That worktree, the plug-in source and the T2-03 threshold were not changed here.

## The two investigations

- **Real first-callback performance defect:** the original and approved RC1b binaries took
  17–18 ms for the first 30 serial callbacks against a 2.667 ms deadline. Independent thread-cycle
  measurements reproduced the cost; warm batches took about 0.37 ms. Preparation marks three LFO
  tables dirty, and the first audio callback rebuilds them. Scheduling noise also affects paced
  stress counts but does not explain this cold callback work. The separate fix remains necessary.
- **REAPER harness defect, not an audio mismatch:** its evaluation/About modal blocked focus and
  normal Quit. The previous fixed delay could start rendering while that modal was still present.
  Waiting for the named enabled button and for the modal to disappear fixed the launch/exit flow.
  This follow-up again passed: native Quit exited zero after 29.09 s, the reference and repeat
  renders matched exactly over 120,000 frames, automation was finite/bounded/different, and both
  projects were saved.

Full original evidence: [investigation-2026-10-30.md](investigation-2026-10-30.md).

## Changes verified

- All 35 Blocks test IDs, GitHub checkbox values and Suite relations match `sequence.json` in both
  repositories. T2-03 remains an additional real local test.
- Each row has its platform wrapper; product folders are `win/product` and `mac/product`.
  Product runners discover files, produce their own summaries, and are invoked by the main runners.
  NewPluginTemplate's product folders contain only READMEs; its existing generic unit checks remain
  outside those folders. An empty folder explicitly reports “no product tests yet”.
- Both workflows reuse the platform/web scripts. CI excludes DAWs, human and Mac-day-only rows.
  An unregistered product script is executed locally and reported as an ID error; CI reports that
  error without executing it because no registry checkbox authorizes it for CI.
- Existing build/sign/deploy steps were compared with the pre-layout revisions and preserved.
  Test/staging uploads retain artifacts seven days; release/tag installer retention is unchanged.
  Summaries print before artifact uploads, and the first Mac's tests run before transferring its build.
- Nineteen runner/restoration contract tests pass in each repository, including missing/stale
  results, incorrect IDs, failed restoration, exhaustive discovery, CI eligibility and AU applicability.
  Python, PowerShell and Bash syntax checks and workflow YAML/retention checks passed.

## Windows

Command: `powershell -NoProfile -ExecutionPolicy Bypass -File tests/win/Run-All.ps1 -ResultsDir tests/results/layout-windows-final`.

Result: **16 PASS, 1 FAIL, 2 SKIP, 14 PLANNED checks** (8 PASS, 1 FAIL, 2 SKIP, 13 PLANNED script rows).
Exit 1 is correct: T2-03 remains failed. No failure was converted into a pass or excluded from the local run.

| Evidence | Before layout / previous CI | This local run |
|---|---:|---:|
| Unit sections | 43 | 43 |
| UI assertions | 493 | 493 |
| HostTest VST3 passes | 90 | 96 |
| Renders | 36 quick | 63 full |

Both LFO regressions, strictness-10 GUI pluginval, all standalone smoke assertions, REAPER and
two-instance isolation passed. Product aggregation passed all three scripts (six report checks).
The independently invoked product runner also passed all three scripts/six checks in
`tests/results/layout-product-standalone/`, once the other run's test processes had finished.
The render/reference row remains PLANNED overall because no approved-release render directory was
provided; generating 63 renders is not a claim that they match the previous release.

Cubase was already running the warm-up worktree's fixture and was left untouched. The local web
build was absent. Other planned rows include unavailable DAWs, ASan, human/release checks and an
agreed performance budget. These are open coverage, not release approval.

T2-03: 30 instances, 128 samples, 60.447 s, exit 3; 68 late blocks, 7 missed periods,
mean 0.682 ms, p99 1.870 ms, maximum 20.304 ms. Another run used Cubase during this sequence;
these paced counts are not used to refine the isolated first-callback diagnosis above.

Evidence is retained locally under `tests/results/layout-windows-final/`, including per-ID JSON,
logs, binary identities, restore journals and summary. The tested VST3 embeds `17b85ad+`, SHA256
`5b8db0b2461338c5ffa0e371aced3c4a9e51f1df7cbb9992c1d412e337a89311`.
The unchanged PerfTest SHA256 is `7131c4161a402aab8505be4611c89b274eda06a89a9cad279b7eb19b984f6d7b`.
The current checkout commit is not claimed as the compiled plug-in's identity.

## Physical Mac

Logic and the warm-up run were left alone while their processes were active. After repeated clear
process checks, including a guard immediately before launch at about 23:29 EDT, the updated scripts
ran in the isolated `/Users/joe/macday/test-layout` folder with `--no-logic`.

Result: **30 PASS, 1 FAIL, 1 SKIP, 13 PLANNED checks**. The only failure was the retained T2-03 gate.
Units (43), both LFO regressions, UI (493), full renders (63), isolation, strict AU validation, four
pluginval passes (VST3/AU, with/without GUI), HostTest (64 VST3 / 63 AU), screenshots (eight PNGs,
four scales including Retina 2x), standalone launch/native Quit, and REAPER in both formats passed.
Installed AU bundles and app settings were restored. The product runner passed all three scripts;
all nineteen runner/restoration contracts also passed on the Mac. The template Mac product runner
was separately invoked and correctly reported **no product tests yet**.

T2-03: 30 instances, 128 samples, 60.291 s, exit 3; 53 late blocks, 7 missed periods,
mean 1.875 ms, p99 2.427 ms, maximum 17.492 ms. Its threshold and PerfTest implementation were unchanged.

Native Mac binaries embed `f24dc90`; the standalone embeds `2ca9f34`. These are the previously
imported repo-local builds, with exact hashes in each result. The hosted CI below compiled the
current test-layout revision independently. Local evidence is retained under
`tests/results/layout-mac-followup/` on the Mac and copied back to this worktree.

This branch still has planned rows for microphone permission and Logic (the separate run's work),
real Safari/hands-on checks, ASan and release/installer sign-off. The local web build is absent.
None of those gaps is reported as a pass or as release approval.

## GitHub

**All five Tertiary jobs passed** at `e0e1319`: Windows, Apple Silicon, Intel, web, and cross-platform
render comparison. Result and comparison artifacts uploaded successfully with seven-day retention.
Run: https://github.com/CaulfieldEngineering/Tertiary/actions/runs/41164346979.
Both comparisons covered all 36 files with zero failures at the unchanged `1e-3` tolerance:
Windows versus Apple Silicon had worst maximum difference `2.98e-7`; Windows versus Intel was exact.

| Preserved CI measurement | Before | After |
|---|---:|---:|
| Unit sections, each native platform | 43 | 43 |
| UI assertions, each native platform | 493 | 493 |
| Windows VST3 HostTest | 90 | 96 |
| Mac VST3 / AU HostTest, both chips | 58 / 57 | 64 / 63 |
| Mac architecture/minimum-OS assertions | 18 | 18 |
| Quick renders, each native platform | 36 | 36 |

An intermediate Intel run exposed missing AU registration. Its detailed logs showed no AU was
discovered, before any audio assertion could run. The fix restores the copied AU executable's
permissions and the old CI's forced, current-user registrar restart, and retains registry listings.
The successful Intel receipt records **mode 0644 -> 0755**, then `aufx EXPL WLAD` in `auval -a`,
followed by successful auval, strict GUI pluginval and all 63 AU HostTest passes. No AU check was
removed or relaxed. This is harness/artifact preparation, not a plug-in-source change.

## Template Windows verification

`NewPluginTemplate-pipeline` also completed `tests/win/Run-All.ps1 -BuildDir build-runall`:
**62 PASS, 0 FAIL, 17 PLANNED, 1 SKIP** in `tests/results/layout-windows-verified/`.
The product runner reports **no product tests yet**, both independently and when called by Run-All.
Native counts: six unit sections, eleven HostTest passes, strictness-10 GUI pluginval, six full renders
(four in the quick CI matrix). Its native binaries explicitly identify the earlier `5ae2d15+` build;
this is harness verification against those repo-local binaries, not a claim they were recompiled at the new tip.

The web build was freshly compiled at `06c0289`; all 54 Chromium/Pixel browser assertions and its
harness check passed. No screenshot reference or assertion threshold changed. The earlier diagnostic
run in `layout-windows-final/` correctly rejected an old `8c88f9a` web build and exposed the missing
engine-option forwarding: Windows had attempted audio-dependent WebKit cases. Windows now defaults
to Chromium and forwards `-Engine`; the existing Mac web job still owns all iPhone/WebKit cases.
The one final SKIP is the template's T2-03 placeholder, blocked by the other run's active PerfTest;
the template does not supply a product PerfTest implementation.

Pre-layout template run `41158822526` recorded 54 Chromium assertions, 10 web-harness assertions and
37 Mac WebKit assertions. This local rerun retained 54/54 and 10/10; the separate Mac web job retains
the WebKit invocation, but its latest execution is blocked by the account restriction below.

The corrected earlier template Mac run at `f1b83df` completed **20 PASS, 2 PLANNED, no failures** and
printed its summary before cancellation and an artifact-quota error. It retained six unit sections,
four renders, twelve architecture/minimum-OS checks and eleven VST3 HostTest passes. AU is explicitly
not applicable because this template configures VST3 and Standalone. Its latest hosted run at
`2898254` is blocked before any step by GitHub's failed-payment/spending-limit restriction:
https://github.com/CaulfieldEngineering/NewPluginTemplate/actions/runs/41164350589.
That run is **not green** and must be rerun after the account restriction is resolved.
