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

namespace
{
    const juce::String serverAddress { "https://wonderlandaudio.com/wp-json/wcsn/" };
    const juce::String productId { "45" };
    const int timeoutMs { 5000 };
}

LicenseReply WonderlandLicenseServer::validate(const juce::String& key, const juce::String& email)
{
    juce::StringPairArray parameters;
    parameters.set("serial_key", key);
    parameters.set("request", "validate");
    parameters.set("email", email);
    return call("validate", parameters);
}

LicenseReply WonderlandLicenseServer::activate(const juce::String& key, const juce::String& email,
                                               const juce::String& instance, const juce::String& platform)
{
    juce::StringPairArray parameters;
    parameters.set("serial_key", key);
    parameters.set("instance", instance);
    parameters.set("platform", platform);
    parameters.set("email", email);
    return call("activate", parameters);
}

LicenseReply WonderlandLicenseServer::deactivate(const juce::String& key, const juce::String& email,
                                                 const juce::String& instance, const juce::String& platform)
{
    juce::StringPairArray parameters;
    parameters.set("serial_key", key);
    parameters.set("instance", instance);
    parameters.set("platform", platform);
    parameters.set("email", email);
    return call("deactivate", parameters);
}

LicenseReply WonderlandLicenseServer::versionCheck(const juce::String& key, const juce::String& email)
{
    juce::StringPairArray parameters;
    parameters.set("serial_key", key);
    parameters.set("email", email);
    return call("version_check", parameters);
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

LicenseReply WonderlandLicenseServer::call(const juce::String& route, const juce::StringPairArray& parameters)
{
    LicenseReply reply;

    juce::URL url(serverAddress + route);
    url = url.withParameter("product_id", productId);

    for (auto& name : parameters.getAllKeys())
        if (parameters[name].isNotEmpty())
            url = url.withParameter(name, parameters[name]);

    juce::WebInputStream stream(url, false);
    stream.withConnectionTimeout(timeoutMs);

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
        reply.reachable = reply.code.isNotEmpty();
    }

    return reply;
}
