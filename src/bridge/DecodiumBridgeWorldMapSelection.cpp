#include "DecodiumBridge.h"
#include "Sequencer/MessageTokenRules.hpp"
#include <QApplication>
#include <QDateTime>
#include <QScopedValueRollback>
#include <QTimer>

using decodium::seq::isGridTokenStrict;
QString decodiumBridgeNormalizeWorldMapCall(QString);
QString decodiumBridgeWorldMapCallKey(QString);
void decodiumBridgeAudioLevelLog(const QString&);
bool decodiumBridgeTruthyEnvironmentFlag(const char*);
bool decodiumBridgeTxPeriodIsEven(int);

void DecodiumBridge::processMapContactClick(const QString& call, const QString& grid)
{
    QString const mapCall = decodiumBridgeNormalizeWorldMapCall(call);
    if (mapCall.isEmpty()) {
        return;
    }

    if (m_mapRosterCallPending) {
        m_mapRosterCallPending = false;
        ++m_mapRosterCallSerial;
        decodiumBridgeAudioLevelLog(QStringLiteral("Map roster CALL cancelled by map selection"));
    }

    QString mapGrid = grid.trimmed().toUpper();
    if (isGridTokenStrict(mapGrid)) {
        mapGrid = mapGrid.left(qMin(6, mapGrid.size()));
        rememberWorldMapGrid(mapCall, mapGrid);
    } else {
        mapGrid = lookupWorldMapGrid(mapCall);
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    int const dblIntervalMs = qMax(180, QApplication::doubleClickInterval());
    bool const isDoubleClick = !m_mapLastClickCall.isEmpty()
        && m_mapLastClickCall == mapCall
        && m_mapLastClickMs > 0
        && (nowMs - m_mapLastClickMs) >= 0
        && (nowMs - m_mapLastClickMs) <= (dblIntervalMs + 120);
    m_mapLastClickCall = mapCall;
    m_mapLastClickMs = nowMs;

    if (m_autoCqRepeat) {
        setAutoCqRepeat(false);
    }
    clearCallerQueue();
    clearAutoCqPartnerLock();
    clearManualTxHold(QStringLiteral("map-contact-click"));

    bool const newQso = decodiumBridgeWorldMapCallKey(mapCall) != decodiumBridgeWorldMapCallKey(m_dxCall);
    QString const previousMapDx = m_dxCall.trimmed();
    clearPendingAutoSeqTx(QStringLiteral("map-contact manual selection"));
    if (newQso) {
        forgetPartnerMemoryForCall(previousMapDx,
                                   QStringLiteral("map-contact takeover -> %1").arg(mapCall));
    }
    setDxCall(mapCall);
    if (isGridTokenStrict(mapGrid)) {
        setDxGrid(mapGrid);
    }

    m_nTx73 = 0;
    m_autoSeqRogerReportBase.clear();
    m_txRetryCount = 0;
    m_lastNtx = -1;
    m_lastCqPidx = -1;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;
    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    m_quickPeerSignaled = false;
    m_qsoLogged = false;
    m_lastTransmittedMessage.clear();
    m_qsoStartedOn = QDateTime::currentDateTimeUtc();
    m_qsoFirstReplyOn = QDateTime {};
    clearPendingAutoLogSnapshot();
    if (newQso) {
        clearAutoCqPartnerLock();
    }

    genStdMsgs(mapCall, mapGrid);
    if (!advanceQsoState(1)) {
        return;
    }
    updateAutoCqPartnerLock();

    bool const singleClickStartsTx = getSetting(QStringLiteral("MapSingleClickTX"), false).toBool();
    bool const startTxNow = singleClickStartsTx ? !isDoubleClick : isDoubleClick;
    if (startTxNow) {
        armExplicitManualTxRequest(m_currentTx, QStringLiteral("map contact"));
        setTxEnabled(true);
        m_mapLastClickCall.clear();
        m_mapLastClickMs = 0;
    }

    emit statusMessage(isGridTokenStrict(mapGrid)
                           ? tr("Map selection: %1 %2").arg(mapCall, mapGrid)
                           : tr("Map selection: %1").arg(mapCall));
}

void DecodiumBridge::processMapRosterCall(const QString& call, const QString& grid)
{
    QString const targetCall = decodiumBridgeNormalizeWorldMapCall(call);
    if (targetCall.isEmpty()) {
        return;
    }

    bool const deferToFullSyncSlot =
        shouldAlignTxAudioToCurrentSyncSlot()
        && !decodiumBridgeTruthyEnvironmentFlag("DECODIUM_LAB_FORCE_TX_IMMEDIATE");
    QScopedValueRollback<bool> dispatchGuard(m_suppressImmediateTxEnableDispatch,
                                             deferToFullSyncSlot);

    // The map selection path prepares the standard messages and resets the
    // current QSO state. Its plain-click TX behavior is intentionally
    // conservative, but a CALL command in the roster is an explicit request
    // to start a QSO and must arm the selected TX step.
    processMapContactClick(targetCall, grid);

    bool const selectedTarget =
        decodiumBridgeWorldMapCallKey(m_dxCall) == decodiumBridgeWorldMapCallKey(targetCall);
    bool const selectedQsoTx = m_currentTx >= 1 && m_currentTx <= 5;
    if (!selectedTarget || !selectedQsoTx) {
        decodiumBridgeAudioLevelLog(QStringLiteral("Map roster CALL not armed: target=%1 selected=%2 tx=%3 progress=%4")
                      .arg(targetCall)
                      .arg(selectedTarget ? 1 : 0)
                      .arg(m_currentTx)
                      .arg(m_qsoProgress));
        emit statusMessage(tr("Map CALL could not select TX1 for %1").arg(targetCall));
        return;
    }

    int const deferredPeriodMs =
        deferToFullSyncSlot ? effectivePeriodMsForMode(m_mode) : 0;
    quint64 rosterSerial = 0;
    if (deferredPeriodMs > 0) {
        m_mapRosterCallPending = true;
        rosterSerial = ++m_mapRosterCallSerial;
    }

    armExplicitManualTxRequest(m_currentTx, QStringLiteral("map roster CALL"));
    setTxEnabled(true);
    if (!m_txEnabled) {
        if (m_mapRosterCallPending) {
            m_mapRosterCallPending = false;
            ++m_mapRosterCallSerial;
        }
        decodiumBridgeAudioLevelLog(QStringLiteral("Map roster CALL TX enable rejected: target=%1 tx=%2")
                      .arg(targetCall)
                      .arg(m_currentTx));
        return;
    }

    if (deferredPeriodMs > 0) {
        int const periodMs = deferredPeriodMs;
        qint64 const nowMs = correctedUtcMsecsSinceStartOfDay();
        qint64 const slotIndex = nowMs / static_cast<qint64>(periodMs);
        int const elapsedMs = static_cast<int>(
            nowMs % static_cast<qint64>(periodMs));
        int slotsAhead = 1;
        while (slotsAhead <= 4) {
            qint64 const candidate = slotIndex + slotsAhead;
            bool const candidateEven = ((candidate % 2) == 0);
            bool const isOurPeriod = decodiumBridgeTxPeriodIsEven(m_txPeriod)
                ? candidateEven
                : !candidateEven;
            if (isOurPeriod) {
                break;
            }
            ++slotsAhead;
        }

        qint64 const delayMs = qMax<qint64>(
            40,
            static_cast<qint64>(slotsAhead) * periodMs - elapsedMs + 40);
        QString const modeSnapshot = m_mode;
        QString const targetKey = decodiumBridgeWorldMapCallKey(targetCall);
        int const txSnapshot = m_currentTx;
        quint64 const requestGeneration = m_txRequestGeneration;

        decodiumBridgeAudioLevelLog(QStringLiteral(
            "Map roster CALL queued for full slot: target=%1 tx=%2 mode=%3 "
            "txPeriod=%4 elapsed=%5ms delay=%6ms serial=%7")
                      .arg(targetCall)
                      .arg(txSnapshot)
                      .arg(modeSnapshot)
                      .arg(m_txPeriod)
                      .arg(elapsedMs)
                      .arg(delayMs)
                      .arg(rosterSerial));
        emit statusMessage(tr("QSO with %1 armed - TX%2 at next full slot")
                               .arg(targetCall)
                               .arg(txSnapshot));

        QTimer::singleShot(delayMs, this,
                           [this, rosterSerial, modeSnapshot, targetKey,
                            txSnapshot, requestGeneration]() {
            if (rosterSerial != m_mapRosterCallSerial
                || requestGeneration != m_txRequestGeneration
                || !m_txEnabled
                || m_mode != modeSnapshot
                || decodiumBridgeWorldMapCallKey(m_dxCall) != targetKey
                || m_currentTx != txSnapshot
                || m_transmitting
                || m_tuning) {
                decodiumBridgeAudioLevelLog(QStringLiteral(
                    "Map roster CALL full-slot dispatch cancelled: "
                    "serial=%1 current=%2 mode=%3/%4 tx=%5/%6 enabled=%7")
                              .arg(rosterSerial)
                              .arg(m_mapRosterCallSerial)
                              .arg(modeSnapshot, m_mode)
                              .arg(txSnapshot)
                              .arg(m_currentTx)
                              .arg(m_txEnabled ? 1 : 0));
                if (rosterSerial == m_mapRosterCallSerial) {
                    m_mapRosterCallPending = false;
                }
                return;
            }

            decodiumBridgeAudioLevelLog(QStringLiteral(
                "Map roster CALL full-slot dispatch: target=%1 tx=%2 "
                "mode=%3 serial=%4")
                          .arg(m_dxCall)
                          .arg(m_currentTx)
                          .arg(m_mode)
                          .arg(rosterSerial));
            // Keep the pending guard set through setTxEnabled(): that call
            // first synchronizes the legacy state and only then forwards
            // the enable command to the backend.
            armExplicitManualTxRequest(txSnapshot, QStringLiteral("map roster CALL slot"));
            setTxEnabled(true);
            m_mapRosterCallPending = false;
            checkAndStartPeriodicTx();
        });

        m_mapLastClickCall.clear();
        m_mapLastClickMs = 0;
        return;
    }

    // Asynchronous modes can start immediately. Synchronized modes only reach
    // this fallback when their period is unavailable.
    m_mapRosterCallPending = false;
    checkAndStartPeriodicTx();

    m_mapLastClickCall.clear();
    m_mapLastClickMs = 0;
    decodiumBridgeAudioLevelLog(QStringLiteral("Map roster CALL armed: target=%1 tx=%2 progress=%3")
                  .arg(targetCall)
                  .arg(m_currentTx)
                  .arg(m_qsoProgress));
    emit statusMessage(tr("QSO with %1 armed - TX%2")
                           .arg(targetCall)
                           .arg(m_currentTx));
}

