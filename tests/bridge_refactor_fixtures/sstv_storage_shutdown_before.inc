void DecodiumBridge::shutdownSstvStorage()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    const bool wasReady = m_sstvStorageReady;
    m_sstvStorageReady = false;
    const QList<quint64> pendingQsoRequests
        = m_sstvQsoLogRequests.keys();
    for (quint64 requestId : pendingQsoRequests) {
        finishSstvQsoLog(
            requestId, false,
            tr("Decodium shut down before the SSTV QSO operation completed"));
    }
    m_sstvIssuedExistingQsoIds.clear();
    if (m_sstvGalleryModel) {
        m_sstvGalleryModel->setThumbnailProvider(nullptr);
        m_sstvGalleryModel->shutdown();
    }
    if (m_sstvThumbnailProvider) {
        m_sstvThumbnailProvider->shutdown();
        m_sstvThumbnailProvider = nullptr;
    }

    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (worker && m_sstvStorageThread
        && m_sstvStorageThread->isRunning()) {
        QMetaObject::invokeMethod(
            worker.data(), &decodium::sstv::SstvStorageWorker::shutdown,
            Qt::BlockingQueuedConnection);
    }
    if (m_sstvStorageThread) {
        m_sstvStorageThread->quit();
        m_sstvStorageThread->wait();
        delete m_sstvStorageThread;
        m_sstvStorageThread = nullptr;
    }
    m_sstvStorageWorker = nullptr;
    m_sstvRxSaveRequests.clear();
    m_sstvTxGallerySaveRequests.clear();
#if DECODIUM_HAS_HAMDRM
    m_pendingHamDrmGalleryImages.clear();
    m_pendingHamDrmGalleryBytes = 0;
    m_sstvStorageInitializationPending = false;
#endif
    m_sstvIncomingImportHandoffs.clear();
    m_sstvIncomingImportRetryCounts.clear();
    m_sstvRxPendingSaveKeys.clear();
    if (wasReady) {
        emit sstvStorageStateChanged();
    }
#endif
}

