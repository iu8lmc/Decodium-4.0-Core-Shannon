// SPDX-License-Identifier: GPL-3.0-or-later

#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"

#if DECODIUM_HAS_SSTV
#include "Audio/soundout.h"
#include "Modulator/Modulator.hpp"
#include "DecodiumAudioSink.h"
#include "DecoPortLink.h"
#include "DecolinkLink.h"
#include "src/rtl/RtlSdrInput.h"
#include "src/sstv/core/SstvModeRegistry.h"
#include "src/sstv/diagnostics/SstvDiagnosticLogging.h"
#include "src/sstv/diagnostics/SstvDiagnosticsController.h"
#include "src/sstv/integration/SstvRxAudioJobController.h"
#include "src/sstv/integration/SstvRxRuntime.h"
#include "src/sstv/integration/SstvQsoLog.h"
#include "src/sstv/integration/SstvStudioController.h"
#include "src/sstv/integration/SstvTxCoordinator.h"
#include "src/sstv/integration/SstvTxSources.h"
#include "src/sstv/integration/SstvWavReplayController.h"
#include "src/sstv/image/SstvImageFrame.h"
#include "src/sstv/models/SstvGalleryModel.h"
#include "src/sstv/models/SstvShareController.h"
#include "src/sstv/models/SstvThumbnailProvider.h"
#include "src/sstv/storage/SstvImageStorage.h"
#include "src/sstv/storage/SstvStorageWorker.h"
#include "src/sstv/storage/SstvTxGalleryArchive.h"
#include "SecureSettings.hpp"
#include "DecodiumProfileSettings.h"
#if DECODIUM_HAS_HAMDRM
#include "src/sstv/digital/HamDrmController.h"
#include "src/sstv/digital/waveform/HamDrmWaveformAdapters.h"
#endif
#endif

#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QMetaObject>
#include <QMutexLocker>
#include <QSettings>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

#if DECODIUM_HAS_SSTV
bool decodiumSstvQsoRequestFromMap(const QVariantMap&, decodium::sstv::SstvQsoLogRequest*, QString*);
#endif

void DecodiumBridge::initialiseSstvStorage()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_sstvGalleryModel) {
        return;
    }

    m_sstvGalleryModel = new decodium::sstv::SstvGalleryModel(this);
    m_sstvGalleryModel->setObjectName(QStringLiteral("sstvGallery"));

    const QString appData = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (appData.isEmpty()) {
        const QString error = QStringLiteral(
            "Qt did not provide an application data directory for SSTV storage");
        m_sstvGalleryModel->setUnavailableError(error);
        setSstvRxSaveStatus(QStringLiteral("unavailable"), error);
        emit sstvStorageStateChanged();
        return;
    }
    const decodium::sstv::SstvStorageLayout layout(
        QDir(appData).absoluteFilePath(QStringLiteral("sstv")));
    if (layout.rootPath().isEmpty() || layout.databasePath().isEmpty()) {
        const QString error = QStringLiteral(
            "could not resolve the SSTV storage layout");
        m_sstvGalleryModel->setUnavailableError(error);
        setSstvRxSaveStatus(QStringLiteral("unavailable"), error);
        emit sstvStorageStateChanged();
        return;
    }

    m_sstvStorageThread = new QThread(this);
    m_sstvStorageThread->setObjectName(QStringLiteral("SSTV storage worker"));
    auto* worker = new decodium::sstv::SstvStorageWorker(
        layout.databasePath(), layout.rootPath());
    m_sstvStorageWorker = worker;
    worker->moveToThread(m_sstvStorageThread);
    if (m_sstvShareController) {
        connect(m_sstvShareController,
                &decodium::sstv::SstvShareController::incomingHandoffReady,
                this,
                [this, worker](QVariantMap handoff) {
                    if (m_sstvStorageWorker != worker) {
                        return;
                    }
                    const QString transferId = handoff.value(
                        QStringLiteral("transferId")).toString();
                    if (transferId.isEmpty()) {
                        emit errorMessage(tr(
                            "Incoming SSTV Gallery import rejected: "
                            "validated handoff has no transfer identifier"));
                        return;
                    }
                    m_sstvIncomingImportHandoffs.insert(
                        transferId, std::move(handoff));
                    m_sstvIncomingImportRetryCounts.insert(transferId, 0);
                    if (!queueSstvIncomingImport(transferId)) {
                        m_sstvIncomingImportHandoffs.remove(transferId);
                        m_sstvIncomingImportRetryCounts.remove(transferId);
                        emit errorMessage(tr(
                            "Incoming SSTV Gallery import could not be queued; "
                            "the validated staging file was retained"));
                    }
                },
                Qt::QueuedConnection);
    }
    connect(m_sstvStorageThread, &QThread::finished,
            worker, &QObject::deleteLater);
    connect(worker, &decodium::sstv::SstvStorageWorker::initialized,
            this,
            [this, worker](bool ok, const QString& error, int, quintptr) {
                if (m_sstvStorageWorker != worker || !m_sstvGalleryModel) {
                    return;
                }
                const bool readyChanged = m_sstvStorageReady != ok;
                m_sstvStorageReady = ok;
#if DECODIUM_HAS_HAMDRM
                m_sstvStorageInitializationPending = false;
                auto pendingHamDrm = std::move(m_pendingHamDrmGalleryImages);
                m_pendingHamDrmGalleryImages.clear();
                m_pendingHamDrmGalleryBytes = 0;
#endif
                if (ok) {
                    m_sstvGalleryModel->setStorageWorker(worker);
                    // The QSO dialog must never cold-parse the active ADIF
                    // logbook on the GUI thread. Start Decodium's existing
                    // asynchronous native cache as soon as Gallery is ready.
                    warmLogCacheAsync();
                    if (m_sstvStudioController) {
                        const decodium::sstv::SstvStorageLayout readyLayout(
                            worker->storageRoot());
                        m_sstvStudioController->setWavExportRoot(
                            readyLayout.wavExportRoot());
                        // A Studio image may have been prepared while the
                        // asynchronous worker was opening its database.
                        // Archive that immutable revision now instead of
                        // requiring the operator to prepare it again.
                        queueSstvStudioDraftImage();
                    }
                    if (m_sstvShareController) {
                        m_sstvShareController->setStorageRoot(
                            worker->storageRoot(), m_callsign);
                    }
                    if (m_sstvRxSaveRequests.isEmpty()) {
                        setSstvRxSaveStatus(QStringLiteral("idle"));
                    }
                    maybeAutoSaveSstvRxImage();
#if DECODIUM_HAS_HAMDRM
                    for (auto& pending : pendingHamDrm)
                        queueHamDrmTransmittedImage(std::move(pending.image), std::move(pending.profileId),
                                                    pending.occupiedBandwidthHz, true, pending.hybrid, pending.eventAtUtc);
#endif
                } else {
                    m_sstvGalleryModel->setUnavailableError(
                        QStringLiteral("SSTV gallery storage: %1").arg(error));
                    setSstvRxSaveStatus(QStringLiteral("unavailable"), error);
#if DECODIUM_HAS_HAMDRM
                    if (!pendingHamDrm.empty())
                        emit statusMessage(tr("Received HAMDRM images could not be archived because Gallery storage failed to start"));
                    if (!pendingHamDrm.empty() && QStandardPaths::isTestModeEnabled()
                        && qEnvironmentVariableIntValue("DECODIUM_LAB_VERIFY_HAMDRM_GALLERY") == 1)
                        qInfo() << "[LAB][HAMDRM] deferred Gallery storage failed";
#endif
                }
                if (readyChanged) {
                    emit sstvStorageStateChanged();
                }
            }, Qt::QueuedConnection);
    connect(worker, &decodium::sstv::SstvStorageWorker::imageStoreFinished,
            this,
            [this, worker](quint64 requestId, bool ok,
                           const decodium::sstv::SstvImageRecord& record,
                           const QString& error) {
                if (m_sstvStorageWorker != worker) {
                    return;
                }
                const auto archive = m_sstvTxGallerySaveRequests.find(
                    requestId);
                if (archive != m_sstvTxGallerySaveRequests.end()) {
                    const QString category = archive.value();
                    m_sstvTxGallerySaveRequests.erase(archive);
                    if (QStandardPaths::isTestModeEnabled()
                        && qEnvironmentVariableIntValue("DECODIUM_LAB_VERIFY_HAMDRM_GALLERY") == 1
                        && category == QLatin1String("received")) {
                        const QVariantMap detail {
                            {QStringLiteral("ok"), ok},
                            {QStringLiteral("id"), record.id},
                            {QStringLiteral("imagePath"), record.imagePath},
                            {QStringLiteral("storageRoot"), worker->storageRoot()},
                        };
                        qInfo().noquote() << "[LAB][HAMDRM] received Gallery result"
                                          << QJsonDocument::fromVariant(detail).toJson(QJsonDocument::Compact);
                    }
                    if (ok) {
                        emit statusMessage(
                            category == QLatin1String("received")
                                ? tr("Received HAMDRM image saved in Gallery: %1").arg(record.mode)
                                : category == QLatin1String("draft")
                                ? tr("Prepared SSTV Studio image saved in Gallery: %1")
                                      .arg(record.mode)
                                : tr("Transmitted SSTV image saved in Gallery: %1")
                                      .arg(record.mode));
                    } else {
                        emit errorMessage(
                            category == QLatin1String("received")
                                ? tr("Received HAMDRM image save failed: %1")
                                      .arg(error.isEmpty() ? tr("The SSTV image could not be saved") : error)
                                : category == QLatin1String("draft")
                                ? tr("Prepared SSTV Studio image save failed: %1")
                                      .arg(error.isEmpty()
                                           ? tr("The SSTV image could not be saved")
                                           : error)
                                : tr("Transmitted SSTV image save failed: %1")
                                      .arg(error.isEmpty()
                                           ? tr("The SSTV image could not be saved")
                                           : error));
                    }
                    return;
                }
                if (!m_sstvRxSaveRequests.contains(requestId)) {
                    return;
                }
                const QString key = m_sstvRxSaveRequests.take(requestId);
                m_sstvRxPendingSaveKeys.remove(key);
                if (ok) {
                    if (!m_sstvRxSavedKeys.contains(key)) {
                        m_sstvRxSavedKeys.insert(key);
                        m_sstvRxSavedKeyOrder.enqueue(key);
                        constexpr qsizetype kMaximumRememberedSaves = 256;
                        while (m_sstvRxSavedKeyOrder.size()
                               > kMaximumRememberedSaves) {
                            m_sstvRxSavedKeys.remove(
                                m_sstvRxSavedKeyOrder.dequeue());
                        }
                    }
                    setSstvRxSaveStatus(
                        m_sstvRxSaveRequests.isEmpty()
                            ? QStringLiteral("saved")
                            : QStringLiteral("saving"));
                    emit statusMessage(tr("SSTV image saved in Gallery: %1")
                                           .arg(record.mode));
                } else {
                    setSstvRxSaveStatus(
                        QStringLiteral("error"),
                        error.isEmpty()
                            ? tr("The SSTV image could not be saved") : error);
                    emit errorMessage(tr("SSTV image save failed: %1")
                                          .arg(m_sstvRxSaveError));
                }
            }, Qt::QueuedConnection);
    connect(worker, &decodium::sstv::SstvStorageWorker::recordFetched,
            this,
            [this, worker](quint64 requestId, bool found,
                           const decodium::sstv::SstvImageRecord& record,
                           const QString& error) {
                if (m_sstvStorageWorker != worker) {
                    return;
                }
                auto pending = m_sstvQsoLogRequests.find(requestId);
                if (pending == m_sstvQsoLogRequests.end()
                    || pending->value(QStringLiteral("phase")).toString()
                           != QStringLiteral("fetch")) {
                    return;
                }
                if (!found) {
                    finishSstvQsoLog(
                        requestId, false,
                        error.isEmpty()
                            ? tr("The selected SSTV Gallery image no longer exists")
                            : error);
                    return;
                }

                decodium::sstv::SstvQsoLogRequest request;
                QString validationError;
                if (!decodiumSstvQsoRequestFromMap(
                        pending->value(QStringLiteral("request")).toMap(),
                        &request, &validationError)) {
                    finishSstvQsoLog(requestId, false, validationError);
                    return;
                }
                if (record.id != request.imageRecordId) {
                    finishSstvQsoLog(
                        requestId, false,
                        tr("The Gallery returned a different SSTV image record"));
                    return;
                }
                if (record.mode.trimmed().compare(
                        request.imageMode.trimmed(),
                        Qt::CaseInsensitive) != 0) {
                    finishSstvQsoLog(
                        requestId, false,
                        tr("The SSTV image mode changed before the QSO was logged"));
                    return;
                }

                QString qsoId = request.existingQsoId.trimmed();
                bool qsoCreated = false;
                if (request.createNewQso) {
                    QString commitError;
                    if (!commitSstvNewQso(
                            request, &qsoId, &commitError)) {
                        finishSstvQsoLog(requestId, false, commitError);
                        return;
                    }
                    qsoCreated = true;
                } else if (!isSstvExistingQsoChoiceCurrent(qsoId)) {
                    m_sstvIssuedExistingQsoIds.clear();
                    invalidateQsoSearchCache();
                    warmLogCacheAsync();
                    finishSstvQsoLog(
                        requestId, false,
                        tr("Select an existing QSO from the current Decodium logbook list"));
                    return;
                }

                pending = m_sstvQsoLogRequests.find(requestId);
                if (pending == m_sstvQsoLogRequests.end()) {
                    return;
                }
                pending->insert(QStringLiteral("phase"),
                                QStringLiteral("associate"));
                pending->insert(QStringLiteral("qsoId"), qsoId);
                pending->insert(QStringLiteral("qsoCreated"), qsoCreated);
                if (!queueSstvQsoAssociation(
                        requestId, request.imageRecordId, qsoId)) {
                    finishSstvQsoLog(
                        requestId, false,
                        qsoCreated
                            ? tr("The SSTV QSO was logged, but its local image association could not be queued; associate the image with the existing QSO to retry")
                            : tr("The SSTV image association could not be queued"));
                }
            }, Qt::QueuedConnection);
    connect(worker, &decodium::sstv::SstvStorageWorker::operationFinished,
            this,
            [this, worker](quint64 requestId,
                           decodium::sstv::SstvStorageOperation operation,
                           bool ok, const QString& error) {
                if (m_sstvStorageWorker != worker
                    || operation
                           != decodium::sstv::SstvStorageOperation::AssociateQso) {
                    return;
                }
                const auto pending = m_sstvQsoLogRequests.constFind(requestId);
                if (pending == m_sstvQsoLogRequests.cend()
                    || pending->value(QStringLiteral("phase")).toString()
                           != QStringLiteral("associate")) {
                    return;
                }
                finishSstvQsoLog(
                    requestId, ok,
                    ok ? QString {}
                       : (error.isEmpty()
                              ? tr("The local SSTV image association was not stored")
                              : error));
            }, Qt::QueuedConnection);
    connect(worker,
            &decodium::sstv::SstvStorageWorker::incomingImportFinished,
            this,
            [this, worker](
                const decodium::sstv::SstvIncomingImportResult& result) {
                if (m_sstvStorageWorker != worker) {
                    return;
                }
                if (result.ok) {
                    m_sstvIncomingImportHandoffs.remove(result.transferId);
                    m_sstvIncomingImportRetryCounts.remove(result.transferId);
                    emit statusMessage(
                        result.idempotent
                            ? tr("Received SSTV image is already in Gallery: %1")
                                  .arg(result.record.mode)
                            : tr("Received SSTV image imported into Gallery: %1")
                                  .arg(result.record.mode));
                    return;
                }
                const QString detail = result.error.isEmpty()
                    ? tr("The validated incoming SSTV image could not be imported")
                    : result.error;
                constexpr int kMaximumAutomaticRetries = 2;
                const auto handoff = m_sstvIncomingImportHandoffs.constFind(
                    result.transferId);
                const int completedRetries
                    = m_sstvIncomingImportRetryCounts.value(
                        result.transferId, 0);
                if (result.retryable
                    && handoff != m_sstvIncomingImportHandoffs.cend()
                    && completedRetries < kMaximumAutomaticRetries) {
                    const int retryNumber = completedRetries + 1;
                    m_sstvIncomingImportRetryCounts.insert(
                        result.transferId, retryNumber);
                    const int delayMs = retryNumber == 1 ? 250 : 1'000;
                    const QString transferId = result.transferId;
                    QTimer::singleShot(
                        delayMs, this,
                        [this, transferId, retryNumber]() {
                            if (m_sstvIncomingImportRetryCounts.value(
                                    transferId, -1) != retryNumber) {
                                return;
                            }
                            if (!queueSstvIncomingImport(transferId)) {
                                m_sstvIncomingImportHandoffs.remove(transferId);
                                m_sstvIncomingImportRetryCounts.remove(
                                    transferId);
                                emit errorMessage(tr(
                                    "Incoming SSTV Gallery retry could not be "
                                    "queued; use Import again or restart "
                                    "Decodium"));
                            }
                        });
                    emit statusMessage(tr(
                        "Incoming SSTV Gallery import retry %1 of %2 scheduled: %3")
                                           .arg(retryNumber)
                                           .arg(kMaximumAutomaticRetries)
                                           .arg(detail));
                    return;
                }
                m_sstvIncomingImportHandoffs.remove(result.transferId);
                m_sstvIncomingImportRetryCounts.remove(result.transferId);
                emit errorMessage(
                    result.retryable
                        ? tr("Incoming SSTV Gallery import still needs retry; "
                             "use Import again or restart Decodium: %1")
                              .arg(detail)
                        : tr("Incoming SSTV Gallery import rejected: %1")
                              .arg(detail));
            }, Qt::QueuedConnection);
    connect(worker,
            &decodium::sstv::SstvStorageWorker::threadOwnershipViolation,
            this, [this](const QString& operation) {
                emit errorMessage(tr(
                    "SSTV storage thread ownership violation: %1")
                                      .arg(operation));
            }, Qt::QueuedConnection);
    m_sstvStorageThread->start();
#if DECODIUM_HAS_HAMDRM
    m_sstvStorageInitializationPending = true;
#endif
    const int labStorageDelay = QStandardPaths::isTestModeEnabled()
        ? qBound(0, qEnvironmentVariableIntValue("DECODIUM_LAB_SSTV_STORAGE_DELAY_MS"), 30000) : 0;
    bool queued = true;
    if (labStorageDelay > 0) {
        QTimer::singleShot(labStorageDelay, worker, [worker] { worker->initialize(); });
    } else {
        queued = QMetaObject::invokeMethod(
            worker, &decodium::sstv::SstvStorageWorker::initialize,
            Qt::QueuedConnection);
    }
    if (!queued) {
#if DECODIUM_HAS_HAMDRM
        m_sstvStorageInitializationPending = false;
#endif
        const QString error = QStringLiteral(
            "could not queue SSTV storage initialization");
        m_sstvGalleryModel->setUnavailableError(error);
        setSstvRxSaveStatus(QStringLiteral("unavailable"), error);
        emit sstvStorageStateChanged();
    }
#endif
}


