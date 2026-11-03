// [Codex 2026-10-30] Test-only, read-only inspection. Compiled into test builds, never deployed.
// No parameter setters, timer overrides or DSP changes. Playwright supplies every UI gesture.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <emscripten.h>
#if __has_include("tertiary-inspect.h")
#include "tertiary-inspect.h"
#endif
#if __has_include("build_id.h")
#include "build_id.h"
#else
#define WLA_BUILD_ID "unknown"
#endif

namespace {
using namespace juce;
AudioProcessorEditor* editorIn(Component* c) {
    if (auto* e = dynamic_cast<AudioProcessorEditor*>(c)) return e;
    for (auto* child : c->getChildren()) if (auto* e = editorIn(child)) return e;
    return nullptr;
}
void visit(Component& c, Component& editor, const String& key, Array<var>& list) {
    auto* o = new DynamicObject();
    var v(o);
    auto r = editor.getLocalArea(&c, c.getLocalBounds());
    o->setProperty("key", key); o->setProperty("name", c.getName());
    o->setProperty("id", c.getComponentID()); o->setProperty("visible", c.isShowing());
    o->setProperty("enabled", c.isEnabled());
    o->setProperty("alpha", c.getAlpha());
    o->setProperty("x", r.getX()); o->setProperty("y", r.getY());
    o->setProperty("width", r.getWidth()); o->setProperty("height", r.getHeight());
    String kind = "component";
    if (auto* s = dynamic_cast<Slider*>(&c)) {
        kind = "slider"; o->setProperty("value", s->getValue());
        o->setProperty("min", s->getMinimum()); o->setProperty("max", s->getMaximum());
        o->setProperty("text", s->getTextFromValue(s->getValue()));
        o->setProperty("style", (int)s->getSliderStyle());
    } else if (auto* b = dynamic_cast<Button*>(&c)) {
        kind = "button"; o->setProperty("text", b->getButtonText());
        o->setProperty("value", b->getToggleState()); o->setProperty("tooltip", b->getTooltip());
    } else if (auto* b = dynamic_cast<ComboBox*>(&c)) {
        kind = "combo"; o->setProperty("text", b->getText()); o->setProperty("value", b->getSelectedId());
        Array<var> items; for (int i = 0; i < b->getNumItems(); ++i) items.add(b->getItemText(i));
        o->setProperty("items", var(items));
    } else if (auto* l = dynamic_cast<Label*>(&c)) {
        kind = "label"; o->setProperty("text", l->getText());
    } else if (auto* t = dynamic_cast<TextEditor*>(&c)) {
        kind = "text"; o->setProperty("text", t->getText());
    }
#if __has_include("tertiary-inspect.h")
    if (dynamic_cast<Cursor*>(&c)) kind = "cursor";
    if (dynamic_cast<ScrollPad*>(&c)) kind = "scrollpad";
#endif
    o->setProperty("kind", kind); list.add(v);
    if (auto* a = c.getAccessibilityHandler()) o->setProperty("title", a->getTitle());
    for (int i = 0; i < c.getNumChildComponents(); ++i)
        visit(*c.getChildComponent(i), editor, key + "/" + String(i), list);
}
}
extern "C" EMSCRIPTEN_KEEPALIVE const char* webtest_snapshot() {
    static juce::String json;
    auto& d = juce::Desktop::getInstance();
    juce::AudioProcessorEditor* editor = nullptr;
    for (int i = 0; i < d.getNumComponents() && !editor; ++i) editor = editorIn(d.getComponent(i));
    if (!editor) return "{}";
    auto* o = new juce::DynamicObject(); juce::var result(o);
    juce::Array<juce::var> controls, params;
    visit(*editor, *editor, "editor", controls);
    for (int i = 0; i < d.getNumComponents(); ++i)
        if (!editorIn(d.getComponent(i))) visit(*d.getComponent(i), *editor, "popup/" + juce::String(i), controls);
    for (auto* p : editor->getAudioProcessor()->getParameters()) {
        auto* q = new juce::DynamicObject(); juce::var v(q);
        q->setProperty("name", p->getName(128)); q->setProperty("value", p->getValue());
        q->setProperty("text", p->getCurrentValueAsText());
        if (auto* id = dynamic_cast<juce::AudioProcessorParameterWithID*>(p)) q->setProperty("id", id->paramID);
        params.add(v);
    }
    o->setProperty("controls", juce::var(controls)); o->setProperty("parameters", juce::var(params));
    o->setProperty("width", editor->getWidth()); o->setProperty("height", editor->getHeight());
    auto editorArea = editor->getTopLevelComponent()->getLocalArea(editor, editor->getLocalBounds());
    o->setProperty("originX", editorArea.getX()); o->setProperty("originY", editorArea.getY());
    auto* processor = editor->getAudioProcessor();
    o->setProperty("program", processor->getCurrentProgram());
    o->setProperty("programName", processor->getProgramName(processor->getCurrentProgram()));
    juce::Array<juce::var> programs;
    for (int i = 0; i < processor->getNumPrograms(); ++i) programs.add(processor->getProgramName(i));
    o->setProperty("programs", juce::var(programs));
    o->setProperty("compiledBuildId", juce::String(juce::CharPointer_UTF8(WLA_BUILD_ID)));
#if __has_include("tertiary-inspect.h")
    UiTestAccess::append(*editor, *o);
#endif
    json = juce::JSON::toString(result, true);
    return json.toRawUTF8();
}
