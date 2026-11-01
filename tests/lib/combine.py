#!/usr/bin/env python3
"""[Claude 2026-10-31] One release report from one test run: each machine's Run-All plus one GitHub run.

Joe, 2026-10-31: "when we go to release in a normal process, I'm expecting one test run/GitHub run which
yields a passing report before moving on." This step compiles the live progress files (progress.jsonl) and summaries that Run-All
wrote on each machine (Windows including web, Mac including Logic and the microphone) and the GitHub Actions run for
the same commit into ONE report. It runs no tests and decides nothing a script did not already decide.

The report passes only when every summary passed with no failed check, every summary and the GitHub run are
on one commit with a clean checkout, and that GitHub run concluded success. Anything else is FAIL, with why.

  python tests/lib/combine.py --run win=tests/results/2026-10-31_1400 --run mac=path/to/mac-results \
      --github-run 41210000000 --out tests/results/2026-10-31_rc --report tests/reports/rc-1.1.0.md
"""
import argparse
import datetime as dt
import json
import os
from pathlib import Path
import sys
import urllib.request

from common import ROOT, now

def repo_slug():
    """owner/name from this checkout's origin, so the same step works in every plugin repo."""
    import re
    import subprocess
    url = subprocess.run(['git', '-C', str(ROOT), 'remote', 'get-url', 'origin'], capture_output=True, text=True).stdout.strip()
    match = re.search(r'github\.com[:/]([^/]+/[^/]+?)(?:\.git)?$', url)
    if not match:
        raise SystemExit('origin is not a GitHub repository: ' + url)
    return match.group(1)


REPO = repo_slug()


def when(text):
    """An ISO time from progress.jsonl or the GitHub API ('...Z'), as an aware datetime; None if absent."""
    if not text:
        return None
    try:
        return dt.datetime.fromisoformat(str(text).replace('Z', '+00:00')).astimezone()
    except ValueError:
        return None


def duration(seconds):
    """[Claude 2026-10-31] Run time as Joe reads it: 3725 -> '1 h 2 min 5 s'. Blank when unknown."""
    if seconds is None or seconds == '':
        return ''
    seconds = int(round(float(seconds)))
    h, rest = divmod(seconds, 3600)
    m, s = divmod(rest, 60)
    return ' '.join(x for x in (f'{h} h' if h else '', f'{m} min' if (h or m) else '', f'{s} s') if x)


def span(start, end):
    a, b = when(start), when(end)
    return (b - a).total_seconds() if a and b else None


def stamp(text):
    t = when(text)
    return t.strftime('%Y-%m-%d %H:%M:%S') if t else ''


def github(run_id):
    """The run and its jobs from the public API (GH_TOKEN is used when set, for rate limits only)."""
    def get(url):
        request = urllib.request.Request(url, headers={'Accept': 'application/vnd.github+json'})
        if os.environ.get('GH_TOKEN'):
            request.add_header('Authorization', 'Bearer ' + os.environ['GH_TOKEN'])
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)
    base = f'https://api.github.com/repos/{REPO}/actions/runs/{run_id}'
    run = get(base)
    jobs = get(base + '/jobs?per_page=100').get('jobs', [])
    return run, jobs


def read_progress(path, name, problems):
    """progress.jsonl, format v1 (tests/README.md). A missing file or a bad line is a problem, not a skip."""
    if not path.is_file():
        problems.append(f'{name}: no progress.jsonl in {path.parent}')
        return []
    events = []
    for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        try:
            event = json.loads(line)
            if event.get('v') != 1 or 'event' not in event:
                raise ValueError('not a v1 event')
            events.append(event)
        except ValueError as error:
            problems.append(f'{name}: progress.jsonl line {number}: {error}')
    return events


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--run', action='append', required=True, metavar='MACHINE=DIR',
                   help='A results folder written by Run-All on one machine (repeat per machine)')
    p.add_argument('--github-run', required=True, help='GitHub Actions run id for the same commit')
    p.add_argument('--out', required=True, help='Folder for report.md / report.json (tests/results/...)')
    p.add_argument('--report', help='Also copy report.md here, e.g. tests/reports/rc-1.1.0.md (committed)')
    p.add_argument('--title', default='Release candidate test report')
    args = p.parse_args()

    problems, machines = [], []
    for entry in args.run:
        name, _, folder = entry.partition('=')
        path = Path(folder).resolve()
        events = read_progress(path / 'progress.jsonl', name, problems)
        summary_path = path / 'summary.json'
        data = json.loads(summary_path.read_text(encoding='utf-8')) if summary_path.is_file() else {}
        if not data:
            problems.append(f'{name}: no summary.json in {path}')
        begin = next((e for e in events if e['event'] == 'run-start'), {})
        finish = next((e for e in reversed(events) if e['event'] == 'run-end'), {})
        tests = {}
        for e in events:
            if e['event'] == 'test-start':
                tests.setdefault(e['id'], dict(id=e['id'], title=e.get('title', e['id']), status='RUNNING'))
            elif e['event'] == 'test-finish':
                tests.setdefault(e['id'], dict(id=e['id'], title=e['id'])).update(
                    status=e['status'], counts=e.get('counts', {}), seconds=e.get('seconds'))
        # The machine's facts come from the live progress file Joe watched; summary.json must agree with it.
        data.update(commit=begin.get('commit'), dirty=begin.get('dirty'), machine=begin.get('machine'),
                    build=begin.get('build'), started=begin.get('time'), finished=finish.get('time'))
        counts = finish.get('counts', {})
        machines.append(dict(name=name, folder=str(path), summary=data, tests=list(tests.values())))
        if begin.get('run') != 'all':
            problems.append(f'{name}: progress.jsonl has no full Run-All start (run={begin.get("run")})')
        if not finish:
            problems.append(f'{name}: the run has not ended (no run-end event)')
        if begin.get('profile') != 'local':
            problems.append(f'{name}: profile {begin.get("profile")}, not a full local Run-All')
        if finish and (finish.get('status') != 'PASS' or counts.get('FAIL')):
            problems.append(f'{name}: {counts.get("FAIL", 0)} failed checks')
        if data.get('counts') and counts and data['counts'] != counts:
            problems.append(f'{name}: summary.json counts {data["counts"]} differ from the live run-end {counts}')
        unfinished = [t['id'] for t in tests.values() if t['status'] == 'RUNNING']
        if unfinished:
            problems.append(f'{name}: tests started but never finished: {", ".join(unfinished)}')
        if not begin.get('commit'):
            problems.append(f'{name}: run-start records no commit')
        elif begin.get('dirty'):
            problems.append(f'{name}: checkout had uncommitted changes: {", ".join(begin["dirty"])[:300]}')

    commits = {m['summary'].get('commit') for m in machines}
    commit = commits.pop() if len(commits) == 1 else None
    if commit is None:
        problems.append('the machines ran different commits: ' + ', '.join(f'{m["name"]}={m["summary"].get("commit")}' for m in machines))

    try:
        run, jobs = github(args.github_run)
        if run.get('head_sha') != commit:
            problems.append(f'GitHub run {args.github_run} is on {run.get("head_sha")}, not {commit}')
        if run.get('status') != 'completed' or run.get('conclusion') != 'success':
            problems.append(f'GitHub run {args.github_run}: {run.get("status")} / {run.get("conclusion")}')
    except Exception as error:
        run, jobs = {}, []
        problems.append(f'GitHub run {args.github_run} could not be read: {error}')

    status = 'FAIL' if problems else 'PASS'
    url = run.get('html_url') or f'https://github.com/{REPO}/actions/runs/{args.github_run}'
    # [Claude 2026-10-31] Run times (Joe: "test suite started at xxx and ended at yyy so I can feel for how long
    # they all take"). The whole run is the earliest machine start to the last of the machines and the GitHub run.
    gh_start, gh_end = run.get('run_started_at') or run.get('created_at'), run.get('updated_at')
    starts = [t for t in [when(m['summary'].get('started')) for m in machines] + [when(gh_start)] if t]
    ends = [t for t in [when(m['summary'].get('finished')) for m in machines] + [when(gh_end)] if t]
    whole = (min(starts), max(ends)) if starts and ends else None
    run_line = (f'Run time: started {whole[0].strftime("%Y-%m-%d %H:%M:%S")}, ended {whole[1].strftime("%Y-%m-%d %H:%M:%S")}, '
                f'total {duration((whole[1] - whole[0]).total_seconds())}.') if whole else 'Run time: unknown (no start or end recorded).'
    lines = [f'# {args.title}', '',
             f'**{status}.** Commit `{commit or "mixed"}`, compiled {now()}.', '', run_line, '',
             'One test run: a full local Run-All on each machine and one GitHub run, all on this commit. '
             'PASS means no check failed anywhere and nothing was waived. PLANNED and SKIP rows are listed '
             'coverage that does not exist yet or does not apply on that machine; they are not passes.', '']
    if problems:
        lines += ['## Why this is not a pass', ''] + [f'- {x}' for x in problems] + ['']
    lines += ['## Runs', '', '| Machine | Status | Pass | Fail | Skip | Planned | Started | Finished | Duration | Results |',
              '|---|---|---|---|---|---|---|---|---|---|']
    for m in machines:
        s, c = m['summary'], m['summary'].get('counts', {})
        lines.append(f"| {m['name']} ({s.get('machine')}, {s.get('platform')}) | {s.get('status')} | {c.get('PASS', 0)} | {c.get('FAIL', 0)} | "
                     f"{c.get('SKIP', 0)} | {c.get('PLANNED', 0)} | {stamp(s.get('started'))} | {stamp(s.get('finished'))} | "
                     f"{duration(span(s.get('started'), s.get('finished')))} | `{m['folder']}` |")
    lines.append(f"| GitHub run [{args.github_run}]({url}) | {run.get('conclusion') or run.get('status') or 'unknown'} | "
                 f"{sum(j.get('conclusion') == 'success' for j in jobs)} jobs | "
                 f"{sum(j.get('conclusion') not in ('success', 'skipped') for j in jobs)} jobs | "
                 f"{sum(j.get('conclusion') == 'skipped' for j in jobs)} jobs | | {stamp(gh_start)} | {stamp(gh_end)} | "
                 f"{duration(span(gh_start, gh_end))} | |")
    lines += ['', '## Tests', '', '| Machine | ID | Test | Status | Checks (pass / fail / skip / planned) | Duration |',
              '|---|---|---|---|---|---|']
    for m in machines:
        for t in m['tests']:
            k = [t.get('counts', {}).get(x, 0) for x in ('PASS', 'FAIL', 'SKIP', 'PLANNED')]
            lines.append(f"| {m['name']} | {t['id']} | {t['title']} | {t['status']} | {' / '.join(map(str, k))} | {duration(t.get('seconds'))} |")
    lines += ['', 'Builds: ' + '; '.join(f"{m['name']} `{m['summary'].get('build')}`" for m in machines)]
    if jobs:
        lines += ['', '## GitHub jobs', '', '| Job | Conclusion | Started | Finished | Duration |', '|---|---|---|---|---|']
        lines += [f"| {j['name']} | {j.get('conclusion')} | {stamp(j.get('started_at'))} | {stamp(j.get('completed_at'))} | "
                  f"{duration(span(j.get('started_at'), j.get('completed_at')))} |" for j in jobs]
    open_rows = [(m['name'], r['id'], c) for m in machines for r in m['summary'].get('results', [])
                 for c in r['checks'] if c['status'] in ('SKIP', 'PLANNED')]
    if open_rows:
        lines += ['', '## Open coverage (SKIP and PLANNED, not passes)', '', '| Machine | ID | Check | Status | Why |', '|---|---|---|---|---|']
        lines += [f"| {n} | {i} | {c['id']} | {c['status']} | {str(c['detail']).replace('|', '/')[:240]} |" for n, i, c in open_rows]
    text = '\n'.join(lines) + '\n'

    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'report.md').write_text(text, encoding='utf-8')
    (out / 'report.json').write_text(json.dumps(dict(status=status, commit=commit, problems=problems,
        started=whole[0].isoformat(timespec='seconds') if whole else None,
        finished=whole[1].isoformat(timespec='seconds') if whole else None,
        seconds=round((whole[1] - whole[0]).total_seconds()) if whole else None,
        github=dict(id=args.github_run, url=url, conclusion=run.get('conclusion'), head_sha=run.get('head_sha'),
                    started=gh_start, finished=gh_end, seconds=span(gh_start, gh_end)),
        machines=[dict(name=m['name'], folder=m['folder'], status=m['summary'].get('status'),
                       counts=m['summary'].get('counts'), started=m['summary'].get('started'),
                       finished=m['summary'].get('finished'),
                       seconds=span(m['summary'].get('started'), m['summary'].get('finished'))) for m in machines]),
        indent=2) + '\n', encoding='utf-8')
    if args.report:
        target = (ROOT / args.report).resolve()
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding='utf-8')
        # [Claude 2026-10-31] The same view Joe watched live, frozen with its checks, logs and screenshots.
        dashboard = ROOT / 'tests/dashboard/server.py'
        if dashboard.exists():
            page = target.with_suffix('.html')
            command = [sys.executable, str(dashboard), '--export', str(page), '--github-run', str(args.github_run)]
            for entry in args.run:
                name, _, folder = entry.partition('=')
                command += ['--run', name + '=' + str(Path(folder).resolve())]
            import subprocess
            done = subprocess.run(command, capture_output=True, text=True)
            print(done.stdout.strip() or done.stderr.strip())
            if done.returncode:
                print('The saved dashboard page could not be written; the report above stands on its own.')
    print(text)
    print('Report: ' + str(out / 'report.md'))
    return 0 if status == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
