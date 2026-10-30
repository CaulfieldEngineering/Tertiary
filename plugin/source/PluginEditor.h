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
    friend struct ::UiTestAccess;   // [Claude 2026-10-29] the UI test runner reads private state (TestHooks.h)
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
        std::unique_ptr<juce::Component> mMenuPreview;

        // [Claude 2026-10-29] The window can be any size (Joe: "make the UI resizable or offer a 150%"). The whole UI is
        // laid out once at 750 x 478 inside `content`, which is scaled to fill the window, so text, lines and images
        // are drawn sharp at every size rather than stretched.
        struct Content : juce::Component
        {
            std::function<void(juce::Graphics&)> onPaint;
            void paint(juce::Graphics& g) override { if (onPaint) onPaint(g); }
        };
        Content content;
        static constexpr int baseWidth = 750, baseHeight = 478;
        float uiScale { 1.f };   // [Claude 2026-10-30] the size chosen from the menu (or remembered); never the host's idea of it
        void paintContent(juce::Graphics& g);
        void setUiScale(float scale);

        // [Claude 2026-10-30] RC1 bug (Joe, Cubase on his laptop): the window opened nearly full screen, black, with the UI
        // at its size in the top left; choosing a size from the menu put it right. The host had answered the first
        // resize request with another size, and nothing asked again. The window's size is now the UI's size, checked
        // a few times a second: when the host's window differs (or the host sized the UI itself), it is asked again,
        // the same way the menu does it. Not in the standalone, whose window is its own.
        juce::Rectangle<int> wantedSize() const;
        void keepWindowAtUiSize();
        void requestHostSize();
        struct SizeKeeper : juce::Timer
        {
            std::function<void()> onTick;
            void timerCallback() override { if (onTick) onTick(); }
        };
        SizeKeeper sizeKeeper;
        int sizeRequests { 0 };   // asked again without success; gives up after a few (a host that insists wins)
        static float loadUiScale();
        void saveUiScale();   // review renders only   // [Claude 2026-10-27] top left of the banner

        /* Header displayed in top corner */
        juce::Label companyTitle;

        void buildFlexboxLayout();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor);
    };
    
} // namespace audio_plugin
