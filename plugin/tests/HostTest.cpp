/*
  ==============================================================================

    HostTest.cpp
    Created: 27 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-27] New file. Joe: "Can we build some kind of automated test harness?
    So I don't have to do manual testing?"

    A small host: it loads the BUILT plugin (the .vst3, or the installed AU on a Mac) the way
    a DAW does, through juce::AudioPluginFormatManager, and runs the manual checklist as
    automated checks. Every check prints one line: PASS, FAIL, SKIP or INFO.

        HostTest --vst3 <path to Tertiary.vst3>        (Windows and Mac)
        HostTest --au                                   (Mac: the AU installed in Components)
        options:  --golden <folder>   reference renders (default plugin/tests/golden)
                  --make-golden       write the reference renders instead of comparing
                  --expect-demo       this machine has no key, so the demo gaps must be there
                  --only <n>          run one section (1 to 10)
                  --write-session <file>   save a session (and its values, .txt) from the loaded plugin
                  --describe          print the plug-in's description as XML  [Claude 2026-10-30]
        HostTest --write-input <file.wav>               the golden renders' input, for the REAPER test
        HostTest --live-license <key> <email>           [Claude 2026-10-30] the licence routes against the REAL server,
                                                        with a test key; leaves the key with nothing activated

    Exit code 0 when nothing FAILed.

    The sections
      1  Load, prepare, process at 44.1/48/96 kHz, blocks 32 to 2048, changing mid-run
      2  Automation of every automatable parameter through the host's parameter API
      3  Session save and restore (exact values, bit-identical render)
      4  Tempo: the synced LFO follows the host's tempo
      5  Host bypass, band bypass, mute and solo
      6  Demo mode: 50 s on, 10 s off
      7  License states with the pretend server (in this program, not the plugin binary:
         the shipped plugin only ever talks to the real server, so it can't be pointed at a
         pretend one. Never calls the live server.)
      8  Golden renders: fixed input and settings, compared with files in the repo
      9  Presets through the host: the list, each one loads and is heard, Default = golden,
         remembered in the session, a 1.0.2 session loads unchanged  [Claude 2026-10-27]
     10  The window through the host: at every window size and host scale, the plug-in's window is
         exactly the UI's size, and is put back when the host sizes it otherwise  [Claude 2026-10-30]
     11  Two instances are independent: automating, presets, a session and 70 s of audio on one
         leave the other alone (Joe, RC1b in Cubase)  [Claude 2026-10-30]

  ==============================================================================
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PretendServer.h"
#include "PresetManager.h"   // [Claude 2026-10-27] the factory preset list, to check what the host sees
#if JUCE_WINDOWS
 // [Claude 2026-10-30] section 10 opens the plug-in's view itself, as a DAW does, and measures its window in
 // screen pixels. The VST3 interface headers only: the interface IDs are already in JUCE's VST3 hosting code.
 #define JUCE_VST3HEADERS_INCLUDE_HEADERS_ONLY 1
 #include <juce_audio_processors/format_types/juce_VST3Headers.h>
 #undef JUCE_VST3HEADERS_INCLUDE_HEADERS_ONLY
 #include <windows.h>
#endif

namespace
{
//==============================================================================
// Results
//==============================================================================
int passes = 0, failures = 0, skips = 0;

void report(const juce::String& status, const juce::String& name, const juce::String& detail = {})
{
    if (status == "PASS") ++passes;
    if (status == "FAIL") ++failures;
    if (status == "SKIP") ++skips;
    std::cout << status.paddedRight(' ', 5) << " " << name.paddedRight(' ', 58) << " " << detail << std::endl;
}

void check(bool ok, const juce::String& name, const juce::String& detail = {})
{
    report(ok ? "PASS" : "FAIL", name, detail);
}

//==============================================================================
// Loading the plugin
//==============================================================================
juce::AudioPluginFormatManager formatManager;
juce::PluginDescription description;

bool findPlugin(const juce::StringArray& args)
{
    formatManager.addDefaultFormats();

    juce::String formatName, fileOrId;

    if (args.contains("--vst3"))
    {
        formatName = "VST3";
        fileOrId = juce::File::getCurrentWorkingDirectory().getChildFile(args[args.indexOf("--vst3") + 1]).getFullPathName();
    }
    else if (args.contains("--au"))
    {
        formatName = "AudioUnit";
    }

    for (auto* format : formatManager.getFormats())
    {
        if (format->getName() != formatName)
            continue;

        juce::StringArray candidates;
        if (fileOrId.isNotEmpty())
            candidates.add(fileOrId);
        else
            candidates = format->searchPathsForPlugins({}, true, false);   // AU: every installed component

        for (auto& candidate : candidates)
        {
            if (formatName == "AudioUnit" && ! candidate.contains(",EXPL,WLAD"))
                continue;

            juce::OwnedArray<juce::PluginDescription> found;
            format->findAllTypesForFile(found, candidate);

            if (! found.isEmpty())
            {
                description = *found[0];
                return true;
            }
        }
    }
    return false;
}

std::unique_ptr<juce::AudioPluginInstance> create(double rate, int maxBlock)
{
    juce::String error;
    auto instance = formatManager.createPluginInstance(description, rate, maxBlock, error);

    if (instance == nullptr)
    {
        report("FAIL", "create instance", error);
        return nullptr;
    }

    juce::AudioProcessor::BusesLayout stereo;
    stereo.inputBuses.add(juce::AudioChannelSet::stereo());
    stereo.outputBuses.add(juce::AudioChannelSet::stereo());
    instance->setBusesLayout(stereo);
    instance->setRateAndBufferSizeDetails(rate, maxBlock);
    instance->prepareToPlay(rate, maxBlock);
    return instance;
}

//==============================================================================
// Parameters, by the names Tertiary gives them (Params.h)
//==============================================================================
juce::AudioProcessorParameter* findParam(juce::AudioPluginInstance& p, const juce::String& name)
{
    for (auto* prm : p.getParameters())
        if (prm->getName(256) == name)
            return prm;
    return nullptr;
}

/* Sets a parameter from its normalised (0 to 1) value, as a host's automation does */
void setNorm(juce::AudioPluginInstance& p, const juce::String& name, float value)
{
    if (auto* prm = findParam(p, name))
        prm->setValueNotifyingHost(value);
    else
        report("FAIL", "parameter exists: " + name);
}

// Normalised values from the ranges in PluginProcessor::createParameterLayout
float choiceNorm(int index, int numChoices)     { return (float) index / (float) (numChoices - 1); }
float rateNorm(float hz)                        { return (hz - 0.5f) / 12.0f; }
float phaseNorm(float degrees)                  { return (degrees + 180.0f) / 360.0f; }
float percentNorm(float percent)                { return percent / 100.0f; }
float skewNorm(float percent)                   { return (percent - 12.5f) / 75.0f; }
float gainNorm(float dB)                        { return (dB + 24.0f) / 48.0f; }

/* Low band only, free-running sine at full depth: with a DC input the output follows the low LFO */
void lowBandLfoOnly(juce::AudioPluginInstance& p, bool synced, float rateHz)
{
    setNorm(p, "Waveshape Low Band", choiceNorm(4, 7));     // Sine
    setNorm(p, "Depth Low Band", 1.0f);
    setNorm(p, "Sync to Host Low Band", synced ? 1.0f : 0.0f);
    setNorm(p, "Rate Low Band", rateNorm(rateHz));
    setNorm(p, "Multiplier Low Band", choiceNorm(1, 6));    // 1/4 note
}

//==============================================================================
// Signals and rendering
//==============================================================================
juce::AudioBuffer<float> sine(double rate, double seconds, float freq = 220.0f, float amplitude = 0.25f)
{
    juce::AudioBuffer<float> b(2, (int) (rate * seconds));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        auto v = amplitude * (float) std::sin(juce::MathConstants<double>::twoPi * freq * i / rate);
        b.setSample(0, i, v);
        b.setSample(1, i, v);
    }
    return b;
}

/* One tone in each band (crossovers at 500 Hz and 1 kHz by default), so every band's controls are heard */
juce::AudioBuffer<float> threeTones(double rate, double seconds)
{
    juce::AudioBuffer<float> b(2, (int) (rate * seconds));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / rate, w = juce::MathConstants<double>::twoPi;
        auto v = (float) (0.2 * std::sin(w * 110 * t) + 0.1 * std::sin(w * 700 * t) + 0.05 * std::sin(w * 4000 * t));
        b.setSample(0, i, v);
        b.setSample(1, i, v);
    }
    return b;
}

juce::AudioBuffer<float> dc(double rate, double seconds, float value = 0.5f)
{
    juce::AudioBuffer<float> b(2, (int) (rate * seconds));
    for (int c = 0; c < 2; ++c)
        juce::FloatVectorOperations::fill(b.getWritePointer(c), value, b.getNumSamples());
    return b;
}

/* The golden-render input: soft noise, a sweep and a click every 0.25 s. The same on every machine. */
juce::AudioBuffer<float> testSignal(double rate, double seconds)
{
    juce::AudioBuffer<float> b(2, (int) (rate * seconds));
    juce::Random rnd(1234);
    double phase = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double f = 40.0 * std::pow(250.0, i / (double) b.getNumSamples());   // 40 Hz to 10 kHz
        phase += juce::MathConstants<double>::twoPi * f / rate;
        const float click = (i % (int) (rate / 4) == 0) ? 0.5f : 0.0f;
        b.setSample(0, i, 0.25f * (float) std::sin(phase) + 0.05f * (rnd.nextFloat() * 2 - 1) + click);
        b.setSample(1, i, 0.20f * (float) std::sin(phase) + 0.05f * (rnd.nextFloat() * 2 - 1) - click);
    }
    return b;
}

using BlockCallback = std::function<void(juce::AudioPluginInstance&, int position)>;

/* Runs input through the plugin in blocks (sizes cycle through the list); returns the output */
juce::AudioBuffer<float> render(juce::AudioPluginInstance& p, const juce::AudioBuffer<float>& input,
                                std::vector<int> blocks = { 512 }, BlockCallback beforeBlock = {})
{
    const int maxBlock = *std::max_element(blocks.begin(), blocks.end());
    juce::AudioBuffer<float> out(2, input.getNumSamples()), io(2, maxBlock);
    juce::MidiBuffer midi;

    for (int pos = 0, k = 0; pos < input.getNumSamples(); ++k)
    {
        const int n = juce::jmin(blocks[(size_t) k % blocks.size()], input.getNumSamples() - pos);
        if (beforeBlock)
            beforeBlock(p, pos);
        juce::AudioBuffer<float> view(io.getArrayOfWritePointers(), 2, n);
        for (int c = 0; c < 2; ++c)
            view.copyFrom(c, 0, input, c, pos, n);
        p.processBlock(view, midi);
        for (int c = 0; c < 2; ++c)
            out.copyFrom(c, pos, view, c, 0, n);
        pos += n;
    }
    return out;
}

struct Stats { float peak = 0, rms = 0, maxJump = 0; int nonFinite = 0, subnormal = 0; };

Stats stats(const juce::AudioBuffer<float>& b, int from = 0)
{
    Stats s;
    double sum = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        auto* x = b.getReadPointer(c);
        for (int i = from; i < b.getNumSamples(); ++i)
        {
            if (! std::isfinite(x[i])) { ++s.nonFinite; continue; }
            if (x[i] != 0 && std::abs(x[i]) < std::numeric_limits<float>::min()) ++s.subnormal;
            s.peak = juce::jmax(s.peak, std::abs(x[i]));
            sum += x[i] * x[i];
            if (i > from) s.maxJump = juce::jmax(s.maxJump, std::abs(x[i] - x[i - 1]));
        }
    }
    s.rms = (float) std::sqrt(sum / juce::jmax(1, b.getNumChannels() * (b.getNumSamples() - from)));
    return s;
}

float maxDiff(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float d = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < juce::jmin(a.getNumSamples(), b.getNumSamples()); ++i)
            d = juce::jmax(d, std::abs(a.getSample(c, i) - b.getSample(c, i)));
    return d;
}

/* Average distance in samples between the low points of a modulated DC signal (left channel) */
double lfoPeriod(const juce::AudioBuffer<float>& b, int from)
{
    auto* x = b.getReadPointer(0);
    std::vector<int> minima;
    const int w = 200;
    for (int i = from + w; i < b.getNumSamples() - w; ++i)
    {
        bool isMin = x[i] < 0.1f;
        for (int k = -w; k <= w && isMin; ++k)
            if (x[i + k] < x[i]) isMin = false;
        if (isMin && (minima.empty() || i - minima.back() > 4 * w))
            minima.push_back(i);
    }
    return minima.size() < 3 ? 0.0 : (minima.back() - minima.front()) / double(minima.size() - 1);
}

juce::String dB(float x) { return juce::String(juce::Decibels::gainToDecibels(x, -300.0f), 1) + " dBFS"; }

/* A host clock at a settable tempo, playing from the top */
struct Clock : juce::AudioPlayHead
{
    double bpm = 120, rate = 48000;
    juce::int64 sample = 0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm(bpm);
        p.setTimeSignature(TimeSignature { 4, 4 });
        p.setIsPlaying(false);
        p.setTimeInSamples(sample);
        return p;
    }
};

//==============================================================================
// 1. Load, prepare, process
//==============================================================================
void section1()
{
    const std::vector<int> blocks { 32, 64, 128, 256, 512, 1024, 2048, 1000, 333, 37 };

    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        auto p = create(rate, 2048);
        if (p == nullptr) return;
        auto in = sine(rate, 3);
        juce::AudioBuffer<float> withSilence(2, in.getNumSamples() + (int) rate);
        withSilence.clear();
        for (int c = 0; c < 2; ++c) withSilence.copyFrom(c, 0, in, c, 0, in.getNumSamples());

        auto out = render(*p, withSilence, blocks);
        auto s = stats(out), si = stats(in);
        auto tail = stats(out, out.getNumSamples() - 4800);
        auto name = "1 process at " + juce::String(rate / 1000.0, 1) + " kHz, blocks 32-2048";
        check(s.nonFinite == 0 && s.subnormal == 0 && s.peak < 2.0f
                  && std::abs(juce::Decibels::gainToDecibels(s.rms / juce::jmax(1e-9f, stats(in).rms))) < 12.0f
                  && tail.peak < 1.0e-4f,
              name, "peak " + dB(s.peak) + " (input " + dB(si.peak) + "), NaN/inf " + juce::String(s.nonFinite)
                  + ", denormals " + juce::String(s.subnormal) + ", tail after 1 s silence " + dB(tail.peak));
    }

    // Change sample rate and block size on a running instance, as a host does when the project changes
    {
        auto p = create(44100, 512);
        if (p == nullptr) return;
        lowBandLfoOnly(*p, false, 2.0f);              // set before the change: nothing moves afterwards
        render(*p, sine(44100, 1), { 512 });
        p->releaseResources();
        p->setRateAndBufferSizeDetails(96000, 2048);
        p->prepareToPlay(96000, 2048);
        auto out = render(*p, dc(96000, 3), { 2048, 31, 700 });
        auto s = stats(out);
        auto period = lfoPeriod(out, 9600);
        check(s.nonFinite == 0 && std::abs(period - 48000.0) < 480.0,
              "1 change 44.1 to 96 kHz mid-run: LFO keeps its speed",
              "2 Hz LFO period " + juce::String(period, 0) + " samples (expect 48000)");
    }
}

//==============================================================================
// 2. Automation
//==============================================================================
bool expectsNoEffect(const juce::String& name)
{
    // Display preferences, and Rate (the LFOs are synced by default, so Rate isn't used)
    return name.startsWith("Show") || name.startsWith("Stack") || name.startsWith("Scope")
        || name.startsWith("FFT") || name.startsWith("Rate");
}

void section2()
{
    const double rate = 48000;
    auto input = threeTones(rate, 2);

    juce::AudioBuffer<float> reference;
    float refJump = 0;
    int numParams = 0;
    {
        auto p = create(rate, 512);
        if (p == nullptr) return;
        numParams = p->getParameters().size();
        reference = render(*p, input, { 64 });
        refJump = stats(reference).maxJump;
        report("INFO", "2 reference render (all defaults)", "largest sample-to-sample step " + juce::String(refJump, 4)
               + ", " + juce::String(numParams) + " parameters");
    }

    for (int index = 0; index < numParams; ++index)
    {
        auto p = create(rate, 512);
        if (p == nullptr) return;
        auto* prm = p->getParameters()[index];
        if (! prm->isAutomatable())
            continue;

        const auto name = prm->getName(256);
        const bool isSwitch = prm->isBoolean() || prm->isDiscrete();
        const int total = input.getNumSamples();

        // Ramp from 0 to 1 over the render, a new value every 64 samples, like host automation
        auto out = render(*p, input, { 64 }, [prm, total](juce::AudioPluginInstance&, int pos) {
            prm->setValueNotifyingHost((float) pos / (float) total);
        });

        auto s = stats(out);
        auto diff = maxDiff(out, reference);
        juce::String label = "2 automate " + name;

        if (s.nonFinite > 0)
        {
            check(false, label, "NaN or inf in the output");
            continue;
        }

        if (expectsNoEffect(name))
        {
            check(diff == 0.0f, label, diff == 0.0f ? "no effect on the sound, as expected" : "changed the sound by " + juce::String(diff));
            continue;
        }

        if (diff < 1.0e-4f)
        {
            check(false, label, "expected to change the sound, max change " + juce::String(diff));
            continue;
        }

        if (isSwitch)
        {
            // A switch (shape, rhythm, on/off) changes instantly by design: report the step, don't judge it
            report("INFO", label, "switches instantly; largest step " + juce::String(s.maxJump, 4) + " (reference " + juce::String(refJump, 4) + ")");
            continue;
        }

        // A smooth parameter must not make clicks. Gains may make the signal up to 24 dB (x15.85) louder.
        const float gainHeadroom = name.contains("Gain") ? 15.85f : 1.0f;
        const float limit = refJump * gainHeadroom * 2.0f;
        check(s.maxJump <= limit, label, "largest step " + juce::String(s.maxJump, 4) + " (limit " + juce::String(limit, 4) + ")");
    }
}

//==============================================================================
// 3. Session save and restore
//==============================================================================
void section3()
{
    const double rate = 48000;
    auto input = testSignal(rate, 2);
    juce::Random rnd(99);
    juce::MemoryBlock state;
    juce::Array<float> values;
    juce::AudioBuffer<float> first, second;
    juce::AudioBuffer<float> silence(2, 256);
    silence.clear();

    // A session with random settings
    juce::MemoryBlock session;
    {
        auto a = create(rate, 512);
        if (a == nullptr) return;
        for (auto* prm : a->getParameters())
            if (prm != a->getBypassParameter() && prm->getName(256) != "Program")   // not the host's preset selector
                prm->setValueNotifyingHost(rnd.nextFloat());
        render(*a, silence, { 256 });                 // let the changes reach the plugin
        a->getStateInformation(session);
    }

    // The session opened: its values, its sound, and its state saved again. (The host's own copy of values it
    // sent isn't rounded to the plugin's steps, so the values are read from an instance that loaded the session.)
    {
        auto p = create(rate, 512);
        if (p == nullptr) return;
        p->setStateInformation(session.getData(), (int) session.getSize());
        render(*p, silence, { 256 });
        for (auto* prm : p->getParameters())
            values.add(prm->getValue());
        p->getStateInformation(state);
        first = render(*p, input, { 512 });
    }   // destroyed, as when the project is closed

    auto b = create(rate, 512);
    if (b == nullptr) return;
    b->setStateInformation(state.getData(), (int) state.getSize());
    render(*b, silence, { 256 });
    int mismatches = 0;
    juce::String firstMismatch;
    for (int i = 0; i < b->getParameters().size(); ++i)
        if (b->getParameters()[i]->getValue() != values[i])
        {
            if (mismatches++ == 0)
                firstMismatch = b->getParameters()[i]->getName(64) + " " + juce::String(values[i]) + " -> " + juce::String(b->getParameters()[i]->getValue());
        }
    check(mismatches == 0, "3 restore: every parameter exactly the same",
          juce::String(values.size()) + " parameters, " + juce::String(mismatches) + " differ " + firstMismatch);

    juce::MemoryBlock resaved;
    b->getStateInformation(resaved);
    check(resaved == state, "3 restore: saving again gives the same bytes", juce::String((int) state.getSize()) + " bytes");

    second = render(*b, input, { 512 });
    auto d = maxDiff(first, second);
    check(d == 0.0f, "3 restore: render bit-identical to before the save", d == 0 ? "identical" : "max diff " + juce::String(d));
}

//==============================================================================
// 4. Tempo
//==============================================================================
void section4()
{
    const double rate = 48000;
    auto p = create(rate, 512);
    if (p == nullptr) return;
    Clock clock;
    p->setPlayHead(&clock);
    lowBandLfoOnly(*p, true, 3.0f);   // synced, 1/4 note: one LFO cycle per beat

    for (double bpm : { 120.0, 90.0, 60.0, 140.0 })
    {
        clock.bpm = bpm;
        auto out = render(*p, dc(rate, 4), { 512 }, [&clock](juce::AudioPluginInstance&, int pos) { clock.sample = pos; });
        auto period = lfoPeriod(out, 4800);
        auto expected = rate * 60.0 / bpm;
        check(std::abs(period - expected) < expected * 0.01, "4 synced 1/4-note LFO at " + juce::String(bpm, 0) + " bpm",
              "period " + juce::String(period, 0) + " samples (expect " + juce::String(expected, 0) + ")");
    }
    p->setPlayHead(nullptr);
}

//==============================================================================
// 5. Bypass, mute, solo
//==============================================================================
juce::AudioBuffer<float> renderWith(std::function<void(juce::AudioPluginInstance&)> setUp, const juce::AudioBuffer<float>& input)
{
    auto p = create(48000, 512);
    if (p == nullptr) return {};
    setUp(*p);
    juce::AudioBuffer<float> silence(2, 256);
    silence.clear();
    render(*p, silence);
    return render(*p, input);
}

void section5()
{
    auto input = testSignal(48000, 2);

    auto bypassed = renderWith([](juce::AudioPluginInstance& p) {
        if (auto* b = p.getBypassParameter()) b->setValueNotifyingHost(1.0f);
    }, input);
    auto d = maxDiff(bypassed, input);
    check(d < 1.0e-7f, "5 host bypass passes the audio through untouched", "max diff " + juce::String(d));

    auto allMuted = renderWith([](juce::AudioPluginInstance& p) {
        for (auto n : { "Mute Low Band", "Mute Mid Band", "Mute High Band" }) setNorm(p, n, 1.0f);
    }, input);
    check(stats(allMuted).peak == 0.0f, "5 all three bands muted: silence", "peak " + dB(stats(allMuted).peak));

    auto soloMid = renderWith([](juce::AudioPluginInstance& p) { setNorm(p, "Solo Mid Band", 1.0f); }, input);
    auto muteOthers = renderWith([](juce::AudioPluginInstance& p) {
        setNorm(p, "Mute Low Band", 1.0f);
        setNorm(p, "Mute High Band", 1.0f);
    }, input);
    d = maxDiff(soloMid, muteOthers);
    check(d == 0.0f && stats(soloMid).peak > 0.01f, "5 solo mid = mute low and high", "max diff " + juce::String(d));

    auto soloWins = renderWith([](juce::AudioPluginInstance& p) {
        setNorm(p, "Solo Mid Band", 1.0f);
        setNorm(p, "Mute Mid Band", 1.0f);
    }, input);
    d = maxDiff(soloWins, soloMid);
    check(d == 0.0f, "5 solo wins over mute on the same band", "max diff " + juce::String(d));

    auto bandsBypassed = renderWith([](juce::AudioPluginInstance& p) {
        for (auto n : { "Bypassed Low Band", "Bypassed Mid Band", "Bypassed High Band" }) setNorm(p, n, 1.0f);
    }, input);
    auto depthZero = renderWith([](juce::AudioPluginInstance& p) {
        for (auto n : { "Depth Low Band", "Depth Mid Band", "Depth High Band" }) setNorm(p, n, 0.0f);
    }, input);
    d = maxDiff(bandsBypassed, depthZero);
    check(d == 0.0f, "5 band bypass = no tremolo (same as depth 0)", "max diff " + juce::String(d));

    auto lowBypassed = renderWith([](juce::AudioPluginInstance& p) { setNorm(p, "Bypassed Low Band", 1.0f); }, input);
    auto lowDepthZero = renderWith([](juce::AudioPluginInstance& p) { setNorm(p, "Depth Low Band", 0.0f); }, input);
    d = maxDiff(lowBypassed, lowDepthZero);
    check(d == 0.0f, "5 low band bypass affects only the low band", "max diff " + juce::String(d));
}

//==============================================================================
// 6. Demo mode
//==============================================================================
void section6(bool expectDemo)
{
    const double rate = 48000;
    auto p = create(rate, 512);
    if (p == nullptr) return;
    lowBandLfoOnly(*p, false, 4.0f);
    auto out = render(*p, dc(rate, 63), { 512 });

    // 250 ms windows (one LFO cycle): modulated = effect on, flat = the demo's gap
    const int w = (int) (rate / 4);
    double gapStart = -1, gapEnd = -1;
    int flatOutsideGap = 0;
    for (int i = (int) rate; i + w <= out.getNumSamples(); i += w)
    {
        auto range = juce::FloatVectorOperations::findMinAndMax(out.getReadPointer(0) + i, w);
        const bool flat = range.getLength() < 0.01f;
        const double t = i / rate;
        if (flat && gapStart < 0) gapStart = t;
        if (flat) gapEnd = t + 0.25;
        if (flat && t > 61) ++flatOutsideGap;
    }

    if (gapStart < 0)
    {
        if (expectDemo)
            check(false, "6 demo: effect off 10 s in every 60 s", "no gap found, but this machine has no key");
        else
            report("SKIP", "6 demo: effect off 10 s in every 60 s", "no gap: Tertiary is activated on this machine");
        return;
    }

    check(std::abs(gapStart - 50.0) <= 0.3 && std::abs(gapEnd - 60.0) <= 0.3 && flatOutsideGap == 0,
          "6 demo: effect off 10 s in every 60 s",
          "gap from " + juce::String(gapStart, 2) + " s to " + juce::String(gapEnd, 2) + " s (expect 50 to 60)");
}

//==============================================================================
// 7. License states, with the pretend server
//==============================================================================
void section7()
{
    const juce::int64 day = 24LL * 60 * 60 * 1000, now = 1800000000000LL;

    {
        TempSettings s;
        s.activated(now - 8 * day, now - 8 * day);
        PretendServer server;
        server.validateReply = PretendServer::reply("key_valid");
        auto r = LicenseCheck::runStartupCheck(server, s.get(), now, "1.1.0");
        check(! r.revoked && ! r.offlineTooLong && s.get().getValue("licenseKey").isNotEmpty(),
              "7 license valid: stays activated", "validate called " + juce::String(server.validateCalls) + "x");
    }
    {
        TempSettings s;
        s.activated(now - 8 * day, now - 8 * day);
        PretendServer server;
        server.validateReply = PretendServer::reply("invalid_order", "Please complete your order to activate the serial key.");
        auto r = LicenseCheck::runStartupCheck(server, s.get(), now, "1.1.0");
        check(r.revoked && s.get().getValue("licenseKey").isEmpty() && r.message.isNotEmpty(),
              "7 license revoked (refund): demo, with a message", "\"" + r.message.substring(0, 60) + "...\"");
    }
    {
        TempSettings s;
        s.activated(now - 20 * day, now - 20 * day);
        PretendServer server;   // unreachable
        auto r = LicenseCheck::runStartupCheck(server, s.get(), now, "1.1.0");
        check(! r.revoked && r.message.isEmpty() && s.get().getValue("licenseKey").isNotEmpty(),
              "7 server unreachable: nothing changes", "no message, key kept");
    }
    {
        TempSettings s;
        s.activated(now - 31 * day, now - 31 * day);
        PretendServer server;
        auto r = LicenseCheck::runStartupCheck(server, s.get(), now, "1.1.0");
        check(! r.revoked && r.offlineTooLong && s.get().getValue("licenseKey").isNotEmpty(),
              "7 offline 31 days: a note only, still activated", "\"" + r.message.substring(0, 60) + "...\"");
    }
}

//==============================================================================
// 8. Golden renders
//==============================================================================
struct GoldenSetting { juce::String name; std::function<void(juce::AudioPluginInstance&)> setUp; };

std::vector<GoldenSetting> goldenSettings()
{
    return {
        { "default", [](juce::AudioPluginInstance&) {} },
        { "varied", [](juce::AudioPluginInstance& p) {
            setNorm(p, "Input Gain", gainNorm(3));
            setNorm(p, "Waveshape Low Band", choiceNorm(2, 7));        // Square
            setNorm(p, "Depth Low Band", 1.0f);
            setNorm(p, "Skew Low Band", skewNorm(30));
            setNorm(p, "Invert Low Band", 1.0f);
            setNorm(p, "Multiplier Low Band", choiceNorm(3, 6));       // 1/8
            setNorm(p, "Waveshape Mid Band", choiceNorm(3, 7));        // Triangle
            setNorm(p, "Depth Mid Band", percentNorm(60));
            setNorm(p, "Relative Phase Mid Band", phaseNorm(90));
            setNorm(p, "Multiplier Mid Band", choiceNorm(2, 6));       // 1/4 triplet
            setNorm(p, "Waveshape High Band", choiceNorm(0, 7));       // Ramp Down
            setNorm(p, "Relative Phase High Band", phaseNorm(-120));
            setNorm(p, "Multiplier High Band", choiceNorm(5, 6));      // 1/16
            setNorm(p, "High Output Gain", gainNorm(4));
        } },
        { "free", [](juce::AudioPluginInstance& p) {
            for (auto band : { "Low", "Mid", "High" })
                setNorm(p, juce::String("Sync to Host ") + band + " Band", 0.0f);
            setNorm(p, "Rate Low Band", rateNorm(0.5f));
            setNorm(p, "Rate Mid Band", rateNorm(4.7f));
            setNorm(p, "Rate High Band", rateNorm(12.5f));
            setNorm(p, "Waveshape Low Band", choiceNorm(5, 7));        // Hills
            setNorm(p, "Waveshape Mid Band", choiceNorm(6, 7));        // Valleys
            setNorm(p, "Depth Mid Band", percentNorm(90));
        } },
    };
}

/* Per platform: Windows and Intel Macs have given bit-identical renders; Apple Silicon fuses
   multiply-adds, measured at 2.7e-7 (-131 dBFS). The limits leave room above that and are
   still about 100 dB below the music. */
float goldenTolerance()
{
   #if JUCE_ARM
    return 1.0e-5f;     // -100 dBFS
   #else
    return 1.0e-6f;     // -120 dBFS
   #endif
}

void section8(const juce::File& folder, bool make)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto input = testSignal(48000, 2);

    for (auto& setting : goldenSettings())
    {
        auto out = renderWith(setting.setUp, input);
        auto file = folder.getChildFile(setting.name + ".wav");

        if (make)
        {
            folder.createDirectory();
            file.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(file), 48000, 2, 32, {}, 0));
            writer->writeFromAudioSampleBuffer(out, 0, out.getNumSamples());
            report("INFO", "8 golden " + setting.name, "written to " + file.getFullPathName());
            continue;
        }

        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr)
        {
            check(false, "8 golden " + setting.name, "reference missing: " + file.getFullPathName());
            continue;
        }
        juce::AudioBuffer<float> golden(2, (int) reader->lengthInSamples);
        reader->read(&golden, 0, golden.getNumSamples(), 0, true, true);
        auto d = maxDiff(out, golden);
        check(golden.getNumSamples() == out.getNumSamples() && d <= goldenTolerance(), "8 golden " + setting.name,
              (d == 0 ? juce::String("identical") : "max diff " + juce::String(d) + " (" + dB(d) + ")")
              + ", tolerance " + juce::String(goldenTolerance()));
    }
}

//==============================================================================
// 9. Presets through the host's program list  [Claude 2026-10-27]
//==============================================================================
juce::File oldSessionFile(const juce::File& golden)
{
    return golden.getChildFile("session-1.0.2-" + description.pluginFormatName + ".bin");
}

void section9(const juce::File& golden)
{
    const auto& factory = PresetManager::getFactoryPresets();
    auto input = testSignal(48000, 2);
    juce::AudioBuffer<float> silence(2, 256);
    silence.clear();

    auto p = create(48000, 512);
    if (p == nullptr) return;

    // a. The list
    juce::StringArray missing;
    for (int i = 0; i < (int) factory.size(); ++i)
        if (p->getProgramName(i) != factory[(size_t) i].name)
            missing.add(factory[(size_t) i].name + " (got \"" + p->getProgramName(i) + "\")");
    check(p->getNumPrograms() >= (int) factory.size() && missing.isEmpty(), "9 host sees the factory presets in order",
          juce::String(p->getNumPrograms()) + " programs " + missing.joinIntoString(", "));

    // b. Each preset loads through the host and changes the sound (Default is the reference)
    juce::AudioBuffer<float> defaultRender;
    int silentPresets = 0, wrongIndex = 0;
    juce::String silentNames;
    for (int i = 0; i < (int) factory.size(); ++i)
    {
        auto q = create(48000, 512);
        q->setCurrentProgram(i);
        render(*q, silence);
        if (q->getCurrentProgram() != i) ++wrongIndex;
        auto out = render(*q, input);
        if (i == 0)
            defaultRender = out;
        else if (maxDiff(out, defaultRender) < 1.0e-3f)
        {
            ++silentPresets;
            silentNames << factory[(size_t) i].name << " ";
        }
    }
    check(wrongIndex == 0 && silentPresets == 0, "9 every preset loads by program number and changes the sound",
          juce::String((int) factory.size()) + " presets, " + juce::String(wrongIndex) + " wrong program number, "
          + juce::String(silentPresets) + " same as Default " + silentNames);

    // c. Default through the program list sounds exactly like the golden default
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(golden.getChildFile("default.wav")));
        if (reader != nullptr)
        {
            auto q = create(48000, 512);
            q->setCurrentProgram(6);
            q->setCurrentProgram(0);
            render(*q, silence);
            auto out = render(*q, input);
            juce::AudioBuffer<float> ref(2, (int) reader->lengthInSamples);
            reader->read(&ref, 0, ref.getNumSamples(), 0, true, true);
            auto d = maxDiff(out, ref);
            check(d <= goldenTolerance(), "9 preset Default = golden default render", "max diff " + juce::String(d));
        }
    }

    // d. The preset is remembered in the session
    {
        juce::MemoryBlock state;
        juce::AudioBuffer<float> first;
        {
            auto a = create(48000, 512);
            a->setCurrentProgram(9);
            render(*a, silence);
            a->getStateInformation(state);
            first = render(*a, input);
        }
        auto b = create(48000, 512);
        b->setStateInformation(state.getData(), (int) state.getSize());
        render(*b, silence);
        auto d = maxDiff(render(*b, input), first);
        // An Audio Unit reports a state loaded from a session as preset -1 (a custom state): that is how AU hosts
        // work. The settings are what count (the render), and Tertiary's own bar keeps the name (unit-tested).
        const bool au = description.pluginFormatName == "AudioUnit";
        const int program = b->getCurrentProgram();
        check((program == 9 || (au && program == -1)) && d == 0.0f, "9 session remembers the preset (" + factory[9].name + ")",
              "program " + juce::String(program) + (au && program == -1 ? " (AU: a restored state is preset -1)" : "")
              + ", render max diff " + juce::String(d));
    }

    // e. A session saved by Tertiary 1.0.2 loads with the same values
    {
        auto file = oldSessionFile(golden);
        juce::MemoryBlock old;
        juce::StringArray expected;
        if (! file.loadFileAsData(old) || ! file.withFileExtension(".txt").exists())
        {
            report("SKIP", "9 a 1.0.2 session loads unchanged", "no 1.0.2 " + description.pluginFormatName + " session to load (1.0.2 had no AU)");
            return;
        }
        expected.addLines(file.withFileExtension(".txt").loadFileAsString());
        auto b = create(48000, 512);
        b->setStateInformation(old.getData(), (int) old.getSize());
        render(*b, silence);
        int differ = 0;
        juce::String first;
        for (auto& line : expected)
        {
            auto name = line.upToLastOccurrenceOf("=", false, false);
            if (name.isEmpty()) continue;
            auto* prm = findParam(*b, name);
            const float want = line.fromLastOccurrenceOf("=", false, false).getFloatValue();
            if (prm == nullptr || std::abs(prm->getValue() - want) > 1.0e-6f)
            {
                if (differ++ < 3) first << name << " " << juce::String(want) << " -> " << (prm ? juce::String(prm->getValue()) : juce::String("missing")) << "; ";
            }
        }
        check(differ == 0, "9 a 1.0.2 session loads unchanged", juce::String(expected.size()) + " values, " + juce::String(differ) + " differ " + first);
    }
}

//==============================================================================
// 10. The window through the host  [Claude 2026-10-30]
//==============================================================================
/* RC1 bug (Joe, Cubase on his laptop): the window opened nearly full screen, black, with the UI at its normal
   size in the corner; choosing a size from the menu put it right. Here the built VST3 is opened the way a DAW
   opens it (createView, the host's scale factor, attached), at each window size and host scale, and the
   plug-in's own window (its HWND, in screen pixels) must be exactly the UI's size: 750 x 478 x the chosen size
   x the host's scale. Then the host gives the window a different size, as Cubase did, and the plug-in must put
   it back. Windows only (the plug-in's window is found by its HWND). */
#if JUCE_WINDOWS
struct PluginWindowProbe
{
    HWND top = nullptr, plugin = nullptr;
    juce::String hostClass;

    static juce::String className(HWND h)
    {
        wchar_t buf[256] = {};
        GetClassNameW(h, buf, 255);
        return juce::String(buf);
    }

    static BOOL CALLBACK visit(HWND h, LPARAM self)
    {
        auto& probe = *reinterpret_cast<PluginWindowProbe*>(self);
        // the plug-in's JUCE has its own window class (another copy of JUCE, another class name)
        if (probe.plugin == nullptr && className(h).startsWith("JUCE_") && className(h) != probe.hostClass)
            probe.plugin = h;
        return TRUE;
    }

    explicit PluginWindowProbe(juce::Component& hostWindow)
    {
        if (auto* peer = hostWindow.getPeer())
        {
            top = (HWND) peer->getNativeHandle();
            hostClass = className(top);
            EnumChildWindows(top, visit, (LPARAM) this);
        }
    }

    static juce::Rectangle<int> clientSize(HWND h)
    {
        RECT r {};
        GetClientRect(h, &r);
        return { 0, 0, (int) (r.right - r.left), (int) (r.bottom - r.top) };
    }
};
#endif

void pump(int ms)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
}

#if JUCE_WINDOWS
/* A small host that opens the plug-in's view itself, in the order Cubase does: the frame, the host's scale,
   getSize, a window of that size, attached. Its frame can answer the plug-in's resize requests made while the
   view is being attached with another size (the whole screen), as Cubase did on Joe's laptop (RC1: the window
   nearly full screen, black, the UI at its size in the corner), and answers every later request as asked (Joe:
   choosing a size from the menu put it right). */
struct DawLikeHost
{
    struct Frame : public Steinberg::IPlugFrame
    {
        HWND window = nullptr;
        Steinberg::ViewRect attachAnswer {};   // empty: answer every request as asked
        bool attaching = false;
        bool alwaysWrong = false;   // answer every request with attachAnswer (a host that never agrees)
        int requests = 0;

        Steinberg::tresult PLUGIN_API resizeView(Steinberg::IPlugView* view, Steinberg::ViewRect* asked) override
        {
            if (view == nullptr || asked == nullptr) return Steinberg::kInvalidArgument;
            ++requests;
            auto answer = ((attaching || alwaysWrong) && attachAnswer.getWidth() > 0) ? attachAnswer : *asked;
            SetWindowPos(window, nullptr, 0, 0, answer.getWidth(), answer.getHeight(), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            view->onSize(&answer);
            return Steinberg::kResultTrue;
        }
        Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override
        {
            if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::IPlugFrame::iid) || Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid))
            { *obj = this; return Steinberg::kResultOk; }
            *obj = nullptr;
            return Steinberg::kNoInterface;
        }
        Steinberg::uint32 PLUGIN_API addRef() override  { return 1; }
        Steinberg::uint32 PLUGIN_API release() override { return 1; }
    };

    HMODULE module = nullptr;
    Steinberg::IPluginFactory* factory = nullptr;
    Steinberg::Vst::IComponent* component = nullptr;
    Steinberg::Vst::IEditController* controller = nullptr;
    Steinberg::Vst::IConnectionPoint* componentPoint = nullptr;
    Steinberg::Vst::IConnectionPoint* controllerPoint = nullptr;
    Steinberg::IPlugView* view = nullptr;
    Frame frame;
    juce::String error;

    explicit DawLikeHost(const juce::File& bundle)
    {
        using namespace Steinberg;
        const auto binary = bundle.getChildFile("Contents").getChildFile("x86_64-win").getChildFile(bundle.getFileName());
        module = LoadLibraryW(binary.getFullPathName().toWideCharPointer());
        if (module == nullptr) { error = "can't load " + binary.getFullPathName(); return; }
        using GetFactory = IPluginFactory* (PLUGIN_API*)();
        if (auto get = (GetFactory) (void*) GetProcAddress(module, "GetPluginFactory")) factory = get();
        if (factory == nullptr) { error = "no factory"; return; }
        for (int32 i = 0; i < factory->countClasses() && component == nullptr; ++i)
        {
            PClassInfo info;
            if (factory->getClassInfo(i, &info) == kResultOk && std::strcmp(info.category, kVstAudioEffectClass) == 0)
                factory->createInstance(info.cid, Vst::IComponent::iid, (void**) &component);
        }
        if (component == nullptr || component->initialize(nullptr) != kResultOk) { error = "no component"; return; }
        TUID controllerId;
        component->getControllerClassId(controllerId);
        factory->createInstance(controllerId, Vst::IEditController::iid, (void**) &controller);
        if (controller == nullptr || controller->initialize(nullptr) != kResultOk) { error = "no controller"; return; }
        component->queryInterface(Vst::IConnectionPoint::iid, (void**) &componentPoint);
        controller->queryInterface(Vst::IConnectionPoint::iid, (void**) &controllerPoint);
        if (componentPoint == nullptr || controllerPoint == nullptr) { error = "no connection points"; return; }
        controllerPoint->connect(componentPoint);
        componentPoint->connect(controllerPoint);
    }

    /* Opens the view at the host's scale, as Cubase does */
    void open(float hostScale, Steinberg::ViewRect attachAnswer = {})
    {
        using namespace Steinberg;
        frame.attachAnswer = attachAnswer;
        frame.requests = 0;
        view = controller->createView(Vst::ViewType::kEditor);
        if (view == nullptr) { error = "no view"; return; }
        view->setFrame(&frame);
        IPlugViewContentScaleSupport* scale = nullptr;
        if (view->queryInterface(IPlugViewContentScaleSupport::iid, (void**) &scale) == kResultOk && scale != nullptr)
        {
            scale->setContentScaleFactor(hostScale);
            scale->release();
        }
        ViewRect size;
        view->getSize(&size);
        frame.window = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"DawLikeHost", WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
                                       40, 40, size.getWidth(), size.getHeight(), nullptr, nullptr, nullptr, nullptr);
        frame.attaching = true;
        view->attached((void*) frame.window, kPlatformTypeHWND);
        frame.attaching = false;
    }

    /* The scale of the screen the host's window opens on (40, 40), as a DAW sends it */
    static float screenScale()
    {
        const POINT p { 40, 40 };
        UINT x = 96, y = 96;
        using GetDpiForMonitorFn = HRESULT (WINAPI*)(HMONITOR, int, UINT*, UINT*);
        if (auto shcore = LoadLibraryW(L"shcore.dll"))
        {
            if (auto get = (GetDpiForMonitorFn) (void*) GetProcAddress(shcore, "GetDpiForMonitor"))
                get(MonitorFromPoint(p, MONITOR_DEFAULTTOPRIMARY), 0, &x, &y);
            FreeLibrary(shcore);
        }
        return (float) x / 96.0f;
    }

    /* The scale of the screen the host's window is on now */
    float windowScale() const   { return frame.window != nullptr ? (float) GetDpiForWindow(frame.window) / 96.0f : screenScale(); }

    HWND pluginWindow() const   { return frame.window != nullptr ? GetWindow(frame.window, GW_CHILD) : nullptr; }

    static juce::Rectangle<int> clientSize(HWND h)
    {
        RECT r {};
        if (h != nullptr) GetClientRect(h, &r);
        return { 0, 0, (int) (r.right - r.left), (int) (r.bottom - r.top) };
    }

    void close()
    {
        if (view != nullptr)
        {
            view->removed();
            view->setFrame(nullptr);
            view->release();
            view = nullptr;
        }
        if (frame.window != nullptr) { DestroyWindow(frame.window); frame.window = nullptr; }
    }

    ~DawLikeHost()
    {
        close();
        if (componentPoint != nullptr && controllerPoint != nullptr)
        {
            componentPoint->disconnect(controllerPoint);
            controllerPoint->disconnect(componentPoint);
        }
        if (componentPoint != nullptr) componentPoint->release();
        if (controllerPoint != nullptr) controllerPoint->release();
        if (controller != nullptr) { controller->terminate(); controller->release(); }
        if (component != nullptr) { component->terminate(); component->release(); }
        if (factory != nullptr) factory->release();
        if (module != nullptr) FreeLibrary(module);
    }
};
#endif

void section10()
{
   #if ! JUCE_WINDOWS
    report("SKIP", "10 window size through the host", "Windows only");
   #else
    // The plug-in reads its window size from Tertiary's settings file; put back what was there afterwards
    // (a fresh read each time: the plug-in writes the file too, when its window closes)
    struct RestoreSetting   // put back on every way out of this section, after the plug-ins below are gone
    {
        bool had = juce::PropertiesFile(LicenseCheck::getSettingsOptions()).containsKey("uiScale");
        juce::String before = juce::PropertiesFile(LicenseCheck::getSettingsOptions()).getValue("uiScale");
        ~RestoreSetting()
        {
            juce::PropertiesFile props(LicenseCheck::getSettingsOptions());
            if (had) props.setValue("uiScale", before); else props.removeValue("uiScale");
            props.saveIfNeeded();
        }
    } restoreSetting;
    auto setRemembered = [](float scale) {
        juce::PropertiesFile props(LicenseCheck::getSettingsOptions());
        props.setValue("uiScale", (double) scale);
        props.saveIfNeeded();
    };

    const double system = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale;
    report("INFO", "10 this screen's scaling", juce::String(juce::roundToInt(system * 100)) + " %");

    for (float uiScale : { 1.0f, 1.25f, 1.5f, 2.0f })
        for (float hostScale : { 1.0f, 1.25f, 1.5f })
        {
            setRemembered(uiScale);
            auto p = create(48000, 512);
            if (p == nullptr) return;
            const auto label = "10 " + juce::String(juce::roundToInt(uiScale * 100)) + " % window, host scale "
                               + juce::String(hostScale, 2);

            std::unique_ptr<juce::AudioProcessorEditor> hostWindow(p->createEditorIfNeeded());
            if (hostWindow == nullptr) { check(false, label, "no editor"); continue; }
            hostWindow->setScaleFactor(hostScale);   // the host's scale before the window is shown, as a DAW does
            hostWindow->addToDesktop(0);
            hostWindow->setTopLeftPosition(40, 40);
            hostWindow->setVisible(true);
            pump(900);   // the plug-in checks the screen's scale on a 500 ms timer

            const double total = system * hostScale;
            const int wantW = juce::roundToInt(juce::roundToInt(750 * uiScale) * total);
            const int wantH = juce::roundToInt(juce::roundToInt(478 * uiScale) * total);
            auto fits = [](juce::Rectangle<int> r, int w, int h) { return std::abs(r.getWidth() - w) <= 2 && std::abs(r.getHeight() - h) <= 2; };
            auto show = [](juce::Rectangle<int> r) { return juce::String(r.getWidth()) + " x " + juce::String(r.getHeight()); };

            PluginWindowProbe probe(*hostWindow);
            if (probe.plugin == nullptr) { check(false, label, "the plug-in's window was not found"); continue; }
            const auto opened = PluginWindowProbe::clientSize(probe.plugin);
            check(fits(opened, wantW, wantH), label + ": opens at the UI's size",
                  show(opened) + " screen pixels (want " + juce::String(wantW) + " x " + juce::String(wantH) + ")");

            // The host sizes the plug-in's window itself (as a user dragging, or a host restoring a size): it goes back
            SetWindowPos(probe.plugin, nullptr, 0, 0, wantW * 2, wantH * 2, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            pump(900);
            const auto after = PluginWindowProbe::clientSize(probe.plugin);
            check(fits(after, wantW, wantH), label + ": sized otherwise by the host, it goes back",
                  show(after) + " screen pixels (want " + juce::String(wantW) + " x " + juce::String(wantH) + ")");

            // The host's scale changes while the window is open (a move to another screen, a new Windows setting)
            if (juce::approximatelyEqual(hostScale, 1.0f))
                for (float live : { 1.25f, 1.5f, 1.0f })
                {
                    hostWindow->setScaleFactor(live);
                    pump(900);
                    const int w = juce::roundToInt(juce::roundToInt(750 * uiScale) * system * live);
                    const int h = juce::roundToInt(juce::roundToInt(478 * uiScale) * system * live);
                    const auto now = PluginWindowProbe::clientSize(probe.plugin);
                    check(fits(now, w, h), label + ": host scale changed to " + juce::String(live, 2) + " while open",
                          show(now) + " screen pixels (want " + juce::String(w) + " x " + juce::String(h) + ")");
                }

            hostWindow.reset();
            p.reset();
            pump(50);
        }

    // A host that answers the first resize request with the whole screen (Cubase on Joe's laptop, RC1)
    {
        DawLikeHost host(juce::File(description.fileOrIdentifier));
        // Cubase sends the screen's scale; JUCE's plug-in follows the screen the window is on
        const float hostScale = DawLikeHost::screenScale();
        report("INFO", "10 Cubase-like host: Windows scaling", juce::String(juce::roundToInt(hostScale * 100)) + " %");
        for (float uiScale : { 1.0f, 1.25f, 1.5f, 2.0f })
            for (bool wrongFirstAnswer : { false, true })
            {
                if (host.error.isNotEmpty()) break;
                setRemembered(uiScale);
                const auto label = "10 Cubase-like host, " + juce::String(juce::roundToInt(uiScale * 100)) + " % window"
                                   + (wrongFirstAnswer ? ", answers the whole screen while opening" : "");
                const juce::Rectangle<int> screen { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
                host.open(hostScale, wrongFirstAnswer ? Steinberg::ViewRect { 0, 0, juce::roundToInt(screen.getWidth()), juce::roundToInt(screen.getHeight()) - 72 }
                                                      : Steinberg::ViewRect {});
                pump(1200);
                if (const char* hold = std::getenv("WLA_HOSTTEST_HOLD"))   // time to take a screenshot of the window
                    pump(juce::String(hold).getIntValue());
                const float settled = host.windowScale();   // JUCE's plug-in follows the screen its window is on
                const int wantW = juce::roundToInt(juce::roundToInt(750 * uiScale) * settled);
                const int wantH = juce::roundToInt(juce::roundToInt(478 * uiScale) * settled);
                const auto plugin = DawLikeHost::clientSize(host.pluginWindow());
                const auto window = DawLikeHost::clientSize(host.frame.window);
                auto show = [](juce::Rectangle<int> r) { return juce::String(r.getWidth()) + " x " + juce::String(r.getHeight()); };
                check(std::abs(window.getWidth() - wantW) <= 2 && std::abs(window.getHeight() - wantH) <= 2
                        && std::abs(plugin.getWidth() - wantW) <= 2 && std::abs(plugin.getHeight() - wantH) <= 2,
                      label + ": the window ends at the UI's size",
                      "window " + show(window) + ", plug-in " + show(plugin) + " (want " + juce::String(wantW) + " x " + juce::String(wantH)
                      + "; " + juce::String(host.frame.requests) + " resize requests)");
                host.close();
                pump(50);
            }
        // A host that never agrees: the plug-in asks a few times, then stops (no endless resizing)
        if (host.error.isEmpty())
        {
            setRemembered(1.25f);
            const juce::Rectangle<int> screen { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
            host.frame.alwaysWrong = true;
            host.open(hostScale, Steinberg::ViewRect { 0, 0, screen.getWidth(), screen.getHeight() - 72 });
            pump(3000);
            const int settledRequests = host.frame.requests;
            pump(2000);
            check(settledRequests <= 2 + 3 * 2 && host.frame.requests == settledRequests,
                  "10 Cubase-like host that never agrees: the plug-in stops asking",
                  juce::String(settledRequests) + " requests in 3 s, " + juce::String(host.frame.requests - settledRequests) + " more in the next 2 s");
            host.frame.alwaysWrong = false;
            host.close();
            pump(50);
        }
        if (host.error.isNotEmpty())
            check(false, "10 Cubase-like host", host.error);
    }
   #endif
}

/* Writes a session from whichever Tertiary is loaded (run against the 1.0.2 build to make the old-session file) */
void writeSession(const juce::File& file)
{
    auto p = create(48000, 512);
    setNorm(*p, "Depth Low Band", percentNorm(42));
    setNorm(*p, "Waveshape Mid Band", choiceNorm(2, 7));
    setNorm(*p, "Multiplier High Band", choiceNorm(4, 6));
    setNorm(*p, "Output Gain", gainNorm(-3));
    setNorm(*p, "Show FFT", 0.0f);
    juce::AudioBuffer<float> silence(2, 256);
    silence.clear();
    render(*p, silence);
    // Let the message loop run, as in a DAW: Tertiary 1.0.2 only copies changed values into its saved state on
    // a timer (fixed in 1.1), so saving straight away would store the old values
    juce::MessageManager::getInstance()->runDispatchLoopUntil(500);
    juce::MemoryBlock state;
    p->getStateInformation(state);
    file.replaceWithData(state.getData(), state.getSize());
    juce::String values;
    for (auto* prm : p->getParameters())
        if (prm->getName(256) != "Bypass" && prm->getName(256) != "Program")
            values << prm->getName(256) << "=" << juce::String(prm->getValue(), 9) << "\n";
    file.withFileExtension(".txt").replaceWithText(values);
    report("INFO", "wrote session", file.getFullPathName());
}

//==============================================================================
// [Claude 2026-10-30] M-01 to M-03: the live licence check. The only part of HostTest that talks to the real server,
// and only when asked: HostTest --live-license <key> <order email>. It drives the plugin's own code
// (WonderlandLicenseServer, LicenseCheck::activateThisComputer, LicenseCheck::runStartupCheck) with a test key:
// validate, the email with other capitals, activate, activate again, the weekly check, deactivate, reactivate.
// Every instance it activates is deactivated at the end, and the key is checked to have no activations left.
// Use a test key only (the 2026-10-29 test orders), never a customer's.
//==============================================================================
int liveLicenseCheck(const juce::String& key, const juce::String& email)
{
    WonderlandLicenseServer server;
    const juce::String platform = "HostTest";
    const juce::String instance = "TRT-LIVECHECK-" + juce::String(juce::Time::currentTimeMillis());
    juce::StringArray activated;   // to deactivate at the end
    auto check = [](bool ok, const juce::String& name, const juce::String& detail) { report(ok ? "PASS" : "FAIL", name, detail); };
    auto listed = [&](const juce::String& who) { return server.validate(key, email).activations.contains(who); };

    auto start = server.validate(key, email);
    check(start.code == "key_valid", "live: validate, the order email", start.code + ", " + juce::String(start.activations.size()) + " activations");

    // M-01: the server compares the email exactly. The plugin sends it as typed.
    auto mixed = email.substring(0, 1).toUpperCase() + email.substring(1);
    if (mixed == email)
        mixed = email.toLowerCase();
    auto wrongCase = server.validate(key, mixed);
    check(wrongCase.code == "invalid_email", "live: validate, other capitals (" + mixed + ")", wrongCase.code);
    check(LicenseCheck::emailFromField("  " + email + " ") == email, "live: the field's email is sent as typed", LicenseCheck::emailFromField(email));

    // M-03: first activation
    auto first = LicenseCheck::activateThisComputer(server, key, email, instance, platform);
    if (first.activated) activated.addIfNotAlreadyThere(first.instance);
    check(first.activated && first.code == "key_activated" && first.instance == instance, "live: activate", first.code + ": " + first.status);
    check(listed(instance), "live: validate right after lists it (M-02, no cached reply)", instance);

    // M-03: activate again while active: confirmed by validate, not claimed
    auto again = LicenseCheck::activateThisComputer(server, key, email, instance, platform);
    check(again.activated && again.code == "instance_already_activated" && again.instance == instance
          && again.status == "Already activated on this computer.", "live: activate again while active", again.code + ": " + again.status);

    // M-01: the weekly check with the email saved as typed, and with other capitals saved (the retry)
    {
        TempSettings s;
        LicenseCheck::saveActivation(s.get(), key, email, juce::Time::currentTimeMillis() - 8LL * 24 * 60 * 60 * 1000, instance);
        auto r = LicenseCheck::runStartupCheck(server, s.get(), juce::Time::currentTimeMillis(), "1.1.0");
        check(! r.revoked && s.get().getValue("licenseKey") == key, "live: weekly check, email as saved", r.revoked ? r.message : "still activated");
    }
    if (email == email.toLowerCase())
    {
        TempSettings s;
        LicenseCheck::saveActivation(s.get(), key, mixed, juce::Time::currentTimeMillis() - 8LL * 24 * 60 * 60 * 1000, instance);
        auto r = LicenseCheck::runStartupCheck(server, s.get(), juce::Time::currentTimeMillis(), "1.1.0");
        check(! r.revoked && s.get().getValue("licenseEmail") == email, "live: weekly check, saved with other capitals",
              r.revoked ? r.message : "kept, saved as " + s.get().getValue("licenseEmail"));
    }

    // Deactivate, then reactivate the same computer
    auto off = server.deactivate(key, email, instance, platform);
    if (off.code == "key_deactivated") activated.removeString(instance);
    check(off.code == "key_deactivated" && ! off.activations.contains(instance), "live: deactivate", off.code);
    check(! listed(instance), "live: validate right after: not listed (M-02)", instance);

    auto raw = server.activate(key, email, instance, platform);
    report("INFO", "live: the server on reactivating the same instance", raw.code + " (" + juce::String(raw.activations.size()) + " listed)");
    if (raw.code == "key_activated")
    {
        activated.addIfNotAlreadyThere(instance);
        server.deactivate(key, email, instance, platform);
        activated.removeString(instance);
    }

    auto back = LicenseCheck::activateThisComputer(server, key, email, instance, platform);
    if (back.activated) activated.addIfNotAlreadyThere(back.instance);
    const bool reallyActive = back.activated && listed(back.instance);
    check(reallyActive && ! back.status.contains("Reactivation"), "live: reactivate (M-03): really active, the truth shown",
          back.code + ", instance " + back.instance + ": " + back.status);

    // Leave the key as it was: nothing activated
    for (auto& who : juce::StringArray(activated))
    {
        auto r = server.deactivate(key, email, who, platform);
        if (r.code == "key_deactivated") activated.removeString(who);
        report("INFO", "live: clean up, deactivate " + who, r.code);
    }
    auto end = server.validate(key, email);
    juce::StringArray ours;
    for (auto& who : end.activations)
        if (who.startsWith("TRT-LIVECHECK-"))
            ours.add(who);
    check(end.code == "key_valid" && ours.isEmpty(), "live: at the end, none of ours activated",
          juce::String(end.activations.size()) + " activations listed" + (end.activations.isEmpty() ? "" : ": " + end.activations.joinIntoString(", ")));

    std::cout << std::endl << passes << " passed, " << failures << " failed" << std::endl;
    return failures == 0 ? 0 : 1;
}

//==============================================================================
// 11. Two instances are independent  [Claude 2026-10-30]
//==============================================================================
/* Joe, RC1b in Cubase: "It looks like automation is happening on my 2nd instance of Tertiary ... all of the
   automation is assigned to the test track." Two copies in one host, both windows open, as on two tracks: A is
   automated, changed by program changes (presets), given a session, and run past the demo's 50 s; B must not move
   at any time. A listener on B catches a change even if it is put back later. */
struct ChangeSpy : juce::AudioProcessorParameter::Listener
{
    juce::StringArray changed;
    juce::CriticalSection lock;
    void parameterValueChanged(int index, float value) override
    {
        const juce::ScopedLock sl(lock);
        changed.addIfNotAlreadyThere(juce::String(index) + "=" + juce::String(value, 4));
    }
    void parameterGestureChanged(int, bool) override {}
    int count() { const juce::ScopedLock sl(lock); return changed.size(); }
    juce::String first() { const juce::ScopedLock sl(lock); return changed.isEmpty() ? juce::String() : changed[0]; }
};

std::vector<float> values(juce::AudioPluginInstance& p)
{
    std::vector<float> v;
    for (auto* prm : p.getParameters()) v.push_back(prm->getValue());
    return v;
}

juce::String differences(juce::AudioPluginInstance& p, const std::vector<float>& before)
{
    juce::StringArray out;
    const auto now = values(p);
    for (size_t i = 0; i < now.size() && i < before.size(); ++i)
        if (std::abs(now[i] - before[i]) > 1.0e-6f)
            out.add(p.getParameters()[(int) i]->getName(64) + " " + juce::String(before[i], 4) + " -> " + juce::String(now[i], 4));
    return out.isEmpty() ? juce::String("none") : out.joinIntoString(", ");
}

void section11()
{
    const double rate = 48000;
    const int block = 512;
    auto a = create(rate, block);
    auto b = create(rate, block);
    if (a == nullptr || b == nullptr) return;

    // B: something other than the defaults, set by hand (as Joe did), then left alone
    setNorm(*b, "Depth Low Band", 0.25f);
    setNorm(*b, "Rate Low Band", rateNorm(3.0f));
    setNorm(*b, "Sync to Host Low Band", 0.0f);
    setNorm(*b, "Low-Mid Crossover Freq", 0.5f);

    std::unique_ptr<juce::AudioProcessorEditor> editorA(a->createEditorIfNeeded()), editorB(b->createEditorIfNeeded());
    for (auto* e : { editorA.get(), editorB.get() })
        if (e != nullptr) { e->addToDesktop(0); e->setTopLeftPosition(40, 40); e->setVisible(true); }
    check(editorA != nullptr && editorB != nullptr, "11 both windows open");

    juce::AudioBuffer<float> io(2, block);
    juce::MidiBuffer midi;
    juce::Random rnd(11);
    auto runBlocks = [&](int n, std::function<void(int)> beforeA) {
        for (int k = 0; k < n; ++k)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < block; ++i) io.setSample(c, i, 0.1f * (rnd.nextFloat() * 2.f - 1.f));
            if (beforeA) beforeA(k);
            a->processBlock(io, midi);
            b->processBlock(io, midi);
            if (k % 8 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil(1);
        }
    };
    runBlocks(32, {});
    pump(300);

    const auto start = values(*b);
    ChangeSpy spy;
    for (auto* prm : b->getParameters()) prm->addListener(&spy);

    // 1. A automated: every automatable parameter, a new value each block, for 12 s of audio
    auto& paramsA = a->getParameters();
    runBlocks((int) (12 * rate / block), [&](int) {
        for (int j = 0; j < 4; ++j)
            if (auto* prm = paramsA[rnd.nextInt(paramsA.size())]; prm->isAutomatable())
                prm->setValueNotifyingHost(rnd.nextFloat());
    });
    if (std::getenv("WLA_HOSTTEST_11_SELFTEST") != nullptr)   // proves the check can fail: B is moved on purpose
        setNorm(*b, "Depth Low Band", 0.9f);
    pump(500);
    check(differences(*b, start) == "none" && spy.count() == 0, "11 automating A leaves B alone",
          "B changed: " + differences(*b, start) + (spy.count() ? ", listener saw " + spy.first() : juce::String()));

    // 2. A's program list (presets), each one, as a host's program change does
    for (int i = 0; i < a->getNumPrograms(); ++i)
    {
        a->setCurrentProgram(i);
        runBlocks(8, {});
    }
    pump(500);
    check(differences(*b, start) == "none" && spy.count() == 0, "11 loading every preset on A leaves B alone",
          juce::String(a->getNumPrograms()) + " presets; B changed: " + differences(*b, start));

    // 3. A given a session (as a project load or undo does)
    {
        auto c = create(rate, block);
        for (auto* prm : c->getParameters()) if (prm->isAutomatable()) prm->setValueNotifyingHost(rnd.nextFloat());
        juce::MemoryBlock state;
        c->getStateInformation(state);
        a->setStateInformation(state.getData(), (int) state.getSize());
        runBlocks(16, {});
        pump(500);
    }
    check(differences(*b, start) == "none" && spy.count() == 0, "11 a session loaded into A leaves B alone",
          "B changed: " + differences(*b, start));

    // 4. Over time: 70 s of audio on both (past the demo's 50 s on and into its gap), windows open throughout
    runBlocks((int) (70 * rate / block), [&](int k) {
        if (k % 64 == 0)
            if (auto* prm = paramsA[rnd.nextInt(paramsA.size())]; prm->isAutomatable())
                prm->setValueNotifyingHost(rnd.nextFloat());
    });
    pump(1500);
    check(differences(*b, start) == "none" && spy.count() == 0, "11 over 70 s (demo on and off) B stays as set",
          "B changed: " + differences(*b, start) + (spy.count() ? ", listener saw " + spy.first() : juce::String()));

    // 5. And the other way: B changed by hand still sticks while A is automated
    setNorm(*b, "Depth Low Band", 0.6f);
    runBlocks(64, {});
    const auto afterHand = values(*b);
    runBlocks((int) (4 * rate / block), [&](int) {
        if (auto* prm = paramsA[rnd.nextInt(paramsA.size())]; prm->isAutomatable()) prm->setValueNotifyingHost(rnd.nextFloat());
    });
    pump(500);
    check(differences(*b, afterHand) == "none", "11 a change made by hand on B sticks", "B changed: " + differences(*b, afterHand));

    for (auto* prm : b->getParameters()) prm->removeListener(&spy);
    editorA.reset();
    editorB.reset();
}

} // namespace

//==============================================================================
int main(int argc, char* argv[])
{
   #if JUCE_WINDOWS
    // [Claude 2026-10-30] per-monitor DPI aware, as Cubase, REAPER and JUCE's own hosts are (section 10 measures windows)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
   #endif
    juce::ScopedJuceInitialiser_GUI init;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add(argv[i]);

    // The golden renders' input as a WAV, for the REAPER test (tests/reaper)
    if (args.contains("--write-input"))
    {
        const juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile(args[args.indexOf("--write-input") + 1]));
        file.deleteFile();
        auto input = testSignal(48000, 2);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(file), 48000, 2, 32, {}, 0));
        writer->writeFromAudioSampleBuffer(input, 0, input.getNumSamples());
        std::cout << "wrote " << file.getFullPathName() << std::endl;
        return 0;
    }

    // [Claude 2026-10-30] The live licence check (the real server, a test key): see liveLicenseCheck
    if (args.contains("--live-license"))
    {
        const int i = args.indexOf("--live-license");
        if (i + 2 >= args.size())
        {
            std::cout << "usage: HostTest --live-license <test key> <its order email>" << std::endl;
            return 2;
        }
        return liveLicenseCheck(args[i + 1], args[i + 2]);
    }

    if (! args.contains("--vst3") && ! args.contains("--au"))
    {
        std::cout << "usage: HostTest --vst3 <Tertiary.vst3> | --au  [--golden <folder>] [--make-golden] [--expect-demo] [--only n]" << std::endl;
        return 2;
    }

    const auto start = juce::Time::getMillisecondCounterHiRes();

    if (! findPlugin(args))
    {
        report("FAIL", "find the plugin", args.contains("--au") ? "no AU aufx/EXPL/WLAD installed" : "can't load " + args[args.indexOf("--vst3") + 1]);
        return 1;
    }
    report("PASS", "load " + description.pluginFormatName + " " + description.name + " " + description.version,
           description.fileOrIdentifier);

    auto option = [&](const juce::String& name, const juce::String& fallback) {
        const int i = args.indexOf(name);
        return i >= 0 && i + 1 < args.size() ? args[i + 1] : fallback;
    };
    const int only = option("--only", "0").getIntValue();
    const juce::File golden = juce::File::getCurrentWorkingDirectory().getChildFile(option("--golden", "plugin/tests/golden"));
    const bool makeGolden = args.contains("--make-golden");

    // [Claude 2026-10-30] the plug-in's description as XML (to open it in JUCE's AudioPluginHost from a .filtergraph)
    if (args.contains("--describe"))
    {
        std::cout << description.createXml()->toString().toStdString() << std::endl;
        return 0;
    }

    if (args.contains("--write-session"))
    {
        writeSession(juce::File::getCurrentWorkingDirectory().getChildFile(option("--write-session", "session.bin")));
        return 0;
    }

    auto run = [&](int n, std::function<void()> section) {
        if (only != 0 && only != n) return;
        const auto t = juce::Time::getMillisecondCounterHiRes();
        section();
        std::cout << "      section " << n << " took " << juce::String((juce::Time::getMillisecondCounterHiRes() - t) / 1000.0, 1) << " s" << std::endl;
    };

    if (! makeGolden)
    {
        run(1, section1);
        run(2, section2);
        run(3, section3);
        run(4, section4);
        run(5, section5);
        run(6, [&] { section6(args.contains("--expect-demo")); });
        run(7, section7);
        run(9, [&] { section9(golden); });
        run(10, section10);   // [Claude 2026-10-30]
        run(11, section11);   // [Claude 2026-10-30] two instances are independent
    }
    run(8, [&] { section8(golden, makeGolden); });

    std::cout << "HostTest " << description.pluginFormatName << ": " << passes << " passed, " << failures << " failed, "
              << skips << " skipped, " << juce::String((juce::Time::getMillisecondCounterHiRes() - start) / 1000.0, 1) << " s" << std::endl;
    return failures == 0 ? 0 : 1;
}
