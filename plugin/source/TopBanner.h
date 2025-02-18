/*
  ==============================================================================
    TopBanner.h
    Created: 29 Jan 2023 1:32:49pm
    Author:  Joe Caulfield
  ==============================================================================
*/
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "AllColors.h"
#include <BinaryData.h>
#include "UtilityFunctions.h"

struct TopBanner : juce::Component
{
public:
    TopBanner();
    ~TopBanner();
    
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Original source images
    juce::Image imageTopBanner;
    juce::Image imageCompanyTitle;
    juce::Image imagePluginTitle;

    // Cached scaled versions
    juce::Image scaledCompanyTitle;
    juce::Image scaledPluginTitle;
    
    // DPI scale tracking
    float lastCompanyTitleScale = 0.0f;
    float lastPluginTitleScale = 0.0f;
    
    // Cached aspect ratios
    float companyTitleOriginalAspect = 1.0f;
    float pluginTitleOriginalAspect = 1.0f;

    // Your existing dimensions
    float targetTitleWidth{ 750.f };
    float targetTitleHeight{ 50.f };
};