"""Dry demo tracks for Tertiary's live web demo (Wonderlab). Joe, 2026-10-27: "we need to generate 5-10 audio samples
to upload to the Wonderlab Audio engine that demonstrates Tertiary live on the website."

The site plays a track through the Tertiary WASM build and the visitor turns the knobs, so each track is dry (no
Tertiary on it), loops without a click, and sits at the same loudness as the others so the A/B is fair:
-18 LUFS, true peak at or under -1 dBTP, 16-bit 44.1 kHz WAV (the old 24-bit 96 kHz files may not decode in older
browsers). Mono sources stay mono; the engine copies mono to both sides.

The file name is the title in the player (the build lists plugin/sample_audio/ alphabetically), so names are plain.

  python web_tracks.py <out dir> [track name ...]
"""
import json
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

OUT = Path(sys.argv[1])
ONLY = set(sys.argv[2:])
HERE = Path(__file__).resolve().parent
REPO = HERE.parent
WORK = HERE / "work" / "web"
FFMPEG = r"C:\Users\Joe\AppData\Local\Microsoft\WinGet\Packages\Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-8.1.2-full_build\bin\ffmpeg.exe"
SOURCES = Path(r"C:\Users\Joe\Dropbox\Caulfield Engineering\Plugin Ideas\_sources")
RENDERS = HERE / "work" / "web" / "renders"  # Duet renders from the project "Tertiary Web Demo"

# name: (source files to sum, start s, end s, crossfade s, repeats, mono)
TRACKS = {
}


def read(path, sr=44100):
    tmp = WORK / (Path(path).stem + f"-{sr}.wav")
    subprocess.run([FFMPEG, "-y", "-loglevel", "error", "-i", str(path), "-af", "aresample=44100:resampler=soxr",
                    "-c:a", "pcm_f32le", str(tmp)], check=True)
    x, _ = sf.read(str(tmp), always_2d=True)
    return x


def quiet_near(x, sr, t, reach=0.6):
    """The quietest 20 ms within reach of t: a cut there lands between notes."""
    m = np.abs(x).mean(1)
    w = int(0.02 * sr)
    lo, hi = max(0, int((t - reach) * sr)), min(len(m) - w, int((t + reach) * sr))
    e = np.convolve(m[lo:hi + w] ** 2, np.ones(w) / w, "valid")
    return lo + int(np.argmin(e))


def loop(x, a, b, xf):
    """x[a:b], with the xf samples after b faded into its start (equal power), so the end runs into the start."""
    out = x[a:b].copy()
    if b + xf <= len(x):
        k = np.linspace(0, np.pi / 2, xf)[:, None]
        out[:xf] = x[a:a + xf] * np.sin(k) + x[b:b + xf] * np.cos(k)
    return out


def loudness(src, lufs=-18.0, tp=-1.0):
    r = subprocess.run([FFMPEG, "-hide_banner", "-i", str(src), "-af", f"loudnorm=I={lufs}:TP={tp}:LRA=20:print_format=json", "-f", "null", "-"],
                       capture_output=True, text=True)
    m = json.loads(r.stderr[r.stderr.rindex("{"):r.stderr.rindex("}") + 1])
    gain = lufs - float(m["input_i"])
    # a straight gain change, so nothing about the dynamics moves; the true peak is checked after
    return gain, float(m["input_tp"]) + gain


def measure(path):
    r = subprocess.run([FFMPEG, "-hide_banner", "-nostats", "-i", str(path), "-af", "ebur128=peak=true", "-f", "null", "-"],
                       capture_output=True, text=True)
    tail = r.stderr[r.stderr.rindex("Summary:"):]
    return float(tail.split("I:")[1].split("LUFS")[0]), float(tail.split("Peak:")[1].split("dBFS")[0])


def seam_jump(path):
    """The step across the loop point, against the track's typical sample-to-sample step."""
    x, _ = sf.read(str(path), always_2d=True)
    d = np.abs(np.diff(x, axis=0)).mean(1)
    return float(np.abs(x[-1] - x[0]).mean() / (np.percentile(d, 99) + 1e-12))


def build(name, spec):
    files, start, end, xf_s, reps, mono = spec
    sr = 44100
    x = sum(read(f) for f in files)
    if mono:
        x = x.mean(1, keepdims=True)
    end = end if end is not None else len(x) / sr - xf_s * 1.01
    a = quiet_near(x, sr, start) if xf_s > 0.1 else int(start * sr)
    b = quiet_near(x, sr, end) if xf_s > 0.1 else int(end * sr)
    y = loop(x, a, b, int(xf_s * sr))
    y = np.concatenate([y] * reps)
    tmp = WORK / f"{name}.f32.wav"
    sf.write(tmp, y.astype(np.float32), sr, subtype="FLOAT")
    gain, tp = loudness(tmp)
    if tp > -1.0:  # a gentle peak limit would change the sound; take the level down instead and say so
        gain -= tp + 1.0
    y = y * 10 ** (gain / 20)
    sf.write(tmp, y.astype(np.float32), sr, subtype="FLOAT")
    out = OUT / f"{name}.wav"
    subprocess.run([FFMPEG, "-y", "-loglevel", "error", "-i", str(tmp), "-af", "aresample=44100:dither_method=triangular",
                    "-c:a", "pcm_s16le", str(out)], check=True)
    lufs, peak = measure(out)
    info = dict(name=name, seconds=round(len(y) / sr, 2), channels=y.shape[1], lufs=lufs, true_peak=peak, seam=round(seam_jump(out), 2))
    print(info)
    return info


# Tracks from original MIDI (web_midi.py) in the Duet project "Tertiary Web Demo", held until the new Kontakt
# libraries are loaded on their tracks (Joe, 2026-10-27). No drums and no Octavio material in the demo tracks (Joe). name: (first bar, bars a pass, bpm, Duet tracks)
HELD = {
    "Warm Pad": (1, 8, 75, "WARM PAD"),
    "Electric Piano": (27, 12, 88, "EPIANO"),
    "Organ": (65, 8, 70, "ORGAN"),
    "Strings": (91, 12, 92, "STRINGS"),
    "Airy Pad": (129, 8, 80, "AIRY PAD"),
    "Ambient Swell": (167, 8, 60, "AMBIENT PAD"),
    "Dark Pad": (245, 8, 64, "DARK PAD"),
    "Clean Guitar Chords": (275, 12, 72, "GUITAR"),
}


def render_held(name):
    """Render all three passes through Duet and keep the middle one: its start carries the first pass's tails."""
    import os
    first, bars, bpm, tracks = HELD[name]
    full = RENDERS / f"{name} (3 passes).wav"
    env = dict(os.environ, DUET_PROJECT="Tertiary Web Demo")
    subprocess.run(["node", "duet.mjs", "render", str(full), "--from", str(first), "--to", str(first + 3 * bars),
                    "--tracks", tracks + ",Room,Hall"], cwd=r"C:\Users\Joe\Documents\GitHub\duet", env=env, check=True)
    one = bars * 4 * 60.0 / bpm
    # a 50 ms crossfade from the third pass into the start: free-running modulation (rotary, LFOs) is not in phase
    TRACKS[name] = ([str(full)], one, 2 * one, 0.05, 1, False)


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    if "--held" in ONLY:  # python web_tracks.py <out dir> --held [name ...]: render the MIDI tracks too
        ONLY.discard("--held")
        RENDERS.mkdir(parents=True, exist_ok=True)
        for n in HELD:
            if not ONLY or n in ONLY:
                render_held(n)
    done = [build(n, s) for n, s in TRACKS.items() if not ONLY or n in ONLY]
    (WORK / "tracks.json").write_text(json.dumps(done, indent=1))
