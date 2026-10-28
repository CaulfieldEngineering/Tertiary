"""Instagram Reels for Tertiary, v4 (static wide pads, the rhythm all Tertiary's; see the v4 block). From v3: one change at a time, never during a camera move, and music arranged to end on
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
WORK = HERE / "work" / "v4"
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

    def __init__(self, reel, intro_caption, colour="xo", intro=3.0):
        self.r, self.beat = reel, 60.0 / reel.bpm
        self.cur = FULL
        self.log = {"camera": [], "changes": []}
        reel.look(0, FULL)
        reel.say(0.4, intro_caption, colour)
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


# Each bed: a Duet track (a Content Rig patch), a key, a tempo, and its chords bar by bar (body; the last entry is the
# tonic it ends on). Voicings are wide on purpose: a root an octave or two down for the lows, close tones in the
# middle, and upper tones for the air, which enter as the bed builds (layer 0 from the start, 1 a third of the way,
# 2 two thirds of the way).
BEDS = {
    "dark": {"track": "PAD DARK", "patch": "PAD DARK", "bpm": 90, "name": "Dark drone",
             "about": "a dark analog drone in D minor (Analog Dreams, Darkest Dawn), the low D held under slow chords",
             "body": [("Dm9", 2), ("Bbmaj7/D", 2), ("Gm9/D", 2), ("Dm9", 2), ("Bbmaj7#11", 1), ("C6sus2", 1), ("A7sus4", 1)],
             "end": "Dm(add9)",
             "chords": {"Dm9": [[26, 38, 45], [53, 57, 64], [69, 76]], "Bbmaj7/D": [[26, 38, 46], [53, 57, 62], [69, 77]],
                        "Gm9/D": [[26, 38, 43], [50, 58, 62], [69, 74]], "Bbmaj7#11": [[34, 46, 53], [57, 62, 64], [69, 76]],
                        "C6sus2": [[36, 48, 55], [57, 62, 64], [67, 74]], "A7sus4": [[33, 45, 52], [55, 62, 64], [67, 74]],
                        "Dm(add9)": [[26, 38, 45], [53, 57, 64], [69, 74, 77]]}},
    "bright": {"track": "PAD AIRY", "patch": "PAD AIRY", "bpm": 100, "name": "Bright shimmer",
               "about": "a bright, shimmering pad in E (Analog Dreams, Flying Angels), wide open voicings with plenty of air",
               "body": [("Emaj9", 2), ("C#m11", 2), ("Amaj9", 2), ("Emaj9/G#", 2), ("Amaj7#11", 1), ("Bsus4", 1), ("B", 1)],
               "end": "Emaj9",
               "chords": {"Emaj9": [[28, 40, 47], [56, 59, 63, 66], [71, 75, 78]], "C#m11": [[25, 37, 44], [54, 59, 64, 66], [71, 76]],
                          "Amaj9": [[33, 45, 52], [56, 59, 61, 64], [71, 73, 76]], "Emaj9/G#": [[32, 44, 52], [56, 59, 63, 66], [71, 75, 78]],
                          "Amaj7#11": [[33, 45, 52], [56, 61, 63, 64], [68, 75]], "Bsus4": [[35, 47, 54], [59, 64, 66], [71, 76]],
                          "B": [[35, 47, 54], [59, 63, 66], [71, 75]]}},
    "warm": {"track": "PAD WARM", "patch": "PAD WARM", "bpm": 84, "name": "Warm keys",
             "about": "a warm pad in A flat under held electric piano chords (Analog Dreams Coastline Sunset and Factory Selection 2 Mark II)",
             "layer": {"track": "EPIANO", "patch": "EPIANO", "vel": 70},
             "body": [("Abmaj9", 2), ("Fm11", 2), ("Dbmaj9", 2), ("Bbm9", 1), ("Eb7sus4", 1), ("Cm7", 1), ("Fm9", 1), ("Bbm9", 1), ("Eb9sus4", 1)],
             "end": "Abmaj9",
             "chords": {"Abmaj9": [[32, 44, 51], [55, 58, 60, 63], [67, 70]], "Fm11": [[29, 41, 48], [56, 58, 63, 65], [68, 72]],
                        "Dbmaj9": [[25, 37, 44], [53, 56, 60, 63], [68, 72]], "Bbm9": [[34, 46, 53], [56, 60, 61, 65], [68, 72]],
                        "Eb7sus4": [[27, 39, 46], [56, 58, 61, 63], [68, 70]], "Cm7": [[36, 48, 55], [58, 63, 67], [70, 75]],
                        "Fm9": [[29, 41, 48], [55, 56, 60, 63], [67, 72]], "Eb9sus4": [[27, 39, 46], [56, 61, 63, 65], [68, 70]]}},
    "texture": {"track": "AMBIENT", "patch": "AMBIENT", "bpm": 76, "name": "Slow texture",
                "about": "an evolving texture in A minor (Analog Dreams, Offworld Swell), voices entering one by one and overlapping",
                "body": [("Am(add9)", 2), ("Fmaj7#11", 2), ("Cmaj9/G", 2), ("Dm11", 1), ("Esus4", 1), ("E7", 1)],
                "end": "Am(add9)", "stagger": True,
                "chords": {"Am(add9)": [[33, 45, 52], [57, 60, 64, 71], [76, 83]], "Fmaj7#11": [[29, 41, 48], [57, 60, 64, 71], [76, 81]],
                           "Cmaj9/G": [[31, 43, 48], [55, 59, 62, 64], [71, 79]], "Dm11": [[26, 38, 45], [53, 57, 60, 67], [72, 79]],
                           "Esus4": [[28, 40, 47], [57, 59, 64], [69, 76]], "E7": [[28, 40, 47], [56, 59, 62, 64], [68, 76]]}},
}


# Fewer changes, each held long (Joe: "You don't have to go in order with the bands necessarily, but a lot of coverage
# per setting would be ideal."): every setting gets 8 beats, about two bars, before anything else moves. Each Reel
# opens on one of Tertiary 1.1's factory presets, named on screen (the preset bar shows it), and the low band ends up at
# full depth, well above the mids and highs where it differs (Joe: "low frequencies tend to need more depth to be
# audible especially on mobile devices").
HOLD = 8.0
VIEW_ONLY = ["FFT Pickoff Point=Output"]   # a view setting: the preset is not marked modified by it

# 1. Dark drone on "Alternating Chop": squares in eighths on all three bands, the mids inverted against the others.
r = Reel("01 Dark drone", 90, 0, [("dark/mix.wav", 0)], [], VIEW_ONLY)
r.bed, r.preset = "dark", "Alternating Chop"
pl = Plan(r, "Factory preset: Alternating Chop. One held chord underneath.", "xo", intro=2 * 240 / 90)
pl.change(MID, "Mids alone: inverted against the others", "mid", "step", [("Solo Mid Band", 1)], hold=HOLD)
pl.change(FULL, "All three bands", "xo", "step", [("Solo Mid Band", 0)], hold=HOLD)
pl.change(HIGH, "Highs to sixteenths", "high", "step", [("Multiplier High Band", MULT["1/16"])], hold=HOLD)
pl.change(LOW, "Lows to quarter notes", "low", "step", [("Multiplier Low Band", MULT["1/4"])], hold=HOLD)
r.plan = pl
REELS.append(r)

# 2. Bright shimmer on "Rolling Triplets": eighth-note triplets, the bands a third of a cycle apart.
r = Reel("02 Bright shimmer", 100, 0, [("bright/mix.wav", 0)], [], VIEW_ONLY)
r.bed, r.preset = "bright", "Rolling Triplets"
pl = Plan(r, "Factory preset: Rolling Triplets. A held pad, nothing played in time.", "xo", intro=2 * 240 / 100)
pl.change(LOW, "Lows to full depth", "low", "ramp", "Depth Low Band", 85, 100, 2.5, value=lambda v: f"Depth {v:.0f}%", hold=HOLD)
pl.change(HIGH, "Highs alone: triplets a third behind", "high", "step", [("Solo High Band", 1)], hold=HOLD)
pl.change(FULL, "All three bands", "xo", "step", [("Solo High Band", 0)], hold=HOLD)
pl.change(MID, "Mids to sixteenths", "mid", "step", [("Multiplier Mid Band", MULT["1/16"])], hold=HOLD)
r.plan = pl
REELS.append(r)

# 3. Warm keys on "Three Rhythms": quarters in the lows, quarter triplets in the mids, eighths in the highs.
r = Reel("03 Warm keys", 84, 0, [("warm/mix.wav", 0)], [], VIEW_ONLY)
r.bed, r.preset = "warm", "Three Rhythms"
pl = Plan(r, "Factory preset: Three Rhythms. Held chords on keys and a pad.", "xo", intro=2 * 240 / 84)
pl.change(LOW, "Lows to full depth", "low", "ramp", "Depth Low Band", 80, 100, 2.5, value=lambda v: f"Depth {v:.0f}%", hold=HOLD)
pl.change(MID, "Mids alone: quarter-note triplets", "mid", "step", [("Solo Mid Band", 1)], hold=HOLD)
pl.change(FULL, "All three bands", "xo", "step", [("Solo Mid Band", 0)], hold=HOLD)
r.plan = pl
REELS.append(r)

# 4. Slow texture on "Half-Time Sway": half notes, the lows and highs moving against each other.
r = Reel("04 Slow texture", 76, 0, [("texture/mix.wav", 0)], [], VIEW_ONLY)
r.bed, r.preset = "texture", "Half-Time Sway"
pl = Plan(r, "Factory preset: Half-Time Sway. A slow texture, held.", "xo", intro=2 * 240 / 76)
pl.change(FREQ, "High crossover up", "xo", "ramp", "Mid-High Crossover Freq", 1000, 3000, 3.0, value=hz, hold=HOLD)
pl.change(LOW, "Lows alone: a half-note sway", "low", "step", [("Solo Low Band", 1)], hold=HOLD)
pl.change(FULL, "All three bands", "xo", "step", [("Solo Low Band", 0)], hold=HOLD)
r.plan = pl
REELS.append(r)

RING = 3.2   # the end card holds this long after the final chord, which rings and fades with its tail


def finish(reel):
    """Sets the Reel's end from its plan: the final chord on the first downbeat at or after the last change's hold."""
    log = reel.plan.check()
    bar = 240.0 / reel.bpm
    n = math.ceil(reel.plan.t / bar - 1e-6)
    BEDS[reel.bed]["bars"] = n
    reel.end = n * bar
    reel.seconds = reel.end + RING
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
LOGO_MARK = fit(LOGO, 360)
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
    for line in formats + ["wonderlandaudio.com"]:
        text_center(d, y, line, SMALL, MUTED)
        y += 52
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
def build(reel):
    global SHADOW
    w = WORK / reel.name
    (w / "frames").mkdir(parents=True, exist_ok=True)
    for f in (w / "frames").glob("*.png"):
        f.unlink()
    sec = reel.seconds
    x = sum(load(p if Path(p).is_absolute() else STEMS / p, sec) * 10 ** (g / 20) for p, g in reel.inputs)
    sf.write(w / "in.wav", x.astype(np.float32), 48000, subtype="FLOAT")
    cmd = [RR, "--in", str(w / "in.wav"), "--out", str(w / "wet.wav"), "--bpm", str(reel.bpm),
           "--frames", str(w / "frames"), "--fps", str(FPS), "--scale", str(SCALE)]
    if getattr(reel, "preset", None):
        cmd += ["--preset", reel.preset]
    for s in reel.sets:
        cmd += ["--set", s]
    cmd += reel.args
    r = subprocess.run(cmd, capture_output=True, text=True)
    print(reel.name, "|", r.stdout.strip(), r.stderr.strip())
    wet, _ = sf.read(str(w / "wet.wav"), always_2d=True)
    mix = wet[: len(x)]
    for p, g in reel.dry:
        mix = mix + load(STEMS / p, sec)[: len(mix)] * 10 ** (g / 20)
    fade = int(0.5 * 48000)   # the music has already ended and rung out; this only clears the last of the tail
    mix[-fade:] *= np.linspace(1, 0, fade)[:, None] ** 2
    sf.write(w / "mix.wav", mix.astype(np.float32), 48000, subtype="FLOAT")
    loudnorm(w / "mix.wav", w / "audio.wav")
    lufs, tp = measure(w / "audio.wav")
    frames = sorted((w / "frames").glob("*.png"))
    vw, vh = VIEW[2] - VIEW[0], VIEW[3] - VIEW[1]
    SHADOW = Image.new("RGBA", (vw + 80, vh + 80), (0, 0, 0, 0))
    ImageDraw.Draw(SHADOW).rounded_rectangle((40, 50, vw + 40, vh + 50), 24, fill=(0, 0, 0, 180))
    SHADOW = SHADOW.filter(ImageFilter.GaussianBlur(24))
    out = OUT / f"Reel {reel.name}.mp4"
    enc = subprocess.Popen([FFMPEG, "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS),
                            "-i", "-", "-i", str(w / "audio.wav"), "-c:v", "libx264", "-preset", "slow", "-crf", "16",
                            "-pix_fmt", "yuv420p", "-profile:v", "high", "-c:a", "aac", "-b:a", "256k", "-ar", "48000",
                            "-movflags", "+faststart", "-shortest", str(out)], stdin=subprocess.PIPE)
    for i in range(int(sec * FPS)):
        ui = Image.open(frames[min(i, len(frames) - 1)]).convert("RGB")
        enc.stdin.write(frame(reel, ui, i / FPS).tobytes())
    enc.stdin.close()
    enc.wait()
    print(f"  -> {out.name}  {sec:.1f} s  {lufs:.1f} LUFS  true peak {tp:.1f} dBTP  {len(frames)} UI frames")
    return dict(name=reel.name, seconds=round(sec, 1), lufs=lufs, tp=tp)


DUET = Path(r"C:\Users\Joe\Documents\GitHub\duet")
PATCHES = Path(r"C:\Users\Joe\Documents\GitHub\WLA-Playbook\tools\content-rig\patches")
V4_PROJECT = "Tertiary Promo v4"
START = {"dark": 1, "bright": 31, "warm": 61, "texture": 91}   # each bed's first bar in the Duet project
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


def compose_bed(key):
    """Held chords, re-struck only when the chord changes; upper layers enter as the bed builds; a CC11 swell over it;
    the tonic on bar n, held and released. Returns {track: [(tick, pitch, length, velocity)]} (pitch < 0: CC)."""
    import random as _r
    rnd = _r.Random(key)
    bd = BEDS[key]
    n = bd["bars"]
    bars = fit(bd["body"], n)
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
        low = layers[0]
        body = low[0] + 24
        return [low + ([body] if body not in low and 43 <= body <= 62 else [])] + layers[1:]

    for start, length, c in segs:
        for layer, notes in enumerate(with_body(bd["chords"][c])):
            first = max(start, enter[layer])
            if first >= start + length:
                continue
            for k, p in enumerate(notes):
                lag = (k + 3 * layer) * PPQ4 // 2 if bd.get("stagger") else k * 18
                t = max(0, first * BAR4 + lag + rnd.randint(-6, 6))
                end = (start + length) * BAR4 - 30
                out[T].append((t, p, end - t, max(1, min(127, (80, 70, 62)[layer] + rnd.randint(-4, 4)))))
    ring = int(1.6 * bd["bpm"] / 60 * PPQ4)
    for layer, notes in enumerate(bd["chords"][bd["end"]]):
        for k, p in enumerate(notes):
            out[T].append((n * BAR4 + k * 16, p, ring - k * 16, (76, 70, 64)[layer]))
    # CC11: in, building, and down a little into the ending
    for b in range(n + 1):
        v = int(78 + 40 * min(1.0, b / max(1, n - 1))) if b < n else 104
        for q in range(4):
            out[T].append((b * BAR4 + q * PPQ4, -11, 0, v))
    lay = bd.get("layer")
    if lay:   # the keys: the lower two layers, struck once at each chord change, left to ring
        L = lay["track"]
        out[L] = []
        for start, length, c in segs + [[n, 1, bd["end"]]]:
            for k, p in enumerate([q for layer in bd["chords"][c][:2] for q in layer if q >= 40]):
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
