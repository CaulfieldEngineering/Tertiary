/*
  ==============================================================================

    TertiaryTests.cpp
    Created: 27 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-27] New file. Unit tests for the changes made in 1.1, using JUCE's
    own UnitTest class (no extra library). Built only with -DWLA_TESTS=ON:

        cmake -B build-tests -DWLA_TESTS=ON
        cmake --build build-tests --config Release --target TertiaryTests
        TertiaryTests          (prints each test; exit code 0 when all pass)

    Nothing here touches the network: the license tests use PretendServer below,
    and the processor tests never open the editor (which is what starts the real check).

    What is tested
      License      the weekly re-check decisions, the grace period, the update check,
                   reading a 1.0.x settings file, the machine ID, version numbers
      Processor    LFO speed after a sample-rate change, synced LFOs after a tempo change,
                   no memory allocation in processBlock, same output at any block size,
                   state save/restore, the demo's on/off timing and activating during the gap

  ==============================================================================
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "LicenseCheck.h"
#include <thread>   // [Claude 2026-10-29] the V-05 test plays on a second thread

//==============================================================================
// Allocation counter: every new/delete in this program goes through here. While
// countAllocations is true on a thread, allocations on that thread are counted.
//==============================================================================
namespace
{
    thread_local bool countAllocations = false;
    std::atomic<int> allocationCount { 0 };
}

void* operator new (std::size_t size)
{
    if (countAllocations)
        ++allocationCount;

    if (auto* p = std::malloc (size == 0 ? 1 : size))
        return p;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)                         { return operator new (size); }
void operator delete (void* p) noexcept                         { std::free (p); }
void operator delete[] (void* p) noexcept                       { std::free (p); }
void operator delete (void* p, std::size_t) noexcept            { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept          { std::free (p); }


#include "PretendServer.h"   // the pretend license server and TempSettings, shared with HostTest.cpp


const juce::int64 day = 24LL * 60 * 60 * 1000;
const juce::int64 now = 1800000000000LL;   // A fixed "now" (January 2027), so the tests don't depend on the clock


//==============================================================================
struct LicenseTests : juce::UnitTest
{
    LicenseTests() : juce::UnitTest ("License re-check", "Tertiary") {}

    void runTest() override
    {
        beginTest ("Demo copy: no server calls at all");
        {
            TempSettings s;
            PretendServer server;
            auto r = LicenseCheck::runStartupCheck (server, s.get(), now, "1.1.0");
            expect (r.finished && ! r.revoked && ! r.offlineTooLong);
            expectEquals (server.validateCalls + server.versionCalls, 0);
        }

        beginTest ("Checked less than a week ago: no validate call");
        {
            TempSettings s;
            s.activated (now - 2 * day, now - 2 * day);
            PretendServer server;
            server.versionReply = PretendServer::reply ("version_checked", {}, "1.1.0");
            auto r = LicenseCheck::runStartupCheck (server, s.get(), now, "1.1.0");
            expectEquals (server.validateCalls, 0);
            expect (! r.revoked && r.message.isEmpty());
        }

        beginTest ("A week later, key still good: dates move on, still activated");
        {
            TempSettings s;
            s.activated (now - 8 * day, now - 8 * day);
            PretendServer server;
            server.validateReply = PretendServer::reply ("key_valid");
            auto r = LicenseCheck::runStartupCheck (server, s.get(), now, "1.1.0");
            expectEquals (server.validateCalls, 1);
            expect (! r.revoked);
            expectEquals (s.get().getValue ("licenseKey"), juce::String ("ABCD-1234"));
            expectEquals (s.get().getValue ("licenseLastValid").getLargeIntValue(), now);
            expectEquals (server.lastEmail, juce::String ("joe@example.com"));   // sent in lower case
        }

        beginTest ("Refunded order: back to demo, with the server's reason");
        {
            TempSettings s;
            s.activated (now - 8 * day, now - 8 * day);
            PretendServer server;
            server.validateReply = PretendServer::reply ("invalid_order", "Please complete your order to activate the serial key.");
            auto r = LicenseCheck::runStartupCheck (server, s.get(), now, "1.1.0");
            expect (r.revoked);
            expect (r.message.contains ("Please complete your order"));
            expect (s.get().getValue ("licenseKey").isEmpty());
            expectEquals (s.get().getValue ("licenseEmail"), juce::String ("Joe@Example.com"));   // kept for re-entry

            s.reload();   // and it is saved to disk
            expect (s.get().getValue ("licenseKey").isEmpty());
        }

        beginTest ("Every revoking code revokes; other codes never do");
        {
            for (auto code : { "invalid_key", "invalid_order", "invalid_product_key", "invalid_email",
                               "expired_key", "key_cancelled", "invalid_key_status" })
                expect (LicenseCheck::isRevokingCode (code), code);

            for (auto code : { "key_valid", "rest_no_route", "invalid_product_id", "missing_key", "", "internal_server_error" })
                expect (! LicenseCheck::isRevokingCode (code), code);
        }

        beginTest ("Server unreachable within 30 days: nothing changes, nothing said");
        {
            TempSettings s;
            s.activated (now - 20 * day, now - 20 * day);
            PretendServer server;   // replies are unreachable by default
            auto r = LicenseCheck::runStartupCheck (server, s.get(), now, "1.1.0");
            expect (! r.revoked && ! r.offlineTooLong && r.message.isEmpty());
            expectEquals (s.get().getValue ("licenseKey"), juce::String ("ABCD-1234"));
            expectEquals (s.get().getValue ("licenseLastCheck").getLargeIntValue(), now - 20 * day);   // retried next time
        }

        beginTest ("Server unreachable for over 30 days: a quiet note, still activated");
        {
            TempSettings s;
            s.activated (now - 31 * day, now - 31 * day);
            PretendServer server;
            auto r = LicenseCheck::runStartupCheck (server, s.get(), now, "1.1.0");
            expect (! r.revoked && r.offlineTooLong && r.message.isNotEmpty());
            expectEquals (s.get().getValue ("licenseKey"), juce::String ("ABCD-1234"));
        }

        beginTest ("An answer we don't understand (e.g. the route missing) is treated as unreachable");
        {
            TempSettings s;
            s.activated (now - 8 * day, now - 8 * day);
            PretendServer server;
            server.validateReply = PretendServer::reply ("rest_no_route", "No route was found");
            auto r = LicenseCheck::runStartupCheck (server, s.get(), now, "1.1.0");
            expect (! r.revoked);
            expectEquals (s.get().getValue ("licenseKey"), juce::String ("ABCD-1234"));
        }

        beginTest ("Update check: newer version shown, at most once a day");
        {
            TempSettings s;
            s.activated (now, now);
            PretendServer server;
            server.versionReply = PretendServer::reply ("version_checked", {}, "1.1.1");
            auto r = LicenseCheck::runStartupCheck (server, s.get(), now, "1.1.0");
            expectEquals (r.newVersion, juce::String ("1.1.1"));
            expectEquals (server.versionCalls, 1);

            r = LicenseCheck::runStartupCheck (server, s.get(), now + day / 2, "1.1.0");
            expectEquals (server.versionCalls, 1);                      // not again the same day
            expectEquals (r.newVersion, juce::String ("1.1.1"));        // but still shown

            r = LicenseCheck::runStartupCheck (server, s.get(), now + day, "1.1.1");
            expect (r.newVersion.isEmpty());                            // up to date: nothing shown
        }

        beginTest ("Version numbers");
        {
            expect (LicenseCheck::isNewerVersion ("1.1.1", "1.1.0"));
            expect (LicenseCheck::isNewerVersion ("1.10.0", "1.9.9"));
            expect (LicenseCheck::isNewerVersion ("2.0", "1.9.9"));
            expect (! LicenseCheck::isNewerVersion ("1.1.0", "1.1.0"));
            expect (! LicenseCheck::isNewerVersion ("1.0.2", "1.1.0"));
            expect (! LicenseCheck::isNewerVersion ("", "1.1.0"));
            expect (! LicenseCheck::isNewerVersion ("beta", "1.1.0"));
        }

        beginTest ("A 1.0.x settings file (key and email only) is read without trouble");
        {
            TempSettings s;
            s.props.reset();
            s.file.replaceWithText ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<PROPERTIES>\n"
                                    "  <VALUE name=\"licenseKey\" val=\"OLD-KEY\"/>\n"
                                    "  <VALUE name=\"licenseEmail\" val=\"old@example.com\"/>\n</PROPERTIES>\n");
            s.reload();
            PretendServer server;
            auto r = LicenseCheck::runStartupCheck (server, s.get(), now, "1.1.0");
            expect (! r.revoked && ! r.offlineTooLong);
            expectEquals (s.get().getValue ("licenseLastValid").getLargeIntValue(), now);   // counted from now
            expectEquals (LicenseCheck::getActivatedInstance (s.get()), juce::SystemStats::getDeviceDescription());
        }

        beginTest ("A damaged settings file doesn't crash, and counts as a demo copy");
        {
            TempSettings s;
            s.props.reset();
            s.file.replaceWithText ("this is not xml <<<");
            s.reload();
            PretendServer server;
            auto r = LicenseCheck::runStartupCheck (server, s.get(), now, "1.1.0");
            expect (r.finished && ! r.revoked);
            expectEquals (server.validateCalls, 0);
        }

        beginTest ("Machine instance: stable, not empty, and not the raw machine ID");
        {
            TempSettings s;
            auto a = LicenseCheck::getMachineInstance (s.get());
            auto b = LicenseCheck::getMachineInstance (s.get());
            expect (a.startsWith ("TRT-") && a.length() == 20, a);
            expectEquals (a, b);
            auto raw = juce::SystemStats::getUniqueDeviceID();
            expect (raw.isEmpty() || ! a.containsIgnoreCase (raw));
            logMessage ("This machine's instance: " + a);
        }
    }
};


//==============================================================================
// Processor tests
//==============================================================================
struct TestPlayHead : juce::AudioPlayHead
{
    double bpm = 120;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setIsPlaying (false);
        return p;
    }
};

static void setParam (juce::AudioProcessor& p, const juce::String& id, float value)
{
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID() == id)
                r->setValueNotifyingHost (r->convertTo0to1 (value));
}

/* Runs a constant (DC) signal through the processor; returns the left output */
static std::vector<float> runDC (audio_plugin::AudioPluginAudioProcessor& p, int numSamples, int blockSize)
{
    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    for (int pos = 0; pos < numSamples; pos += blockSize)
    {
        const int n = juce::jmin (blockSize, numSamples - pos);
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, n);
        for (int c = 0; c < 2; ++c)
            juce::FloatVectorOperations::fill (view.getWritePointer (c), 0.5f, n);
        p.processBlock (view, midi);
        out.insert (out.end(), view.getReadPointer (0), view.getReadPointer (0) + n);
    }
    return out;
}

/* The average distance in samples between the low points of a modulated DC signal */
static double measurePeriod (const std::vector<float>& x, int skip)
{
    std::vector<int> minima;
    const int window = 200;
    for (int i = skip + window; i < (int) x.size() - window; ++i)
    {
        bool isMin = x[(size_t) i] < 0.1f;
        for (int k = -window; k <= window && isMin; ++k)
            if (x[(size_t) (i + k)] < x[(size_t) i]) isMin = false;
        if (isMin && (minima.empty() || i - minima.back() > 4 * window))
            minima.push_back (i);
    }
    if (minima.size() < 3)
        return 0;
    return (minima.back() - minima.front()) / double (minima.size() - 1);
}

/* The sample of the first low point after skip */
static int firstMinimum (const std::vector<float>& x, int skip)
{
    const int window = 200;
    for (int i = skip + window; i < (int) x.size() - window; ++i)
    {
        bool isMin = x[(size_t) i] < 0.1f;
        for (int k = -window; k <= window && isMin; ++k)
            if (x[(size_t) (i + k)] < x[(size_t) i]) isMin = false;
        if (isMin)
            return i;
    }
    return -1;
}

/* Low band only, slow sine at full depth, so the DC output follows the low LFO */
static void setUpForPeriod (audio_plugin::AudioPluginAudioProcessor& p, bool synced)
{
    p.setDemoState (false);
    setParam (p, "Waveshape Low Band", 4);   // Sine
    setParam (p, "Depth Low Band", 100);
    setParam (p, "Sync to Host Low Band", synced ? 1.f : 0.f);
    setParam (p, "Rate Low Band", 2);
    setParam (p, "Multiplier Low Band", 1);  // 1/4 note
}

struct ProcessorTests : juce::UnitTest
{
    ProcessorTests() : juce::UnitTest ("Processor", "Tertiary") {}

    void runTest() override
    {
        beginTest ("LFO speed is right after a sample-rate change");
        {
            audio_plugin::AudioPluginAudioProcessor p;
            setUpForPeriod (p, false);
            p.setRateAndBufferSizeDetails (44100, 512);
            p.prepareToPlay (44100, 512);
            runDC (p, 44100, 512);
            expectEquals ((int) p.lowLFO.getWaveTableSize(), 22050);

            p.setRateAndBufferSizeDetails (96000, 512);
            p.prepareToPlay (96000, 512);
            auto out = runDC (p, 96000 * 3, 512);
            expectEquals ((int) p.lowLFO.getWaveTableSize(), 48000);
            auto period = measurePeriod (out, 9600);
            logMessage ("2 Hz LFO at 96 kHz, period " + juce::String (period) + " samples (expect 48000)");
            expectWithinAbsoluteError (period, 48000.0, 480.0);
        }

        beginTest ("Relative phase in the audio matches the knob (90 degrees = a quarter cycle)");
        {
            // [Claude 2026-10-27] 1.0.2 shifted the audio by twice the knob's value: 90 gave half a cycle
            auto render = [] (float phase)
            {
                audio_plugin::AudioPluginAudioProcessor p;
                setUpForPeriod (p, false);                     // 2 Hz free-running sine on the low band
                setParam (p, "Relative Phase Low Band", phase);
                p.setRateAndBufferSizeDetails (48000, 512);
                p.prepareToPlay (48000, 512);
                return runDC (p, 48000 * 2, 512);
            };

            const int period = 24000;                         // 2 Hz at 48 kHz
            const int reference = firstMinimum (render (0), 4800);

            for (float phase : { 90.f, -90.f, 45.f, 180.f })
            {
                const int m = firstMinimum (render (phase), 4800);
                int shift = ((m - reference) % period + period) % period;
                shift = juce::jmin (shift, period - shift);    // the direction doesn't matter here
                const double expected = std::abs (phase) / 360.0 * period;
                logMessage ("phase " + juce::String (phase) + ": shift " + juce::String (shift) + " samples (expect " + juce::String (expected) + ")");
                expectWithinAbsoluteError ((double) shift, expected, period * 0.01);
            }
        }

        beginTest ("Synced LFO follows a host tempo change");
        {
            audio_plugin::AudioPluginAudioProcessor p;
            TestPlayHead head;
            p.setPlayHead (&head);
            setUpForPeriod (p, true);
            p.setRateAndBufferSizeDetails (48000, 512);
            p.prepareToPlay (48000, 512);
            auto out = runDC (p, 48000 * 3, 512);
            auto period120 = measurePeriod (out, 4800);

            head.bpm = 60;
            out = runDC (p, 48000 * 5, 512);
            auto period60 = measurePeriod (out, 4800);
            logMessage ("1/4 note at 120 bpm: " + juce::String (period120) + ", at 60 bpm: " + juce::String (period60) + " samples");
            expectWithinAbsoluteError (period120, 24000.0, 240.0);
            expectWithinAbsoluteError (period60, 48000.0, 480.0);
            p.setPlayHead (nullptr);
        }

        beginTest ("No allocation in processBlock (changing block sizes, parameters and tempo)");
        {
            audio_plugin::AudioPluginAudioProcessor p;
            TestPlayHead head;
            p.setPlayHead (&head);
            p.setDemoState (false);
            p.setRateAndBufferSizeDetails (48000, 1024);
            p.prepareToPlay (48000, 1024);
            juce::AudioBuffer<float> buffer (2, 1024);
            juce::MidiBuffer midi;
            juce::Random rnd (42);

            auto process = [&] (int n)
            {
                juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, n);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                        view.setSample (c, i, rnd.nextFloat() - 0.5f);
                countAllocations = true;
                p.processBlock (view, midi);
                countAllocations = false;
            };

            // JUCE's buffers and arrays allocate with malloc, which the counter can't see. So also check that the
            // LFO wavetables were allocated by prepareToPlay and don't move while processing (1.0.2 grew them in
            // the first processBlock). The filter buffers are private; their copy (makeCopyOf, no reallocation)
            // is checked by reading the code and by the varying-block renders being identical.
            auto* tableBefore = p.highLFO.getWaveTableScaled().getRawDataPointer();
            expect (tableBefore != nullptr, "wavetable allocated in prepareToPlay");

            allocationCount = 0;
            for (int n : { 1024, 1, 7, 64, 333, 512, 1024, 17, 256, 1000, 3 })
                process (n);
            setParam (p, "Depth Mid Band", 40);         // makes processBlock recalculate a wavetable
            setParam (p, "Waveshape High Band", 2);
            head.bpm = 97;
            for (int n : { 128, 1024, 5 })
                process (n);
            expectEquals (allocationCount.load(), 0);
            expect (p.highLFO.getWaveTableScaled().getRawDataPointer() == tableBefore, "wavetable did not move");
            p.setPlayHead (nullptr);
        }

        beginTest ("Same output at any block size");
        {
            auto render = [] (std::initializer_list<int> sizes)
            {
                audio_plugin::AudioPluginAudioProcessor p;
                p.setDemoState (false);
                setParam (p, "Waveshape Low Band", 2);
                setParam (p, "Relative Phase Mid Band", 90);
                setParam (p, "Sync to Host High Band", 0);
                p.setRateAndBufferSizeDetails (48000, 1024);
                p.prepareToPlay (48000, 1024);
                juce::AudioBuffer<float> buffer (2, 1024);
                juce::MidiBuffer midi;
                const int total = 48000 * 3;
                juce::AudioBuffer<float> input (2, total + 1024);   // the same noise whatever the block sizes
                juce::Random rnd (7);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < input.getNumSamples(); ++i)
                        input.setSample (c, i, rnd.nextFloat() - 0.5f);
                std::vector<float> out;
                std::vector<int> pattern (sizes);
                for (int pos = 0, k = 0; pos < total; ++k)
                {
                    const int n = pattern[(size_t) k % pattern.size()];
                    juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, n);
                    for (int c = 0; c < 2; ++c)
                        view.copyFrom (c, 0, input, c, pos, n);
                    p.processBlock (view, midi);
                    out.insert (out.end(), view.getReadPointer (0), view.getReadPointer (0) + n);
                    pos += n;
                }
                out.resize (48000 * 3);
                return out;
            };

            auto a = render ({ 512 });
            auto b = render ({ 1, 1000, 64, 333, 7, 1024 });
            float maxDiff = 0;
            for (size_t i = 0; i < a.size(); ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (a[i] - b[i]));
            logMessage ("max difference " + juce::String (maxDiff));
            expectLessThan (maxDiff, 1.0e-6f);
        }

        beginTest ("State save and restore");
        {
            audio_plugin::AudioPluginAudioProcessor a, b;
            setParam (a, "Low-Mid Crossover Freq", 333);
            setParam (a, "Waveshape Mid Band", 6);
            setParam (a, "Invert High Band", 1);
            setParam (a, "Relative Phase Low Band", -45);
            setParam (a, "Multiplier High Band", 5);
            setParam (a, "Output Gain", -6.5f);
            juce::MemoryBlock state;
            a.getStateInformation (state);
            b.setStateInformation (state.getData(), (int) state.getSize());

            auto pa = a.getParameters(), pb = b.getParameters();
            expectEquals (pa.size(), pb.size());
            for (int i = 0; i < pa.size(); ++i)
                expectWithinAbsoluteError (pb[i]->getValue(), pa[i]->getValue(), 1.0e-6f, pa[i]->getName (64));

            b.setStateInformation ("garbage", 7);   // a bad state is ignored, not a crash
        }

        // [Claude 2026-10-29] V-04 of the 2026-10-29 review: the band gains were never prepared, so they had no sample
        // rate and their 50 ms ramp did nothing: a change jumped straight to the new level (zipper noise, clicks).
        beginTest ("Band gain changes are smoothed (50 ms), not a jump");
        {
            audio_plugin::AudioPluginAudioProcessor p;
            p.setDemoState (false);
            for (auto band : { "Low", "Mid", "High" })
                setParam (p, juce::String ("Depth ") + band + " Band", 0);   // no tremolo: the DC goes straight through
            p.setRateAndBufferSizeDetails (48000, 64);
            p.prepareToPlay (48000, 64);
            auto settled = runDC (p, 48000, 64);
            const float before = settled.back();
            expectWithinAbsoluteError (before, 0.5f, 0.01f);   // the DC is in the low band only

            setParam (p, "Low Output Gain", -20);              // 0.5 -> 0.05
            auto after = runDC (p, 48000 / 2, 64);
            logMessage ("Low band gain 0 -> -20 dB: " + juce::String (before) + " before, " + juce::String (after[0])
                        + " on the next sample, " + juce::String (after[1200]) + " after 25 ms, " + juce::String (after.back()) + " after 0.5 s");
            expect (after[0] > 0.45f, "no jump on the first sample after the change");
            expect (after[1200] > 0.1f && after[1200] < 0.45f, "half way through the 50 ms ramp");
            expectWithinAbsoluteError (after.back(), 0.05f, 0.005f);
        }

        // [Claude 2026-10-29] V-05 of the 2026-10-29 review: an LFO setting that changed while processBlock was rebuilding the wave (after the
        // settings were read, before the "changed" flag was cleared) was lost: the band kept the old setting. A second
        // thread plays while this one changes the low band's depth twice in quick succession, as a preset change does.
        beginTest ("An LFO setting changed during the wave rebuild is not lost");
        {
            audio_plugin::AudioPluginAudioProcessor p;
            p.setDemoState (false);
            p.setRateAndBufferSizeDetails (192000, 32);        // the largest table, so the rebuild takes longest
            p.prepareToPlay (192000, 32);
            runDC (p, 64, 32);

            std::atomic<bool> stop { false };
            std::thread audio ([&]
            {
                juce::AudioBuffer<float> buffer (2, 32);
                juce::MidiBuffer midi;
                while (! stop)
                {
                    buffer.clear();
                    p.processBlock (buffer, midi);
                }
            });

            int lost = 0;
            const int trials = 100;
            for (int trial = 0; trial < trials; ++trial)
            {
                const float target = (trial % 2 == 0) ? 30.f : 70.f;
                setParam (p, "Depth Low Band", target + 10.f);   // starts a rebuild
                const auto start = juce::Time::getHighResolutionTicks();
                while (juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start) < 100.0e-6) {}
                setParam (p, "Depth Low Band", target);          // most likely lands during that rebuild
                juce::Thread::sleep (20);                        // many blocks later
                if (std::abs (p.lowLFO.getWaveDepth() - target) > 0.01f)
                    ++lost;
            }
            stop = true;
            audio.join();
            logMessage ("Depth changes lost: " + juce::String (lost) + " of " + juce::String (trials));
            expectEquals (lost, 0);
        }

        beginTest ("Demo timing: on 50 s, off 10 s; activating during the gap turns it back on");
        {
            audio_plugin::AudioPluginAudioProcessor p;
            p.setDemoState (true);
            p.setRateAndBufferSizeDetails (48000, 480);
            p.prepareToPlay (48000, 480);
            runDC (p, 48000 * 49, 480);
            expect (p.getIsDemoModeEnabled());
            runDC (p, 48000 * 2, 480);              // 51 s
            expect (! p.getIsDemoModeEnabled());
            runDC (p, 48000 * 10, 480);             // 61 s
            expect (p.getIsDemoModeEnabled());
            runDC (p, 48000 * 50, 480);             // 111 s, in the next gap
            expect (! p.getIsDemoModeEnabled());

            p.setDemoState (false);                 // the user activates now
            runDC (p, 480, 480);
            expect (p.getIsDemoModeEnabled());
        }
    }
};

//==============================================================================
// [Claude 2026-10-27] Presets (PresetManager, FactoryPresets.cpp). User presets go to a temp folder.
//==============================================================================
struct PresetTests : juce::UnitTest
{
    PresetTests() : juce::UnitTest ("Presets", "Tertiary") {}

    static float valueOf (juce::AudioProcessor& p, const juce::String& id)
    {
        for (auto* prm : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (r->getParameterID() == id) return r->convertFrom0to1 (r->getValue());
        return -1.0e9f;
    }

    void runTest() override
    {
        const auto& factory = PresetManager::getFactoryPresets();

        beginTest ("Factory presets: 10 or more, unique names, every parameter name real, first is Default");
        {
            audio_plugin::AudioPluginAudioProcessor p;
            expect (factory.size() >= 10);
            expectEquals (factory[0].name, juce::String ("Default"));
            juce::StringArray names;
            for (auto& preset : factory)
            {
                expect (! names.contains (preset.name), "duplicate " + preset.name);
                names.add (preset.name);
                for (auto& [id, value] : preset.values)
                {
                    auto* prm = p.apvts.getParameter (id);
                    expect (prm != nullptr, preset.name + ": no parameter " + id);
                    if (prm != nullptr)
                        expect (prm->getNormalisableRange().getRange().contains (value) || value == prm->getNormalisableRange().end,
                                preset.name + ": " + id + " out of range");
                }
            }
            logMessage (names.joinIntoString (", "));
        }

        beginTest ("Default = the plugin defaults; a new instance is on Default, not modified");
        {
            audio_plugin::AudioPluginAudioProcessor p;
            expectEquals (p.presets.getCurrentName(), juce::String ("Default"));
            expect (! p.presets.isModified());
            p.presets.loadPreset (5);
            p.presets.loadPreset (0);
            for (auto* prm : p.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                    if (PresetManager::isSoundParameter (r->getParameterID()))
                        expectWithinAbsoluteError (r->getValue(), r->getDefaultValue(), 1.0e-6f, r->getParameterID());
        }

        beginTest ("Loading every factory preset; the modified marker");
        {
            audio_plugin::AudioPluginAudioProcessor p;
            for (int i = 0; i < (int) factory.size(); ++i)
            {
                expect (p.presets.loadPreset (i));
                expectEquals (p.presets.getCurrentIndex(), i);
                expect (! p.presets.isModified(), factory[(size_t) i].name + " modified right after loading");
            }
            p.presets.loadPreset (6);   // Hard Chop
            expectEquals (valueOf (p, "Waveshape Low Band"), 2.0f);
            p.apvts.getParameter ("Depth Mid Band")->setValueNotifyingHost (0.1f);
            expect (p.presets.isModified());
            p.apvts.getParameter ("Show FFT")->setValueNotifyingHost (0.0f);   // view settings are not part of a preset
            p.presets.loadPreset (6);
            p.apvts.getParameter ("Show FFT")->setValueNotifyingHost (1.0f);
            expect (! p.presets.isModified());
        }

        beginTest ("Opening the plugin window changes no parameter");
        {
            audio_plugin::AudioPluginAudioProcessor p;
            juce::Array<float> before;
            for (auto* prm : p.getParameters()) before.add (prm->getValue());
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorIfNeeded());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            for (int i = 0; i < p.getParameters().size(); ++i)
                expectEquals (p.getParameters()[i]->getValue(), before[i], p.getParameters()[i]->getName (64));
            expect (! p.presets.isModified());
            editor.reset();
        }

        beginTest ("Host programs: count, names, select");
        {
            audio_plugin::AudioPluginAudioProcessor p;
            expectEquals (p.getNumPrograms(), p.presets.getNumPresets());
            expect (p.getNumPrograms() >= (int) factory.size());
            expectEquals (p.getProgramName (1), factory[1].name);
            p.setCurrentProgram (4);
            expectEquals (p.getCurrentProgram(), 4);
            expectEquals (p.presets.getCurrentName(), factory[4].name);
        }

        beginTest ("User presets: save, list, load, rename, delete (temp folder)");
        {
            audio_plugin::AudioPluginAudioProcessor p;
            auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("TertiaryPresets", "");
            PresetManager m (p.apvts, folder);
            m.useTrash = false;
            const int numFactory = (int) factory.size();

            m.loadPreset (7);
            p.apvts.getParameter ("Depth Low Band")->setValueNotifyingHost (0.33f);
            expect (m.saveCurrentAsUser ("My Chop"));
            expect (folder.getChildFile ("My Chop.tertiarypreset").existsAsFile());
            expectEquals (m.getNumPresets(), numFactory + 1);
            expectEquals (m.getCurrentIndex(), numFactory);
            expect (! m.isModified());
            expect (! m.saveCurrentAsUser ("Hard Chop"));      // a factory name
            expect (! m.saveCurrentAsUser ("   "));

            m.loadPreset (0);
            expect (m.loadPreset (numFactory));
            expectWithinAbsoluteError (p.apvts.getParameter ("Depth Low Band")->getValue(), 0.33f, 0.01f);
            expectEquals (valueOf (p, "Invert Mid Band"), 1.0f);   // from Alternating Chop

            expect (m.saveCurrentAsUser ("Another"));               // sorted: Another, My Chop
            expectEquals (m.getPresetName (numFactory), juce::String ("Another"));
            expect (m.renameUser (numFactory + 1, "Zed Chop"));
            expectEquals (m.getPresetName (numFactory + 1), juce::String ("Zed Chop"));
            expect (! m.renameUser (numFactory + 1, "Another"));    // taken
            expect (m.deleteUser (numFactory));
            expectEquals (m.getNumPresets(), numFactory + 1);
            expect (! folder.getChildFile ("Another.tertiarypreset").exists());
            folder.deleteRecursively();
        }

        beginTest ("Session: the preset name is saved and restored; modified survives; 1.0.x sessions show No preset");
        {
            juce::MemoryBlock state;
            {
                audio_plugin::AudioPluginAudioProcessor a;
                a.setCurrentProgram (9);
                a.getStateInformation (state);
            }
            audio_plugin::AudioPluginAudioProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (b.presets.getCurrentIndex(), 9);
            expectEquals (b.presets.getCurrentName(), factory[9].name);
            expect (! b.presets.isModified());

            b.apvts.getParameter ("Depth High Band")->setValueNotifyingHost (0.05f);
            state.reset();   // getStateInformation appends to the block it is given; hosts pass an empty one
            b.getStateInformation (state);
            audio_plugin::AudioPluginAudioProcessor c;
            c.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (c.presets.getCurrentIndex(), 9);
            expect (c.presets.isModified());

            // A 1.0.x session: the parameters only, no preset name
            auto tree = c.apvts.copyState();
            tree.removeProperty ("presetName", nullptr);
            tree.removeProperty ("presetIsUser", nullptr);
            juce::MemoryOutputStream old;
            tree.writeToStream (old);
            audio_plugin::AudioPluginAudioProcessor d;
            d.setStateInformation (old.getData(), (int) old.getDataSize());
            expectEquals (d.presets.getCurrentName(), juce::String ("No preset"));
            expectEquals (d.getCurrentProgram(), 0);
            expectWithinAbsoluteError (d.apvts.getParameter ("Depth High Band")->getValue(), 0.05f, 0.01f);
        }
    }
};

static PresetTests presetTests;
static LicenseTests licenseTests;
static ProcessorTests processorTests;


/* Prints the test log to the console (JUCE writes it to the debugger by default) */
struct ConsoleLogger : juce::Logger
{
    void logMessage (const juce::String& message) override    { std::cout << message << std::endl; }
};

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    ConsoleLogger logger;
    juce::Logger::setCurrentLogger (&logger);

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("Tertiary");

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED: " + std::to_string (failures)) << std::endl;
    juce::Logger::setCurrentLogger (nullptr);
    return failures == 0 ? 0 : 1;
}
