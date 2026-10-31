# Test-layout follow-up investigation, 2026-10-30

**Follow-up authorized by Joe on 2026-10-30:** the performance fix is being handled separately on `fix-first-block`. Layout/CI migration may proceed; T2-03 remains an unchanged real test and may fail until that fix merges. The investigation below is the original evidence.

**Originally stopped before the requested layout/CI migration because the Windows investigation confirms a real first-callback performance defect in the approved RC1b plug-in. Joe must decide whether it blocks 1.1.0.** No plug-in code or performance threshold was changed. The independent REAPER harness investigation is recorded below. No Mac connection, SSH command, or Mac run was made.

## 30-instance stress

This is `PerfTest` test **T2-03**, invoked by `tests/win/Test-Performance.ps1` and `tests/mac/test-performance.sh` through `tests/lib/hosts.py::performance`. It is neither pluginval's multi-instance test nor HostTest. It runs 30 VST3 instances sequentially on one paced thread, at 48 kHz with 128-sample buffers, for 60 seconds. Each block has **2.667 ms** to finish. Exit 3 means late blocks or missed periods; neither rerun crashed or reached a watchdog timeout.

**Neither repository's `test.yml` nor `workflow.yml` runs this stress test.** The successful [RC1b CI run](https://github.com/CaulfieldEngineering/Tertiary/actions/runs/41114706907) therefore does not cover this failure.

### Original executable, same command for both builds

Machine: Windows mini PC, AMD Ryzen 7 8845HS. These four runs were serial; the REAPER investigation and diagnostic compilation waited until they finished. Other existing machine services were left alone.

| Binary / instances | Measured seconds | Exit | Late blocks | Whole periods missed | Mean / p99 batch | Maximum, first block |
|---|---:|---:|---:|---:|---:|---:|
| test-layout / 30 | 60.367 | 3 | 545 | 5 | 0.639 / 0.952 ms | 17.451 ms |
| approved RC1b / 30 | 60.378 | 3 | 312 | 5 | 0.651 / 0.892 ms | 17.940 ms |
| RC1b path / 0 | 60.222 | 0 | 0 | 0 | effectively zero | 0.008 ms |
| test-layout path / 0 | 60.206 | 3 | 2 | 1 | effectively zero | 0.006 ms |

The zero-instance controls use the same executable, scan the specified binary, and run the same clock without any processing instances. They show that scheduling alone can cause a deadline failure. They do **not** explain the reproducible expensive first processing block. Later late-block counts mix scheduling and processing, and are not all attributed to the plug-in.

Reproduce the unmodified stress test with either VST3 bundle:

```powershell
& build-tests/plugin/PerfTest_artefacts/Release/PerfTest.exe `
  --vst3 <bundle-path> instances --count 30 --seconds 60 --out <result.json>
```

### Independent callback timing

A temporary diagnostic host loaded the same unchanged binaries and timed only their `processBlock` calls. It excluded signal generation, loading, preparation, timers, and sleeps; one worker ran unpaced. It measured wall time and the calling thread's CPU cycles, with three fresh batches per build. It also repeated the measurement after `releaseResources` / `prepareToPlay`.

| Binary | First 30 callbacks | CPU cycles for first batch | First batch after reprepare | Warm batch, mean of 100 |
|---|---:|---:|---:|---:|
| test-layout | 17.16-17.46 ms | 64.96-66.15 million | 17.07-17.15 ms | 0.365-0.366 ms |
| approved RC1b | 17.58-18.09 ms | 66.59-67.48 million | 17.19-17.31 ms | 0.367-0.371 ms |

Every cold/reprepare batch also records 15.625 ms of worker CPU time in Windows' coarse `GetThreadTimes` accounting. The first batch costs about 46-47 times the warm batch in actual thread cycles and exceeds the audio deadline by about 6.4-6.8 times. This establishes real callback work, independent of the paced harness's scheduling noise.

Source-supported cause: `prepareToPlay` sizes the three LFO tables and sets all three dirty flags (`plugin/source/PluginProcessor.cpp`, lines 705-713). The first `processBlock` calls `updateState`, which consumes those flags and calls `updateLFO` (lines 1015-1027). `plugin/source/LFO.cpp::updateLFO` calls `calculateWaveShape`; each table contains 24,000 entries at 48 kHz, with waveform generation and scaling passes on the processing thread. Repreparing marks the tables dirty again. This is a strong source-supported explanation; the exact time spent in each internal function was not profiled.

**Scope of the finding:** initial/reprepare CPU overrun for 30 serial instances on one thread. It does not establish a crash, memory leak, sustained overload of 30 warmed instances, or an audible failure in a particular DAW. DAWs can distribute instances across threads or pre-roll them. The release decision remains Joe's; this investigation neither changes the gate nor grants a release sign-off.

### Binary provenance

The approved Windows RC1b binaries predate the `fix-editor-size` tip. The tested `build-rc` VST3 embeds `4ee4fc0`, built at 14:47. `git diff f24dc90 4ee4fc0 --name-status` shows **only `plugin/tests/UiTest.cpp`**, with no plug-in-source or build-configuration differences. The build directory's standalone and installer match the approved Dropbox RC1b copies byte for byte; the VST3 was taken from that same directory. It is not falsely labelled as a newly compiled `f24dc90` binary.

| Artifact | SHA256 |
|---|---|
| Original PerfTest, unchanged before/after | `7131c4161a402aab8505be4611c89b274eda06a89a9cad279b7eb19b984f6d7b` |
| test-layout VST3, embedded `17b85ad+`, 20:43 | `5b8db0b2461338c5ffa0e371aced3c4a9e51f1df7cbb9992c1d412e337a89311` |
| Approved RC1b VST3, embedded `4ee4fc0`, 14:47 | `35bb8efdad14d09d07032987174cd0d40d6c26bcc260ef3b02134f289295893d` |
| RC1b standalone, matching Dropbox | `d47ab4f91268c4e345028e8e4e39f4be91c6068da8f964882600d5da861d9ab0` |
| RC1b installer, matching Dropbox | `312ede7c3d4dd58ed762e933a78ca58e80d8cbd1dd330392d95ce80b3733869c` |

Raw local evidence is under `tests/results/investigation-2026-10-30/` (ignored by Git): `runs.json`, `*-instances.log/json`, `diagnostic-{testlayout,rc1b}.log/json`, `diagnostic-summary.json`, and `cold-callback-evidence.md`. The latter includes the independent host's exact rebuild/run commands. Its source, build script, response files, and executable are preserved there. No original host object/executable or shipped binary was overwritten.

The earlier Mac artifacts already record a VST3 embedding `f24dc90`: 69 late blocks, 6 missed periods, 1.935 ms mean, 2.439 ms p99, 17.912 ms maximum at the first block. Those are **prior evidence**, not a new Mac run or proof of the Mac cause. Once the Mac is free, repeat the same RC1b and zero-instance controls plus first/warm/reprepare callback timing there.

## Windows REAPER

The unchanged harness reproduced its original failure while the audio comparison passed. The blocking window is **About REAPER v7.83/win64**, with its evaluation countdown still showing a disabled **Buy Me [4]** button after foreground focus was lost. The old driver released the Lua render gate after a fixed eight seconds, even while this modal dialog remained open; its generic close attempt did not dismiss the evaluation notice. Rendering completed underneath it, but native Quit could not finish before the 90-second watchdog (90.2 seconds observed). This is a harness startup/quit defect, with no audio failure observed.

The harness fix waits for the named, enabled **Still Evaluating** button, acknowledges it, and verifies the dialog has disappeared before releasing rendering. It handles the separately named **REAPER New Version Notification** through its **Close** button, restores focus to the evaluation countdown, captures dialog-specific screenshots and window/button transitions, and sends REAPER's native **File: Quit (40004)** only after rendering finishes and the modal windows are gone. Startup grace begins when the main window appears, so a long initial scan cannot consume it before notices appear. It operates only on its own PID. Unknown dialogs remain visible in failure evidence rather than being blindly accepted. Screenshot capture is bounded and uses PrintWindow so unrelated foreground windows do not obscure the evidence; failed initialization/capture cannot bypass owned-process cleanup.

Four corrected launches completed with zero exit codes in 29.65, 29.85, 30.18, and **27.21 seconds** (total REAPER runtime, not just quit latency). The final launch verifies the final code: **Still Evaluating** acknowledged at 23.922 s, render gate released at 24.953 s, native Quit requested at 26.063 s, then exit zero. Default/reference and repeat renders both matched exactly over 120,000 frames; the automation render remained finite, bounded, and different (maximum difference 0.142701786); both projects saved. No REAPER process remained.

Local final evidence: [result JSON](results/investigation-2026-10-30/reaper-final-verified/reaper.json), [log](results/investigation-2026-10-30/reaper-final-verified/reaper-artifacts/reaper-vst3.log), [clear dialog screenshot](results/investigation-2026-10-30/reaper-final-verified/reaper-artifacts/vst3/reaper-still-evaluating.png), and `windows.log` beside the screenshot. The reproduced original timeout remains in `reaper-before/`; intermediate attempts remain separately preserved.

Only the REAPER Python driver, its documentation, and `tests/lib/reaper_windows.py` changed. The generic helper is identical in NewPluginTemplate, with its integration contract documented; the template's product-specific REAPER harness remains unconfigured. Python compilation and `git diff --check` pass in both repositories. No Lua, plug-in source, sequence, layout, or workflow was changed.

## Deferred work and CI baseline

Per Joe's explicit stop condition, the row-ID split, platform product folders/runners, CI profile migration, suite mapping, and seven-day retention changes have **not** been applied. The full Windows `Run-All` and new product runner verification therefore remain deferred. The REAPER fix is the separate investigation requested in part A. No staging/production push, tag, or release is part of this work.

The read-only CI audit preserved the pre-change counts in local `ci-baseline.json`: RC1b Windows had 43 unit sections, 493 UI assertions, 90 HostTest assertions, 15 standalone assertions, pluginval VST3, and 36 renders. Each Mac had 18 architecture/minimum-OS assertions, 43 unit sections, 493 UI assertions, auval, pluginval VST3/AU, HostTest 58/57 passes, and 36 renders; each cross-platform comparison checked 36 files. The current HostTest source adds six isolation assertions. No workflow tests were removed in this follow-up.

The audit also found that artifact quota failure occurs before native tests in an existing Mac CI job, causing those tests to be skipped. This ordering must be corrected with the deferred workflow migration; successful browser checks followed by a failed upload must not be described as an entirely green run.
