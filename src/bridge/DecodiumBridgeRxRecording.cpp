#include "DecodiumBridge.h"
#include <QDir>

bool DecodiumBridge::recordRxEnabled() const
{
    return m_wavManager ? m_wavManager->recording() : m_recordRxEnabled;
}

void DecodiumBridge::setRecordRxEnabled(bool value)
{
    if (m_recordRxEnabled == value && recordRxEnabled() == value) {
        return;
    }

    m_recordRxEnabled = value;

    if (!m_wavManager) {
        emit recordRxEnabledChanged();
        return;
    }

    if (m_wavManager->recording() == value) {
        emit recordRxEnabledChanged();
        return;
    }

    if (value) {
        m_wavManager->startRecording();
        if (m_wavManager->recording()) {
            emit statusMessage(QStringLiteral("Registrazione RX avviata: %1")
                               .arg(QDir::toNativeSeparators(m_wavManager->lastFilePath())));
        } else {
            emit errorMessage(QStringLiteral("Impossibile avviare la registrazione RX"));
            emit recordRxEnabledChanged();
        }
        return;
    }

    QString const savedPath = m_wavManager->lastFilePath();
    m_wavManager->stopRecording();
    emit statusMessage(savedPath.isEmpty()
                           ? QStringLiteral("Registrazione RX fermata")
                           : QStringLiteral("Registrazione RX salvata: %1")
                                 .arg(QDir::toNativeSeparators(savedPath)));
}

