/*
  ==============================================================================

    LicenseCheck.cpp
    Created: 27 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-27] New file. See LicenseCheck.h for what it does and why.

  ==============================================================================
*/

#include "LicenseCheck.h"
#include "PluginProcessor.h"

namespace LicenseCheck
{

/* The same options AboutWindow and the processor use, so it is the same Tertiary.settings file */
//==============================================================================
juce::PropertiesFile::Options getSettingsOptions()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Tertiary";
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::StorageFormat::storeAsXML;
    return options;
}

/* A stable ID for this computer. JUCE's getUniqueDeviceID() is the motherboard / platform ID.
   Only a hash of it is sent, prefixed TRT- so it is easy to spot in the WooCommerce admin. */
//==============================================================================
juce::String getMachineInstance(juce::PropertiesFile& settings)
{
    auto machineId = juce::SystemStats::getUniqueDeviceID();

    // Fallbacks: the file-system ID or network address, then a random ID kept in the settings file
    if (machineId.isEmpty())
    {
        auto ids = juce::SystemStats::getMachineIdentifiers(juce::SystemStats::MachineIdFlags::fileSystemId
                                                          | juce::SystemStats::MachineIdFlags::macAddresses);
        if (! ids.isEmpty())
            machineId = ids[0];
    }

    if (machineId.isEmpty())
    {
        machineId = settings.getValue("licenseMachineId");

        if (machineId.isEmpty())
        {
            machineId = juce::Uuid().toString();
            settings.setValue("licenseMachineId", machineId);
            settings.saveIfNeeded();
        }
    }

    auto hash = (juce::uint64) ("Tertiary:" + machineId).hashCode64();
    return "TRT-" + juce::String::toHexString((juce::int64) hash).paddedLeft('0', 16).toUpperCase();
}

/* 1.0.x saved no instance; it activated with the device description, so deactivate with that */
//==============================================================================
juce::String getActivatedInstance(juce::PropertiesFile& settings)
{
    auto instance = settings.getValue("licenseInstance");

    if (instance.isEmpty())
        instance = juce::SystemStats::getDeviceDescription();

    return instance;
}

/* From the server's source (Serial Numbers for WooCommerce, RestAPI::validate_request).
   invalid_order is what a refunded or cancelled order gives. */
//==============================================================================
bool isRevokingCode(const juce::String& code)
{
    return code == "invalid_key"            // No such key, or it has no order
        || code == "invalid_order"          // Order refunded, cancelled or not completed
        || code == "invalid_product_key"    // Key is for another product
        || code == "invalid_email"          // Email doesn't match the order
        || code == "expired_key"
        || code == "key_cancelled"
        || code == "invalid_key_status";
}

//==============================================================================
bool isNewerVersion(const juce::String& latest, const juce::String& current)
{
    juce::StringArray a, b;
    a.addTokens(latest.trim(), ".", "");
    b.addTokens(current.trim(), ".", "");

    if (latest.trim().isEmpty() || ! latest.trim().containsOnly("0123456789."))
        return false;

    for (int i = 0; i < juce::jmax(a.size(), b.size()); ++i)
    {
        auto x = a[i].getIntValue();
        auto y = b[i].getIntValue();

        if (x != y)
            return x > y;
    }

    return false;
}

// [Claude 2026-10-30] M-01: trimmed only, never lower-cased (it was toLowerCase(), which the server rejects as
// invalid_email when the order's email has capitals). See LicenseCheck.h.
//==============================================================================
juce::String normaliseEmail(const juce::String& email)
{
    return email.trim();
}

// [Claude 2026-10-30] M-01. See LicenseCheck.h.
//==============================================================================
juce::StringArray otherEmailForms(const juce::String& savedEmail)
{
    juce::StringArray forms;
    const auto first = normaliseEmail(savedEmail);

    for (auto form : { savedEmail, first.toLowerCase() })
        if (form.isNotEmpty() && form != first && ! forms.contains(form))
            forms.add(form);

    return forms;
}

// [Claude 2026-10-29] V-01. See LicenseCheck.h.
//==============================================================================
juce::String emailFromField(const juce::String& fieldText)
{
    auto text = fieldText.trim();
    if (text.startsWith("Email: "))
        text = text.substring(7);
    return normaliseEmail(text);
}

// [Claude 2026-10-29] V-01. See LicenseCheck.h.
//==============================================================================
void saveActivation(juce::PropertiesFile& settings, const juce::String& key, const juce::String& email, juce::int64 nowMs,
                    const juce::String& instance)
{
    auto now = juce::String(nowMs);
    settings.setValue("licenseKey", key.trim());
    settings.setValue("licenseEmail", email);
    settings.setValue("licenseInstance", instance.isNotEmpty() ? instance : getMachineInstance(settings));   // [Claude 2026-10-30] M-03
    settings.setValue("licenseLastCheck", now);
    settings.setValue("licenseLastValid", now);
    settings.saveIfNeeded();
}

// [Claude 2026-10-30] M-03. See LicenseCheck.h.
//==============================================================================
ActivationOutcome activateThisComputer(LicenseServer& server, const juce::String& key, const juce::String& email,
                                       const juce::String& instance, const juce::String& platform)
{
    ActivationOutcome outcome;
    outcome.instance = instance;

    auto failed = [&outcome](const LicenseReply& reply)
    {
        outcome.code = reply.code;
        outcome.status = ! reply.reachable ? juce::String("Activation connection failed.")
                                           : "Activation error: " + reply.message;
        return outcome;
    };
    auto succeeded = [&outcome](const juce::String& code, const juce::String& activatedInstance, const juce::String& status)
    {
        outcome.activated = true;
        outcome.code = code;
        outcome.instance = activatedInstance;
        outcome.status = status;
        return outcome;
    };

    auto reply = server.activate(key, email, instance, platform);

    if (reply.reachable && reply.code == "key_activated")
        return succeeded(reply.code, instance, "Activation successful!");

    if (! reply.reachable || reply.code != "instance_already_activated")
        return failed(reply);

    // 1. Is this computer really on the key?
    auto check = server.validate(key, email);
    if (! check.reachable || check.code != "key_valid")
        return failed(check);

    if (check.activations.contains(instance))
        return succeeded(reply.code, instance, "Already activated on this computer.");

    // 2. Not listed: deactivate that instance, then activate again
    server.deactivate(key, email, instance, platform);
    reply = server.activate(key, email, instance, platform);

    if (reply.reachable && reply.code == "key_activated")
        return succeeded(reply.code, instance, "Activation successful!");

    if (! reply.reachable || reply.code != "instance_already_activated")
        return failed(reply);

    // 3. The server keeps refusing this instance: a new one for this computer
    auto fresh = instance + "-" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt(0x10000)).paddedLeft('0', 4).toUpperCase();
    reply = server.activate(key, email, fresh, platform);

    if (reply.reachable && reply.code == "key_activated")
        return succeeded(reply.code, fresh, "Activation successful!");

    return failed(reply);
}

/* The weekly re-check and the daily version check. See LicenseCheck.h. */
//==============================================================================
Result runStartupCheck(LicenseServer& server, juce::PropertiesFile& settings,
                       juce::int64 nowMs, const juce::String& currentVersion)
{
    Result result;
    result.finished = true;

    auto key = settings.getValue("licenseKey").trim();
    auto savedEmail = settings.getValue("licenseEmail");
    auto email = normaliseEmail(savedEmail);   // [Claude 2026-10-30] M-01: as saved (trimmed), capitals kept

    // Demo copy: nothing to check (and the server only gives versions to valid keys)
    if (key.isEmpty() || email.isEmpty())
        return result;

    // A 1.0.x activation has no dates yet: count from now
    if (settings.getValue("licenseLastValid").getLargeIntValue() <= 0)
        settings.setValue("licenseLastValid", juce::String(nowMs));

    auto lastCheck = settings.getValue("licenseLastCheck").getLargeIntValue();
    auto lastValid = settings.getValue("licenseLastValid").getLargeIntValue();

    // [1] The weekly re-check
    if (nowMs - lastCheck >= recheckIntervalMs || nowMs < lastCheck)
    {
        auto reply = server.validate(key, email);

        // [Claude 2026-10-30] M-01: before revoking for invalid_email, try the saved email's other forms once each
        // (as saved, then lower case). A form that works is kept and saved. If a retry can't reach the server,
        // nothing changes this time, as for any unreachable check.
        if (reply.reachable && reply.code == "invalid_email")
        {
            bool retryUnreachable = false;

            for (auto& form : otherEmailForms(savedEmail))
            {
                auto retry = server.validate(key, form);

                if (retry.reachable && retry.code == "key_valid")
                {
                    reply = retry;
                    email = form;
                    settings.setValue("licenseEmail", form);
                    break;
                }

                if (! retry.reachable)
                    retryUnreachable = true;
            }

            if (reply.code != "key_valid" && retryUnreachable)
                reply = {};   // treated as unreachable below
        }

        if (reply.reachable && reply.code == "key_valid")
        {
            settings.setValue("licenseLastCheck", juce::String(nowMs));
            settings.setValue("licenseLastValid", juce::String(nowMs));
            lastValid = nowMs;
        }
        else if (reply.reachable && isRevokingCode(reply.code))
        {
            // Back to demo mode. The email stays, so the user can re-enter the key easily.
            settings.setValue("licenseKey", juce::String());
            settings.setValue("licenseInstance", juce::String());
            settings.setValue("licenseLastCheck", juce::String(nowMs));
            settings.saveIfNeeded();

            result.revoked = true;
            result.message = "This serial key is no longer valid (" + reply.message.trim()
                           + "). Tertiary is back in demo mode. Questions: support@wonderlandaudio.com";
            return result;
        }
        // Otherwise unreachable, or an answer we don't understand: change nothing, try again next time
    }

    // [2] The daily version check
    auto lastVersionCheck = settings.getValue("versionLastCheck").getLargeIntValue();

    if (nowMs - lastVersionCheck >= versionIntervalMs || nowMs < lastVersionCheck)
    {
        auto reply = server.versionCheck(key, email);

        if (reply.reachable && reply.code == "version_checked")
        {
            settings.setValue("versionLastCheck", juce::String(nowMs));
            settings.setValue("versionLatest", reply.version.trim());
        }
    }

    if (isNewerVersion(settings.getValue("versionLatest"), currentVersion))
        result.newVersion = settings.getValue("versionLatest").trim();

    // [3] A long time offline: say so, keep working
    if (nowMs - lastValid > offlineGraceMs)
    {
        result.offlineTooLong = true;
        result.message = "Tertiary hasn't been able to check your serial key for over 30 days. "
                         "It keeps working; connect to the internet now and then so it can check.";
    }

    settings.saveIfNeeded();
    return result;
}

} // namespace LicenseCheck



/* Background thread, one per plugin instance */
//==============================================================================
LicenseCheckThread::LicenseCheckThread() : juce::Thread("Tertiary License Check")
{
}

LicenseCheckThread::~LicenseCheckThread()
{
    stop();
}

void LicenseCheckThread::start(audio_plugin::AudioPluginAudioProcessor& processor)
{
    if (hasStarted)
        return;

    hasStarted = true;
    audioProcessor = &processor;
    startThread(juce::Thread::Priority::low);   // Fails quietly in the web demo, which has no threads
}

void LicenseCheckThread::stop()
{
    signalThreadShouldExit();
    server.cancel();
    stopThread(10000);
}

LicenseCheck::Result LicenseCheckThread::getResult()
{
    const juce::ScopedLock lock(resultLock);
    return result;
}

void LicenseCheckThread::run()
{
    juce::PropertiesFile settings(LicenseCheck::getSettingsOptions());

    auto checkResult = LicenseCheck::runStartupCheck(server, settings,
                                                     juce::Time::currentTimeMillis(),
                                                     JucePlugin_VersionString);
    if (threadShouldExit())
        return;

    if (checkResult.revoked && audioProcessor != nullptr)
        audioProcessor->setDemoState(true);

    {
        const juce::ScopedLock lock(resultLock);
        result = checkResult;
    }

    sendChangeMessage();   // The editor updates on the message thread
}
