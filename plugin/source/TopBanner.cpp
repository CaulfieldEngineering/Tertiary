/*
  ==============================================================================

    TopBanner.cpp
    Created: 29 Jan 2023 1:32:49pm
    Author:  Joe Caulfield

  ==============================================================================
*/

#include "TopBanner.h"


#if JUCE_INTEL
 #define USE_SSE 1
#endif

TopBanner::TopBanner()
{
    imageCompanyTitle = juce::ImageCache::getFromMemory(BinaryData::CompanyTitle_png, BinaryData::CompanyTitle_pngSize);
    imagePluginTitle = juce::ImageCache::getFromMemory(BinaryData::PluginTitle_png, BinaryData::PluginTitle_pngSize);
}

TopBanner::~TopBanner()
{
    
}

void TopBanner::resized()
{
    // Any layout code can go here, if needed
}

void TopBanner::paint(juce::Graphics& g)
{
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality); // Ensure high-quality resampling

    auto bounds = getLocalBounds().toFloat();
    auto boundsHeight = bounds.getHeight();

    // Draw the plugin title centered in the bounds
    if (!imagePluginTitle.isNull()) {
        float aspectRatio = imagePluginTitle.getWidth() / (float)imagePluginTitle.getHeight();
        float newWidth = boundsHeight * aspectRatio;

        auto pluginTitleBounds = juce::Rectangle<float>(
            bounds.getCentreX() - (newWidth / 2.0f),
            bounds.getY(),
            newWidth,
            boundsHeight
        );

        // Draw the image with the new scaled dimensions
        g.drawImage(imagePluginTitle, pluginTitleBounds,
                    juce::RectanglePlacement::centred);
    }
    
    // Draw the company title image with resizing
    if (!imageCompanyTitle.isNull())
    {
        float aspectRatio = imageCompanyTitle.getWidth() / (float)imageCompanyTitle.getHeight();
        float companyHeight = boundsHeight * 0.65f; // 75% of the bounds height
        int newWidth = juce::roundToInt(companyHeight * aspectRatio);
        int newHeight = juce::roundToInt(companyHeight);

        // Call applyResize directly to get the resized image
        juce::Image resizedImage = applyResize(imageCompanyTitle, newWidth, newHeight);

        if (!resizedImage.isNull())
        {
            // Calculate where to position the image
            auto xPos = juce::roundToInt(bounds.getRight() - newWidth);
            auto yPos = juce::roundToInt(bounds.getCentreY() - (newHeight / 2.0f));

            // Draw the resized image at the calculated position
            g.drawImageAt(resizedImage, xPos, yPos);
        }
        else
        {
            DBG("Resized image is invalid.");
        }
    }

	// Label Demo Version as needed
	#ifdef DEMO_VERSION
		auto demoBounds = getLocalBounds().withTrimmedRight(getWidth() * 0.60f);

		// Use a system font by name
		juce::Font systemFont ("Helvetica", 36.0f, juce::Font::bold); // Replace "Arial" with your desired font name

		g.setFont(systemFont);

		// Set the color to a dimmed grey
		g.setColour(juce::Colours::lightgrey.withAlpha(0.5f)); // Adjust the alpha for dimming

		g.drawFittedText("DEMO VERSION", demoBounds, juce::Justification::centredLeft, 1);
	#endif
}