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

