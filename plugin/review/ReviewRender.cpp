// ReviewRender: runs this plugin's own processor and editor offline, for the review package (Joe, 2026-10-26: "produce
// a little promo and demo video with audio samples and demonstration. That's how I'll review it from now on.").
//
//   ReviewRender --in in.wav [--sc sidechain.wav] --out out.wav [--start s] [--seconds s] [--bpm 100]
//                [--set id=value ...] [--ramp id:t0:t1:v0:v1 ...] [--frames dir --fps 30 --scale 1]
//
// Values are in the parameter's own units (dB, ms, x) or a choice's text. Ramps are linear, applied every 32 samples.
// Frames are the editor drawn at that moment of the render (every component's timer is run by hand), for the video.
// From Brownout's copy; Tertiary's own additions (for its Instagram promo, Joe 2026-10-26): it runs as the full
// version (no demo bypass), and a host clock at --bpm plays from the start, so tempo-synced LFOs lock to the music.
// Built only with -DWLA_REVIEW_RENDER=ON, so release builds are untouched.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../source/PluginProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
struct Ramp { juce::String id; double t0, t1; float v0, v1; };

/** A host that is playing from the top at a fixed tempo in 4/4. */
struct Clock : juce::AudioPlayHead
{
    double bpm = 120, rate = 48000;
    juce::int64 sample = 0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setTimeSignature (TimeSignature { 4, 4 });
        p.setIsPlaying (true);
        p.setTimeInSamples (sample);
        p.setTimeInSeconds (sample / rate);
        p.setPpqPosition (sample / rate * bpm / 60.0);
        p.setPpqPositionOfLastBarStart (std::floor (sample / rate * bpm / 60.0 / 4.0) * 4.0);
        return p;
    }
};

/** Runs the timer of every component that has one, so meters, the scope and the FFT draw as they would live. */
void tick (juce::Component& c)
{
    if (auto* t = dynamic_cast<juce::Timer*> (&c)) t->timerCallback();
    for (auto* child : c.getChildren()) tick (*child);
}

juce::RangedAudioParameter* findParam (juce::AudioProcessor& p, const juce::String& id)
{
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID() == id) return r;
    return nullptr;
}

void setParam (juce::AudioProcessor& p, const juce::String& id, const juce::String& text)
{
    auto* prm = findParam (p, id);
    if (prm == nullptr) { std::cerr << "no parameter " << id << "\n"; std::exit (2); }
    float norm;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (prm))
    {
        const int i = c->choices.indexOf (text);
        if (i < 0) { std::cerr << "no choice " << text << " for " << id << "\n"; std::exit (2); }
        norm = c->convertTo0to1 ((float) i);
    }
    else norm = prm->convertTo0to1 (text.getFloatValue());
    prm->setValueNotifyingHost (norm);
}

std::unique_ptr<juce::AudioBuffer<float>> readWav (const juce::File& f, double& rate, double start, double seconds)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) { std::cerr << "cannot read " << f.getFullPathName() << "\n"; std::exit (2); }
    rate = r->sampleRate;
    const auto from = (juce::int64) (start * rate);
    auto len = r->lengthInSamples - from;
    if (seconds > 0) len = juce::jmin (len, (juce::int64) (seconds * rate));
    auto b = std::make_unique<juce::AudioBuffer<float>> ((int) juce::jmax (1u, r->numChannels), (int) len);
    r->read (b.get(), 0, (int) len, from, true, true);
    return b;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);
    if (args.contains ("--list"))
    {
        // Every parameter: its id, its value now as text, and a choice's options.
        std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
        for (auto* prm : p->getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            {
                std::cout << r->getParameterID() << " = " << r->getCurrentValueAsText();
                if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (r)) std::cout << "  [" << c->choices.joinIntoString (" | ") << "]";
                std::cout << "\n";
            }
        return 0;
    }
    auto opt = [&] (const juce::String& name, const juce::String& def = {}) {
        const int i = args.indexOf (name);
        return i >= 0 && i + 1 < args.size() ? args[i + 1] : def;
    };
    const juce::File in (opt ("--in")), out (opt ("--out"));
    const double start = opt ("--start", "0").getDoubleValue(), seconds = opt ("--seconds", "0").getDoubleValue();
    double rate = 48000, scRate = 48000;
    auto input = readWav (in, rate, start, seconds);
    std::unique_ptr<juce::AudioBuffer<float>> side;
    if (opt ("--sc").isNotEmpty()) side = readWav (juce::File (opt ("--sc")), scRate, start, seconds);

    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    if (side && proc->getBusCount (true) > 1) proc->getBus (true, 1)->enable (true);
    const int block = 32;
    Clock clock;
    clock.bpm = opt ("--bpm", "120").getDoubleValue();
    clock.rate = rate;
    proc->setPlayHead (&clock);
    proc->setRateAndBufferSizeDetails (rate, block);
    proc->prepareToPlay (rate, block);
    proc->setNonRealtime (true);

    std::vector<Ramp> ramps;
    for (int i = 0; i < args.size(); ++i)
    {
        if (args[i] == "--set" && i + 1 < args.size())
        {
            const auto kv = args[i + 1];
            setParam (*proc, kv.upToFirstOccurrenceOf ("=", false, false), kv.fromFirstOccurrenceOf ("=", false, false));
        }
        if (args[i] == "--ramp" && i + 1 < args.size())
        {
            juce::StringArray t;
            t.addTokens (args[i + 1], ":", "");
            ramps.push_back ({ t[0], t[1].getDoubleValue(), t[2].getDoubleValue(), t[3].getFloatValue(), t[4].getFloatValue() });
        }
    }
    // Settings take effect before the first sample: prepare again so smoothed values start where they are set.
    proc->prepareToPlay (rate, block);
    // The full version: the demo bypasses the effect for a few seconds each minute.
    if (auto* t = dynamic_cast<audio_plugin::AudioPluginAudioProcessor*> (proc.get())) t->setDemoState (false);

    const juce::File frames (opt ("--frames"));
    const double fps = opt ("--fps", "30").getDoubleValue();
    const float scale = opt ("--scale", "1").getFloatValue();
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    if (frames.getFullPathName().isNotEmpty())
    {
        frames.createDirectory();
        editor.reset (proc->createEditorIfNeeded());
        editor->setVisible (true);
    }

    const int total = input->getNumSamples();
    const int outCh = proc->getTotalNumOutputChannels();
    const int busCh = juce::jmax (proc->getTotalNumInputChannels(), outCh);
    juce::AudioBuffer<float> io (busCh, block), result (juce::jmax (2, outCh), total);
    juce::MidiBuffer midi;
    const int mainIn = proc->getBus (true, 0)->getNumberOfChannels();
    int frameIndex = 0;
    double nextFrame = 0;
    float peak = 0;
    double sumSq = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        const double t = pos / rate;
        for (auto& r : ramps)
            if (t >= r.t0 && t <= r.t1 + block / rate)
            {
                const double a = juce::jlimit (0.0, 1.0, (t - r.t0) / juce::jmax (1e-9, r.t1 - r.t0));
                auto* prm = findParam (*proc, r.id);
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (r.v0 + (r.v1 - r.v0) * a)));
            }
        io.clear();
        for (int c = 0; c < mainIn; ++c) io.copyFrom (c, 0, *input, juce::jmin (c, input->getNumChannels() - 1), pos, n);
        if (side)
            for (int c = 0; c < proc->getBus (true, 1)->getNumberOfChannels(); ++c)
                if (pos < side->getNumSamples())
                    io.copyFrom (mainIn + c, 0, *side, juce::jmin (c, side->getNumChannels() - 1), pos, juce::jmin (n, side->getNumSamples() - pos));
        juce::AudioBuffer<float> view (io.getArrayOfWritePointers(), busCh, n);
        clock.sample = pos;
        proc->processBlock (view, midi);
        for (int c = 0; c < result.getNumChannels(); ++c) result.copyFrom (c, pos, io, juce::jmin (c, outCh - 1), 0, n);
        for (int c = 0; c < juce::jmin (2, outCh); ++c)
            for (int i = 0; i < n; ++i)
            {
                const float v = io.getSample (c, i);
                peak = juce::jmax (peak, std::abs (v));
                sumSq += v * v;
            }

        if (editor && t + n / rate >= nextFrame)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (1); // parameter attachments, async repaints
            tick (*editor);
            auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
            juce::PNGImageFormat png;
            auto f = frames.getChildFile (juce::String::formatted ("%05d.png", frameIndex++));
            f.deleteFile();
            juce::FileOutputStream os (f);
            png.writeImageToStream (img, os);
            nextFrame += 1.0 / fps;
        }
    }

    out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (out), rate, (unsigned) result.getNumChannels(), 24, {}, 0));
    if (w == nullptr) { std::cerr << "cannot write " << out.getFullPathName() << "\n"; return 2; }
    w->writeFromAudioSampleBuffer (result, 0, result.getNumSamples());
    w.reset();
    const double rms = std::sqrt (sumSq / juce::jmax (1.0, (double) total * juce::jmin (2, outCh)));
    std::cout << "wrote " << out.getFullPathName() << "  peak " << juce::Decibels::gainToDecibels (peak) << " dBFS  rms "
              << juce::Decibels::gainToDecibels ((float) rms) << " dBFS  latency " << proc->getLatencySamples() << "  frames " << frameIndex << "\n";
    editor.reset();
    proc.reset();
    return 0;
}
