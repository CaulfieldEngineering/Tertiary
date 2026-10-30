# lfo_rate_step.py
# Written by Claude (2026-10-29) for Tertiary 1.1, after Joe's report that changing a free-running band's rate made
# its waveform "go haywire": checks that the audio LFO has no jump when its rate changes.
#
#   python lfo_rate_step.py <ReviewRender executable> [work folder]
#
# A constant (DC) input goes through the low band only, so the output is the low LFO's gain curve. The low band runs
# free (Sync off), Sine, depth 100. Two renders: the rate held at its maximum (12.5 Hz, the steepest normal slope),
# and the rate stepped mid-run (1 -> 12.5 -> 0.5 -> 7 -> 12.5 Hz). The largest sample-to-sample change in the stepped
# render must stay within the steady render's. A phase jump at a step would show as a bigger one.
# [Claude 2026-10-29] Also (Joe: "the low channel initiated to not-synced. But the frequency slider did not do
# anything"): with Sync off from the start, and with Sync switched off mid-run, moving the rate must change the LFO's
# measured frequency (3 Hz, then 9 Hz; synced at 120 bpm it would be 2 Hz). Exit code 0 = all pass.

import os, struct, subprocess, sys, tempfile, wave

RATE = 48000
SECONDS = 6.0
SETTLE = 0.5   # the crossover filters settle on the DC first

def write_dc(path):
    frame = struct.pack("<hh", 16384, 16384)   # 0.5 on both channels
    with wave.open(path, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes(frame * int(RATE * SECONDS))

def read_float(path):
    with open(path, "rb") as f:
        data = f.read()
    # find the 'data' chunk of a 32-bit float WAV; return the left channel
    i = 12
    while i < len(data):
        cid, size = data[i:i+4], struct.unpack("<I", data[i+4:i+8])[0]
        if cid == b"fmt ":
            channels = struct.unpack("<H", data[i+10:i+12])[0]
        if cid == b"data":
            n = size // 4
            vals = struct.unpack("<%df" % n, data[i+8:i+8+size])
            return vals[0::channels]
        i += 8 + size + (size & 1)
    raise RuntimeError("no data chunk in " + path)

def render(exe, dc, out, extra, sync_off=True):
    base = ["--in", dc, "--out", out, "--bits", "32",
            "--set", "Waveshape Low Band=Sine", "--set", "Depth Low Band=100"]
    if sync_off:
        base += ["--set", "Sync to Host Low Band=0"]
    subprocess.run([exe] + base + extra, check=True, stdout=subprocess.DEVNULL)
    return read_float(out)

def max_step(y):
    start = int(SETTLE * RATE)
    return max(abs(y[k] - y[k - 1]) for k in range(start + 1, len(y)))

def frequency(y, t0, t1):
    seg = y[int(t0 * RATE):int(t1 * RATE)]
    mean = sum(seg) / len(seg)
    ups = [k for k in range(1, len(seg)) if seg[k - 1] < mean <= seg[k]]
    if len(ups) < 2:
        return 0.0
    return (len(ups) - 1) / ((ups[-1] - ups[0]) / RATE)

def rate_follows(exe, dc, work, name, extra, sync_off):
    y = render(exe, dc, os.path.join(work, name.replace(" ", "_") + ".wav"),
               extra + ["--set", "Rate Low Band=3", "--ramp", "Rate Low Band:3:3.0005:3:9"], sync_off)
    f1, f2 = frequency(y, 1.5, 2.9), frequency(y, 3.5, 5.9)
    ok = abs(f1 - 3) < 0.1 and abs(f2 - 9) < 0.2
    print("%s: %.2f Hz, after moving the rate to 9 Hz: %.2f Hz -> %s" % (name, f1, f2, "PASS" if ok else "FAIL"))
    return ok

def main():
    exe = sys.argv[1]
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp()
    os.makedirs(work, exist_ok=True)
    dc = os.path.join(work, "dc.wav"); write_dc(dc)

    steady = render(exe, dc, os.path.join(work, "steady.wav"), ["--set", "Rate Low Band=12.5"])
    steps = []
    for t, v0, v1 in [(1.5, 1, 12.5), (2.5, 12.5, 0.5), (3.5, 0.5, 7), (4.5, 7, 12.5)]:
        steps += ["--ramp", "Rate Low Band:%g:%g:%g:%g" % (t, t + 0.0005, v0, v1)]
    stepped = render(exe, dc, os.path.join(work, "stepped.wav"), ["--set", "Rate Low Band=1"] + steps)

    a, b = max_step(steady), max_step(stepped)
    ok = b <= a * 1.01 + 1e-6
    print("largest sample-to-sample change: steady 12.5 Hz %.6f, with rate steps %.6f -> %s" % (a, b, "PASS" if ok else "FAIL"))

    ok &= rate_follows(exe, dc, work, "sync off from the start", [], True)
    ok &= rate_follows(exe, dc, work, "sync switched off mid-run", ["--ramp", "Sync to Host Low Band:1:1:0:0"], False)
    sys.exit(0 if ok else 1)

if __name__ == "__main__":
    main()
