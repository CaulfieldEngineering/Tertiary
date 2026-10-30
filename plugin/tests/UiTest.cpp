/*
  ==============================================================================

    UiTest.cpp
    Created: 29 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-29] New file. Joe, 2026-10-29: "Ultimately, the human testing i want to keep minimal.
    I'll check for feel but I have low confidence in my patience or ability to perform repeatable testing
    each time we push a release. So I want to check as much as possible automated prior."

    The UI test runner: Tertiary's real processor and real editor, in this program, in a window placed off
    the screen, driven the way a person drives it. Every step goes through the real components:
      - the mouse: moves, presses, drags, releases, double-clicks and modifier clicks (Ctrl+Shift) are fed to
        the window as the operating system would (ComponentPeer::handleMouseEvent), so JUCE does the hit
        testing, hover, capture and double-click detection itself. The pointer position the components read
        back (isMouseOver, the pop-up menus) is this program's pretend pointer, not the real mouse, which is
        never moved.
      - the keyboard: key presses go to the focused component (or, when the system gives this off-screen
        window no keyboard focus, straight to that component; said once in the log).
      - menus (the Menu (the icon at the top right), Window size, the Shape and Rhythm drop-downs, the preset menu) are opened by
        clicking and their items chosen through the menu's own accessibility actions, which is what a screen
        reader or a keyboard user does.
    After each step it checks the parameters, the components (visible, enabled, colour), what the scope draws,
    and the audio: a short render through the processor, measuring the LFO rate, depth and level.

    Nothing touches the real settings file, the real preset folder, the network, the browser or Explorer:
    see source/TestHooks.h (only this target defines WLA_UI_TEST_BUILD).

        UiTest                          every scenario
        UiTest --only U-15              one scenario (or several, comma separated; a test-plan ID like R-06 works too)
        UiTest --snapshots <folder>     also save PNGs of the window at each step that has a picture
        UiTest --cycles 200             how many times the window is opened and closed in U-24 (default 200)
        UiTest --list                   the scenarios and the test-plan IDs they cover

    Each check prints one line: PASS or FAIL, the scenario, the test-plan IDs it covers, what was done, and
    for a failure why. Exit code 0 when nothing failed. A crash or a hang names the scenario it happened in.

  ==============================================================================
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetManager.h"
#include "PretendServer.h"
#include "ui/UiRobot.h"   // the generic part (mouse, keys, menus, checks, crash and hang reports); one .cpp only
#if __has_include("build_id.h")
 #include "build_id.h"   // [Claude 2026-10-30] WLA_BUILD_ID, to check what the About window copies
#else
 #define WLA_BUILD_ID "unknown"
#endif

//==============================================================================
// Private state the checks read, through the friend declared in each class (source/TestHooks.h)
//==============================================================================
struct UiTestAccess
{
    using Editor = audio_plugin::AudioPluginAudioProcessorEditor;

    static GlobalControls& global (Editor& e)                     { return e.globalControls; }
    static TopBanner& banner (Editor& e)                          { return e.topBanner; }
    static AboutWindow& about (Editor& e)                         { return e.aboutWindow; }
    static PresetBar& presetBar (Editor& e)                       { return e.presetBar; }
    static WindowWrapperOscilloscope& scopeWrapper (Editor& e)    { return e.wrapperOscilloscope; }
    static WindowWrapperFrequency& freqWrapper (Editor& e)        { return e.wrapperFrequency; }
    static juce::Component& content (Editor& e)                   { return e.content; }
    static float uiScale (Editor& e)                              { return e.uiScale; }

    static Oscilloscope& oscilloscope (WindowWrapperOscilloscope& w)   { return w.oscilloscope; }
    static OptionsMenu& options (WindowWrapperOscilloscope& w)         { return w.optionsMenu; }
    static OptionsMenu& options (WindowWrapperFrequency& w)            { return w.optionsMenu; }

    static ScopeChannel& scope (Editor& e, int band)
    {
        auto& o = e.wrapperOscilloscope.oscilloscope;
        return band == 0 ? o.lowScope : band == 1 ? o.midScope : o.highScope;
    }
    static void tick (ScopeChannel& s)                  { static_cast<juce::Timer&> (s).timerCallback(); }
    static float drawnRate (ScopeChannel& s)            { return s.localLFO.getWaveRate(); }
    static float targetRate (ScopeChannel& s)           { return s.targetRate; }
    static LFO& drawnLfo (ScopeChannel& s)              { return s.localLFO; }
    static bool bypassed (ScopeChannel& s)              { return s.isBandBypassed; }
    static juce::Colour stroke (ScopeChannel& s)        { return s.strokeColor; }
    static double drawnBpm (ScopeChannel& s)            { return s.mHostBpm; }
    static float drawnZoom (ScopeChannel& s)            { return s.scrollZoom; }
    static float drawnCentre (ScopeChannel& s)          { return s.scrollCenter; }
    static const juce::Path& path (ScopeChannel& s)     { return s.lfoPath; }

    static bool synced (BandControl& b)                 { return b.isSyncedToHost; }

    static Cursor& cursor (FrequencyResponse& f, int which)   // 0 low-mid, 1 mid-high, 2..4 low/mid/high gain
    {
        switch (which)
        {
            case 0: return f.newCursorLM;
            case 1: return f.newCursorMH;
            case 2: return f.newCursorLowGain;
            case 3: return f.newCursorMidGain;
            default: return f.newCursorHighGain;
        }
    }
    static juce::Label& freqLabel (FrequencyResponse& f, int which) { return which == 0 ? f.newFreqLabelLow.mLabel : f.newFreqLabelHigh.mLabel; }

    static juce::TextButton& optionsButton (OptionsMenu& m)  { return m.mButtonOptions; }
    static bool optionsOpen (OptionsMenu& m)                 { return m.shouldShowDropdown; }
    static juce::ToggleButton* option (OptionsMenu& m, const juce::String& title)
    {
        for (auto* o : m.optionsArray)
            if (o->optionTitle == title)
                return &o->toggleButton;
        return nullptr;
    }

    static bool buildIdShown (AboutWindow& a)                     { return a.mShowBuildId; }
    static juce::TextEditor& keyField (AboutWindow& a)            { return a.mTextEditorSerialKey; }
    static juce::TextEditor& emailField (AboutWindow& a)          { return a.mTextEditorEmailAddress; }
    static juce::TextButton& actionButton (AboutWindow& a)        { return a.mActionButton; }
    static juce::TextButton& closeButton (AboutWindow& a)         { return a.mButtonClose; }
    static juce::Label& status (AboutWindow& a)                   { return a.mLabelStatus; }
    static juce::HyperlinkButton& websiteLink (AboutWindow& a)    { return a.mButtonWebsiteLink; }
    static bool aboutDemo (AboutWindow& a)                        { return a.mIsDemoVersion; }
    static void setServer (AboutWindow& a, std::shared_ptr<LicenseServer> s) { a.licenseServer = std::move (s); }

    static juce::Rectangle<float> menuArea (TopBanner& t)         { return t.mMenuArea; }
    static juce::Rectangle<float> activateArea (TopBanner& t)     { return t.mActivateArea; }
    static bool bannerDemo (TopBanner& t)                         { return t.mIsDemoVersion; }
    static bool bannerBypassed (TopBanner& t)                     { return t.mDemoBypassed; }

    static juce::Button& previous (PresetBar& p)                  { return p.mButtonPrevious; }
    static juce::Button& next (PresetBar& p)                      { return p.mButtonNext; }
    static juce::Rectangle<int> nameArea (PresetBar& p)           { return p.nameArea; }
    static juce::String shownName (PresetBar& p)                  { return p.shownName; }
    static bool shownModified (PresetBar& p)                      { return p.shownModified; }

    static bool showingBypass (GlobalControls& g)                 { return g.mShowingBypass; }
};

namespace
{
using namespace uirobot;
using Processor = audio_plugin::AudioPluginAudioProcessor;
using Editor = audio_plugin::AudioPluginAudioProcessorEditor;
using A = UiTestAccess;

int openCloseCycles = 200;   // U-22; --cycles

//==============================================================================
// Tertiary's audio checks: a test tone in one band, through the processor
//==============================================================================
constexpr double sampleRate = 48000;
constexpr int blockSize = 480;     // 10 ms, as a host might
constexpr int envelopeStep = 96;   // the level is read every 2 ms

const char* bandNames[] = { "Low", "Mid", "High" };
const char* bandModes[] = { "LOW", "MID", "HIGH" };

juce::String bandParam (const char* stem, int band)   { return juce::String (stem) + " " + bandNames[band] + " Band"; }

/* What one band's LFO does to the sound: a test tone in that band only (DC for the low band, a sine in the
   middle of the mid band, a sine above the high crossover) through the processor, read as a level every 2 ms */
Envelope render (Processor& p, HostClock& clock, int band, double seconds, double settle = 0.3)
{
    const double lm = value (p, "Low-Mid Crossover Freq"), mh = value (p, "Mid-High Crossover Freq");
    const double tone = band == 0 ? 0.0 : band == 1 ? std::sqrt (lm * mh) : juce::jmin (mh * 3.0, 15000.0);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    Envelope env;
    const int total = (int) ((settle + seconds) * sampleRate), skip = (int) (settle * sampleRate);
    double phase = 0;
    float peak = 0;
    int inWindow = 0;
    for (int pos = 0; pos < total; pos += blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const float x = tone == 0.0 ? 0.5f : 0.5f * (float) std::sin (phase);
            phase += juce::MathConstants<double>::twoPi * tone / sampleRate;
            buffer.setSample (0, i, x);
            buffer.setSample (1, i, x);
        }
        clock.sample += blockSize;
        p.processBlock (buffer, midi);
        if (clock.playing) clock.ppq += blockSize / sampleRate * clock.bpm / 60.0;
        for (int i = 0; i < blockSize; ++i)
        {
            if (pos + i < skip) continue;
            peak = juce::jmax (peak, std::abs (buffer.getSample (0, i)));
            if (++inWindow == envelopeStep) { env.level.push_back (peak); peak = 0; inWindow = 0; }
        }
    }
    return env;
}

/* A component's area in the editor's own pixels (the window is scaled, so this is where it really is) */
juce::Rectangle<int> areaInEditor (Editor& editor, juce::Component& c)
{
    return areaIn (editor, c);
}

//==============================================================================
// One processor with its window
//==============================================================================
float savedUiScale()
{
    juce::PropertiesFile props (LicenseCheck::getSettingsOptions());
    return props.containsKey ("uiScale") ? (float) props.getDoubleValue ("uiScale") : 0.f;
}

void saveUiScale (float scale)
{
    juce::PropertiesFile props (LicenseCheck::getSettingsOptions());
    props.setValue ("uiScale", (double) scale);
    props.saveIfNeeded();
}

struct Rig
{
    HostClock clock;
    std::unique_ptr<Processor> proc;
    std::unique_ptr<juce::AudioProcessorEditor> window;

    explicit Rig (bool demo = false, const juce::MemoryBlock* state = nullptr)
    {
        proc = std::make_unique<Processor>();
        proc->presets.useTrash = false;   // a deleted test preset is just deleted
        proc->setPlayHead (&clock);
        proc->setRateAndBufferSizeDetails (sampleRate, blockSize);
        proc->prepareToPlay (sampleRate, blockSize);
        if (state != nullptr)
            proc->setStateInformation (state->getData(), (int) state->getSize());
        proc->setDemoState (demo);
    }

    ~Rig() { close(); proc.reset(); pump (5); }

    Editor& open (float scale = 1.f)
    {
        if (scale > 0.f) saveUiScale (scale);
        window.reset (proc->createEditorIfNeeded());
        window->addToDesktop (0);
        window->setTopLeftPosition (offScreen());
        window->setVisible (true);
        pump (30);
        settle();
        return editor();
    }

    void close()
    {
        if (window != nullptr)
        {
            mouse.leave (*window);
            window.reset();
            pump (10);
        }
    }

    Editor& editor()   { return *dynamic_cast<Editor*> (window.get()); }

    /* Lets the window catch up: async messages, a frame of every scope's timer, and a paint (which also lays out
       the parts that are placed while painting, as a real window does before anyone can click it) */
    void settle (int scopeFrames = 1)
    {
        pump (10);
        for (int f = 0; f < scopeFrames; ++f)
            for (int b = 0; b < 3; ++b)
                A::tick (A::scope (editor(), b));
        picture (*window);
        pump (5);
    }

    BandControl& band (int b)
    {
        auto& g = A::global (editor());
        return b == 0 ? g.getLowControl() : b == 1 ? g.getMidControl() : g.getHighControl();
    }

    Envelope listen (int b, double seconds = 2.0, double settle = 0.3)   { return render (*proc, clock, b, seconds, settle); }
};

/* Set-up for listening to one band: crossovers far apart (120 Hz and 5 kHz), so each band's test tone sits well
   inside its band and the other bands' filters don't let it through. Set directly: this is the starting state, not
   a step under test (the crossover's own controls are tested in U-07). */
void separateBands (Rig& rig)
{
    setValue (*rig.proc, "Mid-High Crossover Freq", 5000.f);
    setValue (*rig.proc, "Low-Mid Crossover Freq", 120.f);
    rig.settle();
}

/* Where a horizontal slider shows a given value, as a point in the slider */
juce::Point<float> pointForProportion (juce::Slider& s, double proportion)
{
    return { (float) (proportion * s.getWidth()), s.getHeight() * 0.5f };
}

/* Drags a value slider (the band rows) from 20 % of its length to the given proportion; returns the parameter's
   value expected there */
double dragSliderTo (Rig& rig, juce::Slider& s, double proportion)
{
    mouse.dragPath (s, pointForProportion (s, 0.2), pointForProportion (s, proportion), 10);
    rig.settle();
    return s.proportionOfLengthToValue (proportion);
}

/* Clicks a drop-down and chooses an item by name */
bool chooseFromComboBox (Rig& rig, juce::ComboBox& box, const juce::String& item)
{
    mouse.click (box);
    if (! waitForMenu())
        return false;
    const bool chosen = chooseMenuItem (item);
    waitFor ([&] { return box.getText() == item; }, 2000);   // the drop-down shows it once the choice has arrived
    rig.settle();
    return chosen;
}

/* Opens the Menu (the icon at the top right) by clicking its icon in the header */
bool openMainMenu (Rig& rig)
{
    auto& banner = A::banner (rig.editor());
    const auto area = A::menuArea (banner);
    if (area.isEmpty())
        return false;
    mouse.click (banner, area.getCentre());
    const bool ok = waitForMenu();
    return ok;
}

bool chooseWindowSize (Rig& rig, int percent)
{
    if (! openMainMenu (rig)) return false;
    if (! chooseMenuItem ("Window size")) { closeMenus(); return false; }
    const bool ok = chooseMenuItem (juce::String (percent) + " %");
    pump (30);
    rig.settle();
    closeMenus();
    return ok;
}

/* The height of a band's drawn wave at the scope's centre gridline (its origin), or dx pixels from it */
float waveYAt (ScopeChannel& s, float dx)
{
    const float cx = s.getLocalBounds().toFloat().getCentreX() + dx;
    juce::PathFlatteningIterator it (A::path (s));
    while (it.next())
        if ((it.x1 - cx) * (it.x2 - cx) <= 0.f && it.x1 != it.x2)
            return it.y1 + (it.y2 - it.y1) * (cx - it.x1) / (it.x2 - it.x1);
    return std::numeric_limits<float>::quiet_NaN();
}

void resetTestFolder()
{
    TestHooks::testFolder().deleteRecursively();
    TestHooks::testFolder().createDirectory();
    TestHooks::opened().clear();
}

//==============================================================================
// The scenarios
//==============================================================================

// U-01  The window at 100, 125, 150 and 200 %
void windowAtEachScale()
{
    for (float scale : { 1.f, 1.25f, 1.5f, 2.f })
    {
        Rig rig;
        auto& ed = rig.open (scale);
        const auto pct = juce::String (juce::roundToInt (scale * 100)) + " %";
        check (ed.getWidth() == juce::roundToInt (750 * scale) && ed.getHeight() == juce::roundToInt (478 * scale),
               "opens at " + pct + " from the saved size", juce::String (ed.getWidth()) + " x " + juce::String (ed.getHeight()),
               "expected " + juce::String (juce::roundToInt (750 * scale)) + " x " + juce::String (juce::roundToInt (478 * scale)));
        auto& content = A::content (ed);
        const auto shown = ed.getLocalArea (&content, content.getLocalBounds());
        check (closeTo (shown.getWidth(), ed.getWidth(), 1.0) && closeTo (shown.getHeight(), ed.getHeight(), 1.0),
               "the whole layout fills the window at " + pct, fmt (shown.getWidth(), 1) + " x " + fmt (shown.getHeight(), 1));
        bool allShowing = true;
        juce::String missing;
        for (juce::Component* c : { (juce::Component*) &A::banner (ed), (juce::Component*) &A::global (ed), (juce::Component*) &A::scopeWrapper (ed),
                                    (juce::Component*) &A::freqWrapper (ed), (juce::Component*) &A::presetBar (ed) })
            if (! c->isShowing() || c->getWidth() <= 0) { allShowing = false; missing << c->getName() << " "; }
        check (allShowing, "header, preset bar, scope, crossover and controls all showing at " + pct, {}, "not showing: " + missing);
        check (! A::about (ed).isVisible(), "the About window starts closed at " + pct);
        {
            // [Claude 2026-10-30] R-16: the preset name was filled in on the bar's first timer tick, so a window opened
            // with an empty name box for up to 100 ms (and this test's pictures caught it at random). A new bar, before
            // any message is handled, must already show the current preset.
            PresetBar fresh (*rig.proc);
            check (A::shownName (fresh) == rig.proc->presets.getCurrentName() && A::shownName (fresh).isNotEmpty(),
                   "the preset name shows the moment the window opens (R-16) at " + pct, A::shownName (fresh));
        }
        snapshot (ed, "window_" + juce::String (juce::roundToInt (scale * 100)));
    }
}

// U-24  [Claude 2026-10-30] RC1 bug (Joe, Cubase on his laptop): the window opened nearly full screen, black, with
// the UI at its size in the corner. The editor in a stand-in for the plug-in wrapper (as JUCE's VST3 wrapper holds
// it), opened with the host's scale already set (1.25 and 1.5, as a DAW on a scaled screen does), at each window
// size: the editor is exactly the layout times the chosen size, the layout fills it, and the wrapper is that times
// the host's scale. Then the host gives the window another size (JUCE's wrapper passes it to the editor): the layout
// still fills it, and the size chosen from the menu is neither changed nor remembered as the host's (RC1 remembered
// it, so every later window opened at the host's size). The real wrapper is HostTest section 10. (Joe's full-screen
// window itself was a maximized-window record in his Cubase preferences, left by a local resizable build.)
struct WrapperStandIn : juce::Component
{
    juce::AudioProcessorEditor* editor = nullptr;
    bool resizingChild = false;

    void fitEditor()   { setSize (getLocalArea (editor, editor->getLocalBounds()).getSmallestIntegerContainer().getWidth(),
                                  getLocalArea (editor, editor->getLocalBounds()).getSmallestIntegerContainer().getHeight()); }
    void childBoundsChanged (juce::Component*) override   { if (! resizingChild) fitEditor(); }   // asks the host, which says yes
    void resized() override
    {
        if (editor == nullptr) return;
        const juce::ScopedValueSetter<bool> s (resizingChild, true);
        editor->setBounds (editor->getLocalArea (this, getLocalBounds()).withZeroOrigin());
    }
    void paint (juce::Graphics& g) override   { g.fillAll (juce::Colours::black); }
};

void hostScaleFirstOpen()
{
    for (float hostScale : { 1.25f, 1.5f })
        for (float scale : { 1.f, 1.25f, 1.5f, 2.f })
        {
            Rig rig;
            saveUiScale (scale);
            WrapperStandIn wrapper;
            rig.window.reset (rig.proc->createEditorIfNeeded());
            wrapper.editor = rig.window.get();
            rig.window->setScaleFactor (hostScale);   // before the window is shown, as the VST3 wrapper does
            wrapper.addAndMakeVisible (*rig.window);
            wrapper.fitEditor();
            wrapper.addToDesktop (0);
            wrapper.setTopLeftPosition (offScreen());
            wrapper.setVisible (true);
            pump (30);
            rig.settle();
            auto& ed = rig.editor();
            const auto at = juce::String (juce::roundToInt (scale * 100)) + " %, host scale " + fmt (hostScale, 2);
            const int w = juce::roundToInt (750 * scale), h = juce::roundToInt (478 * scale);
            auto sizeOf = [] (juce::Component& c) { return juce::String (c.getWidth()) + " x " + juce::String (c.getHeight()); };
            auto fills = [&] {
                auto& content = A::content (ed);
                const auto shown = wrapper.getLocalArea (&content, content.getLocalBounds());
                return closeTo (shown.getWidth(), wrapper.getWidth(), 1.0) && closeTo (shown.getHeight(), wrapper.getHeight(), 1.0);
            };
            auto wrapperRight = [&] { return closeTo (wrapper.getWidth(), w * hostScale, 3.0) && closeTo (wrapper.getHeight(), h * hostScale, 3.0); };

            // within 2 pixels: JUCE's wrapper rounds the editor's size at a fractional host scale
            auto editorRight = [&] { return std::abs (ed.getWidth() - w) <= 2 && std::abs (ed.getHeight() - h) <= 2; };
            check (editorRight(), "opens at the layout times the chosen size: " + at,
                   sizeOf (ed), "expected " + juce::String (w) + " x " + juce::String (h));
            check (wrapperRight() && fills(), "the window is that times the host's scale, filled by the layout: " + at, sizeOf (wrapper));

            // the host sizes the window and JUCE's wrapper passes it on to the editor
            wrapper.setSize (wrapper.getWidth() * 2, wrapper.getHeight() * 2);
            pump (100);
            check (fills(), "sized otherwise by the host: the layout fills the window: " + at, sizeOf (ed) + ", window " + sizeOf (wrapper));

            check (closeTo (A::uiScale (ed), scale, 0.001), "the chosen size is unchanged: " + at, fmt (A::uiScale (ed), 3));
            rig.close();
            check (closeTo (savedUiScale(), scale, 0.001), "and that is what is remembered: " + at, fmt (savedUiScale(), 3));
            auto& again = rig.open (0.f);   // 0: what was remembered
            check (again.getWidth() == w && again.getHeight() == h, "opened again: the chosen size, not the host's: " + at, sizeOf (again));
        }
}

// U-02  Window size from the Menu (the icon at the top right), and remembered
void windowSizeMenu()
{
    Rig rig;
    rig.open (1.f);
    for (int pct : { 125, 150, 200, 100 })
    {
        const bool chosen = chooseWindowSize (rig, pct);
        auto& ed = rig.editor();
        check (chosen, "Menu > Window size > " + juce::String (pct) + " % found and chosen", {}, "menu items: " + menuTitles().joinIntoString (", "));
        check (ed.getWidth() == juce::roundToInt (7.5f * pct) && ed.getHeight() == juce::roundToInt (4.78f * pct),
               "the window is " + juce::String (pct) + " %", juce::String (ed.getWidth()) + " x " + juce::String (ed.getHeight()));
        check (closeTo (savedUiScale(), pct / 100.0, 0.001), "the size is saved for next time", "saved " + fmt (savedUiScale(), 2));
        if (openMainMenu (rig) && chooseMenuItem ("Window size"))
        {
            check (menuItemTicked (juce::String (pct) + " %"), "the menu ticks " + juce::String (pct) + " %");
            closeMenus();
        }
        else check (false, "the menu opens again to show the tick");
        rig.settle();
    }
    chooseWindowSize (rig, 150);
    rig.close();
    auto& reopened = rig.open (0.f);   // 0: don't write a size first, use what the menu saved
    check (reopened.getWidth() == 1125 && reopened.getHeight() == 717, "closed and opened again: still 150 %",
           juce::String (reopened.getWidth()) + " x " + juce::String (reopened.getHeight()));
    snapshot (reopened, "reopened_150");
}

// U-03  Depth, skew, phase and gain sliders on each band, dragged
void bandSliders()
{
    for (float scale : { 1.f, 1.5f })
    {
        Rig rig;
        rig.open (scale);
        const auto at = " at " + juce::String (juce::roundToInt (scale * 100)) + " %";
        for (int b = 0; b < 3; ++b)
        {
            auto& band = rig.band (b);
            struct Row { const char* name; CustomSlider* slider; const char* stem; };
            Row rows[] = { { "Depth", &band.mSliderDepth, "Depth" }, { "Skew", &band.mSliderSkew, "Skew" },
                           { "Phase", &band.mSliderPhase, "Relative Phase" }, { "Gain", &band.mSliderBandGain, "Output Gain" } };
            for (auto& row : rows)
            {
                const auto id = juce::String (row.stem) == "Output Gain" ? juce::String (bandNames[b]) + " Output Gain" : bandParam (row.stem, b);
                auto* prm = param (*rig.proc, id);
                const auto range = prm->getNormalisableRange();
                for (double p : { 0.8, 0.35 })
                {
                    const double expected = dragSliderTo (rig, row.slider->slider, p);
                    const double got = value (*rig.proc, id);
                    check (closeTo (got, expected, 0.03 * (range.end - range.start)),
                           juce::String (bandNames[b]) + " " + row.name + ": dragged to " + juce::String (juce::roundToInt (p * 100)) + " % of the slider" + at,
                           "parameter " + fmt (got) + ", slider shows " + fmt (expected),
                           "the parameter did not follow the drag");
                    check (row.slider->label.getText().startsWith (juce::String (row.slider->slider.getValue()).substring (0, 3)),
                           juce::String (bandNames[b]) + " " + row.name + ": the value label shows it", row.slider->label.getText());
                }
            }
            // what the scope draws follows the depth and skew
            auto& scope = A::scope (rig.editor(), b);
            check (closeTo (A::drawnLfo (scope).getWaveDepth(), value (*rig.proc, bandParam ("Depth", b)), 0.6),
                   juce::String (bandNames[b]) + ": the scope draws the new depth", fmt (A::drawnLfo (scope).getWaveDepth()));
            check (closeTo (A::drawnLfo (scope).getWaveSkew(), value (*rig.proc, bandParam ("Skew", b)), 0.6),
                   juce::String (bandNames[b]) + ": the scope draws the new skew", fmt (A::drawnLfo (scope).getWaveSkew()));
        }
    }

    // and it is heard: depth changes how deep the tremolo goes, gain changes the level
    for (int b = 0; b < 3; ++b)
    {
        Rig rig;
        rig.open (1.f);
        separateBands (rig);
        auto& band = rig.band (b);
        chooseFromComboBox (rig, band.mDropWaveshape, "Sine");
        dragSliderTo (rig, band.mSliderDepth.slider, 0.999);
        const auto deep = rig.listen (b, 1.5);
        dragSliderTo (rig, band.mSliderDepth.slider, 0.25);
        const auto shallow = rig.listen (b, 1.5);
        check (deep.modulation() > 0.8 && shallow.modulation() < deep.modulation() * 0.6,
               juce::String (bandNames[b]) + ": depth heard: full depth vs a quarter",
               "modulation " + fmt (deep.modulation()) + " vs " + fmt (shallow.modulation()),
               "the audio's modulation depth didn't follow the slider");
        const double before = rig.listen (b, 1.0).mean();
        const double gainBefore = value (*rig.proc, juce::String (bandNames[b]) + " Output Gain");
        dragSliderTo (rig, band.mSliderBandGain.slider, 0.8);
        const double gainAfter = value (*rig.proc, juce::String (bandNames[b]) + " Output Gain");
        const double after = rig.listen (b, 1.0).mean();
        const double heardDb = juce::Decibels::gainToDecibels (after / juce::jmax (1.0e-9, before));
        check (closeTo (heardDb, gainAfter - gainBefore, 1.0),
               juce::String (bandNames[b]) + ": band gain heard", "slider moved " + fmt (gainAfter - gainBefore, 1) + " dB, heard " + fmt (heardDb, 1) + " dB");
    }
}

// U-04  Shape and invert on each band
void bandShapeAndInvert()
{
    Rig rig;
    rig.open (1.25f);
    auto* wave = dynamic_cast<juce::AudioParameterChoice*> (param (*rig.proc, bandParam ("Waveshape", 0)));
    for (int b = 0; b < 3; ++b)
    {
        auto& band = rig.band (b);
        for (int i = 0; i < wave->choices.size(); ++i)
        {
            const auto name = wave->choices[i];
            const bool chosen = chooseFromComboBox (rig, band.mDropWaveshape, name);
            const int got = juce::roundToInt (value (*rig.proc, bandParam ("Waveshape", b)));
            check (chosen && got == i, juce::String (bandNames[b]) + " Shape menu: " + name,
                   "parameter " + wave->choices[got] + ", scope draws " + juce::String (A::drawnLfo (A::scope (rig.editor(), b)).getWaveform()),
                   chosen ? "the parameter didn't change" : "the menu item wasn't there: " + menuTitles().joinIntoString (", "));
            check (A::drawnLfo (A::scope (rig.editor(), b)).getWaveform() == i, juce::String (bandNames[b]) + " Shape " + name + ": the scope draws it");
        }
        const bool before = value (*rig.proc, bandParam ("Invert", b)) > 0.5f;
        mouse.click (band.mToggleInvert.mToggleButton);
        rig.settle();
        check ((value (*rig.proc, bandParam ("Invert", b)) > 0.5f) != before && band.mToggleInvert.mToggleButton.getToggleState() != before,
               juce::String (bandNames[b]) + " INV: one click flips it");
        check (A::drawnLfo (A::scope (rig.editor(), b)).getWaveInvert() == (before ? 0 : 1), juce::String (bandNames[b]) + " INV: the scope draws it inverted");
    }
}

// U-05  Rate: free (slider) and synced (Rhythm menu), each band, heard at the right speed
void bandRates()
{
    Rig rig;
    rig.open (1.f);
    separateBands (rig);
    auto* mult = dynamic_cast<juce::AudioParameterChoice*> (param (*rig.proc, bandParam ("Multiplier", 0)));
    for (int b = 0; b < 3; ++b)
    {
        auto& band = rig.band (b);
        chooseFromComboBox (rig, band.mDropWaveshape, "Sine");
        dragSliderTo (rig, band.mSliderDepth.slider, 0.999);

        // synced (the default): the Rhythm menu, heard at 120 bpm
        check (value (*rig.proc, bandParam ("Sync to Host", b)) > 0.5f && band.mDropRhythm.isVisible() && ! band.mSliderRate.isVisible(),
               juce::String (bandNames[b]) + ": synced by default, the Rhythm menu shows, the rate slider is hidden");
        for (auto [name, hz] : { std::pair<const char*, double> { "1/4 Note", 2.0 }, { "1/2 Note", 1.0 } })
        {
            const bool chosen = chooseFromComboBox (rig, band.mDropRhythm, name);
            const int got = juce::roundToInt (value (*rig.proc, bandParam ("Multiplier", b)));
            check (chosen && mult->choices[got] == name, juce::String (bandNames[b]) + " Rhythm menu: " + name, "parameter " + mult->choices[got]);
            const double heard = rig.listen (b, 3.0).frequency();
            check (closeTo (heard, hz, 0.06 * hz), juce::String (bandNames[b]) + " " + name + " at 120 bpm is heard at " + fmt (hz, 1) + " Hz",
                   "measured " + fmt (heard) + " Hz");
        }

        // free: Sync off, the slider shows and is heard
        mouse.click (band.mToggleSync.mToggleButton);
        rig.settle();
        check (value (*rig.proc, bandParam ("Sync to Host", b)) < 0.5f && band.mSliderRate.isVisible() && band.mSliderRate.isEnabled()
                   && ! band.mDropRhythm.isVisible() && ! A::synced (band),
               juce::String (bandNames[b]) + " SYNC off: the rate slider shows and works, the Rhythm menu is hidden");
        for (double p : { 0.3, 0.7 })
        {
            const double expected = dragSliderTo (rig, band.mSliderRate.slider, p);
            const double rate = value (*rig.proc, bandParam ("Rate", b));
            check (closeTo (rate, expected, 0.05 * expected + 0.05), juce::String (bandNames[b]) + " free rate dragged", fmt (rate) + " Hz");
            const double heard = rig.listen (b, 3.0).frequency();
            check (closeTo (heard, rate, 0.06 * rate + 0.05), juce::String (bandNames[b]) + " free rate " + fmt (rate) + " Hz is heard at that speed",
                   "measured " + fmt (heard) + " Hz");
            for (int f = 0; f < 60; ++f) A::tick (A::scope (rig.editor(), b));
            check (closeTo (A::drawnRate (A::scope (rig.editor(), b)), rate, 0.01), juce::String (bandNames[b]) + " the scope draws " + fmt (rate) + " Hz",
                   "scope " + fmt (A::drawnRate (A::scope (rig.editor(), b)), 3));
        }
        // and back on
        mouse.click (band.mToggleSync.mToggleButton);
        rig.settle();
        check (value (*rig.proc, bandParam ("Sync to Host", b)) > 0.5f && band.mDropRhythm.isVisible() && ! band.mSliderRate.isVisible(),
               juce::String (bandNames[b]) + " SYNC on again: the Rhythm menu is back");
    }
}

// U-06  Bypass, mute and solo on each band: parameters, the crossover and the scope together, the sound
void bypassMuteSolo()
{
    Rig rig;
    auto& ed = rig.open (1.f);
    separateBands (rig);
    auto& freq = A::freqWrapper (ed).getFrequencyResponse();
    for (int b = 0; b < 3; ++b)
    {
        auto& band = rig.band (b);
        auto& scope = A::scope (ed, b);
        chooseFromComboBox (rig, band.mDropWaveshape, "Sine");
        dragSliderTo (rig, band.mSliderDepth.slider, 0.999);
        const auto colourBefore = A::stroke (scope);
        const auto pictureBefore = picture (ed);

        mouse.click (band.mToggleBypass.mToggleButton);
        rig.settle();
        const bool crossoverGrey = b == 0 ? freq.lowBandBypass : b == 1 ? freq.midBandBypass : freq.highBandBypass;
        check (value (*rig.proc, bandParam ("Bypassed", b)) > 0.5f, juce::String (bandNames[b]) + " BYP: on");
        check (crossoverGrey && A::bypassed (scope), juce::String (bandNames[b]) + " BYP: the crossover and the scope grey together (R-02)",
               juce::String ("crossover ") + (crossoverGrey ? "grey" : "colour") + ", scope " + (A::bypassed (scope) ? "grey" : "colour"));
        check (A::stroke (scope).getSaturation() < 0.25f * juce::jmax (0.05f, colourBefore.getSaturation()),
               juce::String (bandNames[b]) + " BYP: the scope's wave colour is grey", "saturation " + fmt (colourBefore.getSaturation()) + " -> " + fmt (A::stroke (scope).getSaturation()));
        const auto pictureAfter = picture (ed);
        const auto scopeArea = areaInEditor (ed, scope);
        check (saturation (pictureAfter, scopeArea) < saturation (pictureBefore, scopeArea),
               juce::String (bandNames[b]) + " BYP: the scope lane is drawn greyer", fmt (saturation (pictureBefore, scopeArea)) + " -> " + fmt (saturation (pictureAfter, scopeArea)));
        check (rig.listen (b, 1.0).modulation() < 0.05, juce::String (bandNames[b]) + " BYP: heard without tremolo", "modulation " + fmt (rig.listen (b, 1.0).modulation()));
        snapshot (ed, juce::String (bandNames[b]) + "_bypassed");
        mouse.click (band.mToggleBypass.mToggleButton);
        rig.settle();
        check (! A::bypassed (scope) && ! (b == 0 ? freq.lowBandBypass : b == 1 ? freq.midBandBypass : freq.highBandBypass)
                   && closeTo (A::stroke (scope).getSaturation(), colourBefore.getSaturation(), 0.02),
               juce::String (bandNames[b]) + " BYP off: colour back in both");

        mouse.click (band.mToggleMute.mToggleButton);
        rig.settle();
        check (value (*rig.proc, bandParam ("Mute", b)) > 0.5f && rig.listen (b, 0.5).highest() < 0.01f,
               juce::String (bandNames[b]) + " MUTE: on, and the band is silent", "peak " + fmt (rig.listen (b, 0.5).highest(), 4));
        snapshot (ed, juce::String (bandNames[b]) + "_muted");
        mouse.click (band.mToggleSolo.mToggleButton);
        rig.settle();
        check (value (*rig.proc, bandParam ("Solo", b)) > 0.5f && value (*rig.proc, bandParam ("Mute", b)) < 0.5f && ! band.mToggleMute.mToggleButton.getToggleState(),
               juce::String (bandNames[b]) + " SOLO: on, and it turns MUTE off (never both)");
        snapshot (ed, juce::String (bandNames[b]) + "_soloed");
        const int other = (b + 1) % 3;
        check (rig.listen (b, 0.5).highest() > 0.05f && rig.listen (other, 0.5).highest() < 0.01f,
               juce::String (bandNames[b]) + " SOLO: this band heard, " + bandNames[other] + " silent");
        mouse.click (band.mToggleMute.mToggleButton);
        rig.settle();
        check (value (*rig.proc, bandParam ("Mute", b)) > 0.5f && value (*rig.proc, bandParam ("Solo", b)) < 0.5f,
               juce::String (bandNames[b]) + " MUTE again: turns SOLO off");
        mouse.click (band.mToggleMute.mToggleButton);
        rig.settle();
        check (value (*rig.proc, bandParam ("Mute", b)) < 0.5f && value (*rig.proc, bandParam ("Solo", b)) < 0.5f, juce::String (bandNames[b]) + " all off again");
    }
}

// U-07  The crossover: drag both handles, the gain handles, type a frequency
void crossover()
{
    for (float scale : { 1.f, 2.f })
    {
        Rig rig;
        auto& ed = rig.open (scale);
        auto& freq = A::freqWrapper (ed).getFrequencyResponse();
        const auto at = " at " + juce::String (juce::roundToInt (scale * 100)) + " %";
        // the gain handles ("pills"): drag up raises that band's gain
        for (int b = 0; b < 3; ++b)
        {
            auto& handle = A::cursor (freq, 2 + b);
            if (handle.getWidth() <= 0) { check (false, juce::String (bandNames[b]) + " gain handle is on the crossover" + at); continue; }
            const auto id = juce::String (bandNames[b]) + " Output Gain";
            const double before = value (*rig.proc, id);
            const auto start = freq.getLocalPoint (&handle, handle.getLocalBounds().getCentre().toFloat());
            mouse.move (freq, start.translated (1.f, 0.f));
            mouse.move (freq, start);
            mouse.dragPath (freq, start, start.translated (0.f, -25.f), 10);
            rig.settle();
            const double after = value (*rig.proc, id);
            check (after > before + 1.0, juce::String (bandNames[b]) + " gain handle dragged up" + at, fmt (before, 1) + " dB -> " + fmt (after, 1) + " dB");
        }
        for (int which : { 1, 0 })
        {
            const auto id = which == 0 ? juce::String ("Low-Mid Crossover Freq") : juce::String ("Mid-High Crossover Freq");
            for (float dx : { 30.f, -40.f })
            {
                auto& handle = A::cursor (freq, which);
                const double before = value (*rig.proc, id);
                const auto start = freq.getLocalPoint (&handle, handle.getLocalBounds().getCentre().toFloat());
                mouse.move (freq, start.translated (0.f, 3.f));
                mouse.move (freq, start);
                mouse.dragPath (freq, start, start.translated (dx, 0.f), 10);
                rig.settle();
                const double after = value (*rig.proc, id);
                check (dx > 0 ? after > before * 1.05 : after < before / 1.05,
                       juce::String (which == 0 ? "Low-mid" : "Mid-high") + " crossover dragged " + (dx > 0 ? "right" : "left") + at,
                       fmt (before, 0) + " Hz -> " + fmt (after, 0) + " Hz", "the frequency didn't follow the drag");
                check (value (*rig.proc, "Low-Mid Crossover Freq") <= value (*rig.proc, "Mid-High Crossover Freq") + 0.5f,
                       "low-mid stays at or below mid-high");
            }
        }
        snapshot (ed, "crossover_dragged");
    }

    // typing a frequency: double-click the low-mid label, type, Enter
    Rig rig;
    auto& ed = rig.open (1.f);
    auto& freq = A::freqWrapper (ed).getFrequencyResponse();
    auto& label = A::freqLabel (freq, 0);
    mouse.doubleClick (label);
    pump (20);
    auto* field = label.getCurrentTextEditor();
    check (field != nullptr, "double-click the low-mid frequency: a text field opens");
    if (field != nullptr)
    {
        pressKey (*field, juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 'a'));
        typeText (*field, "350");
        pressKey (*field, juce::KeyPress (juce::KeyPress::returnKey));
        pump (30);
        check (closeTo (value (*rig.proc, "Low-Mid Crossover Freq"), 350.0, 2.0), "typed 350 and Enter: the crossover is 350 Hz",
               fmt (value (*rig.proc, "Low-Mid Crossover Freq"), 0) + " Hz");
    }
}

// U-08  A value typed into a band slider's label
void typedValue()
{
    Rig rig;
    rig.open (1.f);
    auto& depth = rig.band (1).mSliderDepth;
    mouse.doubleClick (depth.label);
    pump (20);
    auto* field = depth.label.getCurrentTextEditor();
    check (field != nullptr, "double-click Mid Depth's value: a text field opens");
    if (field != nullptr)
    {
        pressKey (*field, juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 'a'));
        typeText (*field, "42");
        pressKey (*field, juce::KeyPress (juce::KeyPress::returnKey));
        pump (30);
        check (closeTo (value (*rig.proc, "Depth Mid Band"), 42.0, 0.6), "typed 42 and Enter: Mid Depth is 42", fmt (value (*rig.proc, "Depth Mid Band")));
    }
}

// U-09  Input and output gain faders
void inputOutputFaders()
{
    Rig rig;
    rig.open (1.25f);
    auto& g = A::global (rig.editor());
    for (auto [fader, id] : { std::pair<InputOutputGain*, const char*> { &g.inputGain, "Input Gain" }, { &g.outputGain, "Output Gain" } })
    {
        auto& s = fader->sliderGain;
        const double before = value (*rig.proc, id);
        const auto from = s.getLocalBounds().getCentre().toFloat();
        mouse.dragPath (s, from, from.translated (0.f, -s.getHeight() * 0.25f), 10);
        rig.settle();
        const double up = value (*rig.proc, id);
        check (up > before + 1.0, juce::String (id) + " fader dragged up", fmt (before, 1) + " -> " + fmt (up, 1) + " dB");
        mouse.dragPath (s, from, from.translated (0.f, s.getHeight() * 0.25f), 10);
        rig.settle();
        check (value (*rig.proc, id) < up - 1.0, juce::String (id) + " fader dragged down", fmt (value (*rig.proc, id), 1) + " dB");
    }
}

// U-10  The scope's options and the crossover's options (their own small menus)
void displayOptions()
{
    Rig rig;
    auto& ed = rig.open (1.f);
    auto& scopeOptions = A::options (A::scopeWrapper (ed));
    for (auto [title, id] : { std::pair<const char*, const char*> { "Low", "Show Low Band In Scope" }, { "Mid", "Show Mid Band In Scope" },
                              { "High", "Show High Band In Scope" }, { "Stack Bands", "Stack All Bands" } })
    {
        mouse.click (A::optionsButton (scopeOptions));
        rig.settle();
        check (A::optionsOpen (scopeOptions), juce::String ("scope Options opens (for ") + title + ")");
        auto* toggle = A::option (scopeOptions, title);
        if (toggle == nullptr) { check (false, juce::String ("scope option ") + title + " exists"); continue; }
        const bool before = value (*rig.proc, id) > 0.5f;
        mouse.click (*toggle);
        rig.settle();
        check ((value (*rig.proc, id) > 0.5f) != before, juce::String ("scope option ") + title + " toggles " + id);
        mouse.click (A::optionsButton (scopeOptions));   // close it
        if (A::optionsOpen (scopeOptions)) mouse.click (A::optionsButton (scopeOptions));
        rig.settle();
        mouse.leave (ed);
        // back as it was
        mouse.click (A::optionsButton (scopeOptions));
        rig.settle();
        if (auto* t = A::option (scopeOptions, title)) mouse.click (*t);
        rig.settle();
        if (A::optionsOpen (scopeOptions)) mouse.click (A::optionsButton (scopeOptions));
        rig.settle();
        check ((value (*rig.proc, id) > 0.5f) == before, juce::String ("scope option ") + title + " toggled back");
    }
    auto& freqOptions = A::options (A::freqWrapper (ed));
    for (auto [title, id] : { std::pair<const char*, const char*> { "Show FFT", "Show FFT" }, { "FFT Pre", "FFT Pickoff Point" } })
    {
        mouse.click (A::optionsButton (freqOptions));
        rig.settle();
        auto* toggle = A::option (freqOptions, title);
        if (toggle == nullptr) { check (false, juce::String ("crossover option ") + title + " exists"); continue; }
        const bool before = value (*rig.proc, id) > 0.5f;
        mouse.click (*toggle);
        rig.settle();
        check ((value (*rig.proc, id) > 0.5f) != before, juce::String ("crossover option ") + title + " toggles " + id);
        if (A::optionsOpen (freqOptions)) mouse.click (A::optionsButton (freqOptions));
        rig.settle();
    }
}

// U-11  R-07: zoom is smooth: the range follows the mouse in proportion, the view eases
void smoothZoom()
{
    Rig rig;
    auto& ed = rig.open (1.f);
    auto& pad = A::oscilloscope (A::scopeWrapper (ed)).sliderScroll;
    auto& scope = A::scope (ed, 0);
    for (int i = 0; i < 40; ++i) A::tick (scope);
    const auto mid = juce::Point<float> ((pad.getPoint1() + pad.getPoint2()) / 2.f, pad.getHeight() / 2.f);

    // small and big moves: the range moves in proportion to the mouse (it was a fixed step per event)
    auto zoomAfterMoves = [&] (float stepPx, int steps)
    {
        std::vector<float> zooms;
        nextEventTime (1000);
        mouse.move (pad, mid);
        mouse.down (pad, mid);
        auto p = mid;
        for (int i = 0; i < steps; ++i)
        {
            p = p.translated (0.f, stepPx);
            mouse.drag (pad, p);
            zooms.push_back (pad.getScrollZoom());
        }
        mouse.up (pad, p);
        pump (10);
        return zooms;
    };
    const float startZoom = pad.getScrollZoom();
    auto smallMoves = zoomAfterMoves (2.f, 6);    // down: zooms out, 2 px a move
    const float afterSmall = pad.getScrollZoom();
    auto bigMoves = zoomAfterMoves (6.f, 6);      // 6 px a move
    const float afterBig = pad.getScrollZoom();
    const double smallStep = (afterSmall - startZoom) / 6.0, bigStep = (afterBig - afterSmall) / 6.0;
    check (smallStep > 0 && bigStep > 0, "dragging down on the scroll bar zooms out", fmt (startZoom, 3) + " -> " + fmt (afterSmall, 3) + " -> " + fmt (afterBig, 3));
    check (bigStep > 2.2 * smallStep, "the zoom follows how far the mouse moves (6 px moves zoom about 3x as much as 2 px moves)",
           "per move: " + fmt (smallStep, 4) + " vs " + fmt (bigStep, 4), "zoom moved by a fixed step per event: the old stepped zoom (R-07)");
    bool everyMove = true;
    for (size_t i = 1; i < smallMoves.size(); ++i) if (smallMoves[i] == smallMoves[i - 1]) everyMove = false;
    check (everyMove, "every small move changes the zoom (no stuck steps)");

    // the view eases to it: a frame after a big change, the scope is part of the way there, then arrives
    zoomAfterMoves (-6.f, 8);   // zoom back in
    const float target = pad.getScrollZoom();
    const float shownBefore = A::drawnZoom (scope);
    pump (10);
    A::tick (scope);
    const float oneFrame = A::drawnZoom (scope);
    for (int i = 0; i < 60; ++i) A::tick (scope);
    const float settled = A::drawnZoom (scope);
    check (std::abs (oneFrame - target) < std::abs (shownBefore - target) && std::abs (oneFrame - target) > 0.1f * std::abs (shownBefore - target),
           "the scope eases toward the new zoom (one frame: part of the way)", fmt (shownBefore, 3) + " -> " + fmt (oneFrame, 3) + " -> " + fmt (settled, 3) + ", target " + fmt (target, 3),
           "the view jumped (or didn't move)");
    check (closeTo (settled, target, 0.002), "and arrives");
    check (closeTo (A::drawnCentre (scope), pad.getScrollCenter(), 0.002), "and stays centred on the scroll bar", fmt (A::drawnCentre (scope), 4));

    // double-click resets it
    mouse.doubleClick (pad, mid);
    rig.settle();
    check (closeTo (pad.getPoint2() - pad.getPoint1(), pad.getWidth() * 0.4, 1.5), "double-click the scroll bar: back to the default zoom (the range 40 % of the bar)",
           fmt (pad.getPoint2() - pad.getPoint1(), 1) + " of " + juce::String (pad.getWidth()) + " px");
}

// U-12  R-05: the scope follows the host's tempo, smoothly
void scopeFollowsTempo()
{
    Rig rig;
    auto& ed = rig.open (1.f);
    auto& band = rig.band (0);
    mouse.click (band.mToggleSync.mToggleButton);   // a free band, whose drawn span depends on the tempo
    rig.settle();
    auto& scope = A::scope (ed, 0);
    rig.clock.bpm = 120;
    rig.listen (0, 0.2);
    for (int i = 0; i < 60; ++i) A::tick (scope);
    check (closeTo (A::drawnBpm (scope), 120.0, 0.05), "the scope starts at the host's 120 bpm", fmt (A::drawnBpm (scope)));
    rig.clock.bpm = 90;
    rig.listen (0, 0.1);
    std::vector<double> shown;
    for (int i = 0; i < 40; ++i) { A::tick (scope); shown.push_back (A::drawnBpm (scope)); }
    bool monotonic = true;
    for (size_t i = 1; i < shown.size(); ++i) if (shown[i] > shown[i - 1] + 1.0e-9) monotonic = false;
    check (closeTo (shown.back(), 90.0, 0.05), "after the host changes to 90 bpm, the scope draws 90 bpm", fmt (shown.back()), "the scope ignored the tempo change (R-05)");
    check (shown[0] > 91.0 && monotonic, "and gets there smoothly over several frames, never back", "first frame " + fmt (shown[0]) + " bpm");
}

// U-13  R-06: a session saved with Sync off: the rate slider works the moment the window opens
void freshLoadSyncOff()
{
    juce::MemoryBlock state;
    {
        Processor p;
        for (int b = 0; b < 3; ++b)
        {
            setValue (p, bandParam ("Sync to Host", b), 0.f);
            setValue (p, bandParam ("Rate", b), 3.f);
            setValue (p, bandParam ("Waveshape", b), 4.f);   // Sine
            setValue (p, bandParam ("Depth", b), 100.f);
        }
        setValue (p, "Mid-High Crossover Freq", 5000.f);
        setValue (p, "Low-Mid Crossover Freq", 120.f);
        p.getStateInformation (state);
    }
    Rig rig (false, &state);
    auto& ed = rig.open (1.f);
    for (int b = 0; b < 3; ++b)
    {
        auto& band = rig.band (b);
        check (band.mSliderRate.isVisible() && band.mSliderRate.isEnabled() && ! band.mDropRhythm.isVisible() && ! A::synced (band),
               juce::String (bandNames[b]) + ": opened unsynced, the rate slider shows");
        const double heardBefore = rig.listen (b, 2.0).frequency();
        check (closeTo (heardBefore, 3.0, 0.2), juce::String (bandNames[b]) + ": heard at the saved 3 Hz", fmt (heardBefore) + " Hz");
        const double expected = dragSliderTo (rig, band.mSliderRate.slider, 0.75);
        const double rate = value (*rig.proc, bandParam ("Rate", b));
        check (closeTo (rate, expected, 0.05 * expected + 0.05) && rate > 4.0, juce::String (bandNames[b]) + ": dragging the rate slider moves the rate", fmt (rate) + " Hz");
        auto& scope = A::scope (ed, b);
        check (closeTo (A::targetRate (scope), rate, 0.01), juce::String (bandNames[b]) + ": the scope hears the slider at once",
               "scope's rate " + fmt (A::targetRate (scope)), "the scope never heard the rate slider: the dead free-rate slider (R-06)");
        const double heard = rig.listen (b, 2.0).frequency();
        check (closeTo (heard, rate, 0.06 * rate + 0.05), juce::String (bandNames[b]) + ": and the audio follows it", fmt (heard) + " Hz");
    }
}

// U-14  R-04: free rate changes don't scramble the scope: it eases, and the wave stays anchored on the centre line
void freeRateScope()
{
    Rig rig;
    auto& ed = rig.open (1.f);
    auto& band = rig.band (1);
    chooseFromComboBox (rig, band.mDropWaveshape, "Sine");
    mouse.click (band.mToggleSync.mToggleButton);
    rig.settle (40);
    auto& scope = A::scope (ed, 1);
    std::vector<std::array<float, 3>> anchors;   // the wave's height at the centre line and a pixel either side
    for (double p : { 0.15, 0.5, 0.85 })
    {
        dragSliderTo (rig, band.mSliderRate.slider, p);
        const float target = A::targetRate (scope);
        const float before = A::drawnRate (scope);
        A::tick (scope);
        const float oneFrame = A::drawnRate (scope);
        for (int i = 0; i < 80; ++i) A::tick (scope);
        rig.settle();
        check (std::abs (target - before) < 0.01f || (std::abs (oneFrame - target) < std::abs (before - target) && std::abs (oneFrame - target) > 0.2f * std::abs (before - target)),
               "free rate " + fmt (target) + " Hz: the drawn wave eases to it (no jump)", fmt (before) + " -> " + fmt (oneFrame) + " -> " + fmt (A::drawnRate (scope)));
        anchors.push_back ({ waveYAt (scope, -1.f), waveYAt (scope, 0.f), waveYAt (scope, 1.f) });
    }
    // at every rate the wave crosses the centre line at the height it had at the first rate, give or take one pixel
    // sideways (a steep wave changes height fast, so it is compared within a pixel either side of the line)
    bool anchored = true;
    juce::String heights;
    for (auto& a : anchors)
    {
        const float lo = juce::jmin (a[0], a[1], a[2]) - 1.f, hi = juce::jmax (a[0], a[1], a[2]) + 1.f;
        if (std::isnan (a[1]) || anchors[0][1] < lo || anchors[0][1] > hi) anchored = false;
        heights << fmt (a[1], 1) << " ";
    }
    check (anchored, "the wave crosses the centre line at the same height at every rate (anchored, not scrambled)",
           "heights " + heights, "the wave slid when the rate changed (R-04)");
    snapshot (ed, "free_rate");
}

// U-15  R-08: synced LFOs land back on the beat after a loop or a jump
void relockAfterJump()
{
    Rig rig;
    rig.open (1.f);
    auto& band = rig.band (0);
    chooseFromComboBox (rig, band.mDropWaveshape, "Sine");
    dragSliderTo (rig, band.mSliderDepth.slider, 0.999);
    chooseFromComboBox (rig, band.mDropRhythm, "1/4 Note");
    rig.clock.ppq = 0;
    // where in the beat the low point falls, before and after a jump to an odd position
    auto lowPointInBeat = [&] (const Envelope& e, double ppqAtStart)
    {
        size_t best = 0;
        for (size_t i = 0; i < std::min<size_t> (e.level.size(), (size_t) (e.rate * 0.5)); ++i)
            if (e.level[i] < e.level[best]) best = i;
        const double beat = ppqAtStart + best / e.rate * rig.clock.bpm / 60.0;
        return beat - std::floor (beat);
    };
    rig.listen (0, 1.0);
    const double ppqBefore = rig.clock.ppq + 0.3 * 2.0;   // listen() settles 0.3 s first
    const auto before = rig.listen (0, 1.0);
    const double phaseBefore = lowPointInBeat (before, ppqBefore);
    for (double jumpTo : { 13.37, 2.5, 7.81 })
    {
        rig.clock.ppq = jumpTo;
        const double ppqAfter = jumpTo + 0.3 * 2.0;
        const auto after = rig.listen (0, 1.0);
        double d = std::abs (lowPointInBeat (after, ppqAfter) - phaseBefore);
        d = juce::jmin (d, 1.0 - d);
        check (d < 0.03, "after a jump to beat " + fmt (jumpTo) + " the synced wave is on the grid again",
               "low point " + fmt (lowPointInBeat (after, ppqAfter), 3) + " of a beat, was " + fmt (phaseBefore, 3), "it kept its own phase after the jump (R-08)");
    }
}

// U-16  R-03: Ctrl+Shift+click the About window's version shows the build; a plain click doesn't
void aboutBuildId()
{
    for (float scale : { 1.f, 1.25f })
    {
        Rig rig;
        auto& ed = rig.open (scale);
        const auto at = " at " + juce::String (juce::roundToInt (scale * 100)) + " %";
        check (openMainMenu (rig) && chooseMenuItem ("About Tertiary..."), "Menu > About Tertiary... is there (activated copy)" + at, {}, "items: " + menuTitles().joinIntoString (", "));
        pump (30);
        rig.settle();
        auto& about = A::about (ed);
        check (about.isVisible(), "the About window opens" + at);
        const juce::Point<float> version (about.getWidth() * 0.75f, 52.f);
        const auto plain = picture (ed);
        mouse.click (about, version);
        rig.settle();
        check (! A::buildIdShown (about), "a plain click on the version doesn't show the build" + at);
        mouse.keys = juce::ModifierKeys (juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier);
        mouse.click (about, version);
        mouse.keys = {};
        rig.settle();
        check (A::buildIdShown (about), "Ctrl+Shift+click on the version shows the build id" + at, {}, "the click toggled twice (it reached mouseUp twice) or not at all (R-03)");
        const auto shown = picture (ed);
        const auto line = areaInEditor (ed, about).withHeight (juce::roundToInt (70 * scale));
        check (differingPixels (plain, shown, line) > 20, "and the version line is drawn differently" + at, juce::String (differingPixels (plain, shown, line)) + " pixels changed");
        snapshot (ed, "about_build_id_" + juce::String (juce::roundToInt (scale * 100)));
        // [Claude 2026-10-30] a plain click on the build line copies the build id (plain text) and says so briefly
        TestHooks::copied() = {};
        const auto beforeCopy = picture (ed);
        mouse.click (about, version);
        rig.settle();
        check (TestHooks::copied() == juce::String (juce::CharPointer_UTF8 (WLA_BUILD_ID)) && TestHooks::copied().isNotEmpty(),
               "a click on the build line copies the build id" + at, TestHooks::copied());
        check (A::buildIdShown (about), "and the build line stays shown" + at);
        check (differingPixels (beforeCopy, picture (ed), line) > 10, "\"Copied\" shows in its place" + at);
        pump (1400);
        rig.settle();
        check (differingPixels (shown, picture (ed), line) < 10, "then the build line comes back" + at,
               juce::String (differingPixels (shown, picture (ed), line)) + " pixels differ from before");
        mouse.keys = juce::ModifierKeys (juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier);
        mouse.click (about, version);
        mouse.keys = {};
        rig.settle();
        check (! A::buildIdShown (about), "Ctrl+Shift+click again hides it" + at);
    }
}

// U-17  The About window opens and closes every way, its link and the menu's links don't crash or open anything
void aboutWindowAndLinks()
{
    resetTestFolder();
    Rig rig (true);
    auto& ed = rig.open (1.f);
    auto& about = A::about (ed);
    auto& banner = A::banner (ed);
    check (! A::activateArea (banner).isEmpty(), "demo: the header has the Activate key");
    mouse.click (banner, A::activateArea (banner).getCentre());
    rig.settle();
    check (about.isVisible(), "clicking Activate in the header opens the About window");
    mouse.click (A::closeButton (about));
    rig.settle();
    check (! about.isVisible(), "its X closes it");
    check (openMainMenu (rig) && chooseMenuItem ("Activate Tertiary..."), "Menu > Activate Tertiary... (demo copy)", {}, "items: " + menuTitles().joinIntoString (", "));
    pump (30);
    rig.settle();
    check (about.isVisible(), "opens it too");
    snapshot (ed, "about_demo");
    TestHooks::opened().clear();
    mouse.click (A::websiteLink (about));
    rig.settle();
    check (TestHooks::opened().size() == 1 && TestHooks::opened()[0].containsIgnoreCase ("wonderlandaudio.com"),
           "the website link would open WonderlandAudio.com (recorded, not opened), no crash", TestHooks::opened().joinIntoString (" "));
    mouse.click (A::closeButton (about));
    rig.settle();
    mouse.doubleClick (banner, { 20.f, banner.getHeight() / 2.f });
    rig.settle();
    check (about.isVisible(), "double-click on the header opens it");
    mouse.doubleClick (banner, { 20.f, banner.getHeight() / 2.f });
    rig.settle();
    check (! about.isVisible(), "and again closes it");

    TestHooks::opened().clear();
    check (openMainMenu (rig), "Menu opens");
    const auto upToDateItem = "Tertiary " + juce::String (JucePlugin_VersionString) + " is up to date";
    check (! menuTitles().joinIntoString ("|").contains ("Get version") && (findMenuItem (upToDateItem) == nullptr || ! menuItemEnabled (upToDateItem)),
           "Menu offers no update (none known), and the version line can't be chosen", menuTitles().joinIntoString (", "));
    check (chooseMenuItem ("WonderlandAudio.com"), "Menu > WonderlandAudio.com");
    pump (30);
    check (TestHooks::opened().size() == 1 && TestHooks::opened()[0].containsIgnoreCase ("wonderlandaudio.com"), "would open the site (recorded), no crash", TestHooks::opened().joinIntoString (" "));
    TestHooks::opened().clear();
    check (openMainMenu (rig) && chooseMenuItem ("Open presets folder"), "Menu > Open presets folder");
    pump (30);
    check (TestHooks::opened().size() == 1 && TestHooks::opened()[0].startsWith (TestHooks::testFolder().getFullPathName()),
           "would open the test's own presets folder, never the real one", TestHooks::opened().joinIntoString (" "));
}

/* A pretend licence server that accepts one key */
struct AcceptingServer : PretendServer
{
    juce::String goodKey;
    int activateCalls = 0, deactivateCalls = 0;
    LicenseReply validate (const juce::String& key, const juce::String& email) override
    {
        ++validateCalls;
        lastEmail = email;
        return key == goodKey ? reply ("key_valid") : reply ("invalid_key", "That serial key isn't valid.");
    }
    LicenseReply activate (const juce::String&, const juce::String&, const juce::String&, const juce::String&) override   { ++activateCalls; return reply ("key_activated"); }
    LicenseReply deactivate (const juce::String&, const juce::String&, const juce::String&, const juce::String&) override { ++deactivateCalls; return reply ("key_deactivated"); }
};

// U-18  The licence box: a wrong key, then a right one (pretend server), then deactivate; the DEMO tag
void licenceBox()
{
    resetTestFolder();
    Rig rig (true);
    auto& ed = rig.open (1.f);
    auto& about = A::about (ed);
    auto& banner = A::banner (ed);
    auto server = std::make_shared<AcceptingServer>();
    server->goodKey = "TERT-TEST-1234-ABCD";
    A::setServer (about, server);
    check (A::bannerDemo (banner) && ! A::activateArea (banner).isEmpty(), "demo: the header shows DEMO and Activate");
    mouse.click (banner, A::activateArea (banner).getCentre());
    rig.settle();

    auto typeInto = [&] (juce::TextEditor& field, const juce::String& text)
    {
        mouse.click (field);
        pressKey (field, juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 'a'));
        pressKey (field, juce::KeyPress (juce::KeyPress::backspaceKey));
        typeText (field, text);
    };
    typeInto (A::emailField (about), "Joe@Example.com");
    typeInto (A::keyField (about), "WRONG-KEY");
    check (A::keyField (about).getText() == "WRONG-KEY" && A::emailField (about).getText() == "Joe@Example.com", "typing into the email and key fields",
           A::emailField (about).getText() + " / " + A::keyField (about).getText());
    mouse.click (A::actionButton (about));
    waitFor ([&] { return A::status (about).getText().startsWith ("Error"); });
    check (A::status (about).getText().contains ("isn't valid") && rig.proc->getDemoState() && A::bannerDemo (banner),
           "a wrong key: the server's message, still the demo", A::status (about).getText());

    typeInto (A::keyField (about), server->goodKey);
    mouse.click (A::actionButton (about));
    waitFor ([&] { return ! rig.proc->getDemoState(); });
    rig.settle();
    check (! rig.proc->getDemoState() && A::status (about).getText().contains ("successful"), "the right key: activated", A::status (about).getText());
    // [Claude 2026-10-30] M-01: the server compares the email exactly, capitals included, so it goes as typed (this
    // check expected lower case, the 1.1 behaviour M-01 reversed; found when ui-tests was merged into the release branch)
    check (server->lastEmail == "Joe@Example.com", "the email is sent exactly as typed (M-01)", server->lastEmail);
    check (! A::bannerDemo (banner) && A::activateArea (banner).isEmpty(), "the DEMO tag and the Activate key are gone");
    check (A::keyField (about).isReadOnly() && A::keyField (about).getText().endsWith ("ABCD") && ! A::keyField (about).getText().contains ("TERT-TEST"),
           "the key shows masked, read-only, its last four kept", A::keyField (about).getText());
    {
        juce::PropertiesFile props (LicenseCheck::getSettingsOptions());
        check (props.getValue ("licenseKey") == server->goodKey && props.getFile().isAChildOf (TestHooks::testFolder()),
               "saved in the test's own settings file, never the real one", props.getFile().getFullPathName());
        check (props.getValue ("licenseEmail") == "Joe@Example.com", "the email is saved as sent, so a reload stays activated (M-01, R-09)",
               props.getValue ("licenseEmail"));
    }
    snapshot (ed, "about_activated");
    check (openMainMenu (rig) && findMenuItem ("About Tertiary...") != nullptr, "Menu now says About Tertiary...");
    closeMenus();

    mouse.click (A::actionButton (about));
    waitFor ([&] { return rig.proc->getDemoState(); });
    rig.settle();
    check (rig.proc->getDemoState() && server->deactivateCalls == 1 && A::bannerDemo (banner) && ! A::activateArea (banner).isEmpty(),
           "Deactivate: back to the demo, DEMO and Activate back", A::status (about).getText());
    resetTestFolder();
}

// U-19  Demo mode: the bypass gap greys the window and the tag fills; controls don't take clicks; then back
void demoBypass()
{
    Rig rig (true);
    auto& ed = rig.open (1.f);
    auto& g = A::global (ed);
    rig.listen (0, 49.0, 0.0);
    rig.settle();
    check (! A::showingBypass (g) && ! A::bannerBypassed (A::banner (ed)), "demo, first 49 s: not bypassed");
    rig.listen (0, 2.0, 0.0);
    g.timerCallback();
    rig.settle();
    check (A::showingBypass (g) && A::bannerBypassed (A::banner (ed)) && A::scopeWrapper (ed).getAlpha() < 0.5f,
           "demo, after 50 s: the bypass gap: the window fades and the DEMO tag fills");
    snapshot (ed, "demo_bypass");
    const bool syncBefore = value (*rig.proc, "Sync to Host Low Band") > 0.5f;
    mouse.click (rig.band (0).mToggleSync.mToggleButton);
    rig.settle();
    check ((value (*rig.proc, "Sync to Host Low Band") > 0.5f) == syncBefore, "during the gap the controls don't take clicks");
    rig.listen (0, 9.0, 0.0);
    g.timerCallback();
    rig.settle();
    check (! A::showingBypass (g) && A::scopeWrapper (ed).getAlpha() > 0.99f && ! A::bannerBypassed (A::banner (ed)), "after the 10 s gap: back to normal");
}

// U-20  Presets: every factory preset from the preset menu; the arrows
void factoryPresets()
{
    resetTestFolder();
    Rig rig;
    auto& ed = rig.open (1.f);
    auto& bar = A::presetBar (ed);
    const auto& factory = PresetManager::getFactoryPresets();
    for (size_t i = 0; i < factory.size(); ++i)
    {
        const auto& preset = factory[i];
        mouse.click (bar, A::nameArea (bar).getCentre().toFloat());
        const bool open = waitForMenu();
        const bool chosen = open && chooseMenuItem (preset.name);
        pump (30);
        rig.settle();
        bar.timerCallback();
        juce::String wrong;
        for (const auto& [id, v] : preset.values)
            if (! closeTo (value (*rig.proc, id), v, 0.002 * (param (*rig.proc, id)->getNormalisableRange().end - param (*rig.proc, id)->getNormalisableRange().start) + 0.01))
                wrong << id << " = " << fmt (value (*rig.proc, id)) << " (preset " << fmt (v) << "); ";
        check (chosen && wrong.isEmpty() && A::shownName (bar) == preset.name && ! A::shownModified (bar),
               "preset menu > " + preset.name + ": loads, its name shows, not modified",
               A::shownName (bar), chosen ? wrong : "not in the menu: " + menuTitles().joinIntoString (", "));
        // and the band controls show it
        auto& low = rig.band (0);
        check (closeTo (low.mSliderDepth.slider.getValue(), value (*rig.proc, "Depth Low Band"), 0.01)
                   && low.mToggleSync.mToggleButton.getToggleState() == (value (*rig.proc, "Sync to Host Low Band") > 0.5f)
                   && low.mSliderRate.isVisible() != low.mToggleSync.mToggleButton.getToggleState(),
               preset.name + ": the controls show its values");
    }
    // arrows step through them
    mouse.click (bar, A::nameArea (bar).getCentre().toFloat());
    waitForMenu();
    chooseMenuItem (factory[0].name);
    pump (30);
    mouse.click (A::next (bar));
    rig.settle();
    bar.timerCallback();
    check (A::shownName (bar) == factory[1].name, "> steps to the next preset", A::shownName (bar));
    mouse.click (A::previous (bar));
    rig.settle();
    bar.timerCallback();
    check (A::shownName (bar) == factory[0].name, "< steps back", A::shownName (bar));
}

bool presetMenu (Rig& rig, const juce::String& item)
{
    auto& bar = A::presetBar (rig.editor());
    mouse.click (bar, A::nameArea (bar).getCentre().toFloat());
    if (! waitForMenu()) return false;
    const bool ok = chooseMenuItem (item);
    pump (30);
    return ok;
}

bool answerNameWindow (const juce::String& name, bool pressEnter)
{
    auto* w = modalAlertWindow();
    if (w == nullptr) return false;
    lastAnswered = w;
    pump (150);   // a person looks at it before typing (and the window finishes setting itself up)
    auto* field = w->getTextEditor ("name");
    if (field == nullptr) return false;
    mouse.click (*field);
    pressKey (*field, juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 'a'));
    pressKey (*field, juce::KeyPress (juce::KeyPress::backspaceKey));
    typeText (*field, name);
    if (pressEnter)
        pressKey (*field, juce::KeyPress (juce::KeyPress::returnKey));
    else if (auto* ok = buttonCalled (*w, "OK"))
        mouse.click (*ok);
    pump (40);
    return true;
}

// U-21  Presets: Save As, modified marker, Save, Rename, reload, Delete; all in the test's temp folder
void userPresets()
{
    resetTestFolder();
    Rig rig;
    auto& ed = rig.open (1.f);
    auto& bar = A::presetBar (ed);
    const auto folder = rig.proc->presets.getUserFolder();
    check (folder.isAChildOf (TestHooks::testFolder()), "the presets folder is the test's own", folder.getFullPathName());

    dragSliderTo (rig, rig.band (0).mSliderDepth.slider, 0.6);
    const float depth = value (*rig.proc, "Depth Low Band");
    check (presetMenu (rig, "Save As..."), "preset menu > Save As...");
    check (answerNameWindow ("UI Test Preset", true), "a name window opens; typed a name and Enter");
    rig.settle();
    bar.timerCallback();
    check (folder.getChildFile ("UI Test Preset.tertiarypreset").existsAsFile() && A::shownName (bar) == "UI Test Preset" && ! A::shownModified (bar),
           "saved: the file is there and its name shows", A::shownName (bar));

    dragSliderTo (rig, rig.band (0).mSliderDepth.slider, 0.2);
    bar.timerCallback();
    check (A::shownModified (bar), "changing a setting shows the * (modified)");
    check (presetMenu (rig, "Save"), "preset menu > Save");
    rig.settle();
    bar.timerCallback();
    const float savedDepth = value (*rig.proc, "Depth Low Band");
    check (! A::shownModified (bar), "saved again: no *");

    check (presetMenu (rig, "Rename..."), "preset menu > Rename...");
    check (answerNameWindow ("UI Test Renamed", false), "typed a new name and clicked OK");
    rig.settle();
    bar.timerCallback();
    check (folder.getChildFile ("UI Test Renamed.tertiarypreset").existsAsFile() && ! folder.getChildFile ("UI Test Preset.tertiarypreset").exists()
               && A::shownName (bar) == "UI Test Renamed",
           "renamed: the file and the name", A::shownName (bar),
           "folder has: " + [&] { juce::StringArray n; for (auto& f : folder.findChildFiles (juce::File::findFiles, false)) n.add (f.getFileName()); return n.joinIntoString (", "); }()
           );

    presetMenu (rig, PresetManager::getFactoryPresets()[0].name);
    rig.settle();
    check (! closeTo (value (*rig.proc, "Depth Low Band"), savedDepth, 0.5), "loaded a factory preset (different depth)");
    check (presetMenu (rig, "UI Test Renamed"), "the preset menu lists it under Your presets");
    rig.settle();
    bar.timerCallback();
    check (closeTo (value (*rig.proc, "Depth Low Band"), savedDepth, 0.05) && A::shownName (bar) == "UI Test Renamed",
           "and reloading it brings its settings back", fmt (value (*rig.proc, "Depth Low Band")) + " (saved " + fmt (savedDepth) + ", first save " + fmt (depth) + ")");

    check (presetMenu (rig, "Delete"), "preset menu > Delete");
    if (auto* w = modalAlertWindow())
    {
        lastAnswered = w;
        if (auto* del = buttonCalled (*w, "Delete"))
            mouse.click (*del);
    }
    pump (40);
    rig.settle();
    check (! folder.getChildFile ("UI Test Renamed.tertiarypreset").exists(), "confirmed: the file is gone");
    resetTestFolder();
}

// U-22  Open and close the window 200 times: no leak, nothing left behind, nothing used after it's gone
void openCloseMany()
{
    Rig rig;
    auto& desktop = juce::Desktop::getInstance();
    const int baseComponents = desktop.getNumComponents(), basePeers = juce::ComponentPeer::getNumPeers();
    long long liveAt20 = 0;
    juce::String memAt20;
    int strays = 0;
    const float scales[] = { 1.f, 1.25f, 1.5f, 2.f };
    for (int i = 0; i < openCloseCycles; ++i)
    {
        AutoreleasePool pool;   // as a host's event loop drains one (macOS)
        auto& ed = rig.open (scales[i % 4]);
        if (i % 10 == 0)
        {
            mouse.click (rig.band (i % 3).mToggleBypass.mToggleButton);   // use it a little
            rig.settle();
        }
        juce::ignoreUnused (ed);
        rig.close();
        pump (5);
        if (desktop.getNumComponents() != baseComponents || juce::ComponentPeer::getNumPeers() != basePeers)
            ++strays;
        if (i == 19) { liveAt20 = liveAllocations.load(); memAt20 = processMemory(); }
    }
    pump (50);
    const long long growth = liveAllocations.load() - liveAt20;
    const int measured = juce::jmax (1, openCloseCycles - 20);
    check (strays == 0, "every close removes the window (no stray windows or peers)", juce::String (strays) + " strays");
    check (growth < measured, "no memory growth: open and closed " + juce::String (openCloseCycles) + " times",
           juce::String (growth) + " live allocations more than after 20 (" + fmt ((double) growth / measured, 2) + " a cycle); working set " + memAt20 + " -> " + processMemory(),
           "something is allocated every time the window opens and never freed");

    // after the window is gone: the host keeps changing parameters, the licence check reports, time passes, audio plays.
    // Freed memory is filled with 0xDD, so a listener or timer still pointing at the closed window crashes here.
    for (auto* prm : rig.proc->getParameters())
        prm->setValueNotifyingHost (1.f - prm->getValue());
    rig.proc->licenseCheck.sendChangeMessage();
    pump (300);
    rig.listen (0, 0.5);
    rig.listen (1, 0.5);
    for (auto* prm : rig.proc->getParameters())
        prm->setValueNotifyingHost (prm->getDefaultValue());
    pump (100);
    check (true, "after closing: parameter changes, a licence result, 0.4 s of timers and audio: nothing touched the closed window");
}

// U-23  R-01: no ghost strip on the crossover's floor when it's quiet
void crossoverFloor()
{
    Rig rig;
    auto& ed = rig.open (1.f);
    setValue (*rig.proc, "Show FFT", 0.f);
    rig.settle();
    for (int i = 0; i < 10; ++i) { juce::AudioBuffer<float> silence (2, blockSize); silence.clear(); juce::MidiBuffer m; rig.proc->processBlock (silence, m); }
    auto& wrapper = A::freqWrapper (ed);
    for (int i = 0; i < 10; ++i) wrapper.timerCallback();
    const auto off = picture (ed);
    setValue (*rig.proc, "Show FFT", 1.f);
    rig.settle();
    for (int i = 0; i < 20; ++i) { juce::AudioBuffer<float> silence (2, blockSize); silence.clear(); juce::MidiBuffer m; rig.proc->processBlock (silence, m); wrapper.timerCallback(); }
    rig.settle();
    const auto on = picture (ed);
    const auto area = areaInEditor (ed, wrapper);
    const auto floorRows = area.withTrimmedTop (area.getHeight() - juce::roundToInt (area.getHeight() * 0.12f)).reduced (6, 0);
    const int diff = differingPixels (off, on, floorRows, 4);
    check (diff < floorRows.getWidth() / 4, "silence with the spectrum on: the floor rows look the same as with it off (no ghost strip)",
           juce::String (diff) + " pixels differ in the bottom rows", "a strip is drawn along the floor (R-01)");
    snapshot (ed, "spectrum_silence");
}

//==============================================================================
std::vector<Scenario> scenarios()
{
    return {
        { "U-01", "T2-04 T1-09", "The window opens at 100, 125, 150 and 200 %", windowAtEachScale },
        { "U-02", "T2-04", "Window size from the Menu (the icon at the top right); the size is remembered", windowSizeMenu },
        { "U-03", "T2-05", "Depth, skew, phase and gain on each band, dragged; heard", bandSliders },
        { "U-04", "T2-05", "Shape menu and INV on each band", bandShapeAndInvert },
        { "U-05", "T2-05 T2-06", "Rate synced (Rhythm menu) and free (slider), SYNC on and off, each band; heard", bandRates },
        { "U-06", "T2-07 R-02", "Bypass, mute and solo on each band: the crossover and scope grey together; heard", bypassMuteSolo },
        { "U-07", "T2-05", "Crossover handles and gain handles dragged; a frequency typed", crossover },
        { "U-08", "T2-05", "A value typed into a slider", typedValue },
        { "U-09", "T2-05", "Input and output faders", inputOutputFaders },
        { "U-10", "T2-05", "The scope's and the crossover's option menus", displayOptions },
        { "U-11", "R-07 T2-10", "Smooth zoom", smoothZoom },
        { "U-12", "R-05 T2-08", "The scope follows a tempo change", scopeFollowsTempo },
        { "U-13", "R-06 T2-06", "Opened with Sync off: the rate slider works at once", freshLoadSyncOff },
        { "U-14", "R-04", "Free rate changes don't scramble the scope", freeRateScope },
        { "U-15", "R-08 T2-09", "Synced LFOs land on the beat after a jump", relockAfterJump },
        { "U-16", "R-03 T2-12 T1-02", "Ctrl+Shift+click the About version shows the build; a click copies it", aboutBuildId },
        { "U-17", "T2-12", "About window open and close; links don't crash or open anything", aboutWindowAndLinks },
        { "U-18", "T2-12 T2-13", "The licence box with a pretend server; DEMO tag", licenceBox },
        { "U-19", "T2-13", "Demo mode bypass gap", demoBypass },
        { "U-20", "T2-11", "Every factory preset from the preset menu; the arrows", factoryPresets },
        { "U-21", "T2-11", "User presets: save, rename, reload, delete (temp folder)", userPresets },
        { "U-22", "T2-02", "Open and close the window many times: no leak, no use after close", openCloseMany },
        { "U-23", "R-01 T1-09", "No ghost strip on the crossover floor", crossoverFloor },
        { "U-24", "R-18 T2-04", "Opened at the host's scale (1.25, 1.5): the layout times the chosen size; a host's size is not remembered", hostScaleFirstOpen },   // [Claude 2026-10-30]
    };
}

} // namespace

//==============================================================================
int main (int argc, char* argv[])
{
    uirobot::Suite suite;
    suite.title = "Tertiary UI tests: the real editor, off screen, driven by mouse and keyboard events";
    suite.scenarios = scenarios();
    suite.beforeAll = [] { resetTestFolder(); std::cout << "settings and presets in " << TestHooks::testFolder().getFullPathName() << std::endl; };
    suite.afterAll = [] { TestHooks::testFolder().deleteRecursively(); };
    suite.option = [] (const juce::String& name, const juce::String& v)
    {
        if (name == "--cycles") openCloseCycles = juce::jmax (21, v.getIntValue());   // U-22 (default 200)
        return true;
    };
    return uirobot::run (argc, argv, suite);
}
