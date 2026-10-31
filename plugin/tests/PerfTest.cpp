/*
  ==============================================================================

    PerfTest.cpp
    Created: 30 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-30] The performance rows of the test plan, T2-01 to T2-03, as one program. Joe: "let's do a final
    software test so that way when I sign off we know there's no more changes needed."

    It is a plain host: it loads a BUILT Tertiary.vst3 (any version, so the same program measures 1.0.2 and the release
    candidate) and plays it a deterministic signal with a host clock running at 120 BPM, so the synced LFOs move.

      PerfTest --vst3 <Tertiary.vst3> dsp                     T2-01: DSP cost per instance at 44.1 and 96 kHz,
                                                              buffers 64 and 512 (fresh instance each, 1 s warm-up,
                                                              3 x 10 s timed; every block timed)
      PerfTest --vst3 <Tertiary.vst3> live [--seconds 120]    T2-02: CPU and memory with the window open, closed, and
                                                              idle (loaded, no audio), each for --seconds, audio paced
                                                              in real time at 48 kHz / 512 as a DAW's audio thread
      PerfTest --vst3 <Tertiary.vst3> instances [--count 30] [--block 128] [--seconds 60]
                                                              T2-03: --count instances on ONE real-time thread (the
                                                              worst case; DAWs spread them over cores); a block whose
                                                              total time passes the block's duration is a dropout
      options: --out <file.json>  the numbers as JSON too

    Nothing is installed or written except --out. Built with -DWLA_TESTS=ON; not part of any shipped build.

  ==============================================================================
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <iostream>

#if JUCE_WINDOWS
 #include <windows.h>
 #include <psapi.h>
 #pragma comment (lib, "psapi.lib")
 #include <timeapi.h>
 #pragma comment (lib, "winmm.lib")
#elif JUCE_MAC
 #include <mach/mach.h>
 #include <sys/resource.h>
#else
 #include <sys/resource.h>
#endif
#include <ctime>

using Clock = std::chrono::steady_clock;

//==============================================================================
// The process's own CPU time and memory
//==============================================================================
static double processCpuSeconds()
{
   #if JUCE_WINDOWS
    FILETIME c, e, k, u;
    GetProcessTimes (GetCurrentProcess(), &c, &e, &k, &u);
    auto toS = [] (FILETIME f) { return (double) (((juce::uint64) f.dwHighDateTime << 32) | f.dwLowDateTime) * 1e-7; };
    return toS (k) + toS (u);
   #else
    rusage r {};
    getrusage (RUSAGE_SELF, &r);
    return (double) r.ru_utime.tv_sec + r.ru_utime.tv_usec * 1e-6 + (double) r.ru_stime.tv_sec + r.ru_stime.tv_usec * 1e-6;
   #endif
}

static double privateMB()   // Windows: private bytes (commit); Mac: resident size
{
   #if JUCE_WINDOWS
    PROCESS_MEMORY_COUNTERS_EX m {};
    GetProcessMemoryInfo (GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*) &m, sizeof (m));
    return (double) m.PrivateUsage / (1024.0 * 1024.0);
   #elif JUCE_MAC
    mach_task_basic_info info {};
    mach_msg_type_number_t n = MACH_TASK_BASIC_INFO_COUNT;
    task_info (mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info, &n);
    return (double) info.resident_size / (1024.0 * 1024.0);
   #else
    return 0.0;
   #endif
}

static double threadCpuSeconds()   // the calling thread's own CPU time
{
   #if JUCE_WINDOWS
    FILETIME c, e, k, u;
    GetThreadTimes (GetCurrentThread(), &c, &e, &k, &u);
    auto toS = [] (FILETIME f) { return (double) (((juce::uint64) f.dwHighDateTime << 32) | f.dwLowDateTime) * 1e-7; };
    return toS (k) + toS (u);
   #else
    timespec t {};
    clock_gettime (CLOCK_THREAD_CPUTIME_ID, &t);
    return (double) t.tv_sec + t.tv_nsec * 1e-9;
   #endif
}

static int guiObjects()   // Windows: GDI + USER handles (a leak shows as a climb); 0 elsewhere
{
   #if JUCE_WINDOWS
    return (int) GetGuiResources (GetCurrentProcess(), GR_GDIOBJECTS) + (int) GetGuiResources (GetCurrentProcess(), GR_USEROBJECTS);
   #else
    return 0;
   #endif
}

//==============================================================================
// A host clock: playing at 120 BPM from bar 1, advanced by the caller
//==============================================================================
struct HostClock : juce::AudioPlayHead
{
    double rate = 48000.0;
    std::atomic<juce::int64> samples { 0 };

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        const auto s = samples.load();
        p.setIsPlaying (true);
        p.setBpm (120.0);
        p.setTimeSignature (TimeSignature { 4, 4 });
        p.setTimeInSamples (s);
        p.setTimeInSeconds ((double) s / rate);
        p.setPpqPosition ((double) s / rate * 2.0);
        p.setPpqPositionOfLastBarStart (std::floor ((double) s / rate * 2.0 / 4.0) * 4.0);
        return p;
    }
};

//==============================================================================
// Loading
//==============================================================================
static juce::AudioPluginFormatManager formats;
static juce::PluginDescription description;

static bool findPlugin (const juce::String& path)
{
    formats.addDefaultFormats();
    for (auto* f : formats.getFormats())
    {
        if (f->getName() != "VST3") continue;
        juce::OwnedArray<juce::PluginDescription> found;
        f->findAllTypesForFile (found, path);
        if (! found.isEmpty()) { description = *found[0]; return true; }
    }
    return false;
}

static std::unique_ptr<juce::AudioPluginInstance> create (double rate, int block, HostClock& clock)
{
    juce::String error;
    auto p = formats.createPluginInstance (description, rate, block, error);
    if (p == nullptr) { std::cout << "FAIL  could not create the plugin: " << error << std::endl; return nullptr; }
    juce::AudioProcessor::BusesLayout stereo;
    stereo.inputBuses.add (juce::AudioChannelSet::stereo());
    stereo.outputBuses.add (juce::AudioChannelSet::stereo());
    p->setBusesLayout (stereo);
    clock.rate = rate;
    p->setPlayHead (&clock);
    p->setRateAndBufferSizeDetails (rate, block);
    p->prepareToPlay (rate, block);
    return p;
}

// A deterministic input: soft noise plus a 220 Hz tone, refilled into the block each time (the plugin works in place)
struct Signal
{
    juce::Random rnd { 1234 };
    double phase = 0.0;
    void fill (juce::AudioBuffer<float>& b, double rate)
    {
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = 0.2f * (float) std::sin (phase) + 0.05f * (rnd.nextFloat() * 2.0f - 1.0f);
            phase += juce::MathConstants<double>::twoPi * 220.0 / rate;
            for (int c = 0; c < b.getNumChannels(); ++c) b.setSample (c, i, v);
        }
        if (phase > 1.0e6) phase = std::fmod (phase, juce::MathConstants<double>::twoPi);
    }
};

static double percentile (std::vector<double> v, double q)
{
    if (v.empty()) return 0.0;
    std::sort (v.begin(), v.end());
    return v[(size_t) juce::jlimit<double> (0.0, (double) v.size() - 1.0, std::round (q * ((double) v.size() - 1.0)))];
}

static juce::var results = juce::var (new juce::DynamicObject());
static void put (const juce::String& k, const juce::var& v) { results.getDynamicObject()->setProperty (k, v); }

//==============================================================================
// T2-01: DSP cost per instance
//==============================================================================
static int runDsp()
{
    juce::Array<juce::var> rows;
    std::cout << "T2-01 DSP cost per instance (one instance, default settings, synced at 120 BPM)\n"
              << "rate    block  mean us/block  p99 us/block  % of one core (mean)  % of one core (p99)\n";
    for (double rate : { 44100.0, 96000.0 })
        for (int block : { 64, 512 })
        {
            HostClock clock;
            auto p = create (rate, block, clock);
            if (p == nullptr) return 1;
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            Signal sig;
            auto play = [&] (double seconds, std::vector<double>* times)
            {
                const int n = (int) std::ceil (seconds * rate / block);
                for (int i = 0; i < n; ++i)
                {
                    sig.fill (buf, rate);
                    const auto t0 = Clock::now();
                    p->processBlock (buf, midi);
                    const auto t1 = Clock::now();
                    clock.samples += block;
                    if (times) times->push_back (std::chrono::duration<double, std::micro> (t1 - t0).count());
                }
            };
            play (1.0, nullptr);   // warm-up
            std::vector<double> times;
            for (int run = 0; run < 3; ++run) play (10.0, &times);   // 31 s of audio in all: inside 1.0.2's demo's first 50 s
            double mean = 0; for (auto t : times) mean += t; mean /= (double) times.size();
            const double p99 = percentile (times, 0.99);
            const double blockUs = block / rate * 1e6;
            std::cout << juce::String (rate, 0).paddedRight (' ', 8) << juce::String (block).paddedRight (' ', 7)
                      << juce::String (mean, 2).paddedRight (' ', 15) << juce::String (p99, 2).paddedRight (' ', 14)
                      << juce::String (100.0 * mean / blockUs, 3).paddedRight (' ', 22) << juce::String (100.0 * p99 / blockUs, 3) << "\n";
            auto* row = new juce::DynamicObject();
            row->setProperty ("rate", rate); row->setProperty ("block", block);
            row->setProperty ("meanUs", mean); row->setProperty ("p99Us", p99);
            row->setProperty ("meanPctCore", 100.0 * mean / blockUs); row->setProperty ("p99PctCore", 100.0 * p99 / blockUs);
            rows.add (juce::var (row));
            p->releaseResources();
        }
    put ("dsp", rows);
    return 0;
}

//==============================================================================
// A real-time audio thread: one block of every instance per block period, paced by the clock
//==============================================================================
struct AudioThread
{
    std::vector<juce::AudioPluginInstance*> plugins;
    HostClock* clock = nullptr;
    double rate = 48000.0;
    int block = 512;
    std::atomic<bool> stop { false };
    std::atomic<double> busySeconds { 0.0 };   // elapsed time inside processBlock (includes being preempted)
    std::atomic<double> cpuSeconds { 0.0 };    // [Claude 2026-10-30] the audio thread's real CPU time (Codex review)
    // [Claude 2026-10-30] a late block is one that finished after its deadline (its scheduled start + one period),
    // however late it started; missedPeriods counts whole periods lost before catching up (Codex review)
    std::atomic<juce::int64> blocks { 0 }, overruns { 0 }, missedPeriods { 0 };
    std::vector<double> blockUs;   // read only after join
    std::thread thread;

   #if JUCE_WINDOWS
    HANDLE timer = nullptr;
   #endif
    void waitUntil (Clock::time_point deadline)
    {
        const auto early = deadline - std::chrono::microseconds (300);
       #if JUCE_WINDOWS
        const auto left = std::chrono::duration_cast<std::chrono::nanoseconds> (early - Clock::now()).count();
        if (timer != nullptr && left > 0)
        {
            LARGE_INTEGER due;
            due.QuadPart = -(LONGLONG) (left / 100);   // relative, in 100 ns units
            if (SetWaitableTimerEx (timer, &due, 0, nullptr, nullptr, nullptr, 0))
                WaitForSingleObject (timer, INFINITE);
        }
       #else
        std::this_thread::sleep_until (early);
       #endif
        while (Clock::now() < deadline) std::this_thread::yield();
    }

    void start()
    {
        thread = std::thread ([this]
        {
           #if JUCE_WINDOWS
            SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
            // [Claude 2026-10-30] Windows sleeps in 15.6 ms steps by default, longer than a block: the first run
            // "missed" thousands of periods with the thread a quarter busy. 1 ms timer steps, then a spin to the deadline.
            // That alone was not enough: Windows 11 ignores timeBeginPeriod for a process with no visible window, and
            // with NO plugin loaded the thread still missed half its periods. A high-resolution waitable timer is not
            // affected; it sleeps to 0.3 ms before the deadline and a spin covers the rest (a DAW's audio thread is
            // woken by the audio driver instead).
            timeBeginPeriod (1);
            timer = CreateWaitableTimerExW (nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
           #endif
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            Signal sig;
            const auto period = std::chrono::duration_cast<Clock::duration> (std::chrono::duration<double> (block / rate));
            const double cpu0 = threadCpuSeconds();
            auto next = Clock::now();   // this block's scheduled start
            blockUs.reserve (1 << 20);
            while (! stop.load())
            {
                const auto t0 = Clock::now();
                for (auto* p : plugins) { sig.fill (buf, rate); p->processBlock (buf, midi); }
                const auto t1 = Clock::now();
                clock->samples += block;
                const double us = std::chrono::duration<double, std::micro> (t1 - t0).count();
                busySeconds = busySeconds.load() + us * 1e-6;
                cpuSeconds = threadCpuSeconds() - cpu0;
                if (t1 > next + period) ++overruns;   // finished after its deadline
                ++blocks;
                if (blockUs.size() < blockUs.capacity()) blockUs.push_back (us);
                next += period;
                if (t1 > next + period) {   // more than a whole period behind: those periods are lost; carry on from now
                    missedPeriods += (juce::int64) ((t1 - next) / period);
                    next = t1;
                }
                waitUntil (next);
            }
           #if JUCE_WINDOWS
            if (timer != nullptr) CloseHandle (timer);
            timeEndPeriod (1);
           #endif
        });
    }
    void finish() { stop = true; if (thread.joinable()) thread.join(); }
};

//==============================================================================
// T2-02: CPU and memory with the window open, closed and idle
//==============================================================================
static juce::var phase (const juce::String& name, double seconds, AudioThread* audio)
{
    struct Sample { double t, cpu, mb; int gui; };
    std::vector<Sample> s;
    const auto t0 = Clock::now();
    const double cpu0 = processCpuSeconds();
    const double busy0 = audio ? audio->busySeconds.load() : 0.0;
    const double acpu0 = audio ? audio->cpuSeconds.load() : 0.0;
    const auto overruns0 = audio ? audio->overruns.load() : 0;
    for (;;)
    {
        const double t = std::chrono::duration<double> (Clock::now() - t0).count();
        s.push_back ({ t, processCpuSeconds() - cpu0, privateMB(), guiObjects() });
        if (t >= seconds) break;
        juce::MessageManager::getInstance()->runDispatchLoopUntil (1000);
    }
    const double wall = s.back().t;
    const double cpuPct = 100.0 * s.back().cpu / wall;
    const double audioPct = audio ? 100.0 * (audio->cpuSeconds.load() - acpu0) / wall : 0.0;      // audio thread CPU
    const double busyPct = audio ? 100.0 * (audio->busySeconds.load() - busy0) / wall : 0.0;      // elapsed in processBlock
    // Memory trend: least-squares slope over the second half (after anything that settles on open)
    double sx = 0, sy = 0, sxx = 0, sxy = 0; int n = 0;
    double mbMax = 0;
    for (auto& x : s)
    {
        mbMax = juce::jmax (mbMax, x.mb);
        if (x.t < wall / 2) continue;
        sx += x.t; sy += x.mb; sxx += x.t * x.t; sxy += x.t * x.mb; ++n;
    }
    const double slope = n > 1 ? (n * sxy - sx * sy) / (n * sxx - sx * sx) * 60.0 : 0.0;   // MB per minute
    std::cout << name.paddedRight (' ', 22) << juce::String (wall, 0).paddedRight (' ', 6)
              << juce::String (cpuPct, 2).paddedRight (' ', 14) << juce::String (audioPct, 2).paddedRight (' ', 13)
              << juce::String (cpuPct - audioPct, 2).paddedRight (' ', 14)
              << juce::String (s.front().mb, 1).paddedRight (' ', 9) << juce::String (s.back().mb, 1).paddedRight (' ', 9)
              << juce::String (mbMax, 1).paddedRight (' ', 9) << juce::String (slope, 3).paddedRight (' ', 12)
              << juce::String (s.front().gui) << " -> " << juce::String (s.back().gui)
              << "   in processBlock (elapsed) " << juce::String (busyPct, 2) << "%   late blocks " << juce::String (audio ? (juce::int64) (audio->overruns.load() - overruns0) : (juce::int64) 0) << "\n";
    auto* o = new juce::DynamicObject();
    o->setProperty ("phase", name); o->setProperty ("seconds", wall);
    o->setProperty ("processCpuPctCore", cpuPct); o->setProperty ("audioThreadPctCore", audioPct);
    o->setProperty ("otherPctCore", cpuPct - audioPct);
    o->setProperty ("audioElapsedPctCore", busyPct);
    o->setProperty ("mbStart", s.front().mb); o->setProperty ("mbEnd", s.back().mb); o->setProperty ("mbMax", mbMax);
    o->setProperty ("mbPerMinSecondHalf", slope);
    o->setProperty ("guiObjectsStart", s.front().gui); o->setProperty ("guiObjectsEnd", s.back().gui);
    o->setProperty ("overruns", audio ? (juce::int64) (audio->overruns.load() - overruns0) : (juce::int64) 0);
    return juce::var (o);
}

static int runLive (double seconds)
{
    const double rate = 48000.0; const int block = 512;
    HostClock clock;
    auto p = create (rate, block, clock);
    if (p == nullptr) return 1;
    juce::Array<juce::var> rows;
    std::cout << "T2-02 CPU and memory, 48 kHz / 512, " << seconds << " s each (CPU in % of one core; memory = "
              << (JUCE_WINDOWS ? "private bytes" : "resident") << ", MB)\n"
              << "phase                 s     process CPU   audio thread  everything else  MB start MB end   MB max   MB/min(2nd half)  GUI objects\n"
              << "(audio thread and everything else: CPU time; in processBlock (elapsed): wall time, preemption included)\n";

    AudioThread audio; audio.plugins = { p.get() }; audio.clock = &clock; audio.rate = rate; audio.block = block;
    audio.start();
    {
        // Window open: the editor in a real window on screen, in front, so it paints as it would in a DAW
        std::unique_ptr<juce::AudioProcessorEditor> editor (p->createEditorIfNeeded());
        juce::DocumentWindow window ("Tertiary PerfTest", juce::Colours::black, juce::DocumentWindow::closeButton);
        window.setUsingNativeTitleBar (true);
        window.setContentNonOwned (editor.get(), true);
        window.setAlwaysOnTop (true);
        window.centreWithSize (window.getWidth(), window.getHeight());
        window.setVisible (true);
        rows.add (phase ("window open, playing", seconds, &audio));
        window.clearContentComponent();
        window.setVisible (false);
        editor.reset();
    }
    rows.add (phase ("window closed, playing", seconds, &audio));
    audio.finish();
    rows.add (phase ("idle (loaded, no audio)", seconds, nullptr));
    put ("live", rows);
    p->releaseResources();
    return 0;
}

//==============================================================================
// T2-03: many instances on one real-time thread
//==============================================================================
static int runInstances (int count, int block, double seconds)
{
    const double rate = 48000.0;
    HostClock clock;
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> owned;
    const double mb0 = privateMB();
    for (int i = 0; i < count; ++i)
    {
        auto p = create (rate, block, clock);
        if (p == nullptr) return 1;
        owned.push_back (std::move (p));
    }
    const double mbLoaded = privateMB();
    AudioThread audio; audio.clock = &clock; audio.rate = rate; audio.block = block;
    for (auto& p : owned) audio.plugins.push_back (p.get());
    audio.start();
    const auto t0 = Clock::now();
    while (std::chrono::duration<double> (Clock::now() - t0).count() < seconds)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
    audio.finish();
    const double wall = std::chrono::duration<double> (Clock::now() - t0).count();
    const double blockUs = block / rate * 1e6;
    double mean = 0; for (auto t : audio.blockUs) mean += t; mean /= juce::jmax<size_t> (1, audio.blockUs.size());
    const double p99 = percentile (audio.blockUs, 0.99);
    const double mx = audio.blockUs.empty() ? 0.0 : *std::max_element (audio.blockUs.begin(), audio.blockUs.end());
    const double first = audio.blockUs.empty() ? 0.0 : audio.blockUs.front();
    std::cout << "T2-03 " << count << " instances on one real-time thread, 48 kHz / " << block << " (" << juce::String (blockUs, 0) << " us per block), "
              << juce::String (wall, 0) << " s\n"
              << "  blocks " << audio.blocks.load() << " of " << (juce::int64) (wall * rate / block) << " due; dropouts (finished after the deadline) "
              << audio.overruns.load() << ", whole periods missed " << audio.missedPeriods.load() << "\n"
              << "  first block: " << juce::String (first, 1) << " us\n"
              << "  all instances per block: mean " << juce::String (mean, 1) << " us (" << juce::String (100.0 * mean / blockUs, 1)
              << "% of the block), p99 " << juce::String (p99, 1) << " us (" << juce::String (100.0 * p99 / blockUs, 1)
              << "%), max " << juce::String (mx, 1) << " us (" << juce::String (100.0 * mx / blockUs, 1) << "%)\n"
              << "  blocks over the deadline, by when: " << [&]
                 {
                     // [Claude 2026-10-30] a one-off at the start (first-use setup) reads differently from a recurring one
                     juce::String when; int shown = 0;
                     for (size_t i = 0; i < audio.blockUs.size() && shown < 12; ++i)
                         if (audio.blockUs[i] > blockUs) { when << juce::String ((double) i * block / rate, 2) << " s (" << juce::String (audio.blockUs[i] / 1000.0, 1) << " ms)  "; ++shown; }
                     return when.isEmpty() ? juce::String ("none") : when;
                 }() << "\n"
              << "  memory: " << juce::String (mbLoaded - mb0, 1) << " MB for " << count << " instances ("
              << juce::String ((mbLoaded - mb0) / count, 2) << " MB each)\n";
    auto* o = new juce::DynamicObject();
    o->setProperty ("count", count); o->setProperty ("block", block); o->setProperty ("seconds", wall);
    o->setProperty ("blocks", audio.blocks.load()); o->setProperty ("dropouts", audio.overruns.load());
    o->setProperty ("missedPeriods", audio.missedPeriods.load()); o->setProperty ("blocksDue", (juce::int64) (wall * rate / block));
    o->setProperty ("meanUs", mean); o->setProperty ("p99Us", p99); o->setProperty ("maxUs", mx);
    o->setProperty ("firstUs", first);
    o->setProperty ("meanPctBlock", 100.0 * mean / blockUs); o->setProperty ("p99PctBlock", 100.0 * p99 / blockUs);
    o->setProperty ("maxPctBlock", 100.0 * mx / blockUs);
    o->setProperty ("mbForAll", mbLoaded - mb0);
    put ("instances_" + juce::String (block), juce::var (o));
    for (auto& p : owned) p->releaseResources();
    return audio.overruns.load() == 0 && audio.missedPeriods.load() == 0 ? 0 : 3;
}

//==============================================================================
int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);
    auto opt = [&] (const juce::String& k, const juce::String& d) { const int i = args.indexOf (k); return i >= 0 && i + 1 < args.size() ? args[i + 1] : d; };

    const auto path = opt ("--vst3", {});
    if (path.isEmpty() || ! findPlugin (juce::File::getCurrentWorkingDirectory().getChildFile (path).getFullPathName()))
    {
        std::cout << "usage: PerfTest --vst3 <Tertiary.vst3> dsp | live [--seconds 120] | instances [--count 30] [--block 128] [--seconds 60]  [--out file.json]\n";
        return 2;
    }
    std::cout << "Plugin: " << description.name << " " << description.version << "  (" << description.fileOrIdentifier << ")\n";
    put ("plugin", description.fileOrIdentifier); put ("version", description.version);
    put ("machine", juce::SystemStats::getComputerName() + ", " + juce::SystemStats::getCpuModel());

    int code = 2;
    if (args.contains ("dsp"))            code = runDsp();
    else if (args.contains ("live"))      code = runLive (opt ("--seconds", "120").getDoubleValue());
    else if (args.contains ("instances")) code = runInstances (opt ("--count", "30").getIntValue(), opt ("--block", "128").getIntValue(),
                                                               opt ("--seconds", "60").getDoubleValue());
    const auto out = opt ("--out", {});
    if (out.isNotEmpty())
        juce::File::getCurrentWorkingDirectory().getChildFile (out).replaceWithText (juce::JSON::toString (results));
    std::cout << std::flush;
    return code;
}
