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
    // Creates only if absent; never replaces an existing or corrupt identity.
    SstvShareStoredKeyResult createOrLoadPublicKey() const;
    // Confirmation must be obtained out-of-band, independently of provider data.
    SstvShareKeyStoreError pinRecipient(const QString& provider, const QString& recipient,
                                       const QByteArray& publicKey, const QString& verifiedFingerprint) const;
    SstvShareStoredKeyResult pinnedRecipient(const QString& provider, const QString& recipient) const;
private:
    QString recipientAccount(const QString& provider, const QString& recipient) const;
    SstvShareStoredKeyResult load(const QString& account) const;
    const secure_settings::Backend& backend_;
    QString service_;
};
} // namespace decodium::sstv::sharing
