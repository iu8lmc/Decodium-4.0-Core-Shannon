// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QCoreApplication>
#include <QImage>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

namespace decodium::sstv {

enum class SstvImageResizeMode
{
    FitLetterbox,
    FillCrop,
    Stretch,
};

enum class SstvImageRotation
{
    None,
    Clockwise90,
    Clockwise180,
    Clockwise270,
};

enum class SstvMonochromeDither
{
    None,
    FloydSteinberg,
};

enum class SstvOverlayKind
{
    Callsign,
    Locator,
    UtcDateTime,
    Frequency,
    Mode,
    CustomText,
    SignalReport,
    Watermark,
};

enum class SstvOverlayAnchor
{
    TopLeft,
    TopCentre,
    TopRight,
    CentreLeft,
    Centre,
    CentreRight,
    BottomLeft,
    BottomCentre,
    BottomRight,
};

struct SstvImageAdjustments final
{
    // Exposure is measured in stops.  Brightness is an additive normalised
    // offset.  Contrast, gamma, saturation and white-balance factors use 1.0
    // as their neutral value.
    double exposureStops {0.0};
    double brightness {0.0};
    double contrast {1.0};
    double gamma {1.0};
    double saturation {1.0};
    double whiteBalanceRed {1.0};
    double whiteBalanceGreen {1.0};
    double whiteBalanceBlue {1.0};
    double sharpness {0.0};
    bool grayscale {false};
    SstvMonochromeDither dither {SstvMonochromeDither::None};
};

struct SstvTextOverlay final
{
    SstvOverlayKind kind {SstvOverlayKind::CustomText};
    QString text;
    SstvOverlayAnchor anchor {SstvOverlayAnchor::BottomLeft};
    QString fontFamily;
    int fontPixelSize {18};
    bool bold {true};
    int marginPixels {8};
    int paddingPixels {3};
    QColor foreground {Qt::white};
    QColor background {0, 0, 0, 144};
    double opacity {1.0};
};

struct SstvImagePreparation final
{
    QSize outputSize;
    // Normalised source coordinates.  The full source image is (0,0,1,1).
    QRectF crop {0.0, 0.0, 1.0, 1.0};
    SstvImageResizeMode resizeMode {SstvImageResizeMode::FitLetterbox};
    SstvImageRotation rotation {SstvImageRotation::None};
    bool flipHorizontal {false};
    bool flipVertical {false};
    bool lockCropToOutputAspect {false};
    bool smoothScaling {true};
    bool convertToSrgb {true};
    QColor background {Qt::black};
    SstvImageAdjustments adjustments;
    QVector<SstvTextOverlay> overlays;
    int borderWidthPixels {0};
    QColor borderColor {Qt::white};
};

struct SstvPreparedImage final
{
    QImage image;
    QRect sourceCropPixels;
    QStringList warnings;
    QString error;

    bool isValid() const noexcept { return error.isEmpty() && !image.isNull(); }
};

// Stateless, deterministic image preparation used by Decodium's native SSTV
// transmit studio.  prepare() never mutates the source image and always emits
// the exact requested mode resolution on success.
class SstvImagePreprocessor final
{
    Q_DECLARE_TR_FUNCTIONS(SstvImagePreprocessor)

public:
    static constexpr int kMaximumDimension = 8'192;
    static constexpr qint64 kMaximumPixels = 8'388'608;
    static constexpr int kMaximumOverlays = 32;
    static constexpr int kMaximumOverlayTextLength = 512;
    static constexpr qint64 kMaximumCompressedBytes = 64LL * 1'024LL * 1'024LL;
    static constexpr qint64 kMaximumDecodedBytes = kMaximumPixels * 8LL;
    static constexpr int kMaximumMetadataKeys = 64;
    static constexpr qsizetype kMaximumMetadataCharacters = 16 * 1'024;
    static constexpr qsizetype kMaximumMetadataValueCharacters = 4 * 1'024;
    static constexpr int kReaderAllocationLimitMiB = 64;

    static SstvPreparedImage prepare(const QImage& source,
                                     const SstvImagePreparation& preparation);

    // Reads by decoded content through QImageReader.  The suffix is not used
    // as proof of format, and decoded dimensions are bounded before reading.
    static QImage readValidated(const QString& path, QString* error = nullptr);

    // A built-in deterministic source for TX/RX calibration and loopback.
    static QImage calibrationPattern(const QSize& size,
                                     QString* error = nullptr);

private:
    SstvImagePreprocessor() = delete;
};

} // namespace decodium::sstv
