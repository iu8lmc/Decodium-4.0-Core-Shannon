#!/usr/bin/env python3
"""Loopback-only, in-memory HTTPS fixture for Decodium sharing tests.

This is deliberately not an authentication service and must never be used as
a real sharing provider. Its certificate and private key are public fixtures.
"""
import base64
from datetime import datetime, timedelta, timezone
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import base64
import re
import secrets
import ssl
import threading
from urllib.parse import urlsplit


def _utc_now():
    return datetime.now(timezone.utc).isoformat(timespec='milliseconds').replace('+00:00', 'Z')


class ShareFixture(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, certificate, private_key, tamper=False, tamper_manifest=False, first_output=None, downgrade_manifest=None, expiry_target=None, expiry_marker=None, expiry_after_chunk=False, swap_payload=None, replay_target=None, replay_after=None, tamper_manifest_field='sstvMode'):
        super().__init__(('127.0.0.1', 0), _Handler)
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.load_cert_chain(str(certificate), str(private_key))
        self.socket = tls.wrap_socket(self.socket, server_side=True)
        self.lock = threading.Lock()
        self.uploads = {}
        self.tamper = tamper
        self.tamper_manifest = tamper_manifest
        if tamper_manifest_field not in ('sstvMode', 'expiresUtc', 'keyId', 'senderId', 'senderSignatureBase64', 'senderAuth'):
            raise ValueError('unsupported manifest tamper field')
        self.tamper_manifest_field = tamper_manifest_field
        self.downgrade_manifest = downgrade_manifest
        self.downgraded_id = None
        self.downgraded_downloads = 0
        self.stripped_signature_downloads = 0
        self.first_output = first_output
        self.tamper_count = 0
        self.expiry_target = expiry_target
        self.expiry_marker = expiry_marker
        self.expiry_count = 0
        self.expiry_after_chunk = expiry_after_chunk
        self.expiry_offsets = []
        self.swap_payload = swap_payload
        self.replay_target = replay_target
        self.replay_after = replay_after
        self.replay_id = None
        self.replay_downloads = 0

    @property
    def url(self):
        return 'https://127.0.0.1:{}/'.format(self.server_port)


class _Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *args):
        pass

    def _body(self):
        try:
            size = int(self.headers.get('Content-Length', '0'))
        except ValueError:
            return None
        if size < 0 or size > 8 * 1024 * 1024:
            return None
        body = self.rfile.read(size)
        return body if len(body) == size else None

    def _reply(self, status, body=b'', content_type='application/json', headers=()):
        self.send_response(status)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Connection', 'close')
        for key, value in headers:
            self.send_header(key, value)
        self.end_headers()
        self.wfile.write(body)
        self.close_connection = True

    def _json(self, status, obj):
        self._reply(status, json.dumps(obj, separators=(',', ':')).encode())

    def do_POST(self):
        path = urlsplit(self.path).path
        body = self._body()
        if body is None:
            return self._reply(400)
        if path == '/api/uploads':
            try:
                manifest = json.loads(body)
                if not isinstance(manifest, dict) or manifest['providerId'] != 'rest-encrypted':
                    return self._reply(400)
                for field in ('senderId', 'recipientId', 'createdUtc', 'expiresUtc', 'sha256', 'byteSize'):
                    if field not in manifest:
                        return self._reply(400)
            except (UnicodeError, ValueError, KeyError):
                return self._reply(400)
            identifier = secrets.token_hex(12)
            with self.server.lock:
                self.server.uploads[identifier] = {'manifest': manifest, 'canonical': body,
                                                   'payload': b'', 'complete': False,
                                                   'receivedUtc': _utc_now()}
            return self._json(201, {'uploadId': identifier, 'committedBytes': 0})
        match = re.fullmatch(r'/api/uploads/([a-f0-9]{24})/complete', path)
        if match:
            with self.server.lock:
                item = self.server.uploads.get(match.group(1))
                if not item:
                    return self._reply(404)
                payload = item['payload']
                digest = hashlib.sha256(payload).hexdigest()
                if digest != item['manifest']['sha256'] or len(payload) != item['manifest']['byteSize']:
                    return self._reply(400)
                if (self.server.tamper is True or
                    (self.server.tamper in ('a', 'b') and
                     item['manifest']['safeDisplayFilename'] ==
                     'image-{}.png'.format(self.server.tamper))):
                    altered = bytearray(payload)
                    altered[0] ^= 1
                    changed_digest = hashlib.sha256(altered).hexdigest()
                    item['canonical'] = item['canonical'].replace(digest.encode(), changed_digest.encode())
                    item['manifest']['sha256'] = changed_digest
                    item['payload'] = bytes(altered)
                    self.server.tamper_count += 1
                if (self.server.tamper_manifest
                        and item['manifest']['safeDisplayFilename'] ==
                            'image-{}.png'.format(self.server.tamper_manifest)):
                    field = self.server.tamper_manifest_field
                    encryption = item['manifest']['encryption']
                    item['signed_before_tamper'] = all(encryption.get(key) for key in
                        ('senderAuthAlgorithm', 'senderKeyFingerprint', 'senderSignatureBase64'))
                    if field == 'senderAuth':
                        if not item['signed_before_tamper']:
                            return self._reply(400)
                        for key in ('senderAuthAlgorithm', 'senderKeyFingerprint', 'senderSignatureBase64'):
                            del encryption[key]
                        item['canonical'] = json.dumps(item['manifest'], ensure_ascii=False,
                            sort_keys=True, separators=(',', ':')).encode('utf-8')
                    else:
                        container = encryption if field in ('keyId', 'senderSignatureBase64') else item['manifest']
                        before_value = container[field]
                        if field == 'expiresUtc':
                            expiry = datetime.fromisoformat(before_value.replace('Z', '+00:00'))
                            after_value = (expiry - timedelta(minutes=5)).isoformat(timespec='milliseconds').replace('+00:00', 'Z')
                        elif field == 'keyId':
                            after_value = 'x25519-v1-' + '09' + '00' * 31
                        elif field == 'senderSignatureBase64':
                            signature = bytearray(base64.b64decode(before_value, validate=True))
                            signature[-1] ^= 1
                            after_value = base64.b64encode(signature).decode('ascii')
                        else:
                            after_value = 'altered'
                        prefix = json.dumps(field).encode() + b':'
                        old = prefix + json.dumps(before_value, ensure_ascii=False).encode()
                        new = prefix + json.dumps(after_value, ensure_ascii=False).encode()
                        if item['canonical'].count(old) != 1 or before_value == after_value:
                            return self._reply(400)
                        item['canonical'] = item['canonical'].replace(old, new)
                        container[field] = after_value
                    self.server.tamper_count += 1
                if (self.server.downgrade_manifest
                        and item['manifest']['safeDisplayFilename'] ==
                            'image-{}.png'.format(self.server.downgrade_manifest)):
                    item['manifest']['encryption'] = {
                        'algorithm': 'none', 'downgradeProtected': True,
                        'keyId': '', 'manifestBoundAsAuthenticatedData': False,
                        'mode': 'transport-tls', 'nonceBase64': '',
                        'recipientKeyFingerprint': ''}
                    item['manifest']['transport']['providerCanReadContent'] = True
                    item['canonical'] = json.dumps(item['manifest'], ensure_ascii=False,
                        sort_keys=True, separators=(',', ':')).encode('utf-8')
                    self.server.downgraded_id = match.group(1)
                    self.server.tamper_count += 1
                item['complete'] = True
            return self._json(200, {'byteSize': len(payload), 'remoteObjectId': match.group(1),
                                    'sha256': digest})
        return self._reply(404)

    def do_PUT(self):
        path = urlsplit(self.path).path
        match = re.fullmatch(r'/api/uploads/([a-f0-9]{24})/content', path)
        body = self._body()
        if not match or body is None:
            return self._reply(400)
        with self.server.lock:
            item = self.server.uploads.get(match.group(1))
            if not item or item['complete']:
                return self._reply(404)
            # The fixture is intentionally limited to one bounded chunk.
            if item['payload'] or len(body) != item['manifest']['byteSize']:
                return self._reply(400)
            item['payload'] = body
        return self._json(200, {'committedBytes': len(body)})

    def do_GET(self):
        parsed = urlsplit(self.path)
        if parsed.path == '/api/v1/capabilities':
            return self._json(200, {'protocolVersion': 1, 'recipientLookup': True,
                'chunkedUpload': True, 'resumableUpload': True, 'download': True,
                'acknowledgement': True, 'rejection': True, 'revocation': False,
                'remoteDelete': False, 'incomingList': True,
                'endToEndEncryptionEnvelope': True, 'strictTlsRequired': True,
                'maximumChunkBytes': 4 * 1024 * 1024,
                'maximumResponseBytes': 64 if self.server.expiry_after_chunk else 65536})
        recipient = re.fullmatch(r'/api/v1/recipients/([a-z0-9._+-]+)', parsed.path)
        if recipient:
            return self._json(200, {'providerId': 'rest-encrypted',
                'stableRecipientId': recipient.group(1), 'displayCallsign': '',
                'displayName': 'Lab recipient', 'publicEncryptionKey': '',
                'publicKeyFingerprint': '', 'verification': 'provider-verified',
                'trust': 'trusted'})
        if parsed.path == '/api/v1/inbox':
            with self.server.lock:
                if (self.server.replay_target and self.server.replay_id is None
                        and self.server.replay_after.exists()):
                    matches = [item for item in self.server.uploads.values()
                               if item['complete'] and item['manifest']['safeDisplayFilename']
                               == 'image-{}.png'.format(self.server.replay_target)]
                    if len(matches) != 1:
                        return self._reply(500)
                    self.server.replay_id = secrets.token_hex(12)
                    self.server.uploads[self.server.replay_id] = dict(matches[0])
                if self.server.swap_payload and self.server.tamper_count == 0:
                    completed = {item['manifest']['safeDisplayFilename']: item
                                 for item in self.server.uploads.values() if item['complete']}
                    target = completed.get('image-{}.png'.format(self.server.swap_payload))
                    other = completed.get('image-{}.png'.format('b' if self.server.swap_payload == 'a' else 'a'))
                    if not target or not other:
                        return self._json(200, {'items': []})
                    if (target['payload'] == other['payload']
                            or target['manifest']['encryption']['nonceBase64'] == other['manifest']['encryption']['nonceBase64']
                            or target['manifest']['encryption']['keyId'] == other['manifest']['encryption']['keyId']):
                        return self._reply(500)
                    target['payload'] = other['payload']
                    target['manifest']['byteSize'] = len(other['payload'])
                    target['manifest']['sha256'] = hashlib.sha256(other['payload']).hexdigest()
                    target['canonical'] = json.dumps(target['manifest'], ensure_ascii=False,
                        sort_keys=True, separators=(',', ':')).encode('utf-8')
                    self.server.tamper_count += 1
                rows = [{'incomingId': identifier, 'providerId': item['manifest']['providerId'],
                         'senderId': item['manifest']['senderId'],
                         'manifestSha256': hashlib.sha256(item['canonical']).hexdigest(),
                         'canonicalManifestJson': item['canonical'].decode('utf-8'),
                         'byteSize': len(item['payload']), 'receivedUtc': item['receivedUtc'],
                         'expiresUtc': item['manifest']['expiresUtc']}
                        for identifier, item in self.server.uploads.items() if item['complete']
                        and not (self.server.first_output is not None
                                 and not self.server.first_output.exists()
                                 and item['manifest']['safeDisplayFilename'] == 'image-b.png')]
            return self._json(200, {'items': rows})
        match = re.fullmatch(r'/api/v1/inbox/([a-f0-9]{24})/content', parsed.path)
        if match:
            with self.server.lock:
                if match.group(1) == self.server.replay_id:
                    self.server.replay_downloads += 1
                if match.group(1) == self.server.downgraded_id:
                    self.server.downgraded_downloads += 1
                item = self.server.uploads.get(match.group(1))
                if (item and item.get('signed_before_tamper')
                        and self.server.tamper_manifest_field == 'senderAuth'):
                    self.server.stripped_signature_downloads += 1
                if not item or not item['complete']:
                    return self._reply(404)
                payload = item['payload']
                match_range = re.fullmatch(r'bytes=(\d+)-(\d+)', self.headers.get('Range', ''))
                if not match_range:
                    return self._reply(400)
                first, last = map(int, match_range.groups())
                if first >= len(payload) or last < first:
                    return self._reply(416)
                last = min(last, len(payload) - 1)
                if (self.server.expiry_target
                        and item['manifest']['safeDisplayFilename'] == 'image-' + self.server.expiry_target + '.png'):
                    self.server.expiry_offsets.append(first)
                    if not self.server.expiry_after_chunk or first > 0:
                        self.server.expiry_marker.write_text('expire\n')
                        self.server.expiry_count += 1
            body = payload[first:last + 1]
            digest = base64.b64encode(hashlib.sha256(body).digest()).decode('ascii')
            return self._reply(206, body, 'application/octet-stream',
                [('Content-Range', 'bytes {}-{}/{}'.format(first, last, len(payload))),
                 ('Digest', 'sha-256=' + digest)])
        return self._reply(404)
