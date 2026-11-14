#!/usr/bin/env python3
"""Non-mutating macOS startup/idle benchmark; not a decode or RF test.

Run on an otherwise idle machine. Each run uses fresh bridge test settings and
allows only loopback network traffic. RSS/CPU samples exclude child processes.
Raw logs and executable/source identities are retained for later A/B comparison.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import secrets
import shutil
import socket
import subprocess
import tempfile
import threading
import time

LAB_SANDBOX_POLICY = ('(version 1)(allow default)(deny network*)(allow network-bind)'
          '(deny process-exec (literal "/usr/bin/security"))'
          '(allow network-inbound (local ip "localhost:*"))'
          '(allow network-outbound (remote ip "localhost:*"))')

ROOT = Path(__file__).resolve().parents[2]
DIAGNOSTIC_LOG = Path.home() / 'Library/Application Support/IU8LMC/Decodium/decodium_diagnostic.log'
EVENTS = (
    'main() START', 'bridge constructing', 'bridge OK', 'window visible at',
    'starting Main.qml load at', 'Main.qml created as top-level',
    'waterfall visual stage ready', 'live map visual stage ready',
)


def collect_diagnostics(rig, output, name):
    """Qt routes performance telemetry to the asynchronous log, not stderr.

    Read rotated files too, but retain only this random test identity's segment.
    This benchmark must run without other Decodium instances (shared logger).
    """
    roots = [Path.home() / '.qttest/Library/Application Support/IU8LMC/Decodium/decodium_diagnostic.log',
             DIAGNOSTIC_LOG]
    marker = f'Application identity: Decodium - {rig} - test'
    for root in roots:
        paths = [root.with_name(f'decodium_diagnostic.{n}.log') for n in (3, 2, 1)] + [root]
        text = ''.join(path.read_text(errors='replace') for path in paths if path.is_file())
        position = text.find(marker)
        if position >= 0:
            break
    else:
        return ''
    start = text.rfind('\n', 0, position) + 1
    end = text.find('Application identity:', position + len(marker))
    segment = text[start:end if end >= 0 else len(text)]
    (output / f'{name}.diagnostic.log').write_text(segment)
    return segment


def _run_case(binary, output, backend, repetition, seconds, headless=False, large_window=False):
    rig = 'PerfBaseline' + secrets.token_hex(6)
    name = f'{backend}-{repetition}'
    legacy_ini = Path.home() / '.qttest/Library/Preferences/decodium4.ini'
    legacy_ini_before = hashlib.sha256(legacy_ini.read_bytes()).hexdigest() if legacy_ini.is_file() else None
    events, samples, metrics = {}, [], []
    udp_failure = threading.Event()
    timed_out = False
    with tempfile.TemporaryDirectory(prefix='decodium-perf-') as settings:
        if large_window:
            profile = Path(settings) / 'Decodium/Decodium3.ini'
            profile.parent.mkdir(parents=True, exist_ok=True)
            profile.write_text(f'[{rig}]\n_ProfileSettingsInitialized=true\n'
                               'WindowState\\mainWindow\\width=1600\n'
                               'WindowState\\mainWindow\\height=900\n'
                               'WorldMapDisplayed=true\nLiveMapUseGpu=true\n'
                               'uiWaterfallHeight=180\n')
        env = os.environ.copy()
        # Do not inherit developer decoder/graphics overrides accidentally.
        for key in list(env):
            if key.startswith(('DECODIUM_', 'QT_', 'QSG_', 'QML_', 'OMP_')):
                del env[key]
        env['DECODIUM_LAB_SETTINGS_DIR'] = settings
        if headless:
            env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
        if backend == 'native':
            env['DECODIUM_FORCE_STANDALONE_UDP'] = '1'
        command = [str(binary), '--test-mode', '--rig-name', rig,
                   '--lab-no-cat', '--lab-no-monitor', '--lab-mode', 'FT8',
                   '--lab-callsign', 'IU8LMC', '--lab-grid', 'JM75',
                   '--lab-quit-ms', str(seconds * 1000)]
        # Qt binds UDP before sending heartbeats. Denying bind causes repeated
        # unbound-socket warnings and invalidates startup timings. Permit bind
        # and localhost traffic; external network access remains denied.
        policy = LAB_SANDBOX_POLICY
        command = ['sandbox-exec', '-p', policy] + command
        start = time.monotonic()
        with (output / f'{name}.log').open('w') as log:
            process = subprocess.Popen(command, cwd=ROOT, env=env,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, errors='replace')

            def read_log():
                for line in process.stdout:
                    log.write(line)
                    if 'QNativeSocketEngine::writeDatagram() was called not in' in line:
                        udp_failure.set()
                    elapsed = round(time.monotonic() - start, 3)
                    for key in EVENTS:
                        if key in line and key not in events:
                            events[key] = elapsed
                    if any(tag in line for tag in ('STARTUP-TIMING', 'MAINWATCH', 'MAIN-TL',
                                                   'initialized lazily', 'mode-change decoder')):
                        metrics.append({'seconds': elapsed, 'text': line.strip()})

            reader = threading.Thread(target=read_log, daemon=True)
            reader.start()
            try:
                while process.poll() is None:
                    elapsed = time.monotonic() - start
                    if udp_failure.is_set():
                        process.terminate()
                        break
                    if elapsed > seconds + 30:
                        timed_out = True
                        process.terminate()
                        break
                    result = subprocess.run(['ps', '-p', str(process.pid), '-o',
                                             'rss=,%cpu=,time='], capture_output=True, text=True,
                                            timeout=5)
                    fields = result.stdout.split()
                    if len(fields) == 3:
                        samples.append({'seconds': round(elapsed, 3),
                                        'rss_kib': int(fields[0]),
                                        'ps_cpu_percent': float(fields[1]),
                                        'cpu_time_seconds': sum(float(part) * (60 ** i)
                                            for i, part in enumerate(reversed(fields[2].split(':'))))})
                    time.sleep(0.5)
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    timed_out = True
                    process.kill()
                    process.wait(timeout=5)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)
                reader.join(timeout=5)
                process.stdout.close()
        # Delete only directories owned by this invocation's random profile.
        for category in ('Application Support', 'Caches'):
            owned = Path.home() / '.qttest/Library' / category / 'IU8LMC' / f'Decodium - {rig} - test'
            if owned.is_dir():
                shutil.rmtree(owned)
    log_text = (output / f'{name}.log').read_text()
    diagnostics = collect_diagnostics(rig, output, name)
    combined = log_text + diagnostics
    udp_errors = combined.count('QNativeSocketEngine::writeDatagram() was called not in')
    backend_verified = ('Legacy TX backend disabled by DECODIUM_FORCE_STANDALONE_UDP' in combined
                        if backend == 'native' else
                        'legacy_backend_ms=' in combined and 'available= true' in combined)
    result = {'backend_requested': backend, 'repetition': repetition,
              'legacy_test_ini_sha256_before': legacy_ini_before,
              'legacy_test_ini_sha256_after': hashlib.sha256(legacy_ini.read_bytes()).hexdigest() if legacy_ini.is_file() else None,
              'backend_verified': backend_verified,
              'diagnostics_captured': bool(diagnostics),
              'unbound_udp_errors': udp_errors, 'headless': headless,
              'wall_seconds': round(time.monotonic() - start, 3),
              'exit': process.returncode, 'timed_out': timed_out,
              'nonempty_gpu_map_frame': '[WorldMapGpuItem] first frame renderer=QSG' in combined,
              'large_window': large_window,
              'valid_startup': (not large_window or '[WorldMapGpuItem] first frame renderer=QSG' in combined)
              and process.returncode == 0 and not timed_out
              and 'Main.qml created as top-level' in events
              and '[LAB] CAT auto-connect disabled' in log_text and backend_verified
              and udp_errors == 0,
              'events_seconds': events, 'metrics': metrics, 'samples': samples,
              'peak_sampled_rss_mib': round(max((s['rss_kib'] for s in samples), default=0) / 1024, 2)}
    (output / f'{name}.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('samples', 'metrics')}), flush=True)
    return result['valid_startup']


def run_case(binary, output, backend, repetition, seconds, headless=False, large_window=False):
    # Bridge settings are isolated but legacy defaults use this shared *test*
    # INI. Restore it after each run, including failed launches, so repetitions
    # start from identical defaults and do not leave altered test preferences.
    legacy_ini = Path.home() / '.qttest/Library/Preferences/decodium4.ini'
    original = legacy_ini.read_bytes() if legacy_ini.is_file() else None
    sinks = []
    try:
        for port in (2237, 2239):
            sink = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sinks.append(sink)
            sink.bind(('127.0.0.1', port))
        legacy_ini.parent.mkdir(parents=True, exist_ok=True)
        legacy_ini.write_text('')
        return _run_case(binary, output, backend, repetition, seconds, headless, large_window)
    finally:
        for sink in sinks:
            sink.close()
        if original is None:
            legacy_ini.unlink(missing_ok=True)
        else:
            legacy_ini.parent.mkdir(parents=True, exist_ok=True)
            legacy_ini.write_bytes(original)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/decodium')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runs', type=int, default=3)
    parser.add_argument('--seconds', type=int, default=20)
    parser.add_argument('--large-window', action='store_true',
                        help='Request 1600x900 and require a nonempty GPU-map frame')
    parser.add_argument('--headless', action='store_true',
                        help='Offscreen smoke test only; not a GUI performance baseline')
    parser.add_argument('--backends', nargs='+', choices=('native', 'legacy'), default=['native', 'legacy'])
    args = parser.parse_args()
    if platform.system() != 'Darwin' or not shutil.which('sandbox-exec'):
        parser.error('This harness currently requires macOS and sandbox-exec')
    if args.runs < 1 or args.seconds < 10:
        parser.error('Use at least one run and ten seconds')
    active = subprocess.run(['pgrep', '-x', 'decodium.*|Decodium4'], capture_output=True)
    if active.returncode == 0:
        parser.error('Close other Decodium instances before running this isolated benchmark')
    binary = args.binary.resolve(strict=True)
    args.output.mkdir(parents=True, exist_ok=False)
    qml_digest = hashlib.sha256()
    qml_root = binary.parent / 'qml'
    for path in sorted(qml_root.rglob('*')):
        if path.is_file() and path.suffix in ('.qml', '.js', '.qmltypes'):
            qml_digest.update(path.relative_to(qml_root).as_posix().encode() + b'\0')
            qml_digest.update(path.read_bytes())
    metadata = {'binary': str(binary), 'sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                'qml_source_sha256': qml_digest.hexdigest() if qml_root.is_dir() else None,
                'source_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                'source_status': subprocess.check_output(['git', 'status', '--short'], cwd=ROOT, text=True),
                'platform': platform.platform(), 'machine': platform.machine(),
                'logical_cpus': os.cpu_count(), 'seconds': args.seconds,
                'headless': args.headless, 'large_window': args.large_window,
                'layout_profile': {'width': 1600, 'height': 900, 'waterfall_height': 180} if args.large_window else None,
                'scope': 'startup/idle; monitor and CAT off; loopback only; main-process samples; fresh bridge and legacy test defaults; credential executable denied; local UDP sinks; default QML cache'}
    (args.output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    valid = True
    for repetition in range(1, args.runs + 1):
        for backend in args.backends:
            valid = run_case(binary, args.output, backend, repetition, args.seconds, args.headless, args.large_window)
            if not valid:
                return 1  # Do not repeat a noisy or invalid scenario.
    return 0 if valid else 1


if __name__ == '__main__':
    raise SystemExit(main())
