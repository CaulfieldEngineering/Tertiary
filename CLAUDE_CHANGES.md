# What Claude changed for Tertiary 1.1

Written by Claude, 2026-10-27, at Joe's request: "Do your best to indicate which new code was Claude generated ... ensure that anything you touch is still maintainable and navigable by me."

How to find it in the code:
- Every block Claude added or changed starts with `// [Claude 2026-10-27] what and why`. Small fixes inside your lines have the same tag at the end of the line. Search the repo for `[Claude 2026-10-27]`.
- New files say at the top that Claude wrote them, when and why.
- Everything is on the `release-1.1` branch, in eight commits, one per area below. Each commit can be reverted on its own (`git revert <hash>`), oldest last, except where "Undo" says otherwise.

Nothing was reformatted, renamed or "modernised". Your files keep their layout, banners and naming.

---

## 1. Testing tools (nothing here ships in the plugin)

| File | What | Why |
|---|---|---|
| `plugin/review/ReviewRender.cpp` | The offline renderer from the promo branch (commit 219af36), plus options `--block n` / `--block var`, `--bits 32`, `--demo`, `--rate` | To render before and after every change and compare |
| `plugin/CMakeLists.txt` (end) | `WLA_REVIEW_RENDER` option (from the promo branch) and the new `WLA_TESTS` option | Both OFF by default: release and deploy builds are unchanged |
| `plugin/tests/regression/render_matrix.py` (new) | Renders every WAV in a folder with 7 settings (default, varied, free-running, solo/mute, ramps, varied at block 512, varied at changing block sizes) | Regression renders |
| `plugin/tests/regression/compare_renders.py` (new) | Compares two folders of renders: identical / max difference / residual | Regression numbers |
| `plugin/tests/TertiaryTests.cpp` (new) | Unit tests (JUCE UnitTest, no new library) | The playbook rule: AI code gets tests |
| `plugin/tests/ci/mac_checks.sh` (new) | The Mac checks run by CI | Used by test.yml |
| `.github/workflows/test.yml` (new) | Builds and tests on Apple Silicon, Intel Mac and Windows; never deploys | Testing without a Mac |

Run them yourself (Windows):
```
cmake -B build-tests -DWLA_REVIEW_RENDER=ON -DWLA_TESTS=ON
cmake --build build-tests --config Release --target TertiaryTests ReviewRender
build-tests\plugin\TertiaryTests_artefacts\Release\TertiaryTests.exe
python plugin\tests\regression\render_matrix.py build-tests\plugin\ReviewRender_artefacts\Release\ReviewRender.exe <wav folder> <out folder>
python plugin\tests\regression\compare_renders.py <reference out folder> <new out folder>
```
Undo: commits `1f1255e`, `1a37154`, `83e4829`, `5d5cd89`. Removing them doesn't change the plugin.

---

## 2. Audio processing fixes (`bb247d9`)

| File / function | Change | Why | Tested by |
|---|---|---|---|
| `PluginProcessor.cpp` `prepareToPlay` | Calls `initializeLFO(sampleRate)` on the three LFOs and sets the three "parameter changed" flags | The wavetables were sized in the constructor, where the sample rate is 0. So the first block grew them on the audio thread (allocation), and after a sample-rate change the LFOs ran at the old rate's speed (and after a drop in rate, read stale table entries) until a control moved | Unit test "LFO speed is right after a sample-rate change": 2 Hz at 96 kHz after 44.1 kHz measured 22050 samples per cycle before (wrong), 48000 after (right) |
| `PluginProcessor.cpp` `prepareToPlay` | Demo timers (`samplesUntilDisable/Enable`) always set | So they are right if the plugin later returns to demo (license re-check) | Unit test "Demo timing" |
| `PluginProcessor.cpp` `processBlock` | `else if (!effectEnabled)` turns the effect back on when not in demo | Activating during the 10 s demo gap left the effect off until the plugin was reloaded (the counter only runs in demo mode) | Unit test "Demo timing ... activating during the gap" (failed on 1.0.2 code) |
| `PluginProcessor.cpp` `processBlock` | `playPosition` read after `getCurrentPosition`, not before | It held the previous block's position, so re-syncing on a multiplier change was one block late | Renders: the only renders that changed (see Results) |
| `PluginProcessor.cpp` `updateState` + `LFO::updateTempo` (new, LFO.h/.cpp) | If the host tempo changed, the three LFOs recalculate their increment (not the whole wavetable) | Synced LFOs kept the tempo they had at the last parameter change | Unit test "Synced LFO follows a host tempo change": 60 bpm measured 24000 samples per beat before, 48000 after |
| `PluginProcessor.cpp` `applyCrossover` | `fb = inputBuffer` became `fb.makeCopyOf(inputBuffer, true)` (twice) | `AudioBuffer::operator=` reallocates whenever the block size changes, on the audio thread. `true` keeps the memory | Renders at changing block sizes identical; unit test "Same output at any block size" |
| `PluginProcessor.cpp` `getStateInformation` | `apvts.copyState().writeToStream(mos)` instead of `apvts.state.writeToStream(mos)` | `apvts.state` is only refreshed from the parameters on a timer, so a save just after a change (or offline) stored old values | Unit test "State save and restore" (6 of 49 values wrong on 1.0.2 code) |
| `PluginProcessor.h` | `std::atomic` for the three LFO flags, `nextFFTBlockReady`, `fftPickoffPointIsInput`, `mIsDemoVersion`, `effectEnabled`; `lastHostBpm` | These are written on one thread and read on another (audio, GUI, license thread). Atomic makes that safe; the code using them reads the same | Build, renders, pluginval |

Undo: `git revert bb247d9`. Note the license commit (section 4) adds lines next to these, so revert that first if you revert both.

## 3. GUI fixes (`9c2c231`)

| File / function | Change | Why |
|---|---|---|
| `GlobalControls.cpp` `timerCallback` | When not in demo, re-enable the band controls if they are disabled | Same demo-gap problem as above: the controls stayed greyed out |
| `ScopeChannel.cpp` `resized` | `while` became `if` | If zoom and center were both 0 it looped forever (host freeze) |
| `ScopeChannel.cpp` `paintGridLines` | `i % juce::jmax(1, (int)beatSpacing)` | `(int)beatSpacing` can be 0 for a very narrow scope or a high zoom: integer divide by zero crashes on Intel |

Tested by: pluginval (opens and resizes the editor), building. Undo: `git revert 9c2c231`.

---

## 4. License and updates (`79fad6c`)

How it works now, in order:
1. Activation (About window) is the same as 1.0.2 from the user's side. The request goes through `LicenseServer` (new). The "instance" sent is `TRT-` plus a hash of the computer's own ID (`SystemStats::getUniqueDeviceID()`, the motherboard/platform UUID), not the model name. It is stored in the settings file as `licenseInstance`, with `licenseLastCheck` and `licenseLastValid`.
2. When the editor opens, the processor starts `LicenseCheckThread` once. If the key was last checked 7 or more days ago, it calls `validate`.
   - `key_valid`: dates updated.
   - `invalid_key`, `invalid_order` (what a refund gives), `invalid_product_key`, `invalid_email`, `expired_key`, `key_cancelled`, `invalid_key_status`: the key is cleared, the plugin is back in demo, the About window opens with "This serial key is no longer valid (server's reason). Tertiary is back in demo mode."
   - No answer, or any other answer (a WordPress error, a missing route): nothing changes, it tries again next time. After 30 days without a good answer, the About window's status line says so. The plugin keeps working. Nobody is locked out for being offline.
3. Once a day (activated copies only; the server only answers `version_check` for a valid key) it asks for the latest version. If it is newer than this build, a small "Version X is available" link appears on the left of the top banner (full version), and the About window's website link reads "Version X is available at WonderlandAudio.com". Nothing is downloaded.

| File | What |
|---|---|
| `LicenseServer.h/.cpp` (new) | `LicenseServer` interface (validate, activate, deactivate, versionCheck, cancel) and `WonderlandLicenseServer`, the real one. `cancel()` lets the plugin close without waiting for a slow network |
| `LicenseCheck.h/.cpp` (new) | The rules above in `runStartupCheck()`, plus `getMachineInstance`, `getActivatedInstance`, `isRevokingCode`, `isNewerVersion`, `normaliseEmail`, and `LicenseCheckThread`. The intervals are three constants at the top of the .h |
| `PluginProcessor.h/.cpp` | `licenseCheck` member, `startLicenseCheck()`; the destructor stops the thread |
| `PluginEditor.h/.cpp` | Listens to `licenseCheck` (ChangeListener); `showLicenseCheckResult()` updates demo state, About window and banner |
| `AboutWindow.h/.cpp` | `attemptSerialKeyValidation/Activation/Deactivation` rewritten around `licenseServer` with the same messages. Fixes: SafePointer (a reply arriving after the window closed used a deleted object: possible host crash); the 5 s timeout was set on a copy and never applied; email sent lower case (the server lowercases only its own side, so "Joe@..." was refused); deactivation uses the stored instance, or for a 1.0.x activation the old device description, so old activations can still be freed. New: `showLicenseCheckResult`, `refreshActivationFields`; `updateLocalKeyData` stores instance and dates and no longer saves the "Email: " prefix. Status label 27 px high (was 20) for two-line messages |
| `TopBanner.h/.cpp` | `setUpdateVersion()` and the hidden `mUpdateLink` |
| `ActivatorWindow.cpp/.h` | Deleted (the old LexActivator code, all commented out). The source list is a folder scan, so no build file named them |

The settings file is the same `Tertiary.settings`; 1.1 adds keys and removes none. A 1.0.x file (key and email only) or a damaged file is read without trouble (unit tests).

Where the server codes come from: the public source of Serial Numbers for WooCommerce (`RestAPI.php`). The site runs the Pro version; I couldn't see its source or call the live server, so check one real purchase end to end (activate, then refund in admin and wait for the weekly check, or set `licenseLastCheck` to 0 in the settings file to force it).

Tested by: 13 unit tests with a pretend server (no network calls in any test or CI run). Undo: `git revert 79fad6c` (restores ActivatorWindow too).

---

## 5. Mac build and version (`5d7ecad`)

| File | Change |
|---|---|
| `config.cmake` | Version 1.1.0. `AU` added to `wFORMATS`. Codes unchanged: plugin `EXPL`, manufacturer `WLAD`. Those are Tertiary's identity in every DAW (the VST3 ID is made from them), so changing them would break saved sessions. JUCE drops AU on Windows by itself |
| `CMakeLists.txt` | `CMAKE_OSX_DEPLOYMENT_TARGET 11.0` before `project()` |
| `.github/workflows/workflow.yml` | Mac arch `"arm64;x86_64"`; "Sign Mac Binaries" also signs the AU; new step "Create AU Package"; "Sign Individual Packages" also signs the AU package. The distribution signing, notarization and stapling steps are unchanged and cover the AU through the installer |
| `package_config/mac/distribution-full.xml`, `distribution-demo.xml` | An AU choice (installs to /Library/Audio/Plug-Ins/Components) |
| `CHANGELOG.md` (new) | 1.0.0, 1.0.1, 1.0.2 from the git history and tags, and 1.1.0. Written for customers (no mention of AI), so it can go on the site |

Not tested by CI: the signed and notarized installer (it needs the signing secrets and only runs on staging/tags). The first staging run after merging is its test; check the "Notarize" and "Staple" steps, then install it on a Mac.

Undo: `git revert 5d7ecad`.

---

## Results

Local, Windows 11, MSVC 2022, JUCE 8.0.8:
- Baseline: staging (9e9b39c) built with ReviewRender, 35 renders (5 inputs: the two sample_audio WAVs, Bass and Drums and Clean Electric Guitar from the web demo tracks, a generated test signal; 44.1, 48 and 96 kHz; 20 s; 7 settings). Rendered twice: bit-identical, so the renders are deterministic. In staging itself, changing only the block size moves samples by at most 5.2e-8 (-146 dBFS): float rounding.
- After: 30 of 35 renders bit-identical to staging, including both changing-block-size sets. The 5 "ramps" renders differ from 3.000 s, the moment the Mid multiplier ramp starts: max difference 2.6e-3 (-51.6 dBFS), residual -50 to -70 dB below the signal. With the single play-position line put back to 1.0.2, all 35 are bit-identical, so that one documented fix is the whole difference.
- Unit tests: all pass. Each processor test was run against the 1.0.2 code and failed there.
- pluginval 1.0.4, strictness 10, VST3: SUCCESS before and after.

CI (Apple Silicon, Intel Mac, Windows): see the PR description.

---

## Suggestions I did not change

1. **Relative phase is twice what the knob says, in the audio.** `LFO::setTempo` converts the phase to samples of a full second (`sampleRate`), but the wavetable holds half a second (`sampleRate / wtScalar`). So -90 shifts the audio LFO by half a cycle (180 degrees), and the full range wraps twice. The scope compensates ("Divide by 2" in `ScopeChannel::generateLFOPathForDrawing`), so the picture shows the knob's value while the sound has double. The fix is one line in `setTempo` (use `sampleRate / wtScalar`), but it changes the sound of any saved session with a phase other than 0, so it is your call.
2. **The repo is public on GitHub**, not private (the API reports `private: false`). The source and the license logic are readable by anyone. Settings, Danger Zone, Change visibility, if you want it private. (Public also means Actions minutes are free.)
3. **The FFT pickoff choice names are swapped**: the parameter's "Input" (index 0) makes the spectrum show the output (`setFftPickoffPoint`). The options menu toggle is right; only the names a DAW shows in automation lanes are backwards.
4. **NewPluginTemplate uses the same codes as Tertiary** (`WLAD` / `EXPL`). A new plugin that forgets to change `wPLUGIN_CODE` will collide with Tertiary in every DAW. Give the template a placeholder like `TMPL`.
5. **Crossover order can be crossed by automation.** The GUI keeps low-mid below mid-high; host automation can set them the other way round. A `jmin`/`jmax` in `updateState` would guard it, but it changes the sound of such automation.
6. **The full wavetable is recalculated on the audio thread** whenever an LFO parameter moves (24,000 points of trig per LFO at 48 kHz). No allocation, but a CPU spike during automation of depth, skew or shape. Could move to a background thread with a swap, but it is a bigger change.
7. **`LFO::mPosition` is a float** counting up to 24,000 with steps as small as 0.25, so slow free-running rates are off by a fraction of a percent. A `double` would fix it but changes the sound slightly.
8. **Version check needs a key.** The server's `version_check` route refuses requests without a valid key, so demo users never hear about updates. A public JSON file on the site (or the WooCommerce product's version) would let demo copies show the link too. Also: the version comes from the product's "Software version" field (`_software_version`) in WooCommerce; set it to 1.1.0 when you release, or no one sees anything.
9. **The standalone package's identifier** (`...Tertiary.app` in workflow.yml) doesn't match its `pkg-ref` id in the distribution files (`...Tertiary.standalone`). The installer works today; aligning them avoids odd receipts.
10. **Unused code**: `WLDebugger` (all calls commented out; `cleanupInstance()` sets the pointer to null before deleting it, so it leaks), `LockFreeQueue.h` (not included anywhere), `Params.cpp` (empty).
11. **Small, harmless**: `FrequencyResponse::makeAttachments` makes `lowMidAttachment` twice; `getPlayPosition()` returns an `std::atomic` by value; the meters read `LinearSmoothedValue` across threads (fine in practice).
12. **Deploy workflow**: the Mac "Sign" steps skip silently if secrets are missing, so an unsigned installer can be uploaded. A tag build could fail instead.
