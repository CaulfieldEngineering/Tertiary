/*
  ==============================================================================

    PretendServer.h
    Created: 27 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-27] New file. The pretend license server and a temp settings file,
    shared by TertiaryTests.cpp and HostTest.cpp. Nothing here touches the network.

  ==============================================================================
*/

#pragma once
#include "LicenseCheck.h"
#include <map>

//==============================================================================
// A pretend license server: answers what the test tells it to, and counts the calls
//==============================================================================
struct PretendServer : LicenseServer
{
    LicenseReply validateReply, versionReply;
    LicenseReply activateReply;          // [Claude 2026-10-29] V-01: the activation test
    int validateCalls = 0, versionCalls = 0;
    juce::String lastEmail;
    juce::String lastActivateEmail;      // [Claude 2026-10-29] the email the last activation sent

    // [Claude 2026-10-30] M-01: the real server's email check. When orderEmail is set, every route compares the
    // email sent with it exactly, capitals included, and answers invalid_email when it differs (as
    // wonderlandaudio.com did on 2026-10-29). Empty: the canned replies above, as before.
    juce::String orderEmail;
    juce::StringArray validateEmails;    // every email validate was sent, in order

    bool emailMatches (const juce::String& email) const { return orderEmail.isEmpty() || email == orderEmail; }

    // [Claude 2026-10-30] M-02: the site's page cache. When pageCache is on, every call is looked up by the address
    // the real plugin would send (WonderlandLicenseServer::requestUrl): the same address within 300 s of clockMs
    // gets the earlier reply, as Batcache did on wonderlandaudio.com. The tests move clockMs themselves.
    bool pageCache = false;
    juce::int64 clockMs = 0;
    int cacheHits = 0;
    std::map<juce::String, std::pair<juce::int64, LicenseReply>> cache;

    // [Claude 2026-10-30] M-03: the real server's activations. When modelActivations is on, validate answers
    // key_valid listing the active instances, activate adds one, deactivate removes it. An instance activated
    // before answers instance_already_activated and is not recorded, even after a deactivate (stickyInstances,
    // as wonderlandaudio.com did on 2026-10-29); with stickyInstances off a deactivate frees it.
    bool modelActivations = false;
    bool stickyInstances = true;
    juce::StringArray activeInstances, usedInstances;
    juce::StringArray calls;   // "activate TRT-..", "deactivate TRT-..", "validate", in order

    LicenseReply modelReply (const juce::String& code)
    {
        auto r = reply (code);
        r.activations = activeInstances;
        return r;
    }

    template <typename Fresh>
    LicenseReply throughCache (const juce::URL& url, Fresh&& fresh)
    {
        if (! pageCache)
            return fresh();

        auto address = url.toString (true);
        auto found = cache.find (address);
        if (found != cache.end() && clockMs - found->second.first < 300 * 1000)
        {
            ++cacheHits;
            return found->second.second;
        }

        auto r = fresh();
        cache[address] = { clockMs, r };
        return r;
    }
    static LicenseReply invalidEmail()  { return reply ("invalid_email", "Invalid email address."); }

    static LicenseReply reply (const juce::String& code, const juce::String& message = {}, const juce::String& version = {})
    {
        LicenseReply r;
        r.reachable = true;
        r.code = code;
        r.message = message;
        r.version = version;
        return r;
    }

    LicenseReply validate (const juce::String& key, const juce::String& email) override
    {
        return throughCache (WonderlandLicenseServer::requestUrl ("validate", key, email), [&]
        {
            ++validateCalls;
            lastEmail = email;
            validateEmails.add (email);
            calls.add ("validate");
            if (! emailMatches (email))
                return invalidEmail();
            if (modelActivations)
                return modelReply ("key_valid");
            return validateReply;
        });
    }

    LicenseReply activate (const juce::String& key, const juce::String& email, const juce::String& instance, const juce::String& platform) override
    {
        return throughCache (WonderlandLicenseServer::requestUrl ("activate", key, email, instance, platform), [&]
        {
            lastActivateEmail = email;
            calls.add ("activate " + instance);
            if (! emailMatches (email))
                return invalidEmail();
            if (! modelActivations)
                return activateReply;
            if (usedInstances.contains (instance))
                return reply ("instance_already_activated", "Instance is already activated.");
            usedInstances.add (instance);
            activeInstances.add (instance);
            return modelReply ("key_activated");
        });
    }

    LicenseReply deactivate (const juce::String& key, const juce::String& email, const juce::String& instance, const juce::String& platform) override
    {
        return throughCache (WonderlandLicenseServer::requestUrl ("deactivate", key, email, instance, platform), [&]
        {
            calls.add ("deactivate " + instance);
            if (! modelActivations)
                return LicenseReply();
            activeInstances.removeString (instance);
            if (! stickyInstances)
                usedInstances.removeString (instance);
            return modelReply ("key_deactivated");
        });
    }

    LicenseReply versionCheck (const juce::String&, const juce::String&) override
    {
        ++versionCalls;
        return versionReply;
    }
};

/* A settings file in the temp folder, deleted afterwards */
struct TempSettings
{
    juce::File file { juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getNonexistentChildFile ("TertiaryTestSettings", ".settings") };
    std::unique_ptr<juce::PropertiesFile> props;

    TempSettings()                          { reload(); }
    ~TempSettings()                         { props.reset(); file.deleteFile(); }
    void reload()                           { props = std::make_unique<juce::PropertiesFile> (file, LicenseCheck::getSettingsOptions()); }
    juce::PropertiesFile& get()             { return *props; }

    void activated (juce::int64 lastValidMs, juce::int64 lastCheckMs)
    {
        props->setValue ("licenseKey", "ABCD-1234");
        props->setValue ("licenseEmail", "Joe@Example.com");
        props->setValue ("licenseLastValid", juce::String (lastValidMs));
        props->setValue ("licenseLastCheck", juce::String (lastCheckMs));
        props->saveIfNeeded();
    }
};

