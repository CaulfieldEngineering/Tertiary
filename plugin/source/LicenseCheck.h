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

    /* [Claude 2026-10-30] M-01: the server compares the email exactly, capitals included (tested 2026-10-29 on
       wonderlandaudio.com: the order's email validates, the same email with other capitals gives invalid_email).
       So the email is sent as the customer typed it and as it is saved, only trimmed. Never lower-cased. */
    juce::String normaliseEmail(const juce::String& email);

    /* [Claude 2026-10-30] M-01: the other forms of a saved email to try once each when the weekly check gets
       invalid_email: as saved (untrimmed, if that differs), then in lower case (if that differs). The first form
       tried is normaliseEmail(saved). */
    juce::StringArray otherEmailForms(const juce::String& savedEmail);

    /* [Claude 2026-10-29] V-01: the email in the About window's field, ready to send (normalised: trimmed only). Once activated the
       field shows "Email: " before it; that is dropped only when it is there. A typed email has no prefix, and taking
       the text after "Email: " gave an empty email, so activation saved an empty one. */
    juce::String emailFromField(const juce::String& fieldText);

    /* [Claude 2026-10-29] V-01: after a successful activation, saves the key and the email the activation request
       used, the machine instance, and the check dates (what AboutWindow::updateLocalKeyData did) */
    void saveActivation(juce::PropertiesFile& settings, const juce::String& key, const juce::String& email, juce::int64 nowMs,
                        const juce::String& instance = {});   // [Claude 2026-10-30] M-03: the instance activated (empty: this computer's)

    /* [Claude 2026-10-30] M-03: what activating this computer came to, for the About window */
    struct ActivationOutcome
    {
        bool activated { false };   // The server has this computer activated: save the key and go out of demo
        juce::String instance;      // The instance that is activated (save it, deactivation uses it)
        juce::String code;          // The server's last answer
        juce::String status;        // Plain words for the status line
    };

    /* [Claude 2026-10-30] M-03: activates this computer, and says only what really happened.
       The server answers instance_already_activated, and records nothing, when the instance was activated before,
       even after it was deactivated (tested on wonderlandaudio.com 2026-10-29). 1.1 showed "Reactivation
       successful!" for it. Now on that answer:
         1. validate: if the instance is in the reply's activations, it is active: success.
         2. if not, deactivate that instance and activate again.
         3. if the server still refuses the instance (it does, on the live server), activate with a new instance
            for this computer (the machine instance plus a random suffix), and save that one.
       Anything else is reported as an error with the server's words. Blocks; call it from a background thread. */
    ActivationOutcome activateThisComputer(LicenseServer& server, const juce::String& key, const juce::String& email,
                                           const juce::String& instance, const juce::String& platform);

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
