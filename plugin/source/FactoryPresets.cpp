/*
  ==============================================================================

    FactoryPresets.cpp
    Created: 27 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-27] New file. The factory presets, built into the plugin so they always
    exist. To add one, add a block below: a name, then only the settings that differ from the
    defaults. Names are the parameter names in Params.h; values are in the parameter's own
    units (Hz, %, degrees, dB). For a choice, the value is its position in the list:

      Waveshape   0 Ramp Down, 1 Ramp Up, 2 Square, 3 Triangle, 4 Sine, 5 Hills, 6 Valleys
      Multiplier  0 1/2 Note, 1 1/4 Note, 2 1/4 Note Triplet, 3 1/8 Note, 4 1/8 Note Triplet, 5 1/16 Note
      On/off      0 or 1

    The defaults: every band Sine, 75 % depth, 50 % skew, synced at 1/4 note, 0 degrees,
    0 dB; crossovers 500 Hz and 1 kHz. "Default" (the first) is exactly that.

    [Claude 2026-11-03] 1.1.1, Joe: "most should use different shapes, rates and phases per
    band ... a few where the low end is bypassed and there's tremolo on the mid/top. Or vice
    versa ... and then a few wacky ones." The presets whose bands were identical are gone or
    reworked. Order: Default, then the clearest mixed-band presets, then split by register,
    then the unusual ones. Every band gain stays 0 dB and no depth is over 90 %, so changing
    preset never jumps the level. Kept in place for the tests: "Hard Chop" (Square low band)
    is 6, "Alternating Chop" (mid inverted) is 7. The web demo opens on "Gentle Movement"
    (WLA_WEB_DEFAULT_PRESET, plugin/web/wonderlab.cmake).

  ==============================================================================
*/

#include "PresetManager.h"

const std::vector<PresetManager::FactoryPreset>& PresetManager::getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets =
    {
        // 0  The plugin's defaults
        { "Default", {} },

        // ---- Every band its own shape, rhythm and phase --------------------------------------

        // 1  Joe's example: a slow triangle under the lows, a lazy sine in the mids, a fast sine on top
        { "Gentle Movement", {
            { "Waveshape Low Band", 3 }, { "Multiplier Low Band", 3 }, { "Depth Low Band", 53.5f },
            { "Multiplier Mid Band", 0 }, { "Depth Mid Band", 51 }, { "Relative Phase Mid Band", 60 },
            { "Multiplier High Band", 5 }, { "Depth High Band", 70.5f }, { "Relative Phase High Band", 120 } } },

        // 2  Quarters, quarter triplets and eighths, three shapes
        { "Three Rhythms", {
            { "Depth Low Band", 70 },
            { "Waveshape Mid Band", 3 }, { "Multiplier Mid Band", 2 }, { "Depth Mid Band", 65 }, { "Relative Phase Mid Band", 90 },
            { "Waveshape High Band", 2 }, { "Multiplier High Band", 3 }, { "Depth High Band", 45 }, { "Skew High Band", 40 },
            { "Relative Phase High Band", 180 } } },

        // 3  A pumping low end, slow mids, a quick triangle shimmer on top
        { "Pulse and Shimmer", {
            { "Low-Mid Crossover Freq", 250 }, { "Mid-High Crossover Freq", 3000 },
            { "Waveshape Low Band", 1 }, { "Depth Low Band", 80 }, { "Skew Low Band", 30 },
            { "Multiplier Mid Band", 0 }, { "Depth Mid Band", 35 },
            { "Waveshape High Band", 3 }, { "Multiplier High Band", 5 }, { "Depth High Band", 55 }, { "Relative Phase High Band", 90 } } },

        // 4  Hills, sine and valleys rolling at three speeds
        { "Rolling Waves", {
            { "Waveshape Low Band", 5 }, { "Multiplier Low Band", 0 }, { "Depth Low Band", 60 },
            { "Multiplier Mid Band", 2 }, { "Depth Mid Band", 55 }, { "Relative Phase Mid Band", 120 },
            { "Waveshape High Band", 6 }, { "Multiplier High Band", 4 }, { "Depth High Band", 50 }, { "Relative Phase High Band", -120 } } },

        // 5  Slow rise and fall, each band in its own place in the cycle
        { "Slow Swell", {
            { "Waveshape Low Band", 5 }, { "Multiplier Low Band", 0 }, { "Depth Low Band", 50 },
            { "Waveshape Mid Band", 6 }, { "Multiplier Mid Band", 0 }, { "Depth Mid Band", 45 }, { "Relative Phase Mid Band", 120 },
            { "Depth High Band", 40 }, { "Relative Phase High Band", -120 } } },

        // 6  Square eighths in the lows, square sixteenths in the mids, a triangle on top
        { "Hard Chop", {
            { "Waveshape Low Band", 2 }, { "Multiplier Low Band", 3 }, { "Depth Low Band", 90 },
            { "Waveshape Mid Band", 2 }, { "Multiplier Mid Band", 5 }, { "Depth Mid Band", 70 }, { "Relative Phase Mid Band", 90 },
            { "Waveshape High Band", 3 }, { "Multiplier High Band", 5 }, { "Depth High Band", 50 } } },

        // 7  Low and mid chop on alternate eighths, hills on top
        { "Alternating Chop", {
            { "Waveshape Low Band", 2 }, { "Multiplier Low Band", 3 }, { "Depth Low Band", 85 },
            { "Waveshape Mid Band", 2 }, { "Multiplier Mid Band", 3 }, { "Depth Mid Band", 85 }, { "Invert Mid Band", 1 },
            { "Waveshape High Band", 5 }, { "Multiplier High Band", 5 }, { "Depth High Band", 50 }, { "Relative Phase High Band", 45 } } },

        // 8  Free-running, three speeds and three shapes, not tied to the tempo
        { "Free Flutter", {
            { "Sync to Host Low Band", 0 }, { "Sync to Host Mid Band", 0 }, { "Sync to Host High Band", 0 },
            { "Rate Low Band", 4.5f }, { "Rate Mid Band", 6.0f }, { "Rate High Band", 7.5f },
            { "Waveshape Low Band", 3 }, { "Depth Low Band", 45 },
            { "Depth Mid Band", 50 },
            { "Waveshape High Band", 5 }, { "Depth High Band", 40 } } },

        // ---- One register moves, the rest holds still ----------------------------------------

        // 9  Bass and kick untouched; chords and air move
        { "Steady Lows, Busy Top", {
            { "Low-Mid Crossover Freq", 250 }, { "Mid-High Crossover Freq", 2500 },
            { "Bypassed Low Band", 1 },
            { "Multiplier Mid Band", 3 }, { "Depth Mid Band", 60 },
            { "Waveshape High Band", 3 }, { "Multiplier High Band", 5 }, { "Depth High Band", 65 }, { "Relative Phase High Band", 90 } } },

        // 10 The low end pumps on the beat; everything above it stays put
        { "Busy Lows, Steady Top", {
            { "Low-Mid Crossover Freq", 200 }, { "Mid-High Crossover Freq", 2000 },
            { "Waveshape Low Band", 1 }, { "Depth Low Band", 85 }, { "Skew Low Band", 25 },
            { "Bypassed Mid Band", 1 }, { "Bypassed High Band", 1 } } },

        // 11 Only the middle band chops, where vocals and guitars sit
        { "Midrange Chop", {
            { "Low-Mid Crossover Freq", 400 }, { "Mid-High Crossover Freq", 2500 },
            { "Bypassed Low Band", 1 }, { "Bypassed High Band", 1 },
            { "Waveshape Mid Band", 2 }, { "Multiplier Mid Band", 3 }, { "Depth Mid Band", 70 } } },

        // ---- Unusual ------------------------------------------------------------------------

        // 12 The mid band's LFO runs upside down against the lows: a phaser-like sweep
        { "Phaser Wobble", {
            { "Depth Low Band", 60 },
            { "Depth Mid Band", 60 }, { "Invert Mid Band", 1 },
            { "Multiplier High Band", 2 }, { "Depth High Band", 50 }, { "Relative Phase High Band", 90 } } },

        // 13 Fast triplets in the mids and highs over a slow half-note low end
        { "Triplets on Halves", {
            { "Multiplier Low Band", 0 }, { "Depth Low Band", 70 },
            { "Waveshape Mid Band", 3 }, { "Multiplier Mid Band", 4 }, { "Depth Mid Band", 65 },
            { "Waveshape High Band", 2 }, { "Multiplier High Band", 4 }, { "Depth High Band", 55 }, { "Relative Phase High Band", 60 } } },

        // 14 Ramps and squares pushed to the ends of the skew range
        { "Skewed Gates", {
            { "Waveshape Low Band", 0 }, { "Multiplier Low Band", 3 }, { "Depth Low Band", 90 }, { "Skew Low Band", 12.5f },
            { "Waveshape Mid Band", 2 }, { "Multiplier Mid Band", 5 }, { "Depth Mid Band", 80 }, { "Skew Mid Band", 87.5f },
            { "Waveshape High Band", 1 }, { "Multiplier High Band", 2 }, { "Depth High Band", 70 }, { "Skew High Band", 85 } } },
    };

    return presets;
}
