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

void paintBorder(juce::Graphics& g, juce::Colour baseColor, juce::Rectangle<float> bounds, const juce::Component* owner = nullptr);

// [Claude 2026-10-29] A rectangle in `owner`'s coordinates moved to the nearest edges of real screen pixels. The window
// scales the whole layout (125 %, 150 % ...), so whole layout pixels are not whole screen pixels; frames snapped only to
// layout pixels showed seams again at 125 % (Joe). Returns the rectangle unchanged without an owner.
juce::Rectangle<float> snapToScreenPixels(juce::Graphics& g, juce::Rectangle<float> r, const juce::Component* owner);

void buildLabelFont(    juce::Label& label,
                        juce::Justification justification,
                        juce::String typeface,
                        float fontSize,
                        juce::Font::FontStyleFlags fontStyle,
                        juce::Colour color,
                        float alpha);


juce::Image applyResize(const juce::Image& src, int width, int height);

float getWindowsDPIScale(juce::Component* component);
