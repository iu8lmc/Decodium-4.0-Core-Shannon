// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/sstv/sharing/SstvShareAead.h"
#include "../../src/sstv/sharing/SstvShareEncryptedPayload.h"
#include "../../src/sstv/sharing/SstvShareKeyAgreement.h"
#include "../../src/sstv/sharing/SstvShareP256.h"
#include "../../src/sstv/sharing/SstvShareRecipientEncryption.h"
#include "../../src/sstv/sharing/SstvShareKeyStore.h"
#include "../../src/sstv/sharing/SstvShareSecureEnvelope.h"
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <iostream>
#include <stdexcept>
using namespace decodium::sstv::sharing;
class MemoryBackend final : public secure_settings::Backend {
public:
    bool online {true};
    bool privateStorage {true};
    bool failStore {false};
    bool inconsistentLookup {false};
    mutable unsigned insertCalls {0};
    mutable QHash<QString, QString> values;
    bool available() const override { return online; }
    bool supports_private_key_storage() const override { return privateStorage; }
    secure_settings::LookupResult lookup(const QString& service, const QString& account) const override {
        secure_settings::LookupResult result; result.backend_available = online;
        if (inconsistentLookup) { result.value = QStringLiteral("partial-result"); return result; }
        const auto key = service + QLatin1Char('/') + account;
        result.found = values.contains(key); if (result.found) result.value = values.value(key);
        return result;
    }
    secure_settings::InsertResult store_if_absent(const QString& service, const QString& account, const QString& value) const override {
        ++insertCalls;
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
static void testP256SigningCandidate()
{
    const QByteArray knownKey = QByteArray::fromHex(
        "3059301306072a8648ce3d020106082a8648ce3d03010703420004"
        "60fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"
        "7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299");
    const QByteArray knownA = QByteArray::fromHex(
        "3045022100efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716"
        "02200834e36ad29a83bf2bc9385e491d6099c8fdf9d1ed67aa7ea5f51f93782857a9");
    const QByteArray highS = QByteArray::fromHex(
        "3046022100efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716"
        "022100f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff4064dc4ab2f843acda8");
    const QByteArray knownB = QByteArray::fromHex(
        "3045022100f1abb023518351cd71d881567b1ea663ed3efcf6c5132b354f28d3b0b7d38367"
        "0220019f4113742a2b14bd25926b49c649155f267e60d3814b4c0cc84250e46f0083");
    check(sstvShareP256Verify(knownKey, "sample", knownA), "P256 RFC sample failed");
    check(!sstvShareP256Verify(knownKey, "sample", highS), "P256 accepted malleable high-S signature");
    check(sstvShareP256Verify(knownKey, "test", knownB), "P256 RFC test failed");
    check(!sstvShareP256Verify(knownKey, "test", knownA), "P256 accepted changed RFC message");
    auto a = sstvShareGenerateP256KeyPair();
    auto b = sstvShareGenerateP256KeyPair();
    check(a && b && a->publicDer != b->publicDer, "P256 generation failed");
    for (const auto* pair : {&*a, &*b}) {
        const auto* other = pair == &*a ? &*b : &*a;
        check(sstvShareP256PublicKey(pair->privateDer) == pair->publicDer, "P256 private key reload failed");
        check(sstvShareP256PublicKeyValid(pair->publicDer) && sstvShareP256Fingerprint(pair->publicDer).size() == 64,
              "P256 public validation failed");
        const QByteArray message("sender-authentication-test");
        const auto signature = sstvShareP256Sign(pair->privateDer, message);
        check(signature && sstvShareP256Verify(pair->publicDer, message, *signature), "P256 signature failed");
        check(!sstvShareP256Verify(other->publicDer, message, *signature), "P256 accepted wrong signer");
        check(!sstvShareP256Verify(pair->publicDer, message + 'x', *signature), "P256 accepted changed message");
        QByteArray altered = *signature; altered[altered.size()-1] ^= 1;
        check(!sstvShareP256Verify(pair->publicDer, message, altered), "P256 accepted changed signature");
        auto invalidPublic = pair->publicDer;
        std::fill(invalidPublic.begin() + 27, invalidPublic.end(), '\0');
        check(!sstvShareP256PublicKeyValid(invalidPublic) && sstvShareP256Fingerprint(invalidPublic).isEmpty(),
              "P256 accepted off-curve public key");
        check(!sstvShareP256PublicKeyValid(pair->publicDer + 'x'), "P256 accepted trailing public-key data");
        check(!sstvShareP256PublicKey(pair->privateDer + 'x'), "P256 accepted trailing private-key data");
        for (int i = 0; i < 24; ++i) {
            const auto repeated = sstvShareP256Sign(pair->privateDer, message + QByteArray::number(i));
            check(repeated && sstvShareP256Verify(pair->publicDer, message + QByteArray::number(i), *repeated),
                  "P256 generated noncanonical signature");
        }
    }
    const QByteArray oversized(kSstvShareP256MaximumMessageBytes + 1, 'x');
    check(!sstvShareP256Sign(a->privateDer, oversized), "P256 signed oversized message");
    check(!sstvShareP256Verify(a->publicDer, oversized, knownA), "P256 accepted oversized message");
    a->privateDer.fill('\0'); b->privateDer.fill('\0');
}

int main() {
 try {
    testP256SigningCandidate();
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
    const auto aliceSigning = aliceStore.createOrLoadSigningPublicKey();
    const auto bobSigning = bobStore.createOrLoadSigningPublicKey();
    check(aliceSigning.ok() && bobSigning.ok() && aliceSigning.key != bobSigning.key,
          "P256 signing identities are not isolated");
    check(aliceReload.createOrLoadSigningPublicKey().key == aliceSigning.key, "P256 identity changed on reload");
    check(aliceStore.loadSigningPrivateKey().key != aliceStore.loadPrivateKey().key,
          "P256 signing and X25519 encryption secrets are not separate");
    const auto aliceSigningFingerprint = sstvShareP256Fingerprint(*aliceSigning.key);
    const auto bobSigningFingerprint = sstvShareP256Fingerprint(*bobSigning.key);
    check(bobStore.pinSenderSigningKey("provider", "alice", *aliceSigning.key, aliceSigningFingerprint)
              == SstvShareKeyStoreError::None, "P256 sender pin failed");
    check(aliceStore.pinSenderSigningKey("provider", "bob", *bobSigning.key, bobSigningFingerprint)
              == SstvShareKeyStoreError::None, "P256 reverse sender pin failed");
    check(bobStore.pinnedSenderSigningKey("provider", "alice").key == aliceSigning.key,
          "P256 sender pin did not reload");
    const auto insertsAfterPin = backend.insertCalls;
    check(bobStore.pinSenderSigningKey("provider", "alice", *aliceSigning.key, aliceSigningFingerprint)
              == SstvShareKeyStoreError::None && backend.insertCalls == insertsAfterPin,
          "P256 repeated pin was not idempotent");
    check(bobStore.pinnedSenderSigningKey("other-provider", "alice").error == SstvShareKeyStoreError::Missing,
          "P256 sender pin escaped provider scope");
    check(bobStore.pinSenderSigningKey("provider", "alice", *bobSigning.key, bobSigningFingerprint)
              == SstvShareKeyStoreError::Conflict, "P256 sender key replacement accepted");
    check(bobStore.pinSenderSigningKey("provider", "other", *aliceSigning.key, QString(64, '0'))
              == SstvShareKeyStoreError::InvalidInput, "unverified P256 fingerprint accepted");
    auto invalidSigningKey = *aliceSigning.key;
    std::fill(invalidSigningKey.begin() + 27, invalidSigningKey.end(), '\0');
    check(bobStore.pinSenderSigningKey("provider", "other", invalidSigningKey, QString(64, '0'))
              == SstvShareKeyStoreError::InvalidInput, "invalid P256 key pinned");
    for (const auto* store : {&aliceStore, &bobStore}) {
        auto key = store->loadSigningPrivateKey();
        check(key.ok(), "stored P256 signing key not available");
        const auto signature = sstvShareP256Sign(*key.key, "stored-identity-proof");
        const auto& publicKey = store == &aliceStore ? aliceSigning.key : bobSigning.key;
        check(signature && sstvShareP256Verify(*publicKey, "stored-identity-proof", *signature),
              "stored P256 signing key did not verify");
        key.key->fill('\0');
    }
    const auto pinnedBob = sstvShareX25519Fingerprint(*bobIdentity.key);
    check(aliceStore.pinRecipient("provider", "bob", *bobIdentity.key, pinnedBob) == SstvShareKeyStoreError::None, "pin failed");
    check(aliceReload.pinnedRecipient("provider", "bob").key == bobIdentity.key, "pin reload failed");
    const auto storedBeforeInconsistentLookup = backend.values;
    const auto insertCallsBeforeInconsistentLookup = backend.insertCalls;
    backend.inconsistentLookup = true;
    check(aliceStore.createOrLoadPublicKey().error == SstvShareKeyStoreError::StorageFailure,
          "inconsistent missing identity lookup was treated as absence");
    check(aliceStore.createOrLoadSigningPublicKey().error == SstvShareKeyStoreError::StorageFailure,
          "inconsistent signing-key lookup was treated as absence");
    check(aliceStore.pinRecipient("provider", "bob", *bobIdentity.key, pinnedBob)
              == SstvShareKeyStoreError::StorageFailure,
          "inconsistent missing pin lookup was treated as absence");
    check(backend.values == storedBeforeInconsistentLookup
              && backend.insertCalls == insertCallsBeforeInconsistentLookup,
          "inconsistent lookup triggered a secret-store write");
    backend.inconsistentLookup = false;
    check(aliceStore.createOrLoadPublicKey().key == aliceIdentity.key
              && aliceStore.pinnedRecipient("provider", "bob").key == bobIdentity.key,
          "identity or pin changed after inconsistent lookup");
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
    check(aliceStore.loadSigningPrivateKey().error == SstvShareKeyStoreError::Unavailable,
          "P256 key accepted without private storage");
    backend.privateStorage = true;
    backend.online = false;
    check(aliceStore.loadPrivateKey().error == SstvShareKeyStoreError::Unavailable, "unavailable backend accepted");
    backend.online = true;
    backend.failStore = true;
    SstvShareKeyStore missingStore(backend, QStringLiteral("new-profile"));
    check(missingStore.createOrLoadPublicKey().error == SstvShareKeyStoreError::StorageFailure, "failed secure write accepted");
    check(missingStore.createOrLoadSigningPublicKey().error == SstvShareKeyStoreError::StorageFailure,
          "failed P256 secure write accepted");
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
    MemoryBackend signingOnlyBackend;
    SstvShareKeyStore signingOnly(signingOnlyBackend, QStringLiteral("signing-only"));
    check(signingOnly.createOrLoadSigningPublicKey().ok() && signingOnlyBackend.values.size() == 1,
          "isolated P256 identity creation failed");
    signingOnlyBackend.values.begin().value() = QStringLiteral("not-base64");
    check(signingOnly.createOrLoadSigningPublicKey().error == SstvShareKeyStoreError::Corrupt,
          "corrupt P256 signing identity accepted");
    check(signingOnlyBackend.values.constBegin().value() == QStringLiteral("not-base64"),
          "corrupt P256 signing identity overwritten");
    MemoryBackend pinOnlyBackend;
    SstvShareKeyStore pinOnly(pinOnlyBackend, QStringLiteral("pin-only"));
    check(pinOnly.pinSenderSigningKey("provider", "alice", *aliceSigning.key, aliceSigningFingerprint)
              == SstvShareKeyStoreError::None, "isolated P256 pin failed");
    pinOnlyBackend.values.begin().value() = QString::fromLatin1(QByteArray(91, '\0').toBase64());
    check(pinOnly.pinnedSenderSigningKey("provider", "alice").error == SstvShareKeyStoreError::Corrupt,
          "corrupt P256 sender pin accepted");
    check(pinOnly.pinSenderSigningKey("provider", "alice", *aliceSigning.key, aliceSigningFingerprint)
              == SstvShareKeyStoreError::Conflict, "corrupt P256 sender pin silently replaced");
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
    check(bobStore.pinRecipient("provider", "alice", *aliceIdentity.key,
                                sstvShareX25519Fingerprint(*aliceIdentity.key)) == SstvShareKeyStoreError::None,
          "reverse recipient pin failed");
    auto reverse = manifest;
    reverse.transferId = QUuid::createUuid();
    reverse.senderId = QStringLiteral("bob"); reverse.recipientId = QStringLiteral("alice");
    const auto signedA = sstvShareSealSignedEnvelope(aliceStore, manifest, QByteArrayLiteral("signed image A"));
    const auto signedB = sstvShareSealSignedEnvelope(bobStore, reverse, QByteArrayLiteral("signed image B"));
    check(signedA && signedB, "signed envelope A/B seal failed");
    for (const auto* envelope : {&*signedA, &*signedB}) {
        const bool forward = envelope == &*signedA;
        const auto& receiver = forward ? bobStore : aliceStore;
        const QString recipient = forward ? QStringLiteral("bob") : QStringLiteral("alice");
        const QByteArray expected = forward ? QByteArrayLiteral("signed image A") : QByteArrayLiteral("signed image B");
        const auto serialized = envelope->manifest.toCanonicalJson();
        const auto parsed = parseSstvShareManifestV1(serialized);
        check(parsed.ok() && sstvShareOpenStoredEnvelope(receiver, "provider", recipient, *parsed.manifest,
                                                          envelope->bytes, true) == expected,
              "signed envelope A/B roundtrip failed");
        auto json = QJsonDocument::fromJson(serialized).object();
        auto encryptionObject = json.value(QStringLiteral("encryption")).toObject();
        encryptionObject.remove(QStringLiteral("senderKeyFingerprint"));
        json.insert(QStringLiteral("encryption"), encryptionObject);
        check(!parseSstvShareManifestV1(QJsonDocument(json).toJson(QJsonDocument::Compact)).ok(),
              "signed envelope parsed with missing sender fingerprint");
        encryptionObject.insert(QStringLiteral("senderKeyFingerprint"), QString {});
        encryptionObject.insert(QStringLiteral("senderAuthAlgorithm"), QString {});
        encryptionObject.insert(QStringLiteral("senderSignatureBase64"), QString {});
        json.insert(QStringLiteral("encryption"), encryptionObject);
        check(!parseSstvShareManifestV1(QJsonDocument(json).toJson(QJsonDocument::Compact)).ok(),
              "signed envelope parsed with empty authentication fields");
        auto changed = envelope->manifest;
        changed.message = QStringLiteral("provider altered metadata");
        check(!sstvShareOpenStoredEnvelope(receiver, "provider", recipient, changed, envelope->bytes, true),
              "signed envelope accepted altered metadata");
        changed = envelope->manifest;
        changed.senderId = QStringLiteral("mallory");
        check(!sstvShareOpenStoredEnvelope(receiver, "provider", recipient, changed, envelope->bytes, true),
              "signed envelope accepted changed sender");
        changed = envelope->manifest;
        QByteArray signature = QByteArray::fromBase64(changed.encryption.senderSignatureBase64.toLatin1());
        signature[signature.size()-1] ^= 1;
        changed.encryption.senderSignatureBase64 = QString::fromLatin1(signature.toBase64());
        check(!sstvShareOpenStoredEnvelope(receiver, "provider", recipient, changed, envelope->bytes, true),
              "signed envelope accepted altered signature");
        auto payload = envelope->bytes; payload[0] ^= 1;
        changed = envelope->manifest;
        changed.sha256 = QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
        check(!sstvShareOpenStoredEnvelope(receiver, "provider", recipient, changed, payload, true),
              "signed envelope accepted changed ciphertext and recomputed hash");
        changed = envelope->manifest;
        changed.encryption.senderAuthAlgorithm.clear();
        changed.encryption.senderKeyFingerprint.clear();
        changed.encryption.senderSignatureBase64.clear();
        check(!sstvShareOpenStoredEnvelope(receiver, "provider", recipient, changed, envelope->bytes, true)
              && !sstvShareOpenStoredEnvelope(receiver, "provider", recipient, changed, envelope->bytes, false),
              "stripped sender authentication accepted");
        changed = envelope->manifest;
        changed.encryption.senderKeyFingerprint.clear();
        check(!changed.validate().ok(), "partially missing sender authentication accepted");
    }
    check(!sstvShareOpenStoredEnvelope(bobStore, "provider", "bob", integratedA->manifest, integratedA->bytes, true),
          "strict sender-authentication policy accepted unsigned legacy envelope");
    SstvShareKeyStore malloryStore(backend, QStringLiteral("mallory"));
    check(malloryStore.createOrLoadSigningPublicKey().ok()
          && malloryStore.pinRecipient("provider", "bob", *bobIdentity.key, pinnedBob) == SstvShareKeyStoreError::None,
          "impersonation fixture setup failed");
    const auto impersonation = sstvShareSealSignedEnvelope(malloryStore, manifest, data);
    check(impersonation && !sstvShareOpenStoredEnvelope(bobStore, "provider", "bob",
                                                         impersonation->manifest, impersonation->bytes, true),
          "signed envelope accepted a different signer impersonating Alice");
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
    QByteArray substitutePublic(32, '\0');
    substitutePublic[0] = char(9);
    for (const auto* envelope : {&*recipientA, &*recipientB}) {
        auto substituted = envelope->manifest;
        substituted.encryption.keyId = QStringLiteral("x25519-v1-") + QString::fromLatin1(substitutePublic.toHex());
        check(substituted.validate().ok(), "substitute key manifest is not valid");
        check(sstvShareDeriveTransferKey(SstvShareKeyRole::Recipient, randomB->privateKey,
              substitutePublic, randomB->publicKey,
              substituted.transferId.toString(QUuid::WithoutBraces).toLower().toLatin1()).has_value(),
              "substitute X25519 public key is not usable");
        check(!sstvShareDecryptForRecipient(substituted, randomB->privateKey, envelope->bytes),
              "substituted valid ephemeral key accepted");
    }
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
    for (const auto* envelope : {&*encryptedA, &*encryptedB}) {
        auto changedExpiry = envelope->manifest;
        changedExpiry.expiresUtc = changedExpiry.expiresUtc.addSecs(-300);
        check(changedExpiry.validate().ok(), "expiry tamper fixture is not a valid manifest");
        const auto& envelopeKey = envelope == &*encryptedA ? *recipientKey : key;
        check(!sstvShareDecryptPayload(changedExpiry, envelopeKey, envelope->bytes),
              "modified expiry accepted");
    }
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
