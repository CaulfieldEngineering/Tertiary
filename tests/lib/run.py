#!/usr/bin/env python3
"""[Codex 2026-10-30] Registry IDs, folder-discovered product tests, shared CI/local reports."""
import argparse
import datetime as dt
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import traceback
from urllib.parse import quote

from common import CODES, PLATFORM, ROOT, Report, host_lock, now, running, write_json


def sequence():
    rows = json.loads((ROOT / 'tests/sequence.json').read_text(encoding='utf-8'))
    ids = [r['id'] for r in rows]
    if len(set(ids)) != len(ids):
        raise ValueError('Duplicate test IDs in sequence.json')
    scripts = set()
    for row in rows:
        for platform, path in row['scripts'].items():
            if not (ROOT / 'tests' / path).is_file() and not (row['scope'] == 'product' and row.get('optional')):
                raise ValueError('Missing sequence script: ' + path)
            if (platform, path) in scripts:
                raise ValueError('Two test rows share a script: ' + path)
            scripts.add((platform, path))
    return rows


def result_folder(value):
    if value:
        path = Path(value).resolve()
        path.mkdir(parents=True, exist_ok=True)
        return path
    base = ROOT / 'tests/results' / dt.datetime.now().strftime('%Y-%m-%d_%H%M')
    path, suffix = base, 0
    while path.exists():
        suffix += 1
        path = base.with_name(base.name + f'-{suffix:02}')
    path.mkdir(parents=True)
    return path


def excluded(row, args, native=True):
    if PLATFORM not in row['scripts']:
        return 'other platform'
    if args.profile == 'ci':
        if not row['ci_ok']:
            return 'GitHub checkbox is off'
        if row.get('daw') or row.get('human') or row.get('mac_day_only'):
            return 'DAW, human or Mac-day-only check'
        if native and row.get('ci_job') == 'web':
            return 'runs in the separate web job with the same script'
    if row.get('optional') and not (ROOT / 'tests' / row['scripts'][PLATFORM]).exists():
        return 'no product test written yet'
    return None


def summary(results, reports, *, product=False, omissions=(), profile='local'):
    counts = {s: sum(c['status'] == s for r in reports for c in r['checks']) for s in CODES}
    total = sum(counts.values())
    coverage = (f"{counts['PASS'] + counts['FAIL']} of {total} checks automated and run; "
                f"{counts['PLANNED']} planned; {counts['SKIP']} skipped here")
    if product and not reports:
        coverage = 'no product tests yet'
    web = bool(reports) and all(r['id'].startswith('WEB-') for r in reports)
    suite_id = ('SU-02' if PLATFORM == 'win' else 'SU-04') if product else ('SU-05' if web else 'SU-01' if PLATFORM == 'win' else 'SU-03')
    data = dict(suite_id=suite_id, platform=PLATFORM, profile=profile, finished=now(),
                status='FAIL' if counts['FAIL'] else 'PASS', coverage=coverage, counts=counts,
                results=reports, excluded=list(omissions))
    stem = 'product-summary' if product else 'summary'
    write_json(results / (stem + '.json'), data)
    def cell(value):
        return str(value).replace('|', '\\|').replace('\n', '<br>')
    def link(path, label=None):
        return f'[{cell(label or Path(path).name)}]({quote(path, safe="/:")})'
    lines = [f'# {suite_id}: {"Product" if product else "Release"} checks ({PLATFORM}, {profile})', '', coverage, '',
             'PASS means the named check ran. PLANNED, SKIP and excluded rows are open coverage, not release approval.', '',
             '## Scripts (failures first; checklist order within each group)', '',
             '| ID | Test | Suites | Status | Build id | Artifacts |', '|---|---|---|---|---|---|']
    for r in sorted(reports, key=lambda item: item['status'] != 'FAIL'):
        artifacts = ' '.join(link(p) for p in r['artifacts'] if Path(p).suffix in ('.log', '.txt', '.json', '.md'))
        lines.append(f"| {r['id']} | {link(r['name'] + '.json', r.get('title', r['name']))} | {', '.join(r['suite_ids'])} | {r['status']} | {cell(r['build']['id'])} | {artifacts} |")
    lines += ['', '## Checks (failures first)', '', '| Test ID | Check | Status | Detail |', '|---|---|---|---|']
    checks = [c for r in reports for c in r['checks']]
    for c in sorted(checks, key=lambda item: item['status'] != 'FAIL'):
        lines.append(f"| {c['test_id']} | {cell(c['id'])} | {c['status']} | {cell(c['detail'])} |")
    metrics = [(r['id'], name, value) for r in reports for name, value in r.get('metrics', {}).items()]
    if metrics:
        lines += ['', '## Coverage compared with the pre-layout CI', '', '| Test ID | Measurement | Before | This run |', '|---|---|---|---|']
        lines += [f"| {id} | {name} | {v['before']} | {v['after']} |" for id, name, v in metrics]
    if omissions:
        lines += ['', '## Not selected', '', '| Test ID | Reason |', '|---|---|']
        lines += [f"| {r['id']} | {r['reason']} |" for r in omissions]
    (results / (stem + '.md')).write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print('\n' + coverage + '\nReport: ' + str(results / (stem + '.md')), flush=True)
    return int(counts['FAIL'] > 0)


def command_for(script, args, results):
    if script.suffix.lower() == '.ps1':
        command = [shutil.which('pwsh') or 'powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script),
                   '-ResultsDir', str(results), '-Profile', args.profile]
        if args.build_dir:
            command += ['-BuildDir', args.build_dir]
        if args.expect_demo:
            command += ['-ExpectDemo']
    else:
        command = ['bash', str(script), '--results-dir', str(results), '--profile', args.profile]
        if args.build_dir:
            command += ['--build-dir', args.build_dir]
        if args.no_logic:
            command += ['--no-logic']
        if args.expect_demo:
            command += ['--expect-demo']
    return command


def validate_result(data, row, code):
    if data['id'] != row['id'] or data['name'] != row['id'] or CODES[data['status']] != code or not data['finished']:
        raise ValueError(f'result/exit mismatch for {row["id"]} (exit {code})')
    for field in ('build', 'host', 'checks', 'artifacts', 'platform', 'started', 'suite_ids'):
        if field not in data:
            raise ValueError('missing result field: ' + field)
    if data['suite_ids'] != row.get('suite_ids', []):
        raise ValueError('Suite IDs differ from the registry')
    if not data['checks'] or any(c.get('test_id') != row['id'] for c in data['checks']):
        raise ValueError('empty checks or missing/mismatched Blocks test ID')


def child(row, args, results):
    script = ROOT / 'tests' / row['scripts'][PLATFORM]
    target = results / (row['id'] + '.json')
    if target.exists():
        target.unlink()  # An earlier PASS can never cover a failed launch.
    try:
        code = subprocess.run(command_for(script, args, results), cwd=ROOT, timeout=5400).returncode
        if row.get('unregistered'):
            raise ValueError('Executed product script, but it has no Blocks ID/metadata in sequence.json: ' + str(script))
        data = json.loads(target.read_text(encoding='utf-8'))
        validate_result(data, row, code)
    except Exception as error:
        report = Report(row['id'], results, args.build_dir, row, args.profile)
        report.check('runner', 'FAIL', repr(error))
        report.finish()
        data = report.data
    return data


def product_rows(rows):
    """Discover files, not a second list. Unknown scripts cannot disappear."""
    folder = ROOT / 'tests' / PLATFORM / 'product'
    pattern = 'Test-*.ps1' if PLATFORM == 'win' else 'test-*.sh'
    known = {r['scripts'].get(PLATFORM): r for r in rows if r['scope'] == 'product'}
    order = {r['id']: i for i, r in enumerate(rows)}
    found = []
    for path in sorted(folder.glob(pattern)):
        relative = path.relative_to(ROOT / 'tests').as_posix()
        found.append(known.get(relative) or dict(id='UNREGISTERED-' + path.stem, name=path.stem,
                     scripts={PLATFORM: relative}, scope='product', suite_ids=[], ci_ok=True, unregistered=True))
    return sorted(found, key=lambda r: order.get(r['id'], len(rows)))


def run_product(args, results, rows):
    reports, omissions = [], []
    for row in product_rows(rows):
        reason = excluded(row, args)
        if reason:
            omissions.append(dict(id=row['id'], reason=reason))
            continue
        reports.append(child(row, args, results))
        summary(results, reports, product=True, omissions=omissions, profile=args.profile)
    return summary(results, reports, product=True, omissions=omissions, profile=args.profile)


def run_all(args, results, rows):
    reports, omissions = [], []
    product_script = ROOT / 'tests' / PLATFORM / ('Run-All-Product-Tests.ps1' if PLATFORM == 'win' else 'run-all-product-tests.sh')
    product_target = results / 'product-summary.json'
    if product_target.exists():
        product_target.unlink()
    try:
        code = subprocess.run(command_for(product_script, args, results), cwd=ROOT, timeout=14400).returncode
        products = json.loads(product_target.read_text(encoding='utf-8'))
        if code != int(products['status'] == 'FAIL'):
            raise ValueError('Product summary/exit mismatch')
        for result in products['results']:
            row = next((r for r in rows if r['id'] == result['id']), None)
            if row:
                validate_result(result, row, CODES[result['status']])
    except Exception as error:
        # A broken aggregator must not suppress the remaining universal/DAW checks.
        affected = [r for r in product_rows(rows) if not excluded(r, args)]
        if not affected:
            affected = [dict(id='SU-02' if PLATFORM == 'win' else 'SU-04', name='Product runner', suite_ids=[])]
        failed = []
        for row in affected:
            r = Report(row['id'], results, args.build_dir, row, args.profile)
            r.check('product-runner', 'FAIL', repr(error))
            r.finish()
            failed.append(r.data)
        summary(results, failed, product=True, profile=args.profile)
        products = json.loads(product_target.read_text(encoding='utf-8'))
    reports.extend(products['results'])
    omissions.extend(products.get('excluded', []))
    for row in rows:
        reason = excluded(row, args)
        if reason:
            if reason != 'other platform':
                omissions.append(dict(id=row['id'], reason=reason))
            continue
        if row['scope'] == 'product':
            continue
        reports.append(child(row, args, results))
        summary(results, reports, omissions=omissions, profile=args.profile)
    order = {r['id']: i for i, r in enumerate(rows)}
    reports.sort(key=lambda r: order.get(r['id'], len(rows)))
    return summary(results, reports, omissions=omissions, profile=args.profile)


def main():
    def interrupted(signum, frame):
        raise KeyboardInterrupt('signal ' + str(signum))
    signal.signal(signal.SIGTERM, interrupted)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('name')
    parser.add_argument('--results-dir')
    parser.add_argument('--build-dir')
    parser.add_argument('--profile', choices=('local', 'ci'), default='local')
    parser.add_argument('--expect-demo', action='store_true')
    parser.add_argument('--engine', default='all', choices=('all', 'chromium', 'firefox', 'webkit'))
    parser.add_argument('--no-logic', action='store_true')
    args = parser.parse_args()
    rows = sequence()
    results = result_folder(args.results_dir)
    if args.name == 'product':
        return run_product(args, results, rows)
    if args.name == 'all':
        return run_all(args, results, rows)
    if args.name == 'summarize':
        reports = [json.loads(p.read_text(encoding='utf-8')) for r in rows
                   if (p := results / (r['id'] + '.json')).exists()]
        return summary(results, reports, profile=args.profile)
    row = next(r for r in rows if r['id'] == args.name)
    report = Report(row['id'], results, args.build_dir, row, args.profile)
    try:
        reason = excluded(row, args, native=False)
        if reason:
            report.check('profile', 'SKIP', reason)
        elif row.get('planned'):
            report.check(row['id'], 'PLANNED', row['description'])
        else:
            from hosts import HANDLERS
            from native import HANDLERS as NATIVE
            handlers = {**HANDLERS, **NATIVE}
            with host_lock() as acquired:
                if not acquired:
                    report.check('host-lock', 'SKIP', 'another release script is using this machine; try again when it finishes')
                elif os.environ.get('WLA_NO_DESKTOP') == '1' and row['handler'] != 'web':
                    report.check('desktop', 'SKIP', 'no logged-in desktop session for GUI/Audio Unit checks')
                else:
                    # Other worktrees may still use the older harness without our OS lock.
                    busy = [name for name in ('UiTest', 'PerfTest', 'HostTest', 'ReviewRender', report.product + 'Tests', report.product)
                            if running(name + ('.exe' if PLATFORM == 'win' else ''))]
                    if busy and row['handler'] != 'web':
                        report.check('other-run', 'SKIP', 'Other test processes are active: ' + ', '.join(busy))
                    else:
                        handlers[row['handler']](report, args)
            if args.profile == 'ci' and any(c['status'] == 'SKIP' for c in report.data['checks']):
                report.check('ci-coverage', 'FAIL', 'A selected CI check could not run; see SKIP details')
    except KeyboardInterrupt:
        report.check('interrupted', 'FAIL', 'test interrupted; cleanup attempted')
    except Exception as error:
        trace = report.work / 'exception.txt'
        trace.write_text(traceback.format_exc(), encoding='utf-8')
        report.artifact(trace)
        report.check('exception', 'FAIL', str(error))
        report.screenshot('exception')
    code = report.finish()
    if row.get('ci_job') == 'web':
        summary(results, [report.data], profile=args.profile)
        return int(report.data['status'] == 'FAIL') if args.profile == 'ci' else code
    return code


if __name__ == '__main__':
    sys.exit(main())
