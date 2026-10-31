#!/usr/bin/env python3
"""[Codex 2026-10-30] Entry point used by every independently runnable wrapper."""
import argparse
import datetime as dt
import json
import os
from pathlib import Path
import subprocess
import signal
import sys
import traceback
from urllib.parse import quote

from common import CODES, PLATFORM, ROOT, Report, host_lock, now, write_json


def sequence():
    rows = json.loads((ROOT / 'tests/sequence.json').read_text(encoding='utf-8'))
    names = [r['name'] for r in rows]
    if len(set(names)) != len(names):
        raise ValueError('Duplicate names in sequence.json')
    for row in rows:
        for path in row['scripts'].values():
            if not (ROOT / 'tests' / path).is_file():
                raise ValueError('Missing sequence script: ' + path)
    return rows


def result_folder(value):
    if value:
        path = Path(value).resolve()
        path.mkdir(parents=True, exist_ok=True)
        return path
    base = ROOT / 'tests/results' / dt.datetime.now().strftime('%Y-%m-%d_%H%M')
    path = base
    suffix = 0
    while path.exists():
        suffix += 1
        path = base.with_name(base.name + f'-{suffix:02}')
    path.mkdir(parents=True)
    return path


def summary(results, reports):
    counts = {s: sum(c['status'] == s for r in reports for c in r['checks']) for s in CODES}
    total = sum(counts.values())
    coverage = f"{counts['PASS'] + counts['FAIL']} of {total} checks automated and run; {counts['PLANNED']} planned; {counts['SKIP']} skipped here"
    data = dict(platform=PLATFORM, finished=now(), status='FAIL' if counts['FAIL'] else 'PASS',
                coverage=coverage, counts=counts, results=reports)
    write_json(results / 'summary.json', data)
    def cell(value):
        return str(value).replace('|', '\\|').replace('\n', '<br>')
    def link(path, label=None):
        return f'[{cell(label or Path(path).name)}]({quote(path, safe="/:")})'
    lines = [f'# Release checks ({PLATFORM})', '', coverage, '',
             'PASS means the named check ran. PLANNED and SKIP are open coverage, not release approval.', '',
             '## Scripts (failures first; checklist order within each group)', '',
             '| # | Script | Status | Build id | Artifacts |', '|---|---|---|---|---|']
    ordered = sorted(enumerate(reports, 1), key=lambda item: item[1]['status'] != 'FAIL')
    for index, r in ordered:
        artifacts = ' '.join(link(p) for p in r['artifacts'] if Path(p).suffix in ('.log', '.txt', '.png', '.json', '.md'))
        lines.append(f"| {index} | {link(r['name'] + '.json', r['name'])} | {r['status']} | {cell(r['build']['id'])} | {artifacts} |")
    lines += ['', '## Checks (failures first)', '', '| Script | Check | Status | Detail |', '|---|---|---|---|']
    checks = [(r['name'], c) for r in reports for c in r['checks']]
    for name, c in sorted(checks, key=lambda item: item[1]['status'] != 'FAIL'):
        lines.append(f"| {name} | {cell(c['id'])} | {c['status']} | {cell(c['detail'])} |")
    (results / 'summary.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print('\n' + coverage + '\nReport: ' + str(results / 'summary.md'), flush=True)
    return int(counts['FAIL'] > 0)


def run_all(args, results, rows):
    reports = []
    for row in rows:
        if PLATFORM not in row['scripts']:
            continue
        script = ROOT / 'tests' / row['scripts'][PLATFORM]
        if PLATFORM == 'win':
            command = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script), '-ResultsDir', str(results)]
            if args.build_dir:
                command += ['-BuildDir', args.build_dir]
        else:
            command = ['bash', str(script), '--results-dir', str(results)]
            if args.build_dir:
                command += ['--build-dir', args.build_dir]
        if args.no_logic and PLATFORM == 'mac':
            command += ['--no-logic']
        target = results / (row['name'] + '.json')
        if target.exists():
            target.unlink()  # Never accept a stale PASS when a child fails before writing JSON.
        try:
            code = subprocess.run(command, cwd=ROOT, timeout=5400).returncode
            data = json.loads(target.read_text(encoding='utf-8'))
            if data['name'] != row['name'] or CODES[data['status']] != code or not data['finished']:
                raise ValueError(f'result/exit mismatch (exit {code})')
            for field in ('build', 'host', 'checks', 'artifacts', 'platform', 'started'):
                if field not in data:
                    raise ValueError('missing result field: ' + field)
        except Exception as error:
            report = Report(row['name'], results, args.build_dir)
            report.check('runner', 'FAIL', repr(error))
            report.finish()
            data = report.data
        reports.append(data)
        summary(results, reports)  # A reviewable partial report survives a later interruption.
    return summary(results, reports)


def main():
    def interrupted(signum, frame):
        raise KeyboardInterrupt('signal ' + str(signum))
    signal.signal(signal.SIGTERM, interrupted)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('name')
    parser.add_argument('--results-dir')
    parser.add_argument('--build-dir')
    parser.add_argument('--no-logic', action='store_true')
    args = parser.parse_args()
    rows = sequence()
    results = result_folder(args.results_dir)
    if args.name == 'summarize':
        reports = []
        for row in rows:
            if PLATFORM in row['scripts']:
                reports.append(json.loads((results / (row['name'] + '.json')).read_text(encoding='utf-8')))
        return summary(results, reports)
    if args.name == 'all':
        return run_all(args, results, rows)
    row = next(r for r in rows if r['name'] == args.name)
    report = Report(args.name, results, args.build_dir)
    try:
        if PLATFORM not in row['scripts']:
            report.check(args.name, 'SKIP', 'not a test for ' + PLATFORM)
        elif row.get('planned'):
            print('Future placeholder: ' + row['description'], flush=True)
            report.check(args.name, 'PLANNED', row['description'])
        else:
            from hosts import HANDLERS
            with host_lock() as acquired:
                if not acquired:
                    report.check('host-lock', 'SKIP', 'another release script is using this machine; try again when it finishes')
                elif os.environ.get('WLA_NO_DESKTOP') == '1' and row['handler'] != 'web':
                    report.check('desktop', 'SKIP', 'no logged-in desktop session for GUI/Audio Unit checks')
                else:
                    HANDLERS[row['handler']](report, args)
    except KeyboardInterrupt:
        report.check('interrupted', 'FAIL', 'test interrupted; cleanup attempted')
    except Exception as error:
        trace = report.work / 'exception.txt'
        trace.write_text(traceback.format_exc(), encoding='utf-8')
        report.artifact(trace)
        report.check('exception', 'FAIL', str(error))
        report.screenshot('exception')
    return report.finish()


if __name__ == '__main__':
    sys.exit(main())
