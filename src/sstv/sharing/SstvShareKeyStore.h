// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../../security/SecureSettings.hpp"
#include <QByteArray>
#include <QString>
#include <optional>

namespace decodium::sstv::sharing {
enum class SstvShareKeyStoreError { None, InvalidInput, Unavailable, Missing, Corrupt, Conflict, StorageFailure };
struct SstvShareStoredKeyResult final {
    std::optional<QByteArray> key;
    SstvShareKeyStoreError error {SstvShareKeyStoreError::None};
    bool ok() const noexcept { return key.has_value() && error == SstvShareKeyStoreError::None; }
};
// Storage calls must run on a worker, never the UI thread. Identity and pin
// creation require the backend's atomic store-if-absent operation.
class SstvShareKeyStore final {
public:
    SstvShareKeyStore(const secure_settings::Backend& backend, QString profile);
    SstvShareStoredKeyResult loadPrivateKey() const;
    // Selects the active identity or an immutable archived identity by its
    // public fingerprint. Does not create or rotate keys.
    SstvShareStoredKeyResult loadPrivateKeyForFingerprint(const QString& fingerprint) const;
    // Preserves the current identity under its fingerprint without replacing
    // any existing entry. Returns the public key, never the private key.
    SstvShareStoredKeyResult archiveCurrentIdentity() const;
    // Rotate only if the caller confirmed the current fingerprint. Existing
    // keys remain available for messages encrypted before rotation.
    SstvShareStoredKeyResult rotateIdentity(const QString& expectedFingerprint) const;
    // Creates only if absent; never replaces an existing or corrupt identity.
    SstvShareStoredKeyResult createOrLoadPublicKey() const;
    // Separate signing identity and sender pins for authenticated envelopes.
    SstvShareStoredKeyResult loadSigningPrivateKey() const;
    SstvShareStoredKeyResult createOrLoadSigningPublicKey() const;
    SstvShareStoredKeyResult rotateSigningIdentity(const QString& expectedFingerprint) const;
    // The fingerprint must be confirmed independently of the provider.
    SstvShareKeyStoreError pinSenderSigningKey(const QString& provider, const QString& sender,
                                              const QByteArray& publicDer, const QString& verifiedFingerprint) const;
    SstvShareStoredKeyResult pinnedSenderSigningKey(const QString& provider, const QString& sender) const;
    // Changes the current sender pin only after independent verification of
    // both fingerprints. Earlier signing keys are no longer accepted.
    SstvShareKeyStoreError replacePinnedSenderSigningKey(const QString& provider, const QString& sender,
        const QString& expectedFingerprint, const QByteArray& publicDer, const QString& verifiedFingerprint) const;
    // Confirmation must be obtained out-of-band, independently of provider data.
    SstvShareKeyStoreError pinRecipient(const QString& provider, const QString& recipient,
                                       const QByteArray& publicKey, const QString& verifiedFingerprint) const;
    SstvShareStoredKeyResult pinnedRecipient(const QString& provider, const QString& recipient) const;
    SstvShareKeyStoreError replacePinnedRecipient(const QString& provider, const QString& recipient,
        const QString& expectedFingerprint, const QByteArray& publicKey, const QString& verifiedFingerprint) const;
private:
    SstvShareStoredKeyResult loadPrivateKeyChain(int* depth) const;
    SstvShareStoredKeyResult loadRecipientChain(const QString& account, int* depth) const;
    QString recipientAccount(const QString& provider, const QString& recipient) const;
    QString senderSigningAccount(const QString& provider, const QString& sender) const;
    SstvShareStoredKeyResult loadSenderSigningChain(const QString& account, int* terminalDepth) const;
    SstvShareStoredKeyResult loadSigningPrivateKeyChain(int* terminalDepth) const;
    SstvShareStoredKeyResult load(const QString& account) const;
    SstvShareStoredKeyResult loadBounded(const QString& account, qsizetype minimum, qsizetype maximum) const;
    const secure_settings::Backend& backend_;
    QString service_;
};
} // namespace decodium::sstv::sharing
