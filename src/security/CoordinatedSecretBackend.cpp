// SPDX-License-Identifier: GPL-3.0-or-later
#include "CoordinatedSecretBackend.h"

#include <QCryptographicHash>
#include <QDir>
#include <QLockFile>
#include <utility>

namespace secure_settings {
CoordinatedSecretBackend::CoordinatedSecretBackend(const Backend& backend, QString lockDirectory)
    : backend_(backend), lockDirectory_(std::move(lockDirectory)) {}

bool CoordinatedSecretBackend::available() const
{
    return !lockDirectory_.isEmpty() && QDir::isAbsolutePath(lockDirectory_)
        && backend_.available() && backend_.protects_private_values();
}

bool CoordinatedSecretBackend::supports_private_key_storage() const { return available(); }

QString CoordinatedSecretBackend::lockPath(const QString& service, const QString& account) const
{
    if (!available() || service.isEmpty() || account.isEmpty()
        || service.size() > 256 || account.size() > 256
        || service.contains(QChar('\0')) || account.contains(QChar('\0'))
        || !QDir().mkpath(lockDirectory_)) return {};
    QByteArray input = service.toUtf8();
    input.append('\0');
    input.append(account.toUtf8());
    const auto digest = QCryptographicHash::hash(input, QCryptographicHash::Sha256).toHex();
    return QDir(lockDirectory_).filePath(QString::fromLatin1(digest) + QStringLiteral(".lock"));
}

LookupResult CoordinatedSecretBackend::lookup(const QString& service, const QString& account) const
{
    if (lockPath(service, account).isEmpty()) return {false, false, {}, QStringLiteral("Secure store unavailable")};
    return backend_.lookup(service, account);
}

InsertResult CoordinatedSecretBackend::store_if_absent(const QString& service, const QString& account,
                                                        const QString& value) const
{
    const QString path = lockPath(service, account);
    if (path.isEmpty() || value.isEmpty() || value.size() > 4096) return InsertResult::Error;
    QLockFile lock(path);
    // Do not consider a live owner's lock stale merely because a secure-store
    // interaction is slow. QLockFile can still recover a dead owner's lock.
    lock.setStaleLockTime(0);
    if (!lock.tryLock(10000)) return InsertResult::Error;
    const auto existing = backend_.lookup(service, account);
    if (!existing.backend_available || !existing.error.isEmpty()) return InsertResult::Error;
    if (existing.found) return InsertResult::AlreadyExists;
    // Missing with a nonempty value is an inconsistent backend response, not
    // permission to overwrite a possibly existing identity or recipient pin.
    if (!existing.value.isEmpty()) return InsertResult::Error;
    QString error;
    return backend_.store(service, account, value, &error)
        ? InsertResult::Created : InsertResult::Error;
}

bool CoordinatedSecretBackend::store(const QString& service, const QString& account,
                                     const QString& value, QString* error) const
{
    if (store_if_absent(service, account, value) == InsertResult::Created) return true;
    if (error) *error = QStringLiteral("Secure item was not created");
    return false;
}

bool CoordinatedSecretBackend::remove(const QString& service, const QString& account, QString* error) const
{
    const QString path = lockPath(service, account);
    if (path.isEmpty()) return false;
    QLockFile lock(path);
    lock.setStaleLockTime(0);
    if (!lock.tryLock(10000)) return false;
    return backend_.remove(service, account, error);
}
} // namespace secure_settings
