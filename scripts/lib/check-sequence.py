#!/usr/bin/env python3
"""[Claude 2026-10-31] GitHub reads scripts/sequence.json too: this check (run by test.yml) fails when the build
sequence and the workflows drift apart. Every step's scripts exist, and every workflow job a step names really runs
that step's script; every scripts/ path a workflow calls exists. No network, no build.

  python scripts/lib/check-sequence.py
"""
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]


def jobs(text):
    """{job name: its text} for a workflow file (top-level jobs, two-space indent)."""
    body = text.split('\njobs:', 1)[1]
    found = {}
    for m in re.finditer(r'^  ([A-Za-z0-9_-]+):\s*$(.*?)(?=^  [A-Za-z0-9_-]+:\s*$|\Z)', body, re.M | re.S):
        found[m.group(1)] = m.group(2)
    return found


def main():
    problems = []
    rows = json.loads((ROOT / 'scripts/sequence.json').read_text(encoding='utf-8'))
    ids = [r['id'] for r in rows]
    if len(ids) != len(set(ids)):
        problems.append('duplicate step IDs in scripts/sequence.json')
    flows = {p.name: p.read_text(encoding='utf-8') for p in (ROOT / '.github/workflows').glob('*.yml')}
    for row in rows:
        for plat, script in row['scripts'].items():
            if not (ROOT / 'scripts' / script).is_file():
                problems.append(f'{row["id"]}: scripts/{script} does not exist')
        for flow, names in row.get('github', {}).items():
            if flow not in flows:
                problems.append(f'{row["id"]}: no workflow {flow}')
                continue
            table = jobs(flows[flow])
            for job in names:
                text = table.get(job)
                if text is None:
                    problems.append(f'{row["id"]}: {flow} has no job {job}')
                elif not any('scripts/' + s in text for s in row['scripts'].values()):
                    if 'Sign-Windows' in ' '.join(row['scripts'].values()) and 'scripts/' + row['scripts'].get('mac', '-') in text:
                        continue   # Windows signing stays in the pinned Azure action; the Mac half is the script
                    problems.append(f'{row["id"]}: {flow} job {job} does not call ' + ' or '.join('scripts/' + s for s in row['scripts'].values()))
    for flow, text in flows.items():
        for path in sorted(set(p.rstrip('.') for p in re.findall(r'scripts/(?:win|mac|web|lib)/[A-Za-z0-9_.-]+', text))):   # a sentence's full stop is not part of a path
            if not (ROOT / path).is_file():
                problems.append(f'{flow} calls {path}, which does not exist')
    for p in problems:
        print('::error::' + p if '--github' in sys.argv else 'FAIL ' + p)
    print(f'{len(rows)} build steps checked against {len(flows)} workflows: ' + ('FAIL' if problems else 'PASS'))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
