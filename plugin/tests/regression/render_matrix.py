# render_matrix.py
# Written by Claude (2026-10-27) for Tertiary 1.1, at Joe's request: "anything you touch ... regression tested".
#
# Renders a fixed set of inputs through ReviewRender with a fixed set of parameter settings, so the output of one
# build can be compared with another (compare_renders.py). Used locally (before/after each change) and in CI
# (.github/workflows/test.yml, to compare Windows with both Macs).
#
#   python render_matrix.py <ReviewRender executable> <input folder> <output folder> [--quick]
#
# Every .wav in the input folder is rendered with every setting below (--quick: the first 8 seconds, fewer settings).
# A synthetic test signal (noise, a sweep and clicks, 48 kHz stereo) is written into the input folder if it is missing.
# Output files are 32-bit float WAVs named <input>__<setting>.wav.

import math, os, random, struct, subprocess, sys

# Each setting: name, ReviewRender arguments. Values are in the parameter's own units, or a choice's text.
SETTINGS = [
    ("default", []),
    ("varied", [
        "--bpm", "100",
        "--set", "Input Gain=3", "--set", "Output Gain=-2",
        "--set", "Low-Mid Crossover Freq=250", "--set", "Mid-High Crossover Freq=3000",
        "--set", "Waveshape Low Band=Square", "--set", "Depth Low Band=100", "--set", "Skew Low Band=30",
        "--set", "Invert Low Band=1", "--set", "Multiplier Low Band=1/8 Note",
        "--set", "Waveshape Mid Band=Triangle", "--set", "Depth Mid Band=60", "--set", "Relative Phase Mid Band=90",
        "--set", "Multiplier Mid Band=1/4 Note Triplet",
        "--set", "Waveshape High Band=Ramp Down", "--set", "Depth High Band=100", "--set", "Skew High Band=70",
        "--set", "Relative Phase High Band=-120", "--set", "Multiplier High Band=1/16 Note",
        "--set", "Low Output Gain=-3", "--set", "Mid Output Gain=2", "--set", "High Output Gain=4"]),
    ("free", [
        "--set", "Sync to Host Low Band=0", "--set", "Sync to Host Mid Band=0", "--set", "Sync to Host High Band=0",
        "--set", "Rate Low Band=0.5", "--set", "Rate Mid Band=4.7", "--set", "Rate High Band=12.5",
        "--set", "Waveshape Low Band=Hills", "--set", "Waveshape Mid Band=Valleys", "--set", "Waveshape High Band=Ramp Up",
        "--set", "Depth Low Band=50", "--set", "Depth Mid Band=90", "--set", "Depth High Band=100"]),
    ("solomute", [
        "--set", "Solo Mid Band=1", "--set", "Mute Low Band=1", "--set", "Bypassed High Band=1"]),
    ("ramps", [
        "--ramp", "Low-Mid Crossover Freq:1:6:200:900",
        "--ramp", "Depth High Band:0:8:0:100",
        "--ramp", "Rate Low Band:2:7:1:9",
        "--ramp", "Multiplier Mid Band:3:9:0:5",               # multiplier changes re-sync the LFOs to the host grid
        "--set", "Sync to Host Low Band=0"]),
    ("varied_block512", ["--block", "512"]),     # the "varied" settings at another block size (filled in below)
    ("varied_blockvar", ["--block", "var"]),     # ... and with block sizes that change every block
]
VARIED = dict(SETTINGS)["varied"]
SETTINGS = [(n, a + VARIED if n.startswith("varied_") else a) for n, a in SETTINGS]
QUICK = ["default", "varied", "free", "varied_blockvar"]


def write_test_signal(path, rate=48000, seconds=20):
    """Deterministic stereo test signal: soft noise, a log sweep (20 Hz to 20 kHz), and a click every half second."""
    rnd = random.Random(1234)
    frames = []
    n = rate * seconds
    for i in range(n):
        t = i / rate
        sweep = 0.25 * math.sin(2 * math.pi * 20 * seconds / math.log(1000) * (1000 ** (t / seconds) - 1))
        click = 0.5 if i % (rate // 2) == 0 else 0.0
        left = sweep + 0.05 * (rnd.random() * 2 - 1) + click
        right = 0.8 * sweep + 0.05 * (rnd.random() * 2 - 1) - click
        frames.append(struct.pack("<ff", left, right))
    data = b"".join(frames)
    with open(path, "wb") as fh:
        fh.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE")
        fh.write(b"fmt " + struct.pack("<IHHIIHH", 16, 3, 2, rate, rate * 8, 8, 32))
        fh.write(b"data" + struct.pack("<I", len(data)) + data)


def main():
    if len(sys.argv) < 4:
        print(__doc__ if __doc__ else "usage: render_matrix.py <ReviewRender> <inputs> <outputs> [--quick]")
        return 2
    exe, inputs, outputs = os.path.abspath(sys.argv[1]), sys.argv[2], sys.argv[3]
    quick = "--quick" in sys.argv
    os.makedirs(outputs, exist_ok=True)
    signal = os.path.join(inputs, "Test Signal.wav")
    if not os.path.exists(signal):
        write_test_signal(signal)
    failures = 0
    for wav in sorted(f for f in os.listdir(inputs) if f.lower().endswith(".wav")):
        for name, args in SETTINGS:
            if quick and name not in QUICK:
                continue
            out = os.path.join(outputs, os.path.splitext(wav)[0] + "__" + name + ".wav")
            cmd = [exe, "--in", os.path.join(inputs, wav), "--out", out, "--bits", "32",
                   "--seconds", "8" if quick else "20"] + args
            result = subprocess.run(cmd, capture_output=True, text=True)
            print(("ok   " if result.returncode == 0 else "FAIL ") + os.path.basename(out) + "  " + result.stdout.strip())
            if result.returncode != 0:
                print(result.stderr)
                failures += 1
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
