#!/usr/bin/env python3
"""Loopback-only, in-memory HTTPS fixture for Decodium sharing tests.

This is deliberately not an authentication service and must never be used as
a real sharing provider. Its certificate and private key are public fixtures.
"""
import base64
from datetime import datetime, timezone
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
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

    def __init__(self, certificate, private_key, tamper=False, tamper_manifest=False, first_output=None):
        super().__init__(('127.0.0.1', 0), _Handler)
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.load_cert_chain(str(certificate), str(private_key))
        self.socket = tls.wrap_socket(self.socket, server_side=True)
        self.lock = threading.Lock()
        self.uploads = {}
        self.tamper = tamper
        self.tamper_manifest = tamper_manifest
        self.first_output = first_output
        self.tamper_count = 0

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
                if self.server.tamper:
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
                    before = json.dumps(item['manifest']['sstvMode'], ensure_ascii=False).encode()
                    old = b'"sstvMode":' + before
                    new = b'"sstvMode":"altered"'
                    if item['canonical'].count(old) != 1 or item['manifest']['sstvMode'] == 'altered':
                        return self._reply(400)
                    item['canonical'] = item['canonical'].replace(old, new)
                    item['manifest']['sstvMode'] = 'altered'
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
                'maximumChunkBytes': 4 * 1024 * 1024, 'maximumResponseBytes': 65536})
        recipient = re.fullmatch(r'/api/v1/recipients/([a-z0-9._+-]+)', parsed.path)
        if recipient:
            return self._json(200, {'providerId': 'rest-encrypted',
                'stableRecipientId': recipient.group(1), 'displayCallsign': '',
                'displayName': 'Lab recipient', 'publicEncryptionKey': '',
                'publicKeyFingerprint': '', 'verification': 'provider-verified',
                'trust': 'trusted'})
        if parsed.path == '/api/v1/inbox':
            with self.server.lock:
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
                item = self.server.uploads.get(match.group(1))
                if not item or not item['complete']:
                    return self._reply(404)
                payload = item['payload']
            digest = base64.b64encode(hashlib.sha256(payload).digest()).decode('ascii')
            return self._reply(206, payload, 'application/octet-stream',
                [('Content-Range', 'bytes 0-{}/{}'.format(len(payload) - 1, len(payload))),
                 ('Digest', 'sha-256=' + digest)])
        return self._reply(404)
