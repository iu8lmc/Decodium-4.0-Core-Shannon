// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvRxRuntime.h"
#include "src/sstv/integration/SstvStudioController.h"
#include "src/sstv/integration/SstvTxCoordinator.h"
#include "src/sstv/integration/SstvTxSources.h"
#include "src/sstv/diagnostics/SstvDiagnosticsController.h"
#include "src/sstv/diagnostics/SstvDiagnosticLogging.h"
#include "src/sstv/models/SstvShareController.h"
#include "src/sstv/models/SstvGalleryModel.h"
#include "src/sstv/models/SstvThumbnailProvider.h"
#include "src/sstv/storage/SstvStorageWorker.h"
#if DECODIUM_HAS_HAMDRM
#include "src/sstv/digital/HamDrmController.h"
#endif
#include <QThread>
#include <algorithm>
using decodium::sstv::SstvRxRuntime;
QString decodiumSstvDiagnosticSourceToken(decodium::sstv::SstvAudioSourceKind);
#define sstvDiagnosticSourceToken decodiumSstvDiagnosticSourceToken
#endif

void DecodiumBridge::refreshSstvDiagnosticsSnapshot()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_sstvDiagnosticsController
        || !m_sstvDiagnosticsController->ready()) {
        return;
    }

    constexpr quint64 maximumExactJsonInteger
        = (quint64 {1} << 53U) - 1U;
    const auto boundedCounter = [maximumExactJsonInteger](quint64 value) {
        return QVariant::fromValue<qulonglong>(
            std::min(value, maximumExactJsonInteger));
    };
    const auto saturatedSum = [maximumExactJsonInteger](
                                  quint64 left, quint64 right) {
        const quint64 boundedLeft = std::min(left, maximumExactJsonInteger);
        const quint64 boundedRight = std::min(right, maximumExactJsonInteger);
        return boundedLeft > maximumExactJsonInteger - boundedRight
            ? maximumExactJsonInteger
            : boundedLeft + boundedRight;
    };

    QVariantMap capabilities {
        {QStringLiteral("analogRx"), m_sstvRxRuntime != nullptr},
        {QStringLiteral("analogTx"), m_sstvTxCoordinator != nullptr},
        {QStringLiteral("wavReplay"), m_sstvWavReplayController != nullptr},
        {QStringLiteral("wavExport"), m_sstvStudioController != nullptr},
        {QStringLiteral("gallery"), m_sstvGalleryModel != nullptr},
        {QStringLiteral("remoteSharing"), m_sstvShareController != nullptr},
        {QStringLiteral("secureCredentials"),
         m_sstvShareController
             && m_sstvShareController->secureStorageAvailable()},
        {QStringLiteral("liveAudio"), m_sstvRxRuntime != nullptr},
        {QStringLiteral("realRadioTx"),
         m_sstvTxCoordinator
             && (sstvTxUsesVoxPtt() || sstvTxCanControlPtt())},
    };
#if DECODIUM_HAS_HAMDRM
    capabilities.insert(QStringLiteral("hamdrm"),
                        m_hamDrmController != nullptr);
#else
    capabilities.insert(QStringLiteral("hamdrm"), false);
#endif

    const QVariantMap controls = sstvRxControls();
    QVariantMap settings {
        {QStringLiteral("autoDetect"),
         controls.value(QStringLiteral("modeControl")).toString()
             == QStringLiteral("auto")},
        {QStringLiteral("autoSave"), m_sstvRxAutoSaveEnabled},
        {QStringLiteral("retainRawAudio"),
         controls.value(QStringLiteral("retainRawAudio")).toBool()},
        {QStringLiteral("backgroundDetector"), false},
        {QStringLiteral("remoteSharingEnabled"),
         m_sstvShareController && m_sstvShareController->enabled()},
        {QStringLiteral("rxSampleRateHz"), 12'000},
    };
#if DECODIUM_HAS_HAMDRM
    settings.insert(QStringLiteral("hamdrmEnabled"),
                    m_hamDrmController != nullptr);
#else
    settings.insert(QStringLiteral("hamdrmEnabled"), false);
#endif

    QVariantMap rx {
        {QStringLiteral("state"), sstvRxState()},
        {QStringLiteral("sourceKind"), QStringLiteral("unavailable")},
        {QStringLiteral("modeId"), sstvDetectedMode()},
    };
    if (m_sstvRxRuntime) {
        const SstvRxRuntime::Snapshot snapshot
            = m_sstvRxRuntime->snapshot();
        rx.insert(QStringLiteral("sourceKind"),
                  sstvDiagnosticSourceToken(snapshot.route.source.kind));
        QString modeId = snapshot.image.mode;
        if (modeId.isEmpty() && snapshot.vis.modeMapped) {
            modeId = snapshot.vis.mappedMode;
        }
        rx.insert(QStringLiteral("modeId"), modeId);
        rx.insert(QStringLiteral("generation"),
                  boundedCounter(snapshot.route.generation));
        rx.insert(QStringLiteral("queuedChunks"),
                  boundedCounter(snapshot.ingress.queue.queuedChunks));
        rx.insert(QStringLiteral("queuedSamples"),
                  boundedCounter(snapshot.ingress.queue.queuedSamples));
        rx.insert(QStringLiteral("droppedChunks"),
                  boundedCounter(snapshot.ingress.queue.droppedChunks));
        rx.insert(QStringLiteral("droppedSamples"),
                  boundedCounter(snapshot.ingress.queue.droppedSamples));
        rx.insert(QStringLiteral("samplesConverted"),
                  boundedCounter(snapshot.samplesConverted));
        rx.insert(QStringLiteral("samplesResampled"),
                  boundedCounter(snapshot.samplesResampled));
        rx.insert(QStringLiteral("frequencyObservations"),
                  boundedCounter(snapshot.frequencyObservations));
        rx.insert(QStringLiteral("staleChunksDiscarded"),
                  boundedCounter(snapshot.staleChunksDiscarded));
        rx.insert(QStringLiteral("processingFailures"),
                  boundedCounter(snapshot.processingFailures));
        rx.insert(QStringLiteral("producerRejectedCalls"),
                  boundedCounter(snapshot.producerRejectedCalls));
        rx.insert(QStringLiteral("coveragePermille"),
                  qRound(std::clamp(snapshot.image.coverage,
                                    0.0, 1.0) * 1'000.0));
        rx.insert(QStringLiteral("offsetHz"), snapshot.afc.correctionHz);
        rx.insert(QStringLiteral("slantPpm"),
                  snapshot.slant.appliedClockErrorPpm);
    }

    QVariantMap tx {
        {QStringLiteral("state"), sstvTxState()},
        {QStringLiteral("active"), sstvTxActive()},
        {QStringLiteral("pttActive"), sstvTxPttActive()},
        {QStringLiteral("progressPermille"),
         qRound(std::clamp(m_sstvTxProgress, 0.0, 1.0) * 1'000.0)},
        {QStringLiteral("underruns"), boundedCounter(m_sstvTxUnderruns)},
    };

    QVariantMap calibration {
        {QStringLiteral("available"), m_sstvTxCoordinator != nullptr},
        {QStringLiteral("completed"), false},
        {QStringLiteral("success"), false},
    };
    // Preserve the last real terminal tone result when a later non-calibration
    // TX replaces the coordinator's current mode. Availability and running are
    // always recomputed from the live bridge state.
    QVariantMap testTone = m_sstvDiagnosticsController->testToneResults();
    testTone.insert(QStringLiteral("available"),
                    m_sstvTxCoordinator
                        && sstvTxGlobalPreflightReady());
    testTone.insert(QStringLiteral("running"), false);
    if (!testTone.contains(QStringLiteral("success"))) {
        testTone.insert(QStringLiteral("success"), false);
    }
    testTone.insert(QStringLiteral("frequencyHz"), 1'500);
    testTone.insert(QStringLiteral("durationMs"), 2'000);
    if (m_sstvTxCoordinator) {
        const decodium::sstv::SstvTxCoordinatorSnapshot snapshot
            = m_sstvTxCoordinator->snapshot();
        const QString modeId = QString::fromStdString(
            snapshot.stateMachine.mode);
        tx.insert(QStringLiteral("modeId"), modeId);
        tx.insert(QStringLiteral("samplesProduced"),
                  boundedCounter(snapshot.stateMachine.encodedSamples));
        tx.insert(QStringLiteral("failures"),
                  boundedCounter(snapshot.stateMachine.sessionsFailed));

        constexpr auto calibrationPrefix = "calibration-";
        const bool isCalibration = modeId.startsWith(
            QString::fromLatin1(calibrationPrefix));
        const bool isTestTone = modeId
            == QStringLiteral("calibration-black-1500");
        const bool running = isCalibration
            && m_sstvTxCoordinator->stateMachine().active();
        const bool completed = isCalibration
            && snapshot.stateMachine.state
                == decodium::sstv::SstvTxState::Completed;
        int frequencyHz = 0;
        if (isCalibration) {
            const QString toneId = modeId.mid(
                static_cast<qsizetype>(
                    std::char_traits<char>::length(calibrationPrefix)));
            const auto toneKind
                = decodium::sstv::calibrationToneKindFromId(
                    toneId.toStdString());
            if (toneKind.has_value()) {
                frequencyHz = qRound(
                    decodium::sstv::calibrationToneSpec(
                        *toneKind).frequencyHz);
            }
        }
        calibration.insert(QStringLiteral("completed"),
                           isCalibration && !running);
        calibration.insert(QStringLiteral("success"), completed);
        calibration.insert(QStringLiteral("frequencyHz"), frequencyHz);
        calibration.insert(QStringLiteral("levelMilli"),
                           qRound(snapshot.headroom * 1'000.0));
        calibration.insert(QStringLiteral("durationMs"),
                           boundedCounter(
                               snapshot.stateMachine.encodedDurationMs));
        calibration.insert(QStringLiteral("errorCode"),
                           static_cast<int>(
                               snapshot.stateMachine.lastErrorCode));
        if (isTestTone) {
            testTone.insert(QStringLiteral("running"), running);
            testTone.insert(QStringLiteral("success"), completed);
            testTone.insert(QStringLiteral("levelMilli"),
                            qRound(snapshot.headroom * 1'000.0));
            testTone.insert(QStringLiteral("errorCode"),
                            static_cast<int>(
                                snapshot.stateMachine.lastErrorCode));
        }
    }

    QVariantMap storage {
        {QStringLiteral("ready"), m_sstvStorageReady},
    };
    if (m_sstvGalleryModel) {
        const QVariantMap quota = m_sstvGalleryModel->quotaSummary();
        for (const QString& key : {
                 QStringLiteral("recordCount"),
                 QStringLiteral("imageBytes"),
                 QStringLiteral("thumbnailBytes"),
                 QStringLiteral("rawAudioBytes")}) {
            if (quota.contains(key)) {
                storage.insert(key, quota.value(key));
            }
        }
        const QVariantMap retention
            = m_sstvGalleryModel->retentionSettings();
        quint64 quotaBytes = 0U;
        for (const QString& key : {
                 QStringLiteral("imageQuotaBytes"),
                 QStringLiteral("thumbnailQuotaBytes"),
                 QStringLiteral("rawAudioQuotaBytes")}) {
            quotaBytes = saturatedSum(
                quotaBytes, retention.value(key).toULongLong());
        }
        storage.insert(QStringLiteral("quotaBytes"),
                       boundedCounter(quotaBytes));
    }
    if (m_sstvStorageWorker) {
        const auto performance
            = m_sstvStorageWorker->performanceSnapshot();
        storage.insert(QStringLiteral("operations"),
                       boundedCounter(
                           performance.databaseOperationsCompleted));
        storage.insert(QStringLiteral("failures"),
                       boundedCounter(saturatedSum(
                           performance.databaseQueueFailures,
                           performance.imageSaveFailures)));
    }

    QVariantMap share {
        {QStringLiteral("enabled"),
         m_sstvShareController && m_sstvShareController->enabled()},
        {QStringLiteral("configured"),
         m_sstvShareController && m_sstvShareController->configured()},
    };
    if (m_sstvShareController) {
        const QVariantMap shareDiagnostics
            = m_sstvShareController->diagnostics();
        for (auto it = shareDiagnostics.cbegin();
             it != shareDiagnostics.cend(); ++it) {
            share.insert(it.key(), it.value());
        }
        share.insert(QStringLiteral("failures"),
                     m_sstvShareController->errorString().isEmpty() ? 0 : 1);
    }

    QVariantMap hamdrm {
        {QStringLiteral("enabled"), false},
        {QStringLiteral("available"), false},
        {QStringLiteral("state"), QStringLiteral("unavailable")},
    };
#if DECODIUM_HAS_HAMDRM
    if (m_hamDrmController) {
        hamdrm.insert(QStringLiteral("enabled"), true);
        hamdrm.insert(
            QStringLiteral("available"),
            m_hamDrmController->waveformRxAvailable()
                || m_hamDrmController->waveformTxAvailable());
        hamdrm.insert(
            QStringLiteral("state"),
            QStringLiteral("rx:%1 tx:%2")
                .arg(static_cast<int>(m_hamDrmController->rxState()))
                .arg(static_cast<int>(m_hamDrmController->txState())));
        hamdrm.insert(QStringLiteral("activeSessions"),
                      m_hamDrmController->busy() ? 1 : 0);
        hamdrm.insert(QStringLiteral("failures"),
                      m_hamDrmController->error().isEmpty() ? 0 : 1);
    }
#endif

    QString error;
    if (!m_sstvDiagnosticsController->setInputSnapshot({
            {QStringLiteral("capabilities"), capabilities},
            {QStringLiteral("settings"), settings},
            {QStringLiteral("rx"), rx},
            {QStringLiteral("tx"), tx},
            {QStringLiteral("storage"), storage},
            {QStringLiteral("share"), share},
            {QStringLiteral("hamdrm"), hamdrm},
            {QStringLiteral("calibration"), calibration},
            {QStringLiteral("testTone"), testTone},
        }, &error)) {
        decodium::sstv::recordSstvDiagnosticEvent(
            sstvSecurityLog(), QtWarningMsg,
            QStringLiteral("diagnostics.bridge-snapshot-rejected"),
            {{QStringLiteral("reasonCode"),
              QStringLiteral("invalid-scalar-snapshot")}});
    }
#endif
}

