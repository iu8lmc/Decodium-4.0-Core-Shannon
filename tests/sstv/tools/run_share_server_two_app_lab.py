#!/usr/bin/env python3
"""macOS-only authenticated two-Decodium test of the experimental server."""
import argparse
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import tempfile
import threading
import time

repo = Path(__file__).resolve().parents[3]
sys.path.insert(0, os.environ.get('DECODIUM_LAB_SERVER_MODULE_DIR', str(repo / 'tools/sstv-share-server')))
from decodium_share_server import Server
from decodium_share_store import ShareStore
from run_share_app_identity_lab import keychains
from run_share_app_transfer_lab import image_bytes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--unsigned', action='store_true')
    parser.add_argument('--binary', type=Path, default=repo / 'build/decodium')
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    root = Path(tempfile.mkdtemp(prefix='decodium-share-server-ab-'))
    cert = repo / 'tests/sstv/fixtures/localhost-test-only.crt'
    key = repo / 'tests/sstv/fixtures/localhost-test-only.key'
    tokens = {'sender': secrets.token_urlsafe(48), 'receiver': secrets.token_urlsafe(48)}
    store = ShareStore(root / 'server.sqlite', 'rest-encrypted')
    store.add_account('labtx', tokens['sender'])
    store.add_account('labrx', tokens['receiver'])
    server = Server(('127.0.0.1', 0), store, str(cert), str(key))
    thread = threading.Thread(target=server.serve_forever)
    thread.start()
    security, core, refs = keychains([root / 'sender.keychain-db', root / 'receiver.keychain-db'])
    processes = []
    try:
        for name in ('a', 'b'):
            shutil.copyfile(repo / f'tests/sstv/fixtures/qsstv-image-{name}.jpg', root / f'image-{name}.jpg')
        (root / 'out').mkdir()
        common = {'endpoint': f'https://127.0.0.1:{server.server_port}/',
                  'certificate': str(cert), 'identity': str(root / 'receiver-public.json')}
        if not args.unsigned:
            common.update(signingIdentity=str(root / 'sender-signing.json'), signingReady=str(root / 'signing-ready'))
        sessions = {'receiver': dict(common, role='receiver', outputDir=str(root / 'out'), result=str(root / 'receiver-ok')),
                    'sender': dict(common, role='sender', recipient='labrx', imageA=str(root / 'image-a.jpg'),
                                   imageB=str(root / 'image-b.jpg'), result=str(root / 'sender-ok'))}
        suffix = secrets.token_hex(4)
        for role in ('receiver', 'sender'):
            config = root / (role + '.json')
            config.write_text(json.dumps(sessions[role]))
            env = os.environ.copy()
            env.update(QT_QUICK_BACKEND='software', DECODIUM_LAB_SETTINGS_DIR=str(root / (role + '-settings')),
                       DECODIUM_LAB_SHARE_BEARER=tokens[role])
            if not args.unsigned:
                env['DECODIUM_LAB_SIGNED_ENVELOPE'] = '1'
            else:
                env.pop('DECODIUM_LAB_SIGNED_ENVELOPE', None)
            cmd = [str(binary), '--test-mode', '--rig-name', 'SHARE' + suffix + role,
                   '--lab-callsign', 'LABRX' if role == 'receiver' else 'LABTX', '--lab-no-cat', '--lab-no-monitor',
                   '--lab-share-keychain', str(root / (role + '.keychain-db')), '--lab-share-session-file', str(config)]
            with (root / (role + '.log')).open('wb') as log:
                processes.append(subprocess.Popen(cmd, env=env, stdout=log, stderr=subprocess.STDOUT))
        deadline = time.monotonic() + 105
        while time.monotonic() < deadline and any(p.poll() is None for p in processes):
            time.sleep(.2)
        if any(p.poll() is None or p.returncode != 0 for p in processes):
            raise RuntimeError('application test failed; artifacts ' + str(root))
        for name in ('a', 'b'):
            original, received = image_bytes(root / f'image-{name}.jpg'), image_bytes(root / 'out' / f'image-{name}.png')
            if len(original) != len(received) or not original or max(abs(a-b) for a,b in zip(original,received)) > 5:
                raise RuntimeError('image differs: ' + name)
        with store.connect() as db:
            rows = db.execute('SELECT * FROM uploads WHERE complete=1').fetchall()
            if len(rows) != 2:
                raise RuntimeError('server did not persist both objects')
            for row in rows:
                manifest = json.loads(row['manifest'])
                encryption = manifest.get('encryption', {})
                if (row['owner'] != 'labtx' or row['recipient'] != 'labrx'
                        or encryption.get('mode') != 'end-to-end'
                        or encryption.get('algorithm') != 'aes-256-gcm'
                        or bool(encryption.get('senderSignatureBase64')) == args.unsigned
                        or len(row['payload']) != manifest['byteSize']):
                    raise RuntimeError('server object is not the expected signed encrypted transfer')
        print('PASS', 'unsigned' if args.unsigned else 'signed', 'encrypted A/B between two Decodium processes and authenticated HTTPS server; artifacts', root)
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
        server.shutdown()
        thread.join()
        server.server_close()
        for ref in refs:
            security.SecKeychainDelete(ref)
            core.CFRelease(ref)


if __name__ == '__main__':
    main()
