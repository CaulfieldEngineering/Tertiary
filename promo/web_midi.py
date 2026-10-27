"""Original MIDI for Tertiary's web demo tracks that need instruments (Joe, 2026-10-27: "yes lets use the Kontakt
libraries"; they arrive tonight, so these are written and set up now and rendered once the libraries are in).

Each piece is a loop. It is written three times in a row; the render keeps the middle pass, so the tails crossing the
loop point are real and the file loops without a click.

  python web_midi.py <out dir>      writes <out>/midi/<piece> - <TRACK>.mid and <out>/midi/song/<TRACK>.mid
"""
import random
import sys
from pathlib import Path

import mido

OUT = Path(sys.argv[1])
PPQ, BAR = 480, 480 * 4
random.seed(27102026)
PASSES = 3

# piece, bpm, bars per pass, tracks and what each is for
PIECES = [
    ("Synth Pad", 75, 8, {"PAD": "a warm analog-style pad"}),
    ("Electric Piano", 88, 12, {"EPIANO": "a Rhodes or Wurli style electric piano"}),
    ("Organ", 70, 8, {"ORGAN": "a drawbar organ, slow rotary or none"}),
    # No sampled guitar (Joe, 2026-10-27: guitar comes only from real recordings), so strings in its place.
    ("Strings", 92, 12, {"STRINGS": "a string ensemble (Kontakt Factory Selection 2)"}),
    ("Small Band", 100, 12, {"DRUMS": "SSD5 or a Kontakt kit (GM notes)", "BASS": "a finger bass", "KEYS": "the electric piano"}),
    ("Ambient Swell", 60, 8, {"PAD": "a soft pad or bowed sound, into a long hall"}),
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
    if name == "Synth Pad":  # A minor, one chord a bar, a slow top line
        prog = [[45, 57, 60, 64, 71], [41, 57, 60, 64, 69], [48, 55, 59, 64, 67], [43, 55, 59, 62, 69],
                [45, 57, 60, 64, 72], [38, 53, 57, 60, 64], [41, 53, 57, 60, 67], [40, 52, 56, 59, 64]]
        chords_held(n, "PAD", prog, 68, 12)
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
    elif name == "Clean Guitar Chords (not used)":  # G D/F# Em C, a strum pattern, down on 1 and 3, up on the ands of 2 and 4
        shapes = [[43, 47, 50, 55, 59, 67], [42, 45, 50, 57, 62, 66], [40, 47, 52, 55, 59, 64], [48, 52, 55, 60, 64]] * 3
        for bar, ch in enumerate(shapes):
            for pos, down, v, ln in [(0, True, 96, 1.4), (1.5, False, 70, 0.4), (2, True, 88, 1.0), (3.5, False, 72, 0.4)]:
                order = ch if down else list(reversed(ch[-4:]))
                for i, p in enumerate(order):
                    t, vv = hum(bar * BAR + int(pos * PPQ) + i * 12, v - i * 2, 3, 5)
                    n.append(("GTR", t, p, int(ln * PPQ), vv))
    elif name == "Small Band":  # E minor, a mid-tempo groove
        prog = [(40, [52, 55, 59, 62]), (36, [52, 55, 60, 64]), (43, [50, 55, 59, 62]), (38, [50, 54, 57, 62])] * 3
        for bar, (root, ch) in enumerate(prog):
            t0 = bar * BAR
            for e in range(8):  # hats in eighths, kick 1 and the and of 3, snare 2 and 4
                t, v = hum(t0 + e * PPQ // 2, 92 if e % 2 == 0 else 66, 4, 7)
                n.append(("DRUMS", t, 42, PPQ // 4, v))
            for e, v in [(0, 110), (5, 96)] + ([(7, 84)] if bar % 4 == 3 else []):
                t, vv = hum(t0 + e * PPQ // 2, v, 3, 4)
                n.append(("DRUMS", t, 36, PPQ // 4, vv))
            for q in (1, 3):
                t, v = hum(t0 + q * PPQ, 108, 3, 4)
                n.append(("DRUMS", t, 38, PPQ // 4, v))
            for e, p, ln, v in [(0, root, 3, 100), (3, root, 1, 84), (4, root + 7, 2, 92), (6, root + 12, 1, 86), (7, root + 10, 1, 80)]:
                t, vv = hum(t0 + e * PPQ // 2, v, 4, 5)
                n.append(("BASS", t, p, ln * PPQ // 2 - 30, vv))
            for pos in (0.5, 2.5):  # keys off the beat
                for p in ch:
                    t, v = hum(t0 + int(pos * PPQ), 70, 4, 5)
                    n.append(("KEYS", t, p + 12, int(0.9 * PPQ), v))
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


(OUT / "midi" / "song").mkdir(parents=True, exist_ok=True)
song, tempo_map, at, lines = {}, [], 1, ["# Web demo MIDI (original, written 2026-10-27)", "",
    "Each piece is one loop, 4/4. The Duet project \"Tertiary Web Demo\" holds each piece three times in a row;",
    "render all three and keep the middle pass (see web_tracks.py).", ""]
for name, bpm, bars, tracks in PIECES:
    ev = compose(name)
    start = (at - 1) * BAR
    tempo_map.append((start, bpm))
    lines.append(f"## {name}: {bpm} bpm, {bars} bars a pass, starts at bar {at} in Duet (passes at bars {at}, {at + bars}, {at + 2 * bars})")
    for track, what in tracks.items():
        one = sorted((t, p, ln, v) for tr, t, p, ln, v in ev if tr == track)
        write(one, [(0, bpm)], track, OUT / "midi" / f"{name} - {track}.mid")
        lines.append(f"- `{name} - {track}.mid`: {what}. {len(one)} notes a pass.")
        for k in range(PASSES):
            song.setdefault(f"{name}|{track}", []).extend((start + k * bars * BAR + t, p, ln, v) for t, p, ln, v in one)
    lines.append("")
    at += bars * PASSES + 2
for key, ev in song.items():
    name, track = key.split("|")
    write(sorted(ev), tempo_map, track, OUT / "midi" / "song" / f"{name} - {track}.mid")
(OUT / "midi" / "README.md").write_text("\n".join(lines), encoding="utf-8")
print("\n".join(lines))
