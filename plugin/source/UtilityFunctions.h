/*
  ==============================================================================

    UtilityFunctions.h
    Created: 16 Jan 2022 2:26:42pm
    Author:  Joe

  ==============================================================================
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "avir.h"

#if JUCE_WINDOWS
    #include "avir_float4_sse.h"
#endif

void paintBorder(juce::Graphics& g, juce::Colour baseColor, juce::Rectangle<float> bounds);

// [Claude 2026-10-29] The same stepped bevel, tightened: three 1 px steps instead of four 2 px ones (Joe: "keep the
// current approach but just tighten up the bevels")
void paintBorderTight(juce::Graphics& g, juce::Colour baseColor, juce::Rectangle<float> bounds);

// [Claude 2026-10-29] A section cut into the plateau: no frame of its own, only the lip of the cut (a shadow along the
// top and left, a faint light along the bottom and right) and a soft shade falling from the top edge (Joe: "This
// should be one monolithic plateaued frame")
void paintRecess(juce::Graphics& g, juce::Rectangle<float> bounds);

void buildLabelFont(    juce::Label& label,
                        juce::Justification justification,
                        juce::String typeface,
                        float fontSize,
                        juce::Font::FontStyleFlags fontStyle,
                        juce::Colour color,
                        float alpha);


juce::Image applyResize(const juce::Image& src, int width, int height);

float getWindowsDPIScale(juce::Component* component);
