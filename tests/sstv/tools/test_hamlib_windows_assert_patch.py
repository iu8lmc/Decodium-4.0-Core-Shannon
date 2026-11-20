#!/usr/bin/env python3
"""Compile the Hamlib Windows timer branch with a minimal Windows API stub."""
import argparse
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('hamlib', type=Path, help='unmodified extracted Hamlib 4.7.0 root')
p.add_argument('--cc', default='clang')
a = p.parse_args()
original = (a.hamlib / 'lib' / 'precise_time.c').read_text()
patch = Path(__file__).resolve().parents[1] / 'fixtures' / 'hamlib-4.7.0-windows-assert.patch'
with tempfile.TemporaryDirectory(prefix='decodium-hamlib-assert-') as tmp:
    root = Path(tmp)
    (root / 'lib').mkdir()
    (root / 'lib' / 'precise_time.c').write_text(original)
    subprocess.run(['patch', '-s', '-d', str(root), '-p1', '-i', str(patch)], check=True)
    fixed = (root / 'lib' / 'precise_time.c').read_text()
    assert fixed == original.replace('#include <windows.h>', '#include <assert.h>\n#include <windows.h>', 1)
    (root / 'windows.h').write_text('typedef struct { long long QuadPart; } LARGE_INTEGER;\n'
                                  'int QueryPerformanceFrequency(LARGE_INTEGER*);\n'
                                  'int QueryPerformanceCounter(LARGE_INTEGER*);\n')
    def windows_branch(source):
        start = source.index('#elif defined(__WIN32__)')
        end = source.index('\n#else\n// Fall back to rdtsc', start)
        return source[start:end].split('\n', 1)[1]
    for label, define in (('a-debug', []), ('b-ndebug', ['-DNDEBUG'])):
        for state, source in (('before', original), ('after', fixed)):
            path = root / (label + '-' + state + '.c')
            path.write_text(windows_branch(source))
            result = subprocess.run([a.cc, '-std=c11', '-Werror=implicit-function-declaration',
                '-D__cdecl=', '-D__declspec(x)=', *define, '-I', str(root), '-fsyntax-only', str(path)],
                text=True, capture_output=True)
            if state == 'before':
                assert result.returncode != 0 and "'assert'" in result.stderr, result.stderr
            else:
                assert result.returncode == 0, result.stderr
    # Reapplying to the same extracted source must fail instead of silently
    # patching a second location or hiding a mismatched archive.
    again = subprocess.run(['patch', '--batch', '--forward', '-s', '-d', str(root), '-p1', '-i', str(patch)],
                           capture_output=True)
    assert again.returncode != 0
print('PASS Windows timer branch A/B: missing assert rejected before and accepted after patch')
