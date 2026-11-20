#!/usr/bin/env python3
"""Check independently decoded A/B phase-impaired HAMDRM captures."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zlib

analyzer = Path(sys.argv[1]).resolve(strict=True)
fixtures = Path(sys.argv[2]).resolve(strict=True)
hashes = {
    'a': '9ae69f982b0946caa3a681ef4dedd50a3075a523f77b56849b81513bec0dd21b',
    'b': '4daa4eafacca5d46d3a461e621b2a6c93cf108e9531af0eae9625e4032d0ae3d',
}
env = os.environ.copy()
for name in list(env):
    if name.startswith('DECODIUM_HAMDRM_'):
        del env[name]
env['DECODIUM_HAMDRM_TEMPORAL_MERGE'] = '1'
with tempfile.TemporaryDirectory(prefix='decodium-qsstv-a23-regression-') as directory:
    for variant, other in (('a', 'b'), ('b', 'a')):
        pcm = zlib.decompress((fixtures / f'{variant}.s16.zlib').read_bytes())
        if hashlib.sha256(pcm).hexdigest() != hashes[variant]:
            raise RuntimeError(f'{variant}: fixture hash mismatch')
        path = Path(directory) / f'{variant}.s16'
        path.write_bytes(pcm)
        temporal_env = env.copy()
        del temporal_env['DECODIUM_HAMDRM_TEMPORAL_MERGE']
        temporal_env['DECODIUM_HAMDRM_TEMPORAL_PILOTS'] = '1'
        temporal = subprocess.run(
            [str(analyzer), str(path), 'A', '2300', '16', '0.12', '0.30',
             str(fixtures / f'{variant}.jp2'), 'short', 'flat-multistage', 'normal'],
            env=temporal_env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
        if temporal.returncode != 0 or b'exact=1' not in temporal.stdout:
            raise RuntimeError(f'{variant}: temporal-only decode failed\n{temporal.stdout.decode(errors="replace")}')
        for reference, expected in ((variant, 0), (other, 1)):
            result = subprocess.run(
                [str(analyzer), str(path), 'A', '2300', '16', '0.12', '0.30',
                 str(fixtures / f'{reference}.jp2'), 'short', 'adaptive-multistage', 'normal'],
                env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
            if result.returncode != expected or f'exact={1 - expected}'.encode() not in result.stdout:
                raise RuntimeError(f'{variant}/{reference}: unexpected result\n{result.stdout.decode(errors="replace")}')
        print(f'PASS {variant}: temporal-only, merged and crossed-reference checks', flush=True)
