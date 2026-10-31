// [Codex 2026-10-30] Reads the same existing test friends as native UiTest.cpp. Never mutates plugin state.
#include "PluginEditor.h"
struct UiTestAccess {
    static void append(juce::AudioProcessorEditor& base, juce::DynamicObject& result) {
        auto* e = dynamic_cast<audio_plugin::AudioPluginAudioProcessorEditor*>(&base);
        if (!e) return;
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
