import hashlib
from datetime import datetime, timedelta, timezone
import http.client
import json
from pathlib import Path
import ssl
import tempfile
import threading
import unittest
import uuid

from decodium_share_store import ShareStore
from decodium_share_server import Server

def expiry():
    return (datetime.now(timezone.utc) + timedelta(days=1)).isoformat(timespec='milliseconds').replace('+00:00', 'Z')

def manifest_for(payload):
    now = datetime.now(timezone.utc).isoformat(timespec='milliseconds').replace('+00:00', 'Z')
    return {'providerId': 'test-provider', 'senderId': 'alice', 'recipientId': 'bob',
            'byteSize': len(payload), 'sha256': hashlib.sha256(payload).hexdigest(),
            'createdUtc': now, 'mediaUtc': now, 'expiresUtc': expiry(),
            'transferId': str(uuid.uuid4()), 'protocolVersion': 1,
            'width': 16, 'height': 16, 'chunkCount': 1, 'completion': 'complete',
            'contentDisposition': 'attachment', 'mediaSource': 'analog-reception',
            'message': '', 'mimeType': 'image/png', 'originalFilename': 'image.png',
            'safeDisplayFilename': 'image.png', 'sstvMode': 'lab',
            'callsign': {'grid': '', 'remoteCallsign': '', 'senderCallsign': ''},
            'privacy': {'automaticIncomingDownloadAllowed': False, 'automaticUploadAllowed': False,
                        'callsignIncluded': False, 'exifRetained': False, 'explicitExpiry': True,
                        'gridIncluded': False, 'locationIncluded': False, 'meteredNetworkAllowed': True,
                        'publicShare': False, 'recipientConfirmed': True},
            'transport': {'certificateValidationRequired': True, 'providerCanReadContent': False,
                          'sameOriginRedirectsOnly': True, 'tlsRequired': True},
            'encryption': {'algorithm': 'aes-256-gcm', 'mode': 'end-to-end',
                           'downgradeProtected': True, 'manifestBoundAsAuthenticatedData': True,
                           'keyId': 'x25519-v1-' + 'a' * 64, 'recipientKeyFingerprint': 'b' * 64,
                           'nonceBase64': 'AAAAAAAAAAAAAAAA'}}


class ServerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name) / 'data.sqlite'
        self.store = ShareStore(self.path, 'test-provider')
        for name, token in [('alice', 'a' * 48), ('bob', 'b' * 48), ('carol', 'c' * 48)]:
            self.store.add_account(name, token)
        fixtures = Path(__file__).resolve().parents[2] / 'tests/sstv/fixtures'
        self.server = Server(('127.0.0.1', 0), self.store,
                             str(fixtures / 'localhost-test-only.crt'),
                             str(fixtures / 'localhost-test-only.key'))
        self.thread = threading.Thread(target=self.server.serve_forever)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.temp.cleanup()

    def request(self, method, path, token, body=b'', headers=None):
        connection = http.client.HTTPSConnection('127.0.0.1', self.server.server_port,
                                                  context=ssl._create_unverified_context(), timeout=5)
        hdr = {'Authorization': 'Bearer ' + token}
        hdr.update(headers or {})
        connection.request(method, path, body=body, headers=hdr)
        response = connection.getresponse()
        result = response.status, response.read()
        connection.close()
        return result

    def test_authenticated_ab_transfer_and_isolation(self):
        self.assertEqual(self.request('GET', '/api/v1/capabilities', 'wrong')[0], 401)
        self.assertEqual(self.request('GET', '/api/v1/recipients/carol', 'a' * 48)[0], 200)
        for i, payload in enumerate((b'first encrypted object', b'second encrypted object')):
            manifest = manifest_for(payload)
            body = json.dumps(manifest, separators=(',', ':')).encode()
            key = hashlib.sha256(bytes([i])).hexdigest()
            status, response = self.request('POST', '/api/v1/transfers', 'a' * 48, body,
                                            {'Idempotency-Key': key})
            self.assertEqual(status, 201)
            upload = json.loads(response)['uploadId']
            path = '/api/v1/transfers/' + upload
            headers = {'Upload-Offset': '0', 'X-Content-SHA256': hashlib.sha256(payload).hexdigest()}
            self.assertEqual(self.request('PUT', path + '/chunks', 'c' * 48, payload, headers)[0], 404)
            self.assertEqual(self.request('PUT', path + '/chunks', 'a' * 48, payload, headers)[0], 200)
            self.assertEqual(self.request('POST', path + '/complete', 'a' * 48, b'{}')[0], 200)
            self.assertEqual(self.request('GET', '/api/v1/inbox/' + upload + '/content', 'c' * 48,
                                          headers={'Range': 'bytes=0-100'})[0], 404)
            status, received = self.request('GET', '/api/v1/inbox/' + upload + '/content', 'b' * 48,
                                            headers={'Range': 'bytes=0-100'})
            self.assertEqual((status, received), (206, payload))
            self.assertEqual(self.request('POST', '/api/v1/inbox/' + upload + '/acknowledge',
                                          'b' * 48, b'{}')[0], 204)

    def test_plaintext_and_duplicate_json_rejected(self):
        for body in (b'{"encryption":{"mode":"transport-tls","algorithm":"none"}}',
                     b'{"x":1,"x":2}'):
            self.assertEqual(self.request('POST', '/api/v1/transfers', 'a' * 48, body,
                                          {'Idempotency-Key': '0' * 64})[0], 400)
        malformed = manifest_for(b'content')
        del malformed['mediaUtc']
        body = json.dumps(malformed).encode()
        self.assertEqual(self.request('POST', '/api/v1/transfers', 'a' * 48, body,
                                      {'Idempotency-Key': '0' * 64})[0], 400)
        malformed = manifest_for(b'content')
        malformed['message'] = '\U0001f600' * 501
        self.assertEqual(self.request('POST', '/api/v1/transfers', 'a' * 48, json.dumps(malformed).encode(),
                                      {'Idempotency-Key': '0' * 64})[0], 400)
        self.assertEqual(json.loads(self.request('GET', '/api/v1/inbox', 'b' * 48)[1]), {'items': []})

    def test_upload_resumes_after_https_server_restart(self):
        payload = b'bounded encrypted bytes for restart'
        manifest = manifest_for(payload)
        body = json.dumps(manifest, separators=(',', ':')).encode()
        headers = {'Idempotency-Key': '1' * 64}
        status, result = self.request('POST', '/api/v1/transfers', 'a' * 48, body, headers)
        self.assertEqual(status, 201)
        upload = json.loads(result)['uploadId']
        path = '/api/v1/transfers/' + upload
        first, rest = payload[:7], payload[7:]
        self.assertEqual(self.request('PUT', path + '/chunks', 'a' * 48, first,
                         {'Upload-Offset': '0', 'X-Content-SHA256': hashlib.sha256(first).hexdigest()})[0], 200)
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        fixtures = Path(__file__).resolve().parents[2] / 'tests/sstv/fixtures'
        self.server = Server(('127.0.0.1', 0), ShareStore(self.path, 'test-provider'),
                             str(fixtures / 'localhost-test-only.crt'), str(fixtures / 'localhost-test-only.key'))
        self.thread = threading.Thread(target=self.server.serve_forever)
        self.thread.start()
        status, result = self.request('POST', '/api/v1/transfers', 'a' * 48, body, headers)
        self.assertEqual((status, json.loads(result)), (200, {'uploadId': upload, 'committedBytes': 7}))
        self.assertEqual(self.request('PUT', path + '/chunks', 'a' * 48, rest,
                         {'Upload-Offset': '7', 'X-Content-SHA256': hashlib.sha256(rest).hexdigest()})[0], 200)
        self.assertEqual(self.request('POST', path + '/complete', 'a' * 48, b'{}')[0], 200)
        self.assertEqual(self.request('GET', '/api/v1/inbox/' + upload + '/content', 'b' * 48,
                         headers={'Range': 'bytes=0-100'}), (206, payload))


if __name__ == '__main__':
    unittest.main()
