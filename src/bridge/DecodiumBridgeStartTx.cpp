#include "BridgeConfigConstants.h"
#include "Ft2LinkPcmStats.h"
#include "FtxMessageHelpers.h"
#include "DecodiumBridgeMamWave.h"
#include "DecodiumBridgeWavIO.h"
#include "DecodiumBridgeTxWave.h"
#include "DecodiumBridgeTxTimingMath.h"
#include "DecodiumBridgeTxPolicy.h"
#include "DecodiumBridgeTxPcm.h"
#include "DecodiumBridgeTxFormat.h"
#include "DecodiumBridge.h"
#include "Q65DopplerTracker.h"


quint64 decodiumCurrentProcessCpuUsec();
bool decodiumCurrentProcessGpuTimeNs(quint64*);

#if defined(Q_OS_MACOS)
#include <execinfo.h>
#include <cstdlib>
#endif

#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvQsoLog.h"
#include "src/sstv/integration/SstvRxRuntime.h"
#include "src/sstv/integration/SstvTxCoordinator.h"
#endif

#include "DecoPortLink.h"
#include "DecolinkLink.h"
#include "DecodiumDecoPortGateway.h"
#include "DecodiumNativeCatShare.h"
#include "DecodiumLogging.hpp"
#include "DecodiumAlertManager.h"
#include "DecodiumProfileSettings.h"
#include "Sequencer/QsoSequencerRules.hpp"
#include "Sequencer/MessageTokenRules.hpp"
#include "Network/MessageClient.hpp"
#include "DxccLookup.h"
#include "DecodiumLegacyBackend.h"
#include "Network/DecodiumPskReporterLite.h"
#include <QHostAddress>
#include <QProcessEnvironment>
#include "helper_functions.h"
#include "Audio/AudioDevice.hpp"
#include "Audio/soundin.h"
#include "Audio/soundout.h"
#include "Modulator/Modulator.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"
#include "commons.h"
#include "DecodiumAudioSink.h"
#include "src/rtl/RtlSdrAudioOutput.h"
#include "src/rtl/RtlSdrInput.h"
#include "Radio.hpp"
Radio::Frequency decodiumBridgeParseFrequencyHzValue(const QVariant&, bool*);

#include <QClipboard>
#include <QCoreApplication>
#include <QByteArray>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QScreen>
#include <QQuickGraphicsConfiguration>
#include <QQuickWindow>
#include <QWindow>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QFormLayout>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QSettings>
#include <QMessageAuthenticationCode>
#include <QNetworkInterface>
#include <QHostInfo>
#include <QUdpSocket>
#include <QTcpSocket>
#include <QSerialPortInfo>
#include "DecoPortHubOwners.h"
#include <QtConcurrent/QtConcurrent>
#include <QPointer>
#include <QSslSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QSqlDatabase>      // 1.0.238 Phase 5.2: session row in `sessions` table
#include <QSqlError>
#include <QSqlQuery>
#include <QMutex>
#include <QMutexLocker>
#include <QList>
#include <QPointer>
#include <QProcess>
#include <QRunnable>
#include <QTimeZone>
#include <QtEndian>
#include <QTimer>
#include <QScopeGuard>
#include <QThread>
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QOperatingSystemVersion>
#include "Network/FoxVerifier.hpp"
#include "wsjtx_config.h"
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSaveFile>
#include <QSysInfo>
#include <QTextStream>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include "CwSidetone.h"
#include "CwTiming.h"
#include <QBuffer>
#include <algorithm>
#include <QMediaDevices>
#include <QRandomGenerator>
#include <QDebug>
#include <QDesktopServices>
#include <QDataStream>
#include <QUrl>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QScopedValueRollback>
#include <functional>
#include <memory>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <complex>
#include <chrono>
#include <cwchar>
#include <limits>
#include <thread>
#include <utility>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#if defined(_MSC_VER)
#pragma comment(lib, "pdh.lib")
#endif
#else
#include <sys/resource.h>
#include <sys/time.h>
#endif
#include "Ft2LogBridgeLauncher.h"
#ifdef Q_OS_MAC
#include <mach/mach.h>
#include <mach/task_info.h>
#endif
#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef FFTW3_SINGLE_FOUND
#include <fftw3.h>
#endif
#include <vector>
#if DECODIUM_HAS_RTTY
// Il piano di banda RTTY: una tabella sola per il menu, la finestra RTTY e
// il QSY che accompagna il cambio di modo. Riscriverla qui vorrebbe dire
// avere due elenchi che prima o poi discordano.
#include "app/BandPlan.h"
#endif


using namespace decodium::seq;
bool decodiumBridgeIsFt2LinkApplicationMode(QString);
bool decodiumBridgeTruthyEnvironmentFlag(const char*);
bool decodiumBridgeCatSuppressedForTx();
#define isFt2LinkApplicationMode decodiumBridgeIsFt2LinkApplicationMode
#define truthyEnvironmentFlag decodiumBridgeTruthyEnvironmentFlag
#define catSuppressedByEnvironment decodiumBridgeCatSuppressedForTx
void decodiumBridgeDecodeFlowLog(const QString&);
void decodiumBridgeTxPrepareTimelineLog(const QString&);
void decodiumBridgeRetireAudioSink(QAudioSink*, QBuffer*, const QString&);
QString decodiumBridgeBuildTxRecordingPath(QString const&, bool);
QString decodiumBridgeTxWaveMetricsSummary(QVector<float> const&);
double decodiumBridgeTxWavePeak(QVector<float> const&);
QString decodiumBridgeAudioDeviceIdForSettings(const QAudioDevice&);
QString decodiumBridgeAudioDeviceIdSettingForLog(QString);
qreal decodiumBridgeTxAttenuationFromSlider(double);
qreal decodiumBridgeTxGainFromSlider(double);
bool decodiumBridgeIsHamlibFamilyBackend(QString const&);
bool decodiumBridgeLegacyRigControlFallback(DecodiumLegacyBackend*, const QString&);
bool decodiumBridgeSharedCatUsesVox(QObject const*, DecodiumTransceiverManager*, const QString&, DecodiumOmniRigManager*);
bool decodiumBridgeSharedCatCanPtt(QObject const*, DecodiumTransceiverManager*, const QString&, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
void decodiumBridgeSharedActiveCatSetPtt(QObject const*, DecodiumTransceiverManager*, const QString&, bool, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
void decodiumBridgeSharedCatSetTxPtt(QObject const*, DecodiumTransceiverManager*, const QString&, bool, double, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
bool decodiumBridgeSharedCatSetTxPttAsync(QObject const*, DecodiumTransceiverManager*, const QString&, bool, double, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
#if defined(Q_OS_MAC)
AudioDevice::Channel decodiumBridgeTxOutputChannelForFormat(const QAudioFormat&, int);
#define txOutputChannelForFormat decodiumBridgeTxOutputChannelForFormat
#endif
#define bridgeLog decodiumBridgeDecodeFlowLog
#define txTimelineLog decodiumBridgeTxPrepareTimelineLog
#define retireAudioSink decodiumBridgeRetireAudioSink
#define buildTxRecordingPath decodiumBridgeBuildTxRecordingPath
#define txWaveMetricsSummary decodiumBridgeTxWaveMetricsSummary
#define txWavePeak decodiumBridgeTxWavePeak
#define audioDeviceIdForSettings decodiumBridgeAudioDeviceIdForSettings
#define audioDeviceIdSettingForLog decodiumBridgeAudioDeviceIdSettingForLog
#define txAttenuationFromSlider decodiumBridgeTxAttenuationFromSlider
#define txGainFromSlider decodiumBridgeTxGainFromSlider
#define isHamlibFamilyBackend decodiumBridgeIsHamlibFamilyBackend
#define useLegacyRigControlFallback decodiumBridgeLegacyRigControlFallback
#define activeCatUsesVoxPtt decodiumBridgeSharedCatUsesVox
#define activeCatCanPtt decodiumBridgeSharedCatCanPtt
#define activeCatSetPtt decodiumBridgeSharedActiveCatSetPtt
#define activeCatSetTxPtt decodiumBridgeSharedCatSetTxPtt
#define activeCatSetTxPttAsync decodiumBridgeSharedCatSetTxPttAsync

void DecodiumBridge::startTx()
{
    if (!startTxPreflight()) return;

    bridgeLog("startTx() called: transmitting=" + QString::number(m_transmitting) +
              " tuning=" + QString::number(m_tuning) +
              " currentTx=" + QString::number(m_currentTx) +
              " mode=" + m_mode +
              " txFreq=" + QString::number(m_txFrequency) +
              " txLevel=" + QString::number(m_txOutputLevel));
    if (m_bridgeAudioTuneActive) {
        bridgeLog("startTx: stopping bridge-owned tune audio before TX");
        stopTune();
    }
    if (m_transmitting || m_tuning) { bridgeLog("startTx: already TX/tuning, abort"); return; }
    bool const customAudioTxActive = m_cwTxActive || m_ft2LinkTxActive || m_telemetryTxActive;
    if (!txDispatchGuardAllows(QStringLiteral("startTx"))) {
        return;
    }
    if (!customAudioTxActive && deferFt8AutoTxForDecode()) return;
    m_activeFt8AutoTxFullFrame = !customAudioTxActive && usesFt8AutoTxDecodeGate();
    m_activeFt8AutoTxContext = ft8AutoTxDecodeContext();
    QString const txMode = m_ft2LinkTxActive ? QStringLiteral("FT2LINK") : m_mode;
    if (!prepareStartTxSelection(customAudioTxActive)) return;

    QString msg = m_ft2LinkTxActive
        ? m_pendingFt2LinkText
        : (m_cwTxActive ? m_pendingCwText
        : (m_telemetryTxActive ? m_pendingTelemetryHex : currentBridgeTxRepresentativeMessage()));
    if (!customAudioTxActive) {
        forceRecentRogerReportSignoffIfNeeded(msg, QStringLiteral("startTx"));
    }
    bridgeLog("startTx: msg=[" + msg + "]"
              + " decoPortRemote=" + QString::number(m_decoPortUseRemote ? 1 : 0)
              + " remoteLinked=" + QString::number(
                    (remoteLinkIfAny() && remoteLinkIfAny()->isLinked()) ? 1 : 0));
    if (msg.trimmed().isEmpty()) {
        // "Nessun messaggio selezionato" manda a cercare nel posto sbagliato
        // quando il messaggio non c'e' perche' non puo' esserci: senza
        // nominativo regenerateTxMessages() esce alla prima riga e restano
        // vuoti tutti e sei. Capita su un computer installato solo per usare
        // una radio in rete, dove i dati di stazione non li ha messi nessuno.
        if (m_callsign.trimmed().isEmpty()) {
            emit errorMessage(tr("Cannot transmit: no callsign set. "
                                 "Fill in your callsign (and locator) in Settings > Station: "
                                 "without it no transmit message can be built."));
            bridgeLog("startTx: abort, callsign empty (no TX messages can exist)");
            return;
        }
        emit errorMessage(tr("No TX message selected"));
        bridgeLog("startTx: empty msg abort (callsign=[" + m_callsign + "] tx6=[" + m_tx6 + "])");
        return;
    }
    if (!customAudioTxActive && !repairOrRejectStalePartnerTxMessage(msg, QStringLiteral("startTx"))) {
        emit statusMessage(QStringLiteral("TX bloccato: messaggio non coerente con il QSO attivo"));
        return;
    }
    bool const forceImmediateLabTx = truthyEnvironmentFlag("DECODIUM_LAB_FORCE_TX_IMMEDIATE");
    if (!customAudioTxActive && m_monitoring && !forceImmediateLabTx) {
        int elapsedMs = -1;
        int latestStartMs = 0;
        int ft2AsyncDelayMs = 0;
        if (!m_foxElasticTxNow && isFt2AsyncTxStartTooLate(&elapsedMs, &latestStartMs, &ft2AsyncDelayMs)) {
            bridgeLog(QStringLiteral("startTx: skipped late FT2 async TX%1 elapsed=%2ms latest=%3ms msg=[%4]")
                          .arg(m_currentTx)
                          .arg(elapsedMs)
                          .arg(latestStartMs)
                          .arg(msg.trimmed()));
            scheduleFt2AsyncTxAtNextSafeSlot(QStringLiteral("startTx"),
                                             elapsedMs,
                                             latestStartMs,
                                             ft2AsyncDelayMs);
            emit statusMessage(QStringLiteral("TX%1 FT2 rimandato: slot troppo avanzato")
                               .arg(m_currentTx));
            return;
        }
        if (isSyncTxStartTooLate(&elapsedMs, &latestStartMs)) {
            bridgeLog(QStringLiteral("startTx: skipped late %1 TX%2 elapsed=%3ms latest=%4ms msg=[%5]")
                          .arg(m_mode)
                          .arg(m_currentTx)
                          .arg(elapsedMs)
                          .arg(latestStartMs)
                          .arg(msg.trimmed()));
            if (usesDeferredManualSyncTx() && !m_txEnabled && !m_autoCqRepeat) {
                m_deferredManualSyncTx = true;
            }
            scheduleSyncTxAtNextValidSlot(QStringLiteral("startTx"), elapsedMs, latestStartMs);
            emit statusMessage(QStringLiteral("TX%1 rimandato: slot %2 troppo avanzato")
                               .arg(m_currentTx)
                               .arg(m_mode));
            return;
        }
    }
    if (!customAudioTxActive && forceImmediateLabTx) {
        bridgeLog(QStringLiteral("startTx: lab force immediate TX timing bypass active"));
    }
    // 1.0.225 — m_lastTransmittedMessage spostato DOPO ensureTxAudioPrepared
    // (BUG #11 agente-tx). Pre-1.0.225 era scritto qui PRIMA del prepare:
    // se ensureTxAudioPrepared falliva, m_lastTransmittedMessage restava
    // settato pur senza aver TX, causando completeFinishedSignoffIfReady
    // falsi positivi su TX5/RR73 inferenza signoff -> log QSO prematuro.
    // Ora settato solo dopo ensureTxAudioPrepared OK.
    m_lastTxActivityUtc = QDateTime::currentDateTimeUtc();

    // MAM multi-stream cannot be emitted by the embedded MainWindow: it owns
    // one FTX message only. Retain legacy RX/CAT, but route the composite PCM
    // through the bridge audio/PTT path on macOS.
    bool const bridgeOwnsMamTx = multiStreamActive()
        && shouldUseBridgeAudioForLegacyDigitalTx();
    if (!customAudioTxActive && usingLegacyBackendForTx()
        && !bridgeOwnsMamTx && !catSuppressedByEnvironment()) {
        if (m_explicitManualTxRequest == m_currentTx) {
            m_explicitManualTxRequest = 0;
        }
        bridgeLog("startTx: delegating to legacy backend");
        if (m_recordTxEnabled) {
            QString recordingError;
            QVector<float> const wave = buildTxWaveformForMessage(
                m_mode, msg, effectiveTxAudioFrequencyHz(),
                effectivePeriodMsForMode(m_mode) / 1000.0, &recordingError);
            if (!wave.isEmpty()) {
                QString const txRecordPath = buildTxRecordingPath(m_mode, false);
                saveTxRecordingAsync(txRecordPath, wave, 48000, QStringLiteral("TX legacy"));
            } else {
                bridgeLog("TX recording skipped (legacy): " + recordingError + " msg=[" + msg + "]");
            }
        }
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->armCurrentTx();
        scheduleLegacyStateRefreshBurst();
        emit statusMessage("TX armato via backend legacy: " + msg.trimmed());
        return;
    }
    if (bridgeOwnsMamTx) {
        bridgeLog(QStringLiteral("startTx: MAM multi-stream using bridge composite audio instead of legacy mono TX"));
    }
    if (!customAudioTxActive && usingLegacyBackendForTx() && catSuppressedByEnvironment()) {
        bridgeLog(QStringLiteral("startTx: CAT disabled, using bridge audio-only TX path instead of legacy PTT"));
    }

    // Usa la cache preparata quando TX è stato armato. Se non è pronta
    // (es. risposta FT2 immediata), cade al build sincrono minimo qui.
    QVector<float> wave;
    QByteArray preparedPcm;
    QAudioFormat preparedFmt;
    QAudioDevice preparedDev;
    bool const tciAudioTx = usingTciAudioInput();
    // CW-audio: il tono audio e' il sidetone (la portante CW reale = dial + sidetone).
    const int txAudioFrequency = m_cwTxActive
        ? m_cwSidetoneHz
        : (m_ft2LinkTxActive ? 1500 : effectiveTxAudioFrequencyHz());
    const int xitHz = catSplitXitHzForTxFrequency(m_txFrequency);
    if (xitHz != 0 || catSplitTxDialFrequencyHz() > 0.0) {
        bridgeLog(QStringLiteral("startTx split audio: ui_tx=%1 audio_tx=%2 xit=%3 tx_dial=%4")
                      .arg(QString::number(m_txFrequency),
                           QString::number(txAudioFrequency),
                           QString::number(xitHz),
                           QString::number(catSplitTxDialFrequencyHz(), 'f', 0)));
    }
    // Come sopra: la radio remota si porta via l'audio prima della scheda.
    bool needPcm = !tciAudioTx && !m_decoPortUseRemote;
#if defined(Q_OS_MAC)
    needPcm = false;
#endif
    if (needPcm) {
        // Windows can keep a QAudioDevice wrapper alive after the user reselects
        // the same visible endpoint. Resolve the real TX route for every TX
        // start, while still reusing the prepared PCM when format/channel match.
        m_cachedTxOutputDeviceValid = false;
    }
    QString prepareError;
    if (!ensureTxAudioPrepared(msg, txAudioFrequency, needPcm,
                               &wave, &preparedPcm, &preparedFmt, &preparedDev,
                               &prepareError)) {
        QString const errorText = prepareError.isEmpty()
            ? QStringLiteral("Audio TX: generazione waveform fallita")
            : prepareError;
        emit errorMessage(errorText.startsWith(QStringLiteral("Audio "), Qt::CaseInsensitive)
                          ? errorText
                          : QStringLiteral("Audio TX: %1 | modo=%2 freqAudio=%3 Hz msg=\"%4\"")
                                .arg(errorText, m_mode, QString::number(txAudioFrequency), msg.trimmed()));
        bridgeLog("startTx: TX audio prepare failed: " + errorText + " msg=[" + msg + "]");
        return;
    }
    bridgeLog("startTx: payload " + txWaveMetricsSummary(wave) +
              " cache=" + QString::number((m_txAudioCache.message == msg.trimmed()) ? 1 : 0));

#if defined(Q_OS_MAC)
    {
        bool requestedDeviceFound = false;
        QAudioDevice outDev = resolveTxOutputDevice(&requestedDeviceFound);

        if (!requestedDeviceFound) {
            bridgeLog("startTx(mac): requested output device not found, fallback to default: " +
                      outDev.description() +
                      " selected_id=" + audioDeviceIdSettingForLog(audioDeviceIdForSettings(outDev)) +
                      " requested=[" + m_audioOutputDevice + "] requested_id=" +
                      audioDeviceIdSettingForLog(m_audioOutputDeviceId));
            emit statusMessage(tr("Transmit audio device not found, using the default one: %1")
                           .arg(outDev.description()));
        }
        const QAudioFormat outFmt = chooseTxAudioFormat(outDev);
        const AudioDevice::Channel outChannel = txOutputChannelForFormat(outFmt, m_audioOutputChannel);
        const qreal attenuationDb = txAttenuationFromSlider(m_txOutputLevel);
        QByteArray macPcm = buildTxPcmBuffer(wave, outFmt, outChannel);
        if (macPcm.isEmpty()) {
            QString const errorText =
                QStringLiteral("Audio TX: PCM macOS non valido | device=\"%1\" formato=%2 canale=%3 msg=\"%4\"")
                    .arg(outDev.description(),
                         audioFormatToString(outFmt),
                         QString::number(static_cast<int>(outChannel)),
                         msg.trimmed());
            bridgeLog("startTx(mac-pcm): " + errorText);
            emit errorMessage(errorText);
            return;
        }

        int const macPeriodMs = effectivePeriodMsForMode(txMode);
        int const macSlotElapsedMs = macPeriodMs > 0
            ? static_cast<int>(correctedUtcEpochMs() % static_cast<qint64>(macPeriodMs))
            : -1;
        bool const macFt2Async =
            txMode == QStringLiteral("FT2") && m_asyncTxEnabled;
        bool const macAlignToSyncSlot = shouldAlignTxAudioToCurrentSyncSlot();
        int const macNominalLeadInMs =
            (txMode == QStringLiteral("FT2"))
                ? ((macFt2Async && !macAlignToSyncSlot) ? 0 : 450)
                : txSyncLeadInMsForMode(txMode);
        int macInitialSilenceMs = macNominalLeadInMs;
        if (macAlignToSyncSlot && macSlotElapsedMs >= 0) {
            macInitialSilenceMs = macSlotElapsedMs < macNominalLeadInMs
                ? macNominalLeadInMs - macSlotElapsedMs
                : 0;
        }
        int const macLeadInFrames =
            qMax(0, qRound(static_cast<double>(outFmt.sampleRate()) * macInitialSilenceMs / 1000.0));
        qsizetype const macLeadInBytes = outFmt.bytesForFrames(macLeadInFrames);
        if (macLeadInBytes > 0 && macLeadInBytes <= std::numeric_limits<int>::max()) {
            macPcm.prepend(QByteArray(static_cast<int>(macLeadInBytes), '\0'));
        }
        qint64 const txPayloadMs = qMax<qint64>(
            1, qRound64(static_cast<double>(wave.size()) * 1000.0 / 48000.0));
        qint64 const txPlaybackMs = txPayloadMs + macInitialSilenceMs;

        updateSoundOutputDevice();
        m_soundOutput->setAttenuation(attenuationDb);
        if (m_modulator && m_modulator->isActive())
            m_modulator->stop(true);

        // === GitHub TxController: aggiorna contatori retry ===
        if (!customAudioTxActive) {
            if (m_currentTx == m_lastNtx) {
                ++m_txRetryCount;
            } else {
                m_txRetryCount = 1;
                m_lastNtx = m_currentTx;
            }

            bool const txCarries73Payload =
                (m_currentTx == 5) || messageCarries73Payload(msg);
            bool const ft2DeferredSignoff =
                txCarries73Payload
                && (m_qsoProgress > 1)
                && (m_mode == QStringLiteral("FT2"))
                && (!m_quickQsoEnabled || !m_quickPeerSignaled)
                && !m_logAfterOwn73;
            bool const autoCqDeferredSignoff =
                m_autoCqRepeat
                && txCarries73Payload
                && (m_qsoProgress > 1)
                && (m_mode == QStringLiteral("FT4") || m_mode == QStringLiteral("FT8"))
                && !m_logAfterOwn73;
            if (ft2DeferredSignoff || autoCqDeferredSignoff) {
                m_ft2DeferredLogPending = true;
                capturePendingAutoLogSnapshot();
            }
        }

        m_activeTxNumber = m_currentTx;
        m_activeTxMessage = msg.trimmed();
        m_activeTxWasPureAutoCqCq = isAutoCqBurstPureCq(m_activeTxNumber,
                                                         m_activeTxMessage);
        applyConfiguredCatRigMode(QStringLiteral("startTx"));
        bool const satelliteHalfDuplex = ft2LinkSatelliteHalfDuplexOperationActive();
        if (!satelliteHalfDuplex) {
            syncActiveCatTxSplitFrequency(QStringLiteral("startTx"));
        }
        bool const voxPtt = activeCatUsesVoxPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat);
        if (satelliteHalfDuplex && m_hamlibCat) {
            // The absolute RX/TX VFO pair was programmed before this call.
            // Reissuing generic fake-split PTT here would collapse it to the
            // FT audio offset, so assert only PTT after the settle guard.
            m_hamlibCat->setRigPtt(true);
            bridgeLog(QStringLiteral("[FT2SAT] PTT ON after CAT settle (macOS)"));
        } else if (!voxPtt && activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
            activeCatSetTxPtt(this, m_hamlibCat, m_catBackend,
                              true, catSplitTxPttDialFrequencyHz(),
                              m_omniRigCat, m_legacyBackend);
        } else if (voxPtt) {
            bridgeLog("PTT VOX(mac): audio-only TX; no CAT/DTR/RTS command will be sent");
        }

        m_lastTransmittedMessage = msg.trimmed();
        m_lastTxActivityUtc = QDateTime::currentDateTimeUtc();
        if (txMode == QStringLiteral("FT8") && !m_tuning) {
            m_lastFt8MessageTxMs = QDateTime::currentMSecsSinceEpoch();
        }
        m_transmitting = true;
        emit transmittingChanged();
        applyTxAudioSchedulingBoost(QStringLiteral("tx-mac"));
        suspendNonAudioTxWork(QStringLiteral("tx-mac"));

        if (m_ft2LinkTxActive) {
            int rxBufferedAtTxStart = 0;
            {
                QMutexLocker locker(&m_audioBufferMutex);
                rxBufferedAtTxStart = m_audioBuffer.size();
            }
            bridgeLog(QStringLiteral("FT2-Link TX start(mac): payloadMs=%1 initialSilenceMs=%2 pcmBytes=%3 monitor=%4 soundInput=%5 rxSuspendedBefore=%6 rxBuffered=%7 periodTimer=%8")
                          .arg(txPayloadMs)
                          .arg(macInitialSilenceMs)
                          .arg(macPcm.size())
                          .arg(m_monitoring ? QStringLiteral("on") : QStringLiteral("off"))
                          .arg(m_soundInput ? QStringLiteral("alive") : QStringLiteral("null"))
                          .arg(m_rxAudioSuspendedForTx ? QStringLiteral("yes") : QStringLiteral("no"))
                          .arg(rxBufferedAtTxStart)
                          .arg(m_periodTimer && m_periodTimer->isActive() ? QStringLiteral("active") : QStringLiteral("stopped")));
        }

        // Anti-collisione: ferma l'ingresso audio durante TX per evitare
        // che il decoder processi il nostro stesso tono come segnale ricevuto.
        // FT2-Link deve restare pronto a ricevere ACK immediati: sospendere
        // l'input su macOS forza restart CoreAudio e puo' tagliare HELLO_ACK.
        if (m_soundInput
            && (!m_ft2LinkTxActive || satelliteHalfDuplex)
            && !(m_mode == "FT2" && m_asyncTxEnabled)) {
            pauseRxAudioForTx(QStringLiteral("TX (sync mode): ft2Link=%1 mode=%2")
                                  .arg(m_ft2LinkTxActive ? 1 : 0)
                                  .arg(m_mode));
        }

        if (!m_ft2LinkTxActive) {
            appendTxDecodeEntry(msg);
        }

        if (m_recordTxEnabled) {
            QString const txRecordPath = buildTxRecordingPath(txMode, false);
            saveTxRecordingAsync(txRecordPath, wave, 48000, QStringLiteral("TX"));
        }

        double const txPeriodSeconds = macPeriodMs / 1000.0;
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        // FT2-Link control frames are CRC-sensitive continuous bursts. On
        // macOS/BlackHole Qt can consume the QBuffer before CoreAudio has
        // actually emitted the tail, so keep TX open longer for this mode.
        qint64 const macTxTailGuardMs = m_ft2LinkTxActive ? qint64(1200) : qint64(300);
        m_txPlaybackHoldUntilMs = nowMs + txPlaybackMs + macTxTailGuardMs;
        m_txPlaybackHardDeadlineMs = nowMs + txPlaybackMs + 5000;
        m_txPlaybackReleasePending = false;
        m_txAudioRestartPending = false;
        if (m_txPcmBuffer) {
            m_txPcmBuffer->close();
            delete m_txPcmBuffer;
            m_txPcmBuffer = nullptr;
        }
        m_txPcmData = macPcm;
        m_txPcmBuffer = new QBuffer(this);
        m_txPcmBuffer->setData(m_txPcmData);
        m_txPcmBuffer->open(QIODevice::ReadOnly);
        m_txPcmBuffer->seek(0);
        QPointer<QBuffer> const bufferGuard(m_txPcmBuffer);

        bridgeLog(QStringLiteral("startTx(mac-pcm): playback hold %1 ms for %2 samples tail_guard_ms=%3")
                      .arg(txPlaybackMs + macTxTailGuardMs)
                      .arg(wave.size())
                      .arg(macTxTailGuardMs));
        bridgeLog(QStringLiteral("TX mac PCM payload mode=%1 msg=[%2] waveSamples=%3 peak=%4 payloadMs=%5 ft2Async=%6 alignSlot=%7 nominalLeadMs=%8 initialSilenceMs=%9 slotElapsedMs=%10 pcmBytes=%11 bufferSize=%12")
                      .arg(txMode,
                           msg.trimmed())
                      .arg(wave.size())
                      .arg(QString::number(txWavePeak(wave), 'f', 6))
                      .arg(txPayloadMs)
                      .arg(macFt2Async ? 1 : 0)
                      .arg(macAlignToSyncSlot ? 1 : 0)
                      .arg(macNominalLeadInMs)
                      .arg(macInitialSilenceMs)
                      .arg(macSlotElapsedMs)
                      .arg(m_txPcmData.size())
                      .arg(m_txPcmBuffer->size()));
        if (!validateFt8AutoTxAudioStart(txPlaybackMs)) return;
        m_soundOutput->restart(m_txPcmBuffer);
        QTimer::singleShot(txPlaybackMs + macTxTailGuardMs + 50, this,
                           [this, bufferGuard, txPlaybackMs, macTxTailGuardMs]() {
            if (!m_transmitting || m_tuning || bufferGuard != m_txPcmBuffer) {
                return;
            }
            bridgeLog(QStringLiteral("startTx(mac-pcm) completion timer: playback_ms=%1 tail_guard_ms=%2 bufPos=%3/%4")
                          .arg(txPlaybackMs)
                          .arg(macTxTailGuardMs)
                          .arg(m_txPcmBuffer ? m_txPcmBuffer->pos() : -1)
                          .arg(m_txPcmBuffer ? m_txPcmBuffer->size() : -1));
            finishModulatorIdlePlayback(QStringLiteral("mac-pcm"));
        });

        bridgeLog("startTx(mac-pcm): mode=" + txMode +
                  " msg=" + msg.trimmed() +
                  " dev=" + outDev.description() +
                  " fmt=" + audioFormatToString(outFmt) +
                  " channel=" + QString::number(static_cast<int>(outChannel)) +
                  " attn=" + QString::number(attenuationDb, 'f', 2) +
                  " gain=" + QString::number(txGainFromSlider(m_txOutputLevel), 'f', 4) +
                  " slot_elapsed=" + QString::number(macSlotElapsedMs) + "ms" +
                  " ft2_async=" + QString::number(macFt2Async ? 1 : 0) +
                  " align_slot=" + QString::number(macAlignToSyncSlot ? 1 : 0) +
                  " lead_in=" + QString::number(macNominalLeadInMs) + "ms" +
                  " initial_silence=" + QString::number(macInitialSilenceMs) + "ms" +
                  " period=" + QString::number(txPeriodSeconds, 'f', 3) + "s" +
                  " peak=" + QString::number(txWavePeak(wave), 'f', 6) +
                  " samples=" + QString::number(wave.size()) +
                  " pcm=" + QString::number(m_txPcmData.size()));

        // The one-shot click is consumed on this early-return path too;
        // subsequent automatic FT8 repeats must rendezvous with RX results.
        if (txMode == QStringLiteral("FT8") && m_explicitManualTxRequest == m_currentTx)
            m_explicitManualTxRequest = 0;
        emit statusMessage("TX: " + msg.trimmed());
        return;
    }
#endif

    QAudioDevice outDev;
    QAudioFormat outFmt;
    // Con la radio remota in uso la scheda audio locale non serve: il segnale
    // parte dalla forma d'onda, e un'uscita locale non configurata non deve
    // bloccare la trasmissione.
    if (!tciAudioTx && !m_decoPortUseRemote) {
        outDev = preparedDev;
        outFmt = preparedFmt;
        m_txPcmData = preparedPcm;
        if (!m_cachedTxOutputDeviceFound && !m_audioOutputDevice.trimmed().isEmpty()) {
            bridgeLog("startTx: requested output device not found, fallback to default: " +
                      outDev.description() +
                      " selected_id=" + audioDeviceIdSettingForLog(audioDeviceIdForSettings(outDev)) +
                      " requested=[" + m_audioOutputDevice + "] requested_id=" +
                      audioDeviceIdSettingForLog(m_audioOutputDeviceId));
            emit statusMessage(tr("Transmit audio device not found, using the default one: %1")
                           .arg(outDev.description()));
        }
        if (!outFmt.isValid() || m_txPcmData.isEmpty()) {
            bridgeLog("startTx: prepared PCM buffer invalid for " + audioFormatToString(outFmt));
            emit errorMessage(QStringLiteral("Audio TX: PCM preparato non valido | device=\"%1\" formato=%2 bytes=%3 canale=%4 msg=\"%5\"")
                                  .arg(outDev.description(),
                                       audioFormatToString(outFmt),
                                       QString::number(m_txPcmData.size()),
                                       QString::number(m_audioOutputChannel),
                                       msg.trimmed()));
            return;
        }
    } else {
        m_txPcmData.clear();
        bridgeLog(tciAudioTx
                      ? QStringLiteral("startTx: TCI audio path selected, skipping local TX audio device")
                      : QStringLiteral("startTx: remote radio selected, skipping local TX audio device"));
    }
    if (m_recordTxEnabled) {
        QString const txRecordPath = buildTxRecordingPath(txMode, false);
        saveTxRecordingAsync(txRecordPath, wave, 48000, QStringLiteral("TX"));
    }

    // === GitHub TxController: aggiorna contatori retry ===
    // Traccia quante volte abbiamo inviato lo stesso TX step senza risposta.
    // NOTE: spostato dopo il check su buildTxPcmBuffer/PCM vuoto (sopra) e
    // prima del PTT SU (sotto). Se la generazione del PCM fallisce, NON
    // vogliamo gonfiare m_txRetryCount e anticipare il retry-limit.
    if (!customAudioTxActive) {
        if (m_currentTx == m_lastNtx) {
            ++m_txRetryCount;
        } else {
            m_txRetryCount = 1;
            m_lastNtx = m_currentTx;
        }

        bool const txCarries73Payload =
            (m_currentTx == 5) || messageCarries73Payload(msg);
        bool const ft2DeferredSignoff =
            txCarries73Payload
            && (m_qsoProgress > 1)
            && (m_mode == QStringLiteral("FT2"))
            && (!m_quickQsoEnabled || !m_quickPeerSignaled)
            && !m_logAfterOwn73;
        bool const autoCqDeferredSignoff =
            m_autoCqRepeat
            && txCarries73Payload
            && (m_qsoProgress > 1)
            && (m_mode == QStringLiteral("FT4") || m_mode == QStringLiteral("FT8"))
            && !m_logAfterOwn73;
        if (ft2DeferredSignoff || autoCqDeferredSignoff) {
            m_ft2DeferredLogPending = true;
            capturePendingAutoLogSnapshot();
        }
    }

    // PTT SU — prima di avviare l'audio (come GitHub pttAssert()).
    // Con TCI-audio il modulatore deve essere armato prima del PTT, altrimenti
    // ExpertSDR riceve "trx:true" senza suffisso/payload TCI.
    m_activeTxNumber = m_currentTx;
    m_activeTxMessage = msg.trimmed();
    m_activeTxWasPureAutoCqCq = isAutoCqBurstPureCq(m_activeTxNumber,
                                                     m_activeTxMessage);
    applyConfiguredCatRigMode(QStringLiteral("startTx"));
    bool const satelliteHalfDuplex = ft2LinkSatelliteHalfDuplexOperationActive();
    bool const voxPtt = activeCatUsesVoxPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat);
    double const startTxDialHz = satelliteHalfDuplex
        ? static_cast<double>(m_ft2LinkSatelliteHalfDuplexTxDialHz)
        : catSplitTxPttDialFrequencyHz();
    bool const startTxCanPtt =
        activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend);
    bool const hamlibAsyncFakeSplitPtt =
        !tciAudioTx
        && !satelliteHalfDuplex
        && !voxPtt
        && startTxCanPtt
        && startTxDialHz > 0.0
        && isHamlibFamilyBackend(m_catBackend)
        && !useLegacyRigControlFallback(m_legacyBackend, m_catBackend);
    bool const cat4OmAsyncPtt =
        !tciAudioTx
        && !voxPtt
        && startTxCanPtt
        && m_catBackend == QStringLiteral("cat4om");
    if (!hamlibAsyncFakeSplitPtt && !satelliteHalfDuplex) {
        syncActiveCatTxSplitFrequency(QStringLiteral("startTx"));
    }
    int asyncPttAudioDelayMs = 0;
    if (satelliteHalfDuplex && m_hamlibCat && startTxCanPtt) {
        QElapsedTimer catTimer;
        catTimer.start();
        m_hamlibCat->setRigPtt(true);
        txTimelineLog(QStringLiteral("[TX-TL] ft2sat_ptt on=1 cat_ms=%1 rxDialHz=%2 txDialHz=%3")
                      .arg(catTimer.elapsed())
                      .arg(m_ft2LinkSatelliteHalfDuplexRxDialHz)
                      .arg(m_ft2LinkSatelliteHalfDuplexTxDialHz));
        bridgeLog(QStringLiteral("[FT2SAT] PTT ON after CAT settle"));
    } else if (!tciAudioTx && !voxPtt && activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
        QElapsedTimer catTimer;
        catTimer.start();
        bool asyncPtt = false;
        if (hamlibAsyncFakeSplitPtt || cat4OmAsyncPtt) {
            asyncPtt = activeCatSetTxPttAsync(this, m_hamlibCat, m_catBackend,
                                              true, startTxDialHz,
                                              m_omniRigCat, m_legacyBackend);
        }
        if (!asyncPtt) {
            activeCatSetTxPtt(this, m_hamlibCat, m_catBackend,
                              true, startTxDialHz,
                              m_omniRigCat, m_legacyBackend);
        }
        asyncPttAudioDelayMs = asyncPtt ? kLegacyBridgeAudioAfterAsyncPttDelayMs : 0;
        txTimelineLog(QStringLiteral("[TX-TL] start_tx_ptt on=1 async=%1 cat_ms=%2 audio_delay_ms=%3 backend=%4 txDialHz=%5 skip_presync=%6")
                      .arg(asyncPtt ? 1 : 0)
                      .arg(catTimer.elapsed())
                      .arg(asyncPttAudioDelayMs)
                      .arg(m_catBackend)
                      .arg(startTxDialHz, 0, 'f', 0)
                      .arg(hamlibAsyncFakeSplitPtt ? 1 : 0));
        bridgeLog(QStringLiteral("PTT ON via %1%2")
                      .arg(m_catBackend,
                           asyncPtt ? QStringLiteral(" (async fake-split)") : QString()));
    } else if (tciAudioTx) {
        bridgeLog(QStringLiteral("PTT ON deferred until TCI TX audio stream is armed"));
    } else if (voxPtt) {
        bridgeLog("PTT VOX: audio-only TX; no CAT/DTR/RTS command will be sent");
        emit statusMessage("TX VOX: audio verso " + (m_audioOutputDevice.trimmed().isEmpty()
                           ? QStringLiteral("device predefinito")
                           : m_audioOutputDevice));
    } else {
        bridgeLog("WARNING: PTT not available — backend=" + m_catBackend + " not connected. TX audio will play but radio stays in RX.");
        emit statusMessage(tr("PTT unavailable: check the CAT connection (%1)").arg(m_catBackend));
    }

    quint64 const txSerial = ++m_txPlaybackSerial;
    // 1.0.225 — set m_lastTransmittedMessage QUI (DOPO ensureTxAudioPrepared
    // OK + PTT successful), BUG #11 agente-tx. Pre-1.0.225 era settato a
    // riga 11506 PRIMA del prepare: prepare fail => m_lastTransmittedMessage
    // settato senza TX reale => completeFinishedSignoffIfReady false-positive.
    m_lastTransmittedMessage = msg.trimmed();
    if (txMode == QStringLiteral("FT8") && !m_tuning) {
        m_lastFt8MessageTxMs = QDateTime::currentMSecsSinceEpoch();
    }
    m_transmitting = true;
    emit transmittingChanged();
    applyTxAudioSchedulingBoost(QStringLiteral("tx"));
    suspendNonAudioTxWork(QStringLiteral("tx"));

    if (m_ft2LinkTxActive) {
        int rxBufferedAtTxStart = 0;
        {
            QMutexLocker locker(&m_audioBufferMutex);
            rxBufferedAtTxStart = m_audioBuffer.size();
        }
        bridgeLog(QStringLiteral("FT2-Link TX start: mode=%1 samples=%2 monitor=%3 soundInput=%4 tci=%5 rxSuspendedBefore=%6 rxBuffered=%7 periodTimer=%8")
                      .arg(txMode)
                      .arg(wave.size())
                      .arg(m_monitoring ? QStringLiteral("on") : QStringLiteral("off"))
                      .arg(m_soundInput ? QStringLiteral("alive") : QStringLiteral("null"))
                      .arg(tciAudioTx ? 1 : 0)
                      .arg(m_rxAudioSuspendedForTx ? QStringLiteral("yes") : QStringLiteral("no"))
                      .arg(rxBufferedAtTxStart)
                      .arg(m_periodTimer && m_periodTimer->isActive() ? QStringLiteral("active") : QStringLiteral("stopped")));
    }

    // Anti-collisione: durante TX sync non dobbiamo ricatturare il nostro
    // stesso audio sul device RX. FT2 async resta full-duplex.
    if (m_monitoring && m_soundInput
        && (!m_ft2LinkTxActive || satelliteHalfDuplex)
        && !(m_mode == "FT2" && m_asyncTxEnabled)) {
        pauseRxAudioForTx(QStringLiteral("TX (sync mode): ft2Link=%1 mode=%2")
                              .arg(m_ft2LinkTxActive ? 1 : 0)
                              .arg(m_mode));
    }

    if (!m_ft2LinkTxActive) {
        appendTxDecodeEntry(msg);
    }

    // Con la radio remota in uso l'audio non va alla scheda di questo computer:
    // va alla radio, che sta da un'altra parte. Tutto il resto della
    // trasmissione — sequencer, log, fine dello slot — resta com'era.
    if (m_decoPortUseRemote) {
        double waveSeconds = 0.0;
        if (!decoPortSendTxWave(wave, &waveSeconds)) {
            m_transmitting = false;
            emit transmittingChanged();
            m_txPcmData.clear();
            m_activeTxNumber = 0;
            m_activeTxMessage.clear();
            bridgeLog(QStringLiteral("startTx: DecoPort transmit refused"));
            return;
        }
        quint64 const serial = m_txPlaybackSerial;
        int const holdMs = static_cast<int>(waveSeconds * 1000.0)
                           + qMax(200, static_cast<int>(
                                 remoteLink()->txAudioLeadMs()))
                           + 250;
        QTimer::singleShot(holdMs, this, [this, serial]() {
            if (serial != m_txPlaybackSerial || !m_transmitting)
                return;
            decoPortStopTx(QStringLiteral("waveform finished"));
            completeTxPlayback(QStringLiteral("decoport-audio-done"));
        });
        return;
    }

    if (tciAudioTx) {
        unsigned symbolsLength = 79;
        double framesPerSymbol = 1920.0;
        double toneSpacing = -3.0;
        bool const ft2LinkTci = txMode == QStringLiteral("FT2LINK");
        QString const metadataMode = txMode.trimmed().toUpper();
        if (ft2LinkTci) {
            symbolsLength = static_cast<unsigned> (
                qMax<qint64> (1, qCeil (static_cast<double> (wave.size ()) / 4.0)));
            framesPerSymbol = 1.0;
            toneSpacing = -6.0;
        } else if (metadataMode == QStringLiteral("FT2")) {
            symbolsLength = 103;
            framesPerSymbol = 288.0;
            toneSpacing = -2.0;
        } else if (metadataMode == QStringLiteral("FT4")) {
            symbolsLength = 105;
            framesPerSymbol = 576.0;
            toneSpacing = -2.0;
        } else if (metadataMode == QStringLiteral("WSPR")) {
            symbolsLength = 162;
            framesPerSymbol = 8192.0;
            toneSpacing = -2.0;
        } else if (metadataMode == QStringLiteral("Q65")
                   || metadataMode.startsWith(QStringLiteral("Q65-"))) {
            symbolsLength = 85;
            framesPerSymbol = static_cast<double>(q65SamplesPerSymbolAt48k(metadataMode)) / 4.0;
            toneSpacing = -2.0;
        } else if (metadataMode == QStringLiteral("FST4")
                   || metadataMode == QStringLiteral("FST4W")
                   || metadataMode.startsWith(QStringLiteral("FST4-"))
                   || metadataMode.startsWith(QStringLiteral("FST4W-"))) {
            symbolsLength = 160;
            framesPerSymbol = static_cast<double>(
                                  fst4SamplesPerSymbolAt48k(metadataMode,
                                                             effectivePeriodMsForMode(metadataMode) / 1000.0))
                              / 4.0;
            toneSpacing = -2.0;
        }

        double const periodSeconds = ft2LinkTci
            ? qMax(0.1, static_cast<double> (wave.size ()) / 48000.0 + 0.3)
            : qMax(0.1, effectivePeriodMsForMode(m_mode) / 1000.0);
        if (!startTciTxAudioStream(wave,
                                   txMode,
                                   symbolsLength,
                                   framesPerSymbol,
                                   static_cast<double>(txAudioFrequency),
                                   toneSpacing,
                                   true,
                                   periodSeconds)) {
            if (activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend))
                activeCatSetPtt(this, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
            m_transmitting = false;
            emit transmittingChanged();
            m_txPcmData.clear();
            m_activeTxNumber = 0;
            m_activeTxMessage.clear();
            restoreTxAudioSchedulingBoost(QStringLiteral("tci-tx-audio-start-failed"));
            resumeRxAudioAfterTx(QStringLiteral("tci-tx-audio-start-failed"));
            resumeNonAudioTxWork(QStringLiteral("tci-tx-audio-start-failed"));
            emit errorMessage(QStringLiteral(
                "Audio TX TCI: impossibile avviare lo stream audio verso il transceiver | "
                "backend=%1 catConnected=%2 modo=%3 freqAudio=%4 Hz msg=\"%5\"")
                .arg(m_catBackend,
                     m_catConnected ? QStringLiteral("true") : QStringLiteral("false"),
                     txMode,
                     QString::number(txAudioFrequency),
                     msg.trimmed()));
            return;
        }

        if (activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
            activeCatSetPtt(this, m_hamlibCat, m_catBackend, true, m_omniRigCat, m_legacyBackend);
            bridgeLog(QStringLiteral("PTT ON via tci after TX audio stream arm"));
        }

        qint64 const txPlaybackMs = qMax<qint64>(
            1, qRound64(static_cast<double>(wave.size()) * 1000.0 / 48000.0));
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        m_txPlaybackHoldUntilMs = nowMs + txPlaybackMs + 300;
        m_txPlaybackHardDeadlineMs = nowMs + txPlaybackMs + 5000;
        m_txPlaybackReleasePending = false;
        m_txAudioRestartPending = false;
        scheduleSyncTxBoundaryStop(QStringLiteral("tci"), txSerial);
        QTimer::singleShot(static_cast<int>(qBound<qint64>(qint64(1000),
                                                           txPlaybackMs + 1200,
                                                           qMax<qint64>(qint64(25000), txPlaybackMs + 5000))),
                           this,
                           [this]() {
                               if (m_transmitting && usingTciAudioInput())
                                   completeTxPlayback(QStringLiteral("tci-watchdog"));
                           });
        int const tciPeriodMs = effectivePeriodMsForMode(txMode);
        int const tciSlotElapsedMs = tciPeriodMs > 0
            ? static_cast<int>(correctedUtcEpochMs() % static_cast<qint64>(tciPeriodMs))
            : -1;
        int const tciLeadInMs = txSyncLeadInMsForMode(txMode);
        bridgeLog(QStringLiteral("startTx TCI audio: mode=%1 msg=%2 samples=%3 hold=%4ms slot_elapsed=%5ms lead_in=%6ms peak=%7 gain=%8")
                      .arg(txMode, msg.trimmed())
                      .arg(wave.size())
                      .arg(txPlaybackMs + 300)
                      .arg(tciSlotElapsedMs)
                      .arg(tciLeadInMs)
                      .arg(txWavePeak(wave), 0, 'f', 6)
                      .arg(txGainFromSlider(m_txOutputLevel), 0, 'f', 4));
        emit statusMessage("TX TCI: " + msg.trimmed());
        return;
    }

    // Pulizia risorse TX precedenti
    if (m_txAudioSink || m_txPcmBuffer) {
#if defined(Q_OS_WIN)
        if (m_bridgeAudioLegacyTxActive && m_soundOutput) {
            m_soundOutput->finishPlayback();
            m_bridgeAudioLegacyTxActive = false;
        }
#endif
        retireAudioSink(m_txAudioSink, m_txPcmBuffer, QStringLiteral("startTx-cleanup"));
        m_txAudioSink = nullptr;
        m_txPcmBuffer = nullptr;
    }

    // Su macOS CoreAudio è più sensibile al timing: lascia il tempo al route
    // di andare in ActiveState prima di spingere il payload.
    int ptt1DelayMs = 0;
#if defined(Q_OS_MAC)
    ptt1DelayMs = 300;
#elif defined(Q_OS_LINUX)
    ptt1DelayMs = (txMode == "FT2" || txMode == "FT2LINK") ? 40 : 80;
#else
    ptt1DelayMs = (txMode == "FT2" || txMode == "FT2LINK") ? 20 : 0;
#endif
    ptt1DelayMs += asyncPttAudioDelayMs;

    // Lambda che avvia effettivamente QAudioSink dopo il delay ptt1
    auto launchAudio = [this, outDev, outFmt, msg, wave, txSerial, txMode]() {
        if (!m_transmitting) {
            bridgeLog("startTx launchAudio skipped: TX no longer active");
            return;
        }

#if defined(Q_OS_WIN)
        {
        if (!m_soundOutput) {
            bridgeLog(QStringLiteral("startTx SoundOutput path unavailable: soundOutput missing"));
            emit errorMessage(QStringLiteral("Audio TX: output engine non disponibile"));
            completeTxPlayback(QStringLiteral("sound-output-missing"), true);
            return;
        }

        m_txPcmBuffer = new QBuffer(this);
        m_txPcmBuffer->setData(m_txPcmData);
        m_txPcmBuffer->open(QIODevice::ReadOnly);
        m_txPcmBuffer->seek(0);
        int syncElapsedMs = -1;
        qint64 const syncOffsetBytes =
            syncTxPcmStartOffsetBytes(outFmt, m_txPcmBuffer->size(), &syncElapsedMs);
        int const leadInMs = txSyncLeadInMsForMode(txMode);
        int const payloadDelayMs =
            (shouldAlignTxAudioToCurrentSyncSlot()
             && syncElapsedMs >= 0
             && leadInMs > 0
             && syncElapsedMs < leadInMs)
            ? leadInMs - syncElapsedMs
            : 0;
        if (syncOffsetBytes > 0) {
            m_txPcmBuffer->seek(syncOffsetBytes);
            bridgeLog(QStringLiteral("startTx SoundOutput sync PCM offset: mode=%1 elapsed=%2ms lead_in=%3ms offset=%4/%5 bytes")
	                          .arg(txMode)
                          .arg(syncElapsedMs)
                          .arg(leadInMs)
                          .arg(syncOffsetBytes)
                          .arg(m_txPcmBuffer->size()));
        }

        qint64 const remainingBytes = qMax<qint64>(0, m_txPcmBuffer->size() - m_txPcmBuffer->pos());
        qint64 const frameBytes = qMax<qint64>(1, outFmt.bytesPerFrame());
        qint64 const bytesPerSecond =
            qMax<qint64>(1, static_cast<qint64>(outFmt.sampleRate()) * frameBytes);
        qint64 const expectedUs =
            qMax<qint64>(1, ((remainingBytes * 1000000LL) + bytesPerSecond - 1) / bytesPerSecond);
        qint64 const txPlaybackMs = (expectedUs + 999) / 1000;
        QPointer<QBuffer> const bufferGuard(m_txPcmBuffer);
        QPointer<SoundOutput> const soundOutputGuard(m_soundOutput);
        qreal const attenuationDb = txAttenuationFromSlider(m_txOutputLevel);
        qreal const effectiveGain = txGainFromSlider(m_txOutputLevel);
        double const wavePeak = txWavePeak(wave);
        qsizetype const txBufferBytes =
            txAudioBufferBytesForMode(outFmt, txMode, false,
                                      m_lowCpuModeEnabled || cpuPressureActive());
        qint64 const pcmBytes = m_txPcmData.size();

        auto startSoundOutput = [this, txSerial, expectedUs, txPlaybackMs, syncElapsedMs,
                                 leadInMs, payloadDelayMs, pcmBytes, txBufferBytes,
                                 wavePeak, effectiveGain, attenuationDb, outDev, outFmt,
                                 msg, wave, txMode, bufferGuard, soundOutputGuard]() {
            if (!m_transmitting || m_tuning || bufferGuard != m_txPcmBuffer || !soundOutputGuard) {
                bridgeLog(QStringLiteral("startTx SoundOutput start skipped: active=%1 tune=%2 buffer_ok=%3 sound_ok=%4")
                              .arg(m_transmitting ? 1 : 0)
                              .arg(m_tuning ? 1 : 0)
                              .arg(bufferGuard == m_txPcmBuffer ? 1 : 0)
                              .arg(soundOutputGuard ? 1 : 0));
                return;
            }

            qint64 const audioStartWallMs = QDateTime::currentMSecsSinceEpoch();
            // FT2-Link frames are continuous data bursts: losing even the last
            // few symbols invalidates the frame CRC. CoreAudio/BlackHole can
            // still have queued audio after Qt has consumed the QBuffer, so use
            // a larger tail guard only for FT2-Link.
            qint64 const txTailGuardMs = m_ft2LinkTxActive ? qint64(1200) : qint64(300);
            m_txPlaybackHoldUntilMs = audioStartWallMs + txPlaybackMs + txTailGuardMs;
            m_txPlaybackHardDeadlineMs = audioStartWallMs + txPlaybackMs + 5000;
            m_txPlaybackReleasePending = false;
            m_txAudioRestartPending = true;
            m_bridgeAudioLegacyTxActive = true;

            soundOutputGuard->setFormat(outDev, static_cast<unsigned>(qMax(1, outFmt.channelCount())), 49152);
            soundOutputGuard->setAttenuation(attenuationDb);
            resetTxAudioTelemetry(txSerial, audioStartWallMs, expectedUs,
                                  syncElapsedMs, leadInMs, payloadDelayMs,
                                  pcmBytes, static_cast<qsizetype>(txBufferBytes),
                                  wavePeak, effectiveGain);
            if (!validateFt8AutoTxAudioStart(txPlaybackMs)) return;
            soundOutputGuard->restart(m_txPcmBuffer);
            m_txAudioRestartPending = false;
            scheduleSyncTxBoundaryStop(QStringLiteral("sound-output"), txSerial);

            txTimelineLog(QStringLiteral("[TX-TL] sound_output_launch mode=%1 msg=[%2] samples=%3 pcm_bytes=%4 pos=%5/%6 dev=%7 fmt=%8 configured_buf=%9 channel=%10 tx_level=%11 gain=%12 peak=%13")
	                              .arg(txMode,
                                   msg.trimmed())
                              .arg(wave.size())
                              .arg(m_txPcmData.size())
                              .arg(m_txPcmBuffer ? m_txPcmBuffer->pos() : -1)
                              .arg(m_txPcmBuffer ? m_txPcmBuffer->size() : -1)
                              .arg(outDev.description(),
                                   audioFormatToString(outFmt))
                              .arg(static_cast<qlonglong>(txBufferBytes))
                              .arg(m_audioOutputChannel)
                              .arg(m_txOutputLevel, 0, 'f', 1)
                              .arg(effectiveGain, 0, 'f', 4)
                              .arg(wavePeak, 0, 'f', 6));

            QTimer::singleShot(txPlaybackMs + txTailGuardMs + 50, this,
                               [this, bufferGuard, txPlaybackMs, txTailGuardMs]() {
                if (!m_transmitting || m_tuning || bufferGuard != m_txPcmBuffer) {
                    return;
                }
                bridgeLog(QStringLiteral("startTx SoundOutput completion timer: playback_ms=%1 tail_guard_ms=%2 bufPos=%3/%4")
                              .arg(txPlaybackMs)
                              .arg(txTailGuardMs)
                              .arg(m_txPcmBuffer ? m_txPcmBuffer->pos() : -1)
                              .arg(m_txPcmBuffer ? m_txPcmBuffer->size() : -1));
                completeTxPlayback(QStringLiteral("sound-output"));
            });
        };

        if (payloadDelayMs > 0) {
            QTimer::singleShot(payloadDelayMs, this, startSoundOutput);
        } else {
            startSoundOutput();
        }
        return;
        }
#endif

        m_txPcmBuffer = new QBuffer(this);
        m_txPcmBuffer->setData(m_txPcmData);
        m_txPcmBuffer->open(QIODevice::ReadOnly);
        m_txPcmBuffer->seek(0);
        int syncElapsedMs = -1;
        qint64 const syncOffsetBytes =
            syncTxPcmStartOffsetBytes(outFmt, m_txPcmBuffer->size(), &syncElapsedMs);
        int const leadInMs = txSyncLeadInMsForMode(txMode);
        int const payloadDelayMs =
            (shouldAlignTxAudioToCurrentSyncSlot()
             && syncElapsedMs >= 0
             && leadInMs > 0
             && syncElapsedMs < leadInMs)
            ? leadInMs - syncElapsedMs
            : 0;
        if (syncOffsetBytes > 0) {
            m_txPcmBuffer->seek(syncOffsetBytes);
            bridgeLog(QStringLiteral("startTx sync PCM offset: mode=%1 elapsed=%2ms lead_in=%3ms offset=%4/%5 bytes")
	                          .arg(txMode)
                          .arg(syncElapsedMs)
                          .arg(leadInMs)
                          .arg(syncOffsetBytes)
                          .arg(m_txPcmBuffer->size()));
        }

        // 1.0.218 — [TX-TL] timeline diagnostic per facilitare la diagnosi
        // di stutter/duplicati/rallentamenti via loopback ALPHA-BRAVO. Ogni
        // step audio sink ha timestamp in ms con etichetta dedicata, cosi'
        // grep [TX-TL] mostra la latenza end-to-end.
        qint64 const sinkCreateStartMs = QDateTime::currentMSecsSinceEpoch();
        m_txAudioSink = new QAudioSink(outDev, outFmt, this);
        qint64 const sinkCreateEndMs = QDateTime::currentMSecsSinceEpoch();
        txTimelineLog(QStringLiteral("[TX-TL] sink_create dt=%1ms dev=%2")
                      .arg(sinkCreateEndMs - sinkCreateStartMs)
                      .arg(outDev.description().isEmpty()
                           ? QStringLiteral("<default>")
                           : outDev.description()));
        qreal const effectiveGain = txGainFromSlider(m_txOutputLevel);
        double const wavePeak = txWavePeak(wave);
        qint64 const pcmBytes = m_txPcmData.size();
        m_txAudioSink->setVolume(static_cast<float>(effectiveGain));
        qsizetype const txBufferBytes =
            txAudioBufferBytesForMode(outFmt, txMode, false,
                                      m_lowCpuModeEnabled || cpuPressureActive());
        if (txBufferBytes > 0) {
            m_txAudioSink->setBufferSize(txBufferBytes);
        }
        bridgeLog(QStringLiteral("startTx audio sink: %1 buffer=%2 channel=%3")
                      .arg(audioFormatToString(outFmt))
                      .arg(static_cast<qlonglong>(txBufferBytes))
                      .arg(m_audioOutputChannel));
        qint64 const remainingBytes = qMax<qint64>(0, m_txPcmBuffer->size() - m_txPcmBuffer->pos());
        qint64 const frameBytes = qMax<qint64>(1, outFmt.bytesPerFrame());
        qint64 const bytesPerSecond =
            qMax<qint64>(1, static_cast<qint64>(outFmt.sampleRate()) * frameBytes);
        qint64 const expectedUs = qMax<qint64>(
            1, ((remainingBytes * 1000000LL) + bytesPerSecond - 1) / bytesPerSecond);
        qint64 const txPlaybackMs = (expectedUs + 999) / 1000;
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        m_txPlaybackHoldUntilMs = nowMs + txPlaybackMs + 300;
        m_txPlaybackHardDeadlineMs = nowMs + txPlaybackMs + 5000;
        m_txPlaybackReleasePending = false;
        m_txAudioRestartPending = false;
        bridgeLog(QStringLiteral("startTx: playback hold %1 ms for %2 samples")
                      .arg(txPlaybackMs + 300)
                      .arg(wave.size()));

        QPointer<QAudioSink> const txSinkGuard(m_txAudioSink);
        QPointer<QBuffer> const txBufferGuard(m_txPcmBuffer);
        connect(m_txAudioSink, &QAudioSink::stateChanged, this, [this, txSinkGuard, txBufferGuard, txSerial](QAudio::State st) {
            if (!txSinkGuard
                || txSinkGuard != m_txAudioSink
                || txSerial != m_txPlaybackSerial) {
                return;
            }
            QString extra;
            extra = " err=" + QString::number((int)txSinkGuard->error())
                + " processed=" + QString::number(txSinkGuard->processedUSecs()) + "us";
            if (txBufferGuard && txBufferGuard == m_txPcmBuffer) {
                extra += " bufPos=" + QString::number(txBufferGuard->pos())
                    + "/" + QString::number(txBufferGuard->size());
            }
            bridgeLog("TX QAudioSink stateChanged: " + QString::number(static_cast<int>(st)) + extra);
            // 1.0.218 — [TX-TL] state transition con timestamp ms
            txTimelineLog(QStringLiteral("[TX-TL] sink_state st=%1%2")
                          .arg(static_cast<int>(st)).arg(extra));
            if (st == QAudio::IdleState) {
                bool const bufferIncomplete =
                    txBufferGuard && txBufferGuard == m_txPcmBuffer && !txBufferGuard->atEnd();
                if (bufferIncomplete) {
                    ++m_txAudioTelemetryIdleCount;
                    if (txSinkGuard->error() == QAudio::UnderrunError) {
                        ++m_txAudioTelemetryUnderrunCount;
                    }
                    qint64 const pos = txBufferGuard->pos();
                    qint64 const size = txBufferGuard->size();
                    m_txPlaybackHoldUntilMs = qMax(m_txPlaybackHoldUntilMs,
                                                   QDateTime::currentMSecsSinceEpoch() + 300);
                    bridgeLog(QStringLiteral("TX QAudioSink partial idle: buffer incomplete pos=%1/%2, no restart; telemetry only")
                                  .arg(pos)
                                  .arg(size));
                    QTimer::singleShot(250, this, [this, txSinkGuard, txBufferGuard, txSerial]() {
                        if (m_transmitting
                            && txSinkGuard
                            && txSinkGuard == m_txAudioSink
                            && txBufferGuard
                            && txBufferGuard == m_txPcmBuffer
                            && txSerial == m_txPlaybackSerial
                            && !txBufferGuard->atEnd()) {
                            completeTxPlayback(QStringLiteral("partial-idle"));
                        }
                    });
                    return;
                }
                // GitHub ptt0Timer: 200ms delay prima di abbassare PTT.
                QTimer::singleShot(200, this, [this, txSinkGuard, txSerial]() {
                    if (m_transmitting
                        && txSinkGuard
                        && txSinkGuard == m_txAudioSink
                        && txSerial == m_txPlaybackSerial) {
                        completeTxPlayback(QStringLiteral("idle"));
                    }
                });
            } else if (st == QAudio::StoppedState
                       && txSinkGuard->error() != QAudio::NoError
                       && m_transmitting) {
                ++m_txAudioTelemetryErrorCount;
                if (txSinkGuard->error() == QAudio::UnderrunError) {
                    ++m_txAudioTelemetryUnderrunCount;
                }
                completeTxPlayback(QStringLiteral("audio-error"), true);
            }
        }, Qt::QueuedConnection);

        auto startSink = [this, txSerial, expectedUs, txPlaybackMs, syncElapsedMs,
                          leadInMs, payloadDelayMs, pcmBytes, txBufferBytes,
                          wavePeak, effectiveGain, outDev, outFmt, msg, wave, txMode]() {
            if (!m_transmitting || m_tuning || !m_txAudioSink || !m_txPcmBuffer) {
                bridgeLog(QStringLiteral("startTx audio sink start skipped: TX inactive or sink missing"));
                return;
            }

            qint64 const audioStartWallMs = QDateTime::currentMSecsSinceEpoch();
            m_txPlaybackHoldUntilMs = audioStartWallMs + txPlaybackMs + 300;
            m_txPlaybackHardDeadlineMs = audioStartWallMs + txPlaybackMs + 5000;
            m_txPlaybackReleasePending = false;
            m_txAudioRestartPending = false;

            resetTxAudioTelemetry(txSerial, audioStartWallMs, expectedUs,
                                  syncElapsedMs, leadInMs, payloadDelayMs,
                                  pcmBytes, m_txAudioSink->bufferSize(),
                                  wavePeak, effectiveGain);
            if (!validateFt8AutoTxAudioStart(txPlaybackMs)) return;
            m_txAudioSink->start(m_txPcmBuffer);
            scheduleTxAudioTelemetryProbe(txSerial);
            scheduleSyncTxBoundaryStop(QStringLiteral("pcm"), txSerial);

            QPointer<QAudioSink> const sinkGuard(m_txAudioSink);
            QPointer<QBuffer> const bufferGuard(m_txPcmBuffer);
            QTimer::singleShot(5200, this, [this, sinkGuard, bufferGuard, audioStartWallMs, expectedUs]() {
                if (!m_transmitting || m_tuning || !sinkGuard || sinkGuard != m_txAudioSink) {
                    return;
                }

                qint64 const wallUs =
                    qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - audioStartWallMs) * 1000LL;
                qint64 const processedUs = sinkGuard->processedUSecs();
                qint64 const lagUs = wallUs - processedUs;
                if (lagUs > 350000) {
                    bridgeLog(QStringLiteral("TX audio lag warning: lag=%1ms processed=%2ms wall=%3ms expected=%4ms bufPos=%5/%6")
                                  .arg(lagUs / 1000)
                                  .arg(processedUs / 1000)
                                  .arg(wallUs / 1000)
                                  .arg(expectedUs / 1000)
                                  .arg(bufferGuard ? bufferGuard->pos() : -1)
                                  .arg(bufferGuard ? bufferGuard->size() : -1));
                }
            });

            int const watchdogMs = qBound(1500,
                                          static_cast<int>((expectedUs + 999) / 1000) + 1500,
                                          25000);
            QTimer::singleShot(watchdogMs, this, [this, sinkGuard, bufferGuard, expectedUs]() {
                if (!m_transmitting || m_tuning || !sinkGuard || sinkGuard != m_txAudioSink) {
                    return;
                }

                qint64 const processedUs = sinkGuard->processedUSecs();
                qint64 const bufferPos = bufferGuard ? bufferGuard->pos() : -1;
                qint64 const bufferSize = bufferGuard ? bufferGuard->size() : -1;
                bool const consumed =
                    !bufferGuard ||
                    bufferGuard->atEnd() ||
                    processedUs >= qMax<qint64>(0, expectedUs - 250000);

                if (!consumed) {
                    bridgeLog("TX QAudioSink watchdog: still active, buffer not consumed processed=" +
                              QString::number(processedUs) + "us expected=" +
                              QString::number(expectedUs) + "us bufPos=" +
                              QString::number(bufferPos) + "/" + QString::number(bufferSize));
                    return;
                }

                bridgeLog("TX QAudioSink watchdog forcing completion: processed=" +
                          QString::number(processedUs) + "us expected=" +
                          QString::number(expectedUs) + "us bufPos=" +
                          QString::number(bufferPos) + "/" + QString::number(bufferSize));
                completeTxPlayback(QStringLiteral("watchdog"));
            });

            bridgeLog("startTx launchAudio: mode=" + txMode + " msg=" + msg.trimmed() +
                      " samples=" + QString::number(wave.size()) +
                      " pcm_bytes=" + QString::number(m_txPcmData.size()) +
                      " pcm_pos=" + QString::number(m_txPcmBuffer ? m_txPcmBuffer->pos() : -1) +
                      " slot_elapsed=" + QString::number(syncElapsedMs) + "ms" +
                      " lead_in=" + QString::number(leadInMs) + "ms" +
                      " payload_delay=" + QString::number(payloadDelayMs) + "ms" +
                      " dev=" + outDev.description() +
                      " fmt=" + audioFormatToString(outFmt) +
                      " buf=" + QString::number(m_txAudioSink->bufferSize()) +
                      " configured_buf=" + QString::number(static_cast<qlonglong>(txBufferBytes)) +
                      " out_channel=" + QString::number(m_audioOutputChannel) +
                      " tx_level=" + QString::number(m_txOutputLevel, 'f', 1) +
                      " peak=" + QString::number(wavePeak, 'f', 6) +
                      " gain=" + QString::number(effectiveGain, 'f', 4) +
                      " TX=" + QString::number(m_currentTx) +
                      " retry=" + QString::number(m_txRetryCount) +
                      " nTx73=" + QString::number(m_nTx73) +
                      " sink_state=" + QString::number(static_cast<int>(m_txAudioSink->state())) +
                      " sink_err=" + QString::number(static_cast<int>(m_txAudioSink->error())));
        };

        if (payloadDelayMs > 0) {
            bridgeLog(QStringLiteral("startTx payload lead-in delay: mode=%1 elapsed=%2ms lead_in=%3ms delay=%4ms")
                          .arg(txMode)
                          .arg(syncElapsedMs)
                          .arg(leadInMs)
                          .arg(payloadDelayMs));
            QTimer::singleShot(payloadDelayMs, this, startSink);
        } else {
            startSink();
        }
    };

    if (ptt1DelayMs > 0) {
        bridgeLog("startTx ptt1 delay " + QString::number(ptt1DelayMs) + "ms");
        QTimer::singleShot(ptt1DelayMs, this, launchAudio);
    } else {
        launchAudio();
    }

    // The request has now passed all timing/payload guards and is being
    // committed to a real TX path.  A delayed callback may not reuse this
    // manual authorisation for a later slot.
    if (m_explicitManualTxRequest == m_currentTx) {
        m_explicitManualTxRequest = 0;
    }

    emit statusMessage("TX: " + msg.trimmed());
}
