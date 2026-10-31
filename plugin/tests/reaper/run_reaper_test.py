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
               WLA_VERIFY_MODULE='1' if args.expected_plugin else '',
               WLA_DRIVER_QUIT='1' if sys.platform == 'win32' and args.expected_plugin else '')
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
    proof_ready = None
    inspection_attempts = 0
    proc = subprocess.Popen(command, env=env, startupinfo=startup)
    last_close = 0
    window_state = None
    about_seen = None
    host_failed = False
    try:
        deadline = time.monotonic() + 90
        while proc.poll() is None and time.monotonic() < deadline:
            log_path = Path(output('_log.txt'))
            if sys.platform == 'win32':
                # [Codex 2026-10-30] A fresh profile can show update/evaluation notices
                # after the startup Lua has already rendered. Close only these named
                # informational dialogs in OUR process; never touch purchase/activation.
                from pywinauto import Desktop
                import ctypes
                windows = Desktop(backend='win32').windows(process=proc.pid)
                state = [(w.window_text(), w.class_name(),
                          [(b.window_text(), b.is_enabled()) for b in w.children(class_name='Button')]) for w in windows]
                if state != window_state:
                    with Path(work, 'windows.log').open('a', encoding='utf-8') as ui_log:
                        ui_log.write(json.dumps(state) + '\n')
                    window_state = state
                for window in windows:
                    title = window.window_text()
                    if title.startswith('About REAPER v'):
                        if about_seen is None:
                            window.set_focus()  # Activate the standard dialog before waiting for its countdown.
                        about_seen = about_seen or time.monotonic()
                        # Closing the informational About window is equivalent to its X.
                        # Honour the evaluation countdown before doing so.
                        if time.monotonic() - about_seen >= 8:
                            ctypes.windll.user32.PostMessageW(window.handle, 0x0010, 0, 0)
                    for button in window.children(class_name='Button'):
                        label = button.window_text().replace('&', '').lower()
                        allowed = (title == 'REAPER New Version Notification' and label == 'close') or (
                            title.startswith('About REAPER v') and 'still evaluating' in label)
                        if allowed and button.is_enabled():
                            # Post BM_CLICK so an unresponsive modal cannot block the watchdog.
                            ctypes.windll.user32.PostMessageW(button.handle, 0x00F5, 0, 0)
                if (not any(w.window_text().startswith('About REAPER v') for w in windows)
                        and time.monotonic() - last_close > 2 and log_path.exists()
                        and ('saved ' + args.prefix + '_test.rpp') in log_path.read_text(errors='replace')):
                    main = next((w for w in windows if w.class_name() == 'REAPERwnd'), None)
                    if main:
                        # Native File: Quit, retried after any startup informational dialog closes.
                        ctypes.windll.user32.PostMessageW(main.handle, 0x0111, 40004, 0)
                        last_close = time.monotonic()
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
                        proof_ready = time.monotonic() + (8 if sys.platform == 'win32' else 0)
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
            # A fresh Windows profile's evaluation notice has a normal countdown.
            # Let its event loop and the named Still Evaluating button finish before
            # Lua renders/quits; quitting behind that startup modal can strand REAPER.
            if proof is not None and time.monotonic() >= proof_ready and not Path(work, 'module-inspected').exists():
                Path(work, 'module-inspected').touch()
            time.sleep(0.1)
        if proc.poll() is None:
            print('FAIL  REAPER did not finish its own Quit within 90 seconds; preserving render assertions', flush=True)
            host_failed = True
        elif proc.returncode:
            print('FAIL  REAPER exited with code ' + str(proc.returncode), flush=True)
            host_failed = True
    finally:
        if proc.poll() is None:
            if sys.platform == 'win32':
                from PIL import ImageGrab
                try:
                    ImageGrab.grab(all_screens=True).save(Path(work, 'reaper-timeout.png'))
                except Exception:
                    pass
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
