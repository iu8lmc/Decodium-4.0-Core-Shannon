#!/usr/bin/env python3
"""Check both application source lists include each split Bridge file once."""
from collections import Counter
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
cmake = (root / 'CMakeLists.txt').read_text()
# The legacy source list precedes the QML target; the QML list is inside it.
prefix, qml = cmake.split('add_executable (decodium_qml', 1)
qml = qml.split('\n)', 1)[0]
pattern = r'src/bridge/DecodiumBridge\w*\.cpp'
files = {p.as_posix().replace(root.as_posix() + '/', '')
         for p in (root / 'src/bridge').glob('DecodiumBridge*.cpp')}
for name, source in (('legacy', prefix), ('qml', qml)):
    entries = Counter(re.findall(pattern, source))
    missing = sorted(files - entries.keys())
    duplicates = sorted(p for p in files if entries[p] > 1)
    stale = sorted(entries.keys() - files)
    if missing or duplicates or stale:
        raise SystemExit(f'{name}: missing={missing} duplicates={duplicates} stale={stale}')
    print(f'{name}: {len(files)} split Bridge sources included once')
