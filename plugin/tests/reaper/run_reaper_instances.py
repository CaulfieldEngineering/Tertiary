"""[Claude 2026-10-31] T2-03: 30 Tertiary instances playing in real time in REAPER, without dropouts.

Joe: "Can you do the 30 instance test inside of a DAW? Reaper?" The requirement is that 30 instances play
without dropouts in a real DAW. The bare single-thread PerfTest harness measured this machine's scheduler,
not Tertiary: on the Windows mini PC a control load with no plugin at all missed the same deadline. So the
pass criterion moved here, and PerfTest stays as a recorded diagnostic.

REAPER runs with an isolated profile at 48 kHz / 128 samples and its normal processing settings (anticipative
FX processing on, as by default). tertiary_reaper_instances.lua builds the project, plays it for 60 s and
reads reaper.GetUnderrunTime(). PASS needs: every instance loaded the tested binary, playback ran the whole
time at 48 kHz / 128, zero audio and zero media xruns, and no lost time (REAPER's dummy device falls behind the clock
under overload instead of flagging an xrun: 600 instances reported 0 xruns but played 17.4 s in 20 s). Media xruns and REAPER's CPU time are recorded.

  python run_reaper_instances.py <HostTest> --reaper <exe> --reaper-config <ini> --fx VST3:Tertiary \
      --work-dir <dir> --expected-plugin <bundle> --out <result.json>
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
if sys.platform == 'win32':
    sys.path.insert(0, str(HERE.parents[2] / 'tests' / 'lib'))
    from reaper_windows import ReaperWindows


def cpu_seconds(pid):
    """REAPER's own CPU time (all threads), from the OS."""
    try:
        import psutil
        t = psutil.Process(pid).cpu_times()
        return t.user + t.system
    except ImportError:
        out = subprocess.check_output(['ps', '-o', 'time=', '-p', str(pid)], text=True).strip()
        parts = [float(x) for x in out.replace('-', ':').split(':')]
        return sum(v * 60 ** i for i, v in enumerate(reversed(parts)))


def mapped(pid):
    if sys.platform == 'win32':
        import psutil
        parent, paths = psutil.Process(pid), []
        for process in [parent, *parent.children(recursive=True)]:
            try:
                paths.extend(m.path for m in process.memory_maps())
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass
        return paths
    maps = subprocess.check_output(['vmmap', '-w', str(pid)], text=True, stderr=subprocess.STDOUT, timeout=30)
    return [line[line.index(' /') + 1:].strip() for line in maps.splitlines() if ' /' in line]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('hosttest')
    p.add_argument('--reaper', required=True)
    p.add_argument('--reaper-config', required=True)
    p.add_argument('--fx', default='VST3:Tertiary')
    p.add_argument('--prefix', default='instances')
    p.add_argument('--work-dir', required=True)
    p.add_argument('--expected-plugin', required=True)
    p.add_argument('--count', type=int, default=30)
    p.add_argument('--seconds', type=float, default=60)
    p.add_argument('--out', required=True)
    args = p.parse_args()

    work = Path(args.work_dir).resolve()
    (work / 'out').mkdir(parents=True, exist_ok=True)
    for name in ('module-inspected', 'input.wav'):
        (work / name).unlink(missing_ok=True)
    log_path = work / 'out' / (args.prefix + '_log.txt')
    result_path = work / 'out' / (args.prefix + '_instances.json')
    for stale in (log_path, result_path):
        stale.unlink(missing_ok=True)
    subprocess.run([str(Path(args.hosttest).resolve()), '--write-reaper-input', work / 'input.wav'], check=True)

    # --expected-plugin none: a diagnostic control run with another plug-in (for example "VST:ReaEQ (Cockos)");
    # its binary is not checked and its result is never a Tertiary pass.
    control = args.expected_plugin == 'none'
    expected = None if control else Path(args.expected_plugin).resolve()
    sub = 'x86_64-win' if sys.platform == 'win32' else 'MacOS'
    binary = None if control else next(x for x in (expected / 'Contents' / sub).iterdir() if x.is_file()) if expected.is_dir() else expected
    digest = lambda x: hashlib.sha256(Path(x).read_bytes()).hexdigest()

    env = dict(os.environ, TERTIARY_TEST_DIR=str(work), TERTIARY_PREFIX=args.prefix, TERTIARY_FX=args.fx,
               TERTIARY_COUNT=str(args.count), TERTIARY_SECONDS=str(args.seconds),
               WLA_DRIVER_QUIT='1' if sys.platform == 'win32' else '')
    command = [args.reaper, '-cfgfile', str(Path(args.reaper_config).resolve()), '-newinst', '-nosplash',
               str(HERE / 'tertiary_reaper_instances.lua')]
    if sys.platform == 'darwin':
        command += ['-ApplePersistence', 'NO']
    startup = None
    if sys.platform == 'win32':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 1
    checks, proof, cpu = [], None, {}
    check = lambda id, ok, detail: checks.append(dict(id=id, status='PASS' if ok else 'FAIL', detail=detail))
    proc = subprocess.Popen(command, env=env, startupinfo=startup)
    ui = ReaperWindows(proc.pid, work) if sys.platform == 'win32' else None
    deadline = time.monotonic() + 120 + args.seconds
    try:
        while proc.poll() is None and time.monotonic() < deadline:
            text = log_path.read_text(errors='replace') if log_path.exists() else ''
            ready = ui.poll(finished='instances-done' in text) if ui else True
            if proof is None and control and 'loaded Tertiary as FX' in text:
                proof = dict(control=args.fx)
            if proof is None and 'loaded Tertiary as FX' in text:
                for module in (Path(x) for x in mapped(proc.pid) if Path(x).name.casefold() == binary.name.casefold()):
                    if module.is_file() and digest(module) == digest(binary) and (
                            args.fx.upper().startswith('AU:') or module.resolve() == binary.resolve()):
                        proof = dict(path=str(module), sha256=digest(module))
                        break
            if proof and ready and not (work / 'module-inspected').exists():
                (work / 'module-inspected').touch()
            if 'play-start' in text and 'start' not in cpu:
                cpu['start'] = cpu_seconds(proc.pid)
            if 'play-end' in text and 'end' not in cpu and 'start' in cpu:
                cpu['end'] = cpu_seconds(proc.pid)
            time.sleep(0.2)
        quit_ok = proc.poll() is not None and proc.returncode == 0
    finally:
        if proc.poll() is None:
            if ui:
                try:
                    ui.capture_timeout()
                except Exception:
                    pass
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
    print(log_path.read_text(errors='replace') if log_path.exists() else 'no REAPER log', flush=True)
    data = json.loads(result_path.read_text(encoding='utf-8')) if result_path.exists() else {}
    fmt = args.fx.split(':')[0].lower()
    check(f'{fmt}-binary', proof is not None and not control, 'REAPER loaded the tested binary: ' + json.dumps(proof))
    check(f'{fmt}-instances', data.get('loaded') == args.count == data.get('count'),
          f"{data.get('loaded')} of {args.count} Tertiary instances online ({data.get('idents', '')})")
    check(f'{fmt}-device', str(data.get('srate')) in ('48000', '48000.0') and str(data.get('block')) == '128',
          f"audio device {data.get('device_mode')} in={data.get('device_in')} out={data.get('device_out')}, "
          f"{data.get('srate')} Hz / {data.get('block')} samples; REAPER {data.get('reaper')}")
    check(f'{fmt}-played', data.get('seconds', 0) >= args.seconds - 0.5 and data.get('not_playing_polls', 1) == 0
          and data.get('play_position', 0) >= args.seconds - 2,
          f"played {data.get('seconds', 0):.1f} s in real time, play position {data.get('play_position', 0):.1f} s, "
          f"{data.get('polls')} polls, never stopped: {data.get('not_playing_polls') == 0}")
    # Lost time: playback that falls behind the clock is audio that did not play. The play position moves
    # once per 128-sample block, and the audio device's clock differs slightly from the system clock (measured
    # 130 to 185 ppm, device ahead, on the mini PC), so the allowance is two blocks plus 200 ppm of the run.
    # Overload is far outside it: 600 instances lost 213 ms in 19 s on WASAPI and 1.9 s on the dummy device.
    block_s = 128 / 48000
    lost = data.get('lost_seconds', 99)
    measured = max(data.get('seconds', 0) - 1, 0)
    allowance = 2 * block_s + 200e-6 * measured
    check(f'{fmt}-real-time', lost <= allowance,
          f"lost time {lost * 1000:.1f} ms over the measured {measured:.0f} s (allowance {allowance * 1000:.1f} ms; negative = device clock ahead)")
    used = (cpu['end'] - cpu['start']) if {'start', 'end'} <= cpu.keys() else None
    # Both xrun kinds are dropouts. With anticipative FX processing (REAPER's default) an overload empties
    # REAPER's render-ahead buffer and shows as a media xrun while the device callback itself never overruns:
    # 600 instances on WASAPI gave 0 audio and 21 media xruns. So PASS needs zero of each.
    check(f'{fmt}-no-dropouts', data.get('audio_xruns') == 0 and data.get('media_xruns') == 0,
          f"audio xruns {data.get('audio_xruns')}, media xruns {data.get('media_xruns')} (both must be 0) {data.get('xrun_times', '')}; "
          f"REAPER CPU {('%.1f s, %.0f%% of one core' % (used, 100 * used / max(data.get('seconds', 1), 1))) if used is not None else 'not measured'}")
    check(f'{fmt}-quit', quit_ok, 'REAPER quit normally' if quit_ok else 'REAPER did not quit by itself in time')
    Path(args.out).write_text(json.dumps(dict(checks=checks, measurements=data, cpu_seconds=used, proof=proof), indent=2) + '\n',
                              encoding='utf-8')
    for c in checks:
        print(f"{c['status']}  {c['id']}: {c['detail']}", flush=True)
    return 0 if all(c['status'] == 'PASS' for c in checks) else 1


if __name__ == '__main__':
    sys.exit(main())
