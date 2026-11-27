#include "DecodiumBridge.h"
#include <QTimer>

bool decodiumBridgeLegacyRigControlFallback(DecodiumLegacyBackend*, const QString&);
void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::syncLegacyBackendTxState()
{
    if (!usingLegacyBackendForRx() && !decodiumBridgeLegacyRigControlFallback(m_legacyBackend, m_catBackend)) {
        return;
    }

    syncLegacyBackendDialogState();
}

void DecodiumBridge::scheduleLegacyStateRefreshBurst()
{
    if (!legacyBackendAvailable()) {
        return;
    }

    if (!usingLegacyBackendForRx() && !decodiumBridgeLegacyRigControlFallback(m_legacyBackend, m_catBackend)) {
        return;
    }

    if (m_legacyStateTimer && !m_legacyStateTimer->isActive()) {
        m_legacyStateTimer->start();
        decodiumBridgeAudioLevelLog(QStringLiteral("Legacy state timer started after backend became available"));
    }

    if (m_syncingLegacyBackendState || m_syncingLegacyBackendDecodeList) {
        if (!m_legacyStateRefreshBurstQueued) {
            m_legacyStateRefreshBurstQueued = true;
            QTimer::singleShot(0, this, [this]() {
                m_legacyStateRefreshBurstQueued = false;
                scheduleLegacyStateRefreshBurst();
            });
        }
        return;
    }

    syncLegacyBackendState();

    static constexpr int kLegacyRefreshDelaysMs[] = {75, 180, 360};
    for (int const delayMs : kLegacyRefreshDelaysMs) {
        QTimer::singleShot(delayMs, this, [this]() {
            syncLegacyBackendState();
        });
    }
}

