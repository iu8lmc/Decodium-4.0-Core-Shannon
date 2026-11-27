#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "MapIntelligenceService.h"
#include "MapLayerModel.h"
#include "MapExternalOverlayService.h"
#include "DecodiumDxCluster.h"
#include "helper_functions.h"

int decodiumBridgeNormalizeWsprPowerDbm(int);
QString decodiumBridgeWsprPowerDbmLabel(int);
QString decodiumBridgeCustomCqPrefix(const QString&, const QString&);
QString decodiumBridgeFtxCqWithPrefix(QString const&, QString const&);
QString decodiumBridgeStandardFtxCq(const QString&, const QString&, const QString&);
QString decodiumBridgeStandardFtxQso(int, QString const&, QString const&, QString const&, QString const&, bool, bool, QString const&);
void decodiumBridgeStationLog(const QString&);

void DecodiumBridge::setCallsign(const QString& v) {
    if (m_callsign != v) {
        m_callsign = v;
        emit callsignChanged();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setStationIdentity(m_callsign, m_grid);
        }
        refreshPskReporterLocalStation();
        if (m_mapIntelligenceService) {
            m_mapIntelligenceService->setRosterStationCall(m_callsign);
        }
        if (m_dxCluster)   m_dxCluster->setCallsign(m_callsign);
        regenerateTxMessages();
    }
}

void DecodiumBridge::refreshMapMoonOverlay()
{
    if (!m_mapIntelligenceService) {
        return;
    }

    auto* const mapLayers = qobject_cast<MapLayerModel*>(
        m_mapIntelligenceService->layerModel());
    auto* const overlays = qobject_cast<MapExternalOverlayService*>(
        m_mapIntelligenceService->externalOverlayService());
    if (!mapLayers || !overlays
        || !mapLayers->layerEnabled(QStringLiteral("moon"))) {
        return;
    }

    double longitude = 0.0;
    double latitude = 0.0;
    if (!grid2deg(m_grid.trimmed(), longitude, latitude)) {
        overlays->setMoonData(false, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        return;
    }

    overlays->updateMoonForStation(latitude, longitude);
}

QString DecodiumBridge::grid() const { return m_grid; }
void DecodiumBridge::setGrid(const QString& v) {
    if (m_grid != v) {
        m_grid = v;
        emit gridChanged();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setStationIdentity(m_callsign, m_grid);
        }
        refreshMapMoonOverlay();
        refreshPskReporterLocalStation();
        if (m_dxCluster)   m_dxCluster->setCallsign(m_callsign);
        regenerateTxMessages();
    }
}

void DecodiumBridge::setStationRigInfo(const QString& v)
{
    if (m_stationRigInfo == v) {
        return;
    }
    m_stationRigInfo = v;
    emit stationRigInfoChanged();
    refreshPskReporterLocalStation();
}

void DecodiumBridge::setStationAntenna(const QString& v)
{
    if (m_stationAntenna == v) {
        return;
    }
    m_stationAntenna = v;
    emit stationAntennaChanged();
    refreshPskReporterLocalStation();
}

void DecodiumBridge::setWsprPowerDbm(int v)
{
    int const normalized = decodiumBridgeNormalizeWsprPowerDbm(v);
    if (m_wsprPowerDbm == normalized) {
        return;
    }
    m_wsprPowerDbm = normalized;
    emit wsprPowerDbmChanged();
    QString const mode = m_mode.trimmed().toUpper();
    if (mode == QStringLiteral("WSPR") || mode.startsWith(QStringLiteral("FST4W"))) {
        regenerateTxMessages();
    }
}

QVariantList DecodiumBridge::wsprPowerOptions() const
{
    QVariantList options;
    static constexpr int kWsprPowerDbmValues[] = {
        0, 3, 7, 10, 13, 17, 20, 23, 27, 30,
        33, 37, 40, 43, 47, 50, 53, 57, 60
    };
    for (int value : kWsprPowerDbmValues) {
        options.append(decodiumBridgeWsprPowerDbmLabel(value));
    }
    return options;
}

// Rigenera TX6 (CQ) sempre da callsign+grid; TX1-5 solo se dxCall è noto.
// Chiamato quando cambiano callsign, grid o dxCall.
// Non sovrascrive messaggi già personalizzati con dati correnti (formato standard).
void DecodiumBridge::regenerateTxMessages()
{
    if (m_callsign.isEmpty()) return;

    // Grid a 4 caratteri (Maidenhead sub-square)
    QString grid4 = m_grid.left(4).toUpper();
    QString const mode = m_mode.trimmed().toUpper();

    if (mode == QStringLiteral("WSPR") || mode.startsWith(QStringLiteral("FST4W"))) {
        QString const my = m_callsign.toUpper();
        int const dbm = decodiumBridgeNormalizeWsprPowerDbm(m_wsprPowerDbm);
        QString const wsprMessage = my + (grid4.isEmpty() ? QString() : " " + grid4)
            + " " + QString::number(dbm);
        setTx6(wsprMessage);
        setTx1(QString());
        setTx2(QString());
        setTx3(QString());
        setTx4(QString());
        setTx5(QString());
        if (m_currentTx >= 1 && m_currentTx <= 5) {
            setCurrentTx(6);
        }
        decodiumBridgeStationLog(QStringLiteral("regenerateTxMessages %1: tx6=%2 power=%3")
                      .arg(mode)
                      .arg(m_tx6)
                      .arg(decodiumBridgeWsprPowerDbmLabel(dbm)));
        return;
    }

    // TX6 — rebuild callsign/grid while retaining an operator-entered CQ
    // modifier such as "CQ WWA".  The legacy widget path does this through
    // m_CQtype; the QML bridge needs to retain it independently as well.
    // I nominativi speciali non standard (es. II9MESC) in FT8/FT4/FT2 devono
    // usare type-4 senza locator, altrimenti il packing standard li tronca.
    if (m_skipCustomCqPrefixRecovery) {
        m_skipCustomCqPrefixRecovery = false;
    } else if (m_customCqPrefix.isEmpty() && m_customCqMessage.isEmpty()) {
        m_customCqPrefix = decodiumBridgeCustomCqPrefix(m_tx6, m_callsign);
    }
    QString newTx6 = m_customCqMessage.isEmpty()
        ? decodiumBridgeFtxCqWithPrefix(
              decodiumBridgeStandardFtxCq(m_callsign, grid4, mode), m_customCqPrefix)
        : m_customCqMessage;
    setTx6(newTx6);

    // TX1-5: rigenerati solo se dxCall è noto
    if (!m_dxCall.isEmpty()) {
        QString rptSent = m_reportSent.isEmpty() ? "-10" : m_reportSent;

        setTx1(decodiumBridgeStandardFtxQso(1, m_dxCall, m_callsign, grid4, rptSent,
                                     m_sendRR73, false, mode));                    // DX MY GRID
        setTx2(decodiumBridgeStandardFtxQso(2, m_dxCall, m_callsign, grid4, rptSent,
                                     m_sendRR73, false, mode));                    // DX MY REPORT
        setTx3(decodiumBridgeStandardFtxQso(3, m_dxCall, m_callsign, grid4, rptSent,
                                     m_sendRR73, false, mode));                    // DX MY R+SENT_RPT
        setTx4(decodiumBridgeStandardFtxQso(4, m_dxCall, m_callsign, grid4, rptSent,
                                     m_sendRR73, false, mode));                    // DX MY RR73/RRR
        setTx5(decodiumBridgeStandardFtxQso(5, m_dxCall, m_callsign, grid4, rptSent,
                                     m_sendRR73, false, mode));                    // DX MY 73
    } else {
        setTx1(QString());
        setTx2(QString());
        setTx3(QString());
        setTx4(QString());
        setTx5(QString());
        if (m_currentTx >= 1 && m_currentTx <= 5 && !m_tx6.trimmed().isEmpty()) {
            setCurrentTx(6);
        }
    }

    decodiumBridgeStationLog("regenerateTxMessages: tx6=" + m_tx6 +
              " tx1=" + m_tx1 + " dxCall=" + m_dxCall);
}
