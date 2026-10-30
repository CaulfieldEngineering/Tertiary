/*
  ==============================================================================

    TestHooks.h
    Created: 29 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-29] New file. The few places the UI test runner (tests/UiTest.cpp) needs to reach into
    the plugin. Joe: "I want to check as much as possible automated prior."

    Everything that changes behaviour is inside #if WLA_UI_TEST_BUILD, which only the UiTest target defines.
    The plugin, the standalone app and every other build compile these to exactly what they did before:
      - settings (licence, window size) go to a temp folder in the UI tests, never the real Tertiary.settings
      - links and "open folder" are recorded in the UI tests instead of opening the browser or Explorer/Finder
    UiTestAccess is the test runner's friend: it reads private state (what the scope draws, the About window's
    build-id flag...) to check it. It is only declared here; nothing in the plugin defines or uses it.

  ==============================================================================
*/

#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

struct UiTestAccess;   // tests/UiTest.cpp

namespace TestHooks
{
    /* The UI tests' own folder: settings and user presets live here while they run */
    inline juce::File testFolder()
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("TertiaryUiTest");
    }

    /* Settings file options: unchanged, except in the UI tests (a temp folder) */
    inline void redirectSettings (juce::PropertiesFile::Options& options)
    {
       #if WLA_UI_TEST_BUILD
        options.folderName = testFolder().getFullPathName();
       #else
        juce::ignoreUnused (options);
       #endif
    }

    /* What the UI tests would have opened (URLs and folders), in order */
    inline juce::StringArray& opened()
    {
        static juce::StringArray list;
        return list;
    }

    inline void openUrl (const juce::URL& url)
    {
       #if WLA_UI_TEST_BUILD
        opened().add (url.toString (true));
       #else
        url.launchInDefaultBrowser();
       #endif
    }

    inline void openFolder (const juce::File& folder)
    {
       #if WLA_UI_TEST_BUILD
        opened().add (folder.getFullPathName());
       #else
        folder.startAsProcess();
       #endif
    }
}
