#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GlobalControls.h"
#include "WindowWrapperFrequency.h"
#include "WindowWrapperOscilloscope.h"
#include "TopBanner.h"
#include "UtilityFunctions.h"
#include "AboutWindow.h"

namespace audio_plugin {

class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::Timer,
                                     public juce::Button::Listener {
    public:
        AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor&);
        ~AudioPluginAudioProcessorEditor() override;

        void paint(juce::Graphics&) override;
        void resized() override;

        void timerCallback() override{};
        void buttonClicked(juce::Button* button) override{};
        void mouseDoubleClick(const juce::MouseEvent& event) override;

    private:
        juce::String mNameSpace{"PluginEditor"};
        bool setDebug{true};
        bool openDebug{false};

		bool fullScreenScope{true};

        /* Reference to the Audio Processor & DSP Parameters */
        AudioPluginAudioProcessor& audioProcessor;

        /* Time-Domain Display */
        WindowWrapperOscilloscope wrapperOscilloscope{audioProcessor};

        /* Contains Lower-Half of Parameters */
        GlobalControls globalControls{audioProcessor};

        /* Frequency-Domain & Crossover Display */
        WindowWrapperFrequency wrapperFrequency{audioProcessor, audioProcessor.apvts};

        /* Company & Plugin Title Banner */
        // void buildTopBanner(juce::Graphics& g);
        TopBanner topBanner;

        AboutWindow aboutWindow;

        /* Header displayed in top corner */
        juce::Label companyTitle;

        void buildFlexboxLayout();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor);
    };
    
} // namespace audio_plugin
