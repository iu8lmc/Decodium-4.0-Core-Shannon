#include "DecodiumBridge.h"
#include "RemoteRadioLink.h"
#include "Q65DopplerTracker.h"
#include <QDateTime>
#include <cmath>

void decodiumBridgeAudioLevelLog(const QString&);
void decodiumBridgeCwSetCatFrequency(QObject const*, DecodiumTransceiverManager*,
                                     const QString&, double, DecodiumOmniRigManager*,
                                     DecodiumLegacyBackend*);

bool DecodiumBridge::cwSendRigMorse(const QString& text, int wpm)
{
    return cwRigKeyerAvailable() && m_hamlibCat->sendRigMorse(text, wpm);
}

void DecodiumBridge::cwStopRigMorse()
{
    if (m_hamlibCat) m_hamlibCat->stopRigMorse();
}

void DecodiumBridge::cwRigPtt(bool on)
{
    if (!m_hamlibCat)
        return;
    if (on && m_startupTxGuard) {
        decodiumBridgeAudioLevelLog(QStringLiteral("TX guard: blocked startup CW PTT ON"));
        return;
    }
    // Release an already-started local CW transmission even if the operator
    // changed mode or switched to the remote path before pressing Stop.
    if (on && (m_mode != QStringLiteral("CW") || m_decoPortUseRemote))
        return;
    m_hamlibCat->setRigPtt(on);
}

bool DecodiumBridge::cwTuneFrequency(double hz)
{
    if (m_ssbTxActive) return false;
    if (!std::isfinite(hz) || hz < 1000 || hz > 1e12 || !m_catConnected
        || m_transmitting || m_tuning || m_cwRemoteKeying || m_cwTxActive
        || sstvTxActive() || m_rttyTxActive) return false;
    if (m_decoPortUseRemote) {
        auto* link = remoteLinkIfAny();
        if (!link || !link->isLinked() || link->ptt()) return false;
        // setFrequency routes remote QSY to the selected link, never local CAT.
        setFrequency(hz);
        return true;
    }
    const double dialHz = applyFrequencyCalibration(hz);
    if (!std::isfinite(dialHz) || dialHz <= 0) return false;
    if (m_q65Doppler) m_q65Doppler->setEnabled(false);
    m_localCatFrequencyPreviousHz = m_frequency;
    m_localCatFrequencyTargetHz = hz;
    m_localCatFrequencyGuardUntilMs = QDateTime::currentMSecsSinceEpoch() + 8000;
    m_localCatFrequencyGuardMaxMs = QDateTime::currentMSecsSinceEpoch() + 20000;
    setFrequency(hz);
    // Frequency only: generic QSY would restore a digital mode instead of CW.
    decodiumBridgeCwSetCatFrequency(this, m_hamlibCat, m_catBackend, dialHz, m_omniRigCat, m_legacyBackend);
    return true;
}

