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

void buildLabelFont(    juce::Label& label,
                        juce::Justification justification,
                        juce::String typeface,
                        float fontSize,
                        juce::Font::FontStyleFlags fontStyle,
                        juce::Colour color,
                        float alpha);


juce::Image applyResize(const juce::Image& src, int width, int height);

float getWindowsDPIScale(juce::Component* component);
