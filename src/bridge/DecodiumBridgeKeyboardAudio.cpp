#include "DecodiumBridge.h"
#include <QAudioDevice>

bool decodiumBridgeIsStreamingKeyboardMode(QString const& mode);
void decodiumBridgeKeyboardAudioLog(const QString& message);

// L'uscita audio condivisa dei modi da tastiera: la usano RTTY e JTTY, uno
// alla volta (e' il modo attivo a decidere chi), con lo stesso PTT e gli
// stessi ritegni verso DecoPort e i modi digitali.
bool DecodiumBridge::rttyCanTransmit()
{
    if (!decodiumBridgeIsStreamingKeyboardMode(m_mode) || m_transmitting || m_tuning
        || sstvTxActive() || (m_decoPortRemoteKeyed && !m_rttyTxActive)
        || m_decoPortUseRemote)
        return false;
    bool found = false;
    const QAudioDevice device = resolveTxOutputDevice(&found);
    return found && !device.isNull();
}

void DecodiumBridge::rttyAlzaPtt(bool on)
{
    if (!on) {
        if (m_rttyTxActive)
            keySharedAudioTransmitter(false, false);
        return;
    }
    if (!rttyCanTransmit() || m_rttyTxActive)
        return;
    keySharedAudioTransmitter(true, true);
    m_rttyTxActive = m_decoPortRemoteKeyed;
    decodiumBridgeKeyboardAudioLog(QStringLiteral("%1 TX start: active=%2 CAT=%3 (without CAT: audio/AFSK, operator VOX/manual PTT)")
                  .arg(m_mode).arg(m_rttyTxActive).arg(m_catConnected));
}

void DecodiumBridge::rttyMandaAudioTx(const QVector<short>& campioni12k)
{
    if (!m_rttyTxActive)
        return;
    if (!rttyCanTransmit()) {
        rttyAlzaPtt(false);
        return;
    }
    decoPortPlayTxAudio(campioni12k, true);
}

void DecodiumBridge::setRttyInAscolto(bool v)
{
    if (m_rttyInAscolto == v)
        return;
    m_rttyInAscolto = v;
    emit rttyInAscoltoChanged();
}

void DecodiumBridge::setCwInAscolto(bool v)
{
    if (m_cwInAscolto == v)
        return;
    m_cwInAscolto = v;
    if (v && m_decoPortUseRemote)
        setDecoPortMonitor(true);
    emit cwInAscoltoChanged();
}

