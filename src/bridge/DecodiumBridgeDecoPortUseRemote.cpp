#include "DecodiumBridge.h"
#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvRxRuntime.h"
#endif

void decodiumBridgeAudioLevelLog(const QString&);
RemoteRadioLink*& decodiumBridgeDecoPortCatLinkRef();
#define bridgeLog decodiumBridgeAudioLevelLog
#define g_decoPortCatLink (decodiumBridgeDecoPortCatLinkRef())

void DecodiumBridge::setDecoPortUseRemote(bool on)
{
    ssbStop();
    if (m_decoPortUseRemote == on)
        return;

    auto* link = remoteLink();
    if (on && (!link || !link->isLinked())) {
        bridgeLog(QStringLiteral("DecoPort remote source refused: not linked to a radio"));
        return;
    }

    // A legacy RX session captures from its own local audio backend.  Stop it
    // before selecting the remote source; after m_decoPortUseRemote is set,
    // startRx() will choose the bridge-owned decoder that consumes the injected
    // DecoPort samples instead.  Without this hand-off the remote monitor can
    // be audible while the decoder keeps receiving silence/local audio.
    bool const handoffFromLegacyRx = on
        && usingLegacyBackendForRx()
        && (m_monitoring || m_monitorRequested);
    if (handoffFromLegacyRx) {
        bridgeLog(QStringLiteral("DecoPort remote source: handing RX from legacy audio backend to remote PCM"));
        stopRx();
    }

    m_decoPortUseRemote = on;
    m_decoPortRemoteSamples = 0;

    if (on) {
        // Il collegamento all'audio si fa una volta sola: Qt::UniqueConnection
        // evita che riaccendere la modalita' lo dupli.
#if DECODIUM_HAS_SSTV
        // Bind the immutable, bounded DirectConnection before the ordinary
        // payload-bearing queued consumer below. This preserves producer-time
        // ordering even if the GUI event loop is temporarily backlogged.
        selectSstvRxSource(decodium::sstv::SstvAudioSourceKind::DecoPort,
                           currentSstvAudioStreamId(
                               decodium::sstv::SstvAudioSourceKind::DecoPort),
                           true);
#endif
        connect(link, &RemoteRadioLink::rxAudio,
                this, &DecodiumBridge::onDecoPortRxAudio,
                static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));
        connect(link, &RemoteRadioLink::stateChanged,
                this, &DecodiumBridge::onDecoPortRemoteState,
                static_cast<Qt::ConnectionType>(Qt::UniqueConnection));
        // Da qui in avanti la radio dell'applicazione e' quella remota, a tutti
        // gli effetti: non una frequenza copiata in una casella, ma il backend
        // CAT da cui passano frequenza, modo e PTT. Il backend precedente si
        // mette da parte e non si tocca il file delle impostazioni: quando si
        // smette, si torna esattamente com'era.
        m_catBackendBeforeDecoPort   = m_catBackend;
        m_catConnectedBeforeDecoPort = m_catConnected;
        m_catRigNameBeforeDecoPort   = m_catRigName;
        m_catModeBeforeDecoPort      = m_catMode;
        g_decoPortCatLink = link;
        m_catBackend  = QStringLiteral("decoport");
        m_catConnected = link->isLinked();
        m_catRigName   = link->rigLabel();
        m_catMode      = link->modeName();
        emit catBackendChanged();
        emit catConnectedChanged();
        emit catRigNameChanged();
        emit catModeChanged();
        // Il sink e' il punto d'ingresso dell'audio remoto: che il monitor sia
        // acceso, spento o delegato al backend legacy, deve esistere.
        ensureAudioSink();
        // Riempire il buffer non basta: se il monitor e' spento, onPeriodTimer
        // esce alla prima riga e nessuno guarda mai quei campioni. Quindi si
        // avvia la ricezione per intero — decoder, timer di periodo, spettro —
        // e solo dopo si lascia andare la scheda locale, che startRx() apre in
        // fondo e a noi non serve.
        if (!m_monitoring) {
            bridgeLog(QStringLiteral("DecoPort remote source: starting RX (monitor was off)"));
            startRx();
        }
        stopAudioCapture();
        // SSB usa il modo radio configurato; CW deve restare CW anche sul
        // gateway remoto, mentre i modi digitali usano DIGU per il codec audio.
        const QString remoteRigMode = m_mode == QStringLiteral("SSB") ? m_ssbRadioMode
            : (m_mode == QStringLiteral("CW") ? QStringLiteral("CW") : QStringLiteral("DIGU"));
        link->setModeName(remoteRigMode);
        onDecoPortRemoteState();
        setDecoPortMonitor(true);
        emit statusMessage(tr("Using the remote radio %1 — decoding its audio")
                               .arg(link->rigLabel()));
        bridgeLog(QStringLiteral("DecoPort remote source ON: local sound card released, "
                                 "audio now comes from %1").arg(link->rigLabel()));
    } else {
        if (link) {
            disconnect(link, &RemoteRadioLink::rxAudio, this, &DecodiumBridge::onDecoPortRxAudio);
            disconnect(link, &RemoteRadioLink::stateChanged, this, &DecodiumBridge::onDecoPortRemoteState);
        }
        g_decoPortCatLink = nullptr;
        if (!m_catBackendBeforeDecoPort.isEmpty()) {
            m_catBackend   = m_catBackendBeforeDecoPort;
            m_catConnected = m_catConnectedBeforeDecoPort;
            m_catRigName   = m_catRigNameBeforeDecoPort;
            m_catMode      = m_catModeBeforeDecoPort;
            m_catBackendBeforeDecoPort.clear();
            emit catBackendChanged();
            emit catConnectedChanged();
            emit catRigNameChanged();
            emit catModeChanged();
        }
        bridgeLog(QStringLiteral("DecoPort remote source OFF: back to the local sound card"));
        setDecoPortMonitor(false);
#if DECODIUM_HAS_SSTV
        const auto source = currentSstvAudioSourceKind();
        selectSstvRxSource(source, currentSstvAudioStreamId(source));
#endif
        if (m_monitoring)
            startAudioCapture(false);
    }
    if (!on)
        m_remoteIsDecolink = false;
    emit decoPortUseRemoteChanged();
}

#undef bridgeLog
#undef g_decoPortCatLink
