"""[Codex 2026-10-30] M-07 / V-08: build, usage string, real prompt and TCC grant."""
import json
from pathlib import Path
import plistlib
import shutil
import sqlite3
import time
from host_common import Stop, embedded_build, plist, run, sha

BUNDLE = 'com.WonderlandAudio.Tertiary'
USAGE = 'Tertiary uses your audio input to process it through the tremolo.'


def grant():
    path = Path.home() / 'Library/Application Support/com.apple.TCC/TCC.db'
    with sqlite3.connect(path.as_uri() + '?mode=ro', uri=True) as db:
        return db.execute('SELECT auth_value FROM access WHERE service=? AND client=?',
                          ('kTCCServiceMicrophone', BUNDLE)).fetchall()


def test(args, ui, result, here, repo):
    destination = Path('/Library/Audio/Plug-Ins/HAL/BlackHole2ch.driver')
    installed = False
    if args.loopback_driver:
        driver = Path(args.loopback_driver).expanduser().resolve()
        if destination.exists():
            raise Stop('An existing BlackHole2ch driver is present; it will not be replaced', 2)
        if driver.name != destination.name or not driver.is_dir():
            raise Stop('Expected a signed BlackHole2ch.driver bundle', 2)
        from logic_test import running
        if running() or run(['pgrep', '-x', 'Tertiary'], check=False).returncode == 0:
            raise Stop('Close audio hosts before installing the temporary input', 2)
        run(['codesign', '--verify', '--deep', '--strict', driver])
        devices = run(['system_profiler', 'SPAudioDataType']).stdout
        (result.out / 'audio-devices-before.txt').write_bytes(devices)
        result.artifact(result.out / 'audio-devices-before.txt')
        run(['sudo', '-n', 'ditto', driver, destination])
        installed = True
        run(['sudo', '-n', 'killall', 'coreaudiod'], check=False)
        time.sleep(3)
    try:
        _test(args, ui, result, here, repo)
    finally:
        if installed:
            # Constant, exact path, created above only when absent. Other HAL drivers stay intact.
            run(['sudo', '-n', 'rm', '-r', destination])
            run(['sudo', '-n', 'killall', 'coreaudiod'], check=False)
            time.sleep(2)
            after = run(['system_profiler', 'SPAudioDataType']).stdout
            (result.out / 'audio-devices-restored.txt').write_bytes(after)
            result.artifact(result.out / 'audio-devices-restored.txt')
            result.check('M-07-input-cleanup', not destination.exists() and b'BlackHole' not in after,
                         'Temporary signed loopback input removed; existing HAL drivers and output settings retained; no reboot')


def source_from_cache(build):
    """[Codex 2026-10-31] An existing build tree must actually belong to the selected checkout."""
    for line in (build / 'CMakeCache.txt').read_text(encoding='utf-8').splitlines():
        if line.startswith('CMAKE_HOME_DIRECTORY:INTERNAL='):
            return Path(line.split('=', 1)[1]).resolve()
    raise Stop('CMake cache has no source-directory provenance: ' + str(build))


def _test(args, ui, result, here, repo):
    source = Path(args.source or repo).expanduser().resolve()
    build = Path(args.build or repo / 'build-tests').expanduser().resolve()
    cmake = Path(args.cmake or shutil.which('cmake') or Path.home() / 'Library/Python/3.9/bin/cmake').expanduser()
    if run(['pgrep', '-x', 'Tertiary'], check=False).returncode == 0:
        raise Stop('Close Tertiary before this test; existing application was not touched', 2)
    commit = run(['git', '-C', source, 'rev-parse', 'HEAD']).stdout.decode().strip()
    expected = args.build_id or run(['git', '-C', source, 'rev-parse', '--short', 'HEAD']).stdout.decode().strip()
    result.check('M-07-source', commit.startswith(expected.rstrip('+')), 'Selected source ' + str(source) + '; commit ' + commit)
    if not (build / 'CMakeCache.txt').is_file():
        options = ['-DCPM_JUCE_SOURCE=' + str(Path(args.juce).expanduser())] if args.juce else []
        configure = run([cmake, '-S', source, '-B', build, '-DCMAKE_BUILD_TYPE=Release',
                         '-DWLA_COPY_PLUGIN_AFTER_BUILD=OFF', *options], timeout=120, check=False)
        (result.out / 'configure.log').write_bytes(configure.stdout + configure.stderr)
        result.artifact(result.out / 'configure.log')
        result.check('M-07-configure', configure.returncode == 0, 'Standalone-only Release configuration')
    result.check('M-07-build-source', source_from_cache(build) == source,
                 'CMake build directory belongs to the selected source checkout: ' + str(build))
    p = run([cmake, '--build', build, '--target', 'Tertiary_Standalone', '--config', 'Release', '-j', '2'], timeout=450, check=False)
    (result.out / 'build.log').write_bytes(p.stdout + p.stderr)
    result.artifact(result.out / 'build.log')
    result.check('M-07-build', p.returncode == 0, 'cmake Tertiary_Standalone Release; exit ' + str(p.returncode))
    app = build / 'plugin/Tertiary_artefacts/Release/Standalone/Tertiary.app'
    info = plist(app / 'Contents/Info.plist')
    binary = app / 'Contents/MacOS/Tertiary'
    actual, build_label = embedded_build(binary)
    after = run(['git', '-C', source, 'rev-parse', 'HEAD']).stdout.decode().strip()
    result.data['build'] = dict(path=str(app), id=build_label, source_commit=commit, sha256=sha(binary))
    result.check('M-07-build-identity', commit == after and commit.startswith(actual.rstrip('+')),
                 'Executable embeds ' + build_label + '; source HEAD stayed at ' + commit)
    result.data['host'] = dict(name='macOS standalone', version=run(['sw_vers', '-productVersion']).stdout.decode().strip())
    result.check('V-08-usage-description', info.get('NSMicrophoneUsageDescription') == USAGE and info.get('CFBundleIdentifier') == BUNDLE,
                 'NSMicrophoneUsageDescription: ' + str(info.get('NSMicrophoneUsageDescription')))
    # Exercise the fix with the same hardened-runtime entitlement as the release
    # signing step, using an ad-hoc identity on a disposable copy (no credentials).
    entitlements = source / 'package_config/mac/standalone.entitlements'
    result.check('V-08-entitlements-source', plist(entitlements).get('com.apple.security.device.audio-input') is True,
                 'The fix supplies com.apple.security.device.audio-input=true')
    built_app = app
    app = result.out / 'Tertiary.app'
    shutil.copytree(built_app, app, symlinks=True)
    signed = run(['codesign', '--force', '--sign', '-', '--verbose', '--deep', '--strict',
                  '--options=runtime', '--timestamp=none', '--entitlements', entitlements, app])
    verified = run(['codesign', '--verify', '--deep', '--strict', '--verbose=2', app])
    details = run(['codesign', '-dv', '--verbose=2', app])
    embedded = run(['codesign', '--display', '--entitlements', '-', '--xml', app])
    (result.out / 'signing.log').write_bytes(signed.stderr + verified.stderr + details.stderr + embedded.stdout)
    result.artifact(result.out / 'signing.log')
    result.check('V-08-hardened-runtime', b'runtime' in details.stderr
                 and plistlib.loads(embedded.stdout).get('com.apple.security.device.audio-input') is True,
                 'Disposable app verifies with hardened runtime and the audio-input entitlement; ad-hoc identity')
    result.data['build'].update(path=str(app), source_path=str(built_app), sha256=sha(app / 'Contents/MacOS/Tertiary'))
    before = grant()
    if before:
        if not args.reset_mic:
            raise Stop('Microphone decision already exists; pass --reset-mic to retest this app\'s prompt', 2)
        run(['tccutil', 'reset', 'Microphone', BUNDLE])
    run(['open', '-n', app])
    try:
        deadline = time.monotonic() + 30
        prompt = None
        for _ in range(75):
            for app_name in ('Tertiary', 'UserNotificationCenter', 'CoreServicesUIAgent', 'SecurityAgent'):
                try:
                    rows = ui.dump(app_name)
                except RuntimeError:
                    continue
                text = ' '.join(e.get(k, '') for e in rows for k in ('Title', 'Value', 'Description'))
                allow = [e for e in rows if e.get('Role') == 'AXButton' and e.get('Title') in ('Allow', 'OK')]
                if 'microphone' in text.lower() and 'Tertiary' in text and allow:
                    prompt = (app_name, allow[0])
                    break
            if prompt or time.monotonic() >= deadline:
                break
            time.sleep(.4)
        result.check('M-07-prompt', prompt is not None, 'The macOS microphone permission prompt names Tertiary')
        ui.app = prompt[0]
        ui.shot('microphone-prompt')
        ui.click(prompt[1])
        deadline = time.monotonic() + 15
        after = []
        while time.monotonic() < deadline:
            after = grant()
            if after and after[0][0] == 2:
                break
            time.sleep(.4)
        evidence = result.out / 'microphone-grant.json'
        evidence.write_text(json.dumps(dict(bundle=BUNDLE, service='kTCCServiceMicrophone',
                               before=before, after=after, query_mode='read-only'), indent=2) + '\n')
        result.artifact(evidence)
        result.check('M-07-tcc-grant', after == [(2,)], 'TCC records auth_value=2 (allowed), read-only query scoped to this app')
        ui.app = 'Tertiary'
        rows = ui.wait(lambda r: r if any(e.get('Role') == 'AXSlider' for e in r) else None, detail='Standalone editor')
        ui.shot('microphone-app')
        text = ' '.join(e.get(k, '') for e in rows for k in ('Title', 'Value', 'Description')).lower()
        result.check('M-07-no-denial', 'input denied' not in text and 'permission denied' not in text,
                     'Standalone editor is alive after Allow; no input-denied state')
        devices = run(['system_profiler', 'SPAudioDataType'], timeout=20).stdout.decode()
        (result.out / 'audio-devices.txt').write_text(devices)
        result.artifact(result.out / 'audio-devices.txt')
        result.data['checks'].append(dict(id='M-07-live-input', status='SKIP',
            detail='This check verifies consent and the absence of denial. No default input device is attached on this Mac; no physical-input meter test is claimed.' if 'Input Channels:' not in devices else
            'Consent regression verified; a physical tone/meter test is outside this script.'))
    finally:
        run(['osascript', '-e', 'tell application "Tertiary" to quit'], timeout=10, check=False)
        for _ in range(20):
            if run(['pgrep', '-x', 'Tertiary'], check=False).returncode:
                break
            time.sleep(.25)
        result.check('M-07-cleanup', run(['pgrep', '-x', 'Tertiary'], check=False).returncode != 0,
                     'Standalone closed; its requested microphone grant is retained')
