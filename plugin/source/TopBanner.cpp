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
    mUpdateLink.setBounds(4, 28, getWidth() / 3, 20);   // under the preset bar
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
    // [Claude 2026-10-29] The header from the house-style work (Joe, header v4): a top-lit grey bar, the name and its
    // line at the left, the preset in the middle (PresetBar, placed by the editor), the logo and a menu at the right,
    // and in the demo a DEMO tag and an Activate key before the logo. Replaces the purple glow and the title image.
    auto bounds = getLocalBounds().toFloat();

    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1f2026), 0.f, 0.f,
                                           juce::Colour(0xff15161a), 0.f, bounds.getBottom(), false));
    g.fillRect(bounds);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawHorizontalLine(0, 0.f, bounds.getRight());
    // [Claude 2026-10-29] a soft seam under the header, like the bevels below it, not a hard black line
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawHorizontalLine((int) bounds.getBottom() - 1, 0.f, bounds.getRight());
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.drawHorizontalLine((int) bounds.getBottom() - 2, 0.f, bounds.getRight());

    const float cy = bounds.getCentreY();

    // Name and line
    auto nameFont = juce::Font(17.f, juce::Font::bold);
    nameFont.setExtraKerningFactor(0.22f);
    g.setFont(nameFont);
    g.setColour(juce::Colour(0xffe9e9ec));
    const juce::String name("TERTIARY");
    const float nameW = (float) nameFont.getStringWidth(name);
    g.drawText(name, juce::Rectangle<float>(14.f, cy - 10.f, nameW + 8.f, 20.f), juce::Justification::centredLeft, false);
    g.setFont(juce::Font(12.5f));
    g.setColour(juce::Colour(0xff8b8e97));
    g.drawText("Multiband Tremolo", juce::Rectangle<float>(14.f + nameW + 12.f, cy - 9.f, 130.f, 18.f), juce::Justification::centredLeft, false);

    // Right side, right to left: menu, logo, then Activate and DEMO in the demo
    float x = bounds.getRight() - 14.f;
    mMenuArea = { x - 18.f, cy - 9.f, 18.f, 18.f };
    g.setColour(juce::Colour(0xffa9abb3));
    for (int i = -1; i <= 1; ++i)
        g.fillRect(juce::Rectangle<float>(x - 16.f, cy + i * 5.f - 0.75f, 16.f, 1.5f));
    const float gap = 16.f;   // [Claude 2026-10-29] one even gap between everything on the right (Joe: "space these out evenly")
    x -= 16.f + gap;

    if (! imageCompanyTitle.isNull())
    {
        const float logoH = 16.f, logoW = logoH * companyTitleOriginalAspect;  // [Claude 2026-10-29] a step bigger (Joe), the most the demo header has room for (was 14)
        // [Claude 2026-10-29] The logo is a 4000 x 500 picture; drawing it straight down to ~128 px aliased badly
        // (Joe: "The logo is super pixelated"). It is resampled once, properly, to the exact device pixels, and cached.
        const float scale = (float) g.getInternalContext().getPhysicalPixelScaleFactor();
        const int pw = juce::roundToInt(logoW * scale), ph = juce::roundToInt(logoH * scale);
        if (scaledCompanyTitle.isNull() || scaledCompanyTitle.getWidth() != pw || scaledCompanyTitle.getHeight() != ph)
            scaledCompanyTitle = applyResize(imageCompanyTitle, pw, ph);
        // drawn one picture pixel to one screen pixel, on whole screen pixels: a fractional position smeared it at
        // 100 / 125 / 150 % (Joe: "125 and 100 look a little teensy bit blurry"). The banner sits at the window's origin.
        const float left = std::round((x - logoW) * scale) / scale, top = std::round((cy - logoH / 2.f) * scale) / scale;
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.drawImage(scaledCompanyTitle, juce::Rectangle<float>(left, top, pw / scale, ph / scale));
        x -= logoW + gap;
    }

    mActivateArea = {};
    if (mIsDemoVersion)
    {
        g.setFont(juce::Font(12.f));
        const float actW = 62.f;
        mActivateArea = { x - actW, cy - 11.f, actW, 22.f };
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff5bbd7d), 0.f, mActivateArea.getY(),
                                               juce::Colour(0xff3f9a5f), 0.f, mActivateArea.getBottom(), false));
        g.fillRoundedRectangle(mActivateArea, 2.f);
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.drawHorizontalLine((int) mActivateArea.getY() + 1, mActivateArea.getX() + 2.f, mActivateArea.getRight() - 2.f);
        g.setColour(juce::Colour(0xff0f1a12));
        g.drawText("Activate", mActivateArea, juce::Justification::centred, false);
        x -= actW + gap;

        auto tagFont = juce::Font(10.f, juce::Font::bold);
        tagFont.setExtraKerningFactor(0.15f);
        g.setFont(tagFont);
        const juce::String tagText = juce::String("DEMO");  // [Claude 2026-10-29] one size always; while bypassed it fills instead (Joe: the BYPASSED text changed its size)
        const float tagW = 46.f;
        juce::Rectangle<float> tag { x - tagW, cy - 9.f, tagW, 18.f };
        g.setColour(juce::Colour(0xffc886dd));
        if (mDemoBypassed)
        {
            g.setColour(juce::Colour(0xffc886dd).withAlpha(0.28f));
            g.fillRoundedRectangle(tag, 2.f);
            g.setColour(juce::Colour(0xffc886dd));
        }
        g.drawRoundedRectangle(tag.reduced(0.5f), 2.f, 1.f);
        g.drawText(tagText, tag, juce::Justification::centred, false);
    }
}

void TopBanner::mouseUp(const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (mActivateArea.contains(p) && onActivate) onActivate();
    else if (mMenuArea.expanded(4.f).contains(p) && onMenu) onMenu();
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
