"""Files for the Cubase "Tertiary harness" project (Claude, 2026-10-29, for Joe's per-DAW release test).
Lives in the Tertiary repo at plugin/tests/daw/cubase/ (moved from the Dropbox pipeline tools, 2026-10-29). Paths
default to the harness folder next to this script ("Tertiary harness/"), so no arguments are needed.

    python tertiary_harness_build.py audio   [HostTest.exe]          Audio/harness-input.wav, rebuilt from HostTest
                                                                     --write-input (the golden renders' input) tiled
                                                                     to 64 s. Byte-identical to the committed project's
                                                                     audio; the WAV itself is not in git.
    python tertiary_harness_build.py input   <2 s input.wav> <out.wav>   tile a given 2 s input to 64 s
    python tertiary_harness_build.py tempo   [out.smt]                   the tempo map below as a Cubase Tempo Track file
                                                                         (default Misc/harness-tempo.smt)
    python tertiary_harness_build.py archive [template.xml] [out.xml]    the harness track: the template's audio track
                                                                         and Tertiary insert, with the automation below
                                                                         (default Misc/learn-3.xml -> Misc/harness-track.xml)
    python tertiary_harness_build.py params                              Tertiary's parameter IDs and Cubase lane tags
    python tertiary_harness_build.py playaudio                           [2026-10-30] Audio/play-loop.wav, the play
                                                                         track's music (not in git; numpy and scipy)
    python tertiary_harness_build.py playarchive [template] [out]        [2026-10-30] Misc/play-track.xml, the muted
                                                                         "Tertiary – play" track for Joe (see below)

The song (4/4, 480 ticks per quarter, 1920 per bar), bar 1 to bar 13, about 24.8 s:
    bars 1-4    120 BPM
    bars 5-6    ramp 120 -> 90 (written as 32 steps, one per 1/16 note)
    bars 7-8    90 BPM
    bars 9-12   140 BPM (a jump)
It is kept under 50 s on purpose: an unactivated Tertiary (demo mode) bypasses itself after 50 s of processing,
counted from when the plug-in is created, so a longer render would not be repeatable.

Automation (Low band and the low/mid crossover), as (bar, value 0..1); a step is two points one tick apart:
    Depth Low Band            0.42 -> 1.0 by bar 3, held, down to 0.3 at bar 11
    Sync to Host Low Band     on; off from bar 7 to bar 10 (the free rate runs there)
    Rate Low Band             0.15 at bar 7 rising to 0.6 at bar 10
    Bypassed Low Band         on for bar 3 only
    Low-Mid Crossover Freq    0.327 -> 0.6 at bar 6 -> back to 0.327 at bar 12
    Input Gain                0.5 (0 dB) throughout

Cubase's lane tag for a parameter of the plug-in in insert slot 1 is the VST3 parameter ID + 4201; JUCE's VST3
parameter ID is juce::String::hashCode() of the parameter ID, masked to 31 bits (found 2026-10-29 from Cubase's own
Selected Tracks export, Misc/learn-3.xml). The template is that export: the track as Cubase wrote it.
"""
import glob, os, re, struct, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
HARNESS = os.path.join(HERE, "Tertiary harness")
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))

PPQ = 480
BAR = 4 * PPQ
SONG_END_BAR = 13
LANE_OFFSET = 4201


def bar(b, extra=0):
    return (b - 1) * BAR + extra


def tempo_events():
    ev = [(0, 120.0)]
    start, end = bar(5), bar(7)
    steps = (end - start) // (PPQ // 4)
    for k in range(steps):
        ev.append((start + k * (PPQ // 4), 120.0 + (90.0 - 120.0) * k / steps))
    ev.append((bar(7), 90.0))
    ev.append((bar(9), 140.0))
    return ev


def seconds_at(tick):
    """Seconds from the song start to a tick, for the stepped map above."""
    ev = tempo_events() + [(10 ** 9, 0)]
    s = 0.0
    for (t0, bpm), (t1, _) in zip(ev, ev[1:]):
        if tick <= t0:
            break
        s += (min(tick, t1) - t0) / PPQ * 60.0 / bpm
    return s


AUTOMATION = {
    "Depth Low Band":         [(bar(1), 0.42), (bar(3), 1.0), (bar(10), 1.0), (bar(11), 0.3), (bar(13), 0.3)],
    "Sync to Host Low Band":  [(bar(1), 1.0), (bar(7), 1.0), (bar(7, 1), 0.0), (bar(10), 0.0), (bar(10, 1), 1.0)],
    "Rate Low Band":          [(bar(1), 0.2083333), (bar(7), 0.15), (bar(10), 0.6), (bar(13), 0.6)],
    "Bypassed Low Band":      [(bar(1), 0.0), (bar(3), 0.0), (bar(3, 1), 1.0), (bar(4), 1.0), (bar(4, 1), 0.0)],
    "Low-Mid Crossover Freq": [(bar(1), 0.3267335), (bar(6), 0.6), (bar(12), 0.3267335)],
    "Input Gain":             [(bar(1), 0.5)],
}

EVENT = ('                        <obj class="MParamEvent" ID="{id}">\n'
         '                           <float name="Start" value="{tick}"/>\n'
         '                           <float name="Value" value="{value}"/>\n'
         '                        </obj>')


def juce_hash(s):
    """juce::String::hashCode (31 * h + char, 32-bit), masked to 31 bits as JUCE's VST3 wrapper does."""
    h = 0
    for c in s:
        h = (31 * h + ord(c)) & 0xFFFFFFFF
    return h & 0x7FFFFFFF


def write_smt(path):
    ids = iter(range(710000000, 720000000))
    nid = lambda: next(ids)
    evs = "\n".join('         <obj class="MTempoEvent" ID="%d">\n            <float name="BPM" value="%.6f"/>\n'
                    '            <float name="PPQ" value="%d"/>\n         </obj>' % (nid(), bpm, t)
                    for t, bpm in tempo_events())
    tempo_id, sig_id, tvc1, tv1, tvc2, tv2, sig_ev = (nid() for _ in range(7))

    def attrs(tlid, tvc, tv):
        return ('      <member name="Additional Attributes">\n'
                '         <int name="TLID" value="%d"/>\n         <int name="LocS" value="0"/>\n'
                '         <int name="Farb" value="5"/>\n'
                '         <obj class="MTrackVariationCollection" name="TVCi" ID="%d">\n'
                '            <int name="ownership" value="2"/>\n            <list name="obj" type="obj">\n'
                '               <obj class="MTrackVariation" ID="%d">\n'
                '                  <string name="name" value="v1" wide="true"/>\n'
                '                  <int name="variationID" value="0"/>\n               </obj>\n'
                '            </list>\n         </obj>\n         <int name="Lock" value="0"/>\n'
                '         <int name="Eths" value="-260498236"/>\n      </member>') % (tlid, tvc, tv)

    xml = ('<?xml version="1.0" encoding="utf-8"?>\n<MasterTrack>\n   <rootObjects>\n'
           '      <root name="Tempo Track" ID="%d"/>\n      <root name="Signature Track" ID="%d"/>\n'
           '   </rootObjects>\n\n   <obj class="MTempoTrackEvent" ID="%d">\n      <list name="TempoEvent" type="obj">\n'
           '%s\n      </list>\n%s\n      <int name="Flags" value="1"/>\n   </obj>\n\n'
           '   <obj class="MSignatureTrackEvent" ID="%d">\n      <list name="SignatureEvent" type="obj">\n'
           '         <obj class="MTimeSignatureEvent" ID="%d">\n            <int name="Flags" value="8"/>\n'
           '            <float name="Start" value="0"/>\n            <float name="Length" value="1"/>\n'
           '            <int name="Bar" value="0"/>\n            <int name="Numerator" value="4"/>\n'
           '            <int name="Denominator" value="4"/>\n            <int name="Position" value="0"/>\n'
           '         </obj>\n      </list>\n%s\n      <int name="Flags" value="1"/>\n   </obj>\n</MasterTrack>\n'
           ) % (tempo_id, sig_id, tempo_id, evs, attrs(1, tvc1, tv1), sig_id, sig_ev, attrs(2, tvc2, tv2))
    open(path, "w", encoding="utf-8").write(xml)
    print("%s: %d tempo events; song end (bar %d) at %.3f s" % (path, len(tempo_events()), SONG_END_BAR,
                                                                 seconds_at(bar(SONG_END_BAR))))


def write_archive(template, out):
    """Cubase's own Selected Tracks export of the harness track (audio track, Tertiary in insert 1, one lane per
    parameter above) with each lane's events replaced. Everything else stays exactly as Cubase wrote it."""
    t = open(template, encoding="utf-8").read()
    tags = {juce_hash(p) + LANE_OFFSET: p for p in AUTOMATION}
    marker = '<obj class="MAutomationTrackEvent"'
    parts = t.split(marker)
    ids = iter(range(1900000000, 1999999999))
    done = set()
    for i in range(1, len(parts)):
        body = parts[i]
        found = re.findall(r'<int name="Tag" value="(\d+)"/>', body)
        tag = int(found[-1]) if found else None      # the lane's own Tag is the last one before the next lane
        if tag not in tags:
            continue
        name = tags[tag]
        events = "\n".join(EVENT.format(id=next(ids), tick=tick, value=value) for tick, value in AUTOMATION[name])
        new_list = '<list name="Events" type="obj">\n' + events + '\n                     </list>'
        body, n = re.subn(r'<list name="Events" type="obj">.*?</list>', lambda m: new_list, body, count=1, flags=re.S)
        if n == 0:
            raise SystemExit("lane %s has no events in the template; draw one point in it and export again" % name)
        parts[i] = body
        done.add(name)
    missing = set(AUTOMATION) - done
    if missing:
        raise SystemExit("lanes not found in the template: " + ", ".join(sorted(missing)))
    open(out, "w", encoding="utf-8").write(marker.join(parts))
    print("%s: %d lanes written (%s)" % (out, len(done), ", ".join(sorted(done))))


def tile_input(src, dst, seconds=64):
    d = open(src, "rb").read()
    i, fmt, data = 12, None, None
    while i + 8 <= len(d):
        cid, size = d[i:i + 4], struct.unpack("<I", d[i + 4:i + 8])[0]
        if cid == b"fmt ":
            fmt = d[i + 8:i + 8 + size]
        elif cid == b"data":
            data = d[i + 8:i + 8 + size]
        i += 8 + size + (size & 1)
    tag, ch, rate, _, align, bits = struct.unpack("<HHIIHH", fmt[:16])
    frames = int(seconds * rate)
    reps = -(-frames * align // len(data))
    body = (data * reps)[:frames * align]
    out = b"fmt " + struct.pack("<I", 16) + fmt[:16] + b"data" + struct.pack("<I", len(body)) + body
    open(dst, "wb").write(b"RIFF" + struct.pack("<I", 4 + len(out)) + b"WAVE" + out)
    print("%s: %d s, %d Hz, %d ch, %d-bit (format %d), the 2 s golden input repeated" % (dst, seconds, rate, ch, bits, tag))


def find_hosttest():
    """This repo's HostTest build first (run_all.ps1 builds it in build-tests/), then the newest in any Tertiary
    worktree next to this one."""
    exe = "HostTest.exe" if os.name == "nt" else "HostTest"
    for b in ("build-tests", "build"):
        for c in glob.glob(os.path.join(REPO, b, "plugin", "HostTest_artefacts", "*", exe)):
            return c
    gh = os.path.dirname(REPO)
    cands = glob.glob(os.path.join(gh, "Tertiary*", "**", "HostTest_artefacts", "Release", exe), recursive=True)
    cands.sort(key=os.path.getmtime, reverse=True)
    if not cands:
        raise SystemExit("no HostTest build found; build it (plugin/tests/run_all.ps1) or pass its path")
    return cands[0]


def write_audio(hosttest=None):
    hosttest = hosttest or find_hosttest()
    out = os.path.join(HARNESS, "Audio", "harness-input.wav")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, "input-2s.wav")
        subprocess.run([hosttest, "--write-input", src], check=True, cwd=tmp)
        tile_input(src, out)


# [Claude 2026-10-30] The play track, for Joe's own listening (not measured). Joe: "add a second channel of Tertiary
# which has the musical input on it. That way the automation lanes do not lock me from changing knobs etc. And you
# can just mute this by default." And: "add something musical at the end of the timeline so I can jump to it easily".
# A second audio track, "Tertiary – play": a music loop (sample_audio/Organ.wav at 48 kHz, crossfaded at each seam)
# over the whole timeline, Tertiary on its insert, no automation lanes (imported with Automation off), muted.
# The marker "Play" at bar 14, just after the measured bars 1 to 13, is set in Cubase (README). Muted, it adds
# nothing to the export, which stays bit-identical to the golden.
PLAY_NAME = "Tertiary – play"
PLAY_WAV = "play-loop.wav"
PLAY_SECONDS = 180
PLAY_SOURCE = os.path.join(REPO, "plugin", "sample_audio", "Organ.wav")


def write_play_audio(src=PLAY_SOURCE, seconds=PLAY_SECONDS):
    """Audio/play-loop.wav: the sample at 48 kHz, 32-bit float stereo, looped to `seconds` with 50 ms crossfades."""
    try:
        import numpy as np
        from scipy.signal import resample_poly
    except ImportError:
        raise SystemExit("playaudio needs numpy and scipy (pip install numpy scipy)")
    d = open(src, "rb").read()
    i, fmt, data = 12, None, None
    while i + 8 <= len(d):
        cid, size = d[i:i + 4], struct.unpack("<I", d[i + 4:i + 8])[0]
        if cid == b"fmt ":
            fmt = d[i + 8:i + 8 + size]
        elif cid == b"data":
            data = d[i + 8:i + 8 + size]
        i += 8 + size + (size & 1)
    tag, ch, rate, _, _, bits = struct.unpack("<HHIIHH", fmt[:16])
    if tag != 1 or bits != 16 or ch != 2:
        raise SystemExit("%s: expected 16-bit stereo PCM" % src)
    x = np.frombuffer(data, dtype="<i2").reshape(-1, 2).astype(np.float64) / 32768.0
    if rate != 48000:
        from math import gcd
        g = gcd(48000, rate)
        x = resample_poly(x, 48000 // g, rate // g, axis=0)
    fade = int(0.05 * 48000)
    ramp = np.linspace(0.0, 1.0, fade)[:, None]
    loop = x[:-fade].copy()
    loop[:fade] = x[:fade] * ramp + x[-fade:] * (1.0 - ramp)   # the end crossfades into the start: no click at the seam
    frames = seconds * 48000
    out = np.tile(loop, (-(-frames // len(loop)), 1))[:frames]
    out[-fade:] *= (1.0 - ramp)                                  # and the very end fades out
    body = out.astype("<f4").tobytes()
    fmt_chunk = struct.pack("<HHIIHH", 3, 2, 48000, 48000 * 8, 8, 32)
    dst = os.path.join(HARNESS, "Audio", PLAY_WAV)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    wav = b"fmt " + struct.pack("<I", 16) + fmt_chunk + b"data" + struct.pack("<I", len(body)) + body
    open(dst, "wb").write(b"RIFF" + struct.pack("<I", 4 + len(wav)) + b"WAVE" + wav)   # data at byte 44, as the archive says
    print("%s: %d s, 48000 Hz, 2 ch, 32-bit float, from %s (%d Hz), %d loops" % (dst, seconds, src, rate, -(-frames // len(loop))))
    return frames


def write_play_archive(template, out, frames=PLAY_SECONDS * 48000):
    """The play track: the template's audio track and Tertiary insert, renamed, playing Audio/play-loop.wav from bar 1.
    Its automation lanes are left in the file but Cubase skips them when Import Options' Automation is off (the
    default): the track then has none, and every knob can be moved by hand."""
    t = open(template, encoding="utf-8").read()
    old_frames = '"3072000"'
    if t.count(old_frames) < 3:
        raise SystemExit("template: the harness clip's length (3072000 frames) was not found")
    t = t.replace(old_frames, '"%d"' % frames)
    t = t.replace('value="harness-input.wav"', 'value="%s"' % PLAY_WAV)
    t = t.replace('value="harness-input"', 'value="%s"' % PLAY_NAME)              # track, event, clip, channel names
    t = re.sub(r'(<string name="Path" value=")[^"]*(" wide="true"/>)',
               lambda m: m.group(1) + os.path.join(HARNESS, "Audio") + "\\" + m.group(2), t)
    t = re.sub(r'<obj class="MHitPointEvent".*?</obj>\s*', "", t, flags=re.S)  # the old file's hit points
    open(out, "w", encoding="utf-8").write(t)
    print("%s: track \"%s\", %s, %d frames from bar 1; import with Automation off" % (out, PLAY_NAME, PLAY_WAV, frames))


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    arg = lambda i, default: sys.argv[i] if len(sys.argv) > i else default
    if cmd == "audio":
        write_audio(arg(2, None))
    elif cmd == "input":
        tile_input(sys.argv[2], sys.argv[3])
    elif cmd == "tempo":
        write_smt(arg(2, os.path.join(HARNESS, "Misc", "harness-tempo.smt")))
    elif cmd == "archive":
        write_archive(arg(2, os.path.join(HARNESS, "Misc", "learn-3.xml")),
                      arg(3, os.path.join(HARNESS, "Misc", "harness-track.xml")))
    elif cmd == "playaudio":
        write_play_audio()
    elif cmd == "playarchive":
        write_play_archive(arg(2, os.path.join(HARNESS, "Misc", "learn-3.xml")),
                           arg(3, os.path.join(HARNESS, "Misc", "play-track.xml")))
    elif cmd == "params":
        for p in AUTOMATION:
            print("%12d  lane tag %12d  %s" % (juce_hash(p), juce_hash(p) + LANE_OFFSET, p))
    else:
        print(__doc__)
        sys.exit(2)
