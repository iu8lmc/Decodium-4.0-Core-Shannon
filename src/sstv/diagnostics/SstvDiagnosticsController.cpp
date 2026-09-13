// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvDiagnosticsController.h"

#include "SstvDiagnosticLogging.h"
#include "../core/SstvModeRegistry.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSysInfo>

#include <array>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace decodium::sstv {
namespace {

constexpr qsizetype kMaximumDestinationLength = 4096;

void appendHashPart(QCryptographicHash& hash, const QByteArray& value)
{
    const QByteArray length = QByteArray::number(value.size());
    hash.addData(length);
    hash.addData(QByteArrayLiteral(":"));
    hash.addData(value);
    hash.addData(QByteArrayLiteral(";"));
}

void appendHashPart(QCryptographicHash& hash, const std::string& value)
{
    appendHashPart(hash, QByteArray(value.data(),
                                    static_cast<qsizetype>(value.size())));
}

template<typename Integer,
         std::enable_if_t<std::is_integral_v<Integer>
                          || std::is_enum_v<Integer>, int> = 0>
void appendHashPart(QCryptographicHash& hash, Integer value)
{
    appendHashPart(hash,
                   QByteArray::number(static_cast<qint64>(value)));
}

template<typename Value>
void appendOptional(QCryptographicHash& hash,
                    const std::optional<Value>& value)
{
    appendHashPart(hash, value.has_value() ? 1 : 0);
    if (value.has_value()) {
        appendHashPart(hash, *value);
    }
}

void appendPicoseconds(QCryptographicHash& hash,
                       const std::optional<Picoseconds>& value)
{
    appendHashPart(hash, value.has_value() ? 1 : 0);
    if (value.has_value()) {
        appendHashPart(hash, value->count);
    }
}

template<typename Value>
void appendVector(QCryptographicHash& hash,
                  const std::vector<Value>& values)
{
    appendHashPart(hash, values.size());
    for (const Value& value : values) {
        appendHashPart(hash, value);
    }
}

QVariantMap canonicalRegistryInfo()
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    appendHashPart(hash, QByteArrayLiteral("decodium-sstv-mode-registry-v1"));
    const SstvModeRegistry registry = SstvModeRegistry::canonical();
    const auto& modes = registry.modes();
    appendHashPart(hash, modes.size());
    for (const SstvModeSpec& mode : modes) {
        appendHashPart(hash, mode.id);
        appendHashPart(hash, mode.longName);
        appendHashPart(hash, mode.shortName);
        appendHashPart(hash, mode.family);
        appendHashPart(hash, mode.classification);
        appendHashPart(hash, mode.catalogStatus);
        appendHashPart(hash, mode.rxStatus);
        appendHashPart(hash, mode.txStatus);
        appendHashPart(hash, mode.autoDetectStatus);
        appendHashPart(hash, mode.protocolDataComplete ? 1 : 0);
        appendHashPart(hash, mode.vis.has_value() ? 1 : 0);
        if (mode.vis.has_value()) {
            appendHashPart(hash, mode.vis->encoding);
            appendHashPart(hash, mode.vis->bitCount);
            appendOptional(hash, mode.vis->standardCode);
            appendVector(hash, mode.vis->standardAliases);
            appendVector(hash, mode.vis->extendedSequence);
            appendHashPart(hash, mode.vis->lsbFirst ? 1 : 0);
            appendHashPart(hash, mode.vis->parity);
            appendHashPart(hash, mode.vis->documentedSharedCodeGroup);
        }
        appendOptional(hash, mode.geometry.imageWidth);
        appendOptional(hash, mode.geometry.imageHeight);
        appendOptional(hash, mode.geometry.sampledPixelWidth);
        appendOptional(hash, mode.geometry.transmittedPixelWidth);
        appendOptional(hash, mode.geometry.transmittedLineCount);
        appendOptional(hash, mode.geometry.displayedLineCount);
        appendOptional(hash, mode.geometry.linesPerScan);
        appendHashPart(hash, mode.colour.colourSpace);
        appendVector(hash, mode.colour.componentOrder);
        appendHashPart(hash, mode.colour.chromaSubsampling);
        appendHashPart(hash, mode.colour.conversionRule);
        appendOptional(hash, mode.timing.syncFrequencyHz);
        appendPicoseconds(hash, mode.timing.syncDuration);
        appendPicoseconds(hash, mode.timing.frontPorch);
        appendPicoseconds(hash, mode.timing.backPorch);
        appendOptional(hash, mode.timing.separatorFrequencyHz);
        appendPicoseconds(hash, mode.timing.separatorDuration);
        appendPicoseconds(hash, mode.timing.pixelDuration);
        appendPicoseconds(hash, mode.timing.componentDuration);
        appendPicoseconds(hash, mode.timing.lineDuration);
        appendPicoseconds(hash, mode.timing.imageDuration);
        appendHashPart(hash,
                       mode.timing.nominalAudioBandwidth.has_value() ? 1 : 0);
        if (mode.timing.nominalAudioBandwidth.has_value()) {
            appendHashPart(hash, mode.timing.nominalAudioBandwidth->lowHz);
            appendHashPart(hash, mode.timing.nominalAudioBandwidth->highHz);
        }
        appendOptional(hash, mode.timing.tolerancePpm);
        appendHashPart(hash, mode.leaderHeaderRules);
        appendHashPart(hash, mode.specialLineOrdering);
        appendPicoseconds(hash,
                          mode.fallbackSignature.nominalLineDuration);
        appendPicoseconds(hash,
                          mode.fallbackSignature.nominalSyncDuration);
        appendOptional(hash, mode.fallbackSignature.syncFrequencyHz);
        appendHashPart(hash, mode.fallbackSignature.discriminator);
        appendVector(hash, mode.catalogueReferences);
        appendVector(hash, mode.protocolProvenance);
        appendHashPart(hash, mode.evidenceStatus);
        appendVector(hash, mode.implementationEvidenceRefs);
        appendHashPart(hash, mode.interoperabilityStatus);
        appendHashPart(hash, mode.fixtureStatus);
        appendHashPart(hash, mode.statusNote);
    }
    return {
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("modeCount"),
         static_cast<qulonglong>(modes.size())},
        {QStringLiteral("valid"), registry.isValid()},
        {QStringLiteral("sha256"),
         QString::fromLatin1(hash.result().toHex())},
    };
}

QVariantMap applicationInfo()
{
    QString version = QCoreApplication::applicationVersion().trimmed();
    if (version.isEmpty()) {
        version = QStringLiteral("unknown");
    }
#if defined(QT_DEBUG)
    const QString buildType = QStringLiteral("debug");
#else
    const QString buildType = QStringLiteral("release");
#endif
    return {
        {QStringLiteral("name"), QStringLiteral("Decodium")},
        {QStringLiteral("version"), version.left(64)},
        {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())},
        {QStringLiteral("buildType"), buildType},
    };
}

QVariantMap platformInfo()
{
    return {
        {QStringLiteral("productType"),
         QSysInfo::productType().left(64)},
        {QStringLiteral("productVersion"),
         QSysInfo::productVersion().left(64)},
        {QStringLiteral("kernelType"), QSysInfo::kernelType().left(64)},
        {QStringLiteral("kernelVersion"),
         QSysInfo::kernelVersion().left(64)},
        {QStringLiteral("cpuArchitecture"),
         QSysInfo::currentCpuArchitecture().left(64)},
        {QStringLiteral("buildAbi"), QSysInfo::buildAbi().left(96)},
    };
}

QString exportErrorText(const QString& errorCode)
{
    if (errorCode == QStringLiteral("invalid-destination")) {
        return SstvDiagnosticsController::tr(
            "Choose a local .json destination.");
    }
    if (errorCode == QStringLiteral("unsafe-destination")) {
        return SstvDiagnosticsController::tr(
            "The selected destination is not safe.");
    }
    if (errorCode == QStringLiteral("already-exists")) {
        return SstvDiagnosticsController::tr(
            "A report already exists at that destination.");
    }
    if (errorCode == QStringLiteral("open-failed")) {
        return SstvDiagnosticsController::tr(
            "The diagnostic report could not be opened for writing.");
    }
    if (errorCode == QStringLiteral("write-failed")) {
        return SstvDiagnosticsController::tr(
            "The diagnostic report could not be written.");
    }
    if (errorCode == QStringLiteral("commit-failed")) {
        return SstvDiagnosticsController::tr(
            "The diagnostic report could not be committed atomically.");
    }
    if (errorCode == QStringLiteral("report-too-large")) {
        return SstvDiagnosticsController::tr(
            "The bounded diagnostic report exceeded its size limit.");
    }
    return SstvDiagnosticsController::tr(
        "The diagnostic export failed.");
}

bool isMapValue(const QVariant& value)
{
    return value.metaType().id() == QMetaType::QVariantMap;
}

} // namespace

class SstvDiagnosticsExportWorker final : public QObject
{
    Q_OBJECT

public slots:
    void writeReport(quint64 requestId,
                     const QByteArray& report,
                     const QString& destinationPath,
                     bool replaceExisting)
    {
        QString errorCode;
        const QFileInfo destination(destinationPath);
        if (destinationPath.isEmpty()
            || destinationPath.size() > kMaximumDestinationLength
            || !destination.isAbsolute()
            || destination.suffix().compare(QStringLiteral("json"),
                                            Qt::CaseInsensitive) != 0) {
            errorCode = QStringLiteral("invalid-destination");
        } else if (destination.isSymLink()
                   || destination.fileName().size() > 255
                   || destinationPath.contains(QChar::Null)) {
            errorCode = QStringLiteral("unsafe-destination");
        } else if (destination.exists() && !replaceExisting) {
            errorCode = QStringLiteral("already-exists");
        } else if (report.isEmpty()
                   || report.size()
                       > SstvDiagnosticsController::kMaximumReportBytes) {
            errorCode = QStringLiteral("report-too-large");
        } else {
            QSaveFile output(destinationPath);
            output.setDirectWriteFallback(false);
            if (!output.open(QIODevice::WriteOnly)) {
                errorCode = QStringLiteral("open-failed");
            } else {
                output.setPermissions(QFileDevice::ReadOwner
                                      | QFileDevice::WriteOwner);
                const qint64 written = output.write(report);
                if (written != static_cast<qint64>(report.size())) {
                    output.cancelWriting();
                    errorCode = QStringLiteral("write-failed");
                } else if (!output.commit()) {
                    errorCode = QStringLiteral("commit-failed");
                }
            }
        }
        emit writeFinished(requestId, errorCode.isEmpty(), errorCode);
    }

signals:
    void writeFinished(quint64 requestId,
                       bool success,
                       const QString& errorCode);
};

SstvDiagnosticsController::SstvDiagnosticsController(QObject* parent)
    : QObject(parent)
    , m_applicationInfo(::decodium::sstv::applicationInfo())
    , m_platformInfo(::decodium::sstv::platformInfo())
    , m_modeRegistryInfo(canonicalRegistryInfo())
    , m_statusText(tr("Diagnostics ready"))
    , m_worker(new SstvDiagnosticsExportWorker)
{
    m_worker->moveToThread(&m_workerThread);
    connect(this, &SstvDiagnosticsController::writeReportRequested,
            m_worker, &SstvDiagnosticsExportWorker::writeReport,
            Qt::QueuedConnection);
    connect(m_worker, &SstvDiagnosticsExportWorker::writeFinished,
            this, &SstvDiagnosticsController::handleWriteFinished,
            Qt::QueuedConnection);
    connect(&m_workerThread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    m_workerThread.setObjectName(QStringLiteral("SstvDiagnosticsExport"));
    m_workerThread.start();
    m_ready = true;
    refresh();
}

SstvDiagnosticsController::~SstvDiagnosticsController()
{
    shutdown();
}

bool SstvDiagnosticsController::ready() const noexcept { return m_ready; }
bool SstvDiagnosticsController::exporting() const noexcept
{
    return m_exporting;
}
QVariantMap SstvDiagnosticsController::applicationInfo() const
{
    return m_applicationInfo;
}
QVariantMap SstvDiagnosticsController::platformInfo() const
{
    return m_platformInfo;
}
QVariantMap SstvDiagnosticsController::modeRegistryInfo() const
{
    return m_modeRegistryInfo;
}
QVariantMap SstvDiagnosticsController::capabilities() const
{
    return m_capabilities;
}
QVariantMap SstvDiagnosticsController::settings() const { return m_settings; }
QVariantMap SstvDiagnosticsController::rxMetrics() const { return m_rxMetrics; }
QVariantMap SstvDiagnosticsController::txMetrics() const { return m_txMetrics; }
QVariantMap SstvDiagnosticsController::storageMetrics() const
{
    return m_storageMetrics;
}
QVariantMap SstvDiagnosticsController::shareMetrics() const
{
    return m_shareMetrics;
}
QVariantMap SstvDiagnosticsController::hamdrmMetrics() const
{
    return m_hamdrmMetrics;
}
QVariantMap SstvDiagnosticsController::calibrationResults() const
{
    return m_calibrationResults;
}
QVariantMap SstvDiagnosticsController::testToneResults() const
{
    return m_testToneResults;
}
QVariantList SstvDiagnosticsController::recentEvents() const
{
    return m_recentEvents;
}
QString SstvDiagnosticsController::statusText() const { return m_statusText; }
QString SstvDiagnosticsController::errorString() const { return m_errorString; }

bool SstvDiagnosticsController::setInputSnapshot(
    const QVariantMap& snapshot, QString* errorMessage)
{
    static const std::array<QString, 9> allowedSections {
        QStringLiteral("capabilities"), QStringLiteral("settings"),
        QStringLiteral("rx"), QStringLiteral("tx"),
        QStringLiteral("storage"), QStringLiteral("share"),
        QStringLiteral("hamdrm"), QStringLiteral("calibration"),
        QStringLiteral("testTone"),
    };
    const auto allowed = [](const QString& key) {
        return std::find(allowedSections.cbegin(), allowedSections.cend(), key)
            != allowedSections.cend();
    };
    for (auto it = snapshot.cbegin(); it != snapshot.cend(); ++it) {
        if (!allowed(it.key()) || !isMapValue(it.value())) {
            const QString message = tr(
                "The diagnostics input contains an unsupported section.");
            if (errorMessage != nullptr) {
                *errorMessage = message;
            }
            recordSstvDiagnosticEvent(
                sstvSecurityLog(), QtWarningMsg,
                QStringLiteral("diagnostics.input-rejected"),
                {{QStringLiteral("reasonCode"),
                  QStringLiteral("unsupported-section")}});
            return false;
        }
    }

    const auto map = [&snapshot](const QString& key) {
        return snapshot.value(key).toMap();
    };
    m_capabilities = SstvDiagnosticRedactor::capabilities(
        map(QStringLiteral("capabilities")));
    m_settings = SstvDiagnosticRedactor::settings(
        map(QStringLiteral("settings")));
    m_rxMetrics = SstvDiagnosticRedactor::metrics(
        QStringLiteral("rx"), map(QStringLiteral("rx")));
    m_txMetrics = SstvDiagnosticRedactor::metrics(
        QStringLiteral("tx"), map(QStringLiteral("tx")));
    m_storageMetrics = SstvDiagnosticRedactor::metrics(
        QStringLiteral("storage"), map(QStringLiteral("storage")));
    m_shareMetrics = SstvDiagnosticRedactor::metrics(
        QStringLiteral("share"), map(QStringLiteral("share")));
    m_hamdrmMetrics = SstvDiagnosticRedactor::metrics(
        QStringLiteral("hamdrm"), map(QStringLiteral("hamdrm")));
    m_calibrationResults = SstvDiagnosticRedactor::metrics(
        QStringLiteral("calibration"), map(QStringLiteral("calibration")));
    m_testToneResults = SstvDiagnosticRedactor::metrics(
        QStringLiteral("testTone"), map(QStringLiteral("testTone")));
    emit reportDataChanged();
    refreshEventSnapshot();
    return true;
}

void SstvDiagnosticsController::refresh()
{
    if (!m_shutdown && m_ready) {
        emit inputSnapshotRequested();
    }
    refreshEventSnapshot();
}

void SstvDiagnosticsController::refreshEventSnapshot()
{
    m_recentEvents = SstvDiagnosticLogBuffer::instance().snapshot();
    emit recentEventsChanged();
}

void SstvDiagnosticsController::exportReport(const QUrl& destination,
                                              bool replaceExisting)
{
    if (m_shutdown || !m_ready) {
        const QString message = tr("Diagnostics are not available.");
        setErrorString(message);
        emit exportFinished(false, message);
        return;
    }
    if (m_exporting) {
        const QString message = tr(
            "A diagnostic export is already running.");
        setErrorString(message);
        emit exportFinished(false, message);
        return;
    }
    if (!destination.isLocalFile()) {
        const QString message = exportErrorText(
            QStringLiteral("invalid-destination"));
        setErrorString(message);
        emit exportFinished(false, message);
        return;
    }

    refresh();
    QString reportError;
    const QByteArray report = buildReport(&reportError);
    if (report.isEmpty()) {
        const QString message = reportError.isEmpty()
            ? exportErrorText(QStringLiteral("report-too-large"))
            : reportError;
        setErrorString(message);
        emit exportFinished(false, message);
        return;
    }
    m_activeRequestId = m_nextRequestId;
    if (m_nextRequestId < (quint64 {1} << 53U) - 1U) {
        ++m_nextRequestId;
    }
    setErrorString({});
    setStatusText(tr("Exporting diagnostic report…"));
    setExporting(true);
    emit writeReportRequested(m_activeRequestId, report,
                              QDir::cleanPath(destination.toLocalFile()),
                              replaceExisting);
}

void SstvDiagnosticsController::clearDiagnosticEvents()
{
    SstvDiagnosticLogBuffer::instance().clear();
    refresh();
    setStatusText(tr("Diagnostic events cleared"));
}

void SstvDiagnosticsController::requestTestTone()
{
    if (m_shutdown || !m_ready) {
        setErrorString(tr("Diagnostics are not available."));
        return;
    }
    if (!m_capabilities.value(QStringLiteral("analogTx")).toBool()) {
        const QString message = tr(
            "The SSTV test tone is unavailable in this build.");
        setErrorString(message);
        return;
    }
    setErrorString({});
    setStatusText(tr("Test tone requested"));
    emit testToneRequested();
}

void SstvDiagnosticsController::shutdown()
{
    if (m_shutdown) {
        return;
    }
    m_shutdown = true;
    m_ready = false;
    emit readyChanged();
    if (m_workerThread.isRunning()) {
        m_workerThread.quit();
        m_workerThread.wait(3000);
    }
}

void SstvDiagnosticsController::handleWriteFinished(
    quint64 requestId, bool success, const QString& errorCode)
{
    if (requestId != m_activeRequestId) {
        return;
    }
    setExporting(false);
    if (success) {
        const QString message = tr("Diagnostic report exported safely");
        setErrorString({});
        setStatusText(message);
        recordSstvDiagnosticEvent(
            sstvCoreLog(), QtInfoMsg,
            QStringLiteral("diagnostics.export-completed"),
            {{QStringLiteral("success"), true}});
        emit exportFinished(true, message);
        return;
    }
    const QString message = exportErrorText(errorCode);
    setErrorString(message);
    setStatusText(tr("Diagnostic export failed"));
    recordSstvDiagnosticEvent(
        sstvSecurityLog(), QtWarningMsg,
        QStringLiteral("diagnostics.export-failed"),
        {{QStringLiteral("success"), false},
         {QStringLiteral("reasonCode"), errorCode}});
    emit exportFinished(false, message);
}

QByteArray SstvDiagnosticsController::buildReport(QString* errorMessage) const
{
    const QVariantMap metrics {
        {QStringLiteral("rx"), m_rxMetrics},
        {QStringLiteral("tx"), m_txMetrics},
        {QStringLiteral("storage"), m_storageMetrics},
        {QStringLiteral("share"), m_shareMetrics},
        {QStringLiteral("hamdrm"), m_hamdrmMetrics},
    };
    const QVariantMap report {
        {QStringLiteral("schemaVersion"), kReportSchemaVersion},
        {QStringLiteral("kind"), QStringLiteral("decodium-sstv-diagnostics")},
        {QStringLiteral("generatedUtc"),
         QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("application"), m_applicationInfo},
        {QStringLiteral("platform"), m_platformInfo},
        {QStringLiteral("modeRegistry"), m_modeRegistryInfo},
        {QStringLiteral("capabilities"), m_capabilities},
        {QStringLiteral("settings"), m_settings},
        {QStringLiteral("metrics"), metrics},
        {QStringLiteral("calibration"), m_calibrationResults},
        {QStringLiteral("testTone"), m_testToneResults},
        {QStringLiteral("logs"), m_recentEvents},
        {QStringLiteral("privacy"), QVariantMap {
             {QStringLiteral("containsImages"), false},
             {QStringLiteral("containsAudio"), false},
             {QStringLiteral("containsLocalPaths"), false},
             {QStringLiteral("containsPersonMetadata"), false},
             {QStringLiteral("containsCredentials"), false},
         }},
    };
    const QByteArray json = QJsonDocument::fromVariant(report).toJson(
        QJsonDocument::Indented);
    if (json.isEmpty() || json.size() > kMaximumReportBytes) {
        if (errorMessage != nullptr) {
            *errorMessage = exportErrorText(
                QStringLiteral("report-too-large"));
        }
        return {};
    }
    return json;
}

void SstvDiagnosticsController::setExporting(bool value)
{
    if (m_exporting == value) {
        return;
    }
    m_exporting = value;
    emit exportingChanged();
}

void SstvDiagnosticsController::setStatusText(const QString& value)
{
    if (m_statusText == value) {
        return;
    }
    m_statusText = value;
    emit statusTextChanged();
}

void SstvDiagnosticsController::setErrorString(const QString& value)
{
    if (m_errorString == value) {
        return;
    }
    m_errorString = value;
    emit errorStringChanged();
}

} // namespace decodium::sstv

#include "SstvDiagnosticsController.moc"
