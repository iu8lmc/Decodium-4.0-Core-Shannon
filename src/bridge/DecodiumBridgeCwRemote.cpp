#include "DecodiumBridge.h"
#include "RemoteRadioLink.h"
#include "CwSidetone.h"
#include "CwTiming.h"
#include <QAudioDevice>
#include <QSettings>

void decodiumBridgeAudioLevelLog(const QString&);

bool DecodiumBridge::cwSendAudio(const QString& text, int wpm)
{
    if (!cwCanTransmit())
        return false;
    sendCwAudio(text, 0, wpm);
    if (m_transmitting && m_decoPortUseRemote) {
        QVector<CwKeyEvent> events;
        for (const auto& e : decodium::cw::timelineFor(text, wpm))
            events.append({e.deltaMs, e.down});
        playCwMonitor(events, m_cwSidetoneHz);
    }
    return m_transmitting;
}

bool DecodiumBridge::cwRemoteKeySupported()
{
    if (!m_decoPortUseRemote || m_transmitting || m_tuning || sstvTxActive() || m_rttyTxActive)
        return false;
    RemoteRadioLink* link = remoteLinkIfAny();
    return link && link->supportsCwKey();
}

bool DecodiumBridge::cwRemoteSendKey(const QVector<CwKeyEvent>& events, int toneHz)
{
    RemoteRadioLink* link = remoteLinkIfAny();
    const bool sent = m_decoPortUseRemote && link && link->sendCwKey(events, toneHz);
    if (sent) playCwMonitor(events, toneHz);
    return sent;
}

// Il PTT della radio remota per il CW a tasto. Mentre e' alzato nessun altro
// modo puo' partire: startTx() lo rifiuta.
void DecodiumBridge::cwRemotePtt(bool on)
{
    RemoteRadioLink* link = remoteLinkIfAny();
    m_cwRemoteKeying = on;
    if (!on && m_cwSidetone)
        m_cwSidetone->clear();
    if (link)
        link->setPtt(on, 0);
    decodiumBridgeAudioLevelLog(QStringLiteral("CW remote key: PTT %1").arg(on ? QStringLiteral("on") : QStringLiteral("off")));
}

int DecodiumBridge::cwRemoteLeadMs()
{
    RemoteRadioLink* link = remoteLinkIfAny();
    return link ? link->txAudioLeadMs() : 250;
}

void DecodiumBridge::cwAbortAudio()
{
    if (m_cwSidetone) m_cwSidetone->clear();
    if (m_cwTxActive && (m_transmitting || m_tuning))
        stopTx();
}

bool DecodiumBridge::cwCanTransmit()
{
    if (m_ssbTxActive) return false;
    if (m_transmitting || m_tuning || sstvTxActive() || m_rttyTxActive)
        return false;
    if (m_mode == QStringLiteral("CW")) {
        if (!m_decoPortUseRemote)
            return cwRigKeyerAvailable();
        // In the CW application mode, do not send an audio sidetone as DATA-U:
        // use an opened local keyer or the supported remote CW-key path.
        QSettings cwSettings(QSettings::IniFormat, QSettings::UserScope,
                             QStringLiteral("Decodium"), QStringLiteral("Decodium"));
        if (!cwSettings.value(QStringLiteral("CW/remoteKey"), true).toBool()
            || !cwRemoteKeySupported())
            return false;
    }
    if (m_decoPortUseRemote) {
        RemoteRadioLink* link = remoteLinkIfAny();
        return link && link->isLinked() && link->canTransmit()
               && link->txBlockedReason().isEmpty();
    }
    bool found = false;
    const QAudioDevice device = resolveTxOutputDevice(&found);
    return found && !device.isNull();
}

