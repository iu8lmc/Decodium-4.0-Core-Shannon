#!/usr/bin/env python3
"""Profile native FT8/FT4/FT2 RX using a repeatable BlackHole audio fixture (macOS).

CAT is disabled. No audio is routed to physical devices. Stack sampling is
optional and must be disabled when comparing elapsed performance measurements.
No application source or QML is edited. Run without other Decodium instances.
"""
import argparse
import hashlib
import json
import math
import os
import re
from pathlib import Path
import secrets
import shutil
import socket
import sqlite3
import subprocess
import tempfile
import threading
import time
import wave

import numpy as np
import sounddevice as sd
from measure_runtime_baseline import collect_diagnostics, LAB_SANDBOX_POLICY as POLICY

ROOT = Path(__file__).resolve().parents[2]



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/decodium')
    parser.add_argument('--wav', type=Path, default=ROOT / 'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=75)
    parser.add_argument('--slots', type=int, default=3)
    parser.add_argument('--mode', choices=('FT8', 'FT4', 'FT2'), default='FT8')
    parser.add_argument('--sample', action='store_true')
    parser.add_argument('--expected-message', default='CQ K1ABC FN42')
    parser.add_argument('--ft-threads', type=int, choices=range(1, 9))
    parser.add_argument('--low-cpu', action='store_true')
    args = parser.parse_args()
    period = {'FT8': 15, 'FT4': 7.5, 'FT2': 3.75}[args.mode]
    if args.slots < 1 or args.seconds < args.slots * period + 30:
        parser.error(f'Allow 30 seconds plus {period} seconds per injected slot')
    if subprocess.run(['pgrep', '-x', 'decodium.*|Decodium4'], capture_output=True).returncode == 0:
        parser.error('Another Decodium instance is running')
    devices = sd.query_devices()
    output_device = next(i for i, d in enumerate(devices)
                         if d['name'] == 'BlackHole 2ch' and d['max_output_channels'] >= 2)
    assert any(d['name'] == 'BlackHole 16ch' for d in devices), 'BlackHole 16ch required'
    with wave.open(str(args.wav), 'rb') as wav:
        assert wav.getnchannels() == 1 and wav.getsampwidth() == 2
        rate = wav.getframerate()
        frame = np.frombuffer(wav.readframes(wav.getnframes()), dtype='<i2').astype(np.float32) / 32768
    assert rate == 12000 and len(frame) == int(period * rate), f'Expected a {period}-second 12 kHz fixture'
    # Integer-rate interpolation happens once, outside the measured app run.
    frame48 = np.interp(np.arange(len(frame) * 4) / 4,
                        np.arange(len(frame)), frame).astype(np.float32)
    stereo = np.repeat(frame48[:, None], 2, axis=1)
    playback = np.tile(stereo, (args.slots, 1))
    binary = args.binary.resolve(strict=True)
    args.output.mkdir(parents=True, exist_ok=False)
    rig = 'PerfRx' + secrets.token_hex(6)
    owned = Path.home() / '.qttest/Library/Application Support/IU8LMC' / f'Decodium - {rig} - test'
    legacy_ini = Path.home() / '.qttest/Library/Preferences/decodium4.ini'
    legacy_files = [legacy_ini, legacy_ini.with_name('ft2.ini')]
    original_inis = {p: p.read_bytes() if p.exists() else None for p in legacy_files}
    failure = threading.Event()
    samples, messages, sampling_processes = [], [], []
    udp_sinks = []
    process = None
    reader = None
    started_audio = None
    timed_out = False
    try:
        # Consume the default reporting destinations inside the lab. If a real
        # application already owns one, fail before launching the test receiver.
        for port in (2237, 2239):
            sink = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            udp_sinks.append(sink)
            sink.bind(('127.0.0.1', port))
        # Earlier laboratory runs may have left a blacklist or user filters in
        # the shared test INI. Start from controlled defaults, restoring the
        # exact original bytes in finally. Production settings are untouched.
        legacy_ini.parent.mkdir(parents=True, exist_ok=True)
        for path in legacy_files:
            path.write_text('')
        with tempfile.TemporaryDirectory(prefix='decodium-rx-profile-') as settings:
            profile_ini = Path(settings) / 'Decodium/Decodium3.ini'
            profile_ini.parent.mkdir(parents=True, exist_ok=True)
            profile = [f'[{rig}]', '_ProfileSettingsInitialized=true',
                       f'LowCpuMode={str(args.low_cpu).lower()}']
            if args.ft_threads is not None:
                profile += ['ftThreadsAuto=false', f'ftThreads={args.ft_threads}']
            profile_ini.write_text('\n'.join(profile) + '\n')
            env = {k: v for k, v in os.environ.items()
                   if not k.startswith(('DECODIUM_', 'QT_', 'QSG_', 'QML_', 'OMP_'))}
            env.update(DECODIUM_LAB_SETTINGS_DIR=settings, DECODIUM_FORCE_STANDALONE_UDP='1')
            command = ['sandbox-exec', '-p', POLICY, str(binary), '--test-mode',
                       '--rig-name', rig, '--lab-no-cat', '--lab-mode', args.mode,
                       '--lab-callsign', 'IU8LMC', '--lab-grid', 'JM75',
                       '--lab-audio-input', 'BlackHole 2ch', '--lab-audio-output', 'BlackHole 16ch',
                       '--lab-monitor-ms', '3000', '--lab-quit-ms', str(args.seconds * 1000)]
            start = time.monotonic()
            audio_start_at = (math.floor((time.time() + 10) / period) + 1) * period
            with (args.output / 'stdout.log').open('w') as log:
                process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=subprocess.PIPE,
                                           stderr=subprocess.STDOUT, text=True, errors='replace')

                def read_output():
                    for line in process.stdout:
                        log.write(line)
                        if 'QNativeSocketEngine::writeDatagram() was called not in' in line:
                            failure.set()

                reader = threading.Thread(target=read_output, daemon=True)
                reader.start()
                while process.poll() is None:
                    elapsed = time.monotonic() - start
                    if failure.is_set() or elapsed > args.seconds + 20:
                        timed_out = elapsed > args.seconds + 20
                        process.terminate()
                        break
                    if started_audio is None and time.time() >= audio_start_at:
                        started_audio = time.time()
                        sd.play(playback, samplerate=48000, device=output_device)
                    if args.sample and started_audio and not sampling_processes and time.time() - started_audio > 12:
                        sampling_processes.append(subprocess.Popen(
                            ['sample', str(process.pid), '10', '10', '-file', str(args.output / 'threads.sample.txt')],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
                    ps = subprocess.run(['ps', '-p', str(process.pid), '-o', 'rss=,%cpu='],
                                        capture_output=True, text=True, timeout=5)
                    values = ps.stdout.split()
                    if len(values) == 2:
                        samples.append({'seconds': round(elapsed, 3), 'rss_kib': int(values[0]),
                                        'ps_cpu_percent': float(values[1])})
                    # Short polling around the start boundary limits injection jitter.
                    remaining = audio_start_at - time.time()
                    time.sleep(min(0.25, max(0.002, remaining)) if remaining > 0 else 0.25)
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    timed_out = True
                    process.kill()
                    process.wait(timeout=5)
                reader.join(timeout=5)
            sd.stop()
            for sampler in sampling_processes:
                sampler.wait(timeout=15)
            diagnostics = collect_diagnostics(rig, args.output, 'rx')
            database = owned / 'db.sqlite'
            if database.is_file():
                with sqlite3.connect('file:' + str(database) + '?mode=ro', uri=True) as db:
                    columns = [r[1] for r in db.execute('pragma table_info(decodes)')]
                    selected = [c for c in ('id', 'ts_utc', 'mode', 'message', 'snr_db', 'dt_s', 'freq_hz', 'confidence') if c in columns]
                    messages = [dict(zip(selected, row)) for row in db.execute('select ' + ','.join(selected) + ' from decodes order by id')]
            log_text = (args.output / 'stdout.log').read_text()
            dispatch_limits = [int(n) for n in re.findall(r'\[FT8DISPATCH\].*?threads=\d+/(\d+)', log_text)]
            if args.mode in ('FT2', 'FT4'):
                pattern = r'\[DECODEMETRIC\] mode=' + args.mode + r'(?:-async)? .*?threads_req=(\d+)'
                dispatch_limits = [int(n) for n in re.findall(pattern, log_text)]
            diagnostic_text = (args.output / 'rx.diagnostic.log').read_text() if diagnostics else ''
            startup_profile = re.search(r'runtime CPU profile: low=(\d+) ft_threads=(\d+).*reason=startup', diagnostic_text)
            profile_verified = (bool(dispatch_limits) and startup_profile is not None
                                and int(startup_profile[1]) == int(args.low_cpu)
                                and (args.ft_threads is None or
                                     (int(startup_profile[2]) == min(args.ft_threads, 2 if args.low_cpu else args.ft_threads)
                                      and all(n <= args.ft_threads for n in dispatch_limits))))
            (args.output / 'final-settings.ini').write_text('\n'.join(
                line for line in profile_ini.read_text().splitlines()
                if line.startswith(('ftThreads=', 'ftThreadsAuto=', 'LowCpuMode='))))
            valid = (profile_verified and process.returncode == 0 and not timed_out and not failure.is_set()
                     and '[LAB] CAT auto-connect disabled' in log_text
                     and any(args.expected_message == r.get('message', '').strip() for r in messages))
            result = {'valid': valid, 'exit': process.returncode, 'timed_out': timed_out,
                      'udp_failure': failure.is_set(), 'audio_start_offset_in_slot_s': started_audio % period if started_audio else None,
                      'audio_start_epoch_s': started_audio,
                      'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                      'fixture_sha256': hashlib.sha256(args.wav.read_bytes()).hexdigest(),
                      'playback_resampler': 'fixed linear interpolation 12000 to 48000 Hz',
                      'settings': 'fresh bridge and legacy test defaults, CLI identity/audio overrides',
                      'credential_isolation': 'macOS security executable denied by sandbox',
                      'requested_profile': {'ft_threads': args.ft_threads, 'low_cpu': args.low_cpu},
                      'observed_dispatch_limits': dispatch_limits,
                      'profile_verified': profile_verified,
                      'injected_slots': args.slots, 'mode': args.mode, 'period_seconds': period,
                      'stack_sampling': args.sample, 'messages': messages, 'samples': samples,
                      'diagnostics_captured': bool(diagnostics), 'scope': f'native {args.mode} virtual-audio RX, no RF'}
            (args.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
            summary = {k: v for k, v in result.items() if k not in ('samples', 'messages')}
            summary['decoded_rows'] = len(messages)
            summary['unique_messages'] = sorted({r['message'] for r in messages})
            print(json.dumps(summary), flush=True)
            return 0 if valid else 1
    finally:
        sd.stop()
        if process and process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        if reader:
            reader.join(timeout=5)
        for sampler in sampling_processes:
            if sampler.poll() is None:
                sampler.terminate()
                sampler.wait(timeout=5)
        for sink in udp_sinks:
            sink.close()
        for path, original in original_inis.items():
            if original is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(original)
            assert (path.read_bytes() if path.exists() else None) == original
        for directory in (owned, Path.home() / '.qttest/Library/Caches/IU8LMC' / f'Decodium - {rig} - test'):
            if directory.is_dir():
                shutil.rmtree(directory)


if __name__ == '__main__':
    raise SystemExit(main())
