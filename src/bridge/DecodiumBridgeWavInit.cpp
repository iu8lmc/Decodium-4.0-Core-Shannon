#include "DecodiumBridge.h"
#include <QTimer>
#include <QDebug>

void DecodiumBridge::initialiseWavManager()
{
    m_wavManager      = new WavManager(this);
    connect(m_wavManager, &WavManager::recordingChanged, this, [this]() {
        bool const recording = m_wavManager && m_wavManager->recording();
        if (m_recordRxEnabled != recording) {
            m_recordRxEnabled = recording;
        }
        emit recordRxEnabledChanged();
    });
    bool autoRecordOk = false;
    int const autoRecordSeconds = qEnvironmentVariableIntValue("DECODIUM_RECORD_RX_SECONDS",
                                                               &autoRecordOk);
    if (autoRecordOk && autoRecordSeconds > 0) {
        QTimer::singleShot(10000, this, [this, autoRecordSeconds]() {
            setRecordRxEnabled(true);
            qInfo().noquote() << QStringLiteral("[Decodium] diagnostic RX WAV recording started for %1 seconds")
                                     .arg(autoRecordSeconds);
            QTimer::singleShot(autoRecordSeconds * 1000, this, [this]() {
                setRecordRxEnabled(false);
                qInfo().noquote() << QStringLiteral("[Decodium] diagnostic RX WAV recording stopped");
            });
        });
    }
}
