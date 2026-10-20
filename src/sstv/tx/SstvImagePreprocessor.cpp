// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvImagePreprocessor.h"

#include "../diagnostics/SstvDiagnosticLogging.h"

#include <QColorSpace>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QImageReader>
#include <QPainter>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

namespace decodium::sstv {
namespace {

constexpr double kAspectTolerance = 1.0e-6;

bool boundedSize(const QSize& size) noexcept
{
    if (size.width() <= 0 || size.height() <= 0
        || size.width() > SstvImagePreprocessor::kMaximumDimension
        || size.height() > SstvImagePreprocessor::kMaximumDimension) {
        return false;
    }
    return static_cast<qint64>(size.width())
            <= SstvImagePreprocessor::kMaximumPixels / size.height();
}

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

double clampUnit(double value) noexcept
{
    return std::clamp(value, 0.0, 1.0);
}

int toByte(double value) noexcept
{
    return std::clamp(static_cast<int>(std::lround(clampUnit(value) * 255.0)),
                      0,
                      255);
}

SstvPreparedImage failure(QString message)
{
    SstvPreparedImage result;
    result.error = std::move(message);
    return result;
}

bool validAdjustments(const SstvImageAdjustments& value) noexcept
{
    return finite(value.exposureStops) && value.exposureStops >= -8.0
        && value.exposureStops <= 8.0 && finite(value.brightness)
        && value.brightness >= -1.0 && value.brightness <= 1.0
        && finite(value.contrast) && value.contrast >= 0.0
        && value.contrast <= 4.0 && finite(value.gamma)
        && value.gamma >= 0.1 && value.gamma <= 10.0
        && finite(value.saturation) && value.saturation >= 0.0
        && value.saturation <= 4.0 && finite(value.whiteBalanceRed)
        && value.whiteBalanceRed >= 0.0 && value.whiteBalanceRed <= 4.0
        && finite(value.whiteBalanceGreen) && value.whiteBalanceGreen >= 0.0
        && value.whiteBalanceGreen <= 4.0 && finite(value.whiteBalanceBlue)
        && value.whiteBalanceBlue >= 0.0 && value.whiteBalanceBlue <= 4.0
        && finite(value.sharpness) && value.sharpness >= 0.0
        && value.sharpness <= 2.0;
}

bool validCrop(const QRectF& crop) noexcept
{
    if (!finite(crop.x()) || !finite(crop.y()) || !finite(crop.width())
        || !finite(crop.height()) || crop.x() < 0.0 || crop.y() < 0.0
        || crop.width() <= 0.0 || crop.height() <= 0.0) {
        return false;
    }
    constexpr double epsilon = 1.0e-9;
    return crop.right() <= 1.0 + epsilon && crop.bottom() <= 1.0 + epsilon;
}

bool validOverlay(const SstvTextOverlay& overlay) noexcept
{
    return overlay.text.size() <= SstvImagePreprocessor::kMaximumOverlayTextLength
        && overlay.fontFamily.size()
            <= SstvImagePreprocessor::kMaximumOverlayTextLength
        && overlay.fontPixelSize >= 1
        && overlay.fontPixelSize <= SstvImagePreprocessor::kMaximumDimension
        && overlay.marginPixels >= 0
        && overlay.marginPixels <= SstvImagePreprocessor::kMaximumDimension
        && overlay.paddingPixels >= 0
        && overlay.paddingPixels <= SstvImagePreprocessor::kMaximumDimension
        && overlay.foreground.isValid() && overlay.background.isValid()
        && finite(overlay.opacity) && overlay.opacity >= 0.0
        && overlay.opacity <= 1.0;
}

QRect pixelCrop(const QSize& sourceSize, const QRectF& normalised)
{
    const double width = static_cast<double>(sourceSize.width());
    const double height = static_cast<double>(sourceSize.height());
    const int left = std::clamp(static_cast<int>(std::floor(normalised.x() * width)),
                                0,
                                sourceSize.width() - 1);
    const int top = std::clamp(static_cast<int>(std::floor(normalised.y() * height)),
                               0,
                               sourceSize.height() - 1);
    const int rightExclusive = std::clamp(
        static_cast<int>(std::ceil((normalised.x() + normalised.width()) * width)),
        left + 1,
        sourceSize.width());
    const int bottomExclusive = std::clamp(
        static_cast<int>(std::ceil((normalised.y() + normalised.height()) * height)),
        top + 1,
        sourceSize.height());
    return {left, top, rightExclusive - left, bottomExclusive - top};
}

bool quarterTurn(SstvImageRotation rotation) noexcept
{
    return rotation == SstvImageRotation::Clockwise90
        || rotation == SstvImageRotation::Clockwise270;
}

QRect aspectLockedCrop(QRect crop,
                       const QSize& outputSize,
                       SstvImageRotation rotation)
{
    double wanted = static_cast<double>(outputSize.width()) / outputSize.height();
    if (quarterTurn(rotation)) {
        wanted = 1.0 / wanted;
    }
    const double current = static_cast<double>(crop.width()) / crop.height();
    if (std::abs(current - wanted) <= kAspectTolerance) {
        return crop;
    }
    if (current > wanted) {
        const int newWidth = std::max(
            1, static_cast<int>(std::floor(crop.height() * wanted)));
        crop.setX(crop.x() + (crop.width() - newWidth) / 2);
        crop.setWidth(newWidth);
    } else {
        const int newHeight = std::max(
            1, static_cast<int>(std::floor(crop.width() / wanted)));
        crop.setY(crop.y() + (crop.height() - newHeight) / 2);
        crop.setHeight(newHeight);
    }
    return crop;
}

QImage rotateAndFlip(QImage image,
                     SstvImageRotation rotation,
                     bool horizontal,
                     bool vertical)
{
    int degrees = 0;
    switch (rotation) {
    case SstvImageRotation::None:
        break;
    case SstvImageRotation::Clockwise90:
        degrees = 90;
        break;
    case SstvImageRotation::Clockwise180:
        degrees = 180;
        break;
    case SstvImageRotation::Clockwise270:
        degrees = 270;
        break;
    }
    if (degrees != 0) {
        QTransform transform;
        transform.rotate(degrees);
        image = image.transformed(transform, Qt::FastTransformation);
    }
    if (horizontal || vertical) {
        QTransform mirror;
        mirror.scale(horizontal ? -1.0 : 1.0, vertical ? -1.0 : 1.0);
        image = image.transformed(mirror, Qt::FastTransformation);
    }
    return image;
}

QImage resizeImage(const QImage& source,
                   const SstvImagePreparation& preparation,
                   QStringList& warnings)
{
    QImage output(preparation.outputSize, QImage::Format_RGB32);
    output.fill(preparation.background.rgb());

    const Qt::TransformationMode transformMode = preparation.smoothScaling
        ? Qt::SmoothTransformation
        : Qt::FastTransformation;
    Qt::AspectRatioMode aspectMode = Qt::KeepAspectRatio;
    if (preparation.resizeMode == SstvImageResizeMode::FillCrop) {
        aspectMode = Qt::KeepAspectRatioByExpanding;
    } else if (preparation.resizeMode == SstvImageResizeMode::Stretch) {
        aspectMode = Qt::IgnoreAspectRatio;
        const double sourceAspect
            = static_cast<double>(source.width()) / source.height();
        const double outputAspect = static_cast<double>(output.width())
            / output.height();
        if (std::abs(sourceAspect - outputAspect) > kAspectTolerance) {
            warnings.push_back(
                SstvImagePreprocessor::tr(
                    "Source aspect ratio was stretched explicitly"));
        }
    }

    const QImage scaled = source.scaled(preparation.outputSize,
                                        aspectMode,
                                        transformMode);
    const QPoint topLeft {(output.width() - scaled.width()) / 2,
                          (output.height() - scaled.height()) / 2};
    QPainter painter(&output);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(topLeft, scaled);
    painter.end();
    return output;
}

void applyColourAdjustments(QImage& image,
                            const SstvImageAdjustments& adjustments)
{
    const bool neutral = adjustments.exposureStops == 0.0
        && adjustments.brightness == 0.0 && adjustments.contrast == 1.0
        && adjustments.gamma == 1.0 && adjustments.saturation == 1.0
        && adjustments.whiteBalanceRed == 1.0
        && adjustments.whiteBalanceGreen == 1.0
        && adjustments.whiteBalanceBlue == 1.0;
    if (neutral) {
        return;
    }

    const double exposure = std::exp2(adjustments.exposureStops);
    const double inverseGamma = 1.0 / adjustments.gamma;
    for (int y = 0; y < image.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = line[x];
            double red = (qRed(pixel) / 255.0) * exposure
                * adjustments.whiteBalanceRed;
            double green = (qGreen(pixel) / 255.0) * exposure
                * adjustments.whiteBalanceGreen;
            double blue = (qBlue(pixel) / 255.0) * exposure
                * adjustments.whiteBalanceBlue;

            const auto tone = [&](double value) {
                value = (value - 0.5) * adjustments.contrast + 0.5
                    + adjustments.brightness;
                return std::pow(clampUnit(value), inverseGamma);
            };
            red = tone(red);
            green = tone(green);
            blue = tone(blue);

            const double luminance
                = 0.2126 * red + 0.7152 * green + 0.0722 * blue;
            red = luminance + (red - luminance) * adjustments.saturation;
            green = luminance + (green - luminance) * adjustments.saturation;
            blue = luminance + (blue - luminance) * adjustments.saturation;
            line[x] = qRgb(toByte(red), toByte(green), toByte(blue));
        }
    }
}

void applySharpness(QImage& image, double amount)
{
    if (amount <= 0.0 || image.width() < 3 || image.height() < 3) {
        return;
    }
    const QImage source = image.copy();
    for (int y = 1; y < image.height() - 1; ++y) {
        auto* destination = reinterpret_cast<QRgb*>(image.scanLine(y));
        const auto* above
            = reinterpret_cast<const QRgb*>(source.constScanLine(y - 1));
        const auto* centre
            = reinterpret_cast<const QRgb*>(source.constScanLine(y));
        const auto* below
            = reinterpret_cast<const QRgb*>(source.constScanLine(y + 1));
        for (int x = 1; x < image.width() - 1; ++x) {
            const auto sharpen = [&](int channelCentre,
                                     int channelAbove,
                                     int channelBelow,
                                     int channelLeft,
                                     int channelRight) {
                const double value = channelCentre * (1.0 + 4.0 * amount)
                    - amount * (channelAbove + channelBelow + channelLeft
                                + channelRight);
                return std::clamp(static_cast<int>(std::lround(value)), 0, 255);
            };
            destination[x] = qRgb(
                sharpen(qRed(centre[x]),
                        qRed(above[x]),
                        qRed(below[x]),
                        qRed(centre[x - 1]),
                        qRed(centre[x + 1])),
                sharpen(qGreen(centre[x]),
                        qGreen(above[x]),
                        qGreen(below[x]),
                        qGreen(centre[x - 1]),
                        qGreen(centre[x + 1])),
                sharpen(qBlue(centre[x]),
                        qBlue(above[x]),
                        qBlue(below[x]),
                        qBlue(centre[x - 1]),
                        qBlue(centre[x + 1])));
        }
    }
}

QPoint overlayTopLeft(SstvOverlayAnchor anchor,
                      const QRect& available,
                      const QSize& box)
{
    int x = available.left();
    int y = available.top();
    switch (anchor) {
    case SstvOverlayAnchor::TopCentre:
    case SstvOverlayAnchor::Centre:
    case SstvOverlayAnchor::BottomCentre:
        x = available.left() + (available.width() - box.width()) / 2;
        break;
    case SstvOverlayAnchor::TopRight:
    case SstvOverlayAnchor::CentreRight:
    case SstvOverlayAnchor::BottomRight:
        x = available.right() - box.width() + 1;
        break;
    default:
        break;
    }
    switch (anchor) {
    case SstvOverlayAnchor::CentreLeft:
    case SstvOverlayAnchor::Centre:
    case SstvOverlayAnchor::CentreRight:
        y = available.top() + (available.height() - box.height()) / 2;
        break;
    case SstvOverlayAnchor::BottomLeft:
    case SstvOverlayAnchor::BottomCentre:
    case SstvOverlayAnchor::BottomRight:
        y = available.bottom() - box.height() + 1;
        break;
    default:
        break;
    }
    return {x, y};
}

void drawOverlays(QImage& image,
                  const QVector<SstvTextOverlay>& overlays,
                  QStringList& warnings)
{
    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    for (qsizetype index = 0; index < overlays.size(); ++index) {
        const SstvTextOverlay& overlay = overlays[index];
        QString text = overlay.text;
        text.replace(QLatin1Char('\n'), QLatin1Char(' '));
        text.replace(QLatin1Char('\r'), QLatin1Char(' '));
        text = text.simplified();
        if (text.isEmpty() || overlay.opacity <= 0.0) {
            continue;
        }

        const int maximumMargin
            = std::max(0, std::min(image.width(), image.height()) / 2 - 1);
        const int margin = std::min(overlay.marginPixels, maximumMargin);
        const QRect available = image.rect().adjusted(margin,
                                                      margin,
                                                      -margin,
                                                      -margin);
        if (available.width() <= 0 || available.height() <= 0) {
            warnings.push_back(SstvImagePreprocessor::tr(
                                   "Overlay %1 had no safe drawing area")
                                   .arg(index));
            continue;
        }
        const int padding = std::min(
            overlay.paddingPixels,
            std::max(0, std::min(available.width(), available.height()) / 4));
        const int maximumTextWidth = std::max(1, available.width() - 2 * padding);
        const int maximumTextHeight
            = std::max(1, available.height() - 2 * padding);

        QFont font(overlay.fontFamily);
        font.setBold(overlay.bold);
        int pixelSize = std::min(overlay.fontPixelSize, maximumTextHeight);
        pixelSize = std::max(1, pixelSize);
        font.setPixelSize(pixelSize);
        QFontMetrics metrics(font);
        while (pixelSize > 1
               && (metrics.horizontalAdvance(text) > maximumTextWidth
                   || metrics.height() > maximumTextHeight)) {
            --pixelSize;
            font.setPixelSize(pixelSize);
            metrics = QFontMetrics(font);
        }

        const QString rendered
            = metrics.elidedText(text, Qt::ElideRight, maximumTextWidth);
        if (rendered != text) {
            warnings.push_back(
                SstvImagePreprocessor::tr(
                    "Overlay %1 text was safely elided").arg(index));
        }
        if (rendered.isEmpty()) {
            continue;
        }
        const QSize box {
            std::min(available.width(),
                     metrics.horizontalAdvance(rendered) + 2 * padding),
            std::min(available.height(), metrics.height() + 2 * padding)};
        const QRect boxRect(overlayTopLeft(overlay.anchor, available, box), box);

        painter.save();
        painter.setOpacity(overlay.opacity);
        if (overlay.background.alpha() != 0) {
            painter.fillRect(boxRect, overlay.background);
        }
        painter.setFont(font);
        painter.setPen(overlay.foreground);
        painter.drawText(boxRect.adjusted(padding,
                                          padding,
                                          -padding,
                                          -padding),
                         Qt::AlignCenter | Qt::TextSingleLine,
                         rendered);
        painter.restore();
    }
    painter.end();
}

void drawBorder(QImage& image, int width, const QColor& colour)
{
    if (width <= 0) {
        return;
    }
    QPainter painter(&image);
    painter.fillRect(0, 0, image.width(), width, colour);
    painter.fillRect(0, image.height() - width, image.width(), width, colour);
    painter.fillRect(0, width, width, image.height() - 2 * width, colour);
    painter.fillRect(image.width() - width,
                     width,
                     width,
                     image.height() - 2 * width,
                     colour);
    painter.end();
}

void convertToMonochrome(QImage& image, SstvMonochromeDither dither)
{
    if (dither == SstvMonochromeDither::None) {
        for (int y = 0; y < image.height(); ++y) {
            auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const int gray = qGray(line[x]);
                line[x] = qRgb(gray, gray, gray);
            }
        }
        return;
    }

    const std::size_t rowSize = static_cast<std::size_t>(image.width()) + 2U;
    std::vector<double> currentError(rowSize, 0.0);
    std::vector<double> nextError(rowSize, 0.0);
    for (int y = 0; y < image.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const std::size_t position = static_cast<std::size_t>(x) + 1U;
            const double oldValue
                = std::clamp(static_cast<double>(qGray(line[x]))
                                 + currentError[position],
                             0.0,
                             255.0);
            const int newValue = oldValue < 127.5 ? 0 : 255;
            const double error = oldValue - newValue;
            line[x] = qRgb(newValue, newValue, newValue);
            currentError[position + 1U] += error * (7.0 / 16.0);
            nextError[position - 1U] += error * (3.0 / 16.0);
            nextError[position] += error * (5.0 / 16.0);
            nextError[position + 1U] += error * (1.0 / 16.0);
        }
        currentError.swap(nextError);
        std::fill(nextError.begin(), nextError.end(), 0.0);
    }
}

void setError(QString* error, const QString& value)
{
    if (error != nullptr) {
        *error = value;
    }
}

QImage imageValidationFailure(QString* error,
                              const QString& reasonCode,
                              QString message)
{
    setError(error, message);
    try {
        recordSstvDiagnosticEvent(
            sstvSecurityLog(),
            QtWarningMsg,
            QStringLiteral("studio.image-validation-rejected"),
            {{QStringLiteral("component"), QStringLiteral("image-reader")},
             {QStringLiteral("reasonCode"), reasonCode},
             {QStringLiteral("success"), false}});
    } catch (...) {
        // Validation behavior must not depend on diagnostic availability.
    }
    return {};
}

} // namespace

SstvPreparedImage SstvImagePreprocessor::prepare(
    const QImage& source,
    const SstvImagePreparation& preparation)
{
    if (source.isNull()) {
        return failure(SstvImagePreprocessor::tr("Source image is empty"));
    }
    if (!boundedSize(source.size())) {
        return failure(SstvImagePreprocessor::tr(
            "Source image exceeds the SSTV image bound"));
    }
    if (!boundedSize(preparation.outputSize)) {
        return failure(SstvImagePreprocessor::tr(
            "Output resolution is invalid or too large"));
    }
    if (!validCrop(preparation.crop)) {
        return failure(SstvImagePreprocessor::tr(
            "Crop rectangle must be normalised and bounded"));
    }
    if (!preparation.background.isValid() || !preparation.borderColor.isValid()) {
        return failure(SstvImagePreprocessor::tr(
            "Image preparation contains an invalid colour"));
    }
    if (!validAdjustments(preparation.adjustments)) {
        return failure(SstvImagePreprocessor::tr(
            "Image adjustment is outside its safe range"));
    }
    if (preparation.overlays.size() > kMaximumOverlays) {
        return failure(SstvImagePreprocessor::tr("Too many image overlays"));
    }
    for (const SstvTextOverlay& overlay : preparation.overlays) {
        if (!validOverlay(overlay)) {
            return failure(SstvImagePreprocessor::tr(
                "Image overlay is outside its safe range"));
        }
    }
    const int maximumBorder
        = std::min(preparation.outputSize.width(), preparation.outputSize.height())
        / 2;
    if (preparation.borderWidthPixels < 0
        || preparation.borderWidthPixels > maximumBorder) {
        return failure(SstvImagePreprocessor::tr(
            "Image border width is invalid"));
    }

    SstvPreparedImage result;
    result.sourceCropPixels = pixelCrop(source.size(), preparation.crop);
    if (preparation.lockCropToOutputAspect) {
        result.sourceCropPixels = aspectLockedCrop(result.sourceCropPixels,
                                                   preparation.outputSize,
                                                   preparation.rotation);
    }

    QImage colourManaged = source;
    if (preparation.convertToSrgb) {
        if (colourManaged.colorSpace().isValid()
            && colourManaged.colorSpace() != QColorSpace::SRgb) {
            colourManaged.convertToColorSpace(QColorSpace::SRgb);
        } else if (!colourManaged.colorSpace().isValid()) {
            colourManaged.setColorSpace(QColorSpace::SRgb);
        }
    }
    QImage working = colourManaged.copy(result.sourceCropPixels)
                         .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    working = rotateAndFlip(std::move(working),
                            preparation.rotation,
                            preparation.flipHorizontal,
                            preparation.flipVertical);
    result.image = resizeImage(working, preparation, result.warnings);
    applyColourAdjustments(result.image, preparation.adjustments);
    applySharpness(result.image, preparation.adjustments.sharpness);
    drawOverlays(result.image, preparation.overlays, result.warnings);
    drawBorder(result.image,
               preparation.borderWidthPixels,
               preparation.borderColor);
    if (preparation.adjustments.grayscale
        || preparation.adjustments.dither != SstvMonochromeDither::None) {
        convertToMonochrome(result.image, preparation.adjustments.dither);
    }
    if (preparation.convertToSrgb) {
        result.image.setColorSpace(QColorSpace::SRgb);
    }
    return result;
}

QImage SstvImagePreprocessor::readValidated(const QString& path, QString* error)
{
    setError(error, QString {});
    if (path.isEmpty()) {
        return imageValidationFailure(
            error,
            QStringLiteral("empty-path"),
            SstvImagePreprocessor::tr("Image path is empty"));
    }
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return imageValidationFailure(
            error,
            QStringLiteral("missing-file"),
            SstvImagePreprocessor::tr("Image file does not exist"));
    }
    if (info.isSymLink()) {
        return imageValidationFailure(
            error,
            QStringLiteral("symbolic-link"),
            SstvImagePreprocessor::tr(
                "Image file must not be a symbolic link"));
    }
    if (info.size() <= 0 || info.size() > kMaximumCompressedBytes) {
        return imageValidationFailure(
            error,
            QStringLiteral("compressed-size"),
            SstvImagePreprocessor::tr(
                "Compressed image exceeds the Studio file bound"));
    }

    QFile file(info.canonicalFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return imageValidationFailure(
            error,
            QStringLiteral("open-failed"),
            SstvImagePreprocessor::tr("Image file could not be opened"));
    }
    if (file.size() <= 0 || file.size() > kMaximumCompressedBytes) {
        return imageValidationFailure(
            error,
            QStringLiteral("compressed-size"),
            SstvImagePreprocessor::tr(
                "Compressed image exceeds the Studio file bound"));
    }

    // Qt's process-wide decoder allocation guard is only tightened here. A
    // stricter application policy is preserved and never relaxed.
    static std::once_flag allocationLimitOnce;
    std::call_once(allocationLimitOnce, [] {
        const int current = QImageReader::allocationLimit();
        if (current <= 0 || current > kReaderAllocationLimitMiB) {
            QImageReader::setAllocationLimit(kReaderAllocationLimitMiB);
        }
    });

    QImageReader reader(&file);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        return imageValidationFailure(
            error,
            QStringLiteral("unsupported-content"),
            reader.errorString().isEmpty()
                ? SstvImagePreprocessor::tr(
                      "File content is not a supported image")
                : reader.errorString());
    }
    if (reader.supportsAnimation() || reader.imageCount() != 1) {
        return imageValidationFailure(
            error,
            QStringLiteral("multi-frame"),
            SstvImagePreprocessor::tr(
                "Animated or multi-frame images are not accepted"));
    }
    const QSize declaredSize = reader.size();
    if (!boundedSize(declaredSize)) {
        return imageValidationFailure(
            error,
            QStringLiteral("decoded-dimensions"),
            SstvImagePreprocessor::tr(
                "Decoded image dimensions are invalid or too large"));
    }
    const qint64 declaredPixels = static_cast<qint64>(declaredSize.width())
        * static_cast<qint64>(declaredSize.height());
    if (declaredPixels <= 0
        || declaredPixels > kMaximumDecodedBytes / 8LL) {
        return imageValidationFailure(
            error,
            QStringLiteral("decoded-allocation"),
            SstvImagePreprocessor::tr(
                "Decoded image allocation exceeds the Studio bound"));
    }

    const QStringList textKeys = reader.textKeys();
    if (textKeys.size() > kMaximumMetadataKeys) {
        return imageValidationFailure(
            error,
            QStringLiteral("metadata-bound"),
            SstvImagePreprocessor::tr(
                "Image metadata exceeds the Studio bound"));
    }
    qsizetype metadataCharacters = 0;
    for (const QString& key : textKeys) {
        const QString value = reader.text(key);
        if (key.size() > kMaximumMetadataValueCharacters
            || value.size() > kMaximumMetadataValueCharacters
            || metadataCharacters
                > kMaximumMetadataCharacters - key.size()
            || metadataCharacters + key.size()
                > kMaximumMetadataCharacters - value.size()) {
            return imageValidationFailure(
                error,
                QStringLiteral("metadata-bound"),
                SstvImagePreprocessor::tr(
                    "Image metadata exceeds the Studio bound"));
        }
        metadataCharacters += key.size() + value.size();
    }
    QImage image = reader.read();
    if (image.isNull()) {
        return imageValidationFailure(
            error,
            QStringLiteral("decode-failed"),
            reader.errorString().isEmpty()
                ? SstvImagePreprocessor::tr(
                      "Image content could not be decoded")
                : reader.errorString());
    }
    if (!boundedSize(image.size())) {
        return imageValidationFailure(
            error,
            QStringLiteral("decoded-dimensions"),
            SstvImagePreprocessor::tr(
                "Decoded image exceeds the SSTV image bound"));
    }
    if (image.sizeInBytes() < 0 || image.sizeInBytes() > kMaximumDecodedBytes) {
        return imageValidationFailure(
            error,
            QStringLiteral("decoded-allocation"),
            SstvImagePreprocessor::tr(
                "Decoded image allocation exceeds the Studio bound"));
    }
    return image;
}

QImage SstvImagePreprocessor::calibrationPattern(const QSize& size,
                                                 QString* error)
{
    setError(error, QString {});
    if (!boundedSize(size)) {
        setError(error, SstvImagePreprocessor::tr(
                            "Calibration-pattern size is invalid"));
        return {};
    }

    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::black);
    image.setColorSpace(QColorSpace::SRgb);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    constexpr std::array<QRgb, 8> bars {
        qRgb(255, 255, 255), qRgb(255, 255, 0), qRgb(0, 255, 255),
        qRgb(0, 255, 0),     qRgb(255, 0, 255), qRgb(255, 0, 0),
        qRgb(0, 0, 255),     qRgb(0, 0, 0)};
    const int barBottom = std::max(1, size.height() * 2 / 3);
    for (std::size_t index = 0U; index < bars.size(); ++index) {
        const int left = static_cast<int>(index) * size.width()
            / static_cast<int>(bars.size());
        const int right = static_cast<int>(index + 1U) * size.width()
            / static_cast<int>(bars.size());
        painter.fillRect(left,
                         0,
                         std::max(1, right - left),
                         barBottom,
                         QColor::fromRgb(bars[index]));
    }
    for (int x = 0; x < size.width(); ++x) {
        const int level = size.width() == 1
            ? 128
            : static_cast<int>(std::lround(
                  static_cast<double>(x) * 255.0 / (size.width() - 1)));
        painter.setPen(QColor(level, level, level));
        painter.drawLine(x, barBottom, x, size.height() - 1);
    }
    painter.setPen(QColor(128, 128, 128));
    painter.drawLine(size.width() / 2, 0, size.width() / 2, size.height() - 1);
    painter.drawLine(0, size.height() / 2, size.width() - 1, size.height() / 2);
    painter.end();
    return image;
}

} // namespace decodium::sstv
