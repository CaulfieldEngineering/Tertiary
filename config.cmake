# config.cmake

# Define global variables
set(wPLUGIN_NAME                "Tertiary")
set(wCOMPANY_NAME               "Wonderland Audio")
set(wCOMPANY_ID                 "WonderlandAudio")
set(wPROJECT_VERSION            "1.1.0")
set(wPLUGIN_MANUFACTURER_CODE   "WLAD")
# [Claude 2026-10-27] 1.1.0, and AU added (Mac only; JUCE leaves AU out on Windows by itself).
# The codes above and below are unchanged: they are Tertiary's identity in every DAW session (VST3 and AU).
set(wFORMATS                    VST3 AU Standalone)

set(wIS_SYNTH                   FALSE)
set(wNEEDS_MIDI_INPUT           FALSE)
set(wNEEDS_MIDI_OUTPUT          FALSE)
set(wPLUGIN_CODE                "EXPL")
set(wPLUGIN_CATEGORY            Modulation)

# [Claude 2026-10-29] The WooCommerce product on the store (Serial Numbers plugin). On a v* tag the pipeline
# checks it has a working key source before publishing, and sets its "Software version" to the tag's version
# once the new installers are live (that is what the plugin's version check reports). Tertiary is 45.
set(wWOO_PRODUCT_ID             "45")

# [Claude 2026-10-30] The web demo folders on CaulfieldEngineering.com/plug-ins/{repo}/. A staging push deploys only
# to the private preview (noindex); a v* tag promotes that preview to the live folder and to WonderlandAudio.com.
set(wWEB_STAGING_DIR            "web-staging")
set(wWEB_LIVE_DIR               "web")

# [Claude 2026-10-30] The public changelog page on the store's site (WordPress page id). On a v* tag the pipeline writes
# CHANGELOG.md into that page's changelog block and the version into the home page's version line
# (.github/scripts/changelog.py). Leave it empty for a plugin with no changelog page. Tertiary's is /changelog/, 4716.
set(wCHANGELOG_PAGE_ID          "4716")

# [Codex 2026-10-30] Local release scripts. Paths are relative to THIS repo, never another worktree.
set(wPRODUCT_SLUG "tertiary")
set(wTEST_BUILD_DIR "build-tests")
set(wTEST_CONFIGURATION "Release")
set(wTEST_ASAN_BUILD_DIR "build-asan")
set(wTEST_GOLDEN_DIR "~/.wla-test/goldens/${wPRODUCT_SLUG}")
set(wTEST_AU_TYPE "aufx")
set(wTEST_CUBASE_RUNNER "plugin/tests/daw/cubase/tertiary_harness_run.ps1")
set(wTEST_CUBASE_BUILDER "plugin/tests/daw/cubase/tertiary_harness_build.py")
set(wTEST_REAPER_RUNNER "plugin/tests/reaper/run_reaper_test.py")
set(wTEST_LOGIC_PROJECT "tests/mac/logic/${wPLUGIN_NAME} harness.logicx")
set(wTEST_LFO_SCRIPTS "plugin/tests/regression/lfo_host_lock.py;plugin/tests/regression/lfo_rate_step.py")
set(wTEST_SETTINGS_WIN "%APPDATA%/${wPLUGIN_NAME}/${wPLUGIN_NAME}.settings")
set(wTEST_SETTINGS_MAC "~/Library/Application Support/${wPLUGIN_NAME}.settings")
set(wTEST_SMOKE_TOGGLE "Low Sync")
set(wTEST_SMOKE_APPEARS "Low Rate")
set(wTEST_SMOKE_SLIDER "Low Depth")
set(wTEST_SMOKE_MENU_AT "727,25.75")
set(wTEST_SMOKE_LAYOUT "750x478")
set(wTEST_SMOKE_MENU_ITEM "About ${wPLUGIN_NAME}...|Activate ${wPLUGIN_NAME}...")
set(wTEST_SMOKE_DIALOG_BUTTON "Activate|Deactivate this computer")
set(wTEST_SMOKE_CLOSE_BUTTON "X")
