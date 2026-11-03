#!/usr/bin/env python3
"""A/B independent QSSTV samples and a wrong-reference negative control."""
from pathlib import Path
import array
import os
import random
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve(strict=True)
fixtures = Path(sys.argv[2]).resolve(strict=True)
exporter = Path(sys.argv[3]).resolve(strict=True)
with tempfile.TemporaryDirectory(prefix='decodium-hamdrm-analyze-') as directory:
    root = Path(directory)
    for variant in ('a', 'b'):
        native = (fixtures / f'qsstv-image-{variant}-8c27d6d.pcm').read_bytes()
        assert len(native) % 8 == 0
        pcm = root / f'{variant}.s16'
        # The pinned mono 48 kHz fixture is an exact 4x waveform; retaining
        # every fourth signed little-endian sample produces its 12 kHz form.
        pcm.write_bytes(b''.join(native[i:i + 2] for i in range(0, len(native), 8)))
        common = [str(binary), str(pcm), 'E', '2500', '16', '0.12', '0.30']
        result = subprocess.run(common + [str(fixtures / f'qsstv-image-{variant}.jpg'), 'short'],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        assert result.returncode == 0 and b'exact=1' in result.stdout, result.stdout + result.stderr
        other = 'b' if variant == 'a' else 'a'
        temporal_env = os.environ.copy()
        temporal_env.pop('DECODIUM_HAMDRM_TEMPORAL_PILOTS', None)
        temporal_env['DECODIUM_HAMDRM_TEMPORAL_MERGE'] = '1'
        for ref, expected_code in ((variant, 0), (other, 1)):
            result = subprocess.run(common + [str(fixtures / f'qsstv-image-{ref}.jpg'), 'short', 'adaptive-multistage'],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30, env=temporal_env)
            assert result.returncode == expected_code and f'exact={1 - expected_code}'.encode() in result.stdout, result.stdout + result.stderr
        result = subprocess.run(common + [str(fixtures / f'qsstv-image-{other}.jpg'), 'short'],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        assert result.returncode == 1 and b'exact=0' in result.stdout, result.stdout + result.stderr
        padded = root / f'{variant}-padded.jpg'
        padded.write_bytes((fixtures / f'qsstv-image-{variant}.jpg').read_bytes() + bytes(7))
        result = subprocess.run(common + [str(padded), 'short'],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        assert result.returncode == 1 and b'exact=0' in result.stdout and b'reference_zero_padding=7' in result.stdout, result.stdout + result.stderr
        padded.write_bytes((fixtures / f'qsstv-image-{variant}.jpg').read_bytes() + b'\x00\x01')
        result = subprocess.run(common + [str(padded), 'short'],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        assert result.returncode == 1 and b'exact=0' in result.stdout and b'reference_zero_padding=0' in result.stdout, result.stdout + result.stderr
        # Two deterministic paths with a 1 ms delayed echo; the hybrid
        # diagnostic may combine only groups accepted by each receiver.
        samples = array.array('h')
        samples.frombytes(pcm.read_bytes())
        if sys.byteorder != 'little': samples.byteswap()
        echo = array.array('h', (round(0.5 * (x + (0.3 * samples[i-12] if i >= 12 else 0)))
                                 for i, x in enumerate(samples)))
        if sys.byteorder != 'little': echo.byteswap()
        impaired = root / f'{variant}-echo.s16'
        impaired.write_bytes(echo.tobytes())
        echo_args = [str(binary), str(impaired), 'E', '2500', '16', '0.12', '0.30']
        for estimator in ('hybrid', 'adaptive', 'adaptive-fallback', 'adaptive-multistage', 'timing-hybrid', 'flat-multistage', 'flat-timing-hybrid', 'half-hybrid', 'sinc-hybrid'):
            result = subprocess.run(echo_args + [str(fixtures / f'qsstv-image-{variant}.jpg'), 'short', estimator],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
            assert result.returncode == 0 and b'exact=1' in result.stdout and b'ingest_errors=0' in result.stdout, result.stdout + result.stderr
            result = subprocess.run(echo_args + [str(fixtures / f'qsstv-image-{other}.jpg'), 'short', estimator],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
            assert result.returncode == 1 and b'exact=0' in result.stdout, result.stdout + result.stderr
        print('PASS independent QSSTV diagnostic', variant)

    for variant, interleaver in (('a', 'short'), ('b', 'long')):
        pcm = root / f'fallback-{variant}.s16'
        subprocess.run([str(exporter), interleaver, str(pcm)], check=True, timeout=30)
        common = [str(binary), str(pcm), 'A', '2500', '16', '0.12', '0.30',
                  str(fixtures / f'qsstv-image-{variant}.jpg'), interleaver]
        original = subprocess.run(common + ['adaptive', 'normal'], capture_output=True, timeout=30)
        recovered = subprocess.run(common + ['adaptive-fallback', 'normal'], capture_output=True, timeout=30)
        # These are valid body groups without headers, not complete images.
        assert original.returncode == 1 and (b'groups=1' in original.stdout or b'groups=2' in original.stdout), original.stdout + original.stderr
        assert (recovered.returncode == 1 and b'groups=2' in recovered.stdout
                and b'ingest_errors=0' in recovered.stdout), recovered.stdout + recovered.stderr
        if b'groups=1' in original.stdout:
            assert b'flat_fallback_packets=1' in recovered.stdout, recovered.stdout + recovered.stderr
        print('PASS CRC-gated flat fallback', variant)

    for seed in (0x12345678, 0x9abcdef0):
        rng = random.Random(seed)
        noise = array.array('h', (rng.randrange(-6000, 6001) for _ in range(8 * 4800)))
        if sys.byteorder != 'little': noise.byteswap()
        pcm = root / f'noise-{seed}.s16'
        pcm.write_bytes(noise.tobytes())
        for estimator in ('hybrid', 'adaptive', 'adaptive-fallback', 'adaptive-multistage', 'timing-hybrid', 'flat-multistage', 'flat-timing-hybrid', 'half-hybrid', 'sinc-hybrid'):
            result = subprocess.run([str(binary), str(pcm), 'E', '2500', '16', '0.40', '0.60',
                                     str(fixtures / 'qsstv-image-a.jpg'), 'long', estimator],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
            assert result.returncode == 1 and b'objects=0' in result.stdout and b'exact=0' in result.stdout, result.stdout + result.stderr
        for estimator in ('timing-hybrid', 'sinc-hybrid'):
            result = subprocess.run([str(binary), str(pcm), 'E', '2500', '16', '0.12', '0.30',
                                     str(fixtures / 'qsstv-image-b.jpg'), 'long', estimator],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
            assert result.returncode == 1 and b'objects=0' in result.stdout and b'exact=0' in result.stdout, result.stdout + result.stderr
        result = subprocess.run([str(binary), str(pcm), 'E', '2500', '16', '0.12', '0.30',
                                 str(fixtures / 'qsstv-image-a.jpg'), 'long', 'adaptive-multistage'],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30, env=temporal_env)
        assert result.returncode == 1 and b'objects=0' in result.stdout and b'exact=0' in result.stdout, result.stdout + result.stderr
        print('PASS hybrid noise rejection', seed)
