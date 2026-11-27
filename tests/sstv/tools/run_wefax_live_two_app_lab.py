#!/usr/bin/env python3
"""Live Decodium pair plus independent WEFAX receiver on macOS/BlackHole."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

if len(sys.argv) != 3 or sys.argv[1] not in ('a', 'b'):
    raise SystemExit('expected a|b and independent Python executable')
variant = sys.argv[1]
oracle_python = Path(sys.argv[2]).expanduser().absolute()
if not oracle_python.is_file():
    raise SystemExit('independent Python executable does not exist')
repo = Path.cwd()
checkout = Path(os.environ.get('WEFAX_ORACLE_CHECKOUT', '/tmp/decodium-wefax-independent')).resolve(strict=True)
expected = '99bfc22824dc970276d7c70b984df578bb7b6b11'
revision = subprocess.check_output(['git', '-C', str(checkout), 'rev-parse', 'HEAD'], text=True).strip()
dirty = subprocess.check_output(['git', '-C', str(checkout), 'status', '--porcelain'], text=True).strip()
if revision != expected or dirty:
    raise SystemExit('independent WEFAX checkout must be clean and at ' + expected)
source = Path(os.environ.get('WEFAX_LAB_IMAGE',
              str(repo / 'tests/sstv/fixtures' / ('radiofax-structured-' + variant + '.ppm')))).resolve(strict=True)
root = Path(tempfile.mkdtemp(prefix='decodium-wefax-live-lab-' + variant + '-'))
print('artifacts', root, flush=True)
env = os.environ.copy()
env.update(OPENBLAS_NUM_THREADS='1', QML_DISABLE_DISK_CACHE='1')
cmd = [str(oracle_python), str(checkout / 'wefax_decoder.py'), '--device', 'BlackHole 2ch',
       '--out', str(root / 'oracle'), '--port', os.environ.get('WEFAX_LAB_PORT', '18809'),
       '--align', 'center', '--no-browser', '--no-ml']
oracle = None
try:
    with (root / 'oracle-console.log').open('wb') as output:
        oracle = subprocess.Popen(cmd, cwd=checkout, env=env, stdout=output, stderr=subprocess.STDOUT)
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        log = (root / 'oracle-console.log').read_text(errors='replace')
        if 'listening - Ctrl+C to stop' in log and 'input: BlackHole 2ch' in log:
            break
        if oracle.poll() is not None:
            raise RuntimeError('independent receiver exited before listening')
        time.sleep(.2)
    else:
        raise RuntimeError('independent receiver did not open BlackHole')
    env.update(DECODIUM_LAB_FAX_CAPTURE='1', DECODIUM_LAB_RADIOFAX_WMO_DEAD_SECTOR='1',
               DECODIUM_LAB_RADIOFAX_WMO_TIMED='1', DECODIUM_LAB_RADIOFAX_IOC='576',
               DECODIUM_LAB_RADIOFAX_IMAGE=str(source))
    with (root / 'decodium.log').open('wb') as output:
        result = subprocess.run([sys.executable, str(repo / 'tests/sstv/tools/run_radiofax_centered_two_app_lab.py'),
                                 variant, '120'], cwd=repo, env=env, stdout=output, stderr=subprocess.STDOUT,
                                timeout=180)
    if result.returncode or oracle.poll() is not None:
        raise RuntimeError('Decodium pair or independent receiver failed')
    text = (root / 'decodium.log').read_text(errors='replace')
    matches = re.findall(r'PASS two Decodium apps:.*?artifacts (\S+)', text)
    if len(matches) != 1:
        raise RuntimeError('missing Decodium pair success evidence')
    decodium = Path(matches[0])
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        metadata = list((root / 'oracle').glob('wefax_*.json'))
        if metadata:
            break
        if oracle.poll() is not None:
            raise RuntimeError('independent receiver exited before saving')
        time.sleep(.2)
    else:
        raise RuntimeError('independent receiver did not save a page')
    if len(metadata) != 1:
        raise RuntimeError('unexpected number of independent pages')
    result = json.loads(metadata[0].read_text())
    m = result.get('meta', {})
    if (result.get('trigger') != 'start tone' or result.get('end_reason') != 'stop tone'
            or result.get('audio_overflows') != 0 or m.get('gaps_filled_ms') != []
            or m.get('ioc') != 576 or m.get('lpm') != 120 or m.get('lines') != 120
            or not m.get('start_tone') or not m.get('stop_tone')):
        raise RuntimeError('independent reception metadata failed: ' + str(metadata[0]))
    raw = root / 'oracle' / result['files']['raw']
    command = [sys.executable, str(repo / 'tests/sstv/tools/analyze_radiofax_dead_sector.py'),
               str(decodium / 'reference.png'), str(raw), '--row', '0', '--start', '41']
    comparison = json.loads(subprocess.check_output(command, cwd=repo, text=True))
    metrics = comparison['posthoc_full_width']
    (root / 'comparison.json').write_text(json.dumps(comparison, indent=2) + '\n')
    if metrics['mae'] > 8 or metrics['p95'] > 15:
        raise RuntimeError('independent image did not meet 8/15 gate: ' + str(metrics))
    print('PASS live independent WEFAX receiver:', variant, metrics, 'Decodium', decodium, flush=True)
finally:
    if oracle and oracle.poll() is None:
        oracle.terminate()
        try:
            oracle.wait(timeout=10)
        except subprocess.TimeoutExpired:
            oracle.kill()
            oracle.wait()
