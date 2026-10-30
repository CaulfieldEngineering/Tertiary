/*
  ==============================================================================

    UiRobot.h
    Created: 29 Oct 2026
    Author:  Claude (for Joe)

    [Claude 2026-10-29] New file. A small framework for testing a JUCE plugin's real editor the way a person uses
    it. Nothing in it is specific to one plugin (Tertiary's scenarios are in ../UiTest.cpp), so every plugin made
    from NewPluginTemplate can use it. Joe, 2026-10-29: "I want to check as much as possible automated prior."

    Include it in exactly one .cpp (it defines the program's operator new and delete), write the scenarios, and
    hand them to uirobot::run from main(). What the scenarios get:

      Mouse              moves, presses, drags, releases, double-clicks and modifier clicks, fed to the window as the
                         operating system would (ComponentPeer::handleMouseEvent), so JUCE does the hit testing,
                         hover, capture and double-click detection itself
      pressKey/typeText  key presses to the focused component
      menus              an open pop-up menu's item found by name and chosen (its own accessibility action)
      modalAlertWindow   the AlertWindow that just opened
      pictures           a component drawn to an image (saved as PNG with --snapshots), colour and difference checks
      param/value        a processor's parameters in their own units; HostClock, a playing host; Envelope, a level
                         read every 2 ms, with an LFO's frequency and depth
      check/note         one PASS or FAIL line per check, naming the scenario and its test-plan IDs, and why

    What it does around them, so the editor can sit in a window placed off the screen and still be used:
      - a pretend pointer: the position the components read back as the mouse is that of the last event sent, never
        the real mouse (which is never moved)
      - pretend focus: as far as JUCE can tell, this program is the app in front and its window has the keyboard
        focus (pop-up menus otherwise close themselves at once); the real focus on the desktop is never taken
      - freed memory is overwritten and live allocations are counted, so a listener or timer left pointing at a
        closed window crashes at once, in the scenario that caused it
      - a crash, or a scenario that runs too long, prints where it was (functions, files and lines) and stops
    On Windows the hooks patch this program's own imports of GetCursorPos, WindowFromPoint, GetFocus, SetFocus and
    GetForegroundWindow; on a Mac they replace +[NSEvent mouseLocation],
    +[NSWindow windowNumberAtPoint:belowWindowWithWindowNumber:] and -[NSApplication isActive]. Nothing outside this
    program is affected.

    Build the program without JUCE_MODAL_LOOPS_PERMITTED, as plugins are built, so pop-ups behave as in a host.
    On Windows, build it with symbols (/Zi, /DEBUG) so crash and hang reports name functions and lines.

    Options every runner has:
        --list                the scenarios and the test-plan IDs they cover
        --only U-05,R-06      some scenarios, by their ID or by a test-plan ID they cover
        --snapshots <folder>  save the pictures the scenarios take
        --timeout <seconds>   stop and report a scenario that runs longer (default 180)
        --trace               print every mouse event sent
        --no-poison           don't overwrite freed memory (to rule it out)

  ==============================================================================
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <vector>

#if JUCE_WINDOWS
 #include <malloc.h>
 #include <psapi.h>
 #include <dbghelp.h>
 #pragma comment (lib, "dbghelp.lib")
#elif JUCE_MAC
 #include <malloc/malloc.h>
 #include <objc/runtime.h>
 #include <objc/message.h>
 #include <CoreFoundation/CoreFoundation.h>
 #include <mach/mach.h>
 extern "C" void* objc_autoreleasePoolPush (void);   // libobjc (what @autoreleasepool compiles to)
 extern "C" void objc_autoreleasePoolPop (void*);
 #include <unistd.h>
#else
 #include <malloc.h>
 #include <unistd.h>
#endif

#ifndef UIROBOT_ASAN
 #define UIROBOT_ASAN 0   // 1 when built with the address sanitiser (which then watches freed memory itself)
#endif

#if ! JUCE_MAC
namespace juce::detail { bool dispatchNextMessageOnSystemQueue (bool returnIfNoPendingMessages); }   // juce_events
#endif

//==============================================================================
// Memory: every new and delete in this program goes through here. The number of live allocations is counted
// (for the leak check in U-24), and freed memory is overwritten with 0xDD before it is released, so anything
// that uses an object after it was deleted (a timer, a listener) reads garbage and crashes at once, in the
// scenario that caused it, instead of quietly working. A poor man's address sanitiser.
//==============================================================================
namespace
{
    std::atomic<long long> liveAllocations { 0 };
    std::atomic<bool> poisonFrees { ! UIROBOT_ASAN };   // the sanitiser does this better when it is on

    size_t allocationSize (void* p)
    {
       #if JUCE_WINDOWS
        return _msize (p);
       #elif JUCE_MAC
        return malloc_size (p);
       #else
        return malloc_usable_size (p);
       #endif
    }
}

void* operator new (std::size_t size)
{
    if (auto* p = std::malloc (size == 0 ? 1 : size))
    {
        ++liveAllocations;
        return p;
    }
    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)                         { return operator new (size); }

void operator delete (void* p) noexcept
{
    if (p == nullptr)
        return;
    --liveAllocations;
    if (poisonFrees)
        std::memset (p, 0xDD, allocationSize (p));
    std::free (p);
}

void operator delete[] (void* p) noexcept                       { operator delete (p); }
void operator delete (void* p, std::size_t) noexcept            { operator delete (p); }
void operator delete[] (void* p, std::size_t) noexcept          { operator delete (p); }

namespace uirobot
{
//==============================================================================
// Results
//==============================================================================
int passes = 0, failures = 0;
juce::StringArray failureLines;
juce::String currentScenario { "start-up" };
std::atomic<juce::int64> scenarioStartMs { 0 };
juce::File snapshotFolder;
bool focusFallbackNoted = false;
bool traceMouse = false;   // --trace

struct Scenario
{
    juce::String id, plan, title;   // plan: the test-plan IDs it covers, e.g. "T2-05 R-06"
    std::function<void()> run;
};

std::vector<Scenario> registered;

juce::String planOf (const juce::String& id)
{
    for (auto& s : registered)
        if (s.id == id)
            return s.plan;
    return {};
}

void check (bool ok, const juce::String& what, const juce::String& detail = {}, const juce::String& why = {})
{
    const auto line = juce::String (ok ? "PASS " : "FAIL ") + currentScenario.paddedRight (' ', 5)
                      + " [" + planOf (currentScenario) + "] " + what
                      + (detail.isNotEmpty() ? "   (" + detail + ")" : juce::String())
                      + (! ok && why.isNotEmpty() ? "\n         why: " + why : juce::String());
    std::cout << line << std::endl;
    if (ok) ++passes;
    else { ++failures; failureLines.add (line); }
}

void note (const juce::String& text)    { std::cout << "INFO " << currentScenario.paddedRight (' ', 5) << " " << text << std::endl; }

juce::String fmt (double v, int places = 2)   { return juce::String (v, places); }

bool closeTo (double a, double b, double tolerance)  { return std::abs (a - b) <= tolerance; }

//==============================================================================
// Crashes and hangs say where they happened
//==============================================================================
char scenarioName[32] = "start-up";   // for the crash and hang messages, which must not allocate

void setScenario (const juce::String& id)
{
    currentScenario = id;
    id.copyToUTF8 (scenarioName, sizeof (scenarioName));
}

void crashed (const char* what)
{
    std::fprintf (stdout, "\nFAIL %s CRASH (%s) during this scenario. A use-after-free or a dangling listener or timer is the usual cause.\n",
                  scenarioName, what);
    std::fflush (stdout);
    std::_Exit (3);
}

#if JUCE_WINDOWS
void printStack (HANDLE thread, CONTEXT context);

LONG WINAPI onWindowsCrash (EXCEPTION_POINTERS* info)
{
    std::fprintf (stdout, "\nFAIL %s CRASH (exception 0x%08lx) during this scenario. A use-after-free or a dangling listener or timer is the usual cause. It was here:\n",
                  scenarioName, (unsigned long) info->ExceptionRecord->ExceptionCode);
    std::fflush (stdout);
    printStack (GetCurrentThread(), *info->ContextRecord);
    std::_Exit (3);
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
void onSignal (int sig)   { crashed (sig == SIGSEGV ? "SIGSEGV" : sig == SIGBUS ? "SIGBUS" : sig == SIGABRT ? "SIGABRT" : "signal"); }
#endif

#if JUCE_WINDOWS
HANDLE mainThread = nullptr;

/* Prints where a thread is (function names, when the program was built with symbols) */
void printStack (HANDLE thread, CONTEXT context)
{
    auto process = GetCurrentProcess();
    static bool symbolsLoaded = false;
    if (! symbolsLoaded) { SymSetOptions (SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES); SymInitialize (process, nullptr, TRUE); symbolsLoaded = true; }
    STACKFRAME64 frame {};
    frame.AddrPC.Offset = context.Rip;    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp; frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp; frame.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 40 && StackWalk64 (IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr,
                                           SymFunctionTableAccess64, SymGetModuleBase64, nullptr); ++i)
    {
        char buffer[sizeof (SYMBOL_INFO) + 256] {};
        auto* symbol = (SYMBOL_INFO*) buffer;
        symbol->SizeOfStruct = sizeof (SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        DWORD64 displacement = 0;
        IMAGEHLP_LINE64 line {};
        line.SizeOfStruct = sizeof (line);
        DWORD lineDisplacement = 0;
        const bool named = SymFromAddr (process, frame.AddrPC.Offset, &displacement, symbol);
        const bool hasLine = SymGetLineFromAddr64 (process, frame.AddrPC.Offset, &lineDisplacement, &line);
        std::fprintf (stdout, "      at %s", named ? symbol->Name : "?");
        if (hasLine) std::fprintf (stdout, "  (%s:%lu)", line.FileName, (unsigned long) line.LineNumber);
        std::fprintf (stdout, "\n");
    }
    std::fflush (stdout);
}
#endif

/* Kills the run if one scenario takes longer than this (a modal loop that never returns, a deadlock) */
int timeoutSeconds = 180;

struct Watchdog : juce::Thread
{
    Watchdog() : juce::Thread ("UiTest watchdog") { startThread(); }
    ~Watchdog() override { stopThread (2000); }
    void run() override
    {
        while (! threadShouldExit())
        {
            wait (500);
            const auto started = scenarioStartMs.load();
            if (started > 0 && juce::Time::currentTimeMillis() - started > timeoutSeconds * 1000LL)
            {
                std::fprintf (stdout, "\nFAIL %s TIMEOUT: this scenario ran for over %d s and was stopped. It was here:\n", scenarioName, timeoutSeconds);
                std::fflush (stdout);
               #if JUCE_WINDOWS
                if (mainThread != nullptr && SuspendThread (mainThread) != (DWORD) -1)
                {
                    CONTEXT context {};
                    context.ContextFlags = CONTEXT_FULL;
                    if (GetThreadContext (mainThread, &context))
                        printStack (mainThread, context);
                }
               #endif
                std::_Exit (4);
            }
        }
    }
};

//==============================================================================
// The pretend pointer. Components ask JUCE where the mouse is (isMouseOver, the pop-up menus' tracking);
// on a real machine that reads the system pointer. Here it reads this program's pretend one, which follows
// the events the tests send. The real mouse is never moved.
//==============================================================================
juce::Point<float> fakePointerLogical;
bool fakePointerWorks = false;

#if JUCE_WINDOWS
POINT fakeCursorPhysical { -30000, -30000 };
bool fakeCursorIsPhysical = true;
using WindowFromPointFn = HWND (WINAPI*) (POINT);
WindowFromPointFn realWindowFromPoint = nullptr;

BOOL WINAPI fakeGetCursorPos (LPPOINT p)
{
    if (p == nullptr) return FALSE;
    *p = fakeCursorPhysical;
    return TRUE;
}

/* "Which window is at this point?": this program's own windows answer first (newest on top, as menus are),
   so the off-screen window counts as under the pretend pointer */
HWND WINAPI fakeWindowFromPoint (POINT pt)
{
    auto isOurs = [] (HWND h)
    {
        for (int i = 0; i < juce::ComponentPeer::getNumPeers(); ++i)
            if (auto* peer = juce::ComponentPeer::getPeer (i))
                if ((HWND) peer->getNativeHandle() == h)
                    return true;
        return false;
    };
    // the system's own stacking order, front to back
    for (HWND h = GetTopWindow (nullptr); h != nullptr; h = GetWindow (h, GW_HWNDNEXT))
    {
        RECT r;
        if (IsWindowVisible (h) && GetWindowRect (h, &r) && PtInRect (&r, pt) && isOurs (h))
            return h;
    }
    return realWindowFromPoint != nullptr ? realWindowFromPoint (pt) : nullptr;
}

/* Keyboard focus and "which app is in front": this program's windows are, as far as JUCE can tell, the focused
   ones. Nothing real changes: the window you are working in keeps the focus. */
HWND fakeFocus = nullptr;
HWND WINAPI fakeGetFocus()                 { return fakeFocus; }
HWND WINAPI fakeSetFocus (HWND h)          { auto old = fakeFocus; fakeFocus = h; return old; }
HWND WINAPI fakeGetForegroundWindow()
{
    if (fakeFocus != nullptr && IsWindow (fakeFocus)) return fakeFocus;
    for (int i = juce::ComponentPeer::getNumPeers(); --i >= 0;)
        if (auto* peer = juce::ComponentPeer::getPeer (i))
            return (HWND) peer->getNativeHandle();
    return nullptr;
}

/* Points this program's own import of a user32 function (the one JUCE calls) at a replacement */
void* hookUser32 (const char* function, void* replacement)
{
    auto* base = (BYTE*) GetModuleHandleW (nullptr);
    auto* dos = (IMAGE_DOS_HEADER*) base;
    auto* nt = (IMAGE_NT_HEADERS*) (base + dos->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* imp = (IMAGE_IMPORT_DESCRIPTOR*) (base + dir.VirtualAddress); imp->Name != 0; ++imp)
    {
        if (_stricmp ((const char*) (base + imp->Name), "user32.dll") != 0)
            continue;
        auto* thunk = (IMAGE_THUNK_DATA*) (base + imp->FirstThunk);
        auto* names = (IMAGE_THUNK_DATA*) (base + (imp->OriginalFirstThunk != 0 ? imp->OriginalFirstThunk : imp->FirstThunk));
        for (; names->u1.AddressOfData != 0; ++names, ++thunk)
        {
            if (IMAGE_SNAP_BY_ORDINAL (names->u1.Ordinal))
                continue;
            auto* byName = (IMAGE_IMPORT_BY_NAME*) (base + names->u1.AddressOfData);
            if (std::strcmp ((const char*) byName->Name, function) == 0)
            {
                auto* original = (void*) thunk->u1.Function;
                DWORD old = 0;
                VirtualProtect (&thunk->u1.Function, sizeof (void*), PAGE_READWRITE, &old);
                thunk->u1.Function = (ULONG_PTR) replacement;
                VirtualProtect (&thunk->u1.Function, sizeof (void*), old, &old);
                return original;
            }
        }
    }
    return nullptr;
}

void setFakePointer (juce::Point<float> logical)
{
    fakePointerLogical = logical;
    auto p = fakeCursorIsPhysical ? juce::Desktop::getInstance().getDisplays().logicalToPhysical (logical) : logical;
    fakeCursorPhysical = { (LONG) std::lround (p.x), (LONG) std::lround (p.y) };
}

bool installFakePointer()
{
    realWindowFromPoint = (WindowFromPointFn) hookUser32 ("WindowFromPoint", (void*) &fakeWindowFromPoint);
    if (hookUser32 ("GetCursorPos", (void*) &fakeGetCursorPos) == nullptr || realWindowFromPoint == nullptr)
        return false;
    hookUser32 ("GetFocus", (void*) &fakeGetFocus);
    hookUser32 ("SetFocus", (void*) &fakeSetFocus);
    hookUser32 ("GetForegroundWindow", (void*) &fakeGetForegroundWindow);
    for (bool physical : { true, false })
    {
        fakeCursorIsPhysical = physical;
        setFakePointer ({ 123.f, 77.f });
        if (juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().getDistanceFrom ({ 123.f, 77.f }) < 1.5f)
            return true;
    }
    return false;
}
#elif JUCE_MAC
struct MacPoint { double x, y; };
MacPoint fakeMouseLocation { -30000, -30000 };
double macMainScreenHeight = 0;
using WindowNumberAtPointFn = long (*) (id, SEL, MacPoint, long);
WindowNumberAtPointFn realWindowNumberAtPoint = nullptr;

MacPoint fakeNSEventMouseLocation (id, SEL)   { return fakeMouseLocation; }

/* "Which window is at this point?": this program's own windows answer first (newest on top, as menus are) */
long fakeWindowNumberAtPoint (id self, SEL cmd, MacPoint p, long below)
{
    const juce::Point<int> point ((int) std::floor (p.x), (int) std::floor (macMainScreenHeight - p.y));
    for (int i = juce::ComponentPeer::getNumPeers(); --i >= 0;)
        if (auto* peer = juce::ComponentPeer::getPeer (i))
            if (peer->getComponent().isShowing() && peer->getBounds().contains (point))
                if (auto view = (id) peer->getNativeHandle())
                    if (auto window = ((id (*) (id, SEL)) objc_msgSend) (view, sel_registerName ("window")))
                        return ((long (*) (id, SEL)) objc_msgSend) (window, sel_registerName ("windowNumber"));
    return realWindowNumberAtPoint != nullptr ? realWindowNumberAtPoint (self, cmd, p, below) : 0;
}

signed char fakeIsActive (id, SEL)   { return 1; }   // NSApp isActive: as if this program were the app in front

void setFakePointer (juce::Point<float> logical)
{
    fakePointerLogical = logical;
    fakeMouseLocation = { (double) logical.x, macMainScreenHeight - (double) logical.y };
}

bool installFakePointer()
{
    auto mouseLocation = class_getClassMethod (objc_getClass ("NSEvent"), sel_registerName ("mouseLocation"));
    auto windowAtPoint = class_getClassMethod (objc_getClass ("NSWindow"), sel_registerName ("windowNumberAtPoint:belowWindowWithWindowNumber:"));
    if (mouseLocation == nullptr || windowAtPoint == nullptr)
        return false;
    method_setImplementation (mouseLocation, (IMP) &fakeNSEventMouseLocation);
    realWindowNumberAtPoint = (WindowNumberAtPointFn) method_setImplementation (windowAtPoint, (IMP) &fakeWindowNumberAtPoint);
    if (auto isActive = class_getInstanceMethod (objc_getClass ("NSApplication"), sel_registerName ("isActive")))
        method_setImplementation (isActive, (IMP) &fakeIsActive);
    fakeMouseLocation = { 0, 0 };   // JUCE reads (0, 0) as (0, main screen height)
    macMainScreenHeight = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().y;
    setFakePointer ({ 123.f, 77.f });
    return juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().getDistanceFrom ({ 123.f, 77.f }) < 1.5f;
}
#else
void setFakePointer (juce::Point<float> logical)   { fakePointerLogical = logical; }
bool installFakePointer()                         { return false; }
#endif

//==============================================================================
// Driving the window
//==============================================================================
/* Runs the message loop for a while: async messages, timers, repaints. Written here rather than with JUCE's
   runDispatchLoopUntil, which needs JUCE_MODAL_LOOPS_PERMITTED; the plugin is built without modal loops, and so is
   this program, so pop-ups behave exactly as they do in a host. */
/* An Objective-C autorelease pool (nothing elsewhere). A host's event loop drains one on every event; without them,
   anything a Cocoa object keeps alive until it is released stays alive for good, and looks like a leak. */
struct AutoreleasePool
{
   #if JUCE_MAC
    AutoreleasePool() : pool (objc_autoreleasePoolPush()) {}
    ~AutoreleasePool() { objc_autoreleasePoolPop (pool); }
    void* pool;
   #endif
};

void pump (int ms = 15)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + ms;
    do
    {
       #if JUCE_MAC
        AutoreleasePool pool;
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.001, true);
       #else
        if (! juce::detail::dispatchNextMessageOnSystemQueue (true))
            juce::Thread::sleep (1);
       #endif
    }
    while (juce::Time::getMillisecondCounterHiRes() < end);
}

juce::int64 eventTime = 0;
juce::int64 nextEventTime (int stepMs)
{
    eventTime = juce::jmax (eventTime + stepMs, juce::Time::currentTimeMillis());
    return eventTime;
}

/* A mouse, fed to the window the target component is in */
struct Mouse
{
    juce::ModifierKeys keys;   // keyboard modifiers held (Ctrl, Shift...)

    static juce::Point<float> inPeer (juce::Component& c, juce::Point<float> local, juce::ComponentPeer*& peer)
    {
        peer = c.getPeer();
        jassert (peer != nullptr);
        return peer->getComponent().getLocalPoint (&c, local);
    }

    void send (juce::Component& c, juce::Point<float> local, bool buttonDown, int stepMs)
    {
        juce::ComponentPeer* peer = nullptr;
        const auto p = inPeer (c, local, peer);
        if (traceMouse)
            std::cout << "  mouse " << (buttonDown ? "down " : "up   ") << typeid (c).name() << " " << p.toString() << std::endl;
        if (peer == nullptr) return;
        setFakePointer (peer->localToGlobal (p));
        const auto mods = keys.withFlags (buttonDown ? juce::ModifierKeys::leftButtonModifier : 0);
        juce::ModifierKeys::currentModifiers = mods;
        peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse, p, mods,
                                juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
                                nextEventTime (stepMs));
        juce::ModifierKeys::currentModifiers = keys;
    }

    void move (juce::Component& c, juce::Point<float> p)   { send (c, p, false, 20); }
    void down (juce::Component& c, juce::Point<float> p)   { send (c, p, true, 30); }
    void drag (juce::Component& c, juce::Point<float> p)   { send (c, p, true, 20); }
    void up (juce::Component& c, juce::Point<float> p)     { send (c, p, false, 30); }

    /* A whole click: the pointer arrives, presses and lets go. Clicks are spaced so two are never a double-click. */
    void click (juce::Component& c, juce::Point<float> p)
    {
        nextEventTime (1000);
        move (c, p.translated (0.f, 0.5f));
        move (c, p);
        down (c, p);
        up (c, p);
    }
    void click (juce::Component& c)   { click (c, c.getLocalBounds().getCentre().toFloat()); }

    void doubleClick (juce::Component& c, juce::Point<float> p)
    {
        nextEventTime (1000);
        move (c, p);
        down (c, p); up (c, p);
        down (c, p); up (c, p);
    }
    void doubleClick (juce::Component& c)   { doubleClick (c, c.getLocalBounds().getCentre().toFloat()); }

    /* Press at from, move to to in steps, release */
    void dragPath (juce::Component& c, juce::Point<float> from, juce::Point<float> to, int steps = 8)
    {
        nextEventTime (1000);
        move (c, from);
        down (c, from);
        for (int i = 1; i <= steps; ++i)
            drag (c, from + (to - from) * ((float) i / (float) steps));
        up (c, to);
    }

    /* Moves the pointer off everything, so the next click starts fresh */
    void leave (juce::Component& c)   { move (c, { -50.f, -50.f }); }
};

Mouse mouse;

/* A key press, as the keyboard sends it: to the focused component, then up through its parents */
bool pressKey (juce::Component& target, const juce::KeyPress& key)
{
    target.grabKeyboardFocus();
    if (juce::Component::getCurrentlyFocusedComponent() == &target)
        if (auto* peer = target.getPeer())
            return peer->handleKeyPress (key);

    if (! focusFallbackNoted)
    {
        note ("The system gives this off-screen window no keyboard focus, so key presses go straight to the focused field (and up through its parents).");
        focusFallbackNoted = true;
    }
    for (auto* c = &target; c != nullptr; c = c->getParentComponent())
        if (c->keyPressed (key))
            return true;
    return false;
}

void typeText (juce::Component& target, const juce::String& text)
{
    for (auto ch : text)
        pressKey (target, juce::KeyPress ((int) ch, {}, ch));
}

//==============================================================================
// Pop-up menus: the open menu windows, their items found by name and chosen through their own actions
//==============================================================================
void collectMenuItems (juce::Component& c, juce::Array<juce::Component*>& items)
{
    if (auto* h = c.getAccessibilityHandler())
        if (h->getRole() == juce::AccessibilityRole::menuItem)
            items.add (&c);
    for (auto* child : c.getChildren())
        collectMenuItems (*child, items);
}

juce::Array<juce::Component*> openMenus()
{
    juce::Array<juce::Component*> menus;
    auto& desktop = juce::Desktop::getInstance();
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        if (auto* c = desktop.getComponent (i))
            if (auto* h = c->getAccessibilityHandler())   // (a menu is made visible a moment after it opens)
                if (h->getRole() == juce::AccessibilityRole::popupMenu)
                    menus.add (c);
    return menus;
}

/* Waits (up to half a second) for a menu to open; ComboBoxes open theirs a moment after the click */
bool waitForMenu()
{
    for (int i = 0; i < 500 && openMenus().isEmpty(); ++i)
        pump (1);
    return ! openMenus().isEmpty();
}

juce::AccessibilityHandler* findMenuItem (const juce::String& title)
{
    for (auto* menu : openMenus())
    {
        juce::Array<juce::Component*> items;
        collectMenuItems (*menu, items);
        for (auto* item : items)
            if (auto* h = item->getAccessibilityHandler())
                if (h->getTitle() == title)
                    return h;
    }
    return nullptr;
}

juce::StringArray menuTitles()
{
    juce::StringArray titles;
    for (auto* menu : openMenus())
    {
        juce::Array<juce::Component*> items;
        collectMenuItems (*menu, items);
        for (auto* item : items)
            if (auto* h = item->getAccessibilityHandler())
                titles.add (h->getTitle());
    }
    return titles;
}

/* Chooses an item of the open menu (a submenu's name opens it). False if there is no such item. */
bool chooseMenuItem (const juce::String& title)
{
    if (auto* h = findMenuItem (title))
    {
        // the pretend pointer rests on the menu, as it would when choosing, so the menu doesn't close itself
        if (auto* menuWindow = h->getComponent().getTopLevelComponent())
            setFakePointer (menuWindow->getScreenBounds().getCentre().toFloat());
        const bool done = h->getActions().invoke (juce::AccessibilityActionType::press);
        return done;
    }
    return false;
}

bool menuItemTicked (const juce::String& title)
{
    if (auto* h = findMenuItem (title))
        return h->getCurrentState().isChecked();
    return false;
}

bool menuItemEnabled (const juce::String& title)
{
    if (auto* h = findMenuItem (title))
        return h->getActions().contains (juce::AccessibilityActionType::press);
    return false;
}

void closeMenus()
{
    juce::PopupMenu::dismissAllActiveMenus();
    pump (30);
}

//==============================================================================
// The host and the audio
//==============================================================================
struct HostClock : juce::AudioPlayHead
{
    double bpm = 120, rate = 48000, ppq = 0;
    bool playing = true;
    juce::int64 sample = 0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setTimeSignature (TimeSignature { 4, 4 });
        p.setIsPlaying (playing);
        p.setTimeInSamples (sample);
        p.setTimeInSeconds ((double) sample / rate);
        p.setPpqPosition (ppq);
        p.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
        return p;
    }
};

juce::RangedAudioParameter* param (juce::AudioProcessor& p, const juce::String& id)
{
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID() == id)
                return r;
    std::cout << "FAIL " << currentScenario << " the test asked for a parameter that doesn't exist: " << id << std::endl;
    std::_Exit (2);
}

float value (juce::AudioProcessor& p, const juce::String& id)
{
    auto* r = param (p, id);
    return r->convertFrom0to1 (r->getValue());
}

void setValue (juce::AudioProcessor& p, const juce::String& id, float v)   // only to set up a starting state
{
    auto* r = param (p, id);
    r->setValueNotifyingHost (r->convertTo0to1 (v));
}

/* A signal's level read every 2 ms, and what an LFO does to it */
struct Envelope
{
    std::vector<float> level;   // peak level of each 2 ms
    double rate = 500.0;   // readings a second (every 2 ms)

    float lowest() const   { return level.empty() ? 0.f : *std::min_element (level.begin(), level.end()); }
    float highest() const  { return level.empty() ? 0.f : *std::max_element (level.begin(), level.end()); }
    double mean() const    { double s = 0; for (auto v : level) s += v; return level.empty() ? 0 : s / (double) level.size(); }

    /* How deep the modulation is: 0 = steady, 1 = down to silence */
    double modulation() const   { return highest() > 1.0e-6f ? 1.0 - lowest() / highest() : 0.0; }

    /* The LFO's frequency, from how often the level crosses its middle */
    double frequency() const
    {
        const float lo = lowest(), hi = highest();
        if (hi - lo < 0.02f * hi) return 0;
        const float mid = 0.5f * (lo + hi), hysteresis = 0.1f * (hi - lo);
        std::vector<double> crossings;
        int state = level[0] > mid ? 1 : -1;
        for (size_t i = 1; i < level.size(); ++i)
        {
            if (state < 0 && level[i] > mid + hysteresis) { state = 1; crossings.push_back ((double) i); }
            else if (state > 0 && level[i] < mid - hysteresis) { state = -1; crossings.push_back ((double) i); }
        }
        if (crossings.size() < 3) return 0;
        return (double) (crossings.size() - 1) / 2.0 / ((crossings.back() - crossings.front()) / rate);
    }
};

//==============================================================================
// Pictures
//==============================================================================
juce::Image picture (juce::Component& c)
{
    return c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
}

void snapshot (juce::Component& c, const juce::String& name)
{
    auto image = picture (c);
    if (snapshotFolder == juce::File())
        return;
    snapshotFolder.createDirectory();
    auto file = snapshotFolder.getChildFile (currentScenario + "_" + name + ".png");
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat().writeImageToStream (image, out);
}

/* The average colour saturation of an area of a picture, 0 (grey) to 1 */
double saturation (const juce::Image& image, juce::Rectangle<int> area)
{
    area = area.getIntersection (image.getBounds());
    double sum = 0;
    int n = 0;
    const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);
    for (int y = area.getY(); y < area.getBottom(); y += 2)
        for (int x = area.getX(); x < area.getRight(); x += 2)
        {
            const auto c = data.getPixelColour (x, y);
            if (c.getBrightness() > 0.08f) { sum += c.getSaturation(); ++n; }
        }
    return n > 0 ? sum / n : 0.0;
}

int differingPixels (const juce::Image& a, const juce::Image& b, juce::Rectangle<int> area, int threshold = 6)
{
    area = area.getIntersection (a.getBounds()).getIntersection (b.getBounds());
    const juce::Image::BitmapData da (a, juce::Image::BitmapData::readOnly), db (b, juce::Image::BitmapData::readOnly);
    int n = 0;
    for (int y = area.getY(); y < area.getBottom(); ++y)
        for (int x = area.getX(); x < area.getRight(); ++x)
        {
            const auto ca = da.getPixelColour (x, y), cb = db.getPixelColour (x, y);
            if (std::abs (ca.getRed() - cb.getRed()) > threshold || std::abs (ca.getGreen() - cb.getGreen()) > threshold
                || std::abs (ca.getBlue() - cb.getBlue()) > threshold)
                ++n;
        }
    return n;
}

/* A component's area in another's coordinates (e.g. the editor's: the window is scaled, so this is where it really is) */
juce::Rectangle<int> areaIn (juce::Component& top, juce::Component& c)
{
    return top.getLocalArea (&c, c.getLocalBounds()).getSmallestIntegerContainer();
}

//==============================================================================
// Modal windows and waiting
//==============================================================================
/* The small modal window that just opened (an AlertWindow: a name to type, or a question). A window already answered can linger for a
   moment while it is being deleted, so only a new one that is showing and modal counts. */
juce::Component::SafePointer<juce::AlertWindow> lastAnswered;

juce::AlertWindow* modalAlertWindow()
{
    for (int i = 0; i < 300; ++i)
    {
        auto& mcm = *juce::ModalComponentManager::getInstance();
        for (int m = 0; m < mcm.getNumModalComponents(); ++m)
            if (auto* w = dynamic_cast<juce::AlertWindow*> (mcm.getModalComponent (m)))
                if (w != lastAnswered.getComponent() && w->isVisible() && w->isCurrentlyModal())
                    return w;
        pump (1);
    }
    return nullptr;
}

juce::Button* buttonCalled (juce::Component& parent, const juce::String& text)
{
    for (auto* c : parent.getChildren())
    {
        if (auto* b = dynamic_cast<juce::Button*> (c))
            if (b->getButtonText() == text)
                return b;
        if (auto* found = buttonCalled (*c, text))
            return found;
    }
    return nullptr;
}

bool waitFor (std::function<bool()> condition, int ms = 3000)
{
    for (int i = 0; i < ms / 10; ++i)
    {
        if (condition()) return true;
        pump (10);
    }
    return condition();
}

/* Somewhere off every screen, for the window under test */
juce::Point<int> offScreen()
{
    const auto all = juce::Desktop::getInstance().getDisplays().getTotalBounds (true);
    return { all.getRight() + 600, all.getY() + 40 };
}

/* Process memory, for the record */
juce::String processMemory()
{
   #if JUCE_WINDOWS
    PROCESS_MEMORY_COUNTERS c {};
    if (GetProcessMemoryInfo (GetCurrentProcess(), &c, sizeof (c)))
        return juce::String ((double) c.WorkingSetSize / 1048576.0, 1) + " MB";
   #elif JUCE_MAC
    mach_task_basic_info info {};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info (mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info, &count) == KERN_SUCCESS)
        return juce::String ((double) info.resident_size / 1048576.0, 1) + " MB";
   #endif
    return "n/a";
}


//==============================================================================
// The run
//==============================================================================
/* What the program around the scenarios supplies */
struct Suite
{
    juce::String title;                                   // printed first
    std::vector<Scenario> scenarios;
    std::function<void()> beforeAll, afterAll;            // e.g. make and remove a temp folder
    std::function<bool (const juce::String& option, const juce::String& value)> option;   // the suite's own --options
};

/* The whole run: options, the pretend pointer and focus, each scenario (a crash or a hang names it), a summary.
   Returns the exit code: 0 when nothing failed. */
int run (int argc, char* argv[], Suite suite)
{
   #if JUCE_WINDOWS
    SetUnhandledExceptionFilter (onWindowsCrash);
   #else
    std::signal (SIGSEGV, onSignal);
    std::signal (SIGBUS, onSignal);
    std::signal (SIGABRT, onSignal);
   #endif
    registered = suite.scenarios;

    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);
    auto opt = [&] (const juce::String& name) { const int i = args.indexOf (name); return i >= 0 && i + 1 < args.size() ? args[i + 1] : juce::String(); };

    if (args.contains ("--list"))
    {
        for (auto& s : registered)
            std::cout << s.id << "  [" << s.plan << "]  " << s.title << std::endl;
        return 0;
    }
    if (opt ("--snapshots").isNotEmpty()) snapshotFolder = juce::File::getCurrentWorkingDirectory().getChildFile (opt ("--snapshots"));
    if (opt ("--timeout").isNotEmpty()) timeoutSeconds = juce::jmax (5, opt ("--timeout").getIntValue());
    if (args.contains ("--no-poison")) poisonFrees = false;
    traceMouse = args.contains ("--trace");
    for (int i = 0; i < args.size(); ++i)
        if (args[i].startsWith ("--") && suite.option)
            suite.option (args[i], i + 1 < args.size() ? args[i + 1] : juce::String());
    juce::StringArray only;
    only.addTokens (opt ("--only"), ",", "");
    only.removeEmptyStrings();

   #if JUCE_WINDOWS
    DuplicateHandle (GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &mainThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
   #endif
    juce::ScopedJuceInitialiser_GUI gui;
    Watchdog watchdog;
    const auto started = juce::Time::getMillisecondCounterHiRes();
    std::cout << suite.title << std::endl;
    if (suite.beforeAll) suite.beforeAll();

    setScenario ("U-00");
    fakePointerWorks = installFakePointer();
    if (! fakePointerWorks)
        std::cout << "INFO U-00  the pretend pointer couldn't be installed on this system; hover-dependent checks may fail" << std::endl;

    for (auto& s : registered)
    {
        if (! only.isEmpty())
        {
            bool wanted = false;
            for (auto& o : only)
                if (s.id == o || s.plan.contains (o)) wanted = true;
            if (! wanted) continue;
        }
        setScenario (s.id);
        scenarioStartMs = juce::Time::currentTimeMillis();
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        std::cout << "---- " << s.id << "  [" << s.plan << "]  " << s.title << std::endl;
        {
            AutoreleasePool pool;
            s.run();
            closeMenus();
        }
        std::cout << "     " << s.id << " took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 1) << " s" << std::endl;
    }
    scenarioStartMs = 0;
    if (suite.afterAll) suite.afterAll();

    std::cout << std::endl << "=== UI tests: " << passes << " passed, " << failures << " failed, "
              << juce::String ((juce::Time::getMillisecondCounterHiRes() - started) / 1000.0, 1) << " s ===" << std::endl;
    for (auto& f : failureLines)
        std::cout << f << std::endl;
    std::cout << (failures == 0 ? "ALL UI TESTS PASSED" : "UI TESTS FAILED: " + std::to_string (failures)) << std::endl;
    std::fflush (stdout);
    return failures == 0 ? 0 : 1;
}
} // namespace uirobot
