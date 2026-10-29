/*
  ==============================================================================

    Cursor.cpp
    Created: 21 Apr 2024 8:58:30pm
    Author:  Joe

  ==============================================================================
*/

#include "Cursor.h"
#include "AllColors.h"

using namespace juce;

Cursor::Cursor()
{

    setSize(5, 25);
    startTimerHz(30);
}

Cursor::~Cursor()
{
}

void Cursor::resized()
{

    auto bounds = getLocalBounds();

    // Establish Horizontal Cursor Bounds
    if (mIsVertical)
    {
        auto x = bounds.getCentreX();

        auto startY = bounds.getY();
        auto endY = bounds.getBottom();

        mCursor.setStart(x, startY);
        mCursor.setEnd(x, endY);
    }
    else
    {
        auto y = bounds.getCentreY();

        auto startX = bounds.getX();
        auto endX = bounds.getRight();

        mCursor.setStart(startX, y);
        mCursor.setEnd(endX, y);
    }

}

void Cursor::timerCallback()
{
    // If this cursor has focus, timerCounter increases until at its max.  
    // If no focus, timerCounter decreases until at its minimum.

    if ((timerCounter > timerCounterMin) || (timerCounter < timerCounterMax))
    {
        if (mHasFocus || mForceFocus)
        {
            if (timerCounter < timerCounterMax)
            {
                timerCounter++;
                repaint();
                if (auto* p = getParentComponent()) p->repaint();  // the gain tag is drawn by the parent
            }
        }
        else
        {
            if (timerCounter > timerCounterMin)
            {
                timerCounter--;
                repaint();
                if (auto* p = getParentComponent()) p->repaint();
            }  
        }
    }

    
}

void Cursor::paint(juce::Graphics& g)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    // [Claude 2026-10-29] Handles in place of the fading lines (Joe: "can you reinvent those fade-in cursors on the
    // crossover?"). A gain cursor is a short pill in its band's colour on the band's flat top; a crossover cursor is
    // a ring where the two bands meet, with ticks at the top and bottom edges. Both are always faintly there and come
    // up on hover; the full line shows only while hovered.
    fadeValue = juce::jmap((float)timerCounter, (float)timerCounterMin, (float)timerCounterMax, fadeValueMin, fadeValueMax);
    const float t = juce::jmap(fadeValue, fadeValueMin, fadeValueMax, 0.f, 1.f);
    const auto bounds = getLocalBounds().toFloat();

    using namespace ColorScheme::BandColors;
    juce::Colour colour = juce::Colours::white;
    if (getName() == "LOW_GAIN_CURSOR")  colour = getLowBandBaseColor();
    if (getName() == "MID_GAIN_CURSOR")  colour = getMidBandBaseColor();
    if (getName() == "HIGH_GAIN_CURSOR") colour = getHighBandBaseColor();

    if (! mIsVertical)
    {
        const float y = bounds.getCentreY();
        // [Claude 2026-10-29] only the pill; the faint band-coloured line across the band is gone (Joe: "remove that
        // horizontal blue line ... Just need the thicker handle")
        const float w = juce::jmin(30.f, bounds.getWidth()), h = 5.f;
        juce::Rectangle<float> pill { bounds.getCentreX() - w / 2.f, y - h / 2.f, w, h };
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle(pill.translated(0.f, 1.f), h / 2.f);
        g.setColour(colour.brighter(0.25f).withAlpha(0.55f + 0.45f * t));
        g.fillRoundedRectangle(pill, h / 2.f);
        return;
    }

    // The crossover handle: an open ring where the two bands meet, on a faint full-height line that brightens on
    // hover (Joe: "i liked the open circles better, maybe an open circle with a more faint vertical line?").
    const float x = bounds.getCentreX();
    g.setColour(juce::Colours::white.withAlpha(0.16f + 0.18f * t));
    g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 1.75f);  // a little heavier than the 1 px grid (Joe)
    const float my = mMarkerY >= 0.f ? mMarkerY : bounds.getCentreY();
    const float r = 3.5f + 0.75f * t;
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(x - r - 1.f, my - r - 1.f, 2.f * r + 2.f, 2.f * r + 2.f);
    g.setColour(juce::Colours::white.withAlpha(0.6f + 0.4f * t));
    g.drawEllipse(x - r, my - r, 2.f * r, 2.f * r, 1.5f);
}

void Cursor::setHorizontalOrientation()
{

    mIsVertical = false;
}

void Cursor::setFocus(bool hasFocus)
{

    mHasFocus = hasFocus;

    juce::String bFocus;

    if (hasFocus)
        bFocus = "TRUE";
    else
        bFocus = "FALSE";

    sendBroadcast("cFOCUS", bFocus);

    repaint();
}


//
// ===========================================================================================
void Cursor::actionListenerCallback(const juce::String& message)
{

    auto paramName = message.replaceSection(0, 10, "");
    paramName = paramName.replaceSection(10, 25, "");
    paramName = paramName.removeCharacters("x");

    juce::String paramValue = message.replaceSection(0, 25, "");
    paramValue = paramValue.removeCharacters("x");

    if (paramName == "cFOCUS")
    {
        if (paramValue == "TRUE")
            mForceFocus = true;
        else
            mForceFocus = false;
    }
}

/* Send Broadcast Message */
// ===========================================================================================
void Cursor::sendBroadcast(juce::String parameterName, juce::String parameterValue)
{

    juce::String delimiter = ":::::";

    juce::String bandName = "xxxxx";

    auto message = bandName + delimiter + parameterName.paddedLeft('x', 10) + delimiter + parameterValue.paddedLeft('x', 10);

    sendActionMessage(message);
}