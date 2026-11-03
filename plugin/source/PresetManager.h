/*
  ==============================================================================

    PresetManager.h
    Created: 27 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-27] New file. Joe: "I just remembered I wanted to implement presets."

    Presets: the factory presets (built into the plugin, in FactoryPresets.cpp) and the
    user's own (files in Documents/Wonderland Audio/Tertiary/Presets, shared by VST3, AU and
    the standalone app). The processor owns one PresetManager; the PresetBar (top left of
    the window) and the host's program list (getNumPrograms etc.) both use it.

    A preset holds only the sound parameters, never the scope and spectrum view settings.
    Parameters a factory preset doesn't list are set to their defaults.

    Preset numbers (as the host sees them): the factory presets first, then the user
    presets in alphabetical order.

    File format (.tertiarypreset, XML):
      <TertiaryPreset name="My Preset" version="1.1.0">
        <PARAM id="Depth Low Band" value="75"/>   (values in the parameter's own units)
        ...
      </TertiaryPreset>

    Session state: the processor saves the current preset's name in the session
    (getStateInformation) and gives it back here (restoreFromSession). Sessions from 1.0.x
    have no name; they load exactly as before and show "No preset".

  ==============================================================================
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

struct PresetManager
{
    /* One factory preset: a name and the values that differ from the defaults (parameter units;
       a choice's value is its index) */
    struct FactoryPreset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };

    PresetManager(juce::AudioProcessorValueTreeState& apvts, juce::File userFolder = getDefaultUserFolder());

    static juce::File getDefaultUserFolder();
    static const std::vector<FactoryPreset>& getFactoryPresets();   // FactoryPresets.cpp
    static bool isSoundParameter(const juce::String& parameterID);  // false for the view settings

    /* The list, as the host sees it */
    int getNumPresets();
    juce::String getPresetName(int index);
    bool isUserPreset(int index);
    int getCurrentIndex();                // -1 when no preset is loaded (e.g. an old session)
    juce::String getCurrentName();        // "No preset" when none
    bool isModified();                    // the settings differ from the loaded preset

    bool loadPreset(int index);
    void loadNext();
    void loadPrevious();

    /* User presets. Names are cleaned to be valid file names. They return false (and do nothing) if
       the file can't be written, or the name is taken by a factory preset. */
    bool saveCurrentAsUser(const juce::String& name);   // Save As (or Save, with the current name)
    bool renameUser(int index, const juce::String& newName);
    bool deleteUser(int index);
    bool userPresetExists(const juce::String& name);
    void refreshUserPresets();            // re-reads the folder (on the message thread)

    /* Session state */
    void writeToSession(juce::ValueTree& state);
    void restoreFromSession(const juce::ValueTree& state);

    static juce::String cleanName(const juce::String& name);
    juce::File getUserFolder() const { return userFolder; }
    bool useTrash { true };               // deleted user presets go to the Trash / Recycle Bin (tests turn it off)

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::File userFolder;

    juce::CriticalSection lock;           // the host can ask for the list from any thread
    juce::Array<juce::File> userFiles;    // sorted by name
    int currentIndex { 0 };               // a new instance starts on "Default"
    juce::String currentName;
    std::map<juce::String, float> loadedValues;   // normalised, as loaded, for isModified()

    std::map<juce::String, float> readUserFile(const juce::File& file);   // parameter units
    void apply(const std::map<juce::String, float>& valuesInUnits);
    void rememberLoadedValues();
};
