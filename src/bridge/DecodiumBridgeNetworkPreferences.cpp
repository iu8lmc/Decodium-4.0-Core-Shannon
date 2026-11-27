#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include "Network/RemoteCommandServer.hpp"
#include <QNetworkInterface>
#include <QSettings>
#include <algorithm>

int DecodiumBridge::remoteWebSocketPort() const
{
    if (m_remoteServer && m_remoteServer->isRunning() && m_remoteServer->wsPort() > 0) {
        return static_cast<int>(m_remoteServer->wsPort());
    }

    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    bool ok = false;
    int wsPort = s.value(QStringLiteral("RemoteWsPort"),
                         s.value(QStringLiteral("WebAppWsPort"), 0)).toInt(&ok);
    if (ok && wsPort > 0) {
        return wsPort;
    }

    int httpPort = s.value(QStringLiteral("RemoteHttpPort"),
                           s.value(QStringLiteral("WebAppHttpPort"), 19091)).toInt(&ok);
    if (ok && httpPort > 1) {
        return httpPort - 1;
    }

    return 19090;
}

QStringList DecodiumBridge::networkInterfaceNames() const
{
    QStringList names;
    for (auto const& netIf : QNetworkInterface::allInterfaces()) {
        auto const flags = netIf.flags();
        bool const isUp = flags.testFlag(QNetworkInterface::IsUp);
        bool const canMulticast = flags.testFlag(QNetworkInterface::CanMulticast);
        bool const isLoopback = flags.testFlag(QNetworkInterface::IsLoopBack);
        bool const usable = (isUp && canMulticast) || isLoopback;
        if (usable) {
            names << netIf.name();
        }
    }
    names.removeDuplicates();
    std::sort(names.begin(), names.end(), [](QString const& a, QString const& b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return names;
}

QString DecodiumBridge::udpInterfaceName() const
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(s);
    QVariant local = s.value(QStringLiteral("UDPInterface"));
    if (local.isValid()) {
        QStringList interfaces = local.toStringList();
        if (!interfaces.isEmpty()) return interfaces.constFirst();
    }

    // Read from legacy INI (canonical source for Configuration/MessageClient)
    QVariant v = readSettingFromLegacyIni(QStringLiteral("UDPInterface"));
    if (v.isValid()) {
        QStringList interfaces = v.toStringList();
        if (!interfaces.isEmpty()) return interfaces.constFirst();
    }
    return {};
}

void DecodiumBridge::setUdpInterfaceName(const QString& name)
{
    QVariant value = name.trimmed().isEmpty()
        ? QVariant()
        : QVariant(QStringList{name.trimmed()});

    // Write to legacy INI (canonical source)
    syncSettingToLegacyIni(QStringLiteral("UDPInterface"), value);

    // Also keep registry copy for backwards compatibility
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    if (name.trimmed().isEmpty()) {
        s.remove(QStringLiteral("UDPInterface"));
    } else {
        s.setValue(QStringLiteral("UDPInterface"), QStringList{name.trimmed()});
    }
    s.sync();
    scheduleUdpMessageClientRestart();
}

