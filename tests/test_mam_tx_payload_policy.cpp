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
    void lateSignoffFromASlowPartnerStillLogsTheQso();
    void lowConfidenceDecodeDoesNotRewriteTheReportSnr();
    void rr73SentToAStationThatLeftIsLoggedAndNotRepeatedForever();
    void aStationNeverCalledIsNotDroppedAsSilent();
    void aLowConfidenceSignoffNeverClosesAQso();
    void adifFieldsAreWhatActuallyHappened();
    void logAnywayNeedsAReportFromThePartner();
    void tx3SignoffCapCoversFt8AndFt4();
    void aWorkedPartnerStaysOutForFiveMinutes();
    void courtesySignoffNeverLogsTwice();
    void aQueuePlaceholderSnrIsTreatedAsUnknownByThePowerBudget();
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

void TestMamTxPayloadPolicy::lateSignoffFromASlowPartnerStillLogsTheQso()
{
    // 24/10/2026, on the air: SP9TNK answered 40 s late, the slot ran out of
    // retries, and his RR73 was thrown away. The recovered exchange must also
    // accept a signoff (RR73/RRR) and log the QSO, but only if we had already
    // sent our report (progress >= 3).
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());
    const QString ingest = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::mamIngestDecode"),
        QStringLiteral("void DecodiumBridge::mamPruneSlots"));
    QVERIFY(ingest.contains(QStringLiteral("|| isFinal73 || hasSignoff)")));
    QVERIFY(ingest.contains(QStringLiteral("MAM late RR73")));
    QVERIFY(ingest.contains(QStringLiteral("recovery.slot.progress >= 3")));
}

void TestMamTxPayloadPolicy::lowConfidenceDecodeDoesNotRewriteTheReportSnr()
{
    // 24/10/2026, on the air: a-priori decodes marked '?' rebuilt SP9UXV's
    // previous message from the tail of our own signal, with SNR +9; the report
    // sent to a -22 dB station became +09.
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());
    const QString ingest = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::mamIngestDecode"),
        QStringLiteral("void DecodiumBridge::mamPruneSlots"));
    QVERIFY(ingest.contains(QStringLiteral("lowConfidenceDecode = message.trimmed().endsWith")));
    QVERIFY(ingest.contains(
        QStringLiteral("decodedSnr != 127 && (!lowConfidenceDecode || s.partnerSnrDb == 127)")));
}

void TestMamTxPayloadPolicy::rr73SentToAStationThatLeftIsLoggedAndNotRepeatedForever()
{
    // 25/10/2026, on the air: two stations acknowledged our report, got our RR73
    // (12 times for one of them, resumed again and again by its repeated
    // R+report) and neither contact was logged.
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());
    const QString prune = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::mamPruneSlots"),
        QStringLiteral("void DecodiumBridge::mamPromoteLateReportRecoveries"));
    QVERIFY(prune.contains(QStringLiteral("m_ft2LogRr73OnPartnerLeft && s.currentTx == 4")));
    QVERIFY(prune.contains(QStringLiteral("logged on RR73 (partner left)")));
    const QString ingest = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::mamIngestDecode"),
        QStringLiteral("void DecodiumBridge::mamPruneSlots"));
    QVERIFY(ingest.contains(QStringLiteral("recovery.slot.lateResumes >= 2")));
}

void TestMamTxPayloadPolicy::aStationNeverCalledIsNotDroppedAsSilent()
{
    // 25/10/2026, on the air (FT8): stations promoted from the queue were kept
    // waiting by the power budget and dropped as "silent > 6 periods" 90 s later
    // without ever being called.
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());
    const QString prune = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::mamPruneSlots"),
        QStringLiteral("void DecodiumBridge::mamPromoteLateReportRecoveries"));
    QVERIFY(prune.contains(QStringLiteral("s.lastTxSlotMs == 0")));
    QVERIFY(prune.contains(QStringLiteral("never called, waited > 12 periods")));
    QVERIFY(prune.contains(QStringLiteral("qMax(s.lastHeardMs, s.lastTxSlotMs)")));
}

void TestMamTxPayloadPolicy::aQueuePlaceholderSnrIsTreatedAsUnknownByThePowerBudget()
{
    // 25/10/2026, on the air (FT8): stations promoted from the queue carried the
    // click placeholder of -10 dB as if it were a measurement; the budget asked
    // 0.63 of the frame for each, so only one fitted and 13 frames in a row
    // served a single station.
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());
    QVERIFY(cpp.contains(QStringLiteral("s.partnerSnrDb = (qSnr == -10) ? 127 : qSnr;")));
}

void TestMamTxPayloadPolicy::aLowConfidenceSignoffNeverClosesAQso()
{
    // 26/10/2026, report from T77RN: PA0BTR was closed on a "73" he never sent,
    // read from a low-confidence a-priori decode. Both sequencers must refuse it.
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());
    const QString mam = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::mamIngestDecode"),
        QStringLiteral("void DecodiumBridge::mamPruneSlots"));
    QVERIFY(mam.contains(QStringLiteral("decodium::seq::mayFeedRescuedDecodeToSequencer(message)")));
    const QString seq = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::autoSequenceStep"),
        QStringLiteral("QString localHashPartner;"));
    QVERIFY(seq.contains(QStringLiteral("mayFeedRescuedDecodeToSequencer(msg)")));
}

void TestMamTxPayloadPolicy::adifFieldsAreWhatActuallyHappened()
{
    // 26/10/2026, report from T77RN: M0PWL was logged with RST_RCVD -10 never
    // received and an empty grid; KV4T with RST_SENT -01 while +05 and -02 had
    // been transmitted.
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());
    const QString snap = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::capturePendingAutoLogSnapshot"),
        QStringLiteral("QDateTime DecodiumBridge::effectiveQsoLogTimeOnUtc"));
    QVERIFY(snap.contains(QStringLiteral("m_pendingAutoLogGrid = lookupWorldMapGrid(snapshotCall)")));
    QVERIFY(snap.contains(QStringLiteral("m_lastReportTxMessage")));
    QVERIFY(snap.contains(QStringLiteral("m_reportReceivedReal ? m_reportReceived.trimmed() : QString()")));
    QVERIFY(!cpp.contains(QStringLiteral("rstRcvd.isEmpty() ? QStringLiteral(\"-10\") : rstRcvd")));
    QVERIFY(cpp.count(QStringLiteral("setReportReceivedPlaceholder(QStringLiteral(\"-10\"))")) == 3);
}

void TestMamTxPayloadPolicy::logAnywayNeedsAReportFromThePartner()
{
    // 26/10/2026, report from T77RN: QSOs were logged and sent to QRZ at the retry
    // cap with no report from the partner, and the log said "partner left" even
    // when the partner was still transmitting (KV4T).
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());
    QVERIFY(cpp.contains(QStringLiteral(
        "logNow && reason.contains(QLatin1String(\"log anyway\")) && !m_reportReceivedReal")));
    QVERIFY(cpp.contains(QStringLiteral("QSO NOT logged: retry cap reached but")));
    QVERIFY(!cpp.contains(QStringLiteral("-> log anyway, partner left\")")));
    QCOMPARE(cpp.count(QStringLiteral("-> log anyway, retry cap reached\")")), 3);
}

void TestMamTxPayloadPolicy::tx3SignoffCapCoversFt8AndFt4()
{
    // 26/10/2026: a QuickQSO TX3 "R-16 TU" to DD8HG, who never answered, was sent 41
    // times in 20 minutes in FT8 because the TX3 signoff cap was FT2 only.
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());
    const QString body = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::checkAndStartPeriodicTx"),
        QStringLiteral("void DecodiumBridge::autoSequenceStep("));
    const int cap = body.indexOf(QStringLiteral("int tx3SignoffCap = "));
    QVERIFY(cap > 0);
    const QString guard = body.left(cap).right(420);
    QVERIFY(guard.contains(QStringLiteral("m_mode == QStringLiteral(\"FT2\")")));
    QVERIFY(guard.contains(QStringLiteral("m_mode == QStringLiteral(\"FT4\")")));
    QVERIFY(guard.contains(QStringLiteral("m_mode == QStringLiteral(\"FT8\")")));
    // the FT8/FT4 cap follows the operator's own Caller retries when it is higher (IT9MRM, 27/10/2026)
    QVERIFY(body.contains(QStringLiteral("tx3SignoffCap = qMax(tx3SignoffCap, m_maxCallerRetries);")));
}

void TestMamTxPayloadPolicy::aWorkedPartnerStaysOutForFiveMinutes()
{
    // 27/10/2026: Z32AW (T77RN's PC) and EA4HUK were reopened ~100 s after the QSO because
    // the already-worked window was 90 s, and answered again.
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());
    QVERIFY(cpp.contains(QStringLiteral("constexpr int kRecentAutoCqPartnerWindowSeconds {300};")));
    QVERIFY(cpp.contains(QStringLiteral("constexpr int kRecentDuplicateLogWindowSeconds {90};")));
    const QString dup = functionBody(
        cpp,
        QStringLiteral("bool DecodiumBridge::isRecentAutoCqDuplicate"),
        QStringLiteral("bool DecodiumBridge::isRecentAutoCqWorkedOrLoggedDuplicate"));
    QVERIFY(dup.contains(QStringLiteral("isWithinRecentAutoCqPartnerWindow(workedIt.value()")));
    QVERIFY(dup.contains(QStringLiteral("isWithinRecentAutoCqPartnerWindow(loggedIt.value()")));
    // an abandoned partner (no answer) is still skipped for 90 s only
    QVERIFY(dup.contains(QStringLiteral("isWithinRecentDuplicateWindow(abandonedIt.value()")));
    const QString wl = functionBody(
        cpp,
        QStringLiteral("bool DecodiumBridge::isRecentAutoCqWorkedOrLoggedDuplicate"),
        QStringLiteral("void DecodiumBridge::touchRecentAutoCqWorked"));
    QVERIFY(wl.contains(QStringLiteral("isWithinRecentAutoCqPartnerWindow(workedIt.value()")));
    QVERIFY(wl.contains(QStringLiteral("isWithinRecentAutoCqPartnerWindow(loggedIt.value()")));
    // the window slides while the partner keeps calling
    QVERIFY(cpp.contains(QStringLiteral("touchRecentAutoCqWorked(messagePartnerBase, m_frequency, m_mode);")));
    // the log de-duplication window must not have grown
    QVERIFY(cpp.contains(QStringLiteral("suppressDuplicateLog = isWithinRecentDuplicateWindow(it.value(), nowUtc);")));
}

void TestMamTxPayloadPolicy::courtesySignoffNeverLogsTwice()
{
    // 1.0.720: a one-shot 73 to a partner whose QSO is already logged must be released
    // without logging by every exit that can finish a QSO.
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());
    const QString fromSignoff = functionBody(
        cpp,
        QStringLiteral("bool DecodiumBridge::finishAutoSequenceQsoFromCompletedSignoff"),
        QStringLiteral("void DecodiumBridge::checkAndStartPeriodicTx"));
    QVERIFY(fromSignoff.left(300).contains(QStringLiteral("releaseCourtesySignoffIfActive(reason)")));
    // the two finishAutoSequenceQso lambdas
    QCOMPARE(cpp.count(QStringLiteral("if (releaseCourtesySignoffIfActive(reason)) {\n            return;")), 2);
    // release never logs and restores the worked entry that finishing without a log removes
    const QString release = functionBody(
        cpp,
        QStringLiteral("bool DecodiumBridge::releaseCourtesySignoffIfActive"),
        QStringLiteral("void DecodiumBridge::clearRecentAutoCqAbandoned"));
    QVERIFY(release.contains(QStringLiteral("complete, QSO was already logged\").arg(base), false)")));
    QVERIFY(release.contains(QStringLiteral("clearRecentAutoCqAbandoned(base")));
    QVERIFY(release.contains(QStringLiteral("rememberRecentAutoCqWorked(base")));
    // only when idle in CQ, for a worked/logged partner, at most twice
    const QString start = functionBody(
        cpp,
        QStringLiteral("bool DecodiumBridge::maybeStartCourtesySignoff"),
        QStringLiteral("bool DecodiumBridge::releaseCourtesySignoffIfActive"));
    QVERIFY(start.contains(QStringLiteral("m_currentTx != 6")));
    QVERIFY(start.contains(QStringLiteral("isRecentAutoCqWorkedOrLoggedDuplicate(partnerBase")));
    QVERIFY(start.contains(QStringLiteral("courtesySignoffDue(msg, int(sent.size()), 2)")));
    QVERIFY(start.contains(QStringLiteral("m_qsoLogged = true;")));
    // triggered from both "already worked" skip branches, only for messages directed at us
    QCOMPARE(cpp.count(QStringLiteral("maybeStartCourtesySignoff(messagePartner, messagePartnerBase, msg);")), 2);
    // the flag never outlives the QSO state
    const QString reset = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::resetQsoStateForNextSequence"),
        QStringLiteral("bool DecodiumBridge::finishAutoSequenceQsoFromCompletedSignoff"));
    QVERIFY(reset.contains(QStringLiteral("m_courtesySignoffBase.clear();")));
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
    QVERIFY(ensure.contains(QStringLiteral("bool const useMultiStream = multiStreamActive() && !m_telemetryTxActive;")));
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
        QStringLiteral("void DecodiumBridge::mamDispatchPeriod(bool elastic)"),
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
