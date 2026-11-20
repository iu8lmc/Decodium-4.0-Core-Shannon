"""Persistent, bounded storage for the Decodium SSTV sharing service.

This module is transport independent. Account tokens must be randomly
generated secrets, never passwords: only their SHA-256 digests are stored.
"""
from __future__ import annotations

from datetime import datetime, timedelta, timezone
import hashlib
import hmac
import re
import secrets
import sqlite3
from pathlib import Path

IDENTIFIER = re.compile(r"[a-z0-9][a-z0-9._+-]{0,63}\Z")
SHA256 = re.compile(r"[0-9a-f]{64}\Z")
MAX_PAYLOAD = 8 * 1024 * 1024
MAX_MANIFEST = 65536
MAX_ACCOUNT_BYTES = 64 * 1024 * 1024
MAX_SERVER_BYTES = 512 * 1024 * 1024


class StoreError(Exception):
    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status


class ShareStore:
    def __init__(self, path: Path, provider_id: str):
        if not IDENTIFIER.fullmatch(provider_id):
            raise ValueError('invalid provider ID')
        self.path = Path(path)
        self.provider_id = provider_id
        with self.connect() as db:
            db.executescript('''
                CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);
                CREATE TABLE IF NOT EXISTS accounts (
                  id TEXT PRIMARY KEY, token_hash BLOB NOT NULL UNIQUE
                );
                CREATE TABLE IF NOT EXISTS uploads (
                  id TEXT PRIMARY KEY, owner TEXT NOT NULL, recipient TEXT NOT NULL,
                  idempotency TEXT NOT NULL, manifest BLOB NOT NULL,
                  digest TEXT NOT NULL, size INTEGER NOT NULL, expires TEXT NOT NULL,
                  received TEXT NOT NULL, payload BLOB NOT NULL DEFAULT X'',
                  complete INTEGER NOT NULL DEFAULT 0, decision TEXT,
                  UNIQUE(owner, idempotency), FOREIGN KEY(owner) REFERENCES accounts(id),
                  FOREIGN KEY(recipient) REFERENCES accounts(id)
                );
                CREATE INDEX IF NOT EXISTS inbox ON uploads(recipient,complete,expires);
            ''')
            db.execute('BEGIN IMMEDIATE')
            stored = db.execute("SELECT value FROM metadata WHERE key='provider_id'").fetchone()
            if stored and stored['value'] != provider_id:
                raise ValueError('database belongs to another provider ID')
            db.execute("INSERT OR IGNORE INTO metadata(key,value) VALUES ('provider_id',?)", (provider_id,))
            db.commit()

    def connect(self):
        db = sqlite3.connect(self.path, timeout=10, isolation_level=None)
        db.row_factory = sqlite3.Row
        db.execute('PRAGMA foreign_keys=ON')
        return db

    def add_account(self, account: str, token: str):
        if not IDENTIFIER.fullmatch(account) or len(token) < 32:
            raise ValueError('invalid account or token')
        with self.connect() as db:
            db.execute('BEGIN IMMEDIATE')
            if db.execute('SELECT count(*) FROM accounts').fetchone()[0] >= 128:
                raise ValueError('account limit reached')
            db.execute('INSERT INTO accounts VALUES (?,?)', (account, hashlib.sha256(token.encode()).digest()))
            db.commit()

    def rotate_account(self, account: str, token: str):
        if not IDENTIFIER.fullmatch(account) or len(token) < 32:
            raise ValueError('invalid account or token')
        with self.connect() as db:
            if db.execute('UPDATE accounts SET token_hash=? WHERE id=?',
                          (hashlib.sha256(token.encode()).digest(), account)).rowcount != 1:
                raise ValueError('account not found')

    def authenticate(self, token: str):
        if not token or len(token) > 256:
            return None
        digest = hashlib.sha256(token.encode()).digest()
        with self.connect() as db:
            for row in db.execute('SELECT id,token_hash FROM accounts'):
                if hmac.compare_digest(row['token_hash'], digest):
                    return row['id']
        return None

    def prune_expired(self):
        now = datetime.now(timezone.utc).isoformat(timespec='milliseconds').replace('+00:00','Z')
        with self.connect() as db:
            return db.execute('DELETE FROM uploads WHERE expires<=?', (now,)).rowcount

    @staticmethod
    def _utc(value: str):
        try:
            if not isinstance(value, str) or not re.fullmatch(r'\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z', value):
                raise ValueError()
            parsed = datetime.fromisoformat(value.replace('Z', '+00:00'))
            if not value.endswith('Z') or parsed.tzinfo != timezone.utc:
                raise ValueError()
            return parsed
        except (AttributeError, ValueError):
            raise StoreError(400, 'invalid expiry') from None

    def create(self, owner: str, key: str, manifest: bytes, data: dict):
        if not SHA256.fullmatch(key) or len(manifest) > MAX_MANIFEST:
            raise StoreError(400, 'invalid manifest or idempotency key')
        sender, recipient = data.get('senderId'), data.get('recipientId')
        size, digest, expires = data.get('byteSize'), data.get('sha256'), data.get('expiresUtc')
        if (sender != owner or data.get('providerId') != self.provider_id
                or not isinstance(recipient, str) or not IDENTIFIER.fullmatch(recipient)
                or type(size) is not int or not 0 < size <= MAX_PAYLOAD
                or not isinstance(digest, str) or not SHA256.fullmatch(digest)
                or not datetime.now(timezone.utc) < self._utc(expires) <= datetime.now(timezone.utc) + timedelta(days=31)):
            raise StoreError(400, 'invalid transfer')
        received = datetime.now(timezone.utc).isoformat(timespec='milliseconds').replace('+00:00','Z')
        with self.connect() as db:
            db.execute('BEGIN IMMEDIATE')
            db.execute('DELETE FROM uploads WHERE expires<=?', (received,))
            old = db.execute('SELECT * FROM uploads WHERE owner=? AND idempotency=?', (owner,key)).fetchone()
            if old:
                if not hmac.compare_digest(old['manifest'], manifest):
                    raise StoreError(409, 'idempotency conflict')
                db.commit()
                return old['id'], len(old['payload']), False
            if not db.execute('SELECT 1 FROM accounts WHERE id=?', (recipient,)).fetchone():
                raise StoreError(404, 'recipient not found')
            if db.execute('SELECT count(*) FROM uploads WHERE owner=? AND expires>?', (owner,received)).fetchone()[0] >= 128:
                raise StoreError(429, 'transfer quota exceeded')
            owner_bytes = db.execute('SELECT coalesce(sum(size),0) FROM uploads WHERE owner=?', (owner,)).fetchone()[0]
            server_bytes = db.execute('SELECT coalesce(sum(size),0) FROM uploads').fetchone()[0]
            if owner_bytes + size > MAX_ACCOUNT_BYTES or server_bytes + size > MAX_SERVER_BYTES:
                raise StoreError(429, 'storage quota exceeded')
            identifier = secrets.token_hex(16)
            db.execute('INSERT INTO uploads(id,owner,recipient,idempotency,manifest,digest,size,expires,received) VALUES (?,?,?,?,?,?,?,?,?)',
                       (identifier,owner,recipient,key,manifest,digest,size,expires,received))
            db.commit()
            return identifier, 0, True

    def cancel(self, identifier: str, owner: str):
        with self.connect() as db:
            db.execute('DELETE FROM uploads WHERE id=? AND owner=?', (identifier, owner))

    def get(self, identifier: str, actor: str, *, receiver=False):
        with self.connect() as db:
            row = db.execute('SELECT * FROM uploads WHERE id=? AND '+('recipient' if receiver else 'owner')+'=?', (identifier,actor)).fetchone()
        if not row or self._utc(row['expires']) <= datetime.now(timezone.utc):
            raise StoreError(404, 'not found')
        if receiver and not row['complete']:
            raise StoreError(404, 'not found')
        return row

    def append(self, identifier: str, owner: str, offset: int, chunk: bytes, digest: str):
        if not chunk or len(chunk) > MAX_PAYLOAD or hashlib.sha256(chunk).hexdigest() != digest:
            raise StoreError(400, 'invalid chunk')
        with self.connect() as db:
            db.execute('BEGIN IMMEDIATE')
            row = db.execute('SELECT * FROM uploads WHERE id=? AND owner=?',(identifier,owner)).fetchone()
            if not row or self._utc(row['expires']) <= datetime.now(timezone.utc):
                raise StoreError(404, 'not found')
            payload = row['payload']
            if row['complete'] or offset < 0 or offset + len(chunk) > row['size']:
                raise StoreError(409, 'invalid offset')
            if offset < len(payload):
                if offset + len(chunk) <= len(payload) and hmac.compare_digest(payload[offset:offset+len(chunk)], chunk):
                    db.commit()
                    return offset + len(chunk)
                raise StoreError(409, 'chunk conflict')
            if offset != len(payload):
                raise StoreError(409, 'invalid offset')
            db.execute('UPDATE uploads SET payload=? WHERE id=?',(payload+chunk,identifier))
            db.commit()
            return offset+len(chunk)

    def complete(self, identifier: str, owner: str):
        with self.connect() as db:
            db.execute('BEGIN IMMEDIATE')
            row = db.execute('SELECT * FROM uploads WHERE id=? AND owner=?',(identifier,owner)).fetchone()
            if not row or self._utc(row['expires']) <= datetime.now(timezone.utc):
                raise StoreError(404, 'not found')
            if len(row['payload']) != row['size'] or hashlib.sha256(row['payload']).hexdigest() != row['digest']:
                raise StoreError(409, 'payload integrity failure')
            db.execute('UPDATE uploads SET complete=1 WHERE id=?',(identifier,))
            db.commit()
            return row['size'],row['digest']

    def inbox(self, recipient: str, limit: int):
        if not 1 <= limit <= 1000:
            raise StoreError(400, 'invalid limit')
        now = datetime.now(timezone.utc).isoformat(timespec='milliseconds').replace('+00:00','Z')
        with self.connect() as db:
            return list(db.execute('SELECT id,owner,manifest,size,received,expires FROM uploads WHERE recipient=? AND complete=1 AND decision IS NULL AND expires>? ORDER BY received,id LIMIT ?', (recipient,now,limit)))

    def decide(self, identifier: str, recipient: str, decision: str):
        if decision not in ('acknowledge','reject'):
            raise StoreError(400, 'invalid decision')
        with self.connect() as db:
            db.execute('BEGIN IMMEDIATE')
            row = db.execute('SELECT * FROM uploads WHERE id=? AND recipient=? AND complete=1',(identifier,recipient)).fetchone()
            if not row or self._utc(row['expires']) <= datetime.now(timezone.utc):
                raise StoreError(404, 'not found')
            if row['decision'] not in (None,decision):
                raise StoreError(409, 'decision conflict')
            db.execute('UPDATE uploads SET decision=? WHERE id=?',(decision,identifier))
            db.commit()
