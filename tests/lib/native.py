"""[Codex 2026-10-30] Independent native checks shared by the platform wrappers."""
import contextlib
import os
from pathlib import Path
import re
import sys

from common import PLATFORM, ROOT, au_reset, identity, running, staged


def metric(r, name, actual):
    """Keep the old CI assertion counts reviewable, and fail any loss of coverage."""
    import json
    baseline = json.loads((ROOT / 'tests/ci-baseline.json').read_text(encoding='utf-8'))
    minimum = baseline.get(PLATFORM, {}).get(name, 0)
    r.data.setdefault('metrics', {})[name] = {'before': minimum, 'after': actual}
    r.check('coverage-' + name, 'PASS' if actual >= minimum else 'FAIL',
            f'{actual} executed; pre-layout baseline {minimum}')


def unit(r, args):
    exe = r.built(r.product + 'Tests')
    if exe:
        r.select(exe)
        _, text = r.command('unit', [exe], timeout=300, required_text=('ALL TESTS PASSED',))
        metric(r, 'unit_sections', len(re.findall(r'^Starting tests in:', text, re.M)))


def uirobot(r, args):
    exe = r.built('UiTest')
    if exe:
        r.select(exe)
        # TestHooks uses a fixed child of the OS temp folder. Give this invocation its
        # own temp root so another worktree cannot rewrite its saved-size assertions.
        temp = r.work / 'temp'
        temp.mkdir()
        env = dict(os.environ, TMP=str(temp), TEMP=str(temp), TMPDIR=str(temp) + os.sep)
        _, text = r.command('ui-robot', [exe, '--snapshots', r.work / 'snapshots'],
                            timeout=1240, env=env, required_text=('ALL UI TESTS PASSED',))
        match = re.search(r'=== UI tests: (\d+) passed, (\d+) failed', text)
        metric(r, 'ui_assertions', sum(map(int, match.groups())) if match else 0)


def lfo(r, args):
    from hosts import lfo as run_lfo
    vst = r.built('VST3')
    if vst:
        r.select(vst)
        run_lfo(r)


def formats(r):
    return ['VST3'] + (['AU'] if PLATFORM == 'mac' and 'AU' in r.cfg['wFORMATS'].split() else [])


@contextlib.contextmanager
def loaded_format(r, fmt):
    path = r.built(fmt)
    if path:
        r.select(path)
        if fmt == 'AU':
            with staged(r, path, Path.home() / 'Library/Audio/Plug-Ins/Components' / path.name, au_reset) as copy:
                yield copy
        else:
            yield path
    else:
        yield None


def hosttest(r, args):
    exe = r.built('HostTest')
    if not exe:
        return
    r.data['host'] = dict(name='HostTest', version=identity(exe)['id'])
    # The old Mac CI checked six universal binaries (five in the template).
    # These packaging checks travel with M-03 instead of disappearing in the split.
    if PLATFORM == 'mac' and args.profile == 'ci':
        architectures(r)
    for fmt in formats(r):
        with loaded_format(r, fmt) as path:
            if not path:
                continue
            command = [exe, *(['--vst3', path] if fmt == 'VST3' else ['--au'])]
            if args.expect_demo:
                command.append('--expect-demo')
            _, text = r.command('hosttest-' + fmt.lower(), command, timeout=360)
            match = re.search(r'HostTest .*?: (\d+) passed, (\d+) failed, (\d+) skipped', text)
            metric(r, 'host_' + fmt.lower() + '_pass', int(match[1]) if match else 0)
            if match:
                r.data.setdefault('host_counts', {})[fmt] = dict(zip(('passed', 'failed', 'skipped'), map(int, match.groups())))


def architectures(r):
    kinds = [*formats(r), 'ReviewRender', r.product + 'Tests', 'HostTest']
    if (ROOT / 'plugin/tests/UiTest.cpp').exists():
        kinds.append('UiTest')
    count = 0
    for kind in kinds:
        path = r.built(kind)
        if not path:
            continue
        if path.is_dir():
            path = path / 'Contents/MacOS' / r.product
        code, text = r.command('architectures-' + kind, ['lipo', '-archs', path], record=False)
        r.check('universal-' + kind, 'PASS' if code == 0 and {'arm64', 'x86_64'} <= set(text.split()) else 'FAIL', text.strip())
        count += 1
        for arch in ('arm64', 'x86_64'):
            code, text = r.command('minos-' + kind + '-' + arch, ['vtool', '-arch', arch, '-show-build', path], record=False)
            minimum = re.findall(r'^\s*minos\s+(\S+)', text, re.M)
            expected = r.cfg.get('wMACOS_MIN_VERSION', '11.0')
            r.check('minos-' + kind + '-' + arch, 'PASS' if code == 0 and minimum == [expected] else 'FAIL',
                    f'{minimum}; expected {expected}')
            count += 1
    metric(r, 'architecture_min_os_assertions', count)


def validator(r):
    candidates = [os.environ.get('WLA_PLUGINVAL'),
                  r.build_dir / ('pluginval/pluginval.exe' if PLATFORM == 'win' else 'pluginval/pluginval.app/Contents/MacOS/pluginval'),
                  '/Applications/pluginval.app/Contents/MacOS/pluginval']
    return next((Path(p) for p in candidates if p and Path(p).is_file()), None)


def pluginval(r, args):
    exe = validator(r)
    if not exe:
        r.check('pluginval', 'SKIP', 'pluginval missing; set WLA_PLUGINVAL or put it in the build folder')
        return
    r.data['host'] = dict(name='pluginval', version=identity(exe)['id'])
    for fmt in formats(r):
        with loaded_format(r, fmt) as path:
            if not path:
                continue
            # Keep Mac-day's separate no-GUI and GUI passes. CI always runs the GUI pass.
            for gui in ([False, True] if PLATFORM == 'mac' and args.profile == 'local' else [True]):
                command = [exe, '--strictness-level', '10', '--validate-in-process', '--timeout-ms', '600000', '--validate', path]
                markers = ['SUCCESS']
                if gui:
                    markers += ['Completed tests in pluginval / Editor\n',
                                'Completed tests in pluginval / Open editor whilst processing\n',
                                'Completed tests in pluginval / Editor Automation\n']
                else:
                    command.append('--skip-gui-tests')
                r.command('pluginval-' + fmt.lower() + ('-gui' if gui else ''), command,
                          timeout=660, required_text=markers)


def auval(r, args):
    if 'AU' not in formats(r):
        r.check('auval', 'SKIP', 'AU is not a configured product format')
        return
    with loaded_format(r, 'AU') as path:
        if path:
            r.command('auval', ['auval', '-strict', '-v', r.cfg.get('wTEST_AU_TYPE', 'aufx'),
                      r.cfg['wPLUGIN_CODE'], r.cfg['wPLUGIN_MANUFACTURER_CODE']], timeout=180,
                      required_text=('AU VALIDATION SUCCEEDED',))


def renders(r, args):
    exe = r.built('ReviewRender')
    if not exe:
        return
    r.select(exe)
    folder = r.work / 'renders'
    _, text = r.command('renders', [sys.executable, ROOT / 'plugin/tests/regression/render_matrix.py', exe,
                        ROOT / 'plugin/sample_audio', folder, *(['--quick'] if args.profile == 'ci' else [])], timeout=900)
    metric(r, 'renders', len(re.findall(r'^(?:ok   |FAIL )', text, re.M)))
    # All three jobs export this stable path for the existing cross-platform comparisons.
    if args.profile == 'ci':
        import shutil
        shutil.copytree(folder, r.results / 'renders', dirs_exist_ok=True)
    reference = os.environ.get('WLA_TEST_REFERENCE')
    if reference:
        r.command('compare-last-release', [sys.executable, ROOT / 'plugin/tests/regression/compare_renders.py',
                  reference, folder, '--tolerance', '1e-6'], timeout=120)
    elif args.profile != 'ci':
        r.check('compare-last-release', 'PLANNED', 'Set WLA_TEST_REFERENCE to the approved release renders; renders alone do not prove a release match')


def screenshots(r, args):
    app = r.built('Standalone')
    if not app:
        return
    r.select(app)
    if running(r.product):
        r.check('screenshots', 'SKIP', 'standalone is already running; leaving it alone')
        return
    env = dict(os.environ, WLA_TEST_APP=str(app), WLA_TEST_OUT=str(r.work), WLA_TEST_PRODUCT=r.product,
               WLA_TEST_SETTINGS=r.cfg['wTEST_SETTINGS_MAC'], WLA_TEST_LAYOUT=r.cfg.get('wTEST_SMOKE_LAYOUT', '750x478'))
    r.command('screenshots', ['bash', ROOT / 'tests/mac/screenshots.sh'], timeout=240, env=env,
              required_text=('PASS screens:',))


HANDLERS = dict(unit=unit, uirobot=uirobot, lfo=lfo, hosttest=hosttest, pluginval=pluginval,
                auval=auval, renders=renders, screenshots=screenshots)
