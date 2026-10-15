// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/tx/SstvImagePreprocessor.h"
#include "../../src/sstv/diagnostics/SstvDiagnosticLogging.h"

#include <QColorSpace>
#include <QFile>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>

#include <cmath>

using namespace decodium::sstv;

namespace {

SstvImagePreparation preparation(QSize size)
{
    SstvImagePreparation value;
    value.outputSize = size;
    value.smoothScaling = false;
    return value;
}

QColor pixel(const QImage& image, int x, int y)
{
    return QColor::fromRgb(image.pixel(x, y));
}

void compareRgb(const QColor& actual, int red, int green, int blue)
{
    QCOMPARE(actual.red(), red);
    QCOMPARE(actual.green(), green);
    QCOMPARE(actual.blue(), blue);
}

} // namespace

class TestSstvImagePreprocessor final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsHostileConfigurationWithoutMutatingSource()
    {
        QImage source(3, 2, QImage::Format_ARGB32);
        source.fill(QColor(10, 20, 30, 40));
        const QImage original = source.copy();

        auto config = preparation({0, 2});
        QVERIFY(!SstvImagePreprocessor::prepare(source, config).isValid());

        config = preparation({2, 2});
        config.crop = {-0.1, 0.0, 1.0, 1.0};
        QVERIFY(!SstvImagePreprocessor::prepare(source, config).isValid());

        config = preparation({2, 2});
        config.adjustments.gamma = 0.0;
        QVERIFY(!SstvImagePreprocessor::prepare(source, config).isValid());

        config = preparation({2, 2});
        config.overlays.resize(SstvImagePreprocessor::kMaximumOverlays + 1);
        QVERIFY(!SstvImagePreprocessor::prepare(source, config).isValid());

        QVERIFY(source == original);
        QVERIFY(source.colorSpace() == original.colorSpace());
        QVERIFY(!SstvImagePreprocessor::prepare(QImage {}, preparation({2, 2}))
                     .isValid());
    }

    void fitLetterboxesAndFillCropsToExactResolution()
    {
        QImage wide(4, 2, QImage::Format_RGB32);
        wide.fill(Qt::red);

        auto fit = preparation({4, 4});
        fit.resizeMode = SstvImageResizeMode::FitLetterbox;
        fit.background = Qt::blue;
        const auto fitted = SstvImagePreprocessor::prepare(wide, fit);
        QVERIFY2(fitted.isValid(), qPrintable(fitted.error));
        QCOMPARE(fitted.image.size(), QSize(4, 4));
        compareRgb(pixel(fitted.image, 0, 0), 0, 0, 255);
        compareRgb(pixel(fitted.image, 0, 1), 255, 0, 0);
        compareRgb(pixel(fitted.image, 3, 2), 255, 0, 0);
        compareRgb(pixel(fitted.image, 3, 3), 0, 0, 255);

        for (int x = 0; x < wide.width(); ++x) {
            const QColor colour = x == 0 ? Qt::red
                : x == 1            ? Qt::green
                : x == 2            ? Qt::blue
                                    : Qt::white;
            for (int y = 0; y < wide.height(); ++y) {
                wide.setPixelColor(x, y, colour);
            }
        }
        auto fill = preparation({2, 2});
        fill.resizeMode = SstvImageResizeMode::FillCrop;
        const auto filled = SstvImagePreprocessor::prepare(wide, fill);
        QVERIFY2(filled.isValid(), qPrintable(filled.error));
        QCOMPARE(filled.image.size(), QSize(2, 2));
        compareRgb(pixel(filled.image, 0, 0), 0, 255, 0);
        compareRgb(pixel(filled.image, 1, 0), 0, 0, 255);
    }

    void stretchIsExplicitAndReported()
    {
        QImage source(4, 2, QImage::Format_RGB32);
        source.fill(Qt::red);
        auto config = preparation({3, 3});
        config.resizeMode = SstvImageResizeMode::Stretch;
        const auto result = SstvImagePreprocessor::prepare(source, config);
        QVERIFY(result.isValid());
        QCOMPARE(result.image.size(), QSize(3, 3));
        QCOMPARE(result.warnings.size(), 1);

        config.outputSize = {8, 4};
        const auto sameAspect = SstvImagePreprocessor::prepare(source, config);
        QVERIFY(sameAspect.isValid());
        QVERIFY(sameAspect.warnings.isEmpty());
    }

    void cropRotateAndFlipHaveDeterministicGeometry()
    {
        QImage source(3, 2, QImage::Format_RGB32);
        const QColor colours[6] {Qt::red,
                                 Qt::green,
                                 Qt::blue,
                                 Qt::cyan,
                                 Qt::magenta,
                                 Qt::yellow};
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 3; ++x) {
                source.setPixelColor(x, y, colours[y * 3 + x]);
            }
        }

        auto config = preparation({2, 3});
        config.resizeMode = SstvImageResizeMode::Stretch;
        config.rotation = SstvImageRotation::Clockwise90;
        const auto rotated = SstvImagePreprocessor::prepare(source, config);
        QVERIFY(rotated.isValid());
        compareRgb(pixel(rotated.image, 0, 0), 0, 255, 255);
        compareRgb(pixel(rotated.image, 1, 0), 255, 0, 0);
        compareRgb(pixel(rotated.image, 0, 2), 255, 255, 0);
        compareRgb(pixel(rotated.image, 1, 2), 0, 0, 255);

        config.flipHorizontal = true;
        const auto flipped = SstvImagePreprocessor::prepare(source, config);
        QVERIFY(flipped.isValid());
        compareRgb(pixel(flipped.image, 0, 0), 255, 0, 0);
        compareRgb(pixel(flipped.image, 1, 0), 0, 255, 255);

        config = preparation({1, 2});
        config.crop = {1.0 / 3.0, 0.0, 1.0 / 3.0, 1.0};
        config.resizeMode = SstvImageResizeMode::Stretch;
        const auto cropped = SstvImagePreprocessor::prepare(source, config);
        QVERIFY(cropped.isValid());
        QCOMPARE(cropped.sourceCropPixels, QRect(1, 0, 1, 2));
        compareRgb(pixel(cropped.image, 0, 0), 0, 255, 0);
        compareRgb(pixel(cropped.image, 0, 1), 255, 0, 255);
    }

    void aspectLockProducesCentredModeShapedCrop()
    {
        QImage source(6, 4, QImage::Format_RGB32);
        source.fill(Qt::black);
        auto config = preparation({4, 4});
        config.lockCropToOutputAspect = true;
        const auto result = SstvImagePreprocessor::prepare(source, config);
        QVERIFY(result.isValid());
        QCOMPARE(result.sourceCropPixels, QRect(1, 0, 4, 4));

        config.rotation = SstvImageRotation::Clockwise90;
        config.outputSize = {8, 4};
        const auto quarterTurn = SstvImagePreprocessor::prepare(source, config);
        QVERIFY(quarterTurn.isValid());
        QCOMPARE(quarterTurn.sourceCropPixels, QRect(2, 0, 2, 4));
    }

    void identityAndColourControlsAreBounded()
    {
        QImage source(1, 1, QImage::Format_RGB32);
        source.setPixelColor(0, 0, QColor(64, 128, 192));
        auto config = preparation({1, 1});
        config.resizeMode = SstvImageResizeMode::Stretch;
        const auto identity = SstvImagePreprocessor::prepare(source, config);
        QVERIFY(identity.isValid());
        compareRgb(pixel(identity.image, 0, 0), 64, 128, 192);

        config.adjustments.exposureStops = 1.0;
        const auto exposed = SstvImagePreprocessor::prepare(source, config);
        QVERIFY(exposed.isValid());
        compareRgb(pixel(exposed.image, 0, 0), 128, 255, 255);

        config.adjustments = {};
        config.adjustments.saturation = 0.0;
        const auto desaturated = SstvImagePreprocessor::prepare(source, config);
        QVERIFY(desaturated.isValid());
        const QColor gray = pixel(desaturated.image, 0, 0);
        QCOMPARE(gray.red(), gray.green());
        QCOMPARE(gray.green(), gray.blue());

        config.adjustments = {};
        config.adjustments.whiteBalanceRed = 2.0;
        const auto balanced = SstvImagePreprocessor::prepare(source, config);
        QVERIFY(balanced.isValid());
        QCOMPARE(pixel(balanced.image, 0, 0).red(), 128);
        QCOMPARE(pixel(balanced.image, 0, 0).green(), 128);
    }

    void transparencyUsesConfiguredBackground()
    {
        QImage source(1, 1, QImage::Format_ARGB32);
        source.setPixelColor(0, 0, QColor(255, 0, 0, 128));
        auto config = preparation({1, 1});
        config.background = Qt::blue;
        const auto result = SstvImagePreprocessor::prepare(source, config);
        QVERIFY(result.isValid());
        const QColor blended = pixel(result.image, 0, 0);
        QVERIFY(std::abs(blended.red() - 128) <= 1);
        QCOMPARE(blended.green(), 0);
        QVERIFY(std::abs(blended.blue() - 127) <= 1);
        QCOMPARE(blended.alpha(), 255);
    }

    void grayscaleAndDitherAreDeterministic()
    {
        QImage source(32, 8, QImage::Format_RGB32);
        for (int y = 0; y < source.height(); ++y) {
            for (int x = 0; x < source.width(); ++x) {
                const int value = x * 255 / (source.width() - 1);
                source.setPixelColor(x, y, QColor(value, value, value));
            }
        }
        auto config = preparation(source.size());
        config.adjustments.grayscale = true;
        config.adjustments.dither = SstvMonochromeDither::FloydSteinberg;
        const auto first = SstvImagePreprocessor::prepare(source, config);
        const auto second = SstvImagePreprocessor::prepare(source, config);
        QVERIFY(first.isValid());
        QVERIFY(first.image == second.image);
        bool sawBlack = false;
        bool sawWhite = false;
        for (int y = 0; y < first.image.height(); ++y) {
            for (int x = 0; x < first.image.width(); ++x) {
                const QColor colour = pixel(first.image, x, y);
                QVERIFY(colour.red() == 0 || colour.red() == 255);
                QCOMPARE(colour.red(), colour.green());
                QCOMPARE(colour.green(), colour.blue());
                sawBlack = sawBlack || colour.red() == 0;
                sawWhite = sawWhite || colour.red() == 255;
            }
        }
        QVERIFY(sawBlack);
        QVERIFY(sawWhite);
    }

    void sharpnessOverlayAndBorderRemainInsideFrame()
    {
        QImage source(64, 32, QImage::Format_RGB32);
        source.fill(QColor(50, 60, 70));
        source.setPixelColor(32, 16, Qt::white);
        auto config = preparation(source.size());
        config.adjustments.sharpness = 1.5;
        config.borderWidthPixels = 2;
        config.borderColor = Qt::yellow;
        SstvTextOverlay overlay;
        overlay.kind = SstvOverlayKind::Callsign;
        overlay.text = QString(300, QLatin1Char('W'));
        overlay.anchor = SstvOverlayAnchor::BottomRight;
        overlay.fontPixelSize = 200;
        overlay.marginPixels = 2;
        config.overlays.push_back(overlay);

        const auto result = SstvImagePreprocessor::prepare(source, config);
        QVERIFY2(result.isValid(), qPrintable(result.error));
        QCOMPARE(result.image.size(), source.size());
        QVERIFY(!result.warnings.isEmpty());
        compareRgb(pixel(result.image, 0, 0), 255, 255, 0);
        compareRgb(pixel(result.image, 63, 31), 255, 255, 0);
        QCOMPARE(pixel(result.image, 32, 16).alpha(), 255);
    }

    void validatedReaderUsesContentNotSuffix()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString disguised = temporary.filePath(QStringLiteral("payload.bin"));
        QImage expected(5, 3, QImage::Format_RGB32);
        expected.fill(QColor(12, 34, 56));
        QVERIFY(expected.save(disguised, "PNG"));

        QString error;
        const QImage decoded
            = SstvImagePreprocessor::readValidated(disguised, &error);
        QVERIFY2(!decoded.isNull(), qPrintable(error));
        QCOMPARE(decoded.size(), expected.size());
        compareRgb(decoded.pixelColor(2, 1), 12, 34, 56);
        QVERIFY(error.isEmpty());

        const QString fake = temporary.filePath(QStringLiteral("not-an-image.png"));
        QFile file(fake);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("not image content"), qint64(17));
        file.close();
        QVERIFY(SstvImagePreprocessor::readValidated(fake, &error).isNull());
        QVERIFY(!error.isEmpty());
    }

    void validatedReaderRejectsBoundedResourceAbuse()
    {
        SstvDiagnosticLogBuffer::instance().clear();
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QString error;

        const QString jpegPath = temporary.filePath(
            QStringLiteral("valid-jpeg.payload"));
        QImage jpeg(12, 8, QImage::Format_RGB32);
        jpeg.fill(QColor(20, 40, 60));
        QVERIFY(jpeg.save(jpegPath, "JPEG"));
        const QImage validJpeg = SstvImagePreprocessor::readValidated(
            jpegPath, &error);
        QVERIFY2(!validJpeg.isNull(), qPrintable(error));
        QCOMPARE(validJpeg.size(), jpeg.size());

        const QString oversizedPath = temporary.filePath(
            QStringLiteral("oversized.bin"));
        QFile oversized(oversizedPath);
        QVERIFY(oversized.open(QIODevice::WriteOnly));
        QVERIFY(oversized.resize(
            SstvImagePreprocessor::kMaximumCompressedBytes + 1));
        oversized.close();
        QVERIFY(SstvImagePreprocessor::readValidated(
                    oversizedPath, &error).isNull());
        QVERIFY(error.contains(QStringLiteral("file bound"),
                               Qt::CaseInsensitive));

        const QString dimensionPath = temporary.filePath(
            QStringLiteral("dimension-bomb.png"));
        QImage dimensionBomb(
            SstvImagePreprocessor::kMaximumDimension + 1,
            1,
            QImage::Format_RGB32);
        dimensionBomb.fill(Qt::black);
        QVERIFY(dimensionBomb.save(dimensionPath, "PNG"));
        QVERIFY(SstvImagePreprocessor::readValidated(
                    dimensionPath, &error).isNull());
        QVERIFY(error.contains(QStringLiteral("dimensions"),
                               Qt::CaseInsensitive));

        const QString animatedPath = temporary.filePath(
            QStringLiteral("two-frame.gif"));
        QFile animated(animatedPath);
        QVERIFY(animated.open(QIODevice::WriteOnly));
        const QByteArray twoFrameGif = QByteArray::fromHex(
            "47494638396101000100800000000000ffffff"
            "21f90401000000002c0000000001000100000202440100"
            "21f90401000000002c0000000001000100000202440100"
            "3b");
        QCOMPARE(animated.write(twoFrameGif), twoFrameGif.size());
        animated.close();
        QImageReader animatedProbe(animatedPath);
        QVERIFY(animatedProbe.canRead());
        QVERIFY(animatedProbe.imageCount() > 1
                || animatedProbe.supportsAnimation());
        QVERIFY(SstvImagePreprocessor::readValidated(
                    animatedPath, &error).isNull());
        QVERIFY(error.contains(QStringLiteral("frame"),
                               Qt::CaseInsensitive));

        const QString metadataPath = temporary.filePath(
            QStringLiteral("metadata.png"));
        QImageWriter metadataWriter(metadataPath, "PNG");
        metadataWriter.setText(QStringLiteral("Comment"),
                               QString(
                                   SstvImagePreprocessor::kMaximumMetadataValueCharacters
                                       + 1,
                                   QLatin1Char('M')));
        QVERIFY2(metadataWriter.write(jpeg),
                 qPrintable(metadataWriter.errorString()));
        QVERIFY(SstvImagePreprocessor::readValidated(
                    metadataPath, &error).isNull());
        QVERIFY(error.contains(QStringLiteral("metadata"),
                               Qt::CaseInsensitive));

        const QString symlinkPath = temporary.filePath(
            QStringLiteral("linked.png"));
        QVERIFY(QFile::link(metadataPath, symlinkPath));
        QVERIFY(SstvImagePreprocessor::readValidated(
                    symlinkPath, &error).isNull());
        QVERIFY(error.contains(QStringLiteral("symbolic"),
                               Qt::CaseInsensitive));

        const QVariantList events
            = SstvDiagnosticLogBuffer::instance().snapshot();
        QSet<QString> reasons;
        int validationRejections = 0;
        for (const QVariant& value : events) {
            const QVariantMap event = value.toMap();
            if (event.value(QStringLiteral("event")).toString()
                != QStringLiteral("studio.image-validation-rejected")) {
                continue;
            }
            ++validationRejections;
            QCOMPARE(event.value(QStringLiteral("category")).toString(),
                     QStringLiteral("sstv.security"));
            const QVariantMap fields
                = event.value(QStringLiteral("fields")).toMap();
            reasons.insert(
                fields.value(QStringLiteral("reasonCode")).toString());
            QCOMPARE(fields.value(QStringLiteral("success")).toBool(), false);
            QVERIFY(!fields.contains(QStringLiteral("path")));
        }
        QCOMPARE(validationRejections, 5);
        QVERIFY(reasons.contains(QStringLiteral("compressed-size")));
        QVERIFY(reasons.contains(QStringLiteral("decoded-dimensions")));
        QVERIFY(reasons.contains(QStringLiteral("multi-frame")));
        QVERIFY(reasons.contains(QStringLiteral("metadata-bound")));
        QVERIFY(reasons.contains(QStringLiteral("symbolic-link")));
        const QByteArray serialized = QJsonDocument::fromVariant(events).toJson(
            QJsonDocument::Compact);
        QVERIFY(!serialized.contains(temporary.path().toUtf8()));
        QVERIFY(!serialized.contains(metadataPath.toUtf8()));
    }

    void calibrationPatternIsBoundedAndRepeatable()
    {
        QString error;
        const QImage first
            = SstvImagePreprocessor::calibrationPattern({320, 256}, &error);
        const QImage second
            = SstvImagePreprocessor::calibrationPattern({320, 256});
        QVERIFY2(!first.isNull(), qPrintable(error));
        QCOMPARE(first.size(), QSize(320, 256));
        QVERIFY(first == second);
        QVERIFY(first.colorSpace() == QColorSpace::SRgb);
        compareRgb(first.pixelColor(1, 1), 255, 255, 255);
        compareRgb(first.pixelColor(318, 1), 0, 0, 0);

        QVERIFY(SstvImagePreprocessor::calibrationPattern({0, 10}, &error)
                    .isNull());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_MAIN(TestSstvImagePreprocessor)
#include "test_sstv_image_preprocessor.moc"
