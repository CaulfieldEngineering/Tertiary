# Tertiary changelog

<!-- How this file works (Claude, 2026-10-30):
     This is the one source of the public changelog page, wonderlandaudio.com/changelog/. On a v* release tag the
     pipeline turns every entry up to that version into the page's changelog block (newest first) and checks it live.
     Format: "## x.y.z (YYYY-MM-DD)" or "## x.y.z (release date)" (the release tag's date, New York time), then "- " bullets; an optional "**Group**" line starts a group.
     A new entry is drafted by scripts/draft-changelog.mjs with a "<!-- draft" line under its heading. Joe approves it by
     deleting that line; the pipeline refuses to release a version whose entry still has it.
     Comments like this one are never published. -->

## 1.1.1 (release date)
- Presets: new factory presets that set each band apart, each band with its own shape, rhythm and phase, and a few that keep one part of the sound still while the rest moves.
- The web demo follows each track's tempo, so synced settings land on the beat, and it starts with one click on Play.
- Bug fixes and stability improvements.

## 1.1.0 (2026-11-03)
- Mac: runs natively on Apple Silicon and Intel Macs, on macOS 11 (Big Sur) and later, and now comes as an Audio Unit (AU) as well as a VST3.
- Presets: a preset bar at the top of the window with 14 factory presets, plus your own presets, shared by the VST3, AU and standalone app. Your DAW's preset list shows them too, and sessions from 1.0 open with all their settings.
- A redesigned window with a new menu, drawn sharp at four sizes (100, 125, 150 and 200%). The scope zooms smoothly, follows tempo and rate changes as you make them, and greys out a bypassed band.
- Your serial key is checked with WonderlandAudio.com about once a week, and Tertiary keeps working offline. Activated copies show a link when an update is out.
- Windows: the installer, the plugin and the standalone app are now signed.
- Bug fixes and stability improvements.

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
