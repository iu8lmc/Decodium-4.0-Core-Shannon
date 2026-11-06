import hashlib
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from decodium_share_store import ShareStore, StoreError

def expiry():
    return (datetime.now(timezone.utc) + timedelta(days=1)).isoformat(timespec='milliseconds').replace('+00:00', 'Z')


class ShareStoreTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name) / 'store.sqlite'
        self.store = ShareStore(self.path, 'test-provider')
        self.store.add_account('alice', 'a' * 48)
        self.store.add_account('bob', 'b' * 48)
        self.store.add_account('carol', 'c' * 48)

    def tearDown(self):
        self.temp.cleanup()

    def test_ab_persistence_isolation_and_retries(self):
        self.assertEqual(self.store.authenticate('a' * 48), 'alice')
        self.assertIsNone(self.store.authenticate('wrong'))
        for index, payload in enumerate((b'first test payload', b'second distinct payload')):
            data = {'senderId': 'alice', 'recipientId': 'bob', 'providerId': 'test-provider',
                    'byteSize': len(payload), 'sha256': hashlib.sha256(payload).hexdigest(),
                    'expiresUtc': expiry()}
            manifest = json.dumps(data, sort_keys=True, separators=(',', ':')).encode()
            key = hashlib.sha256(bytes([index])).hexdigest()
            upload, offset, created = self.store.create('alice', key, manifest, data)
            self.assertTrue(created)
            self.assertEqual(offset, 0)
            part = payload[:5]
            self.assertEqual(self.store.append(upload, 'alice', 0, part, hashlib.sha256(part).hexdigest()), 5)
            reopened = ShareStore(self.path, 'test-provider')
            self.assertEqual(reopened.create('alice', key, manifest, data), (upload, 5, False))
            self.assertEqual(reopened.append(upload, 'alice', 0, part, hashlib.sha256(part).hexdigest()), 5)
            rest = payload[5:]
            reopened.append(upload, 'alice', 5, rest, hashlib.sha256(rest).hexdigest())
            with self.assertRaises(StoreError):
                reopened.get(upload, 'carol', receiver=True)
            self.assertEqual(reopened.complete(upload, 'alice'), (len(payload), data['sha256']))
            self.assertEqual(reopened.get(upload, 'bob', receiver=True)['payload'], payload)
            with self.assertRaises(StoreError):
                reopened.get(upload, 'carol', receiver=True)
            self.assertEqual(len(reopened.inbox('bob', 100)), 1)
            reopened.decide(upload, 'bob', 'acknowledge')
            reopened.decide(upload, 'bob', 'acknowledge')
            with self.assertRaises(StoreError):
                reopened.decide(upload, 'bob', 'reject')
            self.assertFalse(reopened.inbox('bob', 100))

    def test_integrity_and_actor_rejection(self):
        payload = b'payload'
        data = {'senderId': 'alice', 'recipientId': 'bob', 'providerId': 'test-provider',
                'byteSize': len(payload), 'sha256': hashlib.sha256(payload).hexdigest(),
                'expiresUtc': expiry()}
        manifest = json.dumps(data).encode()
        with self.assertRaises(StoreError):
            self.store.create('carol', '0' * 64, manifest, data)
        upload, _, _ = self.store.create('alice', '0' * 64, manifest, data)
        with self.assertRaises(StoreError):
            self.store.append(upload, 'carol', 0, payload, hashlib.sha256(payload).hexdigest())
        with self.assertRaises(StoreError):
            self.store.append(upload, 'alice', 0, payload, '0' * 64)
        with self.assertRaises(StoreError):
            self.store.complete(upload, 'alice')
        with self.assertRaises(StoreError):
            self.store.create('alice', '0' * 64, manifest + b' ', data)

    def test_expired_objects_are_pruned(self):
        payload = b'expired'
        data = {'senderId': 'alice', 'recipientId': 'bob', 'providerId': 'test-provider',
                'byteSize': len(payload), 'sha256': hashlib.sha256(payload).hexdigest(),
                'expiresUtc': expiry()}
        upload, _, _ = self.store.create('alice', '1' * 64, json.dumps(data).encode(), data)
        with self.store.connect() as db:
            db.execute("UPDATE uploads SET expires='2000-01-01T00:00:00.000Z' WHERE id=?", (upload,))
        self.assertEqual(self.store.prune_expired(), 1)
        self.assertEqual(self.store.prune_expired(), 0)
        with self.assertRaises(StoreError):
            self.store.get(upload, 'alice')

    def test_token_rotation(self):
        self.store.rotate_account('alice', 'z' * 48)
        self.assertIsNone(self.store.authenticate('a' * 48))
        self.assertEqual(self.store.authenticate('z' * 48), 'alice')
        with self.assertRaises(ValueError):
            self.store.rotate_account('missing', 'z' * 48)
        with self.assertRaises(ValueError):
            ShareStore(self.path, 'other-provider')

    def test_account_and_server_quota_are_enforced(self):
        payload = b'12345'
        data = {'senderId': 'alice', 'recipientId': 'bob', 'providerId': 'test-provider',
                'byteSize': len(payload), 'sha256': hashlib.sha256(payload).hexdigest(),
                'expiresUtc': expiry()}
        manifest = json.dumps(data).encode()
        for target in ('MAX_ACCOUNT_BYTES', 'MAX_SERVER_BYTES'):
            with patch('decodium_share_store.' + target, 4):
                with self.assertRaises(StoreError) as error:
                    self.store.create('alice', '2' * 64, manifest, data)
                self.assertEqual(error.exception.status, 429)
        self.assertTrue(self.store.create('alice', '2' * 64, manifest, data)[2])


if __name__ == '__main__':
    unittest.main()
