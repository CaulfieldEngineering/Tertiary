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

// [Claude 2026-10-29] A crisp, machined edge for a section: a dark outer line, lit along the top and left inside it and
// shaded along the bottom and right (Joe chose it over the stacked bevel: "Let's try 1")
void paintRidge(juce::Graphics& g, juce::Rectangle<float> bounds);

void buildLabelFont(    juce::Label& label,
                        juce::Justification justification,
                        juce::String typeface,
                        float fontSize,
                        juce::Font::FontStyleFlags fontStyle,
                        juce::Colour color,
                        float alpha);


juce::Image applyResize(const juce::Image& src, int width, int height);

float getWindowsDPIScale(juce::Component* component);
