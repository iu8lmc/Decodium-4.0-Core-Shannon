// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "SecureSettings.hpp"

namespace secure_settings {

// Adds cross-process create-only semantics to an existing secret backend.
// Only use with a backend that protects private values and does not place
// them in process arguments. The lock directory must be shared by all local
// Decodium sessions using the same underlying secret store.
class CoordinatedSecretBackend final : public Backend {
public:
    CoordinatedSecretBackend(const Backend& backend, QString lockDirectory);
    bool available() const override;
    bool supports_private_key_storage() const override;
    LookupResult lookup(const QString& service, const QString& account) const override;
    InsertResult store_if_absent(const QString& service, const QString& account,
                                 const QString& value) const override;
    bool store(const QString& service, const QString& account, const QString& value,
               QString* error = nullptr) const override;
    bool remove(const QString& service, const QString& account,
                QString* error = nullptr) const override;
private:
    QString lockPath(const QString& service, const QString& account) const;
    const Backend& backend_;
    QString lockDirectory_;
};
} // namespace secure_settings
