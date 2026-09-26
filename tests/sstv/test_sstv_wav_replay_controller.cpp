// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/integration/SstvWavReplayController.h"

#include <QElapsedTimer>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>

#include <cmath>
#include <cstdint>
#include <stdexcept>

using namespace decodium::sstv;

namespace {

constexpr quint32 kRate = 12'000U;
constexpr double kPi = 3.141592653589793238462643383279502884;

void appendLe16(QByteArray& output, quint16 value)
{
    output.append(static_cast<char>(value & 0xffU));
    output.append(static_cast<char>((value >> 8U) & 0xffU));
}

void appendLe32(QByteArray& output, quint32 value)
{
    output.append(static_cast<char>(value & 0xffU));
    output.append(static_cast<char>((value >> 8U) & 0xffU));
    output.append(static_cast<char>((value >> 16U) & 0xffU));
    output.append(static_cast<char>((value >> 24U) & 0xffU));
}

QByteArray pcm16Wave(const QVector<short>& samples,
                     quint32 sampleRate = kRate)
{
    QByteArray data;
    data.reserve(samples.size() * static_cast<qsizetype>(sizeof(qint16)));
    for (const short sample : samples) {
        appendLe16(data, static_cast<quint16>(sample));
    }

    QByteArray format;
    appendLe16(format, 1U);
    appendLe16(format, 1U);
    appendLe32(format, sampleRate);
    appendLe32(format, sampleRate * 2U);
    appendLe16(format, 2U);
    appendLe16(format, 16U);

    QByteArray body("WAVE", 4);
    body.append("fmt ", 4);
    appendLe32(body, static_cast<quint32>(format.size()));
    body.append(format);
    body.append("data", 4);
    appendLe32(body, static_cast<quint32>(data.size()));
    body.append(data);

    QByteArray wave("RIFF", 4);
    appendLe32(wave, static_cast<quint32>(body.size()));
    wave.append(body);
    return wave;
}

QString writeFixture(QTemporaryDir& directory,
                     const QString& name,
                     const QByteArray& bytes)
{
    const QString path = directory.filePath(name);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        return {};
    }
    return path;
}

SstvRxRuntime::Config runtimeConfig()
{
    SstvRxRuntime::Config config;
    config.ingress.maximumChunks = 3U;
    config.ingress.maximumQueuedSamples = 1'024U;
    config.ingress.maximumSamplesPerCall = 256U;
    config.snapshotNotificationIntervalMs = 20U;
    config.maximumErrorCharacters = 256U;
    return config;
}

SstvWavReplayController::Config replayConfig()
{
    SstvWavReplayController::Config config;
    config.readerLimits.maximumFramesPerRead = 256U;
    config.tailSilenceMs = 50U;
    config.drainTimeoutMs = 10'000U;
    config.backpressurePollMs = 1U;
    config.maximumBufferedChunks = 1U;
    config.maximumBufferedSamples = 256U;
    return config;
}

void appendTone(QVector<short>& output,
                quint32 durationMs,
                double frequencyHz)
{
    const qsizetype count = static_cast<qsizetype>(
        (static_cast<quint64>(kRate) * durationMs) / 1'000U);
    const qsizetype start = output.size();
    output.resize(start + count);
    for (qsizetype index = 0; index < count; ++index) {
        const double phase = 2.0 * kPi * frequencyHz
            * static_cast<double>(start + index)
            / static_cast<double>(kRate);
        output[start + index] = static_cast<short>(
            std::lround(14'000.0 * std::sin(phase)));
    }
}

QVector<short> martinM1VisHeader()
{
    QVector<short> samples;
    samples.reserve(12'000);
    appendTone(samples, 300U, 1'900.0);
    appendTone(samples, 10U, 1'200.0);
    appendTone(samples, 300U, 1'900.0);
    appendTone(samples, 30U, 1'200.0);
    constexpr quint8 payload = 44U;
    bool parity = false;
    for (unsigned int bit = 0U; bit < 7U; ++bit) {
        const bool one = ((payload >> bit) & 1U) != 0U;
        parity = parity != one;
        appendTone(samples, 30U, one ? 1'100.0 : 1'300.0);
    }
    appendTone(samples, 30U, parity ? 1'100.0 : 1'300.0);
    appendTone(samples, 30U, 1'200.0);
    return samples;
}

} // namespace

class TestSstvWavReplayController final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<SstvWavReplayController::State>();
        QVERIFY(QMetaType::fromType<SstvWavReplayController::State>().isValid());
    }

    void configurationAndPathsFailClosed()
    {
        SstvRxRuntime runtime(runtimeConfig());
        SstvWavReplayController::Config invalid = replayConfig();
        invalid.maximumBufferedChunks = 4U;
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            (void) SstvWavReplayController(&runtime, invalid));

        SstvWavReplayController controller(&runtime, replayConfig());
        QVERIFY(!controller.startReplay(
            QUrl(QStringLiteral("https://example.invalid/test.wav"))));
        QCOMPARE(controller.state(), SstvWavReplayController::State::Error);
        QVERIFY(!controller.lastError().isEmpty());
        QVERIFY(!runtime.isRunning());
    }

    void replaysAsynchronouslyAndWaitsForActualDrain()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVector<short> samples(static_cast<qsizetype>(kRate));
        for (qsizetype index = 0; index < samples.size(); ++index) {
            samples[index] = static_cast<short>(
                std::lround(10'000.0 * std::sin(
                    2.0 * kPi * 1'900.0 * static_cast<double>(index)
                    / static_cast<double>(kRate))));
        }
        const QString path = writeFixture(
            directory, QStringLiteral("bounded.wav"), pcm16Wave(samples));
        QVERIFY(!path.isEmpty());

        SstvRxRuntime runtime(runtimeConfig());
        SstvWavReplayController controller(&runtime, replayConfig());
        QSignalSpy finished(&controller,
                            &SstvWavReplayController::replayFinished);
        int ownerTicks = 0;
        QTimer heartbeat;
        heartbeat.setInterval(1);
        connect(&heartbeat, &QTimer::timeout,
                this, [&ownerTicks] { ++ownerTicks; });
        heartbeat.start();

        QElapsedTimer callDuration;
        callDuration.start();
        QVERIFY(controller.startReplay(QUrl::fromLocalFile(path)));
        QVERIFY(callDuration.elapsed() < 100);
        QVERIFY(controller.active());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10'000);
        heartbeat.stop();

        const QList<QVariant> result = finished.takeFirst();
        QCOMPARE(result.at(0).toBool(), true);
        QCOMPARE(result.at(1).toBool(), false);
        QVERIFY(result.at(2).toULongLong() != 0U);
        QCOMPARE(controller.state(),
                 SstvWavReplayController::State::Completed);
        QCOMPARE(controller.progress(), 1.0);
        QCOMPARE(controller.sampleRate(), kRate);
        QCOMPARE(controller.durationMs(), quint64 {1'000U});
        QVERIFY(ownerTicks > 0);

        const SstvRxRuntime::Snapshot snapshot = runtime.snapshot();
        QCOMPARE(snapshot.route.source.kind, SstvAudioSourceKind::Replay);
        QCOMPARE(snapshot.ingress.queue.queuedChunks, std::size_t {0U});
        QCOMPARE(snapshot.ingress.queue.queuedSamples, std::size_t {0U});
        QCOMPARE(snapshot.ingress.queue.droppedChunks, quint64 {0U});
        QCOMPARE(snapshot.processingFailures, quint64 {0U});
        QCOMPARE(snapshot.generationChunksProcessed,
                 snapshot.ingress.acceptedChunks);
        QVERIFY(snapshot.generationChunksProcessed > 1U);
        QVERIFY(runtime.stop());
    }

    void replayUsesTheSameVisAndRxRuntime()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVector<short> samples = martinM1VisHeader();
        samples.append(QVector<short>(2'400, short {0}));
        const QString path = writeFixture(
            directory, QStringLiteral("vis.wav"), pcm16Wave(samples));
        QVERIFY(!path.isEmpty());

        SstvRxRuntime::Config rx = runtimeConfig();
        rx.ingress.maximumChunks = 8U;
        rx.ingress.maximumQueuedSamples = 8'192U;
        rx.ingress.maximumSamplesPerCall = 1'024U;
        SstvRxRuntime runtime(rx);
        SstvWavReplayController::Config replay = replayConfig();
        replay.readerLimits.maximumFramesPerRead = 1'024U;
        replay.maximumBufferedChunks = 2U;
        replay.maximumBufferedSamples = 2'048U;
        replay.tailSilenceMs = 250U;
        SstvWavReplayController controller(&runtime, replay);
        QSignalSpy finished(&controller,
                            &SstvWavReplayController::replayFinished);

        QVERIFY(controller.startReplay(QUrl::fromLocalFile(path)));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10'000);
        QCOMPARE(controller.state(),
                 SstvWavReplayController::State::Completed);
        const SstvRxRuntime::Snapshot snapshot = runtime.snapshot();
        QVERIFY(snapshot.vis.available);
        QVERIFY(snapshot.vis.valid);
        QCOMPARE(snapshot.vis.primaryPayload, 44);
        QCOMPARE(snapshot.vis.mappedMode, QStringLiteral("martin-m1"));
        QCOMPARE(snapshot.ingress.queue.droppedChunks, quint64 {0U});
        QVERIFY(runtime.stop());
    }

    void cancellationIsPromptAndGenerationSafe()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QVector<short> samples(240'000, short {0});
        const QString path = writeFixture(
            directory, QStringLiteral("cancel.wav"), pcm16Wave(samples));
        QVERIFY(!path.isEmpty());

        SstvRxRuntime runtime(runtimeConfig());
        SstvWavReplayController::Config replay = replayConfig();
        replay.readerLimits.maximumFramesPerRead = 1U;
        replay.maximumBufferedSamples = 1U;
        SstvWavReplayController controller(&runtime, replay);
        QSignalSpy finished(&controller,
                            &SstvWavReplayController::replayFinished);

        QVERIFY(controller.startReplay(QUrl::fromLocalFile(path)));
        const quint64 firstSession = controller.sessionId();
        controller.cancel();
        QCOMPARE(controller.state(),
                 SstvWavReplayController::State::Cancelling);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5'000);
        const QList<QVariant> result = finished.takeFirst();
        QCOMPARE(result.at(0).toBool(), false);
        QCOMPARE(result.at(1).toBool(), true);
        QCOMPARE(result.at(2).toULongLong(), firstSession);
        QCOMPARE(controller.state(),
                 SstvWavReplayController::State::Cancelled);
        QVERIFY(controller.lastError().isEmpty());

        QVERIFY(controller.startReplay(QUrl::fromLocalFile(path)));
        QVERIFY(controller.sessionId() != firstSession);
        controller.cancel();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5'000);
        QCOMPARE(finished.at(0).at(2).toULongLong(), controller.sessionId());
        QCOMPARE(controller.state(),
                 SstvWavReplayController::State::Cancelled);
        QVERIFY(runtime.stop());
    }

    void routeChangeFailsClosedWithoutLateRelabeling()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QVector<short> samples(120'000, short {0});
        const QString path = writeFixture(
            directory, QStringLiteral("route-change.wav"),
            pcm16Wave(samples));
        QVERIFY(!path.isEmpty());

        SstvRxRuntime runtime(runtimeConfig());
        SstvWavReplayController::Config replay = replayConfig();
        replay.readerLimits.maximumFramesPerRead = 1U;
        replay.maximumBufferedSamples = 1U;
        SstvWavReplayController controller(&runtime, replay);
        QSignalSpy finished(&controller,
                            &SstvWavReplayController::replayFinished);

        QVERIFY(controller.startReplay(QUrl::fromLocalFile(path)));
        QVERIFY(runtime.switchSource(SstvAudioSourceKind::LocalSoundCard,
                                     0x1234U));
        const quint64 liveGeneration = runtime.routeToken().generation;
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5'000);
        QCOMPARE(controller.state(), SstvWavReplayController::State::Error);
        QVERIFY(controller.lastError().contains(
            QStringLiteral("route"), Qt::CaseInsensitive));
        const quint64 acceptedAfterFinish =
            runtime.snapshot().ingress.acceptedChunks;
        QTest::qWait(50);
        QCOMPARE(runtime.routeToken().source.kind,
                 SstvAudioSourceKind::LocalSoundCard);
        QCOMPARE(runtime.routeToken().generation, liveGeneration);
        QCOMPARE(runtime.snapshot().ingress.acceptedChunks,
                 acceptedAfterFinish);
        QVERIFY(runtime.stop());
    }

    void shutdownJoinsAnActiveWorker()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QVector<short> samples(120'000, short {0});
        const QString path = writeFixture(
            directory, QStringLiteral("shutdown.wav"), pcm16Wave(samples));
        QVERIFY(!path.isEmpty());

        SstvRxRuntime runtime(runtimeConfig());
        SstvWavReplayController::Config replay = replayConfig();
        replay.readerLimits.maximumFramesPerRead = 1U;
        replay.maximumBufferedSamples = 1U;
        SstvWavReplayController controller(&runtime, replay);
        QVERIFY(controller.startReplay(QUrl::fromLocalFile(path)));
        controller.shutdown();
        QCOMPARE(controller.state(),
                 SstvWavReplayController::State::Shutdown);
        QVERIFY(!controller.active());
        QVERIFY(!controller.canStart());
        QVERIFY(runtime.stop());
    }
};

QTEST_GUILESS_MAIN(TestSstvWavReplayController)
#include "test_sstv_wav_replay_controller.moc"
