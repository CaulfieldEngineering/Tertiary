"""Instagram Reels for Tertiary (Joe, 2026-10-26: "Instagram friendly promotional material demoing the product").
No voice: the real plugin, its real UI drawn by its own editor, the real sound, and a few plain words.

Each Reel is one continuous run of Tertiary (ReviewRender) over one of the original pieces, with its settings
changed on a schedule, so the knobs move in the picture exactly when the sound changes.

  python reels.py <stems dir> <out dir> [reel name ...]
"""
import json
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf
from PIL import Image, ImageDraw, ImageFilter, ImageFont

STEMS = Path(sys.argv[1])
OUT = Path(sys.argv[2])
ONLY = set(sys.argv[3:])
WORK = Path(__file__).resolve().parent / "work"
RR = r"C:\Users\Joe\Documents\GitHub\Tertiary\build-review\plugin\ReviewRender_artefacts\Release\ReviewRender.exe"
FFBIN = r"C:\Users\Joe\AppData\Local\Microsoft\WinGet\Packages\Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-8.1.2-full_build\bin"
FFMPEG = FFBIN + r"\ffmpeg.exe"
ACOUSTIC = r"C:\Users\Joe\Documents\GitHub\Tertiary\plugin\sample_audio\Acoustic Guitar.wav"
FPS = 30
W, H = 1080, 1920
F = r"C:\Windows\Fonts"
font = lambda name, size: ImageFont.truetype(f"{F}\\{name}", size)
KICKER, TITLE, CAP, FOOT, CHIP = font("seguisb.ttf", 30), font("seguisb.ttf", 66), font("segoeui.ttf", 46), font("segoeui.ttf", 32), font("seguisb.ttf", 28)
TEXT, MUTED, FAINT = (236, 236, 240), (160, 160, 172), (96, 96, 110)
GREEN, BLUE, PINK = (120, 224, 120), (120, 120, 232), (226, 110, 222)  # Tertiary's band colours


def bars(bpm, n):
    return n * 4 * 60.0 / bpm


def step(pid, t, value):
    """A setting that changes at t (a ramp too short to hear as one)."""
    return ["--ramp", f"{pid}:{t:.3f}:{t + 0.02:.3f}:{value}:{value}"]


def ramp(pid, t0, t1, v0, v1):
    return ["--ramp", f"{pid}:{t0:.3f}:{t1:.3f}:{v0}:{v1}"]


def bypass_all(t, on):
    out = []
    for b in ("Low", "Mid", "High"):
        out += step(f"Bypassed {b} Band", t, 1 if on else 0)
    return out


MULT = {"1/2": 0, "1/4": 1, "1/4T": 2, "1/8": 3, "1/8T": 4, "1/16": 5}
SHAPE = {"Ramp Down": 0, "Ramp Up": 1, "Square": 2, "Triangle": 3, "Sine": 4, "Hills": 5, "Valleys": 6}

# ------------------------------------------------------------------------------------------------- the Reels
REELS = []

# 1. Three bands, three rhythms: the pad piece. Hook: it is already moving in the first second.
b = bars(84, 1)
REELS.append(dict(
    name="01 Three bands, three rhythms", bpm=84, seconds=bars(84, 10) - 0.3,
    input=[("P1/mix.wav", 0)], dry=[],
    kicker="TERTIARY", title="One tremolo,\nthree bands",
    sets=["Low-Mid Crossover Freq=200", "Mid-High Crossover Freq=1200", "FFT Pickoff Point=Output",
          "Bypassed Low Band=1", "Waveshape Mid Band=Sine", "Depth Mid Band=70", "Multiplier Mid Band=1/4 Note",
          "Waveshape High Band=Square", "Depth High Band=90", "Multiplier High Band=1/16 Note", "Skew High Band=35"],
    moves=bypass_all(2 * b, True) + bypass_all(4 * b, False) + step("Bypassed Low Band", 4 * b + 0.05, 1)
          + step("Multiplier Mid Band", 6 * b, MULT["1/8T"]) + ramp("Depth Mid Band", 6 * b, 7 * b, 70, 95)
          + step("Waveshape High Band", 6 * b, SHAPE["Ramp Down"]),
    captions=[(0, 2 * b, "Low band left alone. Mids on quarter notes. The top on sixteenths.", "ON"),
              (2 * b, 4 * b, "The same pad, Tertiary off.", "OFF"),
              (4 * b, 6 * b, "Back on. Each band keeps its own rhythm.", "ON"),
              (6 * b, 8 * b, "Mids to triplets, deeper. The top to a ramp.", "ON"),
              (8 * b, 99, "Tertiary, a three-band tremolo.", "")],
))

# 2. Leave the low end alone: a normal tremolo pumps the bass; Tertiary leaves it.
b = bars(96, 1)
normal = [f"Waveshape {x} Band=Triangle" for x in ("Low", "Mid", "High")] + [f"Depth {x} Band=85" for x in ("Low", "Mid", "High")] \
       + [f"Multiplier {x} Band=1/8 Note" for x in ("Low", "Mid", "High")]
REELS.append(dict(
    name="02 Leave the low end alone", bpm=96, seconds=bars(96, 10) - 0.3,
    input=[("P2/mix.wav", 0)], dry=[],
    kicker="TERTIARY", title="Tremolo without\nthe pumping bass",
    sets=["Low-Mid Crossover Freq=140", "Mid-High Crossover Freq=1500", "FFT Pickoff Point=Output"] + normal,
    moves=step("Bypassed Low Band", 4 * b, 1),
    captions=[(0, 4 * b, "Every band on the same eighth notes, like a normal tremolo. The bass pulses too.", "ALL BANDS"),
              (4 * b, 8 * b, "Low band left alone. The guitar still moves, the bass holds.", "MID + HIGH"),
              (8 * b, 99, "Tertiary, a three-band tremolo.", "")],
))

# 3. A reverb tail that moves: Tertiary on the Room return only, the dry guitar untouched.
b = bars(80, 1)
REELS.append(dict(
    name="03 A reverb tail that moves", bpm=80, seconds=bars(80, 10) - 1.0,
    input=[("P3/Room.wav", 6.0)], dry=[("P3/GTR.wav", 0.0)],
    kicker="TERTIARY ON THE REVERB", title="A reverb tail\nthat moves",
    sets=["Low-Mid Crossover Freq=350", "Mid-High Crossover Freq=2200", "FFT Pickoff Point=Output",
          "Bypassed Low Band=1", "Waveshape Mid Band=Sine", "Depth Mid Band=55", "Multiplier Mid Band=1/4 Note",
          "Waveshape High Band=Square", "Depth High Band=100", "Multiplier High Band=1/16 Note"],
    moves=bypass_all(3 * b, True) + bypass_all(5 * b, False) + step("Bypassed Low Band", 5 * b + 0.05, 1)
          + ramp("Relative Phase High Band", 6 * b, 7.5 * b, 0, 180),
    captions=[(0, 3 * b, "Only the reverb goes through Tertiary. The top of the tail is cut in sixteenths.", "ON"),
              (3 * b, 5 * b, "The same room, Tertiary off.", "OFF"),
              (5 * b, 8 * b, "On again, sliding the top band's phase.", "ON"),
              (8 * b, 99, "Tertiary, a three-band tremolo.", "")],
))

# 4. Rhythm from the roots: Tertiary on the pad only, in the groove. Low band square, the top left ringing.
b = bars(104, 1)
REELS.append(dict(
    name="04 Rhythm from the roots", bpm=104, seconds=bars(104, 10) - 0.3,
    input=[("P4/PAD.wav", 0)], dry=[("P4/DRUMS.wav", 0), ("P4/BASS.wav", 0)],
    kicker="TERTIARY ON THE PAD", title="Rhythm from\nthe low notes",
    sets=["Low-Mid Crossover Freq=330", "Mid-High Crossover Freq=1400", "FFT Pickoff Point=Output",
          "Waveshape Low Band=Square", "Depth Low Band=100", "Multiplier Low Band=1/8 Note", "Skew Low Band=40",
          "Bypassed Mid Band=1", "Bypassed High Band=1",
          "Waveshape Mid Band=Valleys", "Depth Mid Band=80", "Multiplier Mid Band=1/8 Note", "Invert Mid Band=1",
          "Waveshape High Band=Sine", "Depth High Band=60", "Multiplier High Band=1/16 Note"],
    moves=bypass_all(2 * b, True) + bypass_all(4 * b, False) + step("Bypassed Mid Band", 4 * b + 0.05, 1)
          + step("Bypassed High Band", 4 * b + 0.05, 1)
          + step("Bypassed Mid Band", 6 * b, 0) + step("Bypassed High Band", 6 * b, 0),
    captions=[(0, 2 * b, "The pad's low notes chopped in eighths. The top keeps ringing.", "LOW"),
              (2 * b, 4 * b, "The same groove, Tertiary off.", "OFF"),
              (4 * b, 6 * b, "Back on.", "LOW"),
              (6 * b, 8 * b, "Add the mids, inverted against the lows, and a sine on the top.", "ALL"),
              (8 * b, 99, "Tertiary, a three-band tremolo.", "")],
))

# 5. On an acoustic guitar: the recording that ships with Tertiary's web demo. Free-running rates, no tempo.
REELS.append(dict(
    name="05 On an acoustic guitar", bpm=120, seconds=30.0,
    input=[(ACOUSTIC, 0)], dry=[],
    kicker="TERTIARY", title="On an acoustic\nguitar",
    sets=["Low-Mid Crossover Freq=250", "Mid-High Crossover Freq=2500", "FFT Pickoff Point=Output",
          "Bypassed Low Band=1",
          "Sync to Host Mid Band=0", "Rate Mid Band=3.0", "Waveshape Mid Band=Sine", "Depth Mid Band=45",
          "Sync to Host High Band=0", "Rate High Band=5.5", "Waveshape High Band=Triangle", "Depth High Band=80"],
    moves=bypass_all(9, True) + bypass_all(15, False) + step("Bypassed Low Band", 15.05, 1)
          + ramp("Rate High Band", 20, 26, 5.5, 9.0) + ramp("Relative Phase High Band", 20, 26, 0, 180),
    captions=[(0, 9, "Body left alone. The mids sway slowly, the top shimmers faster.", "ON"),
              (9, 15, "The same take, Tertiary off.", "OFF"),
              (15, 20, "Back on.", "ON"),
              (20, 26, "Speeding up the top band and moving its phase.", "ON"),
              (26, 99, "Tertiary, a three-band tremolo.", "")],
))


# ------------------------------------------------------------------------------------------------- audio
def load(path, seconds, sr=48000):
    x, r = sf.read(str(path), always_2d=True)
    if r != sr:  # resample with ffmpeg (the acoustic take is 96 kHz)
        tmp = WORK / (Path(path).stem + f"-{sr}.wav")
        subprocess.run([FFMPEG, "-y", "-loglevel", "error", "-i", str(path), "-ar", str(sr), str(tmp)], check=True)
        x, r = sf.read(str(tmp), always_2d=True)
    if x.shape[1] == 1:
        x = np.repeat(x, 2, axis=1)
    n = int(seconds * sr)
    if len(x) < n:
        x = np.vstack([x, np.zeros((n - len(x), 2))])
    return x[:n]


def gain_db(x, db):
    return x * 10 ** (db / 20)


def loudnorm(src, dst):
    """Two-pass EBU R128 to -14 LUFS, true peak -1 dBTP: Instagram's own target, so it is not turned down."""
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
    i = float(tail.split("I:")[1].split("LUFS")[0])
    tp = float(tail.split("Peak:")[1].split("dBFS")[0])
    return i, tp


# ------------------------------------------------------------------------------------------------- picture
def background():
    bg = Image.new("RGB", (W, H), (11, 10, 14))
    # A faint wash of the plugin's banner purple at the top, nothing more.
    wash = Image.new("RGB", (W, H), (34, 18, 40))
    mask = Image.linear_gradient("L").resize((W, H)).point(lambda v: int(max(0, 255 - v) * 0.45))
    return Image.composite(wash, bg, mask)


BG = None


def text_center(d, y, s, f, fill, spacing=10):
    for line in s.split("\n"):
        w = d.textlength(line, font=f)
        d.text(((W - w) / 2, y), line, font=f, fill=fill)
        y += f.size + spacing
    return y


def wrap(d, s, f, width):
    words, lines, cur = s.split(), [], ""
    for w in words:
        t = (cur + " " + w).strip()
        if d.textlength(t, font=f) <= width:
            cur = t
        else:
            lines.append(cur)
            cur = w
    lines.append(cur)
    return "\n".join(lines)


def frame(reel, ui, t):
    img = BG.copy()
    d = ImageDraw.Draw(img)
    text_center(d, 250, " ".join(reel["kicker"]), KICKER, MUTED)
    text_center(d, 300, reel["title"], TITLE, TEXT)
    # The plugin, 1040 wide, with a soft shadow.
    uw = 1040
    uh = int(ui.height * uw / ui.width)
    ui_s = ui.resize((uw, uh), Image.LANCZOS)
    x0, y0 = (W - uw) // 2, 500
    shadow = Image.new("RGBA", (uw + 80, uh + 80), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle((40, 50, uw + 40, uh + 50), 22, fill=(0, 0, 0, 170))
    shadow = shadow.filter(ImageFilter.GaussianBlur(22))
    img.paste(shadow, (x0 - 40, y0 - 40), shadow)
    m = Image.new("L", (uw, uh), 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, uw, uh), 16, fill=255)
    img.paste(ui_s, (x0, y0), m)
    # The caption of this moment, and its state.
    cap = next((c for c in reel["captions"] if c[0] <= t < c[1]), reel["captions"][-1])
    y = y0 + uh + 56
    if cap[3]:
        on = cap[3] != "OFF"
        cw = d.textlength(cap[3], font=CHIP) + 40
        cx = (W - cw) / 2
        d.rounded_rectangle((cx, y, cx + cw, y + 48), 24, fill=(70, 160, 110) if on else (58, 58, 66))
        d.text((cx + 20, y + 7), cap[3], font=CHIP, fill=(10, 14, 12) if on else (200, 200, 208))
        y += 74
    text_center(d, y, wrap(d, cap[2], CAP, 900), CAP, TEXT, spacing=12)
    text_center(d, 1530, "Joe Caulfield  ·  Wonderland Audio", FOOT, FAINT)
    return img


# ------------------------------------------------------------------------------------------------- build
def build(reel):
    name = reel["name"]
    w = WORK / name
    (w / "frames").mkdir(parents=True, exist_ok=True)
    for f in (w / "frames").glob("*.png"):
        f.unlink()
    sec = reel["seconds"]
    # The input to Tertiary, and what stays dry beside it.
    x = sum(gain_db(load(p if Path(p).is_absolute() else STEMS / p, sec), g) for p, g in reel["input"])
    sf.write(w / "in.wav", x.astype(np.float32), 48000, subtype="FLOAT")
    cmd = [RR, "--in", str(w / "in.wav"), "--out", str(w / "wet.wav"), "--bpm", str(reel["bpm"]),
           "--frames", str(w / "frames"), "--fps", str(FPS), "--scale", "2"]
    for s in reel["sets"]:
        cmd += ["--set", s]
    cmd += reel["moves"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    print(name, "|", r.stdout.strip(), r.stderr.strip())
    wet, _ = sf.read(str(w / "wet.wav"), always_2d=True)
    mix = wet[: len(x)]
    for p, g in reel["dry"]:
        mix = mix + gain_db(load(STEMS / p, sec), g)[: len(mix)]
    # A short fade at the end so the last frame is not a cut.
    fade = int(0.6 * 48000)
    mix[-fade:] *= np.linspace(1, 0, fade)[:, None]
    sf.write(w / "mix.wav", mix.astype(np.float32), 48000, subtype="FLOAT")
    loudnorm(w / "mix.wav", w / "audio.wav")
    lufs, tp = measure(w / "audio.wav")
    # Picture: every UI frame onto the canvas, piped straight to the encoder.
    frames = sorted((w / "frames").glob("*.png"))
    out = OUT / f"Reel {name}.mp4"
    enc = subprocess.Popen([FFMPEG, "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS),
                            "-i", "-", "-i", str(w / "audio.wav"), "-c:v", "libx264", "-preset", "slow", "-crf", "17",
                            "-pix_fmt", "yuv420p", "-profile:v", "high", "-c:a", "aac", "-b:a", "256k", "-ar", "48000",
                            "-movflags", "+faststart", "-shortest", str(out)], stdin=subprocess.PIPE)
    n = int(sec * FPS)
    for i in range(n):
        ui = Image.open(frames[min(i, len(frames) - 1)]).convert("RGB")
        enc.stdin.write(frame(reel, ui, i / FPS).tobytes())
    enc.stdin.close()
    enc.wait()
    print(f"  -> {out.name}  {sec:.1f} s  {lufs:.1f} LUFS  true peak {tp:.1f} dBTP  {len(frames)} UI frames")
    return dict(name=name, seconds=round(sec, 1), lufs=lufs, tp=tp)


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    BG = background()
    done = [build(r) for r in REELS if not ONLY or any(o in r["name"] for o in ONLY)]
    (WORK / "reels.json").write_text(json.dumps(done, indent=1))
