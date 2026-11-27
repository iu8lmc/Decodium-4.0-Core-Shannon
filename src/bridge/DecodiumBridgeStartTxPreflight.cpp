// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#include "Q65DopplerTracker.h"

void decodiumBridgeDecodeFlowLog(const QString&);
bool decodiumBridgeIsFt2LinkApplicationMode(QString);
#define bridgeLog decodiumBridgeDecodeFlowLog
#define isFt2LinkApplicationMode decodiumBridgeIsFt2LinkApplicationMode

bool DecodiumBridge::startTxPreflight()
{
    if (m_mode == QStringLiteral("SSB") || m_ssbTxActive) return false;
    // CW audio TX is a dedicated, explicitly armed path. Never let a stale
    // FT/AutoCQ scheduler call the generic transmitter while the application
    // is restored in CW; that path can otherwise key the rig at startup.
    if ((m_mode == QStringLiteral("CW") || m_mode == QStringLiteral("CW-R"))
        && !m_cwTxActive) {
        bridgeLog(QStringLiteral("startTx: blocked unarmed generic TX in CW mode"));
        return false;
    }
    // JTTY has its own continuous keyboard modem and audio/PTT lifecycle.
    // A queued FT/JT start must never fall through to buildTxWaveformForMessage:
    // that builder deliberately has no JTTY waveform, and such a fall-through
    // used to produce the misleading "mode not supported by audio bridge"
    // error on every platform. Explicit TX-panel clicks are intercepted in
    // sendTx() and forwarded to JttyController instead.
    if (m_mode.compare(QStringLiteral("JTTY"), Qt::CaseInsensitive) == 0) {
        bridgeLog(QStringLiteral("startTx: blocked generic JTTY TX path; waiting for explicit JTTY controller request"));
        emit statusMessage(tr("JTTY uses its native transmitter; use the JTTY window or click a TX message explicitly."));
        return false;
    }
    // Il CW a tasto verso la radio remota tiene il PTT: un altro modo che
    // partisse adesso trasmetterebbe sopra.
    if (m_cwRemoteKeying) {
        bridgeLog(QStringLiteral("startTx: refused, CW remote keying in progress"));
        emit errorMessage(tr("CW transmission in progress"));
        return false;
    }
    // Refresh before waveform/audio-offset selection, not only at PTT time.
    if (m_q65Doppler && (m_q65Doppler->enabled() || m_q65Doppler->applied()))
        refreshQ65Doppler();
    // RTL-SDR devices have no transmit chain.  Enforce receive-only operation
    // in the backend as well as in QML so an auto-sequence or shortcut can
    // never key an unrelated CAT/PTT configuration while RTL RX is selected.
    if (rtlSdrEnabled()) {
        const QString message = QStringLiteral("TX blocked: RTL-SDR receiver mode is receive-only.");
        bridgeLog(message);
        emit errorMessage(message);
        return false;
    }
    // FT2-Link: la TX passa SOLO dal path dedicato (transmitFt2LinkAudio ->
    // m_ft2LinkTxActive=true). Blocca qui i TX del sequencer FTx standard
    // (AutoCQ/auto-seq) che in FT2-Link genererebbero un CQ non codificabile.
    if (isFt2LinkApplicationMode(m_mode) && !m_ft2LinkTxActive) {
        bridgeLog(QStringLiteral("startTx: TX standard bloccato in FT2-Link (usa ARM + CQ TX del pannello)"));
        return false;
    }

    return true;
}
