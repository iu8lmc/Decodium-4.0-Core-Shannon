#include "DecoPortHubOwners.h"
#include "DecodiumSerialPortRegistry.h"
#include <QCoreApplication>
#include <QSerialPortInfo>
#include <QVariantList>
#include <QVariantMap>

namespace {
#define HUB_TR(s) QCoreApplication::translate("DecodiumBridge", s)

// Riga del pannello: stato reale, programma che occupa la porta e il suo PID.
QVariantMap hubRow(const QString& kind, const QString& name, const QString& label,
                   const QString& state, const QString& detail)
{
    QVariantMap row;
    row.insert(QStringLiteral("kind"), kind);
    row.insert(QStringLiteral("name"), name);
    row.insert(QStringLiteral("label"), label);
    row.insert(QStringLiteral("state"), state);
    row.insert(QStringLiteral("detail"), detail);
    return row;
}

// Presa TCP/UDP locale: chi la tiene. Se il proprietario e' questo processo,
// e' di Decodium; se non si sa, resta "unknown".
QVariantMap hubPortRow(const QString& kind, quint16 port, const QString& label,
                       const decoport_hub::PortUse& use)
{
    QString const name = QStringLiteral("%1 %2").arg(kind == QLatin1String("udp") ? "UDP" : "TCP").arg(port);
    if (!use.known)
        return hubRow(kind, name, label, QStringLiteral("unknown"), QStringLiteral("non verificabile"));
    if (!use.used)
        return hubRow(kind, name, label, QStringLiteral("free"), QStringLiteral("libera"));
    if (quint32(QCoreApplication::applicationPid()) == use.pid)
        return hubRow(kind, name, label, QStringLiteral("decodium"),
                      QStringLiteral("Decodium (PID %1), %2").arg(use.pid).arg(use.state));
    return hubRow(kind, name, label, QStringLiteral("busy"),
                  QStringLiteral("%1 (PID %2), %3").arg(use.program).arg(use.pid).arg(use.state));
}
} // namespace

// Stato reale del hub per il pannello DecoPort. Gira in un thread di lavoro:
// la scansione degli handle e le tabelle TCP non devono bloccare la GUI. Le
// porte seriali non vengono mai aperte: aprirle puo' muovere DTR/RTS sulla radio.
QVariantList buildDecoPortHubRows(QString const& catPort, int sharePort)
{
    QVariantList rows;
    QHash<QString, decoport_hub::PortUse> const owners = decoport_hub::serialOwners();
    for (QSerialPortInfo const& info : QSerialPortInfo::availablePorts()) {
        QString const name = info.portName().toUpper();
        bool const virt = info.description().contains(QStringLiteral("com0com"), Qt::CaseInsensitive);
        QString const kind = virt ? QStringLiteral("virtual") : QStringLiteral("serial");
        QString state = QStringLiteral("free");
        QString detail = HUB_TR("nessun programma la usa");
        auto const it = owners.constFind(name);
        const QString internalOwner = decodium::ports::SerialPortRegistry::ownerForPort(
            info.systemLocation().isEmpty() ? name : info.systemLocation());
        if (!internalOwner.isEmpty()) {
            state = QStringLiteral("decodium");
            detail = HUB_TR("Decodium: %1").arg(internalOwner);
        } else if (!catPort.isEmpty() && name == catPort) {
            // Configuration fallback: before the first successful open CAT
            // has no live lease yet, but the panel should still identify the
            // selected endpoint as the CAT candidate.
            state = QStringLiteral("decodium");
            detail = HUB_TR("CAT configurato in Decodium (porta non ancora aperta)");
        } else if (it != owners.constEnd() && it->used) {
            state = QStringLiteral("busy");
            detail = QStringLiteral("%1 (PID %2), %3").arg(it->program).arg(it->pid).arg(it->state);
        } else if (virt) {
            detail = HUB_TR("porta virtuale, nessun programma la usa");
        }
        rows.append(hubRow(kind, name, info.description(), state, detail));
    }

    // Prese TCP e UDP: il proprietario viene dal sistema, non da una prova.
    rows.append(hubPortRow(QStringLiteral("tcp"), quint16(sharePort), HUB_TR("Condivisione CAT (rigctl)"),
                           decoport_hub::tcpOwner(quint16(sharePort))));
    rows.append(hubPortRow(QStringLiteral("tcp"), 4532, HUB_TR("rigctld Hamlib"),
                           decoport_hub::tcpOwner(4532)));
    rows.append(hubPortRow(QStringLiteral("tcp"), 4534, HUB_TR("Condivisione spot"),
                           decoport_hub::tcpOwner(4534)));
    rows.append(hubPortRow(QStringLiteral("tcp"), 8080, HUB_TR("Web server"),
                           decoport_hub::tcpOwner(8080)));
    rows.append(hubPortRow(QStringLiteral("tcp"), 8082, HUB_TR("Rotore (HTTP)"),
                           decoport_hub::tcpOwner(8082)));
    rows.append(hubPortRow(QStringLiteral("tcp"), 19090, HUB_TR("Controllo remoto (WS)"),
                           decoport_hub::tcpOwner(19090)));
    rows.append(hubPortRow(QStringLiteral("udp"), 5559, HUB_TR("Gateway DecoPort"),
                           decoport_hub::udpOwner(5559)));
    rows.append(hubPortRow(QStringLiteral("udp"), 2237, HUB_TR("Messaggi UDP (WSJT-X/JTDX)"),
                           decoport_hub::udpOwner(2237)));
    return rows;
}

#undef HUB_TR
