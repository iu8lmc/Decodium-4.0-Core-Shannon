// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvRxRuntime.h"
#include "src/sstv/image/SstvImageFrame.h"
#include "src/sstv/storage/SstvStorageWorker.h"
#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QThread>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <limits>
using decodium::sstv::SstvRxRuntime;
QString decodiumSstvSourceName(decodium::sstv::SstvAudioSourceKind);
QImage decodiumSstvSnapshotToImage(const decodium::sstv::SstvImageSnapshot&);
#define sstvSourceName decodiumSstvSourceName
#define sstvSnapshotToImage decodiumSstvSnapshotToImage
#endif

#if DECODIUM_HAS_SSTV
void DecodiumBridge::setSstvRxSaveStatus(QString state, QString error)
{
    state = state.trimmed().toLower();
    error = error.trimmed().left(512);
    if (state.isEmpty()) {
        state = QStringLiteral("idle");
    }
    if (m_sstvRxSaveState == state && m_sstvRxSaveError == error) {
        return;
    }
    m_sstvRxSaveState = std::move(state);
    m_sstvRxSaveError = std::move(error);
    emit sstvRxSaveStateChanged();
}

bool DecodiumBridge::queueSstvRxImageSave(bool automatic)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QPointer<decodium::sstv::SstvStorageWorker> worker =
        m_sstvStorageWorker;
    if (!m_sstvStorageReady || !worker || !worker->isInitialized()) {
        if (!automatic) {
            setSstvRxSaveStatus(
                QStringLiteral("error"),
                tr("SSTV storage is not ready"));
        }
        return false;
    }
    if (!m_sstvRxRuntime) {
        if (!automatic) {
            setSstvRxSaveStatus(
                QStringLiteral("error"), tr("SSTV RX is unavailable"));
        }
        return false;
    }

    const SstvRxRuntime::Snapshot before = m_sstvRxRuntime->snapshot();
    const std::shared_ptr<const decodium::sstv::SstvImageSnapshot> image =
        m_sstvRxRuntime->latestImageSnapshot();
    const SstvRxRuntime::Snapshot runtime = m_sstvRxRuntime->snapshot();
    const auto& summary = runtime.image;
    const bool coherentPublication =
        before.image.generation == summary.generation
        && before.image.acquisitionId == summary.acquisitionId
        && before.image.revision == summary.revision
        && image && image->revision == summary.revision
        && image->width == summary.width && image->height == summary.height;
    if (!summary.available || summary.acquisitionId == 0U
        || summary.coverage <= 0.0 || !coherentPublication) {
        if (!automatic) {
            setSstvRxSaveStatus(
                QStringLiteral("error"),
                summary.available
                    ? tr("The SSTV image is still changing; try saving again")
                    : tr("No received SSTV image is available to save"));
        }
        return false;
    }

    const bool terminal = summary.complete || summary.partial
        || summary.cancelled;
    if (automatic && !terminal) {
        return false;
    }
    const QString frameKey = QStringLiteral("%1:%2")
        .arg(summary.generation)
        .arg(summary.acquisitionId);
    const QString saveKey = terminal
        ? QStringLiteral("terminal:%1").arg(frameKey)
        : QStringLiteral("partial:%1:%2")
              .arg(frameKey)
              .arg(summary.revision);
    if (m_sstvRxSavedKeys.contains(saveKey)
        || m_sstvRxPendingSaveKeys.contains(saveKey)) {
        if (!automatic) {
            setSstvRxSaveStatus(QStringLiteral("saved"));
        }
        return false;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    decodium::sstv::SstvImageSaveRequest request;
    request.record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.record.category = decodium::sstv::SstvImageCategory::Received;
    request.record.capturedAtUtc = now;
    if (m_sstvRxStartAcquisitionId == summary.acquisitionId
        && m_sstvRxStartUtc.isValid()) {
        request.record.capturedAtUtc = m_sstvRxStartUtc;
    }
    request.record.createdAtUtc = now;
    request.record.updatedAtUtc = now;
    request.record.mode = summary.mode.left(64);
    request.record.visCode = runtime.vis.primaryPayload >= 0
            && runtime.vis.primaryPayload <= 255
        ? runtime.vis.primaryPayload : -1;
    request.record.visValid = runtime.vis.valid;
    request.record.fskId = runtime.fskId.valid
        ? runtime.fskId.identifier.left(32) : QString {};
    request.record.localCallsign = m_callsign.trimmed().left(64);
    request.record.source = sstvSourceName(runtime.route.source.kind).left(128);
    request.record.frequencyHz = std::isfinite(m_frequency)
            && m_frequency > 0.0
            && m_frequency
                <= static_cast<double>(std::numeric_limits<qint64>::max())
        ? static_cast<qint64>(std::llround(m_frequency)) : 0;
    request.record.audioFrequencyHz =
        std::isfinite(runtime.afc.measuredOffsetHz)
        ? static_cast<qint64>(std::llround(runtime.afc.measuredOffsetHz))
        : 0;
    request.record.sourceSampleRateHz = runtime.activeSampleRate
            <= static_cast<std::uint32_t>(std::numeric_limits<int>::max())
        ? static_cast<int>(runtime.activeSampleRate) : 0;
    request.record.complete = summary.complete;
    request.record.completionPercent = summary.complete
        ? 100
        : std::clamp(
              static_cast<int>(std::floor(summary.coverage * 100.0)),
              0,
              99);
    request.record.slantCorrectionPpm =
        runtime.slant.appliedClockErrorPpm;
    request.record.qualityMetrics = {
        {QStringLiteral("afcMeasuredOffsetHz"),
         runtime.afc.measuredOffsetHz},
        {QStringLiteral("afcCorrectionHz"), runtime.afc.correctionHz},
        {QStringLiteral("afcConfidence"), runtime.afc.confidence},
        {QStringLiteral("slantMeasuredPpm"),
         runtime.slant.measuredClockErrorPpm},
        {QStringLiteral("slantConfidence"), runtime.slant.confidence},
        {QStringLiteral("syncConfidence"), runtime.sync.confidence},
        {QStringLiteral("signalSnrDb"), runtime.signal.snrDb},
        {QStringLiteral("signalRms"), runtime.signal.rms},
    };
    if (m_sstvRxRawAudioAcquisitionId == summary.acquisitionId
        && !m_sstvRxRawAudioPath.isEmpty()) {
        const decodium::sstv::SstvStorageLayout storageLayout(
            m_sstvStorageWorker->storageRoot());
        QString rawPathError;
        if (QFileInfo::exists(m_sstvRxRawAudioPath)
            && storageLayout.containsPath(
                m_sstvRxRawAudioPath, true, &rawPathError)) {
            request.record.rawAudioPath = m_sstvRxRawAudioPath;
        }
    }
    request.record.remote = false;
    if (!summary.complete) {
        request.record.note = summary.cancelled
            ? QStringLiteral("Native RX snapshot saved after cancellation")
            : QStringLiteral("Native RX partial snapshot");
    }
    request.fileNameTemplate = getSetting(
        QStringLiteral("SSTV/ImageNamingTemplate"),
        QStringLiteral("{date}_{time}_{mode}_{remoteCall}_{id}"))
                                   .toString()
                                   .trimmed();
    if (request.fileNameTemplate.isEmpty()) {
        request.fileNameTemplate = QStringLiteral(
            "{date}_{time}_{mode}_{remoteCall}_{id}");
    }

    if (m_sstvStorageRequestId == 0U) {
        m_sstvStorageRequestId = 1U;
    }
    const quint64 requestId = m_sstvStorageRequestId++;
    m_sstvRxSaveRequests.insert(requestId, saveKey);
    m_sstvRxPendingSaveKeys.insert(saveKey);
    setSstvRxSaveStatus(QStringLiteral("saving"));
    if (!worker->enqueueDatabaseOperation(
            [image, request = std::move(request), requestId](
                decodium::sstv::SstvStorageWorker& storage) mutable {
                request.image = sstvSnapshotToImage(*image);
                storage.storeAndInsertImage(std::move(request), requestId);
            })) {
        m_sstvRxSaveRequests.remove(requestId);
        m_sstvRxPendingSaveKeys.remove(saveKey);
        setSstvRxSaveStatus(
            QStringLiteral("error"),
            tr("Could not queue the SSTV image storage operation"));
        return false;
    }
    return true;
}

void DecodiumBridge::maybeAutoSaveSstvRxImage()
{
    if (m_sstvRxAutoSaveEnabled && m_sstvStorageReady) {
        static_cast<void>(queueSstvRxImageSave(true));
    }
}
#endif

bool DecodiumBridge::saveSstvRxImage()
{
#if DECODIUM_HAS_SSTV
    return queueSstvRxImageSave(false);
#else
    return false;
#endif
}

