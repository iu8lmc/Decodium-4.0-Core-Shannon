#include <QFile>
#include <QtTest>

namespace {

QString readSource(const QString& relativePath)
{
    QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QString source = QString::fromUtf8(file.readAll());
    if (relativePath == QStringLiteral("src/bridge/DecodiumBridge.cpp")) {
        const QString startTxSource = readSource(QStringLiteral("src/bridge/DecodiumBridgeStartTx.cpp"));
        const int startPoint = source.indexOf(QStringLiteral("// BRIDGE_START_TX_INSERTION_POINT"));
        if (startTxSource.isEmpty() || startPoint < 0) return {};
        source.insert(startPoint, startTxSource);
        const QString autoSequence = readSource(QStringLiteral("src/bridge/DecodiumBridgeAutoSequence.cpp"));
        const int sequencePoint = source.indexOf(QStringLiteral("// BRIDGE_LAB_INSERTION_POINT"));
        if (autoSequence.isEmpty() || sequencePoint < 0) return {};
        source.insert(sequencePoint, autoSequence);

        const QString completeTxSource = readSource(QStringLiteral("src/bridge/DecodiumBridgeCompleteTxPlayback.cpp"));
        const int completePoint = source.indexOf(QStringLiteral("// BRIDGE_COMPLETE_TX_INSERTION_POINT"));
        if (completeTxSource.isEmpty() || completePoint < 0) return {};
        source.insert(completePoint, completeTxSource);
        const QString tuneSource = readSource(QStringLiteral("src/bridge/DecodiumBridgeTune.cpp"));
        const int tunePoint = source.indexOf(QStringLiteral("// BRIDGE_TUNE_INSERTION_POINT"));
        if (tuneSource.isEmpty() || tunePoint < 0) return {};
        source.insert(tunePoint, tuneSource);
        const QString stopTxSource = readSource(QStringLiteral("src/bridge/DecodiumBridgeStopTx.cpp"));
        const int stopPoint = source.indexOf(QStringLiteral("// BRIDGE_STOP_TX_INSERTION_POINT"));
        if (stopTxSource.isEmpty() || stopPoint < 0) return {};
        source.insert(stopPoint, stopTxSource);
        const QString periodic = readSource(QStringLiteral("src/bridge/DecodiumBridgePeriodicTx.cpp"));
        const int periodicPosition = source.indexOf(QStringLiteral("void DecodiumBridge::autoSequenceStep("));
        if (periodic.isEmpty() || periodicPosition < 0) return {};
        source.insert(periodicPosition, periodic);
        const QString sendTx = readSource(QStringLiteral("src/bridge/DecodiumBridgeSendTx.cpp"));
        const int txPosition = source.indexOf(QStringLiteral("void DecodiumBridge::stopTx()"));
        if (sendTx.isEmpty() || txPosition < 0) return {};
        source.insert(txPosition, sendTx);
        const QString monitoring = readSource(QStringLiteral("src/bridge/DecodiumBridgeRxMonitoring.cpp"));
        if (monitoring.isEmpty()) return {};
        source.append(monitoring);
        const QString mode = readSource(QStringLiteral("src/bridge/DecodiumBridgeMode.cpp"));
        const int at = source.indexOf(QStringLiteral("void DecodiumBridge::setMonitoring(bool v)"));
        if (mode.isEmpty() || at < 0) return {};
        source.insert(at, mode);
    }
    return source;
}

QString functionBody(const QString& source, const QString& signature,
                     const QString& nextSignature)
{
    const int start = source.indexOf(signature);
    if (start < 0)
        return {};
    const int end = source.indexOf(nextSignature, start + signature.size());
    return source.mid(start, end > start ? end - start : -1);
}

} // namespace

class TestTxDispatchGuard final : public QObject
{
    Q_OBJECT

private slots:
    void commonGuardIsWiredIntoTxEntryPoints();
    void staleTx6RequiresAutoOrExplicitManualAuthority();
    void lifecycleTransitionsInvalidateDeferredRequests();
    void deferredCallbacksCheckRequestGeneration();
    void jttyUsesNativeManualDispatchAndBlocksGenericStart();
    void enteringCwPreservesTheOperatorDial();
    void cwExitVerifiesRadioModeAfterStartupCw();
};

void TestTxDispatchGuard::commonGuardIsWiredIntoTxEntryPoints()
{
    const QString header = readSource(QStringLiteral("src/bridge/DecodiumBridge.h"));
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!header.isEmpty());
    QVERIFY(!cpp.isEmpty());

    QVERIFY(header.contains(QStringLiteral("void invalidateDeferredTxRequests(const QString& reason);")));
    QVERIFY(header.contains(QStringLiteral("bool txDispatchGuardAllows(const QString& reason);")));
    QVERIFY(header.contains(QStringLiteral("quint64 m_txRequestGeneration {0};")));
    QVERIFY(header.contains(QStringLiteral("int m_explicitManualTxRequest {0};")));

    const QString txEnableSource = readSource(QStringLiteral("src/bridge/DecodiumBridgeTxEnable.cpp"));
    const QString setTxEnabled = functionBody(
        txEnableSource,
        QStringLiteral("void DecodiumBridge::setTxEnabled(bool v)"),
        QStringLiteral("bool DecodiumBridge::txEnabled() const"));
    const QString startTx = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::startTx()"),
        QStringLiteral("void DecodiumBridge::sendTx(int n)"));
    const QString periodicTx = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::checkAndStartPeriodicTx()"),
        QStringLiteral("bool DecodiumBridge::isAutoCqBurstPureCq"));

    QVERIFY(setTxEnabled.contains(QStringLiteral("txDispatchGuardAllows")));
    QVERIFY(startTx.contains(QStringLiteral("txDispatchGuardAllows")));
    QVERIFY(periodicTx.contains(QStringLiteral("txDispatchGuardAllows")));
    QVERIFY(cpp.contains(QStringLiteral("armExplicitManualTxRequest(n, QStringLiteral(\"TX panel\"))")));
    const QString decodeClick = readSource(QStringLiteral("src/bridge/DecodiumBridgeDecodeClick.cpp"));
    QVERIFY(decodeClick.contains(QStringLiteral("armExplicitManualTxRequest(newCurrentTx")));
}

void TestTxDispatchGuard::staleTx6RequiresAutoOrExplicitManualAuthority()
{
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridgeTxDispatchGuard.cpp"));
    QVERIFY(!cpp.isEmpty());

    const QString guard = functionBody(
        cpp,
        QStringLiteral("bool DecodiumBridge::txDispatchGuardAllows"),
        QStringLiteral("void DecodiumBridge::invalidateDeferredTxRequests"));
    QVERIFY(!guard.isEmpty());
    QVERIFY(guard.contains(QStringLiteral("m_currentTx == 6")));
    QVERIFY(guard.contains(QStringLiteral("!m_autoCqRepeat")));
    QVERIFY(guard.contains(QStringLiteral("!m_multiAnswerMode")));
    QVERIFY(guard.contains(QStringLiteral("explicitManual")));
    QVERIFY(guard.contains(QStringLiteral("m_txEnabled = false")));
    QVERIFY(guard.contains(QStringLiteral("blocked stale TX6")));
}

void TestTxDispatchGuard::lifecycleTransitionsInvalidateDeferredRequests()
{
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());

    const QString autoCq = functionBody(
        readSource(QStringLiteral("src/bridge/DecodiumBridgeAutoCqRepeat.cpp")),
        QStringLiteral("void DecodiumBridge::setAutoCqRepeat(bool v)"),
        QStringLiteral("bool DecodiumBridge::autoCallModeSupported() const"));
    const QString halt = functionBody(
        readSource(QStringLiteral("src/bridge/DecodiumBridgeHalt.cpp")),
        QStringLiteral("void DecodiumBridge::haltWithReason(const QString& reason)"),
        QStringLiteral("void DecodiumBridge::setTune(bool v)"));
    const QString resetQso = functionBody(
        readSource(QStringLiteral("src/bridge/DecodiumBridgeQsoReset.cpp")),
        QStringLiteral("void DecodiumBridge::resetQsoStateForNextSequence(bool preserveAutoTx)"),
        QStringLiteral("bool DecodiumBridge::finishAutoSequenceQsoFromCompletedSignoff"));
    const QString completedQso = functionBody(
        readSource(QStringLiteral("src/bridge/DecodiumBridgeQsoCompletion.cpp")),
        QStringLiteral("void DecodiumBridge::clearTxArmedAfterCompletedQso"),
        QStringLiteral("void DecodiumBridge::logQsoNow()"));

    QVERIFY(autoCq.contains(QStringLiteral("invalidateDeferredTxRequests")));
    QVERIFY(halt.contains(QStringLiteral("invalidateDeferredTxRequests")));
    QVERIFY(resetQso.contains(QStringLiteral("invalidateDeferredTxRequests")));
    QVERIFY(completedQso.contains(QStringLiteral("invalidateDeferredTxRequests")));
    const QString guardSource = readSource(QStringLiteral("src/bridge/DecodiumBridgeTxDispatchGuard.cpp"));
    QVERIFY(guardSource.contains(QStringLiteral("++m_txRequestGeneration")));
    QVERIFY(guardSource.contains(QStringLiteral("m_periodicTxCheckScheduled = false")));
    QVERIFY(guardSource.contains(QStringLiteral("m_syncTxRetryScheduled = false")));
}

void TestTxDispatchGuard::deferredCallbacksCheckRequestGeneration()
{
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());

    // Every timer-backed TX scheduler must capture the lifecycle generation
    // and drop its callback after HALT/QSO completion invalidates it.
    const QString rendezvous = readSource(QStringLiteral("src/bridge/DecodiumBridgeDecodeTxRendezvous.cpp"));
    QVERIFY(!rendezvous.isEmpty());
    const QString smartFt2 = readSource(QStringLiteral("src/bridge/DecodiumBridgeSmartFt2Scheduler.cpp"));
    QVERIFY(!smartFt2.isEmpty());
    QVERIFY(smartFt2.contains(QStringLiteral("requestGeneration != m_txRequestGeneration")));
    const QString syncSchedule = readSource(QStringLiteral("src/bridge/DecodiumBridgeSyncSchedule.cpp"));
    const QString txEnable = readSource(QStringLiteral("src/bridge/DecodiumBridgeTxEnable.cpp"));
    QVERIFY(!syncSchedule.isEmpty());
    QVERIFY(!txEnable.isEmpty());
    const int generationChecks = (cpp + rendezvous + smartFt2 + syncSchedule + txEnable).count(
        QStringLiteral("requestGeneration != m_txRequestGeneration"));
    QVERIFY2(generationChecks >= 8,
             qPrintable(QStringLiteral("only %1 deferred TX generation checks found")
                            .arg(generationChecks)));
    QVERIFY((cpp + syncSchedule + txEnable).contains(QStringLiteral("tryStartDeferredManualSyncTx")));
    QVERIFY(cpp.contains(QStringLiteral("TX guard")));
}

void TestTxDispatchGuard::jttyUsesNativeManualDispatchAndBlocksGenericStart()
{
    const QString header = readSource(QStringLiteral("src/bridge/DecodiumBridge.h"));
    const QString bridge = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    const QString app = readSource(QStringLiteral("src/app/main_qml.cpp"));
    QVERIFY(!header.isEmpty());
    QVERIFY(!bridge.isEmpty());
    QVERIFY(!app.isEmpty());

    QVERIFY(header.contains(QStringLiteral("void jttyPanelTransmitRequested(const QString& text);")));
    QVERIFY(header.contains(QStringLiteral("void jttyAbortRequested();")));
    QVERIFY(header.contains(QStringLiteral("void reportJttyPanelTransmitResult(bool accepted")));

    const QString startTx = functionBody(
        bridge,
        QStringLiteral("void DecodiumBridge::startTx()"),
        QStringLiteral("void DecodiumBridge::reportJttyPanelTransmitResult"));
    const QString sendTx = functionBody(
        bridge,
        QStringLiteral("void DecodiumBridge::sendTx(int n)"),
        QStringLiteral("QString DecodiumBridge::txMessage(int n) const"));
    const QString stopTx = functionBody(
        bridge,
        QStringLiteral("void DecodiumBridge::stopTx()"),
        QStringLiteral("void DecodiumBridge::startTune()"));
    const QString setMode = functionBody(
        bridge,
        QStringLiteral("void DecodiumBridge::setMode(const QString& v)"),
        QStringLiteral("void DecodiumBridge::setMonitoring(bool v)"));

    const QString preflight = readSource(QStringLiteral("src/bridge/DecodiumBridgeStartTxPreflight.cpp"));
    QVERIFY(startTx.contains(QStringLiteral("if (!startTxPreflight()) return;")));
    QVERIFY(preflight.contains(QStringLiteral("blocked generic JTTY TX path")));
    QVERIFY(sendTx.contains(QStringLiteral("forwarding explicit TX%1 to native controller")));
    QVERIFY(sendTx.contains(QStringLiteral("emit jttyPanelTransmitRequested(message);")));
    QVERIFY(sendTx.indexOf(QStringLiteral("emit jttyPanelTransmitRequested(message);"))
             < sendTx.indexOf(QStringLiteral("armExplicitManualTxRequest(n")));
    QVERIFY(stopTx.contains(QStringLiteral("emit jttyAbortRequested();")));
    QVERIFY(setMode.contains(QStringLiteral("application mode changed %1->%2")));

    QVERIFY(app.contains(QStringLiteral("&DecodiumBridge::jttyPanelTransmitRequested")));
    QVERIFY(app.contains(QStringLiteral("jtty.send (text)")));
    QVERIFY(app.contains(QStringLiteral("&DecodiumBridge::jttyAbortRequested")));
}

void TestTxDispatchGuard::enteringCwPreservesTheOperatorDial()
{
    const QString bridge = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!bridge.isEmpty());

    const QString setMode = functionBody(
        bridge,
        QStringLiteral("void DecodiumBridge::setMode(const QString& v)"),
        QStringLiteral("void DecodiumBridge::setMonitoring(bool v)"));
    QVERIFY(!setMode.isEmpty());

    // Switching application mode to CW changes the radio mode but must not
    // select the band's QRP/calling frequency.  Those frequencies are still
    // used by an explicit band change performed while CW is active.
    const int preservePolicy = setMode.indexOf(QStringLiteral("bool const preserveOperatorDial"));
    const int preserveCw = setMode.indexOf(
        QStringLiteral("normalizedMode == QStringLiteral(\"CW\")"), preservePolicy);
    const int preserveFrequency = setMode.indexOf(
        QStringLiteral("m_bandManager->updateFromFrequency(m_frequency)"), preservePolicy);
    const int automaticQsy = setMode.indexOf(
        QStringLiteral("m_bandManager->updateForMode(bandMode)"), preservePolicy);
    QVERIFY(preservePolicy >= 0);
    QVERIFY(preserveCw > preservePolicy);
    QVERIFY(preserveFrequency > preserveCw);
    QVERIFY(automaticQsy > preserveFrequency);
}

void TestTxDispatchGuard::cwExitVerifiesRadioModeAfterStartupCw()
{
    const QString bridge = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    const QString manager = readSource(QStringLiteral("src/radio/DecodiumTransceiverManager.cpp"));
    QVERIFY(!bridge.isEmpty());
    QVERIFY(!manager.isEmpty());

    const QString setMode = functionBody(
        bridge,
        QStringLiteral("void DecodiumBridge::setMode(const QString& v)"),
        QStringLiteral("void DecodiumBridge::setMonitoring(bool v)"));
    QVERIFY(!setMode.isEmpty());

    // A mode restored from the physical rig at startup has no earlier local
    // CW entry snapshot. Its return target must still be verified, otherwise
    // a delayed CAT state report can put the desired state back to CW.
    QVERIFY(setMode.contains(QStringLiteral("reportedCatModeBeforeChange")));
    QVERIFY(setMode.contains(QStringLiteral("leavingReportedRigCw")));
    QVERIFY(setMode.contains(QStringLiteral("leavingCw || leavingReportedRigCw")));
    QVERIFY(setMode.contains(QStringLiteral("reportedCatModeBeforeChange == QStringLiteral(\"CW\")")));
    QVERIFY(setMode.contains(QStringLiteral("reportedCatModeBeforeChange == QStringLiteral(\"CW-R\")")));
    QVERIFY(setMode.contains(QStringLiteral("QString cwExitVerifyMode;")));
    QVERIFY(setMode.contains(QStringLiteral("cwExitVerifyMode = cwExitRestoreMode;")));
    QVERIFY(setMode.contains(QStringLiteral("kCwExitVerifyDelaysMs[] = {700, 1700}")));
    QVERIFY(setMode.contains(QStringLiteral("mode-change/cw-exit-verify")));
    QVERIFY(setMode.contains(QStringLiteral("setRigMode() always writes desired PTT=false")));

    // A CAT update emitted before the mode request may be delivered to the
    // GUI afterwards. Its physical values remain observable, but its old CW
    // mode must not replace the newer desired DATA-U/USB transaction.
    QVERIFY(manager.contains(QStringLiteral(
        "latestRequestedSequence =\n                    d->seqNum.load(std::memory_order_acquire)")));
    QVERIFY(manager.contains(QStringLiteral(
        "crossedInTransit = sequence < latestRequestedSequence")));
    const int guardedMerge = manager.indexOf(QStringLiteral("if (!crossedInTransit)"));
    const int desiredMerge = manager.indexOf(QStringLiteral("d->desired = state;"));
    QVERIFY(guardedMerge >= 0);
    QVERIFY(desiredMerge > guardedMerge);
    QVERIFY(manager.contains(QStringLiteral(
        "Crossed CAT update kept for telemetry; desired state preserved")));
}

QTEST_MAIN(TestTxDispatchGuard)
#include "test_tx_dispatch_guard.moc"
