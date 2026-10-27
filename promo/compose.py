"""Original MIDI for the Tertiary promo (Joe, 2026-10-26: "let's get in the habit of generating unique midi tracks
for content"). Four short pieces in 4/4, each chosen to show one thing Tertiary does:

  P1 Pads      84 bpm  D minor pad over held roots: three bands, three rhythms.
  P2 Arp       96 bpm  clean guitar arpeggio over bass: the low end left alone.
  P3 Stabs     80 bpm  short guitar chords into a big room: a reverb tail that moves.
  P4 Groove   104 bpm  drums, bass line and pad: rhythm cut from the pad's roots.

Writes, under OUT:
  midi/<piece>/<piece> - <TRACK>.mid   one file per part, from bar 1, for Kontakt or any DAW tomorrow
  midi/song/<TRACK>.mid                each track over the whole running order at its song position, with the
                                       tempo map, for importing into the Duet project "Tertiary Promo"
  midi/README.md                        which instrument each part is for
"""
import random
import sys
from pathlib import Path

import mido

OUT = Path(sys.argv[1])
PPQ = 480
BAR = PPQ * 4
random.seed(26102026)

# Running order in the Duet project: piece, first bar, bars of music, bpm. A two-bar gap after each for the tails.
PIECES = [("P1 Pads", 1, 8, 84), ("P2 Arp", 11, 8, 96), ("P3 Stabs", 21, 8, 80), ("P4 Groove", 31, 8, 104)]
FIRST = {p: b for p, b, _, _ in PIECES}


def hum(t, v, dt=8, dv=8):
    return max(0, t + random.randint(-dt, dt)), max(1, min(127, v + random.randint(-dv, dv)))


notes = {}  # (piece, track) -> [(tick from piece start, pitch, length, velocity)]


def add(piece, track, t, p, ln, v):
    notes.setdefault((piece, track), []).append((t, p, ln, v))


# ---- P1 Pads: one chord a bar, a top voice for the high band, held roots for the low band.
p1 = [([50, 57, 60, 64, 65], 76, 38),   # Dm9
      ([46, 53, 57, 62], 74, 34),       # Bbmaj7
      ([43, 50, 53, 57, 58], 74, 31),   # Gm9
      ([45, 52, 55, 62], 76, 33),       # A7sus4
      ([50, 57, 60, 64, 65], 77, 38),   # Dm9, top up to F
      ([48, 53, 57, 64], 76, 36),       # Fmaj7/C
      ([43, 53, 58, 60, 62], 77, 31),   # Gm11
      ([45, 50, 52, 57], 76, 33)]       # Asus4, resolving to A on beat 3
for bar, (chord, top, root) in enumerate(p1):
    t0 = bar * BAR
    for i, n in enumerate(chord):
        t, v = hum(t0 + i * 6, 72, 4, 5)
        if bar == 7 and n == 50:  # sus4 to major third on beat 3
            add("P1 Pads", "PAD", t, 50, PPQ * 2 - 20, v)
            add("P1 Pads", "PAD", t0 + PPQ * 2, 49, PPQ * 2 + PPQ, v)
            continue
        add("P1 Pads", "PAD", t, n, BAR + (PPQ if bar == 7 else 0) - 20, v)
    t, v = hum(t0 + 30, 64, 4, 4)
    add("P1 Pads", "PAD", t, top, BAR - 40, v)
    t, v = hum(t0, 92, 3, 4)
    add("P1 Pads", "BASS", t, root, BAR + (PPQ if bar == 7 else 0) - 30, v)

# ---- P2 Arp: eighth-note arpeggio, up and back down through a five-note voicing; roots in the bass.
p2 = [([40, 47, 54, 55, 62], 28),   # Em9
      ([48, 55, 59, 64, 66], 36),   # Cmaj7#11
      ([47, 50, 55, 59, 64], 35),   # G6/B
      ([50, 57, 62, 64, 69], 38),   # Dsus2
      ([40, 47, 54, 55, 62], 28),   # Em9
      ([48, 55, 59, 64, 66], 36),   # Cmaj7#11
      ([45, 52, 55, 59, 62], 33),   # Am9
      ([47, 54, 59, 63, 66], 35)]   # B (Bmaj add4 colour on top)
order = [0, 1, 2, 3, 4, 3, 2, 1]
for bar, (voicing, root) in enumerate(p2):
    t0 = bar * BAR
    for step, idx in enumerate(order):
        accent = 92 if step in (0, 4) else 78
        t, v = hum(t0 + step * PPQ // 2, accent, 6, 6)
        add("P2 Arp", "GTR", t, voicing[idx], PPQ // 2 * 3, v)  # let notes ring over the next two
    t, v = hum(t0, 96, 3, 4)
    add("P2 Arp", "BASS", t, root, PPQ * 3 - 30, v)
    t, v = hum(t0 + PPQ * 3 + PPQ // 2, 84, 4, 5)
    add("P2 Arp", "BASS", t, root + 12, PPQ // 2 - 30, v)

# ---- P3 Stabs: one short strummed chord a bar, beat one, into the Room; the tails do the work.
p3 = [[48, 52, 55, 62, 64],        # Cadd9
      [40, 47, 52, 55, 62],        # Em7
      [45, 52, 55, 60, 64],        # Am7
      [41, 48, 52, 57, 59, 64],    # Fmaj7#11
      [48, 52, 55, 62, 64],        # Cadd9
      [40, 47, 52, 55, 62],        # Em7
      [41, 48, 52, 57, 64],        # Fmaj7
      [43, 47, 50, 55, 59, 64]]    # G6
for bar, chord in enumerate(p3):
    t0 = bar * BAR
    for i, n in enumerate(chord):  # a downstroke: low string first, 14 ticks apart
        t, v = hum(t0 + i * 14, 98 - i * 3, 2, 4)
        add("P3 Stabs", "GTR", t, n, PPQ // 2, v)
    if bar % 2 == 1:  # an upstroke ghost on the and of two, every other bar
        for i, n in enumerate(reversed(chord[-3:])):
            t, v = hum(t0 + PPQ + PPQ // 2 + i * 12, 58, 2, 4)
            add("P3 Stabs", "GTR", t, n, PPQ // 4, v)

# ---- P4 Groove: F#m, D, A, E, twice. Pad holds the chords, bass and drums drive.
p4 = [([54, 57, 61, 64, 69], 30), ([50, 57, 61, 66, 69], 38), ([49, 52, 57, 64, 69], 33), ([52, 56, 59, 64, 71], 28)] * 2
KICK, SNARE, HH, OH, CRASH, RIDE = 36, 38, 42, 46, 49, 51
for bar, (chord, root) in enumerate(p4):
    t0 = bar * BAR
    for n in chord:
        t, v = hum(t0, 70, 4, 4)
        add("P4 Groove", "PAD", t, n, BAR - 20, v)
    # bass: root on 1, octave on the and of 1, root on the and of 2 and 3, a walk up into the next bar
    nxt = p4[(bar + 1) % len(p4)][1]
    walk = root + (2 if nxt > root else -2)
    for beat8, p, ln, v in [(0, root, 1, 100), (1, root + 12, 1, 82), (3, root, 1, 92), (4, root, 2, 98), (6, root, 1, 88), (7, walk, 1, 84)]:
        t, vv = hum(t0 + beat8 * PPQ // 2, v, 5, 5)
        add("P4 Groove", "BASS", t, p, ln * PPQ // 2 - 25, vv)
    # drums
    for beat8 in range(8):
        acc = 96 if beat8 % 2 == 0 else 70
        if bar % 4 == 3 and beat8 == 7:
            t, v = hum(t0 + beat8 * PPQ // 2, 90, 4, 5)
            add("P4 Groove", "DRUMS", t, OH, PPQ // 2, v)
        else:
            t, v = hum(t0 + beat8 * PPQ // 2, acc, 5, 8)
            add("P4 Groove", "DRUMS", t, HH, PPQ // 4, v)
    for beat8, v in [(0, 112), (3, 96), (4, 108)] + ([(7, 88)] if bar % 2 == 1 else []):
        t, vv = hum(t0 + beat8 * PPQ // 2, v, 4, 5)
        add("P4 Groove", "DRUMS", t, KICK, PPQ // 4, vv)
    for beat in (1, 3):
        t, v = hum(t0 + beat * PPQ, 110, 4, 4)
        add("P4 Groove", "DRUMS", t, SNARE, PPQ // 4, v)
    if bar == 0 or bar == 4:
        add("P4 Groove", "DRUMS", t0, CRASH, PPQ, 104)
    if bar == 7:  # a snare fill into the loop point
        for i, s16 in enumerate(range(12, 16)):
            t, v = hum(t0 + s16 * PPQ // 4, 70 + i * 10, 3, 4)
            add("P4 Groove", "DRUMS", t, SNARE, PPQ // 8, v)


def track_file(events, tempos, name):
    """events: [(abs tick, pitch, length, velocity)]; tempos: [(abs tick, bpm)]."""
    mf = mido.MidiFile(ticks_per_beat=PPQ)
    tr = mido.MidiTrack()
    mf.tracks.append(tr)
    tr.append(mido.MetaMessage("track_name", name=name, time=0))
    msgs = [(t, 0, mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(b), time=0)) for t, b in tempos]
    msgs += [(0, 0, mido.MetaMessage("time_signature", numerator=4, denominator=4, time=0))]
    ch = 9 if name == "DRUMS" else 0
    for t, p, ln, v in events:
        msgs.append((t, 2, mido.Message("note_on", note=p, velocity=v, channel=ch, time=0)))
        msgs.append((t + max(10, ln), 1, mido.Message("note_off", note=p, velocity=0, channel=ch, time=0)))
    msgs.sort(key=lambda m: (m[0], m[1]))
    last = 0
    for t, _, m in msgs:
        m.time = t - last
        last = t
        tr.append(m)
    tr.append(mido.MetaMessage("end_of_track", time=0))
    return mf


INSTR = {
    "PAD": "HALion Sonic, the PAD sound from Octavio (any warm pad; Kontakt pad or strings tomorrow)",
    "BASS": "Kontakt 8, the BASS from Octavio through Helix Native (a finger or pick bass library tomorrow)",
    "GTR": "Kontakt 8, the clean guitar from Octavio (a clean electric library tomorrow)",
    "DRUMS": "SSD5 (General MIDI drum notes: kick 36, snare 38, closed hat 42, open hat 46, crash 49)",
}
(OUT / "midi" / "song").mkdir(parents=True, exist_ok=True)
lines = ["# Tertiary promo MIDI (original, written 2026-10-26)", "",
         "Each piece is in 4/4. Per-piece files start at bar 1 with the piece's tempo. `song/` holds each track over",
         "the whole running order of the Duet project \"Tertiary Promo\" (P1 bar 1, P2 bar 11, P3 bar 21, P4 bar 31).", ""]
for piece, first, bars, bpm in PIECES:
    d = OUT / "midi" / piece
    d.mkdir(parents=True, exist_ok=True)
    lines.append(f"## {piece}, {bpm} bpm, {bars} bars")
    for (pc, track), ev in sorted(notes.items()):
        if pc != piece:
            continue
        track_file(ev, [(0, bpm)], track).save(d / f"{piece} - {track}.mid")
        lines.append(f"- `{piece} - {track}.mid`: {INSTR[track]}. {len(ev)} notes.")
    lines.append("")
tempo_map = [((first - 1) * BAR, bpm) for _, first, _, bpm in PIECES]
for track in INSTR:
    ev = [((FIRST[pc] - 1) * BAR + t, p, ln, v) for (pc, tr), evs in notes.items() if tr == track for t, p, ln, v in evs]
    track_file(sorted(ev), tempo_map, track).save(OUT / "midi" / "song" / f"{track}.mid")
(OUT / "midi" / "README.md").write_text("\n".join(lines), encoding="utf-8")
print("\n".join(lines))
