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

- `reels_v2.py`: crossover sweeps, slider rides, rhythm changes on the beat, solo and bypass, all as ReviewRender ramps so picture and sound stay locked. The editor is drawn at 3x and a camera eases to the control that moves, with a soft veil over the rest. Captions land on the beat and some show the value live. The logo is `plugin/images/CompanyTitle.png`. Reels 02 and 03 use Joe's DI (2024-09-17, GTR 1_01 and GTR 1_02) from the Duet project "Tertiary Web Demo"; v1's 02 and 03 used Shreddage.
- `carousel_v2.py`: the same rules for the five slides.

## Web demo tracks (2026-10-27)

- `web_tracks.py`: dry, looping, -18 LUFS, 16-bit 44.1 kHz tracks for `plugin/sample_audio/` (the live demo's playlist). `--held` renders the MIDI tracks from the Duet project "Tertiary Web Demo" once their instruments are loaded (Kontakt Factory Selection 2, Analog Dreams, GroveBass).
- `web_midi.py`: their original MIDI (three pads, electric piano, organ, strings, an ambient swell; no drums, no Octavio material, Joe 2026-10-27), each loop written three times so the render keeps a middle pass with real tails.
- `web_test.mjs`: plays every track of a local copy of the web demo in headless Chrome (`node web_test.mjs <url> <profile dir>`, with Wonderlab's `scripts/serve.js` serving it).
