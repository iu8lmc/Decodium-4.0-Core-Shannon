// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvDiagnosticLogging.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

Q_LOGGING_CATEGORY(sstvCoreLog, "sstv.core")
Q_LOGGING_CATEGORY(sstvRxLog, "sstv.rx")
Q_LOGGING_CATEGORY(sstvTxLog, "sstv.tx")
Q_LOGGING_CATEGORY(sstvVisLog, "sstv.vis")
Q_LOGGING_CATEGORY(sstvSyncLog, "sstv.sync")
Q_LOGGING_CATEGORY(sstvStorageLog, "sstv.storage")
Q_LOGGING_CATEGORY(sstvShareLog, "sstv.share")
Q_LOGGING_CATEGORY(sstvHamDrmLog, "sstv.hamdrm")
Q_LOGGING_CATEGORY(sstvSecurityLog, "sstv.security")

namespace decodium::sstv {
namespace {

constexpr quint64 kMaximumExactJsonInteger = (quint64 {1} << 53U) - 1U;
constexpr int kMaximumScalarStringLength = 64;
constexpr int kMaximumFields = 48;

const QSet<QString>& capabilityKeys()
{
    static const QSet<QString> keys {
        QStringLiteral("analogRx"),
        QStringLiteral("analogTx"),
        QStringLiteral("wavReplay"),
        QStringLiteral("wavExport"),
        QStringLiteral("gallery"),
        QStringLiteral("remoteSharing"),
        QStringLiteral("secureCredentials"),
        QStringLiteral("hamdrm"),
        QStringLiteral("liveAudio"),
        QStringLiteral("realRadioTx"),
    };
    return keys;
}

const QSet<QString>& settingKeys()
{
    static const QSet<QString> keys {
        QStringLiteral("autoDetect"),
        QStringLiteral("autoSave"),
        QStringLiteral("retainRawAudio"),
        QStringLiteral("savePartial"),
        QStringLiteral("backgroundDetector"),
        QStringLiteral("fskIdEnabled"),
        QStringLiteral("remoteSharingEnabled"),
        QStringLiteral("hamdrmEnabled"),
        QStringLiteral("rxSampleRateHz"),
        QStringLiteral("txOutputLevelPercent"),
        QStringLiteral("maximumImageBytes"),
        QStringLiteral("maximumPixelCount"),
    };
    return keys;
}

const QHash<QString, QSet<QString>>& metricKeys()
{
    static const QHash<QString, QSet<QString>> keys {
        {QStringLiteral("rx"), {
             QStringLiteral("state"), QStringLiteral("sourceKind"),
             QStringLiteral("modeId"), QStringLiteral("generation"),
             QStringLiteral("queuedChunks"), QStringLiteral("queuedSamples"),
             QStringLiteral("droppedChunks"), QStringLiteral("droppedSamples"),
             QStringLiteral("samplesConverted"),
             QStringLiteral("samplesResampled"),
             QStringLiteral("frequencyObservations"),
             QStringLiteral("staleChunksDiscarded"),
             QStringLiteral("processingFailures"),
             QStringLiteral("producerRejectedCalls"),
             QStringLiteral("coveragePermille"),
             QStringLiteral("offsetHz"), QStringLiteral("slantPpm")}},
        {QStringLiteral("tx"), {
             QStringLiteral("state"), QStringLiteral("modeId"),
             QStringLiteral("active"), QStringLiteral("pttActive"),
             QStringLiteral("progressPermille"),
             QStringLiteral("samplesProduced"), QStringLiteral("underruns"),
             QStringLiteral("failures")}},
        {QStringLiteral("storage"), {
             QStringLiteral("ready"), QStringLiteral("recordCount"),
             QStringLiteral("imageBytes"), QStringLiteral("thumbnailBytes"),
             QStringLiteral("rawAudioBytes"), QStringLiteral("quotaBytes"),
             QStringLiteral("operations"), QStringLiteral("failures")}},
        {QStringLiteral("share"), {
             QStringLiteral("enabled"), QStringLiteral("configured"),
             QStringLiteral("uploadedBytes"),
             QStringLiteral("downloadedBytes"),
             QStringLiteral("reclaimedRows"),
             QStringLiteral("uploadBytesPerSecond"),
             QStringLiteral("downloadBytesPerSecond"),
             QStringLiteral("activeQueueDepth"),
             QStringLiteral("uploadQueueDepth"),
             QStringLiteral("downloadQueueDepth"),
             QStringLiteral("failures")}},
        {QStringLiteral("hamdrm"), {
             QStringLiteral("enabled"), QStringLiteral("available"),
             QStringLiteral("state"), QStringLiteral("activeSessions"),
             QStringLiteral("receivedBytes"),
             QStringLiteral("transmittedBytes"),
             QStringLiteral("crcFailures"), QStringLiteral("bsrRequests"),
             QStringLiteral("failures")}},
        {QStringLiteral("calibration"), {
             QStringLiteral("available"), QStringLiteral("completed"),
             QStringLiteral("success"), QStringLiteral("frequencyHz"),
             QStringLiteral("levelMilli"), QStringLiteral("durationMs"),
             QStringLiteral("errorCode")}},
        {QStringLiteral("testTone"), {
             QStringLiteral("available"), QStringLiteral("running"),
             QStringLiteral("success"), QStringLiteral("frequencyHz"),
             QStringLiteral("levelMilli"), QStringLiteral("durationMs"),
             QStringLiteral("errorCode")}},
    };
    return keys;
}

const QSet<QString>& eventFieldKeys()
{
    static const QSet<QString> keys {
        QStringLiteral("active"), QStringLiteral("attempt"),
        QStringLiteral("bytes"), QStringLiteral("capability"),
        QStringLiteral("component"), QStringLiteral("count"),
        QStringLiteral("coveragePermille"), QStringLiteral("durationMs"),
        QStringLiteral("errorCode"), QStringLiteral("eventId"),
        QStringLiteral("failures"), QStringLiteral("modeId"),
        QStringLiteral("offsetHz"), QStringLiteral("operation"),
        QStringLiteral("previousState"), QStringLiteral("queueDepth"),
        QStringLiteral("reasonCode"), QStringLiteral("requestId"),
        QStringLiteral("retryable"), QStringLiteral("schemaVersion"),
        QStringLiteral("slantPpm"), QStringLiteral("state"),
        QStringLiteral("success"), QStringLiteral("visCode"),
    };
    return keys;
}

bool safeScalarString(const QString& value)
{
    if (value.size() > kMaximumScalarStringLength
        || SstvDiagnosticRedactor::containsForbiddenText(value)) {
        return false;
    }
    static const QRegularExpression safe(
        QStringLiteral("^[A-Za-z0-9_.:+ -]{0,64}$"),
        QRegularExpression::UseUnicodePropertiesOption);
    return safe.match(value).hasMatch();
}

QVariant safeScalar(const QVariant& value)
{
    const int type = value.metaType().id();
    if (type == QMetaType::Bool) {
        return value.toBool();
    }
    if (type == QMetaType::Int || type == QMetaType::LongLong) {
        const qint64 number = value.toLongLong();
        if (number < -static_cast<qint64>(kMaximumExactJsonInteger)
            || number > static_cast<qint64>(kMaximumExactJsonInteger)) {
            return {};
        }
        return number;
    }
    if (type == QMetaType::UInt || type == QMetaType::ULongLong) {
        const quint64 number = value.toULongLong();
        return number <= kMaximumExactJsonInteger
            ? QVariant::fromValue(number) : QVariant {};
    }
    if (type == QMetaType::Double || type == QMetaType::Float) {
        const double number = value.toDouble();
        return std::isfinite(number) && std::abs(number) <= 1.0e15
            ? QVariant(number) : QVariant {};
    }
    if (type == QMetaType::QString && safeScalarString(value.toString())) {
        return value.toString();
    }
    return {};
}

QVariantMap filterMap(const QVariantMap& input,
                      const QSet<QString>& allowed)
{
    QVariantMap output;
    for (auto it = input.cbegin(); it != input.cend()
         && output.size() < kMaximumFields; ++it) {
        if (!allowed.contains(it.key())) {
            continue;
        }
        const QVariant scalar = safeScalar(it.value());
        if (scalar.isValid()) {
            output.insert(it.key(), scalar);
        }
    }
    return output;
}

QString severityName(QtMsgType severity)
{
    switch (severity) {
    case QtDebugMsg: return QStringLiteral("debug");
    case QtInfoMsg: return QStringLiteral("info");
    case QtWarningMsg: return QStringLiteral("warning");
    case QtCriticalMsg: return QStringLiteral("critical");
    case QtFatalMsg: return QStringLiteral("fatal");
    }
    return QStringLiteral("warning");
}

bool allowedCategory(const QString& value)
{
    static const QSet<QString> categories {
        QStringLiteral("sstv.core"), QStringLiteral("sstv.rx"),
        QStringLiteral("sstv.tx"), QStringLiteral("sstv.vis"),
        QStringLiteral("sstv.sync"), QStringLiteral("sstv.storage"),
        QStringLiteral("sstv.share"), QStringLiteral("sstv.hamdrm"),
        QStringLiteral("sstv.security"),
    };
    return categories.contains(value);
}

} // namespace

QVariantMap SstvDiagnosticRedactor::capabilities(const QVariantMap& input)
{
    QVariantMap output;
    for (auto it = input.cbegin(); it != input.cend(); ++it) {
        if (capabilityKeys().contains(it.key())
            && it.value().metaType().id() == QMetaType::Bool) {
            output.insert(it.key(), it.value().toBool());
        }
    }
    return output;
}

QVariantMap SstvDiagnosticRedactor::settings(const QVariantMap& input)
{
    return filterMap(input, settingKeys());
}

QVariantMap SstvDiagnosticRedactor::metrics(const QString& section,
                                            const QVariantMap& input)
{
    const auto found = metricKeys().constFind(section);
    return found == metricKeys().cend() ? QVariantMap {}
                                        : filterMap(input, *found);
}

QVariantMap SstvDiagnosticRedactor::eventFields(const QVariantMap& input)
{
    return filterMap(input, eventFieldKeys());
}

bool SstvDiagnosticRedactor::safeEventName(const QString& value) noexcept
{
    if (value.isEmpty() || value.size() > 64
        || containsForbiddenText(value)) {
        return false;
    }
    static const QRegularExpression safe(
        QStringLiteral("^[a-z][a-z0-9_.-]{0,63}$"));
    return safe.match(value).hasMatch();
}

bool SstvDiagnosticRedactor::containsForbiddenText(
    const QString& value) noexcept
{
    const QString folded = value.toCaseFolded();
    static const QStringList forbidden {
        QStringLiteral("authorization"), QStringLiteral("bearer "),
        QStringLiteral("password"), QStringLiteral("passwd"),
        QStringLiteral("access_token"), QStringLiteral("refresh_token"),
        QStringLiteral("private key"), QStringLiteral("private_key"),
        QStringLiteral("signedurl"), QStringLiteral("signed_url"),
        QStringLiteral("x-amz-signature"), QStringLiteral("envelope"),
        QStringLiteral("file://"), QStringLiteral("http://"),
        QStringLiteral("https://"), QStringLiteral("/users/"),
        QStringLiteral("\\users\\"), QStringLiteral("/home/"),
    };
    for (const QString& marker : forbidden) {
        if (folded.contains(marker)) {
            return true;
        }
    }
    return false;
}

QVariantMap SstvDiagnosticEvent::toVariantMap() const
{
    return {
        {QStringLiteral("sequence"), QVariant::fromValue(sequence)},
        {QStringLiteral("timestampUtc"), timestampUtc},
        {QStringLiteral("category"), category},
        {QStringLiteral("severity"), severity},
        {QStringLiteral("event"), event},
        {QStringLiteral("fields"), fields},
    };
}

SstvDiagnosticLogBuffer& SstvDiagnosticLogBuffer::instance()
{
    static SstvDiagnosticLogBuffer buffer;
    return buffer;
}

void SstvDiagnosticLogBuffer::record(const QLoggingCategory& category,
                                     QtMsgType severity,
                                     const QString& event,
                                     const QVariantMap& fields)
{
    const QString categoryName = QString::fromLatin1(category.categoryName());
    if (!allowedCategory(categoryName)
        || !SstvDiagnosticRedactor::safeEventName(event)) {
        return;
    }
    SstvDiagnosticEvent diagnostic;
    diagnostic.timestampUtc = QDateTime::currentDateTimeUtc();
    diagnostic.category = categoryName;
    diagnostic.severity = severityName(severity);
    diagnostic.event = event;
    diagnostic.fields = SstvDiagnosticRedactor::eventFields(fields);
    {
        const QMutexLocker lock(&m_mutex);
        diagnostic.sequence = m_nextSequence;
        if (m_nextSequence < kMaximumExactJsonInteger) {
            ++m_nextSequence;
        }
        if (m_events.size() == kMaximumEvents) {
            m_events.removeFirst();
        }
        m_events.push_back(diagnostic);
    }

    const QByteArray compactFields = QJsonDocument::fromVariant(
        diagnostic.fields).toJson(QJsonDocument::Compact);
    QMessageLogger(__FILE__, __LINE__, Q_FUNC_INFO,
                   category.categoryName()).info().noquote()
        << diagnostic.event << QString::fromUtf8(compactFields);
}

QVariantList SstvDiagnosticLogBuffer::snapshot(int maximumEvents) const
{
    const int boundedMaximum = std::clamp(
        maximumEvents, 0, kMaximumExportedEvents);
    QVariantList output;
    const QMutexLocker lock(&m_mutex);
    const qsizetype first = std::max(
        qsizetype {0}, m_events.size() - static_cast<qsizetype>(boundedMaximum));
    output.reserve(m_events.size() - first);
    for (qsizetype index = first; index < m_events.size(); ++index) {
        output.push_back(m_events.at(index).toVariantMap());
    }
    return output;
}

int SstvDiagnosticLogBuffer::size() const
{
    const QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_events.size());
}

void SstvDiagnosticLogBuffer::clear()
{
    const QMutexLocker lock(&m_mutex);
    m_events.clear();
}

void recordSstvDiagnosticEvent(const QLoggingCategory& category,
                               QtMsgType severity,
                               const QString& event,
                               const QVariantMap& fields)
{
    SstvDiagnosticLogBuffer::instance().record(
        category, severity, event, fields);
}

} // namespace decodium::sstv
