#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GlobalControls.h"
#include "WindowWrapperFrequency.h"
#include "WindowWrapperOscilloscope.h"
#include "TopBanner.h"
#include "UtilityFunctions.h"
#include "AboutWindow.h"
#include "PresetBar.h"   // [Claude 2026-10-27]

// For DPI Awareness...
#if JUCE_WINDOWS
    #include <Windows.h>
    #include <ShellScalingApi.h>
#endif

namespace audio_plugin {

class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::Timer,
                                     public juce::Button::Listener,
                                     public juce::ChangeListener {   // [Claude 2026-10-27] hears the license check
    public:
        AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor&);
        ~AudioPluginAudioProcessorEditor() override;

        void paint(juce::Graphics&) override;
        void resized() override;

        void timerCallback() override{};
        void buttonClicked(juce::Button* button) override{};
        void mouseDoubleClick(const juce::MouseEvent& event) override;

        //==============================================================================
        // Rendering backend selection (Windows-only)
        //
        // JUCE 8 on Windows supports multiple rendering engines (typically Direct2D and
        // the Software Renderer). Some older Intel iGPU/driver combos can exhibit
        // missing fills / paint corruption with Direct2D, especially with complex
        // path fills and alpha compositing.
        //
        // This hook fires when the component is attached/detached from a native
        // parent/peer. Once we have a peer, we can force the Software Renderer.
        //
        // If you later decide you want a user toggle, you can gate this call on a
        // persisted setting.
        #if JUCE_WINDOWS
        void parentHierarchyChanged() override;
        #endif

		/* Used by GUI and self to update the state, upon Key Checking or GUI Activation */
        void updateDemoState(bool newDemoState);

        /* [Claude 2026-10-27] The weekly license re-check finished (LicenseCheck.h) */
        void changeListenerCallback(juce::ChangeBroadcaster* source) override;
        void showLicenseCheckResult();

    private:
        juce::String mNameSpace{"PluginEditor"};
        bool setDebug{true};
        bool openDebug{false};

		bool fullScreenScope{true};

		/* License Key Establishment */
        bool mIsDemoVersion {true};
        void checkForSerialKeyOnStartup();




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

        PresetBar presetBar{audioProcessor};
        // [Claude 2026-10-29] the ☰ menu
        juce::PopupMenu buildMainMenu();
        void showMainMenu();
        std::unique_ptr<juce::Component> mMenuPreview;   // review renders only   // [Claude 2026-10-27] top left of the banner

        /* Header displayed in top corner */
        juce::Label companyTitle;

        void buildFlexboxLayout();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor);
    };
    
} // namespace audio_plugin
