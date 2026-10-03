// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/image/SstvImageFrame.h"
#include "src/sstv/integration/SstvRxRuntime.h"
#include "src/sstv/integration/SstvWavReplayController.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QString>
#include <QtTest>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#ifndef DECODIUM_SSTV_EXTERNAL_VECTOR_DIR
#error "DECODIUM_SSTV_EXTERNAL_VECTOR_DIR must name the verified vector pack"
#endif

using namespace decodium::sstv;

namespace {

constexpr qint64 kMaximumVectorBytes = 4LL * 1'024LL * 1'024LL;
constexpr qint64 kMaximumSourceImageBytes = 1LL * 1'024LL * 1'024LL;

QByteArray boundedSha256(const QString& path, qint64 maximumBytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0
        || file.size() > maximumBytes) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 consumed = 0;
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(64 * 1'024);
        if (chunk.isEmpty() && !file.atEnd()) {
            return {};
        }
        if (consumed > maximumBytes - chunk.size()) {
            return {};
        }
        consumed += chunk.size();
        hash.addData(chunk);
    }
    return consumed == file.size() ? hash.result().toHex() : QByteArray {};
}

QString checkedFixturePath(const QDir& root, const QString& fileName)
{
    const QFileInfo rootInfo(root.absolutePath());
    const QString canonicalRoot = rootInfo.canonicalFilePath();
    const QFileInfo candidate(root.filePath(fileName));
    const QString canonicalCandidate = candidate.canonicalFilePath();
    if (canonicalRoot.isEmpty() || canonicalCandidate.isEmpty()
        || candidate.isSymLink() || !candidate.isFile()
        || !canonicalCandidate.startsWith(
            canonicalRoot + QDir::separator())) {
        return {};
    }
    return canonicalCandidate;
}

SstvRxRuntime::Config runtimeConfig()
{
    SstvRxRuntime::Config config;
    config.ingress.maximumChunks = 64U;
    config.ingress.maximumQueuedSamples = 262'144U;
    config.ingress.maximumSamplesPerCall = 4'096U;
    config.snapshotNotificationIntervalMs = 25U;
    config.maximumErrorCharacters = 512U;
    return config;
}

SstvWavReplayController::Config replayConfig()
{
    SstvWavReplayController::Config config;
    config.readerLimits.maximumFramesPerRead = 4'096U;
    config.tailSilenceMs = 2'000U;
    config.drainTimeoutMs = 120'000U;
    config.backpressurePollMs = 1U;
    config.maximumBufferedChunks = 8U;
    config.maximumBufferedSamples = 32'768U;
    return config;
}

double imagePsnr(const SstvImageSnapshot& actual,
                 const QImage& source,
                 bool grayscale)
{
    if (source.isNull() || actual.width == 0U || actual.height == 0U
        || actual.pixels.size()
            != static_cast<std::size_t>(actual.width) * actual.height) {
        return 0.0;
    }
    const QImage expected = source.scaled(
        static_cast<int>(actual.width), static_cast<int>(actual.height),
        Qt::IgnoreAspectRatio, Qt::FastTransformation)
        .convertToFormat(QImage::Format_RGB888);
    if (expected.isNull()) {
        return 0.0;
    }

    long double squaredError = 0.0L;
    std::uint64_t components = 0U;
    for (std::uint32_t y = 0U; y < actual.height; ++y) {
        for (std::uint32_t x = 0U; x < actual.width; ++x) {
            const QColor wanted = expected.pixelColor(
                static_cast<int>(x), static_cast<int>(y));
            const SstvRgbPixel& received = actual.pixel(x, y);
            if (grayscale) {
                const int level = qGray(wanted.rgb());
                const int receivedLevel = (static_cast<int>(received.red)
                    + static_cast<int>(received.green)
                    + static_cast<int>(received.blue)) / 3;
                const long double difference =
                    static_cast<long double>(receivedLevel - level);
                squaredError += difference * difference;
                ++components;
            } else {
                const int differences[3] = {
                    static_cast<int>(received.red) - wanted.red(),
                    static_cast<int>(received.green) - wanted.green(),
                    static_cast<int>(received.blue) - wanted.blue(),
                };
                for (const int difference : differences) {
                    const long double value =
                        static_cast<long double>(difference);
                    squaredError += value * value;
                    ++components;
                }
            }
        }
    }
    if (components == 0U) {
        return 0.0;
    }
    const long double meanSquaredError = squaredError
        / static_cast<long double>(components);
    if (meanSquaredError <= std::numeric_limits<long double>::epsilon()) {
        return std::numeric_limits<double>::infinity();
    }
    return static_cast<double>(10.0L * std::log10(
        (255.0L * 255.0L) / meanSquaredError));
}

} // namespace

class TestSstvExternalVectors final : public QObject
{
    Q_OBJECT

private slots:
    void decodesPinnedPysstvWaveform_data();
    void decodesPinnedPysstvWaveform();
};

void TestSstvExternalVectors::decodesPinnedPysstvWaveform_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QByteArray>("sha256");
    QTest::addColumn<QString>("expectedMode");
    QTest::addColumn<quint32>("expectedWidth");
    QTest::addColumn<quint32>("expectedHeight");
    QTest::addColumn<bool>("grayscale");

    QTest::newRow("pysstv-robot-36")
        << QStringLiteral("pysstv-robot36-12000.wav")
        << QByteArrayLiteral(
            "6d5164a9294cbc597a7ef6494efea15a02d5a6267662e5ff98023acbed4bf0cb")
        << QStringLiteral("robot-c36") << quint32 {320U} << quint32 {240U}
        << false;
    QTest::newRow("pysstv-robot-8-bw")
        << QStringLiteral("pysstv-robot8bw-12000.wav")
        << QByteArrayLiteral(
            "660d52ca4427d4d3271281285336bc3feb86559615b005066667c4dc233ecaf0")
        << QStringLiteral("robot-bw8") << quint32 {160U} << quint32 {120U}
        << true;
    QTest::newRow("pysstv-martin-m2")
        << QStringLiteral("pysstv-martinm2-12000.wav")
        << QByteArrayLiteral(
            "4cad290aec3ee249541bcd56c85717263e3d03af18755d806d5e4418085152d5")
        // PySSTV samples 160 effective columns; Decodium preserves the
        // documented 320-column wire/display raster and expands those samples.
        << QStringLiteral("martin-m2") << quint32 {320U} << quint32 {256U}
        << false;
}

void TestSstvExternalVectors::decodesPinnedPysstvWaveform()
{
    QFETCH(QString, fileName);
    QFETCH(QByteArray, sha256);
    QFETCH(QString, expectedMode);
    QFETCH(quint32, expectedWidth);
    QFETCH(quint32, expectedHeight);
    QFETCH(bool, grayscale);

    const QDir root(QStringLiteral(DECODIUM_SSTV_EXTERNAL_VECTOR_DIR));
    const QString vectorPath = checkedFixturePath(root, fileName);
    QVERIFY2(!vectorPath.isEmpty(), qPrintable(fileName));
    QCOMPARE(boundedSha256(vectorPath, kMaximumVectorBytes), sha256);

    const QString sourcePath = checkedFixturePath(
        root, QStringLiteral("pysstv-source-320x256.png"));
    QVERIFY2(!sourcePath.isEmpty(), "pinned PySSTV source image is missing");
    QCOMPARE(boundedSha256(sourcePath, kMaximumSourceImageBytes),
             QByteArrayLiteral(
                 "dc9668e65fc38a2db7ad3a703287ffd16077bb1015c8118e06ca46204235e2e0"));
    const QImage source(sourcePath);
    QVERIFY2(!source.isNull(), "pinned PySSTV source image is invalid");

    SstvRxRuntime runtime(runtimeConfig());
    SstvWavReplayController replay(&runtime, replayConfig());
    QSignalSpy finished(&replay, &SstvWavReplayController::replayFinished);
    QVERIFY2(finished.isValid(), "replay completion signal is unavailable");
    QVERIFY2(replay.startReplay(QUrl::fromLocalFile(vectorPath)),
             qPrintable(replay.lastError()));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 180'000);
    const QList<QVariant> outcome = finished.takeFirst();
    QVERIFY2(outcome.at(0).toBool(), qPrintable(replay.lastError()));
    QCOMPARE(outcome.at(1).toBool(), false);
    QCOMPARE(replay.state(), SstvWavReplayController::State::Completed);

    const SstvRxRuntime::Snapshot snapshot = runtime.snapshot();
    qInfo().noquote()
        << "external vector runtime" << expectedMode
        << "vis" << snapshot.vis.valid
        << "mapped" << snapshot.vis.mappedMode
        << "image" << snapshot.image.available
        << "complete" << snapshot.image.complete
        << "lines" << snapshot.image.linesPublished
        << "coverage" << snapshot.image.coverage
        << "syncs" << snapshot.sync.pulseCount
        << "current-line" << snapshot.sync.currentLine
        << "slant-ppm" << snapshot.slant.appliedClockErrorPpm
        << "afc-hz" << snapshot.afc.correctionHz;
    QVERIFY2(snapshot.vis.available, "external vector produced no VIS event");
    QVERIFY2(snapshot.vis.valid, "external vector VIS was rejected");
    QCOMPARE(snapshot.vis.mappedMode, expectedMode);
    QVERIFY2(snapshot.image.available,
             "external vector produced no progressive image");
    QVERIFY2(snapshot.image.complete,
             "external vector did not complete the image");
    QCOMPARE(snapshot.image.mode, expectedMode);
    QCOMPARE(snapshot.image.width, expectedWidth);
    QCOMPARE(snapshot.image.height, expectedHeight);
    QCOMPARE(snapshot.image.coverage, 1.0);
    QCOMPARE(snapshot.processingFailures, quint64 {0U});
    QCOMPARE(snapshot.ingress.queue.droppedChunks, quint64 {0U});
    QCOMPARE(snapshot.ingress.queue.droppedSamples, quint64 {0U});

    const std::shared_ptr<const SstvImageSnapshot> image =
        runtime.latestImageSnapshot();
    QVERIFY2(image != nullptr, "external vector image copy is unavailable");
    QVERIFY(image->isComplete());
    const double psnr = imagePsnr(*image, source, grayscale);
    qInfo("external vector %s PSNR %.3f dB",
          qPrintable(expectedMode), psnr);
    QVERIFY2(std::isfinite(psnr) && psnr >= 18.0,
             qPrintable(QStringLiteral("external vector PSNR %1 dB is below 18 dB")
                 .arg(psnr, 0, 'f', 3)));
    QVERIFY(runtime.stop());
}

QTEST_GUILESS_MAIN(TestSstvExternalVectors)

#include "test_sstv_external_vectors.moc"
