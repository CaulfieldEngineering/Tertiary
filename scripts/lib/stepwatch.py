#!/usr/bin/env python3
"""[Claude 2026-11-03] Step watch: the suite reports its own progress and stops its own stalls.

Joe, 2026-11-03: "is this watchdog AI-operated or AI-built? i'd prefer if its built into the testing suite/scripts".
A release agent had sat waiting on a GitHub run that had finished long before. So every step of the test and build
scripts now does three things itself, with no agent involved:
  - reports the step it is on and its time limit (a job file the HQ Step Watchdog reads, below)
  - a step that runs past its limit is stopped, with everything it started, and fails as a timeout
  - a wait on a GitHub run polls the run itself, returns when it completes and gives up at its limit

  python scripts/lib/stepwatch.py run --step "Windows: build" --minutes 90 [--log FILE] -- cmake --build ...
      runs one command under a limit: its exit code, or 124 after a timeout (a TIMEOUT line says what was stopped)
  python scripts/lib/stepwatch.py gh-wait --minutes 120 (--run ID | --sha SHA [--branch B] [--workflow "Tests*"])
      polls a GitHub Actions run; prints "RUN <id> <status> <conclusion> <url>"; 0 success, 1 another conclusion,
      124 still running at the limit, 4 no such run appeared
  python scripts/lib/stepwatch.py beat --step "..." --minutes N [--gh-run ID] | done [--note ...] | limit <ID>

Limits: `limit_minutes` on each row of tests/sequence.json and scripts/sequence.json, from the durations of recent
runs with generous headroom. The kill is gentle first: SIGTERM to the step's process group on the Mac, Ctrl+Break to
its process group on Windows (run.py turns both into "interrupted" and restores what it staged), then after a grace
period everything left (the process group / a Windows job object) is killed.

Job files: one JSON per job, the same fields as HQ\\scripts\\watchdog\\watchdog.ps1 writes, so its 5-minute check
alerts on a stalled suite step exactly as on an agent's. The job is $WLA_WATCHDOG_JOB; whoever starts a run without
one names it, and its children join it. Where the files go:
  - $WLA_WATCHDOG_DIR, else on Windows HQ\\scripts\\watchdog\\jobs beside this repo (nothing when HQ is not there),
    else on the Mac ~/.wla-test/watchdog (the HQ check reads it over SSH)
  - nothing on GitHub (GITHUB_ACTIONS) or with WLA_WATCHDOG=0; limits and timeouts still apply
The watchdog's expectation is the step's limit plus 10 minutes: it only speaks when the step's own kill did not work.
Python 3.9 standard library only (the Mac has no psutil).
"""
import argparse
import contextlib
import datetime as dt
import fnmatch
import json
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import subprocess
import sys
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
PLATFORM = {'Windows': 'win', 'Darwin': 'mac'}.get(platform.system(), 'linux')
TIMEOUT_CODE = 124
SLACK_MINUTES = 10     # the HQ checker's expectation = limit + this
JOB_ENV = 'WLA_WATCHDOG_JOB'


# ------------------------------------------------------------------ job files (the HQ Step Watchdog's format)
def jobs_dir():
    if os.environ.get('WLA_WATCHDOG') == '0' or os.environ.get('GITHUB_ACTIONS') == 'true':
        return None
    value = os.environ.get('WLA_WATCHDOG_DIR')
    if value:
        path = Path(value).expanduser()
    elif PLATFORM == 'win':
        path = next((p / 'jobs' for p in (ROOT.parents[1] / 'HQ/scripts/watchdog', Path.home() / 'Documents/GitHub/HQ/scripts/watchdog')
                     if (p / 'watchdog.ps1').exists()), None)
        if path is None:
            return None
    else:
        path = Path.home() / '.wla-test' / 'watchdog'
    try:
        path.mkdir(parents=True, exist_ok=True)
    except OSError:
        return None
    return path


def job_file(folder, name):
    return folder / (re.sub(r'[^A-Za-z0-9._-]', '-', name).strip('-') + '.json')


def iso(t=None):
    return (t or dt.datetime.now(dt.timezone.utc)).strftime('%Y-%m-%dT%H:%M:%SZ')


@contextlib.contextmanager
def locked():
    """The same named mutex as watchdog.ps1 ("HQStepWatchdog"), so a beat never crosses a check in flight."""
    if PLATFORM != 'win':
        yield
        return
    import ctypes
    k32 = ctypes.WinDLL('kernel32', use_last_error=True)
    k32.CreateMutexW.restype = ctypes.c_void_p
    handle = k32.CreateMutexW(None, False, 'HQStepWatchdog')
    got = handle and k32.WaitForSingleObject(ctypes.c_void_p(handle), 90000) in (0, 0x80)
    try:
        yield
    finally:
        if got:
            k32.ReleaseMutex(ctypes.c_void_p(handle))
        if handle:
            k32.CloseHandle(ctypes.c_void_p(handle))


def _save(file, job):
    # One line: the HQ check reads the Mac's files with one `cat` over SSH.
    tmp = file.with_name(file.name + '.tmp')
    tmp.write_text(json.dumps(job, ensure_ascii=False) + '\n', encoding='utf-8')
    os.replace(tmp, file)


def job_name():
    return os.environ.get(JOB_ENV) or ''


def beat(step, minutes, gh_run=0, repo='', name=None):
    """Report the step this job is on and its limit. Never raises: reporting must not fail a test."""
    try:
        name = name or job_name()
        folder = jobs_dir()
        if not name or folder is None:
            return
        file = job_file(folder, name)
        expect = round(float(minutes) + SLACK_MINUTES, 1)
        with locked():
            old = json.loads(file.read_text(encoding='utf-8')) if file.exists() else None
            t = iso()
            if not old or old.get('status') == 'done':
                job = dict(name=name, status='open', started=t, host='mac' if PLATFORM == 'mac' else '', stepNo=0, step='',
                           expectMinutes=0, lastBeat=t, ghRun=0, repo='', alert=None, doneAt=None, note='')
            else:
                job = old
            job.update(stepNo=int(job.get('stepNo') or 0) + 1, step=step, expectMinutes=expect, lastBeat=t,
                       ghRun=int(gh_run or 0), repo=repo if gh_run else '', alert=None)
            _save(file, job)
    except Exception as error:
        print(f'[stepwatch] could not report "{step}": {error}', file=sys.stderr, flush=True)


def done(name=None, note=''):
    try:
        name = name or job_name()
        folder = jobs_dir()
        if not name or folder is None or not job_file(folder, name).exists():
            return
        file = job_file(folder, name)
        with locked():
            job = json.loads(file.read_text(encoding='utf-8'))
            job.update(status='done', doneAt=iso())
            if note:
                job['note'] = note
            _save(file, job)
    except Exception as error:
        print(f'[stepwatch] could not close job: {error}', file=sys.stderr, flush=True)


@contextlib.contextmanager
def job(label):
    """Join the current job, or start one named after this run and close it at the end (only the starter closes it)."""
    if job_name():
        yield job_name()
        return
    name = f'tertiary-{PLATFORM}-{label}'
    os.environ[JOB_ENV] = name
    try:
        yield name
    finally:
        done(name)
        os.environ.pop(JOB_ENV, None)


# ------------------------------------------------------------------ limits
def limit(step_id, default=30):
    """limit_minutes of a row in tests/sequence.json or scripts/sequence.json."""
    for file in (ROOT / 'tests/sequence.json', ROOT / 'scripts/sequence.json'):
        try:
            for row in json.loads(file.read_text(encoding='utf-8')):
                if row.get('id') == step_id and row.get('limit_minutes'):
                    return float(row['limit_minutes'])
        except (OSError, ValueError):
            pass
    return float(default)


# ------------------------------------------------------------------ one command under a limit
class Result:
    def __init__(self, returncode, timed_out, seconds):
        self.returncode, self.timed_out, self.seconds = returncode, timed_out, seconds


class _WinJob:
    """A Windows job object around the step: the timeout kill reaches every process it started, orphans included."""
    def __init__(self, process):
        import ctypes
        self.ctypes = ctypes
        self.k32 = ctypes.WinDLL('kernel32', use_last_error=True)
        self.k32.CreateJobObjectW.restype = ctypes.c_void_p
        self.handle = self.k32.CreateJobObjectW(None, None)
        if not self.handle:
            return
        # Breakaway allowed: a program that asks to leave the job (some launchers do) still starts, as without one.
        # A normal end leaves whatever is still running alone, as before; only a timeout kills the job.
        class Basic(ctypes.Structure):
            _fields_ = [('user', ctypes.c_int64), ('job_user', ctypes.c_int64), ('flags', ctypes.c_uint32),
                        ('min_ws', ctypes.c_size_t), ('max_ws', ctypes.c_size_t), ('procs', ctypes.c_uint32),
                        ('affinity', ctypes.c_size_t), ('priority', ctypes.c_uint32), ('scheduling', ctypes.c_uint32)]
        class Extended(ctypes.Structure):
            _fields_ = [('basic', Basic), ('io', ctypes.c_uint64 * 6), ('proc_mem', ctypes.c_size_t),
                        ('job_mem', ctypes.c_size_t), ('peak_proc', ctypes.c_size_t), ('peak_job', ctypes.c_size_t)]
        limits = Extended()
        limits.basic.flags = 0x00000800   # JOB_OBJECT_LIMIT_BREAKAWAY_OK
        self.k32.SetInformationJobObject(ctypes.c_void_p(self.handle), 9, ctypes.byref(limits), ctypes.sizeof(limits))
        if not self.k32.AssignProcessToJobObject(ctypes.c_void_p(self.handle), ctypes.c_void_p(int(process._handle))):
            self.close()

    def active(self):
        if not self.handle:
            return 0
        c = self.ctypes
        class Info(c.Structure):   # JOBOBJECT_BASIC_ACCOUNTING_INFORMATION
            _fields_ = [('times', c.c_int64 * 4), ('faults', c.c_uint32), ('total', c.c_uint32),
                        ('active', c.c_uint32), ('terminated', c.c_uint32)]
        info = Info()
        if not self.k32.QueryInformationJobObject(c.c_void_p(self.handle), 1, c.byref(info), c.sizeof(info), None):
            return 0
        return info.active

    def kill(self):
        if self.handle:
            self.k32.TerminateJobObject(self.ctypes.c_void_p(self.handle), 1)

    def close(self):
        if self.handle:
            self.k32.CloseHandle(self.ctypes.c_void_p(self.handle))
        self.handle = None


def _group_alive(pgid):
    try:
        os.killpg(pgid, 0)
        return True
    except (ProcessLookupError, PermissionError):
        return False


def _stop(process, winjob, grace):
    """Gentle first (the test's own cleanup restores what it staged), then everything that is left."""
    deadline = time.monotonic() + grace
    if PLATFORM == 'win':
        try:
            os.kill(process.pid, signal.CTRL_BREAK_EVENT)
        except Exception:
            pass
        while time.monotonic() < deadline and (process.poll() is None or (winjob and winjob.active())):
            time.sleep(0.5)
        if winjob and winjob.handle:
            winjob.kill()
        if process.poll() is None:
            subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], capture_output=True, timeout=30)
    else:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except (ProcessLookupError, PermissionError):
            pass
        while time.monotonic() < deadline and (process.poll() is None or _group_alive(process.pid)):
            time.sleep(0.5)
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            pass
    try:
        process.wait(timeout=30)
    except subprocess.TimeoutExpired:
        pass


def run_limited(command, minutes, step=None, cwd=None, log=None, env=None, grace=90):
    """Run command with a limit of `minutes`. Reports `step` first when given. Returns Result; returncode 124 on timeout."""
    command = [str(c) for c in command]
    if PLATFORM == 'win' and command and not Path(command[0]).suffix:
        command[0] = shutil.which(command[0]) or command[0]    # npm, npx: .cmd files
    if step:
        beat(step, minutes)
    seconds = float(minutes) * 60
    output = open(log, 'ab') if log else None
    flags = dict(creationflags=0x00000200) if PLATFORM == 'win' else dict(start_new_session=True)   # new process group
    started = time.monotonic()
    process = subprocess.Popen(command, cwd=cwd, env=env, stdout=output, stderr=subprocess.STDOUT if output else None, **flags)
    winjob = _WinJob(process) if PLATFORM == 'win' else None
    try:
        try:
            code = process.wait(timeout=seconds)
            timed_out = False
        except subprocess.TimeoutExpired:
            _stop(process, winjob, grace)
            code, timed_out = TIMEOUT_CODE, True
        except KeyboardInterrupt:
            _stop(process, winjob, min(grace, 30))
            raise
    finally:
        if winjob:
            winjob.close()
    took = time.monotonic() - started
    if timed_out:
        text = f'TIMEOUT: "{step or " ".join(command)}" ran past its limit of {minutes:g} min and was stopped, with everything it started'
        print(text, flush=True)
        if output:
            output.write(('\n' + text + '\n').encode('utf-8'))
    if output:
        output.close()
    return Result(code, timed_out, took)


# ------------------------------------------------------------------ waiting on a GitHub run
def github_token():
    """The same way as tests/dashboard/server.py: the git credential for github.com, kept in this process."""
    try:
        out = subprocess.run(['git', 'credential', 'fill'], input='protocol=https\nhost=github.com\n\n',
                             capture_output=True, text=True, timeout=10,
                             env=dict(os.environ, GIT_TERMINAL_PROMPT='0')).stdout
        return next((line[9:] for line in out.splitlines() if line.startswith('password=')), None)
    except Exception:
        return None


def github_repo():
    url = subprocess.run(['git', '-C', str(ROOT), 'remote', 'get-url', 'origin'], capture_output=True, text=True).stdout.strip()
    m = re.search(r'github\.com[:/]([^/]+/[^/]+?)(?:\.git)?$', url)
    return m.group(1) if m else 'CaulfieldEngineering/Tertiary'


class GitHub:
    def __init__(self, repo):
        self.repo, self.token = repo, github_token()
        if not self.token:
            print('No GitHub token from git credential; using the public API without one', flush=True)

    def get(self, path):
        request = urllib.request.Request(f'https://api.github.com/repos/{self.repo}/{path}', headers={'Accept': 'application/vnd.github+json'})
        if self.token:
            request.add_header('Authorization', 'Bearer ' + self.token)
        try:
            with urllib.request.urlopen(request, timeout=30) as r:
                return json.loads(r.read().decode('utf-8'))
        except urllib.error.HTTPError as error:
            if error.code == 401 and self.token:
                print('GitHub refused the token (401); using the public API without one', flush=True)
                self.token = None
                return self.get(path)
            raise


def gh_wait(minutes, run_id=None, sha=None, branch=None, workflow=None, event=None, repo=None, poll=30, step=None):
    """Poll a run until it completes. Returns (run dict or None, timed_out). Checks once before the first sleep."""
    repo = repo or github_repo()
    api = GitHub(repo)
    what = f'run {run_id}' if run_id else f'the {workflow or "first"} run for {sha[:12]}' + (f' on {branch}' if branch else '')
    step = step or f'Waiting on GitHub {what}'
    beat(step, minutes, gh_run=run_id or 0, repo=repo)
    deadline = time.monotonic() + float(minutes) * 60
    found, reported = None, run_id
    while True:
        try:
            if run_id:
                found = api.get(f'actions/runs/{run_id}')
            else:
                query = f'actions/runs?head_sha={sha}' + (f'&branch={branch}' if branch else '') + (f'&event={event}' if event else '')
                runs = api.get(query).get('workflow_runs', [])
                found = next((r for r in runs if not workflow or fnmatch.fnmatch(r.get('name', ''), workflow)), None)
            if found and found.get('id') != reported:
                reported = found['id']
                beat(f'{step} (run {reported})', max(1, (deadline - time.monotonic()) / 60), gh_run=reported, repo=repo)
            if found and found.get('status') == 'completed':
                return found, False
        except Exception as error:
            print(f'GitHub API did not answer ({error}); retrying', flush=True)    # a dropped request is retried, not a FAIL
        if time.monotonic() + poll > deadline:
            return found, True
        time.sleep(poll)


# ------------------------------------------------------------------ command line
def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('action', choices=['run', 'gh-wait', 'beat', 'done', 'limit'])
    p.add_argument('id', nargs='?', help='limit: the step or test ID')
    p.add_argument('--step', default='')
    p.add_argument('--minutes', type=float, default=0)
    p.add_argument('--log')
    p.add_argument('--name', help='the job (default $WLA_WATCHDOG_JOB)')
    p.add_argument('--note', default='')
    p.add_argument('--gh-run', type=int, default=0)
    p.add_argument('--run', type=int, default=0, help='gh-wait: the run ID')
    p.add_argument('--sha')
    p.add_argument('--branch')
    p.add_argument('--workflow', help='gh-wait with --sha: the run name, wildcards allowed ("Tests*")')
    p.add_argument('--event')
    p.add_argument('--repo')
    p.add_argument('--poll', type=float, default=30)
    argv = sys.argv[1:]
    command = argv[argv.index('--') + 1:] if '--' in argv else []
    a = p.parse_args(argv[:argv.index('--')] if '--' in argv else argv)
    if a.name:
        os.environ[JOB_ENV] = a.name
    if a.action == 'limit':
        print(f'{limit(a.id):g}')
        return 0
    if a.action == 'done':
        done(note=a.note)
        return 0
    if a.minutes <= 0:
        p.error(f'{a.action} needs --minutes (a realistic limit for this step)')
    if a.action == 'beat':
        beat(a.step, a.minutes, gh_run=a.gh_run, repo=a.repo or (github_repo() if a.gh_run else ''))
        return 0
    if a.action == 'run':
        if not command:
            p.error('run needs a command after --')
        return run_limited(command, a.minutes, step=a.step or None, log=a.log).returncode
    if not a.run and not a.sha:
        p.error('gh-wait needs --run or --sha')
    run, timed_out = gh_wait(a.minutes, run_id=a.run or None, sha=a.sha, branch=a.branch, workflow=a.workflow, event=a.event,
                             repo=a.repo, poll=a.poll, step=a.step or None)
    if run:
        print(f"RUN {run['id']} {run.get('status')} {run.get('conclusion') or '-'} {run.get('html_url', '')}", flush=True)
    if timed_out:
        print(f'TIMEOUT: GitHub {"run " + str(run["id"]) if run else "run"} not finished after {a.minutes:g} min; stopped waiting', flush=True)
        return TIMEOUT_CODE if run else 4
    return 0 if run.get('conclusion') == 'success' else 1


if __name__ == '__main__':
    sys.exit(main())
