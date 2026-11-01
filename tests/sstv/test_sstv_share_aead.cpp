// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/sstv/sharing/SstvShareAead.h"
#include "../../src/sstv/sharing/SstvShareEncryptedPayload.h"
#include "../../src/sstv/sharing/SstvShareKeyAgreement.h"
#include "../../src/sstv/sharing/SstvShareRecipientEncryption.h"
#include "../../src/sstv/sharing/SstvShareKeyStore.h"
#include "../../src/sstv/sharing/SstvShareSecureEnvelope.h"
#include <QHash>
#include <QCryptographicHash>
#include <iostream>
#include <stdexcept>
using namespace decodium::sstv::sharing;
class MemoryBackend final : public secure_settings::Backend {
public:
    bool online {true};
    bool privateStorage {true};
    bool failStore {false};
    mutable QHash<QString, QString> values;
    bool available() const override { return online; }
    bool supports_private_key_storage() const override { return privateStorage; }
    secure_settings::LookupResult lookup(const QString& service, const QString& account) const override {
        secure_settings::LookupResult result; result.backend_available = online;
        const auto key = service + QLatin1Char('/') + account;
        result.found = values.contains(key); if (result.found) result.value = values.value(key);
        return result;
    }
    secure_settings::InsertResult store_if_absent(const QString& service, const QString& account, const QString& value) const override {
        if (failStore) return secure_settings::InsertResult::Error;
        const auto key = service + QLatin1Char('/') + account;
        if (values.contains(key)) return secure_settings::InsertResult::AlreadyExists;
        values.insert(key, value); return secure_settings::InsertResult::Created;
    }
    bool store(const QString& service, const QString& account, const QString& value, QString*) const override {
        if (failStore) return false;
        values.insert(service + QLatin1Char('/') + account, value); return true;
    }
    bool remove(const QString& service, const QString& account, QString*) const override {
        values.remove(service + QLatin1Char('/') + account); return true;
    }
};
static void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
 try {
    const auto alicePrivate = QByteArray::fromHex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
    const auto alicePublic = QByteArray::fromHex("8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
    const auto bobPrivate = QByteArray::fromHex("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb");
    const auto bobPublic = QByteArray::fromHex("de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
    check(sstvShareX25519PublicKey(alicePrivate) == alicePublic && sstvShareX25519PublicKey(bobPrivate) == bobPublic, "RFC 7748 public keys failed");
    const auto senderKey = sstvShareDeriveTransferKey(SstvShareKeyRole::SenderEphemeral, alicePrivate, alicePublic, bobPublic, "transfer-123");
    const auto recipientKey = sstvShareDeriveTransferKey(SstvShareKeyRole::Recipient, bobPrivate, alicePublic, bobPublic, "transfer-123");
    check(senderKey && recipientKey && *senderKey == *recipientKey
       && senderKey->toHex() == QByteArray("ee00043f894a22481a2f71ccf9aec5489f3afc5d821f51b2a169f7ea204a8708"), "X25519 HKDF known answer failed");
    check(sstvShareDeriveTransferKey(SstvShareKeyRole::SenderEphemeral, alicePrivate, alicePublic, bobPublic, "other") != senderKey, "context not bound");
    check(!sstvShareDeriveTransferKey(SstvShareKeyRole::Recipient, alicePrivate, alicePublic, bobPublic, "transfer-123"), "private key role mismatch accepted");
    check(!sstvShareDeriveTransferKey(SstvShareKeyRole::SenderEphemeral, alicePrivate, alicePublic, QByteArray(32, '\0'), "transfer-123"), "low order key accepted");
    const auto randomA = sstvShareGenerateX25519KeyPair();
    const auto randomB = sstvShareGenerateX25519KeyPair();
    check(randomA && randomB && randomA->publicKey != randomB->publicKey
       && sstvShareX25519PublicKey(randomA->privateKey) == randomA->publicKey, "X25519 generation failed");
    MemoryBackend backend;
    SstvShareKeyStore aliceStore(backend, QStringLiteral("alice"));
    SstvShareKeyStore aliceReload(backend, QStringLiteral("alice"));
    SstvShareKeyStore bobStore(backend, QStringLiteral("bob"));
    check(aliceStore.loadPrivateKey().error == SstvShareKeyStoreError::Missing, "missing identity not reported");
    const auto aliceIdentity = aliceStore.createOrLoadPublicKey();
    const auto bobIdentity = bobStore.createOrLoadPublicKey();
    check(aliceIdentity.ok() && bobIdentity.ok() && *aliceIdentity.key != *bobIdentity.key, "isolated identities failed");
    check(aliceReload.createOrLoadPublicKey().key == aliceIdentity.key, "identity changed on reload");
    const auto pinnedBob = sstvShareX25519Fingerprint(*bobIdentity.key);
    check(aliceStore.pinRecipient("provider", "bob", *bobIdentity.key, pinnedBob) == SstvShareKeyStoreError::None, "pin failed");
    check(aliceReload.pinnedRecipient("provider", "bob").key == bobIdentity.key, "pin reload failed");
    check(bobStore.pinnedRecipient("provider", "bob").error == SstvShareKeyStoreError::Missing, "pin leaked between profiles");
    check(aliceStore.pinRecipient("provider", "bob", *aliceIdentity.key,
          sstvShareX25519Fingerprint(*aliceIdentity.key)) == SstvShareKeyStoreError::Conflict, "changed key silently accepted");
    check(aliceStore.pinRecipient("provider", "bob", *bobIdentity.key, QString(64, '0')) == SstvShareKeyStoreError::InvalidInput, "unverified pin accepted");
    const QByteArray lowOrder(32, '\0');
    check(aliceStore.pinRecipient("provider", "invalid", lowOrder,
          sstvShareX25519Fingerprint(lowOrder)) == SstvShareKeyStoreError::InvalidInput,
          "invalid low-order recipient pinned");
    backend.privateStorage = false;
    check(aliceStore.loadPrivateKey().error == SstvShareKeyStoreError::Unavailable,
          "backend without private key guarantee accepted");
    backend.privateStorage = true;
    backend.online = false;
    check(aliceStore.loadPrivateKey().error == SstvShareKeyStoreError::Unavailable, "unavailable backend accepted");
    backend.online = true;
    backend.failStore = true;
    SstvShareKeyStore missingStore(backend, QStringLiteral("new-profile"));
    check(missingStore.createOrLoadPublicKey().error == SstvShareKeyStoreError::StorageFailure, "failed secure write accepted");
    backend.failStore = false;
    // A malformed existing identity must not be silently replaced.
    MemoryBackend isolatedBackend;
    SstvShareKeyStore isolated(isolatedBackend, QStringLiteral("isolated"));
    check(isolated.createOrLoadPublicKey().ok(), "isolated identity failed");
    check(isolatedBackend.values.size() == 1, "unexpected identity store entries");
    isolatedBackend.values.begin().value() = QStringLiteral("not-base64");
    check(isolated.createOrLoadPublicKey().error == SstvShareKeyStoreError::Corrupt,
          "corrupt identity silently replaced");
    check(isolatedBackend.values.constBegin().value() == QStringLiteral("not-base64"),
          "corrupt identity was overwritten");
    const QByteArray key(32, '\0');
    // NIST AES-256-GCM zero key/IV/plaintext known-answer vector.
    SstvShareAeadEnvelope known {QByteArray(12, '\0'),
        QByteArray::fromHex("cea7403d4d606b6e074ec5d3baf39d18"),
        QByteArray::fromHex("d0d1c8a799996bf0265b98b5d48ab919")};
    const auto knownPlain = sstvShareAeadOpen(key, known, {});
    check(knownPlain && *knownPlain == QByteArray(16, '\0'), "known vector failed");
    const QByteArray data("independent encrypted image payload");
    const QByteArray aad("manifest binding");
    const auto a = sstvShareAeadSeal(key, data, aad);
    const auto b = sstvShareAeadSeal(key, data, aad);
    check(a && b && a->nonce != b->nonce && a->ciphertext != data, "sealing or nonce failed");
    check(sstvShareAeadOpen(key, *a, aad) == data && sstvShareAeadOpen(key, *b, aad) == data, "round trip failed");
    auto modified = *a; modified.ciphertext[0] = char(modified.ciphertext[0] ^ 1);
    check(!sstvShareAeadOpen(key, modified, aad), "modified ciphertext accepted");
    modified = *a; modified.tag[0] = char(modified.tag[0] ^ 1);
    check(!sstvShareAeadOpen(key, modified, aad), "modified tag accepted");
    modified = *a; modified.nonce[0] = char(modified.nonce[0] ^ 1);
    check(!sstvShareAeadOpen(key, modified, aad), "modified nonce accepted");
    check(!sstvShareAeadOpen(key, *a, aad + 'x'), "modified AAD accepted");
    check(!sstvShareAeadOpen(QByteArray(32, 1), *a, aad), "wrong key accepted");
    check(!sstvShareAeadSeal(QByteArray(31, 0), data, aad), "short key accepted");
    auto empty = sstvShareAeadSeal(key, {}, {});
    check(empty && sstvShareAeadOpen(key, *empty, {}) == QByteArray(), "empty plaintext failed");
    SstvShareManifestV1 manifest;
    manifest.transferId = QUuid::createUuid();
    manifest.providerId = QStringLiteral("provider");
    manifest.senderId = QStringLiteral("alice");
    manifest.recipientId = QStringLiteral("bob");
    manifest.createdUtc = QDateTime::currentDateTimeUtc();
    manifest.expiresUtc = manifest.createdUtc.addDays(1);
    manifest.mediaUtc = manifest.createdUtc;
    manifest.originalFilename = QStringLiteral("image.png");
    manifest.safeDisplayFilename = QStringLiteral("image.png");
    manifest.mimeType = QStringLiteral("image/png");
    manifest.width = 2; manifest.height = 2;
    manifest.sstvMode = QStringLiteral("Martin M1");
    manifest.privacy.recipientConfirmed = true;
    manifest.encryption.mode = SstvShareEncryptionMode::EndToEnd;
    manifest.encryption.algorithm = QStringLiteral("aes-256-gcm");
    manifest.encryption.keyId = QStringLiteral("key-1");
    manifest.encryption.recipientKeyFingerprint = QString(64, QLatin1Char('a'));
    manifest.encryption.manifestBoundAsAuthenticatedData = true;
    manifest.transport.providerCanReadContent = false;
    const auto peerKey = aliceReload.pinnedRecipient("provider", "bob");
    const auto bobSecret = bobStore.loadPrivateKey();
    check(peerKey.ok() && bobSecret.ok(), "stored key lookup failed");
    const auto storedEncrypted = sstvShareEncryptForRecipient(manifest, *peerKey.key, pinnedBob, data);
    check(storedEncrypted && sstvShareDecryptForRecipient(storedEncrypted->manifest, *bobSecret.key, storedEncrypted->bytes) == data, "stored identity exchange failed");
    const auto integratedA = sstvShareSealPinnedEnvelope(aliceStore, manifest, data);
    const auto integratedB = sstvShareSealPinnedEnvelope(aliceStore, manifest, data);
    check(integratedA && integratedB && integratedA->bytes != integratedB->bytes,
          "pinned envelope A/B failed");
    check(sstvShareOpenStoredEnvelope(bobStore, "provider", "bob", integratedA->manifest, integratedA->bytes) == data
       && sstvShareOpenStoredEnvelope(bobStore, "provider", "bob", integratedB->manifest, integratedB->bytes) == data,
          "stored envelope A/B failed");
    check(!sstvShareOpenStoredEnvelope(aliceStore, "provider", "bob", integratedA->manifest, integratedA->bytes)
       && !sstvShareOpenStoredEnvelope(bobStore, "provider", "alice", integratedA->manifest, integratedA->bytes),
          "wrong profile or recipient accepted");
    auto noPin = manifest; noPin.recipientId = QStringLiteral("mallory");
    check(!sstvShareSealPinnedEnvelope(aliceStore, noPin, data), "unverified recipient accepted");
    auto downgraded = integratedA->manifest; downgraded.encryption.mode = SstvShareEncryptionMode::TransportTls;
    check(!sstvShareOpenStoredEnvelope(bobStore, "provider", "bob", downgraded, integratedA->bytes),
          "envelope downgrade accepted");
    const auto pinned = sstvShareX25519Fingerprint(randomB->publicKey);
    const auto recipientA = sstvShareEncryptForRecipient(manifest, randomB->publicKey, pinned, data);
    const auto recipientB = sstvShareEncryptForRecipient(manifest, randomB->publicKey, pinned, data);
    check(recipientA && recipientB && recipientA->bytes != recipientB->bytes
       && recipientA->manifest.encryption.keyId != recipientB->manifest.encryption.keyId, "recipient A/B ephemeral key failure");
    check(sstvShareDecryptForRecipient(recipientA->manifest, randomB->privateKey, recipientA->bytes) == data
       && sstvShareDecryptForRecipient(recipientB->manifest, randomB->privateKey, recipientB->bytes) == data, "recipient A/B roundtrip failed");
    const auto serialized = recipientA->manifest.toCanonicalJson();
    const auto parsed = parseSstvShareManifestV1(serialized);
    check(parsed.ok() && sstvShareDecryptForRecipient(*parsed.manifest, randomB->privateKey, recipientA->bytes) == data,
          "serialized manifest recipient roundtrip failed");
    check(!sstvShareEncryptForRecipient(manifest, randomA->publicKey, pinned, data), "untrusted replacement recipient accepted");
    check(!sstvShareDecryptForRecipient(recipientA->manifest, randomA->privateKey, recipientA->bytes), "wrong recipient private key accepted");
    auto recipientModified = recipientA->manifest;
    recipientModified.encryption.keyId = recipientB->manifest.encryption.keyId;
    check(!sstvShareDecryptForRecipient(recipientModified, randomB->privateKey, recipientA->bytes), "substituted ephemeral key accepted");
    recipientModified = recipientA->manifest;
    recipientModified.transferId = QUuid::createUuid();
    check(!sstvShareDecryptForRecipient(recipientModified, randomB->privateKey, recipientA->bytes), "substituted transfer accepted");
    recipientModified = recipientA->manifest;
    recipientModified.encryption.keyId = QStringLiteral("x25519-v1-nothex");
    check(!sstvShareDecryptForRecipient(recipientModified, randomB->privateKey, recipientA->bytes), "malformed ephemeral key accepted");
    const auto encryptedA = sstvShareEncryptPayload(manifest, *senderKey, data);
    const auto encryptedB = sstvShareEncryptPayload(manifest, key, data);
    check(encryptedA && encryptedB && encryptedA->bytes != encryptedB->bytes, "payload A/B encryption failed");
    check(sstvShareDecryptPayload(encryptedA->manifest, *recipientKey, encryptedA->bytes) == data
       && sstvShareDecryptPayload(encryptedB->manifest, key, encryptedB->bytes) == data, "payload A/B roundtrip failed");
    auto changedManifest = encryptedA->manifest;
    changedManifest.recipientId = QStringLiteral("mallory");
    check(!sstvShareDecryptPayload(changedManifest, *recipientKey, encryptedA->bytes), "changed recipient accepted");
    changedManifest = encryptedA->manifest;
    changedManifest.message = QStringLiteral("changed");
    check(!sstvShareDecryptPayload(changedManifest, *recipientKey, encryptedA->bytes), "changed message accepted");
    changedManifest = encryptedA->manifest;
    changedManifest.encryption.nonceBase64 = QString::fromLatin1(QByteArray(12, 1).toBase64());
    check(!sstvShareDecryptPayload(changedManifest, *recipientKey, encryptedA->bytes), "changed payload nonce accepted");
    changedManifest = encryptedA->manifest;
    auto changedPayload = encryptedA->bytes;
    changedPayload[0] = char(changedPayload[0] ^ 1);
    changedManifest.sha256 = QString::fromLatin1(QCryptographicHash::hash(changedPayload, QCryptographicHash::Sha256).toHex());
    check(!sstvShareDecryptPayload(changedManifest, *recipientKey, changedPayload), "tamper with recomputed digest accepted");
    changedManifest = encryptedA->manifest;
    changedManifest.encryption.mode = SstvShareEncryptionMode::TransportTls;
    check(!sstvShareDecryptPayload(changedManifest, *recipientKey, encryptedA->bytes), "downgrade accepted");
    std::cout << "AES-256-GCM known-answer and tamper tests passed\n";
    return 0;
 } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
