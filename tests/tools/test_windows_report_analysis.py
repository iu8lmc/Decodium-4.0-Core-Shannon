#!/usr/bin/env python3
"""Synthetic report tests; these do not execute Windows or certify hardware."""
import copy
import io
import json
import unittest
import zipfile
from analyse_windows_performance_report import analyse


class ReportAnalysisTest(unittest.TestCase):
    def setUp(self):
        self.manifest = {'ready': True, 'fixtures': [
            {'id': 'busy', 'mode': 'FT8', 'expected': 'CQ K1ABC FN42'}],
            'builds': {'before': {'sha256': 'baseline'}, 'after': {'sha256': 'candidate'}}}
        self.members = {'machine.json': {'manifest': self.manifest, 'directGraphicsWorker': True,
            'threads': 2, 'lowCpu': False, 'graphicsBackend': 'd3d11',
            'cpu': [{'Name': 'Synthetic test CPU'}], 'os': {'Caption': 'Synthetic Windows'}}}
        for i, role in enumerate(('before', 'after', 'after', 'before'), 1):
            self.members[f'{i}-{role}/console.log'] = '[LAB] CAT auto-connect disabled'
            self.members[f'{i}-{role}/result.json'] = {'index': i, 'version': role,
                'valid': True, 'exit': 0, 'binarySha256': self.manifest['builds'][role]['sha256'],
                'events': [{'event': 'start', 'caseId': 'busy', 'mode': 'FT8', 'depth': 3,
                    'threads': 2, 'lowCpu': False, 'cleared': True},
                    {'event': 'result', 'caseId': 'busy', 'timeout': False, 'tx': False,
                     'stillDecoding': False, 'rows': [{'message': m} for m in
                         ('CQ K1ABC FN42', 'CQ K2ABC FN42', 'CQ K2ABC FN42')]},
                    {'event': 'done'}],
                'samples': [{'seconds': t, 'cpuSeconds': cpu, 'workingSetBytes': 1048576,
                    'privateBytes': 2097152} for t, cpu in ((1, 2), (3, 3))]}

    def run_report(self, extra=None):
        data = io.BytesIO()
        with zipfile.ZipFile(data, 'w') as z:
            for name, value in self.members.items():
                z.writestr(name, value if isinstance(value, str) else json.dumps(value))
            if extra:
                z.writestr(*extra)
        data.seek(0)
        return analyse(data, self.manifest)

    def candidate(self):
        return self.members['2-after/result.json']

    def test_valid_metrics(self):
        r = self.run_report()
        self.assertTrue(r['identicalMessageMultisets'])
        self.assertEqual(r['sampledProcessMetrics'][0]['averageCpuCorePercent'], 50)
        self.assertEqual(r['sampledProcessMetrics'][0]['peakPrivateMiB'], 2)

    def test_same_count_substitution_ignores_claimed_success(self):
        self.members['comparison.json'] = {'identicalMessageMultisets': True}
        self.candidate()['events'][1]['rows'][1]['message'] = 'CQ K3ABC FN42'
        r = self.run_report()
        self.assertFalse(r['identicalMessageMultisets'])
        self.assertEqual(r['comparisons'][0]['missing'], ['CQ K2ABC FN42'])

    def test_missing_duplicate(self):
        self.candidate()['events'][1]['rows'].pop()
        self.assertFalse(self.run_report()['identicalMessageMultisets'])

    def test_wrong_binary(self):
        self.candidate()['binarySha256'] = 'wrong'
        with self.assertRaisesRegex(ValueError, 'binary identity'): self.run_report()

    def test_wrong_configuration(self):
        self.candidate()['events'][0]['lowCpu'] = True
        with self.assertRaisesRegex(ValueError, 'settings'): self.run_report()

    def test_wrong_manifest(self):
        self.members['machine.json']['manifest'] = copy.deepcopy(self.manifest)
        self.members['machine.json']['manifest']['ready'] = False
        with self.assertRaisesRegex(ValueError, 'manifest'): self.run_report()

    def test_nonfinite_or_negative_measurements(self):
        for value in (float('nan'), float('inf'), -1, True):
            with self.subTest(value=value):
                self.candidate()['samples'][0]['cpuSeconds'] = value
                with self.assertRaisesRegex(ValueError, 'measurement'): self.run_report()

    def test_nonmonotonic_samples(self):
        self.candidate()['samples'][1]['seconds'] = 1
        with self.assertRaisesRegex(ValueError, 'non-monotonic'): self.run_report()

    def test_missing_file(self):
        del self.members['1-before/console.log']
        with self.assertRaisesRegex(ValueError, 'Missing'): self.run_report()

    def test_traversal_and_normalized_duplicate(self):
        for name in ('../outside.txt', '1-before\\console.log'):
            with self.subTest(name=name):
                with self.assertRaisesRegex(ValueError, 'Unexpected or duplicate'):
                    self.run_report((name, 'x'))

    def test_udp_failure(self):
        self.members['2-after/console.log'] += '\nQNativeSocketEngine::writeDatagram() was called not in'
        with self.assertRaisesRegex(ValueError, 'UDP'): self.run_report()

    def test_out_of_order_events(self):
        events = self.candidate()['events']
        events[0], events[1] = events[1], events[0]
        with self.assertRaisesRegex(ValueError, 'sequence'): self.run_report()


if __name__ == '__main__':
    unittest.main()
