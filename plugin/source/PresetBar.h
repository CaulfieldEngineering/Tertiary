/*
  ==============================================================================

    PresetBar.h
    Created: 27 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-27] New file. The preset bar at the top left of the window:

        [<]  Preset Name *  [>]

    The arrows step through the presets. Clicking the name opens the menu: the factory
    presets, your presets, then Save, Save As, Rename, Delete and Open Presets Folder.
    The * shows when the settings differ from the loaded preset. All of the work is done
    by the processor's PresetManager; this is only the drawing and the menu.

  ==============================================================================
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

struct PresetBar : juce::Component,
                   juce::Timer
{
    PresetBar(audio_plugin::AudioPluginAudioProcessor& p);
    ~PresetBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void timerCallback() override;

private:
    audio_plugin::AudioPluginAudioProcessor& audioProcessor;
    PresetManager& presets;

    juce::TextButton mButtonPrevious, mButtonNext;
    juce::Rectangle<int> nameArea;

    juce::String shownName;
    bool shownModified { false };

    void showMenu();
    void askForName(const juce::String& title, const juce::String& initial, std::function<void(juce::String)> onOk);
    void saveAs();
    void renameCurrent();
    void deleteCurrent();
    void changed();
};
