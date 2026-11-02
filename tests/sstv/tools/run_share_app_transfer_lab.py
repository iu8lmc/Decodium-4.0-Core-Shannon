#!/usr/bin/env python3
"""Two full Decodium apps exchange two encrypted images via loopback HTTPS."""
import argparse
import json
import os
import platform
from pathlib import Path
import secrets
import shutil
import sqlite3
import subprocess
import tempfile
import threading
import time
import uuid

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
    attacks.add_argument('--tamper-payload', choices=('a', 'b'))
    attacks.add_argument('--tamper-manifest', choices=('a', 'b'))
    attacks.add_argument('--forge-sender', choices=('a', 'b'))
    parser.add_argument('--tamper-manifest-field', choices=('sstvMode', 'expiresUtc', 'keyId', 'senderId', 'senderSignatureBase64', 'senderAuth'), default='sstvMode')
    attacks.add_argument('--swap-payload', choices=('a', 'b'))
    attacks.add_argument('--replay-envelope', choices=('a', 'b'))
    attacks.add_argument('--downgrade-manifest', choices=('a', 'b'))
    attacks.add_argument('--expire-download', choices=('a', 'b'))
    attacks.add_argument('--expire-awaiting', choices=('a', 'b'))
    attacks.add_argument('--accept-restart-cleanup', choices=('a', 'b'))
    attacks.add_argument('--policy-awaiting', choices=('a', 'b'))
    parser.add_argument('--corrupt-gallery', action='store_true')
    parser.add_argument('--gallery-unavailable', action='store_true')
    parser.add_argument('--inbox-unavailable', action='store_true')
    parser.add_argument('--signed', action='store_true')
    parser.add_argument('--expiry-after-first-chunk', action='store_true')
    parser.add_argument('--expiry-restart-cleanup', action='store_true')
    parser.add_argument('decodium', type=Path)
    parser.add_argument('certificate', type=Path)
    parser.add_argument('key', type=Path)
    parser.add_argument('image_a', type=Path)
    parser.add_argument('image_b', type=Path)
    args = parser.parse_args()
    if ((args.gallery_unavailable or args.inbox_unavailable)
            and (not args.accept_restart_cleanup or args.corrupt_gallery)) or (args.gallery_unavailable and args.inbox_unavailable):
        parser.error('choose one unavailable check with acceptance and no corruption')
    if args.corrupt_gallery and not args.accept_restart_cleanup:
        parser.error('--corrupt-gallery requires --accept-restart-cleanup')
    if args.forge_sender and not args.signed:
        parser.error('--forge-sender requires --signed')
    if args.tamper_manifest_field != 'sstvMode' and not args.tamper_manifest:
        parser.error('--tamper-manifest-field requires --tamper-manifest')
    expiry_while_awaiting = bool(args.expire_awaiting)
    args.expire_download = args.expire_download or args.expire_awaiting
    if args.expiry_after_first_chunk and not args.expire_download:
        parser.error('--expiry-after-first-chunk requires --expire-download')
    if args.expiry_restart_cleanup and not args.expiry_after_first_chunk:
        parser.error('--expiry-restart-cleanup requires --expiry-after-first-chunk')
    binary = args.decodium.resolve(strict=True)
    cert, key = args.certificate.resolve(strict=True), args.key.resolve(strict=True)
    source_a, source_b = args.image_a.resolve(strict=True), args.image_b.resolve(strict=True)
    root = Path(tempfile.mkdtemp(prefix='decodium-share-transfer-'))
    base_env = os.environ.copy()
    security = core = None
    refs = []
    if platform.system() == 'Darwin':
        security, core, refs = keychains([root / 'sender.keychain-db', root / 'receiver.keychain-db'])
    elif platform.system() == 'Linux':
        if os.environ.get('DECODIUM_LAB_ALLOW_SECRET_SERVICE') != '1' or not os.environ.get('DBUS_SESSION_BUS_ADDRESS'):
            raise RuntimeError('Linux lab requires an explicitly enabled isolated Secret Service session')
        for role in ('sender', 'receiver'):
            (root / (role + '.keychain-db')).write_bytes(b'decodium-linux-secret-service-lab-v1\n')
    elif platform.system() == 'Windows':
        # Each app's INI root is private to this test, while both processes
        # share an isolated APPDATA lock namespace. DPAPI remains the real OS
        # backend and no production SecureStore entries are touched.
        for name in ('preflight-settings', 'appdata', 'localappdata', 'locks'):
            (root / name).mkdir()
        for role in ('sender', 'receiver'):
            (root / (role + '-settings')).mkdir()
            (root / (role + '.keychain-db')).write_bytes(b'decodium-windows-dpapi-lab-v1\n')
        base_env.update(DECODIUM_LAB_ALLOW_DPAPI='1',
                        DECODIUM_LAB_SETTINGS_DIR=str(root / 'preflight-settings'),
                        DECODIUM_LAB_DPAPI_LOCK_DIR=str(root / 'locks'),
                        APPDATA=str(root / 'appdata'), LOCALAPPDATA=str(root / 'localappdata'))
    else:
        raise RuntimeError('sharing application lab is unsupported on this platform')
    server = ShareFixture(cert, key, tamper=args.tamper_payload or args.tamper, tamper_manifest=args.tamper_manifest,
                          tamper_manifest_field=args.tamper_manifest_field,
                          downgrade_manifest=args.downgrade_manifest,
                          swap_payload=args.swap_payload,
                          replay_target=args.replay_envelope,
                          replay_after=(root / 'out' / ('image-' + args.replay_envelope + '.png'))
                          if args.replay_envelope else None,
                          expiry_target=None if expiry_while_awaiting else args.expire_download,
                          expiry_marker=(root / 'expiry-marker') if args.expire_download else None,
                          expiry_after_chunk=args.expiry_after_first_chunk,
                          first_output=(root / 'out' / 'image-a.png')
                          if args.tamper_payload == 'b' or args.tamper_manifest == 'b' or args.forge_sender == 'b' or args.swap_payload == 'b' or args.replay_envelope or args.downgrade_manifest == 'b' or args.expire_download == 'b' or args.policy_awaiting else None)
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
        if args.signed:
            for session in sessions.values():
                session.update(signingIdentity=str(root / 'sender-signing.json'),
                               signingReady=str(root / 'signing-ready'))
        if args.accept_restart_cleanup:
            sessions['receiver']['acceptTarget'] = args.accept_restart_cleanup
        if args.policy_awaiting:
            sessions['receiver']['policyTarget'] = args.policy_awaiting
        if args.expire_download:
            sessions['receiver'].update(expiryTarget=args.expire_download,
                                        expiryMarker=str(root / 'expiry-marker'),
                                        expiryAfterFirstChunk=args.expiry_after_first_chunk,
                                        expiryWhileAwaiting=expiry_while_awaiting)
        if args.replay_envelope:
            sessions['receiver']['replayTarget'] = args.replay_envelope
        if platform.system() == 'Linux':
            valid = root / 'preflight.json'
            valid.write_text(json.dumps(sessions['receiver']))
            denied_env = os.environ.copy()
            denied_env.pop('DECODIUM_LAB_ALLOW_SECRET_SERVICE', None)
            denied = subprocess.run([str(binary), '--test-mode', '--lab-share-keychain',
                str(root / 'receiver.keychain-db'), '--lab-share-session-file', str(valid)],
                env=denied_env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            if denied.returncode != 2 or (root / 'receiver-ok').exists():
                raise RuntimeError('Linux sharing lab was not rejected without explicit opt-in')
            bad_sentinel = root / 'bad-sentinel'
            bad_sentinel.write_text('invalid sentinel\n')
            denied = subprocess.run([str(binary), '--test-mode', '--lab-share-keychain',
                str(bad_sentinel), '--lab-share-session-file', str(valid)],
                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            if denied.returncode != 2 or (root / 'receiver-ok').exists():
                raise RuntimeError('Linux sharing lab accepted an invalid sentinel')
        elif platform.system() == 'Windows':
            valid = root / 'preflight.json'
            valid.write_text(json.dumps(sessions['receiver']))
            denied = subprocess.run([str(binary), '--lab-share-keychain',
                str(root / 'receiver.keychain-db'), '--lab-share-session-file', str(valid)],
                env=base_env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            if denied.returncode != 2 or (root / 'receiver-ok').exists():
                raise RuntimeError('Windows sharing lab accepted a launch outside test mode')
            denied_env = base_env.copy()
            denied_env.pop('DECODIUM_LAB_ALLOW_DPAPI', None)
            denied = subprocess.run([str(binary), '--test-mode', '--lab-share-keychain',
                str(root / 'receiver.keychain-db'), '--lab-share-session-file', str(valid)],
                env=denied_env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            if denied.returncode != 2 or (root / 'receiver-ok').exists():
                raise RuntimeError('Windows sharing lab was not rejected without explicit opt-in')
            bad_sentinel = root / 'bad-sentinel'
            bad_sentinel.write_text('invalid sentinel\n')
            denied = subprocess.run([str(binary), '--test-mode', '--lab-share-keychain',
                str(bad_sentinel), '--lab-share-session-file', str(valid)], env=base_env,
                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            if denied.returncode != 2 or (root / 'receiver-ok').exists():
                raise RuntimeError('Windows sharing lab accepted an invalid sentinel')
            unsafe_env = base_env.copy()
            unsafe_env.pop('DECODIUM_LAB_SETTINGS_DIR', None)
            denied = subprocess.run([str(binary), '--test-mode', '--lab-share-keychain',
                str(root / 'receiver.keychain-db'), '--lab-share-session-file', str(valid)],
                env=unsafe_env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            if denied.returncode != 2 or (root / 'receiver-ok').exists():
                raise RuntimeError('Windows sharing lab accepted a missing isolated settings root')
            unsafe_env = base_env.copy()
            unsafe_env.pop('DECODIUM_LAB_DPAPI_LOCK_DIR', None)
            denied = subprocess.run([str(binary), '--test-mode', '--lab-share-keychain',
                str(root / 'receiver.keychain-db'), '--lab-share-session-file', str(valid)],
                env=unsafe_env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            if denied.returncode != 2 or (root / 'receiver-ok').exists():
                raise RuntimeError('Windows sharing lab accepted a missing isolated lock root')
        invalid = root / 'invalid.json'
        invalid.write_text(json.dumps(dict(sessions['receiver'], endpoint='https://example.com:443/')))
        blocked = subprocess.run([str(binary), '--test-mode', '--lab-share-keychain',
            str(root / 'receiver.keychain-db'), '--lab-share-session-file', str(invalid)],
            env=base_env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
        if blocked.returncode != 2 or (root / 'receiver-ok').exists():
            raise RuntimeError('session lab did not reject a non-loopback endpoint')
        if args.expire_download:
            invalid.write_text(json.dumps(dict(sessions['receiver'], expiryMarker=str(root.parent / 'unsafe-marker'))))
            blocked = subprocess.run([str(binary), '--test-mode', '--lab-share-keychain',
                str(root / 'receiver.keychain-db'), '--lab-share-session-file', str(invalid)],
                env=base_env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            if blocked.returncode != 2:
                raise RuntimeError('session lab accepted an expiry marker outside the test root')
            invalid.write_text(json.dumps(dict(sessions['sender'], expiryMarker=str(root / 'expiry-marker'),
                                               expiryTarget=args.expire_download)))
            blocked = subprocess.run([str(binary), '--test-mode', '--lab-share-keychain',
                str(root / 'sender.keychain-db'), '--lab-share-session-file', str(invalid)],
                env=base_env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            if blocked.returncode != 2:
                raise RuntimeError('session lab accepted an expiry clock on the sender')
        suffix = secrets.token_hex(4)
        for role in ('receiver', 'sender'):
            config = root / (role + '.json')
            config.write_text(json.dumps(sessions[role]))
            env = base_env.copy()
            if args.signed:
                env['DECODIUM_LAB_SIGNED_ENVELOPE'] = '1'
            if args.forge_sender and role == 'sender':
                env['DECODIUM_LAB_SIGNED_FORGE'] = args.forge_sender
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
            if (args.tamper or args.tamper_payload or args.tamper_manifest or args.forge_sender or args.swap_payload or args.downgrade_manifest) and states[0] is not None:
                if states[0] != 2 or (not args.forge_sender and not server.tamper_count):
                    raise RuntimeError('tampered transfer was not rejected')
                if (args.tamper_payload or args.tamper_manifest or args.swap_payload) and server.tamper_count != 1:
                    raise RuntimeError('targeted content or manifest was not altered exactly once')
                if (root / 'receiver-ok').exists():
                    raise RuntimeError('tampered transfer produced a completion marker')
                accepted = {p.name for p in (root / 'out').iterdir()}
                if args.tamper_payload == 'b' or args.tamper_manifest == 'b' or args.forge_sender == 'b' or args.swap_payload == 'b' or args.downgrade_manifest == 'b':
                    if accepted != {'image-a.png'}:
                        raise RuntimeError('valid A missing or altered B was accepted')
                    reference, received = image_bytes(source_a), image_bytes(root / 'out' / 'image-a.png')
                    errors = [abs(a - b) for a, b in zip(reference, received)]
                    if len(reference) != len(received) or not errors or max(errors) > 5 or sum(errors) / len(errors) > 1:
                        raise RuntimeError('valid A was altered')
                elif accepted:
                    raise RuntimeError('tampered transfer produced accepted output')
                log = (root / 'receiver.log').read_text(errors='replace')
                if args.forge_sender:
                    with server.lock:
                        forged = [item for item in server.uploads.values()
                                  if item['manifest']['safeDisplayFilename'] == 'image-' + args.forge_sender + '.png']
                    pinned = json.loads((root / 'sender-signing.json').read_text())['fingerprint']
                    if (len(forged) != 1 or forged[0]['manifest']['senderId'] != 'labtx'
                            or forged[0]['manifest']['encryption'].get('senderKeyFingerprint') in (None, pinned)):
                        raise RuntimeError('independent signing-key impersonation was not produced')
                if args.signed and args.tamper_manifest:
                    with server.lock:
                        changed = [item for item in server.uploads.values() if item.get('signed_before_tamper')]
                    if len(changed) != 1:
                        raise RuntimeError('signed manifest was not present before attack')
                if args.downgrade_manifest:
                    if 'download: this download requires end-to-end encryption' not in log:
                        raise RuntimeError('receiver did not report the strict encryption policy')
                    with server.lock:
                        if server.downgraded_downloads:
                            raise RuntimeError('downgraded item content was requested')
                elif args.signed and args.tamper_manifest and args.tamper_manifest_field == 'senderAuth':
                    if 'download: this download requires an authenticated sender' not in log:
                        raise RuntimeError('receiver did not reject the stripped signature before download')
                    with server.lock:
                        if server.stripped_signature_downloads:
                            raise RuntimeError('unsigned item content was requested')
                elif 'download failed: encrypted payload authentication or decryption failed' not in log:
                    raise RuntimeError('receiver did not report authenticated-open failure')
                successful = True
                print('PASS: two Decodium applications rejected altered {} with recomputed manifest digest'.format(
                    'downgraded manifest' if args.downgrade_manifest else
                    ('forged sender' if args.forge_sender else 'manifest' if args.tamper_manifest else 'swapped ciphertext' if args.swap_payload else 'ciphertext')))
                return
            if any(code is not None and code != 0 for code in states):
                raise RuntimeError('Decodium sharing actor failed: receiver={}, sender={}'.format(*states))
            if all(code == 0 for code in states):
                break
            if time.monotonic() >= deadline:
                raise RuntimeError('Decodium sharing actors timed out')
            time.sleep(0.2)
        if not (root / 'sender-ok').exists() or not (root / 'receiver-ok').exists():
            raise RuntimeError('missing sharing completion marker')
        if args.expire_download:
            if server.expiry_count != (0 if expiry_while_awaiting else 1) or not (root / 'expiry-marker').exists():
                raise RuntimeError('expiry was not injected exactly once during a download')
            if args.expiry_after_first_chunk and (len(server.expiry_offsets) != 2
                    or server.expiry_offsets[0] != 0 or server.expiry_offsets[1] <= 0):
                raise RuntimeError('expiry did not follow one completed content chunk')
            accepted = {p.name for p in (root / 'out').iterdir()}
            expected = {'image-a.png'} if args.expire_download == 'b' else set()
            if accepted != expected:
                raise RuntimeError('unexpected accepted output after expiry')
            if expected:
                reference, received = image_bytes(source_a), image_bytes(root / 'out' / 'image-a.png')
                errors = [abs(a - b) for a, b in zip(reference, received)]
                if len(reference) != len(received) or not errors or max(errors) > 5 or sum(errors) / len(errors) > 1:
                    raise RuntimeError('valid A was altered')
            log = (root / 'receiver.log').read_text(errors='replace')
            if '[LAB][SHARE SESSION] download expired' not in log:
                raise RuntimeError('receiver did not report expired download')
            if args.expiry_restart_cleanup:
                details = json.loads((root / 'receiver-ok').read_text())
                transfer = str(uuid.UUID(details['transferId']))
                storage = Path(details['storageFolder'])
                if not storage.is_absolute() or not storage.is_dir():
                    raise RuntimeError('receiver did not report a valid storage directory')
                partial_dir = storage / 'sharing' / 'downloads' / '.partial'
                partial = partial_dir / (transfer + '.part')
                sentinel = partial_dir / ('sentinel-' + secrets.token_hex(8))
                if partial.exists() or partial.is_symlink():
                    raise RuntimeError('expired partial file unexpectedly exists')
                with server.lock:
                    matches = [item for item in server.uploads.values()
                               if item['manifest']['safeDisplayFilename'] == 'image-' + args.expire_download + '.png']
                    if len(matches) != 1 or len(matches[0]['payload']) <= 64:
                        raise RuntimeError('missing ciphertext for restart simulation')
                    partial.write_bytes(matches[0]['payload'][:64])
                sentinel.write_text('preserve')
                env = base_env.copy()
                env.update(QT_QUICK_BACKEND='software',
                           DECODIUM_LAB_SETTINGS_DIR=str(root / 'receiver-settings'))
                cmd = [str(binary), '--test-mode', '--rig-name', 'SHARE' + suffix + 'receiver',
                       '--lab-callsign', 'LABRX', '--lab-no-cat', '--lab-no-monitor',
                       '--lab-share-keychain', str(root / 'receiver.keychain-db'),
                       '--lab-share-identity-file', str(root / 'restart-public.json')]
                with (root / 'restart.log').open('wb') as log_file:
                    restarted = subprocess.run(cmd, env=env, stdout=log_file, stderr=subprocess.STDOUT, timeout=90)
                if restarted.returncode != 0 or not (root / 'restart-public.json').exists():
                    raise RuntimeError('receiver restart failed')
                if partial.exists() or sentinel.read_text() != 'preserve':
                    raise RuntimeError('receiver restart did not safely recover expired partial')
                sentinel.unlink()
            successful = True
            print('PASS: two Decodium applications rejected expired image ' + args.expire_download.upper()
                  + ('; restarted receiver removed simulated residual' if args.expiry_restart_cleanup else ''))
            return
        if args.policy_awaiting and 'pending unsigned acceptance rejected by sender policy' not in (root / 'receiver.log').read_text(errors='replace'):
            raise RuntimeError('pending unsigned acceptance was not rejected')
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
        if args.accept_restart_cleanup:
            details = json.loads((root / 'receiver-ok').read_text())
            transfer = str(uuid.UUID(details['transferId']))
            storage = Path(details['storageFolder'])
            if not storage.is_absolute() or not storage.is_dir():
                raise RuntimeError('receiver did not report a valid storage directory')
            downloads = storage / 'sharing' / 'downloads'
            partial_dir = downloads / '.partial'
            partial = partial_dir / (transfer + '.part')
            gallery = list((storage / 'images' / 'imported').rglob('*' + transfer + '.png'))
            if partial.exists() or partial.is_symlink() or len(gallery) != 1 or not gallery[0].is_file():
                raise RuntimeError('acceptance did not leave the expected Gallery file')
            accepted_data = gallery[0].read_bytes()
            if args.corrupt_gallery:
                if len(accepted_data) < 32:
                    raise RuntimeError('Gallery PNG is too small for corruption control')
                altered = bytearray(accepted_data)
                altered[len(altered) // 2] ^= 1
                accepted_data = bytes(altered)
                gallery[0].write_bytes(accepted_data)
            with server.lock:
                matches = [item for item in server.uploads.values()
                           if item['manifest']['safeDisplayFilename'] == 'image-' + args.accept_restart_cleanup + '.png']
                if len(matches) != 1 or not matches[0]['payload']:
                    raise RuntimeError('missing ciphertext for restart simulation')
                partial.write_bytes(matches[0]['payload'][:64])
            sentinel = partial_dir / ('sentinel-' + secrets.token_hex(8))
            sentinel.write_text('preserve')
            env = base_env.copy()
            env.update(QT_QUICK_BACKEND='software',
                       DECODIUM_LAB_SETTINGS_DIR=str(root / 'receiver-settings'))
            cmd = [str(binary), '--test-mode', '--rig-name', 'SHARE' + suffix + 'receiver',
                   '--lab-callsign', 'LABRX', '--lab-no-cat', '--lab-no-monitor',
                   '--lab-share-keychain', str(root / 'receiver.keychain-db'),
                   '--lab-share-identity-file', str(root / 'restart-public.json')]
            first_env = env.copy()
            if args.gallery_unavailable:
                first_env['DECODIUM_LAB_GALLERY_UNAVAILABLE'] = '1'
            if args.inbox_unavailable:
                first_env['DECODIUM_LAB_INBOX_UNAVAILABLE'] = '1'
            with (root / 'restart.log').open('wb') as log_file:
                restarted = subprocess.run(cmd, env=first_env, stdout=log_file, stderr=subprocess.STDOUT, timeout=90)
            if (restarted.returncode != 0 or not (root / 'restart-public.json').exists()
                    or partial.exists() or partial.is_symlink() or sentinel.read_text() != 'preserve'
                    or gallery[0].read_bytes() != accepted_data):
                raise RuntimeError('receiver restart did not safely recover accepted partial')
            with sqlite3.connect(str(storage / 'sharing' / 'queue.sqlite')) as database:
                row = database.execute('SELECT state,last_error_redacted FROM sstv_share_transfers WHERE transfer_id=?', (transfer,)).fetchone()
            expected_state = 'Failed' if args.corrupt_gallery else 'Accepted'
            if not row or row[0] != expected_state:
                raise RuntimeError('unexpected state after Gallery import and restart: ' + repr(row))
            if (args.gallery_unavailable or args.inbox_unavailable) and row[1] != 'Gallery verification temporarily unavailable':
                raise RuntimeError('Gallery check did not remain pending: ' + repr(row))
            if args.gallery_unavailable or args.inbox_unavailable:
                cmd[-1] = str(root / 'restart-public-retry.json')
                with (root / 'restart-retry.log').open('wb') as log_file:
                    retry = subprocess.run(cmd, env=env, stdout=log_file, stderr=subprocess.STDOUT, timeout=90)
                if retry.returncode or not (root / 'restart-public-retry.json').exists():
                    raise RuntimeError('receiver recovery retry failed')
                with sqlite3.connect(str(storage / 'sharing' / 'queue.sqlite')) as database:
                    row = database.execute('SELECT state,last_error_redacted FROM sstv_share_transfers WHERE transfer_id=?', (transfer,)).fetchone()
                if row != ('Accepted', '') or gallery[0].read_bytes() != accepted_data:
                    raise RuntimeError('Gallery recovery retry was not verified: ' + repr(row))
            print('accepted image ' + args.accept_restart_cleanup.upper() + ': restart removed simulated residual'
                  + (' and rejected corrupted Gallery data' if args.corrupt_gallery else ''))
        with server.lock:
            if args.replay_envelope:
                if not server.replay_id or server.replay_downloads:
                    raise RuntimeError('replayed envelope was not rejected before content download')
                replay = server.uploads[server.replay_id]
                originals = [item for ident, item in server.uploads.items() if ident != server.replay_id]
                source = [item for item in originals if item['manifest']['safeDisplayFilename']
                          == 'image-' + args.replay_envelope + '.png']
                if len(source) != 1 or replay['canonical'] != source[0]['canonical'] or replay['payload'] != source[0]['payload']:
                    raise RuntimeError('provider did not replay the exact encrypted envelope')
                if '[LAB][SHARE SESSION] duplicate encrypted envelope rejected' not in (root / 'receiver.log').read_text(errors='replace'):
                    raise RuntimeError('receiver did not report duplicate rejection')
                items = originals
            else:
                items = list(server.uploads.values())
            if args.signed:
                signing_fingerprint = json.loads((root / 'sender-signing.json').read_text())['fingerprint']
                if any(item['manifest']['encryption'].get('senderAuthAlgorithm') != 'ecdsa-p256-sha256-v1'
                       or not item['manifest']['encryption'].get('senderSignatureBase64')
                       or item['manifest']['encryption'].get('senderKeyFingerprint') != signing_fingerprint
                       or item['manifest']['senderId'] != 'labtx' for item in items):
                    raise RuntimeError('fixture did not receive expected sender-signed manifests')
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
        print('PASS: two Decodium applications transferred and decrypted images A/B over verified loopback HTTPS'
              + (' and rejected an exact replay of image ' + args.replay_envelope.upper() if args.replay_envelope else ''))
    finally:
        for process in processes:
            if process.poll() is None:
                process.kill()
                process.wait()
        server.shutdown()
        server.server_close()
        server_thread.join(timeout=5)
        if security and core:
            for handle in refs:
                security.SecKeychainDelete(handle)
                core.CFRelease(handle)
        if successful:
            shutil.rmtree(root)
        else:
            print('Diagnostic files retained at {}'.format(root))


if __name__ == '__main__':
    main()
