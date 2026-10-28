/*
  ==============================================================================

    PresetManager.cpp
    Created: 27 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-27] New file. See PresetManager.h.

  ==============================================================================
*/

#include "PresetManager.h"

namespace
{
    const juce::String fileExtension { ".tertiarypreset" };
    const juce::String noPresetName  { "No preset" };
}

//==============================================================================
PresetManager::PresetManager(juce::AudioProcessorValueTreeState& state, juce::File folder)
    : apvts(state), userFolder(folder)
{
    currentName = getFactoryPresets()[0].name;
    refreshUserPresets();
    rememberLoadedValues();   // a new instance has the defaults, which are "Default"
}

/* Documents/Wonderland Audio/Tertiary/Presets, on Windows and Mac */
juce::File PresetManager::getDefaultUserFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
               .getChildFile("Wonderland Audio").getChildFile("Tertiary").getChildFile("Presets");
}

/* The scope and spectrum view settings are not part of a preset */
bool PresetManager::isSoundParameter(const juce::String& id)
{
    return ! (id.startsWith("Show") || id.startsWith("Stack") || id.startsWith("Scope") || id.startsWith("FFT"));
}

//==============================================================================
int PresetManager::getNumPresets()
{
    const juce::ScopedLock sl(lock);
    return (int) getFactoryPresets().size() + userFiles.size();
}

juce::String PresetManager::getPresetName(int index)
{
    const juce::ScopedLock sl(lock);
    const int numFactory = (int) getFactoryPresets().size();

    if (index >= 0 && index < numFactory)
        return getFactoryPresets()[(size_t) index].name;
    if (index >= numFactory && index < numFactory + userFiles.size())
        return userFiles[index - numFactory].getFileNameWithoutExtension();
    return {};
}

bool PresetManager::isUserPreset(int index)
{
    return index >= (int) getFactoryPresets().size() && index < getNumPresets();
}

int PresetManager::getCurrentIndex()
{
    const juce::ScopedLock sl(lock);
    return currentIndex;
}

juce::String PresetManager::getCurrentName()
{
    const juce::ScopedLock sl(lock);
    return currentIndex < 0 ? noPresetName : currentName;
}

bool PresetManager::isModified()
{
    std::map<juce::String, float> loaded;
    {
        const juce::ScopedLock sl(lock);
        if (currentIndex < 0)
            return false;
        loaded = loadedValues;
    }

    for (auto& [id, value] : loaded)
        if (auto* prm = apvts.getParameter(id))
            if (std::abs(prm->getValue() - value) > 1.0e-4f)
                return true;
    return false;
}

//==============================================================================
bool PresetManager::loadPreset(int index)
{
    const int numFactory = (int) getFactoryPresets().size();
    std::map<juce::String, float> values;
    juce::String name;

    if (index >= 0 && index < numFactory)
    {
        for (auto& [id, value] : getFactoryPresets()[(size_t) index].values)
            values[id] = value;
        name = getFactoryPresets()[(size_t) index].name;
    }
    else if (isUserPreset(index))
    {
        juce::File file;
        {
            const juce::ScopedLock sl(lock);
            file = userFiles[index - numFactory];
        }
        if (! file.existsAsFile())
            return false;
        values = readUserFile(file);
        name = file.getFileNameWithoutExtension();
    }
    else
        return false;

    apply(values);
    {
        const juce::ScopedLock sl(lock);
        currentIndex = index;
        currentName = name;
    }
    rememberLoadedValues();
    return true;
}

void PresetManager::loadNext()
{
    const int n = getNumPresets();
    loadPreset((juce::jmax(-1, getCurrentIndex()) + 1) % n);
}

void PresetManager::loadPrevious()
{
    const int n = getNumPresets();
    const int i = getCurrentIndex();
    loadPreset(i <= 0 ? n - 1 : i - 1);
}

/* Every sound parameter: the preset's value if it has one, else the default */
void PresetManager::apply(const std::map<juce::String, float>& values)
{
    for (auto* p : apvts.processor.getParameters())
    {
        auto* prm = dynamic_cast<juce::RangedAudioParameter*>(p);
        if (prm == nullptr || ! isSoundParameter(prm->getParameterID()))
            continue;

        auto found = values.find(prm->getParameterID());
        const float normalised = found != values.end() ? prm->convertTo0to1(found->second) : prm->getDefaultValue();

        prm->beginChangeGesture();
        prm->setValueNotifyingHost(normalised);
        prm->endChangeGesture();
    }
}

void PresetManager::rememberLoadedValues()
{
    std::map<juce::String, float> values;
    for (auto* p : apvts.processor.getParameters())
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*>(p))
            if (isSoundParameter(prm->getParameterID()))
                values[prm->getParameterID()] = prm->getValue();

    const juce::ScopedLock sl(lock);
    loadedValues = values;
}

//==============================================================================
juce::String PresetManager::cleanName(const juce::String& name)
{
    return juce::File::createLegalFileName(name.trim()).trim().substring(0, 64);
}

bool PresetManager::userPresetExists(const juce::String& name)
{
    return userFolder.getChildFile(cleanName(name) + fileExtension).existsAsFile();
}

void PresetManager::refreshUserPresets()
{
    juce::Array<juce::File> files;
    if (userFolder.isDirectory())
        files = userFolder.findChildFiles(juce::File::findFiles, false, "*" + fileExtension);

    std::sort(files.begin(), files.end(), [](const juce::File& a, const juce::File& b) {
        return a.getFileNameWithoutExtension().compareNatural(b.getFileNameWithoutExtension()) < 0;
    });

    const juce::ScopedLock sl(lock);
    juce::String keepName = currentName;
    bool wasUser = currentIndex >= (int) getFactoryPresets().size();
    userFiles = files;

    // The current user preset may have moved in the list (or gone)
    if (wasUser)
    {
        currentIndex = -1;
        for (int i = 0; i < userFiles.size(); ++i)
            if (userFiles[i].getFileNameWithoutExtension() == keepName)
                currentIndex = (int) getFactoryPresets().size() + i;
    }
}

std::map<juce::String, float> PresetManager::readUserFile(const juce::File& file)
{
    std::map<juce::String, float> values;
    if (auto xml = juce::parseXML(file))
        for (auto* p : xml->getChildWithTagNameIterator("PARAM"))
            values[p->getStringAttribute("id")] = (float) p->getDoubleAttribute("value");
    return values;
}

bool PresetManager::saveCurrentAsUser(const juce::String& name)
{
    const auto clean = cleanName(name);
    if (clean.isEmpty())
        return false;
    for (auto& f : getFactoryPresets())
        if (f.name.equalsIgnoreCase(clean))
            return false;

    juce::XmlElement xml("TertiaryPreset");
    xml.setAttribute("name", clean);
    xml.setAttribute("version", JucePlugin_VersionString);
    for (auto* p : apvts.processor.getParameters())
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*>(p))
            if (isSoundParameter(prm->getParameterID()))
            {
                auto* e = xml.createNewChildElement("PARAM");
                e->setAttribute("id", prm->getParameterID());
                e->setAttribute("value", prm->convertFrom0to1(prm->getValue()));
            }

    if (! userFolder.createDirectory())
        return false;
    auto file = userFolder.getChildFile(clean + fileExtension);
    if (! xml.writeTo(file))
        return false;

    {
        const juce::ScopedLock sl(lock);
        currentName = clean;
        currentIndex = (int) getFactoryPresets().size();   // corrected by refresh below
    }
    refreshUserPresets();
    rememberLoadedValues();
    return true;
}

bool PresetManager::renameUser(int index, const juce::String& newName)
{
    if (! isUserPreset(index))
        return false;
    const auto clean = cleanName(newName);
    if (clean.isEmpty() || userPresetExists(clean))
        return false;
    for (auto& f : getFactoryPresets())
        if (f.name.equalsIgnoreCase(clean))
            return false;

    juce::File file;
    bool isCurrent;
    {
        const juce::ScopedLock sl(lock);
        file = userFiles[index - (int) getFactoryPresets().size()];
        isCurrent = index == currentIndex;
    }
    auto target = userFolder.getChildFile(clean + fileExtension);
    if (! file.moveFileTo(target))
        return false;

    if (auto xml = juce::parseXML(target))   // keep the name inside the file in step
    {
        xml->setAttribute("name", clean);
        xml->writeTo(target);
    }

    if (isCurrent)
    {
        const juce::ScopedLock sl(lock);
        currentName = clean;
    }
    refreshUserPresets();
    return true;
}

bool PresetManager::deleteUser(int index)
{
    if (! isUserPreset(index))
        return false;

    juce::File file;
    {
        const juce::ScopedLock sl(lock);
        file = userFiles[index - (int) getFactoryPresets().size()];
    }
    if (! (useTrash && file.moveToTrash()) && ! file.deleteFile())
        return false;

    refreshUserPresets();   // if it was the current one, the bar shows "No preset" (the settings stay)
    return true;
}

//==============================================================================
void PresetManager::writeToSession(juce::ValueTree& state)
{
    const juce::ScopedLock sl(lock);
    if (currentIndex < 0)
    {
        state.removeProperty("presetName", nullptr);
        state.removeProperty("presetIsUser", nullptr);
        return;
    }
    state.setProperty("presetName", currentName, nullptr);
    state.setProperty("presetIsUser", currentIndex >= (int) getFactoryPresets().size(), nullptr);
}

/* Called after the session's parameters are restored. A 1.0.x session has no preset name: "No preset". */
void PresetManager::restoreFromSession(const juce::ValueTree& state)
{
    const auto name = state.getProperty("presetName").toString();
    const bool isUser = (bool) state.getProperty("presetIsUser", false);
    int index = -1;

    if (name.isNotEmpty())
    {
        refreshUserPresets();
        for (int i = 0; i < getNumPresets(); ++i)
            if (getPresetName(i) == name && isUserPreset(i) == isUser)
                index = i;
    }

    std::map<juce::String, float> values;
    if (index >= 0)
    {
        // What the preset itself holds, so the modified marker shows if the session changed it
        if (isUser)
        {
            juce::File file;
            {
                const juce::ScopedLock sl(lock);
                file = userFiles[index - (int) getFactoryPresets().size()];
            }
            auto inUnits = readUserFile(file);
            for (auto* p : apvts.processor.getParameters())
                if (auto* prm = dynamic_cast<juce::RangedAudioParameter*>(p))
                    if (isSoundParameter(prm->getParameterID()))
                    {
                        auto f = inUnits.find(prm->getParameterID());
                        values[prm->getParameterID()] = f != inUnits.end() ? prm->convertTo0to1(f->second) : prm->getDefaultValue();
                    }
        }
        else
        {
            std::map<juce::String, float> inUnits;
            for (auto& [id, v] : getFactoryPresets()[(size_t) index].values)
                inUnits[id] = v;
            for (auto* p : apvts.processor.getParameters())
                if (auto* prm = dynamic_cast<juce::RangedAudioParameter*>(p))
                    if (isSoundParameter(prm->getParameterID()))
                    {
                        auto f = inUnits.find(prm->getParameterID());
                        // Through convertFrom0to1 so the value is snapped the way the parameter snaps it
                        values[prm->getParameterID()] = f != inUnits.end()
                            ? prm->convertTo0to1(prm->convertFrom0to1(prm->convertTo0to1(f->second)))
                            : prm->getDefaultValue();
                    }
        }
    }

    const juce::ScopedLock sl(lock);
    currentIndex = index;
    currentName = index >= 0 ? name : juce::String();
    loadedValues = values;
}
