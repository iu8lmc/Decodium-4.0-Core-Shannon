#include <QFile>
#include <QtTest>

#include "src/bridge/MamTxPayloadPolicy.h"

namespace {

QString readSource(const QString& relativePath)
{
    QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString functionBody(const QString& source, const QString& signature,
                     const QString& nextSignature)
{
    const int start = source.indexOf(signature);
    if (start < 0) {
        return {};
    }
    const int end = source.indexOf(nextSignature, start + signature.size());
    return source.mid(start, end > start ? end - start : -1);
}

} // namespace

class TestMamTxPayloadPolicy final : public QObject
{
    Q_OBJECT

private slots:
    void liveSequencerCanUseOnlyAValidPayload();
    void manualRearmCannotUseAnAutoCqPayload();
    void clearingPayloadClearsBothVectors();
    void cacheNeverCrossesMonoAndMultiStream();
    void bridgeWiresCleanupAtEveryExit();
    void legacyMirrorRoutesMultiStreamDecodesToSlotState();
    void legacyMirrorCompletesExistingMamQsoAfterMamIsDisabled();
    void lateRogerReportResumesRetryExpiredMamSlot();
    void waterfallUsesTheComposedMamPayloadDuringTx();
};

void TestMamTxPayloadPolicy::liveSequencerCanUseOnlyAValidPayload()
{
    const QStringList messages {
        QStringLiteral("CQ TEST AA00"),
        QStringLiteral("CQ TEST AA00")
    };
    const QVector<int> frequencies {1200, 1260};

    const bool sequencer = decodium::mamtx::multiStreamSequencerIsActive(
        true, true, true, true, true, false);
    QVERIFY(sequencer);
    QVERIFY(decodium::mamtx::multiStreamPayloadIsActive(
        sequencer, messages, frequencies));

    const QVector<int> mismatched {1200};
    QVERIFY(!decodium::mamtx::multiStreamPayloadIsActive(
        sequencer, messages, mismatched));
}

void TestMamTxPayloadPolicy::manualRearmCannotUseAnAutoCqPayload()
{
    const QStringList messages {QStringLiteral("CQ TEST AA00")};
    const QVector<int> frequencies {1200};

    // Multi-Slot may remain enabled as a saved preference, but with neither
    // AutoCQ nor Multi-Answer active a re-armed manual TX must be mono.
    const bool autoCqOff = decodium::mamtx::multiStreamSequencerIsActive(
        true, true, true, false, true, false);
    QVERIFY(!autoCqOff);
    QVERIFY(!decodium::mamtx::multiStreamPayloadIsActive(
        autoCqOff, messages, frequencies));

    const bool heldManualTx = decodium::mamtx::multiStreamSequencerIsActive(
        true, true, true, true, true, true);
    QVERIFY(!heldManualTx);
}

void TestMamTxPayloadPolicy::clearingPayloadClearsBothVectors()
{
    QStringList messages {
        QStringLiteral("CQ TEST AA00"),
        QStringLiteral("CQ TEST AA00")
    };
    QVector<int> frequencies {1200, 1260};

    QVERIFY(decodium::mamtx::clearPendingPayload(messages, frequencies));
    QVERIFY(messages.isEmpty());
    QVERIFY(frequencies.isEmpty());
    QVERIFY(!decodium::mamtx::clearPendingPayload(messages, frequencies));
}

void TestMamTxPayloadPolicy::cacheNeverCrossesMonoAndMultiStream()
{
    QVERIFY(decodium::mamtx::cacheMatchesPayloadKind(false, false));
    QVERIFY(decodium::mamtx::cacheMatchesPayloadKind(true, true));
    QVERIFY(!decodium::mamtx::cacheMatchesPayloadKind(true, false));
    QVERIFY(!decodium::mamtx::cacheMatchesPayloadKind(false, true));
}

void TestMamTxPayloadPolicy::bridgeWiresCleanupAtEveryExit()
{
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());

    const QString helper = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::clearMamPendingTxPayload"),
        QStringLiteral("void DecodiumBridge::scheduleIdleAudioBufferRelease"));
    QVERIFY(helper.contains(QStringLiteral("decodium::mamtx::clearPendingPayload")));
    QVERIFY(helper.contains(QStringLiteral("invalidateTxAudioCache()")));

    const QString txEnabled = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::setTxEnabled(bool v)"),
        QStringLiteral("void DecodiumBridge::setTxDisabled"));
    QVERIFY(txEnabled.contains(
        QStringLiteral("clearMamPendingTxPayload(QStringLiteral(\"tx-disabled\"))")));
    QVERIFY(!txEnabled.contains(QStringLiteral("m_mamSlots.clear()")));

    const QString autoCq = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::setAutoCqRepeat(bool v)"),
        QStringLiteral("bool DecodiumBridge::autoCallModeSupported"));
    QVERIFY(autoCq.contains(QStringLiteral("if (!m_multiAnswerMode)")));
    QVERIFY(autoCq.contains(
        QStringLiteral("clearMamPendingTxPayload(QStringLiteral(\"autocq-disabled\"))")));

    const QString multiAnswer = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::setMultiAnswerMode(bool v)"),
        QStringLiteral("void DecodiumBridge::setAutoCqRepeat(bool v)"));
    QVERIFY(multiAnswer.contains(
        QStringLiteral("clearMamPendingTxPayload(QStringLiteral(\"mam-disabled\"))")));

    const QString multiStream = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::setMamMultiStream(bool on)"),
        QStringLiteral("void DecodiumBridge::setMamMaxStreams"));
    QVERIFY(multiStream.contains(
        QStringLiteral("clearMamPendingTxPayload(QStringLiteral(\"multi-stream-disabled\"))")));

    const QString halt = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::haltWithReason(const QString& reason)"),
        QStringLiteral("void DecodiumBridge::refreshAudioDevices"));
    QVERIFY(halt.contains(QStringLiteral("clearMamPendingTxPayload(")));
    QVERIFY(halt.contains(QStringLiteral("emit mamActiveSlotsChanged()")));
    QVERIFY(halt.contains(QStringLiteral("abortLegacyBridgeTxRequest(QStringLiteral(\"halt:%1\")")));

    const QString gate = functionBody(
        cpp,
        QStringLiteral("bool DecodiumBridge::multiStreamActive() const"),
        QStringLiteral("bool DecodiumBridge::isMamMultiStreamMode() const"));
    QVERIFY(gate.contains(QStringLiteral("mamMultiStreamSequencerActive()")));
    QVERIFY(gate.contains(QStringLiteral("multiStreamPayloadIsActive")));

    const QString ensure = functionBody(
        cpp,
        QStringLiteral("bool DecodiumBridge::ensureTxAudioPrepared("),
        QStringLiteral("void DecodiumBridge::saveTxRecordingAsync"));
    QVERIFY(ensure.contains(QStringLiteral("bool const useMultiStream = multiStreamActive();")));
    QVERIFY(ensure.contains(QStringLiteral("m_txAudioCache.multiStream")));
}

void TestMamTxPayloadPolicy::legacyMirrorRoutesMultiStreamDecodesToSlotState()
{
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());

    // The macOS embedded decoder reaches the sequencer through the legacy
    // mirror.  In native MAM it must never feed autoSequenceStep(), otherwise
    // a serial QSO is created beside the real per-slot state machine.
    QVERIFY(cpp.contains(QStringLiteral("legacy-mirror MAM %1ingest:")));
    QVERIFY(cpp.contains(QStringLiteral("mamIngestDecode(fields);")));
    QVERIFY(cpp.contains(QStringLiteral("MAM slot capacity full: queued initial caller")));
    QVERIFY(cpp.contains(QStringLiteral("plainReportReply")));
    QVERIFY(cpp.contains(QStringLiteral("MAM plain report queued")));
    QVERIFY(cpp.contains(QStringLiteral("MAM queue waiting for caller frequency")));

    const int mamIngest = cpp.indexOf(QStringLiteral("legacy-mirror MAM %1ingest:"));
    const int serialFeed = cpp.indexOf(QStringLiteral("legacy-mirror autoSeq feed:"));
    QVERIFY(mamIngest >= 0);
    QVERIFY(serialFeed >= 0);
    QVERIFY(mamIngest < serialFeed);
}

void TestMamTxPayloadPolicy::legacyMirrorCompletesExistingMamQsoAfterMamIsDisabled()
{
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());

    const QString mirror = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::syncLegacyBackendDecodeList()"),
        QStringLiteral("void DecodiumBridge::syncLegacyBackendState()"));
    QVERIFY(mirror.contains(QStringLiteral("mamCompletionPending")));
    QVERIFY(mirror.contains(QStringLiteral("matchesExistingMam")));
    QVERIFY(mirror.contains(QStringLiteral("QStringLiteral(\"completion \")")));
    QVERIFY(mirror.contains(QStringLiteral("if (!autoSeqActive)")));
}

void TestMamTxPayloadPolicy::lateRogerReportResumesRetryExpiredMamSlot()
{
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());

    const QString ingest = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::mamIngestDecode"),
        QStringLiteral("void DecodiumBridge::mamPruneSlots"));
    QVERIFY(ingest.contains(QStringLiteral("hasRogerReport && receivedReportValid")));
    QVERIFY(ingest.contains(QStringLiteral("MAM late report resumed")));
    QVERIFY(ingest.contains(QStringLiteral("MAM late final 73: %1 -> logged")));
    QVERIFY(ingest.contains(QStringLiteral("recovery.slot.currentTx = 4")));
    QVERIFY(ingest.contains(QStringLiteral("MAM plain report queued")));
    QVERIFY(ingest.contains(QStringLiteral("A reply must always stay on the caller's original audio frequency")));

    const QString prune = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::mamPruneSlots"),
        QStringLiteral("void DecodiumBridge::mamPromoteLateReportRecoveries"));
    QVERIFY(prune.contains(QStringLiteral("MAM slot %1 retained pending final 73 (120s)")));
    QVERIFY(prune.contains(QStringLiteral("s.progress >= 2 && s.progress <= 4")));
    QVERIFY(!prune.contains(QStringLiteral("-> log anyway")));
    // Native MAM bypasses the legacy auto-sequencer cleanup.  Its signoff
    // cooldown must therefore expire here instead of blocking the station for
    // the rest of a long-running session.
    QVERIFY(prune.contains(QStringLiteral("kQsoCooldownWindowMs")));
    QVERIFY(prune.contains(QStringLiteral("m_qsoCooldown.erase(it)")));

    const QString promote = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::mamPromoteLateReportRecoveries"),
        QStringLiteral("void DecodiumBridge::mamPromoteFromQueue"));
    QVERIFY(promote.contains(QStringLiteral("MAM deferred reply promoted")));
    QVERIFY(promote.contains(QStringLiteral("readyForSlot")));

    const QString header = readSource(QStringLiteral("src/bridge/DecodiumBridge.h"));
    QVERIFY(header.contains(QStringLiteral("A late-report recovery has an exchange state")));
    QVERIFY(header.contains(QStringLiteral("callerQueueSize() const { return callerQueue().size(); }")));
}

void TestMamTxPayloadPolicy::waterfallUsesTheComposedMamPayloadDuringTx()
{
    const QString header = readSource(QStringLiteral("src/bridge/DecodiumBridge.h"));
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    const QString waterfall = readSource(QStringLiteral("qml/decodium/components/Waterfall.qml"));
    QVERIFY(!header.isEmpty());
    QVERIFY(!cpp.isEmpty());
    QVERIFY(!waterfall.isEmpty());

    // Active QSO slots do not contain idle parallel CQ streams, so the
    // bridge must expose the exact m_mamMessages/m_mamF0sHz payload separately.
    QVERIFY(header.contains(QStringLiteral("Q_PROPERTY(QVariantList mamTxStreams READ mamTxStreams NOTIFY mamTxStreamsChanged)")));
    const QString streams = functionBody(
        cpp,
        QStringLiteral("QVariantList DecodiumBridge::mamTxStreams() const"),
        QStringLiteral("bool DecodiumBridge::mamModeExperimental() const"));
    QVERIFY(streams.contains(QStringLiteral("m_mamMessages")));
    QVERIFY(streams.contains(QStringLiteral("m_mamF0sHz")));
    QVERIFY(streams.contains(QStringLiteral("stream.insert(QStringLiteral(\"cq\"), isCq)")));

    const QString dispatch = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::mamDispatchPeriod()"),
        QStringLiteral("bool DecodiumBridge::ensureTxAudioPrepared("));
    QVERIFY(dispatch.contains(QStringLiteral("emit mamTxStreamsChanged();")));
    QVERIFY(dispatch.indexOf(QStringLiteral("emit mamTxStreamsChanged();"))
            < dispatch.indexOf(QStringLiteral("startTx();")));

    // The old generic guide is hidden during a composite TX and the cascade
    // receives one marker per actual outgoing stream, including CQ streams.
    QVERIFY(waterfall.contains(QStringLiteral("bridge.mamTxStreams")));
    QVERIFY(waterfall.contains(QStringLiteral("id: mamTxWaterfallMarkers")));
    QVERIFY(waterfall.contains(QStringLiteral("readonly property bool mamCompositeTx")));
}

QTEST_APPLESS_MAIN(TestMamTxPayloadPolicy)
#include "test_mam_tx_payload_policy.moc"
