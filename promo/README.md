# Tertiary promo material

Instagram material demoing Tertiary, made 2026-10-26 (Joe: "Instagram friendly promotional material demoing the
product", no AI voice). The output lives in `Dropbox\Caulfield Engineering\Tertiary\Promo 2026-10-26`.

How it is made, so every frame and every sound is the real thing:

- **Music:** original MIDI (`compose.py`, seeded): four short 4/4 pieces, each chosen to show one use of Tertiary. They play in the Duet project "Tertiary Promo" through the instruments Octavio already had (SSD5, Kontakt bass and clean guitar, HALion pad), copied over with their sounds by `setup-project.mjs`.
- **Tertiary:** `plugin/review/ReviewRender.cpp`, built only with `-DWLA_REVIEW_RENDER=ON` into `build-review/`. It runs Tertiary's own processor and editor offline as the full version, with a host clock at the piece's tempo so synced rates lock to the music, and changes settings on a schedule. The editor is drawn at 30 fps from the same run, so the knobs move in the picture exactly when the sound changes.
- **Reels:** `reels.py` (1080x1920, 23 to 30 s, -14 LUFS, true peak -1 dBTP). **Carousel:** `carousel.py` (five 1080x1350 slides).

## Re-render after changing instruments

1. In Duet, open "Tertiary Promo" and change the instrument or sound on PAD, BASS, GTR or DRUMS (for example a Kontakt library).
2. `python promo/make_promo.py` (about 15 minutes). It renders the stems through the mini PC's engine and rebuilds everything in the Dropbox folder.

To build the tool the first time (python3 must be on PATH for `createSourceList.py`):

    cmake -S . -B build-review -G "Visual Studio 17 2022" -A x64 -DWLA_REVIEW_RENDER=ON
    cmake --build build-review --config Release --target ReviewRender

`ReviewRender --list` prints every parameter, its value and a choice's options.

## v2 (2026-10-27)

Joe: more movement, a camera on the UI, a better tagline, dynamic text, no "Joe Caulfield", the Wonderland Audio logo outside the UI; then: keep the AI tells low, and no sampled guitar (guitar only from real recordings). Output: `Dropbox\Caulfield Engineering\Tertiary\Promo 2026-10-27 v2`.

- `reels_v2.py`: crossover sweeps, slider rides, rhythm changes on the beat, solo and bypass, all as ReviewRender ramps so picture and sound stay locked. The editor is drawn at 3x and a camera eases to the control that moves, with a soft veil over the rest. Captions land on the beat and some show the value live. The logo is `plugin/images/CompanyTitle.png`. Reels 02 and 03 play the arpeggio and stabs on electric piano (the KEYS track in "Tertiary Promo"; the sampled-guitar GTR track is muted). v1's 02 and 03 used Shreddage. `make_promo_v2.py` renders the stems and the Reels.
- `carousel_v2.py`: the same rules for the five slides.

## Web demo tracks (2026-10-27)

- `web_tracks.py`: dry, looping, -18 LUFS, 16-bit 44.1 kHz tracks for `plugin/sample_audio/` (the live demo's playlist). `--held` renders the MIDI tracks from the Duet project "Tertiary Web Demo" once their instruments are loaded (Kontakt Factory Selection 2, Analog Dreams, GroveBass).
- `web_midi.py`: their original MIDI, written to `promo/web_midi/` (source only; never shipped) (three pads, electric piano, organ, strings, an ambient swell; no drums, no Octavio material, Joe 2026-10-27), each loop written three times so the render keeps a middle pass with real tails.
- `web_test.mjs`: plays every track of a local copy of the web demo in headless Chrome (`node web_test.mjs <url> <profile dir>`, with Wonderlab's `scripts/serve.js` serving it).

## v3 (2026-10-27)

Joe: "Can we emphasize showing what happens when the parameter is changed? It seems like right now, the effect is engaged - or the parameter changed - right as or during a pan/zoom. So its hard to tell exactly what is being done or showcased in that moment." and "let's end the video on a musical resolve - currently just looks like its getting cut off". Output: `Dropbox\Caulfield Engineering\Tertiary\Promo 2026-10-27 v3`.

- `reels_v3.py`: the `Plan` class schedules each change as its own beat. The camera eases to the control and settles, the old sound holds (1 to 2 beats), the caption comes up, the control moves with the camera still, the new sound holds (2 beats or more), and only then does the camera move on (the last caption leaves as it does). Every camera move and change goes into `work/v3/timing.json`, and the build stops if any overlap.
- The music is arranged backwards from the plan in the Duet project "Tertiary Promo v3": each piece plays its 8 bars, then its last few bars again so it reaches its V chord, then a written ending on the tonic that lands on the downbeat as the end card comes up and rings out. Reel 05 is a recording, so its end card comes up on the take's own last strum.
- The end card shows Tertiary 1.1's formats (Mac, Apple Silicon and Intel: VST3, AU and standalone; Windows: VST3 and standalone); `End cards` in the output folder also has a 1.0.2 fallback. `carousel_v3.py` updates slide 5 to match. Reel 05 is marked "(not for posting)": the acoustic take's rights are unknown.
- The same two rules are in WLA-Playbook chapter 14 (Demo Videos), 20b and 20c, for every product video.

## v4 (2026-10-27): for paid ads

Joe: "I want a broadband synth/pad whose performance is kind of static, where the rhythm is primarily invented by the tremolo. Showcasing all frequency bands. We really need to make it interesting to be worthy of paid ads.", "the text should be Multiband Tremolo not 'A tremolo, split into three bands'", "a lot of coverage per setting would be ideal", and "low frequencies tend to need more depth to be audible especially on mobile devices". Output: `Dropbox\Caulfield Engineering\Tertiary\Promo 2026-10-27 v4`.

- `reels_v4.py`: four held, wide beds on the Content Rig (Duet project "Tertiary Promo v4"): dark drone, bright shimmer, warm keys and slow texture. Upper voices enter as each bed builds, there's a CC11 swell, and each ends on its tonic. The low layer carries a 100 to 300 Hz body note and the pads have light tube drive, so the low pulse is heard on a phone.
- Each Reel opens on a Tertiary 1.1 factory preset (the preset bar shows it). It makes three or four changes, each held about two bars, with the low band at full depth. v3's camera and ending rules apply.
- The UI comes from Tertiary 1.1 with the preset bar: ReviewRender is built from `claude/presets` (56d1041) in the worktree `Tertiary-promo-presets`, with `--preset <name>` (also added here).
- Checked through a phone-speaker simulation (180 Hz high-pass, presence bump, top roll-off): the low band's pulse still swings 18 to 27 dB there with Tertiary on, against 13 to 16 dB bypassed.

## v5 (2026-10-27): more musical

Joe on v4: "Dark drone is garbage. It sounds like white noise.", "I like the approach but lets make the samples slightly more musical still.", and on the captions: "don't forget to include like an introductory sentence ... Remember to sell the thing. Also mention that you can try it for free directly in the browser". Output: `Dropbox\Caulfield Engineering\Tertiary\Promo 2026-10-27 v5`.

- `reels_v5.py`: three beds (Bright shimmer, Warm keys, Organ), each a real progression of eight or so chords that resolves, voiced for the instrument, with a slow top line. The dark drone and the slow texture are gone, and the String Melody patch was tried and dropped; all three were noisy by spectral flatness. Each bed is checked for pitch clarity (energy in sharp spectral peaks, 93 to 96%).
- Each Reel's first change sweeps both crossovers to 250 Hz and 900 Hz, which balances the bands (soloed lows and mids 3 to 7 dB under the whole) and keeps the low pulse on a phone. The pads were dark, so the bright one is doubled by Factory Selection 2's Chrystal pad and the electric piano plays the upper voices.
- The end card adds "Try it free in your browser" and wonderlandaudio.com.
