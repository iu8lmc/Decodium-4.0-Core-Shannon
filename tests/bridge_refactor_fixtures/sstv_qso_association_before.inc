#if DECODIUM_HAS_SSTV
bool DecodiumBridge::queueSstvQsoAssociation(
    quint64 requestId,
    const QString& imageRecordId,
    const QString& qsoId)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (!worker || !worker->isInitialized()
        || !m_sstvQsoLogRequests.contains(requestId)
        || imageRecordId.isEmpty() || qsoId.isEmpty()) {
        return false;
    }
    return worker->enqueueDatabaseOperation(
        [imageRecordId, qsoId, requestId](
            decodium::sstv::SstvStorageWorker& storage) {
            storage.associateWithQso(imageRecordId, qsoId, requestId);
        });
}

void DecodiumBridge::finishSstvQsoLog(
    quint64 requestId,
    bool associationStored,
    const QString& error)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QVariantMap pending = m_sstvQsoLogRequests.take(requestId);
    if (pending.isEmpty()) {
        return;
    }
    const QVariantMap request = pending.value(
        QStringLiteral("request")).toMap();
    const QString imageRecordId = request.value(
        QStringLiteral("imageRecordId")).toString();
    const QString qsoId = pending.value(QStringLiteral("qsoId")).toString();
    const bool qsoCreated = pending.value(
        QStringLiteral("qsoCreated")).toBool();
    QString detail = error.trimmed().left(1'024);
    if (!associationStored && detail.isEmpty()) {
        detail = tr("The SSTV QSO operation failed");
    }
    if (associationStored) {
        emit statusMessage(
            qsoCreated
                ? tr("SSTV QSO logged and image associated locally")
                : tr("SSTV image associated with the selected QSO"));
    } else {
        emit errorMessage(
            qsoCreated
                ? tr("SSTV QSO was logged, but image association failed: %1")
                      .arg(detail)
                : tr("SSTV QSO operation failed: %1").arg(detail));
    }
    emit sstvQsoLogFinished(QString::number(requestId), imageRecordId, qsoCreated,
                            associationStored, qsoId, detail);
}
#endif

