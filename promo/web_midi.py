"""Original MIDI for Tertiary's web demo tracks that need instruments (Joe, 2026-10-27: "yes lets use the Kontakt
libraries"; they arrive tonight, so these are written and set up now and rendered once the libraries are in).

Each piece is a loop. It is written three times in a row; the render keeps the middle pass, so the tails crossing the
loop point are real and the file loops without a click.

  python web_midi.py [out dir]      writes <out>/<piece> - <TRACK>.mid and <out>/song/<TRACK>.mid (default promo/web_midi/).

The MIDI is source only (Joe, 2026-10-27): it is never shipped; only rendered WAVs go in plugin/sample_audio.
"""
import random
import sys
from pathlib import Path

import mido

OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent / "web_midi"
PPQ, BAR = 480, 480 * 4
random.seed(27102026)
PASSES = 3

# piece, bpm, bars per pass, tracks and what each is for
# piece, bpm, bars a pass, first bar in the Duet project, tracks and what each is for.
# Joe, 2026-10-27: no sampled guitar (guitar only from real recordings), no drums, no Octavio material, and lean
# toward pads. So strings in place of guitar chords, the small band dropped, and three pads.
PIECES = [
    ("Warm Pad", 75, 8, 1, {"WARM PAD": "a warm analog pad (Analog Dreams)"}),
    ("Electric Piano", 88, 12, 27, {"EPIANO": "an electric piano (Kontakt Factory Selection 2)"}),
    ("Organ", 70, 8, 65, {"ORGAN": "a drawbar organ (Kontakt Factory Selection 2)"}),
    ("Strings", 92, 12, 91, {"STRINGS": "a string ensemble (Kontakt Factory Selection 2)"}),
    ("Airy Pad", 80, 8, 129, {"AIRY PAD": "a bright, airy pad (Analog Dreams or Factory Selection 2)"}),
    ("Ambient Swell", 60, 8, 167, {"PAD": "a soft pad or bowed sound, into a long hall"}),
    ("Dark Pad", 64, 8, 245, {"DARK PAD": "a dark, slowly evolving pad (Analog Dreams)"}),
    # Joe, 2026-10-27, on the Content Rig's Jazz Guitar: "make it known that these Kontakt guitar sounds are pretty dang
    # good"; a guitar track for the demo: "yes".
    ("Clean Guitar Chords", 72, 12, 275, {"GUITAR": "the clean jazz guitar (Kontakt Factory Selection 2, Jazz Guitar)"}),
]


def hum(t, v, dt=6, dv=6):
    return max(0, t + random.randint(-dt, dt)), max(1, min(127, v + random.randint(-dv, dv)))


def chords_held(notes, track, progression, vel=70, roll=8):
    for bar, chord in enumerate(progression):
        for i, n in enumerate(chord):
            t, v = hum(bar * BAR + i * roll, vel, 3, 5)
            notes.append((track, t, n, BAR - 30, v))


def compose(name):
    n = []
    if name == "Warm Pad":  # A minor, one chord a bar, a slow top line
        prog = [[45, 57, 60, 64, 71], [41, 57, 60, 64, 69], [48, 55, 59, 64, 67], [43, 55, 59, 62, 69],
                [45, 57, 60, 64, 72], [38, 53, 57, 60, 64], [41, 53, 57, 60, 67], [40, 52, 56, 59, 64]]
        chords_held(n, "WARM PAD", prog, 68, 12)
    elif name == "Airy Pad":  # D, open voicings up high, two chords a bar's worth of air: one chord every bar
        prog = [[62, 69, 74, 76, 81], [59, 66, 71, 74, 78], [55, 62, 67, 71, 76], [57, 64, 69, 73, 78],
                [62, 69, 74, 76, 83], [54, 62, 66, 69, 76], [55, 62, 67, 69, 74], [57, 64, 67, 69, 76]]
        chords_held(n, "AIRY PAD", prog, 62, 20)
    elif name == "Clean Guitar Chords":
        # Joe, 2026-10-27: "just make sure the samples are musically sound. And interesting. Something someone would
        # really play." A slow soul / Little Wing style clean part in E, 72 bpm, 12 bars (40 s), written as a guitarist
        # plays it: each chord a real shape (frets per string, low E to high e, x = not played) with open strings
        # ringing; a picked pattern with loose timing, a muted chuck now and then, hammer-on fills and a short hook in
        # the gaps. Colour: add9s, maj7s, F#m11, sus4 to major, a passing E/G# bass and the borrowed bVI-bVII (Cmaj7 to
        # Dadd9) before it resolves on Emaj7.
        open_ = [40, 45, 50, 55, 59, 64]
        shapes = {"Eadd9": "0 2 4 4 0 0", "C#m7": "x 4 6 4 5 4", "Aadd9": "x 0 2 4 2 0", "Bsus4": "x 2 4 4 5 2", "B": "x 2 4 4 4 2",
                  "E/G#": "4 x 2 1 0 0", "Amaj7": "x 0 2 1 2 0", "F#m11": "2 x 2 2 0 0", "B7sus4": "x 2 4 2 5 2", "B7": "x 2 1 2 0 2",
                  "G#m7": "4 6 4 4 4 4", "Cmaj7": "x 3 2 0 0 0", "Dadd9": "x x 0 2 3 0", "Emaj7": "0 2 1 1 0 0"}
        frets = {k: [None if f == "x" else int(f) for f in v.split()] for k, v in shapes.items()}
        # (chord, beat it starts) per bar
        form = [[("Eadd9", 0)], [("C#m7", 0)], [("Aadd9", 0)], [("Bsus4", 0), ("B", 2)],
                [("Eadd9", 0), ("E/G#", 2)], [("Amaj7", 0)], [("F#m11", 0)], [("B7sus4", 0), ("B7", 2)],
                [("C#m7", 0), ("G#m7", 2)], [("Aadd9", 0)], [("Cmaj7", 0), ("Dadd9", 2)], [("Emaj7", 0)]]
        dyn = [0.8, 0.82, 0.88, 0.92, 0.85, 0.88, 0.95, 1.0, 1.02, 0.96, 1.05, 0.78]
        # the picked pattern over two beats: (beat offset, which string counted from the chord's lowest played one)
        half = [(0.0, 0), (0.5, 2), (1.0, 3), (1.5, 4)]
        def strings(ch):
            return [(k, open_[k] + f) for k, f in enumerate(frets[ch]) if f is not None]
        for bar, chords in enumerate(form):
            t0 = bar * BAR
            last = bar == len(form) - 1
            for ci, (ch, start) in enumerate(chords):
                st = strings(ch)
                span = (4 - start) if ci == len(chords) - 1 else chords[ci + 1][1] - start
                if last:   # the last chord rolled once and left to ring
                    for k, (_, p) in enumerate(st):
                        t, v = hum(t0 + k * 30, int(84 * dyn[bar]) - k * 2, 3, 3)
                        n.append(("GUITAR", t, p, 2 * BAR - 60, v))
                    continue
                for h in range(int(span // 2)):
                    for off, idx in half:
                        beat = start + 2 * h + off
                        sidx = min(idx, len(st) - 1)
                        p = st[sidx][1]
                        accent = 88 if off == 0 else (72 if off == 1.0 else 62)
                        t, v = hum(t0 + int(beat * PPQ), int(accent * dyn[bar]), 9, 7)
                        ring = int((span - (beat - start)) * PPQ) - 30   # let it ring to the next chord
                        n.append(("GUITAR", t, p, max(PPQ // 2, ring), v))
            if bar in (2, 6, 9):   # a muted chuck on the and of 4: three strings, very short and soft
                for k in (2, 3, 4):
                    t, v = hum(t0 + int(3.5 * PPQ) + k * 8, 34, 3, 4)
                    n.append(("GUITAR", t, open_[k] + 2, 45, v))
        # fills and the hook, on the top strings, from the hand position of the chord under them:
        # (bar, beat, pitch, beats long, velocity)
        line = [(1, 3.0, 64, 0.5, 70), (1, 3.5, 66, 0.25, 74), (1, 3.75, 68, 0.75, 80),        # hook: e 0-2-4 hammer-on
                (3, 3.0, 66, 0.25, 74), (3, 3.25, 68, 0.25, 78), (3, 3.5, 71, 1.0, 86),         # fill into bar 5: e 2-4-7
                (5, 3.0, 64, 0.5, 72), (5, 3.5, 66, 0.25, 76), (5, 3.75, 68, 0.75, 82),         # the hook again
                (7, 2.5, 63, 0.5, 80), (7, 3.0, 66, 0.5, 84), (7, 3.5, 64, 0.5, 78),            # B7 fill: B 4, e 2, e 0
                (9, 3.0, 68, 0.5, 80), (9, 3.5, 71, 0.5, 86), (9, 3.75, 73, 0.75, 90),          # the hook reaching up: e 4-7-9
                (10, 3.5, 66, 0.5, 76)]                                                          # F# over D, into the resolve
        for bar, beat, p, l, v in line:
            t, vv = hum(bar * BAR + int(beat * PPQ), v, 6, 4)
            n.append(("GUITAR", t, p, int(l * PPQ) - 15, vv))
    elif name == "Dark Pad":  # C minor, low and close, two bars a chord, one inner voice moving each bar
        prog = [[36, 48, 51, 55, 58], [36, 48, 51, 55, 56], [32, 44, 48, 51, 55], [32, 44, 48, 50, 55],
                [29, 41, 48, 51, 56], [29, 41, 48, 53, 56], [31, 43, 47, 50, 55], [31, 43, 46, 50, 55]]
        chords_held(n, "DARK PAD", prog, 70, 6)
    elif name == "Electric Piano":  # Dmaj9 Bm11 Gmaj7 A6sus, comped: on 1, the and of 2, and 4
        prog = [[50, 54, 57, 61, 64], [47, 57, 61, 64, 66], [43, 54, 59, 62, 66], [45, 54, 59, 62, 64]] * 3
        for bar, ch in enumerate(prog):
            for pos, ln, v in [(0, 1.4, 80), (1.5, 1.2, 70), (3, 0.9, 74)]:
                for i, p in enumerate(ch if pos == 0 else ch[1:]):
                    t, vv = hum(bar * BAR + int(pos * PPQ) + i * 4, v, 4, 6)
                    n.append(("EPIANO", t, p, int(ln * PPQ), vv))
    elif name == "Organ":  # F, inner voice moving
        prog = [[41, 53, 57, 60, 65], [41, 53, 58, 60, 65], [46, 53, 58, 62, 65], [46, 53, 57, 62, 65],
                [43, 55, 58, 62, 67], [45, 57, 60, 64, 69], [38, 57, 60, 62, 65], [48, 55, 58, 64, 67]]
        chords_held(n, "ORGAN", prog, 90, 0)
    elif name == "Strings":  # G D/F# Em C, held, the top voice moving by step
        prog = [[43, 55, 59, 62, 71], [42, 54, 57, 62, 69], [40, 55, 59, 64, 67], [36, 55, 60, 64, 67]] * 3
        prog[3], prog[7], prog[11] = [36, 55, 60, 64, 69], [36, 52, 60, 64, 72], [38, 54, 57, 62, 66]
        chords_held(n, "STRINGS", prog, 72, 16)
    elif name == "Ambient Swell":  # sparse, long notes for a big hall
        for bar, (low, top) in enumerate([(45, [64, 71]), (41, [64, 69]), (48, [67, 74]), (43, [66, 71]),
                                          (45, [64, 72]), (41, [65, 69]), (40, [64, 71]), (40, [62, 68])]):
            t, v = hum(bar * BAR, 64, 6, 5)
            n.append(("PAD", t, low, BAR * 2 - 60 if bar % 2 == 0 else BAR - 30, v))
            for i, p in enumerate(top):
                t, v = hum(bar * BAR + PPQ * (1 + 2 * i), 58, 10, 6)
                n.append(("PAD", t, p, PPQ * 3, v))
    return n


def write(events, tempos, name, path):
    mf = mido.MidiFile(ticks_per_beat=PPQ)
    tr = mido.MidiTrack()
    mf.tracks.append(tr)
    tr.append(mido.MetaMessage("track_name", name=name, time=0))
    msgs = [(t, 0, mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(b), time=0)) for t, b in tempos]
    msgs.append((0, 0, mido.MetaMessage("time_signature", numerator=4, denominator=4, time=0)))
    ch = 9 if name == "DRUMS" else 0
    for t, p, ln, v in events:
        msgs.append((t, 2, mido.Message("note_on", note=p, velocity=v, channel=ch, time=0)))
        msgs.append((t + max(10, ln), 1, mido.Message("note_off", note=p, velocity=0, channel=ch, time=0)))
    msgs.sort(key=lambda m: (m[0], m[1]))
    last = 0
    for t, _, m in msgs:
        m.time, last = t - last, t
        tr.append(m)
    mf.save(path)


(OUT / "song").mkdir(parents=True, exist_ok=True)
song, tempo_map, at, lines = {}, [], 1, ["# Web demo MIDI (original, written 2026-10-27)", "",
    "Each piece is one loop, 4/4. The Duet project \"Tertiary Web Demo\" holds each piece three times in a row;",
    "render all three and keep the middle pass (see web_tracks.py).", ""]
for name, bpm, bars, at, tracks in PIECES:
    ev = compose(name)
    start = (at - 1) * BAR
    tempo_map.append((start, bpm))
    lines.append(f"## {name}: {bpm} bpm, {bars} bars a pass, starts at bar {at} in Duet (passes at bars {at}, {at + bars}, {at + 2 * bars})")
    for track, what in tracks.items():
        one = sorted((t, p, ln, v) for tr, t, p, ln, v in ev if tr == track)
        write(one, [(0, bpm)], track, OUT / f"{name} - {track}.mid")
        lines.append(f"- `{name} - {track}.mid`: {what}. {len(one)} notes a pass.")
        for k in range(PASSES):
            song.setdefault(f"{name}|{track}", []).extend((start + k * bars * BAR + t, p, ln, v) for t, p, ln, v in one)
    lines.append("")
for key, ev in song.items():
    name, track = key.split("|")
    write(sorted(ev), sorted(tempo_map), track, OUT / "song" / f"{name} - {track}.mid")
(OUT / "README.md").write_text("\n".join(lines), encoding="utf-8")
print("\n".join(lines))
