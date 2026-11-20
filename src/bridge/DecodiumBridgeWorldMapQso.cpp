#include "DecodiumBridge.h"
#include "Sequencer/MessageTokenRules.hpp"

using decodium::seq::isGridTokenStrict;
QString decodiumBridgeNormalizeWorldMapCall(QString);
QString decodiumBridgeWorldMapCallKey(QString);
void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::markWorldMapQsoClosed(const QString& call, const QString& reason)
{
    QString mapCall = decodiumBridgeNormalizeWorldMapCall(call);
    if (mapCall.isEmpty()) {
        mapCall = decodiumBridgeNormalizeWorldMapCall(m_dxCall);
    }
    if (mapCall.isEmpty() && m_pendingAutoLogValid) {
        mapCall = decodiumBridgeNormalizeWorldMapCall(m_pendingAutoLogCall);
    }
    if (mapCall.isEmpty() && m_promptLogSnapshotValid) {
        mapCall = decodiumBridgeNormalizeWorldMapCall(m_promptLogCall);
    }
    if (mapCall.isEmpty() && m_lateAutoLogValid) {
        mapCall = decodiumBridgeNormalizeWorldMapCall(m_lateAutoLogCall);
    }

    QString const key = decodiumBridgeWorldMapCallKey(mapCall);
    if (key.isEmpty() || key == decodiumBridgeWorldMapCallKey(m_callsign)) {
        return;
    }

    m_worldMapClosedQsoCallKeys.insert(key);
    if (worldMapFeedEnabled()) {
        emit worldMapContactDowngraded(mapCall);
    }
    if (!reason.isEmpty()) {
        decodiumBridgeAudioLevelLog(QStringLiteral("WorldMap: closed QSO path %1 (%2)").arg(mapCall, reason));
    }
}

void DecodiumBridge::clearWorldMapClosedQso(const QString& call)
{
    QString const key = decodiumBridgeWorldMapCallKey(call);
    if (!key.isEmpty()) {
        m_worldMapClosedQsoCallKeys.remove(key);
    }
}

bool DecodiumBridge::worldMapQsoPathSuppressed(const QString& call) const
{
    QString const key = decodiumBridgeWorldMapCallKey(call);
    return !key.isEmpty() && m_worldMapClosedQsoCallKeys.contains(key);
}

void DecodiumBridge::emitCurrentWorldMapQsoPath()
{
    if (!worldMapFeedEnabled()) {
        return;
    }
    if (m_currentTx == 6) {
        return;
    }
    if (!m_txEnabled && !m_transmitting && !m_tuning) {
        return;
    }

    QString myGrid = m_grid.trimmed().toUpper();
    if (!isGridTokenStrict(myGrid)) {
        return;
    }
    myGrid = myGrid.left(qMin(6, myGrid.size()));

    QString remoteCall = decodiumBridgeNormalizeWorldMapCall(m_dxCall);
    if (remoteCall.isEmpty() && m_txEnabled && m_qsoProgress > 1 && m_qsoProgress < 6) {
        remoteCall = decodiumBridgeNormalizeWorldMapCall(inferredPartnerForAutolog());
    }
    if (remoteCall.isEmpty() || decodiumBridgeWorldMapCallKey(remoteCall) == decodiumBridgeWorldMapCallKey(m_callsign)) {
        return;
    }
    if (worldMapQsoPathSuppressed(remoteCall)) {
        return;
    }

    QString remoteGrid = m_dxGrid.trimmed().toUpper();
    if (isGridTokenStrict(remoteGrid)) {
        remoteGrid = remoteGrid.left(qMin(6, remoteGrid.size()));
        rememberWorldMapGrid(remoteCall, remoteGrid);
    } else {
        remoteGrid = lookupWorldMapGrid(remoteCall);
    }
    if (!isGridTokenStrict(remoteGrid)) {
        remoteGrid = approximateWorldMapGridForCall(remoteCall);
    }
    if (!isGridTokenStrict(remoteGrid)) {
        return;
    }

    static constexpr int roleOutgoingFromMe = 2;
    emit worldMapContactAdded(remoteCall, myGrid, remoteGrid, roleOutgoingFromMe);
}

