#!/usr/bin/env python3
"""Exercise the loopback fixture through the production-linked REST actor."""
import argparse
import json
from pathlib import Path
import ssl
import subprocess
import threading
import urllib.error
import urllib.request

from share_https_fixture import ShareFixture


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('actor', type=Path)
    parser.add_argument('certificate', type=Path)
    parser.add_argument('key', type=Path)
    args = parser.parse_args()
    certificate = args.certificate.read_text()
    public = '8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a'
    private = '77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a'
    for data in (b'independent-test-a', b'independent-test-b-with-different-bytes'):
        server = ShareFixture(args.certificate, args.key)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            try:
                urllib.request.urlopen(server.url + 'api/v1/capabilities', timeout=5)
            except urllib.error.URLError as error:
                if not isinstance(error.reason, ssl.SSLCertVerificationError):
                    raise
            else:
                raise RuntimeError('untrusted certificate was accepted')
            def actor(role, values):
                values.update({'url': server.url, 'ca': certificate})
                return subprocess.run([str(args.actor), '--encrypted-actor', role],
                    input=json.dumps(values).encode(), stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE, timeout=15)
            sent = actor('send', {'key': public, 'data': data.hex()})
            if sent.returncode:
                raise RuntimeError('encrypted upload failed: ' + sent.stderr.decode(errors='replace'))
            received = actor('receive', {'key': private})
            if received.returncode or bytes.fromhex(received.stdout.decode()) != data:
                raise RuntimeError('encrypted download did not reconstruct input')
            with server.lock:
                assert len(server.uploads) == 1
                item = next(iter(server.uploads.values()))
                payload = bytearray(item['payload'])
                if data in payload:
                    raise RuntimeError('provider saw plaintext in encrypted payload')
                payload[0] ^= 1
                old_hash = item['manifest']['sha256']
                import hashlib
                new_hash = hashlib.sha256(payload).hexdigest()
                item['manifest']['sha256'] = new_hash
                item['canonical'] = item['canonical'].replace(old_hash.encode(), new_hash.encode())
                item['payload'] = bytes(payload)
            damaged = actor('receive', {'key': private})
            if damaged.returncode == 0 or damaged.stdout:
                raise RuntimeError('modified ciphertext was accepted')
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)
    print('PASS: two verified-HTTPS encrypted transfers; untrusted TLS and altered ciphertext rejected')


if __name__ == '__main__':
    main()
