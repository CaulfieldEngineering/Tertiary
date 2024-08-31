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
#include "avir.h"
#include "avir_float4_sse.h"

struct TopBanner :  juce::Component //, juce::MouseListener
{
public:
    TopBanner();
    ~TopBanner();
    
    void paint(juce::Graphics& g) override;
    void resized() override;

	juce::Image applyResize (const juce::Image& src, int width, int height);
	void paintRescaledImage(juce::Graphics& g, juce::Rectangle<int> src, juce::Rectangle<int> dest, juce::Image originalImgToDraw);

private:
    
    juce::Image imageTopBanner;
	juce::Image imageCompanyTitle;
	juce::Image imagePluginTitle;

    float targetTitleWidth{ 750.f };
    float targetTitleHeight{ 50.f };

};
