# compare_renders.py
# Written by Claude (2026-10-27) for Tertiary 1.1, to compare two sets of renders from render_matrix.py.
#
#   python compare_renders.py <reference folder> <test folder> [--tolerance 1e-3] [--report out.txt]
#
# For every WAV in the reference folder it reads the file of the same name in the test folder and prints:
#   identical   the samples are bit for bit the same
#   max diff    the largest difference of any sample, linear and in dBFS
#   residual    the RMS of the difference, in dB below the RMS of the reference (bigger is better; -inf is identical)
# A file fails when the max difference is above the tolerance (default 1e-3, about -60 dBFS) or is missing.
# Exit code 0 when every file passes. Standard library only (reads 16/24/32-bit PCM and 32-bit float WAVs).

import math, os, struct, sys


def read_wav(path):
    """Returns (rate, channels, list of interleaved floats)."""
    data = open(path, "rb").read()
    i, fmt, ch, rate, bits, samples = 12, None, 0, 0, 0, None
    while i + 8 <= len(data):
        cid, size = data[i:i + 4], struct.unpack("<I", data[i + 4:i + 8])[0]
        body = data[i + 8:i + 8 + size]
        if cid == b"fmt ":
            fmt, ch, rate, _, _, bits = struct.unpack("<HHIIHH", body[:16])
            if fmt == 0xFFFE:                                       # WAVE_FORMAT_EXTENSIBLE: real format in the GUID
                fmt = struct.unpack("<H", body[24:26])[0]
        elif cid == b"data":
            samples = body
        i += 8 + size + (size & 1)
    if fmt == 3 and bits == 32:
        values = list(struct.unpack("<%df" % (len(samples) // 4), samples))
    elif fmt == 1 and bits == 16:
        values = [v / 32768.0 for v in struct.unpack("<%dh" % (len(samples) // 2), samples)]
    elif fmt == 1 and bits == 24:
        values = [int.from_bytes(samples[k:k + 3], "little", signed=True) / 8388608.0 for k in range(0, len(samples) - 2, 3)]
    elif fmt == 1 and bits == 32:
        values = [v / 2147483648.0 for v in struct.unpack("<%di" % (len(samples) // 4), samples)]
    else:
        raise ValueError("unsupported WAV format %s/%s bits in %s" % (fmt, bits, path))
    return rate, ch, values


def db(x):
    return 20 * math.log10(x) if x > 0 else float("-inf")


def compare(ref_path, test_path):
    r_rate, r_ch, ref = read_wav(ref_path)
    t_rate, t_ch, test = read_wav(test_path)
    if (r_rate, r_ch, len(ref)) != (t_rate, t_ch, len(test)):
        return None, "format or length differs: %s/%s/%s vs %s/%s/%s" % (r_rate, r_ch, len(ref), t_rate, t_ch, len(test))
    max_diff, sum_d, sum_r, first = 0.0, 0.0, 0.0, -1
    for k, (a, b) in enumerate(zip(ref, test)):
        d = abs(a - b)
        if d > max_diff:
            max_diff = d
        if d > 0 and first < 0:
            first = k // r_ch
        sum_d += d * d
        sum_r += a * a
    n = max(1, len(ref))
    rms_d, rms_r = math.sqrt(sum_d / n), math.sqrt(sum_r / n)
    residual = db(rms_d) - db(rms_r) if rms_d > 0 and rms_r > 0 else float("-inf")
    return {"identical": max_diff == 0.0, "max": max_diff, "residual": residual,
            "first": first / r_rate if first >= 0 else None}, None


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        print("usage: compare_renders.py <reference folder> <test folder> [--tolerance 1e-3] [--report out.txt]")
        return 2
    ref_dir, test_dir = args[0], args[1]
    tol = float(args[args.index("--tolerance") + 1]) if "--tolerance" in args else 1e-3
    report = args[args.index("--report") + 1] if "--report" in args else None
    lines, failed, worst = [], 0, 0.0
    for name in sorted(f for f in os.listdir(ref_dir) if f.lower().endswith(".wav")):
        test_path = os.path.join(test_dir, name)
        if not os.path.exists(test_path):
            lines.append("FAIL  %-60s missing" % name)
            failed += 1
            continue
        result, error = compare(os.path.join(ref_dir, name), test_path)
        if error:
            lines.append("FAIL  %-60s %s" % (name, error))
            failed += 1
            continue
        worst = max(worst, result["max"])
        ok = result["max"] <= tol
        failed += 0 if ok else 1
        if result["identical"]:
            detail = "identical"
        else:
            detail = "max diff %.3g (%.1f dBFS)  residual %.1f dB  first at %.3f s" % (
                result["max"], db(result["max"]), result["residual"], result["first"])
        lines.append("%s  %-60s %s" % ("ok  " if ok else "FAIL", name, detail))
    lines.append("%d files, %d failed, worst max diff %.3g (%.1f dBFS), tolerance %.3g" % (
        len(lines), failed, worst, db(worst), tol))
    print("\n".join(lines))
    if report:
        with open(report, "w") as fh:
            fh.write("\n".join(lines) + "\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
