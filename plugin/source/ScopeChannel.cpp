#include "ScopeChannel.h"
#include "UtilityFunctions.h"   // [Claude 2026-10-29] PixelGrid

/* Constructor */
// ========================================================
ScopeChannel::ScopeChannel(juce::AudioProcessorValueTreeState& apvts, LFO& lfo, ScrollPad& scrollPad)
    :   apvts(apvts),
        lfo(lfo),
        sliderScroll(scrollPad)
{
    // Initializes the display's color-scheme based on band type
    // [Claude 2026-10-29] after reading the bypass (below it, a band bypassed at load kept its colour)
    updateBandBypass();
    setBandColors();
    
    using namespace Params;             // Create a Local Reference to Parameter Mapping
    const auto& params = GetParams();   // Create a Local Reference to Parameter Mapping
    
    //mSampleRate = lfo.getSampleRate();
    mSampleRate = apvts.processor.getSampleRate();
    mHostBpm = lfo.getHostBPM();

    localLFO.initializeLFO(mSampleRate);

    if (mSampleRate == 0)
        mSampleRate = 48000;

    if (mHostBpm == 0 || mHostBpm == 1)
        mHostBpm = 120;
    targetBpm = mHostBpm;

    localLFO.setDsp(false);
    
    //sendActionMessage("REQUEST_PARAMS_" + getName());
    //DBG("SC SENDING REQUEST FOR PARAMS");

    //localLFO.setWaveRate            (lfo.getWaveRate());
    //localLFO.setRelativePhase       (lfo.getRelativePhase());
    //localLFO.setWaveMultiplier      (lfo.getWaveMultiplier());
    //localLFO.setSyncedToHost        (lfo.isSyncedToHost());
    //localLFO.setWaveSkew            (lfo.getWaveSkew());
    //localLFO.setWaveDepth           (lfo.getWaveDepth());
    //localLFO.setWaveInvert          (lfo.getWaveInvert());
    //localLFO.setWaveform            (lfo.getWaveform());

    

    // Listen for changes to scrollbar
    apvts.addParameterListener(params.at(Names::Scope_Point1), this);
    apvts.addParameterListener(params.at(Names::Scope_Point2), this);
    
    // Listen for changes to Oscilloscope Options Menu
    apvts.addParameterListener(params.at(Names::Show_Low_Scope), this);
    apvts.addParameterListener(params.at(Names::Show_Mid_Scope), this);
    apvts.addParameterListener(params.at(Names::Show_High_Scope), this);
    apvts.addParameterListener(params.at(Names::Stack_Bands_Scope), this);

    localLFO.updateLFO(mSampleRate, mHostBpm);

    startTimerHz(60);   // [Claude 2026-10-29] follows the host tempo and eases the drawn free rate
}

/* Destructor */
// ========================================================
ScopeChannel::~ScopeChannel()
{
    using namespace Params;             // Create a Local Reference to Parameter Mapping
    const auto& params = GetParams();   // Create a Local Reference to Parameter Mapping

    apvts.removeParameterListener(params.at(Names::Scope_Point1), this);
    apvts.removeParameterListener(params.at(Names::Scope_Point2), this);

    apvts.removeParameterListener(params.at(Names::Show_Low_Scope), this);
    apvts.removeParameterListener(params.at(Names::Show_Mid_Scope), this);
    apvts.removeParameterListener(params.at(Names::Show_High_Scope), this);
    apvts.removeParameterListener(params.at(Names::Stack_Bands_Scope), this);
}

/* Set color theme */
// ========================================================
void ScopeChannel::setBandColors()
{
    //WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    // [Claude 2026-10-29] Always an empty rectangle: this ran only in the constructor, where the bounds are empty, and
    // the waveform's look is that gradient. Now it also runs on a bypass change, so it must not pick up real bounds.
    const juce::Rectangle<float> bounds;
    //using namespace AllColors::OscilloscopeColors;
    using namespace ColorScheme::BandColors;
    using namespace Gradients::Oscilloscope;
    
    if (lfo.getLfoID() == 0)
{
        fillGradient = makeWaveformGradient(bounds, "LOW", isBandBypassed);
        strokeColor = makeOutlineColor(getLowBandBaseColor(), isBandBypassed);
    }
    
    if (lfo.getLfoID() == 1)
    {
        fillGradient = makeWaveformGradient(bounds, "MID", isBandBypassed);
        strokeColor = makeOutlineColor(getMidBandBaseColor(), isBandBypassed);
    }
    
    if (lfo.getLfoID() == 2)
    {
        fillGradient = makeWaveformGradient(bounds, "HIGH", isBandBypassed);
        strokeColor = makeOutlineColor(getHighBandBaseColor(), isBandBypassed);
    }
}

/* Paint graphics */
// ========================================================
void ScopeChannel::paint(juce::Graphics& g)
{

    //WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    using namespace juce;
    using namespace AllColors::OscilloscopeColors;
            
    // ==================================================
    auto bounds = getLocalBounds().toFloat();
    
    g.setGradientFill(BACKGROUND_GRADIENT(bounds));
    
    // [Claude 2026-10-29] Each band its own lane (Joe: "I still want the visualization of 3 different lanes"): when
    // the bands are stacked, a panel a step lighter than the black between lanes, a trace of the band's colour, and a
    // lit top edge; the crossover's near-black stays the ground.
    if (bandsAreStacked)
    {
        g.fillRoundedRectangle(bounds, 5.f);
        // [Claude 2026-10-29] A flat lift and a faint colour wash across the whole width; only the top and bottom
        // soften, below (Joe: "I only want that wash on the top/bottom not the sides", "still a little too much")
        g.setColour(juce::Colour(0xff0a0b11));  // just off the black (Joe: "Just enough to distinguish the black background from the lanes")
        g.fillRect(bounds);
        g.setColour(strokeColor.withAlpha(0.012f));
        g.fillRect(bounds);

        // and darker toward the top and bottom, a vignette across the lane's height (Joe: "soften the top/bottom
        // edges of the lanes")
        juce::ColourGradient shade(juce::Colour(0xff03010d).withAlpha(0.5f), 0.f, bounds.getY(),
                                   juce::Colour(0xff03010d).withAlpha(0.5f), 0.f, bounds.getBottom(), false);
        shade.addColour(0.09, juce::Colour(0xff03010d).withAlpha(0.f));  // tight to the edges (Joe: "tighten the radius? much more")
        shade.addColour(0.91, juce::Colour(0xff03010d).withAlpha(0.f));
        g.setGradientFill(shade);
        g.fillRect(bounds);
    }

    paintGridLines(g);
    paintWaveform(g);
}

/* Recieves broadcasts of parameter changes */
// ========================================================
void ScopeChannel::actionListenerCallback(const juce::String& message)
{
    auto paramName = message.replaceSection(0, 10, "");
    paramName = paramName.replaceSection(10, 25, "");
    paramName = paramName.removeCharacters("x");

    juce::String paramValue = message.replaceSection(0, 25, "");
    paramValue = paramValue.removeCharacters("x");

    bool shouldUpdateScope = false;

    DBG("SCOPE " << paramName);

    if (paramName == "WAVESHAPE")
    {
        int waveform = 0;
        waveform = paramValue.getIntValue();
        localLFO.setWaveform(waveform);
        shouldUpdateScope = true;
    }

    if (paramName == "DEPTH")
    {
        float depth = 0.0f;
        depth = paramValue.getFloatValue();
        localLFO.setWaveDepth(depth);
        shouldUpdateScope = true;
    }

    if (paramName == "SKEW")
    {
        float skew = 50.0f;
        skew = paramValue.getFloatValue();
        localLFO.setWaveSkew(skew);
        shouldUpdateScope = true;
    }

    if (paramName == "PHASE")
    {
        float phase = 0.0f;
        phase = paramValue.getFloatValue();
        localLFO.setRelativePhase(phase);
        shouldUpdateScope = true;
    }

    if (paramName == "RHYTHM")
    {
        int rhythm = 0;
        rhythm = paramValue.getIntValue();
        localLFO.setWaveMultiplier(rhythm);
        shouldUpdateScope = true;
    }

    if (paramName == "RATE")
    {
        float rate = 0.0f;
        rate = paramValue.getFloatValue();
        targetRate = rate;
        // [Claude 2026-10-29] the first value (and any while synced, where the rate is not drawn) is taken at once;
        // later ones are eased toward by timerCallback
        if (! rateShown || localLFO.isSyncedToHost())
        {
            localLFO.setWaveRate(rate);
            rateShown = true;
        }
        shouldUpdateScope = true;
    }

    if (paramName == "INVERT")
    {
        bool invert = false;
        invert = paramValue.getIntValue();
        localLFO.setWaveInvert(invert);
        shouldUpdateScope = true;
    }

    if (paramName == "SYNC")
    {
        bool sync = false;
        sync = paramValue.getIntValue();
        localLFO.setSyncedToHost(sync);
        if (rateShown) localLFO.setWaveRate(targetRate);   // [Claude 2026-10-29] no easing across a switch
        shouldUpdateScope = true;
    }

    // [Claude 2026-10-29] Bypass greys the band's waveform the way it greys the band in the crossover: same message,
    // same bypass grey (Joe: when a band is bypassed its LFO display should go grey like the crossover does)
    if (paramName == "BYPASS")
    {
        isBandBypassed = paramValue.getIntValue() != 0;
        setBandColors();
        repaint();
    }

    if (paramName == "SCROLLBAR")
    {
        scrollZoom = sliderScroll.getScrollZoom();
        scrollCenter = sliderScroll.getScrollCenter();
        //shouldUpdateScope = true;

        generateLFOPathForDrawing(lfoPath, lfoFill, waveTable, localLFO);
        redrawScope();
        repaint();
    }

    if (paramName == "FOCUS")
    {
        isBandFocused = false;

        if (paramValue == getName())
            isBandFocused = true;
        //else
        //    isBandFocused = false;

        //shouldUpdateScope = true;

        generateLFOPathForDrawing(lfoPath, lfoFill, waveTable, localLFO);
        redrawScope();
        repaint();
    }


    if (shouldUpdateScope)
    {
        /* Parameters have changed, so recalculate them in the local LFO */
        localLFO.updateLFO(mSampleRate, mHostBpm);
        redrawScope();
        repaint();
    }



    
}

// [Claude 2026-10-29] Each frame: the drawn free rate and the host tempo each take one easing step toward their
// targets (one-pole, about 60 ms), so a rate drag or a tempo change squeezes or stretches a free band's wave smoothly
// about the centre (Joe: follow live tempo changes). The tempo is the processor's, from its latest block. Synced
// waves are measured in beats, so the tempo does not change them and they are not redrawn for it.
void ScopeChannel::timerCallback()
{
    if (auto* proc = dynamic_cast<audio_plugin::AudioPluginAudioProcessor*>(&apvts.processor))
    {
        const double bpm = proc->getHostBpmForDisplay();
        if (bpm > 1.0) targetBpm = bpm;
    }

    const auto ease = [](double shown, double target, double snap)
    { return std::abs(target - shown) < snap ? target : shown + 0.35 * (target - shown); };

    bool changed = false;
    if (rateShown && localLFO.getWaveRate() != targetRate)
    {
        localLFO.setWaveRate((float) ease(localLFO.getWaveRate(), targetRate, 0.002));
        changed = true;
    }
    if (mHostBpm != targetBpm)
    {
        mHostBpm = ease(mHostBpm, targetBpm, 0.01);
        localLFO.updateTempo(mSampleRate, mHostBpm);
        changed = true;
    }
    if (changed && ! localLFO.isSyncedToHost())
        repaint();   // paint redraws the path from the rate and tempo
}

/* Paint grid lines */
// ========================================================
void ScopeChannel::paintGridLines(juce::Graphics& g)
{

    //WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    using namespace AllColors::OscilloscopeColors;

    auto bounds = getLocalBounds();

    // DRAW VERTICAL LINES =============================
    juce::Line<float> verticalAxis;

    g.setColour(juce::Colours::lightgrey);

    g.setOpacity(0.35f);
    
    if (bounds.getWidth() > 0)
        beatSpacing = bounds.getWidth() / (scrollZoom);
    else
        beatSpacing = 1.f;

    // Allows Grid to Split Center
    float mDisplayPhase = scrollCenter * bounds.getWidth();

    auto c = bounds.getCentreX() + mDisplayPhase;
    int num = 0;

    float leftGridLine;
    float rightGridLine;

    const PixelGrid grid(g, this);   // [Claude 2026-10-29] the grid on whole screen pixels at every window size

    // Draw Gridlines
    for (int i = bounds.getCentreX(); i <= 2 * bounds.getRight(); i++)
    {

        // Draw Markers at Single-Beat Intervals
        if (i % juce::jmax(1, (int)beatSpacing) == 0)   // [Claude 2026-10-27] jmax: (int)beatSpacing could be 0, a divide-by-zero crash
        {
            // [Claude 2026-10-29] The crossover's grid: one quiet grey for every beat, the origin a step brighter
            // (Joe: "make the gridlines the same style as the crossover"). Was dark and light grey alternating at 35 %.
            g.setColour(juce::Colours::lightgrey.withAlpha(0.12f));  // the origin too (Joe: "match the center one too")

            // Draw Twice as Many Gridlines to Account for Panning
            rightGridLine = c + num * beatSpacing;

            verticalAxis.setStart(rightGridLine, bounds.getY());
            verticalAxis.setEnd(rightGridLine, bounds.getBottom());
            fillVLine(g, grid, rightGridLine - 0.5f, (float) bounds.getY(), (float) bounds.getBottom());   // [Claude 2026-10-29] was a 1 px drawLine at a fractional x

            leftGridLine = c - num * beatSpacing;
            verticalAxis.setStart(leftGridLine, bounds.getY());
            verticalAxis.setEnd(leftGridLine, bounds.getBottom());
            if (num > 0)    // at the origin both lines fall on one place and drew it twice as bright
                fillVLine(g, grid, leftGridLine - 0.5f, (float) bounds.getY(), (float) bounds.getBottom());

            // Calculate Left-Most Onscreen Valid (Even) Marker

            if (leftGridLine >= bounds.getX())
                leastOnscreenLeftGridLine = leftGridLine;

            // Calculate Right-Most Onscreen Valid (Even) Marker
            if (rightGridLine <= bounds.getRight())
                greatestOnscreenRightGridLine = rightGridLine;

            num++;
        }

        // Calulate Beat Information

        playBackNumBeats = 2 * num;
        playBackWidth = playBackNumBeats * beatSpacing;

        // Number of Onscreen Beats Present
        onScreenBeatWidth = greatestOnscreenRightGridLine - leastOnscreenLeftGridLine;
        onScreenNumBeats = onScreenBeatWidth / beatSpacing;

        onScreenNumBeatsLeftFromCenter = (c - leastOnscreenLeftGridLine) / beatSpacing;
        onScreenNumBeatsRightFromCenter = (greatestOnscreenRightGridLine - c) / beatSpacing;
    }

    g.setColour(juce::Colours::lightgrey);
    g.setOpacity(0.35f);
    
    // DRAW HORIZONTAL LINES =============================
    for (int i = 1; i < numDepthLines; i++)
    {
        auto y = bounds.getY() + i * (bounds.getHeight() / numDepthLines);
        fillHLine(g, grid, (float) y, (float) bounds.getX(), (float) bounds.getRight());   // [Claude 2026-10-29]
    }

    redrawScope();
}

/* Paint the LFO waveforms */
// ========================================================
void ScopeChannel::paintWaveform(juce::Graphics& g)
{

    //WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    using namespace juce;
    using namespace AllColors::OscilloscopeColors;
    using namespace ColorScheme::BandColors;
    
    /* Fill */
    g.setGradientFill(fillGradient);
    g.setOpacity(isBandFocused ? 0.95f : 0.85f);
    g.fillPath(lfoFill);

    /* Stroke */
    g.setColour(strokeColor);   // [Claude 2026-10-29] grey when bypassed: setBandColors uses the crossover's makeOutlineColor
    g.setOpacity(isBandFocused ? 1.f : 0.75f);
    g.strokePath(lfoPath, isBandFocused?    juce::PathStrokeType(1.f, PathStrokeType::JointStyle::curved, PathStrokeType::EndCapStyle::rounded) :
                                            juce::PathStrokeType(0.85f, PathStrokeType::JointStyle::curved, PathStrokeType::EndCapStyle::rounded));
}

/* Parent method to recalculate scope waveform points*/
// ========================================================
void ScopeChannel::redrawScope()
{
    //WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    waveTable = scaleWaveAmplitude();
    generateLFOPathForDrawing(lfoPath, lfoFill, waveTable, localLFO);
}

/* Scales incoming WaveTable by Amplitude Parameter */
// ========================================================
juce::Array<float> ScopeChannel::scaleWaveAmplitude()
{

    //WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    auto waveTable = localLFO.getWaveTableForDisplay(waveTableDownSampleSize);

    float mDepth = localLFO.getWaveDepth() / 100.f;

    float min = 2.0f;        // Used to calculate min value of LFO, for scaling
    float max = -1.0f;        // Used to calculate max value of LFO, for scaling
    
    for (int sample = 0; sample < waveTable.size(); sample++)
    {
        // Get Min and Max
        if (waveTable[sample] < min)
            min = waveTable[sample];

        if (waveTable[sample] > max)
            max = waveTable[sample];
    }

    // MAP AMPLITUDE TO CENTER AROUND 0 ===================
    for (int i = 0; i < waveTable.size(); i++)
    {
        float value = juce::jmap<float>(waveTable[i], min, max, 0.5f * mDepth, -0.5f * mDepth);
        waveTable.set(fmod(i, waveTable.size()), value);
    }

    return waveTable;
}

/* Generate paths for LFO when new information is available */
// ========================================================
void ScopeChannel::generateLFOPathForDrawing(   juce::Path &lfoStrokePath,
                                                juce::Path &lfoFillPath,
                                                juce::Array<float> &waveTable,
                                                LFO& lfo)
{

    //WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    using namespace juce;

    auto bounds = getLocalBounds();
    
    auto lfoWaveMultiplier = localLFO.getWaveMultiplier();
    auto lfoWaveRate = localLFO.getWaveRate();
    auto lfoHostBPM = localLFO.getHostBPM();
    auto lfoRelativePhase = localLFO.getRelativePhaseInSamples();
    auto lfoSync = localLFO.isSyncedToHost();
    
    /* Vertical Midpoint of Drawing Region */
    float midY = ((float)bounds.getY() + bounds.getHeight() / 2.f);

    /* Add 3px Vertical Margin */
    bounds.reduce(0, 3);

    /* Clear Previous Path Data */
    lfoFillPath.clear();
    lfoStrokePath.clear();

    // Width of Area to Render Paint Data
    auto paintAreaWidth = playBackWidth;

    // Scale WaveTable by Multiplier, or by Rate, but not Both
    const bool sync = lfoSync;
    const float scalar = sync ? (float) lfoWaveMultiplier : lfoWaveRate * 60.f / lfoHostBPM;

    // Match Width of 1x Multiplier to Width of One Quarter-Note
    const float increment = scalar * waveTable.size() / (beatSpacing);

    // [Claude 2026-10-29] Free rate (Joe: changing the rate "goes haywire"): the wave's start (t = 0) is the scope's
    // origin, the centre gridline (point + paintAreaWidth, panned with the scroll bar), with the band's phase there,
    // so a rate change squeezes or stretches the wave evenly about the centre. Before, its phase was counted from
    // point, two playback widths off screen to the left, so each small rate change slid the visible wave by many
    // cycles. Synced waves already meet the origin at their phase (whole cycles from point) and are drawn as before.
    // The window is the same span of beats as the synced view, at the host tempo, for every band.
    const float pixelsPerCycle = increment > 0.f ? (float) waveTable.size() / increment : 1.0e6f;

    // [Claude 2026-10-29] Four points per pixel, so a square's edges draw upright instead of as a one-pixel slant
    // (Joe: "is there any way to get these square waves cleaner?"); more when the cycles are short, so every cycle
    // gets at least 16 points (no moire), up to a cap.
    const int pointsPerPixel = juce::jlimit(4, 64, (int) std::ceil(16.f / juce::jmax(0.01f, pixelsPerCycle)));

    // [Claude 2026-10-29] Denser than 3 pixels a cycle a line only scribbles: draw each pixel column's lowest to
    // highest value as one filled band, which reads as fast oscillation.
    const bool dense = pixelsPerCycle < 3.f;
    juce::Array<float> columnMin, columnMax;

    // Get Relative Phase Shift.
    // [Claude 2026-10-27] No more divide by 2: LFO::setTempo now gives the phase in wavetable samples, so the
    // scope draws exactly what it drew before, and the audio now matches it.
    const auto mRelativePhase = lfoRelativePhase / waveTableDownSampleSize;

    // Get Display Phase Shift
    const float mDisplayPhase = scrollCenter * bounds.getWidth();

    // Display Offset Establishes Wave Starting Point (In-Line with Grid)
    const float displayOffsetStart = bounds.getX() + bounds.getCentreX() - paintAreaWidth;

    // Incorporate Pan Shift
    const int point = displayOffsetStart + mDisplayPhase;

    const float tableSize = (float) waveTable.size();

    for (int j = 0; j <= 3 * paintAreaWidth * pointsPerPixel; j++)
    {
        const float i = j / (float) pointsPerPixel;
        const float x = point + i;

        if (x < (float) bounds.getX() || x > (float) bounds.getRight())
            continue;

        // Index sweeps through WaveTable Array
        float index;
        if (sync)
            index = fmod(i * increment + mRelativePhase, tableSize);
        else
        {
            index = fmod((i - (float) paintAreaWidth) * increment + mRelativePhase, tableSize);
            if (index < 0.f) index += tableSize;
        }

        // Get value in terms of pixels.  Offset by Region's Midpoint
        float y = midY + waveTable[(size_t) index] * (float)bounds.getHeight();

        if (dense)
        {
            const int column = juce::jmin((int) (x - (float) bounds.getX()), bounds.getWidth() - 1);
            while (columnMin.size() <= column) { columnMin.add(1.0e9f); columnMax.add(-1.0e9f); }
            columnMin.set(column, juce::jmin(columnMin[column], y));
            columnMax.set(column, juce::jmax(columnMax[column], y));
            continue;
        }

        /* Of the calculated points, only render those which will be onscreen */
        if (x == (float) bounds.getX())
        {
            lfoStrokePath.startNewSubPath(bounds.getX(), y);
            lfoFillPath.startNewSubPath(bounds.getX(), midY);
            lfoFillPath.lineTo(bounds.getX(), y);
        }

        if (x > bounds.getX() && x < bounds.getRight())
        {
            lfoStrokePath.lineTo(x, y);
            lfoFillPath.lineTo(x, y);
        }

        if (x == (float) bounds.getRight())
        {
            lfoFillPath.lineTo(bounds.getRight(), y);
            lfoFillPath.lineTo(bounds.getRight(), midY);
        }
    }

    // The dense band: along the top edge's highest values, back along the lowest; filled, and its outline stroked
    if (dense && columnMin.size() > 0)
    {
        const float x0 = (float) bounds.getX();
        lfoStrokePath.startNewSubPath(x0, columnMin[0]);
        for (int c = 0; c < columnMin.size(); ++c)
        {
            lfoStrokePath.lineTo(x0 + c, columnMin[c]);
            lfoStrokePath.lineTo(x0 + c + 1.f, columnMin[c]);
        }
        for (int c = columnMax.size(); --c >= 0;)
        {
            lfoStrokePath.lineTo(x0 + c + 1.f, columnMax[c]);
            lfoStrokePath.lineTo(x0 + c, columnMax[c]);
        }
        lfoStrokePath.closeSubPath();
        lfoFillPath = lfoStrokePath;
    }
}

/* Check bypass state */
// ========================================================
void ScopeChannel::updateBandBypass()
{

    //WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    using namespace Params;             // Create a Local Reference to Parameter Mapping
    const auto& params = GetParams();   // Create a Local Reference to Parameter Mapping
    
    if (lfo.getLfoID() == 0)
        isBandBypassed = *apvts.getRawParameterValue(params.at(Names::Bypass_Low_Band));

    if (lfo.getLfoID() == 1)
        isBandBypassed = *apvts.getRawParameterValue(params.at(Names::Bypass_Mid_Band));

    if (lfo.getLfoID() == 2)
        isBandBypassed = *apvts.getRawParameterValue(params.at(Names::Bypass_High_Band));
}

/* Called on component resize */
// ========================================================
void ScopeChannel::resized()
{
    //WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    if (scrollZoom == 0 && scrollCenter == 0)   // [Claude 2026-10-27] was 'while': if both stayed 0 it looped forever
    {
        scrollZoom = sliderScroll.getScrollZoom();
        scrollCenter = sliderScroll.getScrollCenter();
    }
}