/*
  ==============================================================================

    GlobalControls.cpp
    Created: 30 Dec 2021 11:38:20am
    Author:  Joe

  ==============================================================================
*/

#include "GlobalControls.h"

GlobalControls::GlobalControls(audio_plugin::AudioPluginAudioProcessor& p)
	:  apvts(p.apvts), audioProcessor(p)
{
    // Beware the Magic Numbers
    // ======================================
    auto windowWidthScale = 250.f / 750.f;
    auto windowHeightScale = 140.f / 515.f;
    setSize(getWidth() * windowWidthScale, getHeight() * windowHeightScale);

	addAndMakeVisible(inputGain);
	addAndMakeVisible(outputGain);
    addAndMakeVisible(lowBandControls);
    addAndMakeVisible(midBandControls);
    addAndMakeVisible(highBandControls);

    addMouseListener(this, true);
    
    lowBandControls.addMouseListener(this, true);
    midBandControls.addMouseListener(this, true);
    highBandControls.addMouseListener(this, true);

    // Replace custom 'Mode' functionality with native 'Name'
    lowBandControls.setName("LOW");
    midBandControls.setName("MID");
    highBandControls.setName("HIGH");

    lowBandControls.setMode("LOW");
    midBandControls.setMode("MID");
    highBandControls.setMode("HIGH");
    
	// Determine the Label-Meters' Pickoff Point
	inputGain.setPickOffPoint("INPUT");
	outputGain.setPickOffPoint("OUTPUT");

    inputGain.setName("INPUT");
    outputGain.setName("OUTPUT");

	makeAttachments();
    makeGainControlAttachments();
    makeTimingControlAttachments();
    makeWaveControlAttachments();

    //makeLabel(mLabelRhythm, "Rhythm");
    //makeLabel(mLabelWaveShape, "Shape");
    //makeLabel(mLabelPhase, "Phase");
    //makeLabel(mLabelDepth, "Depth");
    //makeLabel(mLabelSkew, "Skew");
    //makeLabel(mLabelBandGain, "Gain");
    

}

void GlobalControls::paint(juce::Graphics& g)
{
    paintWindowBorders(g);
    paintBandLabels(g);
}

void GlobalControls::paintWindowBorders(juce::Graphics& g)
{

    // Disperse into multiple functions
    using namespace juce;
    using namespace AllColors::GlobalControlsColors;
    using namespace FontEditor::ControlLabels;
        
    auto bounds = getLocalBounds().toFloat();

    auto controlLabelsFontJustification = FontEditor::ControlLabels::getJustification();
    auto controlLabelsFontTypeface = FontEditor::ControlLabels::getTypeface();
    auto controlLabelsFontSize = std::min(border2.getHeight(), border2.getWidth()) * 0.175f;
    auto controlLabelsFontStyle = FontEditor::ControlLabels::getFontStyle();
    auto controlLabelsFontColor = FontEditor::ControlLabels::getFontColor();
    auto controlLabelsFontTransparency = FontEditor::ControlLabels::getFontTransparency();
        
    auto myFont = juce::Font(   controlLabelsFontTypeface,
                                controlLabelsFontSize,
                                controlLabelsFontStyle);

    g.setGradientFill(BACKGROUND_GRADIENT(bounds));

    auto windowBorderColor = ColorScheme::WindowBorders::getWindowBorderColor();
    auto dividerBorderColor = ColorScheme::WindowBorders::getWindowBorderColor().withMultipliedAlpha(0.3f);

    // Beware the Magic Numbers
    // =================================
    auto x = bounds.getHeight();

    // Spacer to Draw horizontal lines between buttons / controls
    auto spacerHeightScale = 5.f / 250.f;
    auto spacerHeight = bounds.getHeight() * spacerHeightScale;

    // Shape
    // ==================================================================
    auto shapeBorder = labelBorder;
    shapeBorder.setY(lowBandControls.getWaveBounds().getY() - spacerHeight / 2);
    paintBorder(g, dividerBorderColor, shapeBorder);
    shapeBorder.setWidth(border2.getWidth());
    g.setColour(controlLabelsFontColor);
    g.setOpacity(controlLabelsFontTransparency);
    g.setFont(myFont);
    g.drawFittedText("Shape", shapeBorder.toNearestInt(), juce::Justification::centred, 1);

    // Skew
    // ==================================================================
    auto skewBorder = labelBorder;
    skewBorder.setY(lowBandControls.getSkewBounds().getY() - spacerHeight / 2);
    paintBorder(g, dividerBorderColor, skewBorder);
    skewBorder.setWidth(border2.getWidth());
    g.setColour(controlLabelsFontColor);
    g.setOpacity(controlLabelsFontTransparency);
    g.setFont(myFont);
    g.drawFittedText("Skew", skewBorder.toNearestInt(), juce::Justification::centred, 1);
    
    // Depth
    // ==================================================================
    auto depthBorder = labelBorder;
    depthBorder.setY(lowBandControls.getDepthBounds().getY() - spacerHeight / 2);
    paintBorder(g, dividerBorderColor, depthBorder);
    depthBorder.setWidth(border2.getWidth());
    g.setColour(controlLabelsFontColor);
    g.setOpacity(controlLabelsFontTransparency);
    g.setFont(myFont);
    g.drawFittedText("Depth", depthBorder.toNearestInt(), juce::Justification::centred, 1);

    // Rhythm
    // ==================================================================
    auto rhythmBorder = labelBorder;
    rhythmBorder.setY(lowBandControls.getRhythmBounds().getY() - spacerHeight / 2);
    paintBorder(g, dividerBorderColor, rhythmBorder);
    rhythmBorder.setWidth(border2.getWidth());
    g.setColour(controlLabelsFontColor);
    g.setOpacity(controlLabelsFontTransparency);
    g.setFont(myFont);
    g.drawFittedText("Rhythm", rhythmBorder.toNearestInt(), juce::Justification::centred, 1);
    
    // Phase
    // ==================================================================
    auto phaseBorder = labelBorder;
    phaseBorder.setY(lowBandControls.getPhaseBounds().getY() - spacerHeight / 2);
    paintBorder(g, dividerBorderColor, phaseBorder);
    phaseBorder.setWidth(border2.getWidth());
    g.setColour(controlLabelsFontColor);
    g.setOpacity(controlLabelsFontTransparency);
    g.setFont(myFont);
    g.drawFittedText("Phase", phaseBorder.toNearestInt(), juce::Justification::centred, 1);

    // Gain
    // ==================================================================
    auto gainBorder = labelBorder;
    gainBorder.setY(lowBandControls.getGainBounds().getY() - spacerHeight / 2);
    paintBorder(g, dividerBorderColor, gainBorder);
    gainBorder.setWidth(border2.getWidth());
    g.setColour(controlLabelsFontColor);
    g.setOpacity(controlLabelsFontTransparency);
    g.setFont(myFont);
    g.drawFittedText("Gain", gainBorder.toNearestInt(), juce::Justification::centred, 1);
    
    // Input Gain Border
    // ==================================================================
    paintBorder(g, windowBorderColor, border1);

    // Text Labels Border
    // ==================================================================
    paintBorder(g, windowBorderColor, border2);
    
    // Low-Band Border
    // ==================================================================
    paintBorder(g, windowBorderColor, border3);
    
    // Mid-Band Border
    // ==================================================================
    paintBorder(g, windowBorderColor, border4);
    
    // High-Band Border
    // ==================================================================
    paintBorder(g, windowBorderColor, border5);
    
    // Output Gain Border
    // ==================================================================
    paintBorder(g, windowBorderColor, border6);
}

void GlobalControls::paintBandLabels(juce::Graphics& g)
{
    using namespace FontEditor;
 
    auto bounds = getLocalBounds();
    
    auto controlLabelsFontTypeface = FontEditor::ControlLabels::getTypeface();

    // Beware of Magic Numbers
    // ==================================
    auto controlLabelsFontSize = 23.f * (getHeight() / 250.f);

    auto controlLabelsFontStyle = juce::Font::FontStyleFlags::bold;
    auto controlLabelsFontColor = juce::Colours::white.withAlpha(0.65f);

    g.setColour(controlLabelsFontColor);

    auto titleFont = juce::Font(    controlLabelsFontTypeface,
                                    controlLabelsFontSize,
                                    controlLabelsFontStyle);
    
    int x = border3.getX();
    int height = getHeight() * 0.155f;
    int y = bounds.getBottom() - height;
    int width = border3.getWidth();

    
    g.setFont(titleFont);
    g.setColour(controlLabelsFontColor);
    
    auto lowLabelBounds = juce::Rectangle<int>{x, y, width, height};
    g.drawFittedText("LOW", lowLabelBounds, juce::Justification::centred, 1);
    
    x = border4.getX();
    
    auto midLabelBounds = juce::Rectangle<int>{x, y, width, height};
    g.drawFittedText("MID", midLabelBounds, juce::Justification::centred, 1);
    
    x = border5.getX();
    
    auto highLabelBounds = juce::Rectangle<int>{x, y, width, height};
    g.drawFittedText("HIGH", highLabelBounds, juce::Justification::centred, 1);
}

void GlobalControls::resized()
{
    using namespace juce;

    auto bounds = getLocalBounds();

    // Beware the Magic Numbers
    // ======================================
    auto meterWidthScale = 32.f / 750.f;
    auto meterWidth = bounds.getWidth() * meterWidthScale;

    // Beware the Magic Numbers
    // ======================================
    auto paddingWidthScale = 4.f / 750.f;
    auto padding = getWidth() * paddingWidthScale;
    
    // Beware the Magic Numbers
    // ======================================
    auto labelWidthScale = 75.f / 750.f;
    auto labelWidth = bounds.getWidth() * labelWidthScale;

    auto bandWidth = (getWidth() - labelWidth - meterWidth*2)/3.f;
    
    border1.setBounds(    bounds.getX(),        bounds.getY(), meterWidth,              bounds.getHeight());
    border2.setBounds(    border1.getRight(),   bounds.getY(), labelWidth,              bounds.getHeight());
    border3.setBounds(    border2.getRight(),   bounds.getY(), bandWidth,               bounds.getHeight());
    border4.setBounds(    border3.getRight(),   bounds.getY(), bandWidth,               bounds.getHeight());
    border5.setBounds(    border4.getRight(),   bounds.getY(), bandWidth,               bounds.getHeight());
    border6.setBounds(    bounds.getRight() - meterWidth, bounds.getY(), meterWidth,    bounds.getHeight());
   

    inputGain.setBounds(    border1.getX() + padding,
                            border1.getY() + padding,
                            border1.getWidth() - 2 * padding,
                            border1.getHeight() - 2 * padding);

    lowBandControls.setBounds( border3.getX() + padding,
                              border3.getY() + padding,
                              border3.getWidth() - 2 * padding,
                              border3.getHeight() - 2 * padding);
    
    midBandControls.setBounds( border4.getX() + padding,
                              border4.getY() + padding,
                              border4.getWidth() - 2 * padding,
                              border4.getHeight() - 2 * padding);
    
    highBandControls.setBounds( border5.getX() + padding,
                               border5.getY() + padding,
                               border5.getWidth() - 2 * padding,
                               border5.getHeight() - 2 * padding);
    
    
    outputGain.setBounds(   border6.getX() + padding,
                            border6.getY() + padding,
                            border6.getWidth() - 2 * padding,
                            border6.getHeight() - 2 * padding);

    /* Create the Full Control Layout */
    //============================================================

    auto labelBoundsWidthReductionScale = 5.f / 750.f;
    auto labelBoundsHeightReductionScale = 25.f / 515.f;

    auto labelBoundsWidthReduction = getWidth() * labelBoundsWidthReductionScale;
    auto labelBoundsHeightReduction = getHeight() * labelBoundsHeightReductionScale;

    auto labelBounds = border2.reduced(labelBoundsWidthReduction + padding, labelBoundsHeightReduction + padding);
    
    //makeLabel(mLabelRhythm, "Rhythm");
    //makeLabel(mLabelWaveShape, "Shape");
    //makeLabel(mLabelPhase, "Phase");
    //makeLabel(mLabelDepth, "Depth");
    //makeLabel(mLabelSkew, "Skew");
    //makeLabel(mLabelBandGain, "Gain");

    //mLabelWaveShape.setCentrePosition(labelBounds.getCentreX(), lowBandControls.getWaveBounds().getCentreY() + padding);
    //mLabelSkew.setCentrePosition(labelBounds.getCentreX(), lowBandControls.getSkewBounds().getCentreY() + padding);
    //mLabelDepth.setCentrePosition(labelBounds.getCentreX(), lowBandControls.getDepthBounds().getCentreY() + padding);
    //mLabelRhythm.setCentrePosition(labelBounds.getCentreX(), lowBandControls.getRhythmBounds().getCentreY() + padding);
    //mLabelPhase.setCentrePosition(labelBounds.getCentreX(), lowBandControls.getPhaseBounds().getCentreY() + padding);
    //mLabelBandGain.setCentrePosition(labelBounds.getCentreX(), lowBandControls.getGainBounds().getCentreY() + padding);
    
    //labelBorder.setBounds(border2.getX(), mLabelWaveShape.getY()-20, getWidth()-meterWidth*2, mLabelWaveShape.getHeight()+10);

    // Spacer to Draw horizontal lines between buttons / controls
    auto spacerHeightScale = 5.f / 250.f;
    auto spacerHeight = bounds.getHeight() * spacerHeightScale;

    auto labelBorderX = border2.getX();
    auto labelBorderY = border2.getY();
    auto labelBorderWidth = getWidth() - meterWidth * 2;
    auto labelBorderHeight = lowBandControls.getWaveBounds().getHeight() + spacerHeight * 2;

    labelBorder.setBounds(labelBorderX, labelBorderY, labelBorderWidth, labelBorderHeight);





}

void GlobalControls::makeAttachments()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    using namespace Params;
    const auto& params = GetParams();

    inputGainAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Input_Gain),
                            inputGain.sliderGain);

    outputGainAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Output_Gain),
                            outputGain.sliderGain);
}

//void GlobalControls::makeLabel(juce::Label& label, juce::String textToPrint)
//{
//    using namespace FontEditor::ControlLabels;
//    
//    auto controlLabelsFontJustification = FontEditor::ControlLabels::getJustification();
//    auto controlLabelsFontTypeface = FontEditor::ControlLabels::getTypeface();
//    auto controlLabelsFontSize = FontEditor::ControlLabels::getFontSize();
//    auto controlLabelsFontStyle = FontEditor::ControlLabels::getFontStyle();
//    auto controlLabelsFontColor = FontEditor::ControlLabels::getFontColor();
//    auto controlLabelsFontTransparency = FontEditor::ControlLabels::getFontTransparency();
//    
//    label.setJustificationType(controlLabelsFontJustification);
//    
//    auto myFont = juce::Font(   controlLabelsFontTypeface,
//                                controlLabelsFontSize,
//                                controlLabelsFontStyle);
//    
//    label.setFont(myFont);
//    label.setAlpha(controlLabelsFontTransparency);
//    label.setColour(juce::Label::textColourId, controlLabelsFontColor);
//    
//    label.setText(textToPrint, juce::NotificationType::dontSendNotification);
//
//    // Beware the Magic Numbers
//    // ======================================
//    auto labelWidthScale = 50.f / 750.f;
//    auto labelHeightScale = 20.f / 515.f;
//    label.setSize(getWidth() * labelWidthScale, getHeight() * labelHeightScale);
//    
//    addAndMakeVisible(label);
//}

void GlobalControls::makeGainControlAttachments()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    using namespace Params;
    const auto& params = GetParams();

    // GAIN =====================
    gainLowAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Gain_Low_Band),
                            lowBandControls.mSliderBandGain.slider);
    

    gainMidAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Gain_Mid_Band),
                            midBandControls.mSliderBandGain.slider);

    gainHighAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Gain_High_Band),
                            highBandControls.mSliderBandGain.slider);

    // SOLO =====================
    soloLowAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Solo_Low_Band),
                            lowBandControls.mToggleSolo.mToggleButton);

    soloMidAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Solo_Mid_Band),
                            midBandControls.mToggleSolo.mToggleButton);

    soloHighAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Solo_High_Band),
                            highBandControls.mToggleSolo.mToggleButton);

    // BYPASS =====================
    bypassLowAttachment =   std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Bypass_Low_Band),
                            lowBandControls.mToggleBypass.mToggleButton);

    bypassMidAttachment =   std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Bypass_Mid_Band),
                            midBandControls.mToggleBypass.mToggleButton);

    bypassHighAttachment =   std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Bypass_High_Band),
                            highBandControls.mToggleBypass.mToggleButton);

    // MUTE =====================
    muteLowAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Mute_Low_Band),
                            lowBandControls.mToggleMute.mToggleButton);

    muteMidAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Mute_Mid_Band),
                            midBandControls.mToggleMute.mToggleButton);

    muteHighAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Mute_High_Band),
                            highBandControls.mToggleMute.mToggleButton);
}

void GlobalControls::makeTimingControlAttachments()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    using namespace Params;
    const auto& params = GetParams();

    // SYNC TO HOST ================
    syncLowAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Sync_Low_LFO),
                            lowBandControls.mToggleSync.mToggleButton);

    syncMidAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Sync_Mid_LFO),
                            midBandControls.mToggleSync.mToggleButton);

    syncHighAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Sync_High_LFO),
                            highBandControls.mToggleSync.mToggleButton);

    // RATE ================================
    rateLowAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Rate_Low_LFO),
                            lowBandControls.mSliderRate.slider);

    rateMidAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Rate_Mid_LFO),
                            midBandControls.mSliderRate.slider);

    rateHighAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Rate_High_LFO),
                            highBandControls.mSliderRate.slider);

    // MULTIPLIER AKA RHYTHM ================================
    multLowAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                            apvts,
                            params.at(Names::Multiplier_Low_LFO),
                            lowBandControls.mDropRhythm);

    multMidAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                            apvts,
                            params.at(Names::Multiplier_Mid_LFO),
                            midBandControls.mDropRhythm);

    multHighAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                            apvts,
                            params.at(Names::Multiplier_High_LFO),
                            highBandControls.mDropRhythm);

    // PHASE ================================
    phaseLowAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Relative_Phase_Low_LFO),
                            lowBandControls.mSliderPhase.slider);

    phaseMidAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Relative_Phase_Mid_LFO),
                            midBandControls.mSliderPhase.slider);

    phaseHighAttachment =   std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Relative_Phase_High_LFO),
                            highBandControls.mSliderPhase.slider);
}

void GlobalControls::makeWaveControlAttachments()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    using namespace Params;
    const auto& params = GetParams();

    // WAVESHAPE ===================
    waveLowAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                            apvts,
                            params.at(Names::Wave_Low_LFO),
                            lowBandControls.mDropWaveshape);

    waveMidAttachment =     std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                            apvts,
                            params.at(Names::Wave_Mid_LFO),
                            midBandControls.mDropWaveshape);

    waveHighAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                            apvts,
                            params.at(Names::Wave_High_LFO),
                            highBandControls.mDropWaveshape);

    // DEPTH ===================
    depthLowAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Depth_Low_LFO),
                            lowBandControls.mSliderDepth.slider);

    depthMidAttachment =    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Depth_Mid_LFO),
                            midBandControls.mSliderDepth.slider);

    depthHighAttachment =   std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Depth_High_LFO),
                            highBandControls.mSliderDepth.slider);

    // SYMMETRY AKA SKEW ===================
    symmetryLowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Symmetry_Low_LFO),
                            lowBandControls.mSliderSkew.slider);

    symmetryMidAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Symmetry_Mid_LFO),
                            midBandControls.mSliderSkew.slider);

    symmetryHighAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                            apvts,
                            params.at(Names::Symmetry_High_LFO),
                            highBandControls.mSliderSkew.slider);

    // INVERT ===================
    invertLowAttachment =   std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Invert_Low_LFO),
                            lowBandControls.mToggleInvert.mToggleButton);

    invertMidAttachment =   std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Invert_Mid_LFO),
                            midBandControls.mToggleInvert.mToggleButton);

    invertHighAttachment =  std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                            apvts,
                            params.at(Names::Invert_High_LFO),
                            highBandControls.mToggleInvert.mToggleButton);
}

void GlobalControls::sendBroadcast(juce::String parameterName, juce::String parameterValue)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    juce::String delimiter = ":::::";
    juce::String bandName = "xxxxx";
    auto message = bandName + delimiter + parameterName.paddedLeft('x', 10) + delimiter + parameterValue.paddedLeft('x', 10);
    sendActionMessage(message);
}

void GlobalControls::mouseEnter(const juce::MouseEvent& event)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    checkForBandFocus();
}

void GlobalControls::mouseExit(const juce::MouseEvent& event)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    checkForBandFocus();
}

void GlobalControls::checkForBandFocus()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    auto whoHasFocus = "NONE";

    if (lowBandControls.isMouseOver(true))
        whoHasFocus = "LOW";

    if (midBandControls.isMouseOver(true))
        whoHasFocus = "MID";

    if (highBandControls.isMouseOver(true))
        whoHasFocus = "HIGH";

    sendBroadcast("FOCUS", whoHasFocus);

}