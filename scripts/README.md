# Build and release scripts

<!-- [Claude 2026-10-31] Replaces run_local/ (Joe approved the layout, 2026-10-31: "yes perfecto"). Same pattern as tests/. -->

The build is written once. A desk and GitHub run the same scripts: `.github/workflows/test.yml` (work branches) and
`workflow.yml` (staging and `v*` tags) call them for the build, installer, Mac signing and notarization, verification
and web steps. The order is `sequence.json`; `lib/check-sequence.py` (run in test.yml's compare job) fails when the
sequence and the workflows drift apart.

```text
scripts/
  sequence.json            the build steps B-01..B-07, in order, read by the suites and checked against GitHub
  win/                     Build-Plugin.ps1, Sign-Windows.ps1, Build-Installer.ps1, Verify-Installer.ps1, Build-All.ps1
  mac/                     build-plugin.sh, sign-plugin.sh, build-installer.sh, sign-notarize.sh, verify-installer.sh, build-all.sh
  web/                     build-web.ps1/.sh, bundle.mjs, deploy-staging.ps1/.sh (the private web-staging preview only)
  lib/                     common.ps1/.sh (config.cmake, step reporting), report.py, tools.py + tools.json, check-sequence.py
  results/YYYY-MM-DD_HHMM/ progress.jsonl, <ID>.json, <ID>-artifacts/, summary.md/json (ignored by git)
  draft-changelog.mjs      unrelated helper (CHANGELOG drafts)
```

| ID | Step | Windows | Mac | GitHub |
|---|---|---|---|---|
| B-01 | Plug-in: configure and build the formats in config.cmake | `win/Build-Plugin.ps1` | `mac/build-plugin.sh` | test.yml mac-build, windows; workflow.yml build |
| B-02 | Sign plug-in (Azure Trusted Signing / Developer ID Application, app with the audio-input entitlement) | `win/Sign-Windows.ps1 -Target plugin` | `mac/sign-plugin.sh` | workflow.yml build (Mac via the script; Windows via the pinned `azure/trusted-signing-action`) |
| B-03 | Installer (Inno Setup / pkgbuild, productbuild, productsign) | `win/Build-Installer.ps1` | `mac/build-installer.sh` | workflow.yml build |
| B-04 | Sign installer (Windows) / notarize and staple (Mac) | `win/Sign-Windows.ps1 -Target installer` | `mac/sign-notarize.sh` | workflow.yml build |
| B-05 | Verify installer: every signature, notarization, Gatekeeper; writes `.sha256` | `win/Verify-Installer.ps1` | `mac/verify-installer.sh` | workflow.yml build (when signing ran) |
| B-06 | Web demo: BinaryData, WebAssembly, assembled page, `build/web-bundle` | `web/build-web.ps1` | `web/build-web.sh` | test.yml web, web-mobile (`--test`); workflow.yml web-build |
| B-07 | Web staging preview, private and noindex (manual; never from Build-All) | `web/deploy-staging.ps1` | `web/deploy-staging.sh` | workflow.yml web-build, staging pushes only |

## Run

Prerequisites: CMake 3.25+, a C++ compiler (Visual Studio 2022 Build Tools / Xcode command line tools), Python 3.9+,
Node 20+ and Git. Everything else is fetched by the scripts into `.tools/` (below). No admin rights or sudo: nothing
is copied into the system plug-in folders (`-CopyToSystem` / `--copy-to-system` opts in) and nothing is installed
outside this checkout.

```powershell
# Windows: everything (plug-in, sign, installer, verify, web), or one step
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\win\Build-All.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\win\Build-All.ps1 -NoWeb -DryRun
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\win\Build-Plugin.ps1 -BuildDir build -Tests -ReviewRender
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\web\build-web.ps1 -Test        # what the browser tests need
```

```bash
# Mac
bash scripts/mac/build-all.sh --universal --adhoc        # --adhoc: exercise the signing commands without a certificate
bash scripts/mac/build-all.sh --dry-run
bash scripts/mac/build-plugin.sh --build-dir build-tests --tests --review-render
```

Each step script runs on its own (exit 0 PASS, 1 FAIL, 2 SKIP, 3 PLANNED; on GitHub only FAIL is non-zero) and takes
`-DryRun` / `--dry-run`, which prints every command and changes nothing. Outputs stay in repo-relative folders:
`build/` (plug-ins under `build/plugin/<Name>_artefacts/Release/`, `build/<Name>WinInstaller_<version>.exe`,
`build/<Name>MacInstaller_<version>.pkg`), `build/web/` and `build/web-bundle/`.

Signing and notarization need credentials a desk normally does not have. Without them those steps report **SKIP**
with what they would have signed, and the build still produces unsigned installers. On a `v*` tag missing secrets are
**FAIL**, as before. Optional desk identities: the six `AZURE_*` variables for `Sign-Windows.ps1`;
`WLA_MAC_SIGN_IDENTITY` / `WLA_MAC_INSTALLER_IDENTITY` (already in the login keychain, never changed) and `APPLE_ID`,
`APPLE_TEAM_ID`, `APPLE_APP_SPECIFIC_PASSWORD` on the Mac. On GitHub, the Mac steps read the same secrets into a
temporary keychain exactly as the workflow always did.

Deploys: live demos and installers are deployed only by `workflow.yml` on a `v*` tag; there is no live deploy from a
desk (the old `run_local/QuickScripts` did that and were removed). `deploy-staging` only updates the private
`web-staging` preview, and from a desk only when the checkout is clean, is the current `origin/staging` commit and the
bundle was built from it without the test inspector.

## Results

The same shape as `tests/` (`tests/README.md`, "Results" and "Live progress"): each step appends `test-start` /
`test-finish` to `progress.jsonl` and writes `<ID>.json` with one check and its log under `<ID>-artifacts/`;
Build-All / build-all.sh add `run-start` / `run-end` (with `"kind": "build"`) and `summary.md/json`, and print the
start, end and duration. `WLA_BUILD_RESULTS` picks the folder (GitHub uses `tests/results/build`, so each job's
results upload carries it). The test dashboard can read these folders the same way as test runs.

## Tools: portable, pinned, repo-local

Joe, 2026-10-31: "Anything like pluginval gets installed to a local folder during the script instead of hardcoding
its location on the mini PC." `lib/tools.py` reads `lib/tools.json` and gives each tool from, in order: its override
variable, `.tools/<tool>-<version>/` (git-ignored; `WLA_TOOLS_DIR` moves it, for example to share one emsdk between
checkouts), or a fresh download there with its SHA-256 checked before it is unpacked (git tools: the pinned commit).
Tests use the same lookup (`tests/lib/common.py tool()`), so `$HOME\.wla-test\env.ps1` and `~/.wla-test/env.sh` are
optional overrides only.

| Tool | Version | Override | Fetched |
|---|---|---|---|
| pluginval | v1.0.4 (CI's tag) | `WLA_PLUGINVAL` | automatically, Windows and Mac zips |
| Inno Setup | 6.7.4 | `WLA_ISCC` | automatically, its installer in per-user portable mode into `.tools` (no registry entry, no admin) |
| ninja | v1.13.2 | `WLA_NINJA` (or on PATH) | automatically, for the web build |
| Emscripten SDK | 5.0.3 (CI's version) | `EMSDK` (activated), or `WLA_EMSDK_ENV` (an `emsdk_env.ps1`) | automatically: emsdk at the 5.0.3 tag, then `emsdk install/activate 5.0.3` (about 1 GB, once) |
| Wonderlab | 5248a15 (the workflows' pin) | `WONDERLAB_DIR` | automatically, cloned with this machine's own git credentials (private repo) |
| REAPER | 7.84 | `WLA_REAPER` | **opt-in only**: `python scripts/lib/tools.py install reaper` (portable install). Its licence is a 60-day evaluation, then a paid licence; an installed REAPER is used when no `.tools` copy exists |

`python scripts/lib/tools.py list` shows what is present; `path <tool>` fetches one and prints it.

### Not portable yet, and a suggested home

| What | Today | Suggested home |
|---|---|---|
| Approved reference renders (W-04 previous-release comparison) | `WLA_TEST_REFERENCE`, a folder on the mini PC (`~/.wla-test/references/...`) | A GitHub release asset per approved version (a zip with PROVENANCE.txt and its SHA-256 in `tools.json`), fetched like the tools; or a read-only Dropbox folder |
| Machine goldens (Cubase, Logic) | `wTEST_GOLDEN_DIR` = `~/.wla-test/goldens/<slug>` | The same release-asset scheme, one per host and machine |
| Wonderlab without git access (the Mac has no GitHub credentials) | `WONDERLAB_DIR`, or a clone when git can reach the private repo | A git submodule pinned at the same commit, or a CPM package from a tagged Wonderlab release, so a plain `git clone --recursive` brings it |
| Cubase Pro 12, Logic Pro, FL Studio, Ableton, Bitwig, Studio One | Installed by Joe; tests SKIP without them | Stay installed per machine (licensed, interactive installers); document the version each test expects |
| REAPER licence | Joe's installed, licensed copy; `.tools` copy is evaluation only | Keep the installed copy as the default for release runs; `.tools` for fresh or CI-like machines |
| CMake on the Mac | `WLA_TEST_CMAKE` = `~/Library/Python/3.9/bin/cmake` (pip) | A pinned CMake in `tools.json` (Kitware's universal tarball), or Homebrew |
| Signing identities | GitHub secrets; none on a desk | Stay as secrets; a desk signs only when its own identity is set (above) |
| Mac desktop session, screen-capture and microphone permissions | Granted on the Mac mini by hand | Stay per machine (macOS privacy settings cannot be scripted safely) |
