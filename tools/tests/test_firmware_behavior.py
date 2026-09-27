"""A successful emulator run must not hide restrictions or missing evidence."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from firmware_behavior import CHECKS, digest, validate, render


class BehaviorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        p = self.root / 'evidence'
        p.write_bytes(b'synthetic fixture, not a hardware claim')
        self.report = dict(schema=1, model='fixture', scope='unit test',
            execution=dict(outcome='pass', entry_boundary='entry', stop_boundary='return',
                           mocked=['USB'], not_modeled=['ADC']),
            assets={r: dict(path=p.name, sha256=digest(p), role=r)
                    for r in ('producer-code', 'raw-result', 'firmware-input')},
            findings={k: dict(state='established', detail='fixture', evidence=[
                dict(asset='raw-result', level='host_test', locator=k)]) for k in CHECKS},
            candidates=[dict(id='analog', disposition='suitable', reason='fixture',
                             evidence=[dict(asset='raw-result', locator='candidate')])],
            selection=dict(candidate='analog', rationale='fixture', coverage_limits='fixture only'))

    def check(self):
        return validate(self.report, self.root)

    def test_ready_does_not_require_physical_testing(self):
        self.assertEqual(self.check()['verdict'], 'READY_FOR_REVIEW')

    def test_pass_retains_typing_restriction_and_selected_fallback(self):
        self.report['findings']['normal_typing']['state'] = 'restricted'
        result = self.check()
        self.assertEqual(result['verdict'], 'LIMITED_ONLY')
        self.assertEqual(result['best_available'], 'analog')
        self.assertLess(render(self.report, result).index('normal_typing [restricted]'),
                        render(self.report, result).index('release [established]'))

    def test_unknown_is_not_success(self):
        self.report['findings']['crash_recovery']['state'] = 'unknown'
        self.assertEqual(self.check()['verdict'], 'REVIEW_INCOMPLETE')

    def test_limited_candidate_cannot_be_ready(self):
        self.report['candidates'][0]['disposition'] = 'limited'
        self.assertEqual(self.check()['verdict'], 'LIMITED_ONLY')

    def test_no_selection_is_not_ready(self):
        self.report['selection']['candidate'] = None
        self.assertEqual(self.check()['verdict'], 'NO_SELECTED_PATH')

    def test_failed_execution_is_not_promoted(self):
        self.report['execution']['outcome'] = 'fail'
        self.assertEqual(self.check()['verdict'], 'EVIDENCE_INCOMPLETE')

    def test_incomplete_or_unsupported_assertions_rejected(self):
        original = copy.deepcopy(self.report)
        mutations = [
            lambda r: r['findings'].pop('normal_typing'),
            lambda r: r['execution'].pop('mocked'),
            lambda r: r.update(verdict='READY_FOR_REVIEW'),
            lambda r: r['findings']['normal_typing'].update(evidence=[]),
            lambda r: r['candidates'][0].update(evidence=[]),
            lambda r: r['candidates'][0].update(disposition='rejected'),
            lambda r: r['selection'].pop('coverage_limits'),
            lambda r: r['assets']['raw-result'].update(path='../outside'),
            lambda r: r['assets']['raw-result'].update(sha256='0'*64),
        ]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                self.report = copy.deepcopy(original)
                mutation(self.report)
                with self.assertRaises(ValueError): self.check()


if __name__ == '__main__': unittest.main()
