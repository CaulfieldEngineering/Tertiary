# run_reaper_test.py
# Written by Claude (2026-10-27). PREPARED, NOT RUN: REAPER isn't installed (Joe hasn't decided). See README.md.
#
#   python plugin/tests/reaper/run_reaper_test.py <HostTest executable> [--reaper <path to reaper>]
#          [--fx "AU:Tertiary"] [--prefix reaper_au]
#   [Claude 2026-10-30, Mac day] --fx picks the plug-in (format) REAPER loads; --prefix names this run's files in
#   out/ (so a VST3 run and an AU run sit side by side). Without them: the VST3, prefix "reaper", as before.
#
# 1. HostTest --write-input writes the golden renders' input to plugin/tests/reaper/input.wav
# 2. REAPER starts headless-ish with tertiary_reaper_test.lua, which builds the project, renders, saves
#    tertiary_test.rpp and quits
# 3. out/reaper_default.wav is compared with golden/default.wav (HostTest's render of the same input), and
#    out/reaper_automation.wav is checked for sanity
# Exit code 0 when everything passes. Tertiary must be installed where REAPER finds VST3s
# (C:\Program Files\Common Files\VST3 or /Library/Audio/Plug-Ins/VST3).

import os, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "regression"))
from compare_renders import read_wav  # noqa: E402

DEFAULT_REAPER = {
    "win32": r"C:\Program Files\REAPER (x64)\reaper.exe",
    "darwin": "/Applications/REAPER.app/Contents/MacOS/REAPER",
}


def main():
    if len(sys.argv) < 2:
        print("usage: run_reaper_test.py <HostTest> [--reaper <path>]")
        return 2
    hosttest = os.path.abspath(sys.argv[1])
    reaper = sys.argv[sys.argv.index("--reaper") + 1] if "--reaper" in sys.argv else DEFAULT_REAPER.get(sys.platform, "reaper")
    fx = sys.argv[sys.argv.index("--fx") + 1] if "--fx" in sys.argv else None
    prefix = sys.argv[sys.argv.index("--prefix") + 1] if "--prefix" in sys.argv else "reaper"
    if not os.path.exists(reaper):
        print("SKIP  REAPER is not installed (" + reaper + ")")
        return 0

    out = os.path.join(HERE, "out")
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):
        if f.startswith(prefix + "_"):
            os.remove(os.path.join(out, f))

    subprocess.run([hosttest, "--write-input", os.path.join(HERE, "input.wav")], check=True)

    # -newinst: a fresh REAPER; -nosplash; the .lua runs at start-up. (Command-line options unverified.)
    env = dict(os.environ, TERTIARY_TEST_DIR=HERE, TERTIARY_PREFIX=prefix)
    if fx:
        env["TERTIARY_FX"] = fx
    start = time.time()
    subprocess.run([reaper, "-newinst", "-nosplash", os.path.join(HERE, "tertiary_reaper_test.lua")],
                   env=env, timeout=300)
    print("REAPER ran for %.0f s" % (time.time() - start))
    log = os.path.join(out, prefix + "_log.txt")
    if os.path.exists(log):
        print(open(log).read())

    failed = 0
    golden = os.path.join(HERE, "..", "golden", "default.wav")
    rendered = os.path.join(out, prefix + "_default.wav")
    if not os.path.exists(rendered):
        print("FAIL  no REAPER render")
        return 1

    # REAPER may render a few samples more or less; compare what both have. Tolerance as HostTest uses on
    # Apple Silicon (1e-5, -100 dBFS): REAPER's block sizes differ from HostTest's, which moves samples by
    # about 1e-8 (measured with ReviewRender), so this is generous.
    _, _, a = read_wav(golden)
    _, _, b = read_wav(rendered)
    n = min(len(a), len(b))
    diff = max(abs(x - y) for x, y in zip(a[:n], b[:n]))
    ok = diff <= 1e-5 and abs(len(a) - len(b)) <= 2 * 4096
    print(("PASS" if ok else "FAIL") + "  REAPER render = golden default  max diff %.3g over %d samples" % (diff, n // 2))
    failed += 0 if ok else 1

    auto = os.path.join(out, prefix + "_automation.wav")
    if os.path.exists(auto):
        _, _, c = read_wav(auto)
        sane = all(x == x and abs(x) < 4.0 for x in c)     # x == x is false for NaN
        changed = max(abs(x - y) for x, y in zip(b[:n], c[:n])) > 1e-4
        print(("PASS" if sane and changed else "FAIL") + "  REAPER automation render: sane and different from the default")
        failed += 0 if (sane and changed) else 1
    else:
        print("FAIL  no automation render")
        failed += 1

    rpp = os.path.join(out, prefix + "_test.rpp")
    print(("PASS" if os.path.exists(rpp) else "FAIL") + "  project saved: " + rpp)
    failed += 0 if os.path.exists(rpp) else 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
