// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "SecureSettings.hpp"
#include <QtGlobal>
#if defined(Q_OS_MACOS)
#include <Security/Security.h>

namespace secure_settings {
// Dedicated create-only E2EE backend. Calls can block on Keychain and must be
// made by the sharing worker. The optional path is for an isolated test keychain.
class MacKeychainBackend final : public Backend {
public:
    explicit MacKeychainBackend(const QString& keychainPath = {});
    ~MacKeychainBackend() override;
    MacKeychainBackend(const MacKeychainBackend&) = delete;
    MacKeychainBackend& operator=(const MacKeychainBackend&) = delete;
    bool available() const override;
    bool supports_private_key_storage() const override;
    LookupResult lookup(const QString& service, const QString& account) const override;
    InsertResult store_if_absent(const QString& service, const QString& account, const QString& value) const override;
    bool store(const QString& service, const QString& account, const QString& value, QString* error = nullptr) const override;
    bool remove(const QString& service, const QString& account, QString* error = nullptr) const override;
private:
    SecKeychainRef keychain_ {nullptr};
};
} // namespace secure_settings
#endif
