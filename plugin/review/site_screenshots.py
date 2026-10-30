# site_screenshots.py
# Written by Claude (2026-10-30) for the wonderlandaudio.com screenshots (Joe: "We also need to update the screenshots
# to account for the new UI"). Called by site-screenshots.ps1; it can also be run on its own.
#
#   python site_screenshots.py <ReviewRender.exe> <sample.wav> <output folder>
#
# Renders Tertiary's editor offline with ReviewRender (full version, so no DEMO tag), with audio playing so the
# spectrum and meters are live, at 2x (WLA_UISCALE=2.0: the 750 x 478 window is drawn at 1500 x 956), then writes
# the two images the home page uses:
#
#   tertiary-main-window.png / .webp          the whole window          1500 x 956   (Hero > Hero - Image)
#   tertiary-scope-and-crossover-v2.png/.webp  the scope and crossover  1500 x 404   (Features > Scope and crossover)
#
# Change the settings below for the next UI change, run site-screenshots.ps1, and upload the .webp files.

import os, shutil, subprocess, sys, tempfile

PRESET = "Three Rhythms"      # a factory preset: triangle LFOs at 1/4, 1/4 triplet and 1/8 (three visibly different rates)
BPM = "100"
START, SECONDS = "4", "6"     # seconds into the sample, and how long to play before the still is taken
UI_SCALE = "2.0"              # 2x for retina; the widest the site shows either image is 739 CSS px
FRAME = "00006.png"           # the frame at 6 s (fps 1), after the meters and spectrum have settled
# The scope and crossover strip: the full width, from just under the header to just above the controls
# (rows 106 to 510 at 2x; the frame lines at 102-105 and 510-515 are left out, as in the old crop).
SCOPE_BOX = (0, 106, 1500, 510)


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__ or "usage: site_screenshots.py <ReviewRender.exe> <sample.wav> <out dir>")
    exe, sample, out = sys.argv[1:]
    from PIL import Image  # pip install pillow

    os.makedirs(out, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="tertiary-site-")
    frames = os.path.join(tmp, "frames")
    env = dict(os.environ, WLA_UISCALE=UI_SCALE)
    env.pop("WLA_FOCUS", None)  # nothing hovered, no menus or About window open
    subprocess.run([exe, "--in", sample, "--out", os.path.join(tmp, "render.wav"), "--start", START,
                    "--seconds", SECONDS, "--bpm", BPM, "--preset", PRESET,
                    "--frames", frames, "--fps", "1", "--scale", "1"], check=True, env=env)

    whole = Image.open(os.path.join(frames, FRAME)).convert("RGB")
    if whole.size != (1500, 956):
        print("note: the window is now", whole.size, "- check SCOPE_BOX")
    scope = whole.crop(SCOPE_BOX)
    for name, im in (("tertiary-main-window", whole), ("tertiary-scope-and-crossover-v2", scope)):
        im.save(os.path.join(out, name + ".png"), optimize=True)
        im.save(os.path.join(out, name + ".webp"), quality=92, method=6)
        print(name, im.size)
    shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
