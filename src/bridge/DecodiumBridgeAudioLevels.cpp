#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "Audio/soundin.h"
#include "Audio/soundout.h"
#include <QAudioSink>
#include <QtMath>

void decodiumBridgeAudioLevelLog(const QString&);
qreal decodiumBridgeTxAttenuationFromSlider(double);
float decodiumBridgeRxInputGainFromLevel(double);
qreal decodiumBridgeTxGainFromSlider(double);
int decodiumBridgeLegacyTxAttenuationFromLevel(double);

double DecodiumBridge::audioLevel() const { return m_audioLevel; }
double DecodiumBridge::sMeter() const { return m_sMeter; }

void DecodiumBridge::setRxInputLevel(double v)
{
    applyRxInputLevel(v, false);
}

void DecodiumBridge::setAutoRxInputLevel(bool enabled)
{
    if (m_autoRxInputLevel == enabled) {
        return;
    }

    m_autoRxInputLevel = enabled;
    m_autoRxLevelLowStartMs = 0;
    m_autoRxLevelLastAdjustMs = 0;
    m_autoRxLevelManualHoldUntilMs = 0;
    decodiumBridgeAudioLevelLog(QStringLiteral("Auto RX Input Level %1").arg(enabled ? QStringLiteral("enabled")
                                                                  : QStringLiteral("disabled")));
    emit autoRxInputLevelChanged();
}

void DecodiumBridge::applyRxInputLevel(double level, bool automatic)
{
    double const bounded = qBound(0.0, level, 100.0);
    if (qFuzzyCompare(m_rxInputLevel + 1.0, bounded + 1.0)) {
        return;
    }

    if (!automatic && m_autoRxInputLevel) {
        m_autoRxInputLevel = false;
        m_autoRxLevelLowStartMs = 0;
        m_autoRxLevelLastAdjustMs = 0;
        m_autoRxLevelManualHoldUntilMs = 0;
        decodiumBridgeAudioLevelLog(QStringLiteral("Auto RX Input Level disabled by manual RX level change"));
        emit autoRxInputLevelChanged();
    }

    decodiumBridgeAudioLevelLog(QStringLiteral("%1RxInputLevel: requested=%2 previous=%3 legacy=%4")
                  .arg(automatic ? QStringLiteral("auto") : QStringLiteral("set"),
                       QString::number(bounded, 'f', 1),
                       QString::number(m_rxInputLevel, 'f', 1),
                       usingLegacyBackendForTx() ? QStringLiteral("1") : QStringLiteral("0")));
    m_rxInputLevel = bounded;
    if (usingLegacyBackendForTx()) {
        m_legacyBackend->setRxInputLevel(qRound(m_rxInputLevel));
    }
    if (m_soundInput) m_soundInput->setInputGain(decodiumBridgeRxInputGainFromLevel(m_rxInputLevel));
    emit rxInputLevelChanged();
}

void DecodiumBridge::setTxOutputLevel(double v)
{
    double const bounded = qBound(0.0, v, 450.0);
    if (qFuzzyCompare(m_txOutputLevel + 1.0, bounded + 1.0)) {
        return;
    }

    m_txOutputLevel = bounded;

    if (usingLegacyBackendForTx()) {
        m_legacyBackend->setTxOutputAttenuation(decodiumBridgeLegacyTxAttenuationFromLevel(m_txOutputLevel));
    }

    qreal const attenuationDb = decodiumBridgeTxAttenuationFromSlider(m_txOutputLevel);
    if (m_soundOutput) {
        m_soundOutput->setAttenuation(attenuationDb);
    }
    if (m_txAudioSink) {
        m_txAudioSink->setVolume(static_cast<float>(decodiumBridgeTxGainFromSlider(m_txOutputLevel)));
    }

    emit txOutputLevelChanged();
}

