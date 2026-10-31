# Verification, 2026-10-30

Both full meta-scripts ran for real. After correcting runner issues, affected scripts were rerun in the same result folders and the summaries rebuilt. Superseded attempts remain in `history/`. Both final meta summaries exit 1 because failures remain.

| Run | Script PASS | FAIL | SKIP | PLANNED | Check coverage |
|---|---:|---:|---:|---:|---|
| [Windows](results/2026-10-30_2053/summary.md) | 4 | 2 | 1 | 9 | 17 of 28 checks automated and run; 10 planned; 1 skipped here |
| [M1 Mac](results/2026-10-30_2055-mac/summary.md) | 4 | 1 | 1 | 8 | 24 of 34 checks automated and run; 9 planned; 1 skipped here |

- Windows: native suite, standalone, two-instance isolation and Cubase PASS. Cubase loaded the repo binary, exported to the run folder, matched its machine golden exactly, quit normally and restored the installed slot.
- Windows REAPER FAIL: exact repo binary proven; reference and repeat renders match exactly, automation and saved projects pass. Its evaluation/startup dialog did not close and native Quit could not complete within the watchdog; only the owned test process was terminated.
- Both machines: the existing 30-instance PerfTest reports dropouts/missed block deadlines (Windows 124 dropouts / 6 missed periods; M1 69 / 6). DSP/live measurements completed. No threshold or golden was relaxed.
- Mac: native suite (including AU, pluginval GUI, UI robot and eight screenshots), standalone, isolation and REAPER VST3/AU PASS. Existing Logic PID 61498 was left alone; no Logic project was opened.
- SKIP on both: `web suite not present` in this branch. Future rows are PLANNED: ASan provisioning, other Windows DAWs, Logic harness, installers, Mac mic, Safari, iPhone/manual, staging preview, post-release and performance-budget sign-off as applicable.

## Build provenance

- Windows VST3: fresh repo-local build, embedded `17b85ad+ ? 2026-10-30 20:43`, SHA256 `5b8db0b2461338c5ffa0e371aced3c4a9e51f1df7cbb9992c1d412e337a89311`. Test programs were built here with WLA_TESTS and WLA_REVIEW_RENDER, copying to installed plugin folders disabled.
- M1: CI bundles copied from `~/macday/in` into `~/macday/test-layout/build-tests`. VST3/AU embed `f24dc90 ? 2026-10-30 19:22`; standalone embeds `2ca9f34 ? 2026-10-30 19:21`. Fresh HostTest/PerfTest were built inside that repo folder. Every executable/bundle identity is in the per-script `builds.json` artifacts. These are different compiled revisions across platforms, not a claim that one release candidate passed both.

## Runner checks and restoration

Eleven contract/restoration checks passed in each repository and on the M1. PowerShell and Python parsing, all Mac/web shell syntax, and compilation of the future Logic AppleScript passed. Restore journals are complete; no opened test host remains. Failed attempts, including owned-PID Cubase fallbacks during startup diagnosis, are retained for review.

Mac originals: `~/macday/test-layout/tests/results/2026-10-30_2055`. A copy with all linked artifacts is under the Mac report link above. Results/builds stay out of Git; this dated record and the scripts are committed.
