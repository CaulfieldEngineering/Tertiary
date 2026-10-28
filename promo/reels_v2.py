"""Instagram Reels for Tertiary, v2 (Joe, 2026-10-27: "more movement and engagement in the video itself. Some more slider
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
WORK = HERE / "work" / "v2"
REPO = HERE.parent
RR = str(REPO / "build-review" / "plugin" / "ReviewRender_artefacts" / "Release" / "ReviewRender.exe")
FFMPEG = r"C:\Users\Joe\AppData\Local\Microsoft\WinGet\Packages\Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-8.1.2-full_build\bin\ffmpeg.exe"
ACOUSTIC = str(REPO / "plugin" / "sample_audio" / "Acoustic Guitar.wav")
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

# 1. Three bands, three rhythms. The pad piece; the top band's crossover sweeps through the chords.
r = Reel("01 Three bands, three rhythms", 84, bars(84, 10) - 0.3, [("P1/mix.wav", 0)], [],
         ["Low-Mid Crossover Freq=200", "Mid-High Crossover Freq=1200", "FFT Pickoff Point=Output", "Bypassed Low Band=1",
          "Waveshape Mid Band=Sine", "Depth Mid Band=70", "Multiplier Mid Band=1/4 Note",
          "Waveshape High Band=Square", "Depth High Band=90", "Multiplier High Band=1/16 Note", "Skew High Band=35"])
b = r.b
r.look(0, FULL)
r.say(b(0.5), "Low band left alone", "low")
r.look(b(1), HIGH); r.say(b(1), "Highs in 16ths", "high")
r.look(b(2), FREQ); r.say(b(2), "High crossover down", "xo", (b(2), b(3.5), 1200, 400, hz))
r.ramp("Mid-High Crossover Freq", b(2), b(3.5), 1200, 400)
r.ramp("Mid-High Crossover Freq", b(3.75), b(4.75), 400, 1500)
r.say(b(3.75), "High crossover back up", "xo", (b(3.75), b(4.75), 400, 1500, hz))
r.look(b(5), MID); r.say(b(5), "Mids to triplets", "mid")
r.step("Multiplier Mid Band", b(5), MULT["1/8T"])
r.ramp("Depth Mid Band", b(5.25), b(6), 70, 100); r.say(b(5.5), "Mid depth up", "mid", (b(5.25), b(6), 70, 100, lambda v: f"Depth {v:.0f}%"))
r.look(b(6.25), HIGH); r.say(b(6.25), "Solo the highs", "high")
r.step("Solo High Band", b(6.25), 1); r.step("Solo High Band", b(7), 0)
r.look(b(7), FULL); r.say(b(7), "Tertiary off", "off"); r.bypass(b(7), True)
r.say(b(7.75), "Tertiary on", "xo"); r.bypass(b(7.75), False); r.step("Bypassed Low Band", b(7.75) + 0.05, 1)
REELS.append(r)

# 2. Electric piano, low end left alone. The P2 arpeggio on keys over its original bass line (GroveBass). Joe,
# 2026-10-27: no sampled guitar, and his 2024 DI is out, so keys. Needs the KEYS track's instrument (Kontakt Factory
# Selection 2) before its stems exist.
r = Reel("02 Electric piano, low end left alone", 96, bars(96, 10) - 0.3, [("P2/mix.wav", 0)], [],
         ["Low-Mid Crossover Freq=140", "Mid-High Crossover Freq=1500", "FFT Pickoff Point=Output"]
         + [f"Waveshape {x} Band=Triangle" for x in ("Low", "Mid", "High")]
         + [f"Depth {x} Band=85" for x in ("Low", "Mid", "High")]
         + [f"Multiplier {x} Band=1/8 Note" for x in ("Low", "Mid", "High")])
b = r.b
r.look(0, FULL)
r.say(b(0.25), "Full-band tremolo. The bass pulses too.", "off")
r.look(b(1.5), LOW)
r.say(b(2), "Low band bypassed. Bass holds.", "low"); r.step("Bypassed Low Band", b(2), 1)
r.look(b(3), FREQ); r.say(b(3), "Low crossover up", "xo", (b(3), b(4.5), 140, 600, hz))
r.ramp("Low-Mid Crossover Freq", b(3), b(4.5), 140, 600)
r.ramp("Low-Mid Crossover Freq", b(4.5), b(5), 600, 250)
r.look(b(5), MID); r.say(b(5), "Mids in 16ths", "mid"); r.step("Multiplier Mid Band", b(5), MULT["1/16"])
r.ramp("Depth Mid Band", b(5.5), b(6), 85, 55)
r.look(b(6), HIGH); r.say(b(6), "Highs to square", "high"); r.step("Waveshape High Band", b(6), SHAPE["Square"])
r.ramp("Relative Phase High Band", b(6.75), b(8), 0, 180)
r.say(b(6.75), "High band phase", "high", (b(6.75), b(8), 0, 180, lambda v: f"{v:.0f}°"))
r.look(b(8), FULL)
REELS.append(r)

# 3. A reverb tail that moves. The P3 stabs on electric piano into the Room; Tertiary on the Room return only.
r = Reel("03 A reverb tail that moves", 80, bars(80, 10) - 1.0, [("P3/Room.wav", 6.0)], [("P3/KEYS.wav", 0.0)],
         ["Low-Mid Crossover Freq=350", "Mid-High Crossover Freq=2200", "FFT Pickoff Point=Output", "Bypassed Low Band=1",
          "Waveshape Mid Band=Sine", "Depth Mid Band=0", "Multiplier Mid Band=1/4 Note",
          "Waveshape High Band=Square", "Depth High Band=100", "Multiplier High Band=1/16 Note"])
b = r.b
r.look(0, FULL)
r.say(b(0.25), "Tertiary on the reverb return only. The dry piano is untouched.", "xo")
r.look(b(1.25), FREQ); r.say(b(1.5), "High crossover down", "xo", (b(1.5), b(3), 2200, 700, hz))
r.ramp("Mid-High Crossover Freq", b(1.5), b(3), 2200, 700)
r.say(b(3), "More of the tail in the top band", "high")
r.look(b(3.25), HIGH); r.say(b(3.75), "Highs to triplets", "high"); r.step("Multiplier High Band", b(3.75), MULT["1/8T"])
r.look(b(4.5), MID); r.say(b(4.75), "Mid depth up", "mid", (b(4.75), b(5.75), 0, 90, lambda v: f"Depth {v:.0f}%"))
r.ramp("Depth Mid Band", b(4.75), b(5.75), 0, 90)
r.look(b(6), FULL); r.say(b(6), "Tertiary off", "off"); r.bypass(b(6), True)
r.say(b(7), "Tertiary on", "xo"); r.bypass(b(7), False); r.step("Bypassed Low Band", b(7) + 0.05, 1)
REELS.append(r)

# 4. Rhythm from the low notes. Tertiary on the pad only, in the groove.
r = Reel("04 Rhythm from the low notes", 104, bars(104, 10) - 0.3, [("P4/PAD.wav", 0)], [("P4/DRUMS.wav", 0), ("P4/BASS.wav", 0)],
         ["Low-Mid Crossover Freq=330", "Mid-High Crossover Freq=1400", "FFT Pickoff Point=Output",
          "Waveshape Low Band=Square", "Depth Low Band=100", "Multiplier Low Band=1/8 Note", "Skew Low Band=40",
          "Bypassed Mid Band=1", "Bypassed High Band=1",
          "Waveshape Mid Band=Valleys", "Depth Mid Band=80", "Multiplier Mid Band=1/8 Note", "Invert Mid Band=1",
          "Waveshape High Band=Sine", "Depth High Band=60", "Multiplier High Band=1/16 Note"])
b = r.b
r.look(0, FULL)
r.say(b(0.25), "Tertiary on the pad. Lows in 8ths.", "low")
r.look(b(1.25), FREQ); r.say(b(1.5), "Low crossover up", "xo", (b(1.5), b(3), 330, 900, hz))
r.ramp("Low-Mid Crossover Freq", b(1.5), b(3), 330, 900)
r.look(b(3), LOW); r.say(b(3), "Lows in 16ths", "low"); r.step("Multiplier Low Band", b(3), MULT["1/16"])
r.say(b(4), "Lows back to 8ths", "low"); r.step("Multiplier Low Band", b(4), MULT["1/8"])
r.look(b(4.75), MID); r.say(b(5), "Mid band on, inverted", "mid"); r.bypass(b(5), False, ("Mid",))
r.look(b(5.75), HIGH); r.say(b(6), "High band on, sine in 16ths", "high"); r.bypass(b(6), False, ("High",))
r.look(b(7), FULL); r.say(b(7), "Tertiary off", "off"); r.bypass(b(7), True)
r.say(b(7.75), "Tertiary on", "xo"); r.bypass(b(7.75), False)
REELS.append(r)

# 5. On an acoustic guitar. Free-running rates; the beat grid is 120 bpm for the captions.
r = Reel("05 On an acoustic guitar", 120, 30.0, [(ACOUSTIC, 0)], [],
         ["Low-Mid Crossover Freq=250", "Mid-High Crossover Freq=2500", "FFT Pickoff Point=Output", "Bypassed Low Band=1",
          "Sync to Host Mid Band=0", "Rate Mid Band=3.0", "Waveshape Mid Band=Sine", "Depth Mid Band=45",
          "Sync to Host High Band=0", "Rate High Band=5.5", "Waveshape High Band=Triangle", "Depth High Band=80"])
r.look(0, FULL)
r.say(0.5, "Low band left alone", "low")
r.look(3.5, FREQ); r.say(4, "High crossover down", "xo", (4, 7, 2500, 900, hz))
r.ramp("Mid-High Crossover Freq", 4, 7, 2500, 900); r.ramp("Mid-High Crossover Freq", 7.5, 8.5, 900, 1800)
r.look(8.5, HIGH); r.say(9, "High band rate up", "high", (9, 12, 5.5, 10, lambda v: f"{v:.1f} Hz"))
r.ramp("Rate High Band", 9, 12, 5.5, 10)
r.look(12.5, MID); r.say(13, "Mid depth up", "mid", (13, 15, 45, 90, lambda v: f"Depth {v:.0f}%"))
r.ramp("Depth Mid Band", 13, 15, 45, 90)
r.look(15.5, HIGH); r.say(16, "Solo the highs", "high"); r.step("Solo High Band", 16, 1); r.step("Solo High Band", 18.5, 0)
r.look(19, FULL); r.say(19, "Tertiary off", "off"); r.bypass(19, True)
r.say(22, "Tertiary on", "xo"); r.bypass(22, False); r.step("Bypassed Low Band", 22.05, 1)
r.look(23, HIGH); r.say(23.5, "High band phase", "high", (23.5, 26, 0, 180, lambda v: f"{v:.0f}°"))
r.ramp("Relative Phase High Band", 23.5, 26, 0, 180)
r.look(26, FULL, 0.8)
REELS.append(r)


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
    r = subprocess.run([FFMPEG, "-hide_banner", "-i", str(src), "-af", "loudnorm=I=-14:TP=-1:LRA=11:print_format=json", "-f", "null", "-"],
                       capture_output=True, text=True)
    m = json.loads(r.stderr[r.stderr.rindex("{"):r.stderr.rindex("}") + 1])
    af = (f"loudnorm=I=-14:TP=-1:LRA=11:measured_I={m['input_i']}:measured_TP={m['input_tp']}:measured_LRA={m['input_lra']}"
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


def draw_end(img, reel, t):
    a = ease((t - reel.end) / 0.5)
    if a <= 0:
        return img
    card = BG.copy().convert("RGBA")
    y = 640
    card.alpha_composite(LOGO_END, ((W - LOGO_END.width) // 2, y))
    y += LOGO_END.height + 110
    d = ImageDraw.Draw(card)
    text_center(d, y, "Tertiary", NAME, TEXT)
    y += 150
    text_center(d, y, TAGLINE, TAG, TEXT)
    text_center(d, y + 110, "Multiband Tremolo  ·  VST3, AU, standalone", SMALL, MUTED)
    text_center(d, y + 162, "wonderlandaudio.com", SMALL, MUTED)
    return Image.blend(img, card.convert("RGB"), a)


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
    fade = int(1.6 * 48000)
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


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    done = [build(r) for r in REELS if not ONLY or any(o in r.name for o in ONLY)]
    (WORK / "reels.json").write_text(json.dumps(done, indent=1))
