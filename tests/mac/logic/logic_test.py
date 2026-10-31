"""[Codex 2026-10-30] M-09: the saved Logic project, staged AU, bounce and editor."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import time
import os
import pwd
import signal
import subprocess

from host_common import Stop, compare, embedded_build, plist, run, sha, wave_data

APP = Path('/Applications/Logic Pro Creator Studio.app')
PROJECT_TITLES = ('Tertiary harness - Tracks', 'Tertiary harness.logicx - Tracks')


def running():
    return run(['pgrep', '-f', str(APP / 'Contents/MacOS/Logic Pro Creator Studio')], check=False).returncode == 0


def quit_logic(ui=None):
    if ui:
        rows = ui.dump()
        if any(e.get('Role') == 'AXMenu' and e.get('frame', [0, 0, 0])[2] > 0 for e in rows):
            ui.key(53)
    try:
        run(['osascript', '-e', 'tell application "Logic Pro Creator Studio" to quit saving no'], timeout=12)
    except (subprocess.TimeoutExpired, RuntimeError):
        pass
    for _ in range(30):
        if not running():
            return True
        time.sleep(.2)
    # Called only after this run's closed-host precheck and launch, on a disposable
    # project copy. A stuck menu must not strand the user's installed AU.
    pids = run(['pgrep', '-f', str(APP / 'Contents/MacOS/Logic Pro Creator Studio')], check=False).stdout.split()
    for sig in (signal.SIGTERM, signal.SIGKILL):
        for pid in pids:
            try:
                os.kill(int(pid), sig)
            except ProcessLookupError:
                pass
        time.sleep(1)
        if not running():
            return False
    raise Stop('Owned Logic process could not close; backup retained in the result folder', 3)


def reset_cache():
    cache = Path.home() / 'Library/Caches/AudioUnitCache'
    for name in ('com.apple.audiounits.cache', 'com.apple.audiounits.sandboxed.cache'):
        (cache / name).unlink(missing_ok=True)
    run(['killall', '-u', pwd.getpwuid(os.getuid()).pw_name, 'AudioComponentRegistrar'], check=False)


def gui_run(arguments, **kwargs):
    # An SSH bootstrap cannot discover per-user AUs. Enter the existing Aqua session
    # as the same user (no root AU scan, and no privacy setting changes).
    if run(['launchctl', 'managername']).stdout.strip() != b'Aqua':
        arguments = ['sudo', '-n', 'launchctl', 'asuser', str(os.getuid()),
                     'sudo', '-n', '-u', pwd.getpwuid(os.getuid()).pw_name, *arguments]
    return run(arguments, **kwargs)


def wait_project(ui):
    deadline = time.monotonic() + 70
    handled = set()
    while time.monotonic() < deadline:
        try:
            rows = ui.dump()
        except RuntimeError:
            time.sleep(.4)
            continue
        titles = [e.get('Title', '') for e in rows if e.get('Role') == 'AXWindow']
        texts = '\n'.join(e.get('Value', '') for e in rows if e.get('Role') == 'AXStaticText')
        buttons = {e.get('Title') for e in rows if e.get('Role') == 'AXButton'}
        if any(s in texts.lower() for s in ('could not be found', 'missing audio', 'locate the file')):
            ui.shot('missing-media')
            if 'Cancel' in buttons:
                ui.button('Cancel')
            raise Stop('Logic reported missing media; the packaged media precheck did not resolve it', 3)
        if 'Update' in buttons and 'project' in texts.lower() and 'update' not in handled:
            ui.shot('update-project-notice')
            ui.button('Update')
            handled.add('update')
            continue
        if any(s in texts.lower() for s in ('incompatible audio units', 'couldn’t be opened', 'not available')):
            ui.shot('au-validation-notice')
            if 'OK' in buttons:
                ui.button('OK')
            raise Stop('Logic rejected the staged AU: ' + texts)
        if titles and titles[0] in PROJECT_TITLES and not ui.find(rows, role='AXSheet'):
            return rows
        time.sleep(.4)
    ui.shot('unknown-load-dialog')
    raise Stop('Unknown load dialog or project did not finish loading within 70 seconds', 3)


def bounce(ui, destination):
    ui.key(11, 'cmd')
    ui.wait(lambda r: ui.find(r, role='AXStaticText', value='File Type:'), detail='Bounce settings')
    # Each popup is found from its adjacent label, rather than a fixed screen location.
    for label, value in [('File Type:', 'WAVE'), ('Bit Depth:', '32-bit float'),
                         ('Sample Rate:', '48 kHz'), ('Format:', 'Interleaved'),
                         ('Dithering:', 'None'), ('Mode:', 'Offline'), ('Normalize:', 'Off')]:
        rows = ui.dump()
        text = ui.find(rows, role='AXStaticText', value=label)[0]
        x, y, w, h = text['frame']
        candidates = [e for e in rows if e.get('Role') == 'AXPopUpButton'
                      and abs(e['frame'][1] - y) < 4 and e['frame'][0] >= x + w - 2]
        if not candidates:
            raise Stop('Bounce popup missing: ' + label, 3)
        ui.popup(min(candidates, key=lambda e: e['frame'][0]), value)
    for label in ('Surround Bounce', 'Add to Project', 'Add to Apple Music Library',
                  'Bounce 2nd Cycle Pass', 'Include Audio Tail', 'Include Tempo Information'):
        ui.checkbox(label, False)
    rows = ui.dump()
    for desc, value in [('Uncompressed', True), ('MP3', False), ('M4A', False), ('Burn to CD / DVD', False)]:
        e = ui.find(rows, role='AXCheckBox', desc=desc)[0]
        if e.get('Value') != str(int(value)):
            ui.click(e)
    ui.shot('bounce-settings')
    ui.button('OK')
    ui.wait(lambda r: ui.find(r, role='AXButton', title='Bounce'), detail='Bounce save dialog')
    # The file name field is adjacent to Save As, including on newer Logic versions.
    rows = ui.dump()
    label = ui.find(rows, role='AXStaticText', value='Save As:')[0]
    name = [e for e in rows if e.get('Role') == 'AXTextField' and abs(e['frame'][1] - label['frame'][1]) < 6][0]
    ui.set(name, destination.stem)
    ui.goto_folder(str(destination.parent))
    ui.button('Bounce')
    deadline = time.monotonic() + 90
    last_size, stable = -1, 0
    while time.monotonic() < deadline:
        if destination.exists():
            size = destination.stat().st_size
            stable = stable + 1 if size == last_size else 0
            last_size = size
            if stable >= 3 and size > 1000:
                rows = ui.dump()
                if any(e.get('Role') == 'AXWindow' and e.get('Title') in PROJECT_TITLES for e in rows) and not ui.find(rows, role='AXSheet'):
                    ui.result.artifact(destination)
                    return
        time.sleep(.5)
    ui.shot('bounce-failure')
    raise Stop('Bounce did not create a completed WAV within 90 seconds', 3)


def editor(ui, expected):
    rows = ui.dump()
    tracks = [e for e in rows if e.get('Role') == 'AXTextField' and e.get('Description') == 'Tertiary - measured']
    ui.click(tracks[0])
    rows = ui.dump()
    # The first channel strip is the selected measured track; opening the slot does not alter its state.
    plug = [e for e in rows if e.get('Role') == 'AXButton' and e.get('Description') == 'open' and e['frame'][0] < 118]
    if not plug:
        plug = [e for e in rows if e.get('Description') == 'audio plug-in' and e['frame'][0] < 118]
    if not plug:
        # Logic exposes an occupied slot as three controls, including its name.
        plug = [e for e in rows if e.get('Role') == 'AXButton' and e.get('Description') == 'Tertiary']
    if not plug:
        raise Stop('Tertiary insert not found on the measured channel', 3)
    ui.click(plug[0])
    rows = ui.wait(lambda r: r if ui.find(r, role='AXSlider', title='Low Depth') else None, detail='Tertiary editor')
    win = ui.find(rows, role='AXWindow', title='Tertiary - measured')[0]
    x, y, w, h = win['frame']
    content = [e for e in rows if e.get('Role') == 'AXGroup' and 'frame' in e and e['frame'][2:] == [750, 478]]
    ui.result.check('D-LGC-editor-size', bool(content), 'Editor content is 750 x 478 logical pixels at 100%%; host window %.0f x %.0f' % (w, h))
    ui.shot('editor')
    # Positions are relative to the verified editor content, not to the desktop.
    cx, cy, cw, ch = content[0]['frame']
    ui.xy(cx + 728, cy + 21)
    menu = ui.wait(lambda r: [e for e in r if e.get('Role') == 'AXMenuItem'
                             and e.get('Title') in ('Activate Tertiary...', 'About Tertiary...')], detail='Tertiary menu')
    ui.press(menu[0])
    ui.wait(lambda r: ui.find(r, role='AXButton', title='X'), detail='About')
    # About panel is 360 x 300, centered in the editor's 750 x 478 layout.
    rows = ui.dump()
    close = ui.find(rows, role='AXButton', title='X')[0]
    bx, by, bw, bh = close['frame']
    ui.xy(bx - 32, by + 30, 'ctrl', 'shift')
    ui.shot('about-build')
    ui.xy(bx - 32, by + 30)
    copied = run(['pbpaste']).stdout.decode('utf-8', 'replace').strip()
    ui.result.check('D-LGC-build-id', expected in copied, 'About copied: ' + copied)
    ui.click(close)
    ui.click(ui.one(role='AXButton', desc='close'))


def test(args, ui, result, here, repo):
    if not APP.is_dir():
        raise Stop('Logic Pro Creator Studio is not installed', 2)
    if running():
        raise Stop('Close Logic before running; no existing project was touched', 2)
    result.data['host'] = {'name': 'Logic Pro Creator Studio', 'version': plist(APP / 'Contents/Info.plist')['CFBundleShortVersionString']}
    candidates = [repo / 'build/plugin/Tertiary_artefacts/Release/AU/Tertiary.component',
                  repo / 'build/plugin/Tertiary_artefacts/AU/Tertiary.component', Path.home() / 'macday/in/build/Tertiary.component']
    source = Path(args.build).expanduser().resolve() if args.build else next((p for p in candidates if p.is_dir()), candidates[0])
    if source.is_dir() and source.suffix != '.component':
        source = source / 'Tertiary.component'
    binary = source / 'Contents/MacOS/Tertiary'
    if not binary.is_file():
        raise Stop('Built AU missing: ' + str(source), 2)
    build_id, build_label = embedded_build(binary)
    result.data['build'] = dict(path=str(source), id=build_label, sha256=sha(binary))
    print('Testing AU %s, build %s, SHA256 %s' % (source, build_id, sha(binary)), flush=True)
    if args.build_id:
        result.check('build-selection', build_id.rstrip('+') == args.build_id.rstrip('+'),
                     'Requested ' + args.build_id + '; actual ' + build_label)
    golden = Path(args.golden).expanduser().resolve()
    if not args.record_golden and not (golden / 'harness.wav').is_file():
        raise Stop('No golden; use --record-golden --build-id <approved build> once', 2)
    if args.record_golden and (not args.build_id or (golden / 'harness.wav').exists()):
        raise Stop('Recording requires an explicit approved --build-id and an empty golden folder', 2)
    media = Path(args.media).expanduser().resolve() if args.media else repo / 'plugin/tests/daw/cubase/Tertiary harness/Audio'
    project = result.out / 'Tertiary harness.logicx'
    shutil.copytree(here / project.name, project, ignore=shutil.ignore_patterns('Media', 'Bounces', 'Project File Backups', 'PreviewMovie.mp4', 'WindowImage.jpg'))
    dest_media = project / 'Media/Audio Files'
    dest_media.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((here / 'media.json').read_text())
    for name, expected in manifest.items():
        source_media = media / name
        if not source_media.is_file():
            raise Stop('Generate Cubase media first, or pass --media: ' + str(source_media), 2)
        fmt, data = wave_data(source_media)
        result.check('media-' + name, hashlib.sha256(data).hexdigest() == expected['pcm_sha256'] and fmt == expected['format'], 'Same PCM as Cubase: ' + str(source_media))
        shutil.copy2(source_media, dest_media / name)
    installed = Path.home() / 'Library/Audio/Plug-Ins/Components/Tertiary.component'
    installed.parent.mkdir(parents=True, exist_ok=True)
    backup = result.out / 'previous-Tertiary.component'
    had_previous = installed.exists()
    previous_sha = sha(installed / 'Contents/MacOS/Tertiary') if had_previous else None
    if had_previous:
        shutil.move(str(installed), str(backup))
    launched = False
    try:
        shutil.copytree(source, installed, symlinks=True)
        result.check('stage-au', sha(installed / 'Contents/MacOS/Tertiary') == sha(binary), 'Staged this build in the per-user Components folder; previous copy backed up')
        reset_cache()
        gui_run(['auval', '-a'], timeout=30)
        validation = gui_run(['auval', '-strict', '-v', 'aufx', 'EXPL', 'WLAD'], timeout=100, check=False)
        log = result.out / 'auval.txt'
        log.write_bytes(validation.stdout + validation.stderr)
        result.artifact(log)
        result.check('D-LGC-auval', validation.returncode == 0 and b'AU VALIDATION SUCCEEDED' in validation.stdout, 'Strict auval aufx EXPL WLAD, exit ' + str(validation.returncode))
        renders = []
        for n in range(2 if args.record_golden else 1):
            run(['open', '-a', APP, project])
            launched = True
            rows = wait_project(ui)
            result.check('D-LGC-load-%d' % (n + 1), bool(ui.find(rows, role='AXTextField', desc='Tertiary - measured')), 'Saved two-track harness loaded without a missing-media or AU warning')
            output = result.out / ('bounce-%d.wav' % (n + 1))
            bounce(ui, output)
            fmt, samples = wave_data(output)
            floats = [v[0] for v in struct.iter_unpack('<f', samples)] if fmt['code'] == 3 and fmt['bits'] == 32 else []
            result.check('D-LGC-bounce-%d' % (n + 1), fmt == dict(code=3, channels=2, rate=48000, bits=32, frames=1152000)
                         and bool(floats) and .0001 < max(abs(v) for v in floats) < 1.0, '24 s, stereo 48 kHz / float32, non-silent, no clipping; ' + str(fmt))
            if n == 0:
                editor(ui, build_id)
            normal_quit = quit_logic(ui)
            launched = False
            result.check('D-LGC-quit-%d' % (n + 1), normal_quit, 'Logic quit without saving the disposable project')
            renders.append(output)
        reference = renders[0] if args.record_golden else golden / 'harness.wav'
        report = compare(renders[-1], reference)
        p = result.out / 'compare.json'
        p.write_text(json.dumps(report, indent=2) + '\n')
        result.artifact(p)
        result.check('D-LGC-golden', report['equal'], 'Exact PCM equality, tolerance 0; ' + json.dumps(report))
        if args.record_golden:
            golden.mkdir(parents=True, exist_ok=True)
            shutil.copy2(renders[0], golden / 'harness.wav')
            (golden / 'golden-info.json').write_text(json.dumps(dict(build=result.data['build'], host=result.data['host'],
                method='Two separate Logic launches and bounces; exact float32 PCM equality', compare=report,
                project_sha256=sha(project / 'Alternatives/000/ProjectData')), indent=2) + '\n')
    except Exception:
        ui.shot('logic-failure')
        raise
    finally:
        if launched and running():
            quit_logic(ui)
        if installed.exists():
            shutil.rmtree(installed)
        if had_previous:
            shutil.move(str(backup), str(installed))
        reset_cache()
        result.check('restore-au', (sha(installed / 'Contents/MacOS/Tertiary') == previous_sha) if had_previous else not installed.exists(), 'Previous per-user AU restored byte-identically; Logic closed; AU cache invalidated for the restored copy')
