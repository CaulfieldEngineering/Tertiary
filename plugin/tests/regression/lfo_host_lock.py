# lfo_host_lock.py
# Written by Claude (2026-10-29) for Tertiary 1.1, after Joe asked that synced LFOs follow the host: "loops and jumps
# land on the grid every pass".
#
#   python lfo_host_lock.py <ReviewRender executable> [work folder]
#
# A constant (DC) input goes through the low band only, so the output is the low LFO's gain curve: synced at 1/4 note,
# Sine, depth 100, 120 bpm (one cycle per beat). Three host events, each rendered:
#   jump       at 20 s the host jumps to beat 0.5 (half a cycle off the running count)
#   loop       a 5.5-beat loop from beat 8 to beat 13.5, three passes (each pass lands half a cycle off)
#   stopstart  the host stops at 4 s and plays on from the same beat at 5.3 s (the LFO free-runs meanwhile)
#   preroll    [Claude 2026-10-29] the host starts at beat -1.75 (before bar 1), V-06 of the 2026-10-29 review
# After each event, once one block and the 10 ms fade have passed, the output must match a steady render at the same
# host beat position (within 0.001), so the LFO is back on the grid. And no click: the largest sample-to-sample change
# stays within what the 10 ms fade allows (a full-depth swing over 10 ms), far below a hard jump.
# Exit code 0 = all pass.

import os, struct, subprocess, sys, tempfile, wave

RATE, BPM, BLOCK = 48000, 120.0, 32
SECONDS = 24.0
FADE = 0.010

def write_dc(path):
    with wave.open(path, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes(struct.pack("<hh", 16384, 16384) * int(RATE * SECONDS))

def read_float(path):
    data = open(path, "rb").read(); i = 12
    while i < len(data):
        cid, size = data[i:i+4], struct.unpack("<I", data[i+4:i+8])[0]
        if cid == b"fmt ": channels = struct.unpack("<H", data[i+10:i+12])[0]
        if cid == b"data": return struct.unpack("<%df" % (size // 4), data[i+8:i+8+size])[0::channels]
        i += 8 + size + (size & 1)

def render(exe, dc, out, extra):
    subprocess.run([exe, "--in", dc, "--out", out, "--bits", "32", "--bpm", str(BPM),
                    "--set", "Waveshape Low Band=Sine", "--set", "Depth Low Band=100"] + extra,
                   check=True, stdout=subprocess.DEVNULL)
    return read_float(out)

def ppq_timeline(jumps=(), stops=()):
    """The host's beat position at the start of every block, as ReviewRender's clock runs it."""
    out, ppq, used = [], 0.0, set()
    for b in range(int(SECONDS * RATE) // BLOCK):
        t = b * BLOCK / RATE
        playing = not any(a <= t < z for a, z in stops)
        for k, (a, beat) in enumerate(jumps):
            if k not in used and t >= a:
                ppq = beat; used.add(k)
        out.append(ppq)
        if playing: ppq += BLOCK / RATE * BPM / 60.0
    return out

def check(name, y, ref, ppq, events):
    """After each event (+ one block + the fade), y must match the steady render at the same beat position."""
    ok = True
    for ev in events:
        start = int((ev + FADE) * RATE) + BLOCK
        worst = 0.0
        for n in range(start, start + int(0.5 * RATE)):
            b, frac = divmod(n, BLOCK)
            beat = ppq[b] + frac / RATE * BPM / 60.0
            tref = (4.0 + (beat % 1.0)) * 60.0 / BPM          # the same phase in the steady render, around 2 s
            k = int(tref * RATE); f = tref * RATE - k
            yr = ref[k] * (1 - f) + ref[k + 1] * f
            worst = max(worst, abs(y[n] - yr))
        good = worst < 1e-3
        ok &= good
        print("  %-10s event at %5.2f s: off the grid by at most %.5f after %.0f ms -> %s"
              % (name, ev, worst, (FADE * 1000 + BLOCK / RATE * 1000), "PASS" if good else "FAIL"))
    step = max(abs(y[n] - y[n - 1]) for n in range(RATE, len(y)))
    limit = 0.5 / (FADE * RATE) * 1.5 + 1e-4                    # a full swing of the 0.5 DC over 10 ms, with margin
    good = step <= limit
    print("  %-10s largest sample-to-sample change %.5f (limit %.5f; a hard jump could be 0.5) -> %s"
          % (name, step, limit, "PASS" if good else "FAIL"))
    return ok and good

def main():
    exe = sys.argv[1]
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp()
    os.makedirs(work, exist_ok=True)
    dc = os.path.join(work, "dc.wav"); write_dc(dc)
    ref = render(exe, dc, os.path.join(work, "steady.wav"), [])
    ok = True

    jumps = [(20.0, 0.5)]
    y = render(exe, dc, os.path.join(work, "jump.wav"), ["--jump-at", "20:0.5"])
    ok &= check("jump", y, ref, ppq_timeline(jumps=jumps), [20.0])

    loop = [(6.75, 8.0), (9.5, 8.0), (12.25, 8.0)]             # beat 13.5 reached at 6.75 s, then every 2.75 s
    y = render(exe, dc, os.path.join(work, "loop.wav"), sum([["--jump-at", "%g:%g" % j] for j in loop], []))
    ok &= check("loop", y, ref, ppq_timeline(jumps=loop), [j[0] for j in loop])

    stops = [(4.0, 5.3)]
    y = render(exe, dc, os.path.join(work, "stopstart.wav"), ["--stop", "4:5.3"])
    ok &= check("stopstart", y, ref, ppq_timeline(stops=stops), [5.3])

    # [Claude 2026-10-29] V-06: the host starts before bar 1 (a pre-roll or count-in), so the beat position is negative
    # for the first 0.875 s. The band must play on the grid from the start, not go silent until the position crosses 0.
    # Checked from 0.1 s (after the crossover filters and the 50 ms gain fade-ins settle) to 0.6 s, all before beat 0.
    preroll = [(0.0, -1.75)]
    y = render(exe, dc, os.path.join(work, "preroll.wav"), ["--jump-at", "0:-1.75"])
    ok &= check("preroll", y, ref, ppq_timeline(jumps=preroll), [0.1 - FADE - BLOCK / RATE])

    sys.exit(0 if ok else 1)

if __name__ == "__main__":
    main()
