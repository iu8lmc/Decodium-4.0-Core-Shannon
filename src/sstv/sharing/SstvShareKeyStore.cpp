// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvShareKeyStore.h"
#include "SstvShareKeyAgreement.h"
#include "SstvShareRecipientEncryption.h"
#include "SstvShareSecurity.h"
#include <QCryptographicHash>
#include <openssl/crypto.h>
#include <utility>

namespace decodium::sstv::sharing {
namespace {
QString hash(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
void wipe(QByteArray& value) { if (!value.isEmpty()) OPENSSL_cleanse(value.data(), std::size_t(value.size())); }
bool usablePeerKey(const QByteArray& publicKey) {
    // Public validation scalar: never used as an encryption key. OpenSSL's
    // X25519 derivation rejects low-order points yielding an all-zero secret.
    const QByteArray scalar(32, '\0');
    const auto local = sstvShareX25519PublicKey(scalar);
    if (!local) return false;
    auto derived = sstvShareDeriveTransferKey(SstvShareKeyRole::SenderEphemeral,
        scalar, *local, publicKey, QByteArrayLiteral("pin-validation"));
    if (!derived) return false;
    wipe(*derived);
    return true;
}
}
SstvShareKeyStore::SstvShareKeyStore(const secure_settings::Backend& backend, QString profile)
    : backend_(backend)
{
    if (isSafeShareIdentifier(profile))
        service_ = QStringLiteral("Decodium.SSTV.E2EE.v1.") + hash(profile.toUtf8());
}
QString SstvShareKeyStore::recipientAccount(const QString& provider, const QString& recipient) const
{
    if (!isSafeShareIdentifier(provider) || !isSafeShareIdentifier(recipient)) return {};
    return QStringLiteral("peer-") + hash(provider.toUtf8() + '\0' + recipient.toUtf8());
}
SstvShareStoredKeyResult SstvShareKeyStore::load(const QString& account) const
{
    if (service_.isEmpty() || account.isEmpty()) return {{}, SstvShareKeyStoreError::InvalidInput};
    if (!backend_.available()) return {{}, SstvShareKeyStoreError::Unavailable};
    auto result = backend_.lookup(service_, account);
    if (!result.backend_available || !result.error.isEmpty()) {
        result.value.fill(QChar('\0'));
        return {{}, SstvShareKeyStoreError::StorageFailure};
    }
    if (!result.found) return {{}, SstvShareKeyStoreError::Missing};
    QByteArray encoded = result.value.toLatin1();
    result.value.fill(QChar('\0'));
    QByteArray key = QByteArray::fromBase64(encoded, QByteArray::AbortOnBase64DecodingErrors);
    const bool valid = key.size() == 32 && key.toBase64() == encoded;
    wipe(encoded);
    if (!valid) { wipe(key); return {{}, SstvShareKeyStoreError::Corrupt}; }
    return {std::move(key), SstvShareKeyStoreError::None};
}
SstvShareStoredKeyResult SstvShareKeyStore::loadPrivateKey() const
{
    if (!backend_.supports_private_key_storage()) return {{}, SstvShareKeyStoreError::Unavailable};
    auto result = load(QStringLiteral("identity-x25519-v1"));
    if (result.ok() && !sstvShareX25519PublicKey(*result.key)) {
        wipe(*result.key);
        return {{}, SstvShareKeyStoreError::Corrupt};
    }
    return result;
}
SstvShareStoredKeyResult SstvShareKeyStore::createOrLoadPublicKey() const
{
    auto existing = loadPrivateKey();
    if (existing.ok()) {
        auto publicKey = sstvShareX25519PublicKey(*existing.key);
        wipe(*existing.key);
        if (!publicKey) return {{}, SstvShareKeyStoreError::Corrupt};
        return {std::move(publicKey), SstvShareKeyStoreError::None};
    }
    if (existing.error != SstvShareKeyStoreError::Missing) return {{}, existing.error};
    auto generated = sstvShareGenerateX25519KeyPair();
    if (!generated) return {{}, SstvShareKeyStoreError::StorageFailure};
    QByteArray encodedBytes = generated->privateKey.toBase64();
    QString encoded = QString::fromLatin1(encodedBytes);
    wipe(encodedBytes);
    const auto stored = backend_.store_if_absent(service_, QStringLiteral("identity-x25519-v1"), encoded);
    encoded.fill(QChar('\0'));
    wipe(generated->privateKey);
    if (stored != secure_settings::InsertResult::Created
        && stored != secure_settings::InsertResult::AlreadyExists)
        return {{}, SstvShareKeyStoreError::StorageFailure};
    auto check = loadPrivateKey();
    if (!check.ok()) return {{}, check.error};
    auto publicKey = sstvShareX25519PublicKey(*check.key);
    wipe(*check.key);
    if (!publicKey || (stored == secure_settings::InsertResult::Created
                       && *publicKey != generated->publicKey))
        return {{}, SstvShareKeyStoreError::Conflict};
    return {std::move(publicKey), SstvShareKeyStoreError::None};
}
SstvShareKeyStoreError SstvShareKeyStore::pinRecipient(const QString& provider, const QString& recipient,
    const QByteArray& publicKey, const QString& verifiedFingerprint) const
{
    const auto account = recipientAccount(provider, recipient);
    if (service_.isEmpty() || account.isEmpty() || publicKey.size() != 32
        || sstvShareX25519Fingerprint(publicKey) != verifiedFingerprint
        || !usablePeerKey(publicKey))
        return SstvShareKeyStoreError::InvalidInput;
    auto existing = load(account);
    if (existing.ok()) return *existing.key == publicKey ? SstvShareKeyStoreError::None : SstvShareKeyStoreError::Conflict;
    if (existing.error != SstvShareKeyStoreError::Missing) return existing.error;
    const auto stored = backend_.store_if_absent(service_, account, QString::fromLatin1(publicKey.toBase64()));
    if (stored != secure_settings::InsertResult::Created
        && stored != secure_settings::InsertResult::AlreadyExists)
        return SstvShareKeyStoreError::StorageFailure;
    const auto check = load(account);
    if (!check.ok()) return check.error;
    return *check.key == publicKey ? SstvShareKeyStoreError::None : SstvShareKeyStoreError::Conflict;
}
SstvShareStoredKeyResult SstvShareKeyStore::pinnedRecipient(const QString& provider, const QString& recipient) const
{
    return load(recipientAccount(provider, recipient));
}
} // namespace decodium::sstv::sharing
