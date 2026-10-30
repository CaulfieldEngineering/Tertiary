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

    LicenseReply validate (const juce::String&, const juce::String& email) override
    {
        ++validateCalls;
        lastEmail = email;
        validateEmails.add (email);
        if (! emailMatches (email))
            return invalidEmail();
        return validateReply;
    }

    LicenseReply activate (const juce::String&, const juce::String& email, const juce::String&, const juce::String&) override
    {
        lastActivateEmail = email;
        if (! emailMatches (email))
            return invalidEmail();
        return activateReply;
    }
    LicenseReply deactivate (const juce::String&, const juce::String&, const juce::String&, const juce::String&) override { return {}; }

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

