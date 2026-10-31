"""[Codex 2026-10-30] Private, short-lived Terminal worker; accepts only mac-ui arguments."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

queue, executable = Path(sys.argv[1]), sys.argv[2]
allowed = {'status', 'dump', 'press', 'set', 'activate', 'paste', 'cgkey', 'cgclick', 'input-running'}
deadline = time.monotonic() + 1200
try:
    tty = os.ttyname(0)
except OSError:
    tty = ''
(queue / 'ready').write_text(tty)
try:
    while not (queue / 'stop').exists() and time.monotonic() < deadline:
        for request in queue.glob('request-*.json'):
            data = json.loads(request.read_text())
            request.unlink()
            try:
                if data[0] not in allowed or not all(isinstance(v, str) for v in data):
                    raise ValueError('Unsupported UI operation')
                r = subprocess.run([executable, *data], capture_output=True, timeout=12)
                answer = dict(code=r.returncode, stdout=r.stdout.decode('utf-8', 'replace'), stderr=r.stderr.decode('utf-8', 'replace'))
            except Exception as e:
                answer = dict(code=1, stdout='', stderr=str(e))
            response = queue / request.name.replace('request-', 'response-')
            temp = response.with_suffix('.tmp')
            temp.write_text(json.dumps(answer))
            temp.replace(response)
        time.sleep(.05)
finally:
    if tty:
        script = 'tell application "Terminal"\nrepeat with w in windows\nrepeat with t in tabs of w\nif tty of t is "' + tty + '" then close w saving no\nend repeat\nend repeat\nend tell'
        subprocess.Popen(['osascript', '-e', script], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
