# Logic Pro and microphone checks

[Codex 2026-10-30] M-09 and M-07 / V-08. These are local Mac tests with no agent/model calls during a run. No plug-in code is changed.

## Saved project

`Tertiary harness.logicx` was built once in Logic Pro Creator Studio 12.4 on Joe's M1 Mac mini (macOS 26.6). It contains:

- 48 kHz, 120 BPM, 4/4; cycle from bar 1 to bar 13 (24 seconds).
- **Tertiary - measured**: Cubase's exact synthetic input, one stereo Tertiary AU (`aufx EXPL WLAD`), Default preset with Low Depth automation. Points at bars 1 / 3 / 9 / 11 / 13 are 20 / 83 / 83 / 20 / 20 percent. All other parameters keep the saved Default state.
- **Tertiary - play**: Cubase's exact three-minute organ loop, a separate Tertiary AU at Default, muted, with no automation points. The **Play** marker is at bar 14.

This first Logic harness deliberately covers a fixed tempo and one automation lane. The larger future tempo-map, multi-lane and resize matrix in the DAW test plan is not claimed here. The saved editor is at 100%.

Audio, goldens, previews and run outputs are not committed. `media.json` pins the PCM format and SHA-256 of both Cubase media files. Generate them with the Cubase harness's `tertiary_harness_build.py audio` and `playaudio`, or pass an existing folder containing `harness-input.wav` and `play-loop.wav`. The runner validates and copies those files into a disposable project before opening it. The tracked project package is approximately 160 KB.

For a hands-on listen, open the populated project at `~/macday/logic/Tertiary harness.logicx`, turn off the yellow Cycle button, unmute the play track's M button, put the playhead at the Play marker and press Space. Select that track and click its blue Tertiary insert to change knobs. Close without saving to preserve the test setup.

## Run M-09

Requirements: an unlocked English macOS desktop, Logic already set up and closed, Xcode command-line tools (`swiftc`), system Python 3, and existing Screen Recording/Accessibility permission for the invoking context. Joe's SSH context and Terminal already have those grants. The script never changes privacy settings. When launched over SSH, AU validation enters the existing Aqua session through passwordless sudo; direct `auval` in an SSH bootstrap cannot see per-user AUs reliably.

```sh
bash plugin/tests/daw/logic/test-logic.sh \
  --build "$HOME/macday/in/build/Tertiary.component" \
  --build-id f24dc90 --media "$HOME/macday" \
  --out "$HOME/.wla-test/runs/logic/example"
```

Without `--build`, selection is this checkout's `build/plugin/Tertiary_artefacts/Release/AU/Tertiary.component`, then its non-configuration AU folder, then the current CI artifact at `~/macday/in/build/Tertiary.component`. The selected path, embedded build ID and executable SHA-256 are printed and recorded. `--build-id` can pin an expected build.

Each run refuses an already-open Logic session, backs up the user's installed AU, stages the selected build, invalidates the user AU cache and runs strict auval. It opens a disposable copy of the committed project, bounces with Cmd-B to stereo 48 kHz / 32-bit float WAV (offline, normalize off, no dither or tail), verifies the 24-second file, opens the editor and checks its 750 x 478 content size, reveals/copies About's build ID, quits without saving and compares PCM against the golden. The previous AU is restored even on failure. A failed normal quit is reported as a failure; forced cleanup applies only to the host launched by the run.

Controls are found from Accessibility labels and frames. About's version control uses coordinates relative to the verified editor. Missing media and AU rejection fail with screenshots; a known project-update prompt is answered by name. Unknown dialogs time out with screenshots/AX snapshots. No indefinite UI waits or image recognition are used. `mac-ui.swift` is compiled into the result directory. If direct Accessibility access is unavailable, a bounded Terminal worker exposes only that helper's UI operations.

The normal golden is `~/.wla-test/goldens/tertiary/logic/harness.wav`. To establish a new approved baseline in an **empty** golden folder:

```sh
bash plugin/tests/daw/logic/test-logic.sh --record-golden \
  --build "$HOME/macday/in/build/Tertiary.component" --build-id f24dc90 \
  --media "$HOME/macday" --golden "$HOME/.wla-test/goldens/tertiary/logic"
```

Recording requires two separate Logic launches and bounces to match before writing the golden. Comparisons require bit-identical PCM and matching format: tolerance **0**. WAV metadata is excluded from comparison. A different Logic engine/platform version needs review, not an automatic tolerance increase or golden replacement.

## Run M-07 / V-08

```sh
bash plugin/tests/daw/logic/test-microphone.sh \
  --source "$HOME/macday/mic/src" --build "$HOME/macday/mic/build" \
  --cmake "$HOME/Library/Python/3.9/bin/cmake" \
  --out "$HOME/.wla-test/runs/microphone/example"
```

The source must be the microphone fix `25b0a55`. The script builds only `Tertiary_Standalone`, checks `NSMicrophoneUsageDescription` and bundle ID, signs a disposable copy with the fix's hardened-runtime audio-input entitlement (ad-hoc identity), verifies that signature, launches the copy, captures the real macOS microphone prompt, clicks its Allow button, queries only this app's microphone TCC entry read-only and confirms the editor is alive with no denial state. It closes the app afterward. [Claude 2026-10-31] Every run first resets only `com.WonderlandAudio.Tertiary`'s microphone decision with `tccutil`, so the prompt always appears; the old opt-in `--reset-mic` is gone. A successful grant is retained. macOS shows the prompt only when the app opens an audio input: on 2026-10-31 the prompt check failed because this Mac had no input (not because of SSH; the same SSH run passes with an input). The test uses the Mac's existing input and never installs drivers or restarts coreaudiod, AirPlay or any audio service: on 2026-10-31 such restarts sent the audio services into runaway CPU and hung the Mac until it was power-cycled. No input, or Core Audio not answering within 60 s, is FAIL with what to do. `M-07-live-input` is now a real check: after Allow the default input device is open (Core Audio's running-somewhere flag).

Joe's Mac has no physical input. The 2026-10-30 run (`mic-04`) temporarily installed the signed BlackHole 2ch 0.7.1 HAL driver and restarted Core Audio; that option was removed on 2026-10-31 after restarts hung the Mac. A permanent input (a microphone, or BlackHole 2ch installed once and loaded at the next restart) is now a precondition. This checks consent and that the input opens; it does **not** claim an audible physical-input or meter test.

BlackHole is not bundled or downloaded by the test. Source: [BlackHole official repository](https://github.com/ExistentialAudio/BlackHole), [0.7.1 installer](https://existential.audio/downloads/BlackHole2ch-0.7.1.pkg). The installer used here was notarized/signed by Existential Audio Inc. (Q5C99V536K), SHA-256 `57b540f27a3e29c37e310e01bee0fdfab76733087e47f997ef9dccf851400dcf`. No signed-distribution, notarization or installer release check is claimed for Tertiary by this test.

## Result contract and layout migration

Both entry points emit `result.json` with `{name, platform, status, started, finished, build:{path,id}, host:{name,version}, checks:[{id,status,detail}], artifacts:[...]}`. Status values are PASS/FAIL/SKIP; exit codes are 0 pass, 1 regression, 2 unavailable/precondition, 3 automation error. Screenshots, AX trees and logs stay in the chosen output folder. Default outputs are under `~/.wla-test/runs/`.

For the pending test-layout migration, move this directory's shared files and project into `tests/mac/logic/`, and put the two `test-*.sh` entry points in `tests/mac/`. The wrappers find either the current adjacent helpers or the `logic/` child folder. Pass the migrated Cubase media folder with `--media`. No changes to the parallel test-layout worktree are needed.

## Verified on 2026-10-30

- RC1b AU build **f24dc90**, executable SHA-256 `47f7ce4c7f533061a7bbe34b204308e4f9f1c104359e06894d57fa21f652a046`.
- Golden recording: two independent bounces matched exactly. Two subsequent complete unattended runs, `logic-run-1` and `logic-run-2`, both PASS with maximum sample difference 0 and all stage/auval/load/bounce/editor/build-ID/quit/restore checks passing.
- Golden PCM SHA-256 `e9c3358edbc804fd6b67037ec9d013434a91f18d14759d7330fc0f82aec93f66`; 1,152,000 stereo frames, float32, 48 kHz.
- Microphone `mic-04`: PASS for build, exact usage description, hardened-runtime audio-input entitlement, real permission prompt, Allow/TCC `auth_value=2`, no denied state and app/temporary-driver cleanup. Physical input verification remains unmeasured.
- Mac evidence: `~/macday/logic-results/{golden-05,logic-run-1,logic-run-2,mic-04}`; harness screenshot `~/macday/logic/harness.png`.
- Windows evidence: `C:\Users\Joe\.wla-test\macday-rc1b\logic-results\`; golden mirrored to `C:\Users\Joe\.wla-test\goldens\tertiary\logic\`.

Earlier development attempts are retained on the Mac as diagnostic evidence, not counted as passing runs. All final runs execute only the checked-in scripts, without interactive agent intervention.
