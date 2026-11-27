#include "DecodiumBridge.h"

#if DECODIUM_HAS_SSTV
#include "src/sstv/storage/SstvStorageWorker.h"
#include <QThread>
#include <utility>
#endif

#if DECODIUM_HAS_SSTV
quint64 DecodiumBridge::enqueueSstvStoredRecord(
    decodium::sstv::SstvImageRecord record)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (!worker || !worker->isInitialized()) {
        return 0;
    }
    if (m_sstvStorageRequestId == 0) {
        m_sstvStorageRequestId = 1;
    }
    const quint64 requestId = m_sstvStorageRequestId++;
    if (!worker->enqueueDatabaseOperation(
            [record = std::move(record), requestId](
                decodium::sstv::SstvStorageWorker& storage) mutable {
                storage.insertRecord(std::move(record), requestId);
            })) {
        return 0;
    }
    return requestId;
}

bool DecodiumBridge::queueSstvIncomingImport(const QString& transferId)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto handoff = m_sstvIncomingImportHandoffs.constFind(transferId);
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (handoff == m_sstvIncomingImportHandoffs.cend() || !worker) {
        return false;
    }
    const QVariantMap immutableHandoff = *handoff;
    return worker->enqueueDatabaseOperation(
        [immutableHandoff](decodium::sstv::SstvStorageWorker& storage) {
            storage.importValidatedIncomingHandoff(immutableHandoff);
        });
}


#endif

