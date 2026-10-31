"""[Codex 2026-10-30] Safety/contract checks; no DAW or real plugin installation touched."""
import argparse
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import common
import run
import hosts


class Contracts(unittest.TestCase):
    def test_staging_restores_original_after_body_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            source, dest = folder / 'new.dll', folder / 'slot.dll'
            source.write_bytes(b'new build')
            dest.write_bytes(b'old installation')
            r = common.Report('restore-test', folder / 'results')
            with self.assertRaisesRegex(RuntimeError, 'test failure'):
                with common.staged(r, source, dest):
                    self.assertEqual(dest.read_bytes(), b'new build')
                    raise RuntimeError('test failure')
            self.assertEqual(dest.read_bytes(), b'old installation')
            journal = json.loads(next(r.work.glob('slot.dll-*-restore.json')).read_text())
            self.assertTrue(journal['restored'])
            self.assertFalse(Path(journal['backup']).exists())

    def test_staging_restores_absent_slot_after_failed_copy(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            source, dest = folder / 'new.dll', folder / 'slot.dll'
            source.write_bytes(b'new build')
            r = common.Report('restore-test', folder / 'results')
            with patch('common.shutil.copy2', side_effect=OSError('copy failed')):
                with self.assertRaises(OSError):
                    with common.staged(r, source, dest):
                        self.fail('cannot enter body after copy failure')
            self.assertFalse(dest.exists())

    def test_staging_restores_existing_slot_after_failed_copy(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            source, dest = folder / 'new.dll', folder / 'slot.dll'
            source.write_bytes(b'new build')
            dest.write_bytes(b'old build')
            r = common.Report('restore-test', folder / 'results')
            with patch('common.shutil.copy2', side_effect=OSError('copy failed')):
                with self.assertRaises(OSError):
                    with common.staged(r, source, dest):
                        pass
            self.assertEqual(dest.read_bytes(), b'old build')

    def test_external_build_is_rejected(self):
        with self.assertRaises(ValueError):
            common.local_path('../another-worktree/build')

    def test_failed_restore_retains_backup_and_recovery_journal(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            source, dest = folder / 'new.dll', folder / 'slot.dll'
            source.write_bytes(b'new build')
            dest.write_bytes(b'old build')
            r = common.Report('restore-test', folder / 'results')
            original_unlink = Path.unlink
            def locked(path, *args, **kwargs):
                if path == dest:
                    raise PermissionError('host still has plugin open')
                return original_unlink(path, *args, **kwargs)
            with patch.object(Path, 'unlink', locked):
                with self.assertRaises(PermissionError):
                    with common.staged(r, source, dest):
                        pass
            journal = json.loads(next(r.work.glob('slot.dll-*-restore.json')).read_text())
            self.assertFalse(journal['restored'])
            self.assertEqual(Path(journal['backup']).read_bytes(), b'old build')

    def test_missing_host_is_skip(self):
        with tempfile.TemporaryDirectory() as temp:
            r = common.Report('missing-host', Path(temp) / 'results')
            self.assertIsNone(hosts.host(r, 'DAW', [Path(temp) / 'missing.exe']))
            self.assertEqual(r.data['checks'][0]['status'], 'SKIP')

    def test_same_named_slots_keep_separate_recovery_journals(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            source = folder / 'new.dll'
            source.write_bytes(b'new build')
            r = common.Report('two-slots', folder / 'results')
            destinations = [folder / 'user/Plugin.dll', folder / 'system/Plugin.dll']
            with common.staged(r, source, destinations[0]):
                with common.staged(r, source, destinations[1]):
                    journals = list(r.work.glob('*-restore.json'))
                    self.assertEqual(len(journals), 2)
                    self.assertEqual({json.loads(p.read_text())['destination'] for p in journals},
                                     {str(p) for p in destinations})
            self.assertTrue(all(json.loads(p.read_text())['restored'] for p in journals))
            self.assertTrue(all(not p.exists() for p in destinations))

    def test_existing_host_is_left_alone(self):
        with tempfile.TemporaryDirectory() as temp:
            exe = Path(temp) / 'host.exe'
            exe.write_bytes(b'fixture')
            r = common.Report('existing-host', Path(temp) / 'results')
            with patch('hosts.running', return_value=[123]), patch('hosts.host_version', return_value='test'):
                self.assertIsNone(hosts.host(r, 'DAW', [exe], exe.name))
            self.assertEqual(r.data['checks'][0]['status'], 'SKIP')

    def test_all_statuses_reported_failures_first(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            reports = []
            for state in common.CODES:
                r = common.Report(state.lower(), out)
                r.check('check', state, 'description')
                self.assertEqual(r.finish(), common.CODES[state])
                reports.append(r.data)
            self.assertEqual(run.summary(out, reports), 1)
            text = (out / 'summary.md').read_text()
            self.assertIn('2 of 4 checks automated and run; 1 planned; 1 skipped here', text)
            self.assertLess(text.index('[fail]'), text.index('[pass]'))

    def test_meta_continues_after_broken_child_and_does_not_accept_stale_result(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            (out / 'broken.json').write_text('{"status":"PASS"}')
            rows = [dict(name=name, scripts={common.PLATFORM: 'fake.ps1'}) for name in ('broken', 'later')]
            calls = []
            def child(command, **kwargs):
                name = rows[len(calls)]['name']
                calls.append(name)
                if name == 'broken':
                    return argparse.Namespace(returncode=1)
                r = common.Report(name, out)
                r.check('future', 'PLANNED', 'future')
                return argparse.Namespace(returncode=r.finish())
            with patch('run.subprocess.run', side_effect=child):
                code = run.run_all(argparse.Namespace(build_dir=None, no_logic=False), out, rows)
            self.assertEqual(code, 1)
            self.assertEqual(calls, ['broken', 'later'])
            report = json.loads((out / 'summary.json').read_text())
            self.assertEqual([r['status'] for r in report['results']], ['FAIL', 'PLANNED'])

    def test_every_manifest_row_has_script(self):
        self.assertGreater(len(run.sequence()), 10)


if __name__ == '__main__':
    unittest.main()
