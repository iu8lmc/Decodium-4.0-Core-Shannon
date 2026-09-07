// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvStudioController.h"

#include "../analog/SstvAvt.h"
#include "../analog/SstvMartinM1.h"
#include "../analog/SstvMmsstvExtended.h"
#include "../analog/SstvPd.h"
#include "../analog/SstvRobot.h"
#include "../analog/SstvScottie.h"
#include "../analog/SstvSequentialRgb.h"
#include "SstvTxCoordinator.h"
#include "SstvRxRuntime.h"

#include "../diagnostics/SstvDiagnosticLogging.h"
#include "../image/SstvImageFrame.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QSettings>
#include <QSet>
#include <QThread>
#include <QtConcurrentRun>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <optional>
#include <thread>
#include <utility>

namespace decodium::sstv {
namespace {

void recordStudioLoopbackEvent(const QString& event,
                               QtMsgType severity,
                               const QString& modeId,
                               bool success,
                               const QString& reasonCode = {},
                               qint64 durationMs = -1) noexcept
{
    try {
        QVariantMap fields {
            {QStringLiteral("component"), QStringLiteral("studio-loopback")},
            {QStringLiteral("success"), success},
        };
        if (!modeId.isEmpty()) {
            fields.insert(QStringLiteral("modeId"), modeId.left(64));
        }
        if (!reasonCode.isEmpty()) {
            fields.insert(QStringLiteral("reasonCode"), reasonCode);
        }
        if (durationMs >= 0) {
            fields.insert(QStringLiteral("durationMs"), durationMs);
        }
        recordSstvDiagnosticEvent(sstvTxLog(), severity, event, fields);
    } catch (...) {
        // Diagnostics are best effort and never control Studio lifecycle.
    }
}

bool readDouble(const QVariantMap& values,
                const QString& key,
                double fallback,
                double& output,
                QString& error)
{
    const auto iterator = values.constFind(key);
    if (iterator == values.cend()) {
        output = fallback;
        return true;
    }
    bool converted = false;
    const double value = iterator->toDouble(&converted);
    if (!converted || !std::isfinite(value)) {
        error = SstvStudioController::tr(
                    "Invalid numeric image control: %1").arg(key);
        return false;
    }
    output = value;
    return true;
}

bool readInt(const QVariantMap& values,
             const QString& key,
             int fallback,
             int& output,
             QString& error)
{
    const auto iterator = values.constFind(key);
    if (iterator == values.cend()) {
        output = fallback;
        return true;
    }
    bool converted = false;
    const int value = iterator->toInt(&converted);
    if (!converted) {
        error = SstvStudioController::tr(
                    "Invalid integer image control: %1").arg(key);
        return false;
    }
    output = value;
    return true;
}

bool readColour(const QVariantMap& values,
                const QString& key,
                const QColor& fallback,
                QColor& output,
                QString& error)
{
    const auto iterator = values.constFind(key);
    if (iterator == values.cend()) {
        output = fallback;
        return true;
    }
    const QColor value(iterator->toString());
    if (!value.isValid()) {
        error = SstvStudioController::tr(
                    "Invalid image colour: %1").arg(key);
        return false;
    }
    output = value;
    return true;
}

SstvOverlayAnchor parseAnchor(const QString& value, bool& valid)
{
    valid = true;
    if (value == QStringLiteral("top-left")) {
        return SstvOverlayAnchor::TopLeft;
    }
    if (value == QStringLiteral("top-centre")) {
        return SstvOverlayAnchor::TopCentre;
    }
    if (value == QStringLiteral("top-right")) {
        return SstvOverlayAnchor::TopRight;
    }
    if (value == QStringLiteral("centre-left")) {
        return SstvOverlayAnchor::CentreLeft;
    }
    if (value == QStringLiteral("centre")) {
        return SstvOverlayAnchor::Centre;
    }
    if (value == QStringLiteral("centre-right")) {
        return SstvOverlayAnchor::CentreRight;
    }
    if (value == QStringLiteral("bottom-left")) {
        return SstvOverlayAnchor::BottomLeft;
    }
    if (value == QStringLiteral("bottom-centre")) {
        return SstvOverlayAnchor::BottomCentre;
    }
    if (value == QStringLiteral("bottom-right")) {
        return SstvOverlayAnchor::BottomRight;
    }
    valid = false;
    return SstvOverlayAnchor::BottomLeft;
}

SstvOverlayKind parseOverlayKind(const QString& value, bool& valid)
{
    valid = true;
    if (value == QStringLiteral("callsign")) {
        return SstvOverlayKind::Callsign;
    }
    if (value == QStringLiteral("locator")) {
        return SstvOverlayKind::Locator;
    }
    if (value == QStringLiteral("utc")) {
        return SstvOverlayKind::UtcDateTime;
    }
    if (value == QStringLiteral("frequency")) {
        return SstvOverlayKind::Frequency;
    }
    if (value == QStringLiteral("mode")) {
        return SstvOverlayKind::Mode;
    }
    if (value == QStringLiteral("custom")) {
        return SstvOverlayKind::CustomText;
    }
    if (value == QStringLiteral("report")) {
        return SstvOverlayKind::SignalReport;
    }
    if (value == QStringLiteral("watermark")) {
        return SstvOverlayKind::Watermark;
    }
    valid = false;
    return SstvOverlayKind::CustomText;
}

std::uint64_t nextRevision(std::uint64_t revision) noexcept
{
    ++revision;
    return revision == 0U ? 1U : revision;
}

constexpr int kTemplateSchemaVersion = 1;
const QString kTemplateSettingsGroup = QStringLiteral("SSTV/StudioTemplates");

bool isSafeTemplateName(const QString& name) noexcept
{
    if (name.isEmpty()
        || name.size() > SstvStudioController::MaximumTemplateNameCharacters) {
        return false;
    }
    return std::none_of(name.cbegin(), name.cend(), [](QChar character) {
        return character.isNull()
            || character.category() == QChar::Other_Control;
    });
}

bool containsOnlyKeys(const QVariantMap& values,
                      const QSet<QString>& allowed,
                      QString& error)
{
    for (auto iterator = values.cbegin(); iterator != values.cend(); ++iterator) {
        if (!allowed.contains(iterator.key())) {
            error = SstvStudioController::tr(
                        "Template contains an unsupported control: %1")
                        .arg(iterator.key());
            return false;
        }
    }
    return true;
}

bool validPreparationBounds(const SstvImagePreparation& preparation) noexcept
{
    const auto finite = [](double value) { return std::isfinite(value); };
    const auto& crop = preparation.crop;
    constexpr double epsilon = 1.0e-9;
    if (!finite(crop.x()) || !finite(crop.y()) || !finite(crop.width())
        || !finite(crop.height()) || crop.x() < 0.0 || crop.y() < 0.0
        || crop.width() <= 0.0 || crop.height() <= 0.0
        || crop.right() > 1.0 + epsilon
        || crop.bottom() > 1.0 + epsilon) {
        return false;
    }
    const SstvImageAdjustments& adjustment = preparation.adjustments;
    if (!finite(adjustment.exposureStops)
        || adjustment.exposureStops < -8.0 || adjustment.exposureStops > 8.0
        || !finite(adjustment.brightness)
        || adjustment.brightness < -1.0 || adjustment.brightness > 1.0
        || !finite(adjustment.contrast)
        || adjustment.contrast < 0.0 || adjustment.contrast > 4.0
        || !finite(adjustment.gamma)
        || adjustment.gamma < 0.1 || adjustment.gamma > 10.0
        || !finite(adjustment.saturation)
        || adjustment.saturation < 0.0 || adjustment.saturation > 4.0
        || !finite(adjustment.whiteBalanceRed)
        || adjustment.whiteBalanceRed < 0.0
        || adjustment.whiteBalanceRed > 4.0
        || !finite(adjustment.whiteBalanceGreen)
        || adjustment.whiteBalanceGreen < 0.0
        || adjustment.whiteBalanceGreen > 4.0
        || !finite(adjustment.whiteBalanceBlue)
        || adjustment.whiteBalanceBlue < 0.0
        || adjustment.whiteBalanceBlue > 4.0
        || !finite(adjustment.sharpness)
        || adjustment.sharpness < 0.0 || adjustment.sharpness > 2.0) {
        return false;
    }
    const int maximumBorder = std::min(preparation.outputSize.width(),
                                       preparation.outputSize.height()) / 2;
    if (preparation.borderWidthPixels < 0
        || preparation.borderWidthPixels > maximumBorder) {
        return false;
    }
    for (const SstvTextOverlay& overlay : preparation.overlays) {
        if (overlay.text.size()
                > SstvImagePreprocessor::kMaximumOverlayTextLength
            || overlay.fontFamily.size()
                > SstvImagePreprocessor::kMaximumOverlayTextLength
            || overlay.fontPixelSize < 1
            || overlay.fontPixelSize > SstvImagePreprocessor::kMaximumDimension
            || overlay.marginPixels < 0
            || overlay.marginPixels > SstvImagePreprocessor::kMaximumDimension
            || overlay.paddingPixels < 0
            || overlay.paddingPixels > SstvImagePreprocessor::kMaximumDimension
            || !finite(overlay.opacity)
            || overlay.opacity < 0.0 || overlay.opacity > 1.0) {
            return false;
        }
    }
    return true;
}

QImage imageFromSnapshot(const SstvImageSnapshot& snapshot)
{
    if (snapshot.width == 0U || snapshot.height == 0U
        || snapshot.width > SstvImageFrame::kMaximumDimension
        || snapshot.height > SstvImageFrame::kMaximumDimension
        || static_cast<std::size_t>(snapshot.width)
            > SstvImageFrame::kMaximumPixels
                / static_cast<std::size_t>(snapshot.height)
        || snapshot.pixels.size()
            != static_cast<std::size_t>(snapshot.width)
                * static_cast<std::size_t>(snapshot.height)) {
        return {};
    }
    QImage image(static_cast<int>(snapshot.width),
                 static_cast<int>(snapshot.height),
                 QImage::Format_RGB888);
    if (image.isNull()) {
        return {};
    }
    for (std::uint32_t y = 0U; y < snapshot.height; ++y) {
        uchar* const scan = image.scanLine(static_cast<int>(y));
        if (!scan) {
            return {};
        }
        for (std::uint32_t x = 0U; x < snapshot.width; ++x) {
            const std::size_t pixelIndex
                = static_cast<std::size_t>(y) * snapshot.width + x;
            const SstvRgbPixel& pixel = snapshot.pixels[pixelIndex];
            const std::size_t byte = static_cast<std::size_t>(x) * 3U;
            scan[byte] = pixel.red;
            scan[byte + 1U] = pixel.green;
            scan[byte + 2U] = pixel.blue;
        }
    }
    return image;
}

} // namespace

SstvStudioController::SstvStudioController(QObject* parent)
    : QObject(parent)
    , m_templateSettings(std::make_unique<QSettings>())
{
    initialise();
}

SstvStudioController::SstvStudioController(
    const QString& templateSettingsFile,
    QObject* parent)
    : QObject(parent)
    , m_templateSettings(std::make_unique<QSettings>(
          templateSettingsFile, QSettings::IniFormat))
{
    initialise();
}

void SstvStudioController::initialise()
{
    connect(&m_loadWatcher,
            &QFutureWatcher<SstvStudioLoadResult>::finished,
            this,
            &SstvStudioController::finishLoad);
    connect(&m_preparationWatcher,
            &QFutureWatcher<SstvPreparedImage>::finished,
            this,
            &SstvStudioController::finishPreparation);
    connect(&m_wavExportWatcher,
            &QFutureWatcher<SstvWavExportResult>::finished,
            this,
            &SstvStudioController::finishWavExport);
    connect(&m_loopbackWatcher,
            &QFutureWatcher<SstvStudioLoopbackResult>::finished,
            this,
            &SstvStudioController::finishLoopback);
    m_loopbackProgressTimer.setInterval(100);
    m_loopbackProgressTimer.setSingleShot(false);
    connect(&m_loopbackProgressTimer,
            &QTimer::timeout,
            this,
            &SstvStudioController::loopbackChanged);
    loadTemplates();
}

SstvStudioController::~SstvStudioController()
{
    m_discardPendingResult = true;
    m_discardWavResult = true;
    m_discardLoopbackResult = true;
    m_loadWatcher.disconnect(this);
    m_preparationWatcher.disconnect(this);
    m_wavExportWatcher.disconnect(this);
    m_loopbackWatcher.disconnect(this);
    if (m_wavExportCancel) {
        m_wavExportCancel->store(true, std::memory_order_release);
    }
    if (m_loopbackCancel) {
        m_loopbackCancel->store(true, std::memory_order_release);
    }
    if (m_loadWatcher.isRunning()) {
        m_loadWatcher.cancel();
        m_loadWatcher.waitForFinished();
    }
    if (m_preparationWatcher.isRunning()) {
        m_preparationWatcher.cancel();
        m_preparationWatcher.waitForFinished();
    }
    if (m_wavExportWatcher.isRunning()) {
        m_wavExportWatcher.waitForFinished();
    }
    if (m_loopbackWatcher.isRunning()) {
        m_loopbackWatcher.waitForFinished();
    }
}

bool SstvStudioController::busy() const noexcept
{
    return m_busy;
}

bool SstvStudioController::sourceReady() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_imageMutex);
    return static_cast<bool>(m_source);
}

bool SstvStudioController::preparedReady() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_imageMutex);
    return static_cast<bool>(m_prepared);
}

QString SstvStudioController::sourceName() const
{
    return m_sourceName;
}

QString SstvStudioController::sourceImageSource() const
{
    const std::lock_guard<std::mutex> lock(m_imageMutex);
    return m_source
        ? QStringLiteral("image://decodium-sstv/tx-source/%1")
              .arg(static_cast<qulonglong>(m_sourceRevision))
        : QString {};
}

QString SstvStudioController::preparedImageSource() const
{
    const std::lock_guard<std::mutex> lock(m_imageMutex);
    return m_prepared
        ? QStringLiteral("image://decodium-sstv/tx-prepared/%1")
              .arg(static_cast<qulonglong>(m_preparedRevision))
        : QString {};
}

QString SstvStudioController::modeId() const
{
    return m_modeId;
}

QString SstvStudioController::modeName() const
{
    const ModeDescriptor* mode = findMode(m_modeId);
    return mode ? mode->name : QString {};
}

QSize SstvStudioController::outputSize() const
{
    const ModeDescriptor* mode = findMode(m_modeId);
    return mode ? mode->size : QSize {};
}

double SstvStudioController::estimatedDurationSeconds() const noexcept
{
    const ModeDescriptor* mode = findMode(m_modeId);
    return mode ? mode->durationSeconds : 0.0;
}

QVariantList SstvStudioController::modes() const
{
    QVariantList result;
    const QList<ModeDescriptor>& available = executableModes();
    result.reserve(available.size());
    for (const ModeDescriptor& mode : available) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), mode.id);
        row.insert(QStringLiteral("name"), mode.name);
        row.insert(QStringLiteral("width"), mode.size.width());
        row.insert(QStringLiteral("height"), mode.size.height());
        row.insert(QStringLiteral("durationSeconds"), mode.durationSeconds);
        result.push_back(row);
    }
    return result;
}

QString SstvStudioController::error() const
{
    return m_error;
}

QStringList SstvStudioController::warnings() const
{
    return m_warnings;
}

bool SstvStudioController::wavExportBusy() const noexcept
{
    return m_wavExportBusy;
}

QUrl SstvStudioController::wavExportFolder() const
{
    return m_wavExportRoot.isEmpty()
        ? QUrl {} : QUrl::fromLocalFile(m_wavExportRoot);
}

QVariantList SstvStudioController::wavSampleRates() const
{
    return {12'000, 24'000, 48'000};
}

QString SstvStudioController::wavExportPath() const
{
    return m_wavExportPath;
}

QString SstvStudioController::wavExportWarning() const
{
    return m_wavExportWarning;
}

QVariantList SstvStudioController::templates() const
{
    QVariantList result;
    result.reserve(m_templates.size());
    for (auto iterator = m_templates.cbegin(); iterator != m_templates.cend();
         ++iterator) {
        QVariantMap row;
        row.insert(QStringLiteral("name"), iterator.key());
        row.insert(QStringLiteral("modeId"),
                   iterator.value().value(QStringLiteral("modeId")));
        result.push_back(row);
    }
    return result;
}

bool SstvStudioController::loopbackBusy() const noexcept
{
    return m_loopbackBusy;
}

bool SstvStudioController::loopbackReady() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_imageMutex);
    return static_cast<bool>(m_loopback);
}

QString SstvStudioController::loopbackImageSource() const
{
    const std::lock_guard<std::mutex> lock(m_imageMutex);
    return m_loopback
        ? QStringLiteral("image://decodium-sstv/tx-loopback/%1")
              .arg(static_cast<qulonglong>(m_loopbackRevision))
        : QString {};
}

QString SstvStudioController::loopbackState() const
{
    return m_loopbackState;
}

QString SstvStudioController::loopbackError() const
{
    return m_loopbackError;
}

double SstvStudioController::loopbackProgress() const noexcept
{
    if (!m_loopbackProduced || !m_loopbackTotal) {
        return loopbackReady() ? 1.0 : 0.0;
    }
    const std::uint64_t total = m_loopbackTotal->load(
        std::memory_order_acquire);
    const std::uint64_t produced = m_loopbackProduced->load(
        std::memory_order_acquire);
    return total == 0U ? 0.0
        : std::clamp(static_cast<double>(
              static_cast<long double>(produced)
              / static_cast<long double>(total)), 0.0, 1.0);
}

QVariantMap SstvStudioController::loopbackMetrics() const
{
    return m_loopbackMetrics;
}

void SstvStudioController::setModeId(const QString& id)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QString normalised = id.trimmed().toLower();
    if (normalised == m_modeId) {
        return;
    }
    if (!findMode(normalised)) {
        setError(tr("Selected SSTV TX mode is not executable"));
        return;
    }
    if (m_busy) {
        setError(tr("Wait for the current image operation to finish"));
        return;
    }
    if (m_loopbackBusy) {
        setError(tr("Cancel the current Studio loopback before changing mode"));
        return;
    }
    m_modeId = normalised;
    clearPrepared();
    setError({});
    emit modeChanged();
}

void SstvStudioController::setWavExportRoot(const QString& absolutePath)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QString clean = QDir::cleanPath(absolutePath.trimmed());
    const QString accepted = !clean.isEmpty() && QFileInfo(clean).isAbsolute()
        ? clean : QString {};
    if (accepted == m_wavExportRoot) {
        return;
    }
    m_wavExportRoot = accepted;
    emit wavExportChanged();
}

bool SstvStudioController::loadSource(const QUrl& localFile)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_busy) {
        setError(tr("An image operation is already running"));
        return false;
    }
    if (!localFile.isValid() || !localFile.isLocalFile()) {
        setError(tr("Only local image files can be loaded"));
        return false;
    }
    const QString path = localFile.toLocalFile();
    if (path.isEmpty()) {
        setError(tr("Image path is empty"));
        return false;
    }
    const QString name = QFileInfo(path).fileName();
    m_discardPendingResult = false;
    setError({});
    setBusy(true);
    m_loadWatcher.setFuture(QtConcurrent::run([path, name]() {
        SstvStudioLoadResult result;
        result.sourceName = name;
        try {
            result.image = SstvImagePreprocessor::readValidated(path,
                                                                &result.error);
        } catch (const std::exception& exception) {
            result.error = SstvStudioController::tr(
                               "Image decoding failed: %1")
                               .arg(QString::fromUtf8(exception.what()));
        } catch (...) {
            result.error = SstvStudioController::tr(
                "Image decoding failed unexpectedly");
        }
        return result;
    }));
    return true;
}

bool SstvStudioController::pasteSource()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_busy) {
        setError(tr("An image operation is already running"));
        return false;
    }
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        setError(tr("The system clipboard is unavailable"));
        return false;
    }
    const QImage image = clipboard->image();
    if (image.isNull()) {
        setError(tr("The clipboard does not contain an image"));
        return false;
    }
    return acceptSource(image, tr("Clipboard"));
}

bool SstvStudioController::generateCalibrationPattern()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_busy) {
        setError(tr("An image operation is already running"));
        return false;
    }
    QString patternError;
    QImage image = SstvImagePreprocessor::calibrationPattern(outputSize(),
                                                             &patternError);
    if (image.isNull()) {
        setError(patternError);
        return false;
    }
    return acceptSource(std::move(image), tr("Calibration pattern"));
}

bool SstvStudioController::prepareImage(const QVariantMap& controls)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_busy) {
        setError(tr("An image operation is already running"));
        return false;
    }
    const std::shared_ptr<const QImage> source = sourceSnapshot();
    if (!source) {
        setError(tr("Load or paste a source image first"));
        return false;
    }
    const ModeDescriptor* mode = findMode(m_modeId);
    if (!mode) {
        setError(tr("Selected SSTV TX mode is not executable"));
        return false;
    }

    SstvImagePreparation preparation;
    QString parseError;
    if (!parsePreparation(controls, mode->size, preparation, parseError)) {
        setError(parseError);
        return false;
    }

    m_discardPendingResult = false;
    m_warnings.clear();
    setError({});
    setBusy(true);
    m_preparationWatcher.setFuture(QtConcurrent::run([source, preparation]() {
        try {
            return SstvImagePreprocessor::prepare(*source, preparation);
        } catch (const std::exception& exception) {
            SstvPreparedImage result;
            result.error = SstvStudioController::tr(
                               "Image preparation failed: %1")
                               .arg(QString::fromUtf8(exception.what()));
            return result;
        } catch (...) {
            SstvPreparedImage result;
            result.error = SstvStudioController::tr(
                "Image preparation failed unexpectedly");
            return result;
        }
    }));
    return true;
}

QUrl SstvStudioController::suggestedWavUrl(const QString& callsign) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_wavExportRoot.isEmpty()) {
        return {};
    }
    const QString station = wavFileToken(callsign, QStringLiteral("SSTV"));
    const QString mode = wavFileToken(m_modeId, QStringLiteral("ANALOG"));
    const QString timestamp = QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyyMMdd-HHmmsszzz'Z'"));
    const QString fileName = QStringLiteral("%1_%2_%3.wav")
                                 .arg(timestamp, station, mode);
    return QUrl::fromLocalFile(
        QDir(m_wavExportRoot).absoluteFilePath(fileName));
}

bool SstvStudioController::exportWav(const QUrl& destination,
                                     int sampleRate,
                                     bool writeMetadataSidecar,
                                     bool replaceExisting,
                                     const QString& fskId)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_busy) {
        setError(tr("An image operation is already running"));
        return false;
    }
    const std::shared_ptr<const QImage> prepared = preparedSnapshot();
    if (!prepared || prepared->isNull()) {
        setError(tr("Prepare an SSTV image before exporting WAV"));
        return false;
    }
    if (!destination.isValid() || !destination.isLocalFile()) {
        setError(tr("WAV export requires a local file destination"));
        return false;
    }
    if (!supportedWavSampleRate(sampleRate)) {
        setError(tr("Unsupported SSTV WAV sample rate"));
        return false;
    }

    const std::optional<SstvTxCoordinatorMode> mode
        = SstvTxSourceBuilder::modeFromId(m_modeId.toStdString());
    if (!mode.has_value()) {
        setError(tr("Selected SSTV TX mode is not executable"));
        return false;
    }

    QString outputPath = QDir::cleanPath(destination.toLocalFile().trimmed());
    QFileInfo outputInfo(outputPath);
    if (outputInfo.suffix().isEmpty()) {
        outputPath += QStringLiteral(".wav");
        outputInfo.setFile(outputPath);
    } else if (outputInfo.suffix().compare(QStringLiteral("wav"),
                                           Qt::CaseInsensitive) != 0) {
        setError(tr("SSTV audio exports must use the .wav extension"));
        return false;
    }
    if (outputPath.isEmpty() || !outputInfo.isAbsolute()
        || outputInfo.fileName().isEmpty()) {
        setError(tr("WAV export path is invalid"));
        return false;
    }

    const QString identifier = fskId.trimmed().toUpper();
    SstvTxSourceBuilderConfig sourceConfig;
    sourceConfig.mode = *mode;
    sourceConfig.sampleRate = static_cast<std::uint32_t>(sampleRate);
    if (!identifier.isEmpty()) {
        SstvTxFskIdPlan plan;
        plan.text = identifier.toStdString();
        sourceConfig.fskId = std::move(plan);
    }

    SstvWavExportRequest request;
    request.outputPath = outputPath;
    request.mode = m_modeId;
    request.writeMetadataSidecar = writeMetadataSidecar;
    request.replaceExisting = replaceExisting;
    request.metadata.insert(QStringLiteral("sourceName"),
                            m_sourceName.left(255));
    request.metadata.insert(QStringLiteral("modeName"), modeName());
    request.metadata.insert(QStringLiteral("preparedWidth"),
                            prepared->width());
    request.metadata.insert(QStringLiteral("preparedHeight"),
                            prepared->height());
    if (!identifier.isEmpty()) {
        request.metadata.insert(QStringLiteral("fskId"), identifier);
    }

    m_wavExportCancel = std::make_shared<std::atomic_bool>(false);
    const std::shared_ptr<std::atomic_bool> cancel = m_wavExportCancel;
    m_discardWavResult = false;
    m_wavExportPath.clear();
    m_wavExportWarning.clear();
    m_wavExportBusy = true;
    setError({});
    setBusy(true);
    emit wavExportChanged();

    try {
        m_wavExportWatcher.setFuture(QtConcurrent::run(
            [prepared, sourceConfig, request, cancel]() mutable {
                SstvWavExportResult result;
                if (cancel->load(std::memory_order_acquire)) {
                    result.code = SstvWavExportError::Cancelled;
                    result.error = SstvStudioController::tr(
                        "WAV export was cancelled");
                    return result;
                }
                try {
                    SstvTxBuiltSource source = SstvTxSourceBuilder::build(
                        *prepared, sourceConfig);
                    return SstvWavExporter::exportAtomic(
                        std::move(source.source), request, cancel);
                } catch (const std::exception& exception) {
                    result.code = SstvWavExportError::SourceFailure;
                    result.error = SstvStudioController::tr(
                        "Cannot build SSTV WAV source: %1")
                                       .arg(QString::fromUtf8(exception.what()))
                                       .left(SstvWavExporter::MaximumErrorCharacters);
                } catch (...) {
                    result.code = SstvWavExportError::SourceFailure;
                    result.error = SstvStudioController::tr(
                        "Cannot build SSTV WAV source unexpectedly");
                }
                return result;
            }));
    } catch (const std::exception& exception) {
        m_wavExportCancel.reset();
        m_wavExportBusy = false;
        setBusy(false);
        setError(tr("Cannot start SSTV WAV export: %1")
                     .arg(QString::fromUtf8(exception.what()))
                     .left(SstvWavExporter::MaximumErrorCharacters));
        emit wavExportChanged();
        return false;
    } catch (...) {
        m_wavExportCancel.reset();
        m_wavExportBusy = false;
        setBusy(false);
        setError(tr("Cannot start SSTV WAV export unexpectedly"));
        emit wavExportChanged();
        return false;
    }
    return true;
}

bool SstvStudioController::saveTemplate(const QString& name,
                                        const QVariantMap& controls)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QString cleanName = name.trimmed();
    if (!isSafeTemplateName(cleanName)) {
        setError(tr("Template name must contain 1 to %1 printable characters")
                     .arg(MaximumTemplateNameCharacters));
        return false;
    }
    if (!m_templates.contains(cleanName)
        && m_templates.size() >= MaximumTemplates) {
        setError(tr("The Studio template limit has been reached"));
        return false;
    }
    QString validationError;
    if (!validateTemplateControls(controls, outputSize(), validationError)) {
        setError(validationError);
        return false;
    }

    QVariantMap definition;
    definition.insert(QStringLiteral("modeId"), m_modeId);
    definition.insert(QStringLiteral("controls"), controls);
    const auto previous = m_templates.value(cleanName);
    const bool existed = m_templates.contains(cleanName);
    m_templates.insert(cleanName, definition);
    if (!persistTemplates()) {
        if (existed) {
            m_templates.insert(cleanName, previous);
        } else {
            m_templates.remove(cleanName);
        }
        setError(tr("Could not save Studio templates"));
        return false;
    }
    setError({});
    emit templatesChanged();
    return true;
}

bool SstvStudioController::deleteTemplate(const QString& name)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QString cleanName = name.trimmed();
    const auto iterator = m_templates.find(cleanName);
    if (iterator == m_templates.end()) {
        setError(tr("Studio template was not found"));
        return false;
    }
    const QVariantMap previous = iterator.value();
    m_templates.erase(iterator);
    if (!persistTemplates()) {
        m_templates.insert(cleanName, previous);
        setError(tr("Could not delete Studio template"));
        return false;
    }
    setError({});
    emit templatesChanged();
    return true;
}

QVariantMap SstvStudioController::templateDefinition(const QString& name) const
{
    return m_templates.value(name.trimmed());
}

bool SstvStudioController::startLoopback()
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto reject = [this](QString message,
                               const QString& reasonCode) {
        m_loopbackError = std::move(message);
        recordStudioLoopbackEvent(
            QStringLiteral("studio.loopback-rejected"),
            QtWarningMsg,
            m_modeId,
            false,
            reasonCode);
        emit loopbackChanged();
        return false;
    };
    if (m_loopbackBusy) {
        return reject(tr("A Studio loopback is already running"),
                      QStringLiteral("already-running"));
    }
    if (m_busy) {
        return reject(tr("Wait for the current Studio operation to finish"),
                      QStringLiteral("studio-busy"));
    }
    const std::shared_ptr<const QImage> prepared = preparedSnapshot();
    if (!prepared || prepared->isNull()) {
        return reject(tr("Prepare an SSTV image before loopback"),
                      QStringLiteral("prepared-image-unavailable"));
    }
    if (!SstvTxSourceBuilder::modeFromId(m_modeId.toStdString()).has_value()) {
        return reject(tr("Selected SSTV TX mode is not executable"),
                      QStringLiteral("unsupported-mode"));
    }

    {
        const std::lock_guard<std::mutex> lock(m_imageMutex);
        m_loopback.reset();
        m_loopbackRevision = nextRevision(m_loopbackRevision);
    }
    m_loopbackCancel = std::make_shared<std::atomic_bool>(false);
    m_loopbackProduced
        = std::make_shared<std::atomic<std::uint64_t>>(0U);
    m_loopbackTotal
        = std::make_shared<std::atomic<std::uint64_t>>(0U);
    m_loopbackMetrics.clear();
    m_loopbackError.clear();
    m_loopbackState = tr("Encoding and decoding native PCM");
    m_loopbackBusy = true;
    m_discardLoopbackResult = false;
    m_loopbackDiagnosticElapsed.start();
    m_loopbackProgressTimer.start();
    emit loopbackChanged();

    const auto cancel = m_loopbackCancel;
    const auto produced = m_loopbackProduced;
    const auto total = m_loopbackTotal;
    const QString selectedMode = m_modeId;
    try {
        m_loopbackWatcher.setFuture(QtConcurrent::run(
            [prepared, selectedMode, cancel, produced, total]() {
                return SstvStudioController::runLoopback(
                    prepared, selectedMode, cancel, produced, total);
            }));
    } catch (const std::exception& exception) {
        m_loopbackProgressTimer.stop();
        m_loopbackBusy = false;
        m_loopbackState = tr("Error");
        m_loopbackError = tr("Cannot start Studio loopback: %1")
                              .arg(QString::fromUtf8(exception.what()))
                              .left(256);
        recordStudioLoopbackEvent(
            QStringLiteral("studio.loopback-failed"),
            QtWarningMsg,
            selectedMode,
            false,
            QStringLiteral("dispatch-exception"),
            m_loopbackDiagnosticElapsed.elapsed());
        emit loopbackChanged();
        return false;
    } catch (...) {
        m_loopbackProgressTimer.stop();
        m_loopbackBusy = false;
        m_loopbackState = tr("Error");
        m_loopbackError = tr("Cannot start Studio loopback unexpectedly");
        recordStudioLoopbackEvent(
            QStringLiteral("studio.loopback-failed"),
            QtWarningMsg,
            selectedMode,
            false,
            QStringLiteral("dispatch-exception"),
            m_loopbackDiagnosticElapsed.elapsed());
        emit loopbackChanged();
        return false;
    }
    recordStudioLoopbackEvent(
        QStringLiteral("studio.loopback-started"),
        QtInfoMsg,
        selectedMode,
        true);
    return true;
}

void SstvStudioController::cancelLoopback()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_loopbackBusy || !m_loopbackCancel) {
        return;
    }
    if (m_loopbackCancel->exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    m_loopbackState = tr("Cancelling loopback");
    recordStudioLoopbackEvent(
        QStringLiteral("studio.loopback-cancel-requested"),
        QtInfoMsg,
        m_modeId,
        true,
        QString {},
        m_loopbackDiagnosticElapsed.isValid()
            ? m_loopbackDiagnosticElapsed.elapsed() : 0);
    emit loopbackChanged();
}

void SstvStudioController::clearSource()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_busy) {
        cancelWork();
    }
    if (m_loopbackBusy) {
        cancelLoopback();
    }
    {
        const std::lock_guard<std::mutex> lock(m_imageMutex);
        m_source.reset();
        m_prepared.reset();
        m_loopback.reset();
        m_sourceRevision = nextRevision(m_sourceRevision);
        m_preparedRevision = nextRevision(m_preparedRevision);
        m_loopbackRevision = nextRevision(m_loopbackRevision);
    }
    m_sourceName.clear();
    m_warnings.clear();
    emit sourceChanged();
    emit preparedChanged();
    emit loopbackChanged();
}

void SstvStudioController::cancelWork()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_loopbackBusy) {
        cancelLoopback();
        if (!m_busy) {
            return;
        }
    }
    if (!m_busy) {
        return;
    }
    if (m_wavExportBusy) {
        if (m_wavExportCancel) {
            m_wavExportCancel->store(true, std::memory_order_release);
        }
        setError(tr("WAV export cancellation requested"));
        return;
    }
    m_discardPendingResult = true;
    if (m_loadWatcher.isRunning()) {
        m_loadWatcher.cancel();
    }
    if (m_preparationWatcher.isRunning()) {
        m_preparationWatcher.cancel();
    }
    setError(tr("Image operation cancelled"));
}

std::shared_ptr<const QImage> SstvStudioController::sourceSnapshot() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_imageMutex);
    return m_source;
}

std::shared_ptr<const QImage> SstvStudioController::preparedSnapshot() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_imageMutex);
    return m_prepared;
}

std::shared_ptr<const QImage> SstvStudioController::loopbackSnapshot() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_imageMutex);
    return m_loopback;
}

const QList<SstvStudioController::ModeDescriptor>&
SstvStudioController::executableModes()
{
    static const QList<ModeDescriptor> modes = [] {
        const auto martin = [](SstvMartinMode mode) {
            const SstvMartinModeSpec spec = SstvMartinM1Protocol::spec(mode);
            return ModeDescriptor {
                QString::fromLatin1(spec.stableId),
                QString::fromLatin1(spec.displayName),
                QSize(static_cast<int>(spec.width), static_cast<int>(spec.height)),
                static_cast<double>(SstvMartinM1Protocol::HeaderDuration.count
                                    + spec.imageDuration.count)
                    / kPicosecondsPerSecond};
        };
        const auto scottie = [](SstvScottieMode mode) {
            const SstvScottieModeSpec spec = SstvScottieProtocol::spec(mode);
            return ModeDescriptor {
                QString::fromLatin1(spec.stableId),
                QString::fromLatin1(spec.displayName),
                QSize(static_cast<int>(spec.width), static_cast<int>(spec.height)),
                static_cast<double>(SstvScottieProtocol::HeaderDuration.count
                                    + spec.imageDuration.count)
                    / kPicosecondsPerSecond};
        };
        const auto robot = [](SstvRobotMode mode) {
            const SstvRobotModeSpec spec = SstvRobotProtocol::spec(mode);
            return ModeDescriptor {
                QString::fromLatin1(spec.stableId),
                QString::fromLatin1(spec.displayName),
                QSize(static_cast<int>(spec.width), static_cast<int>(spec.height)),
                static_cast<double>(SstvRobotProtocol::HeaderDuration.count
                                    + spec.imageDuration.count)
                    / kPicosecondsPerSecond};
        };
        const auto sequentialRgb = [](SstvSequentialRgbMode mode) {
            const SstvSequentialRgbModeSpec spec =
                SstvSequentialRgbProtocol::spec(mode);
            return ModeDescriptor {
                QString::fromLatin1(spec.stableId),
                QString::fromLatin1(spec.displayName),
                QSize(static_cast<int>(spec.width),
                      static_cast<int>(spec.height)),
                static_cast<double>(
                    SstvSequentialRgbProtocol::HeaderDuration.count
                    + spec.imageDuration.count)
                    / kPicosecondsPerSecond};
        };
        const auto pd = [](SstvPdMode mode) {
            const SstvPdModeSpec spec = SstvPdProtocol::spec(mode);
            return ModeDescriptor {
                QString::fromLatin1(spec.stableId),
                QString::fromLatin1(spec.displayName),
                QSize(static_cast<int>(spec.width),
                      static_cast<int>(spec.height)),
                static_cast<double>(SstvPdProtocol::HeaderDuration.count
                                    + spec.imageDuration.count)
                    / kPicosecondsPerSecond};
        };
        const auto avt = [](SstvAvtMode mode) {
            const SstvAvtModeSpec spec = SstvAvtProtocol::spec(mode);
            return ModeDescriptor {
                QString::fromLatin1(spec.stableId),
                QString::fromLatin1(spec.displayName),
                QSize(static_cast<int>(spec.width),
                      static_cast<int>(spec.height)),
                static_cast<double>(SstvAvtProtocol::HeaderDuration.count
                                    + spec.imageDuration.count)
                    / kPicosecondsPerSecond};
        };
        const auto mmsstv = [](SstvMmsstvMode mode) {
            const SstvMmsstvModeSpec spec = SstvMmsstvProtocol::spec(mode);
            return ModeDescriptor {
                QString::fromLatin1(spec.stableId),
                QString::fromLatin1(spec.displayName),
                QSize(static_cast<int>(spec.width),
                      static_cast<int>(spec.height)),
                static_cast<double>(spec.headerDuration.count
                                    + spec.imageDuration.count)
                    / kPicosecondsPerSecond};
        };
        return QList<ModeDescriptor> {
            martin(SstvMartinMode::M1),
            martin(SstvMartinMode::M2),
            martin(SstvMartinMode::M3),
            martin(SstvMartinMode::M4),
            scottie(SstvScottieMode::S1),
            scottie(SstvScottieMode::S2),
            scottie(SstvScottieMode::S3),
            scottie(SstvScottieMode::S4),
            scottie(SstvScottieMode::DX),
            robot(SstvRobotMode::Colour12),
            robot(SstvRobotMode::Colour24),
            robot(SstvRobotMode::Colour36),
            robot(SstvRobotMode::Colour72),
            robot(SstvRobotMode::Bw8),
            robot(SstvRobotMode::Bw12),
            robot(SstvRobotMode::Bw24),
            robot(SstvRobotMode::Bw36),
            sequentialRgb(SstvSequentialRgbMode::WraaseSc2_60),
            sequentialRgb(SstvSequentialRgbMode::WraaseSc2_120),
            sequentialRgb(SstvSequentialRgbMode::WraaseSc2_180),
            sequentialRgb(SstvSequentialRgbMode::PasokonP3),
            sequentialRgb(SstvSequentialRgbMode::PasokonP5),
            sequentialRgb(SstvSequentialRgbMode::PasokonP7),
            pd(SstvPdMode::Pd50),
            pd(SstvPdMode::Pd90),
            pd(SstvPdMode::Pd120),
            pd(SstvPdMode::Pd160),
            pd(SstvPdMode::Pd180),
            pd(SstvPdMode::Pd240),
            pd(SstvPdMode::Pd290),
            avt(SstvAvtMode::Avt24),
            avt(SstvAvtMode::Avt90),
            avt(SstvAvtMode::Avt94),
            mmsstv(SstvMmsstvMode::Mp73),
            mmsstv(SstvMmsstvMode::Mp115),
            mmsstv(SstvMmsstvMode::Mp140),
            mmsstv(SstvMmsstvMode::Mp175),
            mmsstv(SstvMmsstvMode::Mr73),
            mmsstv(SstvMmsstvMode::Mr90),
            mmsstv(SstvMmsstvMode::Mr115),
            mmsstv(SstvMmsstvMode::Mr140),
            mmsstv(SstvMmsstvMode::Mr175),
            mmsstv(SstvMmsstvMode::Ml180),
            mmsstv(SstvMmsstvMode::Ml240),
            mmsstv(SstvMmsstvMode::Ml280),
            mmsstv(SstvMmsstvMode::Ml320),
            mmsstv(SstvMmsstvMode::Mp73Narrow),
            mmsstv(SstvMmsstvMode::Mp110Narrow),
            mmsstv(SstvMmsstvMode::Mp140Narrow),
            mmsstv(SstvMmsstvMode::Mc110Narrow),
            mmsstv(SstvMmsstvMode::Mc140Narrow),
            mmsstv(SstvMmsstvMode::Mc180Narrow)};
    }();
    return modes;
}

const SstvStudioController::ModeDescriptor*
SstvStudioController::findMode(const QString& id)
{
    const QList<ModeDescriptor>& modes = executableModes();
    for (const ModeDescriptor& mode : modes) {
        if (mode.id == id) {
            return &mode;
        }
    }
    return nullptr;
}

bool SstvStudioController::parsePreparation(
    const QVariantMap& controls,
    const QSize& outputSize,
    SstvImagePreparation& preparation,
    QString& error)
{
    preparation = {};
    preparation.outputSize = outputSize;
    preparation.smoothScaling
        = controls.value(QStringLiteral("smoothScaling"), true).toBool();
    preparation.convertToSrgb = true;
    preparation.flipHorizontal
        = controls.value(QStringLiteral("flipHorizontal"), false).toBool();
    preparation.flipVertical
        = controls.value(QStringLiteral("flipVertical"), false).toBool();
    preparation.lockCropToOutputAspect
        = controls.value(QStringLiteral("aspectLock"), false).toBool();

    const QString resize = controls.value(QStringLiteral("resizeMode"),
                                          QStringLiteral("fit"))
                               .toString()
                               .trimmed()
                               .toLower();
    if (resize == QStringLiteral("fit")) {
        preparation.resizeMode = SstvImageResizeMode::FitLetterbox;
    } else if (resize == QStringLiteral("fill")) {
        preparation.resizeMode = SstvImageResizeMode::FillCrop;
    } else if (resize == QStringLiteral("stretch")) {
        preparation.resizeMode = SstvImageResizeMode::Stretch;
    } else {
        error = tr("Unknown image resize mode");
        return false;
    }

    int rotation = 0;
    if (!readInt(controls, QStringLiteral("rotation"), 0, rotation, error)) {
        return false;
    }
    if (rotation == 0) {
        preparation.rotation = SstvImageRotation::None;
    } else if (rotation == 90) {
        preparation.rotation = SstvImageRotation::Clockwise90;
    } else if (rotation == 180) {
        preparation.rotation = SstvImageRotation::Clockwise180;
    } else if (rotation == 270) {
        preparation.rotation = SstvImageRotation::Clockwise270;
    } else {
        error = tr("Rotation must be 0, 90, 180 or 270 degrees");
        return false;
    }

    double cropX = 0.0;
    double cropY = 0.0;
    double cropWidth = 1.0;
    double cropHeight = 1.0;
    if (!readDouble(controls, QStringLiteral("cropX"), 0.0, cropX, error)
        || !readDouble(controls, QStringLiteral("cropY"), 0.0, cropY, error)
        || !readDouble(controls,
                       QStringLiteral("cropWidth"),
                       1.0,
                       cropWidth,
                       error)
        || !readDouble(controls,
                       QStringLiteral("cropHeight"),
                       1.0,
                       cropHeight,
                       error)) {
        return false;
    }
    preparation.crop = {cropX, cropY, cropWidth, cropHeight};

    if (!readDouble(controls,
                    QStringLiteral("exposure"),
                    0.0,
                    preparation.adjustments.exposureStops,
                    error)
        || !readDouble(controls,
                       QStringLiteral("brightness"),
                       0.0,
                       preparation.adjustments.brightness,
                       error)
        || !readDouble(controls,
                       QStringLiteral("contrast"),
                       1.0,
                       preparation.adjustments.contrast,
                       error)
        || !readDouble(controls,
                       QStringLiteral("gamma"),
                       1.0,
                       preparation.adjustments.gamma,
                       error)
        || !readDouble(controls,
                       QStringLiteral("saturation"),
                       1.0,
                       preparation.adjustments.saturation,
                       error)
        || !readDouble(controls,
                       QStringLiteral("whiteBalanceRed"),
                       1.0,
                       preparation.adjustments.whiteBalanceRed,
                       error)
        || !readDouble(controls,
                       QStringLiteral("whiteBalanceGreen"),
                       1.0,
                       preparation.adjustments.whiteBalanceGreen,
                       error)
        || !readDouble(controls,
                       QStringLiteral("whiteBalanceBlue"),
                       1.0,
                       preparation.adjustments.whiteBalanceBlue,
                       error)
        || !readDouble(controls,
                       QStringLiteral("sharpness"),
                       0.0,
                       preparation.adjustments.sharpness,
                       error)) {
        return false;
    }
    preparation.adjustments.grayscale
        = controls.value(QStringLiteral("grayscale"), false).toBool();
    preparation.adjustments.dither
        = controls.value(QStringLiteral("dither"), false).toBool()
        ? SstvMonochromeDither::FloydSteinberg
        : SstvMonochromeDither::None;

    if (!readColour(controls,
                    QStringLiteral("background"),
                    Qt::black,
                    preparation.background,
                    error)
        || !readInt(controls,
                    QStringLiteral("borderWidth"),
                    0,
                    preparation.borderWidthPixels,
                    error)
        || !readColour(controls,
                       QStringLiteral("borderColor"),
                       Qt::white,
                       preparation.borderColor,
                       error)) {
        return false;
    }

    const QVariantList overlays
        = controls.value(QStringLiteral("overlays")).toList();
    if (overlays.size() > SstvImagePreprocessor::kMaximumOverlays) {
        error = tr("Too many image overlays");
        return false;
    }
    preparation.overlays.reserve(overlays.size());
    for (const QVariant& value : overlays) {
        if (!value.canConvert<QVariantMap>()) {
            error = tr("Image overlay must be an object");
            return false;
        }
        const QVariantMap map = value.toMap();
        SstvTextOverlay overlay;
        overlay.text = map.value(QStringLiteral("text")).toString();
        bool valid = false;
        overlay.kind = parseOverlayKind(
            map.value(QStringLiteral("kind"), QStringLiteral("custom"))
                .toString()
                .trimmed()
                .toLower(),
            valid);
        if (!valid) {
            error = tr("Unknown image overlay kind");
            return false;
        }
        overlay.anchor = parseAnchor(
            map.value(QStringLiteral("anchor"), QStringLiteral("bottom-left"))
                .toString()
                .trimmed()
                .toLower(),
            valid);
        if (!valid) {
            error = tr("Unknown image overlay anchor");
            return false;
        }
        overlay.fontFamily = map.value(QStringLiteral("fontFamily")).toString();
        overlay.bold = map.value(QStringLiteral("bold"), true).toBool();
        if (!readInt(map,
                     QStringLiteral("fontPixelSize"),
                     18,
                     overlay.fontPixelSize,
                     error)
            || !readInt(map,
                        QStringLiteral("margin"),
                        8,
                        overlay.marginPixels,
                        error)
            || !readInt(map,
                        QStringLiteral("padding"),
                        3,
                        overlay.paddingPixels,
                        error)
            || !readColour(map,
                           QStringLiteral("foreground"),
                           Qt::white,
                           overlay.foreground,
                           error)
            || !readColour(map,
                           QStringLiteral("background"),
                           QColor(0, 0, 0, 144),
                           overlay.background,
                           error)
            || !readDouble(map,
                           QStringLiteral("opacity"),
                           1.0,
                           overlay.opacity,
                           error)) {
            return false;
        }
        preparation.overlays.push_back(std::move(overlay));
    }
    return true;
}

bool SstvStudioController::validateTemplateControls(
    const QVariantMap& controls,
    const QSize& outputSize,
    QString& error)
{
    static const QSet<QString> allowedControls {
        QStringLiteral("resizeMode"), QStringLiteral("rotation"),
        QStringLiteral("flipHorizontal"), QStringLiteral("flipVertical"),
        QStringLiteral("aspectLock"), QStringLiteral("cropX"),
        QStringLiteral("cropY"), QStringLiteral("cropWidth"),
        QStringLiteral("cropHeight"), QStringLiteral("exposure"),
        QStringLiteral("brightness"), QStringLiteral("contrast"),
        QStringLiteral("gamma"), QStringLiteral("saturation"),
        QStringLiteral("whiteBalanceRed"),
        QStringLiteral("whiteBalanceGreen"),
        QStringLiteral("whiteBalanceBlue"), QStringLiteral("sharpness"),
        QStringLiteral("grayscale"), QStringLiteral("dither"),
        QStringLiteral("background"), QStringLiteral("borderWidth"),
        QStringLiteral("borderColor"), QStringLiteral("overlays")};
    static const QSet<QString> allowedOverlayControls {
        QStringLiteral("kind"), QStringLiteral("text"),
        QStringLiteral("anchor"), QStringLiteral("fontFamily"),
        QStringLiteral("fontPixelSize"), QStringLiteral("bold"),
        QStringLiteral("margin"), QStringLiteral("padding"),
        QStringLiteral("foreground"), QStringLiteral("background"),
        QStringLiteral("opacity")};

    if (!containsOnlyKeys(controls, allowedControls, error)) {
        return false;
    }
    const QVariantList overlayValues
        = controls.value(QStringLiteral("overlays")).toList();
    for (const QVariant& value : overlayValues) {
        if (!value.canConvert<QVariantMap>()
            || !containsOnlyKeys(value.toMap(),
                                 allowedOverlayControls,
                                 error)) {
            if (error.isEmpty()) {
                error = tr("Template overlay must be a bounded object");
            }
            return false;
        }
    }

    const QByteArray bytes = QJsonDocument(
        QJsonObject::fromVariantMap(controls)).toJson(QJsonDocument::Compact);
    if (bytes.isEmpty() || bytes.size() > MaximumTemplateBytes) {
        error = tr("Studio template exceeds the storage bound");
        return false;
    }

    SstvImagePreparation preparation;
    if (!parsePreparation(controls, outputSize, preparation, error)) {
        return false;
    }
    if (!validPreparationBounds(preparation)) {
        error = tr("Studio template contains an out-of-range image control");
        return false;
    }
    return true;
}

SstvStudioLoopbackResult SstvStudioController::runLoopback(
    std::shared_ptr<const QImage> prepared,
    QString modeId,
    std::shared_ptr<std::atomic_bool> cancel,
    std::shared_ptr<std::atomic<std::uint64_t>> produced,
    std::shared_ptr<std::atomic<std::uint64_t>> total)
{
    SstvStudioLoopbackResult result;
    QElapsedTimer elapsed;
    elapsed.start();
    try {
        if (!prepared || prepared->isNull() || !cancel || !produced || !total) {
            result.error = tr("Studio loopback input is unavailable");
            return result;
        }
        const std::optional<SstvTxCoordinatorMode> mode
            = SstvTxSourceBuilder::modeFromId(modeId.toStdString());
        if (!mode.has_value()) {
            result.error = tr("Selected SSTV loopback mode is not executable");
            return result;
        }

        SstvTxSourceBuilderConfig sourceConfig;
        sourceConfig.mode = *mode;
        sourceConfig.sampleRate = 12'000U;
        SstvTxBuiltSource built = SstvTxSourceBuilder::build(
            *prepared, sourceConfig);
        total->store(built.totalFrames, std::memory_order_release);

        SstvRxRuntime::Config runtimeConfig;
        runtimeConfig.ingress.maximumChunks = 16U;
        runtimeConfig.ingress.maximumQueuedSamples = 65'536U;
        runtimeConfig.ingress.maximumSamplesPerCall = 4'096U;
        runtimeConfig.snapshotNotificationIntervalMs = 100U;
        SstvRxRuntime runtime(runtimeConfig);
        if (!runtime.start(SstvAudioSourceKind::Replay, 1U)) {
            result.error = tr("Native SSTV receiver could not start for loopback");
            return result;
        }
        const SstvRxRouteToken token = runtime.routeToken();

        constexpr std::size_t chunkCapacity = 4'093U;
        constexpr qint64 observedTimestampNs = 1'000'000'000LL;
        std::uint64_t chunks = 0U;
        std::uint32_t peakMagnitude = 0U;
        std::uint64_t clippedSamples = 0U;
        while (!built.source->complete()) {
            if (cancel->load(std::memory_order_acquire)) {
                built.source->cancel();
                static_cast<void>(runtime.cancel());
                result.cancelled = true;
                static_cast<void>(runtime.shutdown());
                return result;
            }
            QVector<short> pcm(static_cast<qsizetype>(chunkCapacity));
            static_assert(sizeof(short) == sizeof(std::int16_t));
            const std::size_t count = built.source->pullPcm16(
                reinterpret_cast<std::int16_t*>(pcm.data()), chunkCapacity);
            if (count == 0U) {
                if (built.source->complete()) {
                    break;
                }
                result.error = tr("Native SSTV loopback encoder made no progress");
                static_cast<void>(runtime.shutdown());
                return result;
            }
            pcm.resize(static_cast<qsizetype>(count));
            for (const short sample : pcm) {
                const std::int32_t widened = sample;
                const std::uint32_t magnitude = static_cast<std::uint32_t>(
                    widened < 0 ? -widened : widened);
                peakMagnitude = std::max(peakMagnitude, magnitude);
                if (magnitude >= 32'767U) {
                    ++clippedSamples;
                }
            }
            if (!runtime.enqueuePcm16At(
                    std::move(pcm), 12'000, token, observedTimestampNs)) {
                result.error = tr("Native SSTV receiver rejected loopback PCM");
                static_cast<void>(runtime.shutdown());
                return result;
            }
            ++chunks;
            produced->store(built.source->producedSamples(),
                            std::memory_order_release);

            QElapsedTimer chunkDeadline;
            chunkDeadline.start();
            while (runtime.snapshot().generationChunksProcessed < chunks
                   && chunkDeadline.elapsed() < 5'000) {
                if (cancel->load(std::memory_order_acquire)) {
                    built.source->cancel();
                    static_cast<void>(runtime.cancel());
                    result.cancelled = true;
                    static_cast<void>(runtime.shutdown());
                    return result;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (runtime.snapshot().generationChunksProcessed != chunks) {
                result.error = tr("Native SSTV loopback decoder timed out");
                static_cast<void>(runtime.shutdown());
                return result;
            }
        }

        // A bounded zero tail closes the receiver's final demodulation window.
        // It is ordinary PCM routed through SstvRxRuntime, not a decoder-state
        // or pixel shortcut, and is excluded from the native protocol count.
        QVector<short> flushPcm(512, short {0});
        if (!runtime.enqueuePcm16At(
                std::move(flushPcm), 12'000, token, observedTimestampNs)) {
            result.error = tr("Native SSTV receiver rejected loopback flush PCM");
            static_cast<void>(runtime.shutdown());
            return result;
        }
        ++chunks;
        QElapsedTimer flushDeadline;
        flushDeadline.start();
        while (runtime.snapshot().generationChunksProcessed < chunks
               && flushDeadline.elapsed() < 5'000) {
            if (cancel->load(std::memory_order_acquire)) {
                static_cast<void>(runtime.cancel());
                result.cancelled = true;
                static_cast<void>(runtime.shutdown());
                return result;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        const SstvRxRuntime::Snapshot snapshot = runtime.snapshot();
        const std::shared_ptr<const SstvImageSnapshot> decoded
            = runtime.latestImageSnapshot();
        if (!decoded || !snapshot.image.available
            || !snapshot.image.complete || !decoded->isComplete()) {
            result.error = snapshot.lastError.isEmpty()
                ? tr("Native SSTV loopback did not produce a complete image")
                : snapshot.lastError.left(256);
            static_cast<void>(runtime.shutdown());
            return result;
        }
        result.image = imageFromSnapshot(*decoded);
        if (result.image.isNull()) {
            result.error = tr("Native SSTV loopback image is invalid");
            static_cast<void>(runtime.shutdown());
            return result;
        }
        result.metrics.insert(QStringLiteral("mode"), modeId.left(64));
        result.metrics.insert(QStringLiteral("sampleRate"), 12'000);
        result.metrics.insert(QStringLiteral("encodedSamples"),
                              QVariant::fromValue<qulonglong>(
                                  built.source->producedSamples()));
        result.metrics.insert(QStringLiteral("chunks"),
                              QVariant::fromValue<qulonglong>(chunks));
        result.metrics.insert(QStringLiteral("width"), result.image.width());
        result.metrics.insert(QStringLiteral("height"), result.image.height());
        result.metrics.insert(QStringLiteral("coverage"),
                              snapshot.image.coverage);
        result.metrics.insert(QStringLiteral("pcmPeak"),
                              static_cast<double>(peakMagnitude) / 32'768.0);
        result.metrics.insert(QStringLiteral("clippedSamples"),
                              QVariant::fromValue<qulonglong>(clippedSamples));
        result.metrics.insert(QStringLiteral("dspBlocks"),
                              QVariant::fromValue<qulonglong>(
                                  snapshot.performance.measuredDspBlocks));
        result.metrics.insert(QStringLiteral("elapsedMilliseconds"),
                              elapsed.elapsed());
        produced->store(built.totalFrames, std::memory_order_release);
        static_cast<void>(runtime.shutdown());
    } catch (const std::exception& exception) {
        result.error = tr("Native SSTV loopback failed: %1")
                           .arg(QString::fromUtf8(exception.what()))
                           .left(256);
    } catch (...) {
        result.error = tr("Native SSTV loopback failed unexpectedly");
    }
    return result;
}

void SstvStudioController::loadTemplates()
{
    if (!m_templateSettings) {
        return;
    }
    m_templateSettings->beginGroup(kTemplateSettingsGroup);
    const int schema = m_templateSettings
                           ->value(QStringLiteral("schemaVersion"), 0)
                           .toInt();
    const QByteArray catalog
        = m_templateSettings->value(QStringLiteral("catalog")).toByteArray();
    const QVariantMap legacy
        = m_templateSettings->value(QStringLiteral("templates")).toMap();
    m_templateSettings->endGroup();

    if (schema == kTemplateSchemaVersion && !catalog.isEmpty()
        && catalog.size() <= MaximumTemplateCatalogBytes) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            catalog, &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !document.isArray()) {
            return;
        }
        for (const QJsonValue& value : document.array()) {
            if (!value.isObject() || m_templates.size() >= MaximumTemplates) {
                continue;
            }
            const QJsonObject object = value.toObject();
            const QString name = object.value(QStringLiteral("name"))
                                     .toString().trimmed();
            const QString mode = object.value(QStringLiteral("modeId"))
                                     .toString().trimmed().toLower();
            const QVariantMap controls
                = object.value(QStringLiteral("controls")).toObject()
                      .toVariantMap();
            const ModeDescriptor* descriptor = findMode(mode);
            QString error;
            if (!isSafeTemplateName(name) || !descriptor
                || !validateTemplateControls(
                    controls, descriptor->size, error)) {
                continue;
            }
            QVariantMap definition;
            definition.insert(QStringLiteral("modeId"), mode);
            definition.insert(QStringLiteral("controls"), controls);
            m_templates.insert(name, definition);
        }
        return;
    }

    // Schema 0 stored a direct name -> controls map. Import only validated
    // values, then rewrite it in the bounded versioned catalog.
    if (schema == 0 && !legacy.isEmpty()) {
        for (auto iterator = legacy.cbegin(); iterator != legacy.cend()
             && m_templates.size() < MaximumTemplates; ++iterator) {
            const QString name = iterator.key().trimmed();
            const QVariantMap controls = iterator.value().toMap();
            QString error;
            if (!isSafeTemplateName(name)
                || !validateTemplateControls(
                    controls, outputSize(), error)) {
                continue;
            }
            QVariantMap definition;
            definition.insert(QStringLiteral("modeId"), m_modeId);
            definition.insert(QStringLiteral("controls"), controls);
            m_templates.insert(name, definition);
        }
        static_cast<void>(persistTemplates());
    }
}

bool SstvStudioController::persistTemplates()
{
    if (!m_templateSettings || m_templates.size() > MaximumTemplates) {
        return false;
    }
    QJsonArray catalog;
    for (auto iterator = m_templates.cbegin(); iterator != m_templates.cend();
         ++iterator) {
        const QVariantMap definition = iterator.value();
        QJsonObject object;
        object.insert(QStringLiteral("name"), iterator.key());
        object.insert(QStringLiteral("modeId"),
                      definition.value(QStringLiteral("modeId")).toString());
        object.insert(QStringLiteral("controls"),
                      QJsonObject::fromVariantMap(
                          definition.value(QStringLiteral("controls")).toMap()));
        catalog.push_back(object);
    }
    const QByteArray bytes = QJsonDocument(catalog).toJson(
        QJsonDocument::Compact);
    if (bytes.size() > MaximumTemplateCatalogBytes) {
        return false;
    }
    m_templateSettings->beginGroup(kTemplateSettingsGroup);
    m_templateSettings->setValue(QStringLiteral("schemaVersion"),
                                 kTemplateSchemaVersion);
    m_templateSettings->setValue(QStringLiteral("catalog"), bytes);
    m_templateSettings->remove(QStringLiteral("templates"));
    m_templateSettings->endGroup();
    m_templateSettings->sync();
    return m_templateSettings->status() == QSettings::NoError;
}

bool SstvStudioController::boundedImage(const QImage& image) noexcept
{
    const QSize size = image.size();
    return !image.isNull() && size.width() > 0 && size.height() > 0
        && size.width() <= SstvImagePreprocessor::kMaximumDimension
        && size.height() <= SstvImagePreprocessor::kMaximumDimension
        && static_cast<qint64>(size.width())
            <= SstvImagePreprocessor::kMaximumPixels / size.height();
}

bool SstvStudioController::supportedWavSampleRate(int sampleRate) noexcept
{
    constexpr std::array<int, 3> rates {12'000, 24'000, 48'000};
    return std::find(rates.cbegin(), rates.cend(), sampleRate) != rates.cend();
}

QString SstvStudioController::wavFileToken(const QString& value,
                                           const QString& fallback)
{
    QString token;
    token.reserve(std::min(value.size(), qsizetype {32}));
    bool separatorPending = false;
    for (const QChar character : value.trimmed()) {
        if (character.isLetterOrNumber()) {
            if (separatorPending && !token.isEmpty()
                && token.size() < 32) {
                token.append(QLatin1Char('-'));
            }
            separatorPending = false;
            if (token.size() < 32) {
                token.append(character.toUpper());
            }
        } else if (!token.isEmpty()) {
            separatorPending = true;
        }
        if (token.size() >= 32) {
            break;
        }
    }
    return token.isEmpty() ? fallback : token;
}

bool SstvStudioController::acceptSource(QImage image, QString sourceName)
{
    if (!boundedImage(image)) {
        setError(tr("Source image is empty or exceeds the SSTV bound"));
        return false;
    }
    auto immutable = std::make_shared<const QImage>(std::move(image));
    {
        const std::lock_guard<std::mutex> lock(m_imageMutex);
        m_source = std::move(immutable);
        m_prepared.reset();
        m_loopback.reset();
        m_sourceRevision = nextRevision(m_sourceRevision);
        m_preparedRevision = nextRevision(m_preparedRevision);
        m_loopbackRevision = nextRevision(m_loopbackRevision);
    }
    m_sourceName = std::move(sourceName);
    m_warnings.clear();
    setError({});
    emit sourceChanged();
    emit preparedChanged();
    emit loopbackChanged();
    return true;
}

void SstvStudioController::setError(QString value)
{
    if (m_error == value) {
        return;
    }
    m_error = std::move(value);
    emit stateChanged();
}

void SstvStudioController::setBusy(bool value)
{
    if (m_busy == value) {
        return;
    }
    m_busy = value;
    emit stateChanged();
}

void SstvStudioController::finishLoad()
{
    Q_ASSERT(QThread::currentThread() == thread());
    setBusy(false);
    const bool discard = std::exchange(m_discardPendingResult, false);
    if (discard || m_loadWatcher.isCanceled()) {
        return;
    }
    const SstvStudioLoadResult result = m_loadWatcher.result();
    if (!result.error.isEmpty() || result.image.isNull()) {
        setError(result.error.isEmpty()
                     ? tr("Image content could not be decoded")
                     : result.error);
        return;
    }
    acceptSource(result.image, result.sourceName);
}

void SstvStudioController::finishPreparation()
{
    Q_ASSERT(QThread::currentThread() == thread());
    setBusy(false);
    const bool discard = std::exchange(m_discardPendingResult, false);
    if (discard || m_preparationWatcher.isCanceled()) {
        return;
    }
    SstvPreparedImage result = m_preparationWatcher.result();
    if (!result.isValid()) {
        clearPrepared();
        setError(result.error.isEmpty()
                     ? tr("Image preparation did not produce a frame")
                     : result.error);
        return;
    }
    auto immutable
        = std::make_shared<const QImage>(std::move(result.image));
    {
        const std::lock_guard<std::mutex> lock(m_imageMutex);
        m_prepared = std::move(immutable);
        m_preparedRevision = nextRevision(m_preparedRevision);
        m_loopback.reset();
        m_loopbackRevision = nextRevision(m_loopbackRevision);
    }
    m_warnings = std::move(result.warnings);
    setError({});
    emit preparedChanged();
    emit loopbackChanged();
}

void SstvStudioController::finishWavExport()
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_wavExportBusy = false;
    setBusy(false);
    m_wavExportCancel.reset();
    const bool discard = std::exchange(m_discardWavResult, false);
    if (discard) {
        emit wavExportChanged();
        return;
    }

    const SstvWavExportResult result = m_wavExportWatcher.result();
    m_wavExportPath = result.ok ? result.wavPath : QString {};
    m_wavExportWarning = result.warning;
    if (result.ok) {
        setError({});
    } else {
        setError(result.error.isEmpty()
                     ? tr("SSTV WAV export failed")
                     : result.error);
    }
    emit wavExportChanged();
}

void SstvStudioController::finishLoopback()
{
    Q_ASSERT(QThread::currentThread() == thread());
    const qint64 durationMs = m_loopbackDiagnosticElapsed.isValid()
        ? m_loopbackDiagnosticElapsed.elapsed() : 0;
    m_loopbackProgressTimer.stop();
    m_loopbackBusy = false;
    m_loopbackCancel.reset();
    const bool discard = std::exchange(m_discardLoopbackResult, false);
    if (discard) {
        m_loopbackState = tr("Cancelled");
        recordStudioLoopbackEvent(
            QStringLiteral("studio.loopback-cancelled"),
            QtInfoMsg,
            m_modeId,
            true,
            QStringLiteral("result-discarded"),
            durationMs);
        emit loopbackChanged();
        return;
    }

    SstvStudioLoopbackResult result = m_loopbackWatcher.result();
    if (result.cancelled) {
        m_loopbackState = tr("Cancelled");
        m_loopbackError.clear();
        m_loopbackMetrics.clear();
        recordStudioLoopbackEvent(
            QStringLiteral("studio.loopback-cancelled"),
            QtInfoMsg,
            m_modeId,
            true,
            QStringLiteral("operator-cancelled"),
            durationMs);
    } else if (!result.error.isEmpty() || result.image.isNull()) {
        m_loopbackState = tr("Error");
        m_loopbackError = result.error.isEmpty()
            ? tr("Native SSTV loopback produced no image")
            : result.error.left(256);
        m_loopbackMetrics = std::move(result.metrics);
        recordStudioLoopbackEvent(
            QStringLiteral("studio.loopback-failed"),
            QtWarningMsg,
            m_modeId,
            false,
            QStringLiteral("worker-failed"),
            durationMs);
    } else {
        {
            const std::lock_guard<std::mutex> lock(m_imageMutex);
            m_loopback = std::make_shared<const QImage>(
                std::move(result.image));
            m_loopbackRevision = nextRevision(m_loopbackRevision);
        }
        m_loopbackState = tr("Complete");
        m_loopbackError.clear();
        m_loopbackMetrics = std::move(result.metrics);
        recordStudioLoopbackEvent(
            QStringLiteral("studio.loopback-completed"),
            QtInfoMsg,
            m_modeId,
            true,
            QString {},
            durationMs);
    }
    m_loopbackProduced.reset();
    m_loopbackTotal.reset();
    emit loopbackChanged();
}

void SstvStudioController::clearPrepared()
{
    bool changed = false;
    bool loopbackWasReady = false;
    {
        const std::lock_guard<std::mutex> lock(m_imageMutex);
        changed = static_cast<bool>(m_prepared);
        loopbackWasReady = static_cast<bool>(m_loopback);
        m_prepared.reset();
        m_loopback.reset();
        if (changed) {
            m_preparedRevision = nextRevision(m_preparedRevision);
        }
        if (loopbackWasReady) {
            m_loopbackRevision = nextRevision(m_loopbackRevision);
        }
    }
    m_warnings.clear();
    if (changed) {
        emit preparedChanged();
    }
    if (loopbackWasReady) {
        emit loopbackChanged();
    }
}

} // namespace decodium::sstv
