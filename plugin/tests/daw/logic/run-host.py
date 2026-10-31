#!/usr/bin/env python3
"""[Codex 2026-10-30] No model/agent calls. Exit 0 PASS, 1 regression, 2 unavailable, 3 automation error."""
import argparse
import datetime
import os
from pathlib import Path
import signal
import platform
import sys
import traceback
from host_common import Result, Stop, UI, run


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('test', choices=['logic', 'microphone'])
    p.add_argument('--build', help='AU component/build folder, or microphone CMake build directory')
    p.add_argument('--build-id', help='Required when recording an approved Logic golden')
    p.add_argument('--media', help='Folder containing the Cubase harness-input.wav and play-loop.wav')
    p.add_argument('--golden', default=str(Path.home() / '.wla-test/goldens/tertiary/logic'))
    p.add_argument('--record-golden', action='store_true')
    p.add_argument('--out', help='New result directory')
    p.add_argument('--source', help='Microphone-fix source checkout (25b0a55)')
    p.add_argument('--cmake')
    p.add_argument('--juce')
    p.add_argument('--reset-mic', action='store_true', help='Reset only Tertiary\'s existing microphone decision before retesting')
    p.add_argument('--loopback-driver', help='Optional signed BlackHole2ch.driver; installed only for the microphone check, then removed')
    args = p.parse_args()
    here = Path(__file__).resolve().parent
    repo = next((d for d in here.parents if (d / 'CMakeLists.txt').is_file() and (d / 'plugin').is_dir()), here)
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    out = args.out or str(Path.home() / '.wla-test/runs' / args.test / stamp)
    if (Path(out).expanduser() / 'result.json').exists():
        print('Refusing to overwrite a previous result: ' + out)
        return 2
    result = Result('M-09 Logic Pro' if args.test == 'logic' else 'M-07 microphone (V-08)', out)
    ui, code = None, 0
    lock = Path.home() / '.wla-test/mac-host-test.lock'
    owned_lock = False
    try:
        if platform.system() != 'Darwin':
            raise Stop('These tests require macOS and an unlocked desktop', 2)
        try:
            lock.mkdir()
            owned_lock = True
            (lock / 'pid').write_text(str(os.getpid()))
        except FileExistsError:
            raise Stop('Another Mac host test owns ' + str(lock), 2)
        def interrupted(signum, frame):
            raise Stop('Interrupted by signal ' + str(signum), 3)
        signal.signal(signal.SIGTERM, interrupted)
        signal.signal(signal.SIGINT, interrupted)
        exe = result.out / 'mac-ui'
        compiled = run(['swiftc', here / 'mac-ui.swift', '-o', exe], timeout=45, check=False)
        (result.out / 'ui-build.log').write_bytes(compiled.stdout + compiled.stderr)
        result.artifact(result.out / 'ui-build.log')
        if compiled.returncode:
            raise Stop('Swift UI helper did not compile', 3)
        ui = UI(exe, result)
        if args.test == 'logic':
            from logic_test import test
        else:
            from microphone_test import test
        test(args, ui, result, here, repo)
    except Stop as e:
        code = e.code
        result.data['checks'].append(dict(id='stopped', status='SKIP' if code == 2 else 'FAIL', detail=str(e)))
        print(str(e), flush=True)
    except Exception as e:
        code = 3
        result.data['checks'].append(dict(id='exception', status='FAIL', detail=str(e)))
        traceback.print_exc()
    finally:
        if code and ui:
            try:
                ui.shot('stopped')
            except Exception:
                pass
        if ui:
            ui.close()
        if owned_lock:
            (lock / 'pid').unlink(missing_ok=True)
            lock.rmdir()
        result.finish(code)
    return code


if __name__ == '__main__':
    sys.exit(main())
