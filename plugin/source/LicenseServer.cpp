/*
  ==============================================================================

    LicenseServer.cpp
    Created: 27 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-27] New file. See LicenseServer.h.
    The requests are the same ones AboutWindow sent in 1.0.2 (product 45, GET,
    parameters in the address), moved here so they can be replaced in tests.

  ==============================================================================
*/

#include "LicenseServer.h"
#include <atomic>

namespace
{
    const juce::String serverAddress { "https://wonderlandaudio.com/wp-json/wcsn/" };
    const juce::String productId { "45" };
    const int timeoutMs { 5000 };
}

// [Claude 2026-10-30] M-02: the four routes build their address with requestUrl (the same parameters as before, plus t)
LicenseReply WonderlandLicenseServer::validate(const juce::String& key, const juce::String& email)
{
    return call(requestUrl("validate", key, email));
}

LicenseReply WonderlandLicenseServer::activate(const juce::String& key, const juce::String& email,
                                               const juce::String& instance, const juce::String& platform)
{
    return call(requestUrl("activate", key, email, instance, platform));
}

LicenseReply WonderlandLicenseServer::deactivate(const juce::String& key, const juce::String& email,
                                                 const juce::String& instance, const juce::String& platform)
{
    return call(requestUrl("deactivate", key, email, instance, platform));
}

LicenseReply WonderlandLicenseServer::versionCheck(const juce::String& key, const juce::String& email)
{
    return call(requestUrl("version_check", key, email));
}

// [Claude 2026-10-30] M-02. See LicenseServer.h.
juce::URL WonderlandLicenseServer::requestUrl(const juce::String& route, const juce::String& key, const juce::String& email,
                                              const juce::String& instance, const juce::String& platform)
{
    juce::StringPairArray parameters;
    parameters.set("serial_key", key);
    if (route == "validate")
        parameters.set("request", "validate");
    parameters.set("instance", instance);
    parameters.set("platform", platform);
    parameters.set("email", email);

    juce::URL url(serverAddress + route);
    url = url.withParameter("product_id", productId);

    for (auto& name : parameters.getAllKeys())
        if (parameters[name].isNotEmpty())
            url = url.withParameter(name, parameters[name]);

    // Unique on every call, even two in the same millisecond: the time, a random number and a counter
    static std::atomic<juce::uint32> counter { 0 };
    auto unique = juce::String(juce::Time::currentTimeMillis()) + "-"
                + juce::String::toHexString(juce::Random::getSystemRandom().nextInt64())
                + juce::String::toHexString((int) ++counter);
    return url.withParameter("t", unique);
}

// [Claude 2026-10-30] M-02. See LicenseServer.h.
juce::String WonderlandLicenseServer::requestHeaders()
{
    return "Cache-Control: no-cache\r\nPragma: no-cache";
}

void WonderlandLicenseServer::cancel()
{
    const juce::ScopedLock lock(streamLock);
    cancelled = true;

   #if ! JUCE_EMSCRIPTEN   // The web demo's JUCE has no network code
    if (currentStream != nullptr)
        currentStream->cancel();
   #endif
}

LicenseReply WonderlandLicenseServer::call(const juce::URL& url)
{
    LicenseReply reply;

    juce::WebInputStream stream(url, false);
    stream.withConnectionTimeout(timeoutMs);
    stream.withExtraHeaders(requestHeaders());   // [Claude 2026-10-30] M-02: no cached answers

    {
        const juce::ScopedLock lock(streamLock);
        if (cancelled)
            return reply;
        currentStream = &stream;
    }

    juce::String responseText;
    if (stream.connect(nullptr))
        responseText = stream.readEntireStreamAsString();

    {
        const juce::ScopedLock lock(streamLock);
        currentStream = nullptr;
    }

    /* The server answers JSON both for success and for errors:
       { "code": "key_valid", ... } or { "code": "invalid_key", "message": "...", "data": { "status": 400 } } */
    auto json = juce::JSON::parse(responseText);
    if (auto* object = json.getDynamicObject())
    {
        reply.code = object->getProperty("code").toString();
        reply.message = object->getProperty("message").toString();
        reply.version = object->getProperty("version").toString();

        // [Claude 2026-10-30] M-03: "activations": [ { "id": 27, "instance": "TRT-...", "platform": "Windows", ... } ]
        if (auto* list = object->getProperty("activations").getArray())
            for (auto& activation : *list)
                if (auto* entry = activation.getDynamicObject())
                    reply.activations.add(entry->getProperty("instance").toString());
        reply.reachable = reply.code.isNotEmpty();
    }

    return reply;
}
