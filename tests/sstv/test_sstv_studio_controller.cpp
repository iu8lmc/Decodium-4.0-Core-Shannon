// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/integration/SstvStudioController.h"
#include "../../src/sstv/diagnostics/SstvDiagnosticLogging.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSettings>
#include <QTemporaryDir>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <thread>

using namespace decodium::sstv;

namespace {

std::uint16_t littleEndian16(const QByteArray& bytes, qsizetype offset)
{
    return static_cast<std::uint16_t>(
               static_cast<unsigned char>(bytes.at(offset)))
        | static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(
                static_cast<unsigned char>(bytes.at(offset + 1))) << 8U);
}

std::uint32_t littleEndian32(const QByteArray& bytes, qsizetype offset)
{
    return static_cast<std::uint32_t>(
               static_cast<unsigned char>(bytes.at(offset)))
        | (static_cast<std::uint32_t>(
               static_cast<unsigned char>(bytes.at(offset + 1))) << 8U)
        | (static_cast<std::uint32_t>(
               static_cast<unsigned char>(bytes.at(offset + 2))) << 16U)
        | (static_cast<std::uint32_t>(
               static_cast<unsigned char>(bytes.at(offset + 3))) << 24U);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("test cannot read file");
    }
    return file.readAll();
}

} // namespace

class TestSstvStudioController final : public QObject
{
    Q_OBJECT

private slots:
    void executableModeModelIsHonest()
    {
        SstvStudioController controller;
        const QVariantList modes = controller.modes();
        QCOMPARE(modes.size(), 52);
        QCOMPARE(controller.modeId(), QStringLiteral("martin-m1"));
        QCOMPARE(controller.modeName(), QStringLiteral("Martin M1"));
        QCOMPARE(controller.outputSize(), QSize(320, 256));
        QVERIFY(qAbs(controller.estimatedDurationSeconds() - 115.200176)
                < 1.0e-9);

        const struct {
            const char* id;
            const char* name;
            int width;
            int height;
            double duration;
        } expected[] {
            {"martin-m1", "Martin M1", 320, 256, 115.200176},
            {"martin-m2", "Martin M2", 320, 256, 58.970288},
            {"martin-m3", "Martin M3", 320, 128, 58.055088},
            {"martin-m4", "Martin M4", 320, 128, 29.940144},
            {"scottie-s1", "Scottie S1", 320, 256, 110.53432},
            {"scottie-s2", "Scottie S2", 320, 256, 71.999152},
            {"scottie-s3", "Scottie S3", 320, 128, 55.72216},
            {"scottie-s4", "Scottie S4", 320, 128, 36.454576},
            {"scottie-dx", "Scottie DX", 320, 256, 269.7868},
            {"robot-c12", "Robot 12 Colour", 160, 120, 12.91},
            {"robot-c24", "Robot 24 Colour", 320, 120, 24.91},
            {"robot-c36", "Robot 36 Colour", 320, 240, 36.91},
            {"robot-c72", "Robot 72 Colour", 320, 240, 72.91},
            {"robot-bw8", "Robot B/W 8", 160, 120, 8.83},
            {"robot-bw12", "Robot B/W 12", 160, 120, 12.91},
            {"robot-bw24", "Robot B/W 24", 320, 240, 26.11},
            {"robot-bw36", "Robot B/W 36", 320, 240, 36.91},
            {"wraase-sc2-60", "Wraase SC2-60",
             320, 256, 62.4535},
            {"wraase-sc2-120", "Wraase SC2-120",
             320, 256, 122.64376},
            {"wraase-sc2-180", "Wraase SC2-180",
             320, 256, 182.93176},
            {"pasokon-p3", "Pasokon P3", 640, 496, 203.96},
            {"pasokon-p5", "Pasokon P5", 640, 496, 305.485},
            {"pasokon-p7", "Pasokon P7", 640, 496, 407.01},
            {"pd-50", "PD50", 320, 256, 50.59448},
            {"pd-90", "PD90", 320, 256, 90.89912},
            {"pd-120", "PD120", 640, 496, 127.01304},
            {"pd-160", "PD160", 512, 400, 161.7932},
            {"pd-180", "PD180", 640, 496, 187.96152},
            {"pd-240", "PD240", 640, 496, 248.91},
            {"pd-290", "PD290", 800, 616, 289.59224},
            {"avt-24", "AVT24", 128, 120, 30.5425},
            {"avt-90", "AVT90", 320, 240, 98.0425},
            {"avt-94", "AVT94", 320, 200, 101.7925},
            {"mp-73", "MP73", 320, 256, 74.11},
            {"mp-115", "MP115", 320, 256, 116.606},
            {"mp-140", "MP140", 320, 256, 140.67},
            {"mp-175", "MP175", 320, 256, 176.51},
            {"mr-73", "MR73", 320, 256, 74.4428},
            {"mr-90", "MR90", 320, 256, 91.3388},
            {"mr-115", "MR115", 320, 256, 116.4268},
            {"mr-140", "MR140", 320, 256, 141.5148},
            {"mr-175", "MR175", 320, 256, 176.3308},
            {"ml-180", "ML180", 640, 496, 181.3468},
            {"ml-240", "ML240", 640, 496, 240.8668},
            {"ml-280", "ML280", 640, 496, 281.5388},
            {"ml-320", "ML320", 640, 496, 321.2188},
            {"mp-73-narrow", "MP73-Narrow", 320, 256, 73.91},
            {"mp-110-narrow", "MP110-Narrow", 320, 256, 110.774},
            {"mp-140-narrow", "MP140-Narrow", 320, 256, 140.47},
            {"mc-110-narrow", "MC110-Narrow", 320, 256, 110.646},
            {"mc-140-narrow", "MC140-Narrow", 320, 256, 141.366},
            {"mc-180-narrow", "MC180-Narrow", 320, 256, 181.302},
        };
        for (qsizetype index = 0; index < modes.size(); ++index) {
            const QVariantMap values = modes.at(index).toMap();
            const auto& item = expected[static_cast<std::size_t>(index)];
            QCOMPARE(values.value(QStringLiteral("id")).toString(),
                     QString::fromLatin1(item.id));
            QCOMPARE(values.value(QStringLiteral("name")).toString(),
                     QString::fromLatin1(item.name));
            QCOMPARE(values.value(QStringLiteral("width")).toInt(),
                     item.width);
            QCOMPARE(values.value(QStringLiteral("height")).toInt(),
                     item.height);
            const double duration = values.value(
                QStringLiteral("durationSeconds")).toDouble();
            QVERIFY(qAbs(duration - item.duration) < 1.0e-9);
        }

        controller.setModeId(QStringLiteral("MARTIN-M2"));
        QCOMPARE(controller.modeId(), QStringLiteral("martin-m2"));
        QCOMPARE(controller.modeName(), QStringLiteral("Martin M2"));
        QCOMPARE(controller.outputSize(), QSize(320, 256));
        QVERIFY(qAbs(controller.estimatedDurationSeconds() - 58.970288)
                < 1.0e-9);

        controller.setModeId(QStringLiteral("MARTIN-M4"));
        QCOMPARE(controller.modeId(), QStringLiteral("martin-m4"));
        QCOMPARE(controller.modeName(), QStringLiteral("Martin M4"));
        QCOMPARE(controller.outputSize(), QSize(320, 128));
        QVERIFY(qAbs(controller.estimatedDurationSeconds() - 29.940144)
                < 1.0e-9);

        controller.setModeId(QStringLiteral("SCOTTIE-S4"));
        QCOMPARE(controller.modeId(), QStringLiteral("scottie-s4"));
        QCOMPARE(controller.modeName(), QStringLiteral("Scottie S4"));
        QCOMPARE(controller.outputSize(), QSize(320, 128));
        QVERIFY(controller.estimatedDurationSeconds() > 36.0);
        QVERIFY(controller.estimatedDurationSeconds() < 37.0);

        controller.setModeId(QStringLiteral("SCOTTIE-DX"));
        QCOMPARE(controller.modeId(), QStringLiteral("scottie-dx"));
        QCOMPARE(controller.modeName(), QStringLiteral("Scottie DX"));
        QVERIFY(controller.estimatedDurationSeconds() > 260.0);
        controller.setModeId(QStringLiteral("ROBOT-C12"));
        QCOMPARE(controller.modeId(), QStringLiteral("robot-c12"));
        QCOMPARE(controller.modeName(), QStringLiteral("Robot 12 Colour"));
        QCOMPARE(controller.outputSize(), QSize(160, 120));
        QVERIFY(qAbs(controller.estimatedDurationSeconds() - 12.91)
                < 1.0e-9);

        controller.setModeId(QStringLiteral("PASOKON-P3"));
        QCOMPARE(controller.modeId(), QStringLiteral("pasokon-p3"));
        QCOMPARE(controller.modeName(), QStringLiteral("Pasokon P3"));
        QCOMPARE(controller.outputSize(), QSize(640, 496));
        QVERIFY(qAbs(controller.estimatedDurationSeconds() - 203.96)
                < 1.0e-9);

        controller.setModeId(QStringLiteral("PD-290"));
        QCOMPARE(controller.modeId(), QStringLiteral("pd-290"));
        QCOMPARE(controller.modeName(), QStringLiteral("PD290"));
        QCOMPARE(controller.outputSize(), QSize(800, 616));
        QVERIFY(qAbs(controller.estimatedDurationSeconds() - 289.59224)
                < 1.0e-9);

        controller.setModeId(QStringLiteral("AVT-90"));
        QCOMPARE(controller.modeId(), QStringLiteral("avt-90"));
        QCOMPARE(controller.modeName(), QStringLiteral("AVT90"));
        // The effective 256-column AVT90 resolution stays metadata; Studio
        // prepares the audited common 320-column transmitted raster.
        QCOMPARE(controller.outputSize(), QSize(320, 240));
        QVERIFY(qAbs(controller.estimatedDurationSeconds() - 98.0425)
                < 1.0e-9);

        controller.setModeId(QStringLiteral("MR-175"));
        QCOMPARE(controller.modeId(), QStringLiteral("mr-175"));
        QCOMPARE(controller.modeName(), QStringLiteral("MR175"));
        QCOMPARE(controller.outputSize(), QSize(320, 256));
        QVERIFY(qAbs(controller.estimatedDurationSeconds() - 176.3308)
                < 1.0e-9);

        controller.setModeId(QStringLiteral("MC-110-NARROW"));
        QCOMPARE(controller.modeId(), QStringLiteral("mc-110-narrow"));
        QCOMPARE(controller.modeName(), QStringLiteral("MC110-Narrow"));
        QCOMPARE(controller.outputSize(), QSize(320, 256));
        QVERIFY(qAbs(controller.estimatedDurationSeconds() - 110.646)
                < 1.0e-9);

        controller.setModeId(QStringLiteral("robot-36"));
        QCOMPARE(controller.modeId(), QStringLiteral("mc-110-narrow"));
        QVERIFY(!controller.error().isEmpty());
    }

    void calibrationAndPreparationStayAsSeparateSnapshots()
    {
        SstvStudioController controller;
        controller.setModeId(QStringLiteral("scottie-s4"));
        QSignalSpy sourceSpy(&controller,
                             &SstvStudioController::sourceChanged);
        QSignalSpy preparedSpy(&controller,
                               &SstvStudioController::preparedChanged);
        QVERIFY(controller.generateCalibrationPattern());
        QVERIFY(controller.sourceReady());
        QVERIFY(!controller.preparedReady());
        QCOMPARE(sourceSpy.count(), 1);
        QVERIFY(controller.sourceImageSource().startsWith(
            QStringLiteral("image://decodium-sstv/tx-source/")));

        const auto original = controller.sourceSnapshot();
        QVERIFY(original);
        QCOMPARE(original->size(), QSize(320, 128));
        const QImage originalCopy = original->copy();

        QVariantMap controls;
        controls.insert(QStringLiteral("resizeMode"), QStringLiteral("fit"));
        controls.insert(QStringLiteral("brightness"), 0.1);
        controls.insert(QStringLiteral("contrast"), 1.2);
        controls.insert(QStringLiteral("aspectLock"), true);
        controls.insert(QStringLiteral("borderWidth"), 2);
        controls.insert(QStringLiteral("borderColor"), QStringLiteral("#ffff00"));
        QVariantMap overlay;
        overlay.insert(QStringLiteral("kind"), QStringLiteral("callsign"));
        overlay.insert(QStringLiteral("text"), QStringLiteral("9H1TEST"));
        overlay.insert(QStringLiteral("anchor"), QStringLiteral("bottom-right"));
        controls.insert(QStringLiteral("overlays"), QVariantList {overlay});

        QVERIFY(controller.prepareImage(controls));
        QVERIFY(controller.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5'000);
        QVERIFY2(controller.preparedReady(), qPrintable(controller.error()));
        QVERIFY(controller.preparedImageSource().startsWith(
            QStringLiteral("image://decodium-sstv/tx-prepared/")));
        const auto prepared = controller.preparedSnapshot();
        QVERIFY(prepared);
        QCOMPARE(prepared->size(), QSize(320, 128));
        QVERIFY(*prepared != originalCopy);
        QVERIFY(*controller.sourceSnapshot() == originalCopy);
        QVERIFY(preparedSpy.count() >= 1);
    }

    void localReaderAcceptsContentWithWrongSuffix()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString path = temporary.filePath(QStringLiteral("radio-frame.data"));
        QImage source(27, 19, QImage::Format_RGB32);
        source.fill(QColor(8, 90, 170));
        QVERIFY(source.save(path, "PNG"));

        SstvStudioController controller;
        QVERIFY(controller.loadSource(QUrl::fromLocalFile(path)));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5'000);
        QVERIFY2(controller.sourceReady(), qPrintable(controller.error()));
        QCOMPARE(controller.sourceName(), QStringLiteral("radio-frame.data"));
        QCOMPARE(controller.sourceSnapshot()->size(), QSize(27, 19));

        QVERIFY(!controller.loadSource(QUrl(QStringLiteral("https://example.invalid/a.png"))));
        QVERIFY(!controller.error().isEmpty());
        QCOMPARE(controller.sourceSnapshot()->size(), QSize(27, 19));
    }

    void invalidContentAndControlsFailClosed()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString path = temporary.filePath(QStringLiteral("fake.png"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("this is not an image") > 0);
        file.close();

        SstvStudioController controller;
        QVERIFY(controller.loadSource(QUrl::fromLocalFile(path)));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5'000);
        QVERIFY(!controller.sourceReady());
        QVERIFY(!controller.error().isEmpty());
        QVERIFY(!controller.prepareImage());

        QVERIFY(controller.generateCalibrationPattern());
        QVariantMap controls;
        controls.insert(QStringLiteral("resizeMode"), QStringLiteral("explode"));
        QVERIFY(!controller.prepareImage(controls));
        QVERIFY(!controller.busy());

        controls.clear();
        controls.insert(QStringLiteral("rotation"), 45);
        QVERIFY(!controller.prepareImage(controls));
        controls.clear();
        controls.insert(QStringLiteral("gamma"), 0.0);
        QVERIFY(controller.prepareImage(controls));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5'000);
        QVERIFY(!controller.preparedReady());
        QVERIFY(!controller.error().isEmpty());
    }

    void clipboardAndClearAreNativeAndBounded()
    {
        QImage clipboardImage(40, 30, QImage::Format_ARGB32);
        clipboardImage.fill(QColor(100, 110, 120, 130));
        QGuiApplication::clipboard()->setImage(clipboardImage);

        SstvStudioController controller;
        QVERIFY(controller.pasteSource());
        QCOMPARE(controller.sourceName(), QStringLiteral("Clipboard"));
        QCOMPARE(controller.sourceSnapshot()->size(), QSize(40, 30));
        controller.clearSource();
        QVERIFY(!controller.sourceReady());
        QVERIFY(!controller.preparedReady());
        QVERIFY(controller.sourceImageSource().isEmpty());

        QGuiApplication::clipboard()->clear();
        QVERIFY(!controller.pasteSource());
        QVERIFY(!controller.error().isEmpty());
    }

    void cancellationDiscardsWorkerResult()
    {
        SstvStudioController controller;
        QVERIFY(controller.generateCalibrationPattern());
        QVariantMap controls;
        controls.insert(QStringLiteral("sharpness"), 2.0);
        controls.insert(QStringLiteral("dither"), true);
        QVERIFY(controller.prepareImage(controls));
        controller.cancelWork();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5'000);
        QVERIFY(!controller.preparedReady());
        QVERIFY(controller.error().contains(QStringLiteral("cancel"),
                                            Qt::CaseInsensitive));
        QVERIFY(controller.sourceReady());
    }

    void wavSuggestionIsRootedSanitisedAndBounded()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        SstvStudioController controller;
        QCOMPARE(controller.wavSampleRates(),
                 QVariantList({12'000, 24'000, 48'000}));
        QVERIFY(controller.wavExportFolder().isEmpty());
        QVERIFY(controller.suggestedWavUrl(QStringLiteral("IU8LMC")).isEmpty());

        controller.setWavExportRoot(QStringLiteral("relative/export"));
        QVERIFY(controller.wavExportFolder().isEmpty());
        controller.setWavExportRoot(temporary.path());
        QCOMPARE(controller.wavExportFolder(),
                 QUrl::fromLocalFile(temporary.path()));

        const QUrl suggestion = controller.suggestedWavUrl(
            QStringLiteral("  iu8/lmc portable  "));
        QVERIFY(suggestion.isLocalFile());
        const QFileInfo info(suggestion.toLocalFile());
        QCOMPARE(info.absolutePath(), QDir(temporary.path()).absolutePath());
        const QRegularExpression convention(
            QStringLiteral(
                "^\\d{8}-\\d{9}Z_IU8-LMC-PORTABLE_MARTIN-M1\\.wav$"));
        QVERIFY2(convention.match(info.fileName()).hasMatch(),
                 qPrintable(info.fileName()));
    }

    void asynchronousWavExportWritesCanonicalPcmAndSidecar()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString requestedPath = temporary.filePath(
            QStringLiteral("controller-output"));
        const QString wavPath = requestedPath + QStringLiteral(".wav");
        const QString metadataPath = temporary.filePath(
            QStringLiteral("controller-output.json"));

        SstvStudioController controller;
        QVERIFY(controller.generateCalibrationPattern());
        QVERIFY(controller.prepareImage());
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5'000);
        QVERIFY2(controller.preparedReady(), qPrintable(controller.error()));

        QSignalSpy exportSpy(&controller,
                             &SstvStudioController::wavExportChanged);
        QVERIFY(controller.exportWav(QUrl::fromLocalFile(requestedPath),
                                     12'000,
                                     true,
                                     false,
                                     QStringLiteral(" iu8lmc ")));
        QVERIFY(controller.busy());
        QVERIFY(controller.wavExportBusy());
        QVERIFY(controller.wavExportPath().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!controller.wavExportBusy(), 20'000);

        QVERIFY(!controller.busy());
        QVERIFY2(controller.error().isEmpty(), qPrintable(controller.error()));
        QVERIFY2(controller.wavExportWarning().isEmpty(),
                 qPrintable(controller.wavExportWarning()));
        QCOMPARE(controller.wavExportPath(), wavPath);
        QVERIFY(exportSpy.count() >= 2);
        QVERIFY(QFileInfo::exists(wavPath));
        QVERIFY(QFileInfo::exists(metadataPath));

        const QByteArray wav = readFile(wavPath);
        QVERIFY(wav.size() > 44);
        QCOMPARE(wav.first(4), QByteArray("RIFF"));
        QCOMPARE(wav.mid(8, 4), QByteArray("WAVE"));
        QCOMPARE(wav.mid(12, 4), QByteArray("fmt "));
        QCOMPARE(littleEndian16(wav, 20), std::uint16_t {1U});
        QCOMPARE(littleEndian16(wav, 22), std::uint16_t {1U});
        QCOMPARE(littleEndian32(wav, 24), std::uint32_t {12'000U});
        QCOMPARE(littleEndian16(wav, 34), std::uint16_t {16U});
        QCOMPARE(wav.mid(36, 4), QByteArray("data"));
        QCOMPARE(static_cast<quint64>(littleEndian32(wav, 40)) + 44U,
                 static_cast<quint64>(wav.size()));

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            readFile(metadataPath), &parseError);
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        QVERIFY(document.isObject());
        const QJsonObject sidecar = document.object();
        QCOMPARE(sidecar.value(QStringLiteral("schema")).toString(),
                 QStringLiteral("decodium-sstv-wav-metadata"));
        QCOMPARE(sidecar.value(QStringLiteral("schemaVersion")).toInt(), 1);
        QCOMPARE(sidecar.value(QStringLiteral("wavFile")).toString(),
                 QStringLiteral("controller-output.wav"));
        QCOMPARE(sidecar.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("martin-m1"));
        QCOMPARE(sidecar.value(QStringLiteral("sampleRate")).toInt(), 12'000);
        QCOMPARE(sidecar.value(QStringLiteral("channels")).toInt(), 1);
        QCOMPARE(sidecar.value(QStringLiteral("bitsPerSample")).toInt(), 16);
        QCOMPARE(sidecar.value(QStringLiteral("fileSizeBytes")).toInteger(),
                 static_cast<qint64>(wav.size()));
        QCOMPARE(sidecar.value(QStringLiteral("sha256")).toString(),
                 QString::fromLatin1(
                     QCryptographicHash::hash(wav,
                                              QCryptographicHash::Sha256)
                         .toHex()));

        const QJsonObject metadata = sidecar.value(
            QStringLiteral("metadata")).toObject();
        QCOMPARE(metadata.value(QStringLiteral("sourceName")).toString(),
                 QStringLiteral("Calibration pattern"));
        QCOMPARE(metadata.value(QStringLiteral("modeName")).toString(),
                 QStringLiteral("Martin M1"));
        QCOMPARE(metadata.value(QStringLiteral("preparedWidth")).toInt(), 320);
        QCOMPARE(metadata.value(QStringLiteral("preparedHeight")).toInt(), 256);
        QCOMPARE(metadata.value(QStringLiteral("fskId")).toString(),
                 QStringLiteral("IU8LMC"));
    }

    void martinM4StudioWavUsesSelectedGeometryAndProtocolLength()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString wavPath = temporary.filePath(
            QStringLiteral("martin-m4.wav"));
        const QString metadataPath = temporary.filePath(
            QStringLiteral("martin-m4.json"));

        SstvStudioController controller;
        controller.setModeId(QStringLiteral("martin-m4"));
        QCOMPARE(controller.outputSize(), QSize(320, 128));
        QVERIFY(controller.generateCalibrationPattern());
        QCOMPARE(controller.sourceSnapshot()->size(), QSize(320, 128));
        QVERIFY(controller.prepareImage());
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5'000);
        QVERIFY2(controller.preparedReady(), qPrintable(controller.error()));
        QCOMPARE(controller.preparedSnapshot()->size(), QSize(320, 128));

        QVERIFY(controller.exportWav(QUrl::fromLocalFile(wavPath),
                                     12'000,
                                     true,
                                     false));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.wavExportBusy(), 20'000);
        QVERIFY2(controller.error().isEmpty(), qPrintable(controller.error()));
        QCOMPARE(controller.wavExportPath(), wavPath);

        constexpr std::uint64_t totalFrames = 359'281U;
        const QByteArray wav = readFile(wavPath);
        QCOMPARE(static_cast<std::uint64_t>(wav.size()),
                 std::uint64_t {44U} + totalFrames * 2U);
        QCOMPARE(littleEndian32(wav, 40),
                 static_cast<std::uint32_t>(totalFrames * 2U));

        const QJsonDocument document = QJsonDocument::fromJson(
            readFile(metadataPath));
        QVERIFY(document.isObject());
        const QJsonObject sidecar = document.object();
        QCOMPARE(sidecar.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("martin-m4"));
        QCOMPARE(sidecar.value(QStringLiteral("sampleCount")).toInteger(),
                 static_cast<qint64>(totalFrames));
        const QJsonObject metadata = sidecar.value(
            QStringLiteral("metadata")).toObject();
        QCOMPARE(metadata.value(QStringLiteral("modeName")).toString(),
                 QStringLiteral("Martin M4"));
        QCOMPARE(metadata.value(QStringLiteral("preparedWidth")).toInt(), 320);
        QCOMPARE(metadata.value(QStringLiteral("preparedHeight")).toInt(), 128);
    }

    void wavExportRejectsInvalidRequestsAndNeverClobbers()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString wavPath = temporary.filePath(
            QStringLiteral("protected.wav"));
        const QString sidecarPath = temporary.filePath(
            QStringLiteral("protected.json"));
        const QByteArray wavSentinel("pre-existing-wav");
        const QByteArray sidecarSentinel("pre-existing-sidecar");

        SstvStudioController controller;
        QVERIFY(!controller.exportWav(QUrl::fromLocalFile(wavPath),
                                      12'000, false, false));
        QVERIFY(controller.error().contains(QStringLiteral("prepare"),
                                             Qt::CaseInsensitive));
        QVERIFY(controller.generateCalibrationPattern());
        QVERIFY(controller.prepareImage());
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5'000);
        QVERIFY(controller.preparedReady());

        QVERIFY(!controller.exportWav(
            QUrl(QStringLiteral("https://example.invalid/frame.wav")),
            12'000, false, false));
        QVERIFY(!controller.exportWav(QUrl::fromLocalFile(wavPath),
                                      44'100, false, false));
        QVERIFY(!controller.exportWav(
            QUrl::fromLocalFile(temporary.filePath(QStringLiteral("bad.mp3"))),
            12'000, false, false));
        QVERIFY(!QFileInfo::exists(wavPath));

        QFile wavFile(wavPath);
        QVERIFY(wavFile.open(QIODevice::WriteOnly));
        QCOMPARE(wavFile.write(wavSentinel), wavSentinel.size());
        wavFile.close();
        QFile sidecarFile(sidecarPath);
        QVERIFY(sidecarFile.open(QIODevice::WriteOnly));
        QCOMPARE(sidecarFile.write(sidecarSentinel), sidecarSentinel.size());
        sidecarFile.close();

        QVERIFY(controller.exportWav(QUrl::fromLocalFile(wavPath),
                                     12'000, true, false));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.wavExportBusy(), 20'000);
        QVERIFY(!controller.busy());
        QVERIFY(!controller.error().isEmpty());
        QVERIFY(controller.wavExportPath().isEmpty());
        QCOMPARE(readFile(wavPath), wavSentinel);
        QCOMPARE(readFile(sidecarPath), sidecarSentinel);

        const QString sidecarProtectedWav = temporary.filePath(
            QStringLiteral("sidecar-protected.wav"));
        const QString sidecarProtectedJson = temporary.filePath(
            QStringLiteral("sidecar-protected.json"));
        QFile protectedSidecar(sidecarProtectedJson);
        QVERIFY(protectedSidecar.open(QIODevice::WriteOnly));
        QCOMPARE(protectedSidecar.write(sidecarSentinel),
                 sidecarSentinel.size());
        protectedSidecar.close();

        QVERIFY(controller.exportWav(
            QUrl::fromLocalFile(sidecarProtectedWav),
            12'000, true, false));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.wavExportBusy(), 20'000);
        QVERIFY(!controller.busy());
        QVERIFY2(controller.error().isEmpty(), qPrintable(controller.error()));
        QCOMPARE(controller.wavExportPath(), sidecarProtectedWav);
        QVERIFY(!controller.wavExportWarning().isEmpty());
        QVERIFY(QFileInfo::exists(sidecarProtectedWav));
        QCOMPARE(readFile(sidecarProtectedJson), sidecarSentinel);
    }

    void cancellingAsynchronousWavExportLeavesNoPartialFiles()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString wavPath = temporary.filePath(
            QStringLiteral("cancelled.wav"));

        SstvStudioController controller;
        controller.setModeId(QStringLiteral("scottie-dx"));
        QVERIFY(controller.generateCalibrationPattern());
        QVERIFY(controller.prepareImage());
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5'000);
        QVERIFY(controller.preparedReady());

        QVERIFY(controller.exportWav(QUrl::fromLocalFile(wavPath),
                                     48'000, false, false));
        QVERIFY(controller.wavExportBusy());
        controller.cancelWork();
        QVERIFY(controller.error().contains(QStringLiteral("cancel"),
                                             Qt::CaseInsensitive));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.wavExportBusy(), 20'000);

        QVERIFY(!controller.busy());
        QVERIFY(controller.error().contains(QStringLiteral("cancel"),
                                             Qt::CaseInsensitive));
        QVERIFY(controller.wavExportPath().isEmpty());
        QVERIFY(controller.preparedReady());
        QVERIFY(!QFileInfo::exists(wavPath));
        QVERIFY(!QFileInfo::exists(
            temporary.filePath(QStringLiteral("cancelled.json"))));
        QVERIFY(!QFileInfo::exists(wavPath + QStringLiteral(".lock")));
    }

    void immutableSnapshotsCanBeReadOffOwnerThread()
    {
        SstvStudioController controller;
        QVERIFY(controller.generateCalibrationPattern());
        const auto expected = controller.sourceSnapshot();
        QVERIFY(expected);

        bool coherent = false;
        std::thread reader([&controller, &coherent]() {
            for (int iteration = 0; iteration < 1'000; ++iteration) {
                const auto snapshot = controller.sourceSnapshot();
                if (!snapshot || snapshot->size() != QSize(320, 256)
                    || snapshot->isNull()) {
                    return;
                }
            }
            coherent = true;
        });
        reader.join();
        QVERIFY(coherent);
        QVERIFY(controller.sourceSnapshot() == expected);
    }

    void namedTemplatesPersistMigrateAndEnforceBounds()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString settingsPath = temporary.filePath(
            QStringLiteral("studio.ini"));

        QVariantMap controls;
        controls.insert(QStringLiteral("resizeMode"), QStringLiteral("fill"));
        controls.insert(QStringLiteral("cropX"), 0.1);
        controls.insert(QStringLiteral("cropY"), 0.05);
        controls.insert(QStringLiteral("cropWidth"), 0.8);
        controls.insert(QStringLiteral("cropHeight"), 0.9);
        controls.insert(QStringLiteral("exposure"), 0.75);
        controls.insert(QStringLiteral("whiteBalanceRed"), 1.1);
        controls.insert(QStringLiteral("whiteBalanceGreen"), 0.95);
        controls.insert(QStringLiteral("whiteBalanceBlue"), 1.2);
        controls.insert(QStringLiteral("sharpness"), 0.4);
        QVariantList overlays;
        const QStringList kinds {
            QStringLiteral("callsign"), QStringLiteral("locator"),
            QStringLiteral("utc"), QStringLiteral("frequency"),
            QStringLiteral("mode"), QStringLiteral("custom"),
            QStringLiteral("report"), QStringLiteral("watermark")};
        for (qsizetype index = 0; index < kinds.size(); ++index) {
            QVariantMap overlay;
            overlay.insert(QStringLiteral("kind"), kinds.at(index));
            overlay.insert(QStringLiteral("text"),
                           QStringLiteral("T%1").arg(index));
            overlay.insert(QStringLiteral("anchor"),
                           QStringLiteral("bottom-right"));
            overlay.insert(QStringLiteral("fontPixelSize"), 18);
            overlay.insert(QStringLiteral("margin"), 6);
            overlays.push_back(overlay);
        }
        controls.insert(QStringLiteral("overlays"), overlays);

        {
            SstvStudioController controller(settingsPath);
            controller.setModeId(QStringLiteral("robot-c12"));
            QSignalSpy changed(&controller,
                               &SstvStudioController::templatesChanged);
            QVERIFY(controller.saveTemplate(QStringLiteral("Field preset"),
                                             controls));
            QCOMPARE(changed.count(), 1);
            QCOMPARE(controller.templates().size(), 1);
            const QVariantMap definition = controller.templateDefinition(
                QStringLiteral("Field preset"));
            QCOMPARE(definition.value(QStringLiteral("modeId")).toString(),
                     QStringLiteral("robot-c12"));
            QCOMPARE(definition.value(QStringLiteral("controls")).toMap()
                         .value(QStringLiteral("exposure")).toDouble(),
                     0.75);

            QVariantMap rejected = controls;
            rejected.insert(QStringLiteral("sourcePath"),
                            QStringLiteral("/private/operator/secret.png"));
            QVERIFY(!controller.saveTemplate(QStringLiteral("Leaky"),
                                              rejected));
            QCOMPARE(controller.templates().size(), 1);
        }
        {
            SstvStudioController reloaded(settingsPath);
            QCOMPARE(reloaded.templates().size(), 1);
            QVERIFY(!reloaded.templateDefinition(
                QStringLiteral("Field preset")).isEmpty());
            QVERIFY(reloaded.deleteTemplate(QStringLiteral("Field preset")));
            QVERIFY(reloaded.templates().isEmpty());
        }
        QVERIFY(!readFile(settingsPath).contains(
            QByteArrayLiteral("/private/operator/secret.png")));

        const QString boundedPath = temporary.filePath(
            QStringLiteral("bounded.ini"));
        SstvStudioController bounded(boundedPath);
        for (qsizetype index = 0;
             index < SstvStudioController::MaximumTemplates; ++index) {
            QVERIFY(bounded.saveTemplate(
                QStringLiteral("Template %1").arg(index), {}));
        }
        QVERIFY(!bounded.saveTemplate(QStringLiteral("One too many"), {}));
        QCOMPARE(bounded.templates().size(),
                 SstvStudioController::MaximumTemplates);

        const QString legacyPath = temporary.filePath(
            QStringLiteral("legacy.ini"));
        {
            QSettings legacy(legacyPath, QSettings::IniFormat);
            legacy.beginGroup(QStringLiteral("SSTV/StudioTemplates"));
            QVariantMap oldTemplates;
            oldTemplates.insert(QStringLiteral("Legacy"), controls);
            legacy.setValue(QStringLiteral("templates"), oldTemplates);
            legacy.endGroup();
            legacy.sync();
        }
        SstvStudioController migrated(legacyPath);
        QCOMPARE(migrated.templates().size(), 1);
        QSettings migratedSettings(legacyPath, QSettings::IniFormat);
        migratedSettings.beginGroup(QStringLiteral("SSTV/StudioTemplates"));
        QCOMPARE(migratedSettings.value(QStringLiteral("schemaVersion")).toInt(),
                 1);
        QVERIFY(!migratedSettings.value(QStringLiteral("catalog"))
                     .toByteArray().isEmpty());
        QVERIFY(!migratedSettings.contains(QStringLiteral("templates")));
        migratedSettings.endGroup();
    }

    void nativeEncoderDecoderLoopbackIsAsynchronousAndCancellable()
    {
        SstvDiagnosticLogBuffer::instance().clear();
        const QStringList modes {
            QStringLiteral("robot-c12"), QStringLiteral("avt-24")};
        for (const QString& mode : modes) {
            SstvStudioController controller;
            controller.setModeId(mode);
            if (mode == modes.front()) {
                QVERIFY(!controller.startLoopback());
            }
            QVERIFY(controller.generateCalibrationPattern());
            QVERIFY(controller.prepareImage());
            QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5'000);
            QVERIFY2(controller.preparedReady(), qPrintable(controller.error()));

            QSignalSpy loopbackSpy(&controller,
                                   &SstvStudioController::loopbackChanged);
            QVERIFY(controller.startLoopback());
            QVERIFY(controller.loopbackBusy());
            QTRY_VERIFY_WITH_TIMEOUT(!controller.loopbackBusy(), 30'000);
            QVERIFY2(controller.loopbackReady(),
                     qPrintable(controller.loopbackError()));
            QVERIFY(controller.loopbackError().isEmpty());
            QCOMPARE(controller.loopbackState(), QStringLiteral("Complete"));
            QCOMPARE(controller.loopbackProgress(), 1.0);
            const auto image = controller.loopbackSnapshot();
            QVERIFY(image);
            QCOMPARE(image->size(), controller.outputSize());
            const QVariantMap metrics = controller.loopbackMetrics();
            QCOMPARE(metrics.value(QStringLiteral("mode")).toString(), mode);
            QCOMPARE(metrics.value(QStringLiteral("coverage")).toDouble(), 1.0);
            QCOMPARE(metrics.value(QStringLiteral("clippedSamples")).toULongLong(),
                     qulonglong {0U});
            QVERIFY(metrics.value(QStringLiteral("encodedSamples"))
                        .toULongLong() > 0U);
            QVERIFY(loopbackSpy.count() >= 2);
        }

        SstvStudioController cancelled;
        cancelled.setModeId(QStringLiteral("ml-320"));
        QVERIFY(cancelled.generateCalibrationPattern());
        QVERIFY(cancelled.prepareImage());
        QTRY_VERIFY_WITH_TIMEOUT(!cancelled.busy(), 5'000);
        QVERIFY(cancelled.startLoopback());
        cancelled.cancelLoopback();
        QTRY_VERIFY_WITH_TIMEOUT(!cancelled.loopbackBusy(), 10'000);
        QVERIFY(!cancelled.loopbackReady());
        QCOMPARE(cancelled.loopbackState(), QStringLiteral("Cancelled"));

        const QVariantList events
            = SstvDiagnosticLogBuffer::instance().snapshot();
        QSet<QString> names;
        int loopbackEvents = 0;
        for (const QVariant& value : events) {
            const QVariantMap event = value.toMap();
            const QString name = event.value(QStringLiteral("event")).toString();
            if (!name.startsWith(QStringLiteral("studio.loopback-"))) {
                continue;
            }
            ++loopbackEvents;
            names.insert(name);
            QCOMPARE(event.value(QStringLiteral("category")).toString(),
                     QStringLiteral("sstv.tx"));
            const QVariantMap fields
                = event.value(QStringLiteral("fields")).toMap();
            QCOMPARE(fields.value(QStringLiteral("component")).toString(),
                     QStringLiteral("studio-loopback"));
            QVERIFY(!fields.contains(QStringLiteral("path")));
        }
        QCOMPARE(loopbackEvents, 8);
        QVERIFY(names.contains(QStringLiteral("studio.loopback-rejected")));
        QVERIFY(names.contains(QStringLiteral("studio.loopback-started")));
        QVERIFY(names.contains(QStringLiteral("studio.loopback-completed")));
        QVERIFY(names.contains(
            QStringLiteral("studio.loopback-cancel-requested")));
        QVERIFY(names.contains(QStringLiteral("studio.loopback-cancelled")));
    }
};

QTEST_MAIN(TestSstvStudioController)
#include "test_sstv_studio_controller.moc"
