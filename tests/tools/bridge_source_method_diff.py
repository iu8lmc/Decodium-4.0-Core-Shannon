#!/usr/bin/env python3
"""Index obvious method-body matches against a revision; not a coverage test.

This intentionally does not interpret C++ overloads, macros, or preprocessor
branches. Ambiguous names and differences require human review.
"""
import argparse
import collections
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NAME = re.compile(r'\bDecodiumBridge::(~?\w+)\s*\(')


def method_bodies(source):
    # Mask comments and literals without changing byte positions. Count braces
    # only in code, so braces in messages and comments cannot end a method.
    # Apostrophes inside C++ numeric separators (e.g. 1'000) are not
    # character literals. Masking them as quotes can hide subsequent methods.
    token = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|(?<![A-Za-z0-9_])\'(?:\\.|[^\'\\\n])*\'')
    masked = token.sub(lambda m: ''.join('\n' if c == '\n' else ' ' for c in m.group()), source)
    result = collections.defaultdict(list)
    for match in NAME.finditer(masked):
        parens, close = 1, match.end()
        while close < len(masked) and parens:
            parens += (masked[close] == '(') - (masked[close] == ')')
            close += 1
        opening = masked.find('{', close)
        semicolon = masked.find(';', close)
        if opening < 0 or (semicolon >= 0 and semicolon < opening):
            continue
        depth, end = 1, opening + 1
        while end < len(masked) and depth:
            depth += (masked[end] == '{') - (masked[end] == '}')
            end += 1
        if not depth:
            # Preserve literals and comments: exact equality is deliberately
            # conservative and does not normalise changed forwarding calls.
            result[match.group(1)].append(source[opening:end])
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--base', default='v1.0.737')
    args = parser.parse_args()
    routing = (ROOT / 'src/bridge/DecodiumBridgeLogRouting.cpp').read_text()
    # Only recognise one-argument forwarders whose entire implementation is
    # precisely bridgeLog(message). Other helpers remain unclassified.
    forwarding_logs = re.findall(
        r'void\s+(decodiumBridge\w+Log)\s*\(\s*const\s+QString\s*&\s*message\s*\)'
        r'\s*\{\s*bridgeLog\(message\);\s*\}', routing)
    # These additional wrappers are checked explicitly against their small
    # definitions. Do not silently normalise them if their body changes.
    extra = {
        'decodiumBridgeWriteAdifDocument': ('src/bridge/DecodiumAdifSupport.cpp', 'return writeAdifDocument(path, doc);', 'writeAdifDocument'),
        'decodiumBridgeCountAdifRecords': ('src/bridge/DecodiumAdifSupport.cpp', 'return countAdifRecordsLightweight(path);', 'countAdifRecordsLightweight'),
        'decodiumBridgeBandFromFreqHz': ('src/bridge/DecodiumBridgeAdifRecord.cpp', 'return bandFromFreqHz(hz);', 'bandFromFreqHz'),
        'decodiumBridgeUtcTimeZone': ('src/bridge/DecodiumBridgeSharedRuntime.cpp', 'return decodiumUtcTimeZone();', 'decodiumUtcTimeZone'),
    }
    forwarders = {name: 'bridgeLog' for name in forwarding_logs}
    for name, (path, statement, target) in extra.items():
        source = (ROOT / path).read_text()
        if re.search(r'\b' + name + r'\s*\([^;{}]*\)\s*\{\s*' + re.escape(statement) + r'\s*\}', source):
            forwarders[name] = target
    # Also find simple value-preserving wrappers whose only statement returns
    # another function with the same named arguments, in the same order. This
    # is still a textual heuristic, not name-lookup or type analysis.
    simple = re.compile(
        r'\b(?:bool|int|qint64|qreal|double|float|QString|QTimeZone)\s+'
        r'(decodiumBridge\w+)\s*\(([^;{}()]*)\)\s*\{\s*return\s+'
        r'((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)\s*\(([^;{}()]*)\)\s*;\s*\}')
    for path in (ROOT / 'src/bridge').glob('*.cpp'):
        text = path.read_text(errors='replace')
        void_simple = re.compile(
            r'\bvoid\s+(decodiumBridge\w+)\s*\(([^;{}()]*)\)\s*\{\s*'
            r'((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)\s*\(([^;{}()]*)\)\s*;\s*\}')
        for name, params, target, arguments in simple.findall(text) + void_simple.findall(text):
            params = [p.strip() for p in params.split(',') if p.strip()]
            arguments = [a.strip() for a in arguments.split(',') if a.strip()]
            names = [re.search(r'([A-Za-z_]\w*)\s*$', p) for p in params]
            if all(names) and [m.group(1) for m in names] == arguments and name != target:
                forwarders.setdefault(name, target.rsplit('::', 1)[-1])
    old = collections.defaultdict(list)
    for path in ('src/bridge/DecodiumBridge.cpp', 'src/bridge/DecodiumBridgeSstv.cpp'):
        text = subprocess.check_output(['git', 'show', f'{args.base}:{path}'], cwd=ROOT, text=True)
        for name, bodies in method_bodies(text).items():
            old[name].extend(bodies)
    # Some worktree changes postdate the release and were captured immediately
    # before their own extraction. Keep candidate matches separate from the
    # release comparison and do not assume a fixture was executed as a test.
    fixtures = collections.defaultdict(list)
    for path in sorted((ROOT / 'tests/bridge_refactor_fixtures').glob('*_before.inc')):
        for name, bodies in method_bodies(path.read_text(errors='replace')).items():
            fixtures[name].extend((body, str(path.relative_to(ROOT))) for body in bodies)
    rows = []
    for path in sorted((ROOT / 'src/bridge').glob('DecodiumBridge*.cpp')):
        for name, bodies in method_bodies(path.read_text(errors='replace')).items():
            for index, body in enumerate(bodies):
                previous = old.get(name, [])
                normalised = body
                for forwarder, target in forwarders.items():
                    normalised = normalised.replace(forwarder + '(', target + '(')
                state = ('exact-body-match' if body in previous else
                         'forwarder-body-match' if normalised in previous else
                         'different-or-ambiguous' if previous else 'absent-from-two-baseline-files')
                fixture_matches = sorted({path for fixture_body, path in fixtures.get(name, [])
                                          if fixture_body in (body, normalised)})
                rows.append({'module': str(path.relative_to(ROOT)), 'method': name,
                             'module_overload_index': index,
                             'baseline_candidates': len(previous), 'state': state,
                             'matching_fixture_candidates': fixture_matches})
    print(json.dumps({'base': args.base,
                      'warning': 'Textual comparison only; forwarding normalisation is heuristic and does not prove execution, A/B coverage, overload identity, or semantic equivalence.',
                      'counts': dict(collections.Counter(row['state'] for row in rows)),
                      'methods': rows}, indent=2))


if __name__ == '__main__':
    main()
