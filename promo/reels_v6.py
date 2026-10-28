"""Instagram Reels for Tertiary, v6 (v5 plus a waveshape change in each Reel and an opening hook). v5 (v4 made more musical: real progressions and a slow top line, no noisy patches). v4 (static wide pads, the rhythm all Tertiary's; see the v4 block). From v3: one change at a time, never during a camera move, and music arranged to end on
a resolve (see Plan and arrange()). From v2 (Joe, 2026-10-27: "more movement and engagement in the video itself. Some more slider
motion? Adjusting the crossover? Zooming or panning the UI itself to emphasize the features? A little better of a
tagline or maybe some dynamic text? Don't need to say "Joe Caulfield" and maybe we use the Wonderland Audio logo
outside of the UI as well.")

Each Reel is one continuous run of Tertiary (ReviewRender) over one of the original pieces. Settings move on a
schedule (crossover sweeps, slider rides, rhythm changes on the beat, solo, bypass), the editor is drawn at 3x from
the same run, and a camera eases across it to whatever is moving. Captions land on the beat and name what changes,
some with a live readout of the value. The logo is Tertiary's own CompanyTitle.png.

  python reels_v2.py <stems dir> <out dir> [reel name ...]
"""
import json
import math
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

STEMS = Path(sys.argv[1])
OUT = Path(sys.argv[2])
ONLY = set(sys.argv[3:])
HERE = Path(__file__).resolve().parent
WORK = HERE / "work" / "v6"
REPO = HERE.parent
# Built from Tertiary 1.1 with the preset bar (claude/presets, 56d1041), in its own worktree, with --preset added.
RR = str(REPO.parent / "Tertiary-promo-presets" / "build-review" / "plugin" / "ReviewRender_artefacts" / "Release" / "ReviewRender.exe")
FFMPEG = r"C:\Users\Joe\AppData\Local\Microsoft\WinGet\Packages\Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-8.1.2-full_build\bin\ffmpeg.exe"
ACOUSTIC = str(HERE / "work" / "web" / "src" / "Acoustic Guitar 96k.wav")   # the take as it was; it left the web demo
LOGO = Image.open(REPO / "plugin" / "images" / "CompanyTitle.png").convert("RGBA")
LOGO = LOGO.crop(LOGO.getbbox())
TITLE_IMG = Image.open(REPO / "plugin" / "images" / "PluginTitle.png").convert("RGBA")
TITLE_IMG = TITLE_IMG.crop(TITLE_IMG.getbbox())

# Picked from three (Joe 2026-10-27: keep the AI tells low; prefer what a working engineer would say). The others,
# "Three bands. Three tremolos." and "Move the highs. Keep the lows.", are in Captions.md.
TAGLINE = "Multiband Tremolo"   # Joe, 2026-10-27: "the text should be Multiband Tremolo not 'A tremolo, split into three bands'"
FPS, SCALE = 30, 3
W, H = 1080, 1920
VIEW = (20, 520, 1060, 1234)  # where the plugin sits on the canvas: 1040 x 714, the editor's own 750:515
F = r"C:\Windows\Fonts"
font = lambda name, size: ImageFont.truetype(f"{F}\\{name}", size)
TAG, CAP, VAL, SMALL, NAME = font("seguisb.ttf", 54), font("seguisb.ttf", 74), font("segoeui.ttf", 46), font("segoeui.ttf", 36), font("seguisb.ttf", 96)
TEXT, MUTED, FAINT = (240, 240, 244), (168, 168, 180), (110, 110, 124)
COLOUR = {"low": (110, 222, 110), "mid": (122, 122, 236), "high": (226, 110, 222), "xo": (235, 235, 240), "off": (150, 150, 160)}

# The camera's targets, in the editor's own 750 x 515 coordinates: a centre and a zoom.
# The rectangle is the control itself; everything else gets a soft dark veil while the camera is on it.
FULL = (375, 257.5, 1.0, None)
FREQ = (561, 152, 1.9, (379, 52, 745, 254))
SCOPE = (189, 152, 1.9, (7, 52, 372, 254))
LOW, MID, HIGH = (230, 382, 1.85, (114, 258, 313, 512)), (411, 382, 1.85, (315, 258, 512, 512)), (590, 382, 1.85, (514, 258, 712, 512))


def bars(bpm, n):
    return n * 4 * 60.0 / bpm


MULT = {"1/2": 0, "1/4": 1, "1/4T": 2, "1/8": 3, "1/8T": 4, "1/16": 5}
SHAPE = {"Ramp Down": 0, "Ramp Up": 1, "Square": 2, "Triangle": 3, "Sine": 4, "Hills": 5, "Valleys": 6}


class Reel:
    """One Reel's schedule: parameter moves (sound and picture), camera moves and captions, all on one clock."""

    def __init__(self, name, bpm, seconds, inputs, dry, sets):
        self.name, self.bpm, self.seconds, self.inputs, self.dry, self.sets = name, bpm, seconds, inputs, dry, sets
        self.args, self.cam, self.caps = [], [], []
        self.end = seconds - 2.8  # the end card

    def b(self, x):  # bars to seconds
        return bars(self.bpm, x)

    def step(self, pid, t, v):
        self.args += ["--ramp", f"{pid}:{t:.3f}:{t + 0.02:.3f}:{v}:{v}"]

    def ramp(self, pid, t0, t1, v0, v1):
        self.args += ["--ramp", f"{pid}:{t0:.3f}:{t1:.3f}:{v0}:{v1}"]

    def bypass(self, t, on, bands=("Low", "Mid", "High")):
        for x in bands:
            self.step(f"Bypassed {x} Band", t, 1 if on else 0)

    def look(self, t, target, dur=1.0):
        self.cam.append((t, dur, target))

    def say(self, t, text, colour="xo", value=None):
        """value: (t0, t1, v0, v1, format) shows the setting's value as it moves."""
        self.caps.append(dict(t=t, text=text, colour=colour, value=value))


def hz(v):
    return f"{v / 1000:.2f} kHz" if v >= 1000 else f"{v:.0f} Hz"


REELS = []

# ------------------------------------------------------------------------------------------------- v4: the tremolo is the rhythm
# Joe, 2026-10-27: "Can we try a few more types of audio for the promo? I want a broadband synth/pad whose performance
# is kind of static, where the rhythm is primarily invented by the tremolo. Showcasing all frequency bands. We really
# need to make it interesting to be worthy of paid ads." Each bed is a wide pad held almost still (low end, mids and
# air all present, chords changing slowly, voices entering as it builds); every bit of rhythm comes from Tertiary,
# each band on its own shape and subdivision. The Reel solos the lows, the mids and the highs in turn, plays the three
# together, changes one band so the groove moves, and ends on the tonic. v3's rules hold: one change at a time, the
# camera still while a control moves, and a musical resolve under the end card.
MOVE, SETTLE, CAPTION_LEAD = 1.0, 0.25, 0.35


class Plan:
    """Schedules a Reel's changes one at a time and logs every camera move and change, so overlaps can be checked."""

    def __init__(self, reel, intro_caption, colour="xo", intro=3.0, start=None):
        """intro_caption: a line, or [hook, second line]: the hook for the first 2.5 s, then the second. start: the
        opening view (FULL unless an option frames the scope or the band display)."""
        self.r, self.beat = reel, 60.0 / reel.bpm
        self.cur = start or FULL
        self.log = {"camera": [], "changes": []}
        reel.look(0, self.cur, 0.01)
        lines = intro_caption if isinstance(intro_caption, list) else [intro_caption]
        reel.say(0.3, lines[0], colour)
        if len(lines) > 1:
            reel.say(2.8, lines[1], colour)
            intro = max(intro, 2.8 + 2.4)
        self.t = intro

    def change(self, target, caption, colour, kind, *args, value=None, hold=2.5):
        """kind "ramp" (pid, v0, v1, seconds) or "step" ([(pid, value), ...]); hold: beats on the new sound."""
        r, t = self.r, self.t
        if target != self.cur:
            r.say(t, "", colour)
            r.look(t, target, MOVE)
            self.log["camera"].append((t, t + MOVE))
            t += MOVE + SETTLE
            self.cur = target
        t += max(1.0, 1.5 * self.beat)
        if kind == "ramp":
            pid, v0, v1, dur = args
            r.say(t, caption, colour, (t + CAPTION_LEAD, t + CAPTION_LEAD + dur, v0, v1, value) if value else None)
            t += CAPTION_LEAD
            r.ramp(pid, t, t + dur, v0, v1)
            self.log["changes"].append((t, t + dur, caption))
            t += dur
        elif kind == "ramps":   # several controls in one sweep (both crossovers), readout of the first
            moves, dur = args
            pid0, a0, b0 = moves[0]
            r.say(t, caption, colour, (t + CAPTION_LEAD, t + CAPTION_LEAD + dur, a0, b0, value) if value else None)
            t += CAPTION_LEAD
            for pid, v0, v1 in moves:
                r.ramp(pid, t, t + dur, v0, v1)
            self.log["changes"].append((t, t + dur, caption))
            t += dur
        else:
            r.say(t, caption, colour)
            t += CAPTION_LEAD
            for pid, v in args[0]:
                r.step(pid, t, v)
            self.log["changes"].append((t, t + 0.02, caption))
            t += 0.02
        t += max(1.5, hold * self.beat)
        self.t = t

    def check(self):
        bad = [(c, m) for c in self.log["changes"] for m in self.log["camera"] if c[0] < m[1] and m[0] < c[1]]
        assert not bad, f"{self.r.name}: the camera moves during a change: {bad}"
        return self.log


def hz(v):
    return f"{v / 1000:.2f} kHz" if v >= 1000 else f"{v:.0f} Hz"


# ------------------------------------------------------------------------------------------------- v6: ten options
# Joe, 2026-10-27: "Let's build 10 options overnight. I'll get better at feedback tomorrow, and not simply slot machine
# my requests to you." Every option has his two notes ("Can we show changing the wave shape as well?" and an opening
# hook: "the first line of copy at the bottom should still be something introductory - an immediate hook of sorts")
# and opens with a wide mid band ("let's start off with a wider mid-band"), set in the opening state, the lows at full
# depth. They vary on purpose by hook, sound, structure and framing, so each one teaches something when he reacts.
# Each option's bed is fitted to its plan: the progression's opening and its cadence are kept, the middle trimmed or
# its last bars repeated, and the tonic lands on the downbeat after the last hold.
BASE = {
    "bright": {"track": "PAD AIRY", "patch": "PAD AIRY", "bpm": 100, "name": "bright pad", "air": {"layer": 12, "melody": 12},
               "layer": {"track": "PAD CHRYSTAL", "patch": "PAD CHRYSTAL", "vel": 72, "layers": [1, 2]},
               "body": [("Eadd9", 2), ("G#m7", 2), ("C#m9", 2), ("Amaj7#11", 2), ("F#m11", 2), ("Dmaj9", 2), ("A/C#", 2), ("Bsus4", 1), ("B", 1)],
               "end": "Eadd9",
               "chords": {"Eadd9": [[28, 40, 47], [56, 59, 66], [71, 78]], "G#m7": [[32, 44, 51], [54, 59, 63], [66, 71]],
                          "C#m9": [[25, 37, 44], [52, 56, 59, 63], [68, 71]], "Amaj7#11": [[33, 45, 52], [56, 61, 63], [68, 75]],
                          "F#m11": [[30, 42, 49], [54, 57, 59, 64], [69, 73]], "Dmaj9": [[26, 38, 45], [54, 57, 61, 64], [69, 73]],
                          "A/C#": [[25, 37, 45], [52, 57, 61, 64], [69, 76]], "Bsus4": [[35, 47, 54], [59, 64, 66], [71, 76]],
                          "B": [[35, 47, 54], [59, 63, 66], [71, 75]]},
               "melody": [(0, 0, 68, 4), (1, 0, 66, 4), (2, 0, 68, 4), (3, 0, 71, 4), (4, 0, 68, 2), (4, 2, 64, 2), (5, 0, 63, 4),
                          (6, 0, 73, 4), (7, 0, 71, 4), (8, 0, 69, 4), (9, 0, 68, 2), (9, 2, 66, 2), (10, 0, 66, 4), (11, 0, 69, 4),
                          (12, 0, 69, 4), (13, 0, 69, 2), (13, 2, 71, 2), (14, 0, 71, 2), (14, 2, 69, 2), (15, 0, 63, 4)],
               "end_top": 64, "xo": (200, 2500)},
    "warm": {"track": "PAD WARM", "patch": "PAD WARM", "bpm": 84, "name": "warm keys", "air": {"layer": 12, "melody": 0},
             "layer": {"track": "EPIANO", "patch": "EPIANO", "vel": 66, "layers": [1, 2]}, "melody_track": "EPIANO",
             "body": [("Abmaj9", 2), ("Fm9", 2), ("Dbmaj9", 2), ("Bbm7", 1), ("Eb7sus4", 1), ("Cm7", 1), ("Fm7", 1), ("Dbmaj9", 1), ("Cm7", 1), ("Bbm9", 1), ("Eb7", 1)],
             "end": "Abmaj9",
             "chords": {"Abmaj9": [[32, 44, 51], [55, 58, 60, 63], [67, 70]], "Fm9": [[29, 41, 48], [55, 56, 60, 63], [67, 72]],
                        "Dbmaj9": [[25, 37, 44], [53, 56, 60, 63], [68, 72]], "Bbm7": [[34, 46, 53], [56, 61, 65], [68, 73]],
                        "Eb7sus4": [[27, 39, 46], [56, 58, 61, 63], [68, 70]], "Cm7": [[36, 48, 55], [58, 63, 67], [70, 75]],
                        "Fm7": [[29, 41, 48], [56, 60, 63], [68, 72]], "Bbm9": [[34, 46, 53], [56, 60, 61, 65], [68, 72]],
                        "Eb7": [[27, 39, 46], [55, 58, 61, 63], [67, 70]]},
             "melody": [(0, 0, 72, 2), (0, 2, 70, 2), (1, 0, 67, 4), (2, 0, 68, 2), (2, 2, 72, 2), (3, 0, 75, 4), (4, 0, 77, 2), (4, 2, 75, 2),
                        (5, 0, 72, 4), (6, 0, 73, 4), (7, 0, 70, 4), (8, 0, 67, 4), (9, 0, 68, 4), (10, 0, 72, 2), (10, 2, 68, 2),
                        (11, 0, 67, 4), (12, 0, 65, 2), (12, 2, 68, 2), (13, 0, 70, 2), (13, 2, 67, 2)],
             "end_top": 72, "xo": (200, 2500)},
    "organ": {"track": "ORGAN", "patch": "ORGAN", "bpm": 80, "name": "organ",
              "body": [("D", 2), ("Bm", 1), ("Bm/A", 1), ("Gmaj7", 2), ("Em7", 1), ("A7sus4", 1), ("F#m7", 1), ("Bm7", 1), ("Em7", 1), ("Gm6", 1), ("G", 1), ("A7", 1)],
              "end": "D",
              "chords": {"D": [[38, 50], [57, 62, 66], []], "Bm": [[35, 47], [54, 59, 62], []], "Bm/A": [[33, 45], [54, 59, 62], []],
                         "Gmaj7": [[31, 43], [54, 59, 62], []], "Em7": [[28, 40], [55, 59, 62], []], "A7sus4": [[33, 45], [55, 62, 64], []],
                         "F#m7": [[30, 42], [57, 61, 64], []], "Bm7": [[35, 47], [57, 62, 66], []], "G": [[31, 43], [55, 59, 62], []],
                         "A7": [[33, 45], [55, 61, 64], []], "Gm6": [[31, 43], [55, 58, 64], []]},
              "melody": [(0, 0, 69, 2), (0, 2, 74, 2), (1, 0, 78, 4), (2, 0, 78, 2), (2, 2, 74, 2), (3, 0, 73, 4), (4, 0, 71, 4),
                         (5, 0, 74, 2), (5, 2, 78, 2), (6, 0, 79, 2), (6, 2, 76, 2), (7, 0, 74, 4), (8, 0, 73, 2), (8, 2, 76, 2),
                         (9, 0, 74, 4), (10, 0, 71, 2), (10, 2, 67, 2), (11, 0, 70, 4), (12, 0, 71, 2), (12, 2, 74, 2), (13, 0, 74, 2), (13, 2, 73, 2)],
              "end_top": 74, "xo": (200, 3000)},
    # The approved Jazz Guitar (Factory Selection 2): one strum per chord, left to ring, real open-position shapes
    # (frets low E to high e: Gmaj7 3x443x, Em9 022002 as 0 2 0 0 0 2, Cmaj7 x32000, Dsus2 xx0230, Am7 x02010,
    # Bm7 x24232, D7sus4 xx0213, D7 xx0212), a few melody notes in the gaps.
    "guitar": {"track": "GUITAR", "patch": "GUITAR", "bpm": 90, "name": "jazz guitar", "no_body": True, "roll": 30,
               "body": [("Gmaj7", 2), ("Em9", 2), ("Cmaj7", 1), ("Dsus2", 1), ("Am7", 1), ("Bm7", 1), ("Cmaj7", 1), ("D7sus4", 1), ("D7", 1)],
               "end": "Gmaj7",
               "chords": {"Gmaj7": [[43, 54, 59, 62], [], []], "Em9": [[40, 47, 50, 55, 59, 66], [], []], "Cmaj7": [[48, 52, 55, 59, 64], [], []],
                          "Dsus2": [[50, 57, 62, 64], [], []], "Am7": [[45, 52, 55, 60, 64], [], []], "Bm7": [[47, 54, 57, 62, 66], [], []],
                          "D7sus4": [[50, 57, 60, 67], [], []], "D7": [[50, 57, 60, 66], [], []]},
               "melody": [(1, 2, 66, 1), (1, 3, 67, 1), (3, 3, 71, 1), (5, 2, 69, 2), (7, 3, 64, 1), (9, 2, 74, 1), (9, 3, 72, 1)],
               "end_top": 71, "xo": (200, 2500)},
    # Factory Selection 2's piano (Ragtime Piano, played gently): left hand roots, right hand voicings, a slow tune.
    "piano": {"track": "KEYS", "patch": "KEYS", "bpm": 80, "name": "piano", "no_body": True,
              "body": [("Cmaj9", 2), ("Am9", 2), ("Fmaj7", 1), ("G6", 1), ("Em7", 1), ("Am7", 1), ("Dm9", 1), ("G7sus4", 1), ("G7", 1)],
              "end": "Cmaj9",
              "chords": {"Cmaj9": [[36, 48], [59, 62, 64, 67], []], "Am9": [[33, 45], [55, 59, 60, 64], []], "Fmaj7": [[29, 41], [57, 60, 64], []],
                         "G6": [[31, 43], [59, 62, 64], []], "Em7": [[28, 40], [55, 59, 62], []], "Am7": [[33, 45], [55, 60, 64], []],
                         "Dm9": [[26, 38], [57, 60, 64, 65], []], "G7sus4": [[31, 43], [60, 62, 65], []], "G7": [[31, 43], [59, 62, 65], []]},
              "melody": [(0, 0, 76, 2), (0, 2, 74, 2), (1, 0, 71, 4), (2, 0, 72, 2), (2, 2, 76, 2), (3, 0, 79, 4), (4, 0, 77, 2),
                         (4, 2, 76, 2), (5, 0, 74, 4), (6, 0, 71, 2), (6, 2, 74, 2), (7, 0, 72, 4), (8, 0, 72, 2), (8, 2, 74, 2),
                         (9, 0, 72, 2), (9, 2, 71, 2), (10, 0, 71, 4)],
              "end_top": 72, "xo": (200, 2500)},
}
BEDS = {}

HOOKS = {"A": "Lows, mids, highs. Three tremolos. One plugin.",
         "B": "Tremolo, but each band moves on its own.",
         "C": "A separate tremolo for your lows, mids and highs.",
         "D": "Your lows, mids and highs, each on its own pulse.",
         "E": "One sound. Three bands. Three different pulses."}


def option(num, hook, base, preset, structure, tests, steps, start=None, hold=7.0, second_line=True, dry=False):
    """One option. steps: (view, caption, colour, kind, args...) for Plan.change; start: the opening view."""
    bd = BASE[base]
    lo, hi = bd["xo"]
    sets = ["FFT Pickoff Point=Output", f"Low-Mid Crossover Freq={lo}", f"Mid-High Crossover Freq={hi}", "Depth Low Band=100"]
    if dry:
        sets += [f"Bypassed {b} Band=1" for b in ("Low", "Mid", "High")]
    key = f"o{num:02d}"
    r = Reel(f"{num:02d} {base}", bd["bpm"], 0, [(f"{key}/mix.wav", 0)], [], sets)
    r.bed, r.preset, r.base, r.hook, r.structure, r.tests = key, preset, base, hook, structure, tests
    lines = [HOOKS[hook]] + ([f"Tertiary, factory preset {preset}"] if second_line else [])
    pl = Plan(r, lines, "xo", intro=(2.8 + 2.4) if second_line else 2.8, start=start)
    for view, caption, colour, kind, *args in steps:
        h = args.pop() if args and isinstance(args[-1], dict) else {}
        pl.change(view, caption, colour, kind, *args, hold=h.get("hold", hold), value=h.get("value"))
    r.plan = pl
    REELS.append(r)


DEPTH = lambda v: f"Depth {v:.0f}%"
option(1, "A", "bright", "Rolling Triplets", "standard, 4 changes", "the baseline: v5's pad with the hook and a shape change",
       [(MID, "Mids: sine to square", "mid", "step", [("Waveshape Mid Band", SHAPE["Square"])]),
        (MID, "Mids alone", "mid", "step", [("Solo Mid Band", 1)]),
        (FULL, "All three bands", "xo", "step", [("Solo Mid Band", 0)]),
        (LOW, "Lows to quarter notes", "low", "step", [("Multiplier Low Band", MULT["1/4"])])])
option(2, "B", "warm", "Half-Time Sway", "standard, 3 changes", "a plainer hook; the shape change on the lows",
       [(LOW, "Lows: sine to square", "low", "step", [("Waveshape Low Band", SHAPE["Square"])]),
        (MID, "Mids alone: a half-note sway", "mid", "step", [("Solo Mid Band", 1)]),
        (FULL, "All three bands", "xo", "step", [("Solo Mid Band", 0)])])
# With the mid band this wide (to 2.5 or 3 kHz) these sounds leave the high band 19 to 46 dB under the whole, so no
# option solos the highs or puts its shape change there; the lows and mids carry the changes.
option(3, "C", "organ", "Alternating Chop", "standard, 4 changes", "organ; the lows soloed",
       [(MID, "Mids: square to sine", "mid", "step", [("Waveshape Mid Band", SHAPE["Sine"])]),
        (LOW, "Lows alone: squares on eighths, full depth", "low", "step", [("Solo Low Band", 1)]),
        (FULL, "All three bands", "xo", "step", [("Solo Low Band", 0)]),
        (LOW, "Lows to quarter notes", "low", "step", [("Multiplier Low Band", MULT["1/4"])])])
option(4, "D", "guitar", "Hard Chop", "fast cut, 2 changes", "a short cut on the Jazz Guitar",
       [(MID, "Mids: square to triangle", "mid", "step", [("Waveshape Mid Band", SHAPE["Triangle"])], {"hold": 4.0}),
        (LOW, "Lows to quarter notes", "low", "step", [("Multiplier Low Band", MULT["1/4"])], {"hold": 4.0})], second_line=False)
option(5, "E", "piano", "Three Rhythms", "fast cut, 2 changes", "a short cut on piano",
       [(MID, "Mids: triangle to square", "mid", "step", [("Waveshape Mid Band", SHAPE["Square"])], {"hold": 4.0}),
        (LOW, "Lows to eighths", "low", "step", [("Multiplier Low Band", MULT["1/8"])], {"hold": 4.0})], second_line=False)
option(6, "B", "organ", "Alternating Chop", "bypass reveal, 3 changes", "starts on, turns off, so you hear what it adds",
       [(FULL, "Tertiary off", "off", "step", [(f"Bypassed {b} Band", 1) for b in ("Low", "Mid", "High")]),
        (FULL, "Tertiary on", "xo", "step", [(f"Bypassed {b} Band", 0) for b in ("Low", "Mid", "High")]),
        (MID, "Mids: square to sine", "mid", "step", [("Waveshape Mid Band", SHAPE["Sine"])])])
option(7, "A", "warm", "Half-Time Sway", "dry start, 3 changes", "starts dry, then turns on",
       [(FULL, "Tertiary on", "xo", "step", [(f"Bypassed {b} Band", 0) for b in ("Low", "Mid", "High")]),
        (LOW, "Lows: sine to square", "low", "step", [("Waveshape Low Band", SHAPE["Square"])]),
        (MID, "Mids to quarter notes", "mid", "step", [("Multiplier Mid Band", MULT["1/4"])])], dry=True)
option(8, "C", "bright", "Rolling Triplets", "long listen, 2 changes", "fewer changes, each held four bars",
       [(MID, "Mids: sine to triangle", "mid", "step", [("Waveshape Mid Band", SHAPE["Triangle"])], {"hold": 24.0}),
        (LOW, "Lows to quarter notes", "low", "step", [("Multiplier Low Band", MULT["1/4"])], {"hold": 24.0})])
option(9, "D", "organ", "Alternating Chop", "scope view, 2 changes", "framed on the scope: the shapes themselves",
       [(SCOPE, "Lows: square to triangle", "low", "step", [("Waveshape Low Band", SHAPE["Triangle"])]),
        (SCOPE, "Mids: square to sine", "mid", "step", [("Waveshape Mid Band", SHAPE["Sine"])])], start=SCOPE)
option(10, "E", "guitar", "Sixteenth Gate", "band display view, 3 changes", "framed on the band display and the crossovers",
       [(FREQ, "High band wider: crossover down", "xo", "ramp", "Mid-High Crossover Freq", 2500, 1200, 3.0, {"value": hz}),
        (MID, "Mids: ramp to sine", "mid", "step", [("Waveshape Mid Band", SHAPE["Sine"])]),
        (LOW, "Lows to eighths", "low", "step", [("Multiplier Low Band", MULT["1/8"])])], start=FREQ)
START = {r.bed: 1 + 40 * i for i, r in enumerate(REELS)}


def bar_map(L, n):
    """Which body bar each of the n bars plays: the opening two and the cadence kept with the middle trimmed, or the
    whole body followed by its last bars again when n is longer."""
    if n <= L:
        a = min(2, max(0, n - 2))
        return list(range(a)) + list(range(L - (n - a), L))
    out = list(range(L))
    while len(out) < n:
        k = min(L, n - len(out))
        out += list(range(L - k, L))
    return out


RING = 3.2   # the end card holds this long after the final chord, which rings and fades with its tail


def finish(reel):
    """The option's bed, fitted to its plan: the tonic on the first downbeat after the last hold."""
    log = reel.plan.check()
    bar = 240.0 / reel.bpm
    n = max(4, math.ceil(reel.plan.t / bar - 1e-6))
    base = dict(BASE[reel.base])
    body = [c for c, k in base["body"] for _ in range(k)]
    base.update(bars=n, map=bar_map(len(body), n), body_bars=body, base=reel.base)
    BEDS[reel.bed] = base
    reel.end = n * bar
    reel.seconds = reel.end + RING
    short = reel.structure.split(",")[0]
    reel.fname = f"{reel.name[:2]} Hook {reel.hook}, {reel.base}, {short}, {round(reel.seconds)}s"
    return log


# ------------------------------------------------------------------------------------------------- audio
def load(path, seconds, sr=48000):
    x, rate = sf.read(str(path), always_2d=True)
    if rate != sr:
        tmp = WORK / (Path(path).stem + f"-{sr}.wav")
        subprocess.run([FFMPEG, "-y", "-loglevel", "error", "-i", str(path), "-ar", str(sr), str(tmp)], check=True)
        x, rate = sf.read(str(tmp), always_2d=True)
    if x.shape[1] == 1:
        x = np.repeat(x, 2, axis=1)
    n = int(seconds * sr)
    if len(x) < n:
        x = np.vstack([x, np.zeros((n - len(x), 2))])
    return x[:n]


def loudnorm(src, dst):
    """Two-pass EBU R128 to -14 LUFS, true peak -1 dBTP."""
    r = subprocess.run([FFMPEG, "-hide_banner", "-i", str(src), "-af", "loudnorm=I=-14:TP=-1.2:LRA=11:print_format=json", "-f", "null", "-"],
                       capture_output=True, text=True)
    m = json.loads(r.stderr[r.stderr.rindex("{"):r.stderr.rindex("}") + 1])
    af = (f"loudnorm=I=-14:TP=-1.2:LRA=11:measured_I={m['input_i']}:measured_TP={m['input_tp']}:measured_LRA={m['input_lra']}"
          f":measured_thresh={m['input_thresh']}:offset={m['target_offset']}:linear=true")
    subprocess.run([FFMPEG, "-y", "-loglevel", "error", "-i", str(src), "-af", af, "-ar", "48000", str(dst)], check=True)


def measure(path):
    r = subprocess.run([FFMPEG, "-hide_banner", "-nostats", "-i", str(path), "-af", "ebur128=peak=true", "-f", "null", "-"],
                       capture_output=True, text=True)
    tail = r.stderr[r.stderr.rindex("Summary:"):]
    return float(tail.split("I:")[1].split("LUFS")[0]), float(tail.split("Peak:")[1].split("dBFS")[0])


# ------------------------------------------------------------------------------------------------- picture
def ease(a):
    a = min(1.0, max(0.0, a))
    return a * a * (3 - 2 * a) if a < 1 else 1.0


def smoother(a):  # ease in and out, gentler at both ends than smoothstep
    a = min(1.0, max(0.0, a))
    return a * a * a * (a * (a * 6 - 15) + 10)


def camera(reel, t):
    """Where the camera is at t: each move eases from wherever it was to its target."""
    pos = FULL[:3]
    for t0, dur, target in sorted(reel.cam, key=lambda c: c[0]):
        if t < t0:
            break
        a = smoother((t - t0) / dur)
        pos = tuple(p + (q - p) * a for p, q in zip(pos, target[:3]))
    return pos


def spotlight(reel, t):
    """[(rect, strength)]: the control the camera is on fades in, the one it left fades out."""
    looks = sorted(reel.cam, key=lambda c: c[0])
    cur = [i for i, c in enumerate(looks) if c[0] <= t]
    if not cur:
        return []
    k = cur[-1]
    t0, dur, target = looks[k]
    a = smoother((t - t0) / dur)
    out = [(target[3], a)]
    if k > 0:
        out.append((looks[k - 1][2][3], 1 - a))
    return [(r, s) for r, s in out if r is not None and s > 0.001]


def view_box(pos, uw, uh):
    cx, cy, z = pos
    s = uw / 750.0
    w, h = uw / z, uh / z
    x0 = min(max(cx * s - w / 2, 0), uw - w)
    y0 = min(max(cy * s - h / 2, 0), uh - h)
    return (x0, y0, x0 + w, y0 + h)


def background():
    return Image.new("RGB", (W, H), (14, 14, 16))


def fit(img, width):
    return img.resize((width, int(img.height * width / img.width)), Image.LANCZOS)


def with_alpha(img, a):
    if a >= 1:
        return img
    out = img.copy()
    out.putalpha(out.getchannel("A").point(lambda v: int(v * a)))
    return out


BG = background()
LOGO_MARK = fit(LOGO, 660)   # Joe, 2026-10-27: "can we make the logo up top larger?" (about 61% of the width, from the 4000 px original)
LOGO_END = fit(LOGO, 860)
TITLE_END = fit(TITLE_IMG, 640)
SHADOW = None


def text_center(d, y, s, f, fill):
    d.text(((W - d.textlength(s, font=f)) / 2, y), s, font=f, fill=fill)


def draw_tagline(img, reel, t):
    """The tagline, faded in over the first half second, then held."""
    d = ImageDraw.Draw(img, "RGBA")
    a = ease(t / 0.5)
    text_center(d, 356, TAGLINE, TAG, TEXT + (int(255 * a),))


def draw_caption(img, reel, t):
    caps = sorted(reel.caps, key=lambda c: c["t"])
    cur = [c for c in caps if c["t"] <= t]
    if not cur or t >= reel.end:
        return
    c = cur[-1]
    if not c["text"]:
        return
    nxt = next((n["t"] for n in caps if n["t"] > c["t"]), reel.end)
    a_in = ease((t - c["t"]) / 0.28)
    a_out = 1 - ease((t - (nxt - 0.18)) / 0.18)
    a = min(a_in, a_out)
    if a <= 0:
        return
    d = ImageDraw.Draw(img, "RGBA")
    y = 1300 + (1 - a_in) * 10
    col = COLOUR[c["colour"]]
    tw = d.textlength(c["text"], font=CAP)
    # a short bar in the band's colour above the words
    d.rounded_rectangle(((W - 64) / 2, y - 26, (W + 64) / 2, y - 18), 4, fill=col + (int(255 * a),))
    sentences = [x.strip() + ("." if not x.strip().endswith((".", "?")) else "") for x in c["text"].split(". ") if x.strip()]
    if len(sentences) > 1 and all(d.textlength(x, font=CAP) <= 940 for x in sentences):
        # break between sentences, joining short ones: "Lows, mids, highs." / "Three tremolos. One plugin."
        lines, cur = [], ""
        for x in sentences:
            if cur and d.textlength(cur + " " + x, font=CAP) > 940:
                lines.append(cur)
                cur = x
            else:
                cur = (cur + " " + x).strip()
        lines.append(cur)
        lines = [l[:-1] if l.endswith("..") else l for l in lines]
    else:
        lines, cur = [], ""
        for word in c["text"].split():  # wrap to 940 px
            if cur and d.textlength(cur + " " + word, font=CAP) > 940:
                lines.append(cur)
                cur = word
            else:
                cur = (cur + " " + word).strip()
        lines.append(cur)
    for line in lines:
        text_center(d, y, line, CAP, TEXT + (int(255 * a),))
        y += 88
    if c["value"]:
        t0, t1, v0, v1, fmt = c["value"]
        v = v0 + (v1 - v0) * min(1, max(0, (t - t0) / (t1 - t0)))
        text_center(d, y + 6, fmt(v), VAL, col + (int(255 * a),))


def draw_ui(img, ui, reel, t):
    uw, uh = ui.size
    box = view_box(camera(reel, t), uw, uh)
    vw, vh = VIEW[2] - VIEW[0], VIEW[3] - VIEW[1]
    shot = ui.resize((vw, vh), Image.BICUBIC, box=box, reducing_gap=2.0)
    spots = spotlight(reel, t)
    if spots:
        s = uw / 750.0
        veil = Image.new("L", (vw, vh), 0)
        for rect, strength in spots:
            layer = Image.new("L", (vw, vh), int(150 * strength))
            x0 = (rect[0] * s - box[0]) / (box[2] - box[0]) * vw
            x1 = (rect[2] * s - box[0]) / (box[2] - box[0]) * vw
            y0 = (rect[1] * s - box[1]) / (box[3] - box[1]) * vh
            y1 = (rect[3] * s - box[1]) / (box[3] - box[1]) * vh
            ImageDraw.Draw(layer).rounded_rectangle((x0, y0, x1, y1), 14, fill=0)
            veil = ImageChops.lighter(veil, layer)
        veil = veil.filter(ImageFilter.GaussianBlur(10))
        shot = Image.composite(Image.new("RGB", (vw, vh), (8, 8, 10)), shot, veil)
    img.paste(SHADOW, (VIEW[0] - 40, VIEW[1] - 40), SHADOW)
    m = Image.new("L", (vw, vh), 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, vw, vh), 18, fill=255)
    img.paste(shot, VIEW[:2], m)


# The formats on the end card, for the version the Reel is posted with (Joe, 2026-10-27: Tertiary 1.1 ships before the
# post, so 1.1's). FORMATS_1_0_2 is the fallback, true for 1.0.2 (no AU, no Apple Silicon note); see end_cards().
FORMATS_1_1 = ["Mac (Apple Silicon and Intel): VST3, AU and standalone", "Windows: VST3 and standalone"]
FORMATS_1_0_2 = ["Mac and Windows: VST3 and standalone"]
FORMATS = FORMATS_1_1


def end_card(formats):
    card = BG.copy().convert("RGBA")
    y = 600
    card.alpha_composite(LOGO_END, ((W - LOGO_END.width) // 2, y))
    y += LOGO_END.height + 110
    d = ImageDraw.Draw(card)
    text_center(d, y, "Tertiary", NAME, TEXT)
    y += 150
    text_center(d, y, TAGLINE, TAG, TEXT)
    y += 110
    for line in formats:
        text_center(d, y, line, SMALL, MUTED)
        y += 52
    y += 30   # Joe, 2026-10-27: "mention that you can try it for free directly in the browser"
    text_center(d, y, "Try it free in your browser", TAG, TEXT)
    text_center(d, y + 80, "wonderlandaudio.com", TAG, MUTED)
    return card.convert("RGB")


def end_cards(out):
    """The end card as stills: 1.1 (in the Reels) and the 1.0.2 fallback."""
    (out / "End cards").mkdir(parents=True, exist_ok=True)
    end_card(FORMATS_1_1).save(out / "End cards" / "End card, Tertiary 1.1.png")
    end_card(FORMATS_1_0_2).save(out / "End cards" / "End card, Tertiary 1.0.2 (fallback).png")


def draw_end(img, reel, t):
    a = ease((t - reel.end) / 0.5)
    if a <= 0:
        return img
    return Image.blend(img, END_CARD, a)


def frame(reel, ui, t):
    img = BG.copy()
    rgba = img.convert("RGBA")
    rgba.alpha_composite(LOGO_MARK, ((W - LOGO_MARK.width) // 2, 236))
    img = rgba.convert("RGB")
    draw_ui(img, ui, reel, t)
    draw_tagline(img, reel, t)
    draw_caption(img, reel, t)
    return draw_end(img, reel, t)


# ------------------------------------------------------------------------------------------------- build
SHAPES = ["Ramp Down", "Ramp Up", "Square", "Triangle", "Sine", "Hills", "Valleys"]
MULTS = ["1/2 Note", "1/4 Note", "1/4 Note Triplet", "1/8 Note", "1/8 Note Triplet", "1/16 Note"]


def expected_text(pid, v):
    """What the plugin should report for pid set to v (a choice index, a switch, or a number)."""
    if pid.startswith("Waveshape"):
        return SHAPES[int(v)]
    if pid.startswith("Multiplier"):
        return MULTS[int(v)]
    if pid.split()[0] in ("Bypassed", "Solo", "Mute", "Invert", "Sync"):
        return "On" if float(v) else "Off"
    return float(v)


def same(got, want):
    if isinstance(want, float):
        try:
            return abs(float(got) - want) <= max(1.0, abs(want) * 0.01)
        except ValueError:
            return False
    return got == want


def ui_checks(reel, f0, f1):
    """The first kept frame: crossover colours where they should be, the scope moving, the spectrum filled."""
    a = np.asarray(Image.open(f0).convert("RGB")).astype(float)
    b = np.asarray(Image.open(f1).convert("RGB")).astype(float)
    s = a.shape[1] / 750.0
    lo, hi = BASE[reel.base]["xo"]
    x_of = lambda f: (412 + 108.2 * math.log10(f / 20.0)) * s     # the band display's log axis (20 Hz to 20 kHz)
    y = 205 * s
    dry = any("Bypassed" in v and v.endswith("=1") for v in reel.sets)
    problems = []
    for name, f, test in (("low", 60, lambda c: c[1] > c[0] + 10 and c[1] > c[2]),
                          ("mid", math.sqrt(lo * hi), lambda c: c[2] > c[0] + 15 and c[2] > c[1] + 15),
                          ("high", min(15000, hi * 3), lambda c: c[0] > c[1] + 20 and c[2] > c[1] + 20)):
        c = a[int(y) - 4:int(y) + 4, int(x_of(f)) - 4:int(x_of(f)) + 4].reshape(-1, 3).mean(0)
        ok = (max(c) - min(c) < 30) if dry else test(c)
        if not ok:
            problems.append(f"{name} band colour at {f:.0f} Hz is {c.round()}")
    # the scope: each lane must hold a drawn waveform (a flat line covers only a few rows of its lane)
    for top, bottom in (() if dry else ((56, 115), (118, 175), (178, 235))):   # dry: bypassed bands draw no LFO
        lane = a[int(top * s):int(bottom * s), int(10 * s):int(368 * s)]
        lit = (lane.max(2) - lane.min(2) > 60) & (lane.max(2) > 120)
        rows = lit.any(1).mean()
        if rows < 0.35:
            problems.append(f"the scope lane at y {top} is flat ({rows:.0%} of its rows drawn)")
    spec = (slice(int(60 * s), int(235 * s)), slice(int(x_of(25)), int(x_of(15000))))
    if np.abs(a[spec] - b[spec]).mean() < 0.15:
        problems.append("the spectrum is empty or still")
    return problems


def build(reel):
    global SHADOW
    w = WORK / reel.name
    (w / "frames").mkdir(parents=True, exist_ok=True)
    for f in (w / "frames").glob("*.png"):
        f.unlink()
    sec = reel.seconds
    bar = 240.0 / reel.bpm
    # Pre-roll (Joe: "Let's also ensure all parameters are loaded prior to recording"): whole bars of the bed's opening
    # play first, so the preset, the opening state, the scope and the spectrum have settled; they are cut off after.
    pre = math.ceil(2.5 / bar) * bar
    P = int(round(pre * 48000))
    x = sum(load(p if Path(p).is_absolute() else STEMS / p, sec) * 10 ** (g / 20) for p, g in reel.inputs)
    x = np.vstack([x[:P], x])
    sf.write(w / "in.wav", x.astype(np.float32), 48000, subtype="FLOAT")
    shift = lambda a: a if not a.count(":") == 4 else ":".join([a.split(":")[0], f"{float(a.split(':')[1]) + pre:.3f}", f"{float(a.split(':')[2]) + pre:.3f}"] + a.split(":")[3:])
    args = [shift(a) for a in reel.args]
    changes = reel.plan.log["changes"]
    report_t = [pre + 0.05] + [pre + c1 + 0.3 for c0, c1, _ in changes]
    cmd = [RR, "--in", str(w / "in.wav"), "--out", str(w / "wet.wav"), "--bpm", str(reel.bpm),
           "--frames", str(w / "frames"), "--fps", str(FPS), "--scale", str(SCALE),
           "--report-at", ",".join(f"{t:.3f}" for t in report_t), "--report-out", str(w / "report.json")]
    if getattr(reel, "preset", None):
        cmd += ["--preset", reel.preset]
    for s in reel.sets:
        cmd += ["--set", s]
    cmd += args
    r = subprocess.run(cmd, capture_output=True, text=True)
    print(reel.name, "|", r.stdout.strip()[-160:], r.stderr.strip()[-200:])
    # Read back from the plugin: the opening state, then each change's new value.
    rep = json.load(open(w / "report.json"))
    problems = []
    if rep[0]["preset"] != reel.preset:
        problems.append(f"opening preset is {rep[0]['preset']}")
    for sset in reel.sets:
        pid, v = sset.split("=", 1)
        if pid == "FFT Pickoff Point":
            continue
        if not same(rep[0]["values"][pid], expected_text(pid, v)):
            problems.append(f"opening {pid} is {rep[0]['values'][pid]}, not {v}")
    targets = {}
    for a in reel.args:
        pid, t0, t1, v0, v1 = a.split(":") if a.count(":") == 4 else (None,) * 5
        if pid:
            targets.setdefault(float(t1), []).append((pid, v1))
    for k, (c0, c1, cap) in enumerate(changes):
        want = [pv for t, pvs in targets.items() if abs(t - c1) < 0.05 for pv in pvs]
        for pid, v in want:
            if not same(rep[k + 1]["values"][pid], expected_text(pid, v)):
                problems.append(f"after '{cap}', {pid} is {rep[k + 1]['values'][pid]}, not {v}")
    # The first kept frame and the one after it, on screen.
    frames = sorted((w / "frames").glob("*.png"))
    skip = int(round(pre * FPS))
    problems += ui_checks(reel, frames[skip], frames[skip + 3])
    assert not problems, f"{reel.name}: " + "; ".join(problems)
    wet, _ = sf.read(str(w / "wet.wav"), always_2d=True)
    mix = wet[P:P + int(sec * 48000)]
    fin = int(0.4 * 48000)   # Joe: "have the audio slightly fade in at start"; the bed is already playing under it
    mix[:fin] *= np.linspace(0, 1, fin)[:, None] ** 1.5
    fade = int(0.5 * 48000)
    mix[-fade:] *= np.linspace(1, 0, fade)[:, None] ** 2
    sf.write(w / "mix.wav", mix.astype(np.float32), 48000, subtype="FLOAT")
    loudnorm(w / "mix.wav", w / "audio.wav")
    lufs, tp = measure(w / "audio.wav")
    frames = frames[skip:]
    vw, vh = VIEW[2] - VIEW[0], VIEW[3] - VIEW[1]
    SHADOW = Image.new("RGBA", (vw + 80, vh + 80), (0, 0, 0, 0))
    ImageDraw.Draw(SHADOW).rounded_rectangle((40, 50, vw + 40, vh + 50), 24, fill=(0, 0, 0, 180))
    SHADOW = SHADOW.filter(ImageFilter.GaussianBlur(24))
    out = OUT / f"{reel.fname}.mp4"
    enc = subprocess.Popen([FFMPEG, "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS),
                            "-i", "-", "-i", str(w / "audio.wav"), "-c:v", "libx264", "-preset", "slow", "-crf", "16",
                            "-pix_fmt", "yuv420p", "-profile:v", "high", "-c:a", "aac", "-b:a", "256k", "-ar", "48000",
                            "-movflags", "+faststart", "-shortest", str(out)], stdin=subprocess.PIPE)
    for i in range(int(sec * FPS)):
        ui = Image.open(frames[min(i, len(frames) - 1)]).convert("RGB")
        img = frame(reel, ui, i / FPS)
        if i == int(1.2 * FPS):   # the first frame with the hook up, beside the video
            (OUT / "First frames").mkdir(exist_ok=True)
            img.save(OUT / "First frames" / f"{reel.fname}.png")
        enc.stdin.write(img.tobytes())
    enc.stdin.close()
    enc.wait()
    print(f"  -> {out.name}  {sec:.1f} s  {lufs:.1f} LUFS  true peak {tp:.1f} dBTP  checks passed")
    return dict(name=reel.fname, seconds=round(sec, 1), lufs=lufs, tp=tp)


DUET = Path(r"C:\Users\Joe\Documents\GitHub\duet")
PATCHES = Path(r"C:\Users\Joe\Documents\GitHub\WLA-Playbook\tools\content-rig\patches")
V4_PROJECT = "Tertiary Promo v6 options"
# START (each option's first bar in the Duet project) is set with the options above.
PPQ4, BAR4 = 480, 1920


def fit(body, n):
    """The progression as one chord per bar, fitted to n bars: the opening and the approach to the tonic are kept;
    bars come out of (or the opening chord stretches into) the middle."""
    bars = [c for c, k in body for _ in range(k)]
    if len(bars) > n:
        a = n // 2
        bars = bars[:a] + bars[len(bars) - (n - a):]
    while len(bars) < n:
        bars.insert(0, bars[0])
    return bars


# The air: Flying Angels and Coastline Sunset are dark patches (-25 and -21 dB above 1 kHz as first written), so the
# top layer (and on the bright bed the top line) sits an octave up, giving the high band something to pulse.
AIR_UP = {"bright": {"layer": 12, "melody": 12}, "warm": {"layer": 12, "melody": 0}}


def compose_bed(key):
    """Held chords, re-struck only when the chord changes; upper layers enter as the bed builds; a CC11 swell over it;
    the tonic on bar n, held and released. Returns {track: [(tick, pitch, length, velocity)]} (pitch < 0: CC)."""
    import random as _r
    rnd = _r.Random(key)
    bd = BEDS[key]
    n = bd["bars"]
    bars = [bd["body_bars"][i] for i in bd["map"]]
    segs = []
    for i, c in enumerate(bars):
        if segs and segs[-1][2] == c:
            segs[-1][1] += 1
        else:
            segs.append([i, 1, c])
    enter = [0, max(1, n // 3), max(2, 2 * n // 3)]
    out = {bd["track"]: []}
    T = bd["track"]
    def with_body(layers):
        if bd.get("no_body"):
            return layers
        low = layers[0]
        body = low[0] + 24
        return [low + ([body] if body not in low and 43 <= body <= 62 else [])] + layers[1:]

    for start, length, c in segs:
        for layer, notes in enumerate(with_body(bd["chords"][c])):
            if layer == 2:
                notes = [q + bd.get("air", {}).get("layer", 0) for q in notes]
            first = max(start, enter[layer])
            if first >= start + length:
                continue
            for k, p in enumerate(notes):
                lag = (k + 3 * layer) * PPQ4 // 2 if bd.get("stagger") else k * bd.get("roll", 18)
                t = max(0, first * BAR4 + lag + rnd.randint(-6, 6))
                end = (start + length) * BAR4 - 30
                out[T].append((t, p, end - t, max(1, min(127, (80, 70, 62)[layer] + rnd.randint(-4, 4)))))
    ring = int(1.6 * bd["bpm"] / 60 * PPQ4)
    for layer, notes in enumerate(bd["chords"][bd["end"]]):
        if layer == 2:
            notes = [q + bd.get("air", {}).get("layer", 0) for q in notes]
        for k, p in enumerate(notes):
            out[T].append((n * BAR4 + k * 16, p, ring - k * 16, (76, 70, 64)[layer]))
    # CC11: in, building, and down a little into the ending
    for b in range(n + 1):
        v = int(78 + 40 * min(1.0, b / max(1, n - 1))) if b < n else 104
        for q in range(4):
            out[T].append((b * BAR4 + q * PPQ4, -11, 0, v))
    M = bd.get("melody_track", T)    # the top line, slow and held, over the chords
    out.setdefault(M, [])
    for i, src in enumerate(bd["map"]):
        for bar, beat, p, beats in bd.get("melody", []):
            if bar != src:
                continue
            p += bd.get("air", {}).get("melody", 0)
            t = max(0, int(i * BAR4 + beat * PPQ4) + rnd.randint(-8, 8))
            out[M].append((t, p, int(beats * PPQ4) - 40, 84 + rnd.randint(-4, 4)))
    if bd.get("end_top"):
        out[M].append((n * BAR4 + 40, bd["end_top"] + bd.get("air", {}).get("melody", 0), ring, 80))
    lay = bd.get("layer")
    if lay:   # the keys: the lower two layers, struck once at each chord change, left to ring
        L = lay["track"]
        out[L] = []
        for start, length, c in segs + [[n, 1, bd["end"]]]:
            ups = bd.get("air", {}).get("layer", 0)
            picks = [q + (ups if li == 2 else 0) for li, layer in enumerate(bd["chords"][c]) if li in lay.get("layers", [0, 1]) for q in layer if q >= 40]
            for k, p in enumerate(picks):
                t = max(0, start * BAR4 + k * 22 + rnd.randint(-5, 5))
                dur = (length * BAR4 - 40) if start < n else ring
                out[L].append((t, p, dur, lay["vel"] + rnd.randint(-5, 5)))
    return out


def arrange4():
    import mido
    import os
    env = dict(os.environ, DUET_PROJECT=V4_PROJECT)

    def duet(*a, check=True):
        return subprocess.run(["node", "duet.mjs", *a], cwd=DUET, env=env, check=check, capture_output=True, text=True).stdout

    projects = duet("projects")
    if V4_PROJECT not in projects:
        duet("new", V4_PROJECT)
        duet("add-bus", "Hall", "--aux")
        duet("insert", "Hall", "add", "Duet Verb")
        duet("set-params", "Hall", "Duet Verb", "Type=Hall; Decay=2.80 s; Pre-delay=20.0 ms; Size=120 %")
    state = json.loads(subprocess.run(["curl", "-s", "http://127.0.0.1:4300/api/projects/" + V4_PROJECT.replace(" ", "%20")],
                                      capture_output=True, text=True).stdout)["state"]
    have = {t["name"] for t in state["tracks"]}
    tracks = {}
    for key, bd in BEDS.items():
        tracks[bd["track"]] = bd["patch"]
        if bd.get("layer"):
            tracks[bd["layer"]["track"]] = bd["layer"]["patch"]
    for name, patch in tracks.items():
        if name not in have:
            duet("add-track", name)
            duet("instrument", name, "Kontakt 8")
            subprocess.run(["node", str(PATCHES.parent / "setpatch.mjs"), V4_PROJECT, name, str(PATCHES / f"{patch}.b64")], check=True)
            duet("send", name, "Hall", "-10")
            if name.startswith("PAD"):   # light tube drive: harmonics a phone speaker can play under the low pulse
                duet("insert", name, "add", "Duet Strip")
                duet("set-params", name, "Duet Strip", "Drive=6.0 dB; Drive Type=Tube; Drive Mix=40 %; Low Mid Freq=180 Hz; Low Mid Gain=2.0 dB; Low Mid Q=0.80; Low Pass=Off; High Freq=3.00 kHz; High Gain=12.0 dB; High Mid Freq=1.50 kHz; High Mid Gain=4.0 dB; High Mid Q=0.70")
    song = {}
    for key, bd in BEDS.items():
        off = (START[key] - 1) * BAR4
        for track, ev in compose_bed(key).items():
            song.setdefault(track, []).extend((off + t, p, l, v) for t, p, l, v in ev)
    state = json.loads(subprocess.run(["curl", "-s", "http://127.0.0.1:4300/api/projects/" + V4_PROJECT.replace(" ", "%20")],
                                      capture_output=True, text=True).stdout)["state"]
    for track, ev in song.items():
        mf = mido.MidiFile(ticks_per_beat=PPQ4)
        tr = mido.MidiTrack()
        mf.tracks.append(tr)
        msgs = []
        for a, p, l, v in ev:
            if p < 0:
                msgs.append((a, 0, mido.Message("control_change", control=-p, value=max(0, min(127, v)))))
            else:
                msgs += [(a, 2, mido.Message("note_on", note=p, velocity=v)), (a + max(10, l), 1, mido.Message("note_off", note=p, velocity=0))]
        last = 0
        for t, _, m in sorted(msgs, key=lambda x: (x[0], x[1])):
            m.time, last = t - last, t
            tr.append(m)
        f = WORK / f"v4 {track}.mid"
        mf.save(str(f))
        tid = next(x["id"] for x in state["tracks"] if x["name"] == track)
        for part in [p for p in state.get("parts", []) if p.get("track") == tid]:
            duet("delete-part", part["id"])
        duet("import-midi", str(f), "--onto", track, "--name", f"v4 {track}")
    for key, bd in BEDS.items():
        duet("tempo", str(bd["bpm"]), str(START[key]))
    for key, bd in BEDS.items():
        d = WORK / "stems" / key
        d.mkdir(parents=True, exist_ok=True)
        for f in d.glob("*.wav"):
            f.unlink()
        a = START[key]
        duet("render", str(d / "mix.wav"), "--from", str(a), "--to", str(a + bd["bars"] + 2), "--stems")


def beds_audio(out):
    """Each bed on its own (no Tertiary), as long as its Reel, at -14 LUFS, for judging the sound alone."""
    d = out / "Beds (audio only, no Tertiary)"
    d.mkdir(parents=True, exist_ok=True)
    for r in REELS:
        bd = BEDS[r.bed]
        x = load(WORK / "stems" / r.bed / "mix.wav", r.seconds)
        tmp = WORK / f"bed {r.bed}.wav"
        sf.write(tmp, x.astype(np.float32), 48000, subtype="FLOAT")
        dst = d / f"{r.name[:2]} {bd['name']} (bed only).wav"
        loudnorm(tmp, dst)
        print("  bed", dst.name, "%.1f LUFS, true peak %.1f dBTP" % measure(dst))


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    logs = {r.name: finish(r) for r in REELS}
    (WORK / "timing.json").write_text(json.dumps({r.name: {"camera": logs[r.name]["camera"], "changes": logs[r.name]["changes"],
                                                           "final chord": r.end, "seconds": r.seconds} for r in REELS}, indent=1))
    for r in REELS:
        print(f"{r.name}: {len(r.plan.log['changes'])} changes, plan ends {r.plan.t:.1f} s, final chord at {r.end:.1f} s "
              f"(bar {BEDS[r.bed]['bars'] + 1}), length {r.seconds:.1f} s")
    if "--plan" in ONLY:
        sys.exit(0)
    if "--no-arrange" not in ONLY:
        arrange4()
    ONLY.discard("--no-arrange")
    if "--arrange-only" in ONLY:   # the beds, rendered; the Reels wait for the build with the preset bar
        beds_audio(OUT)
        sys.exit(0)
    STEMS = WORK / "stems"
    END_CARD = end_card(FORMATS)
    end_cards(OUT)
    beds_audio(OUT)
    done = [build(r) for r in REELS if not ONLY or any(o in r.name for o in ONLY)]
    (WORK / "reels.json").write_text(json.dumps(done, indent=1))
