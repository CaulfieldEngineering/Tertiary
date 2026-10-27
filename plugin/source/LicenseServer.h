/*
  ==============================================================================

    LicenseServer.h
    Created: 27 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-27] New file. The calls to the WonderlandAudio.com serial-key
    server (Serial Numbers for WooCommerce: /wp-json/wcsn/...), behind a small
    interface so the tests can use a pretend server instead of the real one.

    AboutWindow uses it to validate, activate and deactivate a key.
    LicenseCheck uses it for the weekly re-check and the update check.

  ==============================================================================
*/

#pragma once
#include <juce_core/juce_core.h>

/* What the server answered. reachable is false when there was no answer we could read
   (no internet, server down, not JSON); then code and message are empty. */
struct LicenseReply
{
    bool reachable { false };
    juce::String code;      // e.g. "key_valid", "key_activated", "invalid_order"
    juce::String message;   // The server's own words, shown to the user on errors
    juce::String version;   // Only filled by versionCheck(): the latest version on the site
};

/* The four routes the plugin uses. All of them block, so call them from a background thread. */
struct LicenseServer
{
    virtual ~LicenseServer() = default;

    virtual LicenseReply validate     (const juce::String& key, const juce::String& email) = 0;
    virtual LicenseReply activate     (const juce::String& key, const juce::String& email,
                                       const juce::String& instance, const juce::String& platform) = 0;
    virtual LicenseReply deactivate   (const juce::String& key, const juce::String& email,
                                       const juce::String& instance, const juce::String& platform) = 0;
    virtual LicenseReply versionCheck (const juce::String& key, const juce::String& email) = 0;

    /* Called from another thread to abandon a call in progress (when the plugin closes). */
    virtual void cancel() {}
};

/* The real server at WonderlandAudio.com */
struct WonderlandLicenseServer : LicenseServer
{
    LicenseReply validate     (const juce::String& key, const juce::String& email) override;
    LicenseReply activate     (const juce::String& key, const juce::String& email,
                               const juce::String& instance, const juce::String& platform) override;
    LicenseReply deactivate   (const juce::String& key, const juce::String& email,
                               const juce::String& instance, const juce::String& platform) override;
    LicenseReply versionCheck (const juce::String& key, const juce::String& email) override;
    void cancel() override;

private:
    /* Sends one GET request to wonderlandaudio.com/wp-json/wcsn/<route> and reads the JSON reply */
    LicenseReply call(const juce::String& route, const juce::StringPairArray& parameters);

    juce::CriticalSection streamLock;
    juce::WebInputStream* currentStream { nullptr };   // The call in progress, so cancel() can stop it
    bool cancelled { false };
};
