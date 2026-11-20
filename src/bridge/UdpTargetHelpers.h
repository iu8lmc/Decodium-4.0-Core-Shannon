#pragma once
#include <QVariant>
#include <QHostAddress>
#include <QHostInfo>

inline quint16 udpPortFromSettingValue(const QVariant& value, quint16 fallback, bool allowZero = false)
{
    bool ok = false;
    uint const raw = value.toUInt(&ok);
    if (!ok || raw > 65535 || (!allowZero && raw == 0)) {
        return fallback;
    }
    return static_cast<quint16>(raw);
}

inline QHostAddress resolveUdpHostAddress(QString const& serverName)
{
    QHostAddress target;
    QString const cleanServer = serverName.trimmed();
    if (target.setAddress(cleanServer)) {
        return target;
    }

    QHostInfo const hostInfo = QHostInfo::fromName(cleanServer);
    for (QHostAddress const& address : hostInfo.addresses()) {
        if (!address.isNull()) {
            return address;
        }
    }
    return {};
}
