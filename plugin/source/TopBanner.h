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

struct TopBanner :  juce::Component //, juce::MouseListener
{
public:
    TopBanner();
    ~TopBanner();
    
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    
    juce::Image imageTopBanner;
	juce::Image imageCompanyTitle;
	juce::Image imagePluginTitle;

    float targetTitleWidth{ 750.f };
    float targetTitleHeight{ 50.f };

};
