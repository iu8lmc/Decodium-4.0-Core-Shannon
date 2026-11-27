#!/usr/bin/env python3
"""Compare two externally sourced EasyPal-labelled captures with QSSTV objects.

Files are not distributed here. The uploader of A attributes it to EasyPal;
B is advertised as an EasyPal reception test. Neither supplies a verified
transmitter version or original source image. See HAMDRM_COMPATIBILITY_MATRIX.
"""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('wav_a', type=Path)
parser.add_argument('reference_a', type=Path)
parser.add_argument('wav_b', type=Path)
parser.add_argument('reference_b', type=Path)
parser.add_argument('--analyzer', type=Path, default=Path('build/tests/sstv/analyze_hamdrm_capture'))
parser.add_argument('--live', action='store_true', help='also replay both WAVs into the full app via BlackHole')
parser.add_argument('--gallery-a', type=Path, help='optional Gallery PNG from the A application run')
parser.add_argument('--gallery-b', type=Path, help='optional Gallery PNG from the B application run')
args = parser.parse_args()
expected = {
    'wav_a': '5ba69af79820c9bbe068d685e94866810384ba2686b22326c1e50b0a7bb0219f',
    'reference_a': '55fa95f664d82cf0d0cb5a57a545cb661e071aa3c33de431cedb9db7960a62fd',
    'wav_b': 'c342ca241620c5278197b8604e42e54d71eafbb3300134100c72409dbd37074d',
    'reference_b': 'a2642ca6c2244134bc04ad7d6922465200274d82b56bbf0a71120862ad65e38a',
}
paths = {}
for name, digest in expected.items():
    path = getattr(args, name).resolve(strict=True)
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise SystemExit('SHA-256 mismatch for ' + name)
    paths[name] = path
analyzer = args.analyzer.resolve(strict=True)
if bool(args.gallery_a) != bool(args.gallery_b):
    parser.error('--gallery-a and --gallery-b must be supplied together')
base_env = os.environ.copy()
for key in ('DECODIUM_HAMDRM_TEMPORAL_MERGE', 'DECODIUM_HAMDRM_TEMPORAL_PILOTS',
            'DECODIUM_HAMDRM_TRACE', 'DECODIUM_HAMDRM_OBJECT_OUTPUT'):
    base_env.pop(key, None)
base_env['DECODIUM_HAMDRM_FRAME_TRACKING'] = '1'
profiles = {'a': ('A', '64'), 'b': ('B', '16')}
with tempfile.TemporaryDirectory(prefix='decodium-public-easypal-check-') as directory:
    for variant in ('a', 'b'):
        pcm = Path(directory) / (variant + '.s16')
        subprocess.run(['ffmpeg', '-v', 'error', '-i', str(paths['wav_' + variant]),
                        '-ac', '1', '-ar', '12000', '-f', 's16le', str(pcm)],
                       check=True, timeout=30)
        mode, qam = profiles[variant]
        for reference in (variant, 'b' if variant == 'a' else 'a'):
            cmd = [str(analyzer), str(pcm), mode, '2500', qam, '0.12', '0.30',
                   str(paths['reference_' + reference]), 'long', 'flat-multistage', 'high']
            result = subprocess.run(cmd, env=base_env, capture_output=True, text=True, timeout=60)
            expected_code = 0 if reference == variant else 1
            if (result.returncode != expected_code or 'exact=' + str(int(reference == variant)) not in result.stdout
                    or 'ingest_errors=0' not in result.stdout):
                raise RuntimeError('offline %s/%s failed:\n%s\n%s' %
                                   (variant, reference, result.stdout, result.stderr))
        print('PASS', variant, 'exact and crossed-reference negative', flush=True)
        gallery = getattr(args, 'gallery_' + variant)
        if gallery:
            def pixel_hash(path):
                result = subprocess.run(['ffmpeg', '-v', 'error', '-i', str(path), '-map', '0:v:0',
                                         '-frames:v', '1', '-pix_fmt', 'rgb24', '-f', 'hash',
                                         '-hash', 'sha256', '-'], capture_output=True, text=True,
                                        check=True, timeout=60)
                return result.stdout.strip()
            if pixel_hash(gallery.resolve(strict=True)) != pixel_hash(paths['reference_' + variant]):
                raise RuntimeError('Gallery pixels differ for ' + variant)
            print('PASS', variant, 'Gallery RGB pixels match QSSTV reference', flush=True)
        if args.live:
            env = os.environ.copy()
            env['DECODIUM_HAMDRM_LAB_PROFILE'] = 'hamdrm-%s-2500-high-%s-QAM-long' % (mode, qam)
            env['DECODIUM_HAMDRM_LAB_DURATION_MS'] = '270000' if variant == 'a' else '125000'
            harness = Path(__file__).with_name('run_qsstv_exp_drm_app_lab.py')
            subprocess.run([sys.executable, str(harness), str(paths['wav_' + variant]),
                            str(paths['reference_' + variant])], env=env, check=True, timeout=330)
