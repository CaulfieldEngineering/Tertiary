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
