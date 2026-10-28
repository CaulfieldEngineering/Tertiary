"""Instagram Reels for Tertiary, v3: one change at a time, never during a camera move, and music arranged to end on
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
WORK = HERE / "work" / "v3"
REPO = HERE.parent
RR = str(REPO / "build-review" / "plugin" / "ReviewRender_artefacts" / "Release" / "ReviewRender.exe")
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

# ------------------------------------------------------------------------------------------------- v3: one thing at a time
# Joe, 2026-10-27: "Can we emphasize showing what happens when the parameter is changed? It seems like right now, the
# effect is engaged - or the parameter changed - right as or during a pan/zoom. So its hard to tell exactly what is
# being done or showcased in that moment." So every change is a beat of its own, in this order, and the camera and
# a change never overlap:
#   1. the camera eases to the control and settles;  2. a hold on the old sound (1 to 2 beats);  3. the caption;
#   4. the control moves, camera still, at a speed you can follow, the value changing;  5. a hold of at least 2 beats
#   on the new sound;  6. only then the camera moves on.
# And (Joe: "let's end the video on a musical resolve - currently just looks like its getting cut off"): the music is
# arranged backwards from the plan, so its last phrase lands on the tonic on a downbeat as the end card comes up, held
# and left to ring with its reverb tail.
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

    def change(self, target, caption, colour, kind, *args, value=None):
        """kind "ramp" (pid, v0, v1, seconds) or "step" ([(pid, value), ...]); value: (v0, v1, format) for a readout."""
        r, t = self.r, self.t
        if target != self.cur:
            r.say(t, "", colour)   # the last caption leaves as the camera moves; the next one comes with its change
            r.look(t, target, MOVE)
            self.log["camera"].append((t, t + MOVE))
            t += MOVE + SETTLE
            self.cur = target
        t += max(1.0, 1.5 * self.beat)                       # the old sound, for reference
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
        t += max(1.5, 2.5 * self.beat)                       # the new sound, everything still
        self.t = t

    def check(self):
        bad = [(c, m) for c in self.log["changes"] for m in self.log["camera"] if c[0] < m[1] and m[0] < c[1]]
        assert not bad, f"{self.r.name}: the camera moves during a change: {bad}"
        return self.log


def hz(v):
    return f"{v / 1000:.2f} kHz" if v >= 1000 else f"{v:.0f} Hz"


# Each piece: its 8 bars, then its last k bars again (so it always reaches bar 8, the V chord), then a written ending on
# the tonic. Notes per track come from the original MIDI (Promo 2026-10-26\midi); the ending is added here.
PIECES = {
    "P1": {"bpm": 84, "files": {"PAD": "P1 Pads/P1 Pads - PAD.mid", "BASS": "P1 Pads/P1 Pads - BASS.mid"},
           "ending": {"PAD": [50, 57, 60, 64, 65, 74], "BASS": [38]}},                          # D minor (add9)
    "P2": {"bpm": 96, "files": {"KEYS": "P2 Arp/P2 Arp - GTR.mid", "BASS": "P2 Arp/P2 Arp - BASS.mid"},
           "ending": {"KEYS": [40, 47, 54, 55, 62, 67], "BASS": [28]}, "roll": {"KEYS": 120}},  # E minor 9, broken upward
    "P3": {"bpm": 80, "files": {"KEYS": "P3 Stabs/P3 Stabs - GTR.mid"},
           "ending": {"KEYS": [36, 48, 52, 55, 62, 64]}, "roll": {"KEYS": 16}},                # C add9, strummed
    "P4": {"bpm": 104, "files": {"PAD": "P4 Groove/P4 Groove - PAD.mid", "BASS": "P4 Groove/P4 Groove - BASS.mid", "DRUMS": "P4 Groove/P4 Groove - DRUMS.mid"},
           "ending": {"PAD": [54, 57, 61, 64, 69], "BASS": [30], "DRUMS": [36, 49]}},           # F# minor, a kick and crash
}
for p in PIECES.values():
    p["bars"] = 8

# 1. Three bands, three rhythms (the pad piece).
r = Reel("01 Three bands, three rhythms", 84, 0, [("P1/mix.wav", 0)], [],
         ["Low-Mid Crossover Freq=200", "Mid-High Crossover Freq=1200", "FFT Pickoff Point=Output", "Bypassed Low Band=1",
          "Waveshape Mid Band=Sine", "Depth Mid Band=70", "Multiplier Mid Band=1/4 Note",
          "Waveshape High Band=Square", "Depth High Band=90", "Multiplier High Band=1/16 Note", "Skew High Band=35"])
r.piece = "P1"
pl = Plan(r, "Low band left alone. Mids in quarters, highs in 16ths.", "low")
pl.change(FREQ, "High crossover down", "xo", "ramp", "Mid-High Crossover Freq", 1200, 450, 3.0, value=hz)
pl.change(MID, "Mids to triplets", "mid", "step", [("Multiplier Mid Band", MULT["1/8T"])])
pl.change(HIGH, "Solo the high band", "high", "step", [("Solo High Band", 1)])
pl.change(HIGH, "Solo off", "high", "step", [("Solo High Band", 0)])
pl.change(FULL, "Tertiary off", "off", "step", [(f"Bypassed {b} Band", 1) for b in ("Low", "Mid", "High")])
pl.change(FULL, "Tertiary on", "xo", "step", [("Bypassed Mid Band", 0), ("Bypassed High Band", 0)])
r.plan = pl
REELS.append(r)

# 2. Electric piano, low end left alone.
r = Reel("02 Electric piano, low end left alone", 96, 0, [("P2/mix.wav", 0)], [],
         ["Low-Mid Crossover Freq=140", "Mid-High Crossover Freq=1500", "FFT Pickoff Point=Output"]
         + [f"Waveshape {x} Band=Triangle" for x in ("Low", "Mid", "High")]
         + [f"Depth {x} Band=85" for x in ("Low", "Mid", "High")]
         + [f"Multiplier {x} Band=1/8 Note" for x in ("Low", "Mid", "High")])
r.piece = "P2"
pl = Plan(r, "Full-band tremolo. The bass pulses too.", "off", intro=4.0)
pl.change(LOW, "Low band bypassed. The bass holds.", "low", "step", [("Bypassed Low Band", 1)])
pl.change(FREQ, "Low crossover up", "xo", "ramp", "Low-Mid Crossover Freq", 140, 600, 3.0, value=hz)
pl.change(HIGH, "Highs to square", "high", "step", [("Waveshape High Band", SHAPE["Square"])])
pl.change(HIGH, "High band phase", "high", "ramp", "Relative Phase High Band", 0, 180, 3.0, value=lambda v: f"{v:.0f}°")
r.plan = pl
REELS.append(r)

# 3. A reverb tail that moves (electric piano stabs into a room; Tertiary on the room only).
r = Reel("03 A reverb tail that moves", 80, 0, [("P3/Room.wav", 6.0)], [("P3/KEYS.wav", 0.0)],
         ["Low-Mid Crossover Freq=350", "Mid-High Crossover Freq=2200", "FFT Pickoff Point=Output", "Bypassed Low Band=1",
          "Waveshape Mid Band=Sine", "Depth Mid Band=0", "Multiplier Mid Band=1/4 Note",
          "Waveshape High Band=Square", "Depth High Band=100", "Multiplier High Band=1/16 Note"])
r.piece = "P3"
pl = Plan(r, "Tertiary on the reverb return only. The dry piano is untouched.", "xo", intro=4.0)
pl.change(FREQ, "High crossover down", "xo", "ramp", "Mid-High Crossover Freq", 2200, 700, 3.0, value=hz)
pl.change(MID, "Mid depth up", "mid", "ramp", "Depth Mid Band", 0, 90, 3.0, value=lambda v: f"Depth {v:.0f}%")
pl.change(FULL, "Tertiary off", "off", "step", [(f"Bypassed {b} Band", 1) for b in ("Low", "Mid", "High")])
pl.change(FULL, "Tertiary on", "xo", "step", [("Bypassed Mid Band", 0), ("Bypassed High Band", 0)])
r.plan = pl
REELS.append(r)

# 4. Rhythm from the low notes (Tertiary on the pad, in the groove).
r = Reel("04 Rhythm from the low notes", 104, 0, [("P4/PAD.wav", 0)], [("P4/DRUMS.wav", 0), ("P4/BASS.wav", 0)],
         ["Low-Mid Crossover Freq=330", "Mid-High Crossover Freq=1400", "FFT Pickoff Point=Output",
          "Waveshape Low Band=Square", "Depth Low Band=100", "Multiplier Low Band=1/8 Note", "Skew Low Band=40",
          "Bypassed Mid Band=1", "Bypassed High Band=1",
          "Waveshape Mid Band=Valleys", "Depth Mid Band=80", "Multiplier Mid Band=1/8 Note", "Invert Mid Band=1",
          "Waveshape High Band=Sine", "Depth High Band=60", "Multiplier High Band=1/16 Note"])
r.piece = "P4"
pl = Plan(r, "Tertiary on the pad. Its low notes chopped in 8ths.", "low", intro=3.5)
pl.change(FREQ, "Low crossover up", "xo", "ramp", "Low-Mid Crossover Freq", 330, 900, 3.0, value=hz)
pl.change(LOW, "Lows to 16ths", "low", "step", [("Multiplier Low Band", MULT["1/16"])])
pl.change(MID, "Mid band on, inverted", "mid", "step", [("Bypassed Mid Band", 0)])
pl.change(FULL, "Tertiary off", "off", "step", [(f"Bypassed {b} Band", 1) for b in ("Low", "Mid", "High")])
pl.change(FULL, "Tertiary on", "xo", "step", [("Bypassed Low Band", 0), ("Bypassed Mid Band", 0)])
r.plan = pl
REELS.append(r)

# 5. On an acoustic guitar: a recording, so no arrangement; the end card comes up on its last strum and it rings out.
r = Reel("05 On an acoustic guitar", 120, 0, [(ACOUSTIC, 0)], [],
         ["Low-Mid Crossover Freq=250", "Mid-High Crossover Freq=2500", "FFT Pickoff Point=Output", "Bypassed Low Band=1",
          "Sync to Host Mid Band=0", "Rate Mid Band=3.0", "Waveshape Mid Band=Sine", "Depth Mid Band=45",
          "Sync to Host High Band=0", "Rate High Band=5.5", "Waveshape High Band=Triangle", "Depth High Band=80"])
r.piece = None
r.recording_end = (30.6, 34.3)   # its last strum, and where the ring-out reaches silence
pl = Plan(r, "Low band left alone. Rates run free, in Hz.", "low")
pl.change(FREQ, "High crossover down", "xo", "ramp", "Mid-High Crossover Freq", 2500, 900, 2.5, value=hz)
pl.change(HIGH, "High band rate up", "high", "ramp", "Rate High Band", 5.5, 10.0, 2.5, value=lambda v: f"{v:.1f} Hz")
pl.change(MID, "Mid depth up", "mid", "ramp", "Depth Mid Band", 45, 90, 2.5, value=lambda v: f"Depth {v:.0f}%")
pl.change(FULL, "Tertiary off", "off", "step", [(f"Bypassed {b} Band", 1) for b in ("Low", "Mid", "High")])
pl.change(FULL, "Tertiary on", "xo", "step", [("Bypassed Mid Band", 0), ("Bypassed High Band", 0)])
r.plan = pl
REELS.append(r)

RING = 3.0   # the end card holds this long after the final chord: the chord rings and its tail fades out


def finish(reel):
    """Sets the Reel's end from its plan: the final chord on a downbeat at or after the last change's hold."""
    log = reel.plan.check()
    if reel.piece is None:
        reel.end, reel.seconds = reel.recording_end
        assert reel.plan.t <= reel.end, f"{reel.name}: the plan runs past the recording's last strum"
    else:
        bar = 240.0 / reel.bpm
        k = next(k for k in (0, 2, 4, 6, 8) if (8 + k) * bar >= reel.plan.t)
        PIECES[reel.piece]["k"] = k
        reel.end = (8 + k) * bar
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


MIDI = Path(r"C:\Users\Joe\Dropbox\Caulfield Engineering\Tertiary\Promo 2026-10-26\midi")
DUET = Path(r"C:\Users\Joe\Documents\GitHub\duet")
V3_PROJECT = "Tertiary Promo v3"
START = {"P1": 1, "P2": 41, "P3": 81, "P4": 121}   # each piece's first bar in the Duet project


def notes_of(path):
    import mido
    m = mido.MidiFile(str(path))
    t, on, out = 0, {}, []
    for msg in mido.merge_tracks(m.tracks):
        t += msg.time
        if msg.type == "note_on" and msg.velocity > 0:
            on[msg.note] = (t, msg.velocity)
        elif msg.type in ("note_off", "note_on") and msg.note in on:
            a, v = on.pop(msg.note)
            out.append((a, msg.note, t - a, v))
    return out, m.ticks_per_beat


def arrange():
    """Writes each piece's arrangement (its 8 bars, its last k bars again so it reaches bar 8's V, then the written
    ending on the tonic) into the Duet project, and renders each piece's stems to WORK/stems/<piece>."""
    import mido
    import os
    ppq = 480
    bar = 4 * ppq
    song = {}
    for key, pc in PIECES.items():
        k = pc.get("k", 0)
        off = (START[key] - 1) * bar
        for track, rel in pc["files"].items():
            ev, tpq = notes_of(MIDI / rel)
            sc = ppq / tpq
            ev = [(int(a * sc), p, int(l * sc), v) for a, p, l, v in ev]
            out = [e for e in ev if e[0] < 8 * bar]
            if k:
                out += [(a + k * bar, p, l, v) for a, p, l, v in ev if (8 - k) * bar <= a < 8 * bar]
            end = (8 + k) * bar
            out = [(a, p, max(10, min(l, end - a - 10)), v) for a, p, l, v in out]   # nothing hangs over the ending
            roll = pc.get("roll", {}).get(track, 12)
            ring = min(bar, int(1.6 * pc["bpm"] / 60 * ppq))   # held about 1.6 s, then the release and the room ring out
            for i, p in enumerate(pc["ending"][track]):
                hold = ring if track != "DRUMS" else ppq
                out.append((end + i * roll, p, hold - i * roll, 84 if track != "DRUMS" else 104))
            song.setdefault(track, []).extend((off + a, p, l, v) for a, p, l, v in out)
    env = dict(os.environ, DUET_PROJECT=V3_PROJECT)

    def duet(*a):
        return subprocess.run(["node", "duet.mjs", *a], cwd=DUET, env=env, check=True, capture_output=True, text=True).stdout

    state = json.loads(subprocess.run(["curl", "-s", "http://127.0.0.1:4300/api/projects/" + V3_PROJECT.replace(" ", "%20")],
                                      capture_output=True, text=True).stdout)["state"]
    for track, ev in song.items():
        mf = mido.MidiFile(ticks_per_beat=ppq)
        tr = mido.MidiTrack()
        mf.tracks.append(tr)
        ch = 9 if track == "DRUMS" else 0
        msgs = []
        for a, p, l, v in ev:
            msgs += [(a, 1, mido.Message("note_on", note=p, velocity=v, channel=ch)),
                     (a + max(10, l), 0, mido.Message("note_off", note=p, velocity=0, channel=ch))]
        last = 0
        for t, _, m in sorted(msgs, key=lambda x: (x[0], x[1])):
            m.time, last = t - last, t
            tr.append(m)
        f = WORK / f"v3 {track}.mid"
        mf.save(str(f))
        tid = next(x["id"] for x in state["tracks"] if x["name"] == track)
        for part in [p for p in state.get("parts", []) if p.get("track") == tid]:
            duet("delete-part", part["id"])
        duet("import-midi", str(f), "--onto", track, "--name", f"v3 {track}")
    for key, pc in PIECES.items():
        duet("tempo", str(pc["bpm"]), str(START[key]))
    for key, pc in PIECES.items():
        d = WORK / "stems" / key
        d.mkdir(parents=True, exist_ok=True)
        for f in d.glob("*.wav"):
            f.unlink()
        a = START[key]
        duet("render", str(d / "mix.wav"), "--from", str(a), "--to", str(a + 8 + pc.get("k", 0) + 2), "--stems")


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    logs = {r.name: finish(r) for r in REELS}
    (WORK / "timing.json").write_text(json.dumps({r.name: {"camera": logs[r.name]["camera"], "changes": logs[r.name]["changes"],
                                                           "final chord": r.end, "seconds": r.seconds} for r in REELS}, indent=1))
    for r in REELS:
        print(f"{r.name}: {len(r.plan.log['changes'])} changes, plan ends {r.plan.t:.1f} s, final chord at {r.end:.1f} s, "
              f"length {r.seconds:.1f} s" + (f", piece {r.piece} with its last {PIECES[r.piece]['k']} bars again" if r.piece else ""))
    if "--plan" in ONLY:
        sys.exit(0)
    if "--no-arrange" not in ONLY:
        arrange()
    ONLY.discard("--no-arrange")
    STEMS = WORK / "stems"
    END_CARD = end_card(FORMATS)
    end_cards(OUT)
    done = [build(r) for r in REELS if not ONLY or any(o in r.name for o in ONLY)]
    (WORK / "reels.json").write_text(json.dumps(done, indent=1))
