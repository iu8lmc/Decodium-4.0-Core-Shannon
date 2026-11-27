#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include "Network/NtpClient.hpp"
#include "Network/DecoSyncTime.hpp"
#include "PrecisionTime.hpp"
#include <QSettings>
#include <QDateTime>
#include <cmath>

extern void decodiumBridgeNtpLog(const QString&);

void DecodiumBridge::applyNtpSettings()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(s);

    // 1.0.159 — DecoSyncTime fase 1: priorita' alla nuova chiave
    // DecoSyncTimeEnabled (default ON). Se l'utente NON ha mai toccato il
    // setting, parte abilitato. Backward compat: se NTPEnabled e' stato
    // toccato esplicitamente in passato (legacy), continua a rispettarlo
    // per non sovrascrivere preferenze utente.
    bool const decoSyncEnabledExplicit = s.contains(QStringLiteral("DecoSyncTimeEnabled"));
    bool const ntpEnabledExplicit      = s.contains(QStringLiteral("NTPEnabled"));
    bool enabled = true;  // default ON
    if (decoSyncEnabledExplicit) {
        enabled = s.value(QStringLiteral("DecoSyncTimeEnabled"), true).toBool();
    } else if (ntpEnabledExplicit) {
        enabled = s.value(QStringLiteral("NTPEnabled"), false).toBool();
    }
    bool const selfCalEnabled = s.value(QStringLiteral("DecoSyncSelfCalEnabled"), false).toBool();
    if (m_decoSyncTime) {
        m_decoSyncTime->setEnabled(enabled);
        m_decoSyncTime->setSelfCalibrationEnabled(enabled && selfCalEnabled);
    }
    QString const customServer =
        s.value(QStringLiteral("NTPCustomServer"), QString()).toString().trimmed();
    double const storedOffset = s.value(QStringLiteral("NTPOffset_ms"), 0.0).toDouble();

    bool const enabledChanged = (m_ntpEnabled != enabled);
    m_ntpEnabled = enabled;
    m_ntpCustomServer = customServer;
    if (enabledChanged) {
        emit ntpEnabledChanged();
    }

    if (!qFuzzyCompare(m_ntpOffsetMs + 1.0, storedOffset + 1.0)) {
        m_ntpOffsetMs = storedOffset;
        emit ntpOffsetMsChanged();
    }

    setGlobalNtpOffsetMs(enabled ? storedOffset : 0.0);

    bool const previousSynced = m_ntpSynced;
    if (!enabled) {
        m_ntpSynced = false;
    }
    if (previousSynced != m_ntpSynced) {
        emit ntpSyncedChanged();
    }

    if (!m_ntpClient) {
        return;
    }

    m_ntpClient->setCustomServer(customServer);
    m_ntpClient->setInitialOffset(storedOffset);
    configureNtpClientForMode(m_mode);
    m_ntpClient->setEnabled(enabled);
    decodiumBridgeNtpLog(QStringLiteral("NTP settings applied: enabled=%1 server=%2 offset=%3 ms")
                  .arg(enabled ? QStringLiteral("1") : QStringLiteral("0"),
                       customServer.isEmpty() ? QStringLiteral("<auto>") : customServer,
                       QString::number(storedOffset, 'f', 1)));
    decodiumBridgeNtpLog(QStringLiteral("DecoSync RF self-calibration: %1")
                  .arg(enabled && selfCalEnabled ? QStringLiteral("enabled") : QStringLiteral("disabled")));
    if (enabled) {
        m_ntpClient->syncNow();
    }
}

void DecodiumBridge::configureNtpClientForMode(const QString& mode)
{
    if (!m_ntpClient) {
        return;
    }

    QString const normalized = mode.trimmed().toUpper();
    int selfCalPeriodMs = 15000;
    if (normalized == QStringLiteral("FT2")) {
        m_ntpClient->setRefreshInterval(60000);
        m_ntpClient->setMaxRtt(55.0);
        selfCalPeriodMs = 2500;   // FT2 async ~2.5s slot
    } else if (normalized == QStringLiteral("FT4")) {
        m_ntpClient->setRefreshInterval(60000);
        m_ntpClient->setMaxRtt(65.0);
        selfCalPeriodMs = 7500;   // FT4 7.5s period
    } else if (normalized == QStringLiteral("FT8")) {
        m_ntpClient->setRefreshInterval(60000);
        m_ntpClient->setMaxRtt(60.0);
        selfCalPeriodMs = 15000;  // FT8 15s period
    } else {
        m_ntpClient->setRefreshInterval(300000);
        m_ntpClient->setMaxRtt(250.0);
        selfCalPeriodMs = 15000;
    }
    // 1.0.162 — DecoSyncTime fase 4: configura il self-cal con il period
    // appropriato per il mode corrente. Il bridge feeda dt via addDecodeDt
    // per ogni decode appended.
    if (m_decoSyncTime) m_decoSyncTime->setSelfCalPeriodMs(selfCalPeriodMs);
}

void DecodiumBridge::syncNtpNow()
{
    if (!m_ntpClient) {
        return;
    }

    if (!m_ntpEnabled) {
        emit statusMessage(QStringLiteral("NTP disabilitato: abilitalo in Impostazioni > Avanzate"));
        decodiumBridgeNtpLog(QStringLiteral("NTP sync requested while disabled"));
        return;
    }

    configureNtpClientForMode(m_mode);
    emit statusMessage(m_ntpCustomServer.isEmpty()
                       ? QStringLiteral("NTP sync in corso sui server pubblici...")
                       : QStringLiteral("NTP sync in corso su %1...").arg(m_ntpCustomServer));
    decodiumBridgeNtpLog(QStringLiteral("NTP sync requested manually: server=%1")
                  .arg(m_ntpCustomServer.isEmpty() ? QStringLiteral("<auto>") : m_ntpCustomServer));
    m_ntpClient->syncNow();
}


#define bridgeLog decodiumBridgeNtpLog
void DecodiumBridge::initialiseTimeSync()
{
    // 1.0.159 — DecoSyncTime fase 1: wrapper centrale time-sync. Possiede
    // un NtpClient interno (per ora unica source). Default ENABLED; il
    // caricamento delle settings successivo puo' sovrascrivere.
    m_decoSyncTime    = new DecoSyncTime(this);
    m_ntpClient       = m_decoSyncTime->legacyNtpClient();
    connect(m_ntpClient, &NtpClient::offsetUpdated, this, [this](double offsetMs) {
        if (qFuzzyCompare(m_ntpOffsetMs + 1.0, offsetMs + 1.0)) {
            return;
        }
        double const previousOffsetMs = m_ntpOffsetMs;
        m_ntpOffsetMs = offsetMs;
        setGlobalNtpOffsetMs(offsetMs);
        emit ntpOffsetMsChanged();
        if (m_ntpEnabled
            && m_ntpSynced
            && m_monitoring
            && isTimeSyncDecodeMode(m_mode)
            && std::abs(offsetMs - previousOffsetMs) >= 50.0) {
            // 1.0.166 — hold-off post-TX: il PTT release causa jitter NTP
            // transitorio. Senza guard, ogni rearm chiama
            // resetRxPeriodAccumulation() che svuota m_audioBuffer e setta
            // m_lastPeriodSlot=-1 → decoder muto fino a stabilizzazione
            // (a volte mai). DecoSyncTime fasi 2-4 hanno aumentato la
            // frequenza degli offsetUpdated emit, peggiorando il problema.
            qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
            bool const recentlyTx = (m_lastTxEndMs > 0)
                                    && (nowMs - m_lastTxEndMs < 5000);
            bool const recentlyRearmed = (m_lastNtpRearmMs > 0)
                                         && (nowMs - m_lastNtpRearmMs < 30000);
            if (recentlyTx || recentlyRearmed) {
                bridgeLog(QStringLiteral("NTP offset update suppressed: %1ms (recentTx=%2 recentRearm=%3)")
                              .arg(QString::number(offsetMs - previousOffsetMs, 'f', 1),
                                   recentlyTx ? "1" : "0",
                                   recentlyRearmed ? "1" : "0"));
            } else {
                m_lastNtpRearmMs = nowMs;
                quint64 const sessionId = ++m_periodTimerSessionId;
                armPeriodTimerForCurrentMode(sessionId, QStringLiteral("NTP offset update"));
            }
        }
    });
    connect(m_ntpClient, &NtpClient::syncStatusChanged, this, [this](bool synced, const QString& statusText) {
        Q_UNUSED(statusText)
        bool const effectiveSynced = m_ntpEnabled && synced;
        bool const becameSynced = !m_ntpSynced && effectiveSynced;
        if (m_ntpSynced != effectiveSynced) {
            m_ntpSynced = effectiveSynced;
            emit ntpSyncedChanged();
        }
        if (becameSynced
            && m_monitoring
            && isTimeSyncDecodeMode(m_mode)
            && std::abs(m_ntpOffsetMs) >= 50.0) {
            quint64 const sessionId = ++m_periodTimerSessionId;
            armPeriodTimerForCurrentMode(sessionId, QStringLiteral("NTP sync"));
        }
        bridgeLog(QStringLiteral("NTP status: enabled=%1 synced=%2 offset=%3 ms")
                      .arg(m_ntpEnabled ? QStringLiteral("1") : QStringLiteral("0"),
                           effectiveSynced ? QStringLiteral("1") : QStringLiteral("0"),
                           QString::number(m_ntpOffsetMs, 'f', 1)));
    });
    connect(m_ntpClient, &NtpClient::errorOccurred, this, [](const QString& msg) {
        bridgeLog(QStringLiteral("NTP error: ") + msg);
    });

}
#undef bridgeLog
