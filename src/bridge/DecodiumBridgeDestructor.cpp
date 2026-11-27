#include "DecodiumBridge.h"
#include "Audio/soundout.h"
#include "Modulator/Modulator.hpp"
#include "DecoPortRigDriver.h"
#include "DecodiumDecoPortGateway.h"
#include "src/rtl/RtlSdrAudioOutput.h"
#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvRxRuntime.h"
#include "src/sstv/integration/SstvTxCoordinator.h"
#endif
#include <QAudioSink>
#include <QBuffer>
#include <QFutureWatcher>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
void decodiumBridgeRetireAudioSink(QAudioSink*, QBuffer*, const QString&);
void decodiumBridgeSharedActiveCatSetPtt(QObject const*, DecodiumTransceiverManager*, QString const&, bool, DecodiumOmniRigManager*, DecodiumLegacyBackend*);

DecodiumBridge::~DecodiumBridge()
{
    ssbStop();
    beginDecodeCallbackShutdown();
    // Close the SSTV queue and join its worker before any Decodium RX source
    // is torn down. The runtime object itself stays alive until member
    // destruction, so a callback already in flight can only observe Shutdown.
    shutdownSstvRuntime();
    if (m_adifImportWatcher) {
        m_adifImportWatcher->disconnect(this);
        m_adifImportWatcher->waitForFinished();
        delete m_adifImportWatcher;
        m_adifImportWatcher = nullptr;
    }
    if (m_confirmedAdifImportWatcher) {
        m_confirmedAdifImportWatcher->disconnect(this);
        m_confirmedAdifImportWatcher->waitForFinished();
        delete m_confirmedAdifImportWatcher;
        m_confirmedAdifImportWatcher = nullptr;
    }
    // 1.0.238 (Phase 5.2 perf roadmap): ferma il worker di persistenza PRIMA
    // di teardown decoder/audio. Flush sincrono delle entry pendenti +
    // chiusura connessione SQLite "decode_history_worker" sul thread giusto.
    stopPersistenceWorker();
    // 1.0.179 — Smooth Decode Flow: ferma drain timer e svuota coda prima
    // di teardown per evitare timer callback su this in distruzione.
    if (m_decodeReleaseTimer) m_decodeReleaseTimer->stop();
    m_pendingDecodeReleaseQueue.clear();
    stopRx();
    teardownAudioCapture();
    // Ferma TX/Tune prima di distruggere tutto
    if (m_txAudioSink || m_txPcmBuffer) {
        decodiumBridgeRetireAudioSink(m_txAudioSink, m_txPcmBuffer, QStringLiteral("bridge-dtor"));
        m_txAudioSink = nullptr;
        m_txPcmBuffer = nullptr;
    }
    if (m_modulator && m_modulator->isActive()) m_modulator->stop(true);
    if (m_soundOutput) m_soundOutput->stop();
    auto safeQuitThread = [](QThread *t, const char *name) {
        if (!t) return;
        t->requestInterruption();
        t->quit();
        if (!t->wait(15000)) {
            qWarning("~DecodiumBridge: %s did not finish in 15s, terminating", name);
            t->terminate();
            if (!t->wait(3000)) {
                qWarning("~DecodiumBridge: %s still running after terminate; detaching to avoid QThread destructor abort", name);
                t->setParent(nullptr);
            }
        }
    };
    safeQuitThread(m_workerThread,       "ft8");
    safeQuitThread(m_workerThreadFt2,    "ft2");
    safeQuitThread(m_workerThreadFt4,    "ft4");
    safeQuitThread(m_workerThreadQ65,    "q65");
    safeQuitThread(m_workerThreadMsk,    "msk");
    safeQuitThread(m_workerThreadWspr,   "wspr");
    safeQuitThread(m_workerThreadLegacyJt, "legacyJt");
    safeQuitThread(m_workerThreadFst4,   "fst4");
    safeQuitThread(m_soundInputThread,   "soundInput");
    safeQuitThread(m_rtlSdrAudioThread,  "rtlSdrAudio");
    m_rtlSdrAudioOutput = nullptr;
    if (m_decoPortMonitorOut && m_decoPortMonitorThread
        && m_decoPortMonitorThread->isRunning()) {
        QMetaObject::invokeMethod(m_decoPortMonitorOut,
                                  &RtlSdrAudioOutput::shutdown,
                                  Qt::BlockingQueuedConnection);
    }
    safeQuitThread(m_decoPortMonitorThread, "decoPortMonitor");
    m_decoPortMonitorOut = nullptr;
    if (m_decoPortRemoteKeyed) {
        decodiumBridgeSharedActiveCatSetPtt(this, m_hamlibCat, m_catBackend, false,
                        m_omniRigCat, m_legacyBackend);
        m_decoPortRemoteKeyed = false;
    }
    if (m_decoPortTxOut && m_decoPortTxOutThread && m_decoPortTxOutThread->isRunning()) {
        QMetaObject::invokeMethod(m_decoPortTxOut, &RtlSdrAudioOutput::shutdown,
                                  Qt::BlockingQueuedConnection);
    }
    safeQuitThread(m_decoPortTxOutThread, "decoPortTxOut");
    m_decoPortTxOut = nullptr;
    // DecoPortRigDriver is intentionally parentless so it can live on its
    // dedicated Hamlib thread.  Stop network callbacks before deleting it;
    // its destructor closes the rig on that worker and joins the thread.
    if (m_decoPortGateway)
        m_decoPortGateway->stop();
    delete m_decoPortRig;
    m_decoPortRig = nullptr;
}

