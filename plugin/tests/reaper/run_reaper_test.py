"""Compare REAPER with HostTest under the same playing-transport conditions.

The legacy golden/default.wav is a stopped/no-playhead render with only 256
silent startup frames. It is not a reference for a running DAW. See README.md.
"""

import argparse
import math
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "regression"))
from compare_renders import read_wav  # noqa: E402

DEFAULT_REAPER = {
    "win32": r"C:\Program Files\REAPER (x64)\reaper.exe",
    "darwin": "/Applications/REAPER.app/Contents/MacOS/REAPER",
}
DEFAULT_VST3 = {
    "win32": os.path.join(os.environ.get("CommonProgramFiles", r"C:\Program Files\Common Files"),
                          "VST3", "Tertiary.vst3"),
    "darwin": os.path.expanduser("~/Library/Audio/Plug-Ins/VST3/Tertiary.vst3"),
}
TOLERANCE = 1e-5
FRAMES = 120000  # 24000 silent frames, then the original 96000-frame test signal


def read_render(path):
    rate, channels, samples = read_wav(path)
    if (rate, channels, len(samples)) != (48000, 2, 2 * FRAMES):
        raise ValueError("unexpected rate/channels/frames in %s: %s/%s/%s" %
                         (path, rate, channels, len(samples) / max(channels, 1)))
    if not all(math.isfinite(x) for x in samples):
        raise ValueError("non-finite audio in " + str(path))
    return samples


def max_diff(a, b):
    return max(abs(x - y) for x, y in zip(a, b))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("hosttest", help="HostTest built with the REAPER reference options")
    parser.add_argument("--reaper", default=DEFAULT_REAPER.get(sys.platform, "reaper"))
    parser.add_argument("--reaper-config", help="isolated REAPER ini with an audio device already configured")
    parser.add_argument("--fx", default="VST3:Tertiary", help="VST3:Tertiary or AU:Tertiary")
    parser.add_argument("--vst3", help="VST3 bundle for HostTest; configure REAPER to scan this same bundle")
    parser.add_argument("--prefix", default="reaper")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.prefix):
        parser.error("--prefix must contain only letters, digits, underscores or hyphens")
    if not args.fx.upper().startswith(("VST3:", "AU:")):
        parser.error("--fx must select VST3 or AU, matching HostTest's reference format")
    if args.fx.upper().startswith("AU:") and args.vst3:
        parser.error("--vst3 cannot be used with --fx AU:Tertiary")
    if not os.path.exists(args.reaper):
        print("FAIL  REAPER is not installed (" + args.reaper + ")")
        return 1

    hosttest = os.path.abspath(args.hosttest)
    out = os.path.join(HERE, "out")
    os.makedirs(out, exist_ok=True)

    def output(suffix):
        return os.path.join(out, args.prefix + suffix)

    # Only this run's known artifacts; no broad prefix deletion of other results.
    for suffix in ("_reference.wav", "_default.wav", "_repeat.wav", "_automation.wav",
                   "_log.txt", "_default.rpp", "_test.rpp"):
        if os.path.exists(output(suffix)):
            os.remove(output(suffix))
    input_path = os.path.join(HERE, "input.wav")
    if os.path.exists(input_path):
        os.remove(input_path)
    subprocess.run([hosttest, "--write-reaper-input", input_path], check=True)
    vst3 = args.vst3 or DEFAULT_VST3.get(sys.platform, "Tertiary.vst3")
    if sys.platform == "darwin" and not args.vst3 and not os.path.exists(vst3):
        vst3 = "/Library/Audio/Plug-Ins/VST3/Tertiary.vst3"
    select = ["--au"] if args.fx.upper().startswith("AU:") else ["--vst3", os.path.abspath(vst3)]
    subprocess.run([hosttest, *select, "--write-reaper-reference", output("_reference.wav")], check=True)
    try:
        read_render(input_path)
        reference = read_render(output("_reference.wav"))
    except (OSError, ValueError) as error:
        print("FAIL  invalid HostTest fixture; rebuild HostTest with the REAPER options: " + str(error))
        return 1

    env = dict(os.environ, TERTIARY_TEST_DIR=HERE, TERTIARY_PREFIX=args.prefix, TERTIARY_FX=args.fx)
    command = [args.reaper]
    if args.reaper_config:
        command += ["-cfgfile", os.path.abspath(args.reaper_config)]
    command += ["-newinst", "-nosplash", os.path.join(HERE, "tertiary_reaper_test.lua")]
    if sys.platform == "darwin":
        command += ["-ApplePersistence", "NO"]
    startup = None
    if sys.platform == "win32":
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 6  # SW_MINIMIZE
    start = time.time()
    subprocess.run(command, env=env, timeout=90, check=True, startupinfo=startup)
    print("REAPER ran for %.1f s" % (time.time() - start))
    if os.path.exists(output("_log.txt")):
        with open(output("_log.txt")) as log:
            print(log.read())

    failed = 0

    def check(ok, text):
        nonlocal failed
        print(("PASS  " if ok else "FAIL  ") + text)
        failed += not ok

    try:
        rendered = read_render(output("_default.wav"))
        diff = max_diff(reference, rendered)
        check(diff <= TOLERANCE,
              "REAPER = HostTest playing reference: max diff %.9g over %d frames; tolerance %g" %
              (diff, FRAMES, TOLERANCE))
        repeat = read_render(output("_repeat.wav"))
        diff = max_diff(rendered, repeat)
        check(diff <= TOLERANCE, "same project rendered twice: max diff %.9g" % diff)
        automation = read_render(output("_automation.wav"))
        changed = max_diff(rendered, automation)
        check(all(abs(x) < 4.0 for x in automation) and changed > 1e-4,
              "automation render: finite, bounded and different (max diff %.9g)" % changed)
    except (OSError, ValueError) as error:
        check(False, str(error))
    check(os.path.exists(output("_default.rpp")) and os.path.exists(output("_test.rpp")),
          "default and automated projects saved")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
