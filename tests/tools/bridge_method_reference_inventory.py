#!/usr/bin/env python3
"""List Bridge method names absent from test source tokens (not a coverage test)."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
methods = {}
pattern = re.compile(r'\bDecodiumBridge::(~?\w+)\s*\(')
for path in sorted((ROOT / 'src/bridge').glob('DecodiumBridge*.cpp')):
    for name in pattern.findall(path.read_text(errors='replace')):
        methods.setdefault(name, set()).add(str(path.relative_to(ROOT)))
tokens = set()
for path in (ROOT / 'tests').rglob('*'):
    if path.is_file() and path.suffix in ('.py', '.cpp') and 'evidence' not in path.parts:
        tokens.update(re.findall(r'[A-Za-z_]\w*', path.read_text(errors='replace')))
missing = [{'method': name, 'sources': sorted(paths)} for name, paths in sorted(methods.items())
           if name.lstrip('~') not in tokens]
print(json.dumps({'warning': 'Token matching does not establish function definitions, A/B execution or runtime coverage; missing names may be tested indirectly.',
                  'distinct_method_tokens': len(methods), 'not_named_in_test_sources': missing}, indent=2))
