"""[Codex 2026-10-30] Bounded UI operations and the shared Mac test result contract."""
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import plistlib
import struct
import subprocess
import time
import shlex
import uuid


def run(args, timeout=30, check=True):
    p = subprocess.run([str(a) for a in args], capture_output=True, timeout=timeout)
    if check and p.returncode:
        raise RuntimeError('%s: %s' % (args[0], p.stderr.decode('utf-8', 'replace')))
    return p


def utc():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def plist(path):
    with open(path, 'rb') as f:
        return plistlib.load(f)


class Stop(RuntimeError):
    def __init__(self, detail, code=1):
        super().__init__(detail)
        self.code = code


class Result:
    def __init__(self, name, out):
        self.out = Path(out).expanduser().resolve()
        self.out.mkdir(parents=True, exist_ok=True)
        self.data = dict(name=name, platform='mac', status='FAIL', started=utc(),
                         finished=None, build={'path': '', 'id': ''},
                         host={'name': '', 'version': ''}, checks=[], artifacts=[])

    def check(self, name, good, detail):
        status = 'PASS' if good else 'FAIL'
        self.data['checks'].append(dict(id=name, status=status, detail=detail))
        print('%s %s: %s' % (status, name, detail), flush=True)
        if not good:
            raise Stop(detail)

    def artifact(self, path):
        path = str(Path(path).resolve())
        if path not in self.data['artifacts']:
            self.data['artifacts'].append(path)

    def finish(self, code):
        self.data['status'] = {0: 'PASS', 1: 'FAIL', 2: 'SKIP', 3: 'FAIL'}[code]
        self.data['finished'] = utc()
        p = self.out / 'result.json'
        p.write_text(json.dumps(self.data, indent=2) + '\n')
        print('%s: %s\nResult: %s' % (self.data['name'], self.data['status'], p), flush=True)


class UI:
    def __init__(self, executable, result, app='Logic Pro'):
        self.exe, self.result, self.app = executable, result, app
        self.queue = None
        if run([self.exe, 'status'], timeout=10, check=False).returncode:
            self.queue = result.out / 'ui-queue'
            self.queue.mkdir(mode=0o700)
            worker = Path(__file__).with_name('ui-worker.py')
            command = self.queue / 'ui.command'
            command.write_text('#!/bin/bash\nexec /usr/bin/python3 ' + ' '.join(shlex.quote(str(x)) for x in (worker, self.queue, self.exe)) + '\n')
            command.chmod(0o700)
            run(['open', '-a', 'Terminal', command])
            for _ in range(100):
                if (self.queue / 'ready').exists():
                    break
                time.sleep(.1)
            else:
                raise Stop('The authorized Terminal UI worker did not start', 3)
        self.call('status')

    def call(self, *args, check=True):
        if self.queue:
            request = self.queue / ('request-' + uuid.uuid4().hex + '.json')
            temp = request.with_suffix('.tmp')
            temp.write_text(json.dumps([str(a) for a in args]))
            temp.replace(request)
            response = request.with_name(request.name.replace('request-', 'response-'))
            for _ in range(160):
                if response.exists():
                    data = json.loads(response.read_text())
                    response.unlink()
                    if check and data['code']:
                        raise RuntimeError(data['stderr'])
                    return data['stdout']
                time.sleep(.1)
            raise Stop('Terminal UI operation timed out: ' + str(args[0]), 3)
        return run([self.exe, *args], timeout=12, check=check).stdout.decode('utf-8', 'replace')

    def close(self):
        if self.queue:
            (self.queue / 'stop').touch()

    def dump(self, app=None):
        return json.loads(self.call('dump', app or self.app, '13'))

    @staticmethod
    def find(rows, role=None, title=None, desc=None, value=None, prefix=None):
        return [e for e in rows if (not role or e.get('Role') == role)
                and (title is None or e.get('Title') == title)
                and (desc is None or e.get('Description') == desc)
                and (value is None or e.get('Value') == value)
                and (prefix is None or e['path'].startswith(prefix))]

    def one(self, **kwargs):
        found = self.find(self.dump(), **kwargs)
        if len(found) != 1:
            raise Stop('Expected one UI element %r, found %d' % (kwargs, len(found)), 3)
        return found[0]

    def wait(self, predicate, seconds=20, detail='UI state'):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            try:
                rows = self.dump()
            except RuntimeError:
                time.sleep(.4)
                continue
            found = predicate(rows)
            if found:
                return found
            time.sleep(.4)
        self.shot('timeout')
        raise Stop('Timed out waiting for ' + detail, 3)

    def key(self, code, *mods):
        self.call('cgkey', str(code), *mods)
        time.sleep(.25)

    def click(self, e):
        x, y, w, h = e['frame']
        if w <= 0 or h <= 0:
            raise Stop('Cannot click invisible UI: ' + str(e), 3)
        self.xy(x + w / 2, y + h / 2)

    def xy(self, x, y, *mods):
        self.call('cgclick', str(round(x)), str(round(y)), '1', *mods)
        time.sleep(.3)

    def press(self, e):
        # Logic sometimes reports AX timeout after actually opening its menu.
        # The caller always waits for and verifies the resulting UI state.
        self.call('press', self.app, e['path'], check=False)
        time.sleep(.3)

    def set(self, e, value):
        self.call('set', self.app, e['path'], str(value))
        time.sleep(.2)

    def button(self, title):
        self.click(self.one(role='AXButton', title=title))

    def popup(self, element, value):
        if element.get('Value') == value:
            return
        self.press(element)
        items = self.wait(lambda r: self.find(r, role='AXMenuItem', title=value), detail=value)
        self.press(items[0])
        self.wait(lambda r: any(e['path'] == element['path'] and e.get('Value') == value for e in r), detail=value)

    def checkbox(self, title, value):
        e = self.one(role='AXCheckBox', title=title)
        if e.get('Value') != str(int(value)):
            self.click(e)
        self.wait(lambda r: self.find(r, role='AXCheckBox', title=title, value=str(int(value))), detail=title)

    def shot(self, name):
        p = self.result.out / (name + '.png')
        run(['screencapture', '-x', p])
        self.result.artifact(p)
        try:
            a = self.result.out / (name + '.ax.json')
            a.write_text(json.dumps(self.dump(), indent=2))
            self.result.artifact(a)
        except Exception:
            pass
        return p

    def goto_folder(self, folder):
        self.key(5, 'cmd', 'shift')
        fields = self.wait(lambda r: [e for e in r if e.get('Role') == 'AXTextField'
                           and e.get('Value', '').startswith('/')], detail='Go to folder')
        self.set(fields[-1], folder)
        self.key(36)


def wave_data(path):
    """Read RIFF samples without depending on numpy or Python's PCM-only wave reader."""
    raw = Path(path).read_bytes()
    if raw[:4] != b'RIFF' or raw[8:12] != b'WAVE':
        raise Stop('Expected RIFF/WAVE: ' + str(path))
    chunks = {}
    pos = 12
    while pos + 8 <= len(raw):
        key, size = struct.unpack_from('<4sI', raw, pos)
        chunks[key] = raw[pos + 8:pos + 8 + size]
        pos += 8 + size + (size & 1)
    fmt = struct.unpack_from('<HHIIHH', chunks[b'fmt '])
    code, channels, rate, _, align, bits = fmt
    if code == 65534:
        code = struct.unpack_from('<H', chunks[b'fmt '], 24)[0]
    return dict(code=code, channels=channels, rate=rate, bits=bits,
                frames=len(chunks[b'data']) // align), chunks[b'data']


def compare(a, b):
    fa, da = wave_data(a)
    fb, db = wave_data(b)
    same = fa == fb and da == db
    peak = None
    if fa == fb and fa['code'] == 3 and fa['bits'] == 32:
        peak = max((abs(x[0] - y[0]) for x, y in zip(struct.iter_unpack('<f', da), struct.iter_unpack('<f', db))), default=0)
    return dict(equal=same, max_abs_difference=peak, tolerance=0.0,
                format=fa, reference_format=fb, pcm_sha256=hashlib.sha256(da).hexdigest())
