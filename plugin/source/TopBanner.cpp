/*
  ==============================================================================
    TopBanner.cpp
    Created: 29 Jan 2023 1:32:49pm
    Author:  Joe Caulfield
  ==============================================================================
*/

#include "TopBanner.h"

//#if JUCE_INTEL
// #define USE_SSE 1
//#endif

TopBanner::TopBanner()
{
    // Load source images
    imageCompanyTitle = juce::ImageCache::getFromMemory(BinaryData::CompanyTitle_png, BinaryData::CompanyTitle_pngSize);
    imagePluginTitle = juce::ImageCache::getFromMemory(BinaryData::PluginTitle_png, BinaryData::PluginTitle_pngSize);
    
    // Store original aspect ratios
    companyTitleOriginalAspect = imageCompanyTitle.getWidth() / (float)imageCompanyTitle.getHeight();
    pluginTitleOriginalAspect = imagePluginTitle.getWidth() / (float)imagePluginTitle.getHeight();

    // [Claude 2026-10-27] Update link, hidden until the update check finds a newer version
    mUpdateLink.setURL(juce::URL("https://www.wonderlandaudio.com"));
    mUpdateLink.setFont(juce::Font(14.0f), false, juce::Justification::centredLeft);
    mUpdateLink.setColour(juce::HyperlinkButton::textColourId, juce::Colours::lightgrey.withAlpha(0.6f));
    addChildComponent(mUpdateLink);
}

TopBanner::~TopBanner()
{
}

void TopBanner::resized()
{
    // Clear cached images to force redraw at new size
    scaledCompanyTitle = juce::Image();
    scaledPluginTitle = juce::Image();

    // [Claude 2026-10-27] Left side, where "DEMO VERSION" is shown in the demo
    mUpdateLink.setBounds(25, getHeight() / 2 - 10, getWidth() / 3, 20);
}

// [Claude 2026-10-27] The update link. Only in the full version: the left side is taken by the demo text,
// and the server only reports versions for activated keys anyway.
void TopBanner::setUpdateVersion(const juce::String& version)
{
    mUpdateVersion = version;
    updateVersionLink();
}

void TopBanner::updateVersionLink()
{
    mUpdateLink.setButtonText("Version " + mUpdateVersion + " is available");
    mUpdateLink.setVisible(mUpdateVersion.isNotEmpty() && !mIsDemoVersion);
}

void TopBanner::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto boundsHeight = bounds.getHeight();
    
	if (mIsMouseOver)
	{
		juce::Graphics::ScopedSaveState state(g); // Optional safety
		g.reduceClipRegion(getLocalBounds());     // Ensure no overpaint
		g.setColour(juce::Colours::purple.withAlpha(0.2f));
		g.fillRect(getLocalBounds());             // Flat and predictable
	}


    // Use Windows DPI scale
    //float scale = getWindowsDPIScale(this);
	float scale = (float) getDesktopScaleFactor();

    DBG("Windows DPI Scale: " + juce::String(scale));
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    // Plugin Title
	// ==========================================================
    if (!imagePluginTitle.isNull())
    {
        float newHeight = boundsHeight;
        float newWidth = newHeight * (imagePluginTitle.getWidth() / (float)imagePluginTitle.getHeight());
        
        if (std::abs(lastPluginTitleScale - scale) > 0.01f || scaledPluginTitle.isNull())
        {
            scaledPluginTitle = applyResize(imagePluginTitle, 
                                          juce::roundToInt(newWidth * scale), 
                                          juce::roundToInt(newHeight * scale));
            lastPluginTitleScale = scale;
        }

        auto pluginTitleBounds = juce::Rectangle<float>(
            bounds.getCentreX() - (newWidth / 2.0f),
            bounds.getY(),
            newWidth,
            boundsHeight
        );

        g.drawImage(scaledPluginTitle,
                   pluginTitleBounds,
                   juce::RectanglePlacement::centred);
    }

    // Company Title
	// ==========================================================
    if (!imageCompanyTitle.isNull())
    {
        float companyHeight = boundsHeight * 0.55f;
        float companyWidth = companyHeight * (imageCompanyTitle.getWidth() / (float)imageCompanyTitle.getHeight());
        
        if (std::abs(lastCompanyTitleScale - scale) > 0.01f || scaledCompanyTitle.isNull())
        {
            scaledCompanyTitle = applyResize(imageCompanyTitle, 
                                           juce::roundToInt(companyWidth * scale), 
                                           juce::roundToInt(companyHeight * scale));
            lastCompanyTitleScale = scale;
        }

        g.drawImage(scaledCompanyTitle,
                   bounds.getRight() - companyWidth,
                   bounds.getCentreY() - (companyHeight / 2.0f),
                   companyWidth,
                   companyHeight,
                   0, 0,
                   scaledCompanyTitle.getWidth(),
                   scaledCompanyTitle.getHeight());
    }

	// Demo Declaration
	// ==========================================================
	if (mIsDemoVersion)
	{
		auto area = getLocalBounds().withTrimmedRight(getWidth() * 0.60f).withLeft(25);

		// "DEMO VERSION" - large text
		g.setFont(juce::Font("Helvetica", 30.0f * scale, juce::Font::bold));
		g.setColour(juce::Colours::lightgrey.withAlpha(0.5f));
		auto demoTextBounds = area.removeFromTop(35);
		g.drawText("DEMO VERSION", demoTextBounds, juce::Justification::left, false);

		// "DOUBLE-CLICK TO ACTIVATE" - smaller text below
		g.setFont(juce::Font("Helvetica", 16.0f * scale, juce::Font::bold));
		g.setColour(juce::Colours::lightgrey.withAlpha(0.4f));
		auto activateTextBounds = demoTextBounds.translated(0, 25); // vertical offset below first line
		g.drawText("DOUBLE-CLICK TO ACTIVATE", activateTextBounds, juce::Justification::left, false);
	}

}


void TopBanner::mouseEnter(const juce::MouseEvent&)
{
	mIsMouseOver = true;
  repaint();
}

void TopBanner::mouseExit(const juce::MouseEvent&)
{
	mIsMouseOver = false;
  repaint();
}
