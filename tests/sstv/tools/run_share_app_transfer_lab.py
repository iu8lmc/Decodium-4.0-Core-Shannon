#!/usr/bin/env python3
"""Two full Decodium apps exchange two encrypted images via loopback HTTPS."""
import argparse
import base64
import hashlib
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
    parser.add_argument('--tamper-manifest-field', choices=('sstvMode', 'expiresUtc', 'keyId', 'nonceBase64', 'senderId', 'senderSignatureBase64', 'senderSignatureHighS', 'senderAuth'), default='sstvMode')
    attacks.add_argument('--swap-payload', choices=('a', 'b'))
    attacks.add_argument('--replay-envelope', choices=('a', 'b'))
    parser.add_argument('--replay-after-restart', action='store_true')
    parser.add_argument('--replay-after-accept', action='store_true')
    parser.add_argument('--replay-high-s', action='store_true')
    parser.add_argument('--replay-resigned', action='store_true')
    parser.add_argument('--drop-replay-indexes-before-restart', action='store_true')
    attacks.add_argument('--downgrade-manifest', choices=('a', 'b'))
    attacks.add_argument('--expire-download', choices=('a', 'b'))
    attacks.add_argument('--expire-awaiting', choices=('a', 'b'))
    attacks.add_argument('--accept-restart-cleanup', choices=('a', 'b'))
    attacks.add_argument('--policy-awaiting', choices=('a', 'b'))
    attacks.add_argument('--invalid-before-valid', choices=('a', 'b'))
    parser.add_argument('--legacy-invalid-record', action='store_true')
    parser.add_argument('--invalid-nonce-restart', action='store_true')
    parser.add_argument('--corrupt-gallery', action='store_true')
    parser.add_argument('--gallery-unavailable', action='store_true')
    parser.add_argument('--inbox-unavailable', action='store_true')
    parser.add_argument('--signed', action='store_true')
    parser.add_argument('--busy-refresh', choices=('a', 'b'))
    parser.add_argument('--busy-exhaust', action='store_true')
    parser.add_argument('--rotate-recipient', action='store_true')
    parser.add_argument('--replace-recipient', action='store_true')
    parser.add_argument('--restart-rotated-identity', action='store_true')
    parser.add_argument('--restart-sender-pin', action='store_true')
    parser.add_argument('--replace-sender-signing', action='store_true')
    parser.add_argument('--rotate-sender-signing', action='store_true')
    parser.add_argument('--retired-signature', choices=('a', 'b'))
    parser.add_argument('--retired-unseen', choices=('a', 'b'))
    parser.add_argument('--expiry-after-first-chunk', action='store_true')
    parser.add_argument('--expiry-restart-cleanup', action='store_true')
    parser.add_argument('decodium', type=Path)
    parser.add_argument('certificate', type=Path)
    parser.add_argument('key', type=Path)
    parser.add_argument('image_a', type=Path)
    parser.add_argument('image_b', type=Path)
    args = parser.parse_args()
    if args.busy_refresh and not args.signed:
        parser.error('--busy-refresh requires --signed')
    if args.busy_exhaust and not args.busy_refresh:
        parser.error('--busy-exhaust requires --busy-refresh')
    if args.rotate_sender_signing:
        args.replace_sender_signing = True
    if args.retired_signature and not args.rotate_sender_signing:
        parser.error('--retired-signature requires --rotate-sender-signing')
    if args.retired_unseen and (not args.rotate_sender_signing or args.retired_signature):
        parser.error('--retired-unseen requires rotation and excludes --retired-signature')
    if args.replace_sender_signing and (not args.signed or platform.system() != 'Darwin'):
        parser.error('--replace-sender-signing requires signed macOS isolated Keychain lab')
    if args.rotate_recipient and not args.signed:
        parser.error('--rotate-recipient requires --signed')
    if args.replace_recipient and not args.rotate_recipient:
        parser.error('--replace-recipient requires --rotate-recipient')
    if args.restart_rotated_identity and not args.rotate_recipient:
        parser.error('--restart-rotated-identity requires --rotate-recipient')
    if args.restart_sender_pin and not args.replace_recipient:
        parser.error('--restart-sender-pin requires --replace-recipient')
    if ((args.gallery_unavailable or args.inbox_unavailable)
            and (not args.accept_restart_cleanup or args.corrupt_gallery)) or (args.gallery_unavailable and args.inbox_unavailable):
        parser.error('choose one unavailable check with acceptance and no corruption')
    if args.corrupt_gallery and not args.accept_restart_cleanup:
        parser.error('--corrupt-gallery requires --accept-restart-cleanup')
    if args.forge_sender and not args.signed:
        parser.error('--forge-sender requires --signed')
    if args.invalid_before_valid and not args.signed:
        parser.error('--invalid-before-valid requires --signed')
    if args.invalid_nonce_restart and (not args.invalid_before_valid or args.legacy_invalid_record):
        parser.error('--invalid-nonce-restart requires invalid-before-valid without legacy injection')
    if args.legacy_invalid_record and (not args.invalid_before_valid or platform.system() != 'Darwin'):
        parser.error('--legacy-invalid-record requires invalid-before-valid and macOS')
    if args.replay_after_restart and not args.replay_envelope:
        parser.error('--replay-after-restart requires --replay-envelope')
    if args.replay_after_accept and (not args.replay_after_restart or not args.signed):
        parser.error('--replay-after-accept requires signed replay after restart')
    if args.replay_high_s and not (args.replay_after_accept and args.replay_after_restart and args.signed):
        parser.error('--replay-high-s requires a signed replay after acceptance and restart')
    if args.replay_resigned and (not args.replay_after_accept or not args.replay_after_restart
                                 or not args.signed or args.replay_high_s):
        parser.error('--replay-resigned requires signed replay after acceptance and restart')
    if args.drop_replay_indexes_before_restart and (not args.replay_resigned or platform.system() != 'Darwin'):
        parser.error('--drop-replay-indexes-before-restart requires re-signed replay in the macOS isolated lab')
    if args.tamper_manifest_field != 'sstvMode' and not args.tamper_manifest:
        parser.error('--tamper-manifest-field requires --tamper-manifest')
    if args.tamper_manifest_field == 'senderSignatureHighS' and not args.signed:
        parser.error('high-S attack requires a signed envelope')
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
        keychain_paths = [root / 'sender.keychain-db', root / 'receiver.keychain-db']
        if args.replace_sender_signing:
            keychain_paths.append(root / 'sender-rekey.keychain-db')
        security, core, refs = keychains(keychain_paths)
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
                          replay_high_s=args.replay_high_s,
                          replay_after=((root / 'replay-after-restart') if args.replay_after_restart
                                        else (root / 'out' / ('image-' + args.replay_envelope + '.png')))
                          if args.replay_envelope else None,
                          expiry_target=None if expiry_while_awaiting else args.expire_download,
                          expiry_marker=(root / 'expiry-marker') if args.expire_download else None,
                          expiry_after_chunk=args.expiry_after_first_chunk,
                          first_output=(root / 'out' / 'image-a.png')
                          if args.tamper_payload == 'b' or args.tamper_manifest == 'b' or args.forge_sender == 'b' or args.swap_payload == 'b' or args.replay_envelope or args.downgrade_manifest == 'b' or args.expire_download == 'b' or args.policy_awaiting or args.invalid_before_valid == 'b'
                          or (args.tamper_manifest == 'a' and args.tamper_manifest_field == 'senderSignatureHighS') else None)
    server.busy_refresh_target = args.busy_refresh
    server.busy_refresh_delays = 0
    server.busy_refresh_seconds = 12.0 if args.busy_exhaust else 3.5
    if args.invalid_before_valid:
        server.invalid_before_target = args.invalid_before_valid
        server.invalid_before_marker = root / 'invalid-signature-rejected'
        server.invalid_before_nonce = args.invalid_nonce_restart
        if args.legacy_invalid_record or args.invalid_nonce_restart:
            server.invalid_before_release = root / 'legacy-record-ready'
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
        if args.rotate_recipient:
            sessions['receiver']['rotateRecipient'] = True
        if args.replace_recipient:
            sessions['sender']['replaceRecipient'] = True
        if args.invalid_before_valid:
            sessions['receiver']['invalidSignatureMarker'] = str(server.invalid_before_marker)
        if args.replay_resigned:
            sessions['sender'].update(resignTarget=args.replay_envelope,
                                      resignOutput=str(root / 'resigned-signature.txt'))
            preflight = root / 'resign-preflight.json'
            preflight.write_text(json.dumps(sessions['sender']))
            denied = subprocess.run([str(binary), '--lab-share-keychain', str(root / 'sender.keychain-db'),
                                     '--lab-share-session-file', str(preflight)], env=base_env,
                                    stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            if denied.returncode != 2 or (root / 'resigned-signature.txt').exists():
                raise RuntimeError('re-signing laboratory was accepted outside test mode')
            unsafe = dict(sessions['sender'], resignOutput=str(root.parent / (root.name + '-outside-signature')))
            preflight.write_text(json.dumps(unsafe))
            denied_env = base_env.copy()
            denied_env['DECODIUM_LAB_SIGNED_ENVELOPE'] = '1'
            denied = subprocess.run([str(binary), '--test-mode', '--lab-share-keychain', str(root / 'sender.keychain-db'),
                                     '--lab-share-session-file', str(preflight)], env=denied_env,
                                    stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            if denied.returncode != 2 or Path(unsafe['resignOutput']).exists():
                raise RuntimeError('re-signing laboratory accepted output outside the isolated root')
        if args.accept_restart_cleanup:
            sessions['receiver']['acceptTarget'] = args.accept_restart_cleanup
        if args.replay_after_accept:
            sessions['receiver']['acceptTarget'] = args.replay_envelope
        if args.policy_awaiting:
            sessions['receiver']['policyTarget'] = args.policy_awaiting
        if args.expire_download:
            sessions['receiver'].update(expiryTarget=args.expire_download,
                                        expiryMarker=str(root / 'expiry-marker'),
                                        expiryAfterFirstChunk=args.expiry_after_first_chunk,
                                        expiryWhileAwaiting=expiry_while_awaiting)
        if args.replay_envelope and not args.replay_after_restart:
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
            if role == 'receiver':
                receiver_cmd, receiver_env = cmd, env.copy()
            with open(root / (role + '.log'), 'wb') as log:
                processes.append(subprocess.Popen(cmd, env=env, stdout=log, stderr=subprocess.STDOUT))
        deadline = time.monotonic() + 105
        legacy_transfer_id = None
        nonce_receiver_restarted = False
        identity_before_nonce_restart = None
        while True:
            states = [process.poll() for process in processes]
            if args.invalid_nonce_restart and server.invalid_before_marker.exists() and not nonce_receiver_restarted:
                if states[0] is not None:
                    raise RuntimeError('receiver stopped before nonce attack restart')
                identity_before_nonce_restart = json.loads((root / 'receiver-public.json').read_text())
                processes[0].terminate()
                processes[0].wait(timeout=10)
                for label in ('a', 'b'):
                    (root / 'out' / ('image-' + label + '.png')).unlink(missing_ok=True)
                restart_config = dict(sessions['receiver'])
                restart_config['invalidSignatureMarker'] = str(root / 'invalid-after-restart')
                restart_config['resumeExistingProvider'] = True
                restart_file = root / 'receiver-restart-nonce.json'
                restart_file.write_text(json.dumps(restart_config))
                receiver_cmd = receiver_cmd[:-1] + [str(restart_file)]
                with (root / 'receiver-restart-nonce.log').open('wb') as log:
                    processes[0] = subprocess.Popen(receiver_cmd, env=receiver_env, stdout=log, stderr=subprocess.STDOUT)
                nonce_receiver_restarted = True
                server.invalid_before_release.write_text('ready\n')
                continue
            if args.legacy_invalid_record and server.invalid_before_marker.exists() and legacy_transfer_id is None:
                try:
                    marker = json.loads(server.invalid_before_marker.read_text())
                except (OSError, json.JSONDecodeError):
                    time.sleep(0.1)
                    continue
                storage = Path(marker['storageFolder']).resolve(strict=True)
                test_root = (Path.home() / '.qttest').resolve(strict=True)
                expected_name = 'Decodium - SHARE' + suffix + 'receiver - test'
                if test_root not in storage.parents or expected_name not in storage.parts or storage.name != 'sstv':
                    raise RuntimeError('refusing to modify a non-isolated test storage directory')
                database_path = (storage / 'sharing' / 'queue.sqlite').resolve(strict=True)
                if storage not in database_path.parents:
                    raise RuntimeError('test queue is outside the isolated storage directory')
                with server.lock:
                    invalid_id = server.invalid_before_id
                    forged = server.uploads[invalid_id]
                    canonical = forged['canonical']
                    manifest = forged['manifest']
                transfer_id = str(uuid.uuid4())
                key = hashlib.sha256(b'decodium-sstv-share-idempotency-v1\0' + canonical).hexdigest()
                with sqlite3.connect('file:' + str(database_path) + '?mode=rw', uri=True, timeout=5) as database:
                    database.execute('BEGIN IMMEDIATE')
                    row = database.execute('SELECT manifest_json,received_ms,updated_ms,transfer_id FROM sstv_share_inbox '
                                           'WHERE provider_id=? AND incoming_id=?', ('rest-encrypted', invalid_id)).fetchone()
                    if not row or row[0] != canonical or row[3]:
                        raise RuntimeError('isolated invalid inbox item was not ready for legacy fixture')
                    timestamp = max(row[1], row[2], int(time.time() * 1000))
                    database.execute('INSERT INTO sstv_share_transfers (transfer_id,direction,state,provider_id,recipient_id,'
                        'manifest_json,transfer_json,recipient_json,source_path,destination_path,staging_path,payload_sha256,'
                        'byte_size,byte_offset,attempts,next_retry_ms,last_failure,last_error_redacted,provider_session_id,'
                        'incoming_id,idempotency_key,cancel_requested,cancel_dispatched,restart_recoveries,revision,created_ms,updated_ms) '
                        'VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)',
                        (transfer_id, 2, 'Paused', 'rest-encrypted', manifest['recipientId'], canonical, b'', b'', '', '', '',
                         manifest['sha256'], manifest['byteSize'], 0, 0, 0, 'none', '', '', invalid_id, key, 0, 0, 0, 1, timestamp, timestamp))
                    updated = database.execute('UPDATE sstv_share_inbox SET disposition=?,transfer_id=?,updated_ms=? '
                                               'WHERE provider_id=? AND incoming_id=? AND transfer_id=?',
                                               ('DownloadQueued', transfer_id, timestamp, 'rest-encrypted', invalid_id, '')).rowcount
                    if updated != 1: raise RuntimeError('legacy inbox fixture lost its isolated identity')
                    database.commit()
                legacy_transfer_id = transfer_id
                server.invalid_before_release.write_text('ready\n')
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
                    if args.tamper_manifest_field == 'senderSignatureHighS':
                        from share_https_fixture import high_s_signature
                        item = changed[0]
                        if (not item.get('high_s_attack')
                                or high_s_signature(item['original_signature'])
                                != item['manifest']['encryption']['senderSignatureBase64']):
                            raise RuntimeError('equivalent high-S signature was not produced')
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
                elif args.signed and (args.tamper_manifest or args.forge_sender or args.tamper_payload):
                    if 'download: incoming sender signature could not be authenticated' not in log:
                        raise RuntimeError('receiver did not reject invalid sender signature before download')
                    if args.tamper_manifest:
                        with server.lock:
                            if server.signed_tampered_downloads:
                                raise RuntimeError('content requested despite invalid sender signature')
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
        if args.restart_rotated_identity:
            env = base_env.copy()
            env.update(QT_QUICK_BACKEND='software', DECODIUM_LAB_SETTINGS_DIR=str(root / 'receiver-settings'))
            cmd = [str(binary), '--test-mode', '--rig-name', 'SHARE' + suffix + 'receiver',
                   '--lab-callsign', 'LABRX', '--lab-no-cat', '--lab-no-monitor',
                   '--lab-share-keychain', str(root / 'receiver.keychain-db'),
                   '--lab-share-identity-file', str(root / 'rotated-after-restart.json')]
            with (root / 'rotated-restart.log').open('wb') as log:
                restart = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
            if restart.returncode != 0 or not (root / 'rotated-after-restart.json').exists():
                raise RuntimeError('rotated recipient identity restart failed')
            if json.loads((root / 'rotated-after-restart.json').read_text()) != json.loads((root / 'receiver-public.json.rotated').read_text()):
                raise RuntimeError('rotated recipient identity changed after application restart')
        if args.invalid_before_valid:
            log = (root / 'receiver.log').read_text(errors='replace')
            if (not server.invalid_before_id or server.invalid_before_downloads
                or not server.invalid_before_marker.exists()
                or '[LAB][SHARE SESSION] invalid sender signature rejected before download' not in log):
                raise RuntimeError('invalid signature was not rejected before valid transfer')
            with server.lock:
                forged = server.uploads[server.invalid_before_id]
                original = [item for ident, item in server.uploads.items()
                            if ident != server.invalid_before_id and item['manifest']['safeDisplayFilename']
                            == 'image-' + args.invalid_before_valid + '.png']
                if len(original) != 1 or forged['payload'] != original[0]['payload']:
                    raise RuntimeError('invalid-first fixture did not preserve ciphertext')
                before, after = json.loads(json.dumps(original[0]['manifest'])), json.loads(json.dumps(forged['manifest']))
                field = 'nonceBase64' if args.invalid_nonce_restart else 'senderSignatureBase64'
                first_value = before['encryption'].pop(field)
                second_value = after['encryption'].pop(field)
                if before != after or first_value == second_value:
                    raise RuntimeError('invalid-first fixture altered an unexpected field')
            if args.invalid_nonce_restart and not nonce_receiver_restarted:
                raise RuntimeError('receiver was not restarted after nonce rejection')
            if args.invalid_nonce_restart:
                identity_after = json.loads((root / 'receiver-public.json').read_text())
                if not identity_before_nonce_restart or identity_after != identity_before_nonce_restart:
                    raise RuntimeError('receiver identity changed across nonce-attack restart')
                with server.lock:
                    original_items = [item for ident, item in server.uploads.items()
                                      if ident != server.invalid_before_id
                                      and item['manifest']['safeDisplayFilename'] in ('image-a.png', 'image-b.png')]
                    if (len(original_items) != 2 or any(
                            item['manifest']['encryption'].get('recipientKeyFingerprint')
                            != identity_after['fingerprint'] for item in original_items)):
                        raise RuntimeError('valid transfers were not addressed to the persisted recipient identity')
            if args.legacy_invalid_record:
                if not legacy_transfer_id or not server.invalid_before_release.exists():
                    raise RuntimeError('legacy invalid record was not installed')
                with sqlite3.connect('file:' + str(database_path) + '?mode=ro', uri=True) as database:
                    legacy_row = database.execute('SELECT state,manifest_json FROM sstv_share_transfers WHERE transfer_id=?',
                                                  (legacy_transfer_id,)).fetchone()
                    valid_count = database.execute('SELECT COUNT(*) FROM sstv_share_transfers WHERE manifest_json=?',
                                                   (original[0]['canonical'],)).fetchone()[0]
                if legacy_row != ('Paused', forged['canonical']) or valid_count != 1:
                    raise RuntimeError('legacy row changed or valid transfer was not persisted')
        if args.replay_after_restart:
            if args.replay_after_accept:
                details = json.loads((root / 'receiver-ok').read_text())
                transfer = str(uuid.UUID(details['transferId']))
                storage = Path(details['storageFolder'])
                with sqlite3.connect(str(storage / 'sharing' / 'queue.sqlite')) as database:
                    row = database.execute('SELECT state FROM sstv_share_transfers WHERE transfer_id=?', (transfer,)).fetchone()
                gallery = list((storage / 'images' / 'imported').rglob('*' + transfer + '.png'))
                if row != ('Accepted',) or len(gallery) != 1:
                    raise RuntimeError('accepted transfer and Gallery import were not persisted')
                gallery_before = gallery[0].read_bytes()
                all_gallery_before = {p.relative_to(storage / 'images' / 'imported'): hashlib.sha256(p.read_bytes()).hexdigest()
                                      for p in (storage / 'images' / 'imported').rglob('*.png') if p.is_file()}
                if args.drop_replay_indexes_before_restart:
                    resolved_storage = storage.resolve(strict=True)
                    test_root = (Path.home() / '.qttest').resolve(strict=True)
                    expected_name = 'Decodium - SHARE' + suffix + 'receiver - test'
                    if (test_root not in resolved_storage.parents or expected_name not in resolved_storage.parts
                            or resolved_storage.name != 'sstv'):
                        raise RuntimeError('refusing to alter indexes outside isolated test storage')
                    queue_path = (resolved_storage / 'sharing' / 'queue.sqlite').resolve(strict=True)
                    if resolved_storage not in queue_path.parents:
                        raise RuntimeError('isolated queue is outside its storage directory')
                    with sqlite3.connect('file:' + str(queue_path) + '?mode=rw', uri=True) as database:
                        if database.execute('PRAGMA user_version').fetchone() != (3,):
                            raise RuntimeError('unexpected queue version before index-upgrade test')
                        database.execute('DROP INDEX idx_sstv_share_replay_key')
                        database.execute('DROP INDEX idx_sstv_share_replay_payload')
                        database.commit()
            if args.replay_resigned:
                signature = (root / 'resigned-signature.txt').read_text(encoding='ascii').strip()
                if not signature: raise RuntimeError('re-signing actor did not produce a signature')
                if platform.system() == 'Darwin':
                    with server.lock:
                        matches = [item for item in server.uploads.values()
                                   if item['manifest']['safeDisplayFilename'] == 'image-' + args.replay_envelope + '.png']
                        if len(matches) != 1: raise RuntimeError('missing signed manifest for independent verification')
                        signed_manifest = json.loads(json.dumps(matches[0]['manifest']))
                    original = signed_manifest['encryption'].pop('senderSignatureBase64')
                    message = b'Decodium-SSTV-sender-signature-v1\0' + json.dumps(
                        signed_manifest, ensure_ascii=False, sort_keys=True, separators=(',', ':')).encode('utf-8')
                    (root / 'signing-public.der').write_bytes(bytes.fromhex(json.loads(
                        (root / 'sender-signing.json').read_text())['publicKey']))
                    (root / 'signed-message.bin').write_bytes(message)
                    subprocess.run(['openssl', 'pkey', '-pubin', '-inform', 'DER',
                        '-in', str(root / 'signing-public.der'), '-out', str(root / 'signing-public.pem')],
                        check=True, capture_output=True, timeout=10)
                    for index, encoded in enumerate((original, signature)):
                        signature_file = root / ('signature-' + str(index) + '.der')
                        signature_file.write_bytes(base64.b64decode(encoded, validate=True))
                        subprocess.run(['openssl', 'dgst', '-sha256', '-verify', str(root / 'signing-public.pem'),
                            '-signature', str(signature_file), str(root / 'signed-message.bin')],
                            check=True, capture_output=True, timeout=10)
                with server.lock: server.replay_resigned_signature = signature
            # Keep the fixture from replaying until both original processes
            # have exited. Restart the same receiver identity and settings.
            (root / 'replay-after-restart').write_text('ready\n')
            (root / 'out-restart').mkdir()
            restart_config = dict(sessions['receiver'], replayTarget=args.replay_envelope,
                                  resumeExistingProvider=True,
                                  outputDir=str(root / 'out-restart'),
                                  result=str(root / 'receiver-restart-ok'))
            if args.replay_after_accept:
                restart_config.pop('acceptTarget', None)
                restart_config['expectedCopies'] = 1
            if args.replay_high_s:
                restart_config['replayExpectInvalidSignature'] = True
            restart_file = root / 'receiver-restart.json'
            restart_file.write_text(json.dumps(restart_config))
            env = base_env.copy()
            env.update(QT_QUICK_BACKEND='software',
                       DECODIUM_LAB_SETTINGS_DIR=str(root / 'receiver-settings'))
            if args.signed:
                env['DECODIUM_LAB_SIGNED_ENVELOPE'] = '1'
            cmd = [str(binary), '--test-mode', '--rig-name', 'SHARE' + suffix + 'receiver',
                   '--lab-callsign', 'LABRX', '--lab-no-cat', '--lab-no-monitor',
                   '--lab-share-keychain', str(root / 'receiver.keychain-db'),
                   '--lab-share-session-file', str(restart_file)]
            with (root / 'receiver-restart.log').open('wb') as log:
                restarted = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=105)
            if args.replay_high_s or args.replay_resigned:
                log_text = (root / 'receiver-restart.log').read_text(errors='replace')
                expected_rejection = ('[LAB][SHARE SESSION] replay sender signature rejected' if args.replay_high_s
                                      else '[LAB][SHARE SESSION] duplicate encrypted envelope rejected')
                if (restarted.returncode or not (root / 'receiver-restart-ok').exists()
                        or expected_rejection not in log_text):
                    raise RuntimeError('receiver did not reject the altered replay before download')
                with sqlite3.connect(str(storage / 'sharing' / 'queue.sqlite')) as database:
                    row = database.execute('SELECT state FROM sstv_share_transfers WHERE transfer_id=?', (transfer,)).fetchone()
                if (row != ('Accepted',) or gallery[0].read_bytes() != gallery_before
                        or len(list((storage / 'images' / 'imported').rglob('*' + transfer + '.png'))) != 1
                        or {p.relative_to(storage / 'images' / 'imported'): hashlib.sha256(p.read_bytes()).hexdigest()
                            for p in (storage / 'images' / 'imported').rglob('*.png') if p.is_file()} != all_gallery_before):
                    raise RuntimeError('accepted image changed after altered replay')
                if args.drop_replay_indexes_before_restart:
                    with sqlite3.connect(str(storage / 'sharing' / 'queue.sqlite')) as database:
                        indexes = {row[1] for row in database.execute("PRAGMA index_list('sstv_share_transfers')")}
                        version = database.execute('PRAGMA user_version').fetchone()
                    if version != (3,) or not {'idx_sstv_share_replay_key', 'idx_sstv_share_replay_payload'} <= indexes:
                        raise RuntimeError('existing queue did not regain replay indexes')
            elif restarted.returncode or not (root / 'receiver-restart-ok').exists():
                raise RuntimeError('receiver restart did not reject the replay')
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
                if len(source) != 1 or replay['payload'] != source[0]['payload']:
                    raise RuntimeError('provider did not replay the original ciphertext')
                if args.replay_high_s or args.replay_resigned:
                    from share_https_fixture import high_s_signature
                    original = source[0]['manifest']
                    changed = replay['manifest']
                    if args.replay_high_s and (high_s_signature(original['encryption']['senderSignatureBase64'])
                            != changed['encryption']['senderSignatureBase64']):
                        raise RuntimeError('replay did not contain the equivalent high-S signature')
                    if args.replay_resigned and (changed['encryption']['senderSignatureBase64'] != signature
                            or signature == original['encryption']['senderSignatureBase64']):
                        raise RuntimeError('replay did not contain the separately verified new signature')
                    original_copy = json.loads(json.dumps(original))
                    altered_copy = json.loads(json.dumps(changed))
                    del original_copy['encryption']['senderSignatureBase64']
                    del altered_copy['encryption']['senderSignatureBase64']
                    if original_copy != altered_copy or replay['canonical'] == source[0]['canonical']:
                        raise RuntimeError('replay changed more than the signature or did not change identity')
                elif replay['canonical'] != source[0]['canonical']:
                    raise RuntimeError('provider did not replay the exact encrypted envelope')
                replay_log = root / ('receiver-restart.log' if args.replay_after_restart else 'receiver.log')
                expected_rejection = ('[LAB][SHARE SESSION] replay sender signature rejected' if args.replay_high_s
                                      else '[LAB][SHARE SESSION] duplicate encrypted envelope rejected')
                if expected_rejection not in replay_log.read_text(errors='replace'):
                    raise RuntimeError('receiver did not report duplicate rejection')
                items = originals
            else:
                items = [item for ident, item in server.uploads.items()
                         if ident != server.invalid_before_id]
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
                expected_fingerprint = fingerprint
                if args.replace_recipient and item['manifest']['safeDisplayFilename'] == 'image-b.png':
                    expected_fingerprint = json.loads((root / 'receiver-public.json.rotated').read_text())['fingerprint']
                if encryption.get('mode') != 'end-to-end' or encryption.get('algorithm') != 'aes-256-gcm':
                    raise RuntimeError('a transfer was not end-to-end encrypted')
                if (encryption.get('recipientKeyFingerprint') != expected_fingerprint
                        or item['manifest']['recipientId'] != 'labrx'):
                    raise RuntimeError('transfer was not bound to the pinned recipient')
                nonces.add(encryption.get('nonceBase64'))
                key_ids.add(encryption.get('keyId'))
                if item['payload'].startswith(b'\x89PNG\r\n\x1a\n'):
                    raise RuntimeError('provider received a plaintext PNG')
            if None in nonces or None in key_ids or len(nonces) != 2 or len(key_ids) != 2:
                raise RuntimeError('encrypted transfers reused a nonce or ephemeral key')
            if os.environ.get('DECODIUM_LAB_VALIDATE_OPENAPI') == '1':
                # Optional external validator; ordinary application labs do not
                # depend on Python YAML or jsonschema packages.
                import yaml
                from jsonschema import Draft202012Validator
                schema_path = Path(__file__).resolve().parents[3] / 'docs/sstv/remote-sharing-openapi.yaml'
                document = yaml.safe_load(schema_path.read_text())
                schema = {'$ref': '#/components/schemas/ManifestV1',
                          'components': document['components']}
                validator = Draft202012Validator(schema)
                for item in items:
                    manifest = item['manifest']
                    validator.validate(manifest)
                    encryption = manifest['encryption']
                    for section, field, value in (
                            ('transport', 'providerCanReadContent', True),
                            ('encryption', 'nonceBase64', 'AA=='),
                            ('encryption', 'keyId', ''),
                            ('encryption', 'recipientKeyFingerprint', ''),
                            ('encryption', 'manifestBoundAsAuthenticatedData', False)):
                        altered = json.loads(json.dumps(manifest))
                        altered[section][field] = value
                        if validator.is_valid(altered):
                            raise RuntimeError('OpenAPI accepted invalid E2EE field ' + field)
                    tls_control = json.loads(json.dumps(manifest))
                    tls_control['encryption'] = {
                        'algorithm': 'none', 'downgradeProtected': True,
                        'keyId': '', 'manifestBoundAsAuthenticatedData': False,
                        'mode': 'transport-tls', 'nonceBase64': '',
                        'recipientKeyFingerprint': ''}
                    tls_control['transport']['providerCanReadContent'] = True
                    validator.validate(tls_control)
                    for section, field, value in (
                            ('transport', 'providerCanReadContent', False),
                            ('encryption', 'nonceBase64', 'AAAAAAAAAAAAAAAA'),
                            ('encryption', 'keyId', 'unexpected'),
                            ('encryption', 'manifestBoundAsAuthenticatedData', True)):
                        altered = json.loads(json.dumps(tls_control))
                        altered[section][field] = value
                        if validator.is_valid(altered):
                            raise RuntimeError('OpenAPI accepted invalid TLS-only field ' + field)
                    if args.signed:
                        for field in ('senderAuthAlgorithm', 'senderKeyFingerprint', 'senderSignatureBase64'):
                            altered = json.loads(json.dumps(manifest))
                            del altered['encryption'][field]
                            if validator.is_valid(altered):
                                raise RuntimeError('OpenAPI accepted incomplete sender authentication')
                        altered = json.loads(json.dumps(manifest))
                        altered['encryption']['mode'] = 'transport-tls'
                        if validator.is_valid(altered):
                            raise RuntimeError('OpenAPI accepted sender authentication with TLS-only mode')
                        altered = json.loads(json.dumps(manifest))
                        altered['encryption']['algorithm'] = 'xchacha20-poly1305'
                        if validator.is_valid(altered):
                            raise RuntimeError('OpenAPI accepted sender authentication with the wrong algorithm')
                    elif any(field in encryption for field in ('senderAuthAlgorithm', 'senderKeyFingerprint', 'senderSignatureBase64')):
                        raise RuntimeError('unsigned OpenAPI control contained sender authentication')
                print('PASS: actual A/B manifests match OpenAPI; invalid E2EE metadata rejected'
                      + ('; invalid sender-authentication controls rejected' if args.signed else ' (unsigned)'))
        if args.rotate_recipient and '[LAB][SHARE SESSION] receiver identity rotated before transfer' not in (root / 'receiver.log').read_text(errors='replace'):
            raise RuntimeError('receiver did not execute the rotation')
        if args.replace_recipient:
            if '[LAB][SHARE SESSION] recipient pin replaced between images' not in (root / 'sender.log').read_text(errors='replace'):
                raise RuntimeError('sender did not replace recipient pin')
            old = json.loads((root / 'receiver-public.json').read_text())['fingerprint']
            new = json.loads((root / 'receiver-public.json.rotated').read_text())['fingerprint']
            with server.lock:
                observed = {item['manifest']['safeDisplayFilename']: item['manifest']['encryption']['recipientKeyFingerprint']
                            for item in server.uploads.values()}
            if old == new or observed.get('image-a.png') != old or observed.get('image-b.png') != new:
                raise RuntimeError('A/B envelopes did not use the old and new recipient pins')
        if args.restart_sender_pin:
            with server.lock:
                prior_ids = set(server.uploads)
            shutil.copyfile(source_a, root / 'restart-a.jpg')
            shutil.copyfile(source_b, root / 'restart-b.jpg')
            restart_config = dict(sessions['sender'], resumeExistingProvider=True, reuseExistingPin=True,
                                  replaceRecipient=False, expectedUploads=4,
                                  imageA=str(root / 'restart-a.jpg'), imageB=str(root / 'restart-b.jpg'),
                                  result=str(root / 'sender-restart-ok'))
            restart_file = root / 'sender-restart.json'
            restart_file.write_text(json.dumps(restart_config))
            env = base_env.copy()
            env.update(QT_QUICK_BACKEND='software', DECODIUM_LAB_SIGNED_ENVELOPE='1',
                       DECODIUM_LAB_SETTINGS_DIR=str(root / 'sender-settings'))
            cmd = [str(binary), '--test-mode', '--rig-name', 'SHARE' + suffix + 'sender',
                   '--lab-callsign', 'LABTX', '--lab-no-cat', '--lab-no-monitor',
                   '--lab-share-keychain', str(root / 'sender.keychain-db'), '--lab-share-session-file', str(restart_file)]
            with (root / 'sender-restart.log').open('wb') as log:
                restart = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=105)
            if (restart.returncode != 0 or not (root / 'sender-restart-ok').exists()
                    or 'reusing stored recipient pin' not in (root / 'sender-restart.log').read_text(errors='replace')):
                raise RuntimeError('sender did not reuse its saved pin after restart')
            with server.lock:
                new_items = [item for ident, item in server.uploads.items() if ident not in prior_ids]
            if (len(new_items) != 2 or any(not item['complete'] or
                    item['manifest']['encryption'].get('recipientKeyFingerprint') != new or
                    not item['manifest']['encryption'].get('senderSignatureBase64') for item in new_items)):
                raise RuntimeError('restarted sender did not encrypt both images to saved new pin')
            (root / 'out-restarted').mkdir()
            receiver_config = dict(sessions['receiver'], resumeExistingProvider=True, rotateRecipient=False,
                                   restartImagesOnly=True, identity=str(root / 'receiver-restart-public.json'),
                                   outputDir=str(root / 'out-restarted'), result=str(root / 'receiver-restart-complete'))
            receiver_file = root / 'receiver-restart-transfer.json'
            receiver_file.write_text(json.dumps(receiver_config))
            env['DECODIUM_LAB_SETTINGS_DIR'] = str(root / 'receiver-settings')
            cmd = [str(binary), '--test-mode', '--rig-name', 'SHARE' + suffix + 'receiver',
                   '--lab-callsign', 'LABRX', '--lab-no-cat', '--lab-no-monitor',
                   '--lab-share-keychain', str(root / 'receiver.keychain-db'), '--lab-share-session-file', str(receiver_file)]
            with (root / 'receiver-restart-transfer.log').open('wb') as log:
                restarted_receiver = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=105)
            if restarted_receiver.returncode or not (root / 'receiver-restart-complete').exists():
                raise RuntimeError('restarted receiver did not decrypt new uploads')
            for label, original in (('a', source_a), ('b', source_b)):
                output = root / 'out-restarted' / ('restart-' + label + '.png')
                reference, received = image_bytes(original), image_bytes(output)
                errors = [abs(a - b) for a, b in zip(reference, received)]
                if len(reference) != len(received) or not errors or max(errors) > 5 or sum(errors) / len(errors) > 1:
                    raise RuntimeError('restarted receiver image ' + label + ' differs')
        if args.replace_sender_signing:
            original_signing = json.loads((root / 'sender-signing.json').read_text())['fingerprint']
            if args.retired_unseen:
                with server.lock:
                    before_retired_ids = set(server.uploads)
                for label, source in (('a', source_a), ('b', source_b)):
                    shutil.copyfile(source, root / ('retired-' + label + '.jpg'))
                old_session = dict(sessions['sender'], resumeExistingProvider=True, reuseExistingPin=True,
                    expectedUploads=4, signingIdentity=str(root / 'retired-signing.json'),
                    imageA=str(root / 'retired-a.jpg'), imageB=str(root / 'retired-b.jpg'),
                    result=str(root / 'retired-sender-ok'))
                old_file = root / 'retired-sender.json'
                old_file.write_text(json.dumps(old_session))
                old_env = base_env.copy()
                old_env.update(QT_QUICK_BACKEND='software', DECODIUM_LAB_SIGNED_ENVELOPE='1',
                               DECODIUM_LAB_SETTINGS_DIR=str(root / 'sender-settings'))
                old_cmd = [str(binary), '--test-mode', '--rig-name', 'SHARE' + suffix + 'sender',
                           '--lab-callsign', 'LABTX', '--lab-no-cat', '--lab-no-monitor',
                           '--lab-share-keychain', str(root / 'sender.keychain-db'), '--lab-share-session-file', str(old_file)]
                with (root / 'retired-sender.log').open('wb') as old_log:
                    old_run = subprocess.run(old_cmd, env=old_env, stdout=old_log, stderr=subprocess.STDOUT, timeout=105)
                if (old_run.returncode or not (root / 'retired-sender-ok').exists()
                        or json.loads((root / 'retired-signing.json').read_text())['fingerprint'] != original_signing):
                    raise RuntimeError('old-key undelivered images were not uploaded')
                with server.lock:
                    old_items = [(ident, item) for ident, item in server.uploads.items() if ident not in before_retired_ids]
                    initial_canonical = {item['canonical'] for ident, item in server.uploads.items() if ident in before_retired_ids}
                    initial_payloads = {item['payload'] for ident, item in server.uploads.items() if ident in before_retired_ids}
                if len(old_items) != 2 or any(not item['complete'] or item['manifest']['encryption'].get('senderKeyFingerprint') != original_signing for _, item in old_items):
                    raise RuntimeError('old-key undelivered images have unexpected signing identities')
                if any(item['canonical'] in initial_canonical or item['payload'] in initial_payloads for _, item in old_items):
                    raise RuntimeError('old-key undelivered envelope duplicates an earlier transfer')
                old_public = bytes.fromhex(json.loads((root / 'sender-signing.json').read_text())['publicKey'])
                (root / 'retired-public.der').write_bytes(old_public)
                subprocess.run(['openssl', 'pkey', '-pubin', '-inform', 'DER', '-in', str(root / 'retired-public.der'),
                                '-out', str(root / 'retired-public.pem')], check=True, capture_output=True, timeout=10)
                for index, (_, item) in enumerate(old_items):
                    signed = json.loads(json.dumps(item['manifest']))
                    encoded = signed['encryption'].pop('senderSignatureBase64')
                    message = b'Decodium-SSTV-sender-signature-v1\0' + json.dumps(
                        signed, ensure_ascii=False, sort_keys=True, separators=(',', ':')).encode('utf-8')
                    message_file = root / ('retired-message-' + str(index))
                    signature_file = root / ('retired-signature-' + str(index))
                    message_file.write_bytes(message)
                    signature_file.write_bytes(base64.b64decode(encoded, validate=True))
                    subprocess.run(['openssl', 'dgst', '-sha256', '-verify', str(root / 'retired-public.pem'),
                                    '-signature', str(signature_file), str(message_file)],
                                   check=True, capture_output=True, timeout=10)
                selected = [ident for ident, item in old_items if item['manifest']['safeDisplayFilename'] == 'retired-' + args.retired_unseen + '.png']
                if len(selected) != 1:
                    raise RuntimeError('missing undelivered old-key image')
                server.replay_id = selected[0]
                server.replay_id_path = root / 'retired-signature-id'
                server.replay_id_path.write_text(server.replay_id, encoding='ascii')
            with server.lock:
                prior_ids = set(server.uploads)
            for label, source in (('a', source_a), ('b', source_b)):
                shutil.copyfile(source, root / ('restart-' + label + '.jpg'))
            (root / 'out-rekey').mkdir()
            identity2 = str(root / 'receiver-rekey-public.json')
            signing2 = str(root / 'sender-rekey-signing.json')
            ready2 = str(root / 'sender-rekey-ready')
            rekey_sessions = {
                'receiver': dict(sessions['receiver'], resumeExistingProvider=True, restartImagesOnly=True,
                                 identity=identity2, signingIdentity=signing2, signingReady=ready2,
                                 oldSigningFingerprint=original_signing, outputDir=str(root / 'out-rekey'),
                                 result=str(root / 'receiver-rekey-ok')),
                'sender': dict(sessions['sender'], identity=identity2, signingIdentity=signing2, signingReady=ready2,
                               imageA=str(root / 'restart-a.jpg'), imageB=str(root / 'restart-b.jpg'),
                               result=str(root / 'sender-rekey-ok')),
            }
            if args.rotate_sender_signing:
                rekey_sessions['sender'].update(resumeExistingProvider=True, reuseExistingPin=True,
                                                expectedUploads=6 if args.retired_unseen else 4, rotateSenderSigning=True,
                                                oldSigningFingerprint=original_signing)
            if args.retired_signature:
                server.replay_target = args.retired_signature
                server.replay_after = Path(ready2)
                server.replay_id_path = root / 'retired-signature-id'
                rekey_sessions['receiver'].update(replayTarget=args.retired_signature,
                    replayExpectInvalidSignature=True, replayIncomingIdFile=str(server.replay_id_path))
            if args.retired_unseen:
                rekey_sessions['receiver'].update(replayTarget=args.retired_unseen, retiredUnseen=True,
                    replayExpectInvalidSignature=True, replayIncomingIdFile=str(server.replay_id_path))
            rekey_processes = []
            for role in ('receiver', 'sender'):
                config = root / (role + '-rekey.json')
                config.write_text(json.dumps(rekey_sessions[role]))
                env = base_env.copy()
                sender_settings = 'sender-settings' if args.rotate_sender_signing else 'sender-rekey-settings'
                env.update(QT_QUICK_BACKEND='software', DECODIUM_LAB_SIGNED_ENVELOPE='1',
                           DECODIUM_LAB_SETTINGS_DIR=str(root / ('receiver-settings' if role == 'receiver' else sender_settings)))
                sender_keychain = 'sender.keychain-db' if args.rotate_sender_signing else 'sender-rekey.keychain-db'
                keychain = root / ('receiver.keychain-db' if role == 'receiver' else sender_keychain)
                cmd = [str(binary), '--test-mode', '--rig-name', 'SHARE' + suffix + role,
                       '--lab-callsign', 'LABRX' if role == 'receiver' else 'LABTX', '--lab-no-cat', '--lab-no-monitor',
                       '--lab-share-keychain', str(keychain), '--lab-share-session-file', str(config)]
                with (root / (role + '-rekey.log')).open('wb') as log:
                    process = subprocess.Popen(cmd, env=env, stdout=log, stderr=subprocess.STDOUT)
                processes.append(process)
                rekey_processes.append(process)
            for process in rekey_processes:
                if process.wait(timeout=105):
                    raise RuntimeError('sender signing replacement session failed')
            new_signing = json.loads(Path(signing2).read_text())['fingerprint']
            receiver_log = (root / 'receiver-rekey.log').read_text(errors='replace')
            if (new_signing == original_signing or not (root / 'receiver-rekey-ok').exists()
                    or not (root / 'sender-rekey-ok').exists()
                    or '[LAB][SHARE SESSION] verified sender signing pin replaced' not in receiver_log):
                raise RuntimeError('sender signing pin replacement was not verified')
            if args.rotate_sender_signing and '[LAB][SHARE SESSION] sender signing identity rotated' not in (root / 'sender-rekey.log').read_text(errors='replace'):
                raise RuntimeError('sender signing identity rotation was not observed')
            if args.retired_signature:
                if ('[LAB][SHARE SESSION] replay sender signature rejected' not in receiver_log
                        or not server.replay_id or server.replay_downloads != 0):
                    raise RuntimeError('retired signature was not rejected before content download')
                with server.lock:
                    retired = server.uploads[server.replay_id]
                    originals = [item for ident, item in server.uploads.items() if ident in prior_ids
                                 and item['manifest']['safeDisplayFilename'] == 'image-' + args.retired_signature + '.png']
                    if (len(originals) != 1 or retired['canonical'] != originals[0]['canonical']
                            or retired['payload'] != originals[0]['payload']
                            or retired['manifest']['encryption']['senderKeyFingerprint'] != original_signing):
                        raise RuntimeError('retired envelope was not an exact old-key replay')
            if args.retired_unseen and ('[LAB][SHARE SESSION] replay sender signature rejected' not in receiver_log
                                        or server.replay_downloads != 0):
                raise RuntimeError('previously unseen old-key envelope was not rejected before content download')
            with server.lock:
                items = [item for ident, item in server.uploads.items() if ident not in prior_ids and ident != server.replay_id]
            if len(items) != 2 or any(not item['complete'] or item['manifest']['encryption'].get('senderKeyFingerprint') != new_signing for item in items):
                raise RuntimeError('new sender uploads did not use the replacement signing identity')
            for label, source in (('a', source_a), ('b', source_b)):
                original, received = image_bytes(source), image_bytes(root / 'out-rekey' / ('restart-' + label + '.png'))
                differences = [abs(a - b) for a, b in zip(original, received)]
                if len(original) != len(received) or not differences or max(differences) > 5 or sum(differences) / len(differences) > 1:
                    raise RuntimeError('replacement signing image differs: ' + label)
            if args.rotate_sender_signing:
                persist_config = dict(sessions['sender'], resumeExistingProvider=True, signingIdentityOnly=True,
                                      identity=identity2, signingIdentity=str(root / 'signing-persist.json'),
                                      signingReady=str(root / 'signing-persist-ready'), result=str(root / 'signing-persist-ok'))
                persist_file = root / 'signing-persist-session.json'
                persist_file.write_text(json.dumps(persist_config))
                env = base_env.copy()
                env.update(QT_QUICK_BACKEND='software', DECODIUM_LAB_SIGNED_ENVELOPE='1',
                           DECODIUM_LAB_SETTINGS_DIR=str(root / 'sender-settings'))
                cmd = [str(binary), '--test-mode', '--rig-name', 'SHARE' + suffix + 'sender', '--lab-callsign', 'LABTX',
                       '--lab-no-cat', '--lab-no-monitor', '--lab-share-keychain', str(root / 'sender.keychain-db'),
                       '--lab-share-session-file', str(persist_file)]
                with (root / 'signing-persist.log').open('wb') as log:
                    persist = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=105)
                if (persist.returncode or not (root / 'signing-persist-ok').exists()
                        or json.loads((root / 'signing-persist.json').read_text())['fingerprint'] != new_signing):
                    raise RuntimeError('rotated signing identity did not persist after application restart')
            print('PASS: replaced sender signing identity and decrypted fresh A/B images', flush=True)
        if args.busy_refresh:
            receiver_log = (root / 'receiver.log').read_text(errors='replace')
            laboratory_retry = '[LAB][SHARE SESSION] inbox busy; polling will retry' in receiver_log
            if (server.busy_refresh_delays < 1
                    or '[SHARE] inbox refresh deferred while provider busy' not in receiver_log
                    or laboratory_retry != args.busy_exhaust):
                raise RuntimeError('production inbox retry was not exercised during delayed download')
        successful = True
        print('PASS: two Decodium applications transferred and decrypted images A/B over verified loopback HTTPS'
              + ((' and rejected a re-signed replay of image ' if args.replay_resigned else
                  ' and rejected a high-S replay of image ' if args.replay_high_s else ' and rejected an exact replay of image ')
                 + args.replay_envelope.upper() if args.replay_envelope else ''))
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
