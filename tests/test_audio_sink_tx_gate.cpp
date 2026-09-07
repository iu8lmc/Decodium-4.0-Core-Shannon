#include <QtTest>

#include "src/bridge/DecodiumAudioSink.h"

class TestAudioSinkTxGate final : public QObject
{
    Q_OBJECT

private slots:
    void discardsTxSamplesWithoutClosingInput();
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
    QVector<qint16> const duringTx {1000, 2000, 3000, 4000};
    QCOMPARE(sink.write(reinterpret_cast<char const *>(duringTx.constData()),
                        static_cast<qint64>(duringTx.size() * sizeof(qint16))),
             static_cast<qint64>(duringTx.size() * sizeof(qint16)));
    QCOMPARE(decoderBuffer, QVector<short>({100, -200, 300, -400}));
    QCOMPARE(callbackCount, beforeTx.size());
    QCOMPARE(healthSignalCount, 1);
    QVERIFY(sink.isOpen());

    sink.setDiscardSamples(false);
    QVector<qint16> const afterTx {500, -600};
    QCOMPARE(sink.write(reinterpret_cast<char const *>(afterTx.constData()),
                        static_cast<qint64>(afterTx.size() * sizeof(qint16))),
             static_cast<qint64>(afterTx.size() * sizeof(qint16)));
    QCOMPARE(decoderBuffer,
             QVector<short>({100, -200, 300, -400, 500, -600}));
    QCOMPARE(callbackCount, beforeTx.size() + afterTx.size());
    QCOMPARE(healthSignalCount, 2);
}

QTEST_GUILESS_MAIN(TestAudioSinkTxGate)

#include "test_audio_sink_tx_gate.moc"
