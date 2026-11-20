#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include <QSettings>

void DecodiumBridge::resetStartupTransientQsoState()
{
    // A startup must never reopen a stale QSO from persisted settings.
    m_txEnabled = false;
    m_manualTxHold = false;
    m_qsoProgress = 0;
    m_dxCall.clear();
    m_dxGrid.clear();
    m_tx1.clear();
    m_tx2.clear();
    m_tx3.clear();
    m_tx4.clear();
    m_tx5.clear();
    m_reportSent.clear();
    m_reportReceived = QStringLiteral("-10"); m_reportReceivedReal = false;
    m_txRetryCount = 0;
    m_lastNtx = -1;
    m_nTx73 = 0;
    m_autoSeqRogerReportBase.clear();
    m_lastTransmittedMessage.clear();
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();
    m_qsoCooldown.clear();
    m_worldMapClosedQsoCallKeys.clear();
    clearPendingAutoLogSnapshot();
    clearLateAutoLogSnapshot();
    clearAutoCqPartnerLock();
    regenerateTxMessages();
    m_currentTx = m_tx6.trimmed().isEmpty() ? 1 : 6;
}

void DecodiumBridge::purgePersistentTransientQsoState()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(s);
    s.remove(QStringLiteral("dxCall"));
    s.remove(QStringLiteral("dxGrid"));
    s.remove(QStringLiteral("tx1"));
    s.remove(QStringLiteral("tx2"));
    s.remove(QStringLiteral("tx3"));
    s.remove(QStringLiteral("tx4"));
    s.remove(QStringLiteral("tx5"));
    s.remove(QStringLiteral("reportReceived"));
    s.sync();
}

