// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#include "BridgeConfigConstants.h"

void decodiumBridgeDecodeFlowLog(const QString&);
#define bridgeLog decodiumBridgeDecodeFlowLog

bool DecodiumBridge::prepareStartTxSelection(bool customAudioTxActive)
{
    auto const selectedSlotMessage = [this]() -> QString {
        switch (m_currentTx) {
        case 1: return m_tx1;
        case 2: return m_tx2;
        case 3: return m_tx3;
        case 4: return m_tx4;
        case 5: return m_tx5;
        case 6: return m_tx6;
        default: return {};
        }
    };

    // Audio custom: salta le guardie FT (validazione payload, fallback CQ,
    // Fox/Hound). CW e FT2-Link hanno testo/waveform arbitrari.
    if (!customAudioTxActive) {
        if (!ensureCurrentTxCanTransmit(QStringLiteral("startTx"))) return false;
        if (!checkSwrAllowsTransmission(QStringLiteral("startTx"))) return false;
        if (legacyTxBackendRequested() && !legacyBackendAvailable() && !ensureLegacyBackendAvailable()) {
            emit errorMessage(QStringLiteral("Backend legacy non disponibile per Fox/Hound"));
            return false;
        }
        if (!prepareHoundTxSelectionForStart(QStringLiteral("startTx"))) {
            return false;
        }
        if (!ensureCurrentTxCanTransmit(QStringLiteral("startTx after hound prepare"))) return false;

        bool const fallbackToCq = (m_currentTx >= 1 && m_currentTx <= 5) &&
                                  m_dxCall.trimmed().isEmpty() &&
                                  selectedSlotMessage().trimmed().isEmpty() &&
                                  !m_tx6.trimmed().isEmpty() &&
                                  m_specialOperationActivity != kSpecialOpHound;
        if (fallbackToCq) {
            bridgeLog("startTx: no DX payload available, selecting TX6/CQ");
            if (!selectCurrentTxIfAllowed(6, QStringLiteral("startTx fallback CQ"))) {
                return false;
            }
        }
    }

    return true;
}
