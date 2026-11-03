/*
  ==============================================================================

    WindowWrapperFrequency.cpp
    Created: 13 Oct 2022 11:12:35pm
    Author:  Joe Caulfield

  ==============================================================================
*/

#include "WindowWrapperOscilloscope.h"

/* Constructor */
// ========================================================
WindowWrapperOscilloscope::WindowWrapperOscilloscope(audio_plugin::AudioPluginAudioProcessor& p) :  audioProcessor(p)
{

    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(optionsMenu);
    
    buildOptionsMenuParameters();
}

/* Destructor */
// ========================================================
WindowWrapperOscilloscope::~WindowWrapperOscilloscope()
{
    using namespace Params;             // Create a Local Reference to Parameter Mapping
    const auto& params = GetParams();   // Create a Local Reference to Parameter Mapping

    audioProcessor.apvts.removeParameterListener(params.at(Names::Show_Low_Scope), this);
    audioProcessor.apvts.removeParameterListener(params.at(Names::Show_Mid_Scope), this);
    audioProcessor.apvts.removeParameterListener(params.at(Names::Show_High_Scope), this);
    audioProcessor.apvts.removeParameterListener(params.at(Names::Stack_Bands_Scope), this);
}

/* Links menu options to parameters */
// ========================================================
void WindowWrapperOscilloscope::buildOptionsMenuParameters()
{
    using namespace Params;             // Create a Local Reference to Parameter Mapping
    const auto& params = GetParams();   // Create a Local Reference to Parameter Mapping

    optionsMenu.addOptionToList("Low",
                                "Show Low Band",
                                audioProcessor.apvts,
                                params.at(Names::Show_Low_Scope));

    optionsMenu.addOptionToList("Mid",
                                "Show Mid Band",
                                audioProcessor.apvts,
                                params.at(Names::Show_Mid_Scope));

    optionsMenu.addOptionToList("High",
                                "Show High Band",
                                audioProcessor.apvts,
                                params.at(Names::Show_High_Scope));

    optionsMenu.addOptionToList("Stack Bands",
                                "Stack All Bands",
                                audioProcessor.apvts,
                                params.at(Names::Stack_Bands_Scope));

    audioProcessor.apvts.addParameterListener(params.at(Names::Show_Low_Scope), this);
    audioProcessor.apvts.addParameterListener(params.at(Names::Show_Mid_Scope), this);
    audioProcessor.apvts.addParameterListener(params.at(Names::Show_High_Scope), this);
    audioProcessor.apvts.addParameterListener(params.at(Names::Stack_Bands_Scope), this);
    
    updateOptionsParameters();
}

/* Pass latest parameters to the oscilloscope */
// ========================================================
void WindowWrapperOscilloscope::updateOptionsParameters()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    //bool showLowBand = showLowBandParam->get();
    //bool showMidBand = showMidBandParam->get();
    //bool showHighBand = showHighBandParam->get();
    //bool stackBands = stackBandsParam->get();

    bool showLowBand = audioProcessor.getShowLowBandParam()->get();
    bool showMidBand = audioProcessor.getShowMidBandParam()->get();
    bool showHighBand = audioProcessor.getShowHighBandParam()->get();
    bool stackBands = audioProcessor.getStackBandsParam()->get();

    oscilloscope.updateScopeParameters(showLowBand, showMidBand, showHighBand, stackBands);
}

/* Called on component resize */
// ========================================================
void WindowWrapperOscilloscope::resized()
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    optionsMenu.setTopLeftPosition(8, 7);  // [Claude 2026-10-29] inside the recess, as on the crossover; was (4, 4)
    
    oscilloscope.setSize(getWidth(), getHeight() );
}

/* When a parameter is changed, update accordingly */
// ========================================================
void WindowWrapperOscilloscope::parameterChanged(const juce::String& parameterID, float newValue)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

    // [Claude 2026-10-27] This can be called on the audio thread or the host's automation thread. Re-laying out
    // the scopes there (setBounds, toFront) is GUI work off the message thread: on the Mac it aborted pluginval's
    // AU "Editor Automation" test. So ask for handleAsyncUpdate() instead, which runs on the message thread.
    triggerAsyncUpdate();
}

// [Claude 2026-10-27] Runs on the message thread after parameterChanged()
// ========================================================
void WindowWrapperOscilloscope::handleAsyncUpdate()
{
    updateOptionsParameters();
}

/* Paint graphics */
// ========================================================
void WindowWrapperOscilloscope::paint(juce::Graphics& g)
{
    //if (setDebug)
    //    WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());
}