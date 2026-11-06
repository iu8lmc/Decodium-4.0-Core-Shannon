// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvShareKeyStore.h"
#include "SstvShareKeyAgreement.h"
#include "SstvShareP256.h"
#include "SstvShareRecipientEncryption.h"
#include "SstvShareSecurity.h"
#include <QCryptographicHash>
#include <QSet>
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
QString SstvShareKeyStore::senderSigningAccount(const QString& provider, const QString& sender) const
{
    if (!isSafeShareIdentifier(provider) || !isSafeShareIdentifier(sender)) return {};
    return QStringLiteral("sender-p256-") + hash(provider.toUtf8() + '\0' + sender.toUtf8());
}
SstvShareStoredKeyResult SstvShareKeyStore::load(const QString& account) const
{
    return loadBounded(account, 32, 32);
}
SstvShareStoredKeyResult SstvShareKeyStore::loadBounded(const QString& account, qsizetype minimum, qsizetype maximum) const
{
    if (service_.isEmpty() || account.isEmpty()) return {{}, SstvShareKeyStoreError::InvalidInput};
    if (!backend_.available()) return {{}, SstvShareKeyStoreError::Unavailable};
    auto result = backend_.lookup(service_, account);
    if (!result.backend_available || !result.error.isEmpty()) {
        result.value.fill(QChar('\0'));
        return {{}, SstvShareKeyStoreError::StorageFailure};
    }
    if (!result.found) {
        const bool inconsistent = !result.value.isEmpty();
        result.value.fill(QChar('\0'));
        return {{}, inconsistent ? SstvShareKeyStoreError::StorageFailure
                                 : SstvShareKeyStoreError::Missing};
    }
    QByteArray encoded = result.value.toLatin1();
    result.value.fill(QChar('\0'));
    if (minimum <= 0 || maximum < minimum || maximum > 512
        || encoded.size() > 4 * ((maximum + 2) / 3)) {
        wipe(encoded);
        return {{}, SstvShareKeyStoreError::Corrupt};
    }
    QByteArray key = QByteArray::fromBase64(encoded, QByteArray::AbortOnBase64DecodingErrors);
    const bool valid = key.size() >= minimum && key.size() <= maximum && key.toBase64() == encoded;
    wipe(encoded);
    if (!valid) { wipe(key); return {{}, SstvShareKeyStoreError::Corrupt}; }
    return {std::move(key), SstvShareKeyStoreError::None};
}
SstvShareStoredKeyResult SstvShareKeyStore::loadPrivateKey() const
{
    return loadPrivateKeyChain(nullptr);
}
SstvShareStoredKeyResult SstvShareKeyStore::loadPrivateKeyChain(int* terminalDepth) const
{
    if (!backend_.supports_private_key_storage()) return {{}, SstvShareKeyStoreError::Unavailable};
    auto result = load(QStringLiteral("identity-x25519-v1"));
    QSet<QString> visited;
    for (int depth = 0; result.ok(); ++depth) {
        const auto publicKey = sstvShareX25519PublicKey(*result.key);
        if (!publicKey) { wipe(*result.key); return {{}, SstvShareKeyStoreError::Corrupt}; }
        const auto fingerprint = sstvShareX25519Fingerprint(*publicKey);
        if (depth >= 128 || visited.contains(fingerprint)) {
            wipe(*result.key);
            return {{}, SstvShareKeyStoreError::Corrupt};
        }
        visited.insert(fingerprint);
        auto successor = load(QStringLiteral("identity-x25519-v1-next-") + fingerprint);
        if (successor.error == SstvShareKeyStoreError::Missing) {
            if (terminalDepth) *terminalDepth = depth;
            return result;
        }
        if (successor.ok()) {
            auto archived = load(QStringLiteral("identity-x25519-v1-archive-") + fingerprint);
            const bool matches = archived.ok() && *archived.key == *result.key;
            if (archived.ok()) wipe(*archived.key);
            if (!matches) {
                wipe(*result.key);
                wipe(*successor.key);
                return {{}, archived.error == SstvShareKeyStoreError::Missing
                             || archived.error == SstvShareKeyStoreError::None
                             ? SstvShareKeyStoreError::Corrupt : archived.error};
            }
        }
        wipe(*result.key);
        if (!successor.ok()) return successor;
        result = std::move(successor);
    }
    return result;
}
SstvShareStoredKeyResult SstvShareKeyStore::loadPrivateKeyForFingerprint(const QString& fingerprint) const
{
    const QByteArray encoded = fingerprint.toLatin1();
    const auto raw = QByteArray::fromHex(encoded);
    if (encoded.size() != 64 || raw.size() != 32 || raw.toHex() != encoded)
        return {{}, SstvShareKeyStoreError::InvalidInput};
    auto active = loadPrivateKey();
    if (active.ok()) {
        const auto publicKey = sstvShareX25519PublicKey(*active.key);
        if (publicKey && sstvShareX25519Fingerprint(*publicKey) == fingerprint) return active;
        wipe(*active.key);
    } else if (active.error != SstvShareKeyStoreError::Missing) {
        return active;
    }
    if (!backend_.supports_private_key_storage()) return {{}, SstvShareKeyStoreError::Unavailable};
    auto archived = load(QStringLiteral("identity-x25519-v1-archive-") + fingerprint);
    if (!archived.ok()) return archived;
    const auto publicKey = sstvShareX25519PublicKey(*archived.key);
    if (!publicKey || sstvShareX25519Fingerprint(*publicKey) != fingerprint) {
        wipe(*archived.key);
        return {{}, SstvShareKeyStoreError::Corrupt};
    }
    return archived;
}
SstvShareStoredKeyResult SstvShareKeyStore::archiveCurrentIdentity() const
{
    auto active = loadPrivateKey();
    if (!active.ok()) return {{}, active.error};
    auto publicKey = sstvShareX25519PublicKey(*active.key);
    if (!publicKey) { wipe(*active.key); return {{}, SstvShareKeyStoreError::Corrupt}; }
    const auto account = QStringLiteral("identity-x25519-v1-archive-") + sstvShareX25519Fingerprint(*publicKey);
    QByteArray encodedBytes = active.key->toBase64();
    QString encoded = QString::fromLatin1(encodedBytes);
    wipe(encodedBytes);
    const auto stored = backend_.store_if_absent(service_, account, encoded);
    encoded.fill(QChar('\0'));
    if (stored != secure_settings::InsertResult::Created
        && stored != secure_settings::InsertResult::AlreadyExists) {
        wipe(*active.key);
        return {{}, SstvShareKeyStoreError::StorageFailure};
    }
    auto archived = load(account);
    if (!archived.ok()) { wipe(*active.key); return {{}, archived.error}; }
    const bool matches = *archived.key == *active.key;
    wipe(*archived.key);
    wipe(*active.key);
    if (!matches) return {{}, SstvShareKeyStoreError::Corrupt};
    return {std::move(publicKey), SstvShareKeyStoreError::None};
}
SstvShareStoredKeyResult SstvShareKeyStore::rotateIdentity(const QString& expectedFingerprint) const
{
    int depth = 0;
    auto current = loadPrivateKeyChain(&depth);
    if (!current.ok()) return {{}, current.error};
    auto publicKey = sstvShareX25519PublicKey(*current.key);
    wipe(*current.key);
    if (depth >= 127) return {{}, SstvShareKeyStoreError::StorageFailure};
    if (!publicKey || sstvShareX25519Fingerprint(*publicKey) != expectedFingerprint)
        return {{}, SstvShareKeyStoreError::Conflict};
    const auto archived = archiveCurrentIdentity();
    if (!archived.ok()) return archived;
    if (*archived.key != *publicKey) return {{}, SstvShareKeyStoreError::Conflict};
    auto generated = sstvShareGenerateX25519KeyPair();
    if (!generated) return {{}, SstvShareKeyStoreError::StorageFailure};
    QByteArray bytes = generated->privateKey.toBase64();
    QString encoded = QString::fromLatin1(bytes);
    wipe(bytes);
    const auto inserted = backend_.store_if_absent(service_,
        QStringLiteral("identity-x25519-v1-next-") + expectedFingerprint, encoded);
    encoded.fill(QChar('\0'));
    wipe(generated->privateKey);
    if (inserted == secure_settings::InsertResult::AlreadyExists) return {{}, SstvShareKeyStoreError::Conflict};
    if (inserted != secure_settings::InsertResult::Created) return {{}, SstvShareKeyStoreError::StorageFailure};
    auto check = loadPrivateKey();
    if (!check.ok()) return {{}, check.error};
    auto actual = sstvShareX25519PublicKey(*check.key);
    wipe(*check.key);
    if (!actual || *actual != generated->publicKey) return {{}, SstvShareKeyStoreError::Conflict};
    return {std::move(actual), SstvShareKeyStoreError::None};
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
SstvShareStoredKeyResult SstvShareKeyStore::loadSigningPrivateKey() const
{
    return loadSigningPrivateKeyChain(nullptr);
}
SstvShareStoredKeyResult SstvShareKeyStore::loadSigningPrivateKeyChain(int* terminalDepth) const
{
    if (!backend_.supports_private_key_storage()) return {{}, SstvShareKeyStoreError::Unavailable};
    auto result = loadBounded(QStringLiteral("identity-p256-v1"), 1, 512);
    QSet<QString> visited;
    for (int depth = 0; result.ok(); ++depth) {
        const auto publicKey = sstvShareP256PublicKey(*result.key);
        const auto fingerprint = publicKey ? sstvShareP256Fingerprint(*publicKey) : QString{};
        if (fingerprint.isEmpty() || depth >= 128 || visited.contains(fingerprint)) {
            wipe(*result.key);
            return {{}, SstvShareKeyStoreError::Corrupt};
        }
        visited.insert(fingerprint);
        auto successor = loadBounded(QStringLiteral("identity-p256-v1-next-") + fingerprint, 1, 512);
        if (successor.error == SstvShareKeyStoreError::Missing) {
            if (terminalDepth) *terminalDepth = depth;
            return result;
        }
        wipe(*result.key);
        result = std::move(successor);
    }
    return result;
}
SstvShareStoredKeyResult SstvShareKeyStore::rotateSigningIdentity(const QString& expectedFingerprint) const
{
    int depth = 0;
    auto current = loadSigningPrivateKeyChain(&depth);
    if (!current.ok()) return {{}, current.error};
    const auto oldPublic = sstvShareP256PublicKey(*current.key);
    wipe(*current.key);
    if (depth >= 127) return {{}, SstvShareKeyStoreError::StorageFailure};
    if (!oldPublic || sstvShareP256Fingerprint(*oldPublic) != expectedFingerprint)
        return {{}, SstvShareKeyStoreError::Conflict};
    auto generated = sstvShareGenerateP256KeyPair();
    if (!generated) return {{}, SstvShareKeyStoreError::StorageFailure};
    QByteArray bytes = generated->privateDer.toBase64();
    QString encoded = QString::fromLatin1(bytes);
    wipe(bytes);
    const auto inserted = backend_.store_if_absent(service_,
        QStringLiteral("identity-p256-v1-next-") + expectedFingerprint, encoded);
    encoded.fill(QChar('\0'));
    wipe(generated->privateDer);
    if (inserted == secure_settings::InsertResult::AlreadyExists) return {{}, SstvShareKeyStoreError::Conflict};
    if (inserted != secure_settings::InsertResult::Created) return {{}, SstvShareKeyStoreError::StorageFailure};
    auto check = loadSigningPrivateKey();
    if (!check.ok()) return {{}, check.error};
    auto actual = sstvShareP256PublicKey(*check.key);
    wipe(*check.key);
    if (!actual || *actual != generated->publicDer) return {{}, SstvShareKeyStoreError::Conflict};
    return {std::move(actual), SstvShareKeyStoreError::None};
}
SstvShareStoredKeyResult SstvShareKeyStore::createOrLoadSigningPublicKey() const
{
    auto existing = loadSigningPrivateKey();
    if (existing.ok()) {
        auto publicKey = sstvShareP256PublicKey(*existing.key);
        wipe(*existing.key);
        return publicKey ? SstvShareStoredKeyResult {std::move(publicKey), SstvShareKeyStoreError::None}
                         : SstvShareStoredKeyResult {{}, SstvShareKeyStoreError::Corrupt};
    }
    if (existing.error != SstvShareKeyStoreError::Missing) return {{}, existing.error};
    auto generated = sstvShareGenerateP256KeyPair();
    if (!generated) return {{}, SstvShareKeyStoreError::StorageFailure};
    QByteArray encodedBytes = generated->privateDer.toBase64();
    QString encoded = QString::fromLatin1(encodedBytes);
    wipe(encodedBytes);
    const auto stored = backend_.store_if_absent(service_, QStringLiteral("identity-p256-v1"), encoded);
    encoded.fill(QChar('\0'));
    wipe(generated->privateDer);
    if (stored != secure_settings::InsertResult::Created
        && stored != secure_settings::InsertResult::AlreadyExists)
        return {{}, SstvShareKeyStoreError::StorageFailure};
    auto check = loadSigningPrivateKey();
    if (!check.ok()) return {{}, check.error};
    auto publicKey = sstvShareP256PublicKey(*check.key);
    wipe(*check.key);
    if (!publicKey || (stored == secure_settings::InsertResult::Created && *publicKey != generated->publicDer))
        return {{}, SstvShareKeyStoreError::Conflict};
    return {std::move(publicKey), SstvShareKeyStoreError::None};
}
SstvShareKeyStoreError SstvShareKeyStore::pinSenderSigningKey(const QString& provider, const QString& sender,
    const QByteArray& publicDer, const QString& verifiedFingerprint) const
{
    const auto account = senderSigningAccount(provider, sender);
    if (service_.isEmpty() || account.isEmpty() || verifiedFingerprint.isEmpty()
        || sstvShareP256Fingerprint(publicDer) != verifiedFingerprint)
        return SstvShareKeyStoreError::InvalidInput;
    auto existing = pinnedSenderSigningKey(provider, sender);
    if (existing.ok()) return *existing.key == publicDer ? SstvShareKeyStoreError::None : SstvShareKeyStoreError::Conflict;
    if (existing.error != SstvShareKeyStoreError::Missing) return existing.error;
    const auto stored = backend_.store_if_absent(service_, account, QString::fromLatin1(publicDer.toBase64()));
    if (stored != secure_settings::InsertResult::Created && stored != secure_settings::InsertResult::AlreadyExists)
        return SstvShareKeyStoreError::StorageFailure;
    const auto check = pinnedSenderSigningKey(provider, sender);
    if (!check.ok()) return check.error;
    return *check.key == publicDer ? SstvShareKeyStoreError::None : SstvShareKeyStoreError::Conflict;
}
SstvShareStoredKeyResult SstvShareKeyStore::pinnedSenderSigningKey(const QString& provider, const QString& sender) const
{
    return loadSenderSigningChain(senderSigningAccount(provider, sender), nullptr);
}
SstvShareStoredKeyResult SstvShareKeyStore::loadSenderSigningChain(const QString& account, int* terminalDepth) const
{
    auto result = loadBounded(account, 91, 91);
    QSet<QString> visited;
    for (int depth = 0; result.ok(); ++depth) {
        const auto fingerprint = sstvShareP256Fingerprint(*result.key);
        if (fingerprint.isEmpty() || depth >= 128 || visited.contains(fingerprint))
            return {{}, SstvShareKeyStoreError::Corrupt};
        visited.insert(fingerprint);
        auto successor = loadBounded(account + QStringLiteral("-next-") + fingerprint, 91, 91);
        if (successor.error == SstvShareKeyStoreError::Missing) {
            if (terminalDepth) *terminalDepth = depth;
            return result;
        }
        result = std::move(successor);
    }
    return result;
}
SstvShareKeyStoreError SstvShareKeyStore::replacePinnedSenderSigningKey(const QString& provider, const QString& sender,
    const QString& expectedFingerprint, const QByteArray& publicDer, const QString& verifiedFingerprint) const
{
    const auto account = senderSigningAccount(provider, sender);
    if (service_.isEmpty() || account.isEmpty() || verifiedFingerprint.isEmpty()
        || sstvShareP256Fingerprint(publicDer) != verifiedFingerprint)
        return SstvShareKeyStoreError::InvalidInput;
    int depth = 0;
    const auto current = loadSenderSigningChain(account, &depth);
    if (!current.ok()) return current.error;
    if (sstvShareP256Fingerprint(*current.key) != expectedFingerprint || *current.key == publicDer)
        return SstvShareKeyStoreError::Conflict;
    if (depth >= 127) return SstvShareKeyStoreError::StorageFailure;
    auto ancestor = loadBounded(account, 91, 91);
    for (int i = 0; i <= depth && ancestor.ok(); ++i) {
        if (*ancestor.key == publicDer) return SstvShareKeyStoreError::Conflict;
        if (i < depth) ancestor = loadBounded(account + QStringLiteral("-next-")
                                              + sstvShareP256Fingerprint(*ancestor.key), 91, 91);
    }
    if (!ancestor.ok()) return ancestor.error;
    const auto inserted = backend_.store_if_absent(service_, account + QStringLiteral("-next-") + expectedFingerprint,
                                                   QString::fromLatin1(publicDer.toBase64()));
    if (inserted == secure_settings::InsertResult::AlreadyExists) return SstvShareKeyStoreError::Conflict;
    if (inserted != secure_settings::InsertResult::Created) return SstvShareKeyStoreError::StorageFailure;
    const auto check = pinnedSenderSigningKey(provider, sender);
    if (!check.ok()) return check.error;
    return *check.key == publicDer ? SstvShareKeyStoreError::None : SstvShareKeyStoreError::Conflict;
}
SstvShareKeyStoreError SstvShareKeyStore::pinRecipient(const QString& provider, const QString& recipient,
    const QByteArray& publicKey, const QString& verifiedFingerprint) const
{
    const auto account = recipientAccount(provider, recipient);
    if (service_.isEmpty() || account.isEmpty() || publicKey.size() != 32
        || sstvShareX25519Fingerprint(publicKey) != verifiedFingerprint
        || !usablePeerKey(publicKey))
        return SstvShareKeyStoreError::InvalidInput;
    auto existing = pinnedRecipient(provider, recipient);
    if (existing.ok()) return *existing.key == publicKey ? SstvShareKeyStoreError::None : SstvShareKeyStoreError::Conflict;
    if (existing.error != SstvShareKeyStoreError::Missing) return existing.error;
    const auto stored = backend_.store_if_absent(service_, account, QString::fromLatin1(publicKey.toBase64()));
    if (stored != secure_settings::InsertResult::Created
        && stored != secure_settings::InsertResult::AlreadyExists)
        return SstvShareKeyStoreError::StorageFailure;
    // Another process can rotate the newly created pin before this read.
    // Confirm the current chain tip, rather than only its immutable root.
    const auto check = pinnedRecipient(provider, recipient);
    if (!check.ok()) return check.error;
    return *check.key == publicKey ? SstvShareKeyStoreError::None : SstvShareKeyStoreError::Conflict;
}
SstvShareStoredKeyResult SstvShareKeyStore::pinnedRecipient(const QString& provider, const QString& recipient) const
{
    return loadRecipientChain(recipientAccount(provider, recipient), nullptr);
}
SstvShareStoredKeyResult SstvShareKeyStore::loadRecipientChain(const QString& account, int* terminalDepth) const
{
    auto result = load(account);
    QSet<QString> visited;
    for (int depth = 0; result.ok(); ++depth) {
        if (!usablePeerKey(*result.key)) return {{}, SstvShareKeyStoreError::Corrupt};
        const auto fingerprint = sstvShareX25519Fingerprint(*result.key);
        if (depth >= 128 || visited.contains(fingerprint)) return {{}, SstvShareKeyStoreError::Corrupt};
        visited.insert(fingerprint);
        auto successor = load(account + QStringLiteral("-next-") + fingerprint);
        if (successor.error == SstvShareKeyStoreError::Missing) {
            if (terminalDepth) *terminalDepth = depth;
            return result;
        }
        result = std::move(successor);
    }
    return result;
}
SstvShareKeyStoreError SstvShareKeyStore::replacePinnedRecipient(const QString& provider, const QString& recipient,
    const QString& expectedFingerprint, const QByteArray& publicKey, const QString& verifiedFingerprint) const
{
    const auto account = recipientAccount(provider, recipient);
    if (service_.isEmpty() || account.isEmpty() || publicKey.size() != 32
        || sstvShareX25519Fingerprint(publicKey) != verifiedFingerprint || !usablePeerKey(publicKey))
        return SstvShareKeyStoreError::InvalidInput;
    int depth = 0;
    const auto current = loadRecipientChain(account, &depth);
    if (!current.ok()) return current.error;
    if (sstvShareX25519Fingerprint(*current.key) != expectedFingerprint || *current.key == publicKey)
        return SstvShareKeyStoreError::Conflict;
    if (depth >= 127) return SstvShareKeyStoreError::StorageFailure;
    // Reject transitions to any previously trusted key: such a link would
    // form a cycle and make the current pin impossible to resolve.
    auto ancestor = load(account);
    for (int i = 0; i <= depth && ancestor.ok(); ++i) {
        if (*ancestor.key == publicKey) return SstvShareKeyStoreError::Conflict;
        if (i < depth) ancestor = load(account + QStringLiteral("-next-") + sstvShareX25519Fingerprint(*ancestor.key));
    }
    if (!ancestor.ok()) return ancestor.error;
    const auto inserted = backend_.store_if_absent(service_, account + QStringLiteral("-next-") + expectedFingerprint,
                                                   QString::fromLatin1(publicKey.toBase64()));
    if (inserted == secure_settings::InsertResult::AlreadyExists) return SstvShareKeyStoreError::Conflict;
    if (inserted != secure_settings::InsertResult::Created) return SstvShareKeyStoreError::StorageFailure;
    const auto check = pinnedRecipient(provider, recipient);
    if (!check.ok()) return check.error;
    return *check.key == publicKey ? SstvShareKeyStoreError::None : SstvShareKeyStoreError::Conflict;
}
} // namespace decodium::sstv::sharing
