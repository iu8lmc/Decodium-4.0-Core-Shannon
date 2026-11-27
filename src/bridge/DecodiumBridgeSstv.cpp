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

#include "DecodiumSstvAudioRelay.h"

#if DECODIUM_HAS_SSTV
using decodium::sstv::SstvAudioSourceKind;
using decodium::sstv::SstvRxAfcMode;
using decodium::sstv::SstvRxControlSettings;
using decodium::sstv::SstvRxModeControl;
using decodium::sstv::SstvRxRedecodeParameters;
using decodium::sstv::SstvRxRuntime;
using decodium::sstv::SstvRxSlantMode;
bool decodiumSstvQsoRequestFromMap(const QVariantMap&, decodium::sstv::SstvQsoLogRequest*, QString*);
bool decodiumSstvTerminalTxState(decodium::sstv::SstvTxState) noexcept;
bool decodiumSstvRxSettingsFromMap(const QVariantMap&, SstvRxControlSettings, SstvRxControlSettings*, QString*);
QString decodiumSstvRxModeControlName(SstvRxModeControl);
QString decodiumSstvRxAfcModeName(SstvRxAfcMode);
QString decodiumSstvRxSlantModeName(SstvRxSlantMode);
#define sstvQsoRequestFromMap decodiumSstvQsoRequestFromMap
#define terminalSstvTxState decodiumSstvTerminalTxState
#define sstvRxSettingsFromMap decodiumSstvRxSettingsFromMap
#define sstvRxModeControlName decodiumSstvRxModeControlName
#define sstvRxAfcModeName decodiumSstvRxAfcModeName
#define sstvRxSlantModeName decodiumSstvRxSlantModeName
#endif

void DecodiumBridge::initialiseSstvRuntime()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_sstvAudioRelay) {
        m_sstvAudioRelay = std::make_shared<DecodiumSstvAudioRelay>();
    }
    m_sstvRxAutoSaveEnabled = getSetting(
        QStringLiteral("SSTV/RxAutoSaveEnabled"), false).toBool();
    if (!m_sstvStudioController) {
        m_sstvStudioController
            = new decodium::sstv::SstvStudioController(this);
        connect(m_sstvStudioController,
                &decodium::sstv::SstvStudioController::preparedChanged,
                this, &DecodiumBridge::handleSstvStudioPreparedChanged);
        // TX readiness depends on the prepared image AND busy state. Neither
        // changes the coordinator's TX state, so forward both notifications
        // even when an image is cleared or an operation is cancelled.
        connect(m_sstvStudioController,
                &decodium::sstv::SstvStudioController::preparedChanged,
                this, &DecodiumBridge::sstvTxStateChanged);
        connect(m_sstvStudioController,
                &decodium::sstv::SstvStudioController::stateChanged,
                this, &DecodiumBridge::sstvTxStateChanged);
        connect(m_sstvStudioController,
                &decodium::sstv::SstvStudioController::sourceChanged,
                this, &DecodiumBridge::sstvTxStateChanged);
    }
    if (!m_sstvShareController) {
        m_sstvShareController = new decodium::sstv::SstvShareController(
            &secure_settings::default_backend(), this);
        m_sstvShareController->setObjectName(QStringLiteral("sstvShare"));
    }
#if DECODIUM_HAS_HAMDRM
    if (!m_hamDrmController) {
        decodium::sstv::hamdrm::HamDrmControllerConfig config;
        const QString appData = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        if (!appData.isEmpty()) {
            config.partialStoreRoot = QDir(appData).absoluteFilePath(
                QStringLiteral("sstv/hamdrm/partial"));
        }
        if (QStandardPaths::isTestModeEnabled()) {
            const QFileInfo labRoot(qEnvironmentVariable("DECODIUM_LAB_SETTINGS_DIR"));
            const QFileInfo partialRoot(qEnvironmentVariable("DECODIUM_LAB_HAMDRM_PARTIAL_ROOT"));
            if (labRoot.isAbsolute() && labRoot.isDir() && !labRoot.isSymLink()
                && partialRoot.isAbsolute() && partialRoot.isDir() && !partialRoot.isSymLink()
                && partialRoot.dir().canonicalPath() == labRoot.canonicalFilePath()) {
                config.partialStoreRoot = partialRoot.canonicalFilePath();
            }
        }
        decodium::sstv::hamdrm::HamDrmControllerBackends backends;
        decodium::sstv::hamdrm::waveform::HamDrmNativeRxHooks rxHooks;
        rxHooks.activateSharedAudioTap = [this]() {
            return activateHamDrmRxTap();
        };
        rxHooks.deactivateSharedAudioTap = [this]() {
            if (QThread::currentThread() == thread()) {
                deactivateHamDrmRxTap();
                return;
            }
            static_cast<void>(QMetaObject::invokeMethod(
                this, [this]() { deactivateHamDrmRxTap(); },
                Qt::QueuedConnection));
        };
        m_hamDrmRxBackend = std::make_shared<
            decodium::sstv::hamdrm::waveform::HamDrmNativeRxBackend>(
                std::move(rxHooks));

        decodium::sstv::hamdrm::waveform::HamDrmNativeTxHooks txHooks;
        txHooks.configuredCallsign = [this]() {
            return m_callsign.trimmed().toUpper().toStdString();
        };
        txHooks.startPreparedAudio = [this](
            std::unique_ptr<decodium::sstv::SstvPcm16Source> source,
            std::string mode) {
                return startHamDrmPreparedAudio(
                    std::move(source), std::move(mode));
            };
        txHooks.cancelAudio = [this](std::uint64_t sessionId) {
            return cancelHamDrmPreparedAudio(sessionId);
        };
        m_hamDrmTxBackend = std::make_shared<
            decodium::sstv::hamdrm::waveform::HamDrmNativeTxBackend>(
                std::move(txHooks));
        backends.waveformRx = m_hamDrmRxBackend;
        backends.waveformTx = m_hamDrmTxBackend;
        backends.jpeg2000 =
            decodium::sstv::hamdrm::makeNativeHamDrmJpeg2000Backend();
        m_hamDrmController =
            new decodium::sstv::hamdrm::HamDrmController(
                std::move(config), std::move(backends), this);
        m_hamDrmController->setObjectName(QStringLiteral("sstvDigital"));
        connect(m_hamDrmController,
                &decodium::sstv::hamdrm::HamDrmController::operationRejected,
                this, [this](const QString& operation,
                             const QString& detail) {
                    emit statusMessage(tr("HAMDRM %1: %2")
                                           .arg(operation, detail));
                });
        connect(m_hamDrmController,
                &decodium::sstv::hamdrm::HamDrmController::txImageAccepted,
                this,
                [this](QImage image, const QString& profileId,
                       int occupiedBandwidthHz) {
                    queueHamDrmTransmittedImage(
                        std::move(image), profileId, occupiedBandwidthHz);
                });
        connect(m_hamDrmController,
                &decodium::sstv::hamdrm::HamDrmController::receivedImageReady,
                this, [this](QImage image, const QString& profileId, bool hybrid) {
                    const auto* profile = decodium::sstv::hamdrm::HamDrmProfileRegistry::findById(profileId.toStdString());
                    queueHamDrmTransmittedImage(std::move(image), profileId,
                                               profile ? static_cast<int>(profile->occupiedBandwidthHz) : 0,
                                               true, hybrid);
                });
    }
#endif
    initialiseSstvStorage();
    if (!m_sstvDiagnosticsController) {
        m_sstvDiagnosticsController
            = new decodium::sstv::SstvDiagnosticsController(this);
        m_sstvDiagnosticsController->setObjectName(
            QStringLiteral("sstvDiagnostics"));
        auto* const txDiagnosticsRefreshTimer
            = new QTimer(m_sstvDiagnosticsController);
        txDiagnosticsRefreshTimer->setObjectName(
            QStringLiteral("SSTV diagnostics TX refresh"));
        txDiagnosticsRefreshTimer->setSingleShot(true);
        txDiagnosticsRefreshTimer->setInterval(250);
        connect(txDiagnosticsRefreshTimer, &QTimer::timeout,
                this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
        const auto scheduleTxDiagnosticsRefresh =
            [txDiagnosticsRefreshTimer]() {
                if (!txDiagnosticsRefreshTimer->isActive()) {
                    txDiagnosticsRefreshTimer->start();
                }
            };
        connect(
            this, &DecodiumBridge::sstvTxStateChanged,
            txDiagnosticsRefreshTimer,
            [this, txDiagnosticsRefreshTimer,
             scheduleTxDiagnosticsRefresh]() {
                const bool terminal = m_sstvTxCoordinator
                    && terminalSstvTxState(
                        m_sstvTxCoordinator->stateMachine().state());
                if (terminal) {
                    txDiagnosticsRefreshTimer->stop();
                    refreshSstvDiagnosticsSnapshot();
                    return;
                }
                scheduleTxDiagnosticsRefresh();
            });
        connect(this, &DecodiumBridge::transmittingChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::pttPendingChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::pttConfirmedChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::tuningChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::txEnabledChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::autoCqRepeatChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::audioOutputDeviceChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::audioOutputChannelChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::catConnectedChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::catManagerChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::catBackendChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::decoPortUseRemoteChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(
            m_sstvDiagnosticsController,
            &decodium::sstv::SstvDiagnosticsController::inputSnapshotRequested,
            this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot,
            Qt::DirectConnection);
        connect(
            m_sstvDiagnosticsController,
            &decodium::sstv::SstvDiagnosticsController::testToneRequested,
            this,
            [this]() {
                static_cast<void>(startSstvCalibrationTone(
                    QStringLiteral("black-1500")));
                refreshSstvDiagnosticsSnapshot();
            },
            Qt::DirectConnection);
        connect(this, &DecodiumBridge::sstvRxStateChanged,
                this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
        connect(this, &DecodiumBridge::sstvStorageStateChanged,
                this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
        if (m_sstvGalleryModel) {
            connect(m_sstvGalleryModel,
                    &decodium::sstv::SstvGalleryModel::quotaSummaryChanged,
                    this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
            connect(m_sstvGalleryModel,
                    &decodium::sstv::SstvGalleryModel::retentionSettingsChanged,
                    this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
        }
        if (m_sstvShareController) {
            connect(m_sstvShareController,
                    &decodium::sstv::SstvShareController::stateChanged,
                    this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
        }
#if DECODIUM_HAS_HAMDRM
        if (m_hamDrmController) {
            connect(
                m_hamDrmController,
                &decodium::sstv::hamdrm::HamDrmController::operationStateChanged,
                this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
            connect(m_hamDrmController,
                    &decodium::sstv::hamdrm::HamDrmController::errorChanged,
                    this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
        }
#endif
    }
    if (m_sstvRxRuntime) {
        refreshSstvDiagnosticsSnapshot();
        return;
    }

    m_sstvRxRuntime = std::make_unique<SstvRxRuntime>();
    {
        QVariantMap savedControls {
            {QStringLiteral("modeControl"), getSetting(
                 QStringLiteral("SSTV/RxModeControl"),
                 QStringLiteral("auto"))},
            {QStringLiteral("manualMode"), getSetting(
                 QStringLiteral("SSTV/RxManualMode"), QString())},
            {QStringLiteral("modeLockEnabled"), getSetting(
                 QStringLiteral("SSTV/RxModeLockEnabled"), false)},
            {QStringLiteral("lockedMode"), getSetting(
                 QStringLiteral("SSTV/RxLockedMode"), QString())},
            {QStringLiteral("receiveWithoutVis"), getSetting(
                 QStringLiteral("SSTV/RxWithoutVis"), false)},
            {QStringLiteral("timingFallbackEnabled"), getSetting(
                 QStringLiteral("SSTV/RxTimingFallbackEnabled"), true)},
            {QStringLiteral("afcMode"), getSetting(
                 QStringLiteral("SSTV/RxAfcMode"),
                 QStringLiteral("auto"))},
            {QStringLiteral("manualFrequencyCorrectionHz"), getSetting(
                 QStringLiteral("SSTV/RxManualFrequencyCorrectionHz"), 0.0)},
            {QStringLiteral("slantMode"), getSetting(
                 QStringLiteral("SSTV/RxSlantMode"),
                 QStringLiteral("auto"))},
            {QStringLiteral("manualClockErrorPpm"), getSetting(
                 QStringLiteral("SSTV/RxManualClockErrorPpm"), 0.0)},
            {QStringLiteral("replayRetentionSeconds"), getSetting(
                 QStringLiteral("SSTV/RxReplayRetentionSeconds"), 180)},
            {QStringLiteral("retainRawAudio"), getSetting(
                 QStringLiteral("SSTV/RxRetainRawAudio"), false)},
            {QStringLiteral("diagnosticScopeEnabled"), getSetting(
                 QStringLiteral("SSTV/RxDiagnosticScopeEnabled"), false)},
        };
        SstvRxControlSettings settings;
        QString error;
        if (sstvRxSettingsFromMap(
                savedControls, settings, &settings, &error)) {
            static_cast<void>(m_sstvRxRuntime->replaceRxControlSettings(
                std::move(settings)));
        } else {
            const SstvRxControlSettings defaults;
            setSetting(QStringLiteral("SSTV/RxModeControl"),
                       sstvRxModeControlName(defaults.modeControl));
            setSetting(QStringLiteral("SSTV/RxManualMode"), QString {});
            setSetting(QStringLiteral("SSTV/RxModeLockEnabled"), false);
            setSetting(QStringLiteral("SSTV/RxLockedMode"), QString {});
            setSetting(QStringLiteral("SSTV/RxWithoutVis"),
                       defaults.receiveWithoutVis);
            setSetting(QStringLiteral("SSTV/RxTimingFallbackEnabled"),
                       defaults.timingFallbackEnabled);
            setSetting(QStringLiteral("SSTV/RxAfcMode"),
                       sstvRxAfcModeName(defaults.afcMode));
            setSetting(QStringLiteral(
                           "SSTV/RxManualFrequencyCorrectionHz"),
                       defaults.manualFrequencyCorrectionHz);
            setSetting(QStringLiteral("SSTV/RxSlantMode"),
                       sstvRxSlantModeName(defaults.slantMode));
            setSetting(QStringLiteral("SSTV/RxManualClockErrorPpm"),
                       defaults.manualClockErrorPpm);
            setSetting(QStringLiteral("SSTV/RxReplayRetentionSeconds"),
                       defaults.replayRetentionSeconds);
            setSetting(QStringLiteral("SSTV/RxRetainRawAudio"),
                       defaults.retainRawAudio);
            setSetting(QStringLiteral("SSTV/RxDiagnosticScopeEnabled"),
                       defaults.diagnosticScopeEnabled);
            emit errorMessage(tr(
                "Invalid saved SSTV RX controls were reset: %1").arg(error));
        }
    }
    connect(m_sstvRxRuntime.get(), &SstvRxRuntime::runtimeStateChanged,
            this, [this](SstvRxRuntime::State, quint64) {
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
    });
    connect(m_sstvRxRuntime.get(), &SstvRxRuntime::snapshotAvailable,
            this, [this](quint64) {
        const SstvRxRuntime::Snapshot snapshot =
            m_sstvRxRuntime->snapshot();
        if (snapshot.image.acquisitionId != 0U
            && snapshot.image.acquisitionId
                != m_sstvRxStartAcquisitionId) {
            m_sstvRxStartAcquisitionId = snapshot.image.acquisitionId;
            m_sstvRxStartUtc = QDateTime::currentDateTimeUtc();
            if (m_sstvRxRawAudioAcquisitionId != 0U
                && m_sstvRxRawAudioAcquisitionId
                    != snapshot.image.acquisitionId) {
                m_sstvRxRawAudioPath.clear();
                m_sstvRxRawAudioAcquisitionId = 0U;
                emit sstvRxAudioJobChanged();
            }
        }
        emit sstvRxSnapshotChanged();
        maybeAutoSaveSstvRxImage();
    });
    connect(m_sstvRxRuntime.get(), &SstvRxRuntime::visDetectionAvailable,
            this, [this](quint64, int, int, int, int primaryPayload,
                         int, double confidence, const QString& mappedMode) {
        const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime->snapshot();
        emit sstvVisDetected(mappedMode, primaryPayload,
                             snapshot.vis.valid, confidence);
        emit sstvRxSnapshotChanged();
    });
    connect(m_sstvRxRuntime.get(), &SstvRxRuntime::workerError,
            this, [this](const QString& detail) {
        emit errorMessage(tr("SSTV RX: %1").arg(detail));
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
    });

    m_sstvRxAudioJobController =
        new decodium::sstv::SstvRxAudioJobController(
            m_sstvRxRuntime.get(), this);
    m_sstvRxAudioJobController->setObjectName(
        QStringLiteral("sstvRxAudioJobs"));
    connect(m_sstvRxAudioJobController,
            &decodium::sstv::SstvRxAudioJobController::stateChanged,
            this, &DecodiumBridge::sstvRxAudioJobChanged);
    connect(m_sstvRxAudioJobController,
            &decodium::sstv::SstvRxAudioJobController::rawAudioExportFinished,
            this,
            [this](bool ok, const QString& path, quint64 acquisitionId,
                   const QString& error) {
                if (ok) {
                    m_sstvRxRawAudioPath = path;
                    const auto current = m_sstvRxRuntime->snapshot();
                    m_sstvRxRawAudioAcquisitionId = acquisitionId != 0U
                        && acquisitionId == m_sstvRxRawAudioPendingRetainedId
                        && current.image.acquisitionId == m_sstvRxRawAudioPendingImageId
                        && (acquisitionId >> 32U) == (current.image.generation & 0xffff'ffffULL)
                        && current.replay.mostRecentAcquisitionId == acquisitionId
                        ? m_sstvRxRawAudioPendingImageId : 0U;
                    emit statusMessage(tr(
                        "Raw SSTV diagnostic audio saved: %1").arg(path));
                } else if (!error.isEmpty()) {
                    emit errorMessage(tr(
                        "Raw SSTV audio save failed: %1").arg(error));
                }
                m_sstvRxRawAudioPendingImageId = 0U;
                m_sstvRxRawAudioPendingRetainedId = 0U;
                emit sstvRxAudioJobChanged();
            });
    connect(m_sstvRxAudioJobController,
            &decodium::sstv::SstvRxAudioJobController::redecodePrepared,
            this,
            [this](bool ok, const QUrl& privateWav,
                   const QString& error) {
                if (!ok || !m_sstvRxRuntime) {
                    if (!error.isEmpty()) {
                        emit errorMessage(tr(
                            "SSTV re-decode preparation failed: %1")
                                              .arg(error));
                    }
                    emit sstvRxAudioJobChanged();
                    return;
                }
                const SstvRxRedecodeParameters parameters =
                    m_sstvRxAudioJobController
                        ->preparedRedecodeParameters();
                m_sstvRedecodeRestoreControls = sstvRxControls();
                SstvRxControlSettings temporary =
                    m_sstvRxRuntime->rxControlSnapshot().settings;
                temporary.modeControl = parameters.mode.empty()
                    ? SstvRxModeControl::Automatic
                    : SstvRxModeControl::Manual;
                temporary.manualMode = parameters.mode;
                temporary.modeLockEnabled = false;
                temporary.lockedMode.clear();
                temporary.receiveWithoutVis = !parameters.mode.empty();
                temporary.timingFallbackEnabled = true;
                temporary.afcMode = parameters.afcMode;
                temporary.manualFrequencyCorrectionHz =
                    parameters.frequencyCorrectionHz;
                temporary.slantMode = parameters.slantMode;
                temporary.manualClockErrorPpm = parameters.clockErrorPpm;
                if (!m_sstvRxRuntime->requestRxRedecode(parameters)
                    || !m_sstvRxRuntime->replaceRxControlSettings(
                        std::move(temporary))) {
                    emit errorMessage(tr(
                        "The requested SSTV re-decode controls were rejected"));
                    m_sstvRxAudioJobController->discardPreparedRedecode();
                    m_sstvRedecodeRestoreControls.clear();
                    emit sstvRxAudioJobChanged();
                    return;
                }
                m_sstvRedecodeActive = true;
                if (!startSstvWavReplay(privateWav)) {
                    SstvRxControlSettings restore =
                        m_sstvRxRuntime->rxControlSnapshot().settings;
                    QString restoreError;
                    if (sstvRxSettingsFromMap(
                            m_sstvRedecodeRestoreControls,
                            restore, &restore, &restoreError)) {
                        static_cast<void>(
                            m_sstvRxRuntime->replaceRxControlSettings(
                                std::move(restore)));
                    }
                    m_sstvRedecodeActive = false;
                    m_sstvRedecodeRestoreControls.clear();
                    m_sstvRxAudioJobController->discardPreparedRedecode();
                    emit errorMessage(tr(
                        "The prepared SSTV re-decode could not start"));
                }
                emit sstvRxAudioJobChanged();
            });

    m_sstvWavReplayController
        = new decodium::sstv::SstvWavReplayController(
            m_sstvRxRuntime.get(), this);
    m_sstvWavReplayController->setObjectName(
        QStringLiteral("sstvWavReplay"));
    connect(m_sstvWavReplayController,
            &decodium::sstv::SstvWavReplayController::stateChanged,
            this, &DecodiumBridge::sstvWavReplayChanged);
    connect(m_sstvWavReplayController,
            &decodium::sstv::SstvWavReplayController::progressChanged,
            this, &DecodiumBridge::sstvWavReplayChanged);
    connect(m_sstvWavReplayController,
            &decodium::sstv::SstvWavReplayController::replayChanged,
            this, &DecodiumBridge::sstvWavReplayChanged);
    connect(m_sstvWavReplayController,
            &decodium::sstv::SstvWavReplayController::errorOccurred,
            this, [this](const QString& detail) {
                emit errorMessage(tr("SSTV WAV replay: %1").arg(detail));
            });
    connect(m_sstvWavReplayController,
            &decodium::sstv::SstvWavReplayController::replayFinished,
            this, &DecodiumBridge::finishSstvWavReplay);
    refreshSstvDiagnosticsSnapshot();
#endif
}

QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)
{
    QVariantMap response {
        {QStringLiteral("accepted"), false},
        {QStringLiteral("requestId"), QString {}},
        {QStringLiteral("error"), QString {}}
    };
#if DECODIUM_HAS_SSTV
    if (m_shuttingDown || QCoreApplication::closingDown()) {
        response.insert(QStringLiteral("error"),
                        tr("Decodium is shutting down"));
        return response;
    }
    if (!m_sstvStorageReady || !m_sstvStorageWorker
        || !m_sstvStorageWorker->isInitialized()) {
        response.insert(QStringLiteral("error"),
                        tr("SSTV Gallery storage is not ready"));
        return response;
    }
    constexpr qsizetype kMaximumPendingQsoLogs = 32;
    if (m_sstvQsoLogRequests.size() >= kMaximumPendingQsoLogs) {
        response.insert(QStringLiteral("error"),
                        tr("Too many SSTV QSO operations are already pending"));
        return response;
    }

    decodium::sstv::SstvQsoLogRequest request;
    QString validationError;
    if (!sstvQsoRequestFromMap(values, &request, &validationError)) {
        response.insert(QStringLiteral("error"), validationError.left(512));
        return response;
    }
    if (!request.createNewQso
        && !isSstvExistingQsoChoiceCurrent(request.existingQsoId)) {
        m_sstvIssuedExistingQsoIds.clear();
        invalidateQsoSearchCache();
        warmLogCacheAsync();
        response.insert(
            QStringLiteral("error"),
            tr("Select an existing QSO from the current Decodium logbook list"));
        return response;
    }
    for (auto it = m_sstvQsoLogRequests.cbegin();
         it != m_sstvQsoLogRequests.cend(); ++it) {
        const QVariantMap queued = it->value(
            QStringLiteral("request")).toMap();
        if (queued.value(QStringLiteral("imageRecordId")).toString()
                == request.imageRecordId) {
            response.insert(QStringLiteral("error"),
                            tr("This SSTV image already has a pending QSO operation"));
            return response;
        }
    }

    constexpr quint64 kRequestNamespace = 0x5353545600000000ULL;
    constexpr quint64 kSerialMask = 0x00000000ffffffffULL;
    quint64 requestId = 0U;
    for (int attempt = 0; attempt < 64 && requestId == 0U; ++attempt) {
        const quint64 serial = m_sstvQsoStorageSerial++ & kSerialMask;
        if (serial == 0U) {
            continue;
        }
        const quint64 candidate = kRequestNamespace | serial;
        if (!m_sstvQsoLogRequests.contains(candidate)) {
            requestId = candidate;
        }
    }
    if (requestId == 0U) {
        response.insert(QStringLiteral("error"),
                        tr("Could not allocate an SSTV QSO request"));
        return response;
    }

    QVariantMap pending {
        {QStringLiteral("phase"), QStringLiteral("fetch")},
        {QStringLiteral("request"), values},
        {QStringLiteral("qsoCreated"), false},
        {QStringLiteral("qsoId"), QString {}}
    };
    m_sstvQsoLogRequests.insert(requestId, std::move(pending));
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (!worker->enqueueDatabaseOperation(
            [imageRecordId = request.imageRecordId, requestId](
                decodium::sstv::SstvStorageWorker& storage) {
                storage.fetchRecord(imageRecordId, requestId);
            })) {
        m_sstvQsoLogRequests.remove(requestId);
        response.insert(QStringLiteral("error"),
                        tr("Could not queue the SSTV Gallery preflight"));
        return response;
    }
    response.insert(QStringLiteral("accepted"), true);
    response.insert(QStringLiteral("requestId"), QString::number(requestId));
    return response;
#else
    Q_UNUSED(values)
    response.insert(QStringLiteral("error"),
                    tr("This Decodium build has no native SSTV support"));
    return response;
#endif
}
