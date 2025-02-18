/*
  ==============================================================================

    ActivatorWindow.h
    Created: 17 Jan 2023 6:19:28pm
    Author:  Joe Caulfield

  ==============================================================================
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <BinaryData.h>
#include "UtilityFunctions.h"

struct AboutWindow : juce::Component, juce::Button::Listener
{
public:
    AboutWindow();
    ~AboutWindow();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:

    juce::String formatBuildDateTime();
  

    juce::TextButton mButtonClose;

    juce::Image imagePluginTitle;
    juce::Image imageCompanyTitle;

    float targetTitleWidth{ 350.f };
    float targetTitleHeight{ 60.f };

    float targetCompanyWidth{ 400.f };
    float targetCompanyHeight{ 50.f };

	// Cached scaled images
    juce::Image scaledCompanyTitle;
    juce::Image scaledPluginTitle;
    
    // Scale tracking
    float lastPluginTitleScale = 0.0f;
    float lastCompanyTitleScale = 0.0f;
    
    // Cached aspect ratios
    float pluginTitleOriginalAspect = 1.0f;
    float companyTitleOriginalAspect = 1.0f;

    juce::HyperlinkButton mButtonWebsiteLink;

};
