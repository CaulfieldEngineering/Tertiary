# Tertiary changelog

## 1.1.0 (not released yet)

**Mac**
- Runs natively on Apple Silicon and Intel Macs (one universal build).
- Runs on macOS 11 (Big Sur) and later. 1.0.2 needed macOS 15.
- New: Audio Unit (AU) version, for Logic Pro and GarageBand. The Mac installer now installs VST3, AU and the standalone app.

**Presets**
- A preset bar at the top left: the preset's name, arrows for the previous and next preset, and a menu.
- 14 factory presets: Default, Gentle Movement, Amp Tremolo, Slow Swell, Low End Pump, High Shimmer, Hard Chop, Alternating Chop, Sixteenth Gate, Three Rhythms, Rolling Triplets, Half-Time Sway, Free Flutter, Midrange Chop.
- Save your own presets (Save, Save As, Rename, Delete). They are files in Documents/Wonderland Audio/Tertiary/Presets, shared by the VST3, AU and standalone versions. A * after the name shows when the settings differ from the preset.
- Your DAW's preset list shows the same presets, and a session remembers which preset was loaded. Sessions from 1.0 load exactly as before.

**Fixes**
- Opening the plugin window moved the Mid-High crossover from 1000 Hz to about 999 Hz. Fixed.
- Relative phase now does what the knob and the scope show: 90 degrees shifts a band's LFO by a quarter of a cycle. Before, the sound got twice the shift (90 degrees gave half a cycle). **Sessions and presets saved with a relative phase other than 0 will sound different in 1.1**: they now match the knob. To get the old sound back, double the phase value (for example 45 becomes 90).
- After a change of sample rate, the LFOs ran at the wrong speed until a control was moved. Fixed.
- Tempo-synced LFOs now follow tempo changes in the host. Before, they kept the old tempo until a control was moved.
- Re-syncing to the host grid (on play, or when the rhythm changes) used the position of the previous audio block. It now uses the current one.
- Saving a session right after changing a control could save the previous value. Fixed.
- Steadier in hosts that change the audio block size while playing (no memory is allocated while processing audio).
- Activating your serial key during the demo's 10-second pause left the effect and the band controls off until the plugin was reopened. Fixed.
- Closing the plugin window while an activation was in progress could crash the host. Fixed.
- Automating the scope's display options from the host could crash the plugin window. Fixed.
- Mac standalone app: macOS gave it no sound from the audio interface (it never asked for permission to use the audio input). It now asks the first time it opens an input.
- Mac installer: if another copy of Tertiary was on the Mac (an old download, a copied plug-in), the installer could update that copy instead of installing into Applications and the plug-in folders. It now always installs into the standard folders.
- Windows installer: a silent install (/VERYSILENT, for managed or scripted installs) waited forever on the final message. Fixed.
- Email addresses with capital letters were refused at activation. Fixed.

**Serial keys and updates**
- Your key is checked with WonderlandAudio.com about once a week. Tertiary keeps working offline; if it hasn't been able to check for 30 days, the About window says so, and it still keeps working.
- A key that has been refunded or cancelled returns the plugin to demo mode, with a message saying why.
- Each computer is now identified by its own machine ID, so two identical computers count as two activations.
- When a new version is out, a small "Version X is available" link appears at the top of the plugin. Nothing is downloaded automatically.

## 1.0.2 (2026-01-28)
- Windows: the plugin no longer needs the Visual C++ runtime installed.
- Windows: uses the software renderer, fixing missing or corrupted drawing on some older Intel graphics.
- The input and output meters' LED steps are drawn more reliably.

## 1.0.1 (2026-01-26)
- Windows: 64-bit only, as modern hosts require.
- VST3 category set to Modulation, so hosts list Tertiary in the right place.
- The top banner's text and images scale correctly with the display scaling.

## 1.0.0 (2025-03-30)
- First release: three-band tremolo with a Linkwitz-Riley crossover, seven LFO shapes per band with skew, depth, invert, relative phase, free rate or tempo sync, per-band gain, solo, mute and bypass, an LFO scope and a spectrum display. VST3 and standalone, Windows and Mac. Demo version with serial-key activation.
