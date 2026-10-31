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
