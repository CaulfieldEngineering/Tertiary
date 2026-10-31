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
import hashlib
import json
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "regression"))
from compare_renders import read_wav  # noqa: E402
# [Codex 2026-10-30] Shared named-dialog watchdog; no product-code changes.
if sys.platform == 'win32':
    sys.path.insert(0, str(Path(HERE).parents[2] / 'tests' / 'lib'))
    from reaper_windows import ReaperWindows

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
    # [Codex 2026-10-30] Isolated artifacts and proof of the module actually loaded by REAPER.
    parser.add_argument("--work-dir", help="write fixtures and renders here instead of next to the script")
    parser.add_argument("--expected-plugin", help="require REAPER to load this binary (hash and path checked)")
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
    work = os.path.abspath(args.work_dir) if args.work_dir else HERE
    out = os.path.join(work, "out")
    os.makedirs(out, exist_ok=True)
    # [Codex 2026-10-30] A previous run's handshake must not skip this binary inspection.
    Path(work, 'module-inspected').unlink(missing_ok=True)

    def output(suffix):
        return os.path.join(out, args.prefix + suffix)

    # Only this run's known artifacts; no broad prefix deletion of other results.
    for suffix in ("_reference.wav", "_default.wav", "_repeat.wav", "_automation.wav",
                   "_log.txt", "_default.rpp", "_test.rpp"):
        if os.path.exists(output(suffix)):
            os.remove(output(suffix))
    input_path = os.path.join(work, "input.wav")
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

    env = dict(os.environ, TERTIARY_TEST_DIR=work, TERTIARY_PREFIX=args.prefix, TERTIARY_FX=args.fx,
               WLA_VERIFY_MODULE='1' if args.expected_plugin or sys.platform == 'win32' else '',
               WLA_DRIVER_QUIT='1' if sys.platform == 'win32' else '')
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
        startup.wShowWindow = 1  # SW_SHOWNORMAL: startup dialogs must receive their normal GUI event loop.
    start = time.time()
    # A short defer in the Lua script holds the loaded plugin while its mapped module is inspected.
    proof = None
    inspection_attempts = 0
    proc = subprocess.Popen(command, env=env, startupinfo=startup)
    windows_ui = None
    host_failed = False
    try:
        if sys.platform == 'win32':
            windows_ui = ReaperWindows(proc.pid, work)
        deadline = time.monotonic() + 90
        while proc.poll() is None and time.monotonic() < deadline:
            log_path = Path(output('_log.txt'))
            # [Codex 2026-10-30] A fixed delay released rendering while About was
            # still modal. Require its enabled Still Evaluating button and closure.
            windows_ready = windows_ui.poll(finished=log_path.exists() and
                ('saved ' + args.prefix + '_test.rpp') in log_path.read_text(errors='replace')) if windows_ui else True
            if args.expected_plugin and proof is None and log_path.exists() and 'loaded Tertiary as FX' in log_path.read_text(errors='replace'):
                expected = Path(args.expected_plugin).resolve()
                sub = 'x86_64-win' if sys.platform == 'win32' else 'MacOS'
                binary = next(p for p in (expected / 'Contents' / sub).iterdir() if p.is_file()) if expected.is_dir() else expected
                if sys.platform == 'win32':
                    import psutil
                    parent = psutil.Process(proc.pid)
                    paths = []
                    for process in [parent, *parent.children(recursive=True)]:
                        try:
                            paths.extend(m.path for m in process.memory_maps())
                        except (psutil.NoSuchProcess, psutil.AccessDenied):
                            pass
                else:
                    maps = subprocess.check_output(['vmmap', '-w', str(proc.pid)], text=True, stderr=subprocess.STDOUT, timeout=15)
                    paths = [line[line.index(' /') + 1:].strip() for line in maps.splitlines() if ' /' in line]
                Path(work, 'mapped-modules.txt').write_text('\n'.join(paths), encoding='utf-8')
                loaded = [Path(p) for p in paths if Path(p).name.casefold() == binary.name.casefold() and Path(p).is_file()]
                digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
                for module in loaded:
                    same = digest(module) == digest(binary)
                    if args.fx.upper().startswith('VST3:'):
                        same = same and module.resolve() == binary.resolve()
                    if same:
                        proof = dict(path=str(module), sha256=digest(module), expected=str(binary))
                        Path(work, 'binary-proof.json').write_text(json.dumps(proof, indent=2), encoding='utf-8')
                        print('PASS  REAPER loaded tested binary: ' + json.dumps(proof), flush=True)
                        break
                inspection_attempts += 1
                if proof is None and not loaded and inspection_attempts < 30:
                    time.sleep(0.2)  # A bridged VST may finish loading on the next REAPER event-loop turn.
                    continue
                if proof is None:
                    Path(work, 'module-inspected').touch()
                    raise RuntimeError('REAPER did not map the requested build; mapped candidates: ' + repr(loaded))
            if (proof is not None or not args.expected_plugin) and windows_ready and not Path(work, 'module-inspected').exists():
                if windows_ui:
                    windows_ui.event('render-ready', detail='startup dialogs closed; binary inspection complete')
                Path(work, 'module-inspected').touch()
            time.sleep(0.1)
        if proc.poll() is None:
            print('FAIL  REAPER did not finish its own Quit within 90 seconds; preserving render assertions', flush=True)
            host_failed = True
        elif proc.returncode:
            print('FAIL  REAPER exited with code ' + str(proc.returncode), flush=True)
            host_failed = True
        else:
            print('PASS  REAPER native Quit exited zero in %.2f s' % (time.time() - start), flush=True)
    finally:
        if proc.poll() is None:
            if windows_ui:
                try:
                    windows_ui.capture_timeout()
                except Exception as error:
                    print('REAPER timeout screenshot unavailable: ' + str(error), flush=True)
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=10)
    if args.expected_plugin and proof is None:
        print('FAIL  no proof of the REAPER module path; not accepting a render from an unknown build')
        return 1
    print("REAPER ran for %.1f s" % (time.time() - start))
    if os.path.exists(output("_log.txt")):
        with open(output("_log.txt")) as log:
            print(log.read())

    failed = int(host_failed)

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
