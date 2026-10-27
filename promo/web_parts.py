"""The web demo's keys, organ, strings and pad parts, written the way a player would play each instrument (Joe,
2026-10-27: "just make sure the samples are musically sound. And interesting. Something someone would really play.").

Each function returns (track, tick, pitch, length, velocity) for one pass. A negative pitch is a controller: CC
number -pitch, value in the velocity slot (CC11 expression, for swells). PROGRESSIONS says, in words, what each part
plays; the readme prints it.
"""
import random

PPQ = 480
BAR = PPQ * 4
rnd = random.Random(27102026)


def hum(t, v, dt=6, dv=6):
    return max(0, t + rnd.randint(-dt, dt)), max(1, min(127, v + rnd.randint(-dv, dv)))


def at(bar, beat=0.0):
    return int(bar * BAR + beat * PPQ)


def swell(track, points, step=PPQ // 4):
    """CC11 from (bar, beat, value) points, linear between them, every sixteenth."""
    out = []
    for (b0, q0, v0), (b1, q1, v1) in zip(points, points[1:]):
        t0, t1 = at(b0, q0), at(b1, q1)
        for t in range(t0, t1, step):
            out.append((track, t, -11, 0, int(v0 + (v1 - v0) * (t - t0) / max(1, t1 - t0))))
    b, q, v = points[-1]
    out.append((track, at(b, q), -11, 0, v))
    return out


PROGRESSIONS = {
    "Electric Piano": "C major, 84 bpm, 12 bars: Fmaj9 | Em7 A13 | Dm9 | G13 | Cmaj9 | Bm7b5 E7#9 | Am9 | Am9/G | Fmaj7#11 | Em7 | Dm9 G13sus | Cmaj9. Left hand roots and sevenths; right hand upper-structure voicings pushed on the and of 1, sixteenth ghost notes, and a melody over bars 3 to 4, 7 to 8 and 11 to 12.",
    "Organ": "F major, 62 bpm, 8 bars, slow gospel: F | F/A Bb | Bbmaj7 Bdim7 | F/C D7 | Gm7 | C7sus4 C7 | Am7 Dm7 | Gm7/C F6. Bass on 1 and 3, right-hand chords held and re-struck on the and of 2, pushed in on the and of 4, a blues slip in bars 4 and 8, expression swells into each phrase.",
    "Strings": "D major, 84 bpm, 12 bars, four-part section (cello, viola, second and first violins): D | A/C# | Bm | Bm/A | G | D/F# | Em7 | A7sus4 A7 | Bm | G | Em7 A7 | D. The bass walks down by step, the inner voices move by step or hold, the first violins carry the tune; swells in and out each phrase.",
    "Warm Pad": "A minor, 60 bpm, 8 bars: Am9 | Fmaj7#11 | C/G | Gsus2 G | Fmaj9 | Dm9 | E7sus4 E7 | Am(add9). Chords held under a top line that moves every two beats and resolves to A.",
    "Airy Pad": "D major, 80 bpm, 12 bars, high and open: Dmaj9 | Bm11 | Gmaj9 | Asus4 A | F#m7 | Bm9 | Em9 | Gm6 (borrowed iv) | Dmaj9/A | Em9 | Asus4 A7 | Dmaj9. A top line over the held chords; the sus4s resolve down.",
    "Dark Pad": "C minor, 60 bpm, 8 bars, low and close: Cm9 | Abmaj7 | Fm9 | G7sus4 G7 | Cm/Eb | Dbmaj7#11 (the flat II) | Fm6 | G7sus4 Cm(add9). A slow inner line; it settles on C minor.",
    "Ambient Swell": "E major, 56 bpm, 8 bars, one slow arc: Emaj9 | Emaj9 C#m11 | Amaj7#11 | Amaj7#11 F#m11 | Cmaj7#11 (borrowed bVI) | Bsus4 | Bsus4 B | Emaj9. Voices enter one at a time and overlap, so the texture keeps changing; it rises to the borrowed C and comes home.",
}


def electric_piano():
    T = "EPIANO"
    form = [[("Fmaj9", 0)], [("Em7", 0), ("A13", 2)], [("Dm9", 0)], [("G13", 0)], [("Cmaj9", 0)], [("Bm7b5", 0), ("E7#9", 2)],
            [("Am9", 0)], [("Am9/G", 0)], [("Fmaj7#11", 0)], [("Em7", 0)], [("Dm9", 0), ("G13sus", 2)], [("Cmaj9", 0)]]
    lh = {"Fmaj9": (41, 52), "Em7": (40, 50), "A13": (45, 55), "Dm9": (38, 48), "G13": (43, 53), "Cmaj9": (36, 47), "Bm7b5": (47, 57),
          "E7#9": (40, 50), "Am9": (45, 55), "Am9/G": (43, 55), "Fmaj7#11": (41, 52), "G13sus": (43, 53)}
    rh = {"Fmaj9": (57, 60, 64, 67), "Em7": (55, 59, 62, 66), "A13": (61, 66, 67), "Dm9": (53, 57, 60, 64), "G13": (59, 64, 69),
          "Cmaj9": (59, 62, 64, 67), "Bm7b5": (62, 65, 69), "E7#9": (56, 62, 67), "Am9": (60, 64, 67, 71), "Am9/G": (60, 64, 67, 71),
          "Fmaj7#11": (57, 59, 64), "G13sus": (60, 64, 69)}
    melody = [(2, 3.0, 69, .5), (2, 3.5, 72, .5), (3, 0, 74, 1.5), (3, 1.5, 72, .5), (3, 2, 69, 1), (3, 3, 67, 1),
              (6, 3.0, 71, .5), (6, 3.5, 72, .5), (7, 0, 76, 1.5), (7, 1.5, 74, .5), (7, 2, 72, 1), (7, 3, 71, 1), (8, 0, 72, 2), (8, 2.5, 69, .5), (8, 3, 67, 1),
              (10, 3.0, 67, .5), (10, 3.5, 69, .5), (11, 0, 72, 1), (11, 1, 74, .5), (11, 1.5, 72, .5), (11, 2, 69, 2)]
    sing = {b for b, *_ in melody}
    dyn = [0.85, 0.85, 0.9, 0.95, 0.9, 0.95, 1.0, 1.05, 1.0, 0.95, 1.0, 0.8]
    n = []
    for bar, chords in enumerate(form):
        last = bar == len(form) - 1
        for i, (ch, start) in enumerate(chords):
            span = (4 - start) if i == len(chords) - 1 else chords[i + 1][1] - start
            d = dyn[bar]
            for k, p in enumerate(lh[ch]):   # left hand: root and seventh, the seventh a hair late
                t, v = hum(at(bar, start) + k * 12, int(78 * d), 5, 4)
                n.append((T, t, p, int(span * PPQ) - 40 if not last else 2 * BAR, v))
            if last:
                for k, p in enumerate(rh[ch]):
                    t, v = hum(at(bar, 0.5) + k * 14, int(70 * d), 4, 4)
                    n.append((T, t, p, 2 * BAR - PPQ, v))
                continue
            # right hand: pushed on the and of the first beat of the chord, a short stab on the last beat
            for k, p in enumerate(rh[ch]):
                t, v = hum(at(bar, start + 0.5) + k * 6, int(74 * d), 5, 5)
                n.append((T, t, p, int(PPQ * 1.2), v))
            if span >= 4 and bar not in sing:
                for k, p in enumerate(rh[ch]):
                    t, v = hum(at(bar, 3.0) + k * 5, int(64 * d), 5, 5)
                    n.append((T, t, p, PPQ // 2, v))
                t, v = hum(at(bar, 2.25), 28, 4, 4)            # a ghost note: the top voice, barely touched
                n.append((T, t, rh[ch][-1], PPQ // 6, v))
                t, v = hum(at(bar, 2.75), 32, 4, 4)
                n.append((T, t, rh[ch][-2], PPQ // 6, v))
    for bar, beat, p, l in melody:
        t, v = hum(at(bar, beat), int(86 * dyn[bar]), 6, 5)
        n.append((T, t, p, int(l * PPQ) - 20, v))
    return n


def organ():
    T = "ORGAN"
    form = [[("F", 0)], [("F/A", 0), ("Bb", 2)], [("Bbmaj7", 0), ("Bdim7", 2)], [("F/C", 0), ("D7", 2)],
            [("Gm7", 0)], [("C7sus4", 0), ("C7", 2)], [("Am7", 0), ("Dm7", 2)], [("Gm7/C", 0), ("F6", 2)]]
    rh = {"F": (57, 60, 65, 69), "F/A": (57, 60, 65), "Bb": (58, 62, 65), "Bbmaj7": (57, 62, 65, 69), "Bdim7": (59, 62, 65, 68),
          "F/C": (60, 65, 69), "D7": (57, 60, 62, 66), "Gm7": (58, 62, 65, 67), "C7sus4": (58, 60, 65, 67), "C7": (58, 60, 64, 67),
          "Am7": (57, 60, 64, 67), "Dm7": (57, 60, 62, 65), "Gm7/C": (58, 62, 65, 69), "F6": (57, 60, 62, 65, 69)}
    bass = {"F": 41, "F/A": 45, "Bb": 46, "Bbmaj7": 46, "Bdim7": 47, "F/C": 48, "D7": 50, "Gm7": 43, "C7sus4": 48, "C7": 48,
            "Am7": 45, "Dm7": 50, "Gm7/C": 48, "F6": 41}
    n = []
    for bar, chords in enumerate(form):
        last = bar == len(form) - 1
        for i, (ch, start) in enumerate(chords):
            span = (4 - start) if i == len(chords) - 1 else chords[i + 1][1] - start
            push = 0.5 if (bar in (1, 3, 5) and start == 0) else 0.0   # pushed in on the and of the beat before
            for k, p in enumerate(rh[ch]):
                t, v = hum(at(bar, start) - int(push * PPQ), 88, 4, 3)
                n.append((T, max(0, t), p, int((span + push) * PPQ) - (PPQ // 2 if span >= 4 and not last else 30), v))
            if span >= 4 and not last:   # re-struck, short, on the and of 2, then held from 3
                for p in rh[ch]:
                    t, v = hum(at(bar, 1.5), 80, 4, 3)
                    n.append((T, t, p, PPQ // 3, v))
                for p in rh[ch]:
                    t, v = hum(at(bar, 2.0), 86, 4, 3)
                    n.append((T, t, p, 2 * PPQ - 30, v))
            for q in ([0, 1.5] if span >= 2 else [0]):    # bass on the chord and a pickup
                if start + q < 4:
                    t, v = hum(at(bar, start + q), 92 if q == 0 else 74, 4, 3)
                    n.append((T, t, bass[ch] - 12 if q == 0 else bass[ch], int(PPQ * (1.4 if q == 0 else 0.45)), v))
        if bar in (3, 7):   # a blues slip on the top: the minor third falling into the major
            for beat, p in ((3.5, 68), (3.75, 69)):
                t, v = hum(at(bar, beat), 82, 3, 3)
                n.append((T, t, p, PPQ // 3, v))
    # swells: each two-bar phrase leans in and backs off
    n += swell(T, [(0, 0, 80), (1, 2, 118), (2, 0, 92), (3, 2, 122), (4, 0, 90), (5, 2, 120), (6, 0, 96), (7, 2, 124), (7, 3.9, 100)])
    return n


def strings():
    T = "STRINGS"
    # (bar, beat, beats) per chord; each voice's note per chord; voice leading by step
    chords = [(0, 0, 4), (1, 0, 4), (2, 0, 4), (3, 0, 4), (4, 0, 4), (5, 0, 4), (6, 0, 4), (7, 0, 2), (7, 2, 2),
              (8, 0, 4), (9, 0, 4), (10, 0, 2), (10, 2, 2), (11, 0, 4)]
    cello = [50, 49, 47, 45, 43, 42, 40, 45, 45, 47, 43, 40, 45, 38]
    viola = [57, 57, 54, 54, 55, 57, 55, 57, 55, 54, 55, 55, 55, 54]
    vln2 = [62, 64, 62, 61, 62, 62, 64, 62, 61, 62, 59, 59, 61, 62]
    vln1 = [(0, 0, 66, 4), (1, 0, 64, 2), (1, 2, 69, 2), (2, 0, 66, 1), (2, 1, 71, 3), (3, 0, 69, 2), (3, 2, 73, 2),
            (4, 0, 74, 2), (4, 2, 71, 2), (5, 0, 69, 4), (6, 0, 67, 2), (6, 2, 71, 2), (7, 0, 69, 4),
            (8, 0, 71, 2), (8, 2, 74, 2), (9, 0, 76, 2), (9, 2, 74, 2), (10, 0, 71, 2), (10, 2, 73, 2), (11, 0, 74, 4)]
    dyn = [0.75, 0.8, 0.88, 0.95, 1.0, 0.9, 0.85, 0.9, 0.95, 1.0, 1.05, 0.8]   # per bar: two phrases, the second bigger
    n = []
    for k, (bar, beat, beats) in enumerate(chords):
        last = k == len(chords) - 1
        for voice, base, late in ((cello, 76, 0), (viola, 68, 10), (vln2, 66, 18)):
            p = voice[k]
            held = k > 0 and voice[k - 1] == p        # the same note again: re-bowed, softer
            t, v = hum(at(bar, beat) + late, int(base * dyn[bar] * (0.85 if held else 1)), 6, 3)
            n.append((T, t, p, int(beats * PPQ) - 50 if not last else 2 * BAR, v))
    for bar, beat, p, l in vln1:
        t, v = hum(at(bar, beat) + 6, int(80 * dyn[bar]), 6, 3)
        n.append((T, t, p, int(l * PPQ) - 50 if bar < 11 else 2 * BAR, v))
    # in and out with each phrase, biggest in bar 10
    n += swell(T, [(0, 0, 60), (1, 2, 100), (3, 3, 84), (4, 0, 96), (5, 2, 112), (7, 3, 86), (8, 0, 98), (10, 0, 124), (11, 0, 104), (11, 3.9, 70)])
    return n


def pad(track, chords, line, dyn_points, vel=70, roll=14):
    """Held chords (bar, beat, beats, pitches) under a top line (bar, beat, pitch, beats); the last chord and the
    line's last note ring on past the end, into the loop's next pass."""
    n = []
    end = max(b for b, *_ in line)
    for i, (bar, beat, beats, ch) in enumerate(chords):
        last = i == len(chords) - 1
        for k, p in enumerate(ch):
            t, v = hum(at(bar, beat) + k * roll, vel, 4, 4)
            n.append((track, t, p, int(beats * PPQ) - 30 if not last else 2 * BAR, v))
    for bar, beat, p, l in line:
        t, v = hum(at(bar, beat), vel + 8, 6, 4)
        n.append((track, t, p, 2 * BAR if (bar, beat) == (end, max(q for b, q, *_ in line if b == end)) else int(l * PPQ) - 30, v))
    return n + swell(track, dyn_points)


def warm_pad():
    ch = [(0, 0, 4, (45, 52, 55, 59)), (1, 0, 4, (41, 52, 57, 59)), (2, 0, 4, (43, 52, 55, 60)), (3, 0, 2, (43, 50, 55, 57)), (3, 2, 2, (43, 50, 55, 59)),
          (4, 0, 4, (41, 48, 52, 55)), (5, 0, 4, (38, 48, 52, 53)), (6, 0, 2, (40, 50, 52, 57)), (6, 2, 2, (40, 50, 52, 56)), (7, 0, 4, (45, 52, 59, 60))]
    line = [(0, 0, 72, 2), (0, 2, 71, 2), (1, 0, 69, 4), (2, 0, 67, 2), (2, 2, 64, 2), (3, 0, 69, 2), (3, 2, 71, 2),
            (4, 0, 72, 4), (5, 0, 74, 2), (5, 2, 72, 2), (6, 0, 71, 2), (6, 2, 68, 2), (7, 0, 69, 4)]
    return pad("WARM PAD", ch, line, [(0, 0, 80), (2, 0, 108), (3, 3, 92), (5, 0, 118), (6, 3, 104), (7, 3.9, 78)])


def airy_pad():
    ch = [(0, 0, 4, (50, 57, 61, 64, 66)), (1, 0, 4, (47, 57, 61, 64, 66)), (2, 0, 4, (43, 57, 59, 62, 66)), (3, 0, 2, (45, 57, 62, 64)), (3, 2, 2, (45, 57, 61, 64)),
          (4, 0, 4, (42, 57, 61, 64)), (5, 0, 4, (47, 57, 61, 62, 66)), (6, 0, 4, (40, 55, 59, 62, 66)), (7, 0, 4, (43, 58, 62, 64)),
          (8, 0, 4, (45, 57, 61, 64, 66)), (9, 0, 4, (40, 55, 59, 62, 66)), (10, 0, 2, (45, 57, 62, 64)), (10, 2, 2, (45, 55, 61, 64)), (11, 0, 4, (50, 57, 61, 64, 66))]
    line = [(0, 0, 73, 2), (0, 2, 76, 2), (1, 0, 74, 4), (2, 0, 74, 2), (2, 2, 78, 2), (3, 0, 76, 2), (3, 2, 73, 2), (4, 0, 73, 4),
            (5, 0, 74, 2), (5, 2, 71, 2), (6, 0, 71, 2), (6, 2, 74, 2), (7, 0, 70, 4), (8, 0, 69, 2), (8, 2, 73, 2),
            (9, 0, 71, 2), (9, 2, 74, 2), (10, 0, 74, 2), (10, 2, 73, 2), (11, 0, 74, 4)]
    return pad("AIRY PAD", ch, line, [(0, 0, 76), (3, 0, 104), (4, 0, 92), (7, 0, 120), (8, 0, 100), (10, 0, 116), (11, 3.9, 80)], vel=66, roll=18)


def dark_pad():
    ch = [(0, 0, 4, (36, 43, 51, 55, 62)), (1, 0, 4, (44, 48, 51, 55)), (2, 0, 4, (41, 48, 51, 55, 56)), (3, 0, 2, (43, 48, 50, 53)), (3, 2, 2, (43, 47, 50, 53)),
          (4, 0, 4, (39, 48, 51, 55)), (5, 0, 4, (37, 48, 53, 55)), (6, 0, 4, (41, 48, 50, 56)), (7, 0, 2, (43, 48, 50, 53)), (7, 2, 2, (36, 51, 55, 62))]
    line = [(0, 0, 62, 4), (1, 0, 63, 2), (1, 2, 67, 2), (2, 0, 68, 4), (3, 0, 67, 2), (3, 2, 65, 2), (4, 0, 63, 4),
            (5, 0, 65, 2), (5, 2, 67, 2), (6, 0, 68, 2), (6, 2, 62, 2), (7, 0, 62, 2), (7, 2, 63, 2)]
    return pad("DARK PAD", ch, line, [(0, 0, 84), (2, 0, 110), (3, 3, 96), (5, 0, 120), (6, 3, 100), (7, 3.9, 82)], vel=72, roll=10)


def ambient_swell():
    """Voices enter one at a time, two beats apart, and ring into the next chord."""
    T = "PAD"
    form = [(0, 0, 8, (40, 47, 56, 63, 66)), (1, 2, 6, (37, 49, 56, 59, 66)), (2, 0, 6, (45, 52, 56, 63, 68)), (3, 2, 6, (42, 52, 57, 59, 64)),
            (4, 0, 8, (36, 48, 55, 59, 66)), (5, 0, 6, (47, 54, 59, 64)), (6, 2, 6, (47, 54, 59, 63, 66)), (7, 0, 8, (40, 47, 54, 59, 63, 68))]
    n = []
    for i, (bar, beat, beats, ch) in enumerate(form):
        last = i == len(form) - 1
        for k, p in enumerate(ch):
            enter = beat + 0.5 * k * (2 if k else 0)   # the bass first, then a voice every beat
            t, v = hum(at(bar, enter), 60 + 4 * k, 10, 4)
            n.append((T, t, p, int((beats - (enter - beat)) * PPQ) if not last else 2 * BAR, v))
    top = [(1, 0, 71, 2), (2, 2, 73, 2), (3, 3, 71, 1), (4, 2, 74, 2), (5, 2, 71, 2), (6, 3, 70, 1), (7, 1, 71, 3)]
    for bar, beat, p, l in top:
        t, v = hum(at(bar, beat), 66, 8, 4)
        n.append((T, t, p, int(l * PPQ) + PPQ, v))
    return n + swell(T, [(0, 0, 60), (2, 0, 96), (4, 0, 124), (5, 2, 110), (7, 0, 92), (7, 3.9, 64)])


PARTS = {"Electric Piano": electric_piano, "Organ": organ, "Strings": strings, "Warm Pad": warm_pad, "Airy Pad": airy_pad,
         "Dark Pad": dark_pad, "Ambient Swell": ambient_swell}
