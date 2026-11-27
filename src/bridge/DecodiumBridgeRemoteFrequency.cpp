#include "DecodiumBridge.h"

void decodiumBridgeRemoteFrequencyLog(const QString& message);

void DecodiumBridge::applyRemoteDialFrequency(double hz, const QString& reason)
{
    if (hz <= 0.0) {
        return;
    }

    requestRigFrequencyFromBridge(hz, reason);
    if (m_monitoring) {
        resetRxPeriodAccumulation(true);
    }
}

void DecodiumBridge::applyRemoteBandChange(const QString& band)
{
    if (!m_bandManager) {
        return;
    }

    QString normalized = band.trimmed().toUpper();
    if (normalized.isEmpty()) {
        return;
    }

    QString const beforeBand = m_bandManager->currentBandLambda();
    double const beforeFrequency = m_frequency;
    // 1.0.374: il cambio banda deve risolvere la freq col modo applicativo corrente
    // (changeBandByLambda usa m_currentMode, va tenuto allineato a m_mode).
    m_bandManager->setCurrentMode(m_mode);
    m_bandManager->changeBandByLambda(normalized);
    QString const afterBand = m_bandManager->currentBandLambda();
    if (afterBand.compare(beforeBand, Qt::CaseInsensitive) == 0
        && normalized.compare(beforeBand, Qt::CaseInsensitive) != 0
        && qFuzzyCompare(beforeFrequency + 1.0, m_frequency + 1.0)) {
        decodiumBridgeRemoteFrequencyLog(QStringLiteral("Remote band change ignored: unknown band %1").arg(normalized));
        return;
    }

    decodiumBridgeRemoteFrequencyLog(QStringLiteral("Remote band change applied: %1 -> %2 Hz")
                  .arg(afterBand, QString::number(m_frequency, 'f', 0)));
}

