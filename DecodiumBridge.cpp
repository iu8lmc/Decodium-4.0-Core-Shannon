#include "DecodiumBridge.h"
#include "DecodiumAlertManager.h"
#include "DecodiumPropagationManager.h"
#include "DecodiumDxCluster.h"
#include "DxccLookup.h"
#include "DecodiumLegacyBackend.h"
#include "Network/DecodiumPskReporterLite.h"
#include "Network/DecodiumCloudlogLite.h"
#include "Network/DecodiumWsprUploader.h"
#include "controllers/ActiveStationsModel.hpp"
#include "helper_functions.h"
#include "Detector/FT8DecodeWorker.hpp"
#include "Detector/FT2DecodeWorker.hpp"
#include "Detector/FT4DecodeWorker.hpp"
#include "Detector/Q65DecodeWorker.hpp"
#include "Detector/MSK144DecodeWorker.hpp"
#include "Detector/WSPRDecodeWorker.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"
#include "Detector/FST4DecodeWorker.hpp"
#include "Audio/soundin.h"
#include "Audio/soundout.h"
#include "Modulator/Modulator.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"
#include "Decoder/decodedtext.h"
#include "DecodiumAudioSink.h"
#include "Radio.hpp"

#include <QGuiApplication>
#include <QApplication>
#include <QDateTime>
#include <QSet>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QDir>
#include <QTimeZone>
#include "Network/FoxVerifier.hpp"
#include "wsjtx_config.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QMediaDevices>
#include <QRandomGenerator>
#include <QDebug>
#include <QRegularExpression>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#ifdef FFTW3_SINGLE_FOUND
#include <fftw3.h>
#endif
#include <vector>

static QString bridgeLogPath()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("C:\\Users\\IU8LMC\\bridge_log.txt");
#else
    return QStringLiteral("/tmp/decodium_bridge.log");
#endif
}

static void bridgeLog(const char* msg) {
    QByteArray path = bridgeLogPath().toLocal8Bit();
    FILE* f = fopen(path.constData(), "a");
    if (f) { fputs(msg, f); fputc('\n', f); fclose(f); }
}
static void bridgeLog(const QString& s) { bridgeLog(s.toLocal8Bit().constData()); }
static QAudioDevice findOutputDevice(const QString& name, bool* requestedDeviceFound = nullptr);

static QString stripLegacyDecodeAppendage(QString line)
{
    line = line.trimmed();
    if (line.startsWith(QStringLiteral("TU; "))) {
        line = line.mid(4).trimmed();
    }

    int const nbPos = line.indexOf(QChar::Nbsp);
    if (nbPos >= 0) {
        return line.left(nbPos).trimmed();
    }

    static const QRegularExpression decodeHeaderPattern {
        R"(^(\d{4}|\d{6})\s+[-+]?\d+\s+[-+]?\d+(?:\.\d+)?\s+\d+\s+\S\s+)"
    };
    auto const match = decodeHeaderPattern.match(line);
    if (match.hasMatch()) {
        int const payloadStart = match.capturedEnd();
        QString payload = line.mid(payloadStart);
        int const annotationPos = payload.indexOf(QRegularExpression {R"(\s{2,})"});
        if (annotationPos >= 0) {
            payload = payload.left(annotationPos);
        }
        return (line.left(payloadStart) + payload.trimmed()).trimmed();
    }

    return line;
}

static QString normalizeUtcDisplayToken(QString raw)
{
    raw.remove(QRegularExpression {R"([^0-9])"});
    QTime const nowUtc = QDateTime::currentDateTimeUtc().time();
    if (raw.size() >= 6) {
        return raw.left(6);
    }
    if (raw.size() == 4) {
        return raw + QStringLiteral("%1").arg(nowUtc.second(), 2, 10, QLatin1Char('0'));
    }
    if (raw.isEmpty()) {
        return nowUtc.toString(QStringLiteral("HHmmss"));
    }
    return raw;
}

struct ParsedAdifRecord
{
    QMap<QString, QString> fields;
};

struct ParsedAdifDocument
{
    QString header;
    QList<ParsedAdifRecord> records;
    bool loaded {false};
};

static QString normalizeAdifModeForDisplay(QMap<QString, QString> const& fields)
{
    QString const submode = fields.value(QStringLiteral("SUBMODE")).trimmed().toUpper();
    if (!submode.isEmpty()) {
        return submode;
    }
    return fields.value(QStringLiteral("MODE")).trimmed().toUpper();
}

static QString normalizedAdifDateTime(QMap<QString, QString> const& fields)
{
    QString const date = fields.value(QStringLiteral("QSO_DATE")).trimmed();
    QString time = fields.value(QStringLiteral("TIME_ON")).trimmed();
    if (time.size() == 4) {
        time += QStringLiteral("00");
    }
    if (date.size() != 8) {
        return {};
    }
    time = time.leftJustified(6, QLatin1Char('0')).left(6);
    QDateTime utc = QDateTime::fromString(date + time, QStringLiteral("yyyyMMddHHmmss"));
    utc.setTimeSpec(Qt::UTC);
    return utc.isValid() ? utc.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString {};
}

static QVariantMap qsoMapFromAdifRecord(QMap<QString, QString> const& fields, QString const& myGrid)
{
    QVariantMap map;
    QString const call = fields.value(QStringLiteral("CALL")).trimmed().toUpper();
    QString const grid = fields.value(QStringLiteral("GRIDSQUARE")).trimmed().toUpper();
    QString const band = fields.value(QStringLiteral("BAND")).trimmed().toUpper();
    QString const mode = normalizeAdifModeForDisplay(fields);
    QString const dateTime = normalizedAdifDateTime(fields);
    QString const reportSent = fields.value(QStringLiteral("RST_SENT")).trimmed().toUpper();
    QString const reportReceived = fields.value(QStringLiteral("RST_RCVD")).trimmed().toUpper();
    QString const comment = fields.value(QStringLiteral("COMMENT")).trimmed();

    map.insert(QStringLiteral("call"), call);
    map.insert(QStringLiteral("grid"), grid);
    map.insert(QStringLiteral("band"), band);
    map.insert(QStringLiteral("mode"), mode);
    map.insert(QStringLiteral("dateTime"), dateTime);
    map.insert(QStringLiteral("reportSent"), reportSent);
    map.insert(QStringLiteral("reportReceived"), reportReceived);
    map.insert(QStringLiteral("comment"), comment);

    if (!myGrid.trimmed().isEmpty() && !grid.isEmpty()) {
        GeoDistanceInfo const geo = geo_distance(myGrid.trimmed().toUpper(), grid, 0.0);
        if (geo.km > 0) {
            map.insert(QStringLiteral("distanceKm"), geo.km);
        }
    }

    return map;
}

static ParsedAdifDocument loadAdifDocument(QString const& path)
{
    ParsedAdifDocument doc;
    QFile file(path);
    if (!file.exists()) {
        doc.header = QStringLiteral("Decodium4 ADIF Log\n<EOH>\n");
        return doc;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return doc;
    }

    QString text = QString::fromUtf8(file.readAll());
    file.close();

    QRegularExpression const eohRe(QStringLiteral("<EOH>"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch const eohMatch = eohRe.match(text);
    QString body = text;
    if (eohMatch.hasMatch()) {
        doc.header = text.left(eohMatch.capturedEnd()).trimmed() + QLatin1Char('\n');
        body = text.mid(eohMatch.capturedEnd());
    } else {
        doc.header = QStringLiteral("Decodium4 ADIF Log\n<EOH>\n");
    }

    QRegularExpression const eorRe(QStringLiteral("<EOR>"), QRegularExpression::CaseInsensitiveOption);
    int pos = 0;
    while (true) {
        QRegularExpressionMatch const eorMatch = eorRe.match(body, pos);
        if (!eorMatch.hasMatch()) {
            break;
        }

        QString const recordText = body.mid(pos, eorMatch.capturedStart() - pos).trimmed();
        pos = eorMatch.capturedEnd();
        if (recordText.isEmpty()) {
            continue;
        }

        ParsedAdifRecord record;
        int cursor = 0;
        while (cursor < recordText.size()) {
            int const tagStart = recordText.indexOf(QLatin1Char('<'), cursor);
            if (tagStart < 0) {
                break;
            }
            int const tagEnd = recordText.indexOf(QLatin1Char('>'), tagStart + 1);
            if (tagEnd < 0) {
                break;
            }

            QString const spec = recordText.mid(tagStart + 1, tagEnd - tagStart - 1).trimmed();
            QStringList const parts = spec.split(QLatin1Char(':'));
            if (parts.size() < 2) {
                cursor = tagEnd + 1;
                continue;
            }

            bool ok = false;
            int const valueLength = parts.at(1).toInt(&ok);
            if (!ok || valueLength < 0) {
                cursor = tagEnd + 1;
                continue;
            }

            QString const fieldName = parts.at(0).trimmed().toUpper();
            QString const value = recordText.mid(tagEnd + 1, valueLength).trimmed();
            if (!fieldName.isEmpty()) {
                record.fields.insert(fieldName, value);
            }
            cursor = tagEnd + 1 + valueLength;
        }

        if (!record.fields.isEmpty()) {
            doc.records.append(record);
        }
    }

    doc.loaded = true;
    return doc;
}

static bool writeAdifDocument(QString const& path, ParsedAdifDocument const& doc)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream ts(&file);
    ts << (doc.header.isEmpty() ? QStringLiteral("Decodium4 ADIF Log\n<EOH>\n") : doc.header);
    if (!doc.header.endsWith(QLatin1Char('\n'))) {
        ts << '\n';
    }

    auto writeField = [&ts](QString const& tag, QString const& value) {
        if (!tag.isEmpty() && !value.isNull() && !value.isEmpty()) {
            ts << '<' << tag << ':' << value.size() << '>' << value << ' ';
        }
    };

    for (ParsedAdifRecord const& record : doc.records) {
        for (auto it = record.fields.cbegin(); it != record.fields.cend(); ++it) {
            writeField(it.key(), it.value());
        }
        ts << "<EOR>\n";
    }
    return true;
}

static void rebuildWorkedCallsFromDocument(QSet<QString>& workedCalls, QList<ParsedAdifRecord> const& records)
{
    workedCalls.clear();
    for (ParsedAdifRecord const& record : records) {
        QString const call = record.fields.value(QStringLiteral("CALL")).trimmed().toUpper();
        if (!call.isEmpty()) {
            workedCalls.insert(call);
        }
    }
}

static void setAdifModeFields(QMap<QString, QString>& fields, QString const& modeValue)
{
    QString const normalized = modeValue.trimmed().toUpper();
    if (normalized.isEmpty()) {
        return;
    }

    if (normalized == QStringLiteral("FT2")
        || normalized == QStringLiteral("FT4")
        || normalized == QStringLiteral("FST4")
        || normalized == QStringLiteral("Q65")) {
        fields.insert(QStringLiteral("MODE"), QStringLiteral("MFSK"));
        fields.insert(QStringLiteral("SUBMODE"), normalized);
        return;
    }

    fields.insert(QStringLiteral("MODE"), normalized);
    fields.remove(QStringLiteral("SUBMODE"));
}

static bool isGridTokenStrict(QString const& token)
{
    static const QRegularExpression gridPattern {
        R"(\A(?![Rr]{2}73)[A-Ra-r]{2}[0-9]{2}(?:[A-Xa-x]{2})?\z)"
    };
    return gridPattern.match(token.trimmed()).hasMatch();
}

static int decodeSnrOrDefault(QString const& text, int fallback = -10)
{
    bool ok = false;
    int const parsed = text.trimmed().toInt(&ok);
    return ok ? qBound(-50, parsed, 49) : fallback;
}

static QString sampleFormatName(QAudioFormat::SampleFormat fmt)
{
    switch (fmt) {
    case QAudioFormat::UInt8: return "UInt8";
    case QAudioFormat::Int16: return "Int16";
    case QAudioFormat::Int32: return "Int32";
    case QAudioFormat::Float: return "Float";
    case QAudioFormat::Unknown:
    default:
        return "Unknown";
    }
}

static QString legacyPaletteNameForUiIndex(int index)
{
    switch (index) {
    case 1: return QStringLiteral("Raptor");
    case 2: return QStringLiteral("Gray1");
    case 3: return QStringLiteral("Blue1");
    case 4: return QStringLiteral("Sunburst");
    case 5: return QStringLiteral("Linrad");
    case 0:
    default:
        return QStringLiteral("Default");
    }
}

static int uiPaletteIndexForLegacyName(QString const& palette)
{
    QString const normalized = palette.trimmed().toCaseFolded();
    if (normalized == QStringLiteral("raptor")) return 1;
    if (normalized.startsWith(QStringLiteral("gray"))) return 2;
    if (normalized == QStringLiteral("blue1") || normalized == QStringLiteral("blue2")
        || normalized == QStringLiteral("blue3") || normalized == QStringLiteral("default")) {
        return normalized == QStringLiteral("default") ? 0 : 3;
    }
    if (normalized == QStringLiteral("sunburst") || normalized == QStringLiteral("banana")
        || normalized == QStringLiteral("orange")) return 4;
    if (normalized == QStringLiteral("linrad") || normalized == QStringLiteral("scope")) return 5;
    return 0;
}

namespace
{
constexpr int kRecentDuplicateLogWindowSeconds {90};
constexpr int kLateAutoLogGraceWindowSeconds {45};

bool isWithinRecentDuplicateWindow(QDateTime const& seenAt, QDateTime const& nowUtc)
{
    qint64 const deltaSec = seenAt.secsTo(nowUtc);
    return deltaSec >= 0 && deltaSec <= kRecentDuplicateLogWindowSeconds;
}

void pruneRecentDuplicateCache(QHash<QString, QDateTime>& cache, QDateTime const& nowUtc)
{
    if (cache.size() <= 512) {
        return;
    }

    for (auto it = cache.begin(); it != cache.end(); ) {
        if (it.value().secsTo(nowUtc) > 3 * kRecentDuplicateLogWindowSeconds) {
            it = cache.erase(it);
        } else {
            ++it;
        }
    }
}
}

static QString audioFormatToString(const QAudioFormat& fmt)
{
    return QString("rate=%1 ch=%2 fmt=%3 bytes=%4")
        .arg(fmt.sampleRate())
        .arg(fmt.channelCount())
        .arg(sampleFormatName(fmt.sampleFormat()))
        .arg(fmt.bytesPerFrame());
}

static QAudioFormat makeAudioFormat(int sampleRate, int channelCount, QAudioFormat::SampleFormat sampleFormat)
{
    QAudioFormat fmt;
    fmt.setSampleRate(sampleRate);
    fmt.setChannelCount(channelCount);
    fmt.setSampleFormat(sampleFormat);
    return fmt;
}

static bool sameAudioFormat(const QAudioFormat& a, const QAudioFormat& b)
{
    return a.sampleRate() == b.sampleRate()
        && a.channelCount() == b.channelCount()
        && a.sampleFormat() == b.sampleFormat();
}

static QAudioFormat chooseTxAudioFormat(const QAudioDevice& device)
{
    const QAudioFormat requested = makeAudioFormat(48000, 1, QAudioFormat::Int16);
    const QAudioFormat preferred = device.preferredFormat();

    bridgeLog("TX audio requested: " + audioFormatToString(requested));
    bridgeLog("TX audio preferred: " + audioFormatToString(preferred) +
              " dev=" + device.description());

    QList<QAudioFormat> candidates;
#if defined(Q_OS_MAC)
    QAudioFormat stereoPreferred48k = makeAudioFormat(48000,
                                                      qMax(2, preferred.channelCount()),
                                                      QAudioFormat::Int16);
    if (preferred.channelCount() > 1)
        candidates.append(stereoPreferred48k);
#endif

    candidates.append(requested);

    if (preferred.channelCount() > 0) {
        candidates.append(makeAudioFormat(48000, preferred.channelCount(), QAudioFormat::Int16));
    }
    if (preferred.sampleFormat() != QAudioFormat::Unknown) {
        candidates.append(makeAudioFormat(48000, 1, preferred.sampleFormat()));
        if (preferred.channelCount() > 0) {
            candidates.append(makeAudioFormat(48000, preferred.channelCount(), preferred.sampleFormat()));
        }
    }
    if (preferred.sampleRate() > 0) {
        candidates.append(makeAudioFormat(preferred.sampleRate(), 1, QAudioFormat::Int16));
        if (preferred.channelCount() > 0) {
            candidates.append(makeAudioFormat(preferred.sampleRate(), preferred.channelCount(), QAudioFormat::Int16));
        }
        if (preferred.sampleFormat() != QAudioFormat::Unknown) {
            candidates.append(makeAudioFormat(preferred.sampleRate(), 1, preferred.sampleFormat()));
            if (preferred.channelCount() > 0) {
                candidates.append(makeAudioFormat(preferred.sampleRate(), preferred.channelCount(), preferred.sampleFormat()));
            }
        }
    }
    if (preferred.isValid()) {
        candidates.append(preferred);
    }

    QList<QAudioFormat> uniqueCandidates;
    for (const QAudioFormat& candidate : candidates) {
        if (!candidate.isValid() || candidate.sampleRate() <= 0 || candidate.channelCount() <= 0
            || candidate.sampleFormat() == QAudioFormat::Unknown) {
            continue;
        }
        bool duplicate = false;
        for (const QAudioFormat& existing : uniqueCandidates) {
            if (sameAudioFormat(candidate, existing)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) uniqueCandidates.append(candidate);
    }

    for (const QAudioFormat& candidate : uniqueCandidates) {
        if (device.isFormatSupported(candidate)) {
            bridgeLog("TX audio chosen: " + audioFormatToString(candidate) +
                      " dev=" + device.description());
            return candidate;
        }
    }

    bridgeLog("TX audio fallback to preferred format: " + audioFormatToString(preferred) +
              " dev=" + device.description());
    return preferred.isValid() ? preferred : requested;
}

static qreal txAttenuationFromSlider(double outputLevel)
{
    return qBound<qreal>(0.0, static_cast<qreal>(outputLevel / 10.0), 45.0);
}

static qreal txGainFromSlider(double outputLevel)
{
    return static_cast<qreal>(std::pow(10.0, -txAttenuationFromSlider(outputLevel) / 20.0));
}

static QByteArray buildTxPcmBuffer(const QVector<float>& wave48kMono, const QAudioFormat& format)
{
    if (wave48kMono.isEmpty() || !format.isValid()) return {};

    const int dstRate = qMax(1, format.sampleRate());
    const int dstChannels = qMax(1, format.channelCount());
    const int bytesPerSample = qMax(0, format.bytesPerSample());
    if (bytesPerSample <= 0 || format.sampleFormat() == QAudioFormat::Unknown) return {};

    const qint64 dstFrames = qMax<qint64>(1, qRound64((double)wave48kMono.size() * dstRate / 48000.0));
    const qint64 totalBytes = dstFrames * dstChannels * bytesPerSample;
    if (totalBytes <= 0 || totalBytes > std::numeric_limits<int>::max()) return {};

    QByteArray pcm(static_cast<int>(totalBytes), Qt::Uninitialized);
    char* out = pcm.data();
    const int srcCount = wave48kMono.size();

    for (qint64 frame = 0; frame < dstFrames; ++frame) {
        const double srcPos = (static_cast<double>(frame) * 48000.0) / dstRate;
        const int src0 = qBound(0, static_cast<int>(std::floor(srcPos)), srcCount - 1);
        const int src1 = qMin(src0 + 1, srcCount - 1);
        const float frac = static_cast<float>(srcPos - src0);
        float sample = wave48kMono[src0];
        if (src1 != src0) sample += (wave48kMono[src1] - wave48kMono[src0]) * frac;
        sample = qBound(-1.0f, sample, 1.0f);

        for (int ch = 0; ch < dstChannels; ++ch) {
            switch (format.sampleFormat()) {
            case QAudioFormat::UInt8: {
                const quint8 v = static_cast<quint8>(qRound((sample * 0.5f + 0.5f) * 255.0f));
                std::memcpy(out, &v, sizeof(v));
                out += sizeof(v);
                break;
            }
            case QAudioFormat::Int16: {
                const qint16 v = static_cast<qint16>(qRound(sample * 32767.0f));
                std::memcpy(out, &v, sizeof(v));
                out += sizeof(v);
                break;
            }
            case QAudioFormat::Int32: {
                const qint32 v = static_cast<qint32>(qRound64(sample * 2147483647.0f));
                std::memcpy(out, &v, sizeof(v));
                out += sizeof(v);
                break;
            }
            case QAudioFormat::Float: {
                const float v = sample;
                std::memcpy(out, &v, sizeof(v));
                out += sizeof(v);
                break;
            }
            case QAudioFormat::Unknown:
            default:
                return {};
            }
        }
    }

    return pcm;
}

#if defined(Q_OS_MAC)
static AudioDevice::Channel txOutputChannelForFormat(const QAudioFormat& format)
{
    return format.channelCount() > 1 ? AudioDevice::Both : AudioDevice::Mono;
}
#endif

// ── helpers PTT / freq che delegano al backend attivo ────────────────────────
static inline bool activeCatCanPtt(DecodiumCatManager* n, DecodiumTransceiverManager* h, const QString& b,
                                   DecodiumOmniRigManager* o = nullptr)
{ if (b=="native") return n->canPtt(); if (b=="omnirig" && o) return o->canPtt(); return h->canPtt(); }

static inline void activeCatSetPtt(DecodiumCatManager* n, DecodiumTransceiverManager* h, const QString& b, bool on,
                                   DecodiumOmniRigManager* o = nullptr)
{ if (b=="native") n->setRigPtt(on); else if (b=="omnirig" && o) o->setRigPtt(on); else h->setRigPtt(on); }

static inline void activeCatSetFreq(DecodiumCatManager* n, DecodiumTransceiverManager* h, const QString& b, double hz,
                                    DecodiumOmniRigManager* o = nullptr)
{ if (b=="native") n->setRigFrequency(hz); else if (b=="omnirig" && o) o->setRigFrequency(hz); else h->setRigFrequency(hz); }

DecodiumBridge::DecodiumBridge(QObject* parent)
    : QObject(parent)
{
    m_themeManager    = new DecodiumThemeManager(this);
    m_propagationManager = new DecodiumPropagationManager(this);
    m_wavManager      = new WavManager(this);
    m_macroManager    = new MacroManager(this);
    m_bandManager     = new BandManager(this);
    m_nativeCat       = new DecodiumCatManager(this);
    m_omniRigCat      = new DecodiumOmniRigManager(this);
    m_hamlibCat       = new DecodiumTransceiverManager(this);
    m_dxCluster       = new DecodiumDxCluster(this);
    m_activeStations  = new ActiveStationsModel(this);
    m_alertManager    = new DecodiumAlertManager(this);

#if defined(Q_OS_MAC)
    m_useLegacyTxBackend = true;
#endif
    if (m_useLegacyTxBackend) {
        m_legacyBackend = new DecodiumLegacyBackend(this);
        if (!m_legacyBackend->available()) {
            bridgeLog("Legacy backend unavailable: " + m_legacyBackend->failureReason());
            m_useLegacyTxBackend = false;
        } else {
            bridgeLog("Legacy backend enabled for macOS TX/Tune using ft2 profile");
            connect(m_legacyBackend, &DecodiumLegacyBackend::waterfallRowReady,
                    this, &DecodiumBridge::onLegacyWaterfallRow);
            connect(m_legacyBackend, &DecodiumLegacyBackend::warningRaised,
                    this, &DecodiumBridge::warningRaised);
            connect(m_legacyBackend, &DecodiumLegacyBackend::rigErrorRaised,
                    this, &DecodiumBridge::rigErrorRaised);
            connect(m_legacyBackend, &DecodiumLegacyBackend::preferencesRequested,
                    this, [this]() {
                        emit setupSettingsRequested(0);
                    });
        }
    }

    // PSK Reporter
    m_pskReporter = new DecodiumPskReporterLite(this);
    connect(m_pskReporter, &DecodiumPskReporterLite::connectedChanged,
            this, [this]() { emit pskReporterConnectedChanged(); });
    connect(m_pskReporter, &DecodiumPskReporterLite::errorOccurred,
            this, [](const QString& msg) { bridgeLog("PSK Reporter: " + msg); });

    // Cloudlog
    m_cloudlog = new DecodiumCloudlogLite(this);
    connect(m_cloudlog, &DecodiumCloudlogLite::qsoLogged,
            this, [this](const QString& c) { emit statusMessage("Cloudlog: QSO loggato " + c); });
    connect(m_cloudlog, &DecodiumCloudlogLite::errorOccurred,
            this, [this](const QString& msg) { emit errorMessage("Cloudlog: " + msg); });

    // WSPR uploader
    m_wsprUploader = new DecodiumWsprUploader(this);

    // DXCC lookup (cty.dat)
    m_dxccLookup = new DxccLookup();
    {
        QString loadedPath;
        if (reloadDxccLookup(&loadedPath)) {
            bridgeLog("DXCC: caricato cty.dat da " + loadedPath +
                      " (" + QString::number(m_dxccLookup->entityCount()) + " entità)");
            refreshDecodeListDxcc();
        } else {
            bridgeLog("DXCC: cty.dat non trovato, DXCC disabilitato finché il file non viene installato o scaricato.");
        }
    }
    connect(m_wsprUploader, &DecodiumWsprUploader::uploadStatus,
            this, [this](const QString& msg) { emit statusMessage("WSPR: " + msg); });

    // Start drift reference clock
    m_driftClock.start();

    // Helper: legge una property QObject* in modo generico
    auto catProp = [this](const char* prop) -> QVariant {
        auto* obj = catManagerObj();
        return obj ? obj->property(prop) : QVariant{};
    };

    // BandManager → aggiorna frequenza sul radio quando si cambia banda
    connect(m_bandManager, &BandManager::bandFrequencyRequested, this, [this](double freq) {
        setFrequency(freq);
        if (m_catConnected)
            activeCatSetFreq(m_nativeCat, m_hamlibCat, m_catBackend, freq, m_omniRigCat);
        if (m_monitoring) {
            m_audioBuffer.clear();
            m_periodTicks = 0;
        }
    });

    // Helper: collega i segnali di un manager alle property del bridge, con guard backend
    auto connectCatSignals = [this, catProp](auto* mgr, const QString& backend) {
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::frequencyChanged, this, [this, backend, catProp]() {
            if (m_catBackend != backend) return;
            double f = catProp("frequency").toDouble();
            if (f > 0 && m_frequency != f) { m_frequency = f; emit frequencyChanged(); }
            m_bandManager->updateFromFrequency(f);
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::modeChanged, this, [this, backend, catProp]() {
            if (m_catBackend != backend) return;
            QString m = catProp("mode").toString();
            if (m_catMode != m) { m_catMode = m; emit catModeChanged(); }
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::connectedChanged, this, [this, backend, catProp]() {
            if (m_catBackend != backend) return;
            bool c = catProp("connected").toBool();
            bridgeLog("CAT[" + backend + "] connectedChanged: " + QString::number(c));
            if (m_catConnected != c) {
                m_catConnected = c;
                m_catRigName = c ? catProp("rigName").toString() : QString();
                emit catConnectedChanged();
                emit catRigNameChanged();
                bool autoStart = catProp("audioAutoStart").toBool();
                if (c && autoStart && !m_monitoring)
                    QTimer::singleShot(300, this, [this]() { setMonitoring(true); });
                if (!c && (m_transmitting || m_tuning))
                    QTimer::singleShot(0, this, [this]() { halt(); });
            }
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::errorOccurred, this, [this, backend](const QString& msg) {
            if (m_catBackend != backend) return;
            bridgeLog("CAT[" + backend + "] error: " + msg);
            emit errorMessage(msg);
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::statusUpdate, this, [this, backend](const QString& msg) {
            if (m_catBackend != backend) return;
            bridgeLog("CAT[" + backend + "] status: " + msg);
            emit statusMessage(msg);
        });
    };
    connectCatSignals(m_nativeCat, "native");
    connectCatSignals(m_omniRigCat, "omnirig");
    connectCatSignals(m_hamlibCat, "hamlib");

    // Worker thread for FT8 decoder
    m_workerThread = new QThread(this);
    m_ft8Worker = new decodium::ft8::FT8DecodeWorker();
    m_ft8Worker->moveToThread(m_workerThread);
    connect(m_ft8Worker, &decodium::ft8::FT8DecodeWorker::decodeReady,
            this, &DecodiumBridge::onFt8DecodeReady);
    connect(m_workerThread, &QThread::finished, m_ft8Worker, &QObject::deleteLater);
    m_workerThread->start();

    // Worker thread for FT2 decoder — stack 8MB (necessario per stage7 C++)
    m_workerThreadFt2 = new QThread(this);
    m_workerThreadFt2->setStackSize(8 * 1024 * 1024);
    m_ft2Worker = new decodium::ft2::FT2DecodeWorker();
    m_ft2Worker->moveToThread(m_workerThreadFt2);
    // path sincrono (fine periodo)
    connect(m_ft2Worker, &decodium::ft2::FT2DecodeWorker::decodeReady,
            this, &DecodiumBridge::onFt2DecodeReady);
    // path async turbo (ogni 100ms, senza serial check)
    connect(m_ft2Worker, &decodium::ft2::FT2DecodeWorker::asyncDecodeReady,
            this, &DecodiumBridge::onFt2AsyncDecodeReady, Qt::QueuedConnection);
    connect(m_workerThreadFt2, &QThread::finished, m_ft2Worker, &QObject::deleteLater);
    m_workerThreadFt2->start();

    // Worker thread for FT4 decoder
    m_workerThreadFt4 = new QThread(this);
    m_ft4Worker = new decodium::ft4::FT4DecodeWorker();
    m_ft4Worker->moveToThread(m_workerThreadFt4);
    connect(m_ft4Worker, &decodium::ft4::FT4DecodeWorker::decodeReady,
            this, &DecodiumBridge::onFt4DecodeReady);
    connect(m_workerThreadFt4, &QThread::finished, m_ft4Worker, &QObject::deleteLater);
    m_workerThreadFt4->start();

    // Worker thread for Q65 decoder
    m_workerThreadQ65 = new QThread(this);
    m_q65Worker = new decodium::q65::Q65DecodeWorker();
    m_q65Worker->moveToThread(m_workerThreadQ65);
    connect(m_q65Worker, &decodium::q65::Q65DecodeWorker::decodeReady,
            this, &DecodiumBridge::onQ65DecodeReady);
    connect(m_workerThreadQ65, &QThread::finished, m_q65Worker, &QObject::deleteLater);
    m_workerThreadQ65->start();

    // Worker thread for MSK144 decoder
    m_workerThreadMsk = new QThread(this);
    m_mskWorker = new decodium::msk144::MSK144DecodeWorker();
    m_mskWorker->moveToThread(m_workerThreadMsk);
    connect(m_mskWorker, &decodium::msk144::MSK144DecodeWorker::decodeReady,
            this, &DecodiumBridge::onMsk144DecodeReady);
    connect(m_workerThreadMsk, &QThread::finished, m_mskWorker, &QObject::deleteLater);
    m_workerThreadMsk->start();

    // Worker thread for WSPR decoder
    m_workerThreadWspr = new QThread(this);
    m_wsprWorker = new decodium::wspr::WSPRDecodeWorker();
    m_wsprWorker->moveToThread(m_workerThreadWspr);
    connect(m_wsprWorker, &decodium::wspr::WSPRDecodeWorker::decodeReady,
            this, &DecodiumBridge::onWsprDecodeReady);
    connect(m_workerThreadWspr, &QThread::finished, m_wsprWorker, &QObject::deleteLater);
    m_workerThreadWspr->start();

    // Worker thread for JT65/JT9/JT4 decoder
    m_workerThreadLegacyJt = new QThread(this);
    m_legacyJtWorker = new decodium::legacyjt::LegacyJtDecodeWorker();
    m_legacyJtWorker->moveToThread(m_workerThreadLegacyJt);
    connect(m_legacyJtWorker, &decodium::legacyjt::LegacyJtDecodeWorker::decodeReady,
            this, &DecodiumBridge::onLegacyJtDecodeReady);
    connect(m_workerThreadLegacyJt, &QThread::finished, m_legacyJtWorker, &QObject::deleteLater);
    m_workerThreadLegacyJt->start();

    // Worker thread for FST4/FST4W decoder
    m_workerThreadFst4 = new QThread(this);
    m_fst4Worker = new decodium::fst4::FST4DecodeWorker();
    m_fst4Worker->moveToThread(m_workerThreadFst4);
    connect(m_fst4Worker, &decodium::fst4::FST4DecodeWorker::decodeReady,
            this, &DecodiumBridge::onFst4DecodeReady);
    connect(m_workerThreadFst4, &QThread::finished, m_fst4Worker, &QObject::deleteLater);
    m_workerThreadFst4->start();

    // Period timer: 250ms tick, mode determines how many ticks = 1 period
    m_periodTimer = new QTimer(this);
    m_periodTimer->setInterval(TIMER_MS);
    connect(m_periodTimer, &QTimer::timeout, this, &DecodiumBridge::onPeriodTimer);

    // UTC display timer
    m_utcTimer = new QTimer(this);
    m_utcTimer->setInterval(1000);
    connect(m_utcTimer, &QTimer::timeout, this, &DecodiumBridge::onUtcTimer);
    m_utcTimer->start();

    // Spectrum timer: emette dati FFT ogni 200ms per la waterfall
    m_spectrumTimer = new QTimer(this);
    m_spectrumTimer->setInterval(200);
    connect(m_spectrumTimer, &QTimer::timeout, this, &DecodiumBridge::onSpectrumTimer);

    // Async decode timer: per FT2 turbo async, decodifica ogni 100ms
    m_asyncDecodeTimer = new QTimer(this);
    m_asyncDecodeTimer->setInterval(100);
    connect(m_asyncDecodeTimer, &QTimer::timeout, this, &DecodiumBridge::onAsyncDecodeTimer);

    m_legacyStateTimer = new QTimer(this);
    m_legacyStateTimer->setInterval(250);
    connect(m_legacyStateTimer, &QTimer::timeout, this, &DecodiumBridge::syncLegacyBackendState);

    // TX audio: Modulator (QIODevice pull) + SoundOutput (QAudioSink Qt6)
    m_modulator  = new Modulator(48000, 15.0, this);
    m_soundOutput = new SoundOutput();

    // PTT giù automatico quando il Modulator si ferma (fine TX/Tune)
    connect(m_modulator, &Modulator::stateChanged, this, [this](Modulator::ModulatorState st) {
        if (st == Modulator::Idle) {
            if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend))
                activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false);
            // Ferma SoundOutput per evitare che continui a fare pull
            if (m_soundOutput) m_soundOutput->stop();
            if (m_tuning) {
                m_tuning = false;
                emit tuningChanged();
                emit statusMessage("Tune terminato");
            } else if (m_transmitting) {
                m_transmitting = false;
                emit transmittingChanged();
                emit statusMessage("TX terminato");
            }
        }
    });
    connect(m_soundOutput, &SoundOutput::error, this, [this](const QString& msg) {
        bridgeLog("SoundOutput error: " + msg);
        emit errorMessage("Audio TX: " + msg);
    });
    connect(m_soundOutput, &SoundOutput::status, this, [](const QString& msg) {
        bridgeLog("SoundOutput: " + msg);
    });

    loadSettings();
    enumerateAudioDevices();

    // Auto-connect CAT all'avvio se abilitato
    {
        auto* activeCat = (m_catBackend == "native") ? (QObject*)m_nativeCat : (QObject*)m_hamlibCat;
        bool autoConn = (m_catBackend == "native")   ? m_nativeCat->catAutoConnect()
                      : (m_catBackend == "omnirig") ? m_omniRigCat->catAutoConnect()
                                                    : m_hamlibCat->catAutoConnect();
        bridgeLog("CAT[" + m_catBackend + "] autoConnect=" + QString::number(autoConn));
        if (usingLegacyBackendForTx()) {
            bridgeLog("CAT auto-connect skipped because legacy backend owns TX/CAT on this platform");
        } else if (autoConn) {
            QTimer::singleShot(1500, this, [this]() {
                bridgeLog("CAT auto-connect: chiamata connectRig()");
                if (m_catBackend == "native")        m_nativeCat->connectRig();
                else if (m_catBackend == "omnirig")  m_omniRigCat->connectRig();
                else                                 m_hamlibCat->connectRig();
            });
        }
        Q_UNUSED(activeCat);
    }

    syncLegacyBackendState();
    if (usingLegacyBackendForTx() && m_legacyStateTimer) {
        m_legacyStateTimer->start();
    }

    if (m_autoStartMonitorOnStartup && m_mode != "Echo") {
        const int startupMonitorDelayMs = usingLegacyBackendForTx() ? 2800 : 900;
        QTimer::singleShot(startupMonitorDelayMs, this, [this]() {
            if (!m_autoStartMonitorOnStartup || m_mode == "Echo" || m_monitoring || m_transmitting || m_tuning) {
                return;
            }
            bridgeLog("startup auto-monitor: enabling monitor from bridge");
            setMonitoring(true);
        });
    }
}

DecodiumBridge::~DecodiumBridge()
{
    stopRx();
    // Ferma TX/Tune prima di distruggere tutto
    if (m_txAudioSink) {
        m_txAudioSink->disconnect();
        m_txAudioSink->stop();
    }
    if (m_modulator && m_modulator->isActive()) m_modulator->stop(true);
    if (m_soundOutput) m_soundOutput->stop();
    m_workerThread->quit();      m_workerThread->wait(3000);
    m_workerThreadFt2->quit();   m_workerThreadFt2->wait(3000);
    m_workerThreadFt4->quit();   m_workerThreadFt4->wait(3000);
    m_workerThreadQ65->quit();   m_workerThreadQ65->wait(3000);
    m_workerThreadMsk->quit();        m_workerThreadMsk->wait(3000);
    m_workerThreadWspr->quit();       m_workerThreadWspr->wait(3000);
    m_workerThreadLegacyJt->quit();   m_workerThreadLegacyJt->wait(3000);
    m_workerThreadFst4->quit();       m_workerThreadFst4->wait(3000);
}

QObject * DecodiumBridge::propagationManager() const
{
    return m_propagationManager;
}

bool DecodiumBridge::usingLegacyBackendForTx() const
{
    return m_useLegacyTxBackend && m_legacyBackend && m_legacyBackend->available();
}

void DecodiumBridge::syncLegacyBackendTxState()
{
    if (!usingLegacyBackendForTx()) {
        return;
    }

    if (!m_audioInputDevice.isEmpty()) {
        m_legacyBackend->setAudioInputDeviceName(m_audioInputDevice);
    }
    if (!m_audioOutputDevice.isEmpty()) {
        m_legacyBackend->setAudioOutputDeviceName(m_audioOutputDevice);
    }
    m_legacyBackend->setAudioInputChannel(qBound(0, m_audioInputChannel, 1));
    m_legacyBackend->setAudioOutputChannel(qBound(0, m_audioOutputChannel, 3));
    m_legacyBackend->setTxOutputAttenuation(qRound(qBound(0.0, m_txOutputLevel, 450.0)));
    m_legacyBackend->setMode(m_mode);
    m_legacyBackend->setDialFrequency(m_frequency);
    m_legacyBackend->setAutoSeq(m_autoSeq);
    m_legacyBackend->setAlt12Enabled(m_alt12Enabled);
    m_legacyBackend->setTxFirst(m_txPeriod != 0);
    m_legacyBackend->setRxFrequency(m_rxFrequency);
    m_legacyBackend->setTxFrequency(m_txFrequency);
    m_legacyBackend->setDxCall(m_dxCall);
    m_legacyBackend->setDxGrid(m_dxGrid);
    m_legacyBackend->setTxMessage(1, m_tx1);
    m_legacyBackend->setTxMessage(2, m_tx2);
    m_legacyBackend->setTxMessage(3, m_tx3);
    m_legacyBackend->setTxMessage(4, m_tx4);
    m_legacyBackend->setTxMessage(5, m_tx5);
    m_legacyBackend->setTxMessage(6, m_tx6);
    m_legacyBackend->selectTxMessage(qBound(1, m_currentTx, 6));
    bridgeLog(QStringLiteral("legacyTxSync: mode=%1 outDev=%2 outChan=%3 inDev=%4 inChan=%5 tx=%6 rx=%7 currentTx=%8 txPeriod=%9 alt12=%10 txAttn=%11")
                  .arg(m_mode,
                       m_audioOutputDevice,
                       QString::number(m_audioOutputChannel),
                       m_audioInputDevice,
                       QString::number(m_audioInputChannel),
                       QString::number(m_txFrequency),
                       QString::number(m_rxFrequency),
                       QString::number(m_currentTx),
                       QString::number(m_txPeriod),
                       m_alt12Enabled ? QStringLiteral("1") : QStringLiteral("0"),
                       QString::number(qRound(qBound(0.0, m_txOutputLevel, 450.0)))));
}

void DecodiumBridge::syncLegacyBackendState()
{
    if (!usingLegacyBackendForTx()) {
        return;
    }

    auto updateBool = [] (bool& target, bool value, auto signal) {
        if (target != value) {
            target = value;
            signal();
        }
    };
    auto updateInt = [] (int& target, int value, auto signal) {
        if (target != value) {
            target = value;
            signal();
        }
    };
    auto updateString = [] (QString& target, const QString& value, auto signal) {
        if (target != value) {
            target = value;
            signal();
        }
    };
    auto updateDouble = [] (double& target, double value, auto signal) {
        if (!qFuzzyCompare(target + 1.0, value + 1.0)) {
            target = value;
            signal();
        }
    };
    auto updateTxMessage = [] (QString& target, QString const& value, auto changeSignal, auto extraSignals) {
        QString const normalized = value.trimmed().toUpper();
        if (target != normalized) {
            target = normalized;
            changeSignal();
            extraSignals();
        }
    };

    QString const legacyCallsign = m_legacyBackend->callsign();
    if (!legacyCallsign.isEmpty()) {
        updateString(m_callsign, legacyCallsign, [this]() { emit callsignChanged(); });
    }
    QString const legacyGrid = m_legacyBackend->grid();
    if (!legacyGrid.isEmpty()) {
        updateString(m_grid, legacyGrid, [this]() { emit gridChanged(); });
    }

    updateBool(m_monitoring, m_legacyBackend->monitoring(), [this]() { emit monitoringChanged(); });
    updateBool(m_decoding, m_monitoring, [this]() { emit decodingChanged(); });
    updateBool(m_transmitting, m_legacyBackend->transmitting(), [this]() { emit transmittingChanged(); });
    updateBool(m_tuning, m_legacyBackend->tuning(), [this]() { emit tuningChanged(); });
    updateBool(m_catConnected, m_legacyBackend->catConnected(), [this]() { emit catConnectedChanged(); });
    updateString(m_catRigName, m_catConnected ? m_legacyBackend->rigName() : QString {}, [this]() { emit catRigNameChanged(); });
    updateDouble(m_sMeter, m_legacyBackend->signalLevel(), [this]() { emit sMeterChanged(); });
    updateDouble(m_txOutputLevel, qBound(0.0, static_cast<double>(m_legacyBackend->txOutputAttenuation()), 450.0),
                 [this]() { emit txOutputLevelChanged(); });

    QString const legacyMode = m_legacyBackend->mode();
    if (!legacyMode.isEmpty()) {
        if (m_bandManager) {
            m_bandManager->setCurrentMode(legacyMode);
        }
        bool legacyModeChanged = false;
        if (m_mode != legacyMode) {
            m_mode = legacyMode;
            legacyModeChanged = true;
        }
        updatePeriodTicksMax();
        if (legacyMode == "FT2") {
            if (!m_autoSeq)            { m_autoSeq = true;            emit autoSeqChanged(); }
            if (!m_asyncTxEnabled)     { m_asyncTxEnabled = true;     emit asyncTxEnabledChanged(); }
            if (!m_asyncDecodeEnabled) { m_asyncDecodeEnabled = true; emit asyncDecodeEnabledChanged(); }
            m_qsoCooldown.clear();
        } else if (m_asyncDecodeEnabled) {
            m_asyncDecodeEnabled = false;
            emit asyncDecodeEnabledChanged();
        }
        if (legacyModeChanged) {
            emit modeChanged();
        }
    }

    double const legacyDialFrequency = m_legacyBackend->dialFrequency();
    if (legacyDialFrequency > 0.0 && !qFuzzyCompare(m_frequency + 1.0, legacyDialFrequency + 1.0)) {
        m_frequency = legacyDialFrequency;
        emit frequencyChanged();
    }

    updateInt(m_rxFrequency, m_legacyBackend->rxFrequency(), [this]() { emit rxFrequencyChanged(); });
    updateInt(m_txFrequency, qBound(0, m_legacyBackend->txFrequency(), 3000), [this]() { emit txFrequencyChanged(); });
    updateInt(m_txPeriod, m_legacyBackend->txFirst() ? 1 : 0, [this]() { emit txPeriodChanged(); });
    updateBool(m_alt12Enabled, m_legacyBackend->alt12Enabled(), [this]() { emit alt12EnabledChanged(); });
    updateTxMessage(m_tx1, m_legacyBackend->txMessage(1),
                    [this]() { emit tx1Changed(); },
                    [this]() { emit txMessagesChanged(); emit currentTxMessageChanged(); });
    updateTxMessage(m_tx2, m_legacyBackend->txMessage(2),
                    [this]() { emit tx2Changed(); },
                    [this]() { emit txMessagesChanged(); emit currentTxMessageChanged(); });
    updateTxMessage(m_tx3, m_legacyBackend->txMessage(3),
                    [this]() { emit tx3Changed(); },
                    [this]() { emit txMessagesChanged(); emit currentTxMessageChanged(); });
    updateTxMessage(m_tx4, m_legacyBackend->txMessage(4),
                    [this]() { emit tx4Changed(); },
                    [this]() { emit txMessagesChanged(); emit currentTxMessageChanged(); });
    updateTxMessage(m_tx5, m_legacyBackend->txMessage(5),
                    [this]() { emit tx5Changed(); },
                    [this]() { emit txMessagesChanged(); emit currentTxMessageChanged(); });
    updateTxMessage(m_tx6, m_legacyBackend->txMessage(6),
                    [this]() { emit tx6Changed(); },
                    [this]() { emit txMessagesChanged(); emit currentTxMessageChanged(); });
    updateInt(m_currentTx, qBound(1, m_legacyBackend->currentTx(), 6),
              [this]() { emit currentTxChanged(); emit currentTxMessageChanged(); });

    QString const legacyAudioInput = m_legacyBackend->audioInputDeviceName();
    if (!legacyAudioInput.isEmpty()) {
        updateString(m_audioInputDevice, legacyAudioInput, [this]() { emit audioInputDeviceChanged(); });
    }
    QString const legacyAudioOutput = m_legacyBackend->audioOutputDeviceName();
    if (!legacyAudioOutput.isEmpty()) {
        updateString(m_audioOutputDevice, legacyAudioOutput, [this]() { emit audioOutputDeviceChanged(); });
    }
    updateInt(m_audioInputChannel, qBound(0, m_legacyBackend->audioInputChannel(), 1),
              [this]() { emit audioInputChannelChanged(); });
    updateInt(m_audioOutputChannel, qBound(0, m_legacyBackend->audioOutputChannel(), 3),
              [this]() { emit audioOutputChannelChanged(); });
    updateInt(m_uiPaletteIndex, uiPaletteIndexForLegacyName(m_legacyBackend->waterfallPalette()),
              [this]() { emit uiPaletteIndexChanged(); });

    syncLegacyBackendDecodeList();
}

void DecodiumBridge::syncLegacyBackendDecodeList()
{
    if (!usingLegacyBackendForTx()) {
        return;
    }

    int const bandRevision = m_legacyBackend->bandActivityRevision();
    int const rxRevision = m_legacyBackend->rxFrequencyRevision();
    bool const bandChanged = bandRevision != m_legacyBandActivityRevision;
    bool const rxChanged = rxRevision != m_legacyRxFrequencyRevision;

    if (!bandChanged && !rxChanged) {
        return;
    }

    if (m_dxccLookup && !m_dxccLookup->isLoaded()) {
        QString loadedPath;
        if (reloadDxccLookup(&loadedPath)) {
            bridgeLog("DXCC: caricato on-demand da " + loadedPath + " durante il mirror del backend legacy");
        }
    }

    if (bandChanged) {
        m_legacyBandActivityRevision = bandRevision;
        QVariantList const mirroredDecodes = mirrorLegacyDecodeLines(m_legacyBackend->bandActivityLines(), false);
        if (m_decodeList != mirroredDecodes) {
            m_decodeList = mirroredDecodes;
            emit decodeListChanged();
        }
    }

    if (rxChanged) {
        m_legacyRxFrequencyRevision = rxRevision;
        QVariantList const mirroredRxDecodes = mirrorLegacyDecodeLines(m_legacyBackend->rxFrequencyLines(), true);
        if (m_rxDecodeList != mirroredRxDecodes) {
            m_rxDecodeList = mirroredRxDecodes;
            emit rxDecodeListChanged();
        }
    }
}

QVariantList DecodiumBridge::mirrorLegacyDecodeLines(QStringList const& lines, bool rxPane) const
{
    QVariantList mirroredDecodes;
    mirroredDecodes.reserve(lines.size());

    static const QRegularExpression timePrefix {R"(^(\d{4}|\d{6}))"};
    static const QRegularExpression txPattern {
        R"(^(\d{4}|\d{6})\s+Tx\S*\s+(\d+)\s+\S+\s+(.*)$)",
        QRegularExpression::CaseInsensitiveOption
    };

    for (QString rawLine : lines) {
        rawLine = stripLegacyDecodeAppendage(rawLine);
        if (rawLine.isEmpty() || rawLine.contains(QStringLiteral("------"))) {
            continue;
        }

        auto const txMatch = txPattern.match(rawLine);
        if (txMatch.hasMatch()) {
            QString const message = txMatch.captured(3).trimmed();
            if (message.isEmpty()) {
                continue;
            }

            bool const isCQ = message.startsWith("CQ ", Qt::CaseInsensitive)
                           || message == "CQ"
                           || message.startsWith("QRZ ", Qt::CaseInsensitive);

            QVariantMap entry;
            entry["time"] = normalizeUtcDisplayToken(txMatch.captured(1));
            entry["db"] = QStringLiteral("TX");
            entry["dt"] = QStringLiteral("0.0");
            entry["freq"] = txMatch.captured(2);
            entry["message"] = message;
            entry["aptype"] = QString {};
            entry["quality"] = QString {};
            entry["isTx"] = true;
            entry["isCQ"] = isCQ;
            entry["isMyCall"] = !m_callsign.isEmpty() && message.contains(m_callsign, Qt::CaseInsensitive);
            entry["fromCall"] = extractDecodedCallsign(message, isCQ);
            entry["forceRxPane"] = rxPane;
            enrichDecodeEntry(entry);
            mirroredDecodes.append(entry);
            continue;
        }

        DecodedText decoded {rawLine};
        QString const cleanLine = decoded.clean_string().trimmed();
        if (cleanLine.isEmpty()) {
            continue;
        }

        int const padding = cleanLine.indexOf(QLatin1Char(' ')) > 4 ? 2 : 0;
        int const payloadStart = 22 + padding;
        if (cleanLine.size() <= payloadStart) {
            continue;
        }

        QString const message = cleanLine.mid(payloadStart).trimmed();
        if (message.isEmpty()) {
            continue;
        }

        bool const isTx = decoded.isTX();
        auto const timeMatch = timePrefix.match(cleanLine);
        QString const timeStr = normalizeUtcDisplayToken(timeMatch.hasMatch() ? timeMatch.captured(1) : QString {});
        QString const freqStr = QString::number(decoded.frequencyOffset());
        bool const isCQ = message.startsWith("CQ ", Qt::CaseInsensitive)
                       || message == "CQ"
                       || message.startsWith("QRZ ", Qt::CaseInsensitive);
        bool const isMyCall = !m_callsign.isEmpty() && message.contains(m_callsign, Qt::CaseInsensitive);

        QVariantMap entry;
        entry["time"] = timeStr;
        entry["db"] = isTx ? QStringLiteral("TX") : QString::number(decoded.snr());
        entry["dt"] = isTx ? QStringLiteral("0.0") : QString::number(decoded.dt(), 'f', 1);
        entry["freq"] = freqStr;
        entry["message"] = message;
        entry["aptype"] = QString {};
        entry["quality"] = isTx ? QString {} : QStringLiteral("100");
        entry["isTx"] = isTx;
        entry["isCQ"] = isCQ;
        entry["isMyCall"] = isMyCall;
        entry["fromCall"] = extractDecodedCallsign(message, isCQ);
        entry["forceRxPane"] = rxPane;
        enrichDecodeEntry(entry);
        mirroredDecodes.append(entry);
    }

    return mirroredDecodes;
}

// === STATION ===
QString DecodiumBridge::callsign() const { return m_callsign; }
bool DecodiumBridge::pskReporterConnected() const { return m_pskReporter && m_pskReporter->isConnected(); }

void DecodiumBridge::setCallsign(const QString& v) {
    if (m_callsign != v) {
        m_callsign = v;
        emit callsignChanged();
        if (m_pskReporter) m_pskReporter->setLocalStation(m_callsign, m_grid);
        regenerateTxMessages();
    }
}
QString DecodiumBridge::grid() const { return m_grid; }
void DecodiumBridge::setGrid(const QString& v) {
    if (m_grid != v) {
        m_grid = v;
        emit gridChanged();
        if (m_pskReporter) m_pskReporter->setLocalStation(m_callsign, m_grid);
        if (m_dxCluster)   m_dxCluster->setCallsign(m_callsign);
        regenerateTxMessages();
    }
}

// Rigenera TX6 (CQ) sempre da callsign+grid; TX1-5 solo se dxCall è noto.
// Chiamato quando cambiano callsign, grid o dxCall.
// Non sovrascrive messaggi già personalizzati con dati correnti (formato standard).
void DecodiumBridge::regenerateTxMessages()
{
    if (m_callsign.isEmpty()) return;

    // Grid a 4 caratteri (Maidenhead sub-square)
    QString grid4 = m_grid.left(4).toUpper();

    // TX6 — CQ: sempre rigenerato da callsign+grid
    QString newTx6 = "CQ " + m_callsign.toUpper() + (grid4.isEmpty() ? "" : " " + grid4);
    setTx6(newTx6);

    // TX1-5: rigenerati solo se dxCall è noto
    if (!m_dxCall.isEmpty()) {
        QString dx      = m_dxCall.toUpper();
        QString my      = m_callsign.toUpper();
        QString rptSent = m_reportSent.isEmpty()     ? "-10" : m_reportSent;
        QString rptRcvd = m_reportReceived.isEmpty() ? "-10" : m_reportReceived;

        setTx1(dx + " " + my + (grid4.isEmpty() ? "" : " " + grid4));              // DX MY GRID
        setTx2(dx + " " + my + " " + rptSent);                                     // DX MY REPORT
        setTx3(dx + " " + my + " R" + rptRcvd);                                    // DX MY R+RCV_RPT
        setTx4(dx + " " + my + " " + (m_sendRR73 ? "RR73" : "RRR"));              // DX MY RR73/RRR
        setTx5(dx + " " + my + " 73");                                              // DX MY 73
    } else {
        setTx1(QString());
        setTx2(QString());
        setTx3(QString());
        setTx4(QString());
        setTx5(QString());
        if (m_currentTx >= 1 && m_currentTx <= 5 && !m_tx6.trimmed().isEmpty()) {
            setCurrentTx(6);
        }
    }

    bridgeLog("regenerateTxMessages: tx6=" + m_tx6 +
              " tx1=" + m_tx1 + " dxCall=" + m_dxCall);
}
double DecodiumBridge::frequency() const { return m_frequency; }
void DecodiumBridge::setFrequency(double v) {
    if (m_frequency != v) {
        m_frequency = v;
        emit frequencyChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setDialFrequency(v);
        }
    }
}
QString DecodiumBridge::mode() const { return m_mode; }
void DecodiumBridge::setMode(const QString& v) {
    if (m_mode != v) {
        m_mode = v;
        updatePeriodTicksMax();
        // FT2 rule (mainwindow: "Async L2 is mandatory and always ON"):
        // cbAsyncDecode forced=true, disabled when in FT2.  Mirror that here.
        if (m_mode == "FT2") {
            if (!m_autoSeq)            { m_autoSeq = true;            emit autoSeqChanged(); }
            if (!m_asyncTxEnabled)     { m_asyncTxEnabled = true;     emit asyncTxEnabledChanged(); }
            if (!m_asyncDecodeEnabled) { m_asyncDecodeEnabled = true;  emit asyncDecodeEnabledChanged(); }
            m_qsoCooldown.clear();
        } else {
            // Uscendo da FT2, forza async decode OFF (mainwindow: cbAsyncDecode->setChecked(false))
            if (m_asyncDecodeEnabled)  { m_asyncDecodeEnabled = false; emit asyncDecodeEnabledChanged(); }
        }
        // Aggiorna frequenza banda per il nuovo modo (FT8/FT2/FT4 hanno sub-bande diverse)
        m_bandManager->setCurrentMode(v);
        m_bandManager->updateForMode(v);
        // Riavvia la cattura se attiva per adattare il periodo
        if (m_monitoring) {
            m_periodTicks = 0;
            m_audioBuffer.clear();
            m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
            // Avvia/ferma async timer in base al modo
            if (m_mode == "FT2") {
                m_asyncAudioPos = 0;
                m_asyncDecodePending = false;
                m_asyncDecodeTimer->start();
            } else {
                m_asyncDecodeTimer->stop();
                m_asyncDecodePending = false;
            }
        }
        emit modeChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setMode(v);
        }
    }
}

// === RX/TX STATE ===
bool DecodiumBridge::monitoring() const { return m_monitoring; }
void DecodiumBridge::setMonitoring(bool v) {
    if (v) startRx(); else stopRx();
}
bool DecodiumBridge::transmitting() const { return m_transmitting; }
bool DecodiumBridge::decoding() const { return m_decoding; }

void DecodiumBridge::setRxFrequency(int f)
{
    if (m_rxFrequency != f) {
        m_rxFrequency = f;
        emit rxFrequencyChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setRxFrequency(f);
        }
    }
}

void DecodiumBridge::setTxFrequency(int f)
{
    f = qBound(0, f, 3000);
    if (m_txFrequency != f) {
        m_txFrequency = f;
        emit txFrequencyChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setTxFrequency(f);
        }
    }
}

// === AUDIO ===
double DecodiumBridge::audioLevel() const { return m_audioLevel; }
double DecodiumBridge::sMeter() const { return m_sMeter; }

void DecodiumBridge::setRxInputLevel(double v)
{
    if (m_rxInputLevel != v) {
        m_rxInputLevel = v;
        // if (m_soundInput) m_soundInput->setVolume(static_cast<float>(v / 100.0));
        emit rxInputLevelChanged();
    }
}

void DecodiumBridge::setTxOutputLevel(double v)
{
    double const bounded = qBound(0.0, v, 450.0);
    if (qFuzzyCompare(m_txOutputLevel + 1.0, bounded + 1.0)) {
        return;
    }

    m_txOutputLevel = bounded;

    if (usingLegacyBackendForTx()) {
        m_legacyBackend->setTxOutputAttenuation(qRound(m_txOutputLevel));
    }

    qreal const attenuationDb = txAttenuationFromSlider(m_txOutputLevel);
    if (m_soundOutput) {
        m_soundOutput->setAttenuation(attenuationDb);
    }
    if (m_txAudioSink) {
        m_txAudioSink->setVolume(static_cast<float>(txGainFromSlider(m_txOutputLevel)));
    }

    emit txOutputLevelChanged();
}

QStringList DecodiumBridge::audioInputDevices() const { return m_audioInputDevices; }
QStringList DecodiumBridge::audioOutputDevices() const { return m_audioOutputDevices; }
QString DecodiumBridge::audioInputDevice() const { return m_audioInputDevice; }
void DecodiumBridge::setAudioInputDevice(const QString& v) {
    if (m_audioInputDevice != v) {
        m_audioInputDevice = v;
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setAudioInputDeviceName(v);
        }
        emit audioInputDeviceChanged();
    }
}
QString DecodiumBridge::audioOutputDevice() const { return m_audioOutputDevice; }
void DecodiumBridge::setAudioOutputDevice(const QString& v) {
    if (m_audioOutputDevice != v) {
        m_audioOutputDevice = v;
        bridgeLog("audioOutputDevice set to: " + m_audioOutputDevice);
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setAudioOutputDeviceName(v);
        }
        emit audioOutputDeviceChanged();
    }
}
int DecodiumBridge::audioInputChannel() const { return m_audioInputChannel; }
void DecodiumBridge::setAudioInputChannel(int v) {
    v = qBound(0, v, 1);
    if (m_audioInputChannel != v) {
        m_audioInputChannel = v;
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setAudioInputChannel(v);
        }
        emit audioInputChannelChanged();
    }
}
int DecodiumBridge::audioOutputChannel() const { return m_audioOutputChannel; }
void DecodiumBridge::setAudioOutputChannel(int v) {
    v = qBound(0, v, 3);
    if (m_audioOutputChannel != v) {
        m_audioOutputChannel = v;
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setAudioOutputChannel(v);
        }
        emit audioOutputChannelChanged();
    }
}

void DecodiumBridge::setUiPaletteIndex(int v)
{
    v = qBound(0, v, 5);
    if (m_uiPaletteIndex == v) {
        return;
    }

    m_uiPaletteIndex = v;
    if (usingLegacyBackendForTx()) {
        m_legacyBackend->setWaterfallPalette(legacyPaletteNameForUiIndex(v));
    }
    emit uiPaletteIndexChanged();
}

// === DECODE ===
QVariantList DecodiumBridge::decodeList() const { return m_decodeList; }
QVariantList DecodiumBridge::rxDecodeList() const { return m_rxDecodeList; }
int DecodiumBridge::periodProgress() const { return m_periodProgress; }
QString DecodiumBridge::utcTime() const { return m_utcTime; }

// === TX MESSAGES ===
QString DecodiumBridge::tx1() const { return m_tx1; }
void DecodiumBridge::setTx1(const QString& v) {
    if (m_tx1!=v) {
        m_tx1=v;
        emit tx1Changed();
        emit txMessagesChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->setTxMessage(1, v);
    }
}
QString DecodiumBridge::tx2() const { return m_tx2; }
void DecodiumBridge::setTx2(const QString& v) {
    if (m_tx2!=v) {
        m_tx2=v;
        emit tx2Changed();
        emit txMessagesChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->setTxMessage(2, v);
    }
}
QString DecodiumBridge::tx3() const { return m_tx3; }
void DecodiumBridge::setTx3(const QString& v) {
    if (m_tx3!=v) {
        m_tx3=v;
        emit tx3Changed();
        emit txMessagesChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->setTxMessage(3, v);
    }
}
QString DecodiumBridge::tx4() const { return m_tx4; }
void DecodiumBridge::setTx4(const QString& v) {
    if (m_tx4!=v) {
        m_tx4=v;
        emit tx4Changed();
        emit txMessagesChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->setTxMessage(4, v);
    }
}
QString DecodiumBridge::tx5() const { return m_tx5; }
void DecodiumBridge::setTx5(const QString& v) {
    if (m_tx5!=v) {
        m_tx5=v;
        emit tx5Changed();
        emit txMessagesChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->setTxMessage(5, v);
    }
}
QString DecodiumBridge::tx6() const { return m_tx6; }
void DecodiumBridge::setTx6(const QString& v) {
    if (m_tx6!=v) {
        m_tx6=v;
        emit tx6Changed();
        emit txMessagesChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->setTxMessage(6, v);
    }
}
int DecodiumBridge::currentTx() const { return m_currentTx; }
void DecodiumBridge::setCurrentTx(int v) {
    if (m_currentTx!=v) {
        m_currentTx=v;
        emit currentTxChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->selectTxMessage(qBound(1, v, 6));
    }
}
QString DecodiumBridge::dxCall() const { return m_dxCall; }
void DecodiumBridge::setDxCall(const QString& v) {
    if (m_dxCall != v) {
        m_dxCall = v;
        emit dxCallChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setDxCall(v);
        }
        regenerateTxMessages();
    }
}
QString DecodiumBridge::dxGrid() const { return m_dxGrid; }
void DecodiumBridge::setDxGrid(const QString& v) {
    if (m_dxGrid!=v) {
        m_dxGrid=v;
        emit dxGridChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setDxGrid(v);
        }
        regenerateTxMessages();
    }
}

void DecodiumBridge::setReportSent(const QString& v)
{
    QString const normalized = v.trimmed();
    if (m_reportSent != normalized) {
        m_reportSent = normalized;
        emit reportSentChanged();
        regenerateTxMessages();
    }
}

void DecodiumBridge::setReportReceived(const QString& v)
{
    QString const normalized = v.trimmed();
    if (m_reportReceived != normalized) {
        m_reportReceived = normalized;
        emit reportReceivedChanged();
        regenerateTxMessages();
    }
}

void DecodiumBridge::setSendRR73(bool v)
{
    if (m_sendRR73 != v) {
        m_sendRR73 = v;
        emit sendRR73Changed();
        regenerateTxMessages();
    }
}

void DecodiumBridge::setAutoSeq(bool v)
{
    if (m_autoSeq != v) {
        m_autoSeq = v;
        emit autoSeqChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setAutoSeq(v);
        }
    }
}

void DecodiumBridge::setTxEnabled(bool v)
{
    if (m_txEnabled != v) {
        m_txEnabled = v;
        emit txEnabledChanged();
        if (usingLegacyBackendForTx()) {
            syncLegacyBackendTxState();
            m_legacyBackend->setTxEnabled(v);
        }
    }
}

void DecodiumBridge::setAutoCqRepeat(bool v)
{
    if (m_autoCqRepeat != v) {
        m_autoCqRepeat = v;
        if (m_autoCqRepeat && !m_autoSeq) {
            // AutoCQ must always behave like legacy auto-reply mode.
            m_autoSeq = true;
            emit autoSeqChanged();
            bridgeLog("setAutoCqRepeat: forcing autoSeq=true");
        }
        if (!m_autoCqRepeat) {
            clearAutoCqPartnerLock();
            clearPendingAutoLogSnapshot();
            clearLateAutoLogSnapshot();
        }
        emit autoCqRepeatChanged();
        if (usingLegacyBackendForTx()) {
            syncLegacyBackendTxState();
            m_legacyBackend->setAutoCq(v);
        }
    }
}

void DecodiumBridge::setTxPeriod(int v)
{
    v = (v != 0) ? 1 : 0;
    if (m_txPeriod != v) {
        m_txPeriod = v;
        emit txPeriodChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setTxFirst(m_txPeriod != 0);
        }
    }
}

void DecodiumBridge::setAlt12Enabled(bool v)
{
    if (m_alt12Enabled != v) {
        m_alt12Enabled = v;
        emit alt12EnabledChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setAlt12Enabled(v);
        }
    }
}

// === CAT ===
bool DecodiumBridge::catConnected() const { return m_catConnected; }
QString DecodiumBridge::catRigName() const { return m_catRigName; }
QString DecodiumBridge::catMode() const { return m_catMode; }

// === SETTINGS ===
int DecodiumBridge::nfa() const { return m_nfa; }
void DecodiumBridge::setNfa(int v) { if (m_nfa!=v) { m_nfa=v; emit nfaChanged(); } }
int DecodiumBridge::nfb() const { return m_nfb; }
void DecodiumBridge::setNfb(int v) { if (m_nfb!=v) { m_nfb=v; emit nfbChanged(); } }
int DecodiumBridge::ndepth() const { return m_ndepth; }
void DecodiumBridge::setNdepth(int v) { if (m_ndepth!=v) { m_ndepth=v; emit ndepthChanged(); } }
int DecodiumBridge::ncontest() const { return m_ncontest; }
void DecodiumBridge::setNcontest(int v) { if (m_ncontest!=v) { m_ncontest=v; emit ncontestChanged(); } }

// === QML INVOKABLE ACTIONS ===

void DecodiumBridge::clearTxMessages()
{
    bridgeLog("clearTxMessages: reset QSO/TX state");

    if (m_transmitting || m_tuning)
        halt();

    setTxEnabled(false);
    m_qsoCooldown.clear();
    m_txRetryCount = 0;
    m_nTx73 = 0;
    m_lastNtx = -1;

    if (m_qsoProgress != 0) {
        m_qsoProgress = 0;
        emit qsoProgressChanged();
    }
    if (m_reportSent != "-10") {
        m_reportSent = "-10";
        emit reportSentChanged();
    }
    if (m_reportReceived != "-10") {
        m_reportReceived = "-10";
        emit reportReceivedChanged();
    }

    setDxCall(QString());
    setDxGrid(QString());
    setTx1(QString());
    setTx2(QString());
    setTx3(QString());
    setTx4(QString());
    setTx5(QString());
    setCurrentTx(m_tx6.trimmed().isEmpty() ? 1 : 6);
}

void DecodiumBridge::startRx()
{
    bridgeLog("startRx() called, m_monitoring=" + QString::number(m_monitoring) +
              " m_audioInputDevice=" + m_audioInputDevice);
    if (m_monitoring) { bridgeLog("startRx: already monitoring, skip"); return; }

    if (usingLegacyBackendForTx()) {
        bridgeLog("startRx: delegating monitoring to legacy backend");
        m_periodTimer->stop();
        m_spectrumTimer->stop();
        m_asyncDecodeTimer->stop();
        m_asyncDecodePending = false;
        stopAudioCapture();
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->setMonitoring(true);
        syncLegacyBackendState();
        emit statusMessage("RX avviato via backend legacy - " + m_mode);
        return;
    }

    updatePeriodTicksMax();
    m_monitoring = true;
    emit monitoringChanged();
    m_audioBuffer.clear();
    m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
    m_spectrumBuf.clear();
    m_periodTicks = 0;
    // A2 — Reset drift counters
    m_driftFrameCount    = 0;
    m_driftExpectedFrames = 0;
    m_driftClock.restart();
    // A3 — Mark NTP as synced (system clock assumed synced on desktop)
    m_ntpSynced = true;
    emit ntpSyncedChanged();
    // Sincronizza il period timer al prossimo boundary UTC (FT8=15s, FT4=7.5s, FT2=3s)
    // Necessario per ricevere le finestre complete dei segnali FT8/FT4/FT2
    {
        int periodMs = periodMsForMode(m_mode);
        qint64 msNow = QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay();
        int msInPeriod = (int)(msNow % (qint64)periodMs);
        int msToNext = periodMs - msInPeriod;
        if (msToNext < 250) msToNext += periodMs; // evita boundary troppo vicino
        bridgeLog("startRx: UTC sync wait=" + QString::number(msToNext) + "ms (period=" + QString::number(periodMs) + "ms)");
        QTimer::singleShot(msToNext, this, [this]() {
            if (!m_monitoring) return;
            m_audioBuffer.clear();
            m_periodTicks = 0;
            m_periodTimer->start();
            bridgeLog("startRx: period timer started at UTC boundary");
        });
    }
    m_spectrumTimer->start();
    if (m_mode == "FT2") {
        m_asyncAudioPos = 0;
        m_asyncDecodePending = false;
        m_asyncDecodeTimer->start();
    }

    startAudioCapture();

    m_decoding = true;
    emit decodingChanged();
    emit statusMessage("RX avviato - " + m_mode);
    bridgeLog("startRx() done");
}

void DecodiumBridge::stopRx()
{
    bridgeLog("stopRx() called");
    if (!m_monitoring) { bridgeLog("stopRx: not monitoring, skip"); return; }

    if (usingLegacyBackendForTx()) {
        bridgeLog("stopRx: delegating monitoring stop to legacy backend");
        m_periodTimer->stop();
        m_spectrumTimer->stop();
        m_asyncDecodeTimer->stop();
        m_asyncDecodePending = false;
        stopAudioCapture();
        m_legacyBackend->setMonitoring(false);
        syncLegacyBackendState();
        emit statusMessage("RX fermato");
        return;
    }

    m_monitoring = false;
    emit monitoringChanged();
    m_periodTimer->stop();
    m_spectrumTimer->stop();
    m_asyncDecodeTimer->stop();
    m_asyncDecodePending = false;
    stopAudioCapture();

    m_decoding = false;
    emit decodingChanged();
    emit statusMessage("RX fermato");
}

// === TX helpers ============================================================

// Aggiorna SoundOutput con il dispositivo di uscita correntemente selezionato
void DecodiumBridge::updateSoundOutputDevice()
{
    bool requestedDeviceFound = false;
    QAudioDevice outDev = findOutputDevice(m_audioOutputDevice, &requestedDeviceFound);
    const QAudioFormat fmt = chooseTxAudioFormat(outDev);
    m_soundOutput->setFormat(outDev, static_cast<unsigned>(qMax(1, fmt.channelCount())), 16384);
    bridgeLog("updateSoundOutputDevice: " + outDev.description() +
              " found=" + QString::number(requestedDeviceFound) +
              " fmt=" + audioFormatToString(fmt));
}

// ===========================================================================

// Restituisce QAudioDevice corrispondente a m_audioOutputDevice (o il default)
static QAudioDevice findOutputDevice(const QString& name, bool* requestedDeviceFound)
{
    bool found = name.isEmpty();
    if (!name.isEmpty()) {
        for (const QAudioDevice& d : QMediaDevices::audioOutputs()) {
            if (d.description() == name ||
                d.description().contains(name, Qt::CaseInsensitive) ||
                name.contains(d.description(), Qt::CaseInsensitive)) {
                found = true;
                if (requestedDeviceFound) *requestedDeviceFound = true;
                return d;
            }
        }
    }
    if (requestedDeviceFound) *requestedDeviceFound = found;
    return QMediaDevices::defaultAudioOutput();
}

void DecodiumBridge::startTx()
{
    auto const selectedSlotMessage = [this]() -> QString {
        switch (m_currentTx) {
        case 1: return m_tx1;
        case 2: return m_tx2;
        case 3: return m_tx3;
        case 4: return m_tx4;
        case 5: return m_tx5;
        case 6: return m_tx6;
        default: return {};
        }
    };

    bridgeLog("startTx() called: transmitting=" + QString::number(m_transmitting) +
              " tuning=" + QString::number(m_tuning) +
              " currentTx=" + QString::number(m_currentTx) +
              " mode=" + m_mode +
              " txFreq=" + QString::number(m_txFrequency) +
              " txLevel=" + QString::number(m_txOutputLevel));
    if (m_transmitting || m_tuning) { bridgeLog("startTx: already TX/tuning, abort"); return; }

    bool const fallbackToCq = (m_currentTx >= 1 && m_currentTx <= 5) &&
                              m_dxCall.trimmed().isEmpty() &&
                              selectedSlotMessage().trimmed().isEmpty() &&
                              !m_tx6.trimmed().isEmpty();
    if (fallbackToCq) {
        bridgeLog("startTx: no DX payload available, selecting TX6/CQ");
        setCurrentTx(6);
    }

    QString msg = buildCurrentTxMessage();
    bridgeLog("startTx: msg=[" + msg + "]");
    if (msg.trimmed().isEmpty()) {
        emit errorMessage("Nessun messaggio TX selezionato");
        bridgeLog("startTx: empty msg abort");
        return;
    }

    if (usingLegacyBackendForTx()) {
        bridgeLog("startTx: delegating to legacy backend");
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->armCurrentTx();
        syncLegacyBackendState();
        emit statusMessage("TX armato via backend legacy: " + msg.trimmed());
        return;
    }

    // Codifica → wave float a 48kHz, campioni in [-1.0, +1.0]
    QVector<float> wave;
    const float freq = static_cast<float>(m_txFrequency);

    if (m_mode == "FT2") {
        auto enc = decodium::txmsg::encodeFt2(msg);
        if (!enc.ok) { emit errorMessage("Codifica FT2 fallita: " + msg); return; }
        wave = decodium::txwave::generateFt2Wave(enc.tones.constData(), 103, 4*288, 48000.0f, freq);
    } else if (m_mode == "FT4") {
        auto enc = decodium::txmsg::encodeFt4(msg);
        if (!enc.ok) { emit errorMessage("Codifica FT4 fallita: " + msg); return; }
        wave = decodium::txwave::generateFt4Wave(enc.tones.constData(), 105, 4*576, 48000.0f, freq);
    } else { // FT8
        auto enc = decodium::txmsg::encodeFt8(msg);
        if (!enc.ok) { emit errorMessage("Codifica FT8 fallita: " + msg); return; }
        wave = decodium::txwave::generateFt8Wave(enc.tones.constData(), 79, 7680, 2.0f, 48000.0f, freq);
    }
    bridgeLog("startTx: wave.size()=" + QString::number(wave.size()));
    if (wave.isEmpty()) { emit errorMessage("Generazione onda TX fallita"); bridgeLog("startTx: wave empty abort"); return; }

#if defined(Q_OS_MAC)
    {
        bool requestedDeviceFound = false;
        QAudioDevice outDev = findOutputDevice(m_audioOutputDevice, &requestedDeviceFound);
        if (!requestedDeviceFound) {
            bridgeLog("startTx(mac): requested output device not found, fallback to default: " +
                      outDev.description() + " requested=[" + m_audioOutputDevice + "]");
            emit statusMessage("Audio TX non trovato, uso default: " + outDev.description());
        }
        const QAudioFormat outFmt = chooseTxAudioFormat(outDev);
        const AudioDevice::Channel outChannel = txOutputChannelForFormat(outFmt);
        const qreal attenuationDb = txAttenuationFromSlider(m_txOutputLevel);

        updateSoundOutputDevice();
        m_soundOutput->setAttenuation(attenuationDb);
        if (m_modulator && m_modulator->isActive())
            m_modulator->stop(true);

        // === GitHub TxController: aggiorna contatori retry ===
        if (m_currentTx == m_lastNtx) {
            ++m_txRetryCount;
        } else {
            m_txRetryCount = 1;
            m_lastNtx = m_currentTx;
        }
        if (m_currentTx == 5) ++m_nTx73;

        if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend))
            activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, true);

        m_transmitting = true;
        emit transmittingChanged();

        {
            QVariantMap txEntry;
            txEntry["time"]     = QDateTime::currentDateTimeUtc().toString("HHmmss");
            txEntry["db"]       = "TX";
            txEntry["dt"]       = "0.0";
            txEntry["freq"]     = QString::number(m_txFrequency);
            txEntry["message"]  = msg;
            txEntry["aptype"]   = "";
            txEntry["quality"]  = "";
            txEntry["isTx"]     = true;
            txEntry["isCQ"]     = false;
            txEntry["isMyCall"] = false;
            txEntry["dxCountry"]      = QString();
            txEntry["dxCallsign"]     = QString();
            txEntry["dxContinent"]    = QString();
            txEntry["dxPrefix"]       = QString();
            txEntry["dxIsWorked"]     = false;
            txEntry["dxIsNewBand"]    = false;
            txEntry["dxIsNewCountry"] = false;
            txEntry["dxIsMostWanted"] = false;
            txEntry["dxDistance"]     = -1.0;
            txEntry["dxBearing"]      = -1.0;
            txEntry["isB4"]           = false;
            m_decodeList.append(txEntry);
            emit decodeListChanged();
        }

        unsigned symbolsLength = 79;
        double framesPerSymbol = 1920.0;
        double toneSpacing = -3.0;
        if (m_mode == "FT2") {
            symbolsLength = 103;
            framesPerSymbol = 288.0;
            toneSpacing = -2.0;
        } else if (m_mode == "FT4") {
            symbolsLength = 103;
            framesPerSymbol = 576.0;
            toneSpacing = -2.0;
        }

        m_modulator->setPrecomputedWave(m_mode, wave);
        m_modulator->start(m_mode, symbolsLength, framesPerSymbol,
                           static_cast<double>(m_txFrequency), toneSpacing,
                           m_soundOutput, outChannel, true, false,
                           99.0, periodMsForMode(m_mode) / 1000.0);

        bridgeLog("startTx(mac) modulator: mode=" + m_mode +
                  " msg=" + msg.trimmed() +
                  " dev=" + outDev.description() +
                  " fmt=" + audioFormatToString(outFmt) +
                  " channel=" + QString::number(static_cast<int>(outChannel)) +
                  " attn=" + QString::number(attenuationDb, 'f', 2) +
                  " samples=" + QString::number(wave.size()));

        emit statusMessage("TX: " + msg.trimmed());
        return;
    }
#endif

    // === GitHub TxController: aggiorna contatori retry ===
    // Traccia quante volte abbiamo inviato lo stesso TX step senza risposta
    if (m_currentTx == m_lastNtx) {
        ++m_txRetryCount;
    } else {
        m_txRetryCount = 1;
        m_lastNtx = m_currentTx;
    }
    // TX5 (73): conta le 73 inviate in questo QSO
    if (m_currentTx == 5) ++m_nTx73;

    bool requestedDeviceFound = false;
    QAudioDevice outDev = findOutputDevice(m_audioOutputDevice, &requestedDeviceFound);
    if (!requestedDeviceFound) {
        bridgeLog("startTx: requested output device not found, fallback to default: " +
                  outDev.description() + " requested=[" + m_audioOutputDevice + "]");
        emit statusMessage("Audio TX non trovato, uso default: " + outDev.description());
    }
    const QAudioFormat outFmt = chooseTxAudioFormat(outDev);
    m_txPcmData = buildTxPcmBuffer(wave, outFmt);
    if (m_txPcmData.isEmpty()) {
        bridgeLog("startTx: unable to build PCM buffer for " + audioFormatToString(outFmt));
        emit errorMessage("Formato audio TX non supportato dal device selezionato");
        return;
    }

    // PTT SU — prima di avviare l'audio (come GitHub pttAssert())
    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend))
        activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, true);

    m_transmitting = true;
    emit transmittingChanged();

    // Aggiungi entry TX alla decode list (visibile in RX Frequency e Band Activity)
    {
        QVariantMap txEntry;
        txEntry["time"]     = QDateTime::currentDateTimeUtc().toString("HHmmss");
        txEntry["db"]       = "TX";
        txEntry["dt"]       = "0.0";
        txEntry["freq"]     = QString::number(m_txFrequency);
        txEntry["message"]  = msg;
        txEntry["aptype"]   = "";
        txEntry["quality"]  = "";
        txEntry["isTx"]     = true;
        txEntry["isCQ"]     = false;
        txEntry["isMyCall"] = false;
        txEntry["dxCountry"]     = QString();
        txEntry["dxCallsign"]    = QString();
        txEntry["dxContinent"]   = QString();
        txEntry["dxPrefix"]      = QString();
        txEntry["dxIsWorked"]    = false;
        txEntry["dxIsNewBand"]   = false;
        txEntry["dxIsNewCountry"] = false;
        txEntry["dxIsMostWanted"] = false;
        txEntry["dxDistance"]    = -1.0;
        txEntry["dxBearing"]     = -1.0;
        txEntry["isB4"]          = false;
        m_decodeList.append(txEntry);
        emit decodeListChanged();
    }

    // Pulizia risorse TX precedenti
    if (m_txAudioSink) {
        m_txAudioSink->disconnect();
        m_txAudioSink->stop();
        delete m_txAudioSink;
        m_txAudioSink = nullptr;
    }
    if (m_txPcmBuffer) {
        m_txPcmBuffer->close();
        delete m_txPcmBuffer;
        m_txPcmBuffer = nullptr;
    }

    // Su macOS CoreAudio è più sensibile al timing: lascia il tempo al route
    // di andare in ActiveState prima di spingere il payload.
    int ptt1DelayMs = 0;
#if defined(Q_OS_MAC)
    ptt1DelayMs = 300;
#elif defined(Q_OS_LINUX)
    ptt1DelayMs = (m_mode == "FT2") ? 20 : 0;
#else
    ptt1DelayMs = (m_mode == "FT2") ? 20 : 0;
#endif

    // Lambda che avvia effettivamente QAudioSink dopo il delay ptt1
    auto launchAudio = [this, outDev, outFmt, msg, wave]() {
        if (!m_transmitting) {
            bridgeLog("startTx launchAudio skipped: TX no longer active");
            return;
        }
        m_txPcmBuffer = new QBuffer(this);
        m_txPcmBuffer->setData(m_txPcmData);
        m_txPcmBuffer->open(QIODevice::ReadOnly);
        m_txPcmBuffer->seek(0);

        m_txAudioSink = new QAudioSink(outDev, outFmt, this);
        m_txAudioSink->setVolume(static_cast<float>(txGainFromSlider(m_txOutputLevel)));

        connect(m_txAudioSink, &QAudioSink::stateChanged, this, [this](QAudio::State st) {
            QString extra;
            if (m_txAudioSink) extra = " err=" + QString::number((int)m_txAudioSink->error())
                + " processed=" + QString::number(m_txAudioSink->processedUSecs()) + "us";
            if (m_txPcmBuffer) extra += " bufPos=" + QString::number(m_txPcmBuffer->pos())
                + "/" + QString::number(m_txPcmBuffer->size());
            bridgeLog("TX QAudioSink stateChanged: " + QString::number(static_cast<int>(st)) + extra);
            if (st == QAudio::IdleState) {
                // GitHub ptt0Timer: 200ms delay prima di abbassare PTT
                // Evita clipping dell'ultimo campione nel PA
                QTimer::singleShot(200, this, [this]() {
                    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend)) activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false);
                    if (m_transmitting) {
                        m_transmitting = false;
                        emit transmittingChanged();
                        emit statusMessage("TX completato");
                        // Segna timestamp fine TX per guard timer FT2 async (Shannon m_asyncTxGuardTimer)
                        if (m_mode == "FT2" && m_asyncTxEnabled)
                            m_asyncLastTxEndMs = QDateTime::currentMSecsSinceEpoch();
                        // FT2 async mode: frequency hopping ±25 Hz dopo CQ (GitHub cbAsyncDecode)
                        // CALLING_CQ = 1 (enum 0=IDLE, 1=CALLING_CQ)
                        if (m_mode == "FT2" && m_asyncTxEnabled && m_qsoProgress == 1) {
                            int hop = static_cast<int>(QRandomGenerator::global()->bounded(51)) - 25;
                            int newFreq = qBound(200, m_txFrequency + hop, 3000);
                            if (newFreq != m_txFrequency) {
                                setTxFrequency(newFreq);
                                bridgeLog("FT2 async freq hop: " + QString::number(newFreq) + " Hz");
                            }
                        }
                    }
                });
            } else if (st == QAudio::StoppedState && m_txAudioSink &&
                       m_txAudioSink->error() != QAudio::NoError && m_transmitting) {
                bridgeLog("TX audio stopped with error, dropping PTT");
                if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend))
                    activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false);
                m_transmitting = false;
                emit transmittingChanged();
                emit errorMessage("Errore audio TX sul device selezionato");
            }
        });

        m_txAudioSink->start(m_txPcmBuffer);

        bridgeLog("startTx launchAudio: mode=" + m_mode + " msg=" + msg.trimmed() +
                  " samples=" + QString::number(wave.size()) +
                  " pcm_bytes=" + QString::number(m_txPcmData.size()) +
                  " dev=" + outDev.description() +
                  " fmt=" + audioFormatToString(outFmt) +
                  " TX=" + QString::number(m_currentTx) +
                  " retry=" + QString::number(m_txRetryCount) +
                  " nTx73=" + QString::number(m_nTx73) +
                  " sink_state=" + QString::number(static_cast<int>(m_txAudioSink->state())) +
                  " sink_err=" + QString::number(static_cast<int>(m_txAudioSink->error())));
    };

    if (ptt1DelayMs > 0) {
        bridgeLog("startTx ptt1 delay " + QString::number(ptt1DelayMs) + "ms");
        QTimer::singleShot(ptt1DelayMs, this, launchAudio);
    } else {
        launchAudio();
    }

    emit statusMessage("TX: " + msg.trimmed());
}

void DecodiumBridge::sendTx(int n)
{
    setCurrentTx(n);
    if (usingLegacyBackendForTx()) {
        bridgeLog(QStringLiteral("sendTx: delegating TX%1 to legacy backend").arg(n));
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->armCurrentTx();
        syncLegacyBackendState();
        return;
    }
    startTx();
}

void DecodiumBridge::stopTx()
{
    if (usingLegacyBackendForTx()) {
        bridgeLog("stopTx: delegating to legacy backend");
        m_legacyBackend->stopTransmission();
        syncLegacyBackendState();
        return;
    }

#if defined(Q_OS_MAC)
    if (m_modulator && m_modulator->isActive())
        m_modulator->stop(true);
    if (m_soundOutput)
        m_soundOutput->stop();
    m_txPcmData.clear();
    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend))
        activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false);
    if (m_transmitting) {
        m_transmitting = false;
        emit transmittingChanged();
        emit statusMessage("TX fermato");
    }
    return;
#endif

    if (m_txAudioSink) {
        m_txAudioSink->disconnect();
        m_txAudioSink->stop();
        delete m_txAudioSink;
        m_txAudioSink = nullptr;
    }
    if (m_txPcmBuffer) {
        m_txPcmBuffer->close();
        delete m_txPcmBuffer;
        m_txPcmBuffer = nullptr;
    }
    m_txPcmData.clear();
    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend)) activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false);
    if (m_transmitting) {
        m_transmitting = false;
        emit transmittingChanged();
        emit statusMessage("TX fermato");
    }
}

void DecodiumBridge::startTune()
{
    if (m_tuning || m_transmitting) return;

    if (m_currentTx >= 1 && m_currentTx <= 5 &&
        m_dxCall.trimmed().isEmpty() &&
        buildCurrentTxMessage().trimmed() == m_tx6.trimmed() &&
        !m_tx6.trimmed().isEmpty()) {
        bridgeLog("startTune: no DX payload available, selecting TX6/CQ");
        setCurrentTx(6);
    }

    if (usingLegacyBackendForTx()) {
        bridgeLog("startTune: delegating to legacy backend");
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->startTune(true);
        syncLegacyBackendState();
        emit statusMessage("TUNE via backend legacy");
        return;
    }

#if defined(Q_OS_MAC)
    {
        bool requestedDeviceFound = false;
        QAudioDevice outDev = findOutputDevice(m_audioOutputDevice, &requestedDeviceFound);
        if (!requestedDeviceFound) {
            bridgeLog("startTune(mac): requested output device not found, fallback to default: " +
                      outDev.description() + " requested=[" + m_audioOutputDevice + "]");
            emit statusMessage("Audio TUNE non trovato, uso default: " + outDev.description());
        }
        const QAudioFormat outFmt = chooseTxAudioFormat(outDev);
        const AudioDevice::Channel outChannel = txOutputChannelForFormat(outFmt);
        const qreal attenuationDb = txAttenuationFromSlider(m_txOutputLevel);
        const double freq = m_txFrequency > 0 ? m_txFrequency : 1500.0;

        updateSoundOutputDevice();
        m_soundOutput->setAttenuation(attenuationDb);
        if (m_modulator && m_modulator->isActive())
            m_modulator->stop(true);

        m_tuning = true;
        emit tuningChanged();

        bridgeLog("startTune(mac): canPtt=" + QString::number(activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend)) +
                  " catConnected=" + QString::number(m_catConnected));
        if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend))
            activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, true);

        m_modulator->tune(true);
        m_modulator->start(QStringLiteral("FT8"), 79, 1920.0, freq, 0.0,
                           m_soundOutput, outChannel, false, false,
                           99.0, 15.0);

        bridgeLog("startTune(mac) modulator: freq=" + QString::number(freq) +
                  " dev=" + outDev.description() +
                  " fmt=" + audioFormatToString(outFmt) +
                  " channel=" + QString::number(static_cast<int>(outChannel)) +
                  " attn=" + QString::number(attenuationDb, 'f', 2));
        emit statusMessage("TUNE: " + QString::number(static_cast<int>(freq)) + " Hz");
        return;
    }
#endif

    m_tuning = true;
    emit tuningChanged();

    bridgeLog("startTune: canPtt=" + QString::number(activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend)) +
              " catConnected=" + QString::number(m_catConnected));
    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend))
        activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, true);

    // Timer di loop per rigenerare il tono ogni 10 secondi
    if (!m_tuneTimer) {
        m_tuneTimer = new QTimer(this);
        m_tuneTimer->setInterval(9800);
        connect(m_tuneTimer, &QTimer::timeout, this, [this]() {
            if (m_tuning && !launchTuneAudio()) {
                bridgeLog("startTune: periodic relaunch failed, stopping tune");
                stopTune();
            }
        });
    }
    m_tuneTimer->start();

    if (!launchTuneAudio()) {
        if (m_tuneTimer) m_tuneTimer->stop();
        if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend))
            activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false);
        m_tuning = false;
        emit tuningChanged();
        emit errorMessage("Impossibile avviare l'audio TUNE");
        return;
    }

    const double freq = m_txFrequency > 0 ? m_txFrequency : 1500.0;
    bridgeLog("startTune push: freq=" + QString::number(freq) + " dev=" + m_audioOutputDevice);
    emit statusMessage("TUNE: " + QString::number(static_cast<int>(freq)) + " Hz");
}

bool DecodiumBridge::launchTuneAudio()
{
    // Genera 10 secondi di tono sinusoidale a 48 kHz, poi converte nel formato
    // realmente supportato dal device di uscita.
    const double freq    = m_txFrequency > 0 ? m_txFrequency : 1500.0;
    const int    nFrames = 48000 * 10;
    const double twoPi   = 2.0 * 3.14159265358979323846;
    const double dphi    = twoPi * freq / 48000.0;

    QVector<float> wave;
    wave.resize(nFrames);
    double phi = 0.0;
    for (int i = 0; i < nFrames; ++i) {
        wave[i] = static_cast<float>(std::sin(phi));
        phi += dphi;
        if (phi > twoPi) phi -= twoPi;
    }

    // Pulizia sink precedente
    if (m_txAudioSink) {
        m_txAudioSink->disconnect();
        m_txAudioSink->stop();
        delete m_txAudioSink;
        m_txAudioSink = nullptr;
    }
    if (m_txPcmBuffer) { delete m_txPcmBuffer; m_txPcmBuffer = nullptr; }

    bool requestedDeviceFound = false;
    QAudioDevice outDev = findOutputDevice(m_audioOutputDevice, &requestedDeviceFound);
    if (!requestedDeviceFound) {
        bridgeLog("launchTuneAudio: requested output device not found, fallback to default: " +
                  outDev.description() + " requested=[" + m_audioOutputDevice + "]");
    }
    const QAudioFormat outFmt = chooseTxAudioFormat(outDev);
    m_txPcmData = buildTxPcmBuffer(wave, outFmt);
    if (m_txPcmData.isEmpty()) {
        bridgeLog("launchTuneAudio: unable to build PCM buffer for " + audioFormatToString(outFmt));
        return false;
    }

    m_txPcmBuffer = new QBuffer(this);
    m_txPcmBuffer->setData(m_txPcmData);
    m_txPcmBuffer->open(QIODevice::ReadOnly);
    m_txPcmBuffer->seek(0);

    m_txAudioSink = new QAudioSink(outDev, outFmt, this);
    m_txAudioSink->setVolume(static_cast<float>(txGainFromSlider(m_txOutputLevel)));
    connect(m_txAudioSink, &QAudioSink::stateChanged, this, [this](QAudio::State st) {
        if (st == QAudio::StoppedState && m_txAudioSink &&
            m_txAudioSink->error() != QAudio::NoError && m_tuning) {
            bridgeLog("TUNE audio stopped with error, dropping PTT");
            stopTune();
            emit errorMessage("Errore audio TUNE sul device selezionato");
        }
    });
    m_txAudioSink->start(m_txPcmBuffer);
    bridgeLog("launchTuneAudio: sink_state=" + QString::number((int)m_txAudioSink->state()) +
              " err=" + QString::number((int)m_txAudioSink->error()) +
              " dev=" + outDev.description() +
              " fmt=" + audioFormatToString(outFmt) +
              " pcm_bytes=" + QString::number(m_txPcmData.size()));
    // Verifica dopo 2s se processedUSecs avanza (conferma audio fluisce)
    QTimer::singleShot(2000, this, [this]() {
        if (m_txAudioSink && m_tuning)
            bridgeLog("tuneAudio 2s check: processed=" + QString::number(m_txAudioSink->processedUSecs()) +
                      "us state=" + QString::number((int)m_txAudioSink->state()));
    });
    return true;
}

void DecodiumBridge::stopTune()
{
    if (usingLegacyBackendForTx()) {
        bridgeLog("stopTune: delegating to legacy backend");
        m_legacyBackend->startTune(false);
        syncLegacyBackendState();
        return;
    }

#if defined(Q_OS_MAC)
    if (m_tuneTimer) m_tuneTimer->stop();
    if (m_modulator)
        m_modulator->tune(false);
    if (m_soundOutput)
        m_soundOutput->stop();
    m_txPcmData.clear();
    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend))
        activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false);
    if (m_tuning) {
        m_tuning = false;
        emit tuningChanged();
        emit statusMessage("Tune terminato");
    }
    return;
#endif

    if (!m_tuning) return;
    if (m_tuneTimer) m_tuneTimer->stop();
    if (m_txAudioSink) {
        m_txAudioSink->disconnect();
        m_txAudioSink->stop();
        delete m_txAudioSink;
        m_txAudioSink = nullptr;
    }
    if (m_txPcmBuffer) {
        m_txPcmBuffer->close();
        delete m_txPcmBuffer;
        m_txPcmBuffer = nullptr;
    }
    m_txPcmData.clear();
    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend))
        activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false);
    m_tuning = false;
    emit tuningChanged();
    emit statusMessage("Tune terminato");
}

void DecodiumBridge::halt()
{
    if (usingLegacyBackendForTx()) {
        bridgeLog("halt: delegating stop to legacy backend");
        m_legacyBackend->stopTransmission();
        m_legacyBackend->startTune(false);
        syncLegacyBackendState();
        return;
    }

    stopTx();
    stopTune();
}

void DecodiumBridge::refreshAudioDevices()
{
    enumerateAudioDevices();
}

void DecodiumBridge::clearDecodeList()
{
    if (usingLegacyBackendForTx()) {
        bridgeLog("clearDecodeList: delegating band activity clear to legacy backend");
        m_legacyBackend->clearBandActivity();
        m_legacyBandActivityRevision = -1;
        m_legacyRxFrequencyRevision = -1;
    }
    m_decodeList.clear();
    emit decodeListChanged();
    m_rxDecodeList.clear();
    emit rxDecodeListChanged();
}

void DecodiumBridge::clearRxDecodes()
{
    if (usingLegacyBackendForTx()) {
        m_legacyRxFrequencyRevision = -1;
    }
    m_rxDecodeList.clear();
    emit rxDecodeListChanged();
}

// ── catBackend switch ─────────────────────────────────────────────────────────
void DecodiumBridge::setCatBackend(const QString& v)
{
    if (m_catBackend == v) return;
    halt();
    // Disconnetti il manager attivo prima di cambiare
    if (m_catBackend == "native"  && m_nativeCat->connected())   m_nativeCat->disconnectRig();
    else if (m_catBackend == "omnirig" && m_omniRigCat->connected()) m_omniRigCat->disconnectRig();
    else if (m_catBackend == "hamlib"  && m_hamlibCat->connected())  m_hamlibCat->disconnectRig();
    m_catConnected = false;
    emit catConnectedChanged();
    m_catBackend = v;
    emit catBackendChanged();
    emit catManagerChanged();
    QSettings s2("Decodium","Decodium3");
    s2.setValue("catBackend", m_catBackend);
}


void DecodiumBridge::saveSettings()
{
    QSettings s("Decodium", "Decodium3");
    s.setValue("callsign", m_callsign);
    s.setValue("grid", m_grid);
    s.setValue("frequency", m_frequency);
    s.setValue("mode", m_mode);
    s.setValue("audioInputDevice", m_audioInputDevice);
    s.setValue("audioOutputDevice", m_audioOutputDevice);
    s.setValue("audioInputChannel", m_audioInputChannel);
    s.setValue("audioOutputChannel", m_audioOutputChannel);
    s.setValue("txOutputLevel", m_txOutputLevel);
    s.setValue("nfa", m_nfa);
    s.setValue("nfb", m_nfb);
    s.setValue("ndepth", m_ndepth);
    s.setValue("dxCall", m_dxCall);
    s.setValue("dxGrid", m_dxGrid);
    s.setValue("tx1", m_tx1);
    s.setValue("tx2", m_tx2);
    s.setValue("tx3", m_tx3);
    s.setValue("tx4", m_tx4);
    s.setValue("tx5", m_tx5);
    s.setValue("tx6", m_tx6);
    s.setValue("rxFrequency", m_rxFrequency);
    s.setValue("txFrequency", m_txFrequency);
    s.setValue("fontScale", m_fontScale);
    // extra features
    s.setValue("alertOnCq",       m_alertOnCq);
    s.setValue("alertOnMyCall",   m_alertOnMyCall);
    s.setValue("recordRxEnabled", m_recordRxEnabled);
    s.setValue("recordTxEnabled", m_recordTxEnabled);
    s.setValue("StationName",     m_stationName);
    s.setValue("StationQTH",      m_stationQth);
    s.setValue("StationRigInfo",  m_stationRigInfo);
    s.setValue("StationAntenna",  m_stationAntenna);
    s.setValue("StationPowerW",   m_stationPowerWatts);
    s.setValue("MonitorOFF",      !m_autoStartMonitorOnStartup);
    s.setValue("startFromTx2",    m_startFromTx2);
    s.setValue("vhfUhfFeatures",  m_vhfUhfFeatures);
    s.setValue("directLogQso",    m_directLogQso);
    s.setValue("confirm73",       m_confirm73);
    s.setValue("contestExchange", m_contestExchange);
    s.setValue("contestNumber",   m_contestNumber);
    s.setValue("swlMode",         m_swlMode);
    s.setValue("splitMode",       m_splitMode);
    s.setValue("txWatchdogMode",  m_txWatchdogMode);
    s.setValue("txWatchdogTime",  m_txWatchdogTime);
    s.setValue("txWatchdogCount", m_txWatchdogCount);
    s.setValue("filterCqOnly",    m_filterCqOnly);
    s.setValue("filterMyCallOnly",m_filterMyCallOnly);
    s.setValue("contestType",     m_contestType);
    s.setValue("zapEnabled",      m_zapEnabled);
    s.setValue("deepSearchEnabled",m_deepSearchEnabled);
    s.setValue("asyncDecodeEnabled",m_asyncDecodeEnabled);
    s.setValue("pskReporterEnabled",m_pskReporterEnabled);
    s.setValue("catBackend",        m_catBackend);
    // TX QSO options
    s.setValue("reportReceived",    m_reportReceived);
    s.setValue("sendRR73",          m_sendRR73);
    s.setValue("txPeriod",          m_txPeriod);
    s.setValue("alt12Enabled",      m_alt12Enabled);
    // B7 — Colors
    s.setValue("colorCQ",        m_colorCQ);
    s.setValue("colorMyCall",    m_colorMyCall);
    s.setValue("colorDXEntity",  m_colorDXEntity);
    s.setValue("color73",        m_color73);
    s.setValue("colorB4",        m_colorB4);
    s.setValue("b4Strikethrough",m_b4Strikethrough);
    // B8 — Alerts
    s.setValue("alertSoundsEnabled", m_alertSoundsEnabled);
    // Cloudlog
    s.setValue("cloudlogEnabled",  m_cloudlogEnabled);
    s.setValue("cloudlogUrl",      m_cloudlogUrl);
    s.setValue("cloudlogApiKey",   m_cloudlogApiKey);
    // LotW / ADIF
    s.setValue("lotwEnabled",      m_lotwEnabled);
    // WSPR upload
    s.setValue("wsprUploadEnabled", m_wsprUploadEnabled);
    // DX Cluster
    if (m_dxCluster) m_dxCluster->saveSettings();
    // UI state (posizioni finestre, impostazioni panadapter)
    s.setValue("uiSpectrumHeight",  m_uiSpectrumHeight);
    s.setValue("uiPaletteIndex",    m_uiPaletteIndex);
    s.setValue("uiZoomFactor",      m_uiZoomFactor);
    s.setValue("uiWaterfallHeight", m_uiWaterfallHeight);
    s.setValue("uiDecodeWinX",      m_uiDecodeWinX);
    s.setValue("uiDecodeWinY",      m_uiDecodeWinY);
    s.setValue("uiDecodeWinWidth",  m_uiDecodeWinWidth);
    s.setValue("uiDecodeWinHeight", m_uiDecodeWinHeight);
    emit statusMessage("Impostazioni salvate");
}

void DecodiumBridge::shutdown()
{
    halt();
    stopRx();
    if (m_catBackend == "native" && m_nativeCat->connected())
        m_nativeCat->disconnectRig();
    else if (m_catBackend == "omnirig" && m_omniRigCat->connected())
        m_omniRigCat->disconnectRig();
    else if (m_catBackend == "hamlib" && m_hamlibCat->connected())
        m_hamlibCat->disconnectRig();
    saveSettings();
}

void DecodiumBridge::openSetupSettings(int tabIndex)
{
    if (usingLegacyBackendForTx()) {
        bridgeLog(QStringLiteral("openSetupSettings: delegating to legacy configuration tab %1").arg(tabIndex));
        m_legacyBackend->openSettings(tabIndex);
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        return;
    }

    emit setupSettingsRequested(tabIndex);
}

void DecodiumBridge::openTimeSyncSettings()
{
    if (usingLegacyBackendForTx()) {
        bridgeLog(QStringLiteral("openTimeSyncSettings: delegating to legacy time-sync panel"));
        m_legacyBackend->openTimeSyncSettings();
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        return;
    }

    emit timeSyncSettingsRequested();
}

void DecodiumBridge::openCatSettings()
{
    if (usingLegacyBackendForTx()) {
        openSetupSettings(1);
        return;
    }

    // Segnale intercettato dal QML per aprire il dialog CAT
    emit catSettingsRequested();
}

void DecodiumBridge::retryRigConnection()
{
    if (usingLegacyBackendForTx()) {
        m_legacyBackend->retryRigConnection();
        return;
    }

    if (m_catBackend == "native" && m_nativeCat) {
        m_nativeCat->connectRig();
    } else if (m_catBackend == "omnirig" && m_omniRigCat) {
        m_omniRigCat->connectRig();
    } else if (m_hamlibCat) {
        m_hamlibCat->connectRig();
    }
}

void DecodiumBridge::sendPskReporterNow()
{
    if (m_pskReporter && m_pskReporterEnabled)
        m_pskReporter->sendReport();
}

void DecodiumBridge::searchPskReporter(const QString& callsign)
{
    if (m_pskSearching) return;
    m_pskSearchCallsign = callsign;
    m_pskSearchFound = false;
    m_pskSearching = true;
    emit pskSearchingChanged();
    emit pskSearchCallsignChanged();
    emit pskSearchFoundChanged();
    // Stub: simulate search end after 1s
    QTimer::singleShot(1000, this, [this]() {
        m_pskSearching = false;
        emit pskSearchingChanged();
    });
}

void DecodiumBridge::processDecodeDoubleClick(const QString& message,
                                              const QString& timeStr,
                                              const QString& db,
                                              int audioFreq)
{
    if (message.isEmpty()) return;

    // Aggiorna il report che invieremo in TX2 basandoci sull'SNR del segnale decodificato
    if (!db.isEmpty()) {
        bool ok = false;
        int snr = db.trimmed().toInt(&ok);
        if (ok) {
            QString rpt = (snr >= 0) ? QString("+%1").arg(snr, 2, 10, QChar('0'))
                                     : QString("-%1").arg(-snr, 2, 10, QChar('0'));
            setReportSent(rpt);
        }
    }

    QStringList parts = message.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;

    QString hisCall, hisGrid;
    int newCurrentTx = 1;

    if (parts[0].compare("CQ", Qt::CaseInsensitive) == 0) {
        // CQ [modifier] CALL GRID
        int callIdx = 1;
        if (parts.size() > 2) {
            QString mod = parts[1];
            if (mod == "DX" || mod == "POTA" || mod == "SOTA" ||
                mod == "QRP" || mod.length() <= 3)
                callIdx = 2;
        }
        if (callIdx < parts.size()) hisCall = parts[callIdx];
        if (callIdx + 1 < parts.size()) hisGrid = parts[callIdx + 1];
        newCurrentTx = 1; // risposta CQ con callsign+grid
    } else if (parts.size() >= 2) {
        // FT8 formato: TO_CALL FROM_CALL INFO
        // FROM_CALL è SEMPRE parts[1] — è chi ha trasmesso il messaggio (call di DESTRA)
        // Shannon: identifica il mittente come parts[1] quando parts[0] è il TO
        if (!m_callsign.isEmpty() &&
            parts[0].compare(m_callsign, Qt::CaseInsensitive) == 0) {
            // Messaggio diretto A noi: TO=noi, FROM=hisCall (dx di destra)
            hisCall = parts[1];
        } else {
            // Messaggio non diretto a noi (es: HA8UM OH6CXP -12)
            // TO=HA8UM (sinistra, non ci interessa), FROM=OH6CXP (destra = chi chiama)
            hisCall = parts[1];  // Shannon: sempre il FROM = call di destra
        }
        if (parts.size() >= 3) {
            QString last = parts.last();
            if (last.length() >= 4 && last[0].isLetter() && last[1].isLetter()
                && last[2].isDigit() && last[3].isDigit()) {
                hisGrid = last;
                newCurrentTx = 2; // ricevuto grid → mandiamo report
            } else if (last.compare("RR73", Qt::CaseInsensitive) == 0 ||
                       last.compare("RRR",  Qt::CaseInsensitive) == 0) {
                newCurrentTx = 5; // ricevuto RR73 → mandiamo 73
            } else if (last.compare("73", Qt::CaseInsensitive) == 0) {
                newCurrentTx = 6; // QSO finito
            } else if (last.length() >= 2 &&
                       last[0] == 'R' &&
                       (last[1] == '-' || last[1] == '+' || last[1].isDigit())) {
                newCurrentTx = 4; // ricevuto R+report → mandiamo RR73
            } else if (last.length() >= 2 &&
                       (last[0] == '-' || last[0] == '+') && last[1].isDigit()) {
                newCurrentTx = 3; // ricevuto report → mandiamo R+report
            }
        }
    }

    if (hisCall.isEmpty()) return;

    // Nuovo QSO: resetta contatori retry/watchdog (GitHub handleDoubleClick)
    bool newQso = (hisCall.compare(m_dxCall, Qt::CaseInsensitive) != 0);
    setDxCall(hisCall);
    if (!hisGrid.isEmpty()) setDxGrid(hisGrid);
    else if (!m_dxGrid.isEmpty()) hisGrid = m_dxGrid;

    if (newQso) {
        m_nTx73 = 0;
        m_txRetryCount = 0;
        m_lastNtx = -1;
        m_lastCqPidx = -1;
        m_txWatchdogTicks = 0;
        m_autoCQPeriodsMissed = 0;
        clearPendingAutoLogSnapshot();
        clearAutoCqPartnerLock();
    }

    genStdMsgs(hisCall, hisGrid);
    // advanceQsoState imposta currentTx + qsoProgress + resetta watchdog
    advanceQsoState(newCurrentTx);
    updateAutoCqPartnerLock();

    if (audioFreq > 0) setRxFrequency(audioFreq);

    // Shannon (riga 11397-11406): usa il TIMESTAMP del messaggio decodificato
    // per determinare il periodo TX (NON il tempo corrente al momento del click)
    // nmod = fmod(message.timeInSeconds(), 2*TRperiod); m_txFirst = (nmod != 0)
    int pMs = periodMsForMode(m_mode);
    {
        // Prova a estrarre i secondi dal timeStr (formato "HHMMSS" o "HHMM")
        int msgSecond = -1;
        if (timeStr.length() >= 6) {
            msgSecond = timeStr.mid(4, 2).toInt();  // SS da HHMMSS
        } else if (timeStr.length() == 4) {
            // Solo HHMM: usa il secondo UTC corrente come approssimazione
            msgSecond = QDateTime::currentDateTimeUtc().time().second();
        }

        if (msgSecond >= 0) {
            // Shannon: m_txFirst = (nmod != 0) dove nmod=fmod(msgSecond, 2*TRperiod_s)
            // Tradotto in Decodium3: TX nel periodo opposto a quello del DX
            int dxPidx = (msgSecond * 1000) / pMs;
            m_txPeriod = (dxPidx + 1) % 2;  // periodo successivo al DX
        } else {
            // Fallback: periodo corrente (comportamento originale)
            qint64 msNow = QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay();
            m_txPeriod = (int)(msNow / (qint64)pMs) % 2;
        }
    }
    emit txPeriodChanged();

    // Abilita TX: doppio click su decode abilita TX automaticamente (quick call behavior)
    setTxEnabled(true);

    bridgeLog("processDecodeDoubleClick: hisCall=" + hisCall +
              " TX=" + QString::number(newCurrentTx) +
              " progress=" + QString::number(m_qsoProgress) +
              " txPeriod=" + QString::number(m_txPeriod));
    emit statusMessage("QSO con " + hisCall + " — TX" + QString::number(newCurrentTx));
}

void DecodiumBridge::genStdMsgs(const QString& hisCall, const QString& hisGrid)
{
    if (m_callsign.isEmpty() || hisCall.isEmpty()) return;
    QString my   = m_callsign;
    QString mygr = m_grid.left(4); // gridsquare 4 chars
    QString rptSent = m_reportSent.isEmpty() ? "-10" : m_reportSent;
    QString rptRcvd = m_reportReceived.isEmpty() ? "-10" : m_reportReceived;

    setTx1(hisCall + " " + my + " " + mygr);                          // risposta CQ
    setTx2(hisCall + " " + my + " " + rptSent);                       // il nostro report
    setTx3(hisCall + " " + my + " R" + rptRcvd);                      // R + report ricevuto
    setTx4(hisCall + " " + my + " " + (m_sendRR73 ? "RR73" : "RRR")); // RR73 o RRR
    setTx5(hisCall + " " + my + " 73");                                // 73
    setTx6("CQ "   + my + " " + mygr);                                // CQ
    Q_UNUSED(hisGrid)
}

QString DecodiumBridge::autoCqBandKeyForFrequency(double freqHz) const
{
    if (freqHz <= 0.0) {
        freqHz = m_frequency;
    }

    struct BandRange { double low; double high; const char* key; };
    static BandRange const ranges[] = {
        {  1800000.0,   2000000.0, "160M"  },
        {  3500000.0,   4000000.0, "80M"   },
        {  5000000.0,   5500000.0, "60M"   },
        {  7000000.0,   7300000.0, "40M"   },
        { 10100000.0,  10150000.0, "30M"   },
        { 14000000.0,  14350000.0, "20M"   },
        { 18068000.0,  18168000.0, "17M"   },
        { 21000000.0,  21450000.0, "15M"   },
        { 24890000.0,  24990000.0, "12M"   },
        { 28000000.0,  29700000.0, "10M"   },
        { 50000000.0,  54000000.0, "6M"    },
        { 70000000.0,  71000000.0, "4M"    },
        {144000000.0, 148000000.0, "2M"    },
        {420000000.0, 450000000.0, "70CM"  },
    };

    for (BandRange const& range : ranges) {
        if (freqHz >= range.low && freqHz <= range.high) {
            return QString::fromLatin1(range.key);
        }
    }

    QString const currentBand = m_bandManager ? m_bandManager->currentBandLambda().trimmed().toUpper()
                                              : QString {};
    return currentBand.isEmpty() ? QStringLiteral("--") : currentBand;
}

bool DecodiumBridge::isRecentAutoCqDuplicate(const QString& call, double freqHz, const QString& mode) const
{
    QString const dedupeCall = Radio::base_callsign(call).trimmed().toUpper();
    if (dedupeCall.isEmpty()) {
        return false;
    }

    QString const dedupeBand = autoCqBandKeyForFrequency(freqHz).trimmed().toUpper();
    QString const dedupeMode = (mode.isEmpty() ? m_mode : mode).trimmed().toUpper();
    if (dedupeBand.isEmpty() || dedupeBand == QStringLiteral("--") || dedupeMode.isEmpty()) {
        return false;
    }

    QString const dedupeKey = QStringLiteral("%1|%2|%3").arg(dedupeCall, dedupeBand, dedupeMode);
    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();

    auto const abandonedIt = m_recentAutoCqAbandonedUtcByKey.constFind(dedupeKey);
    if (abandonedIt != m_recentAutoCqAbandonedUtcByKey.constEnd()) {
        return isWithinRecentDuplicateWindow(abandonedIt.value(), nowUtc);
    }

    auto const workedIt = m_recentAutoCqWorkedUtcByKey.constFind(dedupeKey);
    if (workedIt != m_recentAutoCqWorkedUtcByKey.constEnd()) {
        return isWithinRecentDuplicateWindow(workedIt.value(), nowUtc);
    }

    auto const loggedIt = m_recentQsoLogUtcByKey.constFind(dedupeKey);
    if (loggedIt == m_recentQsoLogUtcByKey.constEnd()) {
        return false;
    }

    return isWithinRecentDuplicateWindow(loggedIt.value(), nowUtc);
}

void DecodiumBridge::rememberRecentAutoCqAbandoned(const QString& call, double freqHz, const QString& mode)
{
    QString const dedupeCall = Radio::base_callsign(call).trimmed().toUpper();
    QString const dedupeBand = autoCqBandKeyForFrequency(freqHz).trimmed().toUpper();
    QString const dedupeMode = mode.trimmed().toUpper();
    if (dedupeCall.isEmpty() || dedupeBand.isEmpty() || dedupeBand == QStringLiteral("--") || dedupeMode.isEmpty()) {
        return;
    }

    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    QString const dedupeKey = QStringLiteral("%1|%2|%3").arg(dedupeCall, dedupeBand, dedupeMode);
    m_recentAutoCqAbandonedUtcByKey.insert(dedupeKey, nowUtc);
    pruneRecentDuplicateCache(m_recentAutoCqAbandonedUtcByKey, nowUtc);
    bridgeLog(QStringLiteral("AutoCQ remember-abandoned: %1").arg(dedupeKey));
}

void DecodiumBridge::rememberRecentAutoCqWorked(const QString& call, double freqHz, const QString& mode)
{
    QString const dedupeCall = Radio::base_callsign(call).trimmed().toUpper();
    QString const dedupeBand = autoCqBandKeyForFrequency(freqHz).trimmed().toUpper();
    QString const dedupeMode = mode.trimmed().toUpper();
    if (dedupeCall.isEmpty() || dedupeBand.isEmpty() || dedupeBand == QStringLiteral("--") || dedupeMode.isEmpty()) {
        return;
    }

    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    QString const dedupeKey = QStringLiteral("%1|%2|%3").arg(dedupeCall, dedupeBand, dedupeMode);
    m_recentAutoCqWorkedUtcByKey.insert(dedupeKey, nowUtc);
    pruneRecentDuplicateCache(m_recentAutoCqWorkedUtcByKey, nowUtc);
    bridgeLog(QStringLiteral("AutoCQ remember-worked: %1").arg(dedupeKey));
}

void DecodiumBridge::removeCallerFromQueue(const QString& call)
{
    QString const targetBase = Radio::base_callsign(call).trimmed().toUpper();
    if (targetBase.isEmpty() || m_callerQueue.isEmpty()) {
        return;
    }

    bool removed = false;
    QStringList remaining;
    remaining.reserve(m_callerQueue.size());
    for (QString const& entry : m_callerQueue) {
        QStringList const parts = entry.split(' ', Qt::SkipEmptyParts);
        QString const entryBase = parts.isEmpty()
            ? QString {}
            : Radio::base_callsign(parts.first()).trimmed().toUpper();
        if (!entryBase.isEmpty() && entryBase == targetBase) {
            removed = true;
            continue;
        }
        remaining.push_back(entry);
    }

    if (removed) {
        m_callerQueue = remaining;
        emit callerQueueChanged();
    }
}

void DecodiumBridge::capturePendingAutoLogSnapshot()
{
    QString snapshotCall = m_dxCall.trimmed();
    if (snapshotCall.isEmpty()) {
        snapshotCall = m_autoCqLockedCall.trimmed();
    }
    if (snapshotCall.isEmpty()) {
        return;
    }

    m_pendingAutoLogValid = true;
    m_pendingAutoLogCall = snapshotCall;
    m_pendingAutoLogGrid = !m_dxGrid.trimmed().isEmpty() ? m_dxGrid.trimmed() : m_autoCqLockedGrid.trimmed();
    m_pendingAutoLogRptSent = m_reportSent.trimmed();
    m_pendingAutoLogRptRcvd = m_reportReceived.trimmed();
    m_pendingAutoLogOn = QDateTime::currentDateTimeUtc();
    m_pendingAutoLogDialFreq = m_frequency;
}

void DecodiumBridge::clearPendingAutoLogSnapshot()
{
    m_pendingAutoLogValid = false;
    m_pendingAutoLogCall.clear();
    m_pendingAutoLogGrid.clear();
    m_pendingAutoLogRptSent.clear();
    m_pendingAutoLogRptRcvd.clear();
    m_pendingAutoLogOn = QDateTime {};
    m_pendingAutoLogDialFreq = 0.0;
}

void DecodiumBridge::armLateAutoLogSnapshot()
{
    capturePendingAutoLogSnapshot();
    if (!m_pendingAutoLogValid) {
        return;
    }

    m_lateAutoLogValid = true;
    m_lateAutoLogCall = m_pendingAutoLogCall;
    m_lateAutoLogGrid = m_pendingAutoLogGrid;
    m_lateAutoLogRptSent = m_pendingAutoLogRptSent;
    m_lateAutoLogRptRcvd = m_pendingAutoLogRptRcvd;
    m_lateAutoLogOn = m_pendingAutoLogOn;
    m_lateAutoLogDialFreq = m_pendingAutoLogDialFreq;
    m_lateAutoLogExpires = QDateTime::currentDateTimeUtc().addSecs(kLateAutoLogGraceWindowSeconds);
    rememberRecentAutoCqAbandoned(m_lateAutoLogCall, m_lateAutoLogDialFreq, m_mode);
}

void DecodiumBridge::clearLateAutoLogSnapshot()
{
    m_lateAutoLogValid = false;
    m_lateAutoLogCall.clear();
    m_lateAutoLogGrid.clear();
    m_lateAutoLogRptSent.clear();
    m_lateAutoLogRptRcvd.clear();
    m_lateAutoLogOn = QDateTime {};
    m_lateAutoLogDialFreq = 0.0;
    m_lateAutoLogExpires = QDateTime {};
}

void DecodiumBridge::clearAutoCqPartnerLock()
{
    m_autoCqLockedCall.clear();
    m_autoCqLockedGrid.clear();
    m_autoCqLockedNtx = 6;
    m_autoCqLockedProgress = 1;
}

void DecodiumBridge::updateAutoCqPartnerLock()
{
    if (!m_autoCqRepeat || m_qsoProgress <= 1) {
        return;
    }

    QString lockedCall = m_dxCall.trimmed();
    if (lockedCall.isEmpty()) {
        return;
    }

    m_autoCqLockedCall = lockedCall;
    m_autoCqLockedGrid = m_dxGrid.trimmed();
    m_autoCqLockedNtx = (m_currentTx >= 1 && m_currentTx <= 5) ? m_currentTx : 2;
    m_autoCqLockedProgress = m_qsoProgress;
}

void DecodiumBridge::restoreAutoCqPartnerLock()
{
    if (!m_autoCqRepeat || m_autoCqLockedCall.trimmed().isEmpty()) {
        return;
    }

    QString const lockedBase = Radio::base_callsign(m_autoCqLockedCall).trimmed().toUpper();
    if (lockedBase.isEmpty()) {
        return;
    }
    if ((m_currentTx == 6 || m_qsoProgress <= 1) && isRecentAutoCqDuplicate(lockedBase, m_frequency, m_mode)) {
        clearAutoCqPartnerLock();
        return;
    }

    QString const currentBase = Radio::base_callsign(m_dxCall).trimmed().toUpper();
    bool const currentMatches = !currentBase.isEmpty() && currentBase == lockedBase;
    bool const unexpectedCqFallback = (m_currentTx == 6) || (m_qsoProgress <= 1) || m_dxCall.trimmed().isEmpty();
    if (currentMatches && !unexpectedCqFallback) {
        return;
    }

    setDxCall(m_autoCqLockedCall);
    if (!m_autoCqLockedGrid.isEmpty()) {
        setDxGrid(m_autoCqLockedGrid);
    }

    int restoredNtx = m_autoCqLockedNtx;
    if (restoredNtx < 1 || restoredNtx > 5) {
        restoredNtx = 2;
    }
    setCurrentTx(restoredNtx);
    m_qsoProgress = qMax(2, m_autoCqLockedProgress);
    emit qsoProgressChanged();
    m_txRetryCount = 0;
    m_txWatchdogTicks = 0;
    regenerateTxMessages();
    setTxEnabled(true);
    bridgeLog(QStringLiteral("AutoCQ lock-restore: %1 TX%2").arg(lockedBase).arg(restoredNtx));
}

void DecodiumBridge::enqueueCallerInternal(const QString& call, int freq, int snr)
{
    QString const queuedCall = Radio::base_callsign(call).trimmed().toUpper();
    if (queuedCall.isEmpty()) {
        return;
    }
    if (isRecentAutoCqDuplicate(queuedCall, m_frequency, m_mode)) {
        bridgeLog(QStringLiteral("AutoCQ queue-skip-recent: %1").arg(queuedCall));
        return;
    }

    for (QString const& entry : m_callerQueue) {
        QStringList const parts = entry.split(' ', Qt::SkipEmptyParts);
        if (!parts.isEmpty() &&
            Radio::base_callsign(parts.first()).trimmed().compare(queuedCall, Qt::CaseInsensitive) == 0) {
            bridgeLog(QStringLiteral("AutoCQ queue-skip-duplicate: %1").arg(queuedCall));
            return;
        }
    }

    if (m_callerQueue.size() >= FOX_QUEUE_MAX) {
        bridgeLog(QStringLiteral("AutoCQ queue-skip-full: %1").arg(queuedCall));
        return;
    }

    QString entry = queuedCall;
    if (freq > 0) {
        entry += QStringLiteral(" %1").arg(freq);
        if (snr >= -99) {
            entry += QStringLiteral(" %1").arg(snr);
        }
    }
    m_callerQueue.append(entry);
    emit callerQueueChanged();
}

// === advanceQsoState — GitHub TxController clone ===
// Mappa TX button number → QSOProgress enum, resetta watchdog
// 0=IDLE, 1=CALLING_CQ, 2=REPLYING, 3=REPORT, 4=ROGER_REPORT, 5=SIGNOFF, 6=IDLE_QSO
void DecodiumBridge::advanceQsoState(int txNum)
{
    // Quick QSO (Ultra2): salta TX1 → vai diretto a TX2 (come FT2QsoFlowPolicy Ultra2)
    if (txNum == 1 && m_quickQsoEnabled && m_mode == "FT2") {
        bridgeLog("advanceQsoState: QuickQSO attivo → salta TX1, va a TX2 (Ultra2)");
        txNum = 2;
    }

    int progress = 0;
    switch (txNum) {
        case 1: progress = 2; break; // TX1 (risposta CQ)  → REPLYING (in attesa risposta)
        case 2: progress = 3; break; // TX2 (report)       → REPORT
        case 3: progress = 4; break; // TX3 (R+report)     → ROGER_REPORT
        case 4: progress = 5; break; // TX4 (RR73/RRR)     → SIGNOFF
        case 5: progress = 5; break; // TX5 (73)           → SIGNOFF
        case 6: progress = 1; break; // TX6 (CQ)           → CALLING_CQ
        default: return;
    }
    setCurrentTx(txNum);
    if (m_qsoProgress != progress) { m_qsoProgress = progress; emit qsoProgressChanged(); }
    m_txWatchdogTicks = 0;  // reset watchdog ad ogni avanzamento di stato
    bridgeLog("advanceQsoState: TX" + QString::number(txNum) +
              " → progress=" + QString::number(progress));
}

void DecodiumBridge::checkAndStartPeriodicTx()
{
    if (!m_monitoring || m_transmitting || m_tuning) return;
    if (!m_txEnabled && !m_autoCqRepeat) return;
    if (m_autoCqRepeat && m_dxCall.isEmpty() && m_currentTx != 6) {
        restoreAutoCqPartnerLock();
    }

    // FT2 async mode:
    // - Per risposte QSO (m_txEnabled, stazione DX nota): salta il controllo di periodo
    //   → risposta immediata alla stazione partner senza aspettare il boundary
    // - Per AutoCQ (nessuna stazione DX): usa il controllo di periodo normale
    //   → evita loop CQ continuo (CQ ogni 3.75s senza pausa RX)
    bool inQsoResponse = (m_mode == "FT2" && m_asyncTxEnabled &&
                          m_txEnabled && !m_dxCall.isEmpty());
    bool skipPeriodCheck = inQsoResponse;

    if (!skipPeriodCheck) {
        qint64 msNow = QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay();
        int pMs  = periodMsForMode(m_mode);
        int pidx = (int)(msNow / (qint64)pMs);
        bool isEven = (pidx % 2 == 0);
        bool isOurPeriod = (m_txPeriod == 0) ? isEven : !isEven;
        if (!isOurPeriod) return;

        // Guardia anti-CQ-consecutivi: FT2=4 periodi (~15s), FT8=2 periodi (~30s)
        bool isCqTx = (m_autoCqRepeat && !m_txEnabled) ||
                      (m_txEnabled && m_currentTx == 6);
        int cqGuardPeriods = (m_mode == "FT2") ? 4 : 2;
        if (isCqTx && m_lastCqPidx >= 0 && (pidx - m_lastCqPidx) < cqGuardPeriods) {
            bridgeLog("checkAndStartPeriodicTx: CQ guard — pidx=" + QString::number(pidx) +
                      " lastCqPidx=" + QString::number(m_lastCqPidx) + " → pausa RX forzata");
            return;
        }
    } else {
        // QSO response: Shannon m_asyncTxGuardTimer 100ms dopo fine TX
        qint64 msNow    = QDateTime::currentMSecsSinceEpoch();
        qint64 guardMs  = 100;
        if (m_asyncLastTxEndMs > 0 && (msNow - m_asyncLastTxEndMs) < guardMs) {
            return;  // in guard PTT
        }
    }

    if (m_txEnabled && !m_tx1.isEmpty()) {
        // TX5 (73) inviato >=5 volte → QSO completo (allineato a macOS/Repo2: nTx73 >= 5)
        if (m_currentTx == 5 && m_nTx73 >= 5) {
            bridgeLog("checkAndStartPeriodicTx: nTx73=" + QString::number(m_nTx73) +
                      " >= 5 → QSO completo (TX5 timeout)");
            if ((m_directLogQso || m_autoCqRepeat) && !m_dxCall.isEmpty()) {
                capturePendingAutoLogSnapshot();
                logQso();
            }
            setTxEnabled(false);
            if (m_qsoProgress != 6) { m_qsoProgress = 6; emit qsoProgressChanged(); } // IDLE_QSO
            // clearDX: azzera contatori e DX prima di processare coda o CQ
            m_nTx73 = 0;
            m_txRetryCount = 0;
            m_lastNtx = -1;
            m_lastCqPidx = -1;
            clearAutoCqPartnerLock();
            setDxCall(QString());
            setDxGrid(QString());
            if (m_multiAnswerMode && !m_callerQueue.isEmpty()) {
                processNextInQueue();
            } else if (m_autoCqRepeat && !m_tx6.isEmpty()) {
                advanceQsoState(6);
                startTx();
            }
            return;
        }

        // === Shannon: maxCallerRetries (5 tentativi per step, poi si ferma) ===
        // Eccezione: CQ (TX6) in AutoCQ mode non ha retry limit — continua indefinitamente
        bool isCqAutoCq = (m_currentTx == 6 && m_autoCqRepeat);
        if (!isCqAutoCq && m_currentTx == m_lastNtx && m_txRetryCount >= m_maxCallerRetries) {
            bridgeLog("checkAndStartPeriodicTx: retry limit " +
                      QString::number(m_maxCallerRetries) + " su TX" +
                      QString::number(m_currentTx) + " → halt");
            if (m_autoCqRepeat && m_qsoProgress > 1) {
                armLateAutoLogSnapshot();
            }
            // Shannon stopTx2(): ferma completamente, torna a CQ SOLO se autoCQ attivo
            setTxEnabled(false);
            m_txRetryCount = 0;
            m_lastNtx = -1;
            m_nTx73    = 0;
            m_lastCqPidx = -1;
            clearAutoCqPartnerLock();
            // clearDX: azzera dxCall (stazione non raggiungibile)
            setDxCall(QString());
            setDxGrid(QString());
            if (m_multiAnswerMode && !m_callerQueue.isEmpty()) {
                processNextInQueue();
            } else if (m_autoCqRepeat && !m_tx6.isEmpty()) {
                advanceQsoState(6);  // CQ
                startTx();
            }
            return;
        }

        bridgeLog("checkAndStartPeriodicTx: TX" + QString::number(m_currentTx) +
                  " retry=" + QString::number(m_txRetryCount) +
                  " async=" + QString::number(m_asyncTxEnabled));
        // Aggiorna lastCqPidx se stiamo inviando CQ (TX6)
        if (m_currentTx == 6 && !skipPeriodCheck) {
            qint64 msNow2 = QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay();
            int pMs2 = periodMsForMode(m_mode);
            m_lastCqPidx = (int)(msNow2 / (qint64)pMs2);
        }
        startTx();
    } else if (m_autoCqRepeat && !m_tx6.isEmpty()) {
        bridgeLog("checkAndStartPeriodicTx: AutoCQ async=" + QString::number(m_asyncTxEnabled));
        advanceQsoState(6);  // CQ → CALLING (0)
        // Aggiorna lastCqPidx per la guardia anti-consecutivi
        if (!skipPeriodCheck) {
            qint64 msNow2 = QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay();
            int pMs2 = periodMsForMode(m_mode);
            m_lastCqPidx = (int)(msNow2 / (qint64)pMs2);
        }
        startTx();
    }
}

void DecodiumBridge::autoSequenceStep(const QStringList& f)
{
    // f = [time, snr, dt, df, message, aptype, quality, freq]
    // Corrisponde a GitHub processDecodedMessage()
    if (f.size() < 5) return;
    QString msg = f[4].trimmed();
    QStringList parts = msg.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() < 2) return;

    // Estrai il mittente: TO_CALL FROM_CALL ...
    QString from;
    if (!m_callsign.isEmpty() &&
        parts[0].compare(m_callsign, Qt::CaseInsensitive) == 0) {
        from = parts[1];
    } else {
        from = parts[0];
    }

    // FT2 QSO cooldown: Shannon m_qsoCooldown — ignora 73/RR73 da stazione già chiusa (<30s)
    // In FT2 async lo stesso 73 viene decodificato ogni ~4s; senza cooldown si fa loop infinito
    QString last_word = parts.last();
    bool is_73 = (last_word.compare("73",   Qt::CaseInsensitive) == 0 ||
                  last_word.compare("RR73",  Qt::CaseInsensitive) == 0 ||
                  last_word.compare("RRR",   Qt::CaseInsensitive) == 0);
    if (is_73 && m_mode == "FT2" && m_asyncTxEnabled && !from.isEmpty()) {
        qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        // purge entries scadute (>30s)
        for (auto it = m_qsoCooldown.begin(); it != m_qsoCooldown.end(); ) {
            if (nowMs - it.value() > 30000) it = m_qsoCooldown.erase(it);
            else ++it;
        }
        if (m_qsoCooldown.contains(from)) {
            bridgeLog("FT2 cooldown: ignoro " + last_word + " da " + from + " (QSO recente)");
            return;
        }
    }

    // Shannon (riga 9699-9720): filtro messaggi — solo quelli rilevanti per noi
    // CQ mode (m_bCallingCQ && m_bAutoReply): accetta solo chi ci chiama (parts[0]==m_callsign)
    // Non CQ mode: accetta solo dalla stazione DX corrente; se dxCall vuoto → non rispondere
    // Shannon: inCqMode attivo solo con autoCqRepeat (allineato a Shannon riga 9699)
    bool inCqMode = m_autoCqRepeat
                    && (m_currentTx == 6 || m_dxCall.isEmpty() || m_qsoProgress <= 1);
    if (inCqMode) {
        // Rispondo solo a chi mi ha mandato un messaggio diretto (parts[0] == mia stazione)
        // Es: "IU8LMC OH6CXP KP20" → parts[0]="IU8LMC" = m_callsign → accettiamo
        if (m_callsign.isEmpty() ||
            parts[0].compare(m_callsign, Qt::CaseInsensitive) != 0) return;
    } else {
        // Non in CQ mode: nessun dxCall → non rispondere automaticamente (Shannon: clearDX)
        if (m_dxCall.isEmpty()) {
            restoreAutoCqPartnerLock();
        }
        if (m_dxCall.isEmpty()) return;
        // Rispondo solo dalla stazione DX corrente
        if (from.compare(m_dxCall, Qt::CaseInsensitive) != 0) {
            // Stazione diversa: in multi-answer mode, aggiunge alla coda se ci sta chiamando
            // (parts[0] == m_callsign indica che il messaggio è indirizzato a noi)
            if (m_multiAnswerMode && !from.isEmpty() && !m_callsign.isEmpty() &&
                parts[0].compare(m_callsign, Qt::CaseInsensitive) == 0) {
                int freq = f.size() >= 8 ? f[7].toInt() : m_rxFrequency;
                int snr = f.size() >= 2 ? f[1].toInt() : -99;
                enqueueCallerInternal(from, freq, snr);
                bridgeLog("autoSeq: multi-answer: enqueue " + from + " (in QSO con " + m_dxCall + ")");
            }
            return;
        }
    }

    QString last = parts.last();
    int nextTx = -1;

    if (inCqMode && !from.isEmpty()) {
        QString const callerBase = Radio::base_callsign(from).trimmed().toUpper();
        if (callerBase.isEmpty()) {
            return;
        }

        if (m_dxCall.trimmed().isEmpty() && isRecentAutoCqDuplicate(callerBase, m_frequency, m_mode)) {
            bridgeLog("autoSeq: skip recent AutoCQ caller " + callerBase);
            return;
        }

        if (Radio::base_callsign(m_dxCall).trimmed().compare(callerBase, Qt::CaseInsensitive) != 0) {
            setDxCall(from);
            if (!isGridTokenStrict(last)) {
                setDxGrid(QString());
            }
        }

        m_txRetryCount = 0;
        m_lastNtx = -1;
        m_nTx73 = 0;
        m_lastCqPidx = -1;
        m_autoCQPeriodsMissed = 0;
        m_txWatchdogTicks = 0;
        setReportSent(QString::number(decodeSnrOrDefault(f.value(1), -10)));

        if (isGridTokenStrict(last)) {
            setDxGrid(last.toUpper());
        }

        bridgeLog("autoSeq: start AutoCQ partner " + callerBase + " msg=" + msg);
    }

    if (last.compare("73", Qt::CaseInsensitive) == 0) {
        // GitHub: qsoComplete → IDLE_QSO (5), halt se autoSeq disattivo
        bridgeLog("autoSeq: ricevuto 73 da " + from + " → QSO completo (IDLE_QSO)");
        // FT2 async: aggiungi al cooldown per evitare re-triggering sullo stesso 73
        if (m_mode == "FT2" && m_asyncTxEnabled && !from.isEmpty())
            m_qsoCooldown[from] = QDateTime::currentMSecsSinceEpoch();
        if ((m_directLogQso || m_autoCqRepeat) && !m_dxCall.isEmpty()) {
            capturePendingAutoLogSnapshot();
            logQso();
        }
        // QSO finito: disabilita TX automatico
        setTxEnabled(false);
        if (m_qsoProgress != 6) { m_qsoProgress = 6; emit qsoProgressChanged(); } // IDLE_QSO
        // mainwindow clearDX(): azzera stato QSO PRIMA di tornare a CQ
        m_nTx73 = 0;
        m_txRetryCount = 0;
        m_lastNtx = -1;
        m_lastCqPidx = -1;
        clearAutoCqPartnerLock();
        setDxCall(QString());   // triggers regenerateTxMessages → aggiorna TX6 con callsign corretto
        setDxGrid(QString());
        if (m_multiAnswerMode && !m_callerQueue.isEmpty()) {
            // Multi-answer mode: prende il prossimo dalla coda (mainwindow: processNextInQueue)
            processNextInQueue();
        } else if (m_autoCqRepeat) {
            // Torna a CQ (mainwindow: stopTx → autoCQ)
            advanceQsoState(6);  // CQ → CALLING
            setTxEnabled(true);
        }
        return;
    } else if (last.compare("RR73", Qt::CaseInsensitive) == 0 ||
               last.compare("RRR",  Qt::CaseInsensitive) == 0) {
        bridgeLog("autoSeq: ricevuto RR73 → TX5 (73)");
        // FT2 async: aggiungi al cooldown (Shannon: addCallerToCooldown quando riceve RR73)
        if (m_mode == "FT2" && m_asyncTxEnabled && m_qsoProgress >= 2 && !from.isEmpty())
            m_qsoCooldown[from] = QDateTime::currentMSecsSinceEpoch();
        nextTx = 5;
    } else if (last.length() >= 2 && last[0] == 'R' &&
               (last[1] == '-' || last[1] == '+' || last[1].isDigit())) {
        bridgeLog("autoSeq: ricevuto R+report → TX4 (RR73)");
        nextTx = 4;
    } else if (last.length() >= 2 &&
               (last[0] == '-' || last[0] == '+') && last[1].isDigit()) {
        bridgeLog("autoSeq: ricevuto report " + last + " → TX3 (R+report)");
        // Salva il report ricevuto dal partner → usato in TX3 come "R<report>" (mainwindow behavior)
        setReportReceived(last);
        // Rigenera TX3 con il report appena ricevuto
        if (!m_dxCall.isEmpty()) genStdMsgs(m_dxCall, m_dxGrid);
        nextTx = 3;
    } else if (isGridTokenStrict(last)) {
        // Shannon (riga 11793-11815): grid ricevuto
        // - Stazione nuova O grid nuovo → TX2 (invia il nostro report)
        // - Stessa stazione con stesso grid già noto (ri-trasmissione) → TX3 (R+report, saltiamo TX2)
        bool sameGrid = (!m_dxGrid.isEmpty() &&
                         last.compare(m_dxGrid, Qt::CaseInsensitive) == 0 &&
                         from.compare(m_dxCall, Qt::CaseInsensitive) == 0 &&
                         m_qsoProgress >= 3);  // già in REPORT state (enum 3=REPORT)
        if (sameGrid) {
            bridgeLog("autoSeq: stesso grid da " + from + " → TX3 (salto TX2)");
            nextTx = 3;
        } else {
            bridgeLog("autoSeq: grid da " + from + " → TX2 (report)");
            if (from.compare(m_dxCall, Qt::CaseInsensitive) != 0) {
                setDxCall(from);
            }
            setDxGrid(last);
            genStdMsgs(from, last);
            nextTx = 2;
        }
    } else {
        return;
    }

    if (nextTx > 0) {
        // Deduplicazione FT2 async: stesso step già in corso → ignora (evita loop)
        if (nextTx == m_currentTx && m_txRetryCount > 0) {
            bridgeLog("autoSeq: nextTx=" + QString::number(nextTx) +
                      " già corrente con retry=" + QString::number(m_txRetryCount) + ", skip");
            return;
        }
        // Shannon: avanza stato + resetta retry counter + watchdog
        advanceQsoState(nextTx);
        m_txRetryCount = 0;
        // Shannon (linea 9681): tx_watchdog(false) → reset watchdog quando risposta valida
        m_txWatchdogTicks = 0;
        // Resetta guard FT2: risposta ricevuta → ritrasmetti subito
        m_asyncLastTxEndMs = 0;
        // Shannon: auto_tx_mode(true) — abilita TX dopo risposta valida da DX
        setTxEnabled(true);
        updateAutoCqPartnerLock();
        bridgeLog("autoSeq: TX" + QString::number(nextTx) + " abilitato");
    }
}

void DecodiumBridge::resetLedStatus()
{
    m_ledCoherentAveraging = false;
    m_ledNeuralSync = false;
    m_ledTurboFeedback = false;
    m_coherentCount = 0;
    emit ledCoherentAveragingChanged();
    emit ledNeuralSyncChanged();
    emit ledTurboFeedbackChanged();
    emit coherentCountChanged();
}

void DecodiumBridge::increaseFontScale()
{
    setFontScale(m_fontScale + 0.1);
}

void DecodiumBridge::decreaseFontScale()
{
    setFontScale(m_fontScale - 0.1);
}

void DecodiumBridge::setFontScale(double s)
{
    double clamped = qBound(0.5, s, 2.0);
    if (!qFuzzyCompare(m_fontScale, clamped)) {
        m_fontScale = clamped;
        emit fontScaleChanged();
    }
}

void DecodiumBridge::loadSettings()
{
    QSettings s("Decodium", "Decodium3");
    m_callsign = s.value("callsign", "IU8LMC").toString();
    m_grid     = s.value("grid", "AA00").toString();
    m_frequency = s.value("frequency", 14074000.0).toDouble();
    m_mode     = s.value("mode", "FT8").toString();
    m_audioInputDevice  = s.value("audioInputDevice", "").toString();
    m_audioOutputDevice = s.value("audioOutputDevice", "").toString();
    m_audioInputChannel = s.value("audioInputChannel", 0).toInt();
    m_audioOutputChannel = s.value("audioOutputChannel", 0).toInt();
    m_txOutputLevel = qBound(0.0, s.value("txOutputLevel", 0.0).toDouble(), 450.0);
    m_nfa      = s.value("nfa", 200).toInt();
    m_nfb      = s.value("nfb", 4000).toInt();
    m_ndepth   = s.value("ndepth", 1).toInt();
    m_dxCall   = s.value("dxCall", "").toString();
    m_dxGrid   = s.value("dxGrid", "").toString();
    m_tx1 = s.value("tx1", "").toString();
    m_tx2 = s.value("tx2", "").toString();
    m_tx3 = s.value("tx3", "").toString();
    m_tx4 = s.value("tx4", "").toString();
    m_tx5 = s.value("tx5", "").toString();
    m_tx6 = s.value("tx6", "").toString();
    m_rxFrequency = s.value("rxFrequency", 1500).toInt();
    m_txFrequency = qBound(0, s.value("txFrequency", 1500).toInt(), 3000);
    m_fontScale   = s.value("fontScale", 1.0).toDouble();
    // extra features
    m_alertOnCq        = s.value("alertOnCq",       false).toBool();
    m_alertOnMyCall    = s.value("alertOnMyCall",    false).toBool();
    m_recordRxEnabled  = s.value("recordRxEnabled",  false).toBool();
    m_recordTxEnabled  = s.value("recordTxEnabled",  false).toBool();
    m_stationName      = s.value("StationName",     QString()).toString();
    m_stationQth       = s.value("StationQTH",      QString()).toString();
    m_stationRigInfo   = s.value("StationRigInfo",  QString()).toString();
    m_stationAntenna   = s.value("StationAntenna",  QString()).toString();
    m_stationPowerWatts= qBound(0, s.value("StationPowerW", 100).toInt(), 9999);
    m_autoStartMonitorOnStartup = !s.value("MonitorOFF", false).toBool();
    m_startFromTx2     = s.value("startFromTx2",     false).toBool();
    m_vhfUhfFeatures   = s.value("vhfUhfFeatures",   false).toBool();
    m_directLogQso     = s.value("directLogQso",      false).toBool();
    m_confirm73        = s.value("confirm73",          true).toBool();
    m_contestExchange  = s.value("contestExchange",   "").toString();
    m_contestNumber    = s.value("contestNumber",      1).toInt();
    m_swlMode          = s.value("swlMode",           false).toBool();
    m_splitMode        = s.value("splitMode",         false).toBool();
    m_txWatchdogMode   = s.value("txWatchdogMode",     0).toInt();
    m_txWatchdogTime   = s.value("txWatchdogTime",     5).toInt();
    m_txWatchdogCount  = s.value("txWatchdogCount",    3).toInt();
    m_filterCqOnly     = s.value("filterCqOnly",      false).toBool();
    m_filterMyCallOnly = s.value("filterMyCallOnly",  false).toBool();
    m_contestType      = s.value("contestType",        0).toInt();
    m_zapEnabled       = s.value("zapEnabled",        false).toBool();
    m_deepSearchEnabled= s.value("deepSearchEnabled", false).toBool();
    m_asyncDecodeEnabled=s.value("asyncDecodeEnabled",false).toBool();
    m_pskReporterEnabled=s.value("pskReporterEnabled",false).toBool();
    m_catBackend        =s.value("catBackend",        "native").toString();
    // TX QSO options
    m_reportReceived    =s.value("reportReceived",    "-10").toString();
    m_sendRR73          =s.value("sendRR73",           true).toBool();
    m_txPeriod          =s.value("txPeriod",            0).toInt() != 0 ? 1 : 0;
    m_alt12Enabled      =s.value("alt12Enabled",     false).toBool();
    // B7 — Colors
    m_colorCQ       = s.value("colorCQ",        "#33FF33").toString();
    m_colorMyCall   = s.value("colorMyCall",     "#FF5555").toString();
    m_colorDXEntity = s.value("colorDXEntity",   "#FFAA33").toString();
    m_color73       = s.value("color73",         "#5599FF").toString();
    m_colorB4       = s.value("colorB4",         "#888888").toString();
    m_b4Strikethrough= s.value("b4Strikethrough",  true).toBool();
    // B8 — Alerts
    m_alertSoundsEnabled = s.value("alertSoundsEnabled", false).toBool();
    // Cloudlog
    m_cloudlogEnabled  = s.value("cloudlogEnabled",  false).toBool();
    m_cloudlogUrl      = s.value("cloudlogUrl",      QString()).toString();
    m_cloudlogApiKey   = s.value("cloudlogApiKey",   QString()).toString();
    if (m_cloudlog) {
        m_cloudlog->setEnabled(m_cloudlogEnabled);
        m_cloudlog->setApiUrl(m_cloudlogUrl);
        m_cloudlog->setApiKey(m_cloudlogApiKey);
    }
    // LotW
    m_lotwEnabled = s.value("lotwEnabled", false).toBool();
    // WSPR upload
    m_wsprUploadEnabled = s.value("wsprUploadEnabled", false).toBool();
    if (m_wsprUploader) m_wsprUploader->setEnabled(m_wsprUploadEnabled);
    // DX Cluster
    if (m_dxCluster) m_dxCluster->loadSettings();
    // UI state
    m_uiSpectrumHeight  = s.value("uiSpectrumHeight",  150).toInt();
    m_uiPaletteIndex    = s.value("uiPaletteIndex",    3).toInt();
    m_uiZoomFactor      = s.value("uiZoomFactor",      1.0).toDouble();
    m_uiWaterfallHeight = s.value("uiWaterfallHeight", 350).toInt();
    m_uiDecodeWinX      = s.value("uiDecodeWinX",      0).toInt();
    m_uiDecodeWinY      = s.value("uiDecodeWinY",      0).toInt();
    m_uiDecodeWinWidth  = s.value("uiDecodeWinWidth",  1100).toInt();
    m_uiDecodeWinHeight = s.value("uiDecodeWinHeight", 600).toInt();
    // PSK Reporter — aggiorna stazione locale con callsign/grid caricati
    if (m_pskReporter) {
        m_pskReporter->setEnabled(m_pskReporterEnabled);
        m_pskReporter->setLocalStation(m_callsign, m_grid);
    }
    // Garantisce che TX6 (CQ) sia sempre valorizzato dopo il caricamento settings
    regenerateTxMessages();
}

QVariantMap DecodiumBridge::loadWindowState(const QString& key) const
{
    QVariantMap state;
    QString const normalizedKey = key.trimmed();
    if (normalizedKey.isEmpty()) {
        return state;
    }

    QSettings s("Decodium", "Decodium3");
    s.beginGroup(QStringLiteral("WindowState/%1").arg(normalizedKey));
    auto const maybeInsertInt = [&] (const char* field) {
        if (s.contains(QLatin1String(field))) {
            state.insert(QLatin1String(field), s.value(QLatin1String(field)).toInt());
        }
    };
    auto const maybeInsertBool = [&] (const char* field) {
        if (s.contains(QLatin1String(field))) {
            state.insert(QLatin1String(field), s.value(QLatin1String(field)).toBool());
        }
    };

    maybeInsertInt("x");
    maybeInsertInt("y");
    maybeInsertInt("width");
    maybeInsertInt("height");
    maybeInsertBool("detached");
    maybeInsertBool("minimized");
    s.endGroup();
    return state;
}

void DecodiumBridge::saveWindowState(const QString& key,
                                     int x,
                                     int y,
                                     int width,
                                     int height,
                                     bool detached,
                                     bool minimized)
{
    QString const normalizedKey = key.trimmed();
    if (normalizedKey.isEmpty()) {
        return;
    }

    QSettings s("Decodium", "Decodium3");
    s.beginGroup(QStringLiteral("WindowState/%1").arg(normalizedKey));
    s.setValue(QStringLiteral("x"), x);
    s.setValue(QStringLiteral("y"), y);
    s.setValue(QStringLiteral("width"), width);
    s.setValue(QStringLiteral("height"), height);
    s.setValue(QStringLiteral("detached"), detached);
    s.setValue(QStringLiteral("minimized"), minimized);
    s.endGroup();
}

QString DecodiumBridge::version() const
{
    return "3.0.0";
}

QString DecodiumBridge::effectiveAdifLogPath() const
{
    if (usingLegacyBackendForTx()) {
        QString const legacyPath = m_legacyBackend->adifLogPath().trimmed();
        if (!legacyPath.isEmpty()) {
            return legacyPath;
        }
    }
    if (!m_adifLogPath.trimmed().isEmpty()) {
        return m_adifLogPath;
    }
    // Usa la directory legacy (AppData\Local\Decodium) se il log esiste gia' li'
    QString const legacyDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/Decodium/decodium_log.adi");
    if (QFile::exists(legacyDir)) {
        return legacyDir;
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/decodium_log.adi");
}

QString DecodiumBridge::ensureAdifLogPath()
{
    QString const resolved = effectiveAdifLogPath();
    if (m_adifLogPath != resolved) {
        m_adifLogPath = resolved;
    }
    return m_adifLogPath;
}

void DecodiumBridge::logQso()
{
    if (usingLegacyBackendForTx()) {
        bridgeLog("logQso: delegating to legacy backend");
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->logQso();
        syncLegacyBackendState();
        QTimer::singleShot(250, this, [this]() {
            emit qsoCountChanged();
            emit workedCountChanged();
        });
        emit statusMessage("Log QSO via backend legacy");
        return;
    }

    QString logDxCall = m_dxCall.trimmed();
    QString logDxGrid = m_dxGrid.trimmed();
    QString logRptSent = m_reportSent.trimmed();
    QString logRptRcvd = m_reportReceived.trimmed();
    double  logFreqHz = m_frequency;
    QDateTime utcNow = QDateTime::currentDateTimeUtc();

    if (m_pendingAutoLogValid) {
        if (!m_pendingAutoLogCall.isEmpty()) logDxCall = m_pendingAutoLogCall;
        if (!m_pendingAutoLogGrid.isEmpty()) logDxGrid = m_pendingAutoLogGrid;
        if (!m_pendingAutoLogRptSent.isEmpty()) logRptSent = m_pendingAutoLogRptSent;
        if (!m_pendingAutoLogRptRcvd.isEmpty()) logRptRcvd = m_pendingAutoLogRptRcvd;
        if (m_pendingAutoLogDialFreq > 0.0) logFreqHz = m_pendingAutoLogDialFreq;
        if (m_pendingAutoLogOn.isValid()) utcNow = m_pendingAutoLogOn;
    }

    if (logDxCall.isEmpty() && m_lateAutoLogValid && m_lateAutoLogExpires >= QDateTime::currentDateTimeUtc()) {
        logDxCall = m_lateAutoLogCall;
        logDxGrid = m_lateAutoLogGrid;
        logRptSent = m_lateAutoLogRptSent;
        logRptRcvd = m_lateAutoLogRptRcvd;
        if (m_lateAutoLogDialFreq > 0.0) logFreqHz = m_lateAutoLogDialFreq;
        if (m_lateAutoLogOn.isValid()) utcNow = m_lateAutoLogOn;
    }

    if (logDxCall.isEmpty()) {
        return;
    }

    QString const dedupeCall = Radio::base_callsign(logDxCall).trimmed().toUpper();
    QString const dedupeBand = autoCqBandKeyForFrequency(logFreqHz).trimmed().toUpper();
    QString const dedupeMode = m_mode.trimmed().toUpper();
    QString const dedupeKey = QStringLiteral("%1|%2|%3").arg(dedupeCall, dedupeBand, dedupeMode);
    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    bool suppressDuplicateLog = false;
    if (!dedupeCall.isEmpty() && !dedupeBand.isEmpty() && dedupeBand != QStringLiteral("--") && !dedupeMode.isEmpty()) {
        auto const it = m_recentQsoLogUtcByKey.constFind(dedupeKey);
        if (it != m_recentQsoLogUtcByKey.constEnd()) {
            suppressDuplicateLog = isWithinRecentDuplicateWindow(it.value(), nowUtc);
        }
        if (!suppressDuplicateLog) {
            m_recentQsoLogUtcByKey.insert(dedupeKey, nowUtc);
        }
        pruneRecentDuplicateCache(m_recentQsoLogUtcByKey, nowUtc);
    }

    if (suppressDuplicateLog) {
        emit statusMessage(QStringLiteral("Duplicate log suppressed for %1").arg(logDxCall));
        clearPendingAutoLogSnapshot();
        return;
    }

    // 1) Log custom (all.txt) — compatibilità esistente
    QString logPath = logAllTxtPath();
    QDir().mkpath(QFileInfo(logPath).absolutePath());
    QFile f(logPath);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream s(&f);
        s << utcNow.toString("yyyyMMdd_hhmmss")
          << " " << m_callsign
          << " " << logDxCall
          << " " << logDxGrid
          << " " << QString::number(logFreqHz / 1e6, 'f', 6) << "MHz"
          << " " << m_mode
          << "\n";
    }

    // 2) Log ADIF (decodium_log.adi) — per import/export e B4 check
    appendAdifRecord(logDxCall, logDxGrid, logFreqHz, m_mode, utcNow,
                     logRptSent, logRptRcvd);

    // 3) Cloudlog upload
    bool snrOk = false;
    int const snr = logRptSent.toInt(&snrOk);
    if (m_cloudlogEnabled && m_cloudlog) {
        m_cloudlog->logQso(logDxCall, logDxGrid, logFreqHz, m_mode, utcNow,
                           snrOk ? snr : 0, logRptSent, logRptRcvd,
                           m_callsign, m_grid);
    }

    rememberRecentAutoCqWorked(logDxCall, logFreqHz, m_mode);
    removeCallerFromQueue(dedupeCall);
    if (m_lateAutoLogValid &&
        dedupeCall == Radio::base_callsign(m_lateAutoLogCall).trimmed().toUpper()) {
        clearLateAutoLogSnapshot();
    }
    clearPendingAutoLogSnapshot();
    emit statusMessage("QSO loggato: " + logDxCall);
}

QString DecodiumBridge::logAllTxtPath() const
{
    // Cartella standard: %APPDATA%\Decodium\all.txt (Windows) o ~/.local/share/Decodium/all.txt
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return path + "/all.txt";
}

// === Shannon FDR (False Decode Reduction) — 4-level filter ===
static bool isValidDecode(const QString& msg, int snr, const QString& aptype, float quality)
{
    // Shannon FDR Step 1: blocca decode con pattern invalidi
    if (msg.contains("> <") || msg.contains("< >")) return false;

    // Blocca messaggi troppo corti
    if (msg.trimmed().length() < 3) return false;

    // Shannon FDR Step 2: soglie SNR per tipo messaggio
    // AP decode con qualità bassa e SNR molto basso → probabilmente falso
    bool hasAP = !aptype.isEmpty() && aptype != "  ";
    bool isLowConfidence = msg.endsWith('?');

    if (hasAP && isLowConfidence && snr < -24) return false;
    if (hasAP && quality < 0.17f && snr < -21) return false;

    // Shannon FDR Step 3: filtro specifico FT2 low confidence
    if (isLowConfidence && snr < -26) return false;

    // Shannon FDR Step 4: validazione callsign base
    // Estrai le parti del messaggio e verifica che ci sia almeno un callsign valido
    QStringList parts = msg.split(' ', Qt::SkipEmptyParts);
    if (parts.size() < 2) return false;

    // Un callsign valido: almeno 3 caratteri, contiene almeno una lettera e un numero
    auto isCallsignLike = [](const QString& s) -> bool {
        if (s.length() < 3 || s.length() > 10) return false;
        if (s == "CQ" || s == "QRZ" || s == "DE" || s == "DX" ||
            s == "RR73" || s == "RRR" || s == "73") return true;
        bool hasLetter = false, hasDigit = false;
        for (const QChar& c : s) {
            if (c.isLetter()) hasLetter = true;
            if (c.isDigit()) hasDigit = true;
        }
        return hasLetter && hasDigit;
    };

    // Almeno una parte deve sembrare un callsign
    bool foundCall = false;
    for (const QString& p : parts) {
        if (isCallsignLike(p)) { foundCall = true; break; }
    }
    if (!foundCall) return false;

    return true;  // decode valido
}

namespace {

bool isGridToken(const QString& token)
{
    QString t = token.trimmed().toUpper();
    if (t == "RR73")
        return false;
    if (t.size() != 4 && t.size() != 6)
        return false;
    if (!t[0].isLetter() || !t[1].isLetter() || !t[2].isDigit() || !t[3].isDigit())
        return false;
    if (t.size() == 6 && (!t[4].isLetter() || !t[5].isLetter()))
        return false;
    return true;
}

bool looksLikeCallsignToken(const QString& token)
{
    QString t = token.trimmed().toUpper();
    if (t.size() < 3 || t.size() > 15 || isGridToken(t))
        return false;

    bool hasLetter = false;
    bool hasDigit = false;
    for (const QChar& ch : t) {
        if (ch.isLetter()) {
            hasLetter = true;
        } else if (ch.isDigit()) {
            hasDigit = true;
        } else if (ch != '/' && ch != '-') {
            return false;
        }
    }

    return hasLetter && hasDigit;
}

}

QString DecodiumBridge::extractDecodedCallsign(const QString& msg, bool isCQ) const
{
    QStringList parts = msg.toUpper().split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return {};

    if (isCQ) {
        for (int i = 1; i < parts.size(); ++i) {
            if (looksLikeCallsignToken(parts[i]))
                return parts[i];
        }
        return {};
    }

    QString myCallUpper = m_callsign.trimmed().toUpper();
    if (!myCallUpper.isEmpty()) {
        for (int i = 0; i < parts.size(); ++i) {
            if (parts[i] != myCallUpper)
                continue;
            for (int j = i + 1; j < parts.size(); ++j) {
                if (looksLikeCallsignToken(parts[j]))
                    return parts[j];
            }
            for (int j = i - 1; j >= 0; --j) {
                if (looksLikeCallsignToken(parts[j]))
                    return parts[j];
            }
            break;
        }
    }

    for (const QString& part : parts) {
        if (looksLikeCallsignToken(part))
            return part;
    }
    return {};
}

QString DecodiumBridge::extractDecodedGrid(const QString& msg) const
{
    QStringList parts = msg.split(' ', Qt::SkipEmptyParts);
    for (int pi = parts.size() - 1; pi >= 0; --pi) {
        if (isGridToken(parts[pi]))
            return parts[pi].toUpper();
    }
    return {};
}

void DecodiumBridge::enrichDecodeEntry(QVariantMap& entry) const
{
    QString msg = entry.value("message").toString();
    bool isCQ = entry.value("isCQ").toBool();
    QString fromCall = entry.value("fromCall").toString().trimmed().toUpper();
    if (fromCall.isEmpty())
        fromCall = extractDecodedCallsign(msg, isCQ);

    bool isB4 = !fromCall.isEmpty() && m_workedCalls.contains(fromCall);
    bool isLotw = m_lotwEnabled && !fromCall.isEmpty() && m_lotwUsers.contains(fromCall);

    QString dxCountry;
    QString dxContinent;
    QString dxPrefix;
    bool dxIsNewCountry = false;
    if (m_dxccLookup && m_dxccLookup->isLoaded() && !fromCall.isEmpty()) {
        DxccEntity ent = m_dxccLookup->lookup(fromCall);
        if (ent.isValid()) {
            dxCountry = ent.name;
            dxContinent = ent.continent;
            dxPrefix = ent.prefix;
        }
    }

    QString dxGridExtracted = extractDecodedGrid(msg);
    double distKm = -1.0;
    double bearing = -1.0;
    if (!dxGridExtracted.isEmpty() && !m_grid.isEmpty()) {
        bearing = calcBearing(m_grid, dxGridExtracted);
        distKm = calcDistance(m_grid, dxGridExtracted);
    }

    entry["fromCall"] = fromCall;
    entry["isB4"] = isB4;
    entry["isLotw"] = isLotw;
    entry["dxCountry"] = dxCountry;
    entry["dxCallsign"] = fromCall;
    entry["dxContinent"] = dxContinent;
    entry["dxPrefix"] = dxPrefix;
    entry["dxIsWorked"] = isB4;
    entry["dxIsNewBand"] = entry.value("dxIsNewBand", false);
    entry["dxIsNewCountry"] = dxIsNewCountry;
    entry["dxIsMostWanted"] = entry.value("dxIsMostWanted", false);
    entry["dxBearing"] = bearing;
    entry["dxDistance"] = distKm;
    entry["dxGrid"] = dxGridExtracted;
}

void DecodiumBridge::refreshDecodeListDxcc()
{
    bool changed = false;
    for (int i = 0; i < m_decodeList.size(); ++i) {
        QVariantMap entry = m_decodeList[i].toMap();
        if (entry.value("isTx").toBool())
            continue;
        QVariantMap original = entry;
        enrichDecodeEntry(entry);
        if (entry != original) {
            m_decodeList[i] = entry;
            changed = true;
        }
    }

    if (changed)
        emit decodeListChanged();
}

// === PRIVATE SLOTS ===

void DecodiumBridge::onFt8DecodeReady(quint64 serial, QStringList rows)
{
    bridgeLog("onFt8DecodeReady: serial=" + QString::number(serial) +
              " rows=" + QString::number(rows.size()));
    for (int dbgI = 0; dbgI < qMin(rows.size(), 3); ++dbgI)
        bridgeLog("  raw[" + QString::number(dbgI) + "]='" + rows[dbgI] + "'");

    if (m_dxccLookup && !m_dxccLookup->isLoaded()) {
        QString loadedPath;
        if (reloadDxccLookup(&loadedPath)) {
            bridgeLog("DXCC: caricato on-demand da " + loadedPath);
            refreshDecodeListDxcc();
        }
    }

    Q_UNUSED(serial)
    bool changed = false;
    for (const auto& row : rows) {
        // parseFt8Row returns: [time, snr, dt, df, message, aptype, quality, freq]
        QStringList f = parseFt8Row(row);
        if (f.size() < 8) {
            bridgeLog("  parseFt8Row FAILED for: '" + row + "'");
            continue;
        }

        // FDR: Shannon False Decode Reduction — 4 livelli di filtro
        {
            int snrVal = f[1].trimmed().toInt();
            float qualVal = f[6].trimmed().toFloat();
            if (!isValidDecode(f[4], snrVal, f[5], qualVal)) {
                bridgeLog("FDR: filtered decode: " + f[4]);
                continue;  // salta questo decode
            }
        }

        // Calcola proprietà derivate
        QString msg     = f[4];
        bool isCQ       = msg.startsWith("CQ ", Qt::CaseInsensitive) || msg == "CQ";
        bool isMyCall   = !m_callsign.isEmpty() && msg.contains(m_callsign, Qt::CaseInsensitive);
        QString fromCall = extractDecodedCallsign(msg, isCQ);

        QVariantMap entry;
        entry["time"]    = f[0];
        entry["db"]      = f[1];
        entry["dt"]      = f[2];
        entry["freq"]    = f[7];   // frequenza assoluta audio Hz
        entry["message"] = msg;
        entry["aptype"]  = f[5];
        entry["quality"] = f[6];
        entry["isTx"]    = false;
        entry["isCQ"]    = isCQ;
        entry["isMyCall"] = isMyCall;
        entry["fromCall"] = fromCall;
        enrichDecodeEntry(entry);

        // B9 — Feed ActiveStationsModel: extract callsign from CQ messages
        if (isCQ && m_activeStations) {
            QString dxGridExtracted = entry.value("dxGrid").toString();
            if (!fromCall.isEmpty()) {
                int freqHz = f[7].toInt();
                QString utc = f[0];
                int snr = f[1].toInt();
                m_activeStations->addStation(fromCall, freqHz, snr, dxGridExtracted, utc);
            }
        }

        // Shannon write_all(): logga OGNI decode in all.txt
        {
            QString allPath = logAllTxtPath();
            QDir().mkpath(QFileInfo(allPath).absolutePath());
            QFile allFile(allPath);
            if (allFile.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream ts(&allFile);
                // Formato Shannon: yyMMdd_hhmmss  freq_MHz  Rx  MODE  SNR  DT  DF  Message
                QString utcNow = QDateTime::currentDateTimeUtc().toString("yyMMdd_hhmmss");
                double freqMhz = m_frequency / 1e6;
                ts << utcNow
                   << QString("  %1").arg(freqMhz, 10, 'f', 3)
                   << "  Rx  "
                   << QString("%1").arg(m_mode, -6)
                   << QString("  %1").arg(f[1].trimmed(), 4)   // SNR
                   << QString("  %1").arg(f[2].trimmed(), 5)    // DT
                   << QString("  %1").arg(f[7].trimmed(), 5)    // DF (freq Hz)
                   << "  " << msg
                   << "\n";
            }
        }

        m_decodeList.append(QVariant(entry));
        changed = true;

        // PSK Reporter: invia spot se abilitato
        if (m_pskReporterEnabled && m_pskReporter && !m_callsign.isEmpty()) {
            QString dxGridExtracted = entry.value("dxGrid").toString();
            // Estrai callsign dal messaggio: "CQ [MOD] CALL [GRID]" o "TO FROM ..."
            QString spCall = extractDecodedCallsign(msg, isCQ);
            if (!spCall.isEmpty() && spCall != m_callsign) {
                quint64 absFreqHz = static_cast<quint64>(m_frequency) + f[7].toULongLong();
                m_pskReporter->addSpot(spCall, dxGridExtracted, absFreqHz, m_mode, f[1].toInt());
            }
        }

        // B8 — Alert sounds
        if (m_alertSoundsEnabled) {
            if (isCQ && m_alertOnCq)         playAlert("CQ");
            else if (isMyCall && m_alertOnMyCall) playAlert("MyCall");
        }

        // C17 — Fox OTP: verifica codice se SuperFox mode attivo
        // Formato messaggio Fox: "CALL OTP" dove OTP è numerico a 6 cifre
        if (m_foxMode && !isMyCall) {
            QStringList parts = msg.split(' ', Qt::SkipEmptyParts);
            if (parts.size() == 2) {
                const QString& possibleCode = parts[1];
                if (possibleCode.length() == 6 && possibleCode[0].isDigit()) {
                    int audioHz = f.size() > 7 ? f[7].toInt() : 750;
                    verifyOtp(parts[0], possibleCode, audioHz);
                }
            }
        }
    }
    // Shannon: max 5000 righe per lista decode (documento().setMaximumBlockCount(5000))
    if (m_decodeList.size() > 5000)
        m_decodeList = m_decodeList.mid(m_decodeList.size() - 5000);

    // Shannon (linea 9699): auto-seq attiva se m_auto=true e cbAutoSeq checked
    // Processa qualsiasi messaggio che contiene il nostro callsign — identico a Shannon
    // (Shannon condizione: "message contains my_call" OR "calling CQ && m_bAutoReply")
    bool autoSeqActive = !m_callsign.isEmpty() &&
                         ((m_autoSeq && (m_txEnabled || !m_dxCall.isEmpty()))
                          || m_autoCqRepeat);
    if (autoSeqActive) {
        for (const auto& row : rows) {
            QStringList f = parseFt8Row(row);
            if (f.size() < 5) continue;
            QString msgText = f[4];
            if (msgText.contains(m_callsign, Qt::CaseInsensitive))
                autoSequenceStep(f);
        }
    }

    if (changed) emit decodeListChanged();
    m_decoding = false;
    emit decodingChanged();
}

void DecodiumBridge::onFt2DecodeReady(quint64 serial, QStringList rows)
{
    onFt8DecodeReady(serial, rows); // stessa logica di parsing
}

void DecodiumBridge::onFt2AsyncDecodeReady(QStringList rows)
{
    // Path turbo async: stessa logica di onFt8DecodeReady ma senza serial.
    // Deduplica per messaggio: non aggiunge righe già presenti nella lista.
    m_asyncDecodePending = false;
    bridgeLog("onFt2AsyncDecodeReady: rows=" + QString::number(rows.size()));
    if (!rows.isEmpty()) bridgeLog("  async_raw[0]='" + rows[0] + "'");

    if (m_dxccLookup && !m_dxccLookup->isLoaded()) {
        QString loadedPath;
        if (reloadDxccLookup(&loadedPath)) {
            bridgeLog("DXCC: caricato on-demand da " + loadedPath);
            refreshDecodeListDxcc();
        }
    }

    // Costruisci set di messaggi già presenti per deduplicazione O(1)
    QSet<QString> existing;
    for (const auto& v : m_decodeList) {
        QString msg = v.toMap().value("message").toString();
        if (!msg.isEmpty()) existing.insert(msg);
    }

    bool changed = false;
    int bestSnr = -99;
    for (const auto& row : rows) {
        QStringList f = parseFt8Row(row);
        if (f.size() < 8) {
            bridgeLog("  async parseFt8Row FAILED: '" + row + "'");
            continue;
        }
        QString msg = f[4];
        // Aggiorna il miglior SNR (per S-meter del AsyncModeWidget)
        bool snrOk = false;
        int snrVal = f[1].toInt(&snrOk);
        if (snrOk && snrVal > bestSnr) bestSnr = snrVal;

        if (existing.contains(msg)) continue;   // già presente, salta
        existing.insert(msg);

        bool isCQ     = msg.startsWith("CQ ", Qt::CaseInsensitive) || msg == "CQ";
        bool isMyCall = !m_callsign.isEmpty() && msg.contains(m_callsign, Qt::CaseInsensitive);
        QVariantMap entry;
        entry["time"]    = f[0];
        entry["db"]      = f[1];
        entry["dt"]      = f[2];
        entry["freq"]    = f[7];
        entry["message"] = msg;
        entry["aptype"]  = f[5];
        entry["quality"] = f[6];
        entry["isTx"]    = false;
        entry["isCQ"]    = isCQ;
        entry["isMyCall"] = isMyCall;
        entry["fromCall"] = extractDecodedCallsign(msg, isCQ);
        enrichDecodeEntry(entry);
        m_decodeList.append(QVariant(entry));
        changed = true;
    }
    if (changed) emit decodeListChanged();
    if (bestSnr > -99) setAsyncSnrDb(bestSnr);

    // Auto-sequenza FT2 async: identico a onFt8DecodeReady
    // Shannon cbAsyncDecode: quando arriva un decode con il nostro callsign,
    // elabora auto-seq e avvia TX IMMEDIATAMENTE senza aspettare il boundary del periodo.
    // IMPORTANTE: checkAndStartPeriodicTx() solo se gotResponse=true.
    // Il CQ periodico è gestito dal boundary del periodo — chiamarlo qui
    // incondizionatamente causerebbe loop CQ ogni ~100ms.
    bool autoSeqActive = !m_callsign.isEmpty() &&
                         ((m_autoSeq && (m_txEnabled || !m_dxCall.isEmpty()))
                          || m_autoCqRepeat);
    if (autoSeqActive) {
        bool gotResponse = false;
        for (const auto& row : rows) {
            QStringList f = parseFt8Row(row);
            if (f.size() < 5) continue;
            if (f[4].contains(m_callsign, Qt::CaseInsensitive)) {
                autoSequenceStep(f);
                gotResponse = true;
            }
        }
        // Avvia TX immediatamente solo se abbiamo ricevuto una risposta valida
        if (gotResponse && !m_transmitting)
            checkAndStartPeriodicTx();
    }
}

void DecodiumBridge::onAsyncDecodeTimer()
{
    if (m_mode != "FT2" || !m_monitoring || m_asyncDecodePending) return;
    if (m_asyncAudioPos < 45000) return;  // non abbastanza audio ancora

    decodium::ft2::AsyncDecodeRequest req;
    req.audio.resize(45000);
    int pos   = m_asyncAudioPos;
    int start = (pos - 45000 + ASYNC_BUF_SIZE) % ASYNC_BUF_SIZE;
    for (int i = 0; i < 45000; ++i)
        req.audio[i] = m_asyncAudio[(start + i) % ASYNC_BUF_SIZE];

    int nfqso = (m_nfa + m_nfb) / 2;
    req.nqsoprogress = 0;
    req.nfqso = nfqso;
    req.nfa   = m_nfa;
    req.nfb   = m_nfb;
    req.ndepth   = m_ndepth;
    req.ncontest = m_ncontest;
    req.mycall   = m_callsign.toLocal8Bit();
    req.hiscall  = m_dxCall.toLocal8Bit();

    m_asyncDecodePending = true;
    auto* worker = m_ft2Worker;
    QMetaObject::invokeMethod(m_ft2Worker, [worker, req]() {
        worker->decodeAsync(req);
    }, Qt::QueuedConnection);
}

void DecodiumBridge::onFt4DecodeReady(quint64 serial, QStringList rows)
{
    onFt8DecodeReady(serial, rows);
}

void DecodiumBridge::onQ65DecodeReady(quint64 serial, QStringList rows)
{
    onFt8DecodeReady(serial, rows);   // stesso formato output
}

void DecodiumBridge::onMsk144DecodeReady(quint64 serial, QStringList rows)
{
    onFt8DecodeReady(serial, rows);   // stesso formato output
}

void DecodiumBridge::onWsprDecodeReady(quint64 serial, QStringList rows,
                                        QStringList /*diagnostics*/, int /*exitCode*/)
{
    // WSPR upload spots
    if (m_wsprUploadEnabled && m_wsprUploader && !m_callsign.isEmpty()) {
        m_wsprUploader->setCallsign(m_callsign);
        m_wsprUploader->setGrid(m_grid);
        for (const QString& row : rows) {
            if (!row.trimmed().isEmpty())
                m_wsprUploader->uploadSpot(row.trimmed(), m_frequency);
        }
    }
    onFt8DecodeReady(serial, rows);   // WSPR usa stesso formato di lista decode
}

void DecodiumBridge::onLegacyJtDecodeReady(quint64 serial, QStringList rows)
{
    bridgeLog("onLegacyJtDecodeReady: serial=" + QString::number(serial) +
              " rows=" + QString::number(rows.size()) + " mode=" + m_mode);
    if (m_dxccLookup && !m_dxccLookup->isLoaded()) {
        QString loadedPath;
        if (reloadDxccLookup(&loadedPath)) {
            bridgeLog("DXCC: caricato on-demand da " + loadedPath);
            refreshDecodeListDxcc();
        }
    }
    Q_UNUSED(serial)
    bool changed = false;
    for (const auto& row : rows) {
        QStringList f = parseJt65Row(row);
        if (f.size() < 8) {
            bridgeLog("  parseJt65Row FAILED for: '" + row + "'");
            continue;
        }

        QString msg   = f[4];
        bool isCQ     = msg.startsWith("CQ ", Qt::CaseInsensitive) || msg == "CQ";
        bool isMyCall = !m_callsign.isEmpty() && msg.contains(m_callsign, Qt::CaseInsensitive);
        QString fromCall = extractDecodedCallsign(msg, isCQ);

        QVariantMap entry;
        entry["time"]       = f[0];
        entry["db"]         = f[1];
        entry["dt"]         = f[2];
        entry["freq"]       = f[7];
        entry["message"]    = msg;
        entry["aptype"]     = f[5];
        entry["quality"]    = f[6];
        entry["isTx"]       = false;
        entry["isCQ"]       = isCQ;
        entry["isMyCall"]   = isMyCall;
        entry["fromCall"]   = fromCall;
        enrichDecodeEntry(entry);

        m_decodeList.append(QVariant(entry));
        changed = true;

        // B8 — Alert sounds
        if (m_alertManager) {
            if (isCQ && m_alertOnCq)     m_alertManager->playAlert("cq");
            if (isMyCall && m_alertOnMyCall) m_alertManager->playAlert("mycall");
        }
    }
    if (changed) emit decodeListChanged();
}

void DecodiumBridge::onFst4DecodeReady(quint64 serial, QStringList rows)
{
    bridgeLog("onFst4DecodeReady: serial=" + QString::number(serial) +
              " rows=" + QString::number(rows.size()) + " mode=" + m_mode);
    // FST4 rows have the same format as FT8 (backtick marker), parseFt8Row works
    onFt8DecodeReady(serial, rows);
}

void DecodiumBridge::onPeriodTimer()
{
    m_periodTicks++;
    m_periodProgress = (m_periodTicks * 100) / m_periodTicksMax;
    emit periodProgressChanged();

    // === TX watchdog configurabile (allineato a mainwindow m_txWatchdogMode) ===
    // m_txWatchdogMode: 0=off, 1=time-based (minuti), 2=count-based (periodi)
    if (m_txEnabled || m_transmitting) {
        ++m_txWatchdogTicks;
        bool watchdogFired = false;
        if (m_txWatchdogMode == 1) {
            // Time-based: m_txWatchdogTime minuti × 240 tick/min (250ms × 240 = 60s = 1 min)
            int maxTicks = m_txWatchdogTime * 240;
            if (maxTicks > 0 && m_txWatchdogTicks >= maxTicks) watchdogFired = true;
        } else if (m_txWatchdogMode == 2) {
            // Count-based: m_txWatchdogCount periodi × ticksMax tick/periodo
            int maxTicks = m_txWatchdogCount * m_periodTicksMax;
            if (maxTicks > 0 && m_txWatchdogTicks >= maxTicks) {
                ++m_autoCQPeriodsMissed;
                m_txWatchdogTicks = 0;  // reset per contare il prossimo periodo
                if (m_autoCQPeriodsMissed >= m_txWatchdogCount) watchdogFired = true;
            }
        } else if (m_txWatchdogMode == 0) {
            // Fallback: hard limit 60s (TX_WATCHDOG_MAX = 240 tick @ 250ms)
            if (m_txWatchdogTicks >= TX_WATCHDOG_MAX) watchdogFired = true;
        }
        if (watchdogFired) {
            bridgeLog("TX watchdog: timeout (mode=" + QString::number(m_txWatchdogMode) + ") → halt TX");
            if (m_autoCqRepeat && m_qsoProgress > 1) {
                armLateAutoLogSnapshot();
            }
            m_txWatchdogTicks = 0;
            m_autoCQPeriodsMissed = 0;
            m_txRetryCount = 0;
            m_lastNtx = -1;
            m_nTx73 = 0;
            clearAutoCqPartnerLock();
            halt();
            setTxEnabled(false);
            emit statusMessage("TX watchdog: timeout, TX fermato");
        }
    } else {
        m_txWatchdogTicks = 0;      // reset quando TX non attivo
        m_autoCQPeriodsMissed = 0;
    }

    if (m_periodTicks >= m_periodTicksMax) {
        m_periodTicks = 0;
        m_periodProgress = 0;
        emit periodProgressChanged();
        feedAudioToDecoder();
        // Al boundary del periodo: verifica se avviare TX automatico
        checkAndStartPeriodicTx();
        // B9 — Age active stations every period
        if (m_activeStations) m_activeStations->ageAllStations();

        // A2 — Update soundcard drift every ~5 periods
        static int driftCheckCount = 0;
        if (++driftCheckCount >= 5) {
            driftCheckCount = 0;
            qint64 elapsedMs = m_driftClock.elapsed();
            qint64 expectedMs = (qint64)m_driftExpectedFrames * 1000LL / SAMPLE_RATE;
            if (expectedMs > 0 && elapsedMs > 0) {
                double driftNew = (double)(elapsedMs - expectedMs) / (double)expectedMs * 1e6;
                if (qAbs(driftNew - m_soundcardDriftPpm) > 0.5) {
                    m_soundcardDriftPpm = driftNew;
                    emit soundcardDriftChanged();
                }
            }
        }
    }
}

void DecodiumBridge::onSpectrumTimer()
{
    if (usingLegacyBackendForTx()) return;
    if (!m_monitoring || m_audioBuffer.isEmpty()) return;
    int avail = m_audioBuffer.size();

    // ── Legacy 512-bin per WaterfallItem ────────────────────────────────────
    {
        int start = qMax(0, avail - SPECTRUM_FFT_SIZE);
        m_spectrumBuf = m_audioBuffer.mid(start, SPECTRUM_FFT_SIZE);
        QVector<float> spectrum = computeSpectrum();
        if (!spectrum.isEmpty())
            emit spectrumDataReady(spectrum);
    }

    // ── Alta risoluzione 4096-bin per PanadapterItem ────────────────────────
    if (avail >= PANADAPTER_FFT_SIZE) {
        int start = qMax(0, avail - PANADAPTER_FFT_SIZE);
        m_spectrumBuf = m_audioBuffer.mid(start, PANADAPTER_FFT_SIZE);
        float minDb = 0.f, maxDb = 0.f;
        QVector<float> hq = computePanadapter(minDb, maxDb);
        if (!hq.isEmpty()) {
            // Passa le frequenze esatte usate per i bin — garantisce allineamento display
            float freqPerBin = (float)SAMPLE_RATE / PANADAPTER_FFT_SIZE;
            float freqMinHz = (int)(m_nfa / freqPerBin) * freqPerBin;
            float freqMaxHz = (int)(m_nfb / freqPerBin) * freqPerBin;
            emit panadapterDataReady(hq, minDb, maxDb, freqMinHz, freqMaxHz);
        }
    }
}

void DecodiumBridge::onLegacyWaterfallRow(QByteArray const& rowLevels,
                                          int startFrequencyHz,
                                          int spanHz,
                                          int rxFrequencyHz,
                                          int txFrequencyHz,
                                          QString const& mode)
{
    Q_UNUSED(rxFrequencyHz);
    Q_UNUSED(txFrequencyHz);
    Q_UNUSED(mode);

    if (!usingLegacyBackendForTx() || rowLevels.isEmpty() || spanHz <= 0)
        {
          return;
        }

    QVector<float> dbValues;
    dbValues.reserve(rowLevels.size());

    for (char level : rowLevels)
        {
          float const value = static_cast<float> (static_cast<unsigned char> (level));
          float const norm = value / 254.0f;
          // Legacy waterfall rows are post-gain intensity levels, not real dB.
          // Map them into the same visual dB span used by the QML panadapter
          // so SmartSDR-style auto-range and labels stay coherent.
          dbValues.push_back (-130.0f + norm * 90.0f);
        }

    emit panadapterDataReady (dbValues,
                              -130.0f,
                              -40.0f,
                              static_cast<float> (startFrequencyHz),
                              static_cast<float> (startFrequencyHz + spanHz));
}

void DecodiumBridge::onUtcTimer()
{
    m_utcTime = QDateTime::currentDateTimeUtc().toString("hh:mm:ss");
    emit utcTimeChanged();
}

// === PRIVATE METHODS ===

QStringList DecodiumBridge::parseFt8Row(const QString& row) const
{
    // Actual format produced by FT2DecodeWorker::build_row() / build_rows():
    //
    //   [utcPrefix]SSSS D.D  FFFF ~ <decoded 37 chars> AA
    //
    //   utcPrefix  = right-justified 4 (HHMM) or 6 (HHMMSS) digit string
    //                prepended directly with NO separator — format_decode_utc()
    //                returns "" for async rows (nutc==0) or "HHMM"/"HHMMSS"
    //   SSSS       = SNR right-justified in 4 chars  e.g. " -12" or "  -7"
    //   D.D        = dt  right-justified in 5 chars  e.g. "  1.3"
    //   FFFF       = audio freq Hz right-justified in 5 chars e.g. " 1200"
    //   ~          = mode marker (single char, always ~ for FT2)
    //   decoded    = 37-char message field (space padded, last char may be '?')
    //   AA         = 2-char annotation: "  " (no AP) or "a1".."a9" etc.
    //
    // Because utcPrefix is prepended with no separator, splitting on whitespace
    // is unreliable when the prefix and the SNR leading spaces merge into one
    // token.  A single regex parse on the raw string is used instead.
    // The 6-digit UTC case (nutc > 9999, i.e. HHMMSS) was also missed
    // by the previous 4-char-only check.
    //
    // Returns: [time, snr, dt, df, message, aptype, quality, freq]

    if (row.trimmed().isEmpty()) return {};

    // Group 1: optional 4- or 6-digit UTC prefix
    // Group 2: SNR (signed integer, may share leading spaces with UTC token)
    // Group 3: dt  (signed float)
    // Group 4: freq (unsigned integer)
    // Group 5: marker (single non-space char, expected '~')
    // Group 6: message body (37-char field, trimmed)
    // Group 7: optional trailing annotation e.g. "a1", "a2"
    static const QRegularExpression re(
        R"(^(\d{4}|\d{6})?\s*(-?\d+)\s+(-?\d+\.\d+)\s+(\d+)\s+(\S)\s{2}(.*?)\s*(a\d+)?\s*$)");

    QRegularExpressionMatch match = re.match(row);
    if (!match.hasMatch()) return {};

    QString timeStr = normalizeUtcDisplayToken(match.captured(1)); // "" or "HHMM" or "HHMMSS"
    QString snrStr  = match.captured(2).trimmed();
    QString dtStr   = match.captured(3).trimmed();
    QString freqStr = match.captured(4).trimmed();
    // captured(5) is the marker character ('~') — not stored separately
    QString message = match.captured(6).trimmed();
    QString aptype;
    if (!match.captured(7).isEmpty()) {
        // annotation is "a<n>"; store only the numeric suffix as aptype
        aptype = match.captured(7).mid(1);
    }

    // df = absolute audio frequency offset relative to lower band edge (nfa)
    bool freqOk = false;
    int freqHz = freqStr.toInt(&freqOk);
    QString dfStr = freqOk ? QString::number(freqHz - m_nfa) : freqStr;

    return {timeStr, snrStr, dtStr, dfStr, message, aptype, "100", freqStr};
}

QStringList DecodiumBridge::parseJt65Row(const QString& row) const
{
    // Format from format_async_decode_line():
    //   "%04d%4d%5.1f%5d %2.2s %22.22s %3.3s"
    //   cols: 0-3(UTC) 4-7(SNR) 8-12(DT) 13-17(freq) 18(sp) 19-20(csync) 21(sp) 22-43(decoded) 44(sp) 45-47(flags)
    if (row.size() < 22) return {};

    QString timeStr = row.mid(0, 4).trimmed();
    QString snrStr  = row.mid(4, 4).trimmed();
    QString dtStr   = row.mid(8, 5).trimmed();
    QString freqStr = row.mid(13, 5).trimmed();
    QString message = (row.size() >= 44) ? row.mid(22, 22).trimmed() : row.mid(22).trimmed();

    if (snrStr.isEmpty() || freqStr.isEmpty()) return {};

    bool freqOk = false;
    int freqHz = freqStr.toInt(&freqOk);
    QString dfStr = freqOk ? QString::number(freqHz - m_nfa) : freqStr;

    // Returns same structure as parseFt8Row: [time, snr, dt, df, message, aptype, quality, freq]
    return {timeStr, snrStr, dtStr, dfStr, message, "", "100", freqStr};
}

void DecodiumBridge::startAudioCapture()
{
    bridgeLog("startAudioCapture() called");
    // Qt6: use QMediaDevices::audioInputs() and QAudioDevice
    QAudioDevice selectedDevice = QMediaDevices::defaultAudioInput();
    bool requestedDeviceFound = m_audioInputDevice.isEmpty();
    if (!m_audioInputDevice.isEmpty()) {
        for (const QAudioDevice& dev : QMediaDevices::audioInputs()) {
            if (dev.description() == m_audioInputDevice ||
                dev.description().contains(m_audioInputDevice, Qt::CaseInsensitive) ||
                m_audioInputDevice.contains(dev.description(), Qt::CaseInsensitive)) {
                selectedDevice = dev;
                requestedDeviceFound = true;
                break;
            }
        }
    }
    if (!requestedDeviceFound) {
        bridgeLog("startAudioCapture: requested input device not found, fallback to default: " +
                  selectedDevice.description());
        emit statusMessage("Audio input non trovato, uso default: " + selectedDevice.description());
    }

    // Create the audio sink (once, reused across start/stop cycles).
    // downSampleFactor=4: QAudioSource a 48000 Hz → buffer a 12000 Hz
    if (!m_audioSink) {
        m_audioSink = new DecodiumAudioSink(m_audioBuffer, 4, this);
        // Ring buffer per il path async FT2: ogni campione decimato va anche nel buffer circolare
        m_audioSink->setSampleCallback([this](short s) {
            m_asyncAudio[m_asyncAudioPos % ASYNC_BUF_SIZE] = s;
            ++m_asyncAudioPos;
            ++m_driftExpectedFrames;  // A2: count frames for drift detection
            if (m_wavManager) m_wavManager->feedSample(s);
        });
        connect(m_audioSink, &DecodiumAudioSink::audioLevelChanged,
                this, [this](double level) {
            static int cnt = 0;
            if (cnt++ < 5) bridgeLog("audioLevelChanged received level=" + QString::number(level));
            m_audioLevel = level;
            emit audioLevelChanged();
        });
        bridgeLog("audioSink created and connected");
    }

    // Create SoundInput (once).
    if (!m_soundInput) {
        m_soundInput = new SoundInput(this);
        connect(m_soundInput, &SoundInput::error, this, [this](const QString& msg) {
            emit errorMessage("Audio: " + msg);
        });
        connect(m_soundInput, &SoundInput::status, this, [this](const QString& msg) {
            emit statusMessage("Audio: " + msg);
        });
    }

    // Map bridge channel index to AudioDevice::Channel.
    // m_audioInputChannel: 0=Left/Mono, 1=Right (same as WSJT-X convention).
    AudioDevice::Channel channel = (m_audioInputChannel == 1)
        ? AudioDevice::Right
        : AudioDevice::Mono;

    // downSampleFactor: SoundInput requests sample rate = 12000 * factor from
    // the driver.  Use 1 if the device natively supports 12 kHz; use 4 for
    // the common 48 kHz USB codec (e.g. Kenwood TS-590S USB Audio CODEC).
    // framesPerBuffer: 4096 frames per QAudioInput notification period.
    const unsigned downSampleFactor = 4; // assumes 48 kHz source
    bridgeLog("SoundInput::start device=" + selectedDevice.description() +
              " channel=" + QString::number((int)channel) +
              " dsf=" + QString::number(downSampleFactor));
    m_soundInput->start(selectedDevice, 4096, m_audioSink, downSampleFactor, channel);
    // m_soundInput->setVolume(static_cast<float>(m_rxInputLevel / 100.0));

    emit statusMessage("Audio capture avviato: " + selectedDevice.description());
    bridgeLog("startAudioCapture() done");
}

void DecodiumBridge::stopAudioCapture()
{
    if (m_soundInput) {
        m_soundInput->stop();
    }
}

void DecodiumBridge::feedAudioToDecoder()
{
    if (m_audioBuffer.isEmpty()) {
        emit statusMessage("Buffer audio vuoto per questo periodo");
        return;
    }

    m_decoding = true;
    emit decodingChanged();

    quint64 serial = ++m_decodeSerial;
    int     nutc   = QDateTime::currentDateTimeUtc().toString("hhmm").toInt();
    int     nfqso  = (m_nfa + m_nfb) / 2;
    bridgeLog("feedAudioToDecoder: mode=" + m_mode +
              " samples=" + QString::number(m_audioBuffer.size()) +
              " nutc=" + QString::number(nutc));

    if (m_mode == "FT8") {
        decodium::ft8::DecodeRequest req;
        req.serial = serial; req.audio = m_audioBuffer;
        req.nutc = nutc; req.nqsoprogress = 0;
        req.nfqso = nfqso; req.nftx = 0;
        req.nfa = m_nfa; req.nfb = m_nfb;
        req.ndepth = m_ndepth; req.ncontest = m_ncontest;
        req.mycall = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        req.hisgrid = QByteArray();
        QMetaObject::invokeMethod(m_ft8Worker, [this, req]() {
            m_ft8Worker->decode(req);
        }, Qt::QueuedConnection);

    } else if (m_mode == "FT2") {
        decodium::ft2::DecodeRequest req;
        req.serial = serial; req.audio = m_audioBuffer;
        req.nutc = nutc; req.nqsoprogress = 0;
        req.nfqso = nfqso;
        req.nfa = m_nfa; req.nfb = m_nfb;
        req.ndepth = m_ndepth; req.ncontest = m_ncontest;
        req.mycall = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        // markLatestDecodeSerial() DEVE essere chiamato prima di decode(),
        // altrimenti il controllo seriale in FT2DecodeWorker::decode() scarta sempre la richiesta.
        m_ft2Worker->markLatestDecodeSerial(serial);
        QMetaObject::invokeMethod(m_ft2Worker, [this, req]() {
            m_ft2Worker->decode(req);
        }, Qt::QueuedConnection);

    } else if (m_mode == "FT4") {
        decodium::ft4::DecodeRequest req;
        req.serial = serial; req.audio = m_audioBuffer;
        req.nutc = nutc; req.nqsoprogress = 0;
        req.nfqso = nfqso;
        req.nfa = m_nfa; req.nfb = m_nfb;
        req.ndepth = m_ndepth; req.ncontest = m_ncontest;
        req.mycall = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        QMetaObject::invokeMethod(m_ft4Worker, [this, req]() {
            m_ft4Worker->decode(req);
        }, Qt::QueuedConnection);

    } else if (m_mode == "Q65") {
        decodium::q65::DecodeRequest req;
        req.serial  = serial; req.audio = m_audioBuffer;
        req.nutc    = nutc;   req.nfqso = nfqso;
        req.nfa     = m_nfa;  req.nfb   = m_nfb;
        req.ndepth  = m_ndepth; req.ncontest = m_ncontest;
        req.mycall  = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        QMetaObject::invokeMethod(m_q65Worker, [this, req]() {
            m_q65Worker->decode(req);
        }, Qt::QueuedConnection);

    } else if (m_mode == "MSK144") {
        decodium::msk144::DecodeRequest req;
        req.serial  = serial; req.audio = m_audioBuffer;
        req.nutc    = nutc;
        req.rxfreq  = m_rxFrequency;
        req.mycall  = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        QMetaObject::invokeMethod(m_mskWorker, [this, req]() {
            m_mskWorker->decode(req);
        }, Qt::QueuedConnection);

    } else if (m_mode == "WSPR") {
        decodium::wspr::DecodeRequest req;
        req.serial = serial;
        req.arguments = QStringList()
            << "-f" << QString::number(m_frequency / 1e6, 'f', 6)
            << "-c" << m_callsign
            << "-g" << m_grid;
        QMetaObject::invokeMethod(m_wsprWorker, [this, req]() {
            m_wsprWorker->decode(req);
        }, Qt::QueuedConnection);

    } else if (m_mode == "JT65" || m_mode == "JT9" || m_mode == "JT4") {
        decodium::legacyjt::DecodeRequest req;
        req.serial  = serial;
        req.mode    = m_mode;
        req.audio   = m_audioBuffer;
        req.nutc    = nutc;
        req.nfqso   = nfqso;
        req.nfa     = m_nfa;
        req.nfb     = m_nfb;
        req.ndepth  = m_ndepth;
        req.mycall  = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        req.hisgrid = m_dxGrid.toLocal8Bit();
        req.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toLocal8Bit();
        QMetaObject::invokeMethod(m_legacyJtWorker, [this, req]() {
            m_legacyJtWorker->decode(req);
        }, Qt::QueuedConnection);

    } else if (m_mode.startsWith("FST4")) {
        decodium::fst4::DecodeRequest req;
        req.serial  = serial;
        req.mode    = m_mode;
        req.audio   = m_audioBuffer;
        req.nutc    = nutc;
        req.nfa     = m_nfa;
        req.nfb     = m_nfb;
        req.nfqso   = nfqso;
        req.ndepth  = m_ndepth;
        req.mycall  = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        req.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toLocal8Bit();
        // Estrai periodo in secondi dal nome della modalità: FST4-30 → 30
        {
            int dashPos = m_mode.indexOf('-');
            req.ntrperiod = (dashPos >= 0) ? m_mode.mid(dashPos + 1).toInt() : 15;
        }
        req.iwspr = m_mode.startsWith("FST4W") ? 1 : 0;
        QMetaObject::invokeMethod(m_fst4Worker, [this, req]() {
            m_fst4Worker->decode(req);
        }, Qt::QueuedConnection);

    } else {
        // Fallback a FT8
        decodium::ft8::DecodeRequest req;
        req.serial = serial; req.audio = m_audioBuffer;
        req.nutc = nutc; req.nfqso = nfqso;
        req.nfa = m_nfa; req.nfb = m_nfb;
        req.ndepth = m_ndepth;
        req.mycall = m_callsign.toLocal8Bit();
        QMetaObject::invokeMethod(m_ft8Worker, [this, req]() {
            m_ft8Worker->decode(req);
        }, Qt::QueuedConnection);
    }

    m_audioBuffer.clear();
    m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
}

void DecodiumBridge::updatePeriodTicksMax()
{
    // Timer ticks at TIMER_MS (250ms). Ticks per period:
    // FT8=15s→60, FT4=7.5s→30, FT2=3.75s→15, Q65=60s→240, MSK144=15s→60, WSPR=120s→480
    if      (m_mode == "FT4")     m_periodTicksMax = 30;
    else if (m_mode == "FT2")     m_periodTicksMax = 15;
    else if (m_mode == "Q65")     m_periodTicksMax = 240;  // 60s
    else if (m_mode == "MSK144")  m_periodTicksMax = 60;   // 15s
    else if (m_mode == "WSPR")    m_periodTicksMax = 480;  // 120s
    else if (m_mode == "JT65")    m_periodTicksMax = 240;   // 60s
    else if (m_mode == "JT9")     m_periodTicksMax = 240;   // 60s
    else if (m_mode == "JT4")     m_periodTicksMax = 240;   // 60s
    else if (m_mode == "FST4")    m_periodTicksMax = 240;   // 60s
    else if (m_mode == "FST4W")   m_periodTicksMax = 480;   // 120s default
    else if (m_mode.startsWith("FST4")) {
        // FST4-15→60, FST4-30→120, FST4-60→240, FST4-120→480, FST4-300→1200, FST4-900→3600
        int dashPos = m_mode.indexOf('-');
        int trPeriod = (dashPos >= 0) ? m_mode.mid(dashPos + 1).toInt() : 15;
        m_periodTicksMax = trPeriod * 1000 / TIMER_MS;
    }
    else                          m_periodTicksMax = 60;   // FT8 default
}

QStringList DecodiumBridge::availableModes() const
{
    return {"FT8", "FT2", "FT4", "Q65", "MSK144", "JT65", "JT9", "JT4", "FST4", "FST4W", "WSPR"};
}

// Simple radix-2 in-place FFT (Cooley-Tukey)
static void fft_inplace(std::vector<std::complex<float>>& a)
{
    int n = (int)a.size();
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * 3.14159265f / len;
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; ++j) {
                auto u = a[i+j], v = a[i+j+len/2] * w;
                a[i+j]         = u + v;
                a[i+j+len/2]   = u - v;
                w *= wlen;
            }
        }
    }
}

QVector<float> DecodiumBridge::computeSpectrum() const
{
    const int N = SPECTRUM_FFT_SIZE; // 512
    if (m_spectrumBuf.size() < N) return {};

    // Hanning window + FFT
    std::vector<std::complex<float>> fftBuf(N);
    for (int i = 0; i < N; ++i) {
        float w = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * i / (N - 1)));
        fftBuf[i] = std::complex<float>(m_spectrumBuf[i] * w, 0.0f);
    }
    fft_inplace(fftBuf);

    // Magnitudes in dB: only positive frequencies (N/2 bins)
    // Frequency resolution: SAMPLE_RATE/N = 12000/512 ≈ 23.4 Hz/bin
    // We want 200-3000 Hz: bins 9 to 128
    const float sampleRate  = SAMPLE_RATE;
    const float freqPerBin  = sampleRate / N;
    const float freqMin     = m_nfa;    // 200 Hz
    const float freqMax     = m_nfb;    // 4000 Hz

    int binStart = (int)(freqMin / freqPerBin);
    int binEnd   = qMin((int)(freqMax / freqPerBin), N / 2 - 1);
    int nBins    = binEnd - binStart;
    if (nBins <= 0) return {};

    // Convert to dB and normalize
    float refLevel = (float)m_spectrumRefLevel;   // e.g. -10 dB
    float dynRange = (float)m_spectrumDynRange;    // e.g. 70 dB
    float floorDb  = refLevel - dynRange;

    QVector<float> result;
    result.reserve(nBins);
    for (int b = binStart; b < binEnd; ++b) {
        float mag = std::abs(fftBuf[b]) / (N / 2.0f);
        float db  = (mag > 1e-10f) ? 20.0f * std::log10(mag) : -200.0f;
        float norm = qBound(0.0f, (db - floorDb) / dynRange, 1.0f);
        result.append(norm);
    }
    return result;
}

// ─── Alta risoluzione: FFTW 4096 bin, output in dB raw ─────────────────────
QVector<float> DecodiumBridge::computePanadapter(float& outMinDb, float& outMaxDb) const
{
    const int N = PANADAPTER_FFT_SIZE; // 4096 → 2.93 Hz/bin @ 12kHz
    if (m_spectrumBuf.size() < N) return {};

    // Usa FFTW se disponibile, altrimenti fallback al DFT interno
#ifdef FFTW3_SINGLE_FOUND
    // FFTW plan (cached per performance — nota: non thread-safe senza mutex)
    static float*         fftIn  = nullptr;
    static fftwf_complex* fftOut = nullptr;
    static fftwf_plan     fftPlan = nullptr;
    static int            lastN   = 0;
    if (lastN != N) {
        if (fftPlan) fftwf_destroy_plan(fftPlan);
        if (fftIn)  fftwf_free(fftIn);
        if (fftOut) fftwf_free(fftOut);
        fftIn   = fftwf_alloc_real(N);
        fftOut  = fftwf_alloc_complex(N/2 + 1);
        fftPlan = fftwf_plan_dft_r2c_1d(N, fftIn, fftOut, FFTW_ESTIMATE);
        lastN   = N;
    }
    // Applica finestra Blackman-Harris (riduzione sidelobes > 92dB — meglio di Hann)
    constexpr float a0=0.35875f, a1=0.48829f, a2=0.14128f, a3=0.01168f;
    for (int i = 0; i < N; ++i) {
        float w = a0 - a1*std::cos(2.f*3.14159265f*i/(N-1))
                     + a2*std::cos(4.f*3.14159265f*i/(N-1))
                     - a3*std::cos(6.f*3.14159265f*i/(N-1));
        fftIn[i] = m_spectrumBuf[i] * w;
    }
    fftwf_execute(fftPlan);

    const float freqPerBin = (float)SAMPLE_RATE / N;
    int binStart = (int)(m_nfa / freqPerBin);
    int binEnd   = qMin((int)(m_nfb / freqPerBin), N/2);
    int nBins    = binEnd - binStart;
    if (nBins <= 0) return {};

    QVector<float> result;
    result.reserve(nBins);
    float minDb =  200.f, maxDb = -200.f;
    for (int b = binStart; b < binEnd; ++b) {
        float re = fftOut[b][0], im = fftOut[b][1];
        float mag = std::sqrt(re*re + im*im) / (N/2.f);
        float db  = (mag > 1e-12f) ? 20.f * std::log10(mag) : -200.f;
        result.append(db);
        if (db > -190.f) { minDb = qMin(minDb, db); maxDb = qMax(maxDb, db); }
    }
    outMinDb = minDb;
    outMaxDb = maxDb;
    return result;
#else
    // Fallback: DFT interno — molto più lento ma funzionale
    std::vector<std::complex<float>> fftBuf(N);
    for (int i = 0; i < N; ++i) {
        float w = 0.5f*(1.f - std::cos(2.f*3.14159265f*i/(N-1)));
        fftBuf[i] = std::complex<float>(m_spectrumBuf[i]*w, 0.f);
    }
    fft_inplace(fftBuf);
    const float freqPerBin = (float)SAMPLE_RATE / N;
    int binStart = (int)(m_nfa / freqPerBin);
    int binEnd   = qMin((int)(m_nfb / freqPerBin), N/2-1);
    int nBins    = binEnd - binStart;
    if (nBins <= 0) return {};
    QVector<float> result;
    result.reserve(nBins);
    float minDb = 200.f, maxDb = -200.f;
    for (int b = binStart; b < binEnd; ++b) {
        float mag = std::abs(fftBuf[b]) / (N/2.f);
        float db  = (mag > 1e-12f) ? 20.f*std::log10(mag) : -200.f;
        result.append(db);
        if (db > -190.f) { minDb = qMin(minDb, db); maxDb = qMax(maxDb, db); }
    }
    outMinDb = minDb; outMaxDb = maxDb;
    return result;
#endif
}

void DecodiumBridge::initTxDevices()
{
    // Il dispositivo TX viene selezionato inline in startTx() tramite QMediaDevices (Qt6).
    // Questa funzione è mantenuta per compatibilità con le chiamate esistenti.
    bridgeLog("initTxDevices: no-op (setup inline in startTx)");
}

QString DecodiumBridge::buildCurrentTxMessage() const
{
    QString selectedMessage;
    switch (m_currentTx) {
        case 1: selectedMessage = m_tx1; break;
        case 2: selectedMessage = m_tx2; break;
        case 3: selectedMessage = m_tx3; break;
        case 4: selectedMessage = m_tx4; break;
        case 5: selectedMessage = m_tx5; break;
        case 6: selectedMessage = m_tx6; break;
        default: selectedMessage = m_tx1; break;
    }

    if (selectedMessage.trimmed().isEmpty() &&
        m_currentTx >= 1 && m_currentTx <= 5 &&
        m_dxCall.trimmed().isEmpty() &&
        !m_tx6.trimmed().isEmpty()) {
        return m_tx6;
    }

    return selectedMessage;
}

void DecodiumBridge::enumerateAudioDevices()
{
    m_audioInputDevices.clear();
    m_audioOutputDevices.clear();

    bridgeLog("=== enumerateAudioDevices INPUT ===");
    for (const QAudioDevice& dev : QMediaDevices::audioInputs()) {
        bridgeLog("  IN: " + dev.description());
        m_audioInputDevices.append(dev.description());
    }
    bridgeLog("=== enumerateAudioDevices OUTPUT ===");
    for (const QAudioDevice& dev : QMediaDevices::audioOutputs()) {
        bridgeLog("  OUT: " + dev.description());
        m_audioOutputDevices.append(dev.description());
    }
    bridgeLog("=== enumerateAudioDevices END ===");

    emit audioInputDevicesChanged();
    emit audioOutputDevicesChanged();
}

// ============================================================
// A4 — Fox/Hound caller queue management
// ============================================================

// mainwindow: processNextInQueue() — prende il prossimo caller dalla coda
// Chiamato dopo QSO completato in multi-answer mode
void DecodiumBridge::processNextInQueue()
{
    while (!m_callerQueue.isEmpty()) {
        QString entry = m_callerQueue.takeFirst();
        emit callerQueueChanged();

        QStringList const parts = entry.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            continue;
        }

        QString const nextCall = Radio::base_callsign(parts.first()).trimmed().toUpper();
        if (nextCall.isEmpty()) {
            continue;
        }
        if (isRecentAutoCqDuplicate(nextCall, m_frequency, m_mode)) {
            bridgeLog("processNextInQueue: skip recent " + nextCall);
            continue;
        }

        int const rxFreq = parts.size() >= 2 ? parts.at(1).toInt() : m_rxFrequency;
        int const snr = parts.size() >= 3 ? qBound(-50, parts.at(2).toInt(), 49)
                                          : (m_reportSent.isEmpty() ? -10 : m_reportSent.toInt());

        bridgeLog("processNextInQueue: prossimo caller=" + nextCall);

        clearPendingAutoLogSnapshot();
        clearAutoCqPartnerLock();
        m_nTx73 = 0;
        m_txRetryCount = 0;
        m_lastNtx = -1;
        m_lastCqPidx = -1;
        m_txWatchdogTicks = 0;
        m_autoCQPeriodsMissed = 0;
        m_reportReceived = QStringLiteral("-10");
        emit reportReceivedChanged();

        setDxCall(nextCall);
        setDxGrid(QString());
        if (rxFreq > 0) {
            setRxFrequency(rxFreq);
        }
        setReportSent(QString::number(snr));
        genStdMsgs(nextCall, m_grid.left(4));

        // Il caller è già in coda: andiamo direttamente a TX2 (report)
        advanceQsoState(2);
        updateAutoCqPartnerLock();
        setTxEnabled(true);
        return;
    }

    if (m_autoCqRepeat) {
        advanceQsoState(6);
        setTxEnabled(true);
        bridgeLog("processNextInQueue: coda vuota → torno a CQ");
    }
}

void DecodiumBridge::enqueueStation(const QString& call)
{
    enqueueCallerInternal(call);
}

void DecodiumBridge::dequeueStation(const QString& call)
{
    removeCallerFromQueue(call);
}

void DecodiumBridge::clearCallerQueue()
{
    if (m_callerQueue.isEmpty()) return;
    m_callerQueue.clear();
    emit callerQueueChanged();
}

// ============================================================
// B9 — Active Stations model accessor
// ============================================================

QObject* DecodiumBridge::activeStations() const
{
    return m_activeStations;
}

// ============================================================
// C13 — Grid distance/bearing invokables
// ============================================================

double DecodiumBridge::calcDistance(const QString& myGrid, const QString& dxGrid) const
{
    double lat1, lon1, lat2, lon2;
    if (!grid2deg(myGrid, lon1, lat1)) return -1.0;
    if (!grid2deg(dxGrid, lon2, lat2)) return -1.0;
    double az;
    return azdist(lat1, lon1, lat2, lon2, az);
}

double DecodiumBridge::calcBearing(const QString& myGrid, const QString& dxGrid) const
{
    double lat1, lon1, lat2, lon2;
    if (!grid2deg(myGrid, lon1, lat1)) return -1.0;
    if (!grid2deg(dxGrid, lon2, lat2)) return -1.0;
    double az;
    azdist(lat1, lon1, lat2, lon2, az);
    return az;
}

// ============================================================
// C14 — Grid → lat/lon (per AstroPanel QML)
// ============================================================

double DecodiumBridge::latFromGrid(const QString& grid) const
{
    double lon, lat;
    if (!grid2deg(grid, lon, lat)) return 0.0;
    return lat;
}

double DecodiumBridge::lonFromGrid(const QString& grid) const
{
    double lon, lat;
    if (!grid2deg(grid, lon, lat)) return 0.0;
    return lon;
}

// ============================================================
// C15 — QSY: cambio frequenza + modo in un'unica chiamata QML
// ============================================================

void DecodiumBridge::qsyTo(double freqHz, const QString& newMode)
{
    setFrequency(freqHz);
    if (!newMode.isEmpty() && newMode != m_mode) {
        setMode(newMode);
    }
    if (m_catConnected) {
        activeCatSetFreq(m_nativeCat, m_hamlibCat, m_catBackend, freqHz);
    }
    emit statusMessage(QString("QSY → %1 MHz (%2)")
        .arg(freqHz / 1e6, 0, 'f', 6)
        .arg(newMode));
}

// ============================================================
// B6 — cty.dat: scarica se mancante o vecchio di >30 giorni
// ============================================================

void DecodiumBridge::checkCtyDatUpdate()
{
    if (m_ctyDatUpdating) return;

    // Cerca cty.dat nelle posizioni standard
    QStringList searchPaths = ctyDatSearchPaths();

    QString existing;
    for (const auto& p : searchPaths) {
        if (QFile::exists(p)) { existing = p; break; }
    }

    bool needsUpdate = existing.isEmpty();
    if (!needsUpdate && !existing.isEmpty()) {
        QFileInfo fi(existing);
        needsUpdate = fi.lastModified().daysTo(QDateTime::currentDateTime()) > 30;
    }

    if (!needsUpdate) {
        emit statusMessage("cty.dat è aggiornato.");
        return;
    }

    m_ctyDatUpdating = true;
    emit ctyDatUpdatingChanged();
    emit statusMessage("Download cty.dat in corso...");

    QString destPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cty.dat";
    QDir().mkpath(QFileInfo(destPath).absolutePath());

    QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    QNetworkReply* reply = nam->get(QNetworkRequest(
        QUrl("https://www.country-files.com/bigcty/cty.dat")));

    connect(reply, &QNetworkReply::finished, this, [this, reply, destPath, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        m_ctyDatUpdating = false;
        emit ctyDatUpdatingChanged();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorMessage("Download cty.dat fallito: " + reply->errorString());
            return;
        }
        QFile f(destPath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(reply->readAll());
            f.close();
            QString loadedPath;
            if (reloadDxccLookup(&loadedPath)) {
                refreshDecodeListDxcc();
                emit statusMessage("cty.dat aggiornato e caricato: " + loadedPath);
            } else {
                emit errorMessage("cty.dat scaricato ma il caricamento DXCC è fallito.");
            }
        }
    });
}

QStringList DecodiumBridge::ctyDatSearchPaths() const
{
    QString const appDir = QCoreApplication::applicationDirPath();
    QStringList paths = {
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cty.dat",
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/cty.dat",
        appDir + "/cty.dat",
        QDir(appDir).absoluteFilePath("../cty.dat"),
        QDir(appDir).absoluteFilePath("../Resources/cty.dat"),
        QDir(appDir).absoluteFilePath("../share/Decodium/cty.dat"),
        QDir(appDir).absoluteFilePath("../share/wsjtx/cty.dat"),
        QDir::current().absoluteFilePath("cty.dat")
    };

#ifdef CMAKE_SOURCE_DIR
    paths << QDir(QStringLiteral(CMAKE_SOURCE_DIR)).absoluteFilePath("cty.dat");
#endif

    QStringList uniquePaths;
    QSet<QString> seen;
    for (const QString& path : paths) {
        QString const cleanPath = QDir::cleanPath(path);
        if (cleanPath.isEmpty() || seen.contains(cleanPath))
            continue;
        seen.insert(cleanPath);
        uniquePaths << cleanPath;
    }
    return uniquePaths;
}

bool DecodiumBridge::reloadDxccLookup(QString* loadedPath)
{
    if (loadedPath)
        loadedPath->clear();
    if (!m_dxccLookup)
        return false;

    for (const QString& path : ctyDatSearchPaths()) {
        if (!QFile::exists(path))
            continue;
        if (m_dxccLookup->loadCtyDat(path)) {
            if (loadedPath)
                *loadedPath = path;
            return true;
        }
    }
    return false;
}

// ============================================================
// B8 — Alert sounds: playback tramite DecodiumAlertManager
// ============================================================

void DecodiumBridge::playAlert(const QString& alertType)
{
    if (!m_alertSoundsEnabled) return;
    if (m_alertManager) {
        m_alertManager->playAlert(alertType);
    } else {
        // Fallback di sistema se il manager non e' ancora inizializzato
        QApplication::beep();
    }
}

// ============================================================
// WAV decode: decodifica un singolo file WAV o un'intera cartella
// ============================================================

void DecodiumBridge::openWavForDecode(const QString& path)
{
    if (path.isEmpty() || !QFile::exists(path)) {
        emit errorMessage("File WAV non trovato: " + path);
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit errorMessage("Impossibile aprire: " + path);
        return;
    }

    QByteArray data = f.readAll();
    f.close();

    // Header RIFF/WAV minimo: 44 byte
    if (data.size() < 44) {
        emit errorMessage("File WAV troppo corto: " + path);
        return;
    }

    const char* hdr = data.constData();

    // Verifica firma RIFF
    if (qstrncmp(hdr, "RIFF", 4) != 0 || qstrncmp(hdr + 8, "WAVE", 4) != 0) {
        emit errorMessage("Formato file non riconosciuto (non RIFF/WAVE): " + path);
        return;
    }

    // Leggi parametri dal fmt chunk (offset standard per RIFF lineare)
    uint16_t channels      = *reinterpret_cast<const uint16_t*>(hdr + 22);
    uint32_t sampleRate    = *reinterpret_cast<const uint32_t*>(hdr + 24);
    uint16_t bitsPerSample = *reinterpret_cast<const uint16_t*>(hdr + 34);

    if (channels == 0 || sampleRate == 0 || bitsPerSample == 0) {
        emit errorMessage("Parametri WAV non validi in: " + path);
        return;
    }

    // Scansiona i chunk per trovare 'data'
    int dataOffset = -1;
    int dataSize   = 0;
    int pos = 12; // dopo RIFF header (4 "RIFF" + 4 size + 4 "WAVE")
    while (pos + 8 <= data.size()) {
        const char* chunkId   = hdr + pos;
        uint32_t    chunkSize = *reinterpret_cast<const uint32_t*>(hdr + pos + 4);
        if (qstrncmp(chunkId, "data", 4) == 0) {
            dataOffset = pos + 8;
            dataSize   = static_cast<int>(chunkSize);
            break;
        }
        pos += 8 + static_cast<int>(chunkSize);
        if (chunkSize & 1) ++pos; // padding byte se chunkSize dispari
    }

    if (dataOffset < 0 || dataOffset >= data.size()) {
        emit errorMessage("Chunk 'data' non trovato nel WAV: " + path);
        return;
    }

    // Limita dataSize al contenuto effettivo del file
    dataSize = qMin(dataSize, data.size() - dataOffset);

    // Converti in QVector<short> mono a 12000 Hz
    QVector<short> samples;
    if (bitsPerSample == 16) {
        const int16_t* pcm = reinterpret_cast<const int16_t*>(data.constData() + dataOffset);
        int nFrames = dataSize / static_cast<int>(channels * sizeof(int16_t));
        // Decimazione: step = sampleRate / 12000 (e.g. 48000 Hz → step 4)
        int step = qMax(1, static_cast<int>(sampleRate / 12000));
        samples.reserve(nFrames / step + 1);
        for (int i = 0; i < nFrames; i += step) {
            samples.append(pcm[i * channels]); // canale 0 (left / mono)
        }
    } else if (bitsPerSample == 8) {
        // PCM unsigned 8-bit → signed 16-bit
        const uint8_t* pcm = reinterpret_cast<const uint8_t*>(data.constData() + dataOffset);
        int nFrames = dataSize / static_cast<int>(channels);
        int step = qMax(1, static_cast<int>(sampleRate / 12000));
        samples.reserve(nFrames / step + 1);
        for (int i = 0; i < nFrames; i += step) {
            int16_t s = static_cast<int16_t>((pcm[i * channels] - 128) << 8);
            samples.append(s);
        }
    } else {
        emit errorMessage(QString("Bit depth %1 non supportato (serve 8 o 16 bit): %2")
                          .arg(bitsPerSample).arg(path));
        return;
    }

    if (samples.isEmpty()) {
        emit errorMessage("Nessun campione audio estratto da: " + path);
        return;
    }

    emit statusMessage(QString("WAV: %1 campioni @ 12000 Hz → decodifica %2 (%3)...")
                       .arg(samples.size()).arg(m_mode).arg(QFileInfo(path).fileName()));

    // Carica i campioni nel buffer audio e avvia la decodifica
    m_audioBuffer.resize(samples.size() * static_cast<int>(sizeof(short)));
    memcpy(m_audioBuffer.data(), samples.constData(),
           static_cast<size_t>(samples.size()) * sizeof(short));

    // Avvia feedAudioToDecoder() in modo asincrono per non bloccare la UI
    QMetaObject::invokeMethod(this, [this]() {
        feedAudioToDecoder();
    }, Qt::QueuedConnection);
}

void DecodiumBridge::openWavFolderDecode(const QString& folderPath)
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        emit errorMessage("Cartella non trovata: " + folderPath);
        return;
    }

    QStringList wavFiles = dir.entryList({"*.wav", "*.WAV"}, QDir::Files, QDir::Name);
    if (wavFiles.isEmpty()) {
        emit statusMessage("Nessun file WAV trovato in: " + folderPath);
        return;
    }

    emit statusMessage(QString("Batch decode: %1 file WAV in %2...")
                       .arg(wavFiles.size()).arg(folderPath));

    // Decodifica ogni file con 500 ms di distanza tra un file e l'altro,
    // cosi i risultati non si sovrappongono nel buffer.
    int delay = 0;
    for (const QString& fname : wavFiles) {
        QString fullPath = dir.filePath(fname);
        QTimer::singleShot(delay, this, [this, fullPath]() {
            openWavForDecode(fullPath);
        });
        delay += 500;
    }
}

// ============================================================
// B11 — Cabrillo export dal log ADIF salvato
// ============================================================

bool DecodiumBridge::exportCabrillo(const QString& filename)
{
    if (filename.isEmpty()) return false;

    // Leggi il log da all.txt (formato custom IU8LMC)
    QString logPath = logAllTxtPath();
    QFile logFile(logPath);
    if (!logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorMessage("Log non trovato: " + logPath);
        return false;
    }

    QFile out(filename);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorMessage("Impossibile scrivere: " + filename);
        return false;
    }

    QTextStream ts(&out);
    // Cabrillo header
    ts << "START-OF-LOG: 3.0\n";
    ts << "CALLSIGN: " << m_callsign << "\n";
    ts << "CONTEST: " << (m_contestExchange.isEmpty() ? "GENERIC-FT" : m_contestExchange) << "\n";
    ts << "CREATED-BY: Decodium 3.0\n";
    ts << "CATEGORY-OPERATOR: SINGLE-OP\n";
    ts << "CATEGORY-MODE: DIGI\n";
    ts << "OPERATORS: " << m_callsign << "\n";

    // Converti ogni riga log → QSO Cabrillo
    QTextStream in(&logFile);
    int qsoCount = 0;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        // formato: YYYYMMDD_HHMMSS MYCALL DXCALL DXGRID freq MODE
        QStringList f = line.split(' ', Qt::SkipEmptyParts);
        if (f.size() < 6) continue;
        QString dt = f[0];   // YYYYMMDD_HHMMSS
        QString dxCall = f[2];
        QString freq = f[4]; // e.g. "14.074000MHz"
        QString mode = f[5];
        // Parse date/time
        QString date = dt.left(8);     // YYYYMMDD
        QString time = dt.mid(9, 6);   // HHMMSS
        // Format: QSO: freq mode date time mycall rst dxcall rst
        QString freqKhz = freq.replace("MHz","").trimmed();
        bool ok;
        double fmhz = freqKhz.toDouble(&ok);
        if (ok) freqKhz = QString::number((int)(fmhz * 1000));
        ts << QString("QSO: %1 %2 %3 %4 %5 59 %6 59\n")
              .arg(freqKhz, -6)
              .arg(mode, -6)
              .arg(date.mid(0,4) + "-" + date.mid(4,2) + "-" + date.mid(6,2))
              .arg(time.left(4))
              .arg(m_callsign, -13)
              .arg(dxCall, -13);
        ++qsoCount;
    }

    ts << "END-OF-LOG:\n";
    emit statusMessage(QString("Cabrillo esportato: %1 QSO → %2").arg(qsoCount).arg(filename));
    return true;
}

// ============================================================
// C16 — UpdateChecker: controlla releases su GitHub
// ============================================================

void DecodiumBridge::checkForUpdates()
{
    QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl("https://api.github.com/repos/IU8LMC/decodium/releases/latest"));
    req.setRawHeader("Accept", "application/vnd.github.v3+json");
    req.setRawHeader("User-Agent", "Decodium/3.0");

    QNetworkReply* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;

        QByteArray data = reply->readAll();
        // Parse "tag_name" from JSON without full JSON dependency
        QString json = QString::fromUtf8(data);
        QRegularExpression re("\"tag_name\"\\s*:\\s*\"([^\"]+)\"");
        auto m = re.match(json);
        if (!m.hasMatch()) return;

        QString latestTag = m.captured(1).remove('v');
        QString currentVer = version();

        m_latestVersion = latestTag;
        emit latestVersionChanged();

        // Simple version compare: split by '.' and compare numerically
        auto splitVer = [](const QString& v) -> QList<int> {
            QList<int> parts;
            for (const auto& p : v.split('.')) parts << p.toInt();
            while (parts.size() < 3) parts << 0;
            return parts;
        };
        auto cv = splitVer(currentVer);
        auto lv = splitVer(latestTag);
        bool newer = (lv[0] > cv[0]) ||
                     (lv[0] == cv[0] && lv[1] > cv[1]) ||
                     (lv[0] == cv[0] && lv[1] == cv[1] && lv[2] > cv[2]);
        if (newer != m_updateAvailable) {
            m_updateAvailable = newer;
            emit updateAvailableChanged();
        }
        if (newer)
            emit statusMessage("Aggiornamento disponibile: v" + latestTag);
    });
}

// ============================================================
// C17 — OTP Fox verification: valida codice Fox via 9dx.cc
// ============================================================

void DecodiumBridge::verifyOtp(const QString& callsign, const QString& code, int audioHz)
{
    if (callsign.isEmpty() || code.isEmpty()) return;

    // Usa il timestamp UTC corrente arrotondato al periodo FT8
    QDateTime ts = QDateTime::currentDateTimeUtc();

    auto* verifier = new FoxVerifier(
        "Decodium/3.0",
        nullptr,  // FoxVerifier crea il proprio QNetworkAccessManager se null
        FOXVERIFIER_DEFAULT_BASE_URL,
        callsign,
        ts,
        code,
        static_cast<unsigned int>(audioHz > 0 ? audioHz : 750)
    );

    connect(verifier, &FoxVerifier::verifyComplete,
            this, [this, verifier](int /*status*/, QDateTime /*ts*/, QString call,
                                   QString /*code*/, unsigned int /*hz*/, QString response) {
        verifier->deleteLater();
        emit statusMessage("Fox OTP OK: " + call + " — " + response);
    });

    connect(verifier, &FoxVerifier::verifyError,
            this, [this, verifier](int /*status*/, QDateTime /*ts*/, QString call,
                                   QString /*code*/, unsigned int /*hz*/, QString response) {
        verifier->deleteLater();
        emit statusMessage("Fox OTP FAIL: " + call + " — " + response);
    });
}

// ============================================================
// ADIF import/export + workedCalls B4 check
// ============================================================

static QString bandFromFreqHz(double hz) {
    double mhz = hz / 1e6;
    if      (mhz <  2.0)  return "160M";
    else if (mhz <  4.0)  return "80M";
    else if (mhz <  6.0)  return "60M";
    else if (mhz <  8.0)  return "40M";
    else if (mhz < 11.0)  return "30M";
    else if (mhz < 15.0)  return "20M";
    else if (mhz < 19.0)  return "17M";
    else if (mhz < 22.0)  return "15M";
    else if (mhz < 25.0)  return "12M";
    else if (mhz < 30.0)  return "10M";
    else if (mhz < 52.0)  return "6M";
    else if (mhz < 150.0) return "2M";
    else                  return "70CM";
}

void DecodiumBridge::appendAdifRecord(const QString& dxCall, const QString& dxGrid,
                                       double freqHz, const QString& mode,
                                       const QDateTime& utc,
                                       const QString& rstSent, const QString& rstRcvd)
{
    QString const logPath = ensureAdifLogPath();
    QDir().mkpath(QFileInfo(logPath).absolutePath());

    QFile f(logPath);
    bool newFile = !f.exists();
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;

    QTextStream ts(&f);
    if (newFile) ts << "Decodium3 ADIF Log\n<EOH>\n";

    auto field = [](const QString& tag, const QString& val) -> QString {
        return QString("<%1:%2>%3 ").arg(tag).arg(val.length()).arg(val);
    };
    double freqMhz = freqHz / 1e6;
    ts << field("CALL",           dxCall)
       << field("BAND",           bandFromFreqHz(freqHz))
       << field("FREQ",           QString::number(freqMhz, 'f', 6))
       << field("MODE",           mode)
       << field("QSO_DATE",       utc.toString("yyyyMMdd"))
       << field("TIME_ON",        utc.toString("HHmmss"))
       << field("RST_SENT",       rstSent.isEmpty() ? "-10" : rstSent)
       << field("RST_RCVD",       rstRcvd.isEmpty() ? "-10" : rstRcvd)
       << field("GRIDSQUARE",     dxGrid)
       << field("MY_CALL",        m_callsign)
       << field("MY_GRIDSQUARE",  m_grid)
       << "<EOR>\n";

    m_workedCalls.insert(dxCall.toUpper());
    emit qsoCountChanged();
    emit workedCountChanged();
}

bool DecodiumBridge::exportAdif(const QString& filename)
{
    if (filename.isEmpty()) return false;
    QString logPath = logAllTxtPath();
    QFile src(logPath);
    if (!src.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString const adifPath = effectiveAdifLogPath();
        if (QFile::exists(adifPath))
            return QFile::copy(adifPath, filename);
        emit errorMessage("Log non trovato: " + logPath);
        return false;
    }
    QFile out(filename);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorMessage("Impossibile scrivere: " + filename);
        return false;
    }
    QTextStream ts(&out);
    ts << "Decodium3 ADIF Export — " << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "\n<EOH>\n";
    auto field = [](const QString& tag, const QString& val) -> QString {
        return QString("<%1:%2>%3 ").arg(tag).arg(val.length()).arg(val);
    };
    QTextStream in(&src);
    int count = 0;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList f = line.split(' ', Qt::SkipEmptyParts);
        if (f.size() < 5) continue;
        QString dtStr  = f[0];
        QString dxCall = f[2];
        QString dxGrid = (f.size() > 3) ? f[3] : "";
        QString freqStr= (f.size() > 4) ? f[4] : "";
        QString mode   = (f.size() > 5) ? f[5] : "FT8";
        QDateTime utc  = QDateTime::fromString(dtStr, "yyyyMMdd_hhmmss");
        utc.setTimeZone(QTimeZone::utc());
        double freqMhz = freqStr.replace("MHz","").toDouble();
        double freqHz  = freqMhz * 1e6;
        ts << field("CALL",          dxCall)
           << field("BAND",          bandFromFreqHz(freqHz))
           << field("FREQ",          QString::number(freqMhz, 'f', 6))
           << field("MODE",          mode)
           << field("QSO_DATE",      utc.toString("yyyyMMdd"))
           << field("TIME_ON",       utc.toString("HHmmss"))
           << field("RST_SENT",      "-10")
           << field("RST_RCVD",      "-10")
           << field("GRIDSQUARE",    dxGrid)
           << field("MY_CALL",       m_callsign)
           << field("MY_GRIDSQUARE", m_grid)
           << "<EOR>\n";
        ++count;
    }
    emit statusMessage(QString("ADIF esportato: %1 QSO → %2").arg(count).arg(filename));
    return true;
}

bool DecodiumBridge::importAdif(const QString& filename)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorMessage("Impossibile aprire ADIF: " + filename);
        return false;
    }
    QString data = QString::fromLocal8Bit(f.readAll());
    f.close();
    int eoh = data.indexOf("<EOH>", Qt::CaseInsensitive);
    if (eoh >= 0) data = data.mid(eoh + 5);
    QRegularExpression reCall("<CALL:\\d+>([^<\\s]+)", QRegularExpression::CaseInsensitiveOption);
    auto it = reCall.globalMatch(data);
    int imported = 0;
    while (it.hasNext()) {
        auto m = it.next();
        m_workedCalls.insert(m.captured(1).toUpper());
        ++imported;
    }
    emit qsoCountChanged();
    emit workedCountChanged();
    emit statusMessage(QString("ADIF importato: %1 callsign caricati").arg(imported));
    return true;
}

// ============================================================
// LotW lite
// ============================================================

bool DecodiumBridge::isLotwUser(const QString& call) const
{
    return m_lotwUsers.contains(call.toUpper());
}

void DecodiumBridge::updateLotwUsers()
{
    if (m_lotwUpdating) return;
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/lotw_users.txt";
    QFileInfo fi(cachePath);
    if (fi.exists() && fi.lastModified().daysTo(QDateTime::currentDateTime()) < 7) {
        QFile fc(cachePath);
        if (fc.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream ts(&fc);
            while (!ts.atEnd()) {
                QString call = ts.readLine().trimmed().toUpper();
                if (!call.isEmpty()) m_lotwUsers.insert(call);
            }
            emit statusMessage(QString("LotW: %1 utenti dalla cache").arg(m_lotwUsers.size()));
            return;
        }
    }
    m_lotwUpdating = true;
    emit lotwUpdatingChanged();
    emit statusMessage("LotW: download lista utenti...");
    QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    QNetworkReply* reply = nam->get(QNetworkRequest(QUrl("https://lotw.arrl.org/lotw-user-activity.csv")));
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, cachePath]() {
        reply->deleteLater();
        nam->deleteLater();
        m_lotwUpdating = false;
        emit lotwUpdatingChanged();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorMessage("LotW download fallito: " + reply->errorString());
            return;
        }
        QByteArray data = reply->readAll();
        QDir().mkpath(QFileInfo(cachePath).absolutePath());
        QFile cache(cachePath);
        if (cache.open(QIODevice::WriteOnly)) { cache.write(data); cache.close(); }
        m_lotwUsers.clear();
        QString text = QString::fromUtf8(data);
        for (const QString& line : text.split('\n')) {
            QString call = line.split(',').first().trimmed().toUpper();
            if (!call.isEmpty() && !call.startsWith('#'))
                m_lotwUsers.insert(call);
        }
        emit statusMessage(QString("LotW: %1 utenti caricati").arg(m_lotwUsers.size()));
    });
}

QStringList DecodiumBridge::workedCallsigns() const { return QStringList(m_workedCalls.values()); }
int         DecodiumBridge::workedCount()      const { return m_workedCalls.size(); }
int         DecodiumBridge::qsoCount()         const { return searchQsos(QString {}, QString {}, QString {}, QString {}, QString {}).size(); }

QVariantList DecodiumBridge::searchQsos(const QString& search,
                                        const QString& band,
                                        const QString& mode,
                                        const QString& fromDate,
                                        const QString& toDate) const
{
    QVariantList results;
    ParsedAdifDocument const doc = loadAdifDocument(effectiveAdifLogPath());
    QString const needle = search.trimmed().toUpper();
    QString const bandFilter = band.trimmed().toUpper();
    QString const modeFilter = mode.trimmed().toUpper();
    QString const fromFilter = fromDate.trimmed();
    QString const toFilter = toDate.trimmed();
    QList<QVariantMap> rows;

    auto matchesDate = [] (QString const& dateTime, QString const& fromValue, QString const& toValue) {
        QString const compact = dateTime.left(10).remove(QLatin1Char('-'));
        QString const fromCompact = QString(fromValue).remove(QRegularExpression {R"([^0-9])"});
        QString const toCompact = QString(toValue).remove(QRegularExpression {R"([^0-9])"});
        if (!fromCompact.isEmpty() && compact < fromCompact) {
            return false;
        }
        if (!toCompact.isEmpty() && compact > toCompact) {
            return false;
        }
        return true;
    };

    for (ParsedAdifRecord const& record : doc.records) {
        QVariantMap const row = qsoMapFromAdifRecord(record.fields, m_grid);
        QString const rowBand = row.value(QStringLiteral("band")).toString().trimmed().toUpper();
        QString const rowMode = row.value(QStringLiteral("mode")).toString().trimmed().toUpper();
        QString const rowCall = row.value(QStringLiteral("call")).toString().trimmed().toUpper();
        QString const rowGrid = row.value(QStringLiteral("grid")).toString().trimmed().toUpper();
        QString const rowComment = row.value(QStringLiteral("comment")).toString().trimmed().toUpper();
        QString const rowDateTime = row.value(QStringLiteral("dateTime")).toString();

        if (!bandFilter.isEmpty() && rowBand != bandFilter) {
            continue;
        }
        if (!modeFilter.isEmpty() && rowMode != modeFilter) {
            continue;
        }
        if (!matchesDate(rowDateTime, fromFilter, toFilter)) {
            continue;
        }
        if (!needle.isEmpty()
            && !rowCall.contains(needle)
            && !rowGrid.contains(needle)
            && !rowComment.contains(needle)
            && !rowDateTime.toUpper().contains(needle)
            && !rowBand.contains(needle)
            && !rowMode.contains(needle)) {
            continue;
        }

        rows.append(row);
    }

    std::sort(rows.begin(), rows.end(), [] (QVariantMap const& lhs, QVariantMap const& rhs) {
        return lhs.value(QStringLiteral("dateTime")).toString()
             > rhs.value(QStringLiteral("dateTime")).toString();
    });

    for (QVariantMap const& row : rows) {
        results.append(row);
    }
    return results;
}

QVariantMap DecodiumBridge::getQsoStats() const
{
    QVariantMap stats;
    QVariantList const rows = searchQsos(QString {}, QString {}, QString {}, QString {}, QString {});
    QSet<QString> uniqueCalls;
    QSet<QString> uniqueGrids;
    int maxDistance = 0;
    QString farthestCall;

    for (QVariant const& rowValue : rows) {
        QVariantMap const row = rowValue.toMap();
        QString const call = row.value(QStringLiteral("call")).toString().trimmed().toUpper();
        QString const grid = row.value(QStringLiteral("grid")).toString().trimmed().toUpper();
        int const distanceKm = row.value(QStringLiteral("distanceKm")).toInt();
        if (!call.isEmpty()) {
            uniqueCalls.insert(call);
        }
        if (!grid.isEmpty()) {
            uniqueGrids.insert(grid);
        }
        if (distanceKm > maxDistance) {
            maxDistance = distanceKm;
            farthestCall = call;
        }
    }

    stats.insert(QStringLiteral("totalQsos"), rows.size());
    stats.insert(QStringLiteral("uniqueCalls"), uniqueCalls.size());
    stats.insert(QStringLiteral("uniqueGrids"), uniqueGrids.size());
    stats.insert(QStringLiteral("maxDistance"), maxDistance);
    stats.insert(QStringLiteral("farthestCall"), farthestCall);
    return stats;
}

int DecodiumBridge::importFromAdif(const QString& filename)
{
    ParsedAdifDocument source = loadAdifDocument(filename);
    if (!source.loaded && source.records.isEmpty()) {
        emit errorMessage(QStringLiteral("Impossibile importare ADIF: %1").arg(filename));
        return 0;
    }

    ParsedAdifDocument dest = loadAdifDocument(ensureAdifLogPath());
    QSet<QString> existingKeys;
    for (ParsedAdifRecord const& record : dest.records) {
        QString const key = record.fields.value(QStringLiteral("CALL")).trimmed().toUpper()
            + QLatin1Char('|')
            + normalizedAdifDateTime(record.fields);
        existingKeys.insert(key);
    }

    int imported = 0;
    for (ParsedAdifRecord const& record : source.records) {
        QString const key = record.fields.value(QStringLiteral("CALL")).trimmed().toUpper()
            + QLatin1Char('|')
            + normalizedAdifDateTime(record.fields);
        if (!existingKeys.contains(key)) {
            dest.records.append(record);
            existingKeys.insert(key);
            ++imported;
        }
    }

    if (imported > 0 && !writeAdifDocument(ensureAdifLogPath(), dest)) {
        emit errorMessage(QStringLiteral("Impossibile aggiornare il log ADIF"));
        return 0;
    }

    rebuildWorkedCallsFromDocument(m_workedCalls, dest.records);
    emit qsoCountChanged();
    emit workedCountChanged();
    emit statusMessage(QStringLiteral("ADIF importato: %1 QSO").arg(imported));
    return imported;
}

bool DecodiumBridge::exportToAdif(const QString& filename)
{
    if (filename.isEmpty()) {
        return false;
    }

    QFile::remove(filename);
    bool const copied = QFile::copy(effectiveAdifLogPath(), filename);
    if (!copied) {
        emit errorMessage(QStringLiteral("Impossibile esportare ADIF: %1").arg(filename));
    }
    return copied;
}

bool DecodiumBridge::deleteQso(const QString& call, const QString& dateTime)
{
    ParsedAdifDocument doc = loadAdifDocument(ensureAdifLogPath());
    QString const targetCall = call.trimmed().toUpper();
    QString const targetDateTime = dateTime.trimmed();

    auto const it = std::find_if(doc.records.begin(), doc.records.end(), [&] (ParsedAdifRecord const& record) {
        return record.fields.value(QStringLiteral("CALL")).trimmed().toUpper() == targetCall
            && normalizedAdifDateTime(record.fields) == targetDateTime;
    });
    if (it == doc.records.end()) {
        return false;
    }

    doc.records.erase(it);
    if (!writeAdifDocument(ensureAdifLogPath(), doc)) {
        emit errorMessage(QStringLiteral("Impossibile aggiornare il log ADIF"));
        return false;
    }

    rebuildWorkedCallsFromDocument(m_workedCalls, doc.records);
    emit qsoCountChanged();
    emit workedCountChanged();
    return true;
}

bool DecodiumBridge::editQso(const QString& call, const QString& dateTime, const QVariantMap& newData)
{
    ParsedAdifDocument doc = loadAdifDocument(ensureAdifLogPath());
    QString const targetCall = call.trimmed().toUpper();
    QString const targetDateTime = dateTime.trimmed();

    auto const it = std::find_if(doc.records.begin(), doc.records.end(), [&] (ParsedAdifRecord const& record) {
        return record.fields.value(QStringLiteral("CALL")).trimmed().toUpper() == targetCall
            && normalizedAdifDateTime(record.fields) == targetDateTime;
    });
    if (it == doc.records.end()) {
        return false;
    }

    if (newData.contains(QStringLiteral("call"))) {
        it->fields.insert(QStringLiteral("CALL"), newData.value(QStringLiteral("call")).toString().trimmed().toUpper());
    }
    if (newData.contains(QStringLiteral("grid"))) {
        it->fields.insert(QStringLiteral("GRIDSQUARE"), newData.value(QStringLiteral("grid")).toString().trimmed().toUpper());
    }
    if (newData.contains(QStringLiteral("band"))) {
        it->fields.insert(QStringLiteral("BAND"), newData.value(QStringLiteral("band")).toString().trimmed().toUpper());
    }
    if (newData.contains(QStringLiteral("mode"))) {
        setAdifModeFields(it->fields, newData.value(QStringLiteral("mode")).toString());
    }
    if (newData.contains(QStringLiteral("reportSent"))) {
        it->fields.insert(QStringLiteral("RST_SENT"), newData.value(QStringLiteral("reportSent")).toString().trimmed().toUpper());
    }
    if (newData.contains(QStringLiteral("reportReceived"))) {
        it->fields.insert(QStringLiteral("RST_RCVD"), newData.value(QStringLiteral("reportReceived")).toString().trimmed().toUpper());
    }
    if (newData.contains(QStringLiteral("comment"))) {
        QString const comment = newData.value(QStringLiteral("comment")).toString().trimmed();
        if (comment.isEmpty()) {
            it->fields.remove(QStringLiteral("COMMENT"));
        } else {
            it->fields.insert(QStringLiteral("COMMENT"), comment);
        }
    }

    if (!writeAdifDocument(ensureAdifLogPath(), doc)) {
        emit errorMessage(QStringLiteral("Impossibile aggiornare il log ADIF"));
        return false;
    }

    rebuildWorkedCallsFromDocument(m_workedCalls, doc.records);
    emit qsoCountChanged();
    emit workedCountChanged();
    return true;
}

void DecodiumBridge::testCloudlogApi()
{
    if (m_cloudlogUrl.isEmpty() || m_cloudlogApiKey.isEmpty()) {
        emit errorMessage("Cloudlog: URL o API key mancante");
        return;
    }
    QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    QString url = m_cloudlogUrl + "/api/auth?key=" + m_cloudlogApiKey;
    QNetworkReply* reply = nam->get(QNetworkRequest{QUrl(url)});
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        reply->deleteLater(); nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorMessage("Cloudlog test fallito: " + reply->errorString());
            return;
        }
        QByteArray data = reply->readAll();
        if (data.contains("auth")) emit statusMessage("Cloudlog: API key OK");
        else emit errorMessage("Cloudlog: API key non valida");
    });
}
