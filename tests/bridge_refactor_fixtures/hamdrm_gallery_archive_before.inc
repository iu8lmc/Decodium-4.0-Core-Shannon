#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
void DecodiumBridge::queueHamDrmTransmittedImage(
    QImage image,
    QString profileId,
    int occupiedBandwidthHz,
    bool received,
    bool hybrid,
    QDateTime eventAtUtc)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (image.isNull() || profileId.trimmed().isEmpty()) return;
    if (received && !m_sstvStorageReady && worker && m_sstvStorageInitializationPending) {
        constexpr qint64 maxPendingBytes = 64LL * 1024LL * 1024LL;
        const qint64 bytes = image.sizeInBytes();
        if (bytes > 0 && bytes <= maxPendingBytes - m_pendingHamDrmGalleryBytes
            && m_pendingHamDrmGalleryImages.size() < 8U) {
            m_pendingHamDrmGalleryBytes += bytes;
            m_pendingHamDrmGalleryImages.push_back({std::move(image), std::move(profileId),
                                                   occupiedBandwidthHz, hybrid,
                                                   eventAtUtc.isValid() ? eventAtUtc : QDateTime::currentDateTimeUtc()});
            if (QStandardPaths::isTestModeEnabled()
                && qEnvironmentVariableIntValue("DECODIUM_LAB_VERIFY_HAMDRM_GALLERY") == 1)
                qInfo() << "[LAB][HAMDRM] Gallery deferred until storage initialization";
            return;
        }
        emit statusMessage(tr("HAMDRM image received, but the pending Gallery queue is full"));
        return;
    }
    if (!m_sstvStorageReady || !worker || !worker->isInitialized()) {
        emit statusMessage(received
            ? tr("HAMDRM image received, but Gallery storage is not ready; the image was not archived")
            : tr("HAMDRM TX was accepted, but Gallery storage is not ready; the transmitted image was not archived"));
        return;
    }

    decodium::sstv::SstvTxGalleryArchiveContext context;
    context.eventAtUtc = eventAtUtc.isValid() ? eventAtUtc : QDateTime::currentDateTimeUtc();
    context.mode = profileId.trimmed().left(64);
    context.localCallsign = m_callsign.trimmed().left(64);
    context.localGrid = m_grid.trimmed().left(16);
    context.source = QStringLiteral("hamdrm");
    context.digital = true;
    context.note = received ? (hybrid ? QStringLiteral("HAMDRM hybrid image received")
                                      : QStringLiteral("HAMDRM image received"))
                            : QStringLiteral("HAMDRM transmission accepted");
    context.frequencyHz = std::isfinite(m_frequency)
            && m_frequency > 0.0
            && m_frequency
                <= static_cast<double>(std::numeric_limits<qint64>::max())
        ? static_cast<qint64>(std::llround(m_frequency)) : 0;
    // The HAMDRM profile identifies occupied bandwidth but not one truthful
    // single-tone audio offset, so retain zero rather than Decodium's unrelated
    // weak-signal TX offset.
    context.audioFrequencyHz = 0;
    context.qualityMetrics = {
        {QStringLiteral("occupiedBandwidthHz"),
         static_cast<double>(std::max(0, occupiedBandwidthHz))},
        {received ? QStringLiteral("rxComplete") : QStringLiteral("txAccepted"), 1.0},
        {QStringLiteral("hybrid"), hybrid ? 1.0 : 0.0},
    };
    context.fileNameTemplate = getSetting(
        QStringLiteral("SSTV/ImageNamingTemplate"),
        QStringLiteral("{date}_{time}_{mode}_{remoteCall}_{id}"))
                                   .toString()
                                   .trimmed();
    const auto archive = decodium::sstv::makeSstvTxGalleryArchiveRequest(
        image, decodium::sstv::SstvImageCategory::Transmitted, context);
    if (!archive.has_value()) {
        emit errorMessage(received ? tr("The received HAMDRM image could not be archived")
                                   : tr("The accepted HAMDRM TX image could not be archived"));
        return;
    }

    if (m_sstvStorageRequestId == 0U) {
        m_sstvStorageRequestId = 1U;
    }
    const quint64 requestId = m_sstvStorageRequestId++;
    const auto archiveImage = std::make_shared<const QImage>(
        std::move(image));
    decodium::sstv::SstvImageSaveRequest request = *archive;
    if (received) request.record.category = decodium::sstv::SstvImageCategory::Received;
    if (!worker->enqueueDatabaseOperation(
            [archiveImage, request = std::move(request), requestId](
                decodium::sstv::SstvStorageWorker& storage) mutable {
                request.image = *archiveImage;
                storage.storeAndInsertImage(std::move(request), requestId);
            })) {
        emit errorMessage(received ? tr("Could not queue the received HAMDRM image for Gallery storage")
                                   : tr("Could not queue the accepted HAMDRM TX image for Gallery storage"));
        return;
    }
    m_sstvTxGallerySaveRequests.insert(requestId,
                                       received ? QStringLiteral("received") : QStringLiteral("transmitted"));
}

#endif
