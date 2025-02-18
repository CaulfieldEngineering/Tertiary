/*
  ==============================================================================

    ActivatorWindow.cpp
    Created: 17 Jan 2023 6:19:28pm
    Author:  Joe Caulfield

  ==============================================================================
*/


#include "AboutWindow.h"





AboutWindow::AboutWindow()
{
    setAlpha(0.95f);


    mButtonClose.addListener(this);
    addAndMakeVisible(mButtonClose);

    addMouseListener(this, true);

    mButtonClose.setColour(juce::TextButton::ColourIds::buttonColourId, juce::Colours::black.withAlpha(0.f));
    mButtonClose.setButtonText("X");

    imagePluginTitle = juce::ImageCache::getFromMemory(BinaryData::PluginTitle_png, BinaryData::PluginTitle_pngSize);
    imageCompanyTitle = juce::ImageCache::getFromMemory(BinaryData::CompanyTitle_png, BinaryData::CompanyTitle_pngSize);

    mButtonWebsiteLink.setButtonText("WonderlandAudio.com");
    mButtonWebsiteLink.setURL(juce::URL("http://www.wonderlandaudio.com"));
    mButtonWebsiteLink.setColour(juce::HyperlinkButton::textColourId, juce::Colours::oldlace);
    addAndMakeVisible(mButtonWebsiteLink);

}

AboutWindow::~AboutWindow()
{

}

void AboutWindow::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    auto bounds = getLocalBounds();
    float scale = getWindowsDPIScale(this);
    
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    // Draw Company Title (Wonderland Audio logo)
	// ==========================================================
    float companyTitleWidth = bounds.getWidth() * 0.75f;
    float aspectRatio = static_cast<float>(targetCompanyHeight) / static_cast<float>(targetCompanyWidth);
    float companyTitleHeight = companyTitleWidth * aspectRatio;
    
    if (std::abs(lastCompanyTitleScale - scale) > 0.01f || scaledCompanyTitle.isNull())
    {
        scaledCompanyTitle = applyResize(imageCompanyTitle, 
                                       juce::roundToInt(companyTitleWidth * scale), 
                                       juce::roundToInt(companyTitleHeight * scale));
        lastCompanyTitleScale = scale;
    }

    juce::Rectangle<float> companyBounds{
        bounds.getCentreX() - companyTitleWidth / 2,
        25,
        companyTitleWidth,
        companyTitleHeight
    };
    
    if (!scaledCompanyTitle.isNull())
    {
        g.drawImage(scaledCompanyTitle, companyBounds);
    }

    // Draw Plugin Title (TERTIARY)
	// ==========================================================
    juce::Rectangle<float> titleBounds{
        bounds.getCentreX() - targetTitleWidth / 2,
        companyBounds.getBottom() + 5,
        static_cast<float>(targetTitleWidth),
        static_cast<float>(targetTitleHeight)
    };

    if (std::abs(lastPluginTitleScale - scale) > 0.01f || scaledPluginTitle.isNull())
    {
        scaledPluginTitle = applyResize(imagePluginTitle, 
                                      juce::roundToInt(targetTitleWidth * scale), 
                                      juce::roundToInt(targetTitleHeight * scale));
        lastPluginTitleScale = scale;
    }

    if (!scaledPluginTitle.isNull())
    {
        g.drawImage(scaledPluginTitle, titleBounds);
    }

    // Set up text drawing with DPI scaling
    float fontSize = 20.0f;
    g.setFont(fontSize);

    // Draw Version and Release info
	// ==========================================================
    g.setColour(juce::Colours::white);
    #ifdef DEBUG
        g.setColour(juce::Colours::red);
    #endif

    juce::Rectangle<int> versionBounds = bounds.removeFromTop(static_cast<int>(titleBounds.getBottom()) + 60)
                                               .removeFromBottom(60);
    juce::String versionNumber = JucePlugin_VersionString;
    #ifdef DEMO_VERSION
        versionNumber += ".D";
    #endif
    
    juce::String releaseDate = formatBuildDateTime();
    g.drawFittedText("Version: " + versionNumber, versionBounds.removeFromTop(30), juce::Justification::centred, 1);
    g.drawFittedText("Release: " + releaseDate, versionBounds, juce::Justification::centred, 1);

    // Draw Demo Version Disclaimer
	// ==========================================================
    #ifdef DEMO_VERSION
        g.setFont(fontSize * 0.8f);  // Scale this font too
        g.setColour(juce::Colours::lightgrey);
        juce::Rectangle<int> disclaimerBounds = bounds.removeFromTop(60);
        disclaimerBounds = disclaimerBounds.reduced(5, 0);
        g.drawFittedText("Demo Version: Effect is disabled once every\n50 seconds, for 10 seconds.",
                         disclaimerBounds, juce::Justification::centred, 2);
    #endif

    // Draw Window Border
	// ==========================================================
    paintBorder(g, juce::Colours::grey, getLocalBounds().toFloat());
}

void AboutWindow::resized()
{
    auto bounds = getLocalBounds();

    mButtonClose.setBounds(bounds.getRight() - 30, 5, 25, 25);

    mButtonWebsiteLink.setBounds(bounds.getX(), bounds.getBottom() - 50, bounds.getWidth(), 30);
}

juce::String AboutWindow::formatBuildDateTime()
{
    // Map month abbreviations to numbers
    std::map<juce::String, int> months = {
        {"Jan", 1}, {"Feb", 2}, {"Mar", 3}, {"Apr", 4},
        {"May", 5}, {"Jun", 6}, {"Jul", 7}, {"Aug", 8},
        {"Sep", 9}, {"Oct", 10}, {"Nov", 11}, {"Dec", 12}
    };

    // Extract components from __DATE__ and __TIME__
    juce::String date(__DATE__);  // "MMM DD YYYY"
    juce::String time(__TIME__);  // "HH:MM:SS"

    // Parse the date string
    juce::String monthAbbr = date.substring(0, 3);
    int day = date.substring(4, 6).getIntValue();
    int year = date.substring(7, 11).getIntValue();
    int month = months[monthAbbr];

    // Parse the time string
    int hour = time.substring(0, 2).getIntValue();
    int minute = time.substring(3, 5).getIntValue();

    // Format the date and time as "YYYY.MM.DD HH:MM"
    return juce::String::formatted("%04d.%02d.%02d %02d:%02d", year, month, day, hour, minute);
}

void AboutWindow::buttonClicked(juce::Button* button)
{
    if (button == &mButtonClose)
    {
        setVisible(false);
    }

}

void AboutWindow::mouseExit(const juce::MouseEvent& event)
{
    if (!isMouseOver())
    {
        if (!mButtonClose.isMouseOver() && !mButtonWebsiteLink.isMouseOver())
            setVisible(false);
    }

}

