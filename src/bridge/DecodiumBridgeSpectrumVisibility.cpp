#include "DecodiumBridge.h"
#include "Network/RemoteCommandServer.hpp"
#include <QDateTime>
#include <QTimer>

void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::setSpectrumVisible(bool v)
{
    if (m_spectrumVisible == v) {
        return;
    }

    m_spectrumVisible = v;
    emit spectrumVisibleChanged();

    bool const remoteWaterfallNeeded =
        m_remoteServer
        && m_remoteServer->isRunning()
        && m_remoteServer->waterfallEnabled();

    if (!m_spectrumTimer) {
        return;
    }
    if (!m_spectrumVisible && !remoteWaterfallNeeded) {
        m_spectrumTimer->stop();
        m_lastWaterfallAudioBufferSize = 0;
        m_lastPanadapterFrameMs = 0;
        m_lastPanadapterData.clear();
        return;
    }
    if (m_spectrumVisible
        && m_monitoring
        && !m_transmitting
        && !m_tuning
        && !m_spectrumTimer->isActive()) {
        m_spectrumTimer->start();
    }
}

void DecodiumBridge::rearmSpectrumAfterQsoLog()
{
    // A QSO log can close a native confirmation window and change the TX
    // panel geometry. Re-arm only the producer's scheduling state here: the
    // audio capture, decoder and CAT session must remain untouched.
    if (!m_monitoring || m_transmitting || m_tuning || !m_spectrumVisible) {
        decodiumBridgeAudioLevelLog(QStringLiteral("[PANDBG] QSO log visual rearm skipped monitoring=%1 tx=%2 tune=%3 visible=%4")
                      .arg(m_monitoring ? 1 : 0)
                      .arg(m_transmitting ? 1 : 0)
                      .arg(m_tuning ? 1 : 0)
                      .arg(m_spectrumVisible ? 1 : 0));
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    // Let the next timer tick produce a fresh frame even if the previous
    // frame timestamp survived the QSO dialog/layout transition.
    m_lastPanadapterFrameMs = nowMs - 1000;
    if (m_spectrumTimer && !m_spectrumTimer->isActive()) {
        m_spectrumTimer->start();
    }
    decodiumBridgeAudioLevelLog(QStringLiteral("[PANDBG] QSO log visual rearm monitoring=1 timer_active=%1 interval_ms=%2")
                  .arg(m_spectrumTimer && m_spectrumTimer->isActive() ? 1 : 0)
                  .arg(m_spectrumTimer ? m_spectrumTimer->interval() : -1));
}

