#include "DecodiumBridge.h"
#include "SpecialOperationBounds.h"
#include "Network/MessageClient.hpp"
#include "Radio.hpp"
#include <QDateTime>

bool DecodiumBridge::udpTrafficEnabled(const QString& destinationPrefix,
                                       const QString& category) const
{
    QString const key = destinationPrefix + QStringLiteral("Send") + category;
    if (category == QStringLiteral("Qso")) {
        // Existing installations used the ADIF switch as their only logged-QSO
        // selector. Use it as the migration fallback until the new filter is
        // explicitly saved.
        bool const legacyAdifDefault =
            getSetting(destinationPrefix + QStringLiteral("LoggedAdifEnabled"), true).toBool();
        return getSetting(key, legacyAdifDefault).toBool();
    }
    return getSetting(key, true).toBool();
}

void DecodiumBridge::udpSendDecode(bool isNew, const QString& rawLine, quint64 serial)
{
    if ((!m_udpMessageClient && !m_udpSecondaryMessageClient && !m_udpTertiaryMessageClient) || usingLegacyBackendForTx()) return;

    // Parse the raw decode line: "HHMM  SNR  DT  FREQ ~ MESSAGE"
    QStringList f = parseFt8Row(rawLine);
    if (f.size() < 5) return;

    QString timeStr = f[0].trimmed();
    int snr = f[1].trimmed().toInt();
    float dt = f[2].trimmed().toFloat();
    quint32 df = f.size() > 7 ? f[7].trimmed().toUInt() : f[3].trimmed().toUInt();
    QString message = f[4];
    if (!shouldAcceptDecodedMessage(message)) {
        return;
    }

    // parseFt8Row restituisce timeStr come "" (async), "HHMM" o "HHMMSS".
    // Fino a quando FT2 usa HHMMSS, la branch precedente cadeva su
    // QTime::currentTime() = tempo di PROCESSING, non dello slot → il
    // downstream (GridTracker, JTAlert) riceveva timestamp in ritardo di
    // 1-3s rispetto ai decode provenienti da altri WSJT-X sincroni,
    // da cui il sintomo "UDP lento / tutto fuori tempo" (IW4BFF).
    QTime time;
    if (timeStr.length() == 6) {
        time = QTime(timeStr.left(2).toInt(),
                     timeStr.mid(2, 2).toInt(),
                     timeStr.mid(4, 2).toInt());
    } else if (timeStr.length() == 4) {
        time = QTime(timeStr.left(2).toInt(), timeStr.mid(2, 2).toInt());
    } else {
        // Fallback: usiamo l'UTC corrente (non il local time) perché il
        // resto del protocollo WSJT-X UDP scambia QTime sempre in UTC.
        time = QDateTime::currentDateTimeUtc().time();
    }

    if (m_udpMessageClient
        && udpTrafficEnabled(QStringLiteral("UDPPrimary"), QStringLiteral("Decode"))) {
        m_udpMessageClient->decode(isNew, time, snr, dt, df, m_mode, message, false, false);
    }
    if (m_udpSecondaryMessageClient
        && udpTrafficEnabled(QStringLiteral("UDPSecondary"), QStringLiteral("Decode"))) {
        m_udpSecondaryMessageClient->decode(isNew, time, snr, dt, df, m_mode, message, false, false);
    }
    if (m_udpTertiaryMessageClient
        && udpTrafficEnabled(QStringLiteral("UDPTertiary"), QStringLiteral("Decode"))) {
        m_udpTertiaryMessageClient->decode(isNew, time, snr, dt, df, m_mode, message, false, false);
    }
    Q_UNUSED(serial)
}

void DecodiumBridge::udpSendWsprDecode(bool isNew, const QString& rawLine)
{
    if ((!m_udpMessageClient && !m_udpSecondaryMessageClient && !m_udpTertiaryMessageClient)
        || usingLegacyBackendForTx()) {
        return;
    }

    QStringList parts = rawLine.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() == 7) {
        // A WSPR decode may omit the locator: keep the packet field ordering.
        parts.insert(6, QString());
    }
    if (parts.size() < 8) {
        return;
    }

    auto sendWspr = [&](MessageClient* client) {
        if (!client) return;
        client->WSPR_decode(isNew,
                            QTime::fromString(parts.at(0), QStringLiteral("hhmm")),
                            parts.at(1).toInt(),
                            parts.at(2).toFloat(),
                            Radio::frequency(parts.at(3).toFloat(), 6),
                            parts.at(4).toInt(),
                            parts.at(5),
                            parts.at(6),
                            parts.at(7).toInt(),
                            false);
    };

    if (udpTrafficEnabled(QStringLiteral("UDPPrimary"), QStringLiteral("Wspr"))) {
        sendWspr(m_udpMessageClient);
    }
    if (udpTrafficEnabled(QStringLiteral("UDPSecondary"), QStringLiteral("Wspr"))) {
        sendWspr(m_udpSecondaryMessageClient);
    }
    if (udpTrafficEnabled(QStringLiteral("UDPTertiary"), QStringLiteral("Wspr"))) {
        sendWspr(m_udpTertiaryMessageClient);
    }
}

void DecodiumBridge::udpSendStatus()
{
    if ((!m_udpMessageClient && !m_udpSecondaryMessageClient && !m_udpTertiaryMessageClient) || usingLegacyBackendForTx()) return;

    auto freq = static_cast<MessageClient::Frequency>(qRound64(m_frequency));
    QString const report = m_reportSent.trimmed().isEmpty() ? QStringLiteral("-10")
                                                            : m_reportSent.trimmed().toUpper();
    auto sendStatus = [&](MessageClient* client) {
        if (!client) return;
        client->status_update(
            freq,
            m_mode,                     // mode
            m_dxCall,                   // dx_call
            report,                     // report
            m_mode,                     // tx_mode
            m_txEnabled,                // tx_enabled
            m_transmitting,             // transmitting
            m_decoding,                 // decoding
            static_cast<quint32>(m_rxFrequency),  // rx_df
            static_cast<quint32>(m_txFrequency),  // tx_df
            m_callsign,                 // de_call
            m_grid,                     // de_grid
            m_dxGrid,                   // dx_grid
            false,                      // watchdog_timeout
            QString(),                  // sub_mode
            false,                      // fast_mode
            static_cast<quint8>(qBound(kSpecialOpNone, m_specialOperationActivity, kSpecialOpMax)), // special_op_mode
            0,                          // frequency_tolerance
            static_cast<quint32>(periodMsForMode(m_mode)), // tr_period
            QString(),                  // configuration_name
            m_lastTransmittedMessage,   // lastTxMsg
            static_cast<quint32>(qBound(0, m_qsoProgress, 6)), // qsoProgress
            m_txPeriod != 0,            // txFirst
            false,                      // cqOnly
            QString(),                  // genMsg
            false,                      // txHaltClk
            m_txEnabled,                // txEnableState
            false,                      // txEnableClk
            QString(),                  // myContinent
            false                       // metricUnits
        );
    };

    if (udpTrafficEnabled(QStringLiteral("UDPPrimary"), QStringLiteral("Status"))) {
        sendStatus(m_udpMessageClient);
    }
    if (udpTrafficEnabled(QStringLiteral("UDPSecondary"), QStringLiteral("Status"))) {
        sendStatus(m_udpSecondaryMessageClient);
    }
    if (udpTrafficEnabled(QStringLiteral("UDPTertiary"), QStringLiteral("Status"))) {
        sendStatus(m_udpTertiaryMessageClient);
    }
}
