// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

#include <QtGlobal>

#include <utility>

namespace decodium::ports {

// A process-local, cross-thread lease for a serial endpoint.  It does not
// replace the operating system's serial-port exclusivity: it closes the
// check-then-open window between Decodium's own CAT, CW and DecoPort users.
class SerialPortLease
{
public:
    SerialPortLease() = default;
    SerialPortLease(SerialPortLease const&) = delete;
    SerialPortLease& operator=(SerialPortLease const&) = delete;
    SerialPortLease(SerialPortLease&& other) noexcept;
    SerialPortLease& operator=(SerialPortLease&& other) noexcept;
    ~SerialPortLease();

    bool isValid() const { return m_token != 0; }
    QStringList ports() const { return m_ports; }
    QString owner() const { return m_owner; }
    void release();

private:
    friend class SerialPortRegistry;
    SerialPortLease(quint64 token, QStringList ports, QString owner)
        : m_token(token), m_ports(std::move(ports)), m_owner(std::move(owner)) {}

    quint64 m_token {0};
    QStringList m_ports;
    QString m_owner;
};

class SerialPortRegistry
{
public:
    // Atomically reserves all distinct ports.  On failure no port is kept and
    // the first conflicting endpoint and owner are returned when requested.
    static SerialPortLease acquire(QStringList const& ports, QString owner,
                                   QString* conflictPort = nullptr,
                                   QString* conflictOwner = nullptr);

    // The same normalisation is used for COM5/\\.\\COM5 and for /dev aliases.
    // It is public so diagnostics and tests can use exactly the registry key.
    static QString normalize(QString value);

    // Empty means that this Decodium process currently has no lease for port.
    static QString ownerForPort(QString const& port);
};

} // namespace decodium::ports
