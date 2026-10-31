"""[Codex 2026-10-30] Shared result contract, local build discovery and bounded commands.

Only scripts make test decisions. A binary hash is always recorded; a source revision
is never substituted for the identity of a previously built binary.
"""
import contextlib
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
PLATFORM = 'win' if sys.platform == 'win32' else 'mac' if sys.platform == 'darwin' else sys.platform
CODES = {'PASS': 0, 'FAIL': 1, 'SKIP': 2, 'PLANNED': 3}


def now():
    return dt.datetime.now().astimezone().isoformat(timespec='seconds')


def config():
    values = dict(re.findall(r'^\s*set\(\s*(\w+)\s+([^\n]*?)\s*\)\s*$',
                             (ROOT / 'config.cmake').read_text(encoding='utf-8'), re.M))
    values = {k: v.strip().strip('"') for k, v in values.items()}
    values['wPRODUCT_SLUG'] = values.get('wPRODUCT_SLUG') or values['wPLUGIN_NAME'].lower()
    for _ in range(4):
        values = {k: re.sub(r'\$\{(\w+)\}', lambda m: values.get(m[1], m[0]), v)
                  for k, v in values.items()}
    return values


def local_path(value):
    path = (ROOT / value).resolve()
    if path != ROOT and ROOT not in path.parents:
        raise ValueError('Build and harness paths must be inside THIS repo: ' + str(path))
    return path


def identity(path):
    path = Path(path)
    binary = path
    if path.is_dir():
        candidates = sorted((path / 'Contents' / ('x86_64-win' if PLATFORM == 'win' else 'MacOS')).glob('*'))
        candidates = [p for p in candidates if p.is_file()]
        if len(candidates) != 1:
            raise ValueError('Expected one executable in bundle: ' + str(path))
        binary = candidates[0]
    data = binary.read_bytes()
    embedded = re.search(rb'[0-9a-f]{7,40}\+? \xc2\xb7 20[0-9-]+ [0-9:]+', data)
    digest = 'sha256:' + hashlib.sha256(data).hexdigest()
    return {'path': str(path), 'id': (embedded.group().decode() + '; ' if embedded else '') + digest}


def status(checks):
    present = {c['status'] for c in checks}
    return next((s for s in ('FAIL', 'PLANNED', 'SKIP', 'PASS') if s in present), 'SKIP')


def write_json(path, value):
    path = Path(path)
    temp = path.with_suffix('.json.tmp')
    temp.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    temp.replace(path)


class Report:
    def __init__(self, name, results, build_dir=None, row=None, profile='local'):
        self.name = name
        self.row = row or {'id': name, 'name': name, 'suite_ids': []}
        self.profile = profile
        self.cfg = config()
        self.product = self.cfg['wPLUGIN_NAME']
        self.results = Path(results).resolve()
        self.results.mkdir(parents=True, exist_ok=True)
        self.work = self.results / (name + '-artifacts')
        if self.work.is_symlink():
            raise ValueError('Artifact directory must not be a symlink')
        if self.work.exists() and any(self.work.iterdir()):
            archive = self.results / 'history' / (name + '-' + dt.datetime.now().strftime('%H%M%S-%f'))
            archive.parent.mkdir(parents=True, exist_ok=True)
            if self.results not in archive.resolve().parents or self.results not in self.work.resolve().parents:
                raise ValueError('Artifact archive escaped its result folder')
            self.work.rename(archive)
            previous = self.results / (name + '.json')
            if previous.exists():
                previous.rename(archive / 'previous-result.json')
        self.work.mkdir(exist_ok=True)
        self.build_dir = local_path(build_dir or os.environ.get('WLA_TEST_BUILD_DIR') or self.cfg.get('wTEST_BUILD_DIR', 'build-tests'))
        self.release = self.cfg.get('wTEST_CONFIGURATION', 'Release')
        self.binaries = {}
        self.data = dict(id=self.row['id'], name=name, title=self.row['name'],
                         suite_ids=self.row.get('suite_ids', []), profile=profile,
                         platform=PLATFORM, status='SKIP', started=now(), finished=None,
                         build={'path': str(self.build_dir), 'id': 'not built / not selected'},
                         host={'name': name, 'version': 'not detected'}, checks=[], artifacts=[])

    def check(self, id, state, detail):
        assert state in CODES
        self.data['checks'].append(dict(test_id=self.row['id'], id=id, status=state, detail=str(detail)))
        print(f'{state:7} {self.row["id"]} / {id}: {detail}', flush=True)

    def artifact(self, path):
        value = os.path.relpath(Path(path).resolve(), self.results).replace(os.sep, '/')
        if value not in self.data['artifacts']:
            self.data['artifacts'].append(value)

    def built(self, kind, required=True):
        ext = '.exe' if PLATFORM == 'win' else ''
        if kind in ('VST3', 'AU', 'Standalone'):
            suffix = {'VST3': '.vst3', 'AU': '.component', 'Standalone': ext if PLATFORM == 'win' else '.app'}[kind]
            path = self.build_dir / 'plugin' / (self.product + '_artefacts') / self.release / kind / (self.product + suffix)
        else:
            path = self.build_dir / 'plugin' / (kind + '_artefacts') / self.release / (kind + ext)
        # CI artifacts can be copied into a repo-local flat build folder.
        if not path.exists():
            flat = self.build_dir / path.name
            if flat.exists():
                path = flat
        local_path(path)
        if not path.exists():
            if required:
                self.check(kind, 'SKIP', 'repo build missing: ' + str(path))
            return None
        if str(path) not in self.binaries:
            info = identity(path)
            self.binaries[str(path)] = info
            write_json(self.work / 'builds.json', list(self.binaries.values()))
            print('Repo binary: ' + str(path) + '\nBuild id: ' + info['id'], flush=True)
        return path

    def select(self, path):
        self.data['build'] = identity(path)
        print('Testing binary: ' + str(path) + '\nBuild id: ' + self.data['build']['id'], flush=True)
        return path

    def screenshot(self, label):
        path = self.work / (label + '.png')
        try:
            if PLATFORM == 'win':
                from PIL import ImageGrab
                ImageGrab.grab(all_screens=True).save(path)
            elif PLATFORM == 'mac':
                subprocess.run(['screencapture', '-x', str(path)], timeout=15, check=True, capture_output=True)
            if path.exists():
                self.artifact(path)
        except Exception as error:
            (self.work / (label + '-screenshot.txt')).write_text(str(error), encoding='utf-8')
            self.artifact(self.work / (label + '-screenshot.txt'))

    def command(self, id, command, timeout=300, cwd=None, env=None, required_text=(), record=True, preserve_children=False):
        log = self.work / (id + '.log')
        self.artifact(log)
        print('Running ' + id, flush=True)
        with log.open('w', encoding='utf-8') as output:
            output.write('argv: ' + json.dumps([str(c) for c in command]) + '\n')
            output.flush()
            process = subprocess.Popen([str(c) for c in command], cwd=cwd or ROOT,
                                       env=env, stdout=output, stderr=subprocess.STDOUT,
                                       start_new_session=(PLATFORM != 'win'))
            try:
                code = process.wait(timeout=timeout)
            except (subprocess.TimeoutExpired, KeyboardInterrupt):
                self.screenshot(id + '-timeout')
                # Owned process group only. Cubase is separately guarded by its wrapper's quit/fallback.
                if PLATFORM == 'win':
                    subprocess.run(['taskkill', '/PID', str(process.pid), *([] if preserve_children else ['/T']), '/F'], capture_output=True, timeout=20)
                else:
                    os.killpg(process.pid, signal.SIGTERM)
                    try:
                        process.wait(timeout=15)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=20)
                output.write(f'\nFAIL watchdog: exceeded {timeout}s or interrupted\n')
                code = 124
        text = log.read_text(encoding='utf-8', errors='replace')
        good = code == 0 and all(marker in text for marker in required_text)
        if record:
            self.check(id, 'PASS' if good else 'FAIL', f'exit {code}; ' + str(log.relative_to(self.results)))
        if not good:
            self.screenshot(id + '-failure')
        return code, text

    def finish(self):
        self.data['status'] = status(self.data['checks'])
        self.data['finished'] = now()
        for path in self.work.rglob('*'):
            if path.is_file():
                # REAPER seeds thousands of themes/scripts and an evaluation record in a
                # throwaway profile. Those are not test evidence or report attachments.
                rel = path.relative_to(self.work)
                if rel.parts[0] == 'profile' and path.name not in ('reaper.ini', 'reaper-vstplugins64.ini',
                        'reaper-vstplugins_arm64.ini', 'reaper-auplugins_arm64.ini'):
                    continue
                self.artifact(path)
        write_json(self.results / (self.name + '.json'), self.data)
        print(f"{self.name}: {self.data['status']} -> {self.results / (self.name + '.json')}", flush=True)
        return CODES[self.data['status']]


@contextlib.contextmanager
def staged(report, source, destination, reset=None):
    """Back up and restore the exact previous file/bundle, including a previously absent slot.

    A durable journal and backup remain if restoration fails. Never discard a backup
    on an exception. The UUID backup is beside its destination, on the same volume.
    """
    import uuid
    destination = Path(destination).expanduser().absolute()
    source = Path(source).resolve()
    if source == destination:
        raise ValueError('Refusing to stage onto the source build')
    destination.parent.mkdir(parents=True, exist_ok=True)
    backup = destination.with_name(destination.name + '.test-layout-backup-' + uuid.uuid4().hex)
    slot_id = hashlib.sha256(str(destination).encode('utf-8')).hexdigest()[:10]
    journal = report.work / (destination.name + '-' + slot_id + '-restore.json')
    existed = destination.exists() or destination.is_symlink()
    write_json(journal, dict(destination=str(destination), backup=str(backup), existed=existed, restored=False))
    moved = copied = False
    try:
        if existed:
            destination.rename(backup)
            moved = True
        copied = True
        if source.is_dir():
            shutil.copytree(source, destination, symlinks=True)
        else:
            shutil.copy2(source, destination)
        if identity(source)['id'] != identity(destination)['id']:
            raise RuntimeError('staged binary hash mismatch')
        if reset:
            reset()
        yield destination
    finally:
        if copied and (destination.exists() or destination.is_symlink()):
            if destination.is_dir() and not destination.is_symlink():
                shutil.rmtree(destination)
            else:
                destination.unlink()
        if moved:
            backup.rename(destination)
        if reset:
            reset()
        write_json(journal, dict(destination=str(destination), backup=str(backup), existed=existed, restored=True))
        report.check('restore-' + destination.suffix.lstrip('.'), 'PASS', 'restored previous ' + str(destination))


def running(name):
    if PLATFORM == 'win':
        text = subprocess.check_output(['tasklist', '/FO', 'CSV', '/NH'], text=True, errors='replace')
        import csv
        return [int(row[1]) for row in csv.reader(text.splitlines()) if row and row[0].lower() == name.lower()]
    found = subprocess.run(['pgrep', '-x', name], capture_output=True, text=True)
    return [int(x) for x in found.stdout.split()]


def host_version(path):
    path = Path(path)
    if PLATFORM == 'mac':
        import plistlib
        parent = next((p for p in [path, *path.parents] if p.suffix == '.app'), None)
        if parent:
            return str(plistlib.loads((parent / 'Contents/Info.plist').read_bytes()).get('CFBundleShortVersionString', 'unknown'))
    elif PLATFORM == 'win':
        # Argument passed through the environment, not interpolated as PowerShell source.
        env = dict(os.environ, WLA_VERSION_FILE=str(path))
        return subprocess.check_output(['powershell', '-NoProfile', '-Command',
            '(Get-Item -LiteralPath $env:WLA_VERSION_FILE).VersionInfo.FileVersion'], env=env, text=True).strip() or 'unknown'
    return 'unknown'


def au_reset():
    # Invalidate this user's registrar only. auval -a repopulates it from the staged component.
    # Match the pre-layout CI's forced restart; TERM can leave the old registration cache in use.
    subprocess.run(['killall', '-9', '-u', str(os.getuid()), 'AudioComponentRegistrar'], capture_output=True, timeout=15)
    subprocess.run(['auval', '-a'], capture_output=True, timeout=60)


@contextlib.contextmanager
def host_lock():
    """OS lock shared across plugin repos; a crashed runner cannot leave a stale lock."""
    path = Path.home() / '.wla-test' / 'release-scripts.lock'
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('a+b') as handle:
        handle.seek(0)
        if not handle.read(1):
            handle.write(b'0')
            handle.flush()
        handle.seek(0)
        try:
            if PLATFORM == 'win':
                import msvcrt
                msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            yield False
            return
        try:
            yield True
        finally:
            handle.seek(0)
            if PLATFORM == 'win':
                msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(handle, fcntl.LOCK_UN)
