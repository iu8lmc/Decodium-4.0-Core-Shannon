#!/usr/bin/env python3
"""A/B sustained invalid-MSC controls for the native waveform receiver."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

exporter, analyzer, reference = map(Path, sys.argv[1:])
with tempfile.TemporaryDirectory(prefix='decodium-hamdrm-crc-stress-') as directory:
    root = Path(directory)
    for depth in ('short', 'long'):
        native = root / (depth + '.s16')
        downsampled = root / (depth + '-12k.s16')
        subprocess.run([str(exporter), depth, str(native)], check=True, timeout=30)
        pcm = native.read_bytes()
        assert len(pcm) % 8 == 0 and len(pcm) >= 48_000 * 2 * 20
        # This generated band-limited test signal is below the 12 kHz Nyquist
        # frequency. Take every fourth signed 16-bit sample, without changing
        # or filtering the retained original 48 kHz fixture.
        downsampled.write_bytes(b''.join(pcm[i:i+2] for i in range(0, len(pcm), 8)))
        for equalizer in ('flat', 'flat-multistage'):
            result = subprocess.run([str(analyzer), str(downsampled), 'E', '2500',
                                     '64', '0.12', '0.30', str(reference), depth,
                                     equalizer, 'normal'], capture_output=True, text=True,
                                    timeout=30)
            if result.returncode != 1:
                raise RuntimeError('unexpected analyzer exit: ' + result.stdout + result.stderr)
            fields = dict(re.findall(r'(\w+)=(\d+)', result.stdout))
            if (int(fields.get('packet_failures', '0')) < 50
                or fields.get('groups') != '0' or fields.get('exact') != '0'):
                raise RuntimeError('stress input did not meet the CRC/group control: ' + result.stdout)
print('PASS sustained CRC stress: short/long, retry off/on')
