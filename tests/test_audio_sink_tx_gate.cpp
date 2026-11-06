#include <QtTest>

#include "src/bridge/DecodiumAudioSink.h"

class TestAudioSinkTxGate final : public QObject
{
    Q_OBJECT

private slots:
    void discardsTxSamplesWithoutClosingInput();
    void silentInputStillReportsPcm();
    void sourceTimeFollowsPcmWithoutDuplicateDelivery();
};

void TestAudioSinkTxGate::discardsTxSamplesWithoutClosingInput()
{
    QVector<short> decoderBuffer;
    QMutex bufferMutex;
    DecodiumAudioSink sink(decoderBuffer, 1, nullptr, &bufferMutex);
    QVERIFY(sink.initialize(QIODevice::WriteOnly, AudioDevice::Mono, 1));

    int callbackCount = 0;
    int healthSignalCount = 0;
    sink.setSampleCallback([&callbackCount](short) { ++callbackCount; });
    connect(&sink, &DecodiumAudioSink::audioHealthChanged,
            this, [&healthSignalCount](double, double, int, int, int) {
                ++healthSignalCount;
            });

    QVector<qint16> const beforeTx {100, -200, 300, -400};
    QCOMPARE(sink.write(reinterpret_cast<char const *>(beforeTx.constData()),
                        static_cast<qint64>(beforeTx.size() * sizeof(qint16))),
             static_cast<qint64>(beforeTx.size() * sizeof(qint16)));
    QCOMPARE(decoderBuffer, QVector<short>({100, -200, 300, -400}));
    QCOMPARE(callbackCount, beforeTx.size());
    QCOMPARE(healthSignalCount, 1);

    sink.setDiscardSamples(true);
    QVERIFY(sink.discardingSamples());
    QVector<qint16> const duringTx {1000, 2000, 3000, 4000};
    QCOMPARE(sink.write(reinterpret_cast<char const *>(duringTx.constData()),
                        static_cast<qint64>(duringTx.size() * sizeof(qint16))),
             static_cast<qint64>(duringTx.size() * sizeof(qint16)));
    QCOMPARE(decoderBuffer, QVector<short>({100, -200, 300, -400}));
    QCOMPARE(callbackCount, beforeTx.size());
    QCOMPARE(healthSignalCount, 1);
    QVERIFY(sink.isOpen());

    sink.setDiscardSamples(false);
    QVERIFY(!sink.discardingSamples());
    QVector<qint16> const afterTx {500, -600};
    QCOMPARE(sink.write(reinterpret_cast<char const *>(afterTx.constData()),
                        static_cast<qint64>(afterTx.size() * sizeof(qint16))),
             static_cast<qint64>(afterTx.size() * sizeof(qint16)));
    QCOMPARE(decoderBuffer,
             QVector<short>({100, -200, 300, -400, 500, -600}));
    QCOMPARE(callbackCount, beforeTx.size() + afterTx.size());
    QCOMPARE(healthSignalCount, 2);
}

void TestAudioSinkTxGate::sourceTimeFollowsPcmWithoutDuplicateDelivery()
{
    QVector<short> buffer;
    QMutex mutex;
    DecodiumAudioSink sink(buffer, 1, nullptr, &mutex);
    QVERIFY(sink.initialize(QIODevice::WriteOnly, AudioDevice::Mono, 1));
    QSignalSpy timed(&sink, &DecodiumAudioSink::audioSamplesProducedWithSourceTime);
    QSignalSpy legacy(&sink, &DecodiumAudioSink::audioSamplesProduced);
    const QVector<qint16> samples {12, -34, 56, -78};
    const auto* data = reinterpret_cast<const char*>(samples.constData());
    const qint64 bytes = samples.size() * sizeof(qint16);
    QCOMPARE(sink.writeWithSourceTime(data, bytes, 123'000'000), bytes);
    QCOMPARE(sink.write(data, bytes), bytes);
    QCOMPARE(timed.size(), 2);
    QCOMPARE(legacy.size(), 2);
    QCOMPARE(timed[0][0].value<QVector<short>>(), samples);
    QCOMPARE(timed[0][1].toLongLong(), qint64 {123'000'000});
    QCOMPARE(timed[1][1].toLongLong(), qint64 {-1});
    sink.setDiscardSamples(true);
    QCOMPARE(sink.writeWithSourceTime(data, bytes, 124'000'000), bytes);
    QCOMPARE(timed.size(), 2);
}

void TestAudioSinkTxGate::silentInputStillReportsPcm()
{
    // The RTTY-exit recovery uses health callbacks to distinguish a missing
    // capture stream from a running stream carrying silence. Zero amplitude
    // must not be mistaken for absence of PCM and trigger endless restarts.
    QVector<short> buffer;
    QMutex mutex;
    DecodiumAudioSink sink(buffer, 1, nullptr, &mutex);
    QVERIFY(sink.initialize(QIODevice::WriteOnly, AudioDevice::Mono, 1));
    QSignalSpy health(&sink, &DecodiumAudioSink::audioHealthChanged);
    QVector<qint16> silence(1200, 0);
    sink.setDiscardSamples(true);
    sink.setDiscardSamples(false);
    sink.resetDecimationPhase();
    const qint64 bytes = silence.size() * sizeof(qint16);
    QCOMPARE(sink.write(reinterpret_cast<const char *>(silence.constData()), bytes), bytes);
    QVERIFY(!health.isEmpty());
    QCOMPARE(health.last().at(0).toDouble(), 0.0);
    QCOMPARE(health.last().at(1).toDouble(), 0.0);
    QCOMPARE(buffer.size(), silence.size());
}

QTEST_GUILESS_MAIN(TestAudioSinkTxGate)

#include "test_audio_sink_tx_gate.moc"
