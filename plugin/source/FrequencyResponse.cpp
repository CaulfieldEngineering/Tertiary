/*
  ==============================================================================
    FrequencyResponse.cpp
    Created: 30 Dec 2021 11:38:20am
    Author:  Joe Caulfield
    Class to display and control the frequency response of the crossover
  ==============================================================================
*/


#include "FrequencyResponse.h"
#include <cstdlib>

/* Constructor */
// ===========================================================================================
FrequencyResponse::FrequencyResponse(   audio_plugin::AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& apv)
    : audioProcessor(p), apvts(apv)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    buildLowMidFreqSlider();
    buildMidHighFreqSlider();
    buildLowGainSlider();
    buildMidGainSlider();
    buildHighGainSlider();
    buildLabels();
    
    makeAttachments();

    // Initialize Parameters

    using namespace Params;
    const auto& params = GetParams();

    // Update Band Mute States
    lowBandMute = *apvts.getRawParameterValue(params.at(Names::Mute_Low_Band));
    midBandMute = *apvts.getRawParameterValue(params.at(Names::Mute_Mid_Band));
    highBandMute = *apvts.getRawParameterValue(params.at(Names::Mute_High_Band));

    // Update Band Bypass States
    lowBandBypass = *apvts.getRawParameterValue(params.at(Names::Bypass_Low_Band));
    midBandBypass = *apvts.getRawParameterValue(params.at(Names::Bypass_Mid_Band));
    highBandBypass = *apvts.getRawParameterValue(params.at(Names::Bypass_High_Band));

    //Update Band Solo States
    lowBandSolo = *apvts.getRawParameterValue(params.at(Names::Solo_Low_Band));
    midBandSolo = *apvts.getRawParameterValue(params.at(Names::Solo_Mid_Band));
    highBandSolo = *apvts.getRawParameterValue(params.at(Names::Solo_High_Band));

    newFreqLabelLow.addActionListener(&newCursorLM);
    newFreqLabelHigh.addActionListener(&newCursorMH);

    newCursorLM.addActionListener(&newFreqLabelLow);
    newCursorMH.addActionListener(&newFreqLabelHigh);

    newCursorLM.setName("LOW_CURSOR");
    newCursorMH.setName("HIGH_CURSOR");

    newCursorLowGain.setName("LOW_GAIN_CURSOR");
    newCursorMidGain.setName("MID_GAIN_CURSOR");
    newCursorHighGain.setName("HIGH_GAIN_CURSOR");

    updateResponse();
}

/* Destructor */
// ===========================================================================================
FrequencyResponse::~FrequencyResponse()
{
}

/* Build Low-Mid Slider */
// ===========================================================================================
void FrequencyResponse::buildLowMidFreqSlider()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    // Linear Slider from 0 to 1
    sliderLowMidInterface.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    sliderLowMidInterface.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    sliderLowMidInterface.addListener(this);
    sliderLowMidInterface.setRange(0, 1.f, 0.001f);
    sliderLowMidInterface.setAlpha(0.f);
    addAndMakeVisible(sliderLowMidInterface);

    // Linear Slider from 20 to 20k
    sliderLowMidCutoff.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    sliderLowMidCutoff.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    sliderLowMidCutoff.addListener(this);

    addAndMakeVisible(newCursorLM);
    newCursorLM.addMouseListener(this, true);
    newCursorLM.toBehind(&sliderLowMidInterface);
}

/* Build Mid-High Slider */
// ===========================================================================================
void FrequencyResponse::buildMidHighFreqSlider()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    // Linear Slider from 0 to 1
    sliderMidHighInterface.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    sliderMidHighInterface.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    sliderMidHighInterface.addListener(this);
    sliderMidHighInterface.setRange(0, 1.f, 0.001f);
    sliderMidHighInterface.setAlpha(0.f);
    addAndMakeVisible(sliderMidHighInterface);

    // Linear Slider from 20 to 20k
    sliderMidHighCutoff.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    sliderMidHighCutoff.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    sliderMidHighCutoff.addListener(this);

    addAndMakeVisible(newCursorMH);
    newCursorMH.addMouseListener(this, true);
    newCursorMH.toBehind(&sliderMidHighInterface);
}

/* Build Low Gain Slider */
// ===========================================================================================
void FrequencyResponse::buildLowGainSlider()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    sliderLowGain.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    sliderLowGain.setSliderSnapsToMousePosition(false);   // [Claude 2026-10-29] drag from where you grab the handle; no jump
    sliderLowGain.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    sliderLowGain.addListener(this);
    sliderLowGain.setRange(0, 1.f, 0.001f);
    sliderLowGain.setAlpha(0.f);
    addAndMakeVisible(sliderLowGain);

    addAndMakeVisible(newCursorLowGain);
    newCursorLowGain.setHorizontalOrientation();
    newCursorLowGain.addMouseListener(this, true);
    newCursorLowGain.toBehind(&sliderLowGain);
}

/* Build Mid Gain Slider */
// ===========================================================================================
void FrequencyResponse::buildMidGainSlider()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    sliderMidGain.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    sliderMidGain.setSliderSnapsToMousePosition(false);   // [Claude 2026-10-29] drag from where you grab the handle; no jump
    sliderMidGain.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    sliderMidGain.addListener(this);
    sliderMidGain.setRange(0, 1.f, 0.001f);
    sliderMidGain.setAlpha(0.f);
    addAndMakeVisible(sliderMidGain);

    addAndMakeVisible(newCursorMidGain);
    newCursorMidGain.setHorizontalOrientation();
    newCursorMidGain.addMouseListener(this, true);
    newCursorMidGain.toBehind(&sliderMidGain);
}

/* Build High Gain Slider */
// ===========================================================================================
void FrequencyResponse::buildHighGainSlider()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    sliderHighGain.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    sliderHighGain.setSliderSnapsToMousePosition(false);   // [Claude 2026-10-29] drag from where you grab the handle; no jump
    sliderHighGain.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    sliderHighGain.addListener(this);
    sliderHighGain.setRange(0, 1.f, 0.001f);
    sliderHighGain.setAlpha(0.f);
    addAndMakeVisible(sliderHighGain);

    addAndMakeVisible(newCursorHighGain);
    newCursorHighGain.setHorizontalOrientation();
    newCursorHighGain.addMouseListener(this, true);
    newCursorHighGain.toBehind(&sliderHighGain);
}

/* Build Labels */
// ===========================================================================================
void FrequencyResponse::buildLabels()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    newFreqLabelLow.addActionListener(this);
    newFreqLabelHigh.addActionListener(this);
    newFreqLabelLow.setName("LOWFREQ");
    newFreqLabelHigh.setName("HIGHFREQ");
    addAndMakeVisible(newFreqLabelLow);
    addAndMakeVisible(newFreqLabelHigh);
}

/* Attach parameters to components */
// ===========================================================================================
void FrequencyResponse::makeAttachments()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    using namespace Params;
    const auto& params = GetParams();

    lowMidAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                        apvts,
                        params.at(Names::Low_Mid_Crossover_Freq),
                        sliderLowMidCutoff);

    midHighAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                        apvts,
                        params.at(Names::Mid_High_Crossover_Freq),
                        sliderMidHighCutoff);

    lowMidAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                        apvts,
                        params.at(Names::Low_Mid_Crossover_Freq),
                        sliderLowMidCutoff);

    lowGainAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                        apvts,
                        params.at(Names::Gain_Low_Band),
                        sliderLowGain);

    midGainAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                        apvts,
                        params.at(Names::Gain_Mid_Band),
                        sliderMidGain);

    highGainAttachment =std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                        apvts,
                        params.at(Names::Gain_High_Band),
                        sliderHighGain);
}

/* Called on component resize */
// ===========================================================================================
void FrequencyResponse::resized()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    // =============================
    responseArea = getLocalBounds().toFloat();
    responseArea.reduce(2, 2);
    responseArea.removeFromBottom(20);
    responseArea.removeFromLeft(4);  // [Claude 2026-10-29] the dB labels now sit inside the plot; was 32
    responseArea.removeFromRight(6);

    // =============================
    sliderLowMidCutoff.setBounds(responseArea.getX(), responseArea.getY()+80, 25, 25);
    sliderMidHighCutoff.setBounds(responseArea.getX(), responseArea.getY()+90, 50, 25);

    // =============================
    sliderLowMidInterface.setBounds(responseArea.getX() - 10, 25, responseArea.getWidth() + 20, 10);
    sliderMidHighInterface.setBounds(responseArea.getX() - 10, 50, responseArea.getWidth() + 20, 10);

    sliderMidGain.setBounds(responseArea.getX()+25, responseArea.getY(), 20, responseArea.getHeight());
    sliderHighGain.setBounds(responseArea.getX()+50, responseArea.getY(), 20, responseArea.getHeight());

    sliderLowGain.setBounds(responseArea.getX(), responseArea.getY(), 1, responseArea.getHeight());
    sliderMidGain.setBounds(responseArea.getX(), responseArea.getY(), 1, responseArea.getHeight());
    sliderHighGain.setBounds(responseArea.getX(), responseArea.getY(), 1, responseArea.getHeight());
}

/* Timer Callback */
// ===========================================================================================
//void FrequencyResponse::timerCallback()
//{
//    setLabelOpacity();
//}

/* Paint Graphics Class */
// ===========================================================================================
void FrequencyResponse::paint(juce::Graphics& g)
{
    using namespace juce;
    using namespace AllColors::FrequencyResponseColors;

    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    auto bounds = getLocalBounds().toFloat();

    g.setGradientFill(BACKGROUND_GRADIENT(bounds));
    g.fillAll();

    /* Paint grids */
    // =========================
    paintGridGain(g);       // These should be buffered to images?
    paintGridFrequency(g);  // These should be buffered to images?

    /* Paint Response Regions */
    // =========================
    paintResponseRegions(g);

    /* [Claude 2026-10-29] A gain tag over each band's handle while it is hovered or dragged, styled like the
       frequency labels */
    // =========================
    paintGainTag(g, newCursorLowGain, mLowGain);
    paintGainTag(g, newCursorMidGain, mMidGain);
    paintGainTag(g, newCursorHighGain, mHighGain);

    /* Draw the frequency labels */
    // =========================
    drawLabels();

    /* Paint Border */
    // =========================
    paintBorder(g, ColorScheme::WindowBorders::getWindowBorderColor(), bounds, this);
}

void FrequencyResponse::paintGainTag(juce::Graphics& g, Cursor& cursor, float gainDb)
{
    const float t = juce::jmap(cursor.getFadeValue(), cursor.getFadeValueMin(), cursor.getFadeValueMax(), 0.f, 1.f);
    if (t <= 0.01f || cursor.getWidth() == 0)
        return;
    juce::String text = (gainDb > 0.05f ? "+" : "") + juce::String(gainDb, 1) + " dB";
    const auto c = cursor.getBounds().toFloat().getCentre();
    // above the pill, or below it when there is no room above (Joe: "the gain labels get pushed off of the screen")
    juce::Rectangle<float> r { c.x - 26.f, c.y - 24.f, 52.f, 17.f };
    if (r.getY() < responseArea.getY() + 2.f)
        r.setY(c.y + 7.f);
    r.setX(juce::jlimit(responseArea.getX() + 2.f, responseArea.getRight() - r.getWidth() - 2.f, r.getX()));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff26282e).withMultipliedAlpha(t), r.getX(), r.getY(),
                                           juce::Colour(0xff17181c).withMultipliedAlpha(t), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 3.f);
    g.setColour(juce::Colours::white.withAlpha(0.14f * t));
    g.drawRoundedRectangle(r, 3.f, 1.f);
    g.setColour(juce::Colour(0xffd9dade).withAlpha(t));
    g.setFont(11.f);
    g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
}

/* Draw vertical grid lines (frequency) */
// ===========================================================================================
void FrequencyResponse::paintGridFrequency(juce::Graphics& g)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    using namespace juce;
    const auto textWidth = 30;
    const auto fontHeight = 12;

    // DRAW THE VERTICAL GRIDS ===============
    for (int i = 0; i < freqs.size()-1; i++)
    {
        float x = responseArea.getX() + responseArea.getWidth() * mapLog2(freqs[i]);

        g.setColour(Colours::lightgrey.withAlpha(0.12f));
        g.drawVerticalLine(x, responseArea.getY(), responseArea.getBottom());

        String strFreq;

        if (freqs[i] > 999.f)
        {
            strFreq << (freqs[i] / 1000.f) << "k";
        }
        else
            strFreq << freqs[i];
        
        g.setFont(fontHeight - 1);
        g.setColour(Colours::lightgrey.withAlpha(0.6f));

        float xC = x - textWidth / 2;
        // the first label sits inside the plot now that the dB scale has moved to the right
        if (i == 0)
            g.drawFittedText(strFreq, x + 2, getHeight()-fontHeight-7, textWidth, fontHeight, Justification::centredLeft, 1, 1);
        else
            g.drawFittedText(strFreq, xC, getHeight()-fontHeight-7, textWidth, fontHeight, Justification::centred, 1, 1);
    }
}

/* Draw horizontal grid lines (gain) */
// ===========================================================================================
void FrequencyResponse::paintGridGain(juce::Graphics& g)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    using namespace juce;
    const auto textWidth = 26;
    const auto fontHeight = 13;

    const auto gMin = gain.getFirst();
    const auto gMax = gain.getLast();

    // DRAW THE HORIZONTAL GRIDS ===============
    for (int i = 1; i < gain.size()-1; i++)
    {
        auto y = jmap(gain[i], gMin, gMax, float(responseArea.getBottom()), responseArea.getY());  // [Claude 2026-10-29] no second getY(), as for the gain cursors

        g.setColour(juce::Colours::lightgrey.withAlpha(0.12f));
        g.drawHorizontalLine(y, responseArea.getX(), responseArea.getRight());

        if (int(gain[i]) % 12 != 0)
            continue;

        String strGain;

        if (gain[i] > 0)
            strGain << "+";

        strGain << gain[i];

        g.setFont(fontHeight - 2);
        g.setColour(Colours::lightgrey.withAlpha(0.5f));
        g.drawFittedText(strGain, responseArea.getRight() - textWidth - 3, y - fontHeight, textWidth, fontHeight - 1, juce::Justification::centredRight, 1, 1);

    }
}

/* Parent function to paint the frequency response trapezoids */
// ===========================================================================================
void FrequencyResponse::paintResponseRegions(juce::Graphics& g)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    /* If low-band is hovered, draw low-band on top with highlight */
    if (mLowFocus)
    {
        paintHighRegion( g,    responseArea, gainHighPixel,    freq2Pixel,                mHighFocus    ? focusAlpha : noFocusAlpha);
        paintMidRegion(  g,    responseArea, gainMidPixel,     freq1Pixel, freq2Pixel,    mMidFocus    ? focusAlpha : noFocusAlpha);
        paintLowRegion(  g,    responseArea, gainLowPixel,     freq1Pixel,                mLowFocus    ? focusAlpha : noFocusAlpha);
    }

    // If mid-band is hovered, draw mid-band on top with highlight.  Doubles as default view.
    if (mMidFocus || (!mLowFocus && !mMidFocus && !mHighFocus))
    {
        paintHighRegion( g,    responseArea, gainHighPixel,    freq2Pixel,                mHighFocus    ? focusAlpha : noFocusAlpha);
        paintLowRegion(  g,    responseArea, gainLowPixel,     freq1Pixel,                mLowFocus    ? focusAlpha : noFocusAlpha);
        paintMidRegion(  g,    responseArea, gainMidPixel,     freq1Pixel, freq2Pixel,    mMidFocus    ? focusAlpha : noFocusAlpha);
    }

    // If high-band is hovered, draw high-band on top with highlight.
    if (mHighFocus)
    {
        paintLowRegion(  g,    responseArea, gainLowPixel,     freq1Pixel,                mLowFocus    ? focusAlpha : noFocusAlpha);
        paintMidRegion(  g,    responseArea, gainMidPixel,     freq1Pixel, freq2Pixel,    mMidFocus    ? focusAlpha : noFocusAlpha);
        paintHighRegion( g,    responseArea, gainHighPixel,    freq2Pixel,                mHighFocus    ? focusAlpha : noFocusAlpha);
    }
}

/* [Claude 2026-10-29] The bands drawn as their real response, not trapezoids (Joe liked this crossover view
   better: "your version of the crossover better"): each band's gain with 24 dB/oct skirts at its crossovers,
   the fill under the curve, and the line only where the band is above the floor. */
// ===========================================================================================
namespace
{
    struct BandCurve { juce::Path fill, line; };

    BandCurve makeBandCurve(juce::Rectangle<float> area, float gainDb, float fLow, float fHigh)
    {
        BandCurve c;
        const float bottom = area.getBottom();
        const auto yOf = [&](float db) { return juce::jmap(juce::jmin(30.f, db), -30.f, 30.f, bottom, area.getY()); };
        bool pen = false;
        c.fill.startNewSubPath(area.getX(), bottom);
        for (float x = area.getX(); x <= area.getRight(); x += 1.f)
        {
            const float t = (x - area.getX()) / area.getWidth();
            const float f = 20.f * std::pow(1000.f, t);
            float db = gainDb;
            // Linkwitz-Riley 4th order, as the processor's juce::dsp::LinkwitzRileyFilter: 24 dB/oct, -6 dB at the crossover
            if (fLow > 0.f)  db += -20.f * std::log10(1.f + std::pow(fLow / f, 4.f));
            if (fHigh > 0.f) db += -20.f * std::log10(1.f + std::pow(f / fHigh, 4.f));
            const float yRaw = yOf(db);
            c.fill.lineTo(x, juce::jmin(bottom, yRaw));
            // the line carries on below the floor and is clipped there, so no end shows (Joe: "Let them run off frame
            // so you don't see the end caps")
            if (yRaw < bottom + 24.f) { if (pen) c.line.lineTo(x, yRaw); else c.line.startNewSubPath(x, yRaw); pen = true; }
            else pen = false;
        }
        c.fill.lineTo(area.getRight(), bottom);
        c.fill.closeSubPath();
        return c;
    }
}

/* Paints the low-frequency trapezoid */
// ===========================================================================================
void FrequencyResponse::paintLowRegion(juce::Graphics& g, juce::Rectangle<float> bounds, float gainPixel, float freq1Pixel, float alpha)
{
    using namespace AllColors::FrequencyResponseColors;
    using namespace ColorScheme::BandColors;
    using namespace Gradients::FrequencyResponse;
    
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    auto slope = 10;

    bounds.reduce(2, 0);

    // LOW REGION ========================================================
    juce::Path lowRegion;

    lowRegion.startNewSubPath(    bounds.getX(),    bounds.getBottom());
    lowRegion.lineTo(            bounds.getX(),    gainPixel);
    lowRegion.lineTo(            freq1Pixel,        gainPixel);

    if (bounds.getRight() - freq1Pixel > slope)
        lowRegion.lineTo(freq1Pixel + slope, bounds.getBottom());
    else
        lowRegion.lineTo(bounds.getRight(), bounds.getBottom());

    if (!lowBandMute && !lowBandSoloMute)
    {
        const auto curve = makeBandCurve(responseArea, mLowGain, 0.f, mLowMidCutoff);
        g.setGradientFill    (  makeLowRegionGradient(bounds.toFloat(), lowBandBypass) );
        g.setOpacity(alpha);
        g.fillPath(curve.fill);
        g.setColour        (    makeOutlineColor(getLowBandBaseColor(), lowBandBypass ));
        {
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion(responseArea.toNearestInt());
            g.strokePath(curve.line, juce::PathStrokeType(0.85f));  // [Claude 2026-10-29] thinner (Joe); was 1.2
        }
    }
}

/* Paints the mid-frequency trapezoid */
// ===========================================================================================
void FrequencyResponse::paintMidRegion(juce::Graphics& g, juce::Rectangle<float> bounds, float gainPixel, float freq1Pixel, float freq2Pixel, float alpha)
{
    using namespace AllColors::FrequencyResponseColors;
    using namespace ColorScheme::BandColors;
    using namespace Gradients::FrequencyResponse;
    
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    auto slope = 10;

    bounds.reduce(2, 0);

    juce::Path midRegion;
    
    if (freq1Pixel - bounds.getX() > slope)
        midRegion.startNewSubPath(freq1Pixel - slope, bounds.getBottom());
    else
        midRegion.startNewSubPath(bounds.getX(), bounds.getBottom());

    midRegion.lineTo(freq1Pixel, gainPixel);
    midRegion.lineTo(freq2Pixel, gainPixel);

    if (bounds.getRight() - freq2Pixel > slope)
        midRegion.lineTo(freq2Pixel + slope, bounds.getBottom());
    else
        midRegion.lineTo(bounds.getRight(), bounds.getBottom());

    if (!midBandMute && !midBandSoloMute)
    {
        const auto curve = makeBandCurve(responseArea, mMidGain, mLowMidCutoff, mMidHighCutoff);
        g.setGradientFill    ( makeMidRegionGradient(bounds.toFloat(), midBandBypass ));
        g.setOpacity(alpha);
        g.fillPath(curve.fill);
        g.setColour        (    makeOutlineColor(getMidBandBaseColor(), midBandBypass ));
        {
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion(responseArea.toNearestInt());
            g.strokePath(curve.line, juce::PathStrokeType(0.85f));  // [Claude 2026-10-29] thinner (Joe); was 1.2
        }
    }

}

/* Paints the high-frequency trapezoid */
// ===========================================================================================
void FrequencyResponse::paintHighRegion(juce::Graphics& g, juce::Rectangle<float> bounds, float gainPixel, float freq2Pixel, float alpha)
{
    using namespace AllColors::FrequencyResponseColors;
    using namespace ColorScheme::BandColors;
    using namespace Gradients::FrequencyResponse;
    
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    auto slope = 10;

    bounds.reduce(2, 0);

    juce::Path highRegion;

    if (freq2Pixel - bounds.getX() > slope)
        highRegion.startNewSubPath(freq2Pixel - slope, bounds.getBottom());
    else
        highRegion.startNewSubPath(bounds.getX(), bounds.getBottom());

    highRegion.lineTo(freq2Pixel, gainPixel);
    highRegion.lineTo(bounds.getRight(), gainPixel);
    highRegion.lineTo(bounds.getRight(), bounds.getBottom());

    if (!highBandMute && !highBandSoloMute)
    {
        const auto curve = makeBandCurve(responseArea, mHighGain, mMidHighCutoff, 0.f);
        g.setGradientFill    ( makeHighRegionGradient(bounds.toFloat(), highBandBypass ));
        g.setOpacity(alpha);
        g.fillPath(curve.fill);
        g.setColour        (    makeOutlineColor(getHighBandBaseColor(), highBandBypass ));
        {
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion(responseArea.toNearestInt());
            g.strokePath(curve.line, juce::PathStrokeType(0.85f));  // [Claude 2026-10-29] thinner (Joe); was 1.2
        }
    }
}

/* Calculates and repositions labels with respect to cursors */
// ===========================================================================================
void FrequencyResponse::drawLabels()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    auto bounds = getLocalBounds();
    int labelWidth = 58;
    int labelHeight = 20;
    
    int marginX = 5;
    int marginY = 3;

    int labelX1 = newCursorLM.getX() + (newCursorLM.getWidth() / 2) - labelWidth / 2;
    int labelX2 = newCursorMH.getX() + (newCursorMH.getWidth() / 2) - labelWidth / 2;

    int labelY1 = bounds.getY() + marginY;
    int labelY2 = bounds.getY() + marginY;

    // Bump at left bounds
    if (labelX1 <= responseArea.getX() + marginX)
        labelX1 = responseArea.getX() + marginX;

    if (labelX2 <= responseArea.getX() + marginX)
        labelX2 = responseArea.getX() + marginX;

    // Bump at right bounds
    if (labelX1 + labelWidth  >= responseArea.getRight() - marginX)
        labelX1 = responseArea.getRight() - labelWidth - marginX;

    if (labelX2 + labelWidth  >= responseArea.getRight() - marginX)
        labelX2 = responseArea.getRight() - labelWidth - marginX;
    
    // Bump near each other
    auto distTooClose = labelWidth + marginX;
    auto center = (labelX1 + labelX2) / 2;

    if (    (   (lowBandMute || lowBandSoloMute) && (midBandMute || midBandSoloMute)    )  || 
            (   (midBandMute || midBandSoloMute) && (highBandMute || highBandSoloMute)  ) )
    {
        /* Then ignore the label-2-label boundary checks */
    }
    else
    {
        if (labelX2 - labelX1 <= distTooClose)
        {
            labelX1 = center - (labelWidth / 2) - marginX;
            labelX2 = center + (labelWidth / 2) + marginX;
        }
    }

    newFreqLabelLow.setBounds(labelX1, labelY1, labelWidth, labelHeight);
    newFreqLabelHigh.setBounds(labelX2, labelY2, labelWidth, labelHeight);
}

/* Calculates the boundary values for the three frequency bands before painting */
// ===========================================================================================
void FrequencyResponse::updateResponse()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    checkSolos();

    auto killLowBand = lowBandMute || lowBandSoloMute;
    auto killMidBand = midBandMute || midBandSoloMute;
    auto killHighBand = highBandMute || highBandSoloMute;

    auto showLowBand = !lowBandMute && !lowBandSoloMute;
    auto showMidBand = !midBandMute && !midBandSoloMute;
    auto showHighBand = !highBandMute && !highBandSoloMute;

    using namespace Params;
    const auto& params = GetParams();

    auto cursorWidth = 10;

    // Get cutoff frequency parameter values
    mLowMidCutoff = *apvts.getRawParameterValue(params.at(Names::Low_Mid_Crossover_Freq));
    mMidHighCutoff = *apvts.getRawParameterValue(params.at(Names::Mid_High_Crossover_Freq));

    // Convert cutoff values to relative pixel values
    freq1Pixel = responseArea.getX() + mapLog2(mLowMidCutoff) * responseArea.getWidth();
    freq2Pixel = responseArea.getX() + mapLog2(mMidHighCutoff) * responseArea.getWidth();

    if (killLowBand && killMidBand)
        newCursorLM.setBounds(0, 0, 0, 0);
    else
        newCursorLM.setBounds(freq1Pixel - cursorWidth / 2, responseArea.getY(), cursorWidth, responseArea.getHeight());
        


    if (killMidBand && killHighBand)
        newCursorMH.setBounds(0, 0, 0, 0);
    else
        newCursorMH.setBounds(freq2Pixel - cursorWidth / 2, responseArea.getY(), cursorWidth, responseArea.getHeight());



    // Set Mid-High Crossover Cursor Bounds
    mLowGain = *apvts.getRawParameterValue(params.at(Names::Gain_Low_Band));
    mMidGain = *apvts.getRawParameterValue(params.at(Names::Gain_Mid_Band));
    mHighGain = *apvts.getRawParameterValue(params.at(Names::Gain_High_Band));

    // Convert band gain values to relative pixel values
    // [Claude 2026-10-29] jmap already returns a position inside responseArea; adding getY() again put the gain
    // cursors and the grid a little below the curves (Joe: "the gain ones, look like they are not center aligned").
    gainLowPixel = juce::jmap(mLowGain,    -30.f, 30.f, float(responseArea.getBottom()), responseArea.getY());
    gainMidPixel = juce::jmap(mMidGain,    -30.f, 30.f, float(responseArea.getBottom()), responseArea.getY());
    gainHighPixel = juce::jmap(mHighGain, -30.f, 30.f, float(responseArea.getBottom()), responseArea.getY());

    // Set Cursor Widths. Cursors A Fraction of Band Width
    auto lowWidth = (freq1Pixel - responseArea.getX()) * 0.75f;
    auto midWidth = (freq2Pixel - freq1Pixel) * 0.75f;
    auto highWidth = (responseArea.getRight() - freq2Pixel) * 0.75f;

    // [Claude 2026-10-29] Each gain handle sits on its band's drawn curve at the handle's centre, not on the gain line:
    // the crossover slopes pull a band's top below its gain (most for the mid band), and the handle floated above the
    // peak (Joe: "the blue bar on the mid band gain doesn't look center straddled across the peak")
    const auto lr4dB = [](float f, float cut, bool high) { return -20.f * std::log10(1.f + std::pow(high ? cut / f : f / cut, 4.f)); };
    const auto freqAt = [&](float px) { return 20.f * std::pow(1000.f, (px - responseArea.getX()) / responseArea.getWidth()); };   // inverse of mapLog2
    float pillLowY = gainLowPixel, pillMidY = gainMidPixel, pillHighY = gainHighPixel;
    const auto dbToPixel = [&](float db) { return juce::jmap(juce::jlimit(-30.f, 30.f, db), -30.f, 30.f, float(responseArea.getBottom()), responseArea.getY()); };
    {
        const float fL = freqAt((responseArea.getX() + freq1Pixel) / 2.f);
        const float fM = freqAt((freq1Pixel + freq2Pixel) / 2.f);
        const float fH = freqAt((freq2Pixel + responseArea.getRight()) / 2.f);
        pillLowY  = dbToPixel(mLowGain  + lr4dB(fL, mLowMidCutoff, false));
        pillMidY  = dbToPixel(mMidGain  + lr4dB(fM, mLowMidCutoff, true) + lr4dB(fM, mMidHighCutoff, false));
        pillHighY = dbToPixel(mHighGain + lr4dB(fH, mMidHighCutoff, true));
    }

    // Set Low-Gain Cursor Bounds
    // =========================================================================
    auto center = (responseArea.getX() + freq1Pixel) / 2.f;

    if (showLowBand)
    {
        newCursorLowGain.setBounds  (   center - lowWidth / 2.f, 
                                        pillLowY - cursorWidth / 2.f, 
                                        lowWidth, 
                                        cursorWidth
                                    );
    }
    else
        newCursorLowGain.setBounds(0,0,0,0);



    // Set Mid-Gain Cursor Bounds
    // =========================================================================
    center = (freq1Pixel + freq2Pixel) / 2.f;

    if (showMidBand)
    {
        newCursorMidGain.setBounds  (   center - midWidth / 2.f, 
                                        pillMidY - cursorWidth / 2.f, 
                                        midWidth, 
                                        cursorWidth
                                    );
    }
    else
        newCursorMidGain.setBounds(0,0,0,0);


    // Set High-Gain Cursor Bounds
    // =========================================================================
    center = (freq2Pixel + responseArea.getRight()) / 2.f;

    if (showHighBand)
    {
    newCursorHighGain.setBounds (   center - highWidth / 2.f, 
                                    pillHighY - cursorWidth / 2.f, 
                                    highWidth, 
                                    cursorWidth
                                );
    }
    else
        newCursorHighGain.setBounds(0,0,0,0);

    // [Claude 2026-10-29] Each crossover's handle sits where its two bands meet: 6 dB under the higher of the two gains
    const auto yAtDb = [&](float db) { return juce::jmap(juce::jlimit(-30.f, 30.f, db), -30.f, 30.f, float(responseArea.getBottom()), responseArea.getY()); };
    // Each ring sits on its crossover line where the drawn outline crosses it: the higher of the two bands' real
    // responses at that frequency. With equal gains that is exactly where the curves meet (-6 dB); with different
    // gains it follows the louder band's curve, so it always sits on a line you can see (Joe: "Their position is
    // based on whatever band moved last ... Are they supposed to be at the intersection")
    const auto lr4 = [](float f, float cut, bool high) { return -20.f * std::log10(1.f + std::pow(high ? cut / f : f / cut, 4.f)); };
    // Only bands that are showing count: soloed or muted bands are not drawn, so a ring on them floated in the air
    // (Joe: "when you solo, the circles don't update positions"). With neither band showing, the ring goes away.
    const float hidden = -1000.f;
    const float lowAtLM  = showLowBand  ? mLowGain + lr4(mLowMidCutoff, mLowMidCutoff, false) : hidden;
    const float midAtLM  = showMidBand  ? mMidGain + lr4(mLowMidCutoff, mLowMidCutoff, true) + lr4(mLowMidCutoff, mMidHighCutoff, false) : hidden;
    const float midAtMH  = showMidBand  ? mMidGain + lr4(mMidHighCutoff, mLowMidCutoff, true) + lr4(mMidHighCutoff, mMidHighCutoff, false) : hidden;
    const float highAtMH = showHighBand ? mHighGain + lr4(mMidHighCutoff, mMidHighCutoff, true) : hidden;
    const float atLM = juce::jmax(lowAtLM, midAtLM), atMH = juce::jmax(midAtMH, highAtMH);
    const float offScreen = 1.0e6f;   // drawn far below the display, so clipped away
    newCursorLM.setMarkerY(atLM > hidden ? yAtDb(atLM) - newCursorLM.getY() : offScreen);
    newCursorMH.setMarkerY(atMH > hidden ? yAtDb(atMH) - newCursorMH.getY() : offScreen);

    // [Claude 2026-10-29] Review renders only: WLA_FOCUS=LM, MH or MID shows that handle as if hovered, so a still can
    // show what emerges on hover. Never set in a release.
    if (const char* focus = std::getenv("WLA_FOCUS"))
    {
        const juce::String f(focus);
        if (f.contains("LM"))  newCursorLM.setFocus(true);
        if (f.contains("MH"))  newCursorMH.setFocus(true);
        if (f.contains("MID")) newCursorMidGain.setFocus(true);
    }

    repaint(1,1,getWidth()-2, getHeight()-2);
}

/* Checks if a given band is soloed, and mutes the other bands if so */
// ===========================================================================================
void FrequencyResponse::checkSolos()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");



    auto x = lowBandSolo * 100;
    auto y = midBandSolo * 10;
    auto z = highBandSolo;
    auto s = x + y + z;

    lowBandSoloMute = false;
    midBandSoloMute = false;
    highBandSoloMute = false;

    switch (s)
    {
    case 1:   { lowBandSoloMute = true; midBandSoloMute = true;     break; }
    case 10:  { lowBandSoloMute = true; highBandSoloMute = true;    break; }
    case 11:  { lowBandSoloMute = true;                             break; }
    case 100: { midBandSoloMute = true; highBandSoloMute = true;    break; }
    case 101: { midBandSoloMute = true;                             break; }
    case 110: { highBandSoloMute = true;                            break; }
    }
}

/* Map linear slider value to true octave-logarithmic value */
// ===========================================================================================
float FrequencyResponse::mapLog2(float freq)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    auto logMin = std::log2(20.f);
    auto logMax = std::log2(20000.f);

    return (std::log2(freq) - logMin) / (logMax - logMin);
}

/* Called on slider value change */
// ===========================================================================================
void FrequencyResponse::sliderValueChanged(juce::Slider* slider)
{
    // 'Interface' slider is linear from 0 to 1.
    // Changed by user by dragging Cutoff Cursors
    // Slider is converted from [0 to 1] to [20 to 20k] to store frequency

    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    if (slider == &sliderLowMidInterface || slider == &sliderMidHighInterface)
    {
        if (slider == &sliderLowMidInterface)
        {
            // Convert [0 to 1] (Interface) value to [20 to 20k] (Cutoff) value.
            // Send to Cutoff slider to update params.

            auto y = slider->getValue();
            auto f = 20 * pow(2, std::log2(1000.f) * y);
            sliderLowMidCutoff.setValue(f);
        }

        if (slider == &sliderMidHighInterface)
        {
            /* Convert[0 to 1](Interface) value to [20 to 20k](Cutoff) value. Send to Cutoff slider to update params. */

            auto y = slider->getValue();
            auto f = 20 * pow(2, std::log2(1000.f) * y);
            sliderMidHighCutoff.setValue(f);
        }

    }

    /*
        'Cutoff' slider is linear from 20 to 20k.
        Changed by user via attachment to external sliders.
        Slider is converted from [20 to 20k] to [0 to 1] for pixel representation 
     */ 

    if (slider == &sliderLowMidCutoff || slider == &sliderMidHighCutoff)
    {
        // [Claude 2026-10-27] dontSendNotification: without it, the 0.001-step interface sliders sent their rounded
        // value back to the crossover, so just opening the window moved Mid-High from 1000 Hz to about 999 Hz
        sliderMidHighInterface.setValue(mapLog2(sliderMidHighCutoff.getValue()), juce::dontSendNotification);
        sliderLowMidInterface.setValue(mapLog2(sliderLowMidCutoff.getValue()), juce::dontSendNotification);
    }
    
    if (slider == &sliderLowMidCutoff)
    {
        if (slider->getValue() > sliderMidHighCutoff.getValue())
            sliderMidHighCutoff.setValue(slider->getValue());
    }

    if (slider == &sliderMidHighCutoff)
    {
        if (slider->getValue() < sliderLowMidCutoff.getValue())
            sliderLowMidCutoff.setValue(slider->getValue());
    }
    
    //updateStringText();
    newFreqLabelLow.setLabelValue(sliderLowMidCutoff.getValue());
    newFreqLabelHigh.setLabelValue(sliderMidHighCutoff.getValue());

    updateResponse();
}

/* Called on mouse move */
// ===========================================================================================
void FrequencyResponse::mouseMove(const juce::MouseEvent& event)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    checkCursorFocus(event);
}

/* Called when mouse enters parent or any child components */
// ===========================================================================================
void FrequencyResponse::mouseEnter(const juce::MouseEvent& event)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    checkCursorFocus(event);
}

/* Called when mouse exits parent or any child components */
// ===========================================================================================
void FrequencyResponse::mouseExit(const juce::MouseEvent& event)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    checkCursorFocus(event);
}

/* Inspects mouse position to see if mouse is hovering over any cursor */
// ===========================================================================================
void FrequencyResponse::checkCursorFocus(const juce::MouseEvent& event)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, "");

    auto xM = event.getPosition().getX();
    auto yM = event.getPosition().getY();

    auto mousePoint = getLocalPoint(event.eventComponent, event.getPosition());

    // Establish top-bottom bounds of Dummy Sliders
    auto sliderTop = juce::jmap(24.f, -30.f, 30.f, float(responseArea.getBottom()), responseArea.getY())-5;
    auto sliderBot = juce::jmap(-24.f, -30.f, 30.f, float(responseArea.getBottom()), responseArea.getY())+5;

    // Check if mouse is on cursor, place dummy slider underneath mouse

    // Low-Mid
    // =============================================================
    if (newCursorLM.getBounds().contains(mousePoint))
    {
        sliderLowMidInterface.setBounds(responseArea.getX(), yM, responseArea.getWidth(), 5);
        newCursorLM.setFocus(true);
    }
    else
    {
        sliderLowMidInterface.setBounds(0, 0, 0, 0);
        newCursorLM.setFocus(false);
    }

    // Mid-High
    // =============================================================
    if (newCursorMH.getBounds().contains(mousePoint))
    {
        sliderMidHighInterface.setBounds(responseArea.getX(), yM, responseArea.getWidth(), 5);
        newCursorMH.setFocus(true);
    }
    else
    {
        sliderMidHighInterface.setBounds(0, 0, 0, 0);
        newCursorMH.setFocus(false);
    }

    // Low Gain
    // =============================================================
    if (newCursorLowGain.getBounds().contains(mousePoint))
    {
        //sliderLowGain.setBounds(xM, sliderTop, 5, (sliderBot - sliderTop));
        sliderLowGain.setBounds(newCursorLowGain.getX() + xM, sliderTop, 5, (sliderBot - sliderTop));
        newCursorLowGain.setFocus(true);
    }
    else
    {
        sliderLowGain.setBounds(0, 0, 0, 0);
        newCursorLowGain.setFocus(false);
    }

    // Mid Gain
    // =============================================================
    if (newCursorMidGain.getBounds().contains(mousePoint))
    {
        sliderMidGain.setBounds(newCursorMidGain.getX() + xM, sliderTop, 5, (sliderBot - sliderTop));
        newCursorMidGain.setFocus(true);
    }
    else
    {
        sliderMidGain.setBounds(0, 0, 0, 0);
        newCursorMidGain.setFocus(false);
    }

    // High Gain
    // =============================================================
    if (newCursorHighGain.getBounds().contains(mousePoint))
    {
        sliderHighGain.setBounds(newCursorHighGain.getX() + xM, sliderTop, 5, (sliderBot - sliderTop));
        newCursorHighGain.setFocus(true);
    }
    else
    {
        sliderHighGain.setBounds(0, 0, 0, 0);
        newCursorHighGain.setFocus(false);
    }
}

/* Receives broadcasts of parameter changes from relevant areas of program */
// ===========================================================================================
void FrequencyResponse::actionListenerCallback(const juce::String& message)
{
    auto bandName = message.replaceSection(5, 30, "");
    bandName = bandName.removeCharacters("x");

    auto paramName = message.replaceSection(0, 10, "");
    paramName = paramName.replaceSection(10, 25, "");
    paramName = paramName.removeCharacters("x");

    juce::String paramValue = message.replaceSection(0, 25, "");
    paramValue = paramValue.removeCharacters("x");

    DBG("FREQ " << paramName);

    if (paramName == "GAIN") 
    {
    }

    if (paramName == "BYPASS")
    {
        if (bandName == "LOW")
            lowBandBypass = paramValue.getIntValue();

        if (bandName == "MID")
            midBandBypass = paramValue.getIntValue();

        if (bandName == "HIGH")
            highBandBypass = paramValue.getIntValue();
    }

    if (paramName == "SOLO")
    {
        if (bandName == "LOW")
            lowBandSolo = paramValue.getIntValue();

        if (bandName == "MID")
            midBandSolo = paramValue.getIntValue();

        if (bandName == "HIGH")
            highBandSolo = paramValue.getIntValue();
    }

    if (paramName == "MUTE")
    {
        if (bandName == "LOW")
            lowBandMute = paramValue.getIntValue();

        if (bandName == "MID")
            midBandMute = paramValue.getIntValue();

        if (bandName == "HIGH")
            highBandMute = paramValue.getIntValue();
    }

    /* Reformat this code standard */
    if (paramName == "FOCUS")
    {
        mLowFocus = false;
        mMidFocus = false;
        mHighFocus = false;

        if (paramValue == "LOW")
            mLowFocus = true;

        if (paramValue == "MID")
            mMidFocus = true;

        if (paramValue == "HIGH")
            mHighFocus = true;
    }

    if (paramName == "LOWFREQ")
    {
        auto value = paramValue.getFloatValue();
        sliderLowMidCutoff.setValue(value);
    }

    if (paramName == "HIGHFREQ")
    {
        auto value = paramValue.getFloatValue();
        sliderMidHighCutoff.setValue(value);
    }

    updateResponse();
}
