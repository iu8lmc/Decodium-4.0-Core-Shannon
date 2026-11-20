#if DECODIUM_HAS_SSTV
void DecodiumBridge::handleSstvStudioPreparedChanged()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_sstvStudioController || !m_sstvStudioController->preparedReady()) {
        return;
    }
    decodium::sstv::advanceSstvTxDraftGeneration(
        m_sstvStudioPreparedGeneration,
        m_sstvStudioDraftQueuedGeneration);
    queueSstvStudioDraftImage();
}

void DecodiumBridge::queueSstvStudioDraftImage()
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    const std::shared_ptr<const QImage> prepared
        = m_sstvStudioController
        ? m_sstvStudioController->preparedSnapshot()
        : std::shared_ptr<const QImage> {};
    if (!m_sstvStorageReady || !worker || !worker->isInitialized()
        || !prepared || prepared->isNull()) {
        return;
    }
    if (!decodium::sstv::sstvTxDraftNeedsArchive(
            m_sstvStudioPreparedGeneration,
            m_sstvStudioDraftQueuedGeneration)) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    decodium::sstv::SstvTxGalleryArchiveContext context;
    context.eventAtUtc = now;
    context.mode = m_sstvStudioController->modeId().left(64);
    context.localCallsign = m_callsign.trimmed().left(64);
    context.localGrid = m_grid.trimmed().left(16);
    context.source = QStringLiteral("sstv-studio");
    context.frequencyHz = std::isfinite(m_frequency)
            && m_frequency > 0.0
            && m_frequency
                <= static_cast<double>(std::numeric_limits<qint64>::max())
        ? static_cast<qint64>(std::llround(m_frequency)) : 0;
    context.qualityMetrics = {
        {QStringLiteral("audioToneLowHz"), 1'200.0},
        {QStringLiteral("audioToneCentreHz"), 1'900.0},
        {QStringLiteral("audioToneHighHz"), 2'300.0},
        {QStringLiteral("txAccepted"), 0.0},
    };
    context.fileNameTemplate = getSetting(
        QStringLiteral("SSTV/ImageNamingTemplate"),
        QStringLiteral("{date}_{time}_{mode}_{remoteCall}_{id}"))
                                   .toString()
                                   .trimmed();
    const auto archive = decodium::sstv::makeSstvTxGalleryArchiveRequest(
        *prepared, decodium::sstv::SstvImageCategory::Draft, context);
    if (!archive.has_value()) {
        emit errorMessage(tr("The prepared SSTV Studio image could not be archived"));
        return;
    }

    if (m_sstvStorageRequestId == 0U) {
        m_sstvStorageRequestId = 1U;
    }
    const quint64 requestId = m_sstvStorageRequestId++;
    decodium::sstv::SstvImageSaveRequest request = *archive;
    if (!worker->enqueueDatabaseOperation(
            [prepared, request = std::move(request), requestId](
                decodium::sstv::SstvStorageWorker& storage) mutable {
                request.image = *prepared;
                storage.storeAndInsertImage(std::move(request), requestId);
            })) {
        emit errorMessage(tr("Could not queue the prepared SSTV Studio image for Gallery storage"));
        return;
    }
    m_sstvStudioDraftQueuedGeneration = m_sstvStudioPreparedGeneration;
    m_sstvTxGallerySaveRequests.insert(requestId, QStringLiteral("draft"));
}

void DecodiumBridge::queueSstvStudioTransmittedImage(
    std::shared_ptr<const QImage> prepared,
    QString fskId,
    quint64 sessionId)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (!m_sstvStorageReady || !worker || !worker->isInitialized()
        || !prepared || prepared->isNull() || !m_sstvStudioController) {
        emit statusMessage(tr(
            "SSTV TX was accepted, but Gallery storage is not ready; "
            "the transmitted image was not archived"));
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    decodium::sstv::SstvTxGalleryArchiveContext context;
    context.eventAtUtc = now;
    context.mode = m_sstvStudioController->modeId().left(64);
    context.fskId = fskId.trimmed().left(128);
    context.localCallsign = m_callsign.trimmed().left(64);
    context.localGrid = m_grid.trimmed().left(16);
    context.source = QStringLiteral("sstv-studio");
    context.frequencyHz = std::isfinite(m_frequency)
            && m_frequency > 0.0
            && m_frequency
                <= static_cast<double>(std::numeric_limits<qint64>::max())
        ? static_cast<qint64>(std::llround(m_frequency)) : 0;
    context.qualityMetrics = {
        {QStringLiteral("audioToneLowHz"), 1'200.0},
        {QStringLiteral("audioToneCentreHz"), 1'900.0},
        {QStringLiteral("audioToneHighHz"), 2'300.0},
        {QStringLiteral("txAccepted"), 1.0},
        {QStringLiteral("txSessionId"),
         static_cast<double>(std::min<quint64>(
             sessionId, static_cast<quint64>(9'007'199'254'740'991ULL)))},
    };
    context.fileNameTemplate = getSetting(
        QStringLiteral("SSTV/ImageNamingTemplate"),
        QStringLiteral("{date}_{time}_{mode}_{remoteCall}_{id}"))
                                   .toString()
                                   .trimmed();
    const auto archive = decodium::sstv::makeSstvTxGalleryArchiveRequest(
        *prepared, decodium::sstv::SstvImageCategory::Transmitted, context);
    if (!archive.has_value()) {
        emit errorMessage(tr("The accepted SSTV TX image could not be archived"));
        return;
    }

    if (m_sstvStorageRequestId == 0U) {
        m_sstvStorageRequestId = 1U;
    }
    const quint64 requestId = m_sstvStorageRequestId++;
    decodium::sstv::SstvImageSaveRequest request = *archive;
    if (!worker->enqueueDatabaseOperation(
            [prepared, request = std::move(request), requestId](
                decodium::sstv::SstvStorageWorker& storage) mutable {
                request.image = *prepared;
                storage.storeAndInsertImage(std::move(request), requestId);
            })) {
        emit errorMessage(tr("Could not queue the accepted SSTV TX image for Gallery storage"));
        return;
    }
    m_sstvTxGallerySaveRequests.insert(requestId,
                                       QStringLiteral("transmitted"));
}


#endif
