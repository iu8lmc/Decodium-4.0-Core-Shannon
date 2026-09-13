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

QTEST_APPLESS_MAIN(TestMamTxPayloadPolicy)
#include "test_mam_tx_payload_policy.moc"
