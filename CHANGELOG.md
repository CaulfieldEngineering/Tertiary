# Tertiary changelog

<!-- How this file works (Claude, 2026-10-30):
     This is the one source of the public changelog page, wonderlandaudio.com/changelog/. On a v* release tag the
     pipeline turns every entry up to that version into the page's changelog block (newest first) and checks it live.
     Format: "## x.y.z (YYYY-MM-DD)" or "## x.y.z (release date)" (the release tag's date, New York time), then "- " bullets; an optional "**Group**" line starts a group.
     A new entry is drafted by scripts/draft-changelog.mjs with a "<!-- draft" line under its heading. Joe approves it by
     deleting that line; the pipeline refuses to release a version whose entry still has it.
     Comments like this one are never published. -->

## 1.1.0 (release date)
<!-- draft: written by scripts/draft-changelog.mjs on 2026-10-30. Joe approves it by deleting this line; the release pipeline won't publish it until then. -->
- Mac: runs natively on Apple Silicon and Intel Macs, on macOS 11 (Big Sur) and later, and now comes as an Audio Unit (AU) as well as a VST3. The installer always puts the VST3, AU and standalone app in the standard folders.
- Presets: a preset bar at the top of the window with 14 factory presets, plus your own presets saved in Documents/Wonderland Audio/Tertiary/Presets and shared by the VST3, AU and standalone app. Your DAW's preset list shows them too, and sessions from 1.0 open with all their settings.
- Relative phase now matches the knob and the scope: 90 degrees shifts a band by a quarter of a cycle, where before the sound got twice the shift. Sessions from 1.0 with a relative phase other than 0 will sound different; to get the old sound back, double the value, then take away 360 if it goes past 180 or add 360 if it goes past -180 (45 becomes 90, 120 becomes -120, -120 becomes 120).
- A redesigned window with a new menu, drawn sharp at four sizes (100, 125, 150 and 200%). The scope zooms smoothly, follows tempo and rate changes as you make them, and greys out a bypassed band.
- Tempo-synced LFOs stay on the beat: they follow tempo changes, return to the grid smoothly after a loop, a jump or a start mid-bar, and play from the start when the host begins before bar 1. LFOs keep the right speed after a change of sample rate, and a band that opens unsynced answers its rate control straight away.
- Band gain changes now glide instead of clicking, and a preset change while playing reaches every band. Saving right after moving a control keeps the new value, and opening the window no longer nudges the Mid-High crossover.
- Your serial key is checked with WonderlandAudio.com about once a week, Tertiary keeps working offline, and a refunded or cancelled key returns to demo mode with a message saying why. Activated copies show a "Version X is available" link when an update is out, and activating during the demo's pause turns the effect and its controls straight back on.
- Steadier in hosts that change the buffer size while playing, with no crash when the window closes during activation or when the host automates the scope's display options. On Windows the plugin and standalone app are now signed, and silent installs (/VERYSILENT) finish.

## 1.0.2 (2026-01-28)
**Windows**
- No longer needs the Microsoft Visual C++ runtime installed separately. Everything Tertiary needs is inside the plugin.
- Fixed missing or corrupted drawing in the plugin window on some older Intel graphics chips and drivers.

**All**
- Fixed the input and output level meters, which could misbehave at the bottom of their range.

## 1.0.1 (2026-01-26)
**Windows**
- The VST3 is now always built 64-bit. Some installs could end up with a 32-bit build, which Cubase and most current DAWs won't load.
- Fixed the top banner drawing at the wrong size on high-resolution and scaled displays.

**All**
- Tertiary now shows up under Modulation in DAWs that sort VST3 plugins by type.
- Built on a newer version of the JUCE framework.

## 1.0.0 (2025-03-30)
First release.
- Three-band tremolo with a continuous crossover from 20 Hz to 20 kHz, one LFO per band.
- Seven LFO shapes, INV, skew, depth, rhythm (tempo sync or 0.5 to 12.5 Hz), phase and gain per band. Solo, bypass and mute per band.
- Oscilloscope and crossover views with a real-time FFT.
- Sharp on high-resolution displays.
- One installer for both the demo and the full version, unlocked with a key. Up to five machines.
- VST3 and standalone for Windows and Mac. Signed installers on both; notarized on Mac.
