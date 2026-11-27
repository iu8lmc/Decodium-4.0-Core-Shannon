#include "DecodiumBridge.h"
#include <QSettings>
#include <QDateTime>
#include "DecodiumAudioSink.h"

void decodiumBridgeRemoteAudioLog(const QString& message);

int DecodiumBridge::remoteRxVolume() const
{
    return qBound(0, QSettings().value("RemoteAudio/rxVolume", 70).toInt(), 100);
}

QString DecodiumBridge::remoteRxDevice() const
{
    return QSettings().value("RemoteAudio/rxDevice").toString();
}

void DecodiumBridge::setRemoteRxVolume(int volume)
{
    const int bounded = qBound(0, volume, 100);
    m_remoteRxGain = bounded / 100.0;
    QSettings().setValue("RemoteAudio/rxVolume", bounded);
    emit remoteRxAudioChanged();
}

void DecodiumBridge::setRemoteRxDevice(const QString& device)
{
    if (remoteRxDevice() == device) return;
    QSettings().setValue("RemoteAudio/rxDevice", device);
    if (m_decoPortMonitor) {
        setDecoPortMonitor(false);
        setDecoPortMonitor(true);
    }
    emit remoteRxAudioChanged();
}


void DecodiumBridge::onDecoPortRxAudio(const QVector<short>& samples, quint64 captureTsNs)
{
    Q_UNUSED(captureTsNs)
    if (!m_decoPortUseRemote || samples.isEmpty() || !m_audioSink)
        return;
    if (m_transmitting || m_tuning || m_cwRemoteKeying)
        return;

    m_audioSink->injectExternalSamples(samples);
    m_decoPortRemoteSamples += samples.size();

    ssbMonitorAudio(samples);

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastDecoPortAudioLogMs >= 5000) {
        m_lastDecoPortAudioLogMs = nowMs;
        decodiumBridgeRemoteAudioLog(QStringLiteral("DecoPort remote audio: %1 samples fed to the decoder (%2 s)")
                      .arg(m_decoPortRemoteSamples)
                      .arg(static_cast<double>(m_decoPortRemoteSamples) / SAMPLE_RATE, 0, 'f', 1));
    }
}

