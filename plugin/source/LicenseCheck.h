/*
  ==============================================================================

    LicenseCheck.h
    Created: 27 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-27] New file. What Tertiary 1.1 does with the serial key after
    it has been activated, and the "new version available" check.

    - About once a week (when the editor opens), the saved key is checked again
      with the server's validate route.
    - If the server says the key is no longer good (refunded, cancelled, expired,
      wrong email...), the plugin goes back to demo mode with a plain message.
    - If the server can't be reached, nothing changes: a paying user is never
      locked out for being offline. After 30 days without a successful check
      the About window says so, quietly, and the plugin keeps working.
    - At most once a day it asks the version_check route for the latest version,
      and the editor shows a quiet "Version X is available" link to the site.
      Nothing is ever downloaded. (The server only answers this for a valid key,
      so it runs for activated copies only.)

    The decisions are in runStartupCheck(), which takes the server and the settings
    file as arguments so the tests can hand it a pretend server and a temp file.

    Settings file (the same Tertiary.settings as 1.0.x; new keys are added, none removed):
      licenseKey, licenseEmail      as in 1.0.x
      licenseInstance               the machine ID used to activate (empty for 1.0.x activations)
      licenseMachineId              a random ID, only used if the computer gives none
      licenseLastCheck              when the key was last checked, in ms since 1970
      licenseLastValid              when the server last said the key is good
      versionLastCheck, versionLatest

  ==============================================================================
*/

#pragma once
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include "LicenseServer.h"
#include "TestHooks.h"   // [Claude 2026-10-29] UI tests: settings in a temp folder, UiTestAccess

namespace audio_plugin { class AudioPluginAudioProcessor; }

namespace LicenseCheck
{
    /* How often, and how long offline before the About window mentions it */
    const juce::int64 recheckIntervalMs  { 7LL  * 24 * 60 * 60 * 1000 };   // a week
    const juce::int64 offlineGraceMs     { 30LL * 24 * 60 * 60 * 1000 };   // 30 days
    const juce::int64 versionIntervalMs  { 1LL  * 24 * 60 * 60 * 1000 };   // a day

    /* What the check found, for the editor to show */
    struct Result
    {
        bool finished { false };        // The check has run
        bool revoked { false };         // The key was cleared: back to demo mode
        bool offlineTooLong { false };  // No successful check for over 30 days (still activated)
        juce::String message;           // Plain words for the About window, empty if nothing to say
        juce::String newVersion;        // e.g. "1.1.1" when newer than this build, else empty
    };

    /* The settings file options, the same as AboutWindow and the processor use */
    juce::PropertiesFile::Options getSettingsOptions();

    /* This computer's activation "instance": a hash of the machine's own ID, never the raw ID */
    juce::String getMachineInstance(juce::PropertiesFile& settings);

    /* The instance this copy was activated with (1.0.x activations used the device description) */
    juce::String getActivatedInstance(juce::PropertiesFile& settings);

    /* Server codes that mean the key is no longer good. Anything else (a server error,
       a missing route...) is treated like "unreachable", so nobody loses their activation to it. */
    bool isRevokingCode(const juce::String& code);

    /* True when latest (e.g. "1.2.0") is a higher version than current (e.g. "1.1.0") */
    bool isNewerVersion(const juce::String& latest, const juce::String& current);

    /* The emails are compared in lower case by the server, so send them that way */
    juce::String normaliseEmail(const juce::String& email);

    /* The whole check. Blocks while it talks to the server; call it from a background thread. */
    Result runStartupCheck(LicenseServer& server, juce::PropertiesFile& settings,
                           juce::int64 nowMs, const juce::String& currentVersion);
}

/* Runs runStartupCheck() once on its own thread, owned by the processor.
   The editor listens (ChangeListener) and shows the result when it arrives. */
struct LicenseCheckThread : juce::Thread,
                            juce::ChangeBroadcaster
{
    LicenseCheckThread();
    ~LicenseCheckThread() override;

    /* Starts the check if it hasn't run yet for this plugin instance */
    void start(audio_plugin::AudioPluginAudioProcessor& processor);

    /* Cancels a server call in progress and waits for the thread (called by the processor's destructor) */
    void stop();

    LicenseCheck::Result getResult();

    void run() override;

private:
    audio_plugin::AudioPluginAudioProcessor* audioProcessor { nullptr };
    WonderlandLicenseServer server;
    bool hasStarted { false };

    juce::CriticalSection resultLock;
    LicenseCheck::Result result;
};
