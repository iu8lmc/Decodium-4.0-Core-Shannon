// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumSerialPortRegistry.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>

#include <utility>

namespace decodium::ports {

namespace {

struct Entry
{
    quint64 token {0};
    QString owner;
};

QMutex& registryMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QString, Entry>& entries()
{
    static QHash<QString, Entry> value;
    return value;
}

quint64& nextToken()
{
    static quint64 value = 1;
    return value;
}

} // namespace

QString SerialPortRegistry::normalize(QString value)
{
    value = value.trimmed();
    if (value.isEmpty()
        || value.compare(QStringLiteral("CAT"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("None"), Qt::CaseInsensitive) == 0) {
        return {};
    }

    if (value.startsWith(QStringLiteral("\\\\.\\")))
        value.remove(0, 4);
#if !defined(Q_OS_WIN)
    // Hamlib and Qt may receive either "cu.usbserial..." or the full /dev
    // path.  Make both spellings one key before comparing them.
    if (!value.startsWith(QLatin1Char('/')) && !value.contains(QLatin1Char(':')))
        value.prepend(QStringLiteral("/dev/"));
    const QString canonical = QFileInfo(value).canonicalFilePath();
    if (!canonical.isEmpty())
        value = canonical;
#endif
    return QDir::cleanPath(value).toCaseFolded();
}

SerialPortLease SerialPortRegistry::acquire(QStringList const& ports, QString owner,
                                            QString* conflictPort,
                                            QString* conflictOwner)
{
    QStringList normalized;
    for (const QString& port : ports) {
        const QString key = normalize(port);
        if (!key.isEmpty() && !normalized.contains(key))
            normalized.append(key);
    }
    owner = owner.trimmed();
    if (normalized.isEmpty() || owner.isEmpty())
        return {};

    QMutexLocker lock(&registryMutex());
    for (const QString& key : normalized) {
        const auto it = entries().constFind(key);
        if (it == entries().constEnd())
            continue;
        if (conflictPort)
            *conflictPort = key;
        if (conflictOwner)
            *conflictOwner = it->owner;
        qWarning().noquote() << "[PORTREG] denied" << key
                             << "requestedBy=" << owner
                             << "heldBy=" << it->owner;
        return {};
    }

    quint64 token = nextToken()++;
    if (token == 0)
        token = nextToken()++;
    for (const QString& key : normalized)
        entries().insert(key, Entry {token, owner});
    qInfo().noquote() << "[PORTREG] acquired" << normalized
                      << "owner=" << owner << "token=" << token;
    return SerialPortLease(token, normalized, std::move(owner));
}

QString SerialPortRegistry::ownerForPort(QString const& port)
{
    const QString key = normalize(port);
    if (key.isEmpty())
        return {};
    QMutexLocker lock(&registryMutex());
    const auto it = entries().constFind(key);
    return it == entries().constEnd() ? QString() : it->owner;
}

SerialPortLease::SerialPortLease(SerialPortLease&& other) noexcept
    : m_token(other.m_token)
    , m_ports(std::move(other.m_ports))
    , m_owner(std::move(other.m_owner))
{
    other.m_token = 0;
}

SerialPortLease& SerialPortLease::operator=(SerialPortLease&& other) noexcept
{
    if (this == &other)
        return *this;
    release();
    m_token = other.m_token;
    m_ports = std::move(other.m_ports);
    m_owner = std::move(other.m_owner);
    other.m_token = 0;
    return *this;
}

SerialPortLease::~SerialPortLease()
{
    release();
}

void SerialPortLease::release()
{
    if (m_token == 0)
        return;
    QMutexLocker lock(&registryMutex());
    bool released = false;
    for (const QString& key : std::as_const(m_ports)) {
        const auto it = entries().find(key);
        if (it != entries().end() && it->token == m_token) {
            entries().erase(it);
            released = true;
        }
    }
    if (released)
        qInfo().noquote() << "[PORTREG] released" << m_ports
                          << "owner=" << m_owner << "token=" << m_token;
    m_token = 0;
    m_ports.clear();
    m_owner.clear();
}

} // namespace decodium::ports
