#!/usr/bin/env python3
"""Two full Decodium apps exchange two encrypted images via loopback HTTPS."""
import argparse
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import tempfile
import threading
import time

from run_share_app_identity_lab import keychains
from share_https_fixture import ShareFixture


def image_bytes(path):
    return subprocess.check_output(['ffmpeg', '-v', 'error', '-i', str(path),
        '-frames:v', '1', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'])


def image_size(path):
    result = subprocess.check_output(['ffprobe', '-v', 'error', '-select_streams', 'v:0',
        '-show_entries', 'stream=width,height', '-of', 'csv=s=x:p=0', str(path)])
    return result.strip()


def main():
    parser = argparse.ArgumentParser()
    attacks = parser.add_mutually_exclusive_group()
    attacks.add_argument('--tamper', action='store_true')
    attacks.add_argument('--tamper-manifest', choices=('a', 'b'))
    parser.add_argument('decodium', type=Path)
    parser.add_argument('certificate', type=Path)
    parser.add_argument('key', type=Path)
    parser.add_argument('image_a', type=Path)
    parser.add_argument('image_b', type=Path)
    args = parser.parse_args()
    binary = args.decodium.resolve(strict=True)
    cert, key = args.certificate.resolve(strict=True), args.key.resolve(strict=True)
    source_a, source_b = args.image_a.resolve(strict=True), args.image_b.resolve(strict=True)
    root = Path(tempfile.mkdtemp(prefix='decodium-share-transfer-'))
    security, core, refs = keychains([root / 'sender.keychain-db', root / 'receiver.keychain-db'])
    server = ShareFixture(cert, key, tamper=args.tamper, tamper_manifest=args.tamper_manifest,
                          first_output=(root / 'out' / 'image-a.png') if args.tamper_manifest == 'b' else None)
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    processes = []
    successful = False
    try:
        shutil.copyfile(source_a, root / 'image-a.jpg')
        shutil.copyfile(source_b, root / 'image-b.jpg')
        (root / 'out').mkdir()
        identity = str(root / 'receiver-public.json')
        common = {'endpoint': server.url, 'certificate': str(cert), 'identity': identity}
        sessions = {
            'receiver': dict(common, role='receiver', outputDir=str(root / 'out'),
                             result=str(root / 'receiver-ok')),
            'sender': dict(common, role='sender', recipient='labrx',
                           imageA=str(root / 'image-a.jpg'), imageB=str(root / 'image-b.jpg'),
                           result=str(root / 'sender-ok')),
        }
        invalid = root / 'invalid.json'
        invalid.write_text(json.dumps(dict(sessions['receiver'], endpoint='https://example.com:443/')))
        blocked = subprocess.run([str(binary), '--test-mode', '--lab-share-keychain',
            str(root / 'receiver.keychain-db'), '--lab-share-session-file', str(invalid)],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
        if blocked.returncode != 2 or (root / 'receiver-ok').exists():
            raise RuntimeError('session lab did not reject a non-loopback endpoint')
        suffix = secrets.token_hex(4)
        for role in ('receiver', 'sender'):
            config = root / (role + '.json')
            config.write_text(json.dumps(sessions[role]))
            env = os.environ.copy()
            env['QT_QUICK_BACKEND'] = 'software'
            env['DECODIUM_LAB_SETTINGS_DIR'] = str(root / (role + '-settings'))
            cmd = [str(binary), '--test-mode', '--rig-name', 'SHARE' + suffix + role,
                   '--lab-callsign', 'LABRX' if role == 'receiver' else 'LABTX',
                   '--lab-no-cat', '--lab-no-monitor', '--lab-share-keychain',
                   str(root / (role + '.keychain-db')), '--lab-share-session-file', str(config)]
            with open(root / (role + '.log'), 'wb') as log:
                processes.append(subprocess.Popen(cmd, env=env, stdout=log, stderr=subprocess.STDOUT))
        deadline = time.monotonic() + 105
        while True:
            states = [process.poll() for process in processes]
            if (args.tamper or args.tamper_manifest) and states[0] is not None:
                if states[0] != 2 or not server.tamper_count:
                    raise RuntimeError('tampered transfer was not rejected')
                if (root / 'receiver-ok').exists():
                    raise RuntimeError('tampered transfer produced a completion marker')
                accepted = {p.name for p in (root / 'out').iterdir()}
                if args.tamper_manifest == 'b':
                    if accepted != {'image-a.png'}:
                        raise RuntimeError('valid A missing or altered B was accepted')
                    reference, received = image_bytes(source_a), image_bytes(root / 'out' / 'image-a.png')
                    errors = [abs(a - b) for a, b in zip(reference, received)]
                    if len(reference) != len(received) or not errors or max(errors) > 5 or sum(errors) / len(errors) > 1:
                        raise RuntimeError('valid A was altered')
                elif accepted:
                    raise RuntimeError('tampered transfer produced accepted output')
                log = (root / 'receiver.log').read_text(errors='replace')
                if 'download failed: encrypted payload authentication or decryption failed' not in log:
                    raise RuntimeError('receiver did not report authenticated-open failure')
                successful = True
                print('PASS: two Decodium applications rejected altered {} with recomputed manifest digest'.format(
                    'manifest' if args.tamper_manifest else 'ciphertext'))
                return
            if any(code is not None and code != 0 for code in states):
                raise RuntimeError('Decodium sharing actor failed')
            if all(code == 0 for code in states):
                break
            if time.monotonic() >= deadline:
                raise RuntimeError('Decodium sharing actors timed out')
            time.sleep(0.2)
        if not (root / 'sender-ok').exists() or not (root / 'receiver-ok').exists():
            raise RuntimeError('missing sharing completion marker')
        for label, original in (('a', source_a), ('b', source_b)):
            output = root / 'out' / ('image-' + label + '.png')
            if image_size(original) != image_size(output):
                raise RuntimeError('received image dimensions differ')
            reference = image_bytes(original)
            received = image_bytes(output)
            if len(reference) != len(received) or not reference:
                raise RuntimeError('received image dimensions differ')
            errors = [abs(a - b) for a, b in zip(reference, received)]
            if max(errors) > 5 or sum(errors) / len(errors) > 1:
                raise RuntimeError('received image pixels differ')
            print('image {}: mean error {:.4f}, maximum {}'.format(
                label.upper(), sum(errors) / len(errors), max(errors)))
        with server.lock:
            items = list(server.uploads.values())
            if len(items) != 2 or not all(item['complete'] for item in items):
                raise RuntimeError('fixture did not receive two completed transfers')
            fingerprint = json.loads((root / 'receiver-public.json').read_text())['fingerprint']
            nonces, key_ids = set(), set()
            for item in items:
                encryption = item['manifest']['encryption']
                if encryption.get('mode') != 'end-to-end' or encryption.get('algorithm') != 'aes-256-gcm':
                    raise RuntimeError('a transfer was not end-to-end encrypted')
                if (encryption.get('recipientKeyFingerprint') != fingerprint
                        or item['manifest']['recipientId'] != 'labrx'):
                    raise RuntimeError('transfer was not bound to the pinned recipient')
                nonces.add(encryption.get('nonceBase64'))
                key_ids.add(encryption.get('keyId'))
                if item['payload'].startswith(b'\x89PNG\r\n\x1a\n'):
                    raise RuntimeError('provider received a plaintext PNG')
            if None in nonces or None in key_ids or len(nonces) != 2 or len(key_ids) != 2:
                raise RuntimeError('encrypted transfers reused a nonce or ephemeral key')
        successful = True
        print('PASS: two Decodium applications transferred and decrypted images A/B over verified loopback HTTPS')
    finally:
        for process in processes:
            if process.poll() is None:
                process.kill()
                process.wait()
        server.shutdown()
        server.server_close()
        server_thread.join(timeout=5)
        for handle in refs:
            security.SecKeychainDelete(handle)
            core.CFRelease(handle)
        if successful:
            shutil.rmtree(root)
        else:
            print('Diagnostic files retained at {}'.format(root))


if __name__ == '__main__':
    main()
