# site_screenshots.py
# Written by Claude (2026-10-30) for the wonderlandaudio.com screenshots (Joe: "We also need to update the screenshots
# to account for the new UI"). Called by site-screenshots.ps1; it can also be run on its own.
#
#   python site_screenshots.py <ReviewRender.exe> <sample.wav> <output folder>
#
# Renders Tertiary's editor offline with ReviewRender (full version, so no DEMO tag), with audio playing so the
# spectrum and meters are live, then writes the images the home page uses:
#
#   tertiary-main-window.png                      the whole window, as rendered (the master)
#   tertiary-main-window-1x.webp / -2x.webp        Hero > Hero - Image, at the exact size shown (HERO_1X) and double
#   tertiary-scope-and-crossover-1x.webp / -2x.webp  Features > Scope and crossover, likewise (SCOPE_1X)
#
# The .webp files are lossless and made for the exact size the page draws them at: the page serves them from an
# HTML widget with a <picture>, which Jetpack's image CDN does not resize (see the site notes, CHANGES-2026-10-30.md,
# "Demo button colour, sharp UI pictures"). Change the settings below for the next UI change, run
# site-screenshots.ps1, and upload the .webp files.
#
# [Claude 2026-10-30, afternoon] Joe: "Can we do a screenshot with some different LFO shapes? To demonstrate. Sine,
# square, triangle?", "And a slightly wider mid-band crossover range", "Could you make the screenshot bigger?".
# So: SETTINGS (shapes and crossovers on top of the preset), a 2.5x render (the most WLA_UISCALE allows) so the
# bigger hero's 2x file is a downscale, and the exact-size 1x/2x pairs written here instead of by hand.

import os, shutil, subprocess, sys, tempfile

PRESET = "Three Rhythms"      # factory preset: 80 % depth, rhythms 1/4, 1/4 triplet and 1/8 (three visibly different
                              # rates). The settings below change it, so the preset box reads "Three Rhythms *".
SETTINGS = [                  # --set name=value, in the parameter's own units or a choice's text
    "Waveshape Low Band=Sine",
    "Waveshape Mid Band=Square",
    "Waveshape High Band=Triangle",
    "Low-Mid Crossover Freq=250",    # 250 Hz to 2.5 kHz: the mid band is one decade wide on the log axis,
    "Mid-High Crossover Freq=2500",  # about the same as the low (20-250) and the high (2.5k-20k)
]
BPM = "100"
START, SECONDS = "4", "6"     # seconds into the sample, and how long to play before the still is taken
UI_SCALE = "2.5"              # the editor's 750 x 478 window drawn at 1875 x 1195 (2.5 is the most WLA_UISCALE allows)
FRAME = "00006.png"           # the frame at 6 s (fps 1), after the meters and spectrum have settled
# The scope and crossover strip: the full width, from just under the header to just above the controls
# (rows 106 to 510 of the window at 2x, so 265 to 1275 halves at 2.5x; the frame lines either side are left out).
SCOPE_ROWS_AT_1X = (53, 255)  # in the window's own 750-wide pixels
# Sizes the home page draws them at (CSS px, inside the 1 px border). 2x files are double.
HERO_1X = (823, 525)          # hero image column 825 px at desktop widths (1300 px box, 34/66, 50 px gap), less the border
SCOPE_1X = (737, 199)


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: site_screenshots.py <ReviewRender.exe> <sample.wav> <out dir>")
    exe, sample, out = sys.argv[1:]
    from PIL import Image  # pip install pillow

    os.makedirs(out, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="tertiary-site-")
    frames = os.path.join(tmp, "frames")
    env = dict(os.environ, WLA_UISCALE=UI_SCALE)
    env.pop("WLA_FOCUS", None)  # nothing hovered, no menus or About window open
    cmd = [exe, "--in", sample, "--out", os.path.join(tmp, "render.wav"), "--start", START,
           "--seconds", SECONDS, "--bpm", BPM, "--preset", PRESET]
    for s in SETTINGS:
        cmd += ["--set", s]
    cmd += ["--frames", frames, "--fps", "1", "--scale", "1"]
    subprocess.run(cmd, check=True, env=env)

    whole = Image.open(os.path.join(frames, FRAME)).convert("RGB")
    k = whole.width / 750.0
    if whole.size != (round(750 * k), round(478 * k)):
        print("note: the window is now", whole.size, "- check SCOPE_ROWS_AT_1X and the sizes")
    whole.save(os.path.join(out, "tertiary-main-window.png"), optimize=True)
    top, bottom = SCOPE_ROWS_AT_1X
    scope = whole.crop((0, round(top * k), whole.width, round(bottom * k)))

    for name, im, size in (("tertiary-main-window", whole, HERO_1X), ("tertiary-scope-and-crossover", scope, SCOPE_1X)):
        for mult in (1, 2):
            w, h = size[0] * mult, size[1] * mult
            if w > im.width:
                print("warning: %s-%dx is %d px wide but the render is only %d; raise UI_SCALE" % (name, mult, w, im.width))
            im.resize((w, h), Image.LANCZOS).save(os.path.join(out, "%s-%dx.webp" % (name, mult)),
                                                  lossless=True, quality=100, method=6)
            print("%s-%dx.webp" % (name, mult), (w, h))
    shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
