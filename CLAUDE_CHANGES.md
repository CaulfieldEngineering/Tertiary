# What Claude changed for Tertiary 1.1

Written by Claude, 2026-10-27, at Joe's request: "Do your best to indicate which new code was Claude generated ... ensure that anything you touch is still maintainable and navigable by me."

How to find it in the code:
- Every block Claude added or changed starts with `// [Claude 2026-10-27] what and why`. Small fixes inside your lines have the same tag at the end of the line. Search the repo for `[Claude 2026-10-27]`.
- New files say at the top that Claude wrote them, when and why.
- Everything is on the `release-1.1` branch, in small commits, one per area below. Each commit can be reverted on its own (`git revert <hash>`), oldest last, except where "Undo" says otherwise.

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

## 2b. Relative phase matches the knob (Joe approved, 2026-10-27)

| File / function | Change | Why |
|---|---|---|
| `LFO.cpp` `setTempo` | Phase converted to samples of one wavetable cycle (`sampleRate / wtScalar`) instead of `sampleRate` | The wavetable holds half a second, so the audio got twice the knob's phase (-90 gave 180 degrees, the range wrapped twice) |
| `ScopeChannel.cpp` `generateLFOPathForDrawing` | The scope's "divide by 2" removed | It compensated for the old doubling; without it the scope draws exactly what it drew before |

**This changes the sound** of any session or preset saved with a relative phase other than 0 (said plainly in CHANGELOG.md). The old sound comes back by doubling the phase value.

Tested by:
- Unit test "Relative phase in the audio matches the knob": 2 Hz LFO at 48 kHz, shift measured in the audio: 90 and -90 give 6000 samples (a quarter cycle), 45 gives 3000, 180 gives 12000. On the old code it measured 12000, 12000, 6000 and 0.
- Renders against the previous release-1.1 build: the 20 renders with phase 0 bit-identical; the 15 with phase (the three "varied" settings: Mid 90, High -120) changed.
- New build at Mid 90 / High -120 against staging at Mid 45 / High -60, three inputs: bit-identical. So the fix halves the shift and changes nothing else.

Undo: revert this commit (look for "Relative phase" in `git log`).

## 3. GUI fixes (`9c2c231`)

| File / function | Change | Why |
|---|---|---|
| `GlobalControls.cpp` `timerCallback` | When not in demo, re-enable the band controls if they are disabled | Same demo-gap problem as above: the controls stayed greyed out |
| `ScopeChannel.cpp` `resized` | `while` became `if` | If zoom and center were both 0 it looped forever (host freeze) |
| `ScopeChannel.cpp` `paintGridLines` | `i % juce::jmax(1, (int)beatSpacing)` | `(int)beatSpacing` can be 0 for a very narrow scope or a high zoom: integer divide by zero crashes on Intel |
| `WindowWrapperOscilloscope.h/.cpp` `parameterChanged` (commit `c90f23b`) | Calls `triggerAsyncUpdate()`; the new `handleAsyncUpdate()` does what `parameterChanged` did | `parameterChanged` runs on whatever thread changed the parameter (host automation, the audio thread). It re-laid out the scopes there (setBounds, toFront), which is GUI work off the message thread. On the Mac this aborted pluginval's AU "Editor Automation" test (found by the new CI) |

Tested by: pluginval (opens, resizes and automates the editor) on Windows, Apple Silicon and Intel; building. Undo: `git revert c90f23b` and `git revert 9c2c231`.

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

## 6. Automated test harness (Joe: "So I don't have to do manual testing")

| File | What |
|---|---|
| `plugin/tests/HostTest.cpp` (new) | A small host. It loads the BUILT plugin (the .vst3, or the installed AU) through JUCE's `AudioPluginFormatManager`, as a DAW does, and runs the checklist below. One line per check: PASS / FAIL / SKIP / INFO |
| `plugin/tests/PretendServer.h` (new) | The pretend license server, moved out of TertiaryTests.cpp so both programs share it |
| `plugin/tests/golden/*.wav` (new) | Reference renders (default, varied, free; 2 s, 48 kHz, float), made by `HostTest --make-golden` on Windows |
| `plugin/tests/run_all.ps1` (new) | One command on the mini PC: build, pluginval, host test, unit tests, renders, one summary |
| `plugin/tests/reaper/` (new) | A REAPER test, prepared but not run (REAPER isn't installed). See its README |
| `.github/workflows/installer-test.yml` (new), `plugin/tests/ci/installer_test_mac.sh`, `installer_test_windows.ps1`, `build_unsigned_installer_mac.sh` (new) | Installs the signed installers from the staging pipeline on GitHub's Macs (Apple Silicon and Intel) and Windows, and checks them |
| `plugin/CMakeLists.txt` | `HostTest` target (with `-DWLA_TESTS=ON`); `WLA_COPY_PLUGIN_AFTER_BUILD` option (default ON, as before: the runner turns it off, since copying into Program Files needs admin); Python found with `find_package` instead of assuming `python3` (on the mini PC `python3` is the Microsoft Store stub, so configuring failed) |
| `.github/workflows/test.yml`, `plugin/tests/ci/mac_checks.sh` | Build and run HostTest on Windows and both Macs (VST3, and the AU on Macs) |

What HostTest checks:
1. Load, prepare and process at 44.1, 48 and 96 kHz, block sizes from 32 to 2048 changing every block: no NaN, no denormals, sane level, silence after the input stops. Changing sample rate on a running instance: the LFO keeps its speed.
2. Every automatable parameter ramped 0 to 1 through the host's parameter API. Display parameters (and Rate, unused while synced) must leave the sound bit-identical; every other one must change it. Smooth parameters must not click (largest sample-to-sample step at most twice the reference, times 16 for gains). Switches (shape, rhythm, on/off) change instantly by design: their step is reported as INFO.
3. Session: random settings saved, opened in a new instance, recorded, closed, opened again in another. Every parameter exactly equal, the state saved again byte-identical, the render bit-identical.
4. Tempo: a synced 1/4-note LFO at 120, 90, 60 and 140 bpm, within 1 %.
5. Host bypass passes audio untouched; all bands muted = silence; solo mid = mute low and high; solo beats mute; band bypass = depth 0, per band and for all three.
6. Demo: on an unlicensed copy the effect is off from 50 to 60 s (within 0.3 s). `--expect-demo` (used in CI) makes a missing gap a FAIL; on an activated machine it's a SKIP.
7. License states with the pretend server: valid, revoked (back to demo, with the message), unreachable (unchanged), 31 days offline (a note only). This runs inside HostTest, not the plugin binary: the shipped plugin only talks to the real server and has no switch to point it elsewhere (adding one would be a back door).
8. Golden renders through the loaded binary, compared with `plugin/tests/golden`: 1e-6 (-120 dBFS) on x86_64, where Windows and Intel Macs have been bit-identical; 1e-5 (-100 dBFS) on Apple Silicon, measured at 2.7e-7 because ARM fuses multiply-adds.

Against the staging (1.0.2) VST3 it fails the sample-rate change, three tempos, and the golden "varied" render (relative phase), as it should.

What the installer test checks (macOS, on Apple Silicon and Intel): `pkgutil --check-signature`, `spctl --assess --type install`, `xcrun stapler validate`; `sudo installer`; the AU, VST3 and app where DAWs look; `codesign --verify --deep --strict` and Gatekeeper on each bundle; `auval` on the installed AU; pluginval and HostTest on the installed VST3 and AU; the standalone starts, runs 6 s and quits; uninstall leaves no files or receipts. Windows: `Get-AuthenticodeSignature` on the installer and on the installed VST3 and app, `/VERYSILENT` install, pluginval and HostTest on the installed VST3, the standalone starts and closes, the Inno uninstaller leaves no files or registry entry. It runs after every staging pipeline run (once this file is on staging), or by hand with a run ID, and a `claude/installer-test...` branch self-tests it with unsigned installers.

Results (2026-10-27):
- HostTest, 53 checks: Windows VST3 53/53 (2.5 s), Apple Silicon VST3 and AU 53/53 (about 3.5 s each), Intel Mac VST3 and AU 53/53 (about 6 s). Demo gap 50.00 to 60.00 s everywhere. Golden renders: identical on Windows and Intel; within 3.3e-7 (-130 dBFS) on Apple Silicon.
- Against the 1.0.2 VST3: 5 failures (sample-rate change, three tempos, golden "varied").
- run_all.ps1 on the mini PC: all passed, about 6 min with a fresh build, 30 s without.
- Installer self-test (unsigned installers, run 40578740468): Apple Silicon and Intel 11 steps passed, 9 signature steps skipped; Windows 7 passed, 3 skipped; no failures. About 7 to 11 min per machine, including a build. The signed run happens after the next staging pipeline run.

Real problems the installer test found, and fixed:
- **Mac packages were relocatable** (pkgbuild's default). If macOS knew another copy of a bundle (an old download, a build folder, a copied VST3), the installer updated that copy and left /Applications or the plug-in folder empty: on the Intel runner the app "installed" into the build folder. `workflow.yml` now builds each component package with `BundleIsRelocatable` off.
- **The Windows installer couldn't install silently**: the "Installation Complete!" `MsgBox` in `installer_win.iss.in` waits for a click even with `/SUPPRESSMSGBOXES`. Now only shown when not silent (`if not WizardSilent`).

Undo: revert the harness commits; nothing in the plugin depends on them.

## Results

Local, Windows 11, MSVC 2022, JUCE 8.0.8:
- Baseline: staging (9e9b39c) built with ReviewRender, 35 renders (5 inputs: the two sample_audio WAVs, Bass and Drums and Clean Electric Guitar from the web demo tracks, a generated test signal; 44.1, 48 and 96 kHz; 20 s; 7 settings). Rendered twice: bit-identical, so the renders are deterministic. In staging itself, changing only the block size moves samples by at most 5.2e-8 (-146 dBFS): float rounding.
- After: 30 of 35 renders bit-identical to staging, including both changing-block-size sets. The 5 "ramps" renders differ from 3.000 s, the moment the Mid multiplier ramp starts: max difference 2.6e-3 (-51.6 dBFS), residual -50 to -70 dB below the signal. With the single play-position line put back to 1.0.2, all 35 are bit-identical, so that one documented fix is the whole difference.
- Unit tests: all pass. Each processor test was run against the 1.0.2 code and failed there.
- pluginval 1.0.4, strictness 10, VST3: SUCCESS before and after.

CI, run 40546908596 on release-1.1 (all five jobs passed):
- Apple Silicon (macos-14, macOS 14.8; the job now runs on macos-15 because macos-14 had no capacity): VST3, AU, ReviewRender and TertiaryTests are universal (arm64 + x86_64), minimum macOS 11.0 on both slices. Unit tests pass. `auval -strict -v aufx EXPL WLAD`: AU VALIDATION SUCCEEDED. pluginval strictness 10: VST3 SUCCESS, AU SUCCESS.
- Intel (macos-15-intel, macOS 15.7, the same universal files): unit tests pass, auval succeeded, pluginval VST3 and AU SUCCESS.
- Windows (windows-latest): unit tests pass, pluginval VST3 SUCCESS.
- Web demo (Wonderlab): compiles with the new files.
- Renders, 12 per machine (3 inputs x 4 settings, 8 s): Windows vs Intel Mac bit-identical; Windows vs Apple Silicon max difference 2.7e-7 (-131 dBFS), residual about -133 dB (ARM fuses multiply-adds, so rounding differs in the last bit). Inaudible by about 100 dB.
- The first run (40543756608) found the AU abort fixed in `c90f23b`.
- Time: about 10 min per Mac build, 2 min for the Intel checks, 8 min Windows. The repo is public, so GitHub bills 0 minutes (it would be about 10 x 12 = 120 Mac quota minutes per run on a private repo).

---

## 7. Staging release (2026-10-27, Joe: "let's get Tertiary updated and pushed through to release")

Staging is at `7633d26`: release-1.1 (PR #17, now shown as merged), `claude/web-demo` (the web demo assembled with Wonderlab's `assemble-web.js`, phone zoom regions), and the eight web demo tracks from `promo-2026-10-26` (WAVs only). Wonderlab `main` has `claude/production` merged (`5248a15`). No tag.

- `run_all.ps1` on the merged code before the push: all passed (pluginval, HostTest 53/53, unit tests, renders).
- Staging pipeline, run 40588771650: success.
  - Mac: AU, VST3 and app signed with Developer ID, all packages signed, notarization Accepted, ticket stapled.
  - Windows: installer signed with Trusted Signing.
  - Installers on the Caulfield Engineering server: `https://caulfieldengineering.com/plug-ins/Tertiary/installers/TertiaryMacInstaller_1.1.0.pkg` and `.../TertiaryWinInstaller_1.1.0.exe`. The workflow's log line prints the path without `/installers/`; the files are in `/installers/`.
  - Web demo preview: `https://caulfieldengineering.com/plug-ins/Tertiary/web/`, with the eight tracks and their MP3s.
- Installer test on the SIGNED installers, run 40591256479:
  - Apple Silicon: 20/20 steps passed.
  - Intel Mac: 20/20 steps passed.
  - Windows: 8 passed, 2 failed. The installed `Tertiary.exe` and `Tertiary.vst3` are not signed, because the pipeline signed only the installer. Branch `claude/sign-windows-binaries` signs them before the installer is built: a new "Sign Windows Plug-in and App (Azure)" step, with the secrets check moved ahead of it. It's not on staging yet; pushing it there re-runs the pipeline and the installer test.

## 8. Presets (Joe, 2026-10-27: "I just remembered I wanted to implement presets")

| File | What |
|---|---|
| `PresetManager.h/.cpp` (new) | The preset list (factory, then the user's, alphabetical), load, previous/next, the modified check, Save As / Rename / Delete of user preset files, and the preset name in the session. Presets hold only the sound parameters, never the scope and spectrum view settings. |
| `FactoryPresets.cpp` (new) | The 14 factory presets, in plain code: a name and only the settings that differ from the defaults. The comment at the top lists what the numbers mean. Add one by adding a block. |
| `PresetBar.h/.cpp` (new) | The bar at the top left of the window: `<`, the name (with * when modified) opening the menu, `>`. Menu: factory presets, your presets, Save, Save As, Rename, Delete, Open Presets Folder. |
| `PluginProcessor.h/.cpp` | Owns `presets`. `getNumPrograms`/`getCurrentProgram`/`setCurrentProgram`/`getProgramName`/`changeProgramName` use it, so the DAW's preset list works (JUCE adds a "Program" parameter for VST3 hosts). `getStateInformation` adds `presetName`/`presetIsUser` to the saved tree; `setStateInformation` reads them back (none in a 1.0.x session: "No preset"). After loading a state every parameter is snapped to a legal value (see below). |
| `PluginEditor.h/.cpp` | Shows the bar over the left of the banner. |
| `TopBanner.cpp` | The demo notice is one smaller line under the bar ("DEMO - DOUBLE-CLICK TO ACTIVATE"), where it was two large lines; the old drawing is kept under `if (false)` for reference. The update link moved under the bar too. |
| `FrequencyResponse.cpp` `sliderValueChanged` | Fix: the hidden 0.001-step interface sliders fed their rounded value back to the crossover, so opening the window moved Mid-High from 1000 Hz to about 999 Hz (in 1.0.2 too). They are now updated with `dontSendNotification`. |
| `plugin/tests/regression/render_matrix.py` | The generated test signal goes to the temp folder; it had landed in `plugin/sample_audio`, the web demo's track folder. |

User preset files: `Documents/Wonderland Audio/Tertiary/Presets/<name>.tertiarypreset` (XML; values in the parameter's own units), on Windows and Mac, shared by VST3, AU and the standalone. Deleting moves the file to the Recycle Bin / Trash. The web demo gets the factory presets and the bar automatically (it compiles the same source); Save there lasts only for the visit, and "Open Presets Folder" is left out.

Two fixes the new tests found:
- The window-open crossover shift above (unit test "Opening the plugin window changes no parameter").
- pluginval's state test failed once the Program parameter changed its random values: an on/off switch left at 0.49 by a host (which acts as off) kept that raw value after a state load where it is off, because `replaceState` skips values that already mean the same. `setStateInformation` now snaps every parameter to a legal value.

Tests: unit tests (presets: the list and parameter names, Default = defaults, every preset loads, modified marker, host programs, user save/load/rename/delete in a temp folder, session recall and 1.0.x sessions, opening the window changes nothing). HostTest section 9 through the built plugin: the host sees the 14 presets in order; each loads by program number and changes the sound; Default = the golden default render; the session remembers the preset (bit-identical render); a session saved by the real 1.0.2 VST3 (`plugin/tests/golden/session-1.0.2-VST3.*`) loads with all 49 values unchanged. Regression renders: 35/35 bit-identical to the previous build. pluginval strictness 10: SUCCESS.

## 9. UI tests (Joe, 2026-10-29: "I want to check as much as possible automated prior")

The real processor and editor in a window off the screen, used the way a person uses them: mouse moves, drags, double-clicks and Ctrl+Shift-clicks fed to the window as the operating system would, key presses, the menus. After each step it checks the parameters, the controls, what the scope and crossover draw, and the sound (a short render, measuring the LFO's speed, depth and level). 23 scenarios (U-01 to U-23), each naming the test-plan IDs it covers; about a minute.

| File | What |
|---|---|
| `plugin/tests/ui/UiRobot.h` (new, generic) | The framework: mouse, keys, menus, checks, crash and hang reports with function names and lines, a leak counter and freed-memory poisoning. No Tertiary code: meant for NewPluginTemplate too |
| `plugin/tests/UiTest.cpp` (new) | Tertiary's scenarios. `UiTest --list` shows them; `--only R-06` runs one; `--snapshots <folder>` saves pictures |
| `plugin/tests/ui/smoke_standalone.py` (new, generic) | The standalone through Windows UI Automation (pywinauto): Sync, a slider, the About window, quit. Never left running; the app's settings file is put back afterwards |
| `plugin/tests/ci/ui_tests.sh` (new, generic) | Runs the UI tests in CI; failing checks go on the run's summary page |
| `plugin/source/TestHooks.h` (new) | Only the UiTest build changes behaviour (`WLA_UI_TEST_BUILD`): settings and presets in a temp folder, links and "open folder" recorded instead of opened. Every other build compiles to what it did before |
| `plugin/source/*.h` (one line each) | `friend struct UiTestAccess;` so the tests can read private state (what the scope draws, the build-id flag) |
| `plugin/source/BandControls.cpp` `setMode` | Accessible names on the band controls ("Low Depth", "Mid Sync"...), for screen readers and the smoke test. Nothing drawn or done changes |
| `plugin/CMakeLists.txt` | The `UiTest` target (with `-DWLA_TESTS=ON`); `-DWLA_UI_TEST_ASAN=ON` builds it with the address sanitiser |
| `plugin/tests/run_all.ps1` | Runs the UI tests and the smoke test; `-Asan` also runs them under the sanitiser |
| `.github/workflows/test.yml`, `plugin/tests/ci/mac_checks.sh` | The UI tests on Windows and both Macs, the smoke test on Windows, auval's codes read from config.cmake with a plain T1-13 message; runs on every work branch (not staging or production) |

Undo: revert the commit. The plugin's own code changes are the one-line friends, the accessible names, and the `TestHooks` calls (identical behaviour outside the UiTest build).

## 10. Browser demo regression suite (Codex, 2026-10-30)

The `web-tests` worktree starts at web-staging `7760137`, whose plugin code is the approved RC1b `f24dc90`. This change adds build and test infrastructure; it does not change shipping plugin sources or deploy a build.

| File | Change and purpose |
|---|---|
| `plugin/tests/web/build.mjs`, `paths.mjs`, `config.mjs` | Read product identity from `config.cmake`; build native BinaryData and both Wonderlab targets with pinned Emscripten 5.0.3; assemble the demo and record commit/dependency provenance. Product-specific settings stay in config. |
| `plugin/tests/web/inspect.cpp` | `webtest_snapshot` and `visit` expose read-only JUCE state in test builds only. No parameter setters, timer overrides or DSP replacements. |
| `plugin/tests/web/browser.mjs`, `run.mjs` | Playwright user gestures, audio measurement, state and layout checks, viewport screenshots, build identity and JSON/HTML failure reports. Analytics hosts are explicitly stubbed; plugin requests are checked. |
| `plugin/tests/web/tertiary-inspect.h`, `options-tests.mjs`, `window-tests.mjs`, `wrapper-tests.mjs` | Read scope/FFT state without changing it; exercise scope and FFT options, all Window size choices, and the host audio player. FFT Pre is checked against live input versus muted output. Window resize failures retain screenshots and exact geometry. |
| `plugin/tests/web/serve.mjs` | Loopback-only server with COOP/COEP/CORP for SharedArrayBuffer. |
| `plugin/tests/web/visual.mjs`, `refs/` | Pixel comparison and diff images; references require an explicit update and retain build provenance. A reference is visual history, not functional approval, and known product bugs still fail. |
| `plugin/tests/web/ai-review.mjs` | Advisory `codex exec --sandbox read-only` review of reference/actual pairs, saved to `out/ai-review.md`; clean skips for CI or missing Codex. Its verdict never changes the deterministic result. |
| `plugin/tests/web/harness.test.mjs`, `package.json`, `package-lock.json`, `README.md` | Harness negative checks, pinned Node dependencies and Windows setup/run/maintenance instructions. |
| `.github/workflows/test.yml` | Windows `web` job builds and runs the browser suite on work branches, then uploads `out/` as `web-test-results`. AI review is excluded. Deployment routing in `workflow.yml` is unchanged. |

Validation: run `npm run test:harness`, `npm run build` and `npm test` from `plugin/tests/web`; read `out/report.json` and inspect its screenshots. Successful build/test counts are recorded by the run, not asserted here. Report plugin defects with their screenshot paths instead of changing RC1b plugin code. See `plugin/tests/web/README.md` for environment variables and intentional reference updates.

Known existing web defect: About reports `Build unknown` because the web build does not supply the generated `build_id.h`. The displayed-build-ID check remains a hard failure; `out/desktop-about-build-id.png` records the dialog when reached. The harness does not inject a replacement header. The page configuration's correct commit stamp does not waive this failure.

Known existing web defect: selecting Window size resizes the canvas but leaves the web wrapper/stage at 750x478, cropping lower controls at 125% and additionally the right side at 150%/200%. Explicit 100% crops two border pixels. All four resize checks stay hard failures; `out/desktop-window-{100,125,150,200}.png` and matching JSON files record the evidence. Default phone/tablet/desktop layouts and their pixel references pass.

Undo: revert the browser-test commit, removing its `web` job and `plugin/tests/web` files. No plugin behavior or deployment route needs restoring. Generated `build/` and `out/` files are local artifacts.

## Suggestions I did not change

1. ~~Relative phase is twice what the knob says, in the audio.~~ Fixed after Joe's yes: section 2b.
2. **The repo is public on GitHub**, not private (the API reports `private: false`). The source and the license logic are readable by anyone. Settings, Danger Zone, Change visibility, if you want it private. (Public also means Actions minutes are free.)
3. **The FFT pickoff choice names are swapped**: the parameter's "Input" (index 0) makes the spectrum show the output (`setFftPickoffPoint`). The options menu toggle is right; only the names a DAW shows in automation lanes are backwards.
4. ~~NewPluginTemplate uses the same codes as Tertiary.~~ Fixed after Joe's yes, in the NewPluginTemplate repo (see the PR description).
5. **Crossover order can be crossed by automation.** The GUI keeps low-mid below mid-high; host automation can set them the other way round. A `jmin`/`jmax` in `updateState` would guard it, but it changes the sound of such automation.
6. **The full wavetable is recalculated on the audio thread** whenever an LFO parameter moves (24,000 points of trig per LFO at 48 kHz). No allocation, but a CPU spike during automation of depth, skew or shape. Could move to a background thread with a swap, but it is a bigger change.
7. **`LFO::mPosition` is a float** counting up to 24,000 with steps as small as 0.25, so slow free-running rates are off by a fraction of a percent. A `double` would fix it but changes the sound slightly.
8. **Version check needs a key.** The server's `version_check` route refuses requests without a valid key, so demo users never hear about updates. A public JSON file on the site (or the WooCommerce product's version) would let demo copies show the link too. Also: the version comes from the product's "Software version" field (`_software_version`) in WooCommerce; set it to 1.1.0 when you release, or no one sees anything.
9. **The standalone package's identifier** (`...Tertiary.app` in workflow.yml) doesn't match its `pkg-ref` id in the distribution files (`...Tertiary.standalone`). The installer works today; aligning them avoids odd receipts.
10. **Unused code**: `WLDebugger` (all calls commented out; `cleanupInstance()` sets the pointer to null before deleting it, so it leaks), `LockFreeQueue.h` (not included anywhere), `Params.cpp` (empty).
11. **Small, harmless**: `FrequencyResponse::makeAttachments` makes `lowMidAttachment` twice; `getPlayPosition()` returns an `std::atomic` by value; the meters read `LinearSmoothedValue` across threads (fine in practice).
12. **Deploy workflow**: the Mac "Sign" steps skip silently if secrets are missing, so an unsigned installer can be uploaded. A tag build could fail instead.
13. **JUCE 8.0.8 use-after-free in its Windows accessibility code** (found by the UI tests under the address sanitiser, 2026-10-29): `AccessibilityNativeHandle::get_ProviderOptions` uses its element without checking it is still valid, so a screen reader or another UI Automation client asking about a pop-up menu that has just closed reads freed memory. Rare, and in JUCE, not Tertiary; a newer JUCE may have fixed it. `run_all.ps1 -Asan` lists these reports without failing on them.


## Codex 2026-10-30: browser demo fixes and device coverage

- Added production web glue used by local builds, both CI web builds and QuickScripts: desktop build-ID generator for both WASM targets, visual-viewport fitting/refitting, close stale popup menus on rotation, and upload removal clears name/selection/transport and invalidates pending loads. Wonderlab stays pinned and its checkout is not modified.
- Tertiary alone keeps the existing size submenu and About panel, with disabled size/licensing fields and plain hints under JUCE_EMSCRIPTEN. Native plugin behavior remains RC1b. Local full Windows run: all eight groups passed, 90 host checks, all unit/UI/smoke checks, and 63 renders bit-identical to RC1b.
- Added iPhone 13/15 WebKit and Pixel 7 Chromium profiles in both orientations. Windows WebKit has no AudioContext, so CI runs iPhones on macOS using the same Windows-built WASM artifact. ci-ok requires both browser jobs. Real taps, native Chromium touch drags, WebKit TouchEvent handler checks, fit/rotation/visual viewport checks, build identity, disabled activation, continuous audio over a real 65-second wait, explicit sub-40px target warnings, and per-device screenshot references.
- Codex read-only review found and corrected test issues (build separator, touch axis/native injection, visual-only resize and audio observation over the whole timer interval). Final browser counts and screenshots are in plugin/tests/web/out/report.json and report.html; native results are in build-tests/results. No release, staging/production push, or tag.

## Codex 2026-10-30: Logic and microphone harnesses

`plugin/tests/daw/logic/` adds the saved two-track Logic project and local scripted M-09 / M-07 (V-08) runners. Logic stages the selected AU with backup/restore, runs strict auval, bounces 24 seconds, checks the editor/About build ID and compares exact float PCM. The microphone runner builds fix `25b0a55`, captures/accepts the app's real consent prompt and verifies its TCC grant read-only. See that folder's README for requirements, output contract, layout migration and the physical-input limitation.

Validation: RC1b `f24dc90` on M1 / macOS 26.6 / Logic 12.4; two baseline bounces and two complete scripted regression runs matched with tolerance 0. Editor 750 x 478, About f24dc90. Microphone consent and hardened-runtime audio-input entitlement PASS with a temporary signed software input, removed afterward. No plug-in code changed. Undo by reverting the Logic/microphone harness commit.

## Repo-root release test layout (Codex, 2026-10-30)

Joe asked for individually runnable scripts, a complete ordered release checklist, and evidence for AI review.
The existing `test-layout` worktree was reused at merge `17b85ad` (web-staging plus mac-day-harness).
The separate `web-tests` branch/worktree was left alone.

- `tests/sequence.json`, `tests/win`, `tests/mac`, `tests/web`, `tests/lib`, `tests/README.md`: one configurable
  runner and JSON contract, PASS/FAIL/SKIP/PLANNED, failure-first summaries, binary identities, bounded host
  automation, backup/restore journals and visible future coverage. Product paths/codes/settings are in `config.cmake`.
- Existing suites stay under `plugin/tests`. `run_all.ps1` now exports its rows and bounds subprocesses;
  the standalone smoke accepts a screenshot destination. Mac validation/screenshots reuse HQ's Mac-day phases.
- Cubase's export harness accepts the installed host/result paths, records its owned PID, opens the relocated
  project and verifies its mapped plugin hash. Portable copies of the existing production helpers live in tests/win.
  Root cleanup uses the host's Quit before its documented owned-PID fallback. Goldens are never rewritten.
- REAPER's Python/Lua harness adds isolated artifacts/profile, a loaded-module path/hash handshake and bounded
  Windows startup/quit handling. Both Intel and ARM path keys retain the repo build after standard folders;
  old installed copies cannot silently produce a PASS. Render assertions are unchanged.
- `plugin/tests/HostTest.cpp`: reuses section 11 from `fix-instance-isolation`, with its original attribution.
  `PerfTest.cpp`: guards the Windows-only JUCE macro so the existing performance tool compiles on Mac.
  `.gitattributes` preserves shell LF endings when the checkout is copied from Windows.
- The generic skeleton and native-suite adapter are also ported to NewPluginTemplate's `pipeline-sync`, with
  empty product-specific controls/harness paths explicitly PLANNED. No other agent's web changes are included.

Validation: real full Windows and M1 sequences, followed by targeted retries after runner fixes; original
attempts remain in each report's history. Eleven contract/restoration checks pass in both repositories and
on the M1. PowerShell/Python/Bash syntax and the future Logic AppleScript compile were checked. See
`tests/verification-2026-10-30.md` for final outcomes and evidence locations. Logic's existing session was left open.

Undo: revert the commit containing this entry. Test output/build folders are ignored; no runtime DSP change
is included. Restore journals retain recovery paths if a future host prevents restoration.


## Test layout follow-up (Codex, 2026-10-30)

- `tests/sequence.json`: all 35 Blocks test IDs, GitHub checkboxes and suite relations; retained local T2-03.
- `tests/win`, `tests/mac`, `tests/web`: independent row scripts; platform product folders and discovery runners.
- `tests/lib/native.py`: splits unit, LFO, UI robot, host, validator, render, AU and screenshot phases without
  changing their assertions. Mac architecture/minimum-OS checks and pre-layout CI counts remain enforced.
- `tests/lib/run.py`, `common.py`, `hosts.py`: IDs in JSON/summary checks, CI selection, product aggregation,
  explicit coverage gaps, and protection against another worktree's active test processes. UI tests receive
  an isolated temporary settings directory; another worktree's UiTest can otherwise overwrite saved-size assertions.
- Both workflows call the same scripts; test summaries print before upload. Seven-day test/staging artifacts;
  release installer retention unchanged. Original build/sign/deploy steps unchanged; release tests additionally
  build the ReviewRender test tool. No plug-in code or T2-03 threshold changed.
- Windows Run-All forwards its browser selection and defaults to Chromium; Mac WebKit stays in its
  existing job. Copied Mac AUs regain executable permission and use the old CI's forced registrar refresh.

Validation: 19 runner/restoration tests on Windows and Mac, syntax checks, exact Blocks metadata and workflow
retention checks. Windows main: 16 PASS, expected T2-03 FAIL, 14 PLANNED, 2 SKIP; independent product runner
passed all three scripts. Physical Mac main: 30 PASS, expected T2-03 FAIL, 13 PLANNED, 1 SKIP. All five Tertiary
CI jobs passed at e0e1319, including both Macs and cross-platform audio comparison. Template Windows: 62 PASS,
no failures; hosted template CI is blocked before execution by GitHub billing/spending restrictions.
See `tests/verification-layout-2026-10-30.md` for counts, build provenance and remaining coverage.
The separate fix-first-block worktree is untouched.

Undo: revert this layout commit; the older combined wrappers and workflows return together. Ignored result
artifacts preserve per-run evidence and binary hashes.
