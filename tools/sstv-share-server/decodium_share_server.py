#!/usr/bin/env python3
"""Decodium SSTV HTTPS share server. Experimental; see README before use."""
import argparse
import base64
from datetime import datetime, timezone
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import secrets
import sqlite3
import ssl
import sys
import threading
import time
import unicodedata
import uuid
from urllib.parse import parse_qs, urlsplit

from decodium_share_store import ShareStore, StoreError, MAX_MANIFEST, MAX_PAYLOAD

MANIFEST_FIELDS = frozenset(('byteSize', 'callsign', 'chunkCount', 'completion',
    'contentDisposition', 'createdUtc', 'encryption', 'expiresUtc', 'height',
    'mediaSource', 'mediaUtc', 'message', 'mimeType', 'originalFilename',
    'privacy', 'protocolVersion', 'providerId', 'recipientId', 'safeDisplayFilename',
    'senderId', 'sha256', 'sstvMode', 'transferId', 'transport', 'width'))
PRIVACY_FIELDS = frozenset(('automaticIncomingDownloadAllowed', 'automaticUploadAllowed',
    'callsignIncluded', 'exifRetained', 'explicitExpiry', 'gridIncluded', 'locationIncluded',
    'meteredNetworkAllowed', 'publicShare', 'recipientConfirmed'))
ENCRYPTION_FIELDS = frozenset(('algorithm', 'downgradeProtected', 'keyId',
    'manifestBoundAsAuthenticatedData', 'mode', 'nonceBase64', 'recipientKeyFingerprint'))
SIGNATURE_FIELDS = frozenset(('senderAuthAlgorithm', 'senderKeyFingerprint', 'senderSignatureBase64'))


def validate_manifest_shape(obj):
    """Reject structurally malformed objects before they can pollute inboxes.

    Recipients still independently validate the full manifest, key and AEAD.
    """
    def fail():
        raise StoreError(400, 'invalid encrypted manifest')
    if set(obj) != MANIFEST_FIELDS or type(obj['protocolVersion']) is not int or obj['protocolVersion'] != 1:
        fail()
    privacy, transport, callsign, encryption = (obj[k] for k in ('privacy', 'transport', 'callsign', 'encryption'))
    if not all(isinstance(v, dict) for v in (privacy, transport, callsign, encryption)):
        fail()
    if set(privacy) != PRIVACY_FIELDS or any(type(v) is not bool for v in privacy.values()):
        fail()
    if not privacy['explicitExpiry'] or not privacy['recipientConfirmed']:
        fail()
    if set(transport) != {'certificateValidationRequired', 'providerCanReadContent', 'sameOriginRedirectsOnly', 'tlsRequired'}:
        fail()
    if any(type(v) is not bool for v in transport.values()) or transport != {'certificateValidationRequired': True, 'providerCanReadContent': False,
                     'sameOriginRedirectsOnly': True, 'tlsRequired': True}:
        fail()
    if set(callsign) != {'grid', 'remoteCallsign', 'senderCallsign'} or any(not isinstance(v, str) or len(v) > 32 for v in callsign.values()):
        fail()
    for name in ('remoteCallsign', 'senderCallsign'):
        if callsign[name] and not re.fullmatch(r'[A-Z0-9][A-Z0-9/-]{1,22}[A-Z0-9]', callsign[name]):
            fail()
    if callsign['grid'] and not re.fullmatch(r'[A-R]{2}[0-9]{2}(?:[A-X]{2}(?:[0-9]{2})?)?', callsign['grid']):
        fail()
    if (not privacy['callsignIncluded'] and (callsign['senderCallsign'] or callsign['remoteCallsign'])) or (not privacy['gridIncluded'] and callsign['grid']):
        fail()
    keys = set(encryption)
    if keys not in (ENCRYPTION_FIELDS, ENCRYPTION_FIELDS | SIGNATURE_FIELDS):
        fail()
    if (encryption['mode'] != 'end-to-end' or encryption['algorithm'] != 'aes-256-gcm'
            or encryption['downgradeProtected'] is not True or encryption['manifestBoundAsAuthenticatedData'] is not True):
        fail()
    key_id, fingerprint, nonce = (encryption[k] for k in ('keyId', 'recipientKeyFingerprint', 'nonceBase64'))
    if (not isinstance(key_id, str) or not re.fullmatch(r'x25519-v1-[0-9a-f]{64}', key_id)
            or not isinstance(fingerprint, str) or not re.fullmatch(r'[0-9a-f]{64}', fingerprint)
            or not isinstance(nonce, str) or not re.fullmatch(r'[A-Za-z0-9+/]{16}', nonce)):
        fail()
    if SIGNATURE_FIELDS <= keys:
        encoded = encryption['senderSignatureBase64']
        try:
            signature = base64.b64decode(encoded, validate=True) if isinstance(encoded, str) else b''
        except (ValueError, TypeError):
            fail()
        if (encryption['senderAuthAlgorithm'] != 'ecdsa-p256-sha256-v1'
                or not isinstance(encryption['senderKeyFingerprint'], str)
                or not re.fullmatch(r'[0-9a-f]{64}', encryption['senderKeyFingerprint'])
                or not 8 <= len(signature) <= 72 or base64.b64encode(signature).decode() != encoded):
            fail()
    if (type(obj['width']) is not int or type(obj['height']) is not int
            or not 1 <= obj['width'] <= 8192 or not 1 <= obj['height'] <= 8192
            or obj['width'] * obj['height'] > 16777216
            or type(obj['chunkCount']) is not int or type(obj['byteSize']) is not int
            or not 1 <= obj['chunkCount'] <= min(4096, obj['byteSize'])):
        fail()
    if (obj['completion'] not in ('complete', 'partial')
            or obj['contentDisposition'] not in ('attachment', 'inline-preview')
            or obj['mediaSource'] not in ('analog-reception', 'analog-transmission', 'digital-reception', 'digital-transmission')
            or obj['mimeType'] not in ('image/png', 'image/jpeg')):
        fail()
    for field, maximum, empty_allowed in (('originalFilename',128,False), ('safeDisplayFilename',128,False),
                                           ('sstvMode',64,False), ('message',1000,True)):
        value = obj[field]
        if (not isinstance(value, str) or len(value.encode('utf-16-le')) // 2 > maximum or (not value and not empty_allowed)
                or unicodedata.normalize('NFC', value) != value
                or any(unicodedata.category(c).startswith('C') and not (field == 'message' and c == '\n') for c in value)
                or re.search(r'\b(?:https?|wss?|ftp|s3|webdav|file):/{1,2}', value, re.I)
                or value != value.strip() or '  ' in value):
            fail()
        if field.endswith('Filename') and (value.startswith('.') or '..' in value or '__' in value
                or any(not (c.isalnum() or unicodedata.category(c) in ('Mn', 'Mc') or c in ' ._()-[]@+') for c in value)):
            fail()
    try:
        identifier = uuid.UUID(obj['transferId'])
        if str(identifier) != obj['transferId'] or identifier.int == 0:
            fail()
        created = ShareStore._utc(obj['createdUtc'])
        media = ShareStore._utc(obj['mediaUtc'])
        expires = ShareStore._utc(obj['expiresUtc'])
        if not created < expires or (expires - created).total_seconds() > 30 * 86400 or (media - created).total_seconds() > 300:
            fail()
    except (ValueError, TypeError, AttributeError):
        fail()


def strict_json(data):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('duplicate key')
            result[key] = value
        return result
    result = json.loads(data, object_pairs_hook=unique)
    if not isinstance(result, dict):
        raise ValueError('expected object')
    return result


class Server(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, address, store, certificate, key):
        super().__init__(address, Handler)
        self.store = store
        self.slots = threading.BoundedSemaphore(32)
        self.next_prune = time.monotonic() + 60
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(certificate, key)
        self.tls_context = context

    def service_actions(self):
        if time.monotonic() >= self.next_prune:
            try:
                self.store.prune_expired()
            except sqlite3.Error:
                print('Expiry cleanup failed; retrying.', file=sys.stderr)
            finally:
                self.next_prune = time.monotonic() + 60

    def get_request(self):
        raw, address = super().get_request()
        raw.settimeout(20)
        return self.tls_context.wrap_socket(raw, server_side=True, do_handshake_on_connect=False), address

    def process_request(self, request, client_address):
        if not self.slots.acquire(blocking=False):
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except BaseException:
            self.slots.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self.slots.release()


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    server_version = 'DecodiumShare/0.1'
    sys_version = ''

    def setup(self):
        super().setup()
        self.connection.settimeout(20)

    def log_message(self, *args):
        # Request paths, credentials and transfer metadata are deliberately omitted.
        pass

    def reply(self, status, value=None, headers=()):
        data = b'' if value is None else (value if isinstance(value, bytes) else json.dumps(value, separators=(',', ':')).encode())
        self.send_response(status)
        self.send_header('Content-Type', 'application/octet-stream' if isinstance(value, bytes) else 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Connection', 'close')
        for key, val in headers:
            self.send_header(key, val)
        self.end_headers()
        if data:
            self.wfile.write(data)
        self.close_connection = True

    def body(self, limit):
        if self.headers.get('Transfer-Encoding') or len(self.headers.get_all('Content-Length', [])) != 1:
            raise StoreError(400, 'content length required')
        try:
            length = int(self.headers['Content-Length'])
        except ValueError:
            raise StoreError(400, 'invalid content length') from None
        if not 0 <= length <= limit:
            raise StoreError(413, 'request too large')
        data = self.rfile.read(length)
        if len(data) != length:
            raise StoreError(400, 'incomplete request')
        return data

    def actor(self):
        authorization = self.headers.get('Authorization', '')
        actor = self.server.store.authenticate(authorization[7:]) if authorization.startswith('Bearer ') else None
        if actor is None:
            raise StoreError(401, 'authentication required')
        return actor

    def handle_request(self):
        actor = self.actor()
        store = self.server.store
        parsed = urlsplit(self.path)
        path = parsed.path
        method = self.command
        if method == 'GET' and path == '/api/v1/capabilities':
            return self.reply(200, {'protocolVersion': 1, 'recipientLookup': True,
                'chunkedUpload': True, 'resumableUpload': True, 'download': True,
                'acknowledgement': True, 'rejection': True, 'revocation': False,
                'remoteDelete': False, 'incomingList': True, 'endToEndEncryptionEnvelope': True,
                'strictTlsRequired': True, 'maximumChunkBytes': MAX_PAYLOAD,
                'maximumResponseBytes': 65536})
        match = re.fullmatch(r'/api/v1/recipients/([a-z0-9][a-z0-9._+-]{0,63})', path)
        if method == 'GET' and match:
            with store.connect() as db:
                exists = db.execute('SELECT 1 FROM accounts WHERE id=?', (match.group(1),)).fetchone()
            if not exists:
                raise StoreError(404, 'recipient not found')
            return self.reply(200, {'providerId': store.provider_id, 'stableRecipientId': match.group(1),
                'displayCallsign': '', 'displayName': '', 'publicEncryptionKey': '',
                'publicKeyFingerprint': '', 'verification': 'unknown', 'trust': 'unknown'})
        if method == 'POST' and path == '/api/v1/transfers':
            body = self.body(MAX_MANIFEST)
            try:
                manifest = strict_json(body)
            except (ValueError, UnicodeError, RecursionError):
                raise StoreError(400, 'invalid manifest') from None
            validate_manifest_shape(manifest)
            if len(json.dumps({'canonicalManifestJson': body.decode('utf-8')}).encode()) > 55000:
                raise StoreError(413, 'manifest too large for inbox response')
            encryption = manifest.get('encryption')
            if (not isinstance(encryption, dict) or encryption.get('mode') != 'end-to-end'
                    or encryption.get('algorithm') != 'aes-256-gcm'):
                raise StoreError(400, 'encrypted payload required')
            identifier, committed, created = store.create(actor, self.headers.get('Idempotency-Key', ''), body, manifest)
            return self.reply(201 if created else 200, {'uploadId': identifier, 'committedBytes': committed})
        match = re.fullmatch(r'/api/v1/transfers/([0-9a-f]{32})(/chunks|/complete)?', path)
        if match:
            identifier, operation = match.groups()
            if method == 'GET' and operation is None:
                return self.reply(200, {'committedBytes': len(store.get(identifier, actor)['payload'])})
            if method == 'DELETE' and operation is None:
                store.cancel(identifier, actor)
                return self.reply(204)
            if method == 'PUT' and operation == '/chunks':
                body = self.body(MAX_PAYLOAD)
                try:
                    offset = int(self.headers.get('Upload-Offset', ''))
                except ValueError:
                    raise StoreError(400, 'invalid offset') from None
                committed = store.append(identifier, actor, offset, body, self.headers.get('X-Content-SHA256', ''))
                return self.reply(200, {'committedBytes': committed})
            if method == 'POST' and operation == '/complete':
                self.body(MAX_MANIFEST)
                size, digest = store.complete(identifier, actor)
                return self.reply(200, {'remoteObjectId': identifier, 'byteSize': size, 'sha256': digest})
        if method == 'GET' and path == '/api/v1/inbox':
            try:
                query = parse_qs(parsed.query, strict_parsing=True) if parsed.query else {}
                if set(query) - {'limit'} or len(query.get('limit', ['200'])) != 1:
                    raise ValueError()
                limit = int(query.get('limit', ['200'])[0])
            except ValueError:
                raise StoreError(400, 'invalid query') from None
            rows = store.inbox(actor, limit)
            items = [{'incomingId': row['id'], 'providerId': store.provider_id,
                      'senderId': row['owner'], 'manifestSha256': hashlib.sha256(row['manifest']).hexdigest(),
                      'canonicalManifestJson': row['manifest'].decode('utf-8'),
                      'byteSize': row['size'], 'receivedUtc': row['received'],
                      'expiresUtc': row['expires']} for row in rows]
            while items and len(json.dumps({'items': items}, separators=(',', ':')).encode()) > 60000:
                items.pop()
            return self.reply(200, {'items': items})
        match = re.fullmatch(r'/api/v1/inbox/([0-9a-f]{32})/(content|acknowledge|reject)', path)
        if match:
            identifier, operation = match.groups()
            if method == 'GET' and operation == 'content':
                row = store.get(identifier, actor, receiver=True)
                range_match = re.fullmatch(r'bytes=(\d+)-(\d+)', self.headers.get('Range', ''))
                if not range_match:
                    raise StoreError(400, 'range required')
                first, last = map(int, range_match.groups())
                if first >= row['size'] or last < first or last - first >= MAX_PAYLOAD:
                    raise StoreError(416, 'invalid range')
                last = min(last, row['size'] - 1)
                data = row['payload'][first:last + 1]
                digest = base64.b64encode(hashlib.sha256(data).digest()).decode('ascii')
                return self.reply(206, data, [('Content-Range', f'bytes {first}-{last}/{row["size"]}'), ('Digest', 'sha-256=' + digest)])
            if method == 'POST' and operation in ('acknowledge', 'reject'):
                self.body(MAX_MANIFEST)
                store.decide(identifier, actor, operation)
                return self.reply(204)
        raise StoreError(404, 'not found')

    def dispatch(self):
        try:
            self.handle_request()
        except StoreError as error:
            self.reply(error.status, {'error': str(error)})
        except (OSError, ValueError, UnicodeError):
            self.reply(400, {'error': 'invalid request'})

    do_GET = do_POST = do_PUT = do_DELETE = dispatch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database', type=Path, required=True)
    parser.add_argument('--provider-id', required=True)
    commands = parser.add_subparsers(dest='command', required=True)
    account = commands.add_parser('add-account')
    account.add_argument('account')
    rotate = commands.add_parser('rotate-account')
    rotate.add_argument('account')
    serve = commands.add_parser('serve')
    serve.add_argument('--host', default='127.0.0.1')
    serve.add_argument('--port', type=int, default=8443)
    serve.add_argument('--certificate', required=True)
    serve.add_argument('--key', required=True)
    args = parser.parse_args()
    os.umask(0o077)
    store = ShareStore(args.database, args.provider_id)
    if args.command in ('add-account', 'rotate-account'):
        token = secrets.token_urlsafe(48)
        if args.command == 'add-account':
            store.add_account(args.account, token)
        else:
            store.rotate_account(args.account, token)
        print('Bearer token (shown once):', token)
    else:
        with Server((args.host, args.port), store, args.certificate, args.key) as server:
            server.serve_forever()


if __name__ == '__main__':
    main()
