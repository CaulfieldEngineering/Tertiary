"""[Codex 2026-10-30] Adapters for existing harnesses; no installed plugin fallback."""
import contextlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time

from common import PLATFORM, ROOT, au_reset, host_version, identity, local_path, running, staged


def planned(r, id, detail):
    print('Future placeholder: ' + detail, flush=True)
    r.check(id, 'PLANNED', detail)


def host(r, name, candidates, process=None):
    path = next((Path(p).expanduser() for p in candidates if p and Path(p).expanduser().exists()), None)
    r.data['host']['name'] = name
    if not path:
        r.check('host', 'SKIP', name + ' is not installed')
        return None
    r.data['host']['version'] = host_version(path)
    if process and running(process):
        r.check('host', 'SKIP', name + ' is already running; leaving that session untouched')
        return None
    return path


def py_module(r, module):
    try:
        __import__(module)
        return True
    except ImportError:
        r.check('dependency', 'SKIP', f'Python module {module} is not installed for {sys.executable}')
        return False


def standalone(r, args):
    if PLATFORM == 'win' and not r.cfg.get('wTEST_SMOKE_TOGGLE'):
        planned(r, 'standalone-controls', 'Configure product UI controls for the standalone smoke test')
        return
    app = r.built('Standalone')
    if not app:
        return
    r.select(app)
    r.data['host'] = dict(name=r.product + ' standalone', version=r.cfg['wPROJECT_VERSION'])
    if running(r.product + ('.exe' if PLATFORM == 'win' else '')):
        r.check('standalone', 'SKIP', 'standalone is already running; leaving it untouched')
        return
    if PLATFORM == 'win':
        smoke = ROOT / 'plugin/tests/ui/smoke_standalone.py'
        keys = ('TOGGLE', 'SLIDER', 'MENU_AT', 'LAYOUT', 'MENU_ITEM', 'DIALOG_BUTTON')
        if not smoke.exists() or any(not r.cfg.get('wTEST_SMOKE_' + k) for k in keys):
            planned(r, 'standalone-controls', 'Configure the product UI controls in config.cmake for the standalone smoke test')
            return
        if not py_module(r, 'pywinauto'):
            return
        command = [sys.executable, smoke, app]
        for key in (*keys, 'APPEARS', 'CLOSE_BUTTON'):
            if r.cfg.get('wTEST_SMOKE_' + key):
                command += ['--' + key.lower().replace('_', '-'), r.cfg['wTEST_SMOKE_' + key]]
        command += ['--protect', os.path.expandvars(r.cfg.get('wTEST_SETTINGS_WIN', '%APPDATA%/${wPLUGIN_NAME}/${wPLUGIN_NAME}.settings')),
                    '--timeout', '120', '--screenshots', r.work]
        r.command('standalone-smoke', command, timeout=160, required_text=('SMOKE TEST PASSED',))
    else:
        # Mac-day's start / eight seconds / screenshot / native quit check, on the built app directly.
        settings = Path(r.cfg['wTEST_SETTINGS_MAC']).expanduser()
        original = settings.read_bytes() if settings.exists() else None
        log = r.work / 'standalone.log'
        executable = app / 'Contents/MacOS' / r.product
        with log.open('w') as output:
            process = subprocess.Popen([str(executable)], stdout=output, stderr=subprocess.STDOUT)
            try:
                time.sleep(8)
                r.screenshot('standalone')
                r.check('standalone-open', 'PASS' if process.poll() is None else 'FAIL', 'built app remained open for eight seconds')
                if process.poll() is None:
                    quit_mac(app, process.pid)
                    try:
                        code = process.wait(timeout=15)
                        r.check('standalone-quit', 'PASS' if code == 0 else 'FAIL', 'native Quit returned ' + str(code))
                    except subprocess.TimeoutExpired:
                        r.check('standalone-quit', 'FAIL', 'native Quit timed out; terminating only the test process')
            finally:
                stop_owned(process)
                if original is None:
                    if settings.exists():
                        settings.unlink()
                else:
                    settings.write_bytes(original)
                r.check('settings-restored', 'PASS', str(settings))


def stop_owned(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=10)


def quit_mac(app, pid):
    # Pass paths as argv, never as AppleScript source.
    subprocess.run(['osascript', '-e', 'on run argv\ntell application (item 1 of argv) to quit\nend run', str(app)],
                   capture_output=True, timeout=20)


def reaper(r, args):
    exe = host(r, 'REAPER', [os.environ.get('WLA_REAPER'),
        r'C:\Program Files\REAPER (x64)\reaper.exe' if PLATFORM == 'win' else '/Applications/REAPER.app/Contents/MacOS/REAPER'],
        'reaper.exe' if PLATFORM == 'win' else 'REAPER')
    if not exe:
        return
    if PLATFORM == 'win' and (not py_module(r, 'psutil') or not py_module(r, 'pywinauto')):
        return
    runner = r.cfg.get('wTEST_REAPER_RUNNER')
    if not runner or not local_path(runner).exists():
        planned(r, 'reaper-harness', 'Add this product\'s REAPER render and automation harness')
        return
    vst, ht = r.built('VST3'), r.built('HostTest')
    if not vst or not ht:
        return
    r.select(vst)
    profile = r.work / 'profile'
    profile.mkdir(exist_ok=True)
    scan_path = str(vst.parent)
    if PLATFORM == 'win':
        # REAPER appends standard VST3 directories if absent. Name them first so the
        # custom repo directory remains LAST (duplicate names resolve to the last scan).
        scan_path = ';'.join([r'%COMMONPROGRAMFILES%\VST3', r'%LOCALAPPDATA%\Programs\Common\VST3', str(vst.parent)])
        cache = r.build_dir / 'reaper-scan-cache.ini'
        if not cache.exists():
            cache = Path(os.environ['APPDATA']) / 'REAPER/reaper-vstplugins64.ini'
        if cache.exists():
            # Scan metadata only, never licence/settings files. Rescan this product;
            # avoid initialising every unrelated third-party plugin on each fresh run.
            lines = [line for line in cache.read_text(encoding='utf-8', errors='replace').splitlines()
                     if r.product.casefold() not in line.casefold()]
            (profile / 'reaper-vstplugins64.ini').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    else:
        scan_path = ';'.join(['/Library/Audio/Plug-Ins/VST', '/Library/Audio/Plug-Ins/VST3',
                             '~/Library/Audio/Plug-Ins/VST', '~/Library/Audio/Plug-Ins/VST3', str(vst.parent)])
    settings = '[reaper]\n' + ''.join(key + '=' + scan_path + '\n' for key in
            ('vstpath', 'vstpath64', 'vstpath_arm64')) + 'renderclosewhendone=4\n'
    if PLATFORM == 'win':
        # The verified dummy-device profile from plugin/tests/reaper/README.md.
        settings += '[audioconfig]\nmode=4\ndummy_srate=48000\ndummy_blocksize=512\n'
    else:
        settings += 'coreaudioindevnew=<default system devices>\ncoreaudiooutdevnew=<default system devices>\ncoreaudiosrate=48000\ncoreaudiosrateuse=1\ncoreaudiobs=512\ncoreaudiobsuse=1\n'
    ini = profile / 'reaper.ini'
    ini.write_text(settings, encoding='utf-8')
    # Existing harness accepts a working directory, leaving source fixtures and other runs untouched.
    formats = ['VST3'] + (['AU'] if PLATFORM == 'mac' and r.built('AU', False) else [])
    try:
        for fmt in formats:
            source = r.built(fmt)
            with contextlib.ExitStack() as stack:
                if fmt == 'AU':
                    stack.enter_context(staged(r, source, Path.home() / 'Library/Audio/Plug-Ins/Components' / source.name, au_reset))
                fx_name = r.product
                command = [sys.executable, local_path(runner), ht, '--reaper', exe, '--reaper-config', ini,
                           '--fx', fmt + ':' + fx_name, '--prefix', 'reaper_' + fmt.lower(),
                           '--work-dir', r.work / fmt.lower(), '--expected-plugin', source]
                if fmt == 'VST3':
                    command += ['--vst3', vst]
                r.command('reaper-' + fmt.lower(), command, timeout=200)
    finally:
        evidence = r.work / 'profile-evidence'
        evidence.mkdir(exist_ok=True)
        for name in ('reaper.ini', 'reaper-vstplugins64.ini', 'reaper-vstplugins_arm64.ini', 'reaper-auplugins_arm64.ini'):
            if (profile / name).exists():
                shutil.copy2(profile / name, evidence / name)
        if PLATFORM == 'win' and (profile / 'reaper-vstplugins64.ini').exists():
            shutil.copy2(profile / 'reaper-vstplugins64.ini', r.build_dir / 'reaper-scan-cache.ini')
        # Only this run's owned throwaway profile; keep evidence, not bundled themes or licence notices.
        if profile.resolve().parent != r.work.resolve() or profile.is_symlink():
            raise RuntimeError('Unsafe REAPER profile cleanup path')
        shutil.rmtree(profile)


def cubase(r, args):
    exe = host(r, 'Cubase', [os.environ.get('WLA_CUBASE'), r'C:\Program Files\Steinberg\Cubase 12\Cubase12.exe'], 'Cubase12.exe')
    if not exe:
        return
    runner = r.cfg.get('wTEST_CUBASE_RUNNER')
    if not runner or not local_path(runner).exists():
        planned(r, 'cubase-harness', 'Create this product\'s Cubase export harness project and golden')
        return
    if not py_module(r, 'pywinauto') or not py_module(r, 'psutil'):
        return
    vst, ht = r.built('VST3'), r.built('HostTest')
    if not vst or not ht:
        return
    r.select(vst)
    golden = Path(r.cfg['wTEST_GOLDEN_DIR']).expanduser() / 'cubase'
    if not (golden / 'harness.wav').exists():
        r.check('golden', 'SKIP', 'Cubase golden missing: ' + str(golden / 'harness.wav'))
        return
    build_script = local_path(r.cfg['wTEST_CUBASE_BUILDER'])
    for phase, extra in [('audio', [ht]), ('playaudio', [])]:
        code, _ = r.command('cubase-fixture-' + phase, [sys.executable, build_script, phase, *extra], timeout=120)
        if code:
            return
    destination = Path(os.environ['LOCALAPPDATA']) / 'Programs/Common/VST3' / vst.name
    # Other copies of the same product can shadow the per-user scan. Temporarily stage
    # the same bytes there too, and restore each slot. Permissions failures occur before launch.
    destinations = [destination]
    system_copy = Path(os.environ.get('CommonProgramFiles', r'C:\Program Files\Common Files')) / 'VST3' / vst.name
    if system_copy.exists():
        destinations.append(system_copy)
    before = set(running(exe.name))
    if before:
        r.check('host', 'SKIP', 'Cubase opened during preflight; leaving it untouched')
        return
    with contextlib.ExitStack() as stack:
        for dest in destinations:
            stack.enter_context(staged(r, vst, dest))
        try:
            r.command('cubase-export', ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', local_path(runner),
                        '-NoInstall', '-Vst3', vst, '-Golden', golden,
                        '-Tools', ROOT / 'tests/win/cubase-tools', '-Cubase', exe,
                        '-RunDir', r.work], timeout=420, record=True, preserve_children=True)
        finally:
            # The harness normally quits. On an unknown dialog or timeout, invoke the
            # owned app's Quit; the documented last resort ends ONLY its new PID.
            owned = r.work / 'owned-cubase.pid'
            if owned.exists():
                pid = int(owned.read_text(encoding='ascii').strip())
                if pid not in before and pid in running(exe.name):
                    r.command('cubase-cleanup', [sys.executable, ROOT / 'tests/win/close-cubase.py', str(pid), r.work], timeout=65)


def lfo(r):
    review = r.built('ReviewRender', False)
    scripts = r.cfg.get('wTEST_LFO_SCRIPTS', '').split(';')
    if not any(scripts):
        planned(r, 'product-regressions', 'Configure product-specific DSP regression scripts')
    for script in filter(None, scripts):
        path = local_path(script)
        if not path.exists() or not review:
            r.check(path.stem, 'SKIP', 'regression script or ReviewRender build missing')
        else:
            r.command(path.stem, [sys.executable, path, review, r.work / path.stem], timeout=180)


def isolation(r, args):
    ht, vst = r.built('HostTest', False), r.built('VST3', False)
    source = ROOT / 'plugin/tests/HostTest.cpp'
    if not source.exists() or 'run(11, section11)' not in source.read_text(encoding='utf-8'):
        planned(r, 'two-instance-isolation', 'Two editors: automate and load presets in A while B remains unchanged')
        return
    if not ht or not vst:
        r.check('two-instance-isolation', 'SKIP', 'HostTest or VST3 build missing')
        return
    r.select(vst)
    r.data['host'] = dict(name='HostTest section 11', version=identity(ht)['id'])
    # --only 11 on an older HostTest silently succeeds without executing any section.
    # Require its completion marker; an old CI artifact is an explicit SKIP, never PASS.
    code, text = r.command('two-instance-isolation', [ht, '--vst3', vst, '--only', '11'], timeout=180, record=False)
    if code:
        r.check('two-instance-isolation', 'FAIL', f'HostTest exit {code}')
    elif 'section 11 took' not in text:
        r.check('two-instance-isolation', 'SKIP', 'HostTest binary predates section 11; rebuild it from this branch')
    else:
        r.check('two-instance-isolation', 'PASS', 'HostTest section 11 completed; see two-instance-isolation.log')


def asan(r, args):
    folder = local_path(r.cfg.get('wTEST_ASAN_BUILD_DIR', 'build-asan'))
    cache = folder / 'CMakeCache.txt'
    exe = folder / 'plugin/UiTest_artefacts' / r.release / ('UiTest.exe' if PLATFORM == 'win' else 'UiTest')
    if not cache.exists() or 'WLA_UI_TEST_ASAN:BOOL=ON' not in cache.read_text(errors='replace') or not exe.exists():
        planned(r, 'asan', 'Build UiTest with WLA_UI_TEST_ASAN=ON and run the editor under AddressSanitizer')
        return
    r.select(exe)
    r.command('asan', [exe, '--snapshots', r.work / 'snapshots'], timeout=1200, required_text=('ALL UI TESTS PASSED',))
    log = (r.work / 'asan.log').read_text(errors='replace')
    if 'ERROR: AddressSanitizer' in log:
        r.check('sanitizer-reports', 'FAIL', 'AddressSanitizer reported a memory error; see asan.log')


def logic(r, args):
    project = local_path(r.cfg.get('wTEST_LOGIC_PROJECT', 'tests/mac/logic/' + r.product + ' harness.logicx'))
    if not project.exists():
        planned(r, 'logic-harness', 'no Logic harness project yet; AU validation is covered by test-auval.sh')
        return
    if args.no_logic or os.environ.get('WLA_NO_LOGIC') == '1':
        r.check('logic-harness', 'SKIP', 'Logic opening disabled for this run')
        return
    app = host(r, 'Logic Pro', ['/Applications/Logic Pro.app', '/Applications/Logic Pro Creator Studio.app'])
    if not app:
        return
    if running('Logic Pro') or running('Logic Pro Creator Studio'):
        r.check('logic-harness', 'SKIP', 'Logic is already running; leaving that session untouched')
        return
    au = r.built('AU')
    golden = Path(r.cfg['wTEST_GOLDEN_DIR']).expanduser() / 'logic/harness.wav'
    if not golden.exists():
        r.check('logic-golden', 'SKIP', 'Logic golden missing: ' + str(golden))
        return
    if not au:
        return
    r.select(au)
    with staged(r, au, Path.home() / 'Library/Audio/Plug-Ins/Components' / au.name, au_reset):
        code, _ = r.command('logic-auval', ['auval', '-strict', '-v', r.cfg.get('wTEST_AU_TYPE', 'aufx'),
                r.cfg['wPLUGIN_CODE'], r.cfg['wPLUGIN_MANUFACTURER_CODE']], timeout=180, required_text=('AU VALIDATION SUCCEEDED',))
        if code:
            return
        # A copied project owns its bounce and save prompts; the source project stays untouched.
        copy = r.work / project.name
        shutil.copytree(project, copy)
        output = r.work / 'bounce'
        output.mkdir()
        try:
            r.command('logic-bounce', ['osascript', ROOT / 'tests/mac/logic-bounce.applescript', app, copy, output], timeout=240)
            r.command('logic-compare', [sys.executable, ROOT / 'plugin/tests/regression/compare_renders.py', golden.parent,
                                      output, '--tolerance', '1e-5'], timeout=30)
        finally:
            # Both names were absent before launch; never close a pre-existing Logic session.
            for process_name in ('Logic Pro', 'Logic Pro Creator Studio'):
                for pid in running(process_name):
                    try:
                        quit_mac(app, pid)
                    except subprocess.TimeoutExpired:
                        pass
                    deadline = time.monotonic() + 10
                    while pid in running(process_name) and time.monotonic() < deadline:
                        time.sleep(0.2)
                    if pid in running(process_name):
                        r.screenshot('logic-quit-timeout')
                        os.kill(pid, signal.SIGKILL)
                        r.check('logic-quit', 'FAIL', 'Logic Quit timed out; ended only the test session PID ' + str(pid))


def web(r, args):
    suite_path = ROOT / 'plugin/tests/web'
    if not suite_path.exists():
        web_build = local_path(os.environ.get('WEB_BUILD_DIR', 'build/web'))
        outputs = [web_build / (r.product + 'Web' + ext) for ext in ('.js', '.wasm')]
        if all(p.is_file() and p.stat().st_size for p in outputs):
            r.data['build'] = identity(outputs[-1])
            r.check('web-build', 'PASS', 'Compiled JS and WASM are present in ' + str(web_build))
            planned(r, 'browser-suite', 'Browser assertions are on the separate web-tests branch; this checkout previously checked compilation only')
        else:
            r.check('web-build', 'SKIP', 'repo web build missing; build the Wonderlab demo first')
        return
    package = suite_path / 'package.json'
    if not package.exists():
        r.check('web', 'FAIL', 'web suite exists but package.json is missing')
        return
    npm = shutil.which('npm.cmd' if PLATFORM == 'win' else 'npm')
    if not npm:
        r.check('web', 'SKIP', 'Node.js/npm not installed')
        return
    r.data['host'] = dict(name='Playwright Chromium / Firefox / WebKit', version=json.loads(package.read_text(encoding='utf-8')).get('devDependencies', {}).get('playwright', 'unknown'))
    if not (suite_path / 'node_modules').exists():
        r.check('web', 'SKIP', 'web dependencies missing; run npm ci and npx playwright install in plugin/tests/web')
        return
    web_build = local_path(os.environ.get('WEB_BUILD_DIR', 'build/web'))
    metadata = web_build / 'test-build.json'
    if not metadata.exists():
        r.check('web-build', 'SKIP', 'repo web build missing; run npm run build in plugin/tests/web')
        return
    manifest = json.loads(metadata.read_text(encoding='utf-8'))
    build_id = manifest.get('buildId') or manifest.get('commit') or 'no embedded build id'
    r.data['build'] = dict(path=str(web_build), id=build_id + '; manifest-sha256:' + __import__('hashlib').sha256(metadata.read_bytes()).hexdigest())
    print('Testing web build: ' + str(web_build) + '\nBuild manifest id: ' + r.data['build']['id'], flush=True)
    # The supplied Node/Playwright runner owns browser launch, local serving and teardown.
    # --no-ai prevents it spawning another agent; AI reviews this collected report afterwards.
    env = dict(os.environ, WLA_TEST_RESULTS=str(r.work), PLAYWRIGHT_HTML_OUTPUT_DIR=str(r.work / 'html'),
               PLAYWRIGHT_JSON_OUTPUT_NAME=str(r.work / 'playwright.json'), WEB_BUILD_DIR=str(web_build))
    report_path = suite_path / 'out/report.json'
    started = time.time()
    r.command('web-harness', [npm, 'run', 'test:harness'], timeout=300, cwd=suite_path, env=env)
    code, _ = r.command('web', [npm, 'test', '--', '--no-ai', *([] if args.engine == 'all' else ['--engine=' + args.engine])], timeout=1200, cwd=suite_path, env=env, record=False)
    if not report_path.exists() or report_path.stat().st_mtime < started - 1:
        r.check('web', 'FAIL', f'exit {code}; suite produced no fresh out/report.json')
        return
    shutil.copytree(report_path.parent, r.work / 'web-report', dirs_exist_ok=True)
    data = json.loads(report_path.read_text(encoding='utf-8'))
    for check in data.get('rows', []):
        r.check(check['id'], check['status'].upper(), check.get('error') or check.get('title', ''))
    if code and not any(c['status'] == 'FAIL' for c in r.data['checks']):
        r.check('web-runner', 'FAIL', 'exit ' + str(code))
    if not data.get('rows'):
        r.check('web-runner', 'FAIL', 'empty web report')
    # WebKit must not be described as real Safari. Browser completion is explicit in future reports.
    browsers = data.get('browsers', data.get('meta', {}).get('browsers', []))
    source = suite_path / 'run.mjs'
    if not browsers and source.exists():
        # Compatibility with the existing suite before it emits engine metadata:
        # desktop W-01 rows belong to its Chromium loop, M-01 iPhone rows to WebKit.
        runner_source = source.read_text(encoding='utf-8')
        if 'chromium.launch(' in runner_source and any(c['id'].startswith('W-01-') for c in data.get('rows', [])):
            browsers.append('chromium')
        if 'webkit.launch(' in runner_source and any(c['id'].startswith('M-01-iphone-')
                and 'AppleWebKit' in str(c.get('data', {}).get('ua', '')) for c in data.get('rows', [])):
            browsers.append('webkit')
    for browser in (('chromium', 'firefox', 'webkit') if args.engine == 'all' else (args.engine,)):
        if browser not in browsers:
            planned(r, browser, 'Add ' + browser + ' execution and its completion record to plugin/tests/web')


def performance(r, args):
    exe, vst = r.built('PerfTest', False), r.built('VST3', False)
    if not (ROOT / 'plugin/tests/PerfTest.cpp').exists():
        planned(r, 'performance', 'Measure DSP time per instance, editor CPU and memory growth with the plugin loaded')
        return
    if not exe or not vst:
        r.check('performance', 'SKIP', 'PerfTest or VST3 build missing')
        return
    r.select(vst)
    r.data['host'] = dict(name='PerfTest', version=identity(exe)['id'])
    for mode, options in [('dsp', []), ('live', ['--seconds', '120']), ('instances', ['--count', '30', '--seconds', '60'])]:
        r.command('T2-03' if mode == 'instances' else 'performance-' + mode, [exe, '--vst3', vst, mode, *options, '--out', r.work / (mode + '.json')], timeout=430)
    r.check('performance-budget', 'PLANNED', 'Review the measured CPU/memory numbers against agreed release limits; exit zero alone is not a performance-budget pass')


HANDLERS = dict(standalone=standalone, reaper=reaper, cubase=cubase, isolation=isolation,
                asan=asan, logic=logic, web=web, performance=performance)
