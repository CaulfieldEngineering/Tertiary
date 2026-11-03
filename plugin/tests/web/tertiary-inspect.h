// [Codex 2026-10-30] Reads the same existing test friends as native UiTest.cpp. Never mutates plugin state.
#include "PluginEditor.h"
struct UiTestAccess {
    static void append(juce::AudioProcessorEditor& base, juce::DynamicObject& result) {
        auto* e = dynamic_cast<audio_plugin::AudioPluginAudioProcessorEditor*>(&base);
        if (!e) return;
        result.setProperty("aboutVisible", e->aboutWindow.isVisible());
        result.setProperty("demo", e->audioProcessor.getDemoState());
        result.setProperty("activationBadge", !e->topBanner.mActivateArea.isEmpty());
        result.setProperty("licenseThreadRunning", e->audioProcessor.licenseCheck.isThreadRunning());
        juce::Array<juce::var> sizeOptions;
        auto menu = e->buildMainMenu();
        for (juce::PopupMenu::MenuItemIterator it(menu, true); it.next();)
        {
            const auto& item = it.getItem();
            if (item.itemID > 100)
            {
                auto* option = new juce::DynamicObject();
                option->setProperty("text", item.text);
                option->setProperty("enabled", item.isEnabled);
                option->setProperty("checked", item.isTicked);
                sizeOptions.add(juce::var(option));
            }
        }
        result.setProperty("sizeOptions", juce::var(sizeOptions));
        auto& scope = e->wrapperOscilloscope.oscilloscope;
        juce::Array<juce::var> scopes;
        for (auto* s : {&scope.lowScope, &scope.midScope, &scope.highScope}) {
            auto* o = new juce::DynamicObject(); juce::var v(o);
            o->setProperty("path", juce::String(s->lfoPath.toString().hashCode64()));
            o->setProperty("rate", s->localLFO.getWaveRate());
            o->setProperty("bypassed", s->isBandBypassed);
            o->setProperty("visible", s->isVisible());
            o->setProperty("colour", s->strokeColor.toString());
            scopes.add(v);
        }
        result.setProperty("scopes", juce::var(scopes));
        result.setProperty("fftVisible", e->wrapperFrequency.mShouldShowFFT);
        float fftPeak = 0.0f;
        for (float value : e->wrapperFrequency.audioProcessor.scopeData)
            fftPeak = juce::jmax(fftPeak, value);
        result.setProperty("fftPeak", fftPeak);
        result.setProperty("aboutBuildVisible", e->aboutWindow.isVisible() && e->aboutWindow.mShowBuildId);
        auto r = e->getLocalArea(&e->aboutWindow, e->aboutWindow.getLocalBounds());
        auto* a = new juce::DynamicObject(); juce::var av(a);
        a->setProperty("x", r.getX()); a->setProperty("y", r.getY());
        a->setProperty("width", r.getWidth()); a->setProperty("height", r.getHeight());
        result.setProperty("aboutBounds", av);
    }
};
