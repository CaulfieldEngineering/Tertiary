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

  ==============================================================================
*/

#include "PresetManager.h"

const std::vector<PresetManager::FactoryPreset>& PresetManager::getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets =
    {
        // The plugin's defaults
        { "Default", {} },

        // Barely there: slow, shallow, each band a little later than the one below
        { "Gentle Movement", {
            { "Depth Low Band", 25 },  { "Depth Mid Band", 20 },  { "Depth High Band", 15 },
            { "Multiplier Low Band", 0 }, { "Multiplier Mid Band", 0 }, { "Multiplier High Band", 0 },
            { "Relative Phase Mid Band", 60 }, { "Relative Phase High Band", 120 } } },

        // Classic amp tremolo: one free-running rate, the same on every band
        { "Amp Tremolo", {
            { "Sync to Host Low Band", 0 }, { "Sync to Host Mid Band", 0 }, { "Sync to Host High Band", 0 },
            { "Rate Low Band", 5.5f }, { "Rate Mid Band", 5.5f }, { "Rate High Band", 5.5f },
            { "Depth Low Band", 50 }, { "Depth Mid Band", 50 }, { "Depth High Band", 50 } } },

        // Very slow rise and fall, rolling through the bands
        { "Slow Swell", {
            { "Sync to Host Low Band", 0 }, { "Sync to Host Mid Band", 0 }, { "Sync to Host High Band", 0 },
            { "Rate Low Band", 0.5f }, { "Rate Mid Band", 0.5f }, { "Rate High Band", 0.5f },
            { "Waveshape Low Band", 5 }, { "Waveshape Mid Band", 5 }, { "Waveshape High Band", 5 },
            { "Depth Low Band", 70 }, { "Depth Mid Band", 70 }, { "Depth High Band", 70 },
            { "Relative Phase Mid Band", 120 }, { "Relative Phase High Band", -120 } } },

        // The low end ducks on every beat, like a sidechain pump; mids and highs untouched.
        // Crossover at 250 Hz so the pulse includes the bass's body and is heard on small speakers too.
        { "Low End Pump", {
            { "Low-Mid Crossover Freq", 250 }, { "Mid-High Crossover Freq", 2000 },
            { "Waveshape Low Band", 1 }, { "Depth Low Band", 100 }, { "Skew Low Band", 25 },
            { "Depth Mid Band", 0 }, { "Depth High Band", 0 } } },

        // Fast, light flicker on the top end only
        { "High Shimmer", {
            { "Low-Mid Crossover Freq", 300 }, { "Mid-High Crossover Freq", 3000 },
            { "Depth Low Band", 0 }, { "Depth Mid Band", 0 },
            { "Waveshape High Band", 3 }, { "Multiplier High Band", 5 }, { "Depth High Band", 60 } } },

        // Hard on/off eighth notes on everything
        { "Hard Chop", {
            { "Waveshape Low Band", 2 }, { "Waveshape Mid Band", 2 }, { "Waveshape High Band", 2 },
            { "Multiplier Low Band", 3 }, { "Multiplier Mid Band", 3 }, { "Multiplier High Band", 3 },
            { "Depth Low Band", 100 }, { "Depth Mid Band", 100 }, { "Depth High Band", 100 } } },

        // The same chop, with the middle band on the off-beats
        { "Alternating Chop", {
            { "Waveshape Low Band", 2 }, { "Waveshape Mid Band", 2 }, { "Waveshape High Band", 2 },
            { "Multiplier Low Band", 3 }, { "Multiplier Mid Band", 3 }, { "Multiplier High Band", 3 },
            { "Depth Low Band", 100 }, { "Depth Mid Band", 100 }, { "Depth High Band", 100 },
            { "Invert Mid Band", 1 } } },

        // Short gated sixteenths
        { "Sixteenth Gate", {
            { "Waveshape Low Band", 0 }, { "Waveshape Mid Band", 0 }, { "Waveshape High Band", 0 },
            { "Multiplier Low Band", 5 }, { "Multiplier Mid Band", 5 }, { "Multiplier High Band", 5 },
            { "Depth Low Band", 90 }, { "Depth Mid Band", 90 }, { "Depth High Band", 90 } } },

        // A different rhythm in each band: quarters, quarter triplets, eighths
        { "Three Rhythms", {
            { "Multiplier Low Band", 1 }, { "Multiplier Mid Band", 2 }, { "Multiplier High Band", 3 },
            { "Waveshape Low Band", 3 }, { "Waveshape Mid Band", 3 }, { "Waveshape High Band", 3 },
            { "Depth Low Band", 80 }, { "Depth Mid Band", 80 }, { "Depth High Band", 80 } } },

        // Eighth-note triplets that roll from the low band up to the highs
        { "Rolling Triplets", {
            { "Multiplier Low Band", 4 }, { "Multiplier Mid Band", 4 }, { "Multiplier High Band", 4 },
            { "Relative Phase Mid Band", 120 }, { "Relative Phase High Band", -120 },
            { "Depth Low Band", 85 }, { "Depth Mid Band", 85 }, { "Depth High Band", 85 } } },

        // Half-note sway; the lows and highs move against each other
        { "Half-Time Sway", {
            { "Multiplier Low Band", 0 }, { "Multiplier Mid Band", 0 }, { "Multiplier High Band", 0 },
            { "Depth Low Band", 95 }, { "Depth Mid Band", 60 }, { "Depth High Band", 80 },
            { "Invert High Band", 1 } } },

        // Free-running flutter at three slightly different speeds
        { "Free Flutter", {
            { "Sync to Host Low Band", 0 }, { "Sync to Host Mid Band", 0 }, { "Sync to Host High Band", 0 },
            { "Rate Low Band", 4.5f }, { "Rate Mid Band", 6.0f }, { "Rate High Band", 7.5f },
            { "Waveshape Low Band", 3 }, { "Waveshape Mid Band", 3 }, { "Waveshape High Band", 3 },
            { "Depth Low Band", 50 }, { "Depth Mid Band", 50 }, { "Depth High Band", 50 } } },

        // Chop only the middle band, where vocals and guitars sit
        { "Midrange Chop", {
            { "Low-Mid Crossover Freq", 400 }, { "Mid-High Crossover Freq", 2500 },
            { "Depth Low Band", 0 }, { "Depth High Band", 0 },
            { "Waveshape Mid Band", 2 }, { "Multiplier Mid Band", 3 }, { "Depth Mid Band", 70 } } },
    };

    return presets;
}
