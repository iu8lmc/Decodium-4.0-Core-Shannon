#!/usr/bin/env python3
"""Check A/B independently decoded QSSTV-source 64-QAM signals."""
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
    'a': '94d239f403c5d02c36961a8ac4ad5a919579310c6b55ee345161e2f73ae40f52',
    'b': '0f2fc67bd876ed502eb540a4a92fd3bcb5691f03e15eee6b7f66b6f7d8527ddf',
}
env = {key: value for key, value in os.environ.items()
       if not key.startswith('DECODIUM_HAMDRM_')}
with tempfile.TemporaryDirectory(prefix='decodium-qsstv-a25-regression-') as directory:
    for variant, other in (('a', 'b'), ('b', 'a')):
        pcm = zlib.decompress((fixtures / f'{variant}.s16.zlib').read_bytes())
        if hashlib.sha256(pcm).hexdigest() != hashes[variant]:
            raise RuntimeError(f'{variant}: fixture hash mismatch')
        path = Path(directory) / f'{variant}.s16'
        path.write_bytes(pcm)
        for reference, expected in ((variant, 0), (other, 1)):
            result = subprocess.run(
                [str(analyzer), str(path), 'A', '2500', '64', '0.12', '0.30',
                 str(fixtures / f'{reference}.jp2'), 'short', 'flat-multistage', 'normal'],
                env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
            if result.returncode != expected or f'exact={1 - expected}'.encode() not in result.stdout:
                raise RuntimeError(f'{variant}/{reference}: unexpected result\n{result.stdout.decode(errors="replace")}')
        print(f'PASS {variant}: exact payload and crossed-reference rejection', flush=True)
