// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/integration/SstvRxAudioJobController.h"
#include "../../src/sstv/analog/SstvMartinM1.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <cmath>
#include <cstddef>
#include <cstdint>

using namespace decodium::sstv;

namespace {

constexpr int kSampleRate = 12'000;

SstvRxRuntime::Config boundedConfig()
{
    SstvRxRuntime::Config config;
    config.ingress.maximumChunks = 8U;
    config.ingress.maximumQueuedSamples = 48'000U;
    config.ingress.maximumSamplesPerCall = 24'000U;
    config.snapshotNotificationIntervalMs = 20U;
    config.maximumDiagnosticScopePoints = 8U;
    return config;
}

QVector<short> diagnosticTone(std::size_t count,
                              std::size_t firstSample = 0U)
{
    constexpr double kPi = 3.141592653589793238462643383279502884;
    QVector<short> result(static_cast<qsizetype>(count));
    for (std::size_t index = 0U; index < count; ++index) {
        const double phase = 2.0 * kPi * 1'900.0
            * static_cast<double>(firstSample + index)
            / static_cast<double>(kSampleRate);
        result[static_cast<qsizetype>(index)] = static_cast<short>(
            std::lround(14'000.0 * std::sin(phase)));
    }
    return result;
}

void fillRetainedAudio(SstvRxRuntime& runtime, bool retainRawAudio = true)
{
    SstvRxControlSettings settings = runtime.rxControlSnapshot().settings;
    settings.replayRetentionSeconds = 5U;
    settings.retainRawAudio = retainRawAudio;
    QVERIFY(runtime.replaceRxControlSettings(settings));
    QVERIFY(runtime.start(SstvAudioSourceKind::Replay, 501U));
    const SstvRxRouteToken token = runtime.routeToken();
    QVERIFY(token.valid());
    QVERIFY(runtime.enqueuePcm16At(
        diagnosticTone(24'000U), kSampleRate, token, 0));
    QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed,
                              std::uint64_t {1U}, 4'000);
    QTRY_VERIFY_WITH_TIMEOUT(
        runtime.snapshot().replay.retainedSamples >= 23'000U, 4'000);
    QVERIFY(runtime.snapshot().replay.retainedSamples <= 24'000U);
    QCOMPARE(runtime.snapshot().replay.capacitySamples,
             std::size_t {60'000U});
}

QByteArray readAll(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

} // namespace

class TestSstvRxAudioJobController final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void recentBufferIncludesAudioBeforeActiveAcquisition_data()
    {
        QTest::addColumn<int>("leadSeconds");
        QTest::newRow("a-one-second") << 1;
        QTest::newRow("b-two-seconds") << 2;
    }

    void recentBufferIncludesAudioBeforeActiveAcquisition()
    {
        QFETCH(int, leadSeconds);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SstvRxRuntime runtime(boundedConfig());
        QVERIFY(runtime.start(SstvAudioSourceKind::Replay, 502U));
        SstvMartinM1EncoderConfig config;
        config.mode = SstvMartinMode::M1;
        config.sampleRate = kSampleRate;
        std::vector<SstvRgbPixel> pixels(SstvMartinM1Encoder::pixelCount(config.mode),
                                         SstvRgbPixel {210U, 70U, 145U});
        SstvMartinM1Encoder encoder(pixels, config);
        QVector<short> samples = diagnosticTone(static_cast<std::size_t>(leadSeconds * kSampleRate));
        std::vector<std::int16_t> payload(36'000U);
        QCOMPARE(encoder.pullPcm16(payload.data(), payload.size()), payload.size());
        for (const auto value : payload) samples.append(value);
        const auto token = runtime.routeToken();
        std::uint64_t chunks = 0U;
        for (qsizetype offset = 0; offset < samples.size(); offset += 4'000) {
            const qsizetype count = std::min(qsizetype {4'000}, samples.size() - offset);
            QVERIFY(runtime.enqueuePcm16At(samples.mid(offset, count), kSampleRate, token,
                static_cast<qint64>(offset) * 1'000'000'000LL / kSampleRate));
            ++chunks;
            QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot().chunksProcessed, chunks, 4'000);
        }
        const auto state = runtime.snapshot();
        QVERIFY(state.image.acquisitionId != 0U);
        QCOMPARE(state.replay.mostRecentAcquisitionId & 0xffff'ffffULL,
                 state.image.acquisitionId);
        QCOMPARE(state.replay.mostRecentAcquisitionId >> 32U,
                 state.image.generation & 0xffff'ffffULL);
        QVERIFY(runtime.retainedAudioForAcquisition(state.replay.mostRecentAcquisitionId).has_value());
        SstvRxAudioJobController controller(&runtime);
        QSignalSpy finished(&controller, &SstvRxAudioJobController::rawAudioExportFinished);
        const QString imagePath = directory.filePath(QStringLiteral("image.wav"));
        QVERIFY(controller.exportRawAudio(QUrl::fromLocalFile(imagePath), state.replay.mostRecentAcquisitionId));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 8'000);
        QVERIFY(finished.takeFirst().at(0).toBool());
        const QString automaticPath = directory.filePath(QStringLiteral("automatic.wav"));
        QVERIFY(controller.exportRawAudio(QUrl::fromLocalFile(automaticPath)));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 8'000);
        const auto automatic = finished.takeFirst();
        QVERIFY(automatic.at(0).toBool());
        QCOMPARE(automatic.at(2).toULongLong(), state.replay.mostRecentAcquisitionId);
        QCOMPARE(readAll(automaticPath), readAll(imagePath));
        const QString recentPath = directory.filePath(QStringLiteral("recent.wav"));
        QVERIFY(controller.exportRawAudio(QUrl::fromLocalFile(recentPath), state.replay.mostRecentAcquisitionId, true));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 8'000);
        const auto result = finished.takeFirst();
        QVERIFY(result.at(0).toBool());
        QCOMPARE(result.at(2).toULongLong(), quint64 {0});
        QVERIFY(readAll(recentPath).size() > readAll(imagePath).size() + kSampleRate / 2);
        controller.shutdown();
        QVERIFY(runtime.shutdown());
    }

    void exportsBoundedRawWavAndMetadataOffThread_data()
    {
        QTest::addColumn<bool>("retainRawAudio");
        QTest::newRow("diagnostic-retention-enabled") << true;
        QTest::newRow("live-default-no-image-no-raw-retention") << false;
    }

    void exportsBoundedRawWavAndMetadataOffThread()
    {
        QFETCH(bool, retainRawAudio);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SstvRxRuntime runtime(boundedConfig());
        fillRetainedAudio(runtime, retainRawAudio);
        // No VIS or image was received: exporting the recent PCM must still
        // work, including with the live receiver's default retention setting.
        QVERIFY(!runtime.snapshot().vis.valid);
        QVERIFY(!runtime.snapshot().image.available);
        QCOMPARE(runtime.snapshot().image.acquisitionId, std::uint64_t {0U});
        QCOMPARE(runtime.snapshot().replay.mostRecentAcquisitionId,
                 std::uint64_t {0U});
        SstvRxAudioJobController controller(&runtime);
        QSignalSpy finished(&controller,
                            &SstvRxAudioJobController::rawAudioExportFinished);
        QVERIFY(finished.isValid());

        const QString wavPath = directory.filePath(
            QStringLiteral("retained.wav"));
        QVERIFY(controller.exportRawAudio(QUrl::fromLocalFile(wavPath)));
        QVERIFY(controller.busy());
        QVERIFY(!controller.exportRawAudio(
            QUrl::fromLocalFile(directory.filePath(
                QStringLiteral("concurrent.wav")))));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 8'000);

        const QList<QVariant> result = finished.takeFirst();
        QVERIFY(result.at(0).toBool());
        QCOMPARE(result.at(1).toString(), wavPath);
        QCOMPARE(result.at(2).toULongLong(), quint64 {0U});
        QVERIFY(result.at(3).toString().isEmpty());
        QCOMPARE(controller.state(),
                 SstvRxAudioJobController::State::Completed);
        QVERIFY(!controller.busy());
        const QByteArray wav = readAll(wavPath);
        QVERIFY(wav.size() > 44);
        QCOMPARE(wav.first(4), QByteArray("RIFF"));
        QCOMPARE(wav.mid(8, 4), QByteArray("WAVE"));
        QCOMPARE(wav.size(), 44 + static_cast<qsizetype>(
            runtime.snapshot().replay.retainedSamples * 2U));
        QVERIFY(QFileInfo(wavPath).size()
                <= 44 + static_cast<qint64>(
                    runtime.snapshot().replay.capacitySamples * 2U));

        const QString metadataPath =
            SstvWavExporter::metadataPathForWav(wavPath);
        const QJsonDocument metadata = QJsonDocument::fromJson(
            readAll(metadataPath));
        QVERIFY(metadata.isObject());
        const QJsonObject diagnostic = metadata.object().value(
            QStringLiteral("metadata")).toObject();
        QVERIFY(diagnostic.value(
            QStringLiteral("diagnosticRawAudio")).toBool());
        QCOMPARE(diagnostic.value(
            QStringLiteral("sampleRateHz")).toInt(), kSampleRate);
        QCOMPARE(diagnostic.value(QStringLiteral("acquisitionId")).toInteger(),
                 qint64 {0});

        // A stale selected acquisition must not replace an explicitly
        // requested recent-buffer export or leak into its metadata.
        const QString recentPath = directory.filePath(QStringLiteral("recent.wav"));
        QVERIFY(controller.exportRawAudio(QUrl::fromLocalFile(recentPath), 999U, true));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 8'000);
        const QList<QVariant> recent = finished.takeFirst();
        QVERIFY(recent.at(0).toBool());
        QCOMPARE(recent.at(2).toULongLong(), quint64 {0});
        QCOMPARE(readAll(recentPath), wav);
        const QJsonObject recentMetadata = QJsonDocument::fromJson(readAll(
            SstvWavExporter::metadataPathForWav(recentPath))).object().value(
                QStringLiteral("metadata")).toObject();
        QCOMPARE(recentMetadata.value(QStringLiteral("acquisitionId")).toInteger(), qint64 {0});
        const QString expiredPath = directory.filePath(QStringLiteral("expired.wav"));
        QVERIFY(controller.exportRawAudio(QUrl::fromLocalFile(expiredPath), 999U));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 8'000);
        const auto expired = finished.takeFirst();
        QVERIFY(expired.at(0).toBool());
        QCOMPARE(expired.at(2).toULongLong(), quint64 {0});
        QCOMPARE(readAll(expiredPath), wav);

        controller.shutdown();
        QCOMPARE(controller.state(),
                 SstvRxAudioJobController::State::Shutdown);
        QVERIFY(runtime.shutdown());
    }

    void preparesParameterizedRedecodeAndDiscardsPrivateWav()
    {
        SstvRxRuntime runtime(boundedConfig());
        fillRetainedAudio(runtime);
        SstvRxAudioJobController controller(&runtime);
        QSignalSpy prepared(&controller,
                            &SstvRxAudioJobController::redecodePrepared);
        QVERIFY(prepared.isValid());

        SstvRxRedecodeParameters parameters;
        parameters.mode = "martin-m1";
        parameters.afcMode = SstvRxAfcMode::Manual;
        parameters.frequencyCorrectionHz = 100.0;
        parameters.slantMode = SstvRxSlantMode::Manual;
        parameters.clockErrorPpm = -300.0;
        QVERIFY(controller.prepareRecentRedecode(parameters));
        QTRY_COMPARE_WITH_TIMEOUT(prepared.count(), 1, 8'000);

        const QList<QVariant> result = prepared.takeFirst();
        QVERIFY(result.at(0).toBool());
        const QUrl privateWav = result.at(1).toUrl();
        QVERIFY(privateWav.isLocalFile());
        QVERIFY(QFileInfo::exists(privateWav.toLocalFile()));
        QCOMPARE(controller.preparedRedecodeParameters().mode,
                 std::string("martin-m1"));
        QCOMPARE(controller.preparedRedecodeParameters()
                     .frequencyCorrectionHz,
                 100.0);
        QCOMPARE(controller.preparedRedecodeParameters().clockErrorPpm,
                 -300.0);

        controller.discardPreparedRedecode();
        QVERIFY(!QFileInfo::exists(privateWav.toLocalFile()));
        controller.shutdown();
        QVERIFY(!controller.prepareRecentRedecode(parameters));
        QVERIFY(runtime.shutdown());
    }

    void shutdownCancelsOrJoinsAnInFlightJobWithoutOrphanThread()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SstvRxRuntime runtime(boundedConfig());
        fillRetainedAudio(runtime);
        SstvRxAudioJobController controller(&runtime);
        QVERIFY(controller.exportRawAudio(QUrl::fromLocalFile(
            directory.filePath(QStringLiteral("shutdown.wav")))));
        controller.shutdown();
        QCOMPARE(controller.state(),
                 SstvRxAudioJobController::State::Shutdown);
        QVERIFY(!controller.busy());
        QVERIFY(runtime.shutdown());
    }
};

QTEST_MAIN(TestSstvRxAudioJobController)

#include "test_sstv_rx_audio_job_controller.moc"
