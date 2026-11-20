#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include <QDateTime>

int decodiumBridgeLegacyTxAttenuationFromLevel(double);
bool decodiumBridgeIsFt2LinkApplicationMode(QString);
bool decodiumBridgeIsStreamingKeyboardMode(QString const&);
void decodiumBridgeAudioLevelLog(const QString&);
int decodiumBridgeSpecialOpHound();

void DecodiumBridge::syncLegacyBackendDialogState()
{
    if (!legacyBackendAvailable()) {
        return;
    }

    // The QML profile is authoritative.  This must precede setMode(): legacy
    // mode handlers regenerate the CQ message immediately.
    m_legacyBackend->setStationIdentity(m_callsign, m_grid);
    if (!m_audioInputDevice.isEmpty()) {
        m_legacyBackend->setAudioInputDeviceName(m_audioInputDevice);
    }
    if (!m_audioOutputDevice.isEmpty()) {
        m_legacyBackend->setAudioOutputDeviceName(m_audioOutputDevice);
    }
    m_legacyBackend->setAudioInputChannel(qBound(0, m_audioInputChannel, 3));
    m_legacyBackend->setAudioOutputChannel(qBound(0, m_audioOutputChannel, 3));
    m_legacyBackend->setDecodeDepthBits(legacyCompatibleDecodeDepthBits());
    m_legacyBackend->setCqOnly(false);
    m_legacyBackend->setRxInputLevel(qRound(qBound(0.0, m_rxInputLevel, 100.0)));
    int const legacyTxAttn = decodiumBridgeLegacyTxAttenuationFromLevel(m_txOutputLevel);
    m_legacyBackend->setTxOutputAttenuation(legacyTxAttn);
    bool const bridgeOwnedExplicitQ65 = m_mode.trimmed().toUpper().startsWith(QStringLiteral("Q65-"));
    QString const legacyDialogMode = decodiumBridgeIsFt2LinkApplicationMode(m_mode)
        ? QStringLiteral("FT2")
        : m_mode;
    m_legacyBackend->setFt2DecodeEnabled(m_mode == QStringLiteral("FT2"));
    if (decodiumBridgeIsStreamingKeyboardMode(m_mode) || bridgeOwnedExplicitQ65) {
        // Do not hand RTTY to the hidden legacy decoder: it has no RTTY
        // implementation and would retain its previous decoder mode.  Q65-*
        // is similarly native because the embedded backend has no period/A-E
        // submode contract. CAT parameters below are still mirrored for the
        // legacy CAT fallback.
        if (m_legacyBackend->monitoring()) {
            m_legacyBackend->setMonitoring(false);
        }
        m_legacyStartupModeGuard.clear();
        m_legacyStartupModeGuardUntilMs = 0;
        if (bridgeOwnedExplicitQ65) {
            decodiumBridgeAudioLevelLog(QStringLiteral("legacy dialog sync: Q65 variant remains bridge-owned; legacy decoder quiesced"));
        }
    } else {
        m_legacyBackend->setMode(legacyDialogMode);
        m_legacyStartupModeGuard = legacyDialogMode.trimmed();
        m_legacyStartupModeGuardUntilMs = QDateTime::currentMSecsSinceEpoch() + 6000;
    }
    m_legacyBackend->setDialFrequency(m_frequency);
    bool const houndTxSlotLock = m_specialOperationActivity == decodiumBridgeSpecialOpHound();
    m_legacyBackend->setAutoSeq(m_autoSeq);
    m_legacyBackend->setAlt12Enabled(houndTxSlotLock ? false : m_alt12Enabled);
    m_legacyBackend->setTxFirst(houndTxSlotLock ? false : (m_txPeriod != 0));
    m_legacyBackend->setRxFrequency(m_rxFrequency);
    m_legacyBackend->setTxFrequency(m_txFrequency);
    m_legacyBackend->setHoldTxFreq(m_holdTxFreq);
    m_legacyBackend->setDxCall(m_dxCall);
    m_legacyBackend->setDxGrid(m_dxGrid);
    m_legacyBackend->setTxMessage(1, m_tx1);
    m_legacyBackend->setTxMessage(2, m_tx2);
    m_legacyBackend->setTxMessage(3, m_tx3);
    m_legacyBackend->setTxMessage(4, m_tx4);
    m_legacyBackend->setTxMessage(5, m_tx5);
    m_legacyBackend->setTxMessage(6, m_tx6);
    m_legacyBackend->selectTxMessage(qBound(1, m_currentTx, 6));
    int const legacyTxWatchdogMinutes = (m_txWatchdogMode == 1 && m_txWatchdogTime > 0)
        ? m_txWatchdogTime
        : 0;
    m_legacyBackend->setTxWatchdogMinutes(legacyTxWatchdogMinutes);
    syncSpecialOperationToLegacyBackend();
    decodiumBridgeAudioLevelLog(QStringLiteral("legacyTxSync: call=%1 grid=%2 mode=%3 outDev=%4 outChan=%5 inDev=%6 inChan=%7 tx=%8 rx=%9 currentTx=%10 txPeriod=%11 alt12=%12 txLevel=%13 legacyTxAttn=%14 txWatchdogMin=%15")
                  .arg(m_callsign,
                       m_grid,
                       m_mode,
                       m_audioOutputDevice,
                       QString::number(m_audioOutputChannel),
                       m_audioInputDevice,
                       QString::number(m_audioInputChannel),
                       QString::number(m_txFrequency),
                       QString::number(m_rxFrequency),
                       QString::number(m_currentTx),
                       houndTxSlotLock ? QStringLiteral("0") : QString::number(m_txPeriod),
                       (!houndTxSlotLock && m_alt12Enabled) ? QStringLiteral("1") : QStringLiteral("0"),
                       QString::number(qRound(qBound(0.0, m_txOutputLevel, 450.0))),
                       QString::number(legacyTxAttn),
                       QString::number(legacyTxWatchdogMinutes)));
}

