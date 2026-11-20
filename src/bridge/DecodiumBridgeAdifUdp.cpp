#include "DecodiumBridge.h"
#include "Network/MessageClient.hpp"
#include "UdpTargetHelpers.h"
#include "Network/AdifUdpPayload.hpp"
#include <QUdpSocket>

void decodiumBridgeAdifUdpLog(const QString&);

bool DecodiumBridge::udpSendRawAdifDatagram(const QString& label,
                                            const QString& serverName,
                                            quint16 port,
                                            const QString& dxCall,
                                            const QByteArray& adifRecord)
{
    QString const cleanServer = serverName.trimmed();
    if (cleanServer.isEmpty() || port == 0 || adifRecord.trimmed().isEmpty()) {
        decodiumBridgeAdifUdpLog(QStringLiteral("%1 skipped: missing target or ADIF payload").arg(label));
        return false;
    }

    QHostAddress const target = resolveUdpHostAddress(cleanServer);
    if (target.isNull()) {
        decodiumBridgeAdifUdpLog(QStringLiteral("%1 skipped: cannot resolve %2").arg(label, cleanServer));
        return false;
    }

    QUdpSocket socket;
    QByteArray const payload = decodium::adif::udpPayload(adifRecord) + " <eor>";
    qint64 const written = socket.writeDatagram(payload, target, port);
    if (written < 0) {
        decodiumBridgeAdifUdpLog(QStringLiteral("%1 send failed for %2: %3")
                      .arg(label, dxCall, socket.errorString()));
        return false;
    }

    decodiumBridgeAdifUdpLog(QStringLiteral("%1 sent: call=%2 target=%3:%4 bytes=%5")
                  .arg(label)
                  .arg(dxCall)
                  .arg(target.toString())
                  .arg(port)
                  .arg(written));
    return true;
}

void DecodiumBridge::udpSendN1mmLoggedQso(const QString& dxCall, const QByteArray& adifRecord)
{
    if (!getSetting(QStringLiteral("BroadcastToN1MM"), false).toBool()) {
        return;
    }

    QString const serverName = getSetting(QStringLiteral("N1MMServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
    quint16 const port = udpPortFromSettingValue(getSetting(QStringLiteral("N1MMServerPort"), 2333), 2333);
    // This is the ADIF-over-UDP stream used by WSJT-X's “N1MM Logger+
    // Broadcasts” option. HRD Logbook QSO Forwarding accepts that same stream
    // on its WSJT-X receive port (normally 2333).
    if (udpSendRawAdifDatagram(QStringLiteral("N1MM / HRD ADIF UDP"), serverName, port, dxCall, adifRecord)) {
        emit statusMessage(QStringLiteral("QSO %1 -> UDP ADIF (N1MM / HRD) %2:%3 queued")
                           .arg(dxCall, serverName)
                           .arg(port));
    }
}

void DecodiumBridge::udpSendLoggedQso(const QString& dxCall, const QString& dxGrid,
                                      double freqHz, const QString& mode,
                                      const QDateTime& timeOnUtc,
                                      const QDateTime& timeOffUtc,
                                      const QString& rstSent, const QString& rstRcvd,
                                      const QByteArray& adifRecord,
                                      const QString& comments,
                                      const QString& propMode,
                                      const QString& satellite,
                                      const QString& satMode,
                                      const QString& freqRx)
{
    // The embedded FT4/FT8 backend may generate and/or mirror the local ADIF
    // record, but it does not own Decodium's external logger fan-out.  In
    // particular, recovery paths that commit a QSO after TX5 has started can
    // reach this function while the legacy backend is active.  Returning here
    // used to leave those QSOs in decodium_log.adi only: DecoDXLog never saw
    // the QSOLogged/LoggedADIF datagrams.  The caller-side duplicate guards
    // already ensure that one committed QSO is emitted once.
    if (usingLegacyBackendForTx()) {
        decodiumBridgeAdifUdpLog(QStringLiteral("UDP logged QSO: forwarding bridge-committed legacy QSO to external loggers"));
    }

    if (!m_udpMessageClient && !m_udpSecondaryMessageClient && !m_udpTertiaryMessageClient) {
        initUdpMessageClient(usingLegacyBackendForTx());
    }
    bool const wsjtxUdpAvailable = m_udpMessageClient || m_udpSecondaryMessageClient || m_udpTertiaryMessageClient;
    if (!wsjtxUdpAvailable) {
        decodiumBridgeAdifUdpLog(QStringLiteral("UDP logged QSO: WSJT-X MessageClient unavailable; trying raw ADIF targets"));
    }

    QDateTime effectiveTimeOffUtc = timeOffUtc.isValid() ? timeOffUtc : QDateTime::currentDateTimeUtc();
    if (timeOnUtc.isValid() && effectiveTimeOffUtc < timeOnUtc) {
        effectiveTimeOffUtc = timeOnUtc;
    }

    QString const reportSent = rstSent.trimmed().isEmpty() ? QStringLiteral("-10") : rstSent.trimmed();
    QString const reportRcvd = rstRcvd.trimmed();   // empty stays empty: nothing was received
    QString const txPower = m_stationPowerWatts > 0 ? QString::number(m_stationPowerWatts) : QString();
    QString const cleanComments = comments.trimmed();
    QString const cleanPropMode = propMode.trimmed();
    QString const cleanSatellite = satellite.trimmed();
    QString const cleanSatMode = satMode.trimmed();
    QString const cleanFreqRx = freqRx.trimmed();
    QByteArray const udpAdifRecord = decodium::adif::udpPayload(adifRecord);

    int targets = 0;
    int wsjtxAdifTargets = 0;
    int rawAdifTargets = 0;
    bool primaryQsoLoggedEmitted = false;
    bool primaryLoggedAdifEmitted = false;
    auto sendLoggedQso = [&](MessageClient* client, bool sendAdif) {
        if (!client) return;
        client->qso_logged(
            effectiveTimeOffUtc,
            dxCall,
            dxGrid,
            static_cast<MessageClient::Frequency>(qRound64(freqHz)),
            mode,
            reportSent,
            reportRcvd,
            txPower,
            cleanComments,
            QString(),
            timeOnUtc,
            QString(),
            m_callsign,
            m_grid,
            QString(),
            QString(),
            cleanPropMode,
            cleanSatellite,
            cleanSatMode,
            cleanFreqRx);
        if (sendAdif) {
            client->logged_ADIF(udpAdifRecord);
            ++wsjtxAdifTargets;
        }
        ++targets;
    };

    if (wsjtxUdpAvailable) {
        if (udpTrafficEnabled(QStringLiteral("UDPPrimary"), QStringLiteral("Qso"))) {
            // Keep this pair inseparable. The former LoggedAdifEnabled key
            // is no longer exposed in the UI and may be stale in profiles
            // saved before 1.0.540; allowing it to suppress LoggedADIF breaks
            // LogHX while the visible "QSO logged" control says enabled.
            sendLoggedQso(m_udpMessageClient, true);
            primaryQsoLoggedEmitted = m_udpMessageClient != nullptr;
            primaryLoggedAdifEmitted = m_udpMessageClient != nullptr;
        }
        if (m_udpTertiaryMessageClient
            && udpTrafficEnabled(QStringLiteral("UDPTertiary"), QStringLiteral("Qso"))) {
            sendLoggedQso(m_udpTertiaryMessageClient, true);
        }
    }

    bool const secondaryQsoEnabled = getSetting(QStringLiteral("UDPSecondaryEnabled"), true).toBool()
        && udpTrafficEnabled(QStringLiteral("UDPSecondary"), QStringLiteral("Qso"));
    if (secondaryQsoEnabled) {
        QString const secondaryServerName =
            getSetting(QStringLiteral("UDPSecondaryServer"),
                       getSetting(QStringLiteral("UDPServer"), QStringLiteral("127.0.0.1"))).toString().trimmed();
        quint16 const secondaryPort = udpPortFromSettingValue(
            getSetting(QStringLiteral("UDPSecondaryServerPort"), 2239), 2239);
        if (udpSendRawAdifDatagram(QStringLiteral("UDP secondary raw ADIF"),
                                   secondaryServerName, secondaryPort, dxCall, udpAdifRecord)) {
            ++rawAdifTargets;
        }
    }

    bool const tertiaryQsoOnly = getSetting(QStringLiteral("UDPTertiaryEnabled"), false).toBool()
        && udpTrafficEnabled(QStringLiteral("UDPTertiary"), QStringLiteral("Qso"))
        && !m_udpTertiaryMessageClient;
    if (tertiaryQsoOnly) {
        QString const tertiaryServerName =
            getSetting(QStringLiteral("UDPTertiaryServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
        quint16 const tertiaryPort = udpPortFromSettingValue(
            getSetting(QStringLiteral("UDPTertiaryServerPort"), 2237), 2237);
        if (udpSendRawAdifDatagram(QStringLiteral("UDP tertiary QSO-only raw ADIF"),
                                   tertiaryServerName, tertiaryPort, dxCall, udpAdifRecord)) {
            ++rawAdifTargets;
        }
    }

    decodiumBridgeAdifUdpLog(QStringLiteral("UDP logged QSO sent: call=%1 primaryQSOLogged=%2 primaryLoggedADIF=%3 targets=%4 wsjtxAdifTargets=%5 rawAdifTargets=%6 bytes=%7")
                  .arg(dxCall)
                  .arg(primaryQsoLoggedEmitted ? QStringLiteral("emitted") : QStringLiteral("not-emitted"))
                  .arg(primaryLoggedAdifEmitted ? QStringLiteral("emitted") : QStringLiteral("not-emitted"))
                  .arg(targets)
                  .arg(wsjtxAdifTargets)
                  .arg(rawAdifTargets)
                  .arg(adifRecord.size()));

    int const totalTargets = targets + rawAdifTargets;
    if (totalTargets > 0) {
        QString addrInfo;
        if (m_udpMessageClient) {
            addrInfo = m_udpMessageClient->server_address().toString()
                       + QStringLiteral(":") + QString::number(m_udpMessageClient->server_port());
        }
        emit statusMessage(QStringLiteral("QSO %1 -> UDP %2 (%3 datagrammi in coda)")
                           .arg(dxCall, addrInfo)
                           .arg(totalTargets));
    } else if (!m_udpNoTargetWarningSent) {
        m_udpNoTargetWarningSent = true;
        QString const udpPort = QString::number(
            udpPortFromSettingValue(getSetting(QStringLiteral("UDPServerPort"), 2237), 2237));
        emit warningRaised(
            QStringLiteral("Logger esterno non raggiunto"),
            QStringLiteral("Il QSO di %1 e' stato salvato localmente ma non inviato ad alcun logger esterno.")
                .arg(dxCall),
            QStringLiteral("Nessun client UDP attivo (porta %1).\n\n"
                           "Verifica in Impostazioni -> Reporting:\n"
                           "- UDP Server: 127.0.0.1\n"
                           "- Porta UDP: %1\n"
                           "- Il logger esterno deve ricevere UDP sulla stessa porta.")
                .arg(udpPort));
    }
}
