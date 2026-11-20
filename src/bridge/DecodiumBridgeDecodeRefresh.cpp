#include "DecodiumBridge.h"
#include "DecodeChronology.h"

void DecodiumBridge::refreshDecodeListDxcc()
{
    bool changed = false;
    for (int i = 0; i < m_decodeList.size(); ++i) {
        QVariantMap entry = m_decodeList[i].toMap();
        if (entry.value("isTx").toBool())
            continue;
        QVariantMap original = entry;
        enrichDecodeEntry(entry);
        if (entry != original) {
            m_decodeList[i] = entry;
            changed = true;
        }
    }

    bool rxChanged = false;
    for (int i = 0; i < m_rxDecodeList.size(); ++i) {
        QVariantMap entry = m_rxDecodeList[i].toMap();
        if (entry.value("isTx").toBool())
            continue;
        QVariantMap original = entry;
        enrichDecodeEntry(entry);
        if (entry != original) {
            m_rxDecodeList[i] = entry;
            rxChanged = true;
        }
    }

    if (changed) {
        decodiumBridgeNormalizeDecodeEntries(m_decodeList, 1500, m_mode);
    }
    if (rxChanged) {
        decodiumBridgeNormalizeDecodeEntries(m_rxDecodeList, 1500, m_mode);
    }
    if (changed) {
        emitDecodeListChangedThrottled();
    }
    if (rxChanged) {
        emitRxDecodeListChangedThrottled(false);
    }
}
