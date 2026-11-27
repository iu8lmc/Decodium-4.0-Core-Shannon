bool DecodiumBridge::saveSstvRxRawAudio()
{
    return saveSstvRxRawAudioImpl(false);
}

bool DecodiumBridge::saveSstvRecentRxAudio()
{
    return saveSstvRxRawAudioImpl(true);
}

bool DecodiumBridge::saveSstvRxRawAudioImpl(bool recentBuffer)
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvStorageReady || !m_sstvStorageWorker
        || !m_sstvRxRuntime || !m_sstvRxAudioJobController
        || m_sstvRxAudioJobController->busy()) {
        return false;
    }
    const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime->snapshot();
    const quint64 retainedId = snapshot.replay.mostRecentAcquisitionId;
    const bool currentImageMatches = snapshot.image.acquisitionId != 0U
        && snapshot.image.acquisitionId <= 0xffff'ffffULL
        && (retainedId >> 32U) == (snapshot.image.generation & 0xffff'ffffULL)
        && (retainedId & 0xffff'ffffULL) == snapshot.image.acquisitionId;
    // A newer image (including Radiofax) may not have a retained descriptor.
    // Never attach an older image's recording to that newer image.
    recentBuffer = recentBuffer || !currentImageMatches;
    const quint64 acquisitionId = recentBuffer ? 0U : retainedId;
    if (snapshot.replay.retainedSamples == 0U) {
        emit errorMessage(tr("No retained SSTV audio is available to save"));
        return false;
    }
    // A failed VIS acquisition is precisely when diagnostic audio is needed.
    // The worker falls back to the bounded recent-audio buffer for ID zero.
    m_sstvRxRawAudioPath.clear();
    m_sstvRxRawAudioAcquisitionId = 0U;
    m_sstvRxRawAudioPendingImageId = acquisitionId != 0U ? snapshot.image.acquisitionId : 0U;
    m_sstvRxRawAudioPendingRetainedId = acquisitionId;
    const decodium::sstv::SstvStorageLayout layout(
        m_sstvStorageWorker->storageRoot());
    const QString token = QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString path = QDir(layout.wavExportRoot()).absoluteFilePath(
        QStringLiteral("rx-%1-%2.wav").arg(token).arg(acquisitionId));
    const bool started = m_sstvRxAudioJobController->exportRawAudio(
        QUrl::fromLocalFile(path), acquisitionId, recentBuffer);
    if (started) {
        emit sstvRxAudioJobChanged();
    }
    return started;
#else
    Q_UNUSED(recentBuffer)
    return false;
#endif
}

void DecodiumBridge::cancelSstvRxAudioJob()
{
#if DECODIUM_HAS_SSTV
    if (m_sstvRxAudioJobController) {
        m_sstvRxAudioJobController->cancel();
    }
#endif
}
