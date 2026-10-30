/*
  ==============================================================================

    BandControls.cpp
    Created: 29 Oct 2022 8:41:50pm
    Author:  Joe Caulfield

  ==============================================================================
*/

#include "BandControls.h"

/* Constructor */
//============================================================
BandControl::BandControl(juce::AudioProcessorValueTreeState& apv)
    : apvts(apv)
{

    buildToggleSync();
    buildToggleInvert();
    buildRateOrRhythm();
    buildMenuWaveshape();
    buildSliderPhase();
    buildSliderSkew();
    buildSliderDepth();
    buildSliderBandGain();
    buildToggleBypass();
    buildToggleSolo();
    buildToggleMute();
    
    runOnceOnConstructor = false;

    // [Claude 2026-10-29] No longer buffered to an image: at 125 and 150 % the cached picture was laid back down at a
    // fractional screen position and resampled, which blurred every key, menu and slider in the column (their edges
    // smeared over three rows at 125 %) and left odd pixels at its border, and it defeated the screen-pixel snapping
    // inside, which assumed the column's real origin. The column is cheap to paint directly.
}

BandControl::~BandControl()
{
    mToggleSync.setLookAndFeel(nullptr);
    mToggleInvert.setLookAndFeel(nullptr);
    
    mDropRhythm.setLookAndFeel(nullptr);
    mDropWaveshape.setLookAndFeel(nullptr);
}

/* Called on repaint */
//============================================================
void BandControl::paint(juce::Graphics& g)
{
    // [Claude 2026-10-29] the column's shade is painted by GlobalControls over the whole column now; filling only this
    // inset area left a faint edge just inside the bevel (Joe: "Don't need them")
}

/* Called on resize */
//============================================================
void BandControl::resized()
{
    updateFlexBox();
}

void BandControl::updateFlexBox()
{
    using namespace juce;

    auto bounds = getLocalBounds();
    
    // Beware the Magic Numbers
    // ======================================
    auto spacerHeightScale = 10.f / 250.f;
    auto spacerWidthScale = 4.f / 250.f;

    auto columnItemHeightScale = 20.f / 250.f;
    auto buttonRowHeightScale = 30.f / 250.f;

    auto spacerWidth = bounds.getWidth() * spacerWidthScale;

    auto componentBounds = bounds.reduced(spacerWidth, spacerWidth);
    
    /* Create the Waveshape Row */
    //============================================================

    auto invScale = 40.f / 193.f;
    auto invWidth = bounds.getWidth() * invScale;

    FlexBox waveShapeRow;
    waveShapeRow.flexDirection = FlexBox::Direction::row;
    waveShapeRow.flexWrap = FlexBox::Wrap::noWrap;
    
    waveShapeRow.items.add(FlexItem(mDropWaveshape).withFlex(1.f));
    waveShapeRow.items.add(FlexItem().withWidth(spacerWidth));
    waveShapeRow.items.add(FlexItem(mToggleInvert).withWidth(invWidth));


    /* Create the Button Row */
    //============================================================
    FlexBox buttonRow;
    buttonRow.flexDirection = FlexBox::Direction::row;
    buttonRow.flexWrap = FlexBox::Wrap::noWrap;
    buttonRow.items.add(FlexItem(mToggleSync).withFlex(1.f));
    buttonRow.items.add(FlexItem(mToggleBypass).withFlex(1.f));
    buttonRow.items.add(FlexItem(mToggleSolo).withFlex(1.f));
    buttonRow.items.add(FlexItem(mToggleMute).withFlex(1.f));

    /* Create the Full Control Layout */
    //============================================================
    FlexBox componentColumn;
    componentColumn.flexDirection = FlexBox::Direction::column;
    componentColumn.flexWrap = FlexBox::Wrap::noWrap;

    //// Beware the Magic Numbers
    //// ======================================
    //auto spacerHeightScale = 10.f / 250.f;
    //auto columnItemHeightScale = 20.f / 250.f;
    //auto buttonRowHeightScale = 30.f / 250.f;

    auto x = bounds.getHeight();

    // [Claude 2026-10-29] Rows sized from the window's width, not this column's height, and half the gap between them,
    // so the window can be shorter without the controls shrinking (Joe: "reduce all of the padding a bit")
    const float H = 250.f;   // [Claude 2026-10-29] the layout is always 750 wide now; the window scales it (was from the top-level width)
    auto spacer = FlexItem().withHeight(H * spacerHeightScale * 0.5f);

    // [Claude 2026-10-29] The sliders a little shorter than their row, centred in it, so the rows keep their spacing and
    // line up with the labels (Joe: "making the sliders slightly shorter in height")
    const float rowH = H * columnItemHeightScale, sliderH = rowH * 0.72f;
    auto sliderItem = [&](juce::Component& c) { return FlexItem(c).withHeight(sliderH).withMargin(FlexItem::Margin((rowH - sliderH) / 2.f, 0.f, (rowH - sliderH) / 2.f, 0.f)); };
    // and the menus and keys the same way (Joe: "i meant the dropdowns (and buttons) too")
    auto sliderItemFlex = [&](juce::FlexBox& f, float h) { const float hh = h * 0.72f; return FlexItem(f).withHeight(hh).withMargin(FlexItem::Margin((h - hh) / 2.f, 0.f, (h - hh) / 2.f, 0.f)); };

    componentColumn.items.add(sliderItemFlex(waveShapeRow, rowH));
    componentColumn.items.add(spacer);
    componentColumn.items.add(sliderItem(mSliderSkew));
    componentColumn.items.add(spacer);
    componentColumn.items.add(sliderItem(mSliderDepth));
    componentColumn.items.add(spacer);
    componentColumn.items.add(sliderItem(*placeholder));
    componentColumn.items.add(spacer);
    componentColumn.items.add(sliderItem(mSliderPhase));
    componentColumn.items.add(spacer);
    componentColumn.items.add(sliderItem(mSliderBandGain));
    componentColumn.items.add(spacer);
    componentColumn.items.add(sliderItemFlex(buttonRow, H * buttonRowHeightScale));
    
    componentColumn.performLayout(componentBounds);

}

void BandControl::buildToggleSync()
{
    mToggleSync.mToggleButton.addListener(this);
    addAndMakeVisible(mToggleSync);
    
    // Broadcast Sync Changes to Oscilloscope
    mToggleSync.mToggleButton.onClick = [this]()
    {
        auto paramName = "SYNC";
        auto paramValue = mToggleSync.mToggleButton.getToggleStateValue().toString();

        sendBroadcast(paramName, paramValue);
    };
}

void BandControl::buildToggleInvert()
{
    mToggleInvert.mToggleButton.addListener(this);
    addAndMakeVisible(mToggleInvert);
    
    // Broadcast Invert Changes to Oscilloscope
    mToggleInvert.mToggleButton.onClick = [this]()
    {
        auto paramName = "INVERT";
        auto paramValue = mToggleInvert.mToggleButton.getToggleStateValue().toString();

        sendBroadcast(paramName, paramValue);
    };
}

void BandControl::buildRateOrRhythm()
{
    using namespace Params;
    const auto& params = GetParams();
 
    if (runOnceOnConstructor)
        mDropRhythm.addItemList(apvts.getParameter(params.at(Names::Multiplier_Low_LFO))->getAllValueStrings(), 1);
    
    mSliderRate.setStyleStandard(" Hz");
    
    mDropRhythm.setAlpha(0.85f);
    
    addAndMakeVisible(mDropRhythm);
    addAndMakeVisible(mSliderRate);
    
    // [Claude 2026-10-29] Both always broadcast. Each was wired only in the other's branch (the rate slider's only
    // while synced, when it is hidden), so a band that opened unsynced had a rate slider the scope never heard (Joe:
    // "on first startup, the low channel initiated to not-synced. But the frequency slider did not do anything").
    mSliderRate.slider.onValueChange = [this]()
    {
        sendBroadcast("RATE", (juce::String)mSliderRate.slider.getValue());
    };
    mDropRhythm.onChange = [this]()
    {
        sendBroadcast("RHYTHM", (juce::String)mDropRhythm.getSelectedItemIndex());
    };

    if (isSyncedToHost)
    {
        mDropRhythm.setLookAndFeel(&multComboLookAndFeel);
        mDropRhythm.setTextWhenNothingSelected("Rhythm");

        placeholder = &mDropRhythm;
        mDropRhythm.setVisible(true);
        mSliderRate.setVisible(false);
    }
    else
    {
        placeholder = &mSliderRate;
        mDropRhythm.setVisible(false);
        mSliderRate.setVisible(true);
    }


}

void BandControl::buildMenuWaveshape()
{
    using namespace Params;
    const auto& params = GetParams();
    
    if (runOnceOnConstructor)
        mDropWaveshape.addItemList(apvts.getParameter(params.at(Names::Wave_Low_LFO))->getAllValueStrings(), 1);
       
    mDropWaveshape.setLookAndFeel(&waveComboLookAndFeel);
    mDropWaveshape.setTextWhenNothingSelected("Waveshape");
    mDropWaveshape.setAlpha(0.85f);
    addAndMakeVisible(mDropWaveshape);
    
    // Broadcast LFO Changes to Oscilloscope
    mDropWaveshape.onChange = [this]()
    {
        auto paramName = "WAVESHAPE";

        auto paramValue = (juce::String)mDropWaveshape.getSelectedItemIndex();

        sendBroadcast(paramName, paramValue);
    };
    
}

void BandControl::buildSliderPhase()
{
    mSliderPhase.setStyleCenter(juce::CharPointer_UTF8("\xc2\xb0"));
    addAndMakeVisible(mSliderPhase);
    
    // Broadcast LFO Changes to Oscilloscope
    mSliderPhase.slider.onValueChange = [this]()
    {
        auto paramName = "PHASE";
        auto paramValue = (juce::String)mSliderPhase.slider.getValue();

        sendBroadcast(paramName, paramValue);
    };
    
}

void BandControl::buildSliderSkew()
{
    mSliderSkew.setStyleCenter("%");
    addAndMakeVisible(mSliderSkew);
    
    // Broadcast Skew Changes to Oscilloscope
    mSliderSkew.slider.onValueChange = [this]()
    {
            auto paramName = "SKEW";
            auto paramValue = (juce::String)mSliderSkew.slider.getValue();

            sendBroadcast(paramName, paramValue);
    };
}

void BandControl::buildSliderDepth()
{
    mSliderDepth.setStyleStandard("%");
    addAndMakeVisible(mSliderDepth);
    
    // Broadcast Skew Changes to Oscilloscope
    mSliderDepth.slider.onValueChange = [this]()
    {
            auto paramName = "DEPTH";
            auto paramValue = (juce::String)mSliderDepth.slider.getValue();

            sendBroadcast(paramName, paramValue);
    };
    
}

void BandControl::buildSliderBandGain()
{
    mSliderBandGain.setStyleStandard(" dB");
    addAndMakeVisible(mSliderBandGain);
    
    // Broadcast Skew Changes to Oscilloscope
    mSliderBandGain.slider.onValueChange = [this]()
    {
            auto paramName = "GAIN";
            auto paramValue = (juce::String)mSliderBandGain.slider.getValue();

            sendBroadcast(paramName, paramValue);
    };
}

void BandControl::buildToggleBypass()
{
    mToggleBypass.mToggleButton.addListener(this);
    addAndMakeVisible(mToggleBypass);
    
    // Broadcast Bypass Changes to Oscilloscope
    mToggleBypass.mToggleButton.onClick = [this]()
    {
            auto paramName = "BYPASS";
            auto paramValue = mToggleBypass.mToggleButton.getToggleStateValue().toString();

            sendBroadcast(paramName, paramValue);
    };
    
}

void BandControl::buildToggleSolo()
{
    mToggleSolo.mToggleButton.addListener(this);
    addAndMakeVisible(mToggleSolo);
    
    // Broadcast Solo Changes to Oscilloscope
    mToggleSolo.mToggleButton.onClick = [this]()
    {
            auto paramName = "SOLO";
            auto paramValue = mToggleSolo.mToggleButton.getToggleStateValue().toString();

            sendBroadcast(paramName, paramValue);
    };
    
}

void BandControl::buildToggleMute()
{
    mToggleMute.mToggleButton.addListener(this);
    addAndMakeVisible(mToggleMute);
    
    // Broadcast Mute Changes to Oscilloscope
    mToggleMute.mToggleButton.onClick = [this]()
    {
            auto paramName = "MUTE";
            auto paramValue = mToggleMute.mToggleButton.getToggleStateValue().toString();

            sendBroadcast(paramName, paramValue);
    };
}

void BandControl::setMode(juce::String bandMode)
{
    if (bandMode == "LOW") mode = 0;

    if (bandMode == "MID") mode = 1;

    if (bandMode == "HIGH") mode = 2;

    bandModeName = bandMode;
    
    mSliderPhase.setBandMode(mode);
    mSliderRate.setBandMode(mode);
    mSliderSkew.setBandMode(mode);
    mSliderDepth.setBandMode(mode);
    mSliderBandGain.setBandMode(mode);
    waveComboLookAndFeel.setMode(mode);
    multComboLookAndFeel.setMode(mode);
    mToggleSync.setBandMode(mode, "SYNC");
    mToggleInvert.setBandMode(mode, "INV");
    mToggleBypass.setBandMode(mode, "BYP");
    mToggleSolo.setBandMode(mode, "SOLO");
    mToggleMute.setBandMode(mode, "MUTE");

    // [Claude 2026-10-29] Names for screen readers and UI Automation ("Low Depth", "Mid Sync"...): the keys and sliders
    // had none. Only their accessible names; nothing drawn or done changes. The Windows smoke test finds them by these.
    const juce::String band = mode == 0 ? "Low" : mode == 1 ? "Mid" : "High";
    for (auto [c, what] : { std::pair<juce::Component*, const char*> { &mSliderDepth.slider, "Depth" }, { &mSliderSkew.slider, "Skew" },
                            { &mSliderRate.slider, "Rate" }, { &mSliderPhase.slider, "Phase" }, { &mSliderBandGain.slider, "Gain" },
                            { &mToggleSync.mToggleButton, "Sync" }, { &mToggleInvert.mToggleButton, "Invert" },
                            { &mToggleBypass.mToggleButton, "Bypass" }, { &mToggleSolo.mToggleButton, "Solo" },
                            { &mToggleMute.mToggleButton, "Mute" }, { &mDropWaveshape, "Shape" }, { &mDropRhythm, "Rhythm" } })
        c->setTitle(band + " " + what);
    
    
}

void BandControl::buttonClicked(juce::Button* button)
{
    if (button == &mToggleSync.mToggleButton)
    {
        // [Claude 2026-10-29] from the key's state, not flipped: also right when a preset or session sets it
        isSyncedToHost = mToggleSync.mToggleButton.getToggleState();
        
        buildRateOrRhythm();
        updateFlexBox();
    }

    // Prevent both 'Mute' and 'Solo' from being engaged simultaneously

    if (button == &mToggleSolo.mToggleButton)
    {
        if (button->getToggleState())
            mToggleMute.mToggleButton.setToggleState(false, true);
        
        //mBsmChanged = true;
    }

    if (button == &mToggleMute.mToggleButton)
    {
        if (button->getToggleState())
            mToggleSolo.mToggleButton.setToggleState(false, true);
        
        //mBsmChanged = true;
    }
    
    //if (button == &mToggleBypass.mToggleButton)
    //{
    //    mBsmChanged = true;
    //}
    
}

// Guarantees a 30-Character long message
void BandControl::sendBroadcast(juce::String parameterName, juce::String parameterValue)
{
    juce::String delimiter = ":::::";

    juce::String bandName = bandModeName.paddedLeft('x', 5);

    auto message = bandName + delimiter + parameterName.paddedLeft('x', 10) + delimiter + parameterValue.paddedLeft('x', 10);

    sendActionMessage(message);
}

void BandControl::broadcastInitialParameters()
{
    juce::String paramName = "";
    juce::String paramValue = "";

    // [Claude 2026-10-29] Show the rate or the rhythm for the sync state the band opened with (defaults, a preset or
    // a saved session); it was a flag that started false and flipped on each click, and a state that matched the
    // default never clicked it
    if (isSyncedToHost != mToggleSync.mToggleButton.getToggleState())
    {
        isSyncedToHost = mToggleSync.mToggleButton.getToggleState();
        buildRateOrRhythm();
        updateFlexBox();
    }

    paramName = "SYNC";
    paramValue = mToggleSync.mToggleButton.getToggleStateValue().toString();
    sendBroadcast(paramName, paramValue);

    paramName = "INVERT";
    paramValue = mToggleInvert.mToggleButton.getToggleStateValue().toString();
    sendBroadcast(paramName, paramValue);

    paramName = "RATE";
    paramValue = (juce::String)mSliderRate.slider.getValue();
    sendBroadcast(paramName, paramValue);

    paramName = "RHYTHM";
    paramValue = (juce::String)mDropRhythm.getSelectedItemIndex();
    sendBroadcast(paramName, paramValue);

    paramName = "WAVESHAPE";
    paramValue = (juce::String)mDropWaveshape.getSelectedItemIndex();
    sendBroadcast(paramName, paramValue);

    paramName = "PHASE";
    paramValue = (juce::String)mSliderPhase.slider.getValue();
    sendBroadcast(paramName, paramValue);

    paramName = "SKEW";
    paramValue = (juce::String)mSliderSkew.slider.getValue();
    sendBroadcast(paramName, paramValue);

    paramName = "DEPTH";
    paramValue = (juce::String)mSliderDepth.slider.getValue();
    sendBroadcast(paramName, paramValue);

    paramName = "GAIN";
    paramValue = (juce::String)mSliderBandGain.slider.getValue();
    sendBroadcast(paramName, paramValue);

    paramName = "BYPASS";
    paramValue = mToggleBypass.mToggleButton.getToggleStateValue().toString();
    sendBroadcast(paramName, paramValue);

    paramName = "SOLO";
    paramValue = mToggleSolo.mToggleButton.getToggleStateValue().toString();
    sendBroadcast(paramName, paramValue);

    paramName = "MUTE";
    paramValue = mToggleMute.mToggleButton.getToggleStateValue().toString();
    sendBroadcast(paramName, paramValue);

}