/*
  ==============================================================================

    OptionsMenu.cpp
    Created: 24 Nov 2023 10:13:06am
    Author:  Joe Caulfield

  ==============================================================================
*/

#include "OptionsMenu.h"
#include "UtilityFunctions.h"   // [Claude 2026-10-29] PixelGrid
#include <cstdlib>


// ========================================================
OptionsMenu::OptionsMenu()
{

    setSize(100,25);
    
    addMouseListener(this, true);
    
    mButtonOptions.setLookAndFeel(&optionsLookAndFeel);
    mButtonOptions.addListener(this);

    addAndMakeVisible(mButtonOptions);

    shouldShowDropdown = false;
    
    startTimerHz(30);

}


// ========================================================
OptionsMenu::~OptionsMenu()
{
    mButtonOptions.setLookAndFeel(nullptr);
    for (auto* option : optionsArray)
        option->toggleButton.setLookAndFeel(nullptr);
}


// ========================================================
void OptionsMenu::resized()
{

    mButtonOptions.setTopLeftPosition(0,0);
    mButtonOptions.setSize(66,17);  // [Claude 2026-10-29] a small, sharp key (Joe: "a little sharper"); was 100 x 25
    
    buildDropdown();
}


// ========================================================
void OptionsMenu::paint(juce::Graphics& g)
{

    // [Claude 2026-10-29] The open menu is a panel in the header's grey under the key; closed, only the key shows
    auto bounds = getLocalBounds().toFloat();
    if (shouldShowDropdown)
    {
        const PixelGrid grid(g, this);   // [Claude 2026-10-29] the panel's edge and highlight on whole screen pixels
        auto panel = grid.strokeRect(bounds.withTrimmedTop((float) mButtonOptions.getHeight() + 2.f));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff26282f), panel.getX(), panel.getY(),
                                               juce::Colour(0xff1d1f25), panel.getX(), panel.getBottom(), false));
        g.fillRoundedRectangle(panel, 2.f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        fillHLine(g, grid, panel.getY() + grid.rulePx() / 2.f, panel.getX() + 2.f, panel.getRight() - 2.f);
        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.drawRoundedRectangle(panel, 2.f, grid.rulePx());
    }

    setAlpha(fadeValue);
  
}

// ========================================================
void OptionsMenu::mouseEnter(const juce::MouseEvent& event)
{

    if (!isMouseButtonDown())
    {
        if (shouldShowDropdown == false)
            mHasFocus = true;
    }

}

// ========================================================
void OptionsMenu::mouseExit(const juce::MouseEvent& event)
{

    if (!isMouseOver(true)) {
        closeDropdown();
        mHasFocus = false;
    }

}

// ========================================================
void OptionsMenu::timerCallback()
{
    // If this cursor has focus, timerCounter increases until at its max.  
    // If no focus, timerCounter decreases until at its minimum.

    if ((timerCounter > timerCounterMin) || (timerCounter < timerCounterMax))
    {
        if (mHasFocus || mForceFocus)
        {
            if (timerCounter < timerCounterMax)
                timerCounter++;
        }
        else
        {
            if (timerCounter > timerCounterMin)
                timerCounter--;
        }

        fadeValue = juce::jmap((float)timerCounter, (float)timerCounterMin, (float)timerCounterMax, fadeValueMin, fadeValueMax);
        setAlpha(fadeValue);
    }

    // Review renders only (WLA_FOCUS contains OPT): show the crossover's menu open, for a still
    if (const char* focus = std::getenv("WLA_FOCUS"))
        if (juce::String(focus).contains("OPT") && getName() == "FREQ OPTIONS" && ! shouldShowDropdown && optionsArray.size() > 0)
        {
            mForceFocus = true;
            shouldShowDropdown = true;
            openDropdown();
        }
}

// ========================================================
void OptionsMenu::buttonClicked(juce::Button* button)
{

    if (button == &mButtonOptions)
    {
        shouldShowDropdown = !shouldShowDropdown;
        
        if (shouldShowDropdown)
            openDropdown();
        else
            closeDropdown();
    }


}

// ========================================================
void OptionsMenu::openDropdown()
{

    // Allocate 25 Pixels per populated option
    auto dropSize = optionsArray.size() * 18 + 8;

    setSize (   juce::jmax(mButtonOptions.getWidth(), 104),
                mButtonOptions.getHeight() + 2 + dropSize
            );

    buildDropdown();
    
}

// ========================================================
void OptionsMenu::closeDropdown()
{

    setSize (   mButtonOptions.getWidth(),
                mButtonOptions.getHeight()
            );

    shouldShowDropdown = false;
}

// ========================================================
void OptionsMenu::addOptionToList(juce::String optionTitle,
                                  juce::String optionDescription,
                                  juce::AudioProcessorValueTreeState& apvts,
                                  juce::String parameterID
                                  )
{

    using namespace juce;
    
    auto newOption = std::make_unique<OptionItem>();
    
    newOption->optionTitle = optionTitle;
    newOption->optionDescription = optionDescription;
    
    newOption->toggleButton.setButtonText(optionTitle);
    newOption->toggleButton.setHelpText(optionDescription);
    
    newOption->toggleButton.setColour(    ToggleButton::ColourIds::tickDisabledColourId, juce::Colours::black);
    newOption->toggleButton.setColour(    ToggleButton::ColourIds::tickColourId, juce::Colours::black);
    newOption->toggleButton.setColour(    ToggleButton::ColourIds::textColourId, juce::Colours::black);
        
    newOption->toggleButton.setLookAndFeel(&optionsLookAndFeel);
    newOption->toggleButton.addListener(this);
    
    newOption->optionAttachment =  std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts,
                                                                                                          parameterID,
                                                                                                          newOption->toggleButton);
    
    optionsArray.add(std::move(newOption)); // Transfers ownership to the array
}

// ========================================================
void OptionsMenu::buildDropdown()
{

    using namespace juce;
    
    auto bounds = getLocalBounds().reduced(3, 0);
    bounds.setTop(bounds.getY() + mButtonOptions.getHeight() + 2);

    auto margin = FlexItem().withHeight(3);
    auto spacer = FlexItem().withHeight(0);
    auto height = 18.f;

    FlexBox flexBox;
    flexBox.flexDirection = FlexBox::Direction::column;
    flexBox.flexWrap = FlexBox::Wrap::noWrap;
    
    flexBox.items.add(margin);
    
    for (const auto& option : optionsArray)
    {
        flexBox.items.add( FlexItem(option->toggleButton).withHeight(height));
        flexBox.items.add(spacer);
        addAndMakeVisible(option->toggleButton);
    }
    
    flexBox.items.add(margin);

    flexBox.performLayout(bounds);

}