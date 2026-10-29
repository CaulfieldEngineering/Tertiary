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

	/* Tells this class whether or not the app is in Demo Version */
	void setDemoState(bool demoState) {mIsDemoVersion = demoState; updateVersionLink();}

	/* [Claude 2026-10-29] While the demo's bypass runs, the DEMO tag says so */
	void setDemoBypassed(bool b) { if (b != mDemoBypassed) { mDemoBypassed = b; repaint(); } }

	/* [Claude 2026-10-27] Shows a quiet "Version X is available" link (full version only). Empty hides it. */
	void setUpdateVersion(const juce::String& version);

    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    // [Claude 2026-10-29] The new header (Joe's header v4): Activate (demo) and the menu are click targets
    void mouseUp(const juce::MouseEvent&) override;
    std::function<void()> onActivate, onMenu;

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

	bool mIsDemoVersion {true};

	bool mIsMouseOver { false };
	bool mDemoBypassed { false };

	juce::Rectangle<float> mActivateArea, mMenuArea;

	// [Claude 2026-10-27] "Version X is available" link to the website
	juce::HyperlinkButton mUpdateLink;
	juce::String mUpdateVersion;
	void updateVersionLink();
};