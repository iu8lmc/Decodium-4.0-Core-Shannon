#!/usr/bin/env python3
"""Validate a returned kit report against a trusted local kit manifest.

Reads ZIP members without extraction. Recomputes message comparisons rather than
trusting comparison.json. Process CPU/RAM figures describe sampled whole runs,
not pure decoder execution, RF reception or universal decode equivalence.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import re
import zipfile


def analyse(archive, manifest):
    expected_paths = {'machine.json', 'comparison.json'}
    roles = ['before', 'after', 'after', 'before']
    for i, role in enumerate(roles, 1):
        expected_paths.update({f'{i}-{role}/result.json', f'{i}-{role}/console.log'})
    with zipfile.ZipFile(archive) as z:
        files = [i for i in z.infolist() if not i.is_dir()]
        names = [i.filename.replace('\\', '/') for i in files]
        if len(names) != len(set(names)) or set(names) - expected_paths:
            raise ValueError('Unexpected or duplicate report members')
        if sum(i.file_size for i in files) > 100_000_000:
            raise ValueError('Report exceeds diagnostic size limit')
        members = dict(zip(names, files))
        missing = expected_paths - {'comparison.json'} - set(members)
        if missing:
            raise ValueError('Missing report members: ' + ', '.join(sorted(missing)))
        read = lambda name: json.loads(z.read(members[name]).decode('utf-8-sig'))
        machine = read('machine.json')
        if machine['manifest'] != manifest or not manifest['ready']:
            raise ValueError('Report does not match the trusted ready kit manifest')
        if machine.get('directGraphicsWorker') is not True:
            raise ValueError('CPU measurements do not identify the graphics worker')
        fixtures = {f['id']: f for f in manifest['fixtures']}
        runs, metrics = [], []
        for i, role in enumerate(roles, 1):
            report = read(f'{i}-{role}/result.json')
            if (report['index'] != i or report['version'] != role or report['valid'] is not True
                    or report['exit'] != 0 or report['binarySha256'] != manifest['builds'][role]['sha256']):
                raise ValueError(f'Run {i}: invalid status, role or binary identity')
            console = z.read(members[f'{i}-{role}/console.log']).decode('utf-8-sig', errors='replace')
            if '[LAB] CAT auto-connect disabled' not in console or 'QNativeSocketEngine::writeDatagram() was called not in' in console:
                raise ValueError(f'Run {i}: CAT/UDP isolation not verified')
            events = report['events']
            expected_sequence = [(kind, case) for case in fixtures for kind in ('start', 'result')] + [('done', None)]
            if [(e.get('event'), e.get('caseId')) for e in events] != expected_sequence:
                raise ValueError(f'Run {i}: unexpected event sequence')
            starts = [e for e in events if e['event'] == 'start']
            results = [e for e in events if e['event'] == 'result']
            if (Counter(e['caseId'] for e in starts) != Counter({k: 1 for k in fixtures})
                    or Counter(e['caseId'] for e in results) != Counter({k: 1 for k in fixtures})
                    or sum(e['event'] == 'done' for e in events) != 1):
                raise ValueError(f'Run {i}: incomplete or duplicate cases')
            for e in starts:
                if (not e['cleared'] or e['mode'] != fixtures[e['caseId']]['mode'] or e['depth'] != 3
                        or e['threads'] != machine['threads'] or e['lowCpu'] != machine['lowCpu']):
                    raise ValueError(f'Run {i}: mismatched settings')
            messages = {}
            for e in results:
                if e['timeout'] or e['tx'] or e['stillDecoding']:
                    raise ValueError(f'Run {i}: incomplete decode or TX')
                f = fixtures[e['caseId']]
                values = [row['message'] for row in e['rows']]
                wanted = f['expected']
                if wanted:
                    if f['mode'].startswith('Q65'):
                        found = any(re.fullmatch(re.escape(wanted) + r'(?: q\d{1,2})?', ' '.join(v.split())) for v in values)
                    else:
                        found = wanted in values
                    if not found:
                        raise ValueError(f'Run {i}: expected message absent for {f["id"]}')
                messages[e['caseId']] = values
            runs.append(messages)
            samples = report['samples']
            if len(samples) < 2:
                raise ValueError(f'Run {i}: insufficient process measurements')
            for sample in samples:
                for key in ('seconds', 'cpuSeconds', 'workingSetBytes', 'privateBytes'):
                    value = sample.get(key)
                    if (type(value) not in (int, float) or not math.isfinite(value) or value < 0):
                        raise ValueError(f'Run {i}: invalid process measurement {key}')
            if any(b['seconds'] <= a['seconds'] or b['cpuSeconds'] < a['cpuSeconds'] for a, b in zip(samples, samples[1:])):
                raise ValueError(f'Run {i}: non-monotonic process measurements')
            duration = samples[-1]['seconds'] - samples[0]['seconds']
            metrics.append({'run': i, 'version': role, 'sampledSeconds': duration,
                            'averageCpuCorePercent': 100*(samples[-1]['cpuSeconds']-samples[0]['cpuSeconds'])/duration,
                            'peakWorkingSetMiB': max(s['workingSetBytes'] for s in samples)/1048576,
                            'peakPrivateMiB': max(s['privateBytes'] for s in samples)/1048576})
    comparisons = []
    for before, after in [(0, 1), (3, 2)]:
        for case in fixtures:
            a, b = Counter(runs[before][case]), Counter(runs[after][case])
            comparisons.append({'beforeRun': before+1, 'afterRun': after+1, 'case': case,
                                'identical': a == b, 'missing': list((a-b).elements()),
                                'extra': list((b-a).elements())})
    return {'reportValidated': True, 'identicalMessageMultisets': all(c['identical'] for c in comparisons),
            'hardware': {k: machine[k] for k in ['cpu', 'os', 'threads', 'lowCpu', 'graphicsBackend']},
            'comparisons': comparisons, 'sampledProcessMetrics': metrics,
            'scope': 'Reported file-decode A/B on stated hardware; no RF, live-audio or universal no-loss guarantee.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--report', type=Path, required=True)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    result = analyse(a.report, json.loads(a.manifest.read_text()))
    a.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k: result[k] for k in ['reportValidated', 'identicalMessageMultisets']}))
    return 0 if result['identicalMessageMultisets'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
