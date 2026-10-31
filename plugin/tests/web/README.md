# Browser demo regression tests

The suite builds the real Wonderlab/Emscripten demo and drives it with Playwright. The same production glue in `plugin/web/wonderlab.cmake` and `plugin/web/assemble.mjs` is used locally and by both CI web jobs. Nothing is deployed by the test job. Work belongs on work branches; never create a release or push staging/production to run these tests.

## Setup and run

Use Node 24, Python 3, CMake, Ninja, FFmpeg, Visual Studio C++ tools, and Emscripten **5.0.3**. Wonderlab is pinned at `5248a157cfdf472caf1fafc7b9b71dec09485947`. Set `WONDERLAB_DIR` to that checkout (default `build/Wonderlab`). `CPM_JUCE_SOURCE` may point to a native JUCE 8.0.8 checkout; the web build uses Wonderlab's fork. Template builds also set `WIZARDCORE_DIR` to a WizardCore checkout. Never put credentials in these files.

Activate the SDK in the build shell, then from `plugin/tests/web`:

```text
npm ci
npx playwright install chromium webkit
npm run build
npm run test:harness
npm test -- --no-ai --engine=chromium
```

On Windows, `EMSDK` and `EMSDK_PYTHON` must contain Windows paths, and the SDK's `upstream/emscripten` must be on PATH. `WEB_BUILD_DIR` and `WEB_BUILD_JOBS` override the output directory and compiler parallelism. `npm run serve` opens a local server at `http://127.0.0.1:4173` (no deployment). The server sends the isolation headers required by shared-memory audio.

`test-build.json` records the full and abbreviated commit, compiled build ID, SDK and Wonderlab commit. Both web executables get `build_id.h` from the **desktop generator** `plugin/cmake/build_id.cmake`, including its dirty marker and date. This is production build wiring, not a header fabricated by the inspector. W-00 rejects a stale build; W-15 and M-05 compare the compiled value with that generator and the exact Git abbreviation.

## Coverage

The existing W checks run in Chromium at phone, tablet and desktop sizes, exercise every product control and preset, measure audio against bypass, exercise the player and its non-SharedArrayBuffer fallback, and check screenshots and request/console failures. Product adapters supply meaningful DSP expectations; the template is intentionally a passthrough with no plugin controls.

The revised web policy is explicit: the Window size submenu stays where desktop users find it, its existing 100/125/150/200% choices are disabled, and it says "Resize your browser to change the size". Desktop has no 175% choice, so the web build does not invent one. W-31 tries each disabled field. W-32 resizes one live page through desktop, short desktop, tablet, phone, landscape and back, checking canvas clipping, viewport bounds and horizontal overflow. The browser uses `visualViewport.height` so browser chrome and rotation change the fitting budget. Deliberate phone zoom still lets users inspect individual bands.

About retains its email/key fields and action button, greyed out, with "Activation is not needed in the browser demo". The web processor never starts licensing checks or enters the 50/10-second demo bypass cycle. W-33/34/35 and M-01/08/11 check load, user-requested About, inert activation fields, interactions and **65 real seconds**, sampling throughout for transient nags and uninterrupted audible output from a known looping tone. No timers or licensing results are mocked. The shared-source changes are behind `JUCE_EMSCRIPTEN`; native behavior is checked with `plugin/tests/run_all.ps1` and the RC1b render reference.

W-29 checks removal clears the uploaded filename, selected row/index, source, buffer, transport position and file input. Production removal cancels an in-flight decode, too.

### Mobile checks (part of the same CI suite)

M-01 through M-12 run for **iPhone 13**, **iPhone 15** (WebKit), and **Pixel 7** (Chromium), in both portrait and landscape. They use Playwright's complete device profiles: mobile UA, device pixel ratio, touch and the viewport that excludes browser chrome. They verify one trusted **tap** starts actual audible playback, touch drags and menu taps change plugin parameters, disabled fields stay inert, no horizontal scroll, browser-chrome resizing and rotation re-fit (including a Chromium visual-viewport-only pinch resize with unchanged layout viewport), and one screenshot reference per device/orientation.

Playwright provides trusted touchscreen taps but no cross-browser swipe API. Chromium knob drags use native CDP touch injection, including browser gesture arbitration. WebKit continuous drags send DOM `TouchEvent` sequences through JUCE's normal touch handlers, with trusted touchscreen taps for menus. Neither path uses mouse drags or parameter setters. These are desktop browser engines with device emulation, not a physical-device Safari/Android certification.

M-09 measures CSS hit areas and explicitly **flags targets below 40 px** in `report.html`, `report.json` and each `*-touch-targets.json`. Small full-view canvas controls are warnings, not silently called large enough. The existing phone zoom provides larger band controls; its target audit is also saved. A new plugin needs its own control/zoom adapter. `--mobile-only` is a local diagnostic subset, never the CI invocation.

## Results and reference review

`out/report.html` and `out/report.json` hold individual test IDs, counts and touch-target warnings. Screenshots, geometry, controls, pixel diffs and AI advice are alongside them. Original clipping evidence is preserved locally under `out/before-fixes/desktop-window-{100,125,150,200}.png`; replacement evidence is `out/desktop-window-*-disabled.png` and `out/*-fit-after.png`.

The Windows `web` job runs Chromium and Pixel profiles with `--engine=chromium`. `web-mobile` downloads that exact WASM build on macOS 15 and runs all iPhone profiles with `--engine=webkit`: Windows WebKit has no Web Audio. Both jobs upload results even on failure. Together they run the whole suite. A machine with audio-capable WebKit can omit `--engine` to run everything locally. The `ci-ok` gate requires every test job, including `web` and `web-mobile`. Analytics hosts listed in `config.mjs` alone are fulfilled locally; plugin assets, audio, fonts and other requests are never excluded from error checks.

`inspect.cpp` is compiled only into test targets. It reads real JUCE bounds, controls, parameters, menus, licensing state and build identity. It cannot set parameters, replace DSP or override timers. Audio checks call the shipping entry points with the live AudioContext suspended to avoid concurrent DSP calls. `npm run build -- --no-inspector` builds the deployment-equivalent targets without test exports.

Update screenshot references only for an intended visual change:

```text
npm run refs -- --no-ai
npm test -- --no-ai --engine=chromium
```

Inspect **every** new/changed screenshot before accepting it. CI cannot update references. Pixel comparison uses threshold 0.12 and maximum differing-pixel ratio 0.001; missing references and dimension changes fail. Do not change thresholds to cover a bug. `refs/build.json` records provenance.

Normal local runs may invoke `codex exec --sandbox read-only` for advisory review of differing screenshots; `--no-ai` skips it, and `npm run review` reruns it. AI advice never overrides the pixel gate. `npm run test:ai` is the separate synthetic visual-review self-test.

## Generic production port

`plugin/web/patch-shell.mjs` applies small checked overrides to the pinned upstream shell after assembly. It fails if an upstream anchor changes, so upstream updates cannot silently lose the fixes. It never edits the dependency checkout. Local scripts and deployment CI call the same assembler.

NewPluginTemplate carries the generic build ID, fitting, upload removal, browser runner, device matrix, reports and CI wiring. Its blank editor has no licensing or sizing panel to grey out. When adding one, follow the web-only guard policy above; do not add a browser activation path. Product-specific knobs, scope and preset expectations stay in their adapter.
