#include "DecodiumBridge.h"
#include "DecoPortRigDriver.h"
#include "src/rtl/RtlSdrAudioOutput.h"
#include <QPointer>
#include <QTimer>

void decodiumBridgeSharedActiveCatSetPtt(QObject const*, DecodiumTransceiverManager*, const QString&, bool, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
void decodiumBridgeAudioLevelLog(const QString&);
#define activeCatSetPtt decodiumBridgeSharedActiveCatSetPtt
#define bridgeLog decodiumBridgeAudioLevelLog

void DecodiumBridge::decoPortKeyLocalRig(bool on)
{
    // Remote clients must still have a real CAT radio, and may not take
    // ownership of a local RTTY audio session.
    if (on && m_rttyTxActive)
        return;
    keySharedAudioTransmitter(on, false);
}

void DecodiumBridge::keySharedAudioTransmitter(bool on, bool allowWithoutCat)
{
    bool const haveOwnRig = m_decoPortRig && m_decoPortRig->isOpen();
    if (on && ((!allowWithoutCat && !m_catConnected && !haveOwnRig)
               || m_transmitting || m_tuning || sstvTxActive())) {
        bridgeLog(QStringLiteral("DecoPort remote PTT refused: cat=%1 tx=%2 tune=%3")
                      .arg(m_catConnected ? 1 : 0).arg(m_transmitting ? 1 : 0)
                      .arg(m_tuning ? 1 : 0));
        return;
    }
    if (m_decoPortRemoteKeyed == on)
        return;

    if (!m_decoPortTxGuard) {
        m_decoPortTxGuard = new QTimer(this);
        m_decoPortTxGuard->setSingleShot(true);
        m_decoPortTxGuard->setInterval(3000);
        connect(m_decoPortTxGuard, &QTimer::timeout, this, [this]() {
            if (!m_decoPortRemoteKeyed)
                return;
            bridgeLog(QStringLiteral("DecoPort remote PTT dropped: no transmit audio for 3s"));
            keySharedAudioTransmitter(false, false);
        });
    }

    // L'RTTY locale passa dal CAT principale quando c'e': la radio propria del
    // gateway DecoPort puo' essere aperta su un'altra porta (sulla FT-991 la
    // COM "Standard", che i comandi CAT non li ascolta), e il PTT spariva li'
    // mentre l'audio AFSK usciva regolarmente. Il rilascio torna sulla stessa
    // porta dell'aggancio.
    if (on)
        m_sharedPttViaOwnRig = haveOwnRig && !(allowWithoutCat && m_catConnected);
    m_decoPortRemoteKeyed = on;
    if (m_sharedPttViaOwnRig && m_decoPortRig && m_decoPortRig->isOpen())
        m_decoPortRig->setPtt(on);
    else if (m_catConnected)
        activeCatSetPtt(this, m_hamlibCat, m_catBackend, on,
                        m_omniRigCat, m_legacyBackend);
    if (on)
        m_decoPortTxGuard->start();
    else
        m_decoPortTxGuard->stop();
    if (!on && m_rttyTxActive) {
        m_rttyTxActive = false;
        // Stop/abort/watchdog must also silence a VOX radio and discard any
        // queued samples. Stopping CAT alone cannot end an audio-only TX.
        if (m_decoPortTxOut) {
            QPointer<RtlSdrAudioOutput> out(m_decoPortTxOut);
            QMetaObject::invokeMethod(m_decoPortTxOut, [out]() {
                if (out) out->stop(QStringLiteral("rtty-tx-ended"));
            }, Qt::QueuedConnection);
        }
        m_decoPortTxOutRate = 0;
    }
    bridgeLog(QStringLiteral("DecoPort remote PTT %1").arg(on ? "up" : "down"));
}

#undef activeCatSetPtt
#undef bridgeLog
