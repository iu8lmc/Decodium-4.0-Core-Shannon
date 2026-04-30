#include "DecodiumBridge.h"
#include "DecodiumLogging.hpp"
#include "DecodiumAlertManager.h"
#include "DecodiumDiagnostics.h"
#include "DecodiumPropagationManager.h"
#include "DecodiumDxCluster.h"
#include "Network/MessageClient.hpp"
#include "DxccLookup.h"
#include "DecodiumLegacyBackend.h"
#include "Network/DecodiumPskReporterLite.h"
#include "Network/DecodiumCloudlogLite.h"
#include "Network/DecodiumWsprUploader.h"
#include "Network/NtpClient.hpp"
#include "Network/RemoteCommandServer.hpp"
#include "PrecisionTime.hpp"
#include "SecureSettings.hpp"
#include <QHostAddress>
#include <QProcessEnvironment>
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
#include "Audio/AudioDevice.hpp"
#include "Audio/soundin.h"
#include "Audio/soundout.h"
#include "Modulator/Modulator.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"
#include "commons.h"
#include "widgets/FoxWaveGuard.hpp"
#include "widgets/itoneAndicw.h"
#include "Decoder/decodedtext.h"
#include "DecodiumAudioSink.h"
#include "Radio.hpp"
#include "revision_utils.hpp"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDateTime>
#include <QSet>
#include <QSettings>
#include <QNetworkInterface>
#include <QHostInfo>
#include <QUdpSocket>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
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
#include <algorithm>
#include <QMediaDevices>
#include <QRandomGenerator>
#include <QDebug>
#include <QDesktopServices>
#include <QDataStream>
#include <QUrl>
#include <QRegularExpression>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <complex>
#include <chrono>
#include <limits>
#include <thread>
#ifdef FFTW3_SINGLE_FOUND
#include <fftw3.h>
#endif
#include <vector>

static void bridgeLog(const QString& msg) {
    DIAG_INFO(msg);
}

#ifdef Q_OS_WIN
static QString decodiumSlowQmlStartupFlagPath()
{
    return QDir {QStandardPaths::writableLocation(QStandardPaths::TempLocation)}
        .absoluteFilePath(QStringLiteral("decodium-slow-qml-startup.flag"));
}

static QString decodiumStartupLogPath()
{
    return QDir {QStandardPaths::writableLocation(QStandardPaths::TempLocation)}
        .absoluteFilePath(QStringLiteral("decodium-start.log"));
}

static void appendStartupLogLine(const QString& line)
{
    QFile file {decodiumStartupLogPath()};
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream out {&file};
        out << line << '\n';
    }
}

static void writeSlowQmlStartupMarker(const QString& reason)
{
    QFile flagFile {decodiumSlowQmlStartupFlagPath()};
    if (flagFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out {&flagFile};
        out << (reason.trimmed().isEmpty()
                    ? QStringLiteral("QML startup exceeded watchdog threshold")
                    : reason.trimmed())
            << '\n'
            << QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
            << '\n';
    }
}
#endif

static constexpr int kSpecialOpNone = 0;
static constexpr int kSpecialOpFox = 6;
static constexpr int kSpecialOpHound = 7;
static constexpr int kSpecialOpMax = 9;
static QAudioDevice findOutputDevice(const QString& name, bool* requestedDeviceFound = nullptr);

static void storeBridgePrecomputedTxWave(QString const& mode, QVector<float> const& wave)
{
    int constexpr maxSamples {FOXCOM_WAVE_SIZE};
    int const copySamples = qMin(maxSamples, wave.size());
    foxcom_ensure_wave(&foxcom_);
    QWriteLocker waveLock {&fox_wave_lock()};
    if (copySamples > 0) {
        std::copy_n(wave.constBegin(), copySamples, foxcom_.wave);
        std::fill(foxcom_.wave + copySamples, foxcom_.wave + maxSamples, 0.0f);
        precomputed_tx_wave_mode() = mode;
        precomputed_tx_wave_buffer() = wave.mid(0, copySamples);
    } else if (precomputed_tx_wave_mode() == mode) {
        precomputed_tx_wave_mode().clear();
        precomputed_tx_wave_buffer().clear();
        std::fill(foxcom_.wave, foxcom_.wave + maxSamples, 0.0f);
    }
}

static bool audioDeviceNameMatches(QString const& lhs, QString const& rhs)
{
    QString const a = lhs.trimmed();
    QString const b = rhs.trimmed();
    if (a.isEmpty() || b.isEmpty()) {
        return false;
    }
    return a.compare(b, Qt::CaseInsensitive) == 0
        || a.contains(b, Qt::CaseInsensitive)
        || b.contains(a, Qt::CaseInsensitive);
}

static bool hasAudioDeviceNamed(QList<QAudioDevice> const& devices, QString const& name)
{
    if (name.trimmed().isEmpty()) {
        return false;
    }
    for (QAudioDevice const& device : devices) {
        if (audioDeviceNameMatches(device.description(), name)) {
            return true;
        }
    }
    return false;
}

static QString audioChannelSettingValue(int channel)
{
    return QString::fromLatin1(
        AudioDevice::toString(static_cast<AudioDevice::Channel>(qBound(0, channel, 3))));
}

static QString aliasedBridgeSettingKey(const QString& key)
{
    if (key == QStringLiteral("RemoteWebEnabled")) return QStringLiteral("WebAppEnabled");
    if (key == QStringLiteral("RemoteHttpPort")) return QStringLiteral("WebAppHttpPort");
    if (key == QStringLiteral("RemoteWsPort")) return QStringLiteral("WebAppWsPort");
    if (key == QStringLiteral("RemoteWsBind")) return QStringLiteral("WebAppBind");
    if (key == QStringLiteral("RemoteUser")) return QStringLiteral("WebAppUser");
    if (key == QStringLiteral("RemoteToken")) return QStringLiteral("WebAppToken");
    if (key == QStringLiteral("PowerBandTXMemory")) return QStringLiteral("pwrBandTxMemory");
    if (key == QStringLiteral("PowerBandTuneMemory")) return QStringLiteral("pwrBandTuneMemory");
    if (key == QStringLiteral("ShowDXCC")) return QStringLiteral("DXCCEntity");
    if (key == QStringLiteral("ShowCountryNames")) return QStringLiteral("AlwaysShowCountryNames");
    if (key == QStringLiteral("InsertBlankLine")) return QStringLiteral("InsertBlank");
    if (key == QStringLiteral("TXMessagesToRX")) return QStringLiteral("Tx2QSO");
    if (key == QStringLiteral("LogAsRTTY")) return QStringLiteral("toRTTY");
    if (key == QStringLiteral("Log4DigitGrids")) return QStringLiteral("Log4digitGrids");
    if (key == QStringLiteral("SpecOpInComments")) return QStringLiteral("specOptoComments");
    if (key == QStringLiteral("dBReportsToComments")) return QStringLiteral("dBtoComments");
    if (key == QStringLiteral("TXFrequency")) return QStringLiteral("txFrequency");
    if (key == QStringLiteral("TXDelay")) return QStringLiteral("TxDelay");
    if (key == QStringLiteral("AllowTXQSY")) return QStringLiteral("TxQSYAllowed");
    if (key == QStringLiteral("DisableTXon73")) return QStringLiteral("73TxDisable");
    if (key == QStringLiteral("RepeatTXuntil73")) return QStringLiteral("RepeatTx");
    if (key == QStringLiteral("TXWatchdog")) return QStringLiteral("TxWatchdog");
    if (key == QStringLiteral("TuneWatchdogEnabled")) return QStringLiteral("TuneWatchdog");
    if (key == QStringLiteral("TuneWatchdogMin")) return QStringLiteral("TuneWatchdogTime");
    if (key == QStringLiteral("CWIDafter73")) return QStringLiteral("After73");
    if (key == QStringLiteral("CWIDInterval")) return QStringLiteral("IDint");
    if (key == QStringLiteral("AlternateF1F6Bindings")) return QStringLiteral("AlternateBindings");
    if (key == QStringLiteral("OrangeCallsigns")) return QStringLiteral("HighlightOrangeCallsigns");
    if (key == QStringLiteral("BlueCallsigns")) return QStringLiteral("HighlightBlueCallsigns");
    if (key == QStringLiteral("kHzWithoutk")) return QStringLiteral("kHzWithoutK");
    if (key == QStringLiteral("ForceCall1st")) return QStringLiteral("ForceCallFirst");
    if (key == QStringLiteral("EnableWaitFeatures")) return QStringLiteral("WaitFeaturesEnabled");
    if (key == QStringLiteral("EraseBandActivity")) return QStringLiteral("erase_BandActivity");
    if (key == QStringLiteral("ClearDXGrid")) return QStringLiteral("clear_DXgrid");
    if (key == QStringLiteral("ClearDXCall")) return QStringLiteral("clear_DXcall");
    if (key == QStringLiteral("RXtoTXafterQSO")) return QStringLiteral("set_RXtoTX");
    if (key == QStringLiteral("AlternateEraseButton")) return QStringLiteral("AlternateEraseButtonBehavior");
    if (key == QStringLiteral("DisableButtonColoring")) return QStringLiteral("TxWarningDisabled");
    if (key == QStringLiteral("FieldDayExchange")) return QStringLiteral("Field_Day_Exchange");
    if (key == QStringLiteral("RTTYExchange")) return QStringLiteral("RTTY_Exchange");
    if (key == QStringLiteral("ContestName")) return QStringLiteral("Contest_Name");
    if (key == QStringLiteral("IndividualContestName")) return QStringLiteral("Individual_Contest_Name");
    if (key == QStringLiteral("NCCCSprint")) return QStringLiteral("NCCC_Sprint");
    if (key == QStringLiteral("DTClampSynced")) return QStringLiteral("DTClampSynced_ms");
    if (key == QStringLiteral("DTClampUnsynced")) return QStringLiteral("DTClampUnsynced_ms");
    if (key == QStringLiteral("OTPInterval")) return QStringLiteral("OTPinterval");
    if (key == QStringLiteral("OTPURL")) return QStringLiteral("OTPUrl");
    if (key == QStringLiteral("AlertMyCall")) return QStringLiteral("alert_MyCall");
    if (key == QStringLiteral("AlertNewDXCC")) return QStringLiteral("alert_DXCC");
    if (key == QStringLiteral("AlertNewDXCCBand")) return QStringLiteral("alert_DXCCOB");
    if (key == QStringLiteral("AlertNewGrid")) return QStringLiteral("alert_Grid");
    if (key == QStringLiteral("AlertNewGridBand")) return QStringLiteral("alert_GridOB");
    if (key == QStringLiteral("AlertNewContinent")) return QStringLiteral("alert_Continent");
    if (key == QStringLiteral("AlertNewContinentBand")) return QStringLiteral("alert_ContinentOB");
    if (key == QStringLiteral("AlertNewCQZone")) return QStringLiteral("alert_CQZ");
    if (key == QStringLiteral("AlertNewCQZoneBand")) return QStringLiteral("alert_CQZOB");
    if (key == QStringLiteral("AlertNewITUZone")) return QStringLiteral("alert_ITUZ");
    if (key == QStringLiteral("AlertNewITUZoneBand")) return QStringLiteral("alert_ITUZOB");
    if (key == QStringLiteral("AlertDXCallGrid")) return QStringLiteral("alert_DXcall");
    if (key == QStringLiteral("AlertQSYMessage")) return QStringLiteral("alert_QSYmessage");
    if (key == QStringLiteral("BlacklistEnabled")) return QStringLiteral("Blacklisted");
    if (key == QStringLiteral("WhitelistEnabled")) return QStringLiteral("Whitelisted");
    if (key == QStringLiteral("AlwaysPassEnabled")) return QStringLiteral("AlwaysPass");
    if (key == QStringLiteral("FiltersWaitPounceOnly")) return QStringLiteral("FiltersForWaitAndPounceOnly");
    if (key == QStringLiteral("FiltersCallingOnly")) return QStringLiteral("FiltersForWord2");
    static QRegularExpression const alwaysPassAlias {
        QStringLiteral("^AlwaysPass([1-9]|1[0-2])$")
    };
    QRegularExpressionMatch const alwaysPassMatch = alwaysPassAlias.match(key);
    if (alwaysPassMatch.hasMatch()) {
        return QStringLiteral("Pass%1").arg(alwaysPassMatch.captured(1));
    }
    return key;
}

static QFont fallbackBridgeFont(QString const& fallbackFamily, int fallbackPointSize)
{
    static QFont const defaultApplicationFont = QGuiApplication::font();
    QFont font = defaultApplicationFont;
    if (!fallbackFamily.trimmed().isEmpty()) {
        font.setFamily(fallbackFamily.trimmed());
    }
    if (fallbackPointSize > 0) {
        font.setPointSize(fallbackPointSize);
    }
    return font;
}

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

static QString resolveSecureSettingFromProfiles(QString const& settingKey,
                                                QString const& rawValue,
                                                QStringList profiles)
{
    QString plain = rawValue.trimmed();
    if (secure_settings::is_placeholder(plain)) {
        plain.clear();
    }

    profiles.removeAll(QString {});
    profiles.removeDuplicates();
    if (!profiles.contains(QStringLiteral("DEFAULT"), Qt::CaseInsensitive)) {
        profiles << QStringLiteral("DEFAULT");
    }

    auto const& backend = secure_settings::default_backend();
    for (QString profile : profiles) {
        profile = profile.trimmed().toUpper();
        if (profile.isEmpty()) {
            continue;
        }

        auto const lookup = backend.lookup(secure_settings::service(profile), settingKey);
        if (lookup.found) {
            return lookup.value.trimmed();
        }
    }

    return plain;
}

static QString normalizeUtcDisplayToken(QString raw)
{
    raw.remove(QRegularExpression {R"([^0-9])"});
    QTime const nowUtc = QDateTime::currentDateTimeUtc().time();
    if (raw.size() >= 6) {
        return raw.left(6);
    }
    if (raw.size() == 4) {
        return raw;
    }
    if (raw.isEmpty()) {
        return nowUtc.toString(QStringLiteral("HHmmss"));
    }
    return raw;
}

static int secondsFromUtcDisplayToken(QString raw)
{
    raw = normalizeUtcDisplayToken(raw);
    if (raw.size() >= 6) {
        bool ok = false;
        int const sec = raw.mid(4, 2).toInt(&ok);
        if (ok) {
            return qBound(0, sec, 59);
        }
    }
    return -1;
}

static bool bridgeTxPeriodIsEven(int txPeriod)
{
    // Bridge runtime convention:
    //   txPeriod == 1  -> legacy txFirst == true  -> first/even slot (:00/:30)
    //   txPeriod == 0  -> legacy txFirst == false -> second/odd slot (:15/:45)
    return txPeriod != 0;
}

static QString decodeDedupKey(QString const& time,
                              QString const& freq,
                              QString const& message)
{
    return normalizeUtcDisplayToken(time).trimmed()
        + QLatin1Char('|')
        + freq.trimmed()
        + QLatin1Char('|')
        + message.trimmed();
}

// FT2 async turbo (100ms windows): lo stage7 DSP può riportare lo stesso messaggio
// in scansioni consecutive con freq leggermente diversa (±1-10 Hz di jitter). Quantizza
// la frequenza in bucket da 20 Hz così gli stessi decode non producono righe duplicate
// nella decodeList, mantenendo comunque la distinzione tra QSO vicini su bande diverse.
static QString decodeDedupKeyFt2Async(QString const& time,
                                      QString const& freq,
                                      QString const& message)
{
    bool ok = false;
    int const f = freq.trimmed().toInt(&ok);
    QString const fBucket = ok ? QString::number((f / 20) * 20) : freq.trimmed();
    return normalizeUtcDisplayToken(time).trimmed()
        + QLatin1Char('|')
        + fBucket
        + QLatin1Char('|')
        + message.trimmed();
}

static int legacyDepthBitsToModernDepth(int legacyBits)
{
    int depth = legacyBits & 0x7;
    if (depth <= 0) {
        depth = 3;
    }
    if (legacyBits & 0xF0) {
        depth = 4;
    }
    return qBound(1, depth, 4);
}

static int modernDepthToLegacyBits(int depth, bool avgDecodeEnabled, bool deepSearchEnabled)
{
    int bits = qBound(1, depth, 3);
    if (avgDecodeEnabled) {
        bits |= 0x10;
    }
    if (deepSearchEnabled) {
        bits |= 0x20;
    }
    return bits;
}

static QDateTime approxUtcDateTimeForDisplayToken(QString const& utcToken)
{
    QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    QString const normalized = normalizeUtcDisplayToken(utcToken);

    QTime decodedTime;
    if (normalized.size() >= 6) {
        decodedTime = QTime::fromString(normalized.left(6), QStringLiteral("hhmmss"));
    } else if (normalized.size() == 4) {
        decodedTime = QTime::fromString(normalized, QStringLiteral("hhmm"));
    }

    if (!decodedTime.isValid()) {
        return nowUtc;
    }

    QDate decodedDate = nowUtc.date();
    if (decodedTime > nowUtc.time().addSecs(60)) {
        decodedDate = decodedDate.addDays(-1);
    }

    nowUtc.setDate(decodedDate);
    nowUtc.setTime(decodedTime);
    return nowUtc;
}

static void sortDecodeEntriesChronologically(QVariantList& entries)
{
    struct IndexedEntry
    {
        QVariantMap entry;
        int originalIndex {0};
        QDateTime utc;
    };

    QVector<IndexedEntry> indexed;
    indexed.reserve(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        QVariantMap const entry = entries.at(i).toMap();
        indexed.push_back(IndexedEntry {
            entry,
            i,
            approxUtcDateTimeForDisplayToken(entry.value(QStringLiteral("time")).toString())
        });
    }

    std::stable_sort(indexed.begin(), indexed.end(), [] (IndexedEntry const& lhs, IndexedEntry const& rhs) {
        bool const lhsValid = lhs.utc.isValid();
        bool const rhsValid = rhs.utc.isValid();
        if (lhsValid && rhsValid && lhs.utc != rhs.utc) {
            return lhs.utc < rhs.utc;
        }
        if (lhsValid != rhsValid) {
            return lhsValid;
        }
        return lhs.originalIndex < rhs.originalIndex;
    });

    QVariantList sorted;
    sorted.reserve(entries.size());
    for (IndexedEntry const& item : std::as_const(indexed)) {
        sorted.append(item.entry);
    }
    entries = sorted;
}

static int displayDedupPeriodMsForMode(QString mode)
{
    mode = mode.trimmed().toUpper();
    if (mode == QStringLiteral("FT2")) return 3750;
    if (mode == QStringLiteral("FT4")) return 7500;
    if (mode == QStringLiteral("FT8")) return 15000;
    return 0;
}

static QString decodeDisplayTimeKey(QString const& time, QString const& mode)
{
    QString const normalized = normalizeUtcDisplayToken(time).trimmed();
    int const periodMs = displayDedupPeriodMsForMode(mode);
    if (periodMs <= 0) {
        return normalized;
    }

    QDateTime const approxUtc = approxUtcDateTimeForDisplayToken(normalized);
    if (!approxUtc.isValid()) {
        return normalized;
    }

    qint64 const slotIndex = approxUtc.toMSecsSinceEpoch() / static_cast<qint64>(periodMs);
    QTime const slotTime = QDateTime::fromMSecsSinceEpoch(slotIndex * static_cast<qint64>(periodMs), Qt::UTC).time();
    return slotTime.toString(QStringLiteral("HHmmss"));
}

static QString decodeDisplayFrequencyKey(QString const& freq, QString const& mode)
{
    bool ok = false;
    int const value = freq.trimmed().toInt(&ok);
    if (!ok) {
        return freq.trimmed();
    }

    int const bucketHz = displayDedupPeriodMsForMode(mode) > 0 ? 20 : 1;
    return QString::number(qRound(static_cast<double>(value) / bucketHz) * bucketHz);
}

static QString decodeDisplayDuplicateKey(QVariantMap const& entry, QString const& fallbackMode)
{
    QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
    if (message.isEmpty()) {
        return {};
    }

    QString mode = entry.value(QStringLiteral("mode")).toString().trimmed().toUpper();
    if (mode.isEmpty()) {
        mode = fallbackMode.trimmed().toUpper();
    }

    return mode
        + QLatin1Char('|')
        + decodeDisplayTimeKey(entry.value(QStringLiteral("time")).toString(), mode)
        + QLatin1Char('|')
        + decodeDisplayFrequencyKey(entry.value(QStringLiteral("freq")).toString(), mode)
        + QLatin1Char('|')
        + message
        + QLatin1Char('|')
        + (entry.value(QStringLiteral("isTx")).toBool() ? QStringLiteral("1") : QStringLiteral("0"));
}

static int decodeDisplayEntryScore(QVariantMap const& entry)
{
    int score = 0;
    if (normalizeUtcDisplayToken(entry.value(QStringLiteral("time")).toString()).size() >= 6) {
        score += 10000;
    }
    if (!entry.value(QStringLiteral("dxCountry")).toString().trimmed().isEmpty()) {
        score += 1000;
    }

    bool qualityOk = false;
    int const quality = entry.value(QStringLiteral("quality")).toString().trimmed().toInt(&qualityOk);
    if (qualityOk) {
        score += qBound(0, quality, 100);
    }

    bool snrOk = false;
    int const snr = entry.value(QStringLiteral("db")).toString().trimmed().toInt(&snrOk);
    if (snrOk) {
        score += qBound(-99, snr, 99) + 100;
    }
    return score;
}

static void coalesceDecodeEntriesForDisplay(QVariantList& entries, QString const& fallbackMode)
{
    QHash<QString, int> keptIndexByKey;
    for (int i = 0; i < entries.size(); ) {
        QVariantMap const entry = entries.at(i).toMap();
        QString const key = decodeDisplayDuplicateKey(entry, fallbackMode);
        if (key.isEmpty()) {
            ++i;
            continue;
        }

        auto const it = keptIndexByKey.constFind(key);
        if (it == keptIndexByKey.constEnd()) {
            keptIndexByKey.insert(key, i);
            ++i;
            continue;
        }

        int const keptIndex = it.value();
        QVariantMap const keptEntry = entries.at(keptIndex).toMap();
        if (decodeDisplayEntryScore(entry) > decodeDisplayEntryScore(keptEntry)) {
            entries[keptIndex] = entry;
        }
        entries.removeAt(i);
    }
}

static void normalizeDecodeEntriesForDisplay(QVariantList& entries,
                                             int maxEntries = 1500,
                                             QString const& fallbackMode = QString())
{
    coalesceDecodeEntriesForDisplay(entries, fallbackMode);
    sortDecodeEntriesChronologically(entries);
    if (maxEntries > 0 && entries.size() > maxEntries) {
        entries = entries.mid(entries.size() - maxEntries);
    }
}

static int utcTokenForSlotStart(qint64 slotIndex, int periodMs)
{
    if (slotIndex < 0 || periodMs <= 0) {
        return 0;
    }

    qint64 const slotStartMs = slotIndex * static_cast<qint64>(periodMs);
    QTime const slotTime = QDateTime::fromMSecsSinceEpoch(slotStartMs, Qt::UTC).time();
    if (periodMs < 60000) {
        return slotTime.hour() * 10000 + slotTime.minute() * 100 + slotTime.second();
    }
    return slotTime.hour() * 100 + slotTime.minute();
}

static QString utcDisplayTokenForSlotStart(qint64 slotIndex, int periodMs)
{
    if (slotIndex < 0 || periodMs <= 0) {
        return QString();
    }

    qint64 const slotStartMs = slotIndex * static_cast<qint64>(periodMs);
    QTime const slotTime = QDateTime::fromMSecsSinceEpoch(slotStartMs, Qt::UTC).time();
    QString const fmt = periodMs < 60000 ? QStringLiteral("HHmmss")
                                         : QStringLiteral("HHmm");
    return slotTime.toString(fmt);
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

static int countAdifRecordsLightweight(QString const& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }

    int count = 0;
    QByteArray carry;
    while (!file.atEnd()) {
        QByteArray chunk = carry + file.read(64 * 1024).toUpper();
        int cursor = 0;
        while (true) {
            int const pos = chunk.indexOf("<EOR", cursor);
            if (pos < 0) {
                break;
            }
            ++count;
            cursor = pos + 4;
        }
        carry = chunk.right(3);
    }
    return count;
}

struct QsoLogSnapshot
{
    QString path;
    QDateTime modified;
    qint64 size {-1};
    QVariantList rows;
    QVariantMap stats;
};

static QVariantMap qsoStatsFromRows(QVariantList const& rows)
{
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

    QVariantMap stats;
    stats.insert(QStringLiteral("totalQsos"), rows.size());
    stats.insert(QStringLiteral("uniqueCalls"), uniqueCalls.size());
    stats.insert(QStringLiteral("uniqueGrids"), uniqueGrids.size());
    stats.insert(QStringLiteral("maxDistance"), maxDistance);
    stats.insert(QStringLiteral("farthestCall"), farthestCall);
    return stats;
}

static QsoLogSnapshot buildQsoLogSnapshot(QString const& path, QString const& myGrid)
{
    QElapsedTimer timer;
    timer.start();

    QFileInfo const info(path);
    QsoLogSnapshot snapshot;
    snapshot.path = path;
    snapshot.modified = info.exists() ? info.lastModified() : QDateTime();
    snapshot.size = info.exists() ? info.size() : 0;

    ParsedAdifDocument const doc = loadAdifDocument(path);
    QList<QVariantMap> sortedRows;
    sortedRows.reserve(doc.records.size());
    for (ParsedAdifRecord const& record : doc.records) {
        sortedRows.append(qsoMapFromAdifRecord(record.fields, myGrid));
    }

    std::sort(sortedRows.begin(), sortedRows.end(), [] (QVariantMap const& lhs, QVariantMap const& rhs) {
        return lhs.value(QStringLiteral("dateTime")).toString()
             > rhs.value(QStringLiteral("dateTime")).toString();
    });

    snapshot.rows.reserve(sortedRows.size());
    for (QVariantMap const& row : sortedRows) {
        snapshot.rows.append(row);
    }
    snapshot.stats = qsoStatsFromRows(snapshot.rows);

    if (timer.elapsed() > 250) {
        DIAG_INFO(QStringLiteral("QSO log cache built: records=%1 elapsed=%2ms path=%3")
                      .arg(snapshot.rows.size())
                      .arg(timer.elapsed())
                      .arg(path));
    }
    return snapshot;
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

static QString maidenheadFromLonLat(double lonEast, double lat)
{
    if (!std::isfinite(lonEast) || !std::isfinite(lat)) {
        return {};
    }

    while (lonEast < -180.0) lonEast += 360.0;
    while (lonEast >= 180.0) lonEast -= 360.0;
    lat = qBound(-89.999999, lat, 89.999999);

    double x = lonEast + 180.0;
    double y = lat + 90.0;
    int const fieldLon = qBound(0, static_cast<int>(std::floor(x / 20.0)), 17);
    int const fieldLat = qBound(0, static_cast<int>(std::floor(y / 10.0)), 17);
    x -= fieldLon * 20.0;
    y -= fieldLat * 10.0;
    int const squareLon = qBound(0, static_cast<int>(std::floor(x / 2.0)), 9);
    int const squareLat = qBound(0, static_cast<int>(std::floor(y)), 9);

    QString grid;
    grid.reserve(4);
    grid.append(QChar(QLatin1Char('A' + fieldLon)));
    grid.append(QChar(QLatin1Char('A' + fieldLat)));
    grid.append(QChar(QLatin1Char('0' + squareLon)));
    grid.append(QChar(QLatin1Char('0' + squareLat)));
    return grid;
}

static int decodeSnrOrDefault(QString const& text, int fallback = -10)
{
    bool ok = false;
    int const parsed = text.trimmed().toInt(&ok);
    return ok ? qBound(-50, parsed, 49) : fallback;
}

static QString normalizeCallToken(QString token)
{
    token = token.trimmed();
    token.remove(QChar('<'));
    token.remove(QChar('>'));
    if (token.endsWith(QChar(';'))) {
        token.chop(1);
    }
    return token.trimmed();
}

static bool isPlaceholderCallToken(QString const& token)
{
    QString const upper = normalizeCallToken(token).trimmed().toUpper();
    return upper == QStringLiteral("...")
        || upper == QStringLiteral("<...>");
}

static QString normalizedBaseCall(QString token)
{
    token = normalizeCallToken(token).toUpper();
    if (token.isEmpty()) {
        return {};
    }
    return Radio::base_callsign(token).trimmed().toUpper();
}

static QString normalizeWorldMapCall(QString call)
{
    return normalizeCallToken(call).trimmed().toUpper();
}

static QString worldMapCallKey(QString call)
{
    QString const normalized = normalizeWorldMapCall(call);
    if (normalized.isEmpty()) {
        return {};
    }

    QString base = Radio::base_callsign(normalized).trimmed().toUpper();
    return base.isEmpty() ? normalized : base;
}

static bool tokenMatchesCall(QString const& token,
                             QString const& fullCall,
                             QString const& baseCall)
{
    QString const upper = normalizeCallToken(token).toUpper();
    if (upper.isEmpty()) {
        return false;
    }

    QString const fullUpper = normalizeCallToken(fullCall).toUpper();
    QString const baseUpper = normalizeCallToken(baseCall).toUpper();
    QString const tokenBase = normalizedBaseCall(upper);

    return (!fullUpper.isEmpty() && (upper == fullUpper || tokenBase == fullUpper))
        || (!baseUpper.isEmpty() && (upper == baseUpper || tokenBase == baseUpper));
}

static QStringList normalizedMessageTokens(QString const& message)
{
    QStringList const rawTokens = message.toUpper().split(QRegularExpression("\\s+"),
                                                          Qt::SkipEmptyParts);
    QStringList normalized;
    normalized.reserve(rawTokens.size());
    for (QString const& token : rawTokens) {
        QString const cleaned = normalizeCallToken(token);
        if (!cleaned.isEmpty()) {
            normalized.push_back(cleaned);
        }
    }
    return normalized;
}

static bool messageContainsCallToken(QString const& message,
                                     QString const& fullCall,
                                     QString const& baseCall)
{
    if (normalizeCallToken(fullCall).isEmpty() && normalizeCallToken(baseCall).isEmpty()) {
        return false;
    }

    QStringList const tokens = normalizedMessageTokens(message);
    for (QString const& token : tokens) {
        if (tokenMatchesCall(token, fullCall, baseCall)) {
            return true;
        }
    }
    return false;
}

static bool isQsoExchangeToken(QString const& token)
{
    QString const upper = normalizeCallToken(token).toUpper();
    if (upper.isEmpty()) {
        return false;
    }

    static const QRegularExpression signedReportPattern {
        R"(\A(?:R[-+]\d{2}|[-+]\d{2}|R\d{2})\z)"
    };

    return upper == QStringLiteral("R")
        || upper == QStringLiteral("73")
        || upper == QStringLiteral("RR73")
        || upper == QStringLiteral("RRR")
        || upper == QStringLiteral("TU")
        || isGridTokenStrict(upper)
        || signedReportPattern.match(upper).hasMatch();
}

static bool messageContainsExchangeToken(QString const& message)
{
    QStringList const tokens = normalizedMessageTokens(message);
    for (QString const& token : tokens) {
        if (isQsoExchangeToken(token)) {
            return true;
        }
    }
    return false;
}

static bool isPlainSignalReportToken(QString const& token)
{
    static const QRegularExpression plainReportPattern {
        R"(\A[-+]\d{2}\z)"
    };
    return plainReportPattern.match(normalizeCallToken(token).toUpper()).hasMatch();
}

static bool isRogerSignalReportToken(QString const& token)
{
    static const QRegularExpression rogerReportPattern {
        R"(\A(?:R[-+]\d{2}|R\d{2})\z)"
    };
    return rogerReportPattern.match(normalizeCallToken(token).toUpper()).hasMatch();
}

static bool isDirectedDecodeTerminalToken(QString const& token)
{
    QString const upper = normalizeCallToken(token).toUpper();
    return upper == QStringLiteral("73")
        || upper == QStringLiteral("RR73")
        || upper == QStringLiteral("RRR")
        || isGridTokenStrict(upper)
        || isPlainSignalReportToken(upper)
        || isRogerSignalReportToken(upper);
}

static bool isDirectedDecodePayloadValid(QStringList const& payload,
                                         bool contestExchangeEnabled,
                                         QString* reason)
{
    if (payload.isEmpty()) {
        if (reason) *reason = QStringLiteral("missing directed payload");
        return false;
    }

    if (payload.size() == 1) {
        if (isDirectedDecodeTerminalToken(payload.constFirst())) {
            return true;
        }
        if (reason) *reason = QStringLiteral("unknown directed payload");
        return false;
    }

    if (payload.size() == 2) {
        QString const first = normalizeCallToken(payload.at(0)).toUpper();
        QString const second = normalizeCallToken(payload.at(1)).toUpper();
        if (second == QStringLiteral("TU")
            && (isPlainSignalReportToken(first) || isRogerSignalReportToken(first))) {
            return true;
        }
        if (first == QStringLiteral("R") && isGridTokenStrict(second)) {
            if (contestExchangeEnabled) {
                return true;
            }
            if (reason) *reason = QStringLiteral("contest R+grid payload while contest is disabled");
            return false;
        }
        if (reason) *reason = QStringLiteral("unknown two-token directed payload");
        return false;
    }

    if (!contestExchangeEnabled) {
        for (int i = 0; i + 1 < payload.size(); ++i) {
            QString const current = normalizeCallToken(payload.at(i)).toUpper();
            QString const next = normalizeCallToken(payload.at(i + 1)).toUpper();
            if (current == QStringLiteral("R") && isGridTokenStrict(next)) {
                if (reason) *reason = QStringLiteral("contest R+grid payload while contest is disabled");
                return false;
            }
        }
    }

    return true;
}

static int txVisualStage(QString const& message)
{
    QStringList const tokens = normalizedMessageTokens(message);
    if (tokens.isEmpty()) {
        return -1;
    }

    QString const last = tokens.constLast();
    if (last == QStringLiteral("73")) {
        return 5;
    }
    if (last == QStringLiteral("RR73") || last == QStringLiteral("RRR")) {
        return 4;
    }
    if (isRogerSignalReportToken(last)) {
        return 3;
    }
    if (last == QStringLiteral("TU")
        && tokens.size() >= 2
        && isRogerSignalReportToken(tokens.at(tokens.size() - 2))) {
        return 3;
    }
    if (isPlainSignalReportToken(last)) {
        return 2;
    }
    if (isGridTokenStrict(last)) {
        return 1;
    }
    return 0;
}

static bool shouldCoalesceVisualTxRows(QString const& mode)
{
    QString const upperMode = mode.trimmed().toUpper();
    return upperMode == QStringLiteral("FT8")
        || upperMode == QStringLiteral("FT4")
        || upperMode == QStringLiteral("FT2");
}

static QString canonicalApplicationDecodeMode(QString mode)
{
    QString const upperMode = mode.trimmed().toUpper();
    if (upperMode == QStringLiteral("FT8")
        || upperMode == QStringLiteral("FT2")
        || upperMode == QStringLiteral("FT4")
        || upperMode == QStringLiteral("Q65")
        || upperMode == QStringLiteral("MSK144")
        || upperMode == QStringLiteral("JT65")
        || upperMode == QStringLiteral("JT9")
        || upperMode == QStringLiteral("JT4")
        || upperMode == QStringLiteral("FST4")
        || upperMode == QStringLiteral("FST4W")
        || upperMode == QStringLiteral("WSPR")
        || upperMode.startsWith(QStringLiteral("FST4-"))
        || upperMode.startsWith(QStringLiteral("FST4W-"))) {
        return upperMode;
    }
    if (upperMode == QStringLiteral("ECHO")) {
        return QStringLiteral("Echo");
    }
    return {};
}

static QString normalizedVisualTxTimeToken(QString const& rawTime, QString const& mode)
{
    QString const normalizedTime = normalizeUtcDisplayToken(rawTime);
    if (!shouldCoalesceVisualTxRows(mode)) {
        return normalizedTime;
    }

    QString const upperMode = mode.trimmed().toUpper();
    int periodMs = 0;
    if (upperMode == QStringLiteral("FT8")) {
        periodMs = 15000;
    } else if (upperMode == QStringLiteral("FT4")) {
        periodMs = 7500;
    } else if (upperMode == QStringLiteral("FT2")) {
        periodMs = 3750;
    }
    if (periodMs <= 0 || periodMs >= 60000) {
        return normalizedTime;
    }

    QDateTime const approxUtc = approxUtcDateTimeForDisplayToken(normalizedTime);
    if (!approxUtc.isValid()) {
        return normalizedTime;
    }

    qint64 const slotIndex = approxUtc.toMSecsSinceEpoch() / static_cast<qint64>(periodMs);
    QString const slotTime = utcDisplayTokenForSlotStart(slotIndex, periodMs);
    return slotTime.isEmpty() ? normalizedTime : slotTime;
}

static QString rxPaneVisualTxSlotKey(QVariantMap const& entry, QString const& mode)
{
    if (!shouldCoalesceVisualTxRows(mode) || !entry.value(QStringLiteral("isTx")).toBool()) {
        return {};
    }

    QString const time = normalizedVisualTxTimeToken(entry.value(QStringLiteral("time")).toString(), mode).trimmed();
    QString const freq = entry.value(QStringLiteral("freq")).toString().trimmed();
    if (time.isEmpty() || freq.isEmpty()) {
        return {};
    }

    return time + QStringLiteral("|") + freq;
}

static void coalesceRxPaneTxRows(QVariantList& entries, QString const& mode)
{
    if (!shouldCoalesceVisualTxRows(mode) || entries.isEmpty()) {
        return;
    }

    QHash<QString, int> keptIndexBySlot;
    for (int i = 0; i < entries.size(); ) {
        QVariantMap const entry = entries.at(i).toMap();
        QString const slotKey = rxPaneVisualTxSlotKey(entry, mode);
        if (slotKey.isEmpty()) {
            ++i;
            continue;
        }

        auto const it = keptIndexBySlot.constFind(slotKey);
        if (it == keptIndexBySlot.constEnd()) {
            keptIndexBySlot.insert(slotKey, i);
            ++i;
            continue;
        }

        int const keptIndex = it.value();
        QVariantMap const keptEntry = entries.at(keptIndex).toMap();
        int const keptStage = txVisualStage(keptEntry.value(QStringLiteral("message")).toString());
        int const currentStage = txVisualStage(entry.value(QStringLiteral("message")).toString());
        int const keptLength = keptEntry.value(QStringLiteral("message")).toString().trimmed().size();
        int const currentLength = entry.value(QStringLiteral("message")).toString().trimmed().size();
        bool const preferCurrent =
            (currentStage > keptStage)
            || (currentStage == keptStage && currentLength > keptLength);

        if (preferCurrent) {
            entries[keptIndex] = entry;
        }

        entries.removeAt(i);
    }
}

static QString decodeMirrorEntryKey(QVariantMap const& entry)
{
    QString const time = entry.value(QStringLiteral("isTx")).toBool()
        ? normalizedVisualTxTimeToken(entry.value(QStringLiteral("time")).toString(),
                                      entry.value(QStringLiteral("mode")).toString())
        : entry.value(QStringLiteral("time")).toString();
    return time + "|" +
           entry.value("freq").toString() + "|" +
           entry.value("message").toString() + "|" +
           (entry.value("isTx").toBool() ? QStringLiteral("1") : QStringLiteral("0"));
}

static QString currentTxVisualTimeToken(QString const& mode)
{
    QString const upperMode = mode.trimmed().toUpper();
    int periodMs = 0;
    if (upperMode == QStringLiteral("FT8")) {
        periodMs = 15000;
    } else if (upperMode == QStringLiteral("FT4")) {
        periodMs = 7500;
    } else if (upperMode == QStringLiteral("FT2")) {
        periodMs = 3750;
    }

    if (periodMs > 0 && periodMs < 60000) {
        qint64 const slotIndex = QDateTime::currentMSecsSinceEpoch() / static_cast<qint64>(periodMs);
        QString const slotTime = utcDisplayTokenForSlotStart(slotIndex, periodMs);
        if (!slotTime.isEmpty()) {
            return slotTime;
        }
    }

    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("HHmmss"));
}

static bool txDecodeEntryAlreadyPresent(QVariantList const& entries, QVariantMap const& txEntry)
{
    QString const key = decodeMirrorEntryKey(txEntry);
    int const first = qMax(0, entries.size() - 16);
    for (int i = first; i < entries.size(); ++i) {
        QVariantMap const existing = entries.at(i).toMap();
        if (!existing.value(QStringLiteral("isTx")).toBool()) {
            continue;
        }
        if (decodeMirrorEntryKey(existing) == key) {
            return true;
        }
    }
    return false;
}

static QString decodeMirrorLooseKey(QVariantMap const& entry)
{
    return entry.value("db").toString().trimmed() + "|" +
           entry.value("dt").toString().trimmed() + "|" +
           entry.value("freq").toString().trimmed() + "|" +
           entry.value("message").toString().trimmed() + "|" +
           (entry.value("isTx").toBool() ? QStringLiteral("1") : QStringLiteral("0"));
}

static QStringList readTextLinesFromOffset(QString const& path, qint64 startOffset, int maxLines)
{
    if (maxLines <= 0 || startOffset < 0) {
        return {};
    }

    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    qint64 const size = file.size();
    if (startOffset >= size) {
        file.close();
        return {};
    }

    file.seek(startOffset);
    QByteArray data = file.readAll();
    file.close();

    QStringList lines = QString::fromUtf8(data).split(QRegularExpression {R"([\r\n]+)"},
                                                      Qt::SkipEmptyParts);
    if (lines.size() > maxLines) {
        lines = lines.mid(lines.size() - maxLines);
    }
    return lines;
}

static QString inferPartnerFromDirectedMessage(QString const& message,
                                              QString const& myCall,
                                              QString const& myBase)
{
    QStringList const words = message.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (words.size() < 2) {
        return {};
    }

    QString const first = normalizeCallToken(words.at(0));
    QString const second = normalizeCallToken(words.at(1));
    QString const third = words.size() > 2 ? normalizeCallToken(words.at(2)) : QString {};
    if (first.isEmpty() || second.isEmpty()) {
        return {};
    }

    auto const isMyCall = [&](QString const& token) {
        return token.compare(myCall, Qt::CaseInsensitive) == 0
            || token.compare(myBase, Qt::CaseInsensitive) == 0;
    };
    auto const looksLikePartnerCall = [](QString const& token) {
        QString const upper = token.trimmed().toUpper();
        if (isPlaceholderCallToken(upper)) {
            return false;
        }
        if (upper == QStringLiteral("CQ")
            || upper == QStringLiteral("QRZ")
            || upper == QStringLiteral("DE")
            || upper == QStringLiteral("TU")
            || upper == QStringLiteral("TU;")
            || upper == QStringLiteral("73")
            || upper == QStringLiteral("RR73")
            || upper == QStringLiteral("RRR")) {
            return false;
        }
        QString const base = Radio::base_callsign(upper).trimmed().toUpper();
        return !base.isEmpty() && Radio::is_callsign(base);
    };

    if (isMyCall(first) && looksLikePartnerCall(second)) return second;
    if (isMyCall(second) && looksLikePartnerCall(first)) return first;
    if (first.compare(QStringLiteral("DE"), Qt::CaseInsensitive) == 0
        && isMyCall(second)
        && looksLikePartnerCall(third)) {
        return third;
    }

    return {};
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

void beginConfiguredBridgeSettingsGroup(QSettings& settings)
{
    auto const* app = QCoreApplication::instance();
    if (!app) {
        return;
    }

    QString const configName = app->property("decodiumConfigName").toString().trimmed();
    if (configName.isEmpty()) {
        return;
    }

    settings.beginGroup(QStringLiteral("MultiSettings"));
    settings.beginGroup(configName);
}

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

bool messageCarries73Payload(QString const& message)
{
    QStringList const parts = message.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (QString const& token : parts) {
        QString const upper = token.trimmed().toUpper();
        if (upper == QStringLiteral("73")
            || upper == QStringLiteral("RR73")
            || upper == QStringLiteral("RRR")) {
            return true;
        }
    }
    return false;
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
    // Slider 0..450 → attenuazione legacy reale: 0=0dB (massima uscita), 450=45dB
    return qBound<qreal>(0.0, static_cast<qreal>(outputLevel / 10.0), 45.0);
}

static float rxInputGainFromLevel(double inputLevel)
{
    double const bounded = qBound(0.0, inputLevel, 100.0);
    if (bounded <= 50.0) {
        return static_cast<float>(bounded / 50.0);
    }
    return static_cast<float>(1.0 + ((bounded - 50.0) / 50.0) * 3.0);
}

static qreal txGainFromSlider(double outputLevel)
{
    return static_cast<qreal>(std::pow(10.0, -txAttenuationFromSlider(outputLevel) / 20.0));
}

static int legacyTxAttenuationFromLevel(double outputLevel)
{
    return qRound(qBound(0.0, outputLevel, 450.0));
}

static double txLevelFromLegacyAttenuation(int attenuation)
{
    return qBound(0.0, static_cast<double>(attenuation), 450.0);
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

static qsizetype txAudioBufferBytesForMode(const QAudioFormat& format, const QString& mode, bool tune)
{
    if (!format.isValid()) {
        return 0;
    }

    int frames = tune ? 16384 : (mode == QStringLiteral("FT2") ? 8192 : 16384);
#if defined(Q_OS_WIN)
    // Older Windows machines are more likely to underrun the output callback
    // while FT8/FT4 decode/UI work is still settling. Keep FT2 lower to avoid
    // pushing a short 3.75 s payload too far into the slot.
    frames = tune ? 32768 : (mode == QStringLiteral("FT2") ? 12288 : 32768);
#endif
    return static_cast<qsizetype>(format.bytesForFrames(frames));
}

static int txAudioPrecomputeRetryDelayMs(const QString& mode, bool monitoring)
{
    if (!monitoring) {
        return 0;
    }

    QString const normalized = mode.trimmed().toUpper();
    int periodMs = 0;
    int quietAfterSlotStartMs = 0;
    int quietBeforeSlotEndMs = 0;
    if (normalized == QStringLiteral("FT8")) {
        periodMs = 15000;
        quietAfterSlotStartMs = 8500;
        quietBeforeSlotEndMs = 1200;
    } else if (normalized == QStringLiteral("FT4")) {
        periodMs = 7500;
        quietAfterSlotStartMs = 3500;
        quietBeforeSlotEndMs = 800;
    } else if (normalized == QStringLiteral("FT2")) {
        periodMs = 3750;
        quietAfterSlotStartMs = 900;
        quietBeforeSlotEndMs = 450;
    } else {
        return 0;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    int const msInSlot = static_cast<int>(nowMs % periodMs);
    if (msInSlot < quietAfterSlotStartMs) {
        return qMax(50, quietAfterSlotStartMs - msInSlot);
    }
    int const latestSafeMs = periodMs - quietBeforeSlotEndMs;
    if (msInSlot > latestSafeMs) {
        return qMax(50, periodMs - msInSlot + quietAfterSlotStartMs);
    }
    return 0;
}

static QString expandedLocalFilePath(QString path)
{
    path = QDir::fromNativeSeparators(path.trimmed());
    if (path == QStringLiteral("~")) {
        return QDir::homePath();
    }
    if (path.startsWith(QStringLiteral("~/"))) {
        return QDir::home().absoluteFilePath(path.mid(2));
    }
    return path;
}

static bool ensureParentDirectoryForFile(QString const& filePath, QString* errorMessage = nullptr)
{
    QFileInfo const info(filePath);
    QDir dir = info.dir();
    if (dir.exists() || dir.mkpath(QStringLiteral("."))) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("Impossibile creare la cartella: %1")
                            .arg(QDir::toNativeSeparators(dir.absolutePath()));
    }
    return false;
}

static bool isProbablyCall3TxtPayload(QByteArray const& data, QString* errorMessage = nullptr)
{
    if (data.size() < 1024) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("risposta troppo piccola (%1 byte)").arg(data.size());
        }
        return false;
    }

    QByteArray const head = data.left(4096).trimmed().toLower();
    if (head.startsWith("<!doctype") || head.startsWith("<html")
        || head.contains("404 not found") || head.contains("cloudflare")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("il server ha restituito una pagina HTML/errore invece di CALL3.TXT");
        }
        return false;
    }

    if (!data.contains(',') || !data.contains('\n')) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("formato CALL3.TXT non riconosciuto");
        }
        return false;
    }

    return true;
}

static bool writeMono16WavFile(QString const& path, QVector<float> const& wave, int sampleRate)
{
    if (wave.isEmpty() || sampleRate <= 0) {
        return false;
    }

    QString error;
    if (!ensureParentDirectoryForFile(path, &error)) {
        qWarning() << error;
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot open TX WAV for write:" << path;
        return false;
    }

    QDataStream s(&f);
    s.setByteOrder(QDataStream::LittleEndian);

    quint32 const dataSize = static_cast<quint32>(wave.size() * sizeof(qint16));
    quint32 const fileSize = 36u + dataSize;

    f.write("RIFF", 4);
    s << fileSize;
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    s << quint32(16);
    s << quint16(1);
    s << quint16(1);
    s << quint32(sampleRate);
    s << quint32(sampleRate * sizeof(qint16));
    s << quint16(sizeof(qint16));
    s << quint16(16);
    f.write("data", 4);
    s << dataSize;

    for (float sample : wave) {
        qint16 const v = static_cast<qint16>(qRound(qBound(-1.0f, sample, 1.0f) * 32767.0f));
        s << v;
    }

    return true;
}

static QString buildTxRecordingPath(QString const& mode, bool tune)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    dir += QStringLiteral("/Decodium/recordings");
    QString const stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    QString const tag = tune ? QStringLiteral("tune") : QStringLiteral("tx");
    QString const safeMode = mode.trimmed().isEmpty() ? QStringLiteral("FT8") : mode.trimmed().toUpper();
    return QDir(dir).absoluteFilePath(QStringLiteral("decodium_%1_%2_%3.wav").arg(tag, safeMode, stamp));
}

static QVector<float> buildTuneWaveform(double freq, int sampleRate, int durationSeconds)
{
    int const nFrames = sampleRate * durationSeconds;
    double const twoPi = 2.0 * 3.14159265358979323846;
    double const dphi = twoPi * freq / static_cast<double>(sampleRate);

    QVector<float> wave;
    wave.resize(nFrames);
    double phi = 0.0;
    for (int i = 0; i < nFrames; ++i) {
        wave[i] = static_cast<float>(std::sin(phi));
        phi += dphi;
        if (phi > twoPi) phi -= twoPi;
    }
    return wave;
}

static QVector<float> buildTxWaveformForMessage(QString const& mode,
                                                QString const& msg,
                                                int txFrequency,
                                                QString* errorOut = nullptr)
{
    QVector<float> wave;
    float const freq = static_cast<float>(txFrequency);

    if (mode == QStringLiteral("FT2")) {
        auto enc = decodium::txmsg::encodeFt2(msg);
        if (!enc.ok) {
            if (errorOut) *errorOut = QStringLiteral("Codifica FT2 fallita");
            return {};
        }
        wave = decodium::txwave::generateFt2Wave(enc.tones.constData(), 103, 4 * 288, 48000.0f, freq);
    } else if (mode == QStringLiteral("FT4")) {
        auto enc = decodium::txmsg::encodeFt4(msg);
        if (!enc.ok) {
            if (errorOut) *errorOut = QStringLiteral("Codifica FT4 fallita");
            return {};
        }
        wave = decodium::txwave::generateFt4Wave(enc.tones.constData(), 103, 4 * 576, 48000.0f, freq);
    } else {
        auto enc = decodium::txmsg::encodeFt8(msg);
        if (!enc.ok) {
            if (errorOut) *errorOut = QStringLiteral("Codifica FT8 fallita");
            return {};
        }
        wave = decodium::txwave::generateFt8Wave(enc.tones.constData(), 79, 7680, 2.0f, 48000.0f, freq);
    }

    if (wave.isEmpty() && errorOut) {
        *errorOut = QStringLiteral("Generazione onda TX fallita");
    }
    return wave;
}

static int estimatedSyncPayloadMs(const QString& mode)
{
    if (mode == QStringLiteral("FT8"))
        return qRound((79.0 * 4.0 * 1920.0 * 1000.0) / 48000.0);
    if (mode == QStringLiteral("FT4"))
        return qRound(((103.0 + 2.0) * 4.0 * 576.0 * 1000.0) / 48000.0);
    if (mode == QStringLiteral("FT2"))
        return qRound(((103.0 + 2.0) * 4.0 * 288.0 * 1000.0) / 48000.0);
    return 0;
}

static int syncTxStartBudgetMs(const QString& mode)
{
#if defined(Q_OS_MAC)
    if (mode == QStringLiteral("FT8"))
        return 700;   // 500 ms start delay + small safety margin
    if (mode == QStringLiteral("FT4") || mode == QStringLiteral("FT2"))
        return 500;   // 300 ms start delay + safety margin
    return 400;
#else
    if (mode == QStringLiteral("FT2"))
        return 300;   // ptt1 + sink spin-up safety
    return 350;       // conservative cross-platform margin for sink start/PTT
#endif
}

#if defined(Q_OS_MAC)
static AudioDevice::Channel txOutputChannelForFormat(const QAudioFormat& format, int configuredChannel)
{
    AudioDevice::Channel const requestedChannel =
        static_cast<AudioDevice::Channel>(qBound(0, configuredChannel, 3));
    if (format.channelCount() <= 1) {
        return AudioDevice::Mono;
    }
    return requestedChannel;
}
#endif

// ── helpers PTT / freq che delegano al backend attivo ────────────────────────
static inline bool useLegacyRigControlFallback(DecodiumLegacyBackend* legacy, const QString& backend)
{
    // Disabilitato su tutte le piattaforme — usa sempre il CAT manager QML
    // Il legacy fallback causava problemi su Windows (Bug #1: CAT auto-connect non funziona)
    Q_UNUSED(legacy);
    Q_UNUSED(backend);
    return false;
}

static inline bool legacyOwnsRigControl(DecodiumLegacyBackend* legacy)
{
    return legacy && legacy->available() && legacy->rigControlEnabled();
}

static inline bool isHamlibFamilyBackend(QString const& backend)
{
    return backend == QStringLiteral("hamlib") || backend == QStringLiteral("tci");
}

static inline bool catSignalMatchesBackend(QString const& activeBackend, QString const& signalBackend)
{
    return activeBackend == signalBackend
        || (activeBackend == QStringLiteral("tci") && signalBackend == QStringLiteral("hamlib"));
}

static inline bool activeCatCanPtt(DecodiumCatManager* n, DecodiumTransceiverManager* h, const QString& b,
                                   DecodiumOmniRigManager* o = nullptr, DecodiumLegacyBackend* legacy = nullptr)
{
    if (useLegacyRigControlFallback(legacy, b)) return true;
    if (b=="native") return n->canPtt();
    if (b=="omnirig" && o) return o->canPtt();
    return h->canPtt();
}

static inline void activeCatSetPtt(DecodiumCatManager* n, DecodiumTransceiverManager* h, const QString& b, bool on,
                                   DecodiumOmniRigManager* o = nullptr, DecodiumLegacyBackend* legacy = nullptr)
{
    if (useLegacyRigControlFallback(legacy, b)) {
        legacy->setRigPtt(on);
        return;
    }
    if (b=="native") n->setRigPtt(on);
    else if (b=="omnirig" && o) o->setRigPtt(on);
    else h->setRigPtt(on);
}

static inline void activeCatSetFreq(DecodiumCatManager* n, DecodiumTransceiverManager* h, const QString& b, double hz,
                                    DecodiumOmniRigManager* o = nullptr, DecodiumLegacyBackend* legacy = nullptr)
{
    if (useLegacyRigControlFallback(legacy, b)) {
        legacy->setDialFrequency(hz);
        return;
    }
    if (b=="native") n->setRigFrequency(hz);
    else if (b=="omnirig" && o) o->setRigFrequency(hz);
    else h->setRigFrequency(hz);
}

static inline void activeCatSetTxFreq(DecodiumCatManager* n, DecodiumTransceiverManager* h, const QString& b, double hz,
                                      DecodiumOmniRigManager* o = nullptr)
{
    if (b=="native") n->setRigTxFrequency(hz);
    else if (b=="omnirig" && o) o->setRigTxFrequency(hz);
    else h->setRigTxFrequency(hz);
}

DecodiumBridge::DecodiumBridge(QObject* parent)
    : QObject(parent)
{
    m_themeManager    = new DecodiumThemeManager(this);
    m_propagationManager = new DecodiumPropagationManager(this);
    m_diagnostics     = new DecodiumDiagnostics(this);
    m_wavManager      = new WavManager(this);
    connect(m_wavManager, &WavManager::recordingChanged, this, [this]() {
        bool const recording = m_wavManager && m_wavManager->recording();
        if (m_recordRxEnabled != recording) {
            m_recordRxEnabled = recording;
        }
        emit recordRxEnabledChanged();
    });
    m_macroManager    = new MacroManager(this);
    m_bandManager     = new BandManager(this);
    m_nativeCat       = new DecodiumCatManager(this);
    m_omniRigCat      = new DecodiumOmniRigManager(this);
    m_hamlibCat       = new DecodiumTransceiverManager(this);
    m_dxCluster       = new DecodiumDxCluster(this);
    QGuiApplication::setFont(fontSettingFont(QStringLiteral("Font"), QString(), 0));
    connect(m_dxCluster, &DecodiumDxCluster::statusUpdate, this, [](const QString& msg) {
        qDebug() << "[DxCluster]" << msg;
    });
    connect(m_dxCluster, &DecodiumDxCluster::errorOccurred, this, [this](const QString& msg) {
        qDebug() << "[DxCluster ERROR]" << msg;
        QString const lower = msg.toLower();
        if (lower.contains(QStringLiteral("autospot"))
            || lower.contains(QStringLiteral("show/dx did not echo"))
            || lower.contains(QStringLiteral("node accepted the command"))) {
            emit statusMessage(QStringLiteral("DX Cluster: %1").arg(msg));
            return;
        }
        emit errorMessage("DX Cluster: " + msg);
    });
    connect(this, &DecodiumBridge::filterCqOnlyChanged, this, [this]() {
        if (legacyBackendAvailable()) {
            m_legacyBackend->setCqOnly(m_filterCqOnly);
        }
        if (usingLegacyBackendForTx()) {
            m_legacyBandActivityRevision = -1;
            m_legacyRxFrequencyRevision = -1;
            m_legacyAllTxtRevisionKey.clear();
            syncLegacyBackendDecodeList();
        }
    });
    connect(this, &DecodiumBridge::filterMyCallOnlyChanged, this, [this]() {
        if (usingLegacyBackendForTx()) {
            m_legacyBandActivityRevision = -1;
            m_legacyRxFrequencyRevision = -1;
            m_legacyAllTxtRevisionKey.clear();
            syncLegacyBackendDecodeList();
        }
    });
    connect(this, &DecodiumBridge::decodeListChanged, this, [this]() {
        publishRemoteActivityEntries(m_decodeList);
    });
    connect(this, &DecodiumBridge::rxDecodeListChanged, this, [this]() {
        publishRemoteActivityEntries(m_rxDecodeList);
    });
    connect(m_dxCluster, &DecodiumDxCluster::connectedChanged, this, [this]() {
        qDebug() << "[DxCluster] connected =" << m_dxCluster->connected();
        emit dxClusterConnectedChanged();
    });
    connect(m_dxCluster, &DecodiumDxCluster::spotsChanged, this, [this]() {
        emit dxClusterSpotsChanged();
    });
    m_activeStations  = new ActiveStationsModel(this);
    m_alertManager    = new DecodiumAlertManager(this);
    m_ntpClient       = new NtpClient(this);
    connect(m_ntpClient, &NtpClient::offsetUpdated, this, [this](double offsetMs) {
        if (qFuzzyCompare(m_ntpOffsetMs + 1.0, offsetMs + 1.0)) {
            return;
        }
        m_ntpOffsetMs = offsetMs;
        setGlobalNtpOffsetMs(offsetMs);
        emit ntpOffsetMsChanged();
    });
    connect(m_ntpClient, &NtpClient::syncStatusChanged, this, [this](bool synced, const QString& statusText) {
        Q_UNUSED(statusText)
        bool const effectiveSynced = m_ntpEnabled && synced;
        if (m_ntpSynced != effectiveSynced) {
            m_ntpSynced = effectiveSynced;
            emit ntpSyncedChanged();
        }
        bridgeLog(QStringLiteral("NTP status: enabled=%1 synced=%2 offset=%3 ms")
                      .arg(m_ntpEnabled ? QStringLiteral("1") : QStringLiteral("0"),
                           effectiveSynced ? QStringLiteral("1") : QStringLiteral("0"),
                           QString::number(m_ntpOffsetMs, 'f', 1)));
    });
    connect(m_ntpClient, &NtpClient::errorOccurred, this, [](const QString& msg) {
        bridgeLog(QStringLiteral("NTP error: ") + msg);
    });

    bridgeLog(QStringLiteral("DX Cluster auto-connect: waiting for Main.qml ready"));

#if defined(Q_OS_MAC)
    if (qEnvironmentVariableIsSet("DECODIUM_FORCE_STANDALONE_UDP")) {
        m_useLegacyTxBackend = false;
        bridgeLog(QStringLiteral("Legacy TX backend disabled by DECODIUM_FORCE_STANDALONE_UDP"));
    } else {
        m_useLegacyTxBackend = true;
        // Su Mac, crea il legacy backend in modo differito per non bloccare l'avvio
        QTimer::singleShot(3000, this, [this]() {
            if (m_useLegacyTxBackend && !ensureLegacyBackendAvailable()) {
                m_useLegacyTxBackend = false;
            }
        });
    }
#endif
    // Su Windows/Linux NON creare il legacy backend all'avvio — viene creato lazy
    // quando l'utente apre Settings/Setup (ensureLegacyBackendAvailable)

    bridgeLog(QStringLiteral("UDP WSJT-X startup: waiting for Main.qml ready"));

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

    // Remote Web Console (webapp)
    {
        auto const env = QProcessEnvironment::systemEnvironment();
        QSettings webSettings("Decodium", "Decodium3");
        // Env vars hanno priorita', fallback sui setting canonici Remote*
        // e sugli alias storici WebApp*.
        bool const webAppEnabledInSettings =
            getSetting(QStringLiteral("RemoteWebEnabled"),
                       webSettings.value(QStringLiteral("WebAppEnabled"), false)).toBool();
        auto wsPortText = env.value(QStringLiteral("FT2_REMOTE_WS_PORT"));
        if (wsPortText.isEmpty() && webAppEnabledInSettings)
            wsPortText = getSetting(QStringLiteral("RemoteWsPort"),
                                    webSettings.value(QStringLiteral("WebAppWsPort"))).toString();
        if (wsPortText.isEmpty() && webAppEnabledInSettings) {
            bool httpOk = false;
            int const httpPort = getSetting(QStringLiteral("RemoteHttpPort"),
                                            webSettings.value(QStringLiteral("WebAppHttpPort"), 19091)).toInt(&httpOk);
            wsPortText = QString::number(httpOk && httpPort > 1 ? httpPort - 1 : 19090);
        }
        if (!wsPortText.isEmpty()) {
            bool wsPortOk = false;
            auto const wsPort = wsPortText.toUInt(&wsPortOk);
            if (wsPortOk && wsPort > 0 && wsPort <= 65535u) {
                auto bindText = env.value(QStringLiteral("FT2_REMOTE_WS_BIND"));
                if (bindText.isEmpty()) bindText = getSetting(QStringLiteral("RemoteWsBind"),
                                                              webSettings.value(QStringLiteral("WebAppBind"), QStringLiteral("127.0.0.1"))).toString();
                QHostAddress bindAddress(bindText);
                auto authUser = env.value(QStringLiteral("FT2_REMOTE_WS_USER"));
                if (authUser.isEmpty()) authUser = getSetting(QStringLiteral("RemoteUser"),
                                                              webSettings.value(QStringLiteral("WebAppUser"), QStringLiteral("admin"))).toString();
                auto authToken = env.value(QStringLiteral("FT2_REMOTE_WS_TOKEN"));
                if (authToken.isEmpty()) authToken = getSetting(QStringLiteral("RemoteToken"),
                                                                webSettings.value(QStringLiteral("WebAppToken"))).toString();
                {
                    QStringList secureProfiles;
                    secureProfiles << m_callsign.trimmed().toUpper();
                    secureProfiles << readSettingFromLegacyIni(QStringLiteral("MyCall")).toString().trimmed().toUpper();
                    secureProfiles << webSettings.value(QStringLiteral("MyCall")).toString().trimmed().toUpper();
                    QSettings secureIni(legacyIniPath(), QSettings::IniFormat);
                    authToken = secure_settings::load_or_import(&secureIni,
                                                                secure_settings::service(secureProfiles.value(0)),
                                                                QStringLiteral("RemoteToken"),
                                                                authToken).trimmed();
                    if (authToken.isEmpty()) {
                        authToken = resolveSecureSettingFromProfiles(QStringLiteral("RemoteToken"),
                                                                     getSetting(QStringLiteral("RemoteToken"),
                                                                                webSettings.value(QStringLiteral("WebAppToken"))).toString(),
                                                                     secureProfiles);
                    }
                    bridgeLog(QStringLiteral("Remote Web token resolved length=%1").arg(authToken.size()));
                }
                bool const remoteExposed = (bindAddress != QHostAddress::LocalHost && bindAddress != QHostAddress(QStringLiteral("::1")));
                if (remoteExposed && authToken.size() < 12) {
                    bridgeLog("Remote Web disabled: token must be at least 12 characters on LAN/WAN bind.");
                } else {
                    m_remoteServer = new RemoteCommandServer(this);
                    m_remoteServer->setRuntimeStateProvider([this]() -> RemoteCommandServer::RuntimeState {
                        RemoteCommandServer::RuntimeState s;
                        s.mode = m_mode;
                        s.band = m_bandManager ? m_bandManager->currentBandLambda() : QString();
                        if (s.band == QStringLiteral("--")) s.band.clear();
                        s.dialFrequencyHz = static_cast<qint64>(m_frequency);
                        s.rxFrequencyHz = m_rxFrequency;
                        s.txFrequencyHz = m_txFrequency;
                        s.periodMs = qMax<qint64>(1, qRound64(m_txPeriod * 1000.0));
                        s.txEnabled = m_txEnabled;
                        s.autoCqEnabled = m_autoCqRepeat > 0;
                        s.autoSpotEnabled = m_autoSpotEnabled;
                        s.asyncL2Enabled = m_asyncDecodeEnabled;
                        s.dualCarrierEnabled = m_dualCarrierEnabled;
                        s.alt12Enabled = m_alt12Enabled;
                        s.manualTxEnabled = m_manualTxHold;
                        s.quickQsoEnabled = m_quickQsoEnabled;
                        s.monitoring = m_monitoring;
                        s.transmitting = m_transmitting;
                        s.myCall = m_callsign;
                        s.dxCall = m_dxCall;
                        s.nowUtcMs = QDateTime::currentMSecsSinceEpoch();
                        return s;
                    });
                    m_remoteServer->setAuthUser(authUser);
                    if (!authToken.isEmpty()) m_remoteServer->setAuthToken(authToken);

                    auto guardText = env.value(QStringLiteral("FT2_REMOTE_GUARD_PRE_MS"));
                    if (guardText.isEmpty()) guardText = webSettings.value(QStringLiteral("WebAppGuardMs"), QStringLiteral("300")).toString();
                    bool guardOk = false;
                    auto const guardMs = guardText.toInt(&guardOk);
                    if (guardOk) m_remoteServer->setGuardPreMs(guardMs);
                    auto ageText = env.value(QStringLiteral("FT2_REMOTE_MAX_AGE_MS"));
                    if (ageText.isEmpty()) ageText = webSettings.value(QStringLiteral("WebAppMaxAgeMs"), QStringLiteral("7500")).toString();
                    bool ageOk = false;
                    auto const maxAgeMs = ageText.toInt(&ageOk);
                    if (ageOk) m_remoteServer->setMaxCommandAgeMs(maxAgeMs);

                    quint16 httpPort = 0;
                    auto httpPortText = env.value(QStringLiteral("FT2_REMOTE_HTTP_PORT"));
                    if (httpPortText.isEmpty()) httpPortText = getSetting(QStringLiteral("RemoteHttpPort"),
                                                                          webSettings.value(QStringLiteral("WebAppHttpPort"))).toString();
                    if (!httpPortText.isEmpty()) {
                        bool httpOk = false;
                        auto const hp = httpPortText.toUInt(&httpOk);
                        if (httpOk && hp <= 65535u) httpPort = static_cast<quint16>(hp);
                    }

                    // Collega comandi remoti al bridge
                    connect(m_remoteServer, &RemoteCommandServer::setModeRequested, this, [this](const QString& commandId, const QString& mode) {
                        bridgeLog(QStringLiteral("Remote command %1: set_mode %2").arg(commandId, mode));
                        setMode(mode.trimmed().toUpper());
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setBandRequested, this, [this](const QString& commandId, const QString& band) {
                        bridgeLog(QStringLiteral("Remote command %1: set_band %2").arg(commandId, band));
                        applyRemoteBandChange(band);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setDialFrequencyRequested, this, [this](const QString& commandId, qint64 hz) {
                        bridgeLog(QStringLiteral("Remote command %1: set_dial_frequency %2 Hz").arg(commandId, QString::number(hz)));
                        applyRemoteDialFrequency(static_cast<double>(hz), QStringLiteral("remote-command"));
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setRxFrequencyRequested, this, [this](const QString& commandId, int f) {
                        bridgeLog(QStringLiteral("Remote command %1: set_rx_frequency %2 Hz").arg(commandId, QString::number(f)));
                        setRxFrequency(f);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setTxFrequencyRequested, this, [this](const QString& commandId, int f) {
                        bridgeLog(QStringLiteral("Remote command %1: set_tx_frequency %2 Hz").arg(commandId, QString::number(f)));
                        setTxFrequency(f);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setTxEnabledRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_tx_enabled %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setTxEnabled(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setMonitoringRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_monitoring %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setMonitoring(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setAutoCqRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_auto_cq %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setAutoCqRepeat(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setAutoSpotRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_auto_spot %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setAutoSpotEnabled(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setAsyncL2Requested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_async_l2 %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setAsyncDecodeEnabled(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setDualCarrierRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_dual_carrier %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setDualCarrierEnabled(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setAlt12Requested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_alt_12 %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setAlt12Enabled(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setManualTxRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_manual_tx %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        if (e) {
                            engageManualTxHold(QStringLiteral("remote-web"), false);
                        } else {
                            clearManualTxHold(QStringLiteral("remote-web"));
                        }
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setQuickQsoRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_quick_qso %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setQuickQsoEnabled(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setFt2QsoMessageCountRequested, this, [this](const QString& commandId, int count) {
                        bridgeLog(QStringLiteral("Remote command %1: set_ft2_qso_msg_count %2").arg(commandId, QString::number(count)));
                        setQuickQsoEnabled(count == 2);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::logMessage, this, [](const QString& msg) { bridgeLog("Remote: " + msg); }, Qt::QueuedConnection);

                    if (!m_remoteServer->start(static_cast<quint16>(wsPort), bindAddress, httpPort)) {
                        bridgeLog("Remote WS disabled: failed to bind " + bindAddress.toString() + ":" + QString::number(wsPort));
                        webSettings.setValue(QStringLiteral("WebAppActive"), false);
                    } else {
                        bridgeLog("Remote Web Console running: ws://" + bindAddress.toString() + ":" + QString::number(m_remoteServer->wsPort())
                                  + "  http://" + bindAddress.toString() + ":" + QString::number(m_remoteServer->httpPort()));
                        webSettings.setValue(QStringLiteral("WebAppActive"), true);
                        clearRemoteActivityCache(false);
                        publishRemoteActivityEntries(m_decodeList);
                        publishRemoteActivityEntries(m_rxDecodeList);
                    }
                }
            }
        }
    }

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
        bool const monitorShouldStayActive = m_monitoring || m_monitorRequested;
        quint64 const monitorSessionId = m_periodTimerSessionId;
        double const previousFrequency = m_frequency;
        setFrequency(freq);
        requestRigFrequencyFromBridge(freq, QStringLiteral("band/mode"));
        applyConfiguredCatRigMode(QStringLiteral("band-change"));
        if (m_monitoring) {
            {
                QMutexLocker locker(&m_audioBufferMutex);
                m_audioBuffer.clear();
            }
            m_periodTicks = 0;
            m_lastPeriodSlot = -1;  // next onPeriodTimer tick re-anchors UTC slot
        }
        scheduleMonitorRecovery(QStringLiteral("band QSY %1 -> %2 Hz")
                                    .arg(QString::number(previousFrequency, 'f', 0),
                                         QString::number(freq, 'f', 0)),
                                monitorSessionId,
                                monitorShouldStayActive);
    });

    // Helper: collega i segnali di un manager alle property del bridge, con guard backend
    auto connectCatSignals = [this, catProp](auto* mgr, const QString& backend) {
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::frequencyChanged, this, [this, backend, catProp]() {
            if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && backend == QStringLiteral("hamlib")) return;
            if (!catSignalMatchesBackend(m_catBackend, backend)) return;
            double f = catProp("frequency").toDouble();
            if (f > 0.0 && shouldIgnoreCatFrequencyDuringLocalQsy(f, backend)) {
                return;
            }
            if (f > 0 && m_frequency != f) {
                bool const monitorShouldStayActive = m_monitoring || m_monitorRequested;
                quint64 const monitorSessionId = m_periodTimerSessionId;
                double const previousFrequency = m_frequency;
                // Non sovrascrivere la frequenza se l'abbiamo appena impostata noi
                // (il radio riporta la frequenza FT8 anche se abbiamo chiesto FT2)
                double diff = std::abs(f - m_frequency);
                if (diff > 50000.0) {
                    // Cambio banda dal radio — accetta
                    m_frequency = f; emit frequencyChanged();
                    m_bandManager->updateFromFrequency(f);
                    syncActiveCatTxSplitFrequency(QStringLiteral("rig-frequency"));
                    scheduleMonitorRecovery(QStringLiteral("rig QSY %1 -> %2 Hz")
                                                .arg(QString::number(previousFrequency, 'f', 0),
                                                     QString::number(f, 'f', 0)),
                                            monitorSessionId,
                                            monitorShouldStayActive);
                } else if (diff > 1.0) {
                    // Piccola variazione — aggiorna solo la frequenza, non la banda
                    m_frequency = f; emit frequencyChanged();
                    syncActiveCatTxSplitFrequency(QStringLiteral("rig-frequency"));
                }
            }
            if (f > 0.0) {
                maybeApplyStartupModeFromRigFrequency(f, true);
            }
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::modeChanged, this, [this, backend, catProp]() {
            if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && backend == QStringLiteral("hamlib")) return;
            if (!catSignalMatchesBackend(m_catBackend, backend)) return;
            QString m = catProp("mode").toString();
            if (m_catMode != m) { m_catMode = m; emit catModeChanged(); }
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::connectedChanged, this, [this, backend, catProp]() {
            if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && backend == QStringLiteral("hamlib")) return;
            if (!catSignalMatchesBackend(m_catBackend, backend)) return;
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
            if (c) {
                m_lastSuccessfulCatConnected = true;
                m_lastSuccessfulCatBackend = m_catBackend;
                if (!m_lastCatError.isEmpty()) {
                    m_lastCatError.clear();
                    emit lastCatErrorChanged();
                }
                QSettings s("Decodium", "Decodium3");
                s.setValue("lastSuccessfulCatConnected", true);
                s.setValue("lastSuccessfulCatBackend", m_catBackend);
            } else if (!m_shuttingDown) {
                m_lastSuccessfulCatConnected = false;
                m_lastSuccessfulCatBackend = m_catBackend;
                QSettings s("Decodium", "Decodium3");
                s.setValue("lastSuccessfulCatConnected", false);
                s.setValue("lastSuccessfulCatBackend", m_catBackend);
            }

            if (c) {
                if (backend == QStringLiteral("hamlib")
                    && m_catBackend == QStringLiteral("tci")
                    && m_monitoring
                    && usingTciAudioInput()) {
                    QTimer::singleShot(150, this, [this]() {
                        if (m_catBackend == QStringLiteral("tci") && m_monitoring && usingTciAudioInput()) {
                            startTciAudioCapture();
                        }
                    });
                }
                applyConfiguredCatRigMode(QStringLiteral("connect"));
                syncActiveCatTxSplitFrequency(QStringLiteral("connect"));
                auto applyCatStartupFrequency = [this, backend, catProp]() {
                    if (!catSignalMatchesBackend(m_catBackend, backend)) {
                        return;
                    }
                    if (!catProp("connected").toBool()) {
                        return;
                    }
                    double const rigFrequency = catProp("frequency").toDouble();
                    if (rigFrequency <= 0.0) {
                        return;
                    }
                    if (shouldIgnoreCatFrequencyDuringLocalQsy(rigFrequency, backend)) {
                        return;
                    }
                    bool const differsFromBridge =
                        !qFuzzyCompare(m_frequency + 1.0, rigFrequency + 1.0);
                    if (differsFromBridge || m_startupModeAutoPending) {
                        bridgeLog(QStringLiteral("startup CAT sync: using rig frequency %1 Hz from %2")
                                      .arg(QString::number(rigFrequency, 'f', 0), backend));
                        setFrequency(rigFrequency);
                        maybeApplyStartupModeFromRigFrequency(rigFrequency, true);
                        syncActiveCatTxSplitFrequency(QStringLiteral("startup-frequency"));
                    }
                };

                applyCatStartupFrequency();
                static constexpr int kStartupCatSyncRetryMs[] = {75, 200, 500};
                for (int const delayMs : kStartupCatSyncRetryMs) {
                    QTimer::singleShot(delayMs, this, applyCatStartupFrequency);
                }
                QTimer::singleShot(250, this, [this]() {
                    applyConfiguredCatRigMode(QStringLiteral("connect-retry"));
                    syncActiveCatTxSplitFrequency(QStringLiteral("connect-retry"));
                });
                if (m_catBackend == QStringLiteral("tci") && !canonicalApplicationDecodeMode(m_mode).isEmpty()) {
                    QTimer::singleShot(150, this, [this]() {
                        if (m_catBackend == QStringLiteral("tci") && m_hamlibCat && m_hamlibCat->connected()) {
                            if (configuredCatRigMode().isEmpty()) {
                                m_hamlibCat->setRigMode(m_mode);
                            } else {
                                applyConfiguredCatRigMode(QStringLiteral("tci-connect"));
                            }
                            syncActiveCatTxSplitFrequency(QStringLiteral("tci-connect"));
                        }
                    });
                }
            }
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::errorOccurred, this, [this, backend](const QString& msg) {
            if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && backend == QStringLiteral("hamlib")) return;
            if (!catSignalMatchesBackend(m_catBackend, backend)) return;
            if (m_suppressCatErrors) { bridgeLog("CAT[" + backend + "] error suppressed: " + msg); return; }
            bridgeLog("CAT[" + backend + "] error: " + msg);
            if (m_lastCatError != msg) {
                m_lastCatError = msg;
                emit lastCatErrorChanged();
            }
            emit errorMessage(msg);
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::statusUpdate, this, [this, backend](const QString& msg) {
            if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && backend == QStringLiteral("hamlib")) return;
            if (!catSignalMatchesBackend(m_catBackend, backend)) return;
            bridgeLog("CAT[" + backend + "] status: " + msg);
            emit statusMessage(msg);
        });
    };
    connectCatSignals(m_nativeCat, "native");
    connectCatSignals(m_omniRigCat, "omnirig");
    connectCatSignals(m_hamlibCat, "hamlib");

    auto syncHamlibTelemetry = [this]() {
        if (!m_hamlibCat
            || !catSignalMatchesBackend(m_catBackend, QStringLiteral("hamlib"))
            || !m_hamlibCat->connected()) {
            updateRigTelemetry(0.0, 0.0);
            return;
        }
        updateRigTelemetry(m_hamlibCat->powerWatts(), m_hamlibCat->swr());
    };
    connect(m_hamlibCat, &DecodiumTransceiverManager::powerWattsChanged,
            this, syncHamlibTelemetry);
    connect(m_hamlibCat, &DecodiumTransceiverManager::swrChanged,
            this, syncHamlibTelemetry);
    connect(m_hamlibCat, &DecodiumTransceiverManager::connectedChanged,
            this, syncHamlibTelemetry);
    connect(m_hamlibCat, &DecodiumTransceiverManager::tciPcmSamplesReady,
            this, &DecodiumBridge::onTciPcmSamplesReady, Qt::QueuedConnection);
    connect(m_hamlibCat, &DecodiumTransceiverManager::tciModActiveChanged,
            this,
            [this](bool active) {
                if (active || !usingTciAudioInput() || !m_transmitting || m_tuning) {
                    return;
                }
                bridgeLog(QStringLiteral("TCI modulator inactive: finishing TX playback"));
                m_txPlaybackHoldUntilMs = 0;
                m_txPlaybackReleasePending = false;
                QTimer::singleShot(0, this, [this]() {
                    if (m_transmitting && usingTciAudioInput() && !m_tuning) {
                        completeTxPlayback(QStringLiteral("tci-modulator-idle"));
                    }
                });
            },
            Qt::QueuedConnection);

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
#if defined(Q_OS_LINUX)
    m_workerThreadFt4->setStackSize(16 * 1024 * 1024);
#else
    m_workerThreadFt4->setStackSize(8 * 1024 * 1024);
#endif
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

    // Spectrum timer: emette dati FFT per la waterfall (intervallo configurabile)
    m_spectrumTimer = new QTimer(this);
    {
        QSettings s("Decodium", "Decodium3");
        int interval = s.value("spectrumInterval", 20).toInt();
        m_spectrumTimer->setInterval(qBound(10, interval, 500));
    }
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

    // PTT giu' automatico quando il Modulator si ferma (fine TX/Tune).
    // Il device audio puo' consumare il QIODevice prima che il buffer hardware
    // sia realmente uscito: non abbassare PTT prima della durata della waveform.
    connect(m_modulator, &Modulator::stateChanged, this, [this](Modulator::ModulatorState st) {
        if (st == Modulator::Idle) {
            qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
            if (!m_tuning && m_transmitting && m_txPlaybackHoldUntilMs > nowMs) {
                qint64 const delayMs = qMax<qint64>(1, m_txPlaybackHoldUntilMs - nowMs);
                if (!m_txPlaybackReleasePending) {
                    m_txPlaybackReleasePending = true;
                    bridgeLog(QStringLiteral("modulator idle early: holding PTT for %1 ms").arg(delayMs));
                    QTimer::singleShot(delayMs, this, [this]() {
                        if (m_modulator && m_modulator->isActive()) {
                            m_txPlaybackReleasePending = false;
                            return;
                        }
                        finishModulatorIdlePlayback(QStringLiteral("held-idle"));
                    });
                }
                return;
            }
            finishModulatorIdlePlayback(QStringLiteral("idle"));
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
    resetStartupTransientQsoState();
    purgePersistentTransientQsoState();
    m_startupModeAutoPending = true;
    m_startupModeAutoUntilMs = QDateTime::currentMSecsSinceEpoch() + 10000;
    m_startupModeAutoAuthoritativeApplied = false;
    maybeApplyStartupModeFromRigFrequency(m_frequency);
    m_legacyStartupModeGuard = m_mode.trimmed();
    m_legacyStartupModeGuardUntilMs = QDateTime::currentMSecsSinceEpoch() + 10000;
    bridgeLog(QStringLiteral("Audio device enumeration deferred until Main.qml ready"));

    bridgeLog("startup services: waiting for Main.qml ready signal");

    syncLegacyBackendState();
    if ((usingLegacyBackendForTx() || useLegacyRigControlFallback(m_legacyBackend, m_catBackend)) && m_legacyStateTimer) {
        m_legacyStateTimer->start();
    }
}

DecodiumBridge::~DecodiumBridge()
{
    beginDecodeCallbackShutdown();
    stopRx();
    teardownAudioCapture();
    // Ferma TX/Tune prima di distruggere tutto
    if (m_txAudioSink) {
        m_txAudioSink->disconnect();
        m_txAudioSink->stop();
    }
    if (m_modulator && m_modulator->isActive()) m_modulator->stop(true);
    if (m_soundOutput) m_soundOutput->stop();
    auto safeQuitThread = [](QThread *t, const char *name) {
        if (!t) return;
        t->quit();
        if (!t->wait(5000)) {
            qWarning("~DecodiumBridge: %s did not finish in 5s, terminating", name);
            t->terminate();
            t->wait(2000);
        }
    };
    safeQuitThread(m_workerThread,       "ft8");
    safeQuitThread(m_workerThreadFt2,    "ft2");
    safeQuitThread(m_workerThreadFt4,    "ft4");
    safeQuitThread(m_workerThreadQ65,    "q65");
    safeQuitThread(m_workerThreadMsk,    "msk");
    safeQuitThread(m_workerThreadWspr,   "wspr");
    safeQuitThread(m_workerThreadLegacyJt, "legacyJt");
    safeQuitThread(m_workerThreadFst4,   "fst4");
}

QObject * DecodiumBridge::propagationManager() const
{
    return m_propagationManager;
}

bool DecodiumBridge::usingLegacyBackendForTx() const
{
    return legacyTxBackendRequested() && legacyBackendAvailable();
}

void DecodiumBridge::notifyMainQmlReady()
{
    if (m_mainQmlAsyncLoadDone) {
        m_mainQmlAsyncLoadDone->store(true, std::memory_order_relaxed);
    }
    if (m_mainQmlReady) {
        return;
    }

    m_mainQmlReady = true;
    emit mainQmlReadyForNativeWindowing();
    bridgeLog(QStringLiteral("Main.qml ready: releasing deferred startup services"));
    QTimer::singleShot(500, this, [this]() {
        if (m_shuttingDown) {
            return;
        }
        runPostQmlStartupServices();
    });
}

void DecodiumBridge::runPostQmlStartupServices()
{
    if (m_startupServicesStarted) {
        return;
    }
    m_startupServicesStarted = true;
    bridgeLog(QStringLiteral("startup services: starting after Main.qml ready"));

    QTimer::singleShot(100, this, [this]() {
        if (!m_shuttingDown) {
            enumerateAudioDevices();
        }
    });

    QTimer::singleShot(300, this, [this]() {
        if (m_shuttingDown) {
            return;
        }
#if defined(Q_OS_MAC)
        if (m_useLegacyTxBackend && ensureLegacyBackendAvailable()) {
            bridgeLog(QStringLiteral("UDP WSJT-X delegated to legacy backend"));
            shutdownUdpMessageClient();
            return;
        }
#endif
        initUdpMessageClient();
    });

    QTimer::singleShot(700, this, [this]() {
        if (m_shuttingDown || !m_dxCluster || m_callsign.isEmpty()) {
            return;
        }
        m_dxCluster->setCallsign(m_callsign);
        QSettings s(QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        beginConfiguredBridgeSettingsGroup(s);
        bool autoConnect = s.value(QStringLiteral("DXCluster/autoConnect"), false).toBool();
        if (!autoConnect) {
            autoConnect = s.value(QStringLiteral("DXClusterAutoConnect"), false).toBool();
        }
        if (autoConnect) {
            bridgeLog(QStringLiteral("DX Cluster auto-connect: %1:%2")
                          .arg(m_dxCluster->host())
                          .arg(m_dxCluster->port()));
            m_dxCluster->connectCluster();
        }
    });

    bool const autoConn = (m_catBackend == QStringLiteral("native")) ? m_nativeCat->catAutoConnect()
                        : (m_catBackend == QStringLiteral("omnirig")) ? m_omniRigCat->catAutoConnect()
                                                                      : m_hamlibCat->catAutoConnect();
    bool const retryLastSuccessfulCat = (!autoConn
                                         && m_lastSuccessfulCatConnected
                                         && m_lastSuccessfulCatBackend == m_catBackend);
    bridgeLog(QStringLiteral("CAT[%1] autoConnect=%2 retryLastSuccessful=%3")
                  .arg(m_catBackend)
                  .arg(autoConn ? 1 : 0)
                  .arg(retryLastSuccessfulCat ? 1 : 0));

    if (legacyOwnsRigControl(m_legacyBackend) || useLegacyRigControlFallback(m_legacyBackend, m_catBackend)) {
        bridgeLog(QStringLiteral("CAT auto-connect delegated to legacy backend"));
        QTimer::singleShot(250, this, [this]() {
            if (!m_legacyBackend || !m_legacyBackend->available()) {
                return;
            }
            m_legacyBackend->retryRigConnection();
            syncLegacyBackendTxState();
            scheduleLegacyStateRefreshBurst();
        });
    } else if (autoConn || retryLastSuccessfulCat) {
        m_startupCatRetryCount = 0;
        QTimer::singleShot(1000, this, [this]() {
            bridgeLog(QStringLiteral("CAT startup reconnect: retry active backend ") + m_catBackend);
            m_startupCatRetryCount = 1;
            retryRigConnection();
            QTimer::singleShot(2500, this, [this]() {
                if (m_shuttingDown) return;
                if (m_catConnected) return;
                if (m_startupCatRetryCount >= 2) return;
                m_startupCatRetryCount = 2;
                bridgeLog(QStringLiteral("CAT startup reconnect: second attempt on ") + m_catBackend);
                retryRigConnection();
            });
        });
    }

    if (m_autoStartMonitorOnStartup && m_mode != QStringLiteral("Echo")) {
        int const startupMonitorDelayMs = usingLegacyBackendForTx() ? 2800 : 1200;
        QTimer::singleShot(startupMonitorDelayMs, this, [this]() {
            if (!m_autoStartMonitorOnStartup || m_mode == QStringLiteral("Echo")
                || m_monitoring || m_transmitting || m_tuning) {
                bridgeLog(QStringLiteral("startup auto-monitor: skipped autoStart=%1 mode=%2 monitoring=%3 tx=%4 tune=%5")
                              .arg(m_autoStartMonitorOnStartup ? 1 : 0)
                              .arg(m_mode)
                              .arg(m_monitoring ? 1 : 0)
                              .arg(m_transmitting ? 1 : 0)
                              .arg(m_tuning ? 1 : 0));
                return;
            }
            bridgeLog(QStringLiteral("startup auto-monitor: enabling monitor after Main.qml ready"));
            setMonitoring(true);
        });
    } else {
        bridgeLog(QStringLiteral("startup auto-monitor: disabled autoStart=%1 mode=%2")
                      .arg(m_autoStartMonitorOnStartup ? 1 : 0)
                      .arg(m_mode));
    }
}

bool DecodiumBridge::useModernSpectrumFeedWithLegacy() const
{
    // The legacy FT8/FT4 backend must own the input device while it is decoding.
    // Opening a second modern capture for the panadapter can attenuate or disturb
    // USB audio devices on different backends, which lowers decoded SNR.
    return false;
}

bool DecodiumBridge::legacyBackendAvailable() const
{
    return m_legacyBackend && m_legacyBackend->available();
}

bool DecodiumBridge::shouldIgnoreDecodeCallbacks() const
{
    if (m_shuttingDown || QCoreApplication::closingDown()) {
        return true;
    }

    auto *app = QCoreApplication::instance();
    return app && app->property("decodiumShuttingDown").toBool();
}

void DecodiumBridge::beginDecodeCallbackShutdown()
{
    m_shuttingDown = true;

    auto disconnectWorker = [this](QObject *worker) {
        if (worker) {
            QObject::disconnect(worker, nullptr, this, nullptr);
        }
    };

    disconnectWorker(m_ft8Worker);
    disconnectWorker(m_ft2Worker);
    disconnectWorker(m_ft4Worker);
    disconnectWorker(m_q65Worker);
    disconnectWorker(m_mskWorker);
    disconnectWorker(m_wsprWorker);
    disconnectWorker(m_legacyJtWorker);
    disconnectWorker(m_fst4Worker);

    // Drop already queued cross-thread decode deliveries before legacy teardown pumps the event loop.
    QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
    m_activeStations = nullptr;
}

bool DecodiumBridge::ensureLegacyBackendAvailable()
{
    if (legacyBackendAvailable()) {
        return true;
    }

    bool const createdNow = (m_legacyBackend == nullptr);
    if (!m_legacyBackend) {
        if (auto * app = QCoreApplication::instance()) {
            app->setProperty("decodiumEmbeddedLegacyRigControlEnabled", !legacyTxBackendRequested());
        }
        m_legacyBackend = new DecodiumLegacyBackend(this);
    }

    if (!legacyBackendAvailable()) {
        bridgeLog("Legacy backend unavailable: " + (m_legacyBackend ? m_legacyBackend->failureReason()
                                                                    : QStringLiteral("unknown error")));
        return false;
    }

    if (createdNow) {
        connect(m_legacyBackend, &DecodiumLegacyBackend::waterfallRowReady,
                this, &DecodiumBridge::onLegacyWaterfallRow);
        connect(m_legacyBackend, &DecodiumLegacyBackend::audioSamplesReady,
                this, &DecodiumBridge::onLegacyAudioSamples);
        connect(m_legacyBackend, &DecodiumLegacyBackend::warningRaised,
                this, [this](QString const& title, QString const& summary, QString const& details) {
            // Quando il CAT nativo gestisce il rig, i warning Hamlib del legacy backend
            // sono falsi positivi (es. conflitto porta COM). Li sopprimiamo.
            if (m_catBackend == QStringLiteral("native")) {
                bridgeLog("Legacy warning suppressed (native CAT active): " + summary);
                return;
            }
            emit warningRaised(title, summary, details);
        });
        connect(m_legacyBackend, &DecodiumLegacyBackend::rigErrorRaised,
                this, [this](QString const& title, QString const& summary, QString const& details) {
            if (m_catBackend == QStringLiteral("native")) {
                bridgeLog("Legacy rig error suppressed (native CAT active): " + summary);
                return;
            }
            emit rigErrorRaised(title, summary, details);
        });
        connect(m_legacyBackend, &DecodiumLegacyBackend::preferencesRequested,
                this, [this]() {
                    emit setupSettingsRequested(0);
                });
        connect(m_legacyBackend, &DecodiumLegacyBackend::quitRequested,
                this, [this]() {
                    emit quitRequested();
                });
        connect(m_legacyBackend, &DecodiumLegacyBackend::rigPttRequested,
                this, [this](bool enabled) {
                    bridgeLog(QStringLiteral("legacyPttRequested: on=%1 backend=%2 catConnected=%3")
                                  .arg(enabled ? 1 : 0)
                                  .arg(m_catBackend)
                                  .arg(m_catConnected ? 1 : 0));
                    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
                        activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, enabled, m_omniRigCat, m_legacyBackend);
                    } else {
                        bridgeLog(QStringLiteral("legacyPttRequested: active CAT unavailable, retrying connection"));
                        if (enabled) {
                            retryRigConnection();
                            QTimer::singleShot(700, this, [this]() {
                                if (m_shuttingDown || !m_legacyBackend || !m_legacyBackend->transmitting()) {
                                    return;
                                }
                                if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
                                    bridgeLog(QStringLiteral("legacyPttRequested: applying delayed PTT after CAT reconnect"));
                                    activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, true, m_omniRigCat, m_legacyBackend);
                                } else {
                                    bridgeLog(QStringLiteral("legacyPttRequested ignored: no active CAT PTT backend after retry"));
                                }
                            });
                        }
                    }
                });

        bridgeLog(legacyTxBackendRequested()
                      ? QStringLiteral("Legacy backend enabled for TX/Tune and special operation control")
                      : QStringLiteral("Legacy backend enabled for shared Setup/Time Sync UI"));

        m_legacyBackend->setRigControlEnabled(!legacyTxBackendRequested());

        // All'avvio manteniamo il device/channel audio salvato nel bridge, invece di
        // lasciare che il backend legacy riparta dal default di sistema.
        if (!m_audioInputDevice.isEmpty()) {
            m_legacyBackend->setAudioInputDeviceName(m_audioInputDevice);
        }
        if (!m_audioOutputDevice.isEmpty()) {
            m_legacyBackend->setAudioOutputDeviceName(m_audioOutputDevice);
        }
        m_legacyBackend->setAudioInputChannel(qBound(0, m_audioInputChannel, 3));
        m_legacyBackend->setAudioOutputChannel(qBound(0, m_audioOutputChannel, 3));
        m_legacyBackend->setDecodeDepthBits(legacyCompatibleDecodeDepthBits());
        m_legacyBackend->setCqOnly(m_filterCqOnly);
        bridgeLog(QStringLiteral("Legacy backend audio restored: inDev=%1 inChan=%2 outDev=%3 outChan=%4")
                      .arg(m_audioInputDevice,
                           QString::number(m_audioInputChannel),
                           m_audioOutputDevice,
                           QString::number(m_audioOutputChannel)));

        syncLegacyBackendDialogState();

        // Decodium3 espone un sender WSJT-X UDP quando e' il backend TX attivo.
        // Se il bridge moderno resta responsabile del QSO, il suo MessageClient
        // deve restare vivo per inoltrare LoggedADIF/QSOLogged ai log esterni.
        if (usingLegacyBackendForTx()) {
            shutdownUdpMessageClient();
        } else {
            initUdpMessageClient();
        }

        if (legacyTxBackendRequested() && m_monitoring && !m_transmitting && !m_tuning) {
            migrateActiveMonitoringToLegacyBackend();
        }
    }

    m_legacyBackend->setRigControlEnabled(!legacyTxBackendRequested());
    scheduleLegacyStateRefreshBurst();

    return true;
}

void DecodiumBridge::syncLegacyBackendDialogState()
{
    if (!legacyBackendAvailable()) {
        return;
    }

    if (!m_audioInputDevice.isEmpty()) {
        m_legacyBackend->setAudioInputDeviceName(m_audioInputDevice);
    }
    if (!m_audioOutputDevice.isEmpty()) {
        m_legacyBackend->setAudioOutputDeviceName(m_audioOutputDevice);
    }
    m_legacyBackend->setAudioInputChannel(qBound(0, m_audioInputChannel, 3));
    m_legacyBackend->setAudioOutputChannel(qBound(0, m_audioOutputChannel, 3));
    m_legacyBackend->setDecodeDepthBits(legacyCompatibleDecodeDepthBits());
    m_legacyBackend->setCqOnly(m_filterCqOnly);
    m_legacyBackend->setRxInputLevel(qRound(qBound(0.0, m_rxInputLevel, 100.0)));
    int const legacyTxAttn = legacyTxAttenuationFromLevel(m_txOutputLevel);
    m_legacyBackend->setTxOutputAttenuation(legacyTxAttn);
    m_legacyBackend->setMode(m_mode);
    m_legacyStartupModeGuard = m_mode.trimmed();
    m_legacyStartupModeGuardUntilMs = QDateTime::currentMSecsSinceEpoch() + 6000;
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
    syncSpecialOperationToLegacyBackend();
    bridgeLog(QStringLiteral("legacyTxSync: mode=%1 outDev=%2 outChan=%3 inDev=%4 inChan=%5 tx=%6 rx=%7 currentTx=%8 txPeriod=%9 alt12=%10 txLevel=%11 legacyTxAttn=%12")
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
                       QString::number(qRound(qBound(0.0, m_txOutputLevel, 450.0))),
                       QString::number(legacyTxAttn)));
}

void DecodiumBridge::syncLegacyBackendTxState()
{
    if (!usingLegacyBackendForTx() && !useLegacyRigControlFallback(m_legacyBackend, m_catBackend)) {
        return;
    }

    syncLegacyBackendDialogState();
}

void DecodiumBridge::scheduleLegacyStateRefreshBurst()
{
    if (!legacyBackendAvailable()) {
        return;
    }

    if (!usingLegacyBackendForTx() && !useLegacyRigControlFallback(m_legacyBackend, m_catBackend)) {
        return;
    }

    if (m_legacyStateTimer && !m_legacyStateTimer->isActive()) {
        m_legacyStateTimer->start();
        bridgeLog(QStringLiteral("Legacy state timer started after backend became available"));
    }

    syncLegacyBackendState();

    static constexpr int kLegacyRefreshDelaysMs[] = {75, 180, 360};
    for (int const delayMs : kLegacyRefreshDelaysMs) {
        QTimer::singleShot(delayMs, this, [this]() {
            syncLegacyBackendState();
        });
    }
}

void DecodiumBridge::migrateActiveMonitoringToLegacyBackend()
{
    if (!legacyTxBackendRequested() || !legacyBackendAvailable() || !m_monitoring) {
        return;
    }

    bridgeLog(QStringLiteral("Migrating active monitor session to legacy backend"));
    ++m_periodTimerSessionId;

    if (m_periodTimer) {
        m_periodTimer->stop();
    }
    if (m_spectrumTimer) {
        m_spectrumTimer->stop();
    }
    if (m_asyncDecodeTimer) {
        m_asyncDecodeTimer->stop();
    }
    m_asyncDecodePending = false;

    stopAudioCapture();

    if (m_legacyBackend) {
        syncLegacyBackendTxState();
        m_legacyBackend->setMonitoring(true);
    }

    if (useModernSpectrumFeedWithLegacy()) {
        m_legacyPcmSpectrumFeed = false;
        updatePeriodTicksMax();
        {
            QMutexLocker locker(&m_audioBufferMutex);
            m_audioBuffer.clear();
            m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
        }
        m_spectrumBuf.clear();
        m_periodTicks = 0;
        m_lastPeriodSlot = -1;   // re-anchor UTC slot at next tick
        m_wfRingPos = 0;
        m_lastPanadapterData.clear();
        m_driftFrameCount = 0;
        m_driftExpectedFrames = 0;
        m_driftClock.restart();
        startAudioCapture();
        if (m_spectrumTimer) {
            m_spectrumTimer->start();
        }
        bridgeLog(QStringLiteral("Legacy monitor migration keeps modern FFT panadapter feed active"));
    } else {
        resetRxPeriodAccumulation(false);
        m_spectrumBuf.clear();
        m_wfRingPos = 0;
        m_lastPanadapterData.clear();
        m_legacyPcmSpectrumFeed = true;
        if (m_spectrumTimer) {
            m_spectrumTimer->start();
        }
        bridgeLog(QStringLiteral("Legacy monitor migration uses legacy PCM tap for fast QML panadapter"));
    }
}

void DecodiumBridge::syncLegacyBackendState()
{
    if (!usingLegacyBackendForTx() && !useLegacyRigControlFallback(m_legacyBackend, m_catBackend)) {
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

    bool const monitorBeforeLegacySync = m_monitoring;
    updateBool(m_monitoring, m_legacyBackend->monitoring(), [this]() { emit monitoringChanged(); });
    if (m_monitorRequested && monitorBeforeLegacySync && !m_monitoring) {
        scheduleMonitorRecovery(QStringLiteral("legacy state sync"),
                                m_periodTimerSessionId,
                                true);
    }
    updateBool(m_decoding, m_monitoring, [this]() { emit decodingChanged(); });
    updateBool(m_transmitting, m_legacyBackend->transmitting(), [this]() { emit transmittingChanged(); });
    updateBool(m_tuning, m_legacyBackend->tuning(), [this]() { emit tuningChanged(); });
    updateBool(m_holdTxFreq, m_legacyBackend->holdTxFreq(), [this]() { emit holdTxFreqChanged(); });

    bool legacyRigFrequencyAuthoritative = false;
    if (m_legacyBackend->rigControlEnabled()) {
        QString const legacyRigName = m_legacyBackend->rigName().trimmed();
        bool legacyCatConnected = m_legacyBackend->catConnected();
        if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && !legacyCatConnected) {
            legacyCatConnected = !legacyRigName.isEmpty();
        }
        legacyRigFrequencyAuthoritative = legacyCatConnected;
        updateBool(m_catConnected, legacyCatConnected, [this]() { emit catConnectedChanged(); });
        updateString(m_catRigName, m_catConnected ? legacyRigName : QString {}, [this]() { emit catRigNameChanged(); });
    }
    double const legacySignalLevel = m_legacyBackend->signalLevel();
    updateDouble(m_sMeter, legacySignalLevel, [this]() { emit sMeterChanged(); });
    if (usingLegacyBackendForTx() && !m_soundInput && !m_tciAudioCaptureActive) {
        // When the embedded legacy backend owns RX audio, the modern
        // DecodiumAudioSink is intentionally not opened. Feed the QML LVL meter
        // from the legacy S-meter so the header indicator works on macOS and
        // other platforms that run in legacy audio mode.
        double const legacyAudioLevel = m_monitoring
            ? qBound(0.0, legacySignalLevel / 250.0, 1.0)
            : 0.0;
        updateDouble(m_audioLevel, legacyAudioLevel, [this]() { emit audioLevelChanged(); });
    }
    updateDouble(m_txOutputLevel, txLevelFromLegacyAttenuation(m_legacyBackend->txOutputAttenuation()),
                 [this]() { emit txOutputLevelChanged(); });

    QString const legacyModeRaw = m_legacyBackend->mode().trimmed();
    QString const legacyMode = canonicalApplicationDecodeMode(legacyModeRaw);
    if (!legacyModeRaw.isEmpty() && legacyMode.isEmpty()) {
        // The legacy/CAT side can expose the radio modulation mode (USB, DATA-U,
        // FSK-R, ...). That is not a Decodium decoder mode and must not replace
        // m_mode, otherwise FT8/FT4 can show a live waterfall but never decode.
        if (m_catMode.compare(legacyModeRaw, Qt::CaseInsensitive) != 0) {
            m_catMode = legacyModeRaw;
            emit catModeChanged();
        }
    }
    if (!legacyMode.isEmpty()) {
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        if (m_legacyStartupModeGuardUntilMs > 0 && nowMs > m_legacyStartupModeGuardUntilMs) {
            m_legacyStartupModeGuard.clear();
            m_legacyStartupModeGuardUntilMs = 0;
        }
        bool const legacyStartupModeGuardActive =
            !m_legacyStartupModeGuard.isEmpty()
            && m_legacyStartupModeGuardUntilMs > 0
            && nowMs <= m_legacyStartupModeGuardUntilMs;
        if (legacyStartupModeGuardActive
            && legacyMode.compare(m_legacyStartupModeGuard, Qt::CaseInsensitive) != 0) {
            bridgeLog(QStringLiteral("legacy startup mode guard: ignoring backend mode %1, keeping %2")
                          .arg(legacyModeRaw, m_legacyStartupModeGuard));
            m_legacyBackend->setMode(m_legacyStartupModeGuard);
            return;
        }
        if (legacyStartupModeGuardActive
            && legacyMode.compare(m_legacyStartupModeGuard, Qt::CaseInsensitive) == 0) {
            m_legacyStartupModeGuard.clear();
            m_legacyStartupModeGuardUntilMs = 0;
        }
        if (m_bandManager) {
            m_bandManager->setCurrentMode(legacyMode);
        }
        QString const previousMode = m_mode;
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
            clearDecodeWindowsForModeChange(previousMode, legacyMode);
            emit modeChanged();
        }
    }

    double const legacyDialFrequency = m_legacyBackend->dialFrequency();
    if (legacyDialFrequency > 0.0) {
        bool const catOwnsDialFrequency =
            m_catConnected && !useLegacyRigControlFallback(m_legacyBackend, m_catBackend);
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        bool const localDialGuardActive =
            m_localCatFrequencyTargetHz > 0.0
            && m_localCatFrequencyGuardUntilMs > 0
            && nowMs <= m_localCatFrequencyGuardUntilMs;
        if (localDialGuardActive) {
            if (!qFuzzyCompare(m_frequency + 1.0, m_localCatFrequencyTargetHz + 1.0)) {
                m_frequency = m_localCatFrequencyTargetHz;
                emit frequencyChanged();
            }
            if (std::abs(legacyDialFrequency - m_localCatFrequencyTargetHz) > 1.0) {
                m_legacyBackend->setDialFrequency(m_localCatFrequencyTargetHz);
            }
            if (m_bandManager) {
                m_bandManager->updateFromFrequency(m_localCatFrequencyTargetHz);
            }
        } else if (catOwnsDialFrequency) {
            if (m_frequency > 0.0
                && !qFuzzyCompare(m_frequency + 1.0, legacyDialFrequency + 1.0)) {
                bridgeLog(QStringLiteral("legacy dial ignored while CAT authoritative: legacy=%1 bridge=%2")
                              .arg(QString::number(legacyDialFrequency, 'f', 0),
                                   QString::number(m_frequency, 'f', 0)));
                m_legacyBackend->setDialFrequency(m_frequency);
            }
        } else {
            if (!qFuzzyCompare(m_frequency + 1.0, legacyDialFrequency + 1.0)) {
                m_frequency = legacyDialFrequency;
                emit frequencyChanged();
            }
            if (m_bandManager) {
                m_bandManager->updateFromFrequency(legacyDialFrequency);
            }
            maybeApplyStartupModeFromRigFrequency(legacyDialFrequency, legacyRigFrequencyAuthoritative);
        }
    }

    int const legacyRxFrequency = qBound(0, m_legacyBackend->rxFrequency(), 5000);
    if (shouldIgnoreLegacyAudioFrequencyDuringLocalQsy(legacyRxFrequency, false)) {
        m_legacyBackend->setRxFrequency(m_localRxFrequencyTargetHz);
    } else {
        updateInt(m_rxFrequency, legacyRxFrequency, [this]() { emit rxFrequencyChanged(); });
    }
    int const legacyTxFrequency = qBound(0, m_legacyBackend->txFrequency(), 5000);
    if (shouldIgnoreLegacyAudioFrequencyDuringLocalQsy(legacyTxFrequency, true)) {
        m_legacyBackend->setTxFrequency(m_localTxFrequencyTargetHz);
    } else {
        updateInt(m_txFrequency, legacyTxFrequency, [this]() { emit txFrequencyChanged(); });
    }
    updateInt(m_txPeriod, m_legacyBackend->txFirst() ? 1 : 0, [this]() { emit txPeriodChanged(); });
    updateBool(m_alt12Enabled, m_legacyBackend->alt12Enabled(), [this]() { emit alt12EnabledChanged(); });
    if (usingLegacyBackendForTx()) {
        updateSpecialOperationFromLegacy(m_legacyBackend->specialOperationActivity());
    }
    if (usingLegacyBackendForTx() && m_foxMode) {
        QStringList const legacyQueue = m_legacyBackend->foxCallerQueueLines();
        if (m_callerQueue != legacyQueue) {
            m_callerQueue = legacyQueue;
            emit callerQueueChanged();
        }
    } else if (usingLegacyBackendForTx() && !m_multiAnswerMode && !m_callerQueue.isEmpty()) {
        m_callerQueue.clear();
        emit callerQueueChanged();
    }
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

    QString const legacyAudioInput = m_legacyBackend->audioInputDeviceName().trimmed();
    if (!legacyAudioInput.isEmpty()) {
        bool const bridgeInputAvailable =
            hasAudioDeviceNamed(QMediaDevices::audioInputs(), m_audioInputDevice);
        bool const legacyInputAvailable =
            hasAudioDeviceNamed(QMediaDevices::audioInputs(), legacyAudioInput);
        if (!m_audioInputDevice.trimmed().isEmpty()
            && bridgeInputAvailable
            && !audioDeviceNameMatches(legacyAudioInput, m_audioInputDevice)) {
            bridgeLog(QStringLiteral("syncLegacyBackendState: preserving bridge audio input [%1] over legacy [%2]")
                          .arg(m_audioInputDevice, legacyAudioInput));
            m_legacyBackend->setAudioInputDeviceName(m_audioInputDevice);
            m_legacyBackend->setAudioInputChannel(qBound(0, m_audioInputChannel, 3));
        } else if (legacyInputAvailable || m_audioInputDevice.trimmed().isEmpty()) {
            updateString(m_audioInputDevice, legacyAudioInput, [this]() { emit audioInputDeviceChanged(); });
        }
    }
    QString const legacyAudioOutput = m_legacyBackend->audioOutputDeviceName().trimmed();
    if (!legacyAudioOutput.isEmpty()) {
        bool const bridgeOutputAvailable =
            hasAudioDeviceNamed(QMediaDevices::audioOutputs(), m_audioOutputDevice);
        bool const legacyOutputAvailable =
            hasAudioDeviceNamed(QMediaDevices::audioOutputs(), legacyAudioOutput);
        if (!m_audioOutputDevice.trimmed().isEmpty()
            && bridgeOutputAvailable
            && !audioDeviceNameMatches(legacyAudioOutput, m_audioOutputDevice)) {
            bridgeLog(QStringLiteral("syncLegacyBackendState: preserving bridge audio output [%1] over legacy [%2]")
                          .arg(m_audioOutputDevice, legacyAudioOutput));
            m_legacyBackend->setAudioOutputDeviceName(m_audioOutputDevice);
            m_legacyBackend->setAudioOutputChannel(qBound(0, m_audioOutputChannel, 3));
        } else if (legacyOutputAvailable || m_audioOutputDevice.trimmed().isEmpty()) {
            updateString(m_audioOutputDevice, legacyAudioOutput, [this]() { emit audioOutputDeviceChanged(); });
        }
    }
    updateInt(m_audioInputChannel, qBound(0, m_legacyBackend->audioInputChannel(), 3),
              [this]() { emit audioInputChannelChanged(); });
    updateInt(m_audioOutputChannel, qBound(0, m_legacyBackend->audioOutputChannel(), 3),
              [this]() { emit audioOutputChannelChanged(); });
    updateDouble(m_rxInputLevel, qBound(0.0, static_cast<double>(m_legacyBackend->rxInputLevel()), 100.0),
                 [this]() { emit rxInputLevelChanged(); });
    updateInt(m_uiPaletteIndex, uiPaletteIndexForLegacyName(m_legacyBackend->waterfallPalette()),
              [this]() { emit uiPaletteIndexChanged(); });

    syncLegacyBackendDecodeList();
}

void DecodiumBridge::primeLegacyAllTxtCursor()
{
    QString const path = legacyAllTxtPath().trimmed();
    if (path.isEmpty()) {
        m_legacyAllTxtConsumedPath.clear();
        m_legacyAllTxtConsumedSize = -1;
        m_legacyAllTxtRevisionKey.clear();
        return;
    }

    QFileInfo const info(path);
    if (info.exists()) {
        m_legacyAllTxtConsumedPath = info.absoluteFilePath();
        m_legacyAllTxtConsumedSize = info.size();
        m_legacyAllTxtRevisionKey =
            info.absoluteFilePath()
            + QStringLiteral("|")
            + QString::number(info.size())
            + QStringLiteral("|")
            + QString::number(info.lastModified(QTimeZone::UTC).toMSecsSinceEpoch());
    } else {
        m_legacyAllTxtConsumedPath = path;
        m_legacyAllTxtConsumedSize = -1;
        m_legacyAllTxtRevisionKey = path;
    }
}

void DecodiumBridge::clearDecodeWindowsForModeChange(const QString& previousMode, const QString& nextMode)
{
    bridgeLog(QStringLiteral("mode-change decode reset: %1 -> %2").arg(previousMode, nextMode));

    m_legacyModeChangeClearedDecodeKeys.clear();
    auto rememberClearedKeys = [this](QVariantList const& entries) {
        for (QVariant const& value : entries) {
            QString const key = decodeMirrorEntryKey(value.toMap());
            if (!key.isEmpty()) {
                m_legacyModeChangeClearedDecodeKeys.insert(key);
            }
        }
    };
    rememberClearedKeys(m_decodeList);
    rememberClearedKeys(m_rxDecodeList);

    if (usingLegacyBackendForTx() && legacyBackendAvailable()) {
        rememberClearedKeys(mirrorLegacyDecodeLines(m_legacyBackend->bandActivityLines(), false, m_decodeList));
        rememberClearedKeys(mirrorLegacyDecodeLines(m_legacyBackend->rxFrequencyLines(), true, m_rxDecodeList));
        m_legacyBackend->clearBandActivity();
        m_legacyBackend->clearRxFrequency();
        m_legacyBandActivityRevision = -1;
        m_legacyRxFrequencyRevision = -1;
        m_legacyClearedRxMirrorKeys = m_legacyModeChangeClearedDecodeKeys;
        primeLegacyAllTxtCursor();
    } else {
        m_legacyAllTxtRevisionKey.clear();
        m_legacyClearedRxMirrorKeys.clear();
    }

    m_decodeList.clear();
    m_rxDecodeList.clear();
    clearRemoteActivityCache(true);
    if (m_activeStations) {
        m_activeStations->clear();
    }
    emit decodeListChanged();
    emit rxDecodeListChanged();
}

void DecodiumBridge::syncLegacyBackendDecodeList()
{
    if (!usingLegacyBackendForTx()) {
        return;
    }

    auto applyLegacyUiFilters = [this](QVariantList const& entries, bool applyCqOnly) {
        QVariantList filtered;
        filtered.reserve(entries.size());
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            if (!entry.value(QStringLiteral("isTx")).toBool()) {
                if (applyCqOnly
                    && m_filterCqOnly
                    && !entry.value(QStringLiteral("isCQ")).toBool()) {
                    continue;
                }
                if (m_filterMyCallOnly && !entry.value(QStringLiteral("isMyCall")).toBool()) {
                    continue;
                }
            }
            filtered.append(entry);
        }
        return filtered;
    };
    auto dropModeChangeClearedEntries = [this](QVariantList const& entries) {
        if (m_legacyModeChangeClearedDecodeKeys.isEmpty()) {
            return entries;
        }
        QVariantList filtered;
        filtered.reserve(entries.size());
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            if (m_legacyModeChangeClearedDecodeKeys.contains(decodeMirrorEntryKey(entry))) {
                continue;
            }
            filtered.append(entry);
        }
        return filtered;
    };

    int const bandRevision = m_legacyBackend->bandActivityRevision();
    int const rxRevision = m_legacyBackend->rxFrequencyRevision();
    bool const bandChanged = bandRevision != m_legacyBandActivityRevision;
    bool const rxChanged = rxRevision != m_legacyRxFrequencyRevision;
    QString allTxtRevisionKey;
    if (isTimeSyncDecodeMode(m_mode)) {
        QString const path = legacyAllTxtPath().trimmed();
        if (!path.isEmpty()) {
            QFileInfo const info(path);
            if (info.exists()) {
                allTxtRevisionKey =
                    info.absoluteFilePath()
                    + QStringLiteral("|")
                    + QString::number(info.size())
                    + QStringLiteral("|")
                    + QString::number(info.lastModified(QTimeZone::UTC).toMSecsSinceEpoch());
            } else {
                allTxtRevisionKey = path;
            }
        }
    }
    bool const allTxtChanged = allTxtRevisionKey != m_legacyAllTxtRevisionKey;

    if (!bandChanged && !rxChanged && !allTxtChanged) {
        return;
    }

    if (m_dxccLookup && !m_dxccLookup->isLoaded()) {
        QString loadedPath;
        if (reloadDxccLookup(&loadedPath)) {
            bridgeLog("DXCC: caricato on-demand da " + loadedPath + " durante il mirror del backend legacy");
        }
    }

    auto feedMamQueueFromEntries = [this](QVariantList const& entries) {
        if (!m_multiAnswerMode || m_manualTxHold) {
            return;
        }
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            if (entry.value(QStringLiteral("isTx")).toBool()) {
                continue;
            }
            QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
            if (message.isEmpty()) {
                continue;
            }
            bool freqOk = false;
            int const freq = entry.value(QStringLiteral("freq")).toString().trimmed().toInt(&freqOk);
            int const snr = decodeSnrOrDefault(entry.value(QStringLiteral("db")).toString(), -99);
            maybeEnqueueMamCallerFromMessage(message, freqOk ? freq : -1, snr);
        }
    };

    if (bandChanged || allTxtChanged) {
        m_legacyBandActivityRevision = bandRevision;
        QStringList const bandLines = m_legacyBackend->bandActivityLines();
        QVariantList mirroredDecodes = augmentLegacyMirrorWithAllTxt(QVariantList {}, false);
        QVariantList const widgetMirrored = mirrorLegacyDecodeLines(bandLines, false, m_decodeList);
        if (mirroredDecodes.isEmpty()) {
            mirroredDecodes = widgetMirrored;
        } else {
            QSet<QString> seen;
            for (QVariant const& value : std::as_const(mirroredDecodes)) {
                seen.insert(decodeMirrorEntryKey(value.toMap()));
            }
            for (QVariant const& value : std::as_const(widgetMirrored)) {
                QVariantMap const entry = value.toMap();
                QString const key = decodeMirrorEntryKey(entry);
                if (seen.contains(key)) {
                    continue;
                }
                seen.insert(key);
                mirroredDecodes.append(entry);
            }
        }
        mirroredDecodes = dropModeChangeClearedEntries(mirroredDecodes);
        feedMamQueueFromEntries(mirroredDecodes);
        if (m_activeStations) {
            m_activeStations->clear();
            for (QVariant const& value : std::as_const(mirroredDecodes)) {
                QVariantMap const entry = value.toMap();
                if (entry.value(QStringLiteral("isTx")).toBool() || !entry.value(QStringLiteral("isCQ")).toBool()) {
                    continue;
                }
                QString const fromCall = entry.value(QStringLiteral("fromCall")).toString().trimmed();
                if (fromCall.isEmpty()) {
                    continue;
                }
                bool ok = false;
                int const freqHz = entry.value(QStringLiteral("freq")).toString().trimmed().toInt(&ok);
                int const snr = entry.value(QStringLiteral("db")).toString().trimmed().toInt();
                if (!ok) {
                    continue;
                }
                m_activeStations->addStation(fromCall,
                                             freqHz,
                                             snr,
                                             entry.value(QStringLiteral("dxGrid")).toString(),
                                             entry.value(QStringLiteral("time")).toString(),
                                             true);
            }
        }
        mirroredDecodes = applyLegacyUiFilters(mirroredDecodes, true);
        normalizeDecodeEntriesForDisplay(mirroredDecodes, 1500, m_mode);
        bridgeLog(QStringLiteral("legacy-mirror band: raw=%1 mirrored=%2")
                      .arg(bandLines.size())
                      .arg(mirroredDecodes.size()));
        if (m_decodeList != mirroredDecodes) {
            m_decodeList = mirroredDecodes;
            emit decodeListChanged();
        }
    }

    if (rxChanged || allTxtChanged) {
        if (rxChanged) {
            m_legacyRxFrequencyRevision = rxRevision;
        }

        QStringList const rxLines = m_legacyBackend->rxFrequencyLines();
        QVariantList mergedRxDecodes = mirrorLegacyDecodeLines(rxLines, true, m_rxDecodeList);
        mergedRxDecodes = augmentLegacyMirrorWithAllTxt(mergedRxDecodes, true);
        mergedRxDecodes = dropModeChangeClearedEntries(mergedRxDecodes);
        bridgeLog(QStringLiteral("legacy-mirror rx: raw=%1 mirrored=%2")
                      .arg(rxLines.size())
                      .arg(mergedRxDecodes.size()));
        QSet<QString> seen;
        for (const QVariant& value : std::as_const(mergedRxDecodes)) {
            QString const key = decodeMirrorEntryKey(value.toMap());
            seen.insert(key);
            m_legacyClearedRxMirrorKeys.remove(key);
        }

        // CQ Only is a Band Activity filter only: keep using the raw legacy band
        // mirror here so direct callers and QSO replies still reach Signal RX.
        QVariantList bandFallbackDecodes =
            mirrorLegacyDecodeLines(m_legacyBackend->bandActivityLines(), false, m_decodeList);
        bandFallbackDecodes = augmentLegacyMirrorWithAllTxt(bandFallbackDecodes, false);
        bandFallbackDecodes = dropModeChangeClearedEntries(bandFallbackDecodes);

        // Legacy widget routing can occasionally miss FT8/FT4/FT2 messages directed to us
        // or belonging to the current QSO. Mirror them from Band Activity as a fallback.
        for (const QVariant& value : std::as_const(bandFallbackDecodes)) {
            QVariantMap const entry = value.toMap();
            if (!shouldMirrorToRxPane(entry)) {
                continue;
            }
            QString const key = decodeMirrorEntryKey(entry);
            if (m_legacyClearedRxMirrorKeys.contains(key)) {
                continue;
            }
            if (seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            mergedRxDecodes.append(entry);
        }

        feedMamQueueFromEntries(mergedRxDecodes);
        mergedRxDecodes = applyLegacyUiFilters(mergedRxDecodes, false);
        coalesceRxPaneTxRows(mergedRxDecodes, m_mode);
        normalizeDecodeEntriesForDisplay(mergedRxDecodes, 1500, m_mode);

        if (m_rxDecodeList != mergedRxDecodes) {
            m_rxDecodeList = mergedRxDecodes;
            emit rxDecodeListChanged();
        }
    }

    m_legacyAllTxtRevisionKey = allTxtRevisionKey;
}

QVariantList DecodiumBridge::mirrorLegacyDecodeLines(QStringList const& lines,
                                                     bool rxPane,
                                                     QVariantList const& previousEntries) const
{
    QVariantList mirroredDecodes;
    mirroredDecodes.reserve(lines.size());

    static const QRegularExpression timePrefix {R"(^(\d{4}|\d{6}))"};
    static const QRegularExpression txPattern {
        R"(^(\d{4}|\d{6})\s+Tx\S*\s+(\d+)\s+\S+\s+(.*)$)",
        QRegularExpression::CaseInsensitiveOption
    };
    static const QRegularExpression rxPattern {
        R"(^(\d{4}|\d{6})\s+(-?\d+)\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+\S+\s+(.*)$)",
        QRegularExpression::CaseInsensitiveOption
    };

    bool const isTimeSyncMode = isTimeSyncDecodeMode(m_mode);
    int const periodMs = periodMsForMode(m_mode);
    QString const currentSlotUtcToken =
        (isTimeSyncMode && periodMs > 0 && periodMs < 60000)
        ? utcDisplayTokenForSlotStart(QDateTime::currentMSecsSinceEpoch() / periodMs, periodMs)
        : QString {};

    auto stabilizeLegacyTimeToken = [&](QVariantMap& entry) {
        QString const rawTime = normalizeUtcDisplayToken(entry.value(QStringLiteral("time")).toString());
        if (!(isTimeSyncMode && periodMs > 0 && periodMs < 60000 && rawTime.size() == 4)) {
            entry[QStringLiteral("time")] = rawTime;
            return;
        }

        QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
        QString const freq = entry.value(QStringLiteral("freq")).toString().trimmed();
        QString const db = entry.value(QStringLiteral("db")).toString().trimmed();
        QString const dt = entry.value(QStringLiteral("dt")).toString().trimmed();
        QString resolvedTime;

        for (auto it = previousEntries.crbegin(); it != previousEntries.crend(); ++it) {
            QVariantMap const previous = it->toMap();
            QString const previousTime = normalizeUtcDisplayToken(previous.value(QStringLiteral("time")).toString());
            if (previousTime.size() < 6 || !previousTime.startsWith(rawTime)) {
                continue;
            }
            if (previous.value(QStringLiteral("message")).toString().trimmed() != message
                || previous.value(QStringLiteral("freq")).toString().trimmed() != freq
                || previous.value(QStringLiteral("db")).toString().trimmed() != db
                || previous.value(QStringLiteral("dt")).toString().trimmed() != dt) {
                continue;
            }
            resolvedTime = previousTime.left(6);
            break;
        }

        if (resolvedTime.isEmpty()) {
            resolvedTime = currentSlotUtcToken;
        }

        entry[QStringLiteral("time")] = resolvedTime.isEmpty() ? rawTime : resolvedTime;
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
            entry["mode"] = m_mode;
            entry["isTx"] = true;
            entry["isCQ"] = isCQ;
            QString const myCallUpper = m_callsign.trimmed().toUpper();
            QString const myBaseUpper = normalizedBaseCall(myCallUpper);
            entry["isMyCall"] = messageContainsCallToken(message, myCallUpper, myBaseUpper);
            entry["fromCall"] = extractDecodedCallsign(message, isCQ);
            entry["forceRxPane"] = rxPane;
            stabilizeLegacyTimeToken(entry);
            enrichDecodeEntry(entry);
            mirroredDecodes.append(entry);
            continue;
        }

        QString timeStr;
        QString dbStr;
        QString dtStr;
        QString freqStr;
        QString message;

        auto const rxMatch = rxPattern.match(rawLine);
        if (rxMatch.hasMatch()) {
            timeStr = normalizeUtcDisplayToken(rxMatch.captured(1));
            dbStr = rxMatch.captured(2).trimmed();
            dtStr = rxMatch.captured(3).trimmed();
            freqStr = rxMatch.captured(4).trimmed();
            message = rxMatch.captured(5).trimmed();
        } else {
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

            timeStr = normalizeUtcDisplayToken(timePrefix.match(cleanLine).hasMatch()
                                                   ? timePrefix.match(cleanLine).captured(1)
                                                   : QString {});
            dbStr = QString::number(decoded.snr());
            dtStr = QString::number(decoded.dt(), 'f', 1);
            freqStr = QString::number(decoded.frequencyOffset());
            message = cleanLine.mid(payloadStart).trimmed();
        }

        if (message.isEmpty()) {
            continue;
        }
        QString semanticRejectReason;
        if (!shouldAcceptDecodedMessage(message, &semanticRejectReason)) {
            bridgeLog(QStringLiteral("semantic decode filter legacy-mirror: %1 msg='%2'")
                          .arg(semanticRejectReason, message));
            continue;
        }

        bool const isCQ = message.startsWith("CQ ", Qt::CaseInsensitive)
                       || message == "CQ"
                       || message.startsWith("QRZ ", Qt::CaseInsensitive);
        QString const myCallUpper = m_callsign.trimmed().toUpper();
        QString const myBaseUpper = normalizedBaseCall(myCallUpper);
        bool const isMyCall = messageContainsCallToken(message, myCallUpper, myBaseUpper);

        QVariantMap entry;
        entry["time"] = timeStr;
        entry["db"] = dbStr;
        entry["dt"] = dtStr;
        entry["freq"] = freqStr;
        entry["message"] = message;
        entry["aptype"] = QString {};
        entry["quality"] = QStringLiteral("100");
        entry["mode"] = m_mode;
        entry["isTx"] = false;
        entry["isCQ"] = isCQ;
        entry["isMyCall"] = isMyCall;
        entry["fromCall"] = extractDecodedCallsign(message, isCQ);
        entry["forceRxPane"] = rxPane;
        stabilizeLegacyTimeToken(entry);
        enrichDecodeEntry(entry);
        mirroredDecodes.append(entry);
    }

    return mirroredDecodes;
}

bool DecodiumBridge::shouldMirrorToRxPane(const QVariantMap& entry) const
{
    if (entry.value("forceRxPane").toBool() || entry.value("isTx").toBool()) {
        return true;
    }

    QString const message = entry.value("message").toString().trimmed();
    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);
    bool const directedToMe = entry.value("isMyCall").toBool()
        || messageContainsCallToken(message, myCallUpper, myBaseUpper);
    if (directedToMe) {
        return true;
    }

    QString activePartner = m_dxCall.trimmed();
    if (activePartner.isEmpty()) {
        activePartner = m_autoCqLockedCall.trimmed();
    }
    if (activePartner.isEmpty()) {
        activePartner = inferredPartnerForAutolog().trimmed();
    }
    QString const activePartnerBase = normalizedBaseCall(activePartner);
    QString const fromCallBase = normalizedBaseCall(entry.value("fromCall").toString());
    bool const hasExchange = messageContainsExchangeToken(message);
    if (!activePartnerBase.isEmpty()) {
        bool const mentionsActivePartner =
            messageContainsCallToken(message, activePartner, activePartnerBase);
        bool const activePartnerTraffic =
            (mentionsActivePartner && (directedToMe || hasExchange))
            || (fromCallBase == activePartnerBase && hasExchange);
        if (activePartnerTraffic) {
            return true;
        }
    }

    bool ok = false;
    int const audioFreqHz = entry.value("freq").toString().trimmed().toInt(&ok);
    if (!ok) {
        return false;
    }

    return std::abs(audioFreqHz - m_rxFrequency) <= 200;
}

void DecodiumBridge::appendRxDecodeEntry(const QVariantMap& entry)
{
    if (usingLegacyBackendForTx() || !shouldMirrorToRxPane(entry)) {
        return;
    }

    QString const key = decodeMirrorEntryKey(entry);

    for (const QVariant& existingValue : std::as_const(m_rxDecodeList)) {
        QVariantMap const existing = existingValue.toMap();
        QString const existingKey = decodeMirrorEntryKey(existing);
        if (existingKey == key) {
            return;
        }
    }

    m_rxDecodeList.append(entry);
    coalesceRxPaneTxRows(m_rxDecodeList, m_mode);
    normalizeDecodeEntriesForDisplay(m_rxDecodeList, 1500, m_mode);
    emit rxDecodeListChanged();
}

void DecodiumBridge::rebuildRxDecodeList()
{
    if (usingLegacyBackendForTx()) {
        return;
    }

    QVariantList rebuilt;
    rebuilt.reserve(m_decodeList.size() + m_rxDecodeList.size());

    QSet<QString> seen;
    auto appendRelevantEntry = [this, &rebuilt, &seen](QVariant const& value) {
        QVariantMap const entry = value.toMap();
        if (!shouldMirrorToRxPane(entry)) {
            return;
        }

        QString const key = decodeMirrorEntryKey(entry);
        if (seen.contains(key)) {
            return;
        }
        seen.insert(key);
        rebuilt.append(entry);
    };

    for (const QVariant& value : std::as_const(m_rxDecodeList)) {
        appendRelevantEntry(value);
    }
    for (const QVariant& value : std::as_const(m_decodeList)) {
        appendRelevantEntry(value);
    }

    coalesceRxPaneTxRows(rebuilt, m_mode);
    normalizeDecodeEntriesForDisplay(rebuilt, 1500, m_mode);

    if (m_rxDecodeList != rebuilt) {
        m_rxDecodeList = rebuilt;
        emit rxDecodeListChanged();
    }
}

// === STATION ===
QString DecodiumBridge::callsign() const { return m_callsign; }
bool DecodiumBridge::pskReporterConnected() const { return m_pskReporter && m_pskReporter->isConnected(); }

void DecodiumBridge::setPskReporterEnabled(bool v)
{
    if (m_pskReporterEnabled == v) return;
    m_pskReporterEnabled = v;
    if (m_pskReporter) {
        m_pskReporter->setEnabled(v);
        if (v) {
            // Make sure the reporter knows our local station as soon as the
            // user enables it — otherwise the first batch flush would skip
            // because m_myCall is empty.
            m_pskReporter->setLocalStation(m_callsign, m_grid, pskReporterProgramInfo());
        }
    }
    emit pskReporterEnabledChanged();
    // setEnabled() above already emits connectedChanged() via the reporter,
    // but emit here too in case m_pskReporter is null at this point.
    emit pskReporterConnectedChanged();
}

void DecodiumBridge::setFtThreads(int v)
{
    const int clamped = std::clamp(v, 1, 8);
    if (m_ftThreads == clamped) return;
    m_ftThreads = clamped;
    QSettings("Decodium","Decodium3").setValue("ftThreads", m_ftThreads);
    emit ftThreadsChanged();
}

void DecodiumBridge::cycleFtThreads()
{
    setFtThreads(m_ftThreads >= 8 ? 1 : m_ftThreads + 1);
}

QString DecodiumBridge::pskReporterProgramInfo() const
{
    return QStringLiteral("Decodium4 v%1").arg(version()).simplified();
}

void DecodiumBridge::setCallsign(const QString& v) {
    if (m_callsign != v) {
        m_callsign = v;
        emit callsignChanged();
        if (m_pskReporter) m_pskReporter->setLocalStation(m_callsign, m_grid, pskReporterProgramInfo());
        if (m_dxCluster)   m_dxCluster->setCallsign(m_callsign);
        regenerateTxMessages();
    }
}
QString DecodiumBridge::grid() const { return m_grid; }
void DecodiumBridge::setGrid(const QString& v) {
    if (m_grid != v) {
        m_grid = v;
        emit gridChanged();
        if (m_pskReporter) m_pskReporter->setLocalStation(m_callsign, m_grid, pskReporterProgramInfo());
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
    bool const monitorShouldStayActive = m_monitoring || m_monitorRequested;
    quint64 const monitorSessionId = m_periodTimerSessionId;
    bool frequencyChangedForRecovery = false;
    double oldFreq = m_frequency;
    if (!qFuzzyCompare(m_frequency + 1.0, v + 1.0)) {
        m_frequency = v;
        frequencyChangedForRecovery = true;
        emit frequencyChanged();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setDialFrequency(v);
        }
        // Pulisci le finestre decode quando cambia banda (differenza > 100kHz)
        if (std::abs(v - oldFreq) > 100000.0) {
            m_decodeList.clear();
            m_rxDecodeList.clear();
            clearRemoteActivityCache(true);
            emit decodeListChanged();
            emit rxDecodeListChanged();
        }
    }
    if (m_bandManager) {
        m_bandManager->updateFromFrequency(v);
    }
    if (frequencyChangedForRecovery && std::abs(v - oldFreq) > 50000.0) {
        scheduleMonitorRecovery(QStringLiteral("frequency QSY %1 -> %2 Hz")
                                    .arg(QString::number(oldFreq, 'f', 0),
                                         QString::number(v, 'f', 0)),
                                monitorSessionId,
                                monitorShouldStayActive);
    }
}

QString DecodiumBridge::startupModeForFrequency(double dialFrequency) const
{
    if (dialFrequency <= 0.0 || !m_bandManager) {
        return {};
    }
    return m_bandManager->detectModeForFrequency(dialFrequency);
}

void DecodiumBridge::maybeApplyStartupModeFromRigFrequency(double dialFrequency,
                                                           bool authoritativeRigFrequency)
{
    if (dialFrequency <= 0.0) {
        return;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_startupModeAutoUntilMs > 0 && nowMs > m_startupModeAutoUntilMs) {
        m_startupModeAutoPending = false;
        return;
    }

    bool const canApplyStartupMode =
        m_startupModeAutoPending
        || (authoritativeRigFrequency && !m_startupModeAutoAuthoritativeApplied);
    if (!canApplyStartupMode) {
        return;
    }

    QString const startupMode = startupModeForFrequency(dialFrequency);
    if (startupMode.isEmpty()) {
        return;
    }

    m_startupModeAutoPending = false;
    if (authoritativeRigFrequency) {
        m_startupModeAutoAuthoritativeApplied = true;
    }
    if (startupMode == m_mode) {
        return;
    }

    bridgeLog(QStringLiteral("startup auto-mode%1: %2 Hz -> %3 (was %4)")
                  .arg(authoritativeRigFrequency ? QStringLiteral(" from CAT") : QString(),
                       QString::number(dialFrequency, 'f', 0), startupMode, m_mode));

    m_preserveFrequencyOnModeChange = true;
    setMode(startupMode);
    m_preserveFrequencyOnModeChange = false;
}

bool DecodiumBridge::isTimeSyncDecodeMode(const QString& mode) const
{
    QString const normalized = mode.trimmed().toUpper();
    return normalized == QStringLiteral("FT2")
        || normalized == QStringLiteral("FT4")
        || normalized == QStringLiteral("FT8");
}

void DecodiumBridge::resetEarlyDecodeSchedule()
{
    m_ft8EarlyDecodeSlot = -1;
    m_ft8EarlyDecode41Sent = false;
    m_ft8EarlyDecode47Sent = false;
    m_ft4EarlyDecodeSlot = -1;
    m_ft4EarlyDecodeSent = false;

    if (m_ft8Worker) {
        auto *worker = m_ft8Worker;
        QMetaObject::invokeMethod(worker, [worker]() {
            worker->resetDecoderState();
        }, Qt::QueuedConnection);
    }
}

void DecodiumBridge::resetRxPeriodAccumulation(bool reserveAudioBuffer)
{
    m_periodTicks = 0;
    m_lastPeriodSlot = -1;
    m_periodProgress = 0;
    emit periodProgressChanged();
    resetEarlyDecodeSchedule();
    m_pendingTimeSyncDecodeAudio.clear();
    m_pendingTimeSyncDecodeSlot = -1;
    m_pendingTimeSyncDecodeMode.clear();
    m_pendingTimeSyncDecodeActive = false;
    m_forcedDecodeAudioSnapshot.clear();
    m_forcedDecodeAudioSnapshotActive = false;

    QMutexLocker locker(&m_audioBufferMutex);
    m_audioBuffer.clear();
    if (reserveAudioBuffer) {
        m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
    }
}

void DecodiumBridge::requestRigFrequencyFromBridge(double hz, const QString& reason)
{
    if (hz <= 0.0) {
        return;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    m_localCatFrequencyTargetHz = hz;
    m_localCatFrequencyGuardUntilMs = nowMs + 8000;
    setFrequency(hz);

    if (!m_catConnected) {
        bridgeLog(QStringLiteral("Local QSY requested by %1: %2 Hz (CAT not connected, bridge state only)")
                      .arg(reason, QString::number(hz, 'f', 0)));
        return;
    }

    activeCatSetFreq(m_nativeCat, m_hamlibCat, m_catBackend, hz, m_omniRigCat, m_legacyBackend);
    syncActiveCatTxSplitFrequency(reason + QStringLiteral("/dial"));
    bridgeLog(QStringLiteral("CAT local QSY requested by %1: %2 Hz via %3")
                  .arg(reason,
                       QString::number(hz, 'f', 0),
                       m_catBackend));
}

bool DecodiumBridge::catSplitOperationActiveForMode() const
{
    if (!isHamlibFamilyBackend(m_catBackend) || !m_hamlibCat) {
        return false;
    }

    QString const splitMode = m_hamlibCat->splitMode().trimmed().toLower();
    if (splitMode != QStringLiteral("rig") && splitMode != QStringLiteral("emulate")) {
        return false;
    }

    QString const mode = canonicalApplicationDecodeMode(m_mode).trimmed().toUpper();
    bool const wsjtSplitMode =
        mode == QStringLiteral("FT8")
        || mode == QStringLiteral("FT4")
        || mode == QStringLiteral("FT2")
        || mode == QStringLiteral("FST4")
        || mode.startsWith(QStringLiteral("FST4-"));

    return !m_vhfUhfFeatures || wsjtSplitMode;
}

int DecodiumBridge::catSplitXitHzForTxFrequency(int txFrequencyHz) const
{
    if (!catSplitOperationActiveForMode() || txFrequencyHz <= 0) {
        return 0;
    }

    return (txFrequencyHz / 500) * 500 - 1500;
}

int DecodiumBridge::effectiveTxAudioFrequencyHz() const
{
    int const xitHz = catSplitXitHzForTxFrequency(m_txFrequency);
    return qBound(0, m_txFrequency - xitHz, 5000);
}

double DecodiumBridge::catSplitTxDialFrequencyHz() const
{
    if (!catSplitOperationActiveForMode() || m_frequency <= 0.0 || m_txFrequency <= 0) {
        return 0.0;
    }

    return m_frequency + static_cast<double>(catSplitXitHzForTxFrequency(m_txFrequency));
}

void DecodiumBridge::syncActiveCatTxSplitFrequency(const QString& reason)
{
    if (usingLegacyBackendForTx()
        || useLegacyRigControlFallback(m_legacyBackend, m_catBackend)
        || !isHamlibFamilyBackend(m_catBackend)
        || !m_hamlibCat
        || !m_hamlibCat->connected()) {
        return;
    }

    double const txDialHz = catSplitTxDialFrequencyHz();
    activeCatSetTxFreq(m_nativeCat, m_hamlibCat, m_catBackend, txDialHz, m_omniRigCat);

    QString const splitMode = m_hamlibCat->splitMode().trimmed().toLower();
    if (txDialHz > 0.0) {
        bridgeLog(QStringLiteral("CAT split sync (%1): mode=%2 dial=%3 tx_ui=%4 xit=%5 tx_dial=%6 tx_audio=%7 via %8")
                      .arg(reason,
                           splitMode,
                           QString::number(m_frequency, 'f', 0),
                           QString::number(m_txFrequency),
                           QString::number(catSplitXitHzForTxFrequency(m_txFrequency)),
                           QString::number(txDialHz, 'f', 0),
                           QString::number(effectiveTxAudioFrequencyHz()),
                           m_catBackend));
    } else {
        bridgeLog(QStringLiteral("CAT split sync (%1): split off mode=%2 tx_ui=%3 via %4")
                      .arg(reason,
                           splitMode.isEmpty() ? QStringLiteral("none") : splitMode,
                           QString::number(m_txFrequency),
                           m_catBackend));
    }
}

bool DecodiumBridge::shouldIgnoreCatFrequencyDuringLocalQsy(double hz, const QString& backend)
{
    if (hz <= 0.0 || m_localCatFrequencyTargetHz <= 0.0) {
        return false;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_localCatFrequencyGuardUntilMs <= 0 || nowMs > m_localCatFrequencyGuardUntilMs) {
        return false;
    }

    if (std::abs(hz - m_localCatFrequencyTargetHz) <= 1.0) {
        return false;
    }

    if (nowMs - m_lastIgnoredCatFrequencyLogMs >= 500) {
        bridgeLog(QStringLiteral("CAT[%1] stale frequency ignored after local QSY: rig=%2 target=%3 guard_ms=%4")
                      .arg(backend,
                           QString::number(hz, 'f', 0),
                           QString::number(m_localCatFrequencyTargetHz, 'f', 0),
                           QString::number(m_localCatFrequencyGuardUntilMs - nowMs)));
        m_lastIgnoredCatFrequencyLogMs = nowMs;
    }
    return true;
}

void DecodiumBridge::applyRemoteDialFrequency(double hz, const QString& reason)
{
    if (hz <= 0.0) {
        return;
    }

    requestRigFrequencyFromBridge(hz, reason);
    if (m_monitoring) {
        resetRxPeriodAccumulation(true);
    }
}

bool DecodiumBridge::shouldIgnoreLegacyAudioFrequencyDuringLocalQsy(int hz, bool tx)
{
    int const targetHz = tx ? m_localTxFrequencyTargetHz : m_localRxFrequencyTargetHz;
    qint64& guardUntilMs = tx ? m_localTxFrequencyGuardUntilMs : m_localRxFrequencyGuardUntilMs;
    int& targetRef = tx ? m_localTxFrequencyTargetHz : m_localRxFrequencyTargetHz;

    if (targetHz < 0 || guardUntilMs <= 0) {
        return false;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs > guardUntilMs) {
        guardUntilMs = 0;
        targetRef = 0;
        return false;
    }

    if (hz == targetHz) {
        guardUntilMs = 0;
        targetRef = 0;
        return false;
    }

    return true;
}

void DecodiumBridge::applyRemoteBandChange(const QString& band)
{
    if (!m_bandManager) {
        return;
    }

    QString normalized = band.trimmed().toUpper();
    if (normalized.isEmpty()) {
        return;
    }

    QString const beforeBand = m_bandManager->currentBandLambda();
    double const beforeFrequency = m_frequency;
    m_bandManager->changeBandByLambda(normalized);
    QString const afterBand = m_bandManager->currentBandLambda();
    if (afterBand.compare(beforeBand, Qt::CaseInsensitive) == 0
        && normalized.compare(beforeBand, Qt::CaseInsensitive) != 0
        && qFuzzyCompare(beforeFrequency + 1.0, m_frequency + 1.0)) {
        bridgeLog(QStringLiteral("Remote band change ignored: unknown band %1").arg(normalized));
        return;
    }

    double const targetFrequency = m_bandManager->dialFrequency();
    if (targetFrequency > 0.0
        && (!qFuzzyCompare(m_frequency + 1.0, targetFrequency + 1.0)
            || std::abs(m_localCatFrequencyTargetHz - targetFrequency) > 1.0)) {
        setFrequency(targetFrequency);
        requestRigFrequencyFromBridge(targetFrequency, QStringLiteral("remote-band"));
    }
    bridgeLog(QStringLiteral("Remote band change applied: %1 -> %2 Hz")
                  .arg(afterBand, QString::number(m_frequency, 'f', 0)));
}

void DecodiumBridge::armPeriodTimerForCurrentMode(quint64 sessionId, const QString& reason)
{
    if (!m_periodTimer) {
        return;
    }

    m_periodTimer->stop();

    QString const modeSnapshot = m_mode;
    int const periodMs = periodMsForMode(modeSnapshot);
    qint64 const msNow = QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay();
    int const msInPeriod = (periodMs > 0) ? static_cast<int>(msNow % static_cast<qint64>(periodMs)) : 0;
    int msToNext = (periodMs > 0) ? (periodMs - msInPeriod) : TIMER_MS;
    if (msToNext < TIMER_MS) {
        msToNext += periodMs;
    }

    bridgeLog(reason + ": UTC sync wait=" + QString::number(msToNext) +
              "ms (period=" + QString::number(periodMs) +
              "ms mode=" + modeSnapshot +
              " session=" + QString::number(sessionId) + ")");

    QTimer::singleShot(msToNext, this, [this, sessionId, modeSnapshot]() {
        if (!m_monitoring) {
            return;
        }
        if (sessionId != m_periodTimerSessionId || m_mode != modeSnapshot) {
            bridgeLog("period timer start skipped: stale session/mode (wanted mode=" +
                      modeSnapshot + " session=" + QString::number(sessionId) +
                      ", current mode=" + m_mode +
                      " session=" + QString::number(m_periodTimerSessionId) + ")");
            return;
        }

        resetRxPeriodAccumulation(true);
        m_periodTimer->start();
        bridgeLog("period timer started at UTC boundary (mode=" + modeSnapshot +
                  " session=" + QString::number(sessionId) + ")");
    });
}

int DecodiumBridge::minimumDecodeSamplesForMode(const QString& mode) const
{
    QString const normalized = mode.trimmed().toUpper();
    if (normalized == QStringLiteral("FT8")) {
        return 144000; // 80% of a 15s, 12 kHz FT8 window.
    }
    if (normalized == QStringLiteral("FT4")) {
        return 60000;  // Leaves margin for FT4's shorter DSP window while dropping startup fragments.
    }
    return 0;
}

int DecodiumBridge::targetDecodeSamplesForMode(const QString& mode) const
{
    int const periodMs = periodMsForMode(mode);
    if (periodMs <= 0) {
        return 0;
    }
    return periodMs * SAMPLE_RATE / 1000;
}

void DecodiumBridge::applyNtpSettings()
{
    QSettings s(QStringLiteral("Decodium"), QStringLiteral("Decodium3"));

    bool const enabled = s.value(QStringLiteral("NTPEnabled"), false).toBool();
    QString const customServer =
        s.value(QStringLiteral("NTPCustomServer"), QString()).toString().trimmed();
    double const storedOffset = s.value(QStringLiteral("NTPOffset_ms"), 0.0).toDouble();

    bool const enabledChanged = (m_ntpEnabled != enabled);
    m_ntpEnabled = enabled;
    m_ntpCustomServer = customServer;
    if (enabledChanged) {
        emit ntpEnabledChanged();
    }

    if (!qFuzzyCompare(m_ntpOffsetMs + 1.0, storedOffset + 1.0)) {
        m_ntpOffsetMs = storedOffset;
        emit ntpOffsetMsChanged();
    }

    setGlobalNtpOffsetMs(enabled ? storedOffset : 0.0);

    bool const previousSynced = m_ntpSynced;
    if (!enabled) {
        m_ntpSynced = false;
    }
    if (previousSynced != m_ntpSynced) {
        emit ntpSyncedChanged();
    }

    if (!m_ntpClient) {
        return;
    }

    m_ntpClient->setCustomServer(customServer);
    m_ntpClient->setInitialOffset(storedOffset);
    configureNtpClientForMode(m_mode);
    m_ntpClient->setEnabled(enabled);
    bridgeLog(QStringLiteral("NTP settings applied: enabled=%1 server=%2 offset=%3 ms")
                  .arg(enabled ? QStringLiteral("1") : QStringLiteral("0"),
                       customServer.isEmpty() ? QStringLiteral("<auto>") : customServer,
                       QString::number(storedOffset, 'f', 1)));
    if (enabled) {
        m_ntpClient->syncNow();
    }
}

void DecodiumBridge::configureNtpClientForMode(const QString& mode)
{
    if (!m_ntpClient) {
        return;
    }

    QString const normalized = mode.trimmed().toUpper();
    if (normalized == QStringLiteral("FT2")) {
        m_ntpClient->setRefreshInterval(60000);
        m_ntpClient->setMaxRtt(55.0);
    } else if (normalized == QStringLiteral("FT4")) {
        m_ntpClient->setRefreshInterval(60000);
        m_ntpClient->setMaxRtt(65.0);
    } else if (normalized == QStringLiteral("FT8")) {
        m_ntpClient->setRefreshInterval(60000);
        m_ntpClient->setMaxRtt(60.0);
    } else {
        m_ntpClient->setRefreshInterval(300000);
        m_ntpClient->setMaxRtt(250.0);
    }
}

void DecodiumBridge::resetStartupTransientQsoState()
{
    // A startup must never reopen a stale QSO from persisted settings.
    m_txEnabled = false;
    m_manualTxHold = false;
    m_qsoProgress = 0;
    m_dxCall.clear();
    m_dxGrid.clear();
    m_tx1.clear();
    m_tx2.clear();
    m_tx3.clear();
    m_tx4.clear();
    m_tx5.clear();
    m_reportSent.clear();
    m_reportReceived = QStringLiteral("-10");
    m_txRetryCount = 0;
    m_lastNtx = -1;
    m_nTx73 = 0;
    m_lastTransmittedMessage.clear();
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();
    m_qsoCooldown.clear();
    clearPendingAutoLogSnapshot();
    clearLateAutoLogSnapshot();
    clearAutoCqPartnerLock();
    regenerateTxMessages();
    m_currentTx = m_tx6.trimmed().isEmpty() ? 1 : 6;
}

void DecodiumBridge::purgePersistentTransientQsoState()
{
    QSettings s(QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    s.remove(QStringLiteral("dxCall"));
    s.remove(QStringLiteral("dxGrid"));
    s.remove(QStringLiteral("tx1"));
    s.remove(QStringLiteral("tx2"));
    s.remove(QStringLiteral("tx3"));
    s.remove(QStringLiteral("tx4"));
    s.remove(QStringLiteral("tx5"));
    s.remove(QStringLiteral("reportReceived"));
    s.sync();
}

void DecodiumBridge::resetTimeSyncDecodeMetrics()
{
    m_decodeStartMsBySerial.clear();
    m_decodeModeBySerial.clear();
    m_decodeUtcTokenBySerial.clear();
    bool const sampleCountChanged = (m_dtLastSampleCount != 0);
    m_dtLastSampleCount = 0;
    m_totalDecodesForDt = 0;
    m_dtSmoothFactor = 0.5;

    bool const avgChanged = !qFuzzyCompare(m_avgDt + 1.0, 1.0);
    bool const latencyChanged = !qFuzzyCompare(m_decodeLatencyMs + 1.0, 1.0);

    m_avgDt = 0.0;
    m_decodeLatencyMs = 0.0;

    if (avgChanged) {
        emit avgDtChanged();
    }
    if (latencyChanged) {
        emit decodeLatencyMsChanged();
    }
    if (sampleCountChanged) {
        emit timeSyncSampleCountChanged();
    }
}

bool DecodiumBridge::shouldTrackDtSample(int snr, double dtValue, const QString& message,
                                         const QString& decodeMode) const
{
    QString const normalized = decodeMode.trimmed().toUpper();
    if (normalized != QStringLiteral("FT4") && normalized != QStringLiteral("FT8")) {
        return false;
    }

    int const snrThreshold = (normalized == QStringLiteral("FT8")) ? -15 : -16;
    double const dtLimit = (normalized == QStringLiteral("FT8")) ? 0.60 : 0.80;

    if (snr < snrThreshold) {
        return false;
    }
    if (std::abs(dtValue) >= dtLimit) {
        return false;
    }

    return !message.trimmed().endsWith(QLatin1Char('?'));
}

void DecodiumBridge::finalizeTimeSyncDecodeCycle(quint64 serial, const QString& decodeMode,
                                                 const QVector<double>& dtSamples)
{
    qint64 const startedAtMs = m_decodeStartMsBySerial.take(serial);
    m_decodeModeBySerial.remove(serial);
    m_decodeUtcTokenBySerial.remove(serial);

    if (!isTimeSyncDecodeMode(decodeMode)) {
        return;
    }

    if (startedAtMs > 0) {
        double const latencyMs =
            static_cast<double>(QDateTime::currentMSecsSinceEpoch() - startedAtMs);
        if (!qFuzzyCompare(m_decodeLatencyMs + 1.0, latencyMs + 1.0)) {
            m_decodeLatencyMs = latencyMs;
            emit decodeLatencyMsChanged();
        }
    }

    int const nextSampleCount = dtSamples.size();
    if (m_dtLastSampleCount != nextSampleCount) {
        m_dtLastSampleCount = nextSampleCount;
        emit timeSyncSampleCountChanged();
    } else {
        m_dtLastSampleCount = nextSampleCount;
    }

    bool avgChanged = false;
    if (dtSamples.size() >= m_dtMinSamples) {
        QVector<double> sorted = dtSamples;
        std::sort(sorted.begin(), sorted.end());
        double const medianDt = sorted[sorted.size() / 2];

        if (m_totalDecodesForDt < 20) {
            m_dtSmoothFactor = 0.5;
        } else if (std::abs(m_avgDt) < 0.1) {
            m_dtSmoothFactor = 0.15;
        } else {
            m_dtSmoothFactor = 0.3;
        }

        double const nextAvg = (m_totalDecodesForDt == 0)
            ? medianDt
            : (m_dtSmoothFactor * medianDt + (1.0 - m_dtSmoothFactor) * m_avgDt);

        if (!qFuzzyCompare(m_avgDt + 1.0, nextAvg + 1.0)) {
            m_avgDt = nextAvg;
            avgChanged = true;
        }
        m_totalDecodesForDt += dtSamples.size();
    } else if (dtSamples.isEmpty() && m_totalDecodesForDt > 0) {
        double const decayedAvg = (std::abs(m_avgDt) < 0.0005) ? 0.0 : (m_avgDt * 0.9);
        if (!qFuzzyCompare(m_avgDt + 1.0, decayedAvg + 1.0)) {
            m_avgDt = decayedAvg;
            avgChanged = true;
        }
    }

    if (avgChanged) {
        emit avgDtChanged();
    }
}

QString DecodiumBridge::mode() const { return m_mode; }
void DecodiumBridge::setMode(const QString& v) {
    if (m_mode != v) {
        QString const previousMode = m_mode;
        bool const monitorWasActive = m_monitoring;
        bool const monitorShouldStayActive = monitorWasActive || m_monitorRequested;
        bool const rearmModernMonitor = monitorWasActive && !usingLegacyBackendForTx();
        quint64 const monitorSessionId = monitorShouldStayActive ? ++m_periodTimerSessionId : m_periodTimerSessionId;
        if (rearmModernMonitor && m_periodTimer) {
            m_periodTimer->stop();
        }

        m_mode = v;
        // A mode switch invalidates short-lived CAT/audio-frequency guards
        // from the previous mode. Leaving them active can make a later CAT
        // poll look stale and keep the displayed frequency stuck.
        m_localCatFrequencyGuardUntilMs = 0;
        m_localCatFrequencyTargetHz = 0.0;
        m_localRxFrequencyGuardUntilMs = 0;
        m_localRxFrequencyTargetHz = 0;
        m_localTxFrequencyGuardUntilMs = 0;
        m_localTxFrequencyTargetHz = 0;
        invalidateTxAudioCache();
        updatePeriodTicksMax();
        resetTimeSyncDecodeMetrics();
        configureNtpClientForMode(m_mode);
        if (m_ntpClient && m_ntpEnabled && isTimeSyncDecodeMode(m_mode)) {
            m_ntpClient->syncNow();
        }
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
        if (m_preserveFrequencyOnModeChange) {
            m_bandManager->updateFromFrequency(m_frequency);
        } else {
            m_bandManager->updateForMode(v);
            double const targetFrequency = m_bandManager->dialFrequency();
            if (targetFrequency > 0.0
                && (!qFuzzyCompare(m_frequency + 1.0, targetFrequency + 1.0)
                    || std::abs(m_localCatFrequencyTargetHz - targetFrequency) > 1.0)) {
                setFrequency(targetFrequency);
                requestRigFrequencyFromBridge(targetFrequency, QStringLiteral("remote/mode"));
            }
        }
        // Riavvia la cattura se attiva per adattare il periodo
        if (m_monitoring) {
            resetRxPeriodAccumulation(true);
            // Avvia/ferma async timer in base al modo
            if (m_mode == "FT2") {
                // store(release): il writer (callback audio) usa CAS sul commit
                // di m_asyncAudioPos, quindi questo reset annulla correttamente
                // eventuali increment in volo — il CAS del writer fallirà e il
                // campione verrà scartato.
                m_asyncAudioPos.store(0, std::memory_order_release);
                m_asyncDecodePending = false;
                m_asyncDecodeTimer->start();
            } else {
                m_asyncDecodeTimer->stop();
                m_asyncDecodePending = false;
            }
            if (rearmModernMonitor) {
                restartAudioCaptureForModeChange(previousMode);
                armPeriodTimerForCurrentMode(monitorSessionId, QStringLiteral("setMode"));
            }
        }
        clearDecodeWindowsForModeChange(previousMode, m_mode);

        emit modeChanged();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setMode(v);
            m_legacyStartupModeGuard = v.trimmed();
            m_legacyStartupModeGuardUntilMs = QDateTime::currentMSecsSinceEpoch() + 6000;
        }
        QString const configuredRigMode = configuredCatRigMode();
        if (!configuredRigMode.isEmpty()) {
            applyConfiguredCatRigMode(QStringLiteral("mode-change"));
        } else if (m_catBackend == QStringLiteral("tci")
            && m_hamlibCat
            && m_hamlibCat->connected()
            && !canonicalApplicationDecodeMode(v).isEmpty()) {
            m_hamlibCat->setRigMode(v);
        }
        syncActiveCatTxSplitFrequency(QStringLiteral("mode-change"));
        scheduleModeChangeMonitorRecovery(previousMode, m_mode, monitorSessionId, monitorShouldStayActive);
        scheduleTxAudioPrecompute();
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
    f = qBound(0, f, 5000);
    m_localRxFrequencyTargetHz = f;
    m_localRxFrequencyGuardUntilMs = QDateTime::currentMSecsSinceEpoch() + 2500;
    if (m_rxFrequency != f) {
        m_rxFrequency = f;
        emit rxFrequencyChanged();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setRxFrequency(f);
        }
        if (!usingLegacyBackendForTx()) {
            rebuildRxDecodeList();
        }
    }
}

void DecodiumBridge::setTxFrequency(int f)
{
    f = qBound(0, f, 5000);
    m_localTxFrequencyTargetHz = f;
    m_localTxFrequencyGuardUntilMs = QDateTime::currentMSecsSinceEpoch() + 2500;
    if (m_txFrequency != f) {
        m_txFrequency = f;
        emit txFrequencyChanged();
        invalidateTxAudioCache();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setTxFrequency(f);
        }
        syncActiveCatTxSplitFrequency(QStringLiteral("tx-audio-change"));
        scheduleTxAudioPrecompute();
    }
}

// === AUDIO ===
double DecodiumBridge::audioLevel() const { return m_audioLevel; }
double DecodiumBridge::sMeter() const { return m_sMeter; }

void DecodiumBridge::setRxInputLevel(double v)
{
    double const bounded = qBound(0.0, v, 100.0);
    if (!qFuzzyCompare(m_rxInputLevel + 1.0, bounded + 1.0)) {
        bridgeLog(QStringLiteral("setRxInputLevel: requested=%1 previous=%2 legacy=%3")
                      .arg(QString::number(bounded, 'f', 1),
                           QString::number(m_rxInputLevel, 'f', 1),
                           usingLegacyBackendForTx() ? QStringLiteral("1") : QStringLiteral("0")));
        m_rxInputLevel = bounded;
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setRxInputLevel(qRound(m_rxInputLevel));
        }
        if (m_soundInput) m_soundInput->setInputGain(rxInputGainFromLevel(m_rxInputLevel));
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
        m_legacyBackend->setTxOutputAttenuation(legacyTxAttenuationFromLevel(m_txOutputLevel));
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

void DecodiumBridge::syncAudioDeviceSettingsToLegacyIni()
{
    syncSettingToLegacyIni(QStringLiteral("SoundInName"), m_audioInputDevice);
    syncSettingToLegacyIni(QStringLiteral("SoundOutName"), m_audioOutputDevice);
    syncSettingToLegacyIni(QStringLiteral("AudioInputChannel"), audioChannelSettingValue(m_audioInputChannel));
    syncSettingToLegacyIni(QStringLiteral("AudioOutputChannel"), audioChannelSettingValue(m_audioOutputChannel));
}

void DecodiumBridge::applyAudioInputRuntimeChange(const QString& reason)
{
    if (usingLegacyBackendForTx() && !useModernSpectrumFeedWithLegacy()) {
        bridgeLog(QStringLiteral("audio input runtime change handled by legacy backend (%1)").arg(reason));
        return;
    }

    if (!m_monitoring) {
        return;
    }

    if (m_transmitting || m_tuning) {
        bridgeLog(QStringLiteral("audio input runtime change deferred while TX/Tune is active (%1)").arg(reason));
        scheduleMonitorRecovery(QStringLiteral("audio input change"), m_periodTimerSessionId, true);
        return;
    }

    if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend)
        && !m_soundInput
        && !m_tciAudioCaptureActive
        && !usingTciAudioInput()) {
        bridgeLog(QStringLiteral("audio input runtime change skipped; legacy fallback owns RX audio (%1)").arg(reason));
        return;
    }

    qint64 const now = QDateTime::currentMSecsSinceEpoch();
    bridgeLog(QStringLiteral("audio input runtime change: restarting RX capture (%1)").arg(reason));
    stopAudioCapture();
    m_audioWatchdogIgnoreUntilMs = now + 5000;
    m_audioUnhealthyStartMs = 0;
    resetRxPeriodAccumulation(true);
    resetTimeSyncDecodeMetrics();
    m_spectrumBuf.clear();
    m_wfRingPos = 0;
    m_lastPanadapterData.clear();
    m_lastPanadapterFrameMs = now;
    m_lastSpectrumRecoveryMs = now;
    m_driftFrameCount = 0;
    m_driftExpectedFrames = 0;
    m_driftClock.restart();
    m_asyncDecodePending = false;
    if (m_mode == QStringLiteral("FT2")) {
        m_asyncAudioPos.store(0, std::memory_order_release);
    }
    startAudioCapture();
    emit statusMessage(QStringLiteral("Audio RX aggiornato: %1").arg(m_audioInputDevice));
}

void DecodiumBridge::applyAudioOutputRuntimeChange(const QString& reason)
{
    bridgeLog(QStringLiteral("audio output runtime change: invalidating TX output (%1)").arg(reason));
    m_cachedTxOutputDeviceValid = false;
    invalidateTxAudioCache();

    if (m_soundOutput) {
        m_soundOutput->setAttenuation(txAttenuationFromSlider(m_txOutputLevel));
    }
    if (m_txAudioSink) {
        m_txAudioSink->setVolume(static_cast<float>(txGainFromSlider(m_txOutputLevel)));
    }

    if (m_transmitting || m_tuning) {
        bridgeLog(QStringLiteral("audio output device will be used from next TX/Tune (%1)").arg(reason));
        return;
    }

    scheduleTxAudioPrecompute();
}

void DecodiumBridge::setAudioInputDevice(const QString& v) {
    QString const value = v.trimmed();
    if (m_audioInputDevice != value) {
        QString const previous = m_audioInputDevice;
        m_audioInputDevice = value;
        bridgeLog(QStringLiteral("audioInputDevice set to: %1").arg(m_audioInputDevice));
        syncAudioDeviceSettingsToLegacyIni();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setAudioInputDeviceName(value);
        }
        emit audioInputDeviceChanged();
        applyAudioInputRuntimeChange(QStringLiteral("device %1 -> %2").arg(previous, m_audioInputDevice));
    }
}
QString DecodiumBridge::audioOutputDevice() const { return m_audioOutputDevice; }
void DecodiumBridge::setAudioOutputDevice(const QString& v) {
    QString const value = v.trimmed();
    if (m_audioOutputDevice != value) {
        QString const previous = m_audioOutputDevice;
        m_audioOutputDevice = value;
        bridgeLog("audioOutputDevice set to: " + m_audioOutputDevice);
        syncAudioDeviceSettingsToLegacyIni();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setAudioOutputDeviceName(value);
        }
        emit audioOutputDeviceChanged();
        applyAudioOutputRuntimeChange(QStringLiteral("device %1 -> %2").arg(previous, m_audioOutputDevice));
    }
}
int DecodiumBridge::audioInputChannel() const { return m_audioInputChannel; }
void DecodiumBridge::setAudioInputChannel(int v) {
    v = qBound(0, v, 3);
    if (m_audioInputChannel != v) {
        int const previous = m_audioInputChannel;
        m_audioInputChannel = v;
        syncAudioDeviceSettingsToLegacyIni();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setAudioInputChannel(v);
        }
        emit audioInputChannelChanged();
        applyAudioInputRuntimeChange(QStringLiteral("channel %1 -> %2").arg(previous).arg(m_audioInputChannel));
    }
}
int DecodiumBridge::audioOutputChannel() const { return m_audioOutputChannel; }
void DecodiumBridge::setAudioOutputChannel(int v) {
    v = qBound(0, v, 3);
    if (m_audioOutputChannel != v) {
        int const previous = m_audioOutputChannel;
        m_audioOutputChannel = v;
        syncAudioDeviceSettingsToLegacyIni();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setAudioOutputChannel(v);
        }
        emit audioOutputChannelChanged();
        applyAudioOutputRuntimeChange(QStringLiteral("channel %1 -> %2").arg(previous).arg(m_audioOutputChannel));
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
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
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
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
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
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
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
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
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
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
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
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
    }
}
int DecodiumBridge::currentTx() const { return m_currentTx; }
void DecodiumBridge::setCurrentTx(int v) {
    if (m_currentTx!=v) {
        m_currentTx=v;
        emit currentTxChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->selectTxMessage(qBound(1, v, 6));
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
    }
}
QString DecodiumBridge::dxCall() const { return m_dxCall; }
void DecodiumBridge::setDxCall(const QString& v) {
    if (m_dxCall != v) {
        m_dxCall = v;
        QString const activeQueueCall = normalizedBaseCall(v);
        // Cambio partner: resetta l'ultimo messaggio trasmesso, altrimenti il
        // controllo `messageCarries73Payload(m_lastTransmittedMessage)` in
        // autoSequenceStep ricorderebbe il "73" del QSO precedente e marcherebbe
        // localSignoffAlreadySent=true al primo decode del nuovo partner,
        // facendo saltare l'invio del nostro signoff e loggando un QSO
        // inesistente. Pulizia simmetrica al reset di setDxCall("").
        m_lastTransmittedMessage.clear();
        emit dxCallChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setDxCall(v);
        }
        regenerateTxMessages();
        if (m_multiAnswerMode && !activeQueueCall.isEmpty()) {
            removeCallerFromQueue(activeQueueCall);
        }
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

void DecodiumBridge::setHoldTxFreq(bool v)
{
    if (m_holdTxFreq != v) {
        m_holdTxFreq = v;
        emit holdTxFreqChanged();
        if (m_legacyBackend) {
            m_legacyBackend->setHoldTxFreq(v);
        }
    }
}

void DecodiumBridge::setTxEnabled(bool v)
{
    if (v) {
        clearManualTxHold(QStringLiteral("tx-enabled"));
    }

    bool const changed = (m_txEnabled != v);
    if (changed) {
        m_txEnabled = v;
        emit txEnabledChanged();
    }

    // The legacy backend can clear its own TX-enable state after completing a
    // QSO while the QML bridge still shows TX enabled. A subsequent double-click
    // must therefore re-assert the command even when the bridge boolean did not
    // change, otherwise the new message is prepared but never transmitted.
    if (usingLegacyBackendForTx()) {
        syncLegacyBackendTxState();
        m_legacyBackend->setTxEnabled(v);
        scheduleLegacyStateRefreshBurst();
    }

    if (v) {
        scheduleTxAudioPrecompute(25);
    }
}

void DecodiumBridge::setAutoCqRepeat(bool v)
{
    if (m_autoCqRepeat != v) {
        m_autoCqRepeat = v;
        if (m_autoCqRepeat) {
            clearManualTxHold(QStringLiteral("autocq-enabled"));
            m_autoCqCycleCount = 0;  // reset contatore cicli
        }
        if (m_autoCqRepeat && !m_autoSeq) {
            // AutoCQ must always behave like legacy auto-reply mode.
            m_autoSeq = true;
            emit autoSeqChanged();
            bridgeLog("setAutoCqRepeat: forcing autoSeq=true");
        }
        if (!m_autoCqRepeat) {
            clearCallerQueue();
            clearAutoCqPartnerLock();
            clearPendingAutoLogSnapshot();
            clearLateAutoLogSnapshot();
        }
        emit autoCqRepeatChanged();
        if (usingLegacyBackendForTx()) {
            syncLegacyBackendTxState();
            m_legacyBackend->setAutoCq(v);
        }
        if (m_autoCqRepeat) {
            scheduleTxAudioPrecompute(25);
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

void DecodiumBridge::updateRigTelemetry(double powerWatts, double swr)
{
    if (m_rigPowerWatts == powerWatts && m_rigSwr == swr)
        return;
    m_rigPowerWatts = powerWatts;
    m_rigSwr = swr;
    emit rigTelemetryChanged();
    enforceSwrTransmissionLimit(QStringLiteral("telemetry"));
}

bool DecodiumBridge::checkSwrAllowsTransmission(const QString& reason)
{
    if (!getSetting(QStringLiteral("CheckSWR"), false).toBool()) {
        return true;
    }

    constexpr double kSWRStopThreshold = 2.5;
    if (m_rigSwr <= kSWRStopThreshold) {
        return true;
    }

    bridgeLog(QStringLiteral("SWR protection blocked TX (%1): swr=%2 threshold=%3")
                  .arg(reason)
                  .arg(m_rigSwr, 0, 'f', 2)
                  .arg(kSWRStopThreshold, 0, 'f', 1));
    emit errorMessage(QStringLiteral("SWR > 2.5 !!!\n\nTrasmissione bloccata\n\nControlla antenna"));
    emit statusMessage(QStringLiteral("TX bloccato: SWR %1 > 2.5").arg(m_rigSwr, 0, 'f', 2));
    return false;
}

void DecodiumBridge::enforceSwrTransmissionLimit(const QString& reason)
{
    if (!getSetting(QStringLiteral("CheckSWR"), false).toBool()) {
        return;
    }

    constexpr double kSWRStopThreshold = 2.5;
    if (m_rigSwr <= kSWRStopThreshold || (!m_transmitting && !m_tuning)) {
        return;
    }

    bridgeLog(QStringLiteral("SWR protection stopping TX (%1): swr=%2 threshold=%3")
                  .arg(reason)
                  .arg(m_rigSwr, 0, 'f', 2)
                  .arg(kSWRStopThreshold, 0, 'f', 1));

    if (m_tuning) {
        stopTune();
    }
    if (m_transmitting) {
        stopTx();
    }

    emit errorMessage(QStringLiteral("SWR > 2.5 !!!\n\nTrasmissione interrotta\n\nControlla antenna"));
    emit statusMessage(QStringLiteral("TX interrotto: SWR %1 > 2.5").arg(m_rigSwr, 0, 'f', 2));
}

// === SETTINGS ===
int DecodiumBridge::nfa() const { return m_nfa; }
void DecodiumBridge::setNfa(int v) { if (m_nfa!=v) { m_nfa=v; emit nfaChanged(); } }
int DecodiumBridge::nfb() const { return m_nfb; }
void DecodiumBridge::setNfb(int v) { if (m_nfb!=v) { m_nfb=v; emit nfbChanged(); } }
int DecodiumBridge::ndepth() const { return m_ndepth; }
void DecodiumBridge::setNdepth(int v) { if (m_ndepth!=v) { m_ndepth=v; emit ndepthChanged(); } }
int DecodiumBridge::ncontest() const { return m_ncontest; }
void DecodiumBridge::setNcontest(int v) { setSpecialOperationActivity(v); }

void DecodiumBridge::setFoxMode(bool v)
{
    if (v) {
        setSpecialOperationActivity(kSpecialOpFox);
    } else if (m_specialOperationActivity == kSpecialOpFox) {
        setSpecialOperationActivity(kSpecialOpNone);
    }
}

void DecodiumBridge::setHoundMode(bool v)
{
    if (v) {
        setSpecialOperationActivity(kSpecialOpHound);
    } else if (m_specialOperationActivity == kSpecialOpHound) {
        setSpecialOperationActivity(kSpecialOpNone);
    }
}

bool DecodiumBridge::specialOperationRequiresLegacyTx() const
{
    return m_specialOperationActivity == kSpecialOpFox
        || m_specialOperationActivity == kSpecialOpHound;
}

bool DecodiumBridge::legacyTxBackendRequested() const
{
    return m_useLegacyTxBackend || m_forceLegacyTxForSpecialOp;
}

void DecodiumBridge::updateSpecialOperationFromLegacy(int activity)
{
    activity = qBound(kSpecialOpNone, activity, kSpecialOpMax);
    bool changed = false;
    if (m_specialOperationActivity != activity) {
        m_specialOperationActivity = activity;
        changed = true;
        emit specialOperationActivityChanged();
    }
    if (m_ncontest != activity) {
        m_ncontest = activity;
        emit ncontestChanged();
    }
    bool const fox = activity == kSpecialOpFox;
    bool const hound = activity == kSpecialOpHound;
    if (!m_useLegacyTxBackend) {
        m_forceLegacyTxForSpecialOp = fox || hound;
    }
    if (m_foxMode != fox) {
        m_foxMode = fox;
        emit foxModeChanged();
    }
    if (m_houndMode != hound) {
        m_houndMode = hound;
        emit houndModeChanged();
    }
    if (changed) {
        udpSendStatus();
    }
}

void DecodiumBridge::syncSpecialOperationToLegacyBackend()
{
    if (!legacyBackendAvailable()) {
        return;
    }

    bool const superFox = getSetting(QStringLiteral("SuperFox"), true).toBool();
    m_legacyBackend->setSuperFoxEnabled(superFox);
    m_legacyBackend->setSpecialOperationActivity(m_specialOperationActivity);
}

void DecodiumBridge::setSpecialOperationActivity(int activity)
{
    activity = qBound(kSpecialOpNone, activity, kSpecialOpMax);
    bool const wasLegacyTx = usingLegacyBackendForTx();
    bool const wasForced = m_forceLegacyTxForSpecialOp;
    bool const wantLegacyTx = activity == kSpecialOpFox || activity == kSpecialOpHound;

    updateSpecialOperationFromLegacy(activity);

    QSettings s("Decodium", "Decodium3");
    s.setValue(QStringLiteral("SelectedActivity"), activity);
    s.setValue(QStringLiteral("SpecialOpActivity"), activity != kSpecialOpNone);
    s.sync();
    syncSettingToLegacyIni(QStringLiteral("SelectedActivity"), activity);
    syncSettingToLegacyIni(QStringLiteral("SpecialOpActivity"), activity != kSpecialOpNone);
    emit settingValueChanged(QStringLiteral("SelectedActivity"), activity);
    emit settingValueChanged(QStringLiteral("SpecialOpActivity"), activity != kSpecialOpNone);

    m_forceLegacyTxForSpecialOp = wantLegacyTx;

    if (wantLegacyTx) {
        if (!ensureLegacyBackendAvailable()) {
            m_forceLegacyTxForSpecialOp = wasForced;
            updateSpecialOperationFromLegacy(kSpecialOpNone);
            QSettings resetSettings("Decodium", "Decodium3");
            resetSettings.setValue(QStringLiteral("SelectedActivity"), kSpecialOpNone);
            resetSettings.setValue(QStringLiteral("SpecialOpActivity"), false);
            resetSettings.sync();
            syncSettingToLegacyIni(QStringLiteral("SelectedActivity"), kSpecialOpNone);
            syncSettingToLegacyIni(QStringLiteral("SpecialOpActivity"), false);
            emit errorMessage(QStringLiteral("Fox/Hound non disponibile: backend legacy non avviato"));
            return;
        }
        syncLegacyBackendDialogState();
        syncSpecialOperationToLegacyBackend();
        shutdownUdpMessageClient();
        if (m_monitoring && !m_transmitting && !m_tuning) {
            migrateActiveMonitoringToLegacyBackend();
        }
        bridgeLog(QStringLiteral("SpecialOp enabled through legacy backend: activity=%1").arg(activity));
        return;
    }

    if (legacyBackendAvailable()) {
        syncSpecialOperationToLegacyBackend();
    }

    if (wasLegacyTx && wasForced && !m_useLegacyTxBackend) {
        if (m_legacyBackend) {
            m_legacyBackend->setMonitoring(false);
        }
        initUdpMessageClient();
        if (m_monitoring && !m_transmitting && !m_tuning) {
            bool const monitoringSnapshot = m_monitoring;
            m_monitoring = false;
            if (monitoringSnapshot) {
                emit monitoringChanged();
            }
            startRx();
        }
    }

    bridgeLog(QStringLiteral("SpecialOp disabled or set to non-DXpedition activity: activity=%1").arg(activity));
}

// === QML INVOKABLE ACTIONS ===

void DecodiumBridge::clearTxMessages()
{
    bridgeLog("clearTxMessages: reset QSO/TX state");
    clearDeferredManualSyncTx(QStringLiteral("clear-tx-messages"));

    if (m_transmitting || m_tuning)
        halt();

    setTxEnabled(false);
    m_qsoCooldown.clear();
    m_txRetryCount = 0;
    m_nTx73 = 0;
    m_lastNtx = -1;
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();

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
    m_monitorRequested = true;
    if (m_monitoring) { bridgeLog("startRx: already monitoring, skip"); return; }
    ++m_decodeSessionId;
    quint64 const monitorSessionId = ++m_periodTimerSessionId;

    if (legacyTxBackendRequested() && !legacyBackendAvailable() && !ensureLegacyBackendAvailable()) {
        m_monitorRequested = false;
        emit errorMessage(QStringLiteral("Backend legacy non disponibile per Fox/Hound"));
        return;
    }

    if (usingLegacyBackendForTx()) {
        bridgeLog("startRx: delegating monitoring to legacy backend");
        m_periodTimer->stop();
        m_asyncDecodeTimer->stop();
        m_asyncDecodePending = false;
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->setMonitoring(true);
        syncLegacyBackendState();
        if (useModernSpectrumFeedWithLegacy()) {
            m_legacyPcmSpectrumFeed = false;
            updatePeriodTicksMax();
            resetRxPeriodAccumulation(true);
            m_spectrumBuf.clear();
            m_wfRingPos = 0;
            m_lastPanadapterData.clear();
            m_driftFrameCount = 0;
            m_driftExpectedFrames = 0;
            m_driftClock.restart();
            startAudioCapture();
            m_spectrumTimer->start();
            bridgeLog("startRx: modern FFT panadapter feed active alongside legacy RX");
        } else {
            stopAudioCapture();
            resetRxPeriodAccumulation(false);
            m_spectrumBuf.clear();
            m_wfRingPos = 0;
            m_lastPanadapterData.clear();
            m_legacyPcmSpectrumFeed = true;
            if (m_spectrumTimer) {
                m_spectrumTimer->start();
            }
            bridgeLog("startRx: legacy PCM tap feeds fast QML panadapter");
        }
        emit statusMessage("RX avviato via backend legacy - " + m_mode);
        return;
    }

    updatePeriodTicksMax();
    m_legacyPcmSpectrumFeed = false;
    m_monitoring = true;
    emit monitoringChanged();
    resetRxPeriodAccumulation(true);
    m_spectrumBuf.clear();
    // A2 — Reset drift counters
    m_driftFrameCount    = 0;
    m_driftExpectedFrames = 0;
    m_driftClock.restart();
    resetTimeSyncDecodeMetrics();
    if (m_ntpClient && m_ntpEnabled && isTimeSyncDecodeMode(m_mode)) {
        configureNtpClientForMode(m_mode);
        m_ntpClient->syncNow();
    }
    // Sincronizza il period timer al prossimo boundary UTC (FT8=15s, FT4=7.5s, FT2=3.75s).
    // Il session id impedisce a una singleShot vecchia di riattivare il timer dopo stop/cambio modo.
    armPeriodTimerForCurrentMode(monitorSessionId, QStringLiteral("startRx"));
    m_spectrumTimer->start();
    if (m_mode == "FT2") {
        // store(release): vedi commento nel reset di setMode — il writer usa
        // CAS sul commit e un increment in volo verrà scartato correttamente.
        m_asyncAudioPos.store(0, std::memory_order_release);
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
    bool const monitorWasRequested = m_monitorRequested;
    m_monitorRequested = false;
    if (!m_monitoring) {
        if (monitorWasRequested) {
            ++m_periodTimerSessionId;
            if (legacyBackendAvailable()) {
                m_legacyBackend->setMonitoring(false);
            }
        }
        bridgeLog("stopRx: not monitoring, skip");
        return;
    }
    ++m_periodTimerSessionId;
    clearDeferredManualSyncTx(QStringLiteral("stop-rx"));

    if (usingLegacyBackendForTx()) {
        bridgeLog("stopRx: delegating monitoring stop to legacy backend");
        m_periodTimer->stop();
        m_spectrumTimer->stop();
        m_asyncDecodeTimer->stop();
        m_asyncDecodePending = false;
        m_legacyPcmSpectrumFeed = false;
        stopAudioCapture();
        m_spectrumBuf.clear();
        m_lastPanadapterData.clear();
        m_wfRingPos = 0;
        resetRxPeriodAccumulation(true);
        resetTimeSyncDecodeMetrics();
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
    resetRxPeriodAccumulation(true);
    resetTimeSyncDecodeMetrics();

    m_decoding = false;
    emit decodingChanged();
    emit statusMessage("RX fermato");
}

// === TX helpers ============================================================

// Aggiorna SoundOutput con il dispositivo di uscita correntemente selezionato
void DecodiumBridge::updateSoundOutputDevice()
{
    bool requestedDeviceFound = false;
    QAudioDevice outDev = resolveTxOutputDevice(&requestedDeviceFound);

    const QAudioFormat fmt = chooseTxAudioFormat(outDev);
    m_soundOutput->setFormat(outDev, static_cast<unsigned>(qMax(1, fmt.channelCount())), 16384);
    bridgeLog("updateSoundOutputDevice: " + outDev.description() +
              " found=" + QString::number(requestedDeviceFound) +
              " fmt=" + audioFormatToString(fmt));
}

void DecodiumBridge::resumeRxAudioAfterTx(const QString& reason)
{
    const bool wasSuspended = m_rxAudioSuspendedForTx;
    m_rxAudioSuspendedForTx = false;

    if (!m_monitoring) {
        if (wasSuspended) {
            bridgeLog(QStringLiteral("SoundInput resume skipped after TX %1: monitor off").arg(reason));
        }
        return;
    }

    if (wasSuspended) {
        {
            QMutexLocker locker(&m_audioBufferMutex);
            m_audioBuffer.clear();
            m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
        }

        if (m_soundInput) {
            m_soundInput->resume();
            bridgeLog("SoundInput resumed after TX " + reason);
        } else {
            bridgeLog("SoundInput missing after TX " + reason + ", restarting capture");
            restartAudioCaptureFromWatchdog(QStringLiteral("post-TX sound input missing (%1)").arg(reason));
            return;
        }
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = qMax(m_audioWatchdogIgnoreUntilMs, nowMs + 2500);

    QTimer::singleShot(1800, this, [this, reason]() {
        if (!m_monitoring || m_transmitting || m_tuning || m_rxAudioSuspendedForTx) {
            return;
        }

        int bufferedSamples = 0;
        {
            QMutexLocker locker(&m_audioBufferMutex);
            bufferedSamples = m_audioBuffer.size();
        }

        if (bufferedSamples <= 0) {
            bridgeLog("post-TX RX audio watchdog: no samples after " + reason + ", restarting capture");
            restartAudioCaptureFromWatchdog(QStringLiteral("post-TX RX audio did not resume (%1)").arg(reason));
        }
    });
}

void DecodiumBridge::finishModulatorIdlePlayback(const QString& reason)
{
    bridgeLog("finishModulatorIdlePlayback: reason=" + reason +
              " transmitting=" + QString::number(m_transmitting) +
              " tuning=" + QString::number(m_tuning));

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_transmitting && m_txPlaybackHoldUntilMs > nowMs) {
        qint64 const delayMs = qMax<qint64>(1, m_txPlaybackHoldUntilMs - nowMs);
        if (!m_txPlaybackReleasePending) {
            m_txPlaybackReleasePending = true;
            bridgeLog(QStringLiteral("finishModulatorIdlePlayback delayed: reason=%1 hold_ms=%2")
                          .arg(reason).arg(delayMs));
            QTimer::singleShot(delayMs, this, [this, reason]() {
                m_txPlaybackReleasePending = false;
                finishModulatorIdlePlayback(reason + QStringLiteral("-held"));
            });
        }
        return;
    }

    m_txPlaybackReleasePending = false;
    m_txPlaybackHoldUntilMs = 0;

    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend))
        activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);

    // Ferma SoundOutput solo dopo il tempo minimo di playback, altrimenti il
    // buffer hardware puo' essere tagliato mentre il messaggio e' ancora in aria.
    if (m_soundOutput) m_soundOutput->stop();

    bool const wasTransmitting = m_transmitting;
    noteTxPlaybackFinished(reason, false);

    if (m_tuning) {
        m_tuning = false;
        emit tuningChanged();
        emit statusMessage("Tune terminato");
    } else if (wasTransmitting) {
        m_transmitting = false;
        emit transmittingChanged();
        emit statusMessage("TX terminato");
    }

    resumeRxAudioAfterTx(reason);
    resumeNonAudioTxWork(reason);
}

// ===========================================================================

// Restituisce QAudioDevice corrispondente a m_audioOutputDevice (o il default)
static QAudioDevice findOutputDevice(const QString& name, bool* requestedDeviceFound)
{
    auto const outputs = QMediaDevices::audioOutputs();

    if (!name.isEmpty()) {
        for (const QAudioDevice& d : outputs) {
            if (audioDeviceNameMatches(d.description(), name)) {
                if (requestedDeviceFound) *requestedDeviceFound = true;
                return d;
            }
        }
    }

    if (requestedDeviceFound) *requestedDeviceFound = name.isEmpty();
    return QMediaDevices::defaultAudioOutput();
}

void DecodiumBridge::invalidateTxAudioCache()
{
    m_txAudioCache = TxAudioCache {};
}

QAudioDevice DecodiumBridge::resolveTxOutputDevice(bool* requestedDeviceFound)
{
    if (m_cachedTxOutputDeviceValid && m_cachedTxOutputDeviceName == m_audioOutputDevice) {
        if (requestedDeviceFound) {
            *requestedDeviceFound = m_cachedTxOutputDeviceFound;
        }
        return m_cachedTxOutputDevice;
    }

    bool found = false;
    QAudioDevice device = findOutputDevice(m_audioOutputDevice, &found);
    m_cachedTxOutputDeviceName = m_audioOutputDevice;
    m_cachedTxOutputDeviceFound = found;
    m_cachedTxOutputDevice = device;
    m_cachedTxOutputDeviceValid = true;
    if (requestedDeviceFound) {
        *requestedDeviceFound = found;
    }
    return device;
}

void DecodiumBridge::scheduleTxAudioPrecompute(int delayMs)
{
    if (m_txAudioPrecomputeScheduled || m_shuttingDown || QCoreApplication::closingDown()) {
        return;
    }

    m_txAudioPrecomputeScheduled = true;
    QTimer::singleShot(qMax(0, delayMs), this, [this]() {
        m_txAudioPrecomputeScheduled = false;
        precomputeTxAudioForCurrentMessage(QStringLiteral("scheduled"));
    });
}

void DecodiumBridge::precomputeTxAudioForCurrentMessage(const QString& reason)
{
    if (m_shuttingDown || QCoreApplication::closingDown() || m_transmitting || m_tuning) {
        return;
    }
    if (m_decoding) {
        // Do not compete with the current decode pass; the startTx() path still
        // has a synchronous fallback if an immediate FT2 reply is required.
        scheduleTxAudioPrecompute(250);
        return;
    }
    if (!m_decodeStartMsBySerial.isEmpty() || m_asyncDecodePending) {
        scheduleTxAudioPrecompute(250);
        return;
    }
    int const slotDelayMs = txAudioPrecomputeRetryDelayMs(m_mode, m_monitoring);
    if (slotDelayMs > 0) {
        scheduleTxAudioPrecompute(slotDelayMs);
        return;
    }
    if (!m_txEnabled && !m_autoCqRepeat && !m_deferredManualSyncTx) {
        return;
    }

    QString const msg = buildCurrentTxMessage();
    if (msg.trimmed().isEmpty()) {
        return;
    }

    bool needPcm = !usingTciAudioInput();
#if defined(Q_OS_MAC)
    needPcm = false;
#endif

    QVector<float> wave;
    QByteArray pcm;
    QAudioFormat format;
    QAudioDevice device;
    QString error;
    if (ensureTxAudioPrepared(msg, effectiveTxAudioFrequencyHz(), needPcm,
                              &wave, &pcm, &format, &device, &error)) {
        bridgeLog(QStringLiteral("TX audio precomputed (%1): mode=%2 msg=[%3] samples=%4 pcm=%5")
                      .arg(reason, m_mode, msg.trimmed())
                      .arg(wave.size())
                      .arg(pcm.size()));
    } else if (!error.isEmpty()) {
        bridgeLog(QStringLiteral("TX audio precompute skipped (%1): %2").arg(reason, error));
    }
}

bool DecodiumBridge::ensureTxAudioPrepared(const QString& msg, int txAudioFrequency, bool needPcm,
                                           QVector<float>* waveOut, QByteArray* pcmOut,
                                           QAudioFormat* formatOut, QAudioDevice* deviceOut,
                                           QString* errorOut)
{
    QString const mode = m_mode.trimmed().toUpper();
    QString const message = msg.trimmed();
    bool const tciAudio = usingTciAudioInput();

    auto cacheMatchesBase = [&]() {
        return !m_txAudioCache.wave.isEmpty()
            && m_txAudioCache.mode == mode
            && m_txAudioCache.message == message
            && m_txAudioCache.txAudioFrequency == txAudioFrequency
            && m_txAudioCache.tciAudio == tciAudio;
    };

    if (cacheMatchesBase() && (!needPcm || !m_txAudioCache.pcm.isEmpty())) {
        if (needPcm) {
            if (m_cachedTxOutputDeviceValid
                && m_cachedTxOutputDeviceName == m_audioOutputDevice
                && m_txAudioCache.outputDeviceName == m_audioOutputDevice
                && m_txAudioCache.outputDeviceDescription == m_cachedTxOutputDevice.description()
                && m_txAudioCache.outputFormat.isValid()) {
                if (waveOut) *waveOut = m_txAudioCache.wave;
                if (pcmOut) *pcmOut = m_txAudioCache.pcm;
                if (formatOut) *formatOut = m_txAudioCache.outputFormat;
                if (deviceOut) *deviceOut = m_cachedTxOutputDevice;
                return true;
            }
            bool found = false;
            QAudioDevice const device = resolveTxOutputDevice(&found);
            QAudioFormat const format = chooseTxAudioFormat(device);
            if (m_txAudioCache.outputDeviceName == m_audioOutputDevice
                && m_txAudioCache.outputDeviceDescription == device.description()
                && sameAudioFormat(m_txAudioCache.outputFormat, format)) {
                if (waveOut) *waveOut = m_txAudioCache.wave;
                if (pcmOut) *pcmOut = m_txAudioCache.pcm;
                if (formatOut) *formatOut = m_txAudioCache.outputFormat;
                if (deviceOut) *deviceOut = device;
                Q_UNUSED(found)
                return true;
            }
        } else {
            if (waveOut) *waveOut = m_txAudioCache.wave;
            if (pcmOut) pcmOut->clear();
            if (formatOut) *formatOut = QAudioFormat {};
            if (deviceOut) *deviceOut = QAudioDevice {};
            return true;
        }
    }

    QString buildError;
    QVector<float> wave = buildTxWaveformForMessage(mode, message, txAudioFrequency, &buildError);
    if (wave.isEmpty()) {
        if (errorOut) {
            *errorOut = buildError.isEmpty() ? QStringLiteral("Generazione onda TX fallita") : buildError;
        }
        invalidateTxAudioCache();
        return false;
    }

    QByteArray pcm;
    QAudioFormat format;
    QAudioDevice device;
    if (needPcm) {
        bool found = false;
        device = resolveTxOutputDevice(&found);
        if (!found && !m_audioOutputDevice.trimmed().isEmpty()) {
            bridgeLog("TX audio cache: requested output device not found, fallback to default: " +
                      device.description() + " requested=[" + m_audioOutputDevice + "]");
        }
        format = chooseTxAudioFormat(device);
        pcm = buildTxPcmBuffer(wave, format);
        if (pcm.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Formato audio TX non supportato dal device selezionato");
            }
            invalidateTxAudioCache();
            return false;
        }
    }

    m_txAudioCache.mode = mode;
    m_txAudioCache.message = message;
    m_txAudioCache.txAudioFrequency = txAudioFrequency;
    m_txAudioCache.tciAudio = tciAudio;
    m_txAudioCache.outputDeviceName = needPcm ? m_audioOutputDevice : QString {};
    m_txAudioCache.outputDeviceDescription = needPcm ? device.description() : QString {};
    m_txAudioCache.outputFormat = needPcm ? format : QAudioFormat {};
    m_txAudioCache.wave = wave;
    m_txAudioCache.pcm = pcm;

    if (waveOut) *waveOut = m_txAudioCache.wave;
    if (pcmOut) *pcmOut = m_txAudioCache.pcm;
    if (formatOut) *formatOut = m_txAudioCache.outputFormat;
    if (deviceOut) *deviceOut = device;
    return true;
}

void DecodiumBridge::saveTxRecordingAsync(const QString& path, QVector<float> wave, int sampleRate,
                                          const QString& logLabel)
{
    if (path.trimmed().isEmpty() || wave.isEmpty() || sampleRate <= 0) {
        return;
    }

    QThread* writer = QThread::create([path, wave, sampleRate, logLabel]() {
        bool const ok = writeMono16WavFile(path, wave, sampleRate);
        bridgeLog(QStringLiteral("%1 recording %2: %3")
                      .arg(logLabel,
                           ok ? QStringLiteral("saved") : QStringLiteral("failed"),
                           path));
    });
    writer->setObjectName(QStringLiteral("DecodiumTxWavWriter"));
    connect(writer, &QThread::finished, writer, &QObject::deleteLater);
    writer->start(QThread::LowPriority);
}

void DecodiumBridge::suspendNonAudioTxWork(const QString& reason)
{
    if (m_spectrumTimer && m_spectrumTimer->isActive()) {
        m_spectrumTimer->stop();
        m_spectrumTimerPausedForTx = true;
        bridgeLog(QStringLiteral("TX workload: spectrum timer paused (%1)").arg(reason));
    }
}

void DecodiumBridge::resumeNonAudioTxWork(const QString& reason)
{
    if (!m_spectrumTimerPausedForTx) {
        return;
    }
    m_spectrumTimerPausedForTx = false;
    if (m_spectrumTimer && m_monitoring && !m_transmitting && !m_tuning) {
        m_spectrumTimer->start();
        bridgeLog(QStringLiteral("TX workload: spectrum timer resumed (%1)").arg(reason));
    }
}

void DecodiumBridge::noteTxPlaybackFinished(const QString& reason, bool error)
{
    int const finishedTx = m_activeTxNumber;
    QString const finishedMessage = m_activeTxMessage;
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();

    if (finishedTx <= 0) {
        return;
    }

    if (error) {
        bridgeLog(QStringLiteral("tx-playback-finished: TX%1 error, not counting signoff (%2)")
                      .arg(finishedTx)
                      .arg(reason));
        return;
    }

    bool const signoffPayload =
        (finishedTx == 5) || messageCarries73Payload(finishedMessage);
    if (!signoffPayload) {
        return;
    }

    ++m_nTx73;
    bridgeLog(QStringLiteral("tx-playback-finished: completed signoff TX%1 count=%2 reason=%3 msg=[%4]")
                  .arg(finishedTx)
                  .arg(m_nTx73)
                  .arg(reason, finishedMessage));

    QTimer::singleShot(0, this, [this]() {
        if (!m_transmitting && !m_tuning) {
            checkAndStartPeriodicTx();
        }
    });
}

void DecodiumBridge::completeTxPlayback(const QString& reason, bool error)
{
    bridgeLog("completeTxPlayback: reason=" + reason +
              " transmitting=" + QString::number(m_transmitting) +
              " rxSuspended=" + QString::number(m_rxAudioSuspendedForTx));

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!error && m_transmitting && m_txPlaybackHoldUntilMs > nowMs) {
        qint64 const delayMs = qMax<qint64>(1, m_txPlaybackHoldUntilMs - nowMs);
        if (!m_txPlaybackReleasePending) {
            m_txPlaybackReleasePending = true;
            bridgeLog(QStringLiteral("completeTxPlayback delayed: reason=%1 hold_ms=%2")
                          .arg(reason).arg(delayMs));
            QTimer::singleShot(delayMs, this, [this, reason]() {
                m_txPlaybackReleasePending = false;
                completeTxPlayback(reason + QStringLiteral("-held"));
            });
        }
        return;
    }

    m_txPlaybackReleasePending = false;
    m_txPlaybackHoldUntilMs = 0;

    if (m_txAudioSink) {
        m_txAudioSink->disconnect(this);
        if (m_txAudioSink->state() != QAudio::StoppedState) {
            m_txAudioSink->stop();
        }
    }

    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
        activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
    }
    if (usingTciAudioInput()) {
        stopTciTxAudioStream(true);
    }

    bool const wasTransmitting = m_transmitting;
    noteTxPlaybackFinished(reason, error);
    if (m_transmitting) {
        m_transmitting = false;
        emit transmittingChanged();
    }

    resumeRxAudioAfterTx(reason);
    resumeNonAudioTxWork(reason);

    if (wasTransmitting) {
        if (error) {
            emit errorMessage("Errore audio TX sul device selezionato");
        } else {
            emit statusMessage("TX completato");
        }
    }

    if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled) {
        m_asyncLastTxEndMs = QDateTime::currentMSecsSinceEpoch();
        QTimer::singleShot(0, this, [this]() {
            if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled &&
                m_txEnabled && !m_transmitting && !m_tuning) {
                checkAndStartPeriodicTx();
            }
        });
    }

    if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled && m_qsoProgress == 1 && !m_holdTxFreq) {
        int const hop = static_cast<int>(QRandomGenerator::global()->bounded(51)) - 25;
        int const newFreq = qBound(200, m_txFrequency + hop, 3000);
        if (newFreq != m_txFrequency) {
            setTxFrequency(newFreq);
            bridgeLog("FT2 async freq hop: " + QString::number(newFreq) + " Hz");
        }
    }
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
    if (!checkSwrAllowsTransmission(QStringLiteral("startTx"))) return;
    if (legacyTxBackendRequested() && !legacyBackendAvailable() && !ensureLegacyBackendAvailable()) {
        emit errorMessage(QStringLiteral("Backend legacy non disponibile per Fox/Hound"));
        return;
    }

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
    m_lastTransmittedMessage = msg.trimmed();
    m_lastTxActivityUtc = QDateTime::currentDateTimeUtc();

    if (usingLegacyBackendForTx()) {
        bridgeLog("startTx: delegating to legacy backend");
        if (m_recordTxEnabled) {
            QString recordingError;
            QVector<float> const wave = buildTxWaveformForMessage(m_mode, msg, effectiveTxAudioFrequencyHz(), &recordingError);
            if (!wave.isEmpty()) {
                QString const txRecordPath = buildTxRecordingPath(m_mode, false);
                saveTxRecordingAsync(txRecordPath, wave, 48000, QStringLiteral("TX legacy"));
            } else {
                bridgeLog("TX recording skipped (legacy): " + recordingError + " msg=[" + msg + "]");
            }
        }
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->armCurrentTx();
        scheduleLegacyStateRefreshBurst();
        emit statusMessage("TX armato via backend legacy: " + msg.trimmed());
        return;
    }

    // Usa la cache preparata quando TX è stato armato. Se non è pronta
    // (es. risposta FT2 immediata), cade al build sincrono minimo qui.
    QVector<float> wave;
    QByteArray preparedPcm;
    QAudioFormat preparedFmt;
    QAudioDevice preparedDev;
    bool const tciAudioTx = usingTciAudioInput();
    const int txAudioFrequency = effectiveTxAudioFrequencyHz();
    const int xitHz = catSplitXitHzForTxFrequency(m_txFrequency);
    if (xitHz != 0 || catSplitTxDialFrequencyHz() > 0.0) {
        bridgeLog(QStringLiteral("startTx split audio: ui_tx=%1 audio_tx=%2 xit=%3 tx_dial=%4")
                      .arg(QString::number(m_txFrequency),
                           QString::number(txAudioFrequency),
                           QString::number(xitHz),
                           QString::number(catSplitTxDialFrequencyHz(), 'f', 0)));
    }
    bool needPcm = !tciAudioTx;
#if defined(Q_OS_MAC)
    needPcm = false;
#endif
    QString prepareError;
    if (!ensureTxAudioPrepared(msg, txAudioFrequency, needPcm,
                               &wave, &preparedPcm, &preparedFmt, &preparedDev,
                               &prepareError)) {
        QString const errorText = prepareError.isEmpty()
            ? QStringLiteral("Generazione audio TX fallita")
            : prepareError;
        emit errorMessage(errorText);
        bridgeLog("startTx: TX audio prepare failed: " + errorText + " msg=[" + msg + "]");
        return;
    }
    bridgeLog("startTx: wave.size()=" + QString::number(wave.size()) +
              " cache=" + QString::number((m_txAudioCache.message == msg.trimmed()) ? 1 : 0));

#if defined(Q_OS_MAC)
    {
        bool requestedDeviceFound = false;
        QAudioDevice outDev = resolveTxOutputDevice(&requestedDeviceFound);

        if (!requestedDeviceFound) {
            bridgeLog("startTx(mac): requested output device not found, fallback to default: " +
                      outDev.description() + " requested=[" + m_audioOutputDevice + "]");
            emit statusMessage("Audio TX non trovato, uso default: " + outDev.description());
        }
        const QAudioFormat outFmt = chooseTxAudioFormat(outDev);
        const AudioDevice::Channel outChannel = txOutputChannelForFormat(outFmt, m_audioOutputChannel);
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

        bool const txCarries73Payload =
            (m_currentTx == 5) || messageCarries73Payload(msg);
        bool const ft2DeferredSignoff =
            txCarries73Payload
            && (m_qsoProgress > 1)
            && (m_mode == QStringLiteral("FT2"))
            && (!m_quickQsoEnabled || !m_quickPeerSignaled)
            && !m_logAfterOwn73;
        bool const autoCqDeferredSignoff =
            m_autoCqRepeat
            && txCarries73Payload
            && (m_qsoProgress > 1)
            && (m_mode == QStringLiteral("FT4") || m_mode == QStringLiteral("FT8"))
            && !m_logAfterOwn73;
        if (ft2DeferredSignoff || autoCqDeferredSignoff) {
            m_ft2DeferredLogPending = true;
            capturePendingAutoLogSnapshot();
        }

        m_activeTxNumber = m_currentTx;
        m_activeTxMessage = msg.trimmed();
        syncActiveCatTxSplitFrequency(QStringLiteral("startTx"));
        if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend))
            activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, true, m_omniRigCat, m_legacyBackend);

        m_transmitting = true;
        emit transmittingChanged();
        suspendNonAudioTxWork(QStringLiteral("tx-mac"));

        // Anti-collisione: ferma l'ingresso audio durante TX per evitare
        // che il decoder processi il nostro stesso tono come segnale ricevuto.
        // Eccezione: FT2 async è full-duplex, RX deve continuare durante TX.
        if (m_soundInput && !(m_mode == "FT2" && m_asyncTxEnabled)) {
            m_soundInput->suspend();
            m_rxAudioSuspendedForTx = true;
            bridgeLog("SoundInput suspended during TX (sync mode)");
        }

        // Aggiungi TX entry alla decode list. La deduplica deve valere solo per
        // lo stesso slot: in AutoCQ lo stesso testo viene inviato ciclicamente.
        {
            QVariantMap txEntry;
            txEntry["time"]     = currentTxVisualTimeToken(m_mode);
            txEntry["db"]       = "TX";
            txEntry["dt"]       = "0.0";
            txEntry["freq"]     = QString::number(m_txFrequency);
            txEntry["message"]  = msg;
            txEntry["aptype"]   = "";
            txEntry["quality"]  = "";
            txEntry["isTx"]     = true;
            txEntry["mode"]     = m_mode;
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
            if (!txDecodeEntryAlreadyPresent(m_decodeList, txEntry)) {
                m_decodeList.append(txEntry);
                normalizeDecodeEntriesForDisplay(m_decodeList, 1500, m_mode);
                appendRxDecodeEntry(txEntry);
                emit decodeListChanged();
            }
        }

        if (m_recordTxEnabled) {
            QString const txRecordPath = buildTxRecordingPath(m_mode, false);
            saveTxRecordingAsync(txRecordPath, wave, 48000, QStringLiteral("TX"));
        }

        unsigned symbolsLength = 79;
        double framesPerSymbol = 1920.0;
        double toneSpacing = -3.0;
        if (m_mode == "FT2") {
            symbolsLength = 103;
            framesPerSymbol = 288.0;
            toneSpacing = -2.0;
        } else if (m_mode == "FT4") {
            symbolsLength = 105;
            framesPerSymbol = 576.0;
            toneSpacing = -2.0;
        }

        m_modulator->setPrecomputedWave(m_mode, wave);
        qint64 const txPlaybackMs = qMax<qint64>(
            1, qRound64(static_cast<double>(wave.size()) * 1000.0 / 48000.0));
        m_txPlaybackHoldUntilMs = QDateTime::currentMSecsSinceEpoch() + txPlaybackMs + 300;
        m_txPlaybackReleasePending = false;
        bridgeLog(QStringLiteral("startTx(mac): playback hold %1 ms for %2 samples")
                      .arg(txPlaybackMs + 300)
                      .arg(wave.size()));
        m_modulator->start(m_mode, symbolsLength, framesPerSymbol,
                           static_cast<double>(txAudioFrequency), toneSpacing,
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

    QAudioDevice outDev;
    QAudioFormat outFmt;
    if (!tciAudioTx) {
        outDev = preparedDev;
        outFmt = preparedFmt;
        m_txPcmData = preparedPcm;
        if (!m_cachedTxOutputDeviceFound && !m_audioOutputDevice.trimmed().isEmpty()) {
            bridgeLog("startTx: requested output device not found, fallback to default: " +
                      outDev.description() + " requested=[" + m_audioOutputDevice + "]");
            emit statusMessage("Audio TX non trovato, uso default: " + outDev.description());
        }
        if (!outFmt.isValid() || m_txPcmData.isEmpty()) {
            bridgeLog("startTx: prepared PCM buffer invalid for " + audioFormatToString(outFmt));
            emit errorMessage("Formato audio TX non supportato dal device selezionato");
            return;
        }
    } else {
        m_txPcmData.clear();
        bridgeLog(QStringLiteral("startTx: TCI audio path selected, skipping local TX audio device"));
    }
    if (m_recordTxEnabled) {
        QString const txRecordPath = buildTxRecordingPath(m_mode, false);
        saveTxRecordingAsync(txRecordPath, wave, 48000, QStringLiteral("TX"));
    }

    // === GitHub TxController: aggiorna contatori retry ===
    // Traccia quante volte abbiamo inviato lo stesso TX step senza risposta.
    // NOTE: spostato dopo il check su buildTxPcmBuffer/PCM vuoto (sopra) e
    // prima del PTT SU (sotto). Se la generazione del PCM fallisce, NON
    // vogliamo gonfiare m_txRetryCount e anticipare il retry-limit.
    if (m_currentTx == m_lastNtx) {
        ++m_txRetryCount;
    } else {
        m_txRetryCount = 1;
        m_lastNtx = m_currentTx;
    }

    bool const txCarries73Payload =
        (m_currentTx == 5) || messageCarries73Payload(msg);
    bool const ft2DeferredSignoff =
        txCarries73Payload
        && (m_qsoProgress > 1)
        && (m_mode == QStringLiteral("FT2"))
        && (!m_quickQsoEnabled || !m_quickPeerSignaled)
        && !m_logAfterOwn73;
    bool const autoCqDeferredSignoff =
        m_autoCqRepeat
        && txCarries73Payload
        && (m_qsoProgress > 1)
        && (m_mode == QStringLiteral("FT4") || m_mode == QStringLiteral("FT8"))
        && !m_logAfterOwn73;
    if (ft2DeferredSignoff || autoCqDeferredSignoff) {
        m_ft2DeferredLogPending = true;
        capturePendingAutoLogSnapshot();
    }

    // PTT SU — prima di avviare l'audio (come GitHub pttAssert()).
    // Con TCI-audio il modulatore deve essere armato prima del PTT, altrimenti
    // ExpertSDR riceve "trx:true" senza suffisso/payload TCI.
    m_activeTxNumber = m_currentTx;
    m_activeTxMessage = msg.trimmed();
    syncActiveCatTxSplitFrequency(QStringLiteral("startTx"));
    if (!tciAudioTx && activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
        activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, true, m_omniRigCat, m_legacyBackend);
        bridgeLog("PTT ON via " + m_catBackend);
    } else if (tciAudioTx) {
        bridgeLog(QStringLiteral("PTT ON deferred until TCI TX audio stream is armed"));
    } else {
        bridgeLog("WARNING: PTT not available — backend=" + m_catBackend + " not connected. TX audio will play but radio stays in RX.");
        emit statusMessage("PTT non disponibile: verifica connessione CAT (" + m_catBackend + ")");
    }

    m_transmitting = true;
    emit transmittingChanged();
    suspendNonAudioTxWork(QStringLiteral("tx"));

    // Anti-collisione: durante TX sync non dobbiamo ricatturare il nostro
    // stesso audio sul device RX. FT2 async resta full-duplex.
    if (m_monitoring && m_soundInput && !(m_mode == "FT2" && m_asyncTxEnabled)) {
        m_soundInput->suspend();
        m_rxAudioSuspendedForTx = true;
        bridgeLog("SoundInput suspended during TX (sync mode)");
    }

    // Aggiungi entry TX. Non scartare AutoCQ futuri con lo stesso testo:
    // duplica solo se appartengono allo stesso slot temporale.
    {
        QVariantMap txEntry;
        txEntry["time"]     = currentTxVisualTimeToken(m_mode);
        txEntry["db"]       = "TX";
        txEntry["dt"]       = "0.0";
        txEntry["freq"]     = QString::number(m_txFrequency);
        txEntry["message"]  = msg;
        txEntry["aptype"]   = "";
        txEntry["quality"]  = "";
        txEntry["isTx"]     = true;
        txEntry["mode"]     = m_mode;
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
        if (!txDecodeEntryAlreadyPresent(m_decodeList, txEntry)) {
            m_decodeList.append(txEntry);
            normalizeDecodeEntriesForDisplay(m_decodeList, 1500, m_mode);
            appendRxDecodeEntry(txEntry);
            emit decodeListChanged();
        }
    }

    if (tciAudioTx) {
        unsigned symbolsLength = 79;
        double framesPerSymbol = 1920.0;
        double toneSpacing = -3.0;
        if (m_mode == QStringLiteral("FT2")) {
            symbolsLength = 103;
            framesPerSymbol = 288.0;
            toneSpacing = -2.0;
        } else if (m_mode == QStringLiteral("FT4")) {
            symbolsLength = 105;
            framesPerSymbol = 576.0;
            toneSpacing = -2.0;
        }

        double const periodSeconds = qMax(0.1, periodMsForMode(m_mode) / 1000.0);
        if (!startTciTxAudioStream(wave,
                                   m_mode,
                                   symbolsLength,
                                   framesPerSymbol,
                                   static_cast<double>(txAudioFrequency),
                                   toneSpacing,
                                   true,
                                   periodSeconds)) {
            if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend))
                activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
            m_transmitting = false;
            emit transmittingChanged();
            m_txPcmData.clear();
            m_activeTxNumber = 0;
            m_activeTxMessage.clear();
            resumeRxAudioAfterTx(QStringLiteral("tci-tx-audio-start-failed"));
            resumeNonAudioTxWork(QStringLiteral("tci-tx-audio-start-failed"));
            emit errorMessage(QStringLiteral("Impossibile avviare audio TX TCI"));
            return;
        }

        if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
            activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, true, m_omniRigCat, m_legacyBackend);
            bridgeLog(QStringLiteral("PTT ON via tci after TX audio stream arm"));
        }

        qint64 const txPlaybackMs = qMax<qint64>(
            1, qRound64(static_cast<double>(wave.size()) * 1000.0 / 48000.0));
        m_txPlaybackHoldUntilMs = QDateTime::currentMSecsSinceEpoch() + txPlaybackMs + 300;
        m_txPlaybackReleasePending = false;
        QTimer::singleShot(static_cast<int>(qBound<qint64>(qint64(1000), txPlaybackMs + 1200, qint64(25000))),
                           this,
                           [this]() {
                               if (m_transmitting && usingTciAudioInput())
                                   completeTxPlayback(QStringLiteral("tci-watchdog"));
                           });
        bridgeLog(QStringLiteral("startTx TCI audio: mode=%1 msg=%2 samples=%3 hold=%4ms")
                      .arg(m_mode, msg.trimmed())
                      .arg(wave.size())
                      .arg(txPlaybackMs + 300));
        emit statusMessage("TX TCI: " + msg.trimmed());
        return;
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
        qsizetype const txBufferBytes = txAudioBufferBytesForMode(outFmt, m_mode, false);
        if (txBufferBytes > 0) {
            m_txAudioSink->setBufferSize(txBufferBytes);
        }
        qint64 const expectedUs = qMax<qint64>(
            1, qRound64(static_cast<double>(wave.size()) * 1000000.0 / 48000.0));
        qint64 const txPlaybackMs = (expectedUs + 999) / 1000;
        m_txPlaybackHoldUntilMs = QDateTime::currentMSecsSinceEpoch() + txPlaybackMs + 300;
        m_txPlaybackReleasePending = false;
        bridgeLog(QStringLiteral("startTx: playback hold %1 ms for %2 samples")
                      .arg(txPlaybackMs + 300)
                      .arg(wave.size()));

        connect(m_txAudioSink, &QAudioSink::stateChanged, this, [this](QAudio::State st) {
            QString extra;
            if (m_txAudioSink) extra = " err=" + QString::number((int)m_txAudioSink->error())
                + " processed=" + QString::number(m_txAudioSink->processedUSecs()) + "us";
            if (m_txPcmBuffer) extra += " bufPos=" + QString::number(m_txPcmBuffer->pos())
                + "/" + QString::number(m_txPcmBuffer->size());
            bridgeLog("TX QAudioSink stateChanged: " + QString::number(static_cast<int>(st)) + extra);
            if (st == QAudio::IdleState) {
                // GitHub ptt0Timer: 200ms delay prima di abbassare PTT.
                QTimer::singleShot(200, this, [this]() {
                    if (m_transmitting) {
                        completeTxPlayback(QStringLiteral("idle"));
                    }
                });
            } else if (st == QAudio::StoppedState && m_txAudioSink &&
                       m_txAudioSink->error() != QAudio::NoError && m_transmitting) {
                completeTxPlayback(QStringLiteral("audio-error"), true);
            }
        });

        m_txAudioSink->start(m_txPcmBuffer);
        QPointer<QAudioSink> const sinkGuard(m_txAudioSink);
        QPointer<QBuffer> const bufferGuard(m_txPcmBuffer);
        int const watchdogMs = qBound(1500,
                                      static_cast<int>((expectedUs + 999) / 1000) + 1500,
                                      25000);
        QTimer::singleShot(watchdogMs, this, [this, sinkGuard, bufferGuard, expectedUs]() {
            if (!m_transmitting || m_tuning || !sinkGuard || sinkGuard != m_txAudioSink) {
                return;
            }

            qint64 const processedUs = sinkGuard->processedUSecs();
            qint64 const bufferPos = bufferGuard ? bufferGuard->pos() : -1;
            qint64 const bufferSize = bufferGuard ? bufferGuard->size() : -1;
            bool const consumed =
                !bufferGuard ||
                bufferGuard->atEnd() ||
                processedUs >= qMax<qint64>(0, expectedUs - 250000);

            if (!consumed) {
                bridgeLog("TX QAudioSink watchdog: still active, buffer not consumed processed=" +
                          QString::number(processedUs) + "us expected=" +
                          QString::number(expectedUs) + "us bufPos=" +
                          QString::number(bufferPos) + "/" + QString::number(bufferSize));
                return;
            }

            bridgeLog("TX QAudioSink watchdog forcing completion: processed=" +
                      QString::number(processedUs) + "us expected=" +
                      QString::number(expectedUs) + "us bufPos=" +
                      QString::number(bufferPos) + "/" + QString::number(bufferSize));
            completeTxPlayback(QStringLiteral("watchdog"));
        });

        bridgeLog("startTx launchAudio: mode=" + m_mode + " msg=" + msg.trimmed() +
                  " samples=" + QString::number(wave.size()) +
                  " pcm_bytes=" + QString::number(m_txPcmData.size()) +
                  " dev=" + outDev.description() +
                  " fmt=" + audioFormatToString(outFmt) +
                  " buf=" + QString::number(m_txAudioSink->bufferSize()) +
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
    if (legacyTxBackendRequested() && !legacyBackendAvailable() && !ensureLegacyBackendAvailable()) {
        emit errorMessage(QStringLiteral("Backend legacy non disponibile per Fox/Hound"));
        return;
    }
    if (usingLegacyBackendForTx()) {
        bridgeLog(QStringLiteral("sendTx: delegating TX%1 to legacy backend").arg(n));
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->armCurrentTx();
        scheduleLegacyStateRefreshBurst();
        return;
    }

    if (shouldDeferManualSyncTxStart()) {
        m_deferredManualSyncTx = true;
        bridgeLog(QStringLiteral("manualSyncTx: queued TX%1 for next valid slot (mode=%2 txPeriod=%3 transmitting=%4)")
                      .arg(n)
                      .arg(m_mode)
                      .arg(m_txPeriod)
                      .arg(m_transmitting ? 1 : 0));
        emit statusMessage(QStringLiteral("TX%1 armato per il prossimo slot").arg(n));
        return;
    }

    clearDeferredManualSyncTx(QStringLiteral("manual-immediate"));
    startTx();
}

void DecodiumBridge::stopTx()
{
    if (usingLegacyBackendForTx()) {
        bridgeLog("stopTx: delegating to legacy backend");
        m_legacyBackend->stopTransmission();
        scheduleLegacyStateRefreshBurst();
        return;
    }

    m_txPlaybackHoldUntilMs = 0;
    m_txPlaybackReleasePending = false;
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();

#if defined(Q_OS_MAC)
    if (m_modulator && m_modulator->isActive())
        m_modulator->stop(true);
    if (m_soundOutput)
        m_soundOutput->stop();
    m_txPcmData.clear();
    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend))
        activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
    if (m_transmitting) {
        m_transmitting = false;
        emit transmittingChanged();
        emit statusMessage("TX fermato");
    }
    resumeRxAudioAfterTx(QStringLiteral("stopTx"));
    resumeNonAudioTxWork(QStringLiteral("stopTx"));
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
    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
    if (usingTciAudioInput()) stopTciTxAudioStream(true);
    if (m_transmitting) {
        m_transmitting = false;
        emit transmittingChanged();
        emit statusMessage("TX fermato");
    }
    resumeRxAudioAfterTx(QStringLiteral("stopTx"));
    resumeNonAudioTxWork(QStringLiteral("stopTx"));
}

void DecodiumBridge::startTune()
{
    clearDeferredManualSyncTx(QStringLiteral("start-tune"));

    if (m_tuning || m_transmitting) return;
    if (!checkSwrAllowsTransmission(QStringLiteral("startTune"))) return;
    if (legacyTxBackendRequested() && !legacyBackendAvailable() && !ensureLegacyBackendAvailable()) {
        emit errorMessage(QStringLiteral("Backend legacy non disponibile per Fox/Hound"));
        return;
    }
    m_lastTxActivityUtc = QDateTime::currentDateTimeUtc();

    if (m_currentTx >= 1 && m_currentTx <= 5 &&
        m_dxCall.trimmed().isEmpty() &&
        buildCurrentTxMessage().trimmed() == m_tx6.trimmed() &&
        !m_tx6.trimmed().isEmpty()) {
        bridgeLog("startTune: no DX payload available, selecting TX6/CQ");
        setCurrentTx(6);
    }

    if (usingLegacyBackendForTx()) {
        bridgeLog("startTune: delegating to legacy backend");
        if (m_recordTxEnabled) {
            double const freq = m_txFrequency > 0 ? m_txFrequency : 1500.0;
            QVector<float> const wave = buildTuneWaveform(freq, 48000, 10);
            QString const txRecordPath = buildTxRecordingPath(QStringLiteral("TUNE"), true);
            saveTxRecordingAsync(txRecordPath, wave, 48000, QStringLiteral("TUNE legacy"));
        }
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->startTune(true);
        if (!m_tuning) {
            m_tuning = true;
            emit tuningChanged();
        }
        scheduleLegacyStateRefreshBurst();
        emit statusMessage("TUNE via backend legacy");
        return;
    }

    if (usingTciAudioInput()) {
        m_tuning = true;
        emit tuningChanged();
        suspendNonAudioTxWork(QStringLiteral("tune-tci"));

        bridgeLog("startTune(TCI): canPtt=" + QString::number(activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) +
                  " catConnected=" + QString::number(m_catConnected));
        syncActiveCatTxSplitFrequency(QStringLiteral("startTune"));
        const double freq = effectiveTxAudioFrequencyHz() > 0 ? effectiveTxAudioFrequencyHz() : 1500.0;
        if (!startTciTuneAudioStream(freq)) {
            m_tuning = false;
            emit tuningChanged();
            resumeNonAudioTxWork(QStringLiteral("tune-tci-start-failed"));
            emit errorMessage(QStringLiteral("Impossibile avviare audio TUNE TCI"));
            return;
        }
        if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
            activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, true, m_omniRigCat, m_legacyBackend);
            bridgeLog(QStringLiteral("PTT ON via tci after TUNE audio stream arm"));
        }
        emit statusMessage("TUNE TCI: " + QString::number(static_cast<int>(freq)) + " Hz");
        return;
    }

#if defined(Q_OS_MAC)
    {
        bool requestedDeviceFound = false;
        QAudioDevice outDev = resolveTxOutputDevice(&requestedDeviceFound);

        if (!requestedDeviceFound) {
            bridgeLog("startTune(mac): requested output device not found, fallback to default: " +
                      outDev.description() + " requested=[" + m_audioOutputDevice + "]");
            emit statusMessage("Audio TUNE non trovato, uso default: " + outDev.description());
        }
        const QAudioFormat outFmt = chooseTxAudioFormat(outDev);
        const AudioDevice::Channel outChannel = txOutputChannelForFormat(outFmt, m_audioOutputChannel);
        const qreal attenuationDb = txAttenuationFromSlider(m_txOutputLevel);
        const double freq = effectiveTxAudioFrequencyHz() > 0 ? effectiveTxAudioFrequencyHz() : 1500.0;

        updateSoundOutputDevice();
        m_soundOutput->setAttenuation(attenuationDb);
        if (m_modulator && m_modulator->isActive())
            m_modulator->stop(true);

        m_tuning = true;
        emit tuningChanged();
        suspendNonAudioTxWork(QStringLiteral("tune-mac"));

        bridgeLog("startTune(mac): canPtt=" + QString::number(activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) +
                  " catConnected=" + QString::number(m_catConnected));
        syncActiveCatTxSplitFrequency(QStringLiteral("startTune"));
        if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend))
            activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, true, m_omniRigCat, m_legacyBackend);

        m_modulator->tune(true);
        if (m_recordTxEnabled) {
            QVector<float> const wave = buildTuneWaveform(freq, 48000, 10);
            QString const txRecordPath = buildTxRecordingPath(QStringLiteral("TUNE"), true);
            saveTxRecordingAsync(txRecordPath, wave, 48000, QStringLiteral("TUNE"));
        }
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
    suspendNonAudioTxWork(QStringLiteral("tune"));

    bridgeLog("startTune: canPtt=" + QString::number(activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) +
              " catConnected=" + QString::number(m_catConnected));
    syncActiveCatTxSplitFrequency(QStringLiteral("startTune"));
    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend))
        activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, true, m_omniRigCat, m_legacyBackend);

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
        if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend))
            activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
        m_tuning = false;
        emit tuningChanged();
        resumeNonAudioTxWork(QStringLiteral("tune-start-failed"));
        emit errorMessage("Impossibile avviare l'audio TUNE");
        return;
    }

    const double freq = effectiveTxAudioFrequencyHz() > 0 ? effectiveTxAudioFrequencyHz() : 1500.0;
    bridgeLog("startTune push: freq=" + QString::number(freq) + " dev=" + m_audioOutputDevice);
    emit statusMessage("TUNE: " + QString::number(static_cast<int>(freq)) + " Hz");
}

bool DecodiumBridge::launchTuneAudio()
{
    const double freq    = effectiveTxAudioFrequencyHz() > 0 ? effectiveTxAudioFrequencyHz() : 1500.0;
    // Genera 10 secondi di tono sinusoidale a 48 kHz, poi converte nel formato
    // realmente supportato dal device di uscita.
    QVector<float> const wave = buildTuneWaveform(freq, 48000, 10);

    // Pulizia sink precedente
    if (m_txAudioSink) {
        m_txAudioSink->disconnect();
        m_txAudioSink->stop();
        delete m_txAudioSink;
        m_txAudioSink = nullptr;
    }
    if (m_txPcmBuffer) { delete m_txPcmBuffer; m_txPcmBuffer = nullptr; }

    bool requestedDeviceFound = false;
    QAudioDevice outDev = resolveTxOutputDevice(&requestedDeviceFound);

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
    if (m_recordTxEnabled) {
        QString const txRecordPath = buildTxRecordingPath(QStringLiteral("TUNE"), true);
        saveTxRecordingAsync(txRecordPath, wave, 48000, QStringLiteral("TUNE"));
    }

    m_txPcmBuffer = new QBuffer(this);
    m_txPcmBuffer->setData(m_txPcmData);
    m_txPcmBuffer->open(QIODevice::ReadOnly);
    m_txPcmBuffer->seek(0);

    m_txAudioSink = new QAudioSink(outDev, outFmt, this);
    m_txAudioSink->setVolume(static_cast<float>(txGainFromSlider(m_txOutputLevel)));
    qsizetype const tuneBufferBytes = txAudioBufferBytesForMode(outFmt, QStringLiteral("TUNE"), true);
    if (tuneBufferBytes > 0) {
        m_txAudioSink->setBufferSize(tuneBufferBytes);
    }
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
              " buf=" + QString::number(m_txAudioSink->bufferSize()) +
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
        if (m_tuning) {
            m_tuning = false;
            emit tuningChanged();
            emit statusMessage("Tune terminato");
        }
        scheduleLegacyStateRefreshBurst();
        return;
    }

    m_txPlaybackHoldUntilMs = 0;
    m_txPlaybackReleasePending = false;

#if defined(Q_OS_MAC)
    if (m_tuneTimer) m_tuneTimer->stop();
    if (m_modulator)
        m_modulator->tune(false);
    if (m_soundOutput)
        m_soundOutput->stop();
    m_txPcmData.clear();
    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend))
        activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
    if (m_tuning) {
        m_tuning = false;
        emit tuningChanged();
        emit statusMessage("Tune terminato");
    }
    resumeNonAudioTxWork(QStringLiteral("stopTune"));
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
    if (activeCatCanPtt(m_nativeCat, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend))
        activeCatSetPtt(m_nativeCat, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
    if (usingTciAudioInput()) stopTciTxAudioStream(true);
    m_tuning = false;
    emit tuningChanged();
    emit statusMessage("Tune terminato");
    resumeNonAudioTxWork(QStringLiteral("stopTune"));
}

void DecodiumBridge::halt()
{
    engageManualTxHold(QStringLiteral("halt"), true);
    clearDeferredManualSyncTx(QStringLiteral("halt"));

    if (usingLegacyBackendForTx()) {
        bridgeLog("halt: delegating stop to legacy backend");
        m_legacyBackend->stopTransmission();
        m_legacyBackend->startTune(false);
        if (m_transmitting) {
            m_transmitting = false;
            emit transmittingChanged();
        }
        if (m_tuning) {
            m_tuning = false;
            emit tuningChanged();
        }
        setTxEnabled(false);
        scheduleLegacyStateRefreshBurst();
        return;
    }

    stopTx();
    stopTune();
    setTxEnabled(false);
}

void DecodiumBridge::refreshAudioDevices()
{
    enumerateAudioDevices();

    auto preferredDeviceName = [](QAudioDevice const& preferred,
                                  QList<QAudioDevice> const& devices) -> QString {
        QString const preferredName = preferred.description().trimmed();
        if (!preferredName.isEmpty() && hasAudioDeviceNamed(devices, preferredName)) {
            return preferredName;
        }
        return devices.isEmpty() ? QString() : devices.constFirst().description().trimmed();
    };

    bool const tciAudio = usingTciAudioInput();
    QList<QAudioDevice> const inputs = QMediaDevices::audioInputs();
    if (tciAudio) {
        applyAudioInputRuntimeChange(QStringLiteral("refresh: TCI audio selected"));
    } else if (!inputs.isEmpty()) {
        if (hasAudioDeviceNamed(inputs, m_audioInputDevice)) {
            if (legacyBackendAvailable()) {
                m_legacyBackend->setAudioInputDeviceName(m_audioInputDevice);
                m_legacyBackend->setAudioInputChannel(qBound(0, m_audioInputChannel, 3));
            }
            applyAudioInputRuntimeChange(QStringLiteral("refresh: rebind selected input"));
        } else {
            QString const fallbackInput =
                preferredDeviceName(QMediaDevices::defaultAudioInput(), inputs);
            if (!fallbackInput.isEmpty()) {
                setAudioInputDevice(fallbackInput);
            }
        }
    }

    QList<QAudioDevice> const outputs = QMediaDevices::audioOutputs();
    if (tciAudio) {
        applyAudioOutputRuntimeChange(QStringLiteral("refresh: TCI audio selected"));
    } else if (!outputs.isEmpty()) {
        if (hasAudioDeviceNamed(outputs, m_audioOutputDevice)) {
            if (legacyBackendAvailable()) {
                m_legacyBackend->setAudioOutputDeviceName(m_audioOutputDevice);
                m_legacyBackend->setAudioOutputChannel(qBound(0, m_audioOutputChannel, 3));
            }
            applyAudioOutputRuntimeChange(QStringLiteral("refresh: rebind selected output"));
        } else {
            QString const fallbackOutput =
                preferredDeviceName(QMediaDevices::defaultAudioOutput(), outputs);
            if (!fallbackOutput.isEmpty()) {
                setAudioOutputDevice(fallbackOutput);
            }
        }
    }

    emit statusMessage(QStringLiteral("Dispositivi audio aggiornati: %1 input, %2 output")
                           .arg(m_audioInputDevices.size())
                           .arg(m_audioOutputDevices.size()));
}

void DecodiumBridge::clearDecodeList()
{
    if (usingLegacyBackendForTx()) {
        bridgeLog("clearDecodeList: delegating band activity clear to legacy backend");
        m_legacyBackend->clearBandActivity();
        m_legacyBandActivityRevision = -1;
        QString const path = legacyAllTxtPath().trimmed();
        QFileInfo const info(path);
        m_legacyAllTxtConsumedPath = info.exists() ? info.absoluteFilePath() : path;
        m_legacyAllTxtConsumedSize = info.exists() ? info.size() : -1;
    }
    m_decodeList.clear();
    clearRemoteActivityCache(true);
    emit decodeListChanged();
    if (m_activeStations) {
        m_activeStations->clear();
    }
}

void DecodiumBridge::clearRxDecodes()
{
    if (usingLegacyBackendForTx()) {
        m_legacyBackend->clearRxFrequency();
        m_legacyRxFrequencyRevision = -1;
        QString const path = legacyAllTxtPath().trimmed();
        QFileInfo const info(path);
        m_legacyAllTxtConsumedPath = info.exists() ? info.absoluteFilePath() : path;
        m_legacyAllTxtConsumedSize = info.exists() ? info.size() : -1;
        m_legacyAllTxtRevisionKey.clear();
        m_legacyClearedRxMirrorKeys.clear();
        for (QVariant const& value : std::as_const(m_rxDecodeList)) {
            m_legacyClearedRxMirrorKeys.insert(decodeMirrorEntryKey(value.toMap()));
        }
    }
    m_rxDecodeList.clear();
    emit rxDecodeListChanged();
}

void DecodiumBridge::clearRemoteActivityCache(bool notifyRemote)
{
    m_remoteActivityKeys.clear();
    m_remoteActivityKeyOrder.clear();
    if (notifyRemote && m_remoteServer && m_remoteServer->isRunning()) {
        m_remoteServer->clearBandActivity();
    }
}

void DecodiumBridge::publishRemoteActivityEntries(QVariantList const& entries)
{
    if (!m_remoteServer || !m_remoteServer->isRunning()) {
        return;
    }

    for (QVariant const& value : entries) {
        publishRemoteActivityEntry(value.toMap());
    }
}

void DecodiumBridge::publishRemoteActivityEntry(QVariantMap const& entry)
{
    if (!m_remoteServer || !m_remoteServer->isRunning()) {
        return;
    }

    QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
    if (message.isEmpty()) {
        return;
    }

    QString time = normalizeUtcDisplayToken(entry.value(QStringLiteral("time")).toString());
    if (time.isEmpty()) {
        time = QDateTime::currentDateTimeUtc().toString(QStringLiteral("HHmmss"));
    }

    QString db = entry.value(QStringLiteral("db")).toString().trimmed();
    if (db.isEmpty()) {
        db = entry.value(QStringLiteral("isTx")).toBool() ? QStringLiteral("TX") : QStringLiteral("0");
    }

    QString dt = entry.value(QStringLiteral("dt")).toString().trimmed();
    if (dt.isEmpty()) {
        dt = QStringLiteral("0.0");
    }

    QString freq = entry.value(QStringLiteral("freq")).toString().trimmed();
    if (freq.isEmpty()) {
        freq = QString::number(entry.value(QStringLiteral("isTx")).toBool() ? m_txFrequency : m_rxFrequency);
    }

    QString const line = QStringLiteral("%1 %2 %3 %4 %5")
                             .arg(time, db, dt, freq, message)
                             .simplified();
    QString const key = line + QStringLiteral("|") + entry.value(QStringLiteral("mode"), m_mode).toString();
    if (m_remoteActivityKeys.contains(key)) {
        return;
    }

    m_remoteActivityKeys.insert(key);
    m_remoteActivityKeyOrder.append(key);
    while (m_remoteActivityKeyOrder.size() > 4096) {
        m_remoteActivityKeys.remove(m_remoteActivityKeyOrder.takeFirst());
    }

    m_remoteServer->publishBandActivityLine(line);
}

QVariantMap DecodiumBridge::worldClockSnapshot(const QString& timeZoneId) const
{
    QByteArray const zoneId = timeZoneId.trimmed().toUtf8();
    QTimeZone zone = (zoneId.isEmpty() || zoneId == "UTC")
        ? QTimeZone::utc()
        : QTimeZone(zoneId);

    if (!zone.isValid()) {
        zone = QTimeZone::utc();
    }

    QDateTime const zonedNow = QDateTime::currentDateTimeUtc().toTimeZone(zone);

    QVariantMap snapshot;
    snapshot.insert(QStringLiteral("hours"), zonedNow.time().hour());
    snapshot.insert(QStringLiteral("minutes"), zonedNow.time().minute());
    snapshot.insert(QStringLiteral("seconds"), zonedNow.time().second());
    snapshot.insert(QStringLiteral("date"), zonedNow.date().toString(QStringLiteral("dd/MM/yyyy")));
    snapshot.insert(QStringLiteral("abbreviation"), zone.abbreviation(zonedNow));
    snapshot.insert(QStringLiteral("offsetSeconds"), zone.offsetFromUtc(zonedNow));
    snapshot.insert(QStringLiteral("timeZoneId"), QString::fromUtf8(zone.id()));
    return snapshot;
}

// ── catBackend switch ─────────────────────────────────────────────────────────
void DecodiumBridge::setCatBackend(const QString& v)
{
    if (m_catBackend == v) return;
    halt();
    // Disconnetti TUTTI i manager per difesa: anche quelli non segnati come
    // "connected" possono tenere risorse (es. QAxObject di OmniRig o porta
    // seriale QSerialPort del backend nativo). Evita conflitti "serial port
    // already open" quando si alterna fra omnirig, hamlib e native.
    if (m_nativeCat)  m_nativeCat->disconnectRig();
    if (m_omniRigCat) m_omniRigCat->disconnectRig();
    if (m_hamlibCat)  m_hamlibCat->disconnectRig();
    // Lasciamo girare gli eventi in modo che i deleteLater() dei QAxObject
    // OmniRig e la chiusura della porta seriale del backend nativo si
    // completino prima di cedere il controllo al nuovo backend.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    m_catConnected = false;
    emit catConnectedChanged();
    updateRigTelemetry(0.0, 0.0);
    if (!m_lastCatError.isEmpty()) {
        m_lastCatError.clear();
        emit lastCatErrorChanged();
    }
    m_catBackend = v;
    emit catBackendChanged();
    emit catManagerChanged();
    QSettings s2("Decodium","Decodium3");
    s2.setValue("catBackend", m_catBackend);
}

QString DecodiumBridge::configuredCatRigMode() const
{
    QVariant const raw = getSetting(QStringLiteral("CATMode"), 0);
    QString const rawText = raw.toString().trimmed();

    if (rawText.compare(QStringLiteral("None"), Qt::CaseInsensitive) == 0) {
        return {};
    }
    if (rawText.compare(QStringLiteral("Data/Pkt"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("DATA-U"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("USB-D"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("DIGU"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("DATA-U");
    }
    if (rawText.compare(QStringLiteral("USB"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("USB");
    }

    bool ok = false;
    int const index = raw.toInt(&ok);
    if (!ok) {
        return QStringLiteral("USB");
    }
    if (index == 1) {
        return QStringLiteral("DATA-U");
    }
    if (index == 2) {
        return {};
    }
    return QStringLiteral("USB");
}

void DecodiumBridge::applyConfiguredCatRigMode(const QString& reason)
{
    QString const rigMode = configuredCatRigMode();
    if (rigMode.isEmpty()) {
        return;
    }

    if (m_catBackend == QStringLiteral("native")) {
        if (m_nativeCat && m_nativeCat->connected()) {
            bridgeLog(QStringLiteral("CAT rig mode sync (%1): native -> %2").arg(reason, rigMode));
            m_nativeCat->setRigMode(rigMode);
        }
        return;
    }

    if (isHamlibFamilyBackend(m_catBackend)) {
        if (m_hamlibCat && m_hamlibCat->connected()) {
            bridgeLog(QStringLiteral("CAT rig mode sync (%1): %2 -> %3").arg(reason, m_catBackend, rigMode));
            m_hamlibCat->setRigMode(rigMode);
        }
        return;
    }

    if (m_catBackend == QStringLiteral("omnirig") && m_omniRigCat && m_omniRigCat->connected()) {
        bridgeLog(QStringLiteral("CAT rig mode sync (%1): omnirig -> %2").arg(reason, rigMode));
        m_omniRigCat->setRigMode(rigMode);
    }
}


void DecodiumBridge::connectDxCluster()
{
    connectDxCluster(QString(), 0);
}

void DecodiumBridge::connectDxCluster(const QString& host, int port)
{
    if (!m_dxCluster) return;
    QString const h = host.trimmed();
    if (!h.isEmpty()) {
        m_dxCluster->setHost(h);
    }
    if (port > 0) {
        m_dxCluster->setPort(port);
    }
    if (!m_callsign.isEmpty()) m_dxCluster->setCallsign(m_callsign);
    bridgeLog("connectDxCluster: " + m_dxCluster->host() + ":" + QString::number(m_dxCluster->port())
              + " as " + m_callsign);
    m_dxCluster->connectCluster();
}

void DecodiumBridge::disconnectDxCluster()
{
    if (m_dxCluster) m_dxCluster->disconnectCluster();
}

void DecodiumBridge::setAutoSpotEnabled(bool enabled)
{
    setSetting(QStringLiteral("AutoSpotEnabled"), enabled);
}

void DecodiumBridge::setNextLogClusterSpotEnabled(bool enabled)
{
    m_nextLogClusterSpotOverrideValid = true;
    m_nextLogClusterSpotEnabled = enabled;
}

void DecodiumBridge::setNextLogComment(const QString& comment)
{
    m_promptLogComment = comment.trimmed();
    m_promptLogCommentOverrideValid = true;
    setSetting(QStringLiteral("LogComments"), m_promptLogComment);
}

void DecodiumBridge::clearNextLogClusterSpotOverride()
{
    m_nextLogClusterSpotOverrideValid = false;
    m_nextLogClusterSpotEnabled = false;
}

void DecodiumBridge::requestSafeGraphicsNextLaunch(const QString& reason)
{
#ifdef Q_OS_WIN
    writeSlowQmlStartupMarker(reason.trimmed().isEmpty()
                                  ? QStringLiteral("QML startup timed out; use safe graphics on next launch")
                                  : reason.trimmed());
    QString const flagPath = decodiumSlowQmlStartupFlagPath();
    bridgeLog(QStringLiteral("Safe graphics marker requested for next launch: %1").arg(flagPath));
#else
    Q_UNUSED(reason)
#endif
}

void DecodiumBridge::notifyMainQmlLoadStarted()
{
#ifdef Q_OS_WIN
    auto done = std::make_shared<std::atomic_bool>(false);
    m_mainQmlAsyncLoadDone = done;
    appendStartupLogLine(QStringLiteral("Main.qml async load watchdog armed"));
    std::thread([done]() {
        for (int elapsedSeconds = 30; elapsedSeconds <= 300; elapsedSeconds += 30) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            if (done->load(std::memory_order_relaxed)) {
                return;
            }
            QString const msg = QStringLiteral("Main.qml async load still running after %1 s")
                                    .arg(elapsedSeconds);
            appendStartupLogLine(msg);
            if (elapsedSeconds == 30) {
                writeSlowQmlStartupMarker(
                    QStringLiteral("Main.qml async load exceeded 30 seconds; use safe graphics on next launch"));
                appendStartupLogLine(QStringLiteral("Main.qml async load watchdog wrote safe graphics marker"));
            }
        }
    }).detach();
#endif
}

QVariant DecodiumBridge::getSetting(const QString& key, const QVariant& defaultValue) const
{
    QVariant effectiveDefault = defaultValue;
    if (!effectiveDefault.isValid()
        && (key == QStringLiteral("ShowDXCC") || key == QStringLiteral("DXCCEntity"))) {
        effectiveDefault = true;
    }

    if (key == QStringLiteral("PromptToLog") || key == QStringLiteral("AutoLog")) {
        auto readCanonicalSetting = [this](const QString& settingKey) -> QVariant {
            if (isLegacySyncKey(settingKey)) {
                QVariant v = readSettingFromLegacyIni(settingKey);
                if (v.isValid()) {
                    return v;
                }
                QString const legacyKey = aliasedBridgeSettingKey(settingKey);
                if (legacyKey != settingKey) {
                    v = readSettingFromLegacyIni(legacyKey);
                    if (v.isValid()) {
                        return v;
                    }
                }
            }

            QSettings s("Decodium", "Decodium3");
            QVariant v = s.value(settingKey);
            if (!v.isValid()) {
                QString const legacyKey = aliasedBridgeSettingKey(settingKey);
                if (legacyKey != settingKey) {
                    v = s.value(legacyKey);
                }
            }
            return v;
        };

        QVariant const promptToLog = readCanonicalSetting(QStringLiteral("PromptToLog"));
        QVariant const autoLog = readCanonicalSetting(QStringLiteral("AutoLog"));
        bool promptEnabled = promptToLog.isValid() ? promptToLog.toBool() : false;
        bool autoLogEnabled = autoLog.isValid() ? autoLog.toBool() : !promptEnabled;
        if (promptEnabled && autoLogEnabled) {
            // If older or corrupted settings left both modes enabled, prefer
            // the explicit user-facing prompt instead of silently auto-logging.
            autoLogEnabled = false;
        } else if (!promptEnabled && !autoLogEnabled) {
            autoLogEnabled = true;
        }
        return key == QStringLiteral("PromptToLog") ? promptEnabled : autoLogEnabled;
    }

    // For settings owned by the legacy Configuration dialog, read the legacy
    // INI first because that is the canonical source used by Configuration.
    if (isLegacySyncKey(key)) {
        QVariant v = readSettingFromLegacyIni(key);
        if (v.isValid()) {
            return v;
        }
        QString const legacyKey = aliasedBridgeSettingKey(key);
        if (legacyKey != key) {
            v = readSettingFromLegacyIni(legacyKey);
            if (v.isValid()) {
                return v;
            }
        }
    }

    QSettings s("Decodium", "Decodium3");
    QVariant value = s.value(key);
    if (!value.isValid()) {
        QString const legacyKey = aliasedBridgeSettingKey(key);
        if (legacyKey != key) {
            value = s.value(legacyKey);
        }
    }
    if (!value.isValid() && key == QStringLiteral("RemoteHttpPort")) {
        value = s.value(QStringLiteral("WebAppHttpPort"));
    }
    return value.isValid() ? value : effectiveDefault;
}

void DecodiumBridge::setSetting(const QString& key, const QVariant& value)
{
    QSettings s("Decodium", "Decodium3");
    if (key == QStringLiteral("PromptToLog") || key == QStringLiteral("AutoLog")) {
        bool promptMode = false;
        if (key == QStringLiteral("PromptToLog")) {
            promptMode = value.toBool();
        } else {
            promptMode = !value.toBool();
        }

        bool const autoLogMode = !promptMode;
        s.setValue(QStringLiteral("PromptToLog"), promptMode);
        s.setValue(QStringLiteral("AutoLog"), autoLogMode);
        s.sync();

        syncSettingToLegacyIni(QStringLiteral("PromptToLog"), promptMode);
        syncSettingToLegacyIni(QStringLiteral("AutoLog"), autoLogMode);

        emit settingValueChanged(QStringLiteral("PromptToLog"), promptMode);
        emit settingValueChanged(QStringLiteral("AutoLog"), autoLogMode);
        return;
    }

    QString exclusiveLoggingKey;
    if (value.toBool()) {
        if (key == QStringLiteral("PromptToLog")) {
            exclusiveLoggingKey = QStringLiteral("AutoLog");
        } else if (key == QStringLiteral("AutoLog")) {
            exclusiveLoggingKey = QStringLiteral("PromptToLog");
        }
    }

    QString const legacyKey = aliasedBridgeSettingKey(key);
    if (key == QStringLiteral("MonitorOFF")) {
        s.setValue(QStringLiteral("MonitorOFF"), false);
        if (legacyKey != key) {
            s.setValue(legacyKey, false);
        }
        s.sync();
        syncSettingToLegacyIni(QStringLiteral("MonitorOFF"), false);
        if (!m_autoStartMonitorOnStartup) {
            m_autoStartMonitorOnStartup = true;
            emit autoStartMonitorOnStartupChanged();
        }
        bridgeLog(QStringLiteral("setSetting: MonitorOFF forced to 0 autoStartMonitorOnStartup=1"));
        emit settingValueChanged(QStringLiteral("MonitorOFF"), false);
        if (legacyKey != key) {
            emit settingValueChanged(legacyKey, false);
        }
        return;
    }

    s.setValue(key, value);
    if (legacyKey != key) {
        s.setValue(legacyKey, value);
    }
    if (key == QStringLiteral("RemoteWebEnabled") || key == QStringLiteral("WebAppEnabled")) {
        s.setValue(QStringLiteral("RemoteWebEnabled"), value);
        s.setValue(QStringLiteral("WebAppEnabled"), value);
        s.setValue(QStringLiteral("WebAppActive"), value.toBool() && m_remoteServer && m_remoteServer->isRunning());
    }
    if (!exclusiveLoggingKey.isEmpty()) {
        s.setValue(exclusiveLoggingKey, false);
        QString const exclusiveLegacyKey = aliasedBridgeSettingKey(exclusiveLoggingKey);
        if (exclusiveLegacyKey != exclusiveLoggingKey) {
            s.setValue(exclusiveLegacyKey, false);
        }
    }
    if (key == QStringLiteral("RemoteHttpPort")) {
        bool ok = false;
        int httpPort = value.toInt(&ok);
        if (ok && httpPort > 1) {
            int wsPort = httpPort - 1;
            s.setValue(QStringLiteral("RemoteWsPort"), wsPort);
            s.setValue(QStringLiteral("WebAppWsPort"), wsPort);
        }
    }
    if (key == QStringLiteral("CheckSWR") && value.toBool()) {
        s.setValue(QStringLiteral("PWRandSWR"), true);
    }
    if (legacyKey == QStringLiteral("HighDPI")) {
        if (value.toBool()) {
            QFile::remove(QStringLiteral("DisableHighDpiScaling"));
        } else {
            QFile f(QStringLiteral("DisableHighDpiScaling"));
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&f);
                out << QStringLiteral("DisableHighDpiScaling=\"true\"");
            }
        }
    }
    s.sync();
    if (key == QStringLiteral("alertSoundsEnabled") || key == QStringLiteral("alert_Enabled")) {
        bool const enabled = value.toBool();
        if (m_alertSoundsEnabled != enabled) {
            m_alertSoundsEnabled = enabled;
            emit alertSoundsEnabledChanged();
        }
    } else if (key == QStringLiteral("alertOnCq") || key == QStringLiteral("alert_CQ")) {
        bool const enabled = value.toBool();
        if (m_alertOnCq != enabled) {
            m_alertOnCq = enabled;
            emit alertOnCqChanged();
        }
    } else if (key == QStringLiteral("alertOnMyCall") || key == QStringLiteral("alert_MyCall")) {
        bool const enabled = value.toBool();
        if (m_alertOnMyCall != enabled) {
            m_alertOnMyCall = enabled;
            emit alertOnMyCallChanged();
        }
    } else if (key == QStringLiteral("AutoSpotEnabled")) {
        bool const enabled = value.toBool();
        if (m_autoSpotEnabled != enabled) {
            m_autoSpotEnabled = enabled;
            emit autoSpotEnabledChanged();
        }
        if (legacyBackendAvailable()) {
            m_legacyBackend->setAutoSpotEnabled(enabled);
        }
    }
    emit settingValueChanged(key, value);
    if (legacyKey != key) {
        emit settingValueChanged(legacyKey, value);
    }
    if (!exclusiveLoggingKey.isEmpty()) {
        emit settingValueChanged(exclusiveLoggingKey, false);
        QString const exclusiveLegacyKey = aliasedBridgeSettingKey(exclusiveLoggingKey);
        if (exclusiveLegacyKey != exclusiveLoggingKey) {
            emit settingValueChanged(exclusiveLegacyKey, false);
        }
    }
    if (key == QStringLiteral("CheckSWR") && value.toBool()) {
        emit settingValueChanged(QStringLiteral("PWRandSWR"), true);
        enforceSwrTransmissionLimit(QStringLiteral("setting-change"));
    }
    // Aggiorna il timer spectrum se cambia l'intervallo
    if (key == "spectrumInterval" && m_spectrumTimer) {
        int ms = qBound(10, value.toInt(), 500);
        m_spectrumTimer->setInterval(ms);
        bridgeLog("Spectrum timer interval: " + QString::number(ms) + "ms");
    }
    if (key == QStringLiteral("NTPEnabled")
        || key == QStringLiteral("NTPCustomServer")
        || key == QStringLiteral("NTPOffset_ms")) {
        applyNtpSettings();

        if (key == QStringLiteral("NTPEnabled")) {
            bool const enabled = value.toBool();
            emit statusMessage(enabled
                               ? QStringLiteral("NTP abilitato: sincronizzazione in corso...")
                               : QStringLiteral("NTP disabilitato"));
        } else if (key == QStringLiteral("NTPCustomServer")) {
            QString const server = value.toString().trimmed();
            emit statusMessage(server.isEmpty()
                               ? QStringLiteral("NTP: uso server pubblici automatici")
                               : QStringLiteral("NTP: server impostato su %1").arg(server));
        }
    }

    // Sync legacy-owned keys to the legacy INI file so that the embedded
    // MainWindow / Configuration / MessageClient can see them.
    if (isLegacySyncKey(key)) {
        syncSettingToLegacyIni(key, value);
        if (legacyKey != key) {
            syncSettingToLegacyIni(legacyKey, value);
        }
    }
    if (key == QStringLiteral("SelectedActivity")) {
        int const activity = qBound(kSpecialOpNone, value.toInt(), kSpecialOpMax);
        if (activity != m_specialOperationActivity) {
            setSpecialOperationActivity(activity);
        }
    } else if (key == QStringLiteral("SpecialOpActivity")) {
        if (!value.toBool() && m_specialOperationActivity != kSpecialOpNone) {
            setSpecialOperationActivity(kSpecialOpNone);
        }
    } else if (key == QStringLiteral("SuperFox") && legacyBackendAvailable()) {
        syncSpecialOperationToLegacyBackend();
    }
    if (!exclusiveLoggingKey.isEmpty()) {
        syncSettingToLegacyIni(exclusiveLoggingKey, false);
        QString const exclusiveLegacyKey = aliasedBridgeSettingKey(exclusiveLoggingKey);
        if (exclusiveLegacyKey != exclusiveLoggingKey) {
            syncSettingToLegacyIni(exclusiveLegacyKey, false);
        }
    }

    static QSet<QString> const udpRuntimeKeys {
        QStringLiteral("UDPServer"),
        QStringLiteral("UDPServerPort"),
        QStringLiteral("UDPListenPort"),
        QStringLiteral("UDPClientId"),
        QStringLiteral("UDPTTL"),
        QStringLiteral("AcceptUDPRequests"),
        QStringLiteral("UDPSecondaryEnabled"),
        QStringLiteral("UDPSecondaryServer"),
        QStringLiteral("UDPSecondaryServerPort"),
        QStringLiteral("UDPSecondaryInterface"),
        QStringLiteral("UDPSecondaryTTL"),
        QStringLiteral("UDPTertiaryEnabled"),
        QStringLiteral("UDPTertiaryServer"),
        QStringLiteral("UDPTertiaryServerPort"),
        QStringLiteral("UDPTertiaryInterface"),
        QStringLiteral("UDPTertiaryTTL"),
        QStringLiteral("UDPTertiaryLoggedAdifEnabled"),
    };
    if (udpRuntimeKeys.contains(key)) {
        scheduleUdpMessageClientRestart();
    }

    if (key == QStringLiteral("CATMode")) {
        applyConfiguredCatRigMode(QStringLiteral("setting-change"));
    }
    if (key == QStringLiteral("TXAudioSource")
        && isHamlibFamilyBackend(m_catBackend)
        && m_hamlibCat
        && m_hamlibCat->connected()) {
        emit statusMessage(QStringLiteral("TX Audio Src: riconnessione CAT per applicare la sorgente audio"));
        QTimer::singleShot(0, this, [this]() { retryRigConnection(); });
    }
    if ((key == QStringLiteral("PWRandSWR") || key == QStringLiteral("CheckSWR"))
        && isHamlibFamilyBackend(m_catBackend)
        && m_hamlibCat
        && m_hamlibCat->connected()) {
        emit statusMessage(QStringLiteral("PWR/SWR: riconnessione CAT per applicare il polling"));
        QTimer::singleShot(0, this, [this]() { retryRigConnection(); });
    }
}

QStringList DecodiumBridge::satelliteOptions() const
{
    QStringList options {QString()};
    QString const appDir = QCoreApplication::applicationDirPath();
    QStringList candidates {
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).absoluteFilePath(QStringLiteral("sat.dat")),
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).absoluteFilePath(QStringLiteral("sat.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("sat.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../sat.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/sat.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../share/Decodium/sat.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../share/wsjtx/sat.dat")),
        QDir::current().absoluteFilePath(QStringLiteral("sat.dat"))
    };
#ifdef CMAKE_SOURCE_DIR
    candidates << QDir(QStringLiteral(CMAKE_SOURCE_DIR)).absoluteFilePath(QStringLiteral("sat.dat"));
#endif

    QSet<QString> seenPaths;
    QString satFilePath;
    for (QString const& candidate : candidates) {
        QString const clean = QDir::cleanPath(candidate);
        if (clean.isEmpty() || seenPaths.contains(clean)) {
            continue;
        }
        seenPaths.insert(clean);
        QFileInfo const info {clean};
        if (info.exists() && info.isFile() && info.size() <= 4LL * 1024LL * 1024LL) {
            satFilePath = clean;
            break;
        }
    }

    if (satFilePath.isEmpty()) {
        satFilePath = QStringLiteral(":/sat.dat");
    }

    QFile file {satFilePath};
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return options;
    }

    QSet<QString> seenCodes;
    QTextStream stream {&file};
    while (!stream.atEnd()) {
        QString const line = stream.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        QStringList const parts = line.split(QLatin1Char('|'));
        if (parts.size() < 2) {
            continue;
        }
        QString const code = parts.at(0).trimmed();
        QString const name = parts.at(1).trimmed();
        if (code.isEmpty() || seenCodes.contains(code)) {
            continue;
        }
        seenCodes.insert(code);
        options << QStringLiteral("%1 - %2").arg(code, name.isEmpty() ? code : name);
    }
    return options;
}

QStringList DecodiumBridge::satModeOptions() const
{
    return QStringList {
        QString(),
        QStringLiteral("A"),
        QStringLiteral("B"),
        QStringLiteral("BS"),
        QStringLiteral("JA"),
        QStringLiteral("JD"),
        QStringLiteral("K"),
        QStringLiteral("KA"),
        QStringLiteral("KT"),
        QStringLiteral("L"),
        QStringLiteral("LS"),
        QStringLiteral("LU"),
        QStringLiteral("LX"),
        QStringLiteral("S"),
        QStringLiteral("SX"),
        QStringLiteral("T"),
        QStringLiteral("US"),
        QStringLiteral("UV"),
        QStringLiteral("VS"),
        QStringLiteral("VU")
    };
}

QFont DecodiumBridge::fontSettingFont(const QString& key,
                                      const QString& fallbackFamily,
                                      int fallbackPointSize) const
{
    QFont fallback = fallbackBridgeFont(fallbackFamily, fallbackPointSize);
    QString const raw = getSetting(key, fallback.toString()).toString().trimmed();
    if (raw.isEmpty()) {
        return fallback;
    }

    QFont parsed;
    if (parsed.fromString(raw)) {
        return parsed;
    }

    // Be tolerant of older/manual values that only contain a family name.
    fallback.setFamily(raw);
    return fallback;
}

QString DecodiumBridge::fontSettingLabel(const QString& key,
                                         const QString& fallbackFamily,
                                         int fallbackPointSize) const
{
    QFont const font = fontSettingFont(key, fallbackFamily, fallbackPointSize);
    int const pointSize = font.pointSize() > 0 ? font.pointSize() : fallbackBridgeFont(fallbackFamily, fallbackPointSize).pointSize();
    QStringList traits;
    if (font.bold()) traits << QStringLiteral("bold");
    if (font.italic()) traits << QStringLiteral("italic");
    QString label = QStringLiteral("%1, %2 pt").arg(font.family()).arg(pointSize);
    if (!traits.isEmpty()) {
        label += QStringLiteral(" (%1)").arg(traits.join(QStringLiteral(", ")));
    }
    return label;
}

QString DecodiumBridge::fontSettingFamily(const QString& key,
                                          const QString& fallbackFamily,
                                          int fallbackPointSize) const
{
    return fontSettingFont(key, fallbackFamily, fallbackPointSize).family();
}

int DecodiumBridge::fontSettingPixelSize(const QString& key,
                                         const QString& fallbackFamily,
                                         int fallbackPointSize) const
{
    QFont const font = fontSettingFont(key, fallbackFamily, fallbackPointSize);
    if (font.pixelSize() > 0) {
        return qBound(8, font.pixelSize(), 36);
    }
    int pointSize = font.pointSize();
    if (pointSize <= 0) {
        pointSize = fallbackBridgeFont(fallbackFamily, fallbackPointSize).pointSize();
    }
    return qBound(8, qRound(pointSize * 1.25), 36);
}

int DecodiumBridge::fontSettingPointSize(const QString& key,
                                         const QString& fallbackFamily,
                                         int fallbackPointSize) const
{
    QFont const font = fontSettingFont(key, fallbackFamily, fallbackPointSize);
    int pointSize = font.pointSize();
    if (pointSize <= 0) {
        pointSize = fallbackBridgeFont(fallbackFamily, fallbackPointSize).pointSize();
    }
    return qBound(6, pointSize, 48);
}

bool DecodiumBridge::fontSettingBold(const QString& key,
                                     const QString& fallbackFamily,
                                     int fallbackPointSize) const
{
    return fontSettingFont(key, fallbackFamily, fallbackPointSize).bold();
}

bool DecodiumBridge::fontSettingItalic(const QString& key,
                                       const QString& fallbackFamily,
                                       int fallbackPointSize) const
{
    return fontSettingFont(key, fallbackFamily, fallbackPointSize).italic();
}

QStringList DecodiumBridge::availableFontFamilies(bool fixedPitchOnly) const
{
    QStringList families = QFontDatabase::families();
    families.removeDuplicates();
    families.sort(Qt::CaseInsensitive);
    if (!fixedPitchOnly) {
        return families;
    }

    QStringList fixedFamilies;
    fixedFamilies.reserve(families.size());
    for (QString const& family : families) {
        if (QFontDatabase::isFixedPitch(family)) {
            fixedFamilies.append(family);
        }
    }
    return fixedFamilies.isEmpty() ? families : fixedFamilies;
}

void DecodiumBridge::setFontSetting(const QString& key,
                                    const QString& family,
                                    int pointSize,
                                    bool bold,
                                    bool italic,
                                    const QString& fallbackFamily,
                                    int fallbackPointSize)
{
    QFont font = fallbackBridgeFont(fallbackFamily, fallbackPointSize);
    QString const cleanFamily = family.trimmed();
    if (!cleanFamily.isEmpty()) {
        font.setFamily(cleanFamily);
    }
    font.setPointSize(qBound(6, pointSize > 0 ? pointSize : font.pointSize(), 48));
    font.setBold(bold);
    font.setItalic(italic);

    setSetting(key, font.toString());
    if (key == QStringLiteral("Font")) {
        QGuiApplication::setFont(font);
    }
    emit statusMessage(QStringLiteral("%1: %2").arg(key, fontSettingLabel(key, fallbackFamily, fallbackPointSize)));
}

void DecodiumBridge::chooseFontSetting(const QString& key,
                                       const QString& fallbackFamily,
                                       int fallbackPointSize)
{
    Q_UNUSED(fallbackFamily);
    Q_UNUSED(fallbackPointSize);
    emit statusMessage(QStringLiteral("%1: selettore font integrato disponibile nella UI").arg(key));
}

void DecodiumBridge::resetFontSetting(const QString& key,
                                      const QString& fallbackFamily,
                                      int fallbackPointSize)
{
    QFont const font = fallbackBridgeFont(fallbackFamily, fallbackPointSize);
    setSetting(key, font.toString());
    if (key == QStringLiteral("Font")) {
        QGuiApplication::setFont(font);
    }
    emit statusMessage(QStringLiteral("%1 ripristinato: %2").arg(key, fontSettingLabel(key, fallbackFamily, fallbackPointSize)));
}

// ---------------------------------------------------------------------------
// Legacy INI sync helpers
// ---------------------------------------------------------------------------

QString DecodiumBridge::legacyIniPath() const
{
    QString const configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QDir const configRoot(configDir.isEmpty() ? QDir::homePath() : configDir);
    QString const decodium4IniPath = configRoot.absoluteFilePath(QStringLiteral("decodium4.ini"));
    if (QFileInfo::exists(decodium4IniPath)) {
        return decodium4IniPath;
    }
    return configRoot.absoluteFilePath(QStringLiteral("ft2.ini"));
}

QString DecodiumBridge::legacyConfigGroupName() const
{
    // MultiSettings stores the active configuration name at
    // MultiSettings/CurrentName inside the INI file.
    QSettings ini(legacyIniPath(), QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("MultiSettings"));
    QString name = ini.value(QStringLiteral("CurrentName")).toString();
    ini.endGroup();
    if (name.isEmpty()) {
        name = QStringLiteral("Default");
    }
    return name;
}

bool DecodiumBridge::isLegacySyncKey(const QString& key) const
{
    static QSet<QString> const keys {
        QStringLiteral("UDPServer"),
        QStringLiteral("UDPServerPort"),
        QStringLiteral("UDPListenPort"),
        QStringLiteral("UDPClientId"),
        QStringLiteral("UDPInterface"),
        QStringLiteral("UDPTTL"),
        QStringLiteral("UDPPrimaryLoggedAdifEnabled"),
        QStringLiteral("UDPSecondaryEnabled"),
        QStringLiteral("UDPSecondaryServer"),
        QStringLiteral("UDPSecondaryServerPort"),
        QStringLiteral("UDPSecondaryInterface"),
        QStringLiteral("UDPSecondaryTTL"),
        QStringLiteral("UDPSecondaryLoggedAdifEnabled"),
        QStringLiteral("UDPTertiaryEnabled"),
        QStringLiteral("UDPTertiaryServer"),
        QStringLiteral("UDPTertiaryServerPort"),
        QStringLiteral("UDPTertiaryInterface"),
        QStringLiteral("UDPTertiaryTTL"),
        QStringLiteral("UDPTertiaryLoggedAdifEnabled"),
        QStringLiteral("ADIFTcpEnabled"),
        QStringLiteral("ADIFTcpServer"),
        QStringLiteral("ADIFTcpPort"),
        QStringLiteral("N1MMServer"),
        QStringLiteral("N1MMServerPort"),
        QStringLiteral("BroadcastToN1MM"),
        QStringLiteral("AcceptUDPRequests"),
        QStringLiteral("udpWindowToFront"),
        QStringLiteral("udpWindowRestore"),
        QStringLiteral("RemoteWebEnabled"),
        QStringLiteral("WebAppEnabled"),
        QStringLiteral("RemoteHttpPort"),
        QStringLiteral("RemoteWsPort"),
        QStringLiteral("RemoteWsBind"),
        QStringLiteral("RemoteUser"),
        QStringLiteral("RemoteToken"),
        QStringLiteral("PromptToLog"),
        QStringLiteral("AutoLog"),
        QStringLiteral("AutoSpotEnabled"),
        QStringLiteral("ContestingOnly"),
        QStringLiteral("ZZ00"),
        QStringLiteral("LogAsRTTY"),
        QStringLiteral("toRTTY"),
        QStringLiteral("Log4DigitGrids"),
        QStringLiteral("Log4digitGrids"),
        QStringLiteral("SpecOpInComments"),
        QStringLiteral("specOptoComments"),
        QStringLiteral("dBReportsToComments"),
        QStringLiteral("dBtoComments"),
        QStringLiteral("LogComments"),
        QStringLiteral("TxDelay"),
        QStringLiteral("TXDelay"),
        QStringLiteral("TxQSYAllowed"),
        QStringLiteral("AllowTXQSY"),
        QStringLiteral("73TxDisable"),
        QStringLiteral("DisableTXon73"),
        QStringLiteral("RepeatTx"),
        QStringLiteral("RepeatTXuntil73"),
        QStringLiteral("TxWatchdog"),
        QStringLiteral("TXWatchdog"),
        QStringLiteral("TuneWatchdog"),
        QStringLiteral("TuneWatchdogEnabled"),
        QStringLiteral("TuneWatchdogTime"),
        QStringLiteral("TuneWatchdogMin"),
        QStringLiteral("After73"),
        QStringLiteral("CWIDafter73"),
        QStringLiteral("IDint"),
        QStringLiteral("CWIDInterval"),
        QStringLiteral("x2ToneSpacing"),
        QStringLiteral("x4ToneSpacing"),
        QStringLiteral("AlternateBindings"),
        QStringLiteral("AlternateF1F6Bindings"),
        QStringLiteral("Highlight73"),
        QStringLiteral("HighlightOrange"),
        QStringLiteral("HighlightBlue"),
        QStringLiteral("HighlightOrangeCallsigns"),
        QStringLiteral("HighlightBlueCallsigns"),
        QStringLiteral("OrangeCallsigns"),
        QStringLiteral("BlueCallsigns"),
        QStringLiteral("MonitorOFF"),
        QStringLiteral("MonitorLastUsed"),
        QStringLiteral("AutoAstroWindow"),
        QStringLiteral("kHzWithoutK"),
        QStringLiteral("kHzWithoutk"),
        QStringLiteral("ProgressBarRed"),
        QStringLiteral("HighDPI"),
        QStringLiteral("LargerTabWidget"),
        QStringLiteral("QuickCall"),
        QStringLiteral("ForceCallFirst"),
        QStringLiteral("ForceCall1st"),
        QStringLiteral("VHFUHF"),
        QStringLiteral("WaitFeaturesEnabled"),
        QStringLiteral("EnableWaitFeatures"),
        QStringLiteral("erase_BandActivity"),
        QStringLiteral("EraseBandActivity"),
        QStringLiteral("clear_DXgrid"),
        QStringLiteral("ClearDXGrid"),
        QStringLiteral("clear_DXcall"),
        QStringLiteral("ClearDXCall"),
        QStringLiteral("set_RXtoTX"),
        QStringLiteral("RXtoTXafterQSO"),
        QStringLiteral("AlternateEraseButtonBehavior"),
        QStringLiteral("AlternateEraseButton"),
        QStringLiteral("TxWarningDisabled"),
        QStringLiteral("DisableButtonColoring"),
        QStringLiteral("SuperFox"),
        QStringLiteral("ShowOTP"),
        QStringLiteral("SelectedActivity"),
        QStringLiteral("SpecialOpActivity"),
        QStringLiteral("Field_Day_Exchange"),
        QStringLiteral("FieldDayExchange"),
        QStringLiteral("RTTY_Exchange"),
        QStringLiteral("RTTYExchange"),
        QStringLiteral("Contest_Name"),
        QStringLiteral("ContestName"),
        QStringLiteral("Individual_Contest_Name"),
        QStringLiteral("IndividualContestName"),
        QStringLiteral("NCCC_Sprint"),
        QStringLiteral("NCCCSprint"),
        QStringLiteral("OTPEnabled"),
        QStringLiteral("OTPSeed"),
        QStringLiteral("OTPinterval"),
        QStringLiteral("OTPInterval"),
        QStringLiteral("OTPUrl"),
        QStringLiteral("OTPURL"),
        QStringLiteral("alert_Enabled"),
        QStringLiteral("alert_CQ"),
        QStringLiteral("alert_MyCall"),
        QStringLiteral("alert_DXCC"),
        QStringLiteral("alert_DXCCOB"),
        QStringLiteral("alert_Grid"),
        QStringLiteral("alert_GridOB"),
        QStringLiteral("alert_Continent"),
        QStringLiteral("alert_ContinentOB"),
        QStringLiteral("alert_CQZ"),
        QStringLiteral("alert_CQZOB"),
        QStringLiteral("alert_ITUZ"),
        QStringLiteral("alert_ITUZOB"),
        QStringLiteral("alert_DXcall"),
        QStringLiteral("alert_QSYmessage"),
        QStringLiteral("AlertMyCall"),
        QStringLiteral("AlertNewDXCC"),
        QStringLiteral("AlertNewDXCCBand"),
        QStringLiteral("AlertNewGrid"),
        QStringLiteral("AlertNewGridBand"),
        QStringLiteral("AlertNewContinent"),
        QStringLiteral("AlertNewContinentBand"),
        QStringLiteral("AlertNewCQZone"),
        QStringLiteral("AlertNewCQZoneBand"),
        QStringLiteral("AlertNewITUZone"),
        QStringLiteral("AlertNewITUZoneBand"),
        QStringLiteral("AlertDXCallGrid"),
        QStringLiteral("AlertQSYMessage"),
        QStringLiteral("Blacklisted"),
        QStringLiteral("BlacklistEnabled"),
        QStringLiteral("Whitelisted"),
        QStringLiteral("WhitelistEnabled"),
        QStringLiteral("AlwaysPass"),
        QStringLiteral("AlwaysPassEnabled"),
        QStringLiteral("FiltersForWaitAndPounceOnly"),
        QStringLiteral("FiltersWaitPounceOnly"),
        QStringLiteral("FiltersForWord2"),
        QStringLiteral("FiltersCallingOnly"),
        QStringLiteral("Font"),
        QStringLiteral("DecodedTextFont"),
    };
    if (keys.contains(key)) {
        return true;
    }
    static QRegularExpression const indexedLegacyKey {
        QStringLiteral("^(Blacklist|Whitelist|Pass|AlwaysPass|Territory)([1-9]|1[0-2])$")
    };
    return indexedLegacyKey.match(key).hasMatch();
}

void DecodiumBridge::syncSettingToLegacyIni(const QString& key, const QVariant& value)
{
    QString const iniPath = legacyIniPath();
    if (iniPath.isEmpty()) return;

    QSettings ini(iniPath, QSettings::IniFormat);
    ini.setValue(key, value);
    QStringList groups {legacyConfigGroupName(), QStringLiteral("Configuration"), QStringLiteral("Default")};
    groups.removeDuplicates();
    for (QString const& group : groups) {
        if (group.isEmpty()) {
            continue;
        }
        ini.beginGroup(group);
        ini.setValue(key, value);
        ini.endGroup();
    }
    ini.sync();
}

QVariant DecodiumBridge::readSettingFromLegacyIni(const QString& key) const
{
    QString const iniPath = legacyIniPath();
    if (iniPath.isEmpty()) return {};

    QSettings ini(iniPath, QSettings::IniFormat);
    QVariant value = ini.value(key);
    if (value.isValid()) {
        return value;
    }

    QStringList groups {legacyConfigGroupName(), QStringLiteral("Configuration"), QStringLiteral("Default")};
    groups.removeDuplicates();
    for (QString const& group : groups) {
        if (group.isEmpty()) {
            continue;
        }
        ini.beginGroup(group);
        value = ini.value(key);
        ini.endGroup();
        if (value.isValid()) {
            return value;
        }
    }

    return {};
}

// ---------------------------------------------------------------------------
// Standalone UDP MessageClient for WSJT-X protocol
// ---------------------------------------------------------------------------

static quint16 udpPortFromSettingValue(const QVariant& value, quint16 fallback, bool allowZero = false)
{
    bool ok = false;
    uint const raw = value.toUInt(&ok);
    if (!ok || raw > 65535 || (!allowZero && raw == 0)) {
        return fallback;
    }
    return static_cast<quint16>(raw);
}

static QHostAddress resolveUdpHostAddress(QString const& serverName)
{
    QHostAddress target;
    QString const cleanServer = serverName.trimmed();
    if (target.setAddress(cleanServer)) {
        return target;
    }

    QHostInfo const hostInfo = QHostInfo::fromName(cleanServer);
    for (QHostAddress const& address : hostInfo.addresses()) {
        if (!address.isNull()) {
            return address;
        }
    }
    return {};
}

void DecodiumBridge::initUdpMessageClient()
{
    // Read UDP settings from the legacy INI (canonical source)
    QString serverName = getSetting(QStringLiteral("UDPServer"), QStringLiteral("127.0.0.1")).toString();
    quint16 serverPort = udpPortFromSettingValue(getSetting(QStringLiteral("UDPServerPort"), 2237), 2237);
    quint16 listenPort = udpPortFromSettingValue(getSetting(QStringLiteral("UDPListenPort"), 0), 0, true);
    int ttl = qBound(0, getSetting(QStringLiteral("UDPTTL"), 1).toInt(), 255);
    QStringList interfaces;
    QVariant ifaceVal = readSettingFromLegacyIni(QStringLiteral("UDPInterface"));
    if (ifaceVal.isValid()) interfaces = ifaceVal.toStringList();
    auto const boolText = [] (bool value) {
        return value ? QStringLiteral("enabled") : QStringLiteral("disabled");
    };
    auto const interfaceText = [] (QStringList const& names) {
        return names.isEmpty() ? QStringLiteral("all") : names.join(QStringLiteral(","));
    };
    bool const acceptUdpRequests = getSetting(QStringLiteral("AcceptUDPRequests"), true).toBool();
    bool const primaryAdifEnabled = getSetting(QStringLiteral("UDPPrimaryLoggedAdifEnabled"), true).toBool();
    bool const secondaryEnabled = getSetting(QStringLiteral("UDPSecondaryEnabled"), true).toBool();
    QString const secondaryServerName =
        getSetting(QStringLiteral("UDPSecondaryServer"), serverName).toString().trimmed();
    quint16 const secondaryPort = udpPortFromSettingValue(
        getSetting(QStringLiteral("UDPSecondaryServerPort"), 2239), 2239);
    int const secondaryTtl = qBound(0, getSetting(QStringLiteral("UDPSecondaryTTL"), ttl).toInt(), 255);
    QStringList secondaryInterfaces;
    QString const secondaryInterface =
        getSetting(QStringLiteral("UDPSecondaryInterface"), QString()).toString().trimmed();
    if (!secondaryInterface.isEmpty()) {
        secondaryInterfaces << secondaryInterface;
    }
    bool const secondaryAdifEnabled = getSetting(QStringLiteral("UDPSecondaryLoggedAdifEnabled"), true).toBool();
    bool const tertiaryEnabled = getSetting(QStringLiteral("UDPTertiaryEnabled"), false).toBool();
    QString const tertiaryServerName =
        getSetting(QStringLiteral("UDPTertiaryServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
    quint16 const tertiaryPort = udpPortFromSettingValue(
        getSetting(QStringLiteral("UDPTertiaryServerPort"), 2237), 2237);
    int const tertiaryTtl = qBound(0, getSetting(QStringLiteral("UDPTertiaryTTL"), ttl).toInt(), 255);
    QStringList tertiaryInterfaces;
    QString const tertiaryInterface =
        getSetting(QStringLiteral("UDPTertiaryInterface"), QString()).toString().trimmed();
    if (!tertiaryInterface.isEmpty()) {
        tertiaryInterfaces << tertiaryInterface;
    }
    bool const tertiaryAdifEnabled = getSetting(QStringLiteral("UDPTertiaryLoggedAdifEnabled"), true).toBool();
    bool const adifTcpEnabled = getSetting(QStringLiteral("ADIFTcpEnabled"), false).toBool();
    QString const adifTcpServer = getSetting(QStringLiteral("ADIFTcpServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
    quint16 const adifTcpPort = udpPortFromSettingValue(getSetting(QStringLiteral("ADIFTcpPort"), 52001), 52001);
    bool const n1mmEnabled = getSetting(QStringLiteral("BroadcastToN1MM"), false).toBool();
    QString const n1mmServer = getSetting(QStringLiteral("N1MMServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
    quint16 const n1mmPort = udpPortFromSettingValue(getSetting(QStringLiteral("N1MMServerPort"), 2333), 2333);
    QString clientId = getSetting(QStringLiteral("UDPClientId"), QStringLiteral("WSJTX")).toString().simplified();
    if (clientId.isEmpty()) {
        clientId = QStringLiteral("WSJTX");
    }
    if (clientId.size() > 64) {
        clientId = clientId.left(64).trimmed();
    }

    bridgeLog(QStringLiteral("Reporting config: UDP primary server=%1:%2 listen=%3 clientId=%4 interface=%5 ttl=%6 acceptRequests=%7 loggedADIF=%8")
                  .arg(serverName.trimmed(), QString::number(serverPort), QString::number(listenPort),
                       clientId, interfaceText(interfaces), QString::number(ttl),
                       boolText(acceptUdpRequests), boolText(primaryAdifEnabled)));
    bridgeLog(QStringLiteral("Reporting config: UDP secondary %1 server=%2:%3 listen=ephemeral interface=%4 ttl=%5 loggedADIF=%6")
                  .arg(boolText(secondaryEnabled), secondaryServerName, QString::number(secondaryPort),
                       interfaceText(secondaryInterfaces), QString::number(secondaryTtl),
                       boolText(secondaryAdifEnabled)));
    bridgeLog(QStringLiteral("Reporting config: UDP tertiary %1 server=%2:%3 listen=ephemeral interface=%4 ttl=%5 loggedADIF=%6")
                  .arg(boolText(tertiaryEnabled), tertiaryServerName, QString::number(tertiaryPort),
                       interfaceText(tertiaryInterfaces), QString::number(tertiaryTtl),
                       boolText(tertiaryAdifEnabled)));
    bridgeLog(QStringLiteral("Reporting config: ADIF TCP %1 target=%2:%3")
                  .arg(boolText(adifTcpEnabled), adifTcpServer, QString::number(adifTcpPort)));
    bridgeLog(QStringLiteral("Reporting config: N1MM UDP %1 target=%2:%3")
                  .arg(boolText(n1mmEnabled), n1mmServer, QString::number(n1mmPort)));

    if (usingLegacyBackendForTx()) {
        bridgeLog(QStringLiteral("Standalone UDP MessageClient suppressed: legacy TX backend active"));
        shutdownUdpMessageClient();
        return;
    }

    QString const ver = version();
    QString const rev = revision();

    if (!m_udpMessageClient) {
        m_udpMessageClient = new MessageClient(clientId, ver, rev,
                                               serverName, serverPort,
                                               listenPort, interfaces, ttl,
                                               this, QStringLiteral("primary"));

        m_udpMessageClient->enable(acceptUdpRequests);

        connect(m_udpMessageClient, &MessageClient::error, this, [](const QString& msg) {
            bridgeLog("UDP MessageClient error: " + msg);
        });

        // Handle incoming Reply packets from external apps (GridTracker Call
        // Roster, JTAlert, etc.) as the same user action as a decode double-click.
        connect(m_udpMessageClient, &MessageClient::reply, this,
                [this](QTime time, qint32 snr, float /*dt*/, quint32 df,
                       QString const& /*mode*/, QString const& message, bool /*lowConf*/, quint8 /*mods*/) {
            QString const trimmed = message.trimmed();
            if (trimmed.isEmpty()) {
                return;
            }

            QString const timeStr = time.isValid()
                ? time.toString(QStringLiteral("HHmmss"))
                : QDateTime::currentDateTimeUtc().time().toString(QStringLiteral("HHmmss"));

            bridgeLog("UDP reply accepted: " + trimmed + " df=" + QString::number(df));
            processDecodeDoubleClick(trimmed, timeStr, QString::number(snr), static_cast<int>(df));
        });

        connect(m_udpMessageClient, &MessageClient::halt_tx, this, [this](bool autoOnly) {
            if (!autoOnly) {
                bridgeLog("UDP halt_tx received");
                setTxEnabled(false);
            }
        });

        connect(m_udpMessageClient, &MessageClient::clear_decodes, this, [this](quint8 window) {
            bridgeLog(QStringLiteral("UDP clear_decodes received: window=%1").arg(window));
            if (window == 0 || window == 2) {
                clearDecodeList();
            }
            if (window == 1 || window == 2) {
                clearRxDecodes();
            }
        });

        connect(m_udpMessageClient, &MessageClient::replay, this, [this]() {
            QSet<QString> seen;
            int sent = 0;

            auto replayList = [this, &seen, &sent](QVariantList const& entries) {
                for (QVariant const& value : entries) {
                    QVariantMap const entry = value.toMap();
                    QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
                    if (message.isEmpty()) {
                        continue;
                    }

                    QString key = decodeMirrorEntryKey(entry);
                    if (key.isEmpty()) {
                        key = message;
                    }
                    if (seen.contains(key)) {
                        continue;
                    }
                    seen.insert(key);

                    QString const time = entry.value(QStringLiteral("time")).toString().trimmed();
                    QString const db = entry.value(QStringLiteral("db"), QStringLiteral("0")).toString().trimmed();
                    QString dt = entry.value(QStringLiteral("dt"), QStringLiteral("0.0")).toString().trimmed();
                    if (!dt.contains(QLatin1Char('.'))) {
                        dt += QStringLiteral(".0");
                    }
                    QString const freq = entry.value(QStringLiteral("freq"), QString::number(m_rxFrequency)).toString().trimmed();
                    QString const rawLine = QStringLiteral("%1 %2 %3 %4 ~  %5")
                                                .arg(time, db, dt, freq, message);
                    udpSendDecode(false, rawLine, 0);
                    ++sent;
                }
            };

            replayList(m_decodeList);
            replayList(m_rxDecodeList);
            bridgeLog(QStringLiteral("UDP replay received: sent %1 decodes").arg(sent));
        });

        connect(m_udpMessageClient, &MessageClient::free_text, this,
                [this](QString const& message, bool send) {
            QString const text = message.trimmed();
            if (text.isEmpty()) {
                return;
            }

            bridgeLog(QStringLiteral("UDP free_text received: '%1' send=%2")
                          .arg(text, send ? QStringLiteral("true") : QStringLiteral("false")));
            setTx6(text);
            setCurrentTx(6);
            if (send) {
                setAutoCqRepeat(false);
                clearManualTxHold(QStringLiteral("udp-free-text"));
                setTxEnabled(true);
            }
        });

        connect(m_udpMessageClient, &MessageClient::setup_tx, this,
                [this](int newTxMsgIdx, QString const& msg, bool /*skipGrid*/,
                       bool /*useRR73*/, QString const& /*check*/, quint32 offset) {
            int const tx = qBound(1, newTxMsgIdx, 6);
            QString const text = msg.trimmed();
            bridgeLog(QStringLiteral("UDP setup_tx received: TX%1 msg='%2' offset=%3")
                          .arg(tx)
                          .arg(text)
                          .arg(offset));

            if (!text.isEmpty()) {
                switch (tx) {
                case 1: setTx1(text); break;
                case 2: setTx2(text); break;
                case 3: setTx3(text); break;
                case 4: setTx4(text); break;
                case 5: setTx5(text); break;
                case 6: setTx6(text); break;
                default: break;
                }
            }
            setCurrentTx(tx);
            if (offset > 0 && offset <= 5000) {
                if (m_holdTxFreq) {
                    bridgeLog(QStringLiteral("UDP setup_tx offset ignored because HOLD is active: %1 Hz")
                                  .arg(offset));
                } else {
                    setTxFrequency(static_cast<int>(offset));
                }
            }
        });

        bridgeLog("UDP MessageClient started: server=" + serverName + ":" + QString::number(serverPort)
                  + " listen=" + QString::number(listenPort));
    }

    if (secondaryEnabled && !m_udpSecondaryMessageClient) {
        m_udpSecondaryMessageClient = new MessageClient(clientId, ver, rev,
                                                        secondaryServerName, secondaryPort,
                                                        0, secondaryInterfaces, secondaryTtl,
                                                        this, QStringLiteral("secondary"));
        m_udpSecondaryMessageClient->enable(false);

        connect(m_udpSecondaryMessageClient, &MessageClient::error, this, [](const QString& msg) {
            bridgeLog("Secondary UDP MessageClient error: " + msg);
        });

        bridgeLog("Secondary UDP MessageClient started: server=" + secondaryServerName + ":"
                  + QString::number(secondaryPort) + " listen=0 interface="
                  + interfaceText(secondaryInterfaces) + " ttl="
                  + QString::number(secondaryTtl));
    } else if (!secondaryEnabled && m_udpSecondaryMessageClient) {
        bridgeLog(QStringLiteral("Stopping secondary UDP MessageClient"));
        m_udpSecondaryMessageClient->disconnect(this);
        m_udpSecondaryMessageClient->deleteLater();
        m_udpSecondaryMessageClient = nullptr;
    }

    if (tertiaryEnabled && !m_udpTertiaryMessageClient) {
        m_udpTertiaryMessageClient = new MessageClient(clientId, ver, rev,
                                                       tertiaryServerName, tertiaryPort,
                                                       0, tertiaryInterfaces, tertiaryTtl,
                                                       this, QStringLiteral("tertiary"));
        m_udpTertiaryMessageClient->enable(false);

        connect(m_udpTertiaryMessageClient, &MessageClient::error, this, [](const QString& msg) {
            bridgeLog("Tertiary UDP MessageClient error: " + msg);
        });

        bridgeLog("Tertiary UDP MessageClient started: server=" + tertiaryServerName + ":"
                  + QString::number(tertiaryPort) + " listen=0 interface="
                  + interfaceText(tertiaryInterfaces) + " ttl=" + QString::number(tertiaryTtl));
    } else if (!tertiaryEnabled && m_udpTertiaryMessageClient) {
        bridgeLog(QStringLiteral("Stopping tertiary UDP MessageClient"));
        m_udpTertiaryMessageClient->disconnect(this);
        m_udpTertiaryMessageClient->deleteLater();
        m_udpTertiaryMessageClient = nullptr;
    }

    // Automatically send status updates when key properties change
    connect(this, &DecodiumBridge::frequencyChanged,    this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::modeChanged,         this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::monitoringChanged,   this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::transmittingChanged, this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::decodingChanged,     this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::txEnabledChanged,    this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::callsignChanged,     this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::gridChanged,         this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);
    connect(this, &DecodiumBridge::specialOperationActivityChanged, this, &DecodiumBridge::udpSendStatus, Qt::UniqueConnection);

    // Send initial status
    udpSendStatus();
}

void DecodiumBridge::shutdownUdpMessageClient()
{
    if (!m_udpMessageClient && !m_udpSecondaryMessageClient && !m_udpTertiaryMessageClient) {
        return;
    }
    if (m_udpMessageClient) {
        bridgeLog(QStringLiteral("Stopping standalone UDP MessageClient"));
        m_udpMessageClient->disconnect(this);
        m_udpMessageClient->deleteLater();
        m_udpMessageClient = nullptr;
    }
    if (m_udpSecondaryMessageClient) {
        bridgeLog(QStringLiteral("Stopping secondary UDP MessageClient"));
        m_udpSecondaryMessageClient->disconnect(this);
        m_udpSecondaryMessageClient->deleteLater();
        m_udpSecondaryMessageClient = nullptr;
    }
    if (m_udpTertiaryMessageClient) {
        bridgeLog(QStringLiteral("Stopping tertiary UDP MessageClient"));
        m_udpTertiaryMessageClient->disconnect(this);
        m_udpTertiaryMessageClient->deleteLater();
        m_udpTertiaryMessageClient = nullptr;
    }
}

void DecodiumBridge::scheduleUdpMessageClientRestart()
{
    if (m_udpMessageClientRestartPending) {
        return;
    }

    m_udpMessageClientRestartPending = true;
    QTimer::singleShot(150, this, [this]() {
        m_udpMessageClientRestartPending = false;
        shutdownUdpMessageClient();
        initUdpMessageClient();
    });
}

void DecodiumBridge::udpSendStatus()
{
    if ((!m_udpMessageClient && !m_udpSecondaryMessageClient && !m_udpTertiaryMessageClient) || usingLegacyBackendForTx()) return;

    auto freq = static_cast<MessageClient::Frequency>(qRound64(m_frequency));
    QString const report = m_reportSent.trimmed().isEmpty() ? QStringLiteral("-10")
                                                            : m_reportSent.trimmed().toUpper();
    auto sendStatus = [&](MessageClient* client) {
        if (!client) return;
        client->status_update(
            freq,
            m_mode,                     // mode
            m_dxCall,                   // dx_call
            report,                     // report
            m_mode,                     // tx_mode
            m_txEnabled,                // tx_enabled
            m_transmitting,             // transmitting
            m_decoding,                 // decoding
            static_cast<quint32>(m_rxFrequency),  // rx_df
            static_cast<quint32>(m_txFrequency),  // tx_df
            m_callsign,                 // de_call
            m_grid,                     // de_grid
            m_dxGrid,                   // dx_grid
            false,                      // watchdog_timeout
            QString(),                  // sub_mode
            false,                      // fast_mode
            static_cast<quint8>(qBound(kSpecialOpNone, m_specialOperationActivity, kSpecialOpMax)), // special_op_mode
            0,                          // frequency_tolerance
            static_cast<quint32>(periodMsForMode(m_mode)), // tr_period
            QString(),                  // configuration_name
            m_lastTransmittedMessage,   // lastTxMsg
            static_cast<quint32>(qBound(0, m_qsoProgress, 6)), // qsoProgress
            m_txPeriod != 0,            // txFirst
            false,                      // cqOnly
            QString(),                  // genMsg
            false,                      // txHaltClk
            m_txEnabled,                // txEnableState
            false,                      // txEnableClk
            QString(),                  // myContinent
            false                       // metricUnits
        );
    };

    sendStatus(m_udpMessageClient);
    sendStatus(m_udpSecondaryMessageClient);
    sendStatus(m_udpTertiaryMessageClient);
}

void DecodiumBridge::udpSendDecode(bool isNew, const QString& rawLine, quint64 serial)
{
    if ((!m_udpMessageClient && !m_udpSecondaryMessageClient && !m_udpTertiaryMessageClient) || usingLegacyBackendForTx()) return;

    // Parse the raw decode line: "HHMM  SNR  DT  FREQ ~ MESSAGE"
    QStringList f = parseFt8Row(rawLine);
    if (f.size() < 5) return;

    QString timeStr = f[0].trimmed();
    int snr = f[1].trimmed().toInt();
    float dt = f[2].trimmed().toFloat();
    quint32 df = f.size() > 7 ? f[7].trimmed().toUInt() : f[3].trimmed().toUInt();
    QString message = f[4];
    if (!shouldAcceptDecodedMessage(message)) {
        return;
    }

    // parseFt8Row restituisce timeStr come "" (async), "HHMM" o "HHMMSS".
    // Fino a quando FT2 usa HHMMSS, la branch precedente cadeva su
    // QTime::currentTime() = tempo di PROCESSING, non dello slot → il
    // downstream (GridTracker, JTAlert) riceveva timestamp in ritardo di
    // 1-3s rispetto ai decode provenienti da altri WSJT-X sincroni,
    // da cui il sintomo "UDP lento / tutto fuori tempo" (IW4BFF).
    QTime time;
    if (timeStr.length() == 6) {
        time = QTime(timeStr.left(2).toInt(),
                     timeStr.mid(2, 2).toInt(),
                     timeStr.mid(4, 2).toInt());
    } else if (timeStr.length() == 4) {
        time = QTime(timeStr.left(2).toInt(), timeStr.mid(2, 2).toInt());
    } else {
        // Fallback: usiamo l'UTC corrente (non il local time) perché il
        // resto del protocollo WSJT-X UDP scambia QTime sempre in UTC.
        time = QDateTime::currentDateTimeUtc().time();
    }

    if (m_udpMessageClient) {
        m_udpMessageClient->decode(isNew, time, snr, dt, df, m_mode, message, false, false);
    }
    if (m_udpSecondaryMessageClient) {
        m_udpSecondaryMessageClient->decode(isNew, time, snr, dt, df, m_mode, message, false, false);
    }
    if (m_udpTertiaryMessageClient) {
        m_udpTertiaryMessageClient->decode(isNew, time, snr, dt, df, m_mode, message, false, false);
    }
    Q_UNUSED(serial)
}

void DecodiumBridge::udpSendLoggedQso(const QString& dxCall, const QString& dxGrid,
                                      double freqHz, const QString& mode,
                                      const QDateTime& timeOnUtc,
                                      const QDateTime& timeOffUtc,
                                      const QString& rstSent, const QString& rstRcvd,
                                      const QByteArray& adifRecord,
                                      const QString& comments,
                                      const QString& propMode,
                                      const QString& satellite,
                                      const QString& satMode,
                                      const QString& freqRx)
{
    if (usingLegacyBackendForTx()) {
        return;
    }

    if (!m_udpMessageClient && !m_udpSecondaryMessageClient && !m_udpTertiaryMessageClient) {
        initUdpMessageClient();
    }
    bool const wsjtxUdpAvailable = m_udpMessageClient || m_udpSecondaryMessageClient || m_udpTertiaryMessageClient;
    if (!wsjtxUdpAvailable) {
        bridgeLog(QStringLiteral("UDP logged QSO: WSJT-X MessageClient unavailable; trying raw ADIF targets"));
    }

    QDateTime effectiveTimeOffUtc = timeOffUtc.isValid() ? timeOffUtc : QDateTime::currentDateTimeUtc();
    if (timeOnUtc.isValid() && effectiveTimeOffUtc < timeOnUtc) {
        effectiveTimeOffUtc = timeOnUtc;
    }

    QString const reportSent = rstSent.trimmed().isEmpty() ? QStringLiteral("-10") : rstSent.trimmed();
    QString const reportRcvd = rstRcvd.trimmed().isEmpty() ? QStringLiteral("-10") : rstRcvd.trimmed();
    QString const txPower = m_stationPowerWatts > 0 ? QString::number(m_stationPowerWatts) : QString();
    QString const cleanComments = comments.trimmed();
    QString const cleanPropMode = propMode.trimmed();
    QString const cleanSatellite = satellite.trimmed();
    QString const cleanSatMode = satMode.trimmed();
    QString const cleanFreqRx = freqRx.trimmed();

    int targets = 0;
    int wsjtxAdifTargets = 0;
    int rawAdifTargets = 0;
    auto sendLoggedQso = [&](MessageClient* client, bool sendAdif) {
        if (!client) return;
        client->qso_logged(
            effectiveTimeOffUtc,
            dxCall,
            dxGrid,
            static_cast<MessageClient::Frequency>(qRound64(freqHz)),
            mode,
            reportSent,
            reportRcvd,
            txPower,
            cleanComments,
            QString(),
            timeOnUtc,
            QString(),
            m_callsign,
            m_grid,
            QString(),
            QString(),
            cleanPropMode,
            cleanSatellite,
            cleanSatMode,
            cleanFreqRx);
        if (sendAdif) {
            client->logged_ADIF(adifRecord);
            ++wsjtxAdifTargets;
        }
        ++targets;
    };

    if (wsjtxUdpAvailable) {
        sendLoggedQso(m_udpMessageClient,
                      getSetting(QStringLiteral("UDPPrimaryLoggedAdifEnabled"), true).toBool());
        // D3 used the "secondary UDP / N1MM" path as raw ADIF broadcast, not as
        // a second WSJT-X log stream. Keep that default so secondary consumers
        // see the same commit payload as autolog.
        bool const secondaryEnabled = getSetting(QStringLiteral("UDPSecondaryEnabled"), true).toBool();
        if (secondaryEnabled
            && getSetting(QStringLiteral("UDPSecondaryQsoLoggedEnabled"), false).toBool()) {
            sendLoggedQso(m_udpSecondaryMessageClient, false);
        }
        if (m_udpTertiaryMessageClient) {
            bool const tertiaryAdifEnabled =
                getSetting(QStringLiteral("UDPTertiaryLoggedAdifEnabled"), true).toBool();
            sendLoggedQso(m_udpTertiaryMessageClient, tertiaryAdifEnabled);
        }
    }

    bool const secondaryAdifEnabled = getSetting(QStringLiteral("UDPSecondaryEnabled"), true).toBool()
        && getSetting(QStringLiteral("UDPSecondaryLoggedAdifEnabled"), true).toBool();
    if (secondaryAdifEnabled) {
        QString const secondaryServerName =
            getSetting(QStringLiteral("UDPSecondaryServer"),
                       getSetting(QStringLiteral("UDPServer"), QStringLiteral("127.0.0.1"))).toString().trimmed();
        quint16 const secondaryPort = udpPortFromSettingValue(
            getSetting(QStringLiteral("UDPSecondaryServerPort"), 2239), 2239);
        if (udpSendRawAdifDatagram(QStringLiteral("UDP secondary raw ADIF"),
                                   secondaryServerName, secondaryPort, dxCall, adifRecord)) {
            ++rawAdifTargets;
        }
    }

    bridgeLog(QStringLiteral("UDP logged QSO sent: call=%1 targets=%2 wsjtxAdifTargets=%3 rawAdifTargets=%4 bytes=%5")
                  .arg(dxCall)
                  .arg(targets)
                  .arg(wsjtxAdifTargets)
                  .arg(rawAdifTargets)
                  .arg(adifRecord.size()));
}

bool DecodiumBridge::udpSendRawAdifDatagram(const QString& label,
                                            const QString& serverName,
                                            quint16 port,
                                            const QString& dxCall,
                                            const QByteArray& adifRecord)
{
    QString const cleanServer = serverName.trimmed();
    if (cleanServer.isEmpty() || port == 0 || adifRecord.trimmed().isEmpty()) {
        bridgeLog(QStringLiteral("%1 skipped: missing target or ADIF payload").arg(label));
        return false;
    }

    QHostAddress const target = resolveUdpHostAddress(cleanServer);
    if (target.isNull()) {
        bridgeLog(QStringLiteral("%1 skipped: cannot resolve %2").arg(label, cleanServer));
        return false;
    }

    QUdpSocket socket;
    QByteArray const payload = adifRecord + " <eor>";
    qint64 const written = socket.writeDatagram(payload, target, port);
    if (written < 0) {
        bridgeLog(QStringLiteral("%1 send failed for %2: %3")
                      .arg(label, dxCall, socket.errorString()));
        return false;
    }

    bridgeLog(QStringLiteral("%1 sent: call=%2 target=%3:%4 bytes=%5")
                  .arg(label)
                  .arg(dxCall)
                  .arg(target.toString())
                  .arg(port)
                  .arg(written));
    return true;
}

void DecodiumBridge::udpSendN1mmLoggedQso(const QString& dxCall, const QByteArray& adifRecord)
{
    if (!getSetting(QStringLiteral("BroadcastToN1MM"), false).toBool()) {
        return;
    }

    QString const serverName = getSetting(QStringLiteral("N1MMServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
    quint16 const port = udpPortFromSettingValue(getSetting(QStringLiteral("N1MMServerPort"), 2333), 2333);
    udpSendRawAdifDatagram(QStringLiteral("N1MM raw ADIF"), serverName, port, dxCall, adifRecord);
}

void DecodiumBridge::tcpSendLoggedAdifQso(const QString& dxCall, const QByteArray& adifRecord)
{
    if (!getSetting(QStringLiteral("ADIFTcpEnabled"), false).toBool()) {
        return;
    }

    QString const serverName = getSetting(QStringLiteral("ADIFTcpServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
    quint16 const port = udpPortFromSettingValue(getSetting(QStringLiteral("ADIFTcpPort"), 52001), 52001);
    if (serverName.isEmpty() || adifRecord.trimmed().isEmpty()) {
        bridgeLog(QStringLiteral("ADIF TCP skipped: missing server or empty ADIF record"));
        return;
    }

    auto* socket = new QTcpSocket(this);
    auto* timeout = new QTimer(socket);
    timeout->setSingleShot(true);
    QByteArray const payload = adifRecord + " <EOR>\n";

    socket->setProperty("adifTcpFailed", false);

    connect(timeout, &QTimer::timeout, socket, [socket, serverName, port]() {
        socket->setProperty("adifTcpFailed", true);
        bridgeLog(QStringLiteral("ADIF TCP timeout: target=%1:%2")
                      .arg(serverName, QString::number(port)));
        socket->abort();
    });

    connect(socket, &QTcpSocket::connected, socket, [socket, payload]() {
        qint64 const written = socket->write(payload);
        if (written < 0) {
            socket->setProperty("adifTcpFailed", true);
            bridgeLog(QStringLiteral("ADIF TCP write failed: %1").arg(socket->errorString()));
            socket->abort();
            return;
        }
        socket->flush();
    });

    connect(socket, &QTcpSocket::bytesWritten, socket, [socket](qint64) {
        if (socket->bytesToWrite() == 0) {
            socket->disconnectFromHost();
        }
    });

    connect(socket, &QTcpSocket::disconnected, socket, [socket, dxCall, serverName, port, payload]() {
        if (!socket->property("adifTcpFailed").toBool()) {
            bridgeLog(QStringLiteral("ADIF TCP sent: call=%1 target=%2:%3 bytes=%4")
                          .arg(dxCall, serverName, QString::number(port), QString::number(payload.size())));
        }
        socket->deleteLater();
    });

    connect(socket, &QTcpSocket::errorOccurred, socket,
            [socket, dxCall, serverName, port](QAbstractSocket::SocketError) {
        if (socket->property("adifTcpFailed").toBool()) {
            return;
        }
        socket->setProperty("adifTcpFailed", true);
        bridgeLog(QStringLiteral("ADIF TCP failed: call=%1 target=%2:%3 error=%4")
                      .arg(dxCall, serverName, QString::number(port), socket->errorString()));
        socket->abort();
    });

    timeout->start(3000);
    socket->connectToHost(serverName, port);
}

int DecodiumBridge::remoteWebSocketPort() const
{
    if (m_remoteServer && m_remoteServer->isRunning() && m_remoteServer->wsPort() > 0) {
        return static_cast<int>(m_remoteServer->wsPort());
    }

    QSettings s("Decodium", "Decodium3");
    bool ok = false;
    int wsPort = s.value(QStringLiteral("RemoteWsPort"),
                         s.value(QStringLiteral("WebAppWsPort"), 0)).toInt(&ok);
    if (ok && wsPort > 0) {
        return wsPort;
    }

    int httpPort = s.value(QStringLiteral("RemoteHttpPort"),
                           s.value(QStringLiteral("WebAppHttpPort"), 19091)).toInt(&ok);
    if (ok && httpPort > 1) {
        return httpPort - 1;
    }

    return 19090;
}

QStringList DecodiumBridge::networkInterfaceNames() const
{
    QStringList names;
    for (auto const& netIf : QNetworkInterface::allInterfaces()) {
        auto const flags = netIf.flags();
        bool const isUp = flags.testFlag(QNetworkInterface::IsUp);
        bool const canMulticast = flags.testFlag(QNetworkInterface::CanMulticast);
        bool const isLoopback = flags.testFlag(QNetworkInterface::IsLoopBack);
        bool const usable = (isUp && canMulticast) || isLoopback;
        if (usable) {
            names << netIf.name();
        }
    }
    names.removeDuplicates();
    std::sort(names.begin(), names.end(), [](QString const& a, QString const& b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return names;
}

QString DecodiumBridge::udpInterfaceName() const
{
    // Read from legacy INI (canonical source for Configuration/MessageClient)
    QVariant v = readSettingFromLegacyIni(QStringLiteral("UDPInterface"));
    if (v.isValid()) {
        QStringList interfaces = v.toStringList();
        if (!interfaces.isEmpty()) return interfaces.constFirst();
    }
    return {};
}

void DecodiumBridge::setUdpInterfaceName(const QString& name)
{
    QVariant value = name.trimmed().isEmpty()
        ? QVariant()
        : QVariant(QStringList{name.trimmed()});

    // Write to legacy INI (canonical source)
    syncSettingToLegacyIni(QStringLiteral("UDPInterface"), value);

    // Also keep registry copy for backwards compatibility
    QSettings s("Decodium", "Decodium3");
    if (name.trimmed().isEmpty()) {
        s.remove(QStringLiteral("UDPInterface"));
    } else {
        s.setValue(QStringLiteral("UDPInterface"), QStringList{name.trimmed()});
    }
    s.sync();
    scheduleUdpMessageClientRestart();
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
    s.setValue("SoundInName", m_audioInputDevice);
    s.setValue("SoundOutName", m_audioOutputDevice);
    QString const audioInputChannelSetting = audioChannelSettingValue(m_audioInputChannel);
    QString const audioOutputChannelSetting = audioChannelSettingValue(m_audioOutputChannel);

    QString const configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QDir const configRoot(configDir.isEmpty() ? QDir::homePath() : configDir);
    QString const decodium4IniPath = configRoot.absoluteFilePath(QStringLiteral("decodium4.ini"));
    QString const legacyConfigPath = QFileInfo::exists(decodium4IniPath)
        ? decodium4IniPath
        : configRoot.absoluteFilePath(QStringLiteral("ft2.ini"));
    QSettings legacyIni(legacyConfigPath, QSettings::IniFormat);
    legacyIni.setValue(QStringLiteral("MyCall"), m_callsign.trimmed().toUpper());
    legacyIni.setValue(QStringLiteral("MyGrid"), m_grid.trimmed().toUpper());
    legacyIni.setValue(QStringLiteral("SelectedActivity"), m_specialOperationActivity);
    legacyIni.setValue(QStringLiteral("SpecialOpActivity"), m_specialOperationActivity != kSpecialOpNone);
    legacyIni.sync();
    s.setValue("AudioInputChannel", audioInputChannelSetting);
    s.setValue("AudioOutputChannel", audioOutputChannelSetting);
    syncAudioDeviceSettingsToLegacyIni();
    s.setValue("rxInputLevel", m_rxInputLevel);
    s.setValue("txOutputLevel", m_txOutputLevel);
    s.setValue("nfa", m_nfa);
    s.setValue("nfb", m_nfb);
    s.setValue("ndepth", m_ndepth);
    s.setValue("SelectedActivity", m_specialOperationActivity);
    s.setValue("SpecialOpActivity", m_specialOperationActivity != kSpecialOpNone);
    s.setValue("NDepth", legacyCompatibleDecodeDepthBits());
    s.setValue("decodeDepthMigratedFromLegacy", true);
    s.setValue("tx6", m_tx6);
    s.remove("dxCall");
    s.remove("dxGrid");
    s.remove("tx1");
    s.remove("tx2");
    s.remove("tx3");
    s.remove("tx4");
    s.remove("tx5");
    s.setValue("rxFrequency", m_rxFrequency);
    s.setValue("txFrequency", m_txFrequency);
    s.setValue("fontScale", m_fontScale);
    // extra features
    s.setValue("alertOnCq",       m_alertOnCq);
    s.setValue("alertOnMyCall",   m_alertOnMyCall);
    s.setValue("alert_CQ",        m_alertOnCq);
    s.setValue("alert_MyCall",    m_alertOnMyCall);
    s.setValue("recordRxEnabled", m_recordRxEnabled);
    s.setValue("recordTxEnabled", m_recordTxEnabled);
    s.setValue("StationName",     m_stationName);
    s.setValue("StationQTH",      m_stationQth);
    s.setValue("StationRigInfo",  m_stationRigInfo);
    s.setValue("StationAntenna",  m_stationAntenna);
    s.setValue("StationPowerW",   m_stationPowerWatts);
    s.setValue("MonitorOFF",      false);
    s.setValue("autoSeq",         m_autoSeq);
    s.setValue("AutoSeq",         m_autoSeq);
    s.setValue("startFromTx2",    m_startFromTx2);
    s.setValue("vhfUhfFeatures",  m_vhfUhfFeatures);
    s.setValue("VHFUHF",          m_vhfUhfFeatures);
    s.setValue("directLogQso",    m_directLogQso);
    s.setValue("AutoSpotEnabled", m_autoSpotEnabled);
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
    s.setValue("avgDecodeEnabled", m_avgDecodeEnabled);
    s.setValue("FT8AP",             m_ft8ApEnabled);
    s.setValue("asyncDecodeEnabled",m_asyncDecodeEnabled);
    s.setValue("pskReporterEnabled",m_pskReporterEnabled);
    s.setValue("ftThreads",         m_ftThreads);
    s.setValue("catBackend",        m_catBackend);
    // TX QSO options
    s.remove("reportReceived");
    s.setValue("sendRR73",          m_sendRR73);
    s.setValue("txPeriod",          m_txPeriod);
    s.setValue("alt12Enabled",      m_alt12Enabled);
    s.setValue("quickQsoEnabled",   m_quickQsoEnabled);
    // B7 — Colors
    s.setValue("colorCQ",        m_colorCQ);
    s.setValue("colorMyCall",    m_colorMyCall);
    s.setValue("colorDXEntity",  m_colorDXEntity);
    s.setValue("color73",        m_color73);
    s.setValue("colorB4",        m_colorB4);
    s.setValue("b4Strikethrough",m_b4Strikethrough);
    // B8 — Alerts
    s.setValue("alertSoundsEnabled", m_alertSoundsEnabled);
    s.setValue("alert_Enabled",      m_alertSoundsEnabled);
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
    // CAT managers — persist serial port, baud rate, rig name etc.
    if (m_nativeCat)   m_nativeCat->saveSettings();
    if (m_hamlibCat)   m_hamlibCat->saveSettings();
    if (m_omniRigCat)  m_omniRigCat->saveSettings();
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
    beginDecodeCallbackShutdown();
    halt();
    stopRx();
    teardownAudioCapture();
    if (m_catBackend == "native" && m_nativeCat->connected())
        m_nativeCat->disconnectRig();
    else if (m_catBackend == "omnirig" && m_omniRigCat->connected())
        m_omniRigCat->disconnectRig();
    else if (isHamlibFamilyBackend(m_catBackend) && m_hamlibCat->connected())
        m_hamlibCat->disconnectRig();
    saveSettings();
}

void DecodiumBridge::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void DecodiumBridge::openSetupSettings(int tabIndex)
{
    bridgeLog(QStringLiteral("openSetupSettings: requesting QML setup tab %1").arg(tabIndex));
    emit setupSettingsRequested(tabIndex);
}

void DecodiumBridge::openTimeSyncSettings()
{
    bool const nativeCatWasConnected = (m_catBackend == QStringLiteral("native")
                                        && m_nativeCat && m_nativeCat->connected());
    if (nativeCatWasConnected) {
        bridgeLog(QStringLiteral("openTimeSyncSettings: disconnecting native CAT to free COM port"));
        m_suppressCatErrors = true;
        m_nativeCat->disconnectRig();
    }

    if (ensureLegacyBackendAvailable()) {
        bridgeLog(QStringLiteral("openTimeSyncSettings: delegating to legacy time-sync panel"));
        m_legacyBackend->openTimeSyncSettings();
        m_suppressCatErrors = false;
        reloadBridgeSettingsFromPersistentStore();
        if (usingLegacyBackendForTx()) {
            syncLegacyBackendState();
            syncLegacyBackendTxState();
        }
        // Riconnetti il CAT nativo dopo la chiusura del dialog
        if (nativeCatWasConnected) {
            QTimer::singleShot(0, this, [this]() { retryRigConnection(); });
        }
        return;
    }

    // Se il legacy backend non era disponibile, riconnetti
    if (nativeCatWasConnected) {
        retryRigConnection();
    }
    emit timeSyncSettingsRequested();
}

void DecodiumBridge::syncNtpNow()
{
    if (!m_ntpClient) {
        return;
    }

    if (!m_ntpEnabled) {
        emit statusMessage(QStringLiteral("NTP disabilitato: abilitalo in Impostazioni > Avanzate"));
        bridgeLog(QStringLiteral("NTP sync requested while disabled"));
        return;
    }

    configureNtpClientForMode(m_mode);
    emit statusMessage(m_ntpCustomServer.isEmpty()
                       ? QStringLiteral("NTP sync in corso sui server pubblici...")
                       : QStringLiteral("NTP sync in corso su %1...").arg(m_ntpCustomServer));
    bridgeLog(QStringLiteral("NTP sync requested manually: server=%1")
                  .arg(m_ntpCustomServer.isEmpty() ? QStringLiteral("<auto>") : m_ntpCustomServer));
    m_ntpClient->syncNow();
}

void DecodiumBridge::openCatSettings()
{
    emit catSettingsRequested();
}

void DecodiumBridge::retryRigConnection()
{
    if (legacyOwnsRigControl(m_legacyBackend)) {
        m_legacyBackend->retryRigConnection();
        return;
    }

    if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend)) {
        m_legacyBackend->retryRigConnection();
        return;
    }

    // Prima di tentare connectRig sul backend attivo, assicuriamo che gli
    // altri due backend abbiano rilasciato la porta seriale. Questo evita
    // errori "serial port already open / Access denied" quando l'utente
    // passa da OmniRig a Hamlib (OmniRig.exe tiene la COM via COM object).
    if (m_catBackend != "native"  && m_nativeCat)  m_nativeCat->disconnectRig();
    if (m_catBackend != "omnirig" && m_omniRigCat) m_omniRigCat->disconnectRig();
    if (!isHamlibFamilyBackend(m_catBackend) && m_hamlibCat)  m_hamlibCat->disconnectRig();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

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
    QString const call = callsign.trimmed().toUpper();
    if (call.isEmpty()) return;

    m_pskSearchCallsign = call;
    m_pskSearchFound = false;
    m_pskSearchBands.clear();
    m_pskSearching = true;
    emit pskSearchingChanged();
    emit pskSearchCallsignChanged();
    emit pskSearchFoundChanged();
    emit pskSearchBandsChanged();

    // Query reale a PSK Reporter:
    //   GET https://retrieve.pskreporter.info/query
    //     ?senderCallsign=XXX       (callsign cercato come trasmittente)
    //     &flowStartSeconds=-3600   (ultima ora)
    //     &rptlimit=50              (max 50 spot)
    //     &rronly=1                 (solo reception reports, XML compatto)
    // Risposta: XML con elementi <receptionReport frequency="7074000" ... />.
    // Se almeno uno è presente la stazione è "online" e aggrega le bande.
    QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    QUrl url(QStringLiteral("https://retrieve.pskreporter.info/query"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("senderCallsign"), call);
    q.addQueryItem(QStringLiteral("flowStartSeconds"), QStringLiteral("-3600"));
    q.addQueryItem(QStringLiteral("rptlimit"), QStringLiteral("50"));
    q.addQueryItem(QStringLiteral("rronly"), QStringLiteral("1"));
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Decodium/4.0 (+pskreporter search)");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = nam->get(req);

    // Timeout hard: 10s. Qt 5.15+ ha transferTimeout ma teniamolo esplicito.
    QTimer* timeout = new QTimer(this);
    timeout->setSingleShot(true);
    timeout->setInterval(10000);
    connect(timeout, &QTimer::timeout, reply, [reply]() {
        if (reply && reply->isRunning()) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, timeout, &QTimer::deleteLater);
    timeout->start();

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        QByteArray const data = reply->readAll();
        QNetworkReply::NetworkError const err = reply->error();
        QString const errStr = reply->errorString();
        reply->deleteLater();
        nam->deleteLater();

        bool found = false;
        QSet<QString> bandSet;

        if (err == QNetworkReply::NoError) {
            QString const xml = QString::fromUtf8(data);
            // Parsing via regex: XML di PSK Reporter è deterministico,
            // ogni spot è <receptionReport ... frequency="12345678" ... />
            QRegularExpression re(QStringLiteral("<receptionReport\\b[^>]*\\bfrequency=\"([0-9]+)\""));
            auto it = re.globalMatch(xml);
            while (it.hasNext()) {
                auto m = it.next();
                bool ok = false;
                qint64 const freqHz = m.captured(1).toLongLong(&ok);
                if (!ok) continue;
                found = true;
                double const mhz = freqHz / 1000000.0;
                QString band;
                if (mhz < 2.0)        band = QStringLiteral("160m");
                else if (mhz < 4.5)   band = QStringLiteral("80m");
                else if (mhz < 5.8)   band = QStringLiteral("60m");
                else if (mhz < 8.0)   band = QStringLiteral("40m");
                else if (mhz < 11.0)  band = QStringLiteral("30m");
                else if (mhz < 15.0)  band = QStringLiteral("20m");
                else if (mhz < 19.0)  band = QStringLiteral("17m");
                else if (mhz < 22.0)  band = QStringLiteral("15m");
                else if (mhz < 26.0)  band = QStringLiteral("12m");
                else if (mhz < 30.0)  band = QStringLiteral("10m");
                else if (mhz < 55.0)  band = QStringLiteral("6m");
                else if (mhz < 150.0) band = QStringLiteral("2m");
                else                  band = QString::number(mhz, 'f', 1) + QStringLiteral("MHz");
                bandSet.insert(band);
            }
            bridgeLog(QStringLiteral("PSK Reporter search '%1': found=%2 bands=%3")
                          .arg(m_pskSearchCallsign)
                          .arg(found ? "YES" : "NO")
                          .arg(bandSet.size()));
        } else {
            bridgeLog(QStringLiteral("PSK Reporter search '%1': network error: %2")
                          .arg(m_pskSearchCallsign, errStr));
        }

        // Ordina bande per frequenza crescente (160m → 2m)
        static const QStringList bandOrder {
            QStringLiteral("160m"), QStringLiteral("80m"), QStringLiteral("60m"),
            QStringLiteral("40m"),  QStringLiteral("30m"), QStringLiteral("20m"),
            QStringLiteral("17m"),  QStringLiteral("15m"), QStringLiteral("12m"),
            QStringLiteral("10m"),  QStringLiteral("6m"),  QStringLiteral("2m")
        };
        QStringList bands;
        for (const QString& b : bandOrder) {
            if (bandSet.contains(b)) bands.append(b);
        }
        // Eventuali bande VHF/UHF non nella lista conosciuta
        for (const QString& b : std::as_const(bandSet)) {
            if (!bands.contains(b)) bands.append(b);
        }

        m_pskSearchFound = found;
        m_pskSearchBands = bands;
        m_pskSearching = false;
        emit pskSearchFoundChanged();
        emit pskSearchBandsChanged();
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

    if (m_autoCqRepeat) {
        setAutoCqRepeat(false);
    }
    clearCallerQueue();
    clearAutoCqPartnerLock();
    clearManualTxHold(QStringLiteral("decode-double-click"));

    // Nuovo QSO: resetta contatori retry/watchdog (GitHub handleDoubleClick)
    bool newQso = (hisCall.compare(m_dxCall, Qt::CaseInsensitive) != 0);
    setDxCall(hisCall);
    if (!hisGrid.isEmpty()) setDxGrid(hisGrid);
    else if (!m_dxGrid.isEmpty()) hisGrid = m_dxGrid;

    // Sempre resettare contatori su double-click (anche se stessa stazione)
    m_nTx73 = 0;
    m_txRetryCount = 0;
    m_lastNtx = -1;
    m_lastCqPidx = -1;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;
    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    m_quickPeerSignaled = false;
    m_qsoLogged = false;
    m_lastTransmittedMessage.clear();
    m_qsoStartedOn = QDateTime::currentDateTimeUtc();
    clearPendingAutoLogSnapshot();
    if (newQso) {
        clearAutoCqPartnerLock();
    }

    genStdMsgs(hisCall, hisGrid);
    // advanceQsoState imposta currentTx + qsoProgress + resetta watchdog
    advanceQsoState(newCurrentTx);
    updateAutoCqPartnerLock();

    if (audioFreq > 0) setRxFrequency(audioFreq);

    // Usa il timestamp del decode cliccato per determinare il periodo TX.
    // Nel bridge 1 = first/even (:00/:30), 0 = second/odd (:15/:45),
    // coerente con il backend legacy txFirst.
    int pMs = periodMsForMode(m_mode);
    {
        int msgSecond = -1;
        if (!timeStr.isEmpty()) {
            msgSecond = secondsFromUtcDisplayToken(timeStr);
        }
        if (msgSecond < 0 && normalizeUtcDisplayToken(timeStr).size() == 4) {
            // Solo HHMM: fallback all'orologio corrente.
            msgSecond = QDateTime::currentDateTimeUtc().time().second();
        }

        if (msgSecond >= 0) {
            int dxPidx = (msgSecond * 1000) / pMs;
            // Decodium3/legacy: per un DX nel 2nd/odd slot (:15/:45) dobbiamo
            // mettere txFirst=true -> first/even slot. Quindi il bridge salva
            // direttamente la parita' del DX con la convenzione txFirst.
            m_txPeriod = dxPidx % 2;
        } else {
            qint64 msNow = QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay();
            bool const isEvenPeriod = ((msNow / static_cast<qint64>(pMs)) % 2) == 0;
            m_txPeriod = isEvenPeriod ? 1 : 0;
        }
    }
    emit txPeriodChanged();

    // Abilita TX: doppio click su decode abilita TX automaticamente (quick call behavior)
    // Come Shannon: il doppio click equivale sempre a "Enable TX" + avvio sequenza
    setTxEnabled(true);

    bridgeLog("processDecodeDoubleClick: hisCall=" + hisCall +
              " TX=" + QString::number(newCurrentTx) +
              " progress=" + QString::number(m_qsoProgress) +
              " txPeriod=" + QString::number(m_txPeriod));
    emit statusMessage("QSO con " + hisCall + " — TX" + QString::number(newCurrentTx));

    // Decodium3 recalculated m_bTxTime on every GUI tick, so a double-click just
    // after the UTC boundary could still catch the current TX slot. D4 normally
    // checks only when the boundary changes; run the same guarded check now so
    // we do not miss a valid slot that is already in progress.
    checkAndStartPeriodicTx();
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

    // Quick QSO (Ultra2): TX3 include "TU" per segnalare QSO completo in 2 messaggi
    // Formato: "HIS_CALL MY_CALL R+NN TU" — il DX peer risponde con RR73 direttamente
    if (m_quickQsoEnabled) {
        setTx3(hisCall + " " + my + " R" + rptRcvd + " TU");
    } else {
        setTx3(hisCall + " " + my + " R" + rptRcvd);
    }

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

void DecodiumBridge::rememberCompletedAutoCqPartner(const QString& call, bool logged, const QString& reason)
{
    QString const completedBase = Radio::base_callsign(call).trimmed().toUpper();
    if (completedBase.isEmpty()) {
        return;
    }

    m_qsoCooldown[completedBase] = QDateTime::currentMSecsSinceEpoch();

    if (m_autoCqRepeat) {
        if (logged) {
            rememberRecentAutoCqWorked(completedBase, m_frequency, m_mode);
        } else {
            rememberRecentAutoCqAbandoned(completedBase, m_frequency, m_mode);
        }
        removeCallerFromQueue(completedBase);
    }

    bridgeLog(QStringLiteral("AutoCQ completed partner guard: %1 logged=%2 (%3)")
                  .arg(completedBase)
                  .arg(logged ? 1 : 0)
                  .arg(reason));
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

QString DecodiumBridge::activeMamCallerBase() const
{
    QString activePartner = m_dxCall.trimmed();
    if (activePartner.isEmpty()) {
        activePartner = m_autoCqLockedCall.trimmed();
    }
    if (activePartner.isEmpty()) {
        activePartner = inferredPartnerForAutolog().trimmed();
    }
    return normalizedBaseCall(activePartner);
}

void DecodiumBridge::removeActiveMamCallerFromQueue(const QString& reason)
{
    if (!m_multiAnswerMode || m_callerQueue.isEmpty()) {
        return;
    }

    QString const activeBase = activeMamCallerBase();
    if (activeBase.isEmpty()) {
        return;
    }

    int const before = m_callerQueue.size();
    removeCallerFromQueue(activeBase);
    if (m_callerQueue.size() != before) {
        bridgeLog(QStringLiteral("MAM queue: removed active caller %1 (%2)")
                      .arg(activeBase, reason));
    }
}

void DecodiumBridge::maybeEnqueueMamCallerFromDecode(const QStringList& fields)
{
    if (!m_multiAnswerMode || m_manualTxHold || fields.size() < 5) {
        return;
    }

    QString const msg = fields.value(4).trimmed();
    bool freqOk = false;
    int const freq = fields.size() >= 8 ? fields.value(7).toInt(&freqOk) : 0;
    int const snr = decodeSnrOrDefault(fields.value(1), -99);
    maybeEnqueueMamCallerFromMessage(msg, freqOk ? freq : -1, snr);
}

void DecodiumBridge::maybeEnqueueMamCallerFromMessage(const QString& message, int freq, int snr)
{
    if (!m_multiAnswerMode || m_manualTxHold) {
        return;
    }

    QString const msg = message.trimmed();
    QStringList const parts = msg.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return;
    }

    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);
    if (myCallUpper.isEmpty()) {
        return;
    }

    if (!tokenMatchesCall(parts.first(), myCallUpper, myBaseUpper)) {
        return;
    }

    QString activePartner = m_dxCall.trimmed();
    if (activePartner.isEmpty()) {
        activePartner = m_autoCqLockedCall.trimmed();
    }
    if (activePartner.isEmpty()) {
        activePartner = inferredPartnerForAutolog().trimmed();
    }
    QString const activePartnerBase = normalizedBaseCall(activePartner);

    QString const partner = inferPartnerFromDirectedMessage(msg, myCallUpper, myBaseUpper);
    QString const partnerBase = normalizedBaseCall(partner);
    if (partnerBase.isEmpty() || partnerBase == myBaseUpper) {
        return;
    }
    QString const activeMamBase = activeMamCallerBase();
    if ((!activePartnerBase.isEmpty() && partnerBase == activePartnerBase)
        || (!activeMamBase.isEmpty() && partnerBase == activeMamBase)) {
        removeCallerFromQueue(partnerBase);
        return;
    }

    if (enqueueCallerInternal(partnerBase, freq, snr, false)) {
        bridgeLog(QStringLiteral("MAM queue: enqueued %1 from directed decode while active=%2 msg=%3")
                      .arg(partnerBase, activePartnerBase.isEmpty() ? QStringLiteral("IDLE") : activePartnerBase, msg));
        if (activePartnerBase.isEmpty() && (m_autoCqRepeat || m_txEnabled || m_currentTx == 6)) {
            processNextInQueue();
        }
    }
}

QString DecodiumBridge::inferredPartnerForAutolog() const
{
    QString snapshotCall = m_dxCall.trimmed();
    if (snapshotCall.isEmpty()) {
        snapshotCall = m_autoCqLockedCall.trimmed();
    }
    if (!snapshotCall.isEmpty()) {
        return snapshotCall;
    }

    QString const myCall = m_callsign.trimmed().toUpper();
    QString const myBase = Radio::base_callsign(m_callsign).trimmed().toUpper();
    if (myCall.isEmpty() && myBase.isEmpty()) {
        return {};
    }

    auto const inferFromMessage = [&](QString const& message) {
        return inferPartnerFromDirectedMessage(message.trimmed(), myCall, myBase).trimmed();
    };

    snapshotCall = inferFromMessage(buildCurrentTxMessage());
    if (snapshotCall.isEmpty()) {
        snapshotCall = inferFromMessage(m_lastTransmittedMessage);
    }
    return snapshotCall;
}

void DecodiumBridge::capturePendingAutoLogSnapshot()
{
    QString const snapshotCall = inferredPartnerForAutolog();
    if (snapshotCall.isEmpty()) {
        return;
    }

    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    m_pendingAutoLogValid = true;
    m_pendingAutoLogCall = snapshotCall;
    m_pendingAutoLogGrid = !m_dxGrid.trimmed().isEmpty() ? m_dxGrid.trimmed() : m_autoCqLockedGrid.trimmed();
    m_pendingAutoLogRptSent = m_reportSent.trimmed();
    m_pendingAutoLogRptRcvd = m_reportReceived.trimmed();
    m_pendingAutoLogOn = m_qsoStartedOn.isValid() ? m_qsoStartedOn : nowUtc;
    m_pendingAutoLogOff = nowUtc < m_pendingAutoLogOn ? m_pendingAutoLogOn : nowUtc;
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
    m_pendingAutoLogOff = QDateTime {};
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
    m_lateAutoLogOff = m_pendingAutoLogOff;
    m_lateAutoLogDialFreq = m_pendingAutoLogDialFreq;
    m_lateAutoLogExpires = QDateTime::currentDateTimeUtc().addSecs(kLateAutoLogGraceWindowSeconds);
    rememberRecentAutoCqAbandoned(m_lateAutoLogCall, m_lateAutoLogDialFreq, m_mode);
    bridgeLog(QStringLiteral("late-signoff-arm: %1 until %2")
                  .arg(Radio::base_callsign(m_lateAutoLogCall).trimmed().toUpper(),
                       m_lateAutoLogExpires.toString(Qt::ISODate)));
}

void DecodiumBridge::clearLateAutoLogSnapshot()
{
    m_lateAutoLogValid = false;
    m_lateAutoLogCall.clear();
    m_lateAutoLogGrid.clear();
    m_lateAutoLogRptSent.clear();
    m_lateAutoLogRptRcvd.clear();
    m_lateAutoLogOn = QDateTime {};
    m_lateAutoLogOff = QDateTime {};
    m_lateAutoLogDialFreq = 0.0;
    m_lateAutoLogExpires = QDateTime {};
}

void DecodiumBridge::engageManualTxHold(const QString& reason, bool clearQueue)
{
    if (!m_manualTxHold) {
        bridgeLog(QStringLiteral("manualTxHold: engaged (%1)").arg(reason));
    }
    m_manualTxHold = true;
    clearAutoCqPartnerLock();
    clearPendingAutoLogSnapshot();
    clearLateAutoLogSnapshot();
    m_txRetryCount = 0;
    m_lastNtx = -1;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;
    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    m_quickPeerSignaled = false;
    if (clearQueue) {
        clearCallerQueue();
    }
}

void DecodiumBridge::clearManualTxHold(const QString& reason)
{
    if (!m_manualTxHold) {
        return;
    }
    m_manualTxHold = false;
    bridgeLog(QStringLiteral("manualTxHold: cleared (%1)").arg(reason));
}

bool DecodiumBridge::usesDeferredManualSyncTx() const
{
    return !usingLegacyBackendForTx()
        && (m_mode == QStringLiteral("FT8")
            || m_mode == QStringLiteral("FT4")
            || (m_mode == QStringLiteral("FT2") && !m_asyncTxEnabled));
}

bool DecodiumBridge::shouldDeferManualSyncTxStart() const
{
    if (!usesDeferredManualSyncTx() || !m_monitoring) {
        return false;
    }

    if (m_transmitting) {
        return true;
    }

    if (m_tuning) {
        return false;
    }

    int const periodMs = periodMsForMode(m_mode);
    int const payloadMs = estimatedSyncPayloadMs(m_mode);
    if (periodMs <= 0 || payloadMs <= 0) {
        return false;
    }

    qint64 const msNow = QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay();
    qint64 const slotIndex = msNow / static_cast<qint64>(periodMs);
    bool const isEvenPeriod = ((slotIndex % 2) == 0);
    bool const isOurPeriod = bridgeTxPeriodIsEven(m_txPeriod) ? isEvenPeriod
                                                              : !isEvenPeriod;
    if (!isOurPeriod) {
        return true;
    }

    int const remainingMs = periodMs - static_cast<int>(msNow % static_cast<qint64>(periodMs));
    int const requiredMs = payloadMs + syncTxStartBudgetMs(m_mode);
    return remainingMs <= requiredMs;
}

bool DecodiumBridge::tryStartDeferredManualSyncTx()
{
    if (!m_deferredManualSyncTx) {
        return false;
    }

    if (!usesDeferredManualSyncTx()) {
        clearDeferredManualSyncTx(QStringLiteral("unsupported-mode"));
        return false;
    }

    if (!m_monitoring || m_transmitting || m_tuning) {
        return false;
    }

    int const periodMs = periodMsForMode(m_mode);
    if (periodMs <= 0) {
        clearDeferredManualSyncTx(QStringLiteral("invalid-period"));
        return false;
    }

    qint64 const msNow = QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay();
    qint64 const slotIndex = msNow / static_cast<qint64>(periodMs);
    bool const isEvenPeriod = ((slotIndex % 2) == 0);
    bool const isOurPeriod = bridgeTxPeriodIsEven(m_txPeriod) ? isEvenPeriod
                                                              : !isEvenPeriod;
    if (!isOurPeriod) {
        return false;
    }

    int const elapsedMs = static_cast<int>(msNow % static_cast<qint64>(periodMs));
    int const remainingMs = periodMs - elapsedMs;
    int const payloadMs = estimatedSyncPayloadMs(m_mode);
    int const requiredMs = payloadMs + syncTxStartBudgetMs(m_mode);
    if (requiredMs > 0 && remainingMs <= requiredMs) {
        bridgeLog(QStringLiteral("manualSyncTx: slot too late for queued TX%1 remaining=%2ms required=%3ms")
                      .arg(m_currentTx)
                      .arg(remainingMs)
                      .arg(requiredMs));
        return false;
    }

    clearDeferredManualSyncTx(QStringLiteral("slot-ready"));
    bridgeLog(QStringLiteral("manualSyncTx: starting queued TX%1 at slot boundary")
                  .arg(m_currentTx));
    startTx();
    return true;
}

void DecodiumBridge::clearDeferredManualSyncTx(const QString& reason)
{
    if (!m_deferredManualSyncTx) {
        return;
    }
    m_deferredManualSyncTx = false;
    bridgeLog(QStringLiteral("manualSyncTx: cleared (%1)").arg(reason));
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
    if (m_manualTxHold || !m_autoCqRepeat || m_autoCqLockedCall.trimmed().isEmpty()) {
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

bool DecodiumBridge::enqueueCallerInternal(const QString& call,
                                           int freq,
                                           int snr,
                                           bool respectRecentDuplicateGuard)
{
    QString const queuedCall = Radio::base_callsign(call).trimmed().toUpper();
    if (queuedCall.isEmpty()) {
        return false;
    }
    QString const activeBase = m_multiAnswerMode ? activeMamCallerBase() : QString {};
    if (!activeBase.isEmpty() && queuedCall == activeBase) {
        bridgeLog(QStringLiteral("MAM queue-skip-active: %1").arg(queuedCall));
        return false;
    }
    if (respectRecentDuplicateGuard && isRecentAutoCqDuplicate(queuedCall, m_frequency, m_mode)) {
        bridgeLog(QStringLiteral("AutoCQ queue-skip-recent: %1").arg(queuedCall));
        return false;
    }

    for (QString const& entry : m_callerQueue) {
        QStringList const parts = entry.split(' ', Qt::SkipEmptyParts);
        if (!parts.isEmpty() &&
            Radio::base_callsign(parts.first()).trimmed().compare(queuedCall, Qt::CaseInsensitive) == 0) {
            bridgeLog(QStringLiteral("AutoCQ queue-skip-duplicate: %1").arg(queuedCall));
            return false;
        }
    }

    if (m_callerQueue.size() >= FOX_QUEUE_MAX) {
        bridgeLog(QStringLiteral("AutoCQ queue-skip-full: %1").arg(queuedCall));
        return false;
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
    return true;
}

// === advanceQsoState — GitHub TxController clone ===
// Mappa TX button number → QSOProgress enum, resetta watchdog
// 0=IDLE, 1=CALLING_CQ, 2=REPLYING, 3=REPORT, 4=ROGER_REPORT, 5=SIGNOFF, 6=IDLE_QSO
void DecodiumBridge::advanceQsoState(int txNum)
{
    // Quick QSO (Ultra2): salta TX1 → vai diretto a TX2 (come FT2QsoFlowPolicy Ultra2)
    if (txNum == 1 && m_quickQsoEnabled) {
        bridgeLog("advanceQsoState: QuickQSO attivo → salta TX1, va a TX2 (Ultra2)");
        txNum = 2;
    }

    // Quick QSO (Ultra2): TX3 contiene "R+report TU" → dopo TX3 il QSO e' finito
    // Salta TX4/TX5 e va diretto a SIGNOFF con log automatico
    if (txNum == 3 && m_quickQsoEnabled) {
        bridgeLog("advanceQsoState: QuickQSO TX3 con TU → SIGNOFF diretto");
        setCurrentTx(txNum);
        m_qsoProgress = 5; // SIGNOFF
        emit qsoProgressChanged();
        m_txWatchdogTicks = 0;
        return;
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
    if (m_manualTxHold || !m_monitoring || m_transmitting || m_tuning) return;
    if (!m_txEnabled && !m_autoCqRepeat) return;
    if (m_autoCqRepeat && m_dxCall.isEmpty() && m_currentTx != 6) {
        restoreAutoCqPartnerLock();
    }

    auto finishAutoSequenceQso = [&](QString const& reason, bool logNow) {
        bridgeLog(reason);
        QString completedPartner = inferredPartnerForAutolog();
        if (logNow) {
            capturePendingAutoLogSnapshot();
            if (completedPartner.trimmed().isEmpty() && m_pendingAutoLogValid) {
                completedPartner = m_pendingAutoLogCall;
            }
            rememberCompletedAutoCqPartner(completedPartner, true, reason);
            logQso();
        } else {
            rememberCompletedAutoCqPartner(completedPartner, false, reason);
        }
        bool const continueQueuedCaller = m_multiAnswerMode && !m_callerQueue.isEmpty();
        bool const continueAutoCq = m_autoCqRepeat && !m_tx6.isEmpty();
        bool const preserveAutoTx = continueQueuedCaller || continueAutoCq;
        m_qsoLogged = false;  // reset per il prossimo QSO
        m_logAfterOwn73 = false;
        m_ft2DeferredLogPending = false;
        m_quickPeerSignaled = false;
        if (!preserveAutoTx) {
            setTxEnabled(false);
        }
        if (m_qsoProgress != 6) { m_qsoProgress = 6; emit qsoProgressChanged(); } // IDLE_QSO
        m_nTx73 = 0;
        m_txRetryCount = 0;
        m_lastNtx = -1;
        m_lastCqPidx = -1;
        clearAutoCqPartnerLock();
        setDxCall(QString());
        setDxGrid(QString());
        if (continueQueuedCaller) {
            processNextInQueue();
        } else if (continueAutoCq) {
            advanceQsoState(6);
            if (!m_txEnabled) {
                setTxEnabled(true);
            }
            startTx();
        }
    };

    auto completeFinishedSignoffIfReady = [&]() -> bool {
        QString const selectedPayload = buildCurrentTxMessage().trimmed();
        bool const signoffPayload =
            (m_currentTx == 5) || messageCarries73Payload(selectedPayload);
        bool const localSignoffAlreadyStarted =
            (m_nTx73 >= 1)
            || messageCarries73Payload(m_lastTransmittedMessage)
            || (m_lastNtx == m_currentTx && signoffPayload);
        if (!(m_txEnabled && signoffPayload && localSignoffAlreadyStarted)) {
            return false;
        }

        if (m_logAfterOwn73) {
            finishAutoSequenceQso(QStringLiteral("checkAndStartPeriodicTx: completed own 73 → QSO complete"), true);
            return true;
        }

        if (m_ft2DeferredLogPending) {
            if (m_nTx73 < m_maxCallerRetries) {
                bridgeLog(QStringLiteral("checkAndStartPeriodicTx: deferred signoff TX%1 count=%2/%3, waiting for final ack")
                              .arg(m_currentTx)
                              .arg(m_nTx73)
                              .arg(m_maxCallerRetries));
                return false;
            }
            finishAutoSequenceQso(QStringLiteral("checkAndStartPeriodicTx: deferred signoff TX%1 retry limit %2/%3 -> move on without log")
                                      .arg(m_currentTx)
                                      .arg(m_nTx73)
                                      .arg(m_maxCallerRetries), false);
            return true;
        }

        finishAutoSequenceQso("checkAndStartPeriodicTx: completed signoff count=" + QString::number(m_nTx73) +
                              " → QSO completo (TX" + QString::number(m_currentTx) + " sent)", true);
        return true;
    };

    if (completeFinishedSignoffIfReady()) {
        return;
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
        if (pMs <= 0) return;
        int pidx = (int)(msNow / (qint64)pMs);
        bool const isEven = (pidx % 2 == 0);
        bool const isOurPeriod = bridgeTxPeriodIsEven(m_txPeriod) ? isEven : !isEven;
        if (!isOurPeriod) return;

        int const elapsedMs = static_cast<int>(msNow % static_cast<qint64>(pMs));
        int const remainingMs = pMs - elapsedMs;
        int const payloadMs = estimatedSyncPayloadMs(m_mode);
        int const requiredMs = payloadMs + syncTxStartBudgetMs(m_mode);
        if (requiredMs > 0 && remainingMs <= requiredMs) {
            bridgeLog(QStringLiteral("checkAndStartPeriodicTx: too late in %1 slot, defer TX%2 remaining=%3ms required=%4ms")
                          .arg(m_mode)
                          .arg(m_currentTx)
                          .arg(remainingMs)
                          .arg(requiredMs));
            return;
        }

        // Guardia anti-CQ-consecutivi: dopo un CQ aspetta almeno N periodi di pausa RX
        // FT2 (3.75s): 4 periodi = ~15s di pausa per decodificare risposte async
        // FT8 (15s): 2 periodi = ~30s (standard)
        bool isCqTx = (m_autoCqRepeat && !m_txEnabled) ||
                      (m_txEnabled && m_currentTx == 6);
        int cqGuardPeriods = (m_mode == "FT2") ? 4 : 2;
        if (isCqTx && m_lastCqPidx >= 0 && (pidx - m_lastCqPidx) < cqGuardPeriods) {
            bridgeLog("checkAndStartPeriodicTx: CQ guard — pidx=" + QString::number(pidx) +
                      " lastCqPidx=" + QString::number(m_lastCqPidx) + " → pausa RX forzata");
            return;
        }
    } else {
        // FT2 async guard dopo TX:
        // - 200ms PTT debounce (sempre)
        // - Se è un RETRY dello stesso step: attendi 1 periodo per dare tempo al decode
        qint64 msNow = QDateTime::currentMSecsSinceEpoch();
        if (m_asyncLastTxEndMs > 0) {
            qint64 elapsed = msNow - m_asyncLastTxEndMs;
            if (elapsed < 200) return;  // PTT debounce
            // Retry guard: se stiamo per rimandare lo stesso step, aspetta 1 periodo
            if (m_currentTx == m_lastNtx && elapsed < periodMsForMode(m_mode)) return;
        }
    }

    QString const selectedTxPayload = buildCurrentTxMessage().trimmed();
    if (m_txEnabled && !selectedTxPayload.isEmpty()) {
        if (m_logAfterOwn73 && m_currentTx != 5 && !m_tx5.isEmpty()) {
            bridgeLog(QStringLiteral("checkAndStartPeriodicTx: own 73 pending -> force TX5 (current TX%1)")
                          .arg(m_currentTx));
            advanceQsoState(5);
            updateAutoCqPartnerLock();
        }

        if (m_currentTx == 4 && m_quickQsoEnabled && m_quickPeerSignaled) {
            finishAutoSequenceQso(QStringLiteral("checkAndStartPeriodicTx: Quick QSO signoff complete after TX4"), true);
            return;
        }

        // Qualsiasi payload di signoff (RR73/RRR/73) completato >=1 volta chiude il QSO.
        if (completeFinishedSignoffIfReady()) {
            return;
        }

        // Retry limit: vale sia per AutoCQ sia per i QSO manuali avviati con doppio click.
        bool isCqAutoCq = (m_currentTx == 6 && m_autoCqRepeat);
        bool const manualPartnerQso =
            m_txEnabled
            && !m_autoCqRepeat
            && !m_dxCall.trimmed().isEmpty()
            && m_currentTx != 6;
        bool applyRetryLimit =
            (m_autoCqRepeat || manualPartnerQso)
            && !isCqAutoCq
            && !(m_currentTx == 5 && m_ft2DeferredLogPending);
        if (applyRetryLimit && m_currentTx == m_lastNtx && m_txRetryCount >= m_maxCallerRetries) {
            bridgeLog("checkAndStartPeriodicTx: retry limit " +
                      QString::number(m_maxCallerRetries) + " su TX" +
                      QString::number(m_currentTx) + " → halt");
            if (m_autoCqRepeat && m_qsoProgress > 1) {
                armLateAutoLogSnapshot();
            }
            bool const continueQueuedCaller = m_multiAnswerMode && !m_callerQueue.isEmpty();
            bool const continueAutoCq = m_autoCqRepeat && !m_tx6.isEmpty();
            // IMPORTANT: do not pulse TX off while AutoCQ is meant to continue.
            // On the legacy backend, setTxEnabled(false) also clears AutoCQ/Auto.
            if (!continueQueuedCaller && !continueAutoCq) {
                setTxEnabled(false);
            }
            m_txRetryCount = 0;
            m_lastNtx = -1;
            m_nTx73    = 0;
            m_logAfterOwn73 = false;
            m_ft2DeferredLogPending = false;
            m_quickPeerSignaled = false;
            m_lastCqPidx = -1;
            clearAutoCqPartnerLock();
            // clearDX: azzera dxCall (stazione non raggiungibile)
            setDxCall(QString());
            setDxGrid(QString());
            if (continueQueuedCaller) {
                processNextInQueue();
            } else if (continueAutoCq) {
                ++m_autoCqCycleCount;
                bridgeLog("AutoCQ cycle " + QString::number(m_autoCqCycleCount) +
                          " / " + (m_autoCqMaxCycles > 0 ? QString::number(m_autoCqMaxCycles) : "inf"));
                if (m_autoCqMaxCycles > 0 && m_autoCqCycleCount >= m_autoCqMaxCycles) {
                    bridgeLog("AutoCQ: max cycles reached, stopping");
                    setAutoCqRepeat(false);
                    setTxEnabled(false);
                    return;
                }
                if (m_autoCqPauseSec > 0) {
                    bridgeLog("AutoCQ: pausing " + QString::number(m_autoCqPauseSec) + "s before next CQ");
                    QTimer::singleShot(m_autoCqPauseSec * 1000, this, [this]() {
                        if (m_autoCqRepeat && !m_transmitting && !m_tuning) {
                            advanceQsoState(6);
                            if (!m_txEnabled) {
                                setTxEnabled(true);
                            }
                            startTx();
                        }
                    });
                } else {
                    advanceQsoState(6);  // CQ
                    if (!m_txEnabled) {
                        setTxEnabled(true);
                    }
                    startTx();
                }
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
    QString semanticRejectReason;
    if (!shouldAcceptDecodedMessage(msg, &semanticRejectReason)) {
        bridgeLog(QStringLiteral("autoSequenceStep: semantic reject (%1): %2")
                      .arg(semanticRejectReason, msg));
        return;
    }
    QStringList parts = msg.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() < 2) return;

    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = Radio::base_callsign(m_callsign).trimmed().toUpper();
    bool const directedToMe =
        !myCallUpper.isEmpty() &&
        (parts[0].compare(myCallUpper, Qt::CaseInsensitive) == 0 ||
         (!myBaseUpper.isEmpty() &&
          parts[0].compare(myBaseUpper, Qt::CaseInsensitive) == 0));

    // Decodium3/legacy behavior: during FT8/FT4 sync TX, we still have to arm
    // auto-reply as soon as a directed call to us is decoded from our CQ.
    // Keep blocking unrelated/self decodes while TX is still winding down.
    if (m_transmitting && !(m_mode == "FT2" && m_asyncTxEnabled) && !directedToMe) {
        bridgeLog("autoSequenceStep: skipped during TX (not directed): " + msg);
        return;
    }

    // Pulizia periodica cooldown map (previene memory leak su sessioni lunghe)
    {
        qint64 nowPurge = QDateTime::currentMSecsSinceEpoch();
        for (auto it = m_qsoCooldown.begin(); it != m_qsoCooldown.end(); )
            (nowPurge - it.value() > 30000) ? it = m_qsoCooldown.erase(it) : ++it;
    }

    // Anti-collisione: deduplicazione — ignora decode identici processati di recente.
    // FT2 async: il time cambia ad ogni decode, quindi deduplicare solo sul messaggio.
    // FT8 sync: time+message per permettere stesso messaggio in periodi diversi.
    {
        bool isFt2Async = (m_mode == "FT2" && m_asyncTxEnabled);
        QString dedupeKey = isFt2Async ? f[4] : (f[0] + "|" + f[4]);
        int dedupeWindowMs = isFt2Async ? 2000 : 4000;  // FT2: 2s (dedup veloce), FT8: 4s
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastAutoSeqKey == dedupeKey && (now - m_lastAutoSeqMs) < dedupeWindowMs) {
            bridgeLog("autoSequenceStep: dedupe skip: " + f[4]);
            return;
        }
        m_lastAutoSeqKey = dedupeKey;
        m_lastAutoSeqMs = now;
    }
    if (m_manualTxHold) return;

    // Estrai il mittente: TO_CALL FROM_CALL ...
    QString from;
    if (!myCallUpper.isEmpty() &&
        parts[0].compare(myCallUpper, Qt::CaseInsensitive) == 0) {
        from = parts[1];
    } else {
        from = parts[0];
    }
    QString const directedPartner = inferPartnerFromDirectedMessage(msg, myCallUpper, myBaseUpper);
    QString fallbackDirectedPartner = directedPartner;
    if (fallbackDirectedPartner.isEmpty()
        && directedToMe
        && parts.size() >= 2
        && isPlaceholderCallToken(parts[1])
        && !m_dxCall.trimmed().isEmpty()) {
        fallbackDirectedPartner = m_dxCall.trimmed();
        bridgeLog(QStringLiteral("autoSeq: placeholder partner resolved to active DX %1 for msg=%2")
                      .arg(fallbackDirectedPartner, msg));
    }
    QString const messagePartner = fallbackDirectedPartner.isEmpty() ? from : fallbackDirectedPartner;
    QString const messagePartnerBase = Radio::base_callsign(messagePartner).trimmed().toUpper();

    // QSO cooldown: ignora 73/RR73 ripetuti da stazione con cui il QSO è chiuso
    // Previene loop su decode ripetuti dello stesso messaggio finale
    QString last_word = parts.last();
    bool is_73 = (last_word.compare("73",   Qt::CaseInsensitive) == 0 ||
                  last_word.compare("RR73",  Qt::CaseInsensitive) == 0 ||
                  last_word.compare("RRR",   Qt::CaseInsensitive) == 0);
    QString const cooldownKey = !messagePartnerBase.isEmpty()
        ? messagePartnerBase
        : Radio::base_callsign(from).trimmed().toUpper();
    if (is_73 && !cooldownKey.isEmpty()) {
        qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        int cooldownMs = (m_mode == "FT2" && m_asyncTxEnabled) ? 8000 : 20000;
        // purge entries scadute
        for (auto it = m_qsoCooldown.begin(); it != m_qsoCooldown.end(); ) {
            if (nowMs - it.value() > cooldownMs) it = m_qsoCooldown.erase(it);
            else ++it;
        }
        if (m_qsoCooldown.contains(cooldownKey)) {
            bridgeLog("QSO cooldown: ignoro " + last_word + " da " + cooldownKey);
            return;
        }
    }

    QString const latePartnerBase = Radio::base_callsign(m_lateAutoLogCall).trimmed().toUpper();
    bool const latePartnerWindowOpen =
        m_lateAutoLogValid
        && m_lateAutoLogExpires.isValid()
        && QDateTime::currentDateTimeUtc() <= m_lateAutoLogExpires
        && !latePartnerBase.isEmpty();
    bool const fromLatePartner =
        latePartnerWindowOpen
        && !messagePartnerBase.isEmpty()
        && messagePartnerBase == latePartnerBase;
    if (fromLatePartner && is_73) {
        m_pendingAutoLogValid = true;
        m_pendingAutoLogCall = m_lateAutoLogCall;
        m_pendingAutoLogGrid = m_lateAutoLogGrid;
        m_pendingAutoLogRptSent = m_lateAutoLogRptSent;
        m_pendingAutoLogRptRcvd = m_lateAutoLogRptRcvd;
        m_pendingAutoLogOn = m_lateAutoLogOn;
        m_pendingAutoLogOff = m_lateAutoLogOff;
        m_pendingAutoLogDialFreq = m_lateAutoLogDialFreq;
        removeCallerFromQueue(latePartnerBase);
        bridgeLog("late-signoff-log: " + latePartnerBase + " msg=" + msg);
        clearLateAutoLogSnapshot();
        m_qsoLogged = false;
        logQso();
        return;
    }
    if (fromLatePartner) {
        removeCallerFromQueue(latePartnerBase);
        bridgeLog("late-signoff-skip: " + latePartnerBase + " msg=" + msg);
        return;
    }

    if (m_autoCqRepeat && !messagePartnerBase.isEmpty()
        && isRecentAutoCqDuplicate(messagePartnerBase, m_frequency, m_mode)) {
        bridgeLog(QStringLiteral("autoSeq: skip recently completed AutoCQ partner %1 msg=%2")
                      .arg(messagePartnerBase, msg));
        removeCallerFromQueue(messagePartnerBase);
        return;
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
        if (myCallUpper.isEmpty() ||
            parts[0].compare(myCallUpper, Qt::CaseInsensitive) != 0) return;
    } else {
        // Non in CQ mode: nessun dxCall → non rispondere automaticamente (Shannon: clearDX)
        if (m_dxCall.isEmpty()) {
            restoreAutoCqPartnerLock();
        }
        if (m_dxCall.isEmpty()) return;
        QString const activeDxBase = Radio::base_callsign(m_dxCall).trimmed().toUpper();
        // Rispondo solo dalla stazione DX corrente
        if (messagePartnerBase.isEmpty() || activeDxBase.isEmpty() || messagePartnerBase != activeDxBase) {
            // Stazione diversa: in multi-answer mode, aggiunge alla coda se ci sta chiamando
            // (parts[0] == m_callsign indica che il messaggio è indirizzato a noi)
            if (m_multiAnswerMode && !messagePartner.isEmpty() && !myCallUpper.isEmpty() &&
                tokenMatchesCall(parts[0], myCallUpper, myBaseUpper)) {
                int freq = f.size() >= 8 ? f[7].toInt() : m_rxFrequency;
                int snr = f.size() >= 2 ? f[1].toInt() : -99;
                if (enqueueCallerInternal(messagePartner, freq, snr, false)) {
                    bridgeLog("autoSeq: multi-answer: enqueue " + messagePartner + " (in QSO con " + m_dxCall + ")");
                }
            }
            return;
        }

        // Decodium3 behavior: any directed payload from the active partner means
        // the QSO is still alive, even if the token parser does not advance state.
        // Once we have reached signoff, old reports from the same station must not
        // refresh the retry window or the sequencer can be kept alive forever.
        bool const signoffStageAlreadyActive =
            m_qsoProgress >= 5
            || m_currentTx == 4
            || m_currentTx == 5
            || messageCarries73Payload(buildCurrentTxMessage())
            || messageCarries73Payload(m_lastTransmittedMessage)
            || m_ft2DeferredLogPending
            || m_logAfterOwn73;
        if (!signoffStageAlreadyActive) {
            m_txRetryCount = 0;
        }
        m_txWatchdogTicks = 0;
        m_autoCQPeriodsMissed = 0;
    }

    QString last = parts.last();
    int nextTx = -1;

    auto finishAutoSequenceQso = [&](QString const& reason) {
        bridgeLog(reason);
        QString completedPartner = inferredPartnerForAutolog();
        capturePendingAutoLogSnapshot();
        if (completedPartner.trimmed().isEmpty() && m_pendingAutoLogValid) {
            completedPartner = m_pendingAutoLogCall;
        }
        rememberCompletedAutoCqPartner(completedPartner, true, reason);
        logQso();
        bool const continueQueuedCaller = m_multiAnswerMode && !m_callerQueue.isEmpty();
        bool const continueAutoCq = m_autoCqRepeat && !m_tx6.isEmpty();
        bool const preserveAutoTx = continueQueuedCaller || continueAutoCq;
        m_qsoLogged = false;  // reset per il prossimo QSO
        m_logAfterOwn73 = false;
        m_ft2DeferredLogPending = false;
        m_quickPeerSignaled = false;
        if (!preserveAutoTx) {
            setTxEnabled(false);
        }
        if (m_qsoProgress != 6) { m_qsoProgress = 6; emit qsoProgressChanged(); } // IDLE_QSO
        m_nTx73 = 0;
        m_txRetryCount = 0;
        m_lastNtx = -1;
        m_lastCqPidx = -1;
        clearAutoCqPartnerLock();
        setDxCall(QString());
        setDxGrid(QString());
        if (continueQueuedCaller) {
            processNextInQueue();
        } else if (continueAutoCq) {
            advanceQsoState(6);
            if (!m_txEnabled) {
                setTxEnabled(true);
            }
        }
    };

    bool const localSignoffAlreadySent =
        (m_nTx73 > 0)
        || messageCarries73Payload(m_lastTransmittedMessage)
        || (m_lastNtx == 5 && m_qsoProgress >= 5);
    bool const partnerSignoff73 =
        (last.compare(QStringLiteral("73"), Qt::CaseInsensitive) == 0
         && m_qsoProgress >= 4);
    bool const partnerSignoffRR73 =
        (last.compare(QStringLiteral("RR73"), Qt::CaseInsensitive) == 0
         && m_qsoProgress >= 4);
    bool const partnerAnySignoff = partnerSignoff73 || partnerSignoffRR73;
    bool const profileNeedsOwn73BeforeLog =
        ((m_mode == QStringLiteral("FT2") && !m_quickQsoEnabled)
         || m_mode == QStringLiteral("FT8")
         || m_mode == QStringLiteral("FT4"));
    bool const partnerSignoffNeedsOwn73 =
        partnerAnySignoff
        && !localSignoffAlreadySent
        && profileNeedsOwn73BeforeLog;
    bool const partnerFinalAck =
        partnerAnySignoff && !partnerSignoffNeedsOwn73;

    bool const localSignoffStageActive =
        m_qsoProgress >= 5
        || m_currentTx == 4
        || m_currentTx == 5
        || messageCarries73Payload(buildCurrentTxMessage())
        || messageCarries73Payload(m_lastTransmittedMessage)
        || m_ft2DeferredLogPending
        || m_logAfterOwn73;
    bool const stalePreSignoffPayload =
        !partnerAnySignoff
        && (isGridTokenStrict(last)
            || isPlainSignalReportToken(last)
            || isRogerSignalReportToken(last)
            || last.compare(QStringLiteral("TU"), Qt::CaseInsensitive) == 0);
    if (localSignoffStageActive && stalePreSignoffPayload) {
        int const keepTx = (m_currentTx == 5 || m_logAfterOwn73) ? 5 : 4;
        if (m_currentTx != keepTx) {
            advanceQsoState(keepTx);
        }
        setTxEnabled(true);
        updateAutoCqPartnerLock();
        bridgeLog(QStringLiteral("autoSeq: signoff monotonic guard keeps TX%1, ignoring stale payload from %2: %3")
                      .arg(keepTx)
                      .arg(messagePartner.isEmpty() ? from : messagePartner)
                      .arg(msg));
        return;
    }

    if (m_logAfterOwn73 && !partnerAnySignoff) {
        bridgeLog("autoSeq: own 73 pending, ignore non-final payload from " +
                  (messagePartner.isEmpty() ? from : messagePartner) + ": " + msg);
        return;
    }

    if (inCqMode && !messagePartner.isEmpty()) {
        QString const callerBase = messagePartnerBase;
        if (callerBase.isEmpty()) {
            return;
        }

        if (m_dxCall.trimmed().isEmpty() && isRecentAutoCqDuplicate(callerBase, m_frequency, m_mode)) {
            bridgeLog("autoSeq: skip recent AutoCQ caller " + callerBase);
            return;
        }

        if (Radio::base_callsign(m_dxCall).trimmed().compare(callerBase, Qt::CaseInsensitive) != 0) {
            setDxCall(messagePartner);
            m_qsoStartedOn = QDateTime::currentDateTimeUtc();
            m_lastTransmittedMessage.clear();
            if (!isGridTokenStrict(last)) {
                setDxGrid(QString());
            }
        } else if (!m_qsoStartedOn.isValid()) {
            m_qsoStartedOn = QDateTime::currentDateTimeUtc();
        }

        m_txRetryCount = 0;
        m_lastNtx = -1;
        m_nTx73 = 0;
        m_logAfterOwn73 = false;
        m_ft2DeferredLogPending = false;
        m_quickPeerSignaled = false;
        m_lastCqPidx = -1;
        m_autoCQPeriodsMissed = 0;
        m_txWatchdogTicks = 0;
        setReportSent(QString::number(decodeSnrOrDefault(f.value(1), -10)));

        if (isGridTokenStrict(last)) {
            setDxGrid(last.toUpper());
        }

        bridgeLog("autoSeq: start AutoCQ partner " + callerBase + " msg=" + msg);
    }

    if (partnerSignoff73) {
        if (m_mode == "FT2" && m_asyncTxEnabled && !cooldownKey.isEmpty())
            m_qsoCooldown[cooldownKey] = QDateTime::currentMSecsSinceEpoch();
        if (partnerSignoffNeedsOwn73) {
            bridgeLog("autoSeq: ricevuto 73 da " + (messagePartner.isEmpty() ? from : messagePartner) + " → invio nostro 73 prima del log");
            m_ft2DeferredLogPending = false;
            m_nTx73 = 0;
            m_logAfterOwn73 = true;
            nextTx = 5;
        } else {
            finishAutoSequenceQso("autoSeq: ricevuto 73 da " + (messagePartner.isEmpty() ? from : messagePartner) + " → QSO completo (IDLE_QSO)");
            return;
        }
    } else if (last.compare("RR73", Qt::CaseInsensitive) == 0 ||
               last.compare("RRR",  Qt::CaseInsensitive) == 0) {
        // Se siamo già in SIGNOFF (abbiamo già mandato 73), il QSO è completo — non rimandare 73
        if (m_qsoProgress >= 5 && m_nTx73 >= 1) {
            if (!cooldownKey.isEmpty())
                m_qsoCooldown[cooldownKey] = QDateTime::currentMSecsSinceEpoch();
            finishAutoSequenceQso("autoSeq: ricevuto RR73 ma già in SIGNOFF con nTx73=" + QString::number(m_nTx73) + " → QSO completo");
            return;
        }
        if (!cooldownKey.isEmpty())
            m_qsoCooldown[cooldownKey] = QDateTime::currentMSecsSinceEpoch();
        if (partnerSignoffNeedsOwn73) {
            bridgeLog("autoSeq: ricevuto RR73 da " + (messagePartner.isEmpty() ? from : messagePartner) + " → invio nostro 73 prima del log");
            m_ft2DeferredLogPending = false;
            m_nTx73 = 0;
            m_logAfterOwn73 = true;
            nextTx = 5;
        } else if (partnerFinalAck) {
            finishAutoSequenceQso("autoSeq: ricevuto RR73 finale da " + (messagePartner.isEmpty() ? from : messagePartner) + " → QSO completo");
            return;
        } else {
            bridgeLog("autoSeq: ricevuto " + last + " → TX5 (73)");
            nextTx = 5;
        }
    } else if (last.compare("TU", Qt::CaseInsensitive) == 0 && parts.size() >= 4) {
        // Quick QSO (Ultra2): peer ha inviato "R+NN TU" o "+NN TU"
        // Significa che il peer considera il QSO completo → rispondiamo con RR73 (TX4)
        QString reportToken = parts[parts.size() - 2];
        static QRegularExpression const rptRx(R"(^(?:R)?[+-]\d{2}$)");
        if (rptRx.match(reportToken).hasMatch()) {
            bridgeLog("autoSeq: ricevuto Quick QSO TU da " + from + " (report=" + reportToken + ") → TX4 (RR73)");
            setReportReceived(reportToken.startsWith('R') ? reportToken.mid(1) : reportToken);
            if (!m_dxCall.isEmpty()) genStdMsgs(m_dxCall, m_dxGrid);
            m_quickPeerSignaled = true;
            nextTx = 4;
        } else {
            bridgeLog("autoSeq: TU senza report valido, ignoro");
            return;
        }
    } else if (last.length() >= 2 && last[0] == 'R' &&
               (last[1] == '-' || last[1] == '+' || last[1].isDigit())) {
        // Salva il report ricevuto dal partner: la forma "R<report>" è una
        // conferma + report del partner verso di noi. Se l'utente ha saltato
        // lo scambio del report nudo, questo è l'unico punto in cui il valore
        // viene memorizzato — senza, l'ADIF loggerebbe RST_RCVD vuoto/stale.
        // (Parallelismo con i rami report-nudo e Quick-QSO TU qui sotto.)
        QString const rptValue = last.mid(1);
        bridgeLog("autoSeq: ricevuto R+report " + last + " (rpt=" + rptValue + ") → TX4 (RR73)");
        setReportReceived(rptValue);
        if (!m_dxCall.isEmpty()) genStdMsgs(m_dxCall, m_dxGrid);
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
                         Radio::base_callsign(m_dxCall).trimmed().compare(messagePartnerBase, Qt::CaseInsensitive) == 0 &&
                         m_qsoProgress >= 3);  // già in REPORT state (enum 3=REPORT)
        if (sameGrid) {
            bridgeLog("autoSeq: stesso grid da " + (messagePartner.isEmpty() ? from : messagePartner) + " → TX3 (salto TX2)");
            nextTx = 3;
        } else {
            bridgeLog("autoSeq: grid da " + (messagePartner.isEmpty() ? from : messagePartner) + " → TX2 (report)");
            if (messagePartnerBase.isEmpty()) {
                return;
            }
            if (Radio::base_callsign(m_dxCall).trimmed().compare(messagePartnerBase, Qt::CaseInsensitive) != 0) {
                setDxCall(messagePartner);
                m_qsoStartedOn = QDateTime::currentDateTimeUtc();
                m_lastTransmittedMessage.clear();
            }
            setDxGrid(last);
            genStdMsgs(messagePartner, last);
            nextTx = 2;
        }
    } else {
        return;
    }

    if (nextTx > 0) {
        // Deduplicazione: se stiamo già trasmettendo o abbiamo già impostato
        // lo stesso TX step → non avanzare di nuovo (evita loop in FT2 async)
        if (nextTx == m_currentTx && (m_transmitting || m_txEnabled)) {
            bridgeLog("autoSeq: nextTx=" + QString::number(nextTx) +
                      " già attivo (transmitting=" + QString::number(m_transmitting) +
                      " txEnabled=" + QString::number(m_txEnabled) + "), skip");
            return;
        }
        // Resetta contatori PRIMA di avanzare stato (evita race condition)
        m_txRetryCount = 0;
        m_txWatchdogTicks = 0;
        m_qsoLogged = false;  // nuovo step → permetti log
        if (nextTx <= 4) {
            m_logAfterOwn73 = false;
            m_ft2DeferredLogPending = false;
            if (nextTx <= 3) {
                m_quickPeerSignaled = false;
            }
        }
        // Avanza stato QSO
        advanceQsoState(nextTx);
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

QString DecodiumBridge::diagnosticLogPath() const
{
    return DecodiumLogging::diagnosticLogPath();
}

void DecodiumBridge::openDiagnosticLog() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(DecodiumLogging::diagnosticLogPath()));
}

void DecodiumBridge::openHamlibUpdatePage() const
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/Hamlib/Hamlib/releases")));
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
    QString const configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QDir const configRoot(configDir.isEmpty() ? QDir::homePath() : configDir);
    QString const decodium4IniPath = configRoot.absoluteFilePath(QStringLiteral("decodium4.ini"));
    QString const legacyIniPath = QFileInfo::exists(decodium4IniPath)
        ? decodium4IniPath
        : configRoot.absoluteFilePath(QStringLiteral("ft2.ini"));
    QSettings legacyIni(legacyIniPath, QSettings::IniFormat);
    QString const legacyCallsign = legacyIni.value(QStringLiteral("MyCall")).toString().trimmed().toUpper();
    QString const legacyGrid = legacyIni.value(QStringLiteral("MyGrid")).toString().trimmed().toUpper();
    QString const bridgeCallsign = s.value("callsign", QString()).toString().trimmed().toUpper();
    QString const bridgeGrid = s.value("grid", QString()).toString().trimmed().toUpper();
    m_callsign = !legacyCallsign.isEmpty() ? legacyCallsign
                                           : (bridgeCallsign.isEmpty() ? QStringLiteral("IU8LMC") : bridgeCallsign);
    m_grid     = !legacyGrid.isEmpty() ? legacyGrid
                                       : (bridgeGrid.isEmpty() ? QStringLiteral("AA00") : bridgeGrid);
    m_frequency = s.value("frequency", 14074000.0).toDouble();
    m_mode     = s.value("mode", "FT8").toString();
    m_audioInputDevice  = s.value("audioInputDevice", s.value("SoundInName", "")).toString();
    m_audioOutputDevice = s.value("audioOutputDevice", s.value("SoundOutName", "")).toString();
    m_audioInputChannel = s.contains("audioInputChannel")
        ? s.value("audioInputChannel", 0).toInt()
        : static_cast<int>(AudioDevice::fromString(s.value("AudioInputChannel", "Mono").toString()));
    m_audioOutputChannel = s.contains("audioOutputChannel")
        ? s.value("audioOutputChannel", 0).toInt()
        : static_cast<int>(AudioDevice::fromString(s.value("AudioOutputChannel", "Mono").toString()));
    m_audioInputChannel = qBound(0, m_audioInputChannel, 3);
    m_audioOutputChannel = qBound(0, m_audioOutputChannel, 3);
    m_rxInputLevel = qBound(0.0, s.value("rxInputLevel", 50.0).toDouble(), 100.0);
    m_txOutputLevel = qBound(0.0, s.value("txOutputLevel", 0.0).toDouble(), 450.0);
    m_nfa      = s.value("nfa", 200).toInt();
    m_nfb      = s.value("nfb", 4000).toInt();
    {
        // Do not resurrect Fox/Hound from an old D3/legacy INI at startup.
        // D4 starts the legacy backend only after the user explicitly enables
        // a special operation in the D4 settings, otherwise startup monitoring
        // can block or crash on systems where the legacy DB/thread init fails.
        int const savedActivity = qBound(kSpecialOpNone,
                                         s.value(QStringLiteral("SelectedActivity"), 0).toInt(),
                                         kSpecialOpMax);
        bool const specialEnabled = s.value(QStringLiteral("SpecialOpActivity"), false).toBool();
        m_specialOperationActivity = specialEnabled ? savedActivity : kSpecialOpNone;
        m_ncontest = m_specialOperationActivity;
        m_foxMode = m_specialOperationActivity == kSpecialOpFox;
        m_houndMode = m_specialOperationActivity == kSpecialOpHound;
        m_forceLegacyTxForSpecialOp = specialOperationRequiresLegacyTx();
    }
    {
        bool const hasBridgeLegacyNDepth = s.contains("NDepth");
        bool const hasStandaloneLegacyNDepth = legacyIni.contains(QStringLiteral("NDepth"));
        int const legacyNDepthBits = hasBridgeLegacyNDepth
            ? s.value("NDepth", 3).toInt()
            : legacyIni.value(QStringLiteral("NDepth"), 3).toInt();
        bool const hasLegacyNDepth = s.contains("NDepth");
        bool const hasModernNDepth = s.contains("ndepth");
        bool const legacyDepthMigrated = s.value("decodeDepthMigratedFromLegacy", false).toBool();
        int modernDepth = qBound(1, s.value("ndepth", 3).toInt(), 4);
        int const legacyDepth = legacyDepthBitsToModernDepth(legacyNDepthBits);

        if ((hasLegacyNDepth || hasStandaloneLegacyNDepth) && (!hasModernNDepth || !legacyDepthMigrated)) {
            modernDepth = qMax(modernDepth, legacyDepth);
        }

        m_ndepth = modernDepth;
        m_avgDecodeEnabled = s.value("avgDecodeEnabled",
                                     (legacyNDepthBits & 0x10) != 0).toBool();
        m_deepSearchEnabled = s.value("deepSearchEnabled",
                                      (legacyNDepthBits & 0x20) != 0).toBool();
        m_ft8ApEnabled = s.value("FT8AP", true).toBool();
    }
    m_dxCall   = s.value("dxCall", "").toString();
    m_dxGrid   = s.value("dxGrid", "").toString();
    m_tx1 = s.value("tx1", "").toString();
    m_tx2 = s.value("tx2", "").toString();
    m_tx3 = s.value("tx3", "").toString();
    m_tx4 = s.value("tx4", "").toString();
    m_tx5 = s.value("tx5", "").toString();
    m_tx6 = s.value("tx6", "").toString();
    m_rxFrequency = s.value("rxFrequency", 1500).toInt();
    m_txFrequency = qBound(0, s.value("txFrequency", s.value("TXFrequency", 1500)).toInt(), 3000);
    m_fontScale   = s.value("fontScale", 1.0).toDouble();
    // extra features
    m_alertOnCq        = s.value("alertOnCq",       s.value("alert_CQ", false)).toBool();
    m_alertOnMyCall    = s.value("alertOnMyCall",   s.value("alert_MyCall", false)).toBool();
    m_recordRxEnabled  = s.value("recordRxEnabled",  false).toBool();
    m_recordTxEnabled  = s.value("recordTxEnabled",  false).toBool();
    m_stationName      = s.value("StationName",     QString()).toString();
    m_stationQth       = s.value("StationQTH",      QString()).toString();
    m_stationRigInfo   = s.value("StationRigInfo",  QString()).toString();
    m_stationAntenna   = s.value("StationAntenna",  QString()).toString();
    m_stationPowerWatts= qBound(0, s.value("StationPowerW", 100).toInt(), 9999);
    bool const storedMonitorOffAtStartup = s.value("MonitorOFF", false).toBool();
    if (storedMonitorOffAtStartup) {
        s.setValue(QStringLiteral("MonitorOFF"), false);
        syncSettingToLegacyIni(QStringLiteral("MonitorOFF"), false);
    }
    m_autoStartMonitorOnStartup = true;
    bridgeLog(QStringLiteral("loadSettings: MonitorOFF=%1 forced to 0 autoStartMonitorOnStartup=1")
                  .arg(storedMonitorOffAtStartup ? 1 : 0));
    m_autoSeq          = s.value("autoSeq", s.value("AutoSeq", true)).toBool();
    m_startFromTx2     = s.value("startFromTx2",     false).toBool();
    m_vhfUhfFeatures   = s.value("vhfUhfFeatures",   s.value("VHFUHF", false)).toBool();
    m_directLogQso     = s.value("directLogQso",      false).toBool();
    m_autoSpotEnabled  = s.value("AutoSpotEnabled",   false).toBool();
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
    m_asyncDecodeEnabled=s.value("asyncDecodeEnabled",false).toBool();
    m_pskReporterEnabled=s.value("pskReporterEnabled",false).toBool();
    m_ftThreads         =std::clamp(s.value("ftThreads",3).toInt(), 1, 8);
    // Default 'hamlib' per nuove installazioni: copre 400+ radio (incluso ICOM
    // CI-V completo) con molti meno problemi di compatibilita' rispetto al path
    // nativo che supporta solo comandi ASCII Kenwood/Yaesu. Gli utenti esistenti
    // mantengono il proprio backend salvato.
    m_catBackend        =s.value("catBackend",        "hamlib").toString();
    m_lastSuccessfulCatConnected = s.value("lastSuccessfulCatConnected", false).toBool();
    m_lastSuccessfulCatBackend = s.value("lastSuccessfulCatBackend").toString();
    // TX QSO options
    m_reportReceived    =s.value("reportReceived",    "-10").toString();
    m_sendRR73          =s.value("sendRR73",           true).toBool();
    m_txPeriod          =s.value("txPeriod",            0).toInt() != 0 ? 1 : 0;
    m_alt12Enabled      =s.value("alt12Enabled",     false).toBool();
    m_quickQsoEnabled   =s.value("quickQsoEnabled",  false).toBool();
    // B7 — Colors
    m_colorCQ       = s.value("colorCQ",        "#33FF33").toString();
    m_colorMyCall   = s.value("colorMyCall",     "#FF5555").toString();
    m_colorDXEntity = s.value("colorDXEntity",   "#FFAA33").toString();
    m_color73       = s.value("color73",         "#5599FF").toString();
    m_colorB4       = s.value("colorB4",         "#888888").toString();
    m_b4Strikethrough= s.value("b4Strikethrough",  true).toBool();
    // B8 — Alerts
    m_alertSoundsEnabled = s.value("alertSoundsEnabled", s.value("alert_Enabled", false)).toBool();
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
    if (m_dxCluster) {
        m_dxCluster->loadSettings();
        if (!m_callsign.isEmpty()) m_dxCluster->setCallsign(m_callsign);
    }
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
        m_pskReporter->setLocalStation(m_callsign, m_grid, pskReporterProgramInfo());
    }
    applyNtpSettings();
    // Garantisce che TX6 (CQ) sia sempre valorizzato dopo il caricamento settings
    regenerateTxMessages();
}

void DecodiumBridge::reloadBridgeSettingsFromPersistentStore()
{
    bridgeLog(QStringLiteral("reloadBridgeSettingsFromPersistentStore: applying legacy setup changes to QML bridge"));

    QString const previousCatBackend = m_catBackend;
    QString const previousAudioInputDevice = m_audioInputDevice;
    int const previousAudioInputChannel = m_audioInputChannel;

    loadSettings();
    updatePeriodTicksMax();
    if (m_bandManager) {
        m_bandManager->setCurrentMode(m_mode);
        m_bandManager->updateForMode(m_mode);
        m_bandManager->updateFromFrequency(m_frequency);
    }

    enumerateAudioDevices();

    if (m_nativeCat) {
        m_nativeCat->loadSettings();
    }
    if (m_omniRigCat) {
        m_omniRigCat->loadSettings();
    }
    if (m_hamlibCat) {
        m_hamlibCat->loadSettings();
    }

    if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && m_hamlibCat && m_hamlibCat->connected()) {
        m_hamlibCat->disconnectRig();
    }

    bool const audioInputConfigChanged =
        previousAudioInputDevice != m_audioInputDevice
        || previousAudioInputChannel != m_audioInputChannel;
    if (m_soundInput) {
        m_soundInput->setInputGain(rxInputGainFromLevel(m_rxInputLevel));
        if (m_monitoring
            && audioInputConfigChanged
            && !usingLegacyBackendForTx()
            && !useLegacyRigControlFallback(m_legacyBackend, m_catBackend)) {
            bridgeLog(QStringLiteral("reloadBridgeSettingsFromPersistentStore: restarting RX audio after input change"));
            stopAudioCapture();
            startAudioCapture();
        } else if (m_monitoring && !audioInputConfigChanged) {
            bridgeLog(QStringLiteral("reloadBridgeSettingsFromPersistentStore: keeping RX audio stream active"));
        }
    }
    if (m_soundOutput) {
        m_soundOutput->setAttenuation(txAttenuationFromSlider(m_txOutputLevel));
    }
    if (m_txAudioSink) {
        m_txAudioSink->setVolume(static_cast<float>(txGainFromSlider(m_txOutputLevel)));
    }

    if (previousCatBackend != m_catBackend) {
        if (previousCatBackend == QStringLiteral("native") && m_nativeCat->connected()) {
            m_nativeCat->disconnectRig();
        } else if (previousCatBackend == QStringLiteral("omnirig") && m_omniRigCat->connected()) {
            m_omniRigCat->disconnectRig();
        } else if (isHamlibFamilyBackend(previousCatBackend) && m_hamlibCat->connected()) {
            m_hamlibCat->disconnectRig();
        }
        m_catConnected = false;
        m_catRigName.clear();
        m_catMode.clear();
        emit catConnectedChanged();
        emit catRigNameChanged();
        emit catModeChanged();
        updateRigTelemetry(0.0, 0.0);
    } else {
        auto const activeConnected = [this]() -> bool {
            QObject* obj = catManagerObj();
            return obj ? obj->property("connected").toBool() : false;
        }();
        auto const activeRigName = [this, activeConnected]() -> QString {
            QObject* obj = catManagerObj();
            return (obj && activeConnected) ? obj->property("rigName").toString() : QString {};
        }();
        auto const activeMode = [this]() -> QString {
            QObject* obj = catManagerObj();
            return obj ? obj->property("mode").toString() : QString {};
        }();

        if (m_catConnected != activeConnected) {
            m_catConnected = activeConnected;
            emit catConnectedChanged();
        }
        if (m_catRigName != activeRigName) {
            m_catRigName = activeRigName;
            emit catRigNameChanged();
        }
        if (m_catMode != activeMode) {
            m_catMode = activeMode;
            emit catModeChanged();
        }
        if (isHamlibFamilyBackend(m_catBackend) && m_hamlibCat && m_hamlibCat->connected()) {
            updateRigTelemetry(m_hamlibCat->powerWatts(), m_hamlibCat->swr());
        } else {
            updateRigTelemetry(0.0, 0.0);
        }
    }

    emit callsignChanged();
    emit gridChanged();
    emit frequencyChanged();
    emit modeChanged();
    emit rxFrequencyChanged();
    emit txFrequencyChanged();
    emit rxInputLevelChanged();
    emit txOutputLevelChanged();
    emit audioInputDevicesChanged();
    emit audioOutputDevicesChanged();
    emit audioInputDeviceChanged();
    emit audioOutputDeviceChanged();
    emit audioInputChannelChanged();
    emit audioOutputChannelChanged();
    emit autoSeqChanged();
    emit asyncTxEnabledChanged();
    emit asyncDecodeEnabledChanged();
    emit txPeriodChanged();
    emit alt12EnabledChanged();
    emit quickQsoEnabledChanged();
    emit tx1Changed();
    emit tx2Changed();
    emit tx3Changed();
    emit tx4Changed();
    emit tx5Changed();
    emit tx6Changed();
    emit txMessagesChanged();
    emit currentTxChanged();
    emit currentTxMessageChanged();
    emit dxCallChanged();
    emit dxGridChanged();
    emit reportReceivedChanged();
    emit sendRR73Changed();
    emit fontScaleChanged();
    emit uiPaletteIndexChanged();
    emit catBackendChanged();
    emit catManagerChanged();
    emit alertOnCqChanged();
    emit alertOnMyCallChanged();
    emit recordRxEnabledChanged();
    emit recordTxEnabledChanged();
    emit stationNameChanged();
    emit stationQthChanged();
    emit stationRigInfoChanged();
    emit stationAntennaChanged();
    emit stationPowerWattsChanged();
    emit autoStartMonitorOnStartupChanged();
    emit startFromTx2Changed();
    emit vhfUhfFeaturesChanged();
    emit directLogQsoChanged();
    emit confirm73Changed();
    emit contestExchangeChanged();
    emit contestNumberChanged();
    emit swlModeChanged();
    emit splitModeChanged();
    emit txWatchdogModeChanged();
    emit txWatchdogTimeChanged();
    emit txWatchdogCountChanged();
    emit filterCqOnlyChanged();
    emit filterMyCallOnlyChanged();
    emit contestTypeChanged();
    emit zapEnabledChanged();
    emit avgDecodeEnabledChanged();
    emit ft8ApEnabledChanged();
    emit deepSearchEnabledChanged();
    emit pskReporterEnabledChanged();
    emit cloudlogEnabledChanged();
    emit cloudlogUrlChanged();
    emit cloudlogApiKeyChanged();
    emit lotwEnabledChanged();
    emit wsprUploadEnabledChanged();
    emit nfaChanged();
    emit nfbChanged();
    emit ndepthChanged();
    emit ncontestChanged();
    emit specialOperationActivityChanged();
    emit foxModeChanged();
    emit houndModeChanged();
    emit colorCQChanged();
    emit colorMyCallChanged();
    emit colorDXEntityChanged();
    emit color73Changed();
    emit colorB4Changed();
    emit b4StrikethroughChanged();
    emit alertSoundsEnabledChanged();
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

QVariantMap DecodiumBridge::primaryScreenAvailableGeometry() const
{
    QVariantMap geometry;

    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        auto const screens = QGuiApplication::screens();
        if (!screens.isEmpty()) {
            screen = screens.first();
        }
    }
    if (!screen) {
        return geometry;
    }

    QRect rect = screen->availableGeometry();
    if (!rect.isValid() || rect.width() <= 0 || rect.height() <= 0) {
        rect = screen->geometry();
    }
    if (!rect.isValid() || rect.width() <= 0 || rect.height() <= 0) {
        return geometry;
    }

    geometry.insert(QStringLiteral("x"), rect.x());
    geometry.insert(QStringLiteral("y"), rect.y());
    geometry.insert(QStringLiteral("width"), rect.width());
    geometry.insert(QStringLiteral("height"), rect.height());
    geometry.insert(QStringLiteral("name"), screen->name());
    return geometry;
}

QString DecodiumBridge::version() const
{
    return QStringLiteral(FORK_RELEASE_VERSION);
}

bool DecodiumBridge::recordRxEnabled() const
{
    return m_wavManager ? m_wavManager->recording() : m_recordRxEnabled;
}

void DecodiumBridge::setRecordRxEnabled(bool value)
{
    if (m_recordRxEnabled == value && recordRxEnabled() == value) {
        return;
    }

    m_recordRxEnabled = value;

    if (!m_wavManager) {
        emit recordRxEnabledChanged();
        return;
    }

    if (m_wavManager->recording() == value) {
        emit recordRxEnabledChanged();
        return;
    }

    if (value) {
        m_wavManager->startRecording();
        if (m_wavManager->recording()) {
            emit statusMessage(QStringLiteral("Registrazione RX avviata: %1")
                               .arg(QDir::toNativeSeparators(m_wavManager->lastFilePath())));
        } else {
            emit errorMessage(QStringLiteral("Impossibile avviare la registrazione RX"));
            emit recordRxEnabledChanged();
        }
        return;
    }

    QString const savedPath = m_wavManager->lastFilePath();
    m_wavManager->stopRecording();
    emit statusMessage(savedPath.isEmpty()
                           ? QStringLiteral("Registrazione RX fermata")
                           : QStringLiteral("Registrazione RX salvata: %1")
                                 .arg(QDir::toNativeSeparators(savedPath)));
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
    // Usa la directory legacy (AppData\Local\Decodium) se il log esiste gia' li',
    // altrimenti la directory standard Qt (AppData\Local\IU8LMC\Decodium)
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

int DecodiumBridge::effectiveDecodeDepth() const
{
    int depth = qBound(1, m_ndepth, 4);
    if ((m_avgDecodeEnabled || m_deepSearchEnabled) && depth < 4) {
        depth = 4;
    }
    qDebug() << "[DEPTHDBG] effectiveDecodeDepth: m_ndepth=" << m_ndepth
             << " avg=" << m_avgDecodeEnabled
             << " deep=" << m_deepSearchEnabled
             << " -> depth=" << depth;
    return depth;
}

int DecodiumBridge::legacyCompatibleDecodeDepthBits() const
{
    int const eff = effectiveDecodeDepth();
    int const bits = modernDepthToLegacyBits(eff, m_avgDecodeEnabled, m_deepSearchEnabled);
    qDebug() << "[DEPTHDBG] legacyCompatibleDecodeDepthBits: eff=" << eff
             << " avg=" << m_avgDecodeEnabled
             << " deep=" << m_deepSearchEnabled
             << " -> bits=" << bits << "(0x" << QString::number(bits, 16) << ")";
    return bits;
}

int DecodiumBridge::legacyDecodeQsoProgress() const
{
    return qBound(0, m_qsoProgress, 6);
}

int DecodiumBridge::legacyDecodeCqHint() const
{
    if (m_filterCqOnly) {
        return 1;
    }

    return 0;
}

QVariantList DecodiumBridge::augmentLegacyMirrorWithAllTxt(QVariantList const& mirroredEntries,
                                                           bool rxPane) const
{
    if (!isTimeSyncDecodeMode(m_mode)) {
        return mirroredEntries;
    }

    QString const path = legacyAllTxtPath();
    if (path.trimmed().isEmpty()) {
        return mirroredEntries;
    }

    QFileInfo const fileInfo(path);
    if (!fileInfo.exists()) {
        m_legacyAllTxtConsumedPath = path;
        m_legacyAllTxtConsumedSize = -1;
        return mirroredEntries;
    }

    QString const absolutePath = fileInfo.absoluteFilePath();
    qint64 const fileSize = fileInfo.size();
    if (m_legacyAllTxtConsumedPath != absolutePath
        || m_legacyAllTxtConsumedSize < 0
        || fileSize < m_legacyAllTxtConsumedSize) {
        m_legacyAllTxtConsumedPath = absolutePath;
        m_legacyAllTxtConsumedSize = fileSize;
        bridgeLog(QStringLiteral("legacy-alltxt prime %1: size=%2 source=%3")
                      .arg(rxPane ? QStringLiteral("rx") : QStringLiteral("band"))
                      .arg(fileSize)
                      .arg(absolutePath));
        return mirroredEntries;
    }

    // Su slot molto affollati FT8/FT4 il legacy ALL.TXT puo crescere di diverse
    // centinaia di righe in meno di un minuto. Un budget troppo piccolo fa si che
    // il mirror di D4 veda solo la coda finale del file e perda CQ valide che D3
    // ha gia mostrato a schermo. Manteniamo una finestra piu ampia e dinamica.
    int const desiredLineBudget = qMax(4096, mirroredEntries.size() * 8);
    QStringList const tailLines = readTextLinesFromOffset(path, m_legacyAllTxtConsumedSize, desiredLineBudget);
    if (tailLines.isEmpty()) {
        m_legacyAllTxtConsumedSize = fileSize;
        return mirroredEntries;
    }

    static const QRegularExpression allTxtPattern {
        R"(^(\d{6})_(\d{6})\s+([0-9]+(?:\.[0-9]+)?)\s+(Rx|Tx|Ck)\s+([A-Z0-9_+\-]+)\s+(.+)$)",
        QRegularExpression::CaseInsensitiveOption
    };
    static const QRegularExpression payloadPattern {
        R"(^(-?\d+)\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+(.*)$)"
    };

    QVariantList augmented = mirroredEntries;
    QHash<QString, int> exactIndexByKey;
    QMultiHash<QString, int> looseIndexByKey;
    for (int i = 0; i < augmented.size(); ++i) {
        QVariantMap const entry = augmented.at(i).toMap();
        exactIndexByKey.insert(decodeMirrorEntryKey(entry), i);
        looseIndexByKey.insert(decodeMirrorLooseKey(entry), i);
    }

    QString const normalizedMode = m_mode.trimmed().toUpper();
    int upgradedTimes = 0;
    int appended = 0;
    for (QString const& rawLine : tailLines) {
        QRegularExpressionMatch const lineMatch = allTxtPattern.match(rawLine.trimmed());
        if (!lineMatch.hasMatch()) {
            continue;
        }

        QString const txRx = lineMatch.captured(4).trimmed().toUpper();
        if (txRx != QStringLiteral("RX")) {
            continue;
        }

        QString const entryMode = lineMatch.captured(5).trimmed().toUpper();
        if (entryMode != normalizedMode) {
            continue;
        }

        QRegularExpressionMatch const payloadMatch = payloadPattern.match(lineMatch.captured(6).trimmed());
        if (!payloadMatch.hasMatch()) {
            continue;
        }

        QVariantMap entry;
        entry[QStringLiteral("time")] = normalizeUtcDisplayToken(lineMatch.captured(2));
        entry[QStringLiteral("db")] = payloadMatch.captured(1).trimmed();
        entry[QStringLiteral("dt")] = payloadMatch.captured(2).trimmed();
        entry[QStringLiteral("freq")] = payloadMatch.captured(3).trimmed();
        QString const message = payloadMatch.captured(4).trimmed();
        if (message.isEmpty()) {
            continue;
        }
        QString semanticRejectReason;
        if (!shouldAcceptDecodedMessage(message, &semanticRejectReason)) {
            bridgeLog(QStringLiteral("semantic decode filter legacy-alltxt: %1 msg='%2'")
                          .arg(semanticRejectReason, message));
            continue;
        }

        bool const isCQ = message.startsWith(QStringLiteral("CQ "), Qt::CaseInsensitive)
                       || message == QStringLiteral("CQ")
                       || message.startsWith(QStringLiteral("QRZ "), Qt::CaseInsensitive);
        QString const myCallUpper = m_callsign.trimmed().toUpper();
        QString const myBaseUpper = normalizedBaseCall(myCallUpper);

        entry[QStringLiteral("message")] = message;
        entry[QStringLiteral("aptype")] = QString {};
        entry[QStringLiteral("quality")] = QString {};
        entry[QStringLiteral("mode")] = normalizedMode;
        entry[QStringLiteral("isTx")] = false;
        entry[QStringLiteral("isCQ")] = isCQ;
        entry[QStringLiteral("isMyCall")] = messageContainsCallToken(message, myCallUpper, myBaseUpper);
        entry[QStringLiteral("fromCall")] = extractDecodedCallsign(message, isCQ);
        entry[QStringLiteral("forceRxPane")] = rxPane;
        enrichDecodeEntry(entry);

        QString const exactKey = decodeMirrorEntryKey(entry);
        if (exactIndexByKey.contains(exactKey)) {
            continue;
        }

        QString const looseKey = decodeMirrorLooseKey(entry);
        auto const indexes = looseIndexByKey.values(looseKey);
        bool upgradedExisting = false;
        for (int const index : indexes) {
            QVariantMap existing = augmented.at(index).toMap();
            QString const existingTime = normalizeUtcDisplayToken(existing.value(QStringLiteral("time")).toString());
            if (existing.value(QStringLiteral("isTx")).toBool()) {
                continue;
            }
            if (existingTime.size() >= 6) {
                continue;
            }
            if (!existingTime.isEmpty() && !entry.value(QStringLiteral("time")).toString().startsWith(existingTime)) {
                continue;
            }
            existing[QStringLiteral("time")] = entry.value(QStringLiteral("time")).toString();
            augmented[index] = existing;
            exactIndexByKey.insert(decodeMirrorEntryKey(existing), index);
            ++upgradedTimes;
            upgradedExisting = true;
            break;
        }

        if (upgradedExisting) {
            continue;
        }

        if (rxPane) {
            continue;
        }

        int const newIndex = augmented.size();
        augmented.append(entry);
        exactIndexByKey.insert(exactKey, newIndex);
        looseIndexByKey.insert(looseKey, newIndex);
        ++appended;
    }

    if (upgradedTimes > 0 || appended > 0) {
        bridgeLog(QStringLiteral("legacy-alltxt augment %1: upgraded=%2 appended=%3 source=%4")
                      .arg(rxPane ? QStringLiteral("rx") : QStringLiteral("band"))
                      .arg(upgradedTimes)
                      .arg(appended)
                      .arg(path));
    }

    m_legacyAllTxtConsumedSize = fileSize;

    return augmented;
}

QString DecodiumBridge::legacyAllTxtPath() const
{
    if (m_legacyBackend) {
        QString const embeddedPath = m_legacyBackend->allTxtPath().trimmed();
        if (!embeddedPath.isEmpty()) {
            return embeddedPath;
        }
    }

    QString const genericRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString const appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString const appLocal = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QStringList candidates;
    int localCandidateCount = 0;

    if (!appData.isEmpty()) {
        candidates << QDir(appData).absoluteFilePath(QStringLiteral("ALL.TXT"));
        candidates << QDir(appData).absoluteFilePath(QStringLiteral("all.txt"));
        localCandidateCount = candidates.size();
    }
    if (!appLocal.isEmpty() && appLocal != appData) {
        candidates << QDir(appLocal).absoluteFilePath(QStringLiteral("ALL.TXT"));
        candidates << QDir(appLocal).absoluteFilePath(QStringLiteral("all.txt"));
        localCandidateCount = candidates.size();
    }
    if (!genericRoot.isEmpty()) {
        candidates << QDir(genericRoot).absoluteFilePath(QStringLiteral("ft2/ALL.TXT"));
        candidates << QDir(genericRoot).absoluteFilePath(QStringLiteral("IU8LMC/ft2/ALL.TXT"));
        candidates << QDir(genericRoot).absoluteFilePath(QStringLiteral("Decodium/ALL.TXT"));
    }

    candidates.removeDuplicates();

    qint64 newestActiveMsecs = std::numeric_limits<qint64>::min();
    QList<QFileInfo> existingLocalCandidates;
    QList<QFileInfo> existingFallbackCandidates;
    for (int i = 0; i < candidates.size(); ++i) {
        QString const& candidate = candidates.at(i);
        QFileInfo const info(candidate);
        if (!info.exists() || !info.isFile()) {
            continue;
        }
        if (i < localCandidateCount) {
            existingLocalCandidates.append(info);
        } else {
            existingFallbackCandidates.append(info);
        }
        newestActiveMsecs = qMax(newestActiveMsecs,
                                 info.lastModified(QTimeZone::UTC).toMSecsSinceEpoch());
    }

    QList<QFileInfo> const existingCandidates =
        !existingLocalCandidates.isEmpty() ? existingLocalCandidates : existingFallbackCandidates;

    if (!existingCandidates.isEmpty()) {
        QFileInfo bestInfo;
        for (QFileInfo const& info : std::as_const(existingCandidates)) {
            qint64 const modifiedMsecs = info.lastModified(QTimeZone::UTC).toMSecsSinceEpoch();
            // Se piu file stanno venendo aggiornati praticamente insieme,
            // scegli quello piu ricco invece di quello che ha solo qualche
            // millisecondo di vantaggio ma meno decode.
            if (modifiedMsecs + 15000 < newestActiveMsecs) {
                continue;
            }
            if (!bestInfo.exists()
                || info.size() > bestInfo.size()
                || (info.size() == bestInfo.size()
                    && modifiedMsecs > bestInfo.lastModified(QTimeZone::UTC).toMSecsSinceEpoch())) {
                bestInfo = info;
            }
        }
        if (bestInfo.exists()) {
            return bestInfo.absoluteFilePath();
        }
    }

    if (!appData.isEmpty()) {
        return QDir(appData).absoluteFilePath(QStringLiteral("ALL.TXT"));
    }
    if (!appLocal.isEmpty()) {
        return QDir(appLocal).absoluteFilePath(QStringLiteral("ALL.TXT"));
    }
    if (!genericRoot.isEmpty()) {
        return QDir(genericRoot).absoluteFilePath(QStringLiteral("ft2/ALL.TXT"));
    }
    return QStringLiteral("ALL.TXT");
}

void DecodiumBridge::appendLegacyAllTxtDecodeLine(const QVariantMap& entry) const
{
    if (entry.isEmpty() || entry.value(QStringLiteral("isTx")).toBool()) {
        return;
    }

    // Quando il backend legacy embedded è attivo, il suo MainWindow scrive già
    // ALL.TXT in formato D3. Se aggiungiamo anche le righe del decoder moderno,
    // il file privato di D4 finisce contaminato con decode extra che D3 non ha
    // e il mirror del pannello non può più coincidere 1:1 col legacy.
    if (usingLegacyBackendForTx()) {
        return;
    }

    QString const path = legacyAllTxtPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        return;
    }

    QDateTime const utcWhen = approxUtcDateTimeForDisplayToken(entry.value(QStringLiteral("time")).toString());
    QString const modeString = m_mode.leftJustified(6, QLatin1Char(' '));
    QString const db = entry.value(QStringLiteral("db")).toString().trimmed();
    QString const dt = entry.value(QStringLiteral("dt")).toString().trimmed();
    QString const freq = entry.value(QStringLiteral("freq")).toString().trimmed();
    QString const message = entry.value(QStringLiteral("message")).toString().trimmed();

    QTextStream out(&file);
    out << QStringLiteral("%1 %2  Rx  %3%4%5%6  %7")
              .arg(utcWhen.toString(QStringLiteral("yyMMdd_hhmmss")))
              .arg(m_frequency / 1e6, 10, 'f', 3)
              .arg(modeString)
              .arg(db, 4)
              .arg(dt, 5)
              .arg(freq, 5)
              .arg(message)
        << '\n';
}

static QString bridgeAdifRecordText(const QString& dxCall, const QString& dxGrid,
                                    double freqHz, const QString& mode,
                                    const QDateTime& timeOnUtc,
                                    const QDateTime& timeOffUtc,
                                    const QString& rstSent, const QString& rstRcvd,
                                    const QString& myCall, const QString& myGrid,
                                    const QString& comments = QString(),
                                    const QString& propMode = QString(),
                                    const QString& satellite = QString(),
                                    const QString& satMode = QString(),
                                    const QString& freqRx = QString());

QVariantMap DecodiumBridge::pendingLogQsoPreview() const
{
    if (m_promptLogSnapshotValid) {
        QVariantMap preview;
        preview.insert(QStringLiteral("call"), m_promptLogCall);
        preview.insert(QStringLiteral("grid"), m_promptLogGrid);
        preview.insert(QStringLiteral("sent"), m_promptLogRptSent);
        preview.insert(QStringLiteral("rcvd"), m_promptLogRptRcvd);
        preview.insert(QStringLiteral("freq"), m_promptLogDialFreq);
        preview.insert(QStringLiteral("mode"), m_promptLogMode.isEmpty() ? m_mode : m_promptLogMode);
        preview.insert(QStringLiteral("comment"),
                       m_promptLogCommentOverrideValid
                           ? m_promptLogComment
                           : defaultLogCommentForQso(preview.value(QStringLiteral("mode")).toString(),
                                                     m_promptLogRptSent,
                                                     m_promptLogRptRcvd));
        return preview;
    }

    QString logDxCall = m_dxCall.trimmed();
    QString logDxGrid = m_dxGrid.trimmed();
    QString logRptSent = m_reportSent.trimmed();
    QString logRptRcvd = m_reportReceived.trimmed();
    double logFreqHz = m_frequency;

    if (m_pendingAutoLogValid) {
        if (!m_pendingAutoLogCall.isEmpty()) logDxCall = m_pendingAutoLogCall;
        if (!m_pendingAutoLogGrid.isEmpty()) logDxGrid = m_pendingAutoLogGrid;
        if (!m_pendingAutoLogRptSent.isEmpty()) logRptSent = m_pendingAutoLogRptSent;
        if (!m_pendingAutoLogRptRcvd.isEmpty()) logRptRcvd = m_pendingAutoLogRptRcvd;
        if (m_pendingAutoLogDialFreq > 0.0) logFreqHz = m_pendingAutoLogDialFreq;
    }

    if (logDxCall.isEmpty() && m_lateAutoLogValid
        && m_lateAutoLogExpires >= QDateTime::currentDateTimeUtc()) {
        logDxCall = m_lateAutoLogCall;
        logDxGrid = m_lateAutoLogGrid;
        logRptSent = m_lateAutoLogRptSent;
        logRptRcvd = m_lateAutoLogRptRcvd;
        if (m_lateAutoLogDialFreq > 0.0) logFreqHz = m_lateAutoLogDialFreq;
    }

    QVariantMap preview;
    preview.insert(QStringLiteral("call"), logDxCall);
    preview.insert(QStringLiteral("grid"), logDxGrid);
    preview.insert(QStringLiteral("sent"), logRptSent);
    preview.insert(QStringLiteral("rcvd"), logRptRcvd);
    preview.insert(QStringLiteral("freq"), logFreqHz);
    preview.insert(QStringLiteral("mode"), m_mode);
    preview.insert(QStringLiteral("comment"),
                   m_promptLogCommentOverrideValid
                       ? m_promptLogComment
                       : defaultLogCommentForQso(m_mode, logRptSent, logRptRcvd));
    return preview;
}

bool DecodiumBridge::promptToLogEnabled() const
{
    return getSetting(QStringLiteral("PromptToLog"), false).toBool();
}

void DecodiumBridge::promptLogQso()
{
    if (m_qsoLogged) {
        bridgeLog(QStringLiteral("promptLogQso: skipped (already logged this QSO)"));
        return;
    }

    QVariantMap const preview = pendingLogQsoPreview();
    if (preview.value(QStringLiteral("call")).toString().trimmed().isEmpty()) {
        emit statusMessage(QStringLiteral("No QSO to log"));
        return;
    }

    if (!m_promptLogSnapshotValid) {
        capturePromptLogSnapshot(preview);
    }
    if (!m_logPromptOpen) {
        m_logPromptOpen = true;
        bridgeLog(QStringLiteral("logQso: QML prompt requested for %1").arg(m_promptLogCall));
        QTimer::singleShot(0, this, [this]() {
            if (m_mainQmlReady) {
                emit logQsoPromptRequested();
            } else {
                showLogQsoPromptDialog();
            }
        });
    } else {
        QTimer::singleShot(0, this, [this]() {
            if (m_mainQmlReady) {
                emit logQsoPromptRequested();
            } else {
                showLogQsoPromptDialog();
            }
        });
    }
}

void DecodiumBridge::logQso()
{
    if (promptToLogEnabled() && !m_qsoLogged) {
        QVariantMap const preview = pendingLogQsoPreview();
        if (!preview.value(QStringLiteral("call")).toString().trimmed().isEmpty()) {
            promptLogQso();
            return;
        }
    }

    logQsoNow();
}

void DecodiumBridge::confirmLogQso()
{
    bridgeLog(QStringLiteral("logQso: prompt accepted for %1")
                  .arg(m_promptLogSnapshotValid ? m_promptLogCall : m_dxCall));
    m_logPromptOpen = false;
    QTimer::singleShot(0, this, [this]() {
        logQsoNow();
    });
}

void DecodiumBridge::rejectPromptedLogQso()
{
    if (!m_logPromptOpen) {
        return;
    }

    QString skippedCall = m_dxCall.trimmed();
    if (m_promptLogSnapshotValid && !m_promptLogCall.trimmed().isEmpty()) {
        skippedCall = m_promptLogCall.trimmed();
    } else if (m_pendingAutoLogValid && !m_pendingAutoLogCall.trimmed().isEmpty()) {
        skippedCall = m_pendingAutoLogCall.trimmed();
    } else if (m_lateAutoLogValid && !m_lateAutoLogCall.trimmed().isEmpty()) {
        skippedCall = m_lateAutoLogCall.trimmed();
    }

    m_logPromptOpen = false;
    clearNextLogClusterSpotOverride();
    clearPromptLogSnapshot();
    clearPendingAutoLogSnapshot();
    clearLateAutoLogSnapshot();
    m_qsoLogged = true;
    clearTxArmedAfterCompletedQso(skippedCall, QStringLiteral("prompt-skip"));
    emit statusMessage(QStringLiteral("Log QSO skipped"));
}

void DecodiumBridge::capturePromptLogSnapshot(const QVariantMap& preview)
{
    m_promptLogSnapshotValid = true;
    m_promptLogCall = preview.value(QStringLiteral("call")).toString().trimmed();
    m_promptLogGrid = preview.value(QStringLiteral("grid")).toString().trimmed();
    m_promptLogRptSent = preview.value(QStringLiteral("sent")).toString().trimmed();
    m_promptLogRptRcvd = preview.value(QStringLiteral("rcvd")).toString().trimmed();
    m_promptLogMode = preview.value(QStringLiteral("mode"), m_mode).toString().trimmed();
    m_promptLogDialFreq = preview.value(QStringLiteral("freq"), m_frequency).toDouble();
    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    if (m_pendingAutoLogValid && m_pendingAutoLogOn.isValid()) {
        m_promptLogOn = m_pendingAutoLogOn;
        m_promptLogOff = m_pendingAutoLogOff.isValid() ? m_pendingAutoLogOff : nowUtc;
    } else if (m_lateAutoLogValid && m_lateAutoLogOn.isValid()) {
        m_promptLogOn = m_lateAutoLogOn;
        m_promptLogOff = m_lateAutoLogOff.isValid() ? m_lateAutoLogOff : nowUtc;
    } else {
        m_promptLogOn = nowUtc;
        m_promptLogOff = nowUtc;
    }
    if (m_promptLogOff.isValid() && m_promptLogOn.isValid() && m_promptLogOff < m_promptLogOn) {
        m_promptLogOff = m_promptLogOn;
    }
}

void DecodiumBridge::clearPromptLogSnapshot()
{
    m_promptLogSnapshotValid = false;
    m_promptLogCall.clear();
    m_promptLogGrid.clear();
    m_promptLogRptSent.clear();
    m_promptLogRptRcvd.clear();
    m_promptLogMode.clear();
    m_promptLogComment.clear();
    m_promptLogCommentOverrideValid = false;
    m_promptLogOn = QDateTime {};
    m_promptLogOff = QDateTime {};
    m_promptLogDialFreq = 0.0;
}

QString DecodiumBridge::defaultLogCommentForQso(const QString& mode,
                                                const QString& rstSent,
                                                const QString& rstRcvd) const
{
    if (getSetting(QStringLiteral("dBReportsToComments"), false).toBool()) {
        QString comment = mode.trimmed().toUpper();
        if (!rstSent.trimmed().isEmpty()) {
            comment += QStringLiteral("  Sent: ") + rstSent.trimmed();
        }
        if (!rstRcvd.trimmed().isEmpty()) {
            comment += QStringLiteral("  Rcvd: ") + rstRcvd.trimmed();
        }
        return comment.simplified();
    }

    if (getSetting(QStringLiteral("SpecOpInComments"), false).toBool()) {
        QString const contestName = getSetting(QStringLiteral("Contest_Name"), QString()).toString().trimmed();
        bool const individualContestName =
            getSetting(QStringLiteral("Individual_Contest_Name"), false).toBool();
        switch (m_specialOperationActivity) {
        case 1: return individualContestName && !contestName.isEmpty()
                    ? contestName + QStringLiteral(" Contest")
                    : QStringLiteral("NA VHF Contest");
        case 2: return individualContestName && !contestName.isEmpty()
                    ? contestName + QStringLiteral(" Contest")
                    : QStringLiteral("EU VHF Contest");
        case 3: return individualContestName && !contestName.isEmpty()
                    ? contestName + QStringLiteral(" Contest")
                    : QStringLiteral("ARRL Field Day");
        case 4: return individualContestName && !contestName.isEmpty()
                    ? contestName + QStringLiteral(" Contest")
                    : QStringLiteral("FT Roundup messages");
        case 5: return individualContestName && !contestName.isEmpty()
                    ? contestName + QStringLiteral(" Contest")
                    : QStringLiteral("WW Digi Contest");
        case 6: return getSetting(QStringLiteral("SuperFox"), true).toBool()
                    ? QStringLiteral("SF/H mode")
                    : QStringLiteral("F/H mode");
        case 7: return getSetting(QStringLiteral("SuperFox"), true).toBool()
                    ? QStringLiteral("SF/H mode")
                    : QStringLiteral("F/H mode");
        case 8: return individualContestName && !contestName.isEmpty()
                    ? contestName + QStringLiteral(" Contest")
                    : QStringLiteral("ARRL Digi Contest");
        default:
            break;
        }
    }

    return getSetting(QStringLiteral("LogComments"), QString()).toString().trimmed();
}

static QString satelliteCodeFromDisplayText(const QString& displayText)
{
    QString const text = displayText.trimmed();
    int const separator = text.indexOf(QStringLiteral(" - "));
    return separator > 0 ? text.left(separator).trimmed() : text;
}

void DecodiumBridge::showLogQsoPromptDialog()
{
    if (m_logQsoPromptDialog) {
        m_logQsoPromptDialog->show();
        m_logQsoPromptDialog->raise();
        m_logQsoPromptDialog->activateWindow();
        return;
    }

    QVariantMap const preview = pendingLogQsoPreview();
    QString const call = preview.value(QStringLiteral("call")).toString().trimmed();
    if (call.isEmpty()) {
        rejectPromptedLogQso();
        return;
    }

    auto* dialog = new QDialog(QApplication::activeWindow());
    m_logQsoPromptDialog = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("Confirm Log QSO"));
    dialog->setModal(false);
    dialog->setMinimumWidth(420);
    dialog->setProperty("decodiumAccepted", false);

    auto* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->addRow(QStringLiteral("Call:"), new QLabel(call, dialog));
    form->addRow(QStringLiteral("Grid:"), new QLabel(preview.value(QStringLiteral("grid")).toString().trimmed(), dialog));
    form->addRow(QStringLiteral("Report:"), new QLabel(QStringLiteral("%1 / %2")
                                                           .arg(preview.value(QStringLiteral("sent")).toString().trimmed(),
                                                                preview.value(QStringLiteral("rcvd")).toString().trimmed()), dialog));
    form->addRow(QStringLiteral("Mode:"), new QLabel(preview.value(QStringLiteral("mode")).toString().trimmed(), dialog));
    double const freqHz = preview.value(QStringLiteral("freq")).toDouble();
    form->addRow(QStringLiteral("Freq:"), new QLabel(freqHz > 0.0
                                                        ? QStringLiteral("%1 Hz").arg(freqHz, 0, 'f', 0)
                                                        : QStringLiteral("-"), dialog));

    auto* commentEdit = new QLineEdit(
        defaultLogCommentForQso(preview.value(QStringLiteral("mode")).toString().trimmed(),
                                preview.value(QStringLiteral("sent")).toString().trimmed(),
                                preview.value(QStringLiteral("rcvd")).toString().trimmed()),
        dialog);
    commentEdit->setClearButtonEnabled(true);
    form->addRow(QStringLiteral("Comment:"), commentEdit);

    auto* satelliteCombo = new QComboBox(dialog);
    satelliteCombo->addItems(satelliteOptions());
    QString const savedSatellite = getSetting(QStringLiteral("Satellite"), QString()).toString().trimmed();
    if (!savedSatellite.isEmpty()) {
        for (int i = 0; i < satelliteCombo->count(); ++i) {
            if (satelliteCodeFromDisplayText(satelliteCombo->itemText(i)) == savedSatellite) {
                satelliteCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    form->addRow(QStringLiteral("Satellite:"), satelliteCombo);

    auto* satModeCombo = new QComboBox(dialog);
    satModeCombo->addItems(satModeOptions());
    QString const savedSatMode = getSetting(QStringLiteral("SatMode"), QString()).toString().trimmed();
    int const satModeIndex = satModeCombo->findText(savedSatMode);
    if (satModeIndex >= 0) {
        satModeCombo->setCurrentIndex(satModeIndex);
    }
    satModeCombo->setEnabled(!satelliteCodeFromDisplayText(satelliteCombo->currentText()).isEmpty());
    connect(satelliteCombo, &QComboBox::currentTextChanged, dialog,
            [satModeCombo](const QString& text) {
        satModeCombo->setEnabled(!satelliteCodeFromDisplayText(text).isEmpty());
    });
    form->addRow(QStringLiteral("Sat Mode:"), satModeCombo);

    bool const clusterAvailable = dxClusterConnected();
    auto* spotCheck = new QCheckBox(clusterAvailable
                                        ? QStringLiteral("Spot on DX Cluster")
                                        : QStringLiteral("DX Cluster not connected"), dialog);
    spotCheck->setEnabled(clusterAvailable);
    spotCheck->setChecked(clusterAvailable && m_autoSpotEnabled);
    form->addRow(QStringLiteral("DX Cluster:"), spotCheck);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    if (auto* okButton = buttons->button(QDialogButtonBox::Ok)) {
        okButton->setText(QStringLiteral("Add"));
    }
    if (auto* cancelButton = buttons->button(QDialogButtonBox::Cancel)) {
        cancelButton->setText(QStringLiteral("Skip"));
    }
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this,
            [this, dialog, satelliteCombo, satModeCombo, spotCheck, commentEdit]() {
        dialog->setProperty("decodiumAccepted", true);
        m_promptLogComment = commentEdit->text().trimmed();
        m_promptLogCommentOverrideValid = true;
        setSetting(QStringLiteral("LogComments"), m_promptLogComment);
        QString const satellite = satelliteCodeFromDisplayText(satelliteCombo->currentText());
        QString const satMode = satellite.isEmpty() ? QString() : satModeCombo->currentText().trimmed();
        if (!satellite.isEmpty()) {
            setSetting(QStringLiteral("Satellite"), satellite);
            setSetting(QStringLiteral("SatMode"), satMode);
            setSetting(QStringLiteral("PropMode"), QStringLiteral("SAT"));
            setSetting(QStringLiteral("SaveSatellite"), true);
            setSetting(QStringLiteral("SaveSatMode"), !satMode.isEmpty());
            setSetting(QStringLiteral("SavePropMode"), true);
        }
        setNextLogClusterSpotEnabled(spotCheck->isEnabled() && spotCheck->isChecked());
        dialog->accept();
        confirmLogQso();
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(dialog, &QDialog::finished, this, [this, dialog](int) {
        bool const accepted = dialog->property("decodiumAccepted").toBool();
        m_logQsoPromptDialog.clear();
        if (!accepted && m_logPromptOpen) {
            rejectPromptedLogQso();
        }
    });

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void DecodiumBridge::clearTxArmedAfterCompletedQso(const QString& completedCall, const QString& reason)
{
    bool const continueQueuedCaller = m_multiAnswerMode && !m_callerQueue.isEmpty();
    bool const continueAutoCq = m_autoCqRepeat && !m_tx6.trimmed().isEmpty();

    QString const completedBase = Radio::base_callsign(completedCall).trimmed().toUpper();
    QString const activeBase = Radio::base_callsign(m_dxCall).trimmed().toUpper();
    bool const activeDifferentQso = !completedBase.isEmpty()
        && !activeBase.isEmpty()
        && activeBase != completedBase;

    if (continueQueuedCaller || continueAutoCq || activeDifferentQso) {
        if (m_multiAnswerMode && !completedBase.isEmpty()) {
            removeCallerFromQueue(completedBase);
        }
        bridgeLog(QStringLiteral("QSO complete: preserving TX enable (%1, queued=%2 autoCQ=%3 activeDifferent=%4)")
                      .arg(reason)
                      .arg(continueQueuedCaller ? 1 : 0)
                      .arg(continueAutoCq ? 1 : 0)
                      .arg(activeDifferentQso ? 1 : 0));
        return;
    }

    if (m_multiAnswerMode && !completedBase.isEmpty()) {
        removeCallerFromQueue(completedBase);
    }
    clearDeferredManualSyncTx(reason);
    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    m_quickPeerSignaled = false;
    m_nTx73 = 0;
    m_txRetryCount = 0;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;
    m_lastNtx = -1;
    m_lastCqPidx = -1;

    if (m_qsoProgress != 6) {
        m_qsoProgress = 6;
        emit qsoProgressChanged();
    }

    if (m_txEnabled) {
        bridgeLog(QStringLiteral("QSO complete: clearing visual TX enable (%1)").arg(reason));
        setTxEnabled(false);
    }
}

void DecodiumBridge::logQsoNow()
{
    QElapsedTimer logTimer;
    logTimer.start();
    qint64 lastTraceMs = 0;
    auto traceLogStep = [&logTimer, &lastTraceMs](const QString& step) {
        qint64 const now = logTimer.elapsed();
        bridgeLog(QStringLiteral("logQsoNow timing: %1 delta=%2ms total=%3ms")
                      .arg(step)
                      .arg(now - lastTraceMs)
                      .arg(now));
        lastTraceMs = now;
    };

    m_logPromptOpen = false;
    bool const clusterSpotOverrideValid = m_nextLogClusterSpotOverrideValid;
    bool const clusterSpotRequested = clusterSpotOverrideValid
        ? m_nextLogClusterSpotEnabled
        : m_autoSpotEnabled;
    clearNextLogClusterSpotOverride();
    auto completedLogCallForStateClear = [this]() -> QString {
        if (m_promptLogSnapshotValid && !m_promptLogCall.trimmed().isEmpty()) {
            return m_promptLogCall.trimmed();
        }
        if (m_pendingAutoLogValid && !m_pendingAutoLogCall.trimmed().isEmpty()) {
            return m_pendingAutoLogCall.trimmed();
        }
        if (m_lateAutoLogValid && !m_lateAutoLogCall.trimmed().isEmpty()) {
            return m_lateAutoLogCall.trimmed();
        }
        return m_dxCall.trimmed();
    };

    // Anti-doppio log: previeni log multipli dello stesso QSO
    if (m_qsoLogged) {
        bridgeLog("logQso: skipped (already logged this QSO)");
        clearTxArmedAfterCompletedQso(completedLogCallForStateClear(), QStringLiteral("already-logged"));
        clearPromptLogSnapshot();
        return;
    }

    if (usingLegacyBackendForTx()) {
        bridgeLog("logQso: delegating to legacy backend");
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        QString legacyLogPropMode;
        QString legacyLogSatellite;
        QString legacyLogSatMode;
        bool const legacySaveSatellite = getSetting(QStringLiteral("SaveSatellite"), false).toBool();
        bool const legacySaveSatMode = getSetting(QStringLiteral("SaveSatMode"), false).toBool();
        if (legacySaveSatellite) {
            legacyLogSatellite = getSetting(QStringLiteral("Satellite"), QString()).toString().trimmed();
            if (!legacyLogSatellite.isEmpty()) {
                legacyLogPropMode = QStringLiteral("SAT");
                if (legacySaveSatMode) {
                    legacyLogSatMode = getSetting(QStringLiteral("SatMode"), QString()).toString().trimmed();
                }
            }
        }
        m_legacyBackend->setNextLogPromptFields(m_promptLogComment,
                                                m_promptLogCommentOverrideValid,
                                                legacyLogPropMode,
                                                legacyLogSatellite,
                                                legacyLogSatMode,
                                                m_promptLogSnapshotValid);
        m_legacyBackend->setNextLogClusterSpotState(dxClusterConnected(),
                                                    clusterSpotRequested && dxClusterConnected());
        if (m_promptLogSnapshotValid) {
            m_legacyBackend->setNextLogPromptAlreadyAccepted();
        }
        m_legacyBackend->logQso();
        syncLegacyBackendState();
        QTimer::singleShot(250, this, [this]() {
            emit qsoCountChanged();
            emit workedCountChanged();
        });
        QString const legacyCompletedCall = completedLogCallForStateClear();
        clearPromptLogSnapshot();
        clearPendingAutoLogSnapshot();
        m_qsoLogged = true;
        clearTxArmedAfterCompletedQso(legacyCompletedCall, QStringLiteral("legacy-log"));
        emit statusMessage("Log QSO via backend legacy");
        return;
    }

    QString logDxCall = m_dxCall.trimmed();
    QString logDxGrid = m_dxGrid.trimmed();
    QString logRptSent = m_reportSent.trimmed();
    QString logRptRcvd = m_reportReceived.trimmed();
    QString logMode = m_mode.trimmed();
    double  logFreqHz = m_frequency;
    QDateTime utcOn = QDateTime::currentDateTimeUtc();
    QDateTime utcOff = utcOn;

    if (m_promptLogSnapshotValid) {
        logDxCall = m_promptLogCall;
        logDxGrid = m_promptLogGrid;
        logRptSent = m_promptLogRptSent;
        logRptRcvd = m_promptLogRptRcvd;
        if (!m_promptLogMode.isEmpty()) logMode = m_promptLogMode;
        if (m_promptLogDialFreq > 0.0) logFreqHz = m_promptLogDialFreq;
        if (m_promptLogOn.isValid()) utcOn = m_promptLogOn;
        if (m_promptLogOff.isValid()) utcOff = m_promptLogOff;
    } else if (m_pendingAutoLogValid) {
        if (!m_pendingAutoLogCall.isEmpty()) logDxCall = m_pendingAutoLogCall;
        if (!m_pendingAutoLogGrid.isEmpty()) logDxGrid = m_pendingAutoLogGrid;
        if (!m_pendingAutoLogRptSent.isEmpty()) logRptSent = m_pendingAutoLogRptSent;
        if (!m_pendingAutoLogRptRcvd.isEmpty()) logRptRcvd = m_pendingAutoLogRptRcvd;
        if (m_pendingAutoLogDialFreq > 0.0) logFreqHz = m_pendingAutoLogDialFreq;
        if (m_pendingAutoLogOn.isValid()) utcOn = m_pendingAutoLogOn;
        if (m_pendingAutoLogOff.isValid()) utcOff = m_pendingAutoLogOff;
    }

    if (logDxCall.isEmpty() && m_lateAutoLogValid && m_lateAutoLogExpires >= QDateTime::currentDateTimeUtc()) {
        logDxCall = m_lateAutoLogCall;
        logDxGrid = m_lateAutoLogGrid;
        logRptSent = m_lateAutoLogRptSent;
        logRptRcvd = m_lateAutoLogRptRcvd;
        if (m_lateAutoLogDialFreq > 0.0) logFreqHz = m_lateAutoLogDialFreq;
        if (m_lateAutoLogOn.isValid()) utcOn = m_lateAutoLogOn;
        if (m_lateAutoLogOff.isValid()) utcOff = m_lateAutoLogOff;
    }

    if (!utcOn.isValid()) {
        utcOn = QDateTime::currentDateTimeUtc();
    }
    if (!utcOff.isValid()) {
        utcOff = utcOn;
    }
    if (utcOff < utcOn) {
        utcOff = utcOn;
    }

    if (logDxCall.isEmpty()) {
        clearPromptLogSnapshot();
        traceLogStep(QStringLiteral("empty-call"));
        return;
    }
    traceLogStep(QStringLiteral("snapshot-ready"));

    QString const dedupeCall = Radio::base_callsign(logDxCall).trimmed().toUpper();
    QString const dedupeBand = autoCqBandKeyForFrequency(logFreqHz).trimmed().toUpper();
    QString const dedupeMode = logMode.trimmed().toUpper();
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
        clearTxArmedAfterCompletedQso(logDxCall, QStringLiteral("duplicate-log-suppressed"));
        clearPromptLogSnapshot();
        clearPendingAutoLogSnapshot();
        traceLogStep(QStringLiteral("duplicate-suppressed"));
        return;
    }

    QString logPropMode;
    QString logSatellite;
    QString logSatMode;
    QString logFreqRx;
    QString logComments = m_promptLogCommentOverrideValid
        ? m_promptLogComment
        : defaultLogCommentForQso(logMode, logRptSent, logRptRcvd);
    bool const saveSatellite = getSetting(QStringLiteral("SaveSatellite"), false).toBool();
    bool const saveSatMode = getSetting(QStringLiteral("SaveSatMode"), false).toBool();
    if (saveSatellite) {
        logSatellite = getSetting(QStringLiteral("Satellite"), QString()).toString().trimmed();
        if (!logSatellite.isEmpty()) {
            logPropMode = QStringLiteral("SAT");
            if (saveSatMode) {
                logSatMode = getSetting(QStringLiteral("SatMode"), QString()).toString().trimmed();
            }
        }
    }

    // 1) Log custom (all.txt) — compatibilità esistente
    QString logPath = logAllTxtPath();
    QDir().mkpath(QFileInfo(logPath).absolutePath());
    QFile f(logPath);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream s(&f);
        s << utcOn.toString("yyyyMMdd_hhmmss")
          << " " << m_callsign
          << " " << logDxCall
          << " " << logDxGrid
          << " " << QString::number(logFreqHz / 1e6, 'f', 6) << "MHz"
          << " " << logMode
          << "\n";
    }
    traceLogStep(QStringLiteral("alltxt-written"));

    // 2) Log ADIF (decodium_log.adi) — per import/export e B4 check
    QByteArray const adifRecord = bridgeAdifRecordText(logDxCall, logDxGrid, logFreqHz, logMode, utcOn, utcOff,
                                                       logRptSent, logRptRcvd, m_callsign, m_grid,
                                                       logComments, logPropMode, logSatellite, logSatMode, logFreqRx).toUtf8();
    traceLogStep(QStringLiteral("adif-record-built"));
    appendAdifRecord(logDxCall, logDxGrid, logFreqHz, logMode, utcOn, utcOff,
                     logRptSent, logRptRcvd,
                     logComments, logPropMode, logSatellite, logSatMode, logFreqRx);
    traceLogStep(QStringLiteral("adif-appended"));

    // 3) Inoltro ai log esterni compatibili WSJT-X UDP / N1MM.
    udpSendLoggedQso(logDxCall, logDxGrid, logFreqHz, logMode, utcOn, utcOff,
                     logRptSent, logRptRcvd, adifRecord, logComments,
                     logPropMode, logSatellite, logSatMode, logFreqRx);
    traceLogStep(QStringLiteral("udp-sent"));
    udpSendN1mmLoggedQso(logDxCall, adifRecord);
    traceLogStep(QStringLiteral("n1mm-sent"));
    tcpSendLoggedAdifQso(logDxCall, adifRecord);
    traceLogStep(QStringLiteral("adif-tcp-started"));

    // 4) Cloudlog upload
    bool snrOk = false;
    int const snr = logRptSent.toInt(&snrOk);
    if (m_cloudlogEnabled && m_cloudlog) {
        m_cloudlog->logQso(logDxCall, logDxGrid, logFreqHz, logMode, utcOn,
                           snrOk ? snr : 0, logRptSent, logRptRcvd,
                           m_callsign, m_grid);
    }
    traceLogStep(QStringLiteral("cloudlog-started"));

    if (clusterSpotRequested) {
        if (m_dxCluster && m_dxCluster->connected()) {
            QString const spotCall = !dedupeCall.isEmpty()
                ? dedupeCall
                : Radio::base_callsign(logDxCall).trimmed().toUpper();
            QStringList commentParts;
            QString const cleanMode = logMode.trimmed().toUpper();
            if (!cleanMode.isEmpty()) {
                commentParts << cleanMode;
            }
            if (!logDxGrid.trimmed().isEmpty()) {
                commentParts << logDxGrid.trimmed().toUpper();
            }
            commentParts << QStringLiteral("Decodium");
            m_dxCluster->submitSpotVerified(spotCall,
                                             logFreqHz / 1000.0,
                                             commentParts.join(QLatin1Char(' ')));
        } else {
            emit statusMessage(QStringLiteral("DX Cluster spot non inviato: cluster non connesso"));
        }
    }
    traceLogStep(QStringLiteral("cluster-handled"));

    rememberRecentAutoCqWorked(logDxCall, logFreqHz, logMode);
    removeCallerFromQueue(dedupeCall);
    if (m_lateAutoLogValid &&
        dedupeCall == Radio::base_callsign(m_lateAutoLogCall).trimmed().toUpper()) {
        clearLateAutoLogSnapshot();
    }
    clearPromptLogSnapshot();
    clearPendingAutoLogSnapshot();
    m_qsoLogged = true;  // impedisce doppio log per questo QSO
    clearTxArmedAfterCompletedQso(logDxCall, QStringLiteral("log"));
    emit statusMessage("QSO loggato: " + logDxCall);
    traceLogStep(QStringLiteral("done"));
}

QString DecodiumBridge::logAllTxtPath() const
{
    QString const effectivePath = legacyAllTxtPath().trimmed();
    if (!effectivePath.isEmpty()) {
        return effectivePath;
    }

    QString const appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty()) {
        return QDir(appData).absoluteFilePath(QStringLiteral("ALL.TXT"));
    }

    return QStringLiteral("ALL.TXT");
}

bool DecodiumBridge::openAllTxtFolder() const
{
    QString path = logAllTxtPath().trimmed();
    if (path.isEmpty()) {
        return false;
    }

    QFileInfo const info(path);
    QDir dir = info.exists() ? info.absoluteDir() : QFileInfo(info.absolutePath()).absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    return QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
}

// === Shannon FDR (False Decode Reduction) — 4-level filter ===
static bool isValidDecode(const QString& msg, int snr, const QString& aptype, float quality)
{
    Q_UNUSED(snr);
    Q_UNUSED(aptype);
    Q_UNUSED(quality);

    QString const trimmed = msg.trimmed();

    // Manteniamo solo guardrail minimi contro linee palesemente corrotte.
    // Il filtro aggressivo del bridge moderno non esiste in Decodium3 e
    // stava nascondendo decode validi e deboli nel confronto diretto.
    if (trimmed.length() < 2) return false;
    if (trimmed.contains(QStringLiteral("> <")) || trimmed.contains(QStringLiteral("< >"))) {
        return false;
    }

    return true;
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

// Estrae il secondo callsign dal messaggio (quello a destra).
// "CQ IU8LMC JN70" → IU8LMC (primo call dopo CQ)
// "IU8LMC 9H1SR -05" → 9H1SR (secondo call)
// "9H1SR IU8LMC R-05" → IU8LMC (secondo call)
static QString extractRightCallsign(const QString& msg)
{
    QStringList parts = msg.toUpper().split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return {};

    // Raccogli tutti i token che sembrano callsign
    QStringList calls;
    for (const QString& p : parts) {
        if (p == "CQ" || p == "DX" || p == "QRZ" || p == "DE" || p == "TEST") continue;
        if (looksLikeCallsignToken(p))
            calls.append(p);
    }

    // Ritorna l'ultimo callsign trovato (il più a destra)
    if (calls.size() >= 2)
        return calls[1];   // secondo call = destra
    if (calls.size() == 1)
        return calls[0];   // CQ con un solo call
    return {};
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

bool DecodiumBridge::contestDecodeExchangesEnabled() const
{
    int const activity = qBound(kSpecialOpNone, m_specialOperationActivity, kSpecialOpMax);
    bool const vhfGridContest = activity == 1 || activity == 2;
    return vhfGridContest
        || m_contestType > 0
        || getSetting(QStringLiteral("ContestingOnly"), false).toBool();
}

bool DecodiumBridge::shouldAcceptDecodedMessage(const QString& message, QString* reason) const
{
    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);
    if (myCallUpper.isEmpty() && myBaseUpper.isEmpty()) {
        return true;
    }

    QStringList const tokens = normalizedMessageTokens(message);
    if (tokens.isEmpty()) {
        if (reason) *reason = QStringLiteral("empty message");
        return false;
    }

    auto tokenIsMine = [&](int index) {
        return index >= 0
            && index < tokens.size()
            && tokenMatchesCall(tokens.at(index), myCallUpper, myBaseUpper);
    };

    QString const first = tokens.constFirst();
    if (first == QStringLiteral("CQ") || first == QStringLiteral("QRZ")) {
        for (int i = 1; i < tokens.size(); ++i) {
            if (tokenIsMine(i)) {
                if (reason) *reason = QStringLiteral("self CQ decoded on RX");
                return false;
            }
        }
        return true;
    }

    bool const directedToOrFromMe = tokenIsMine(0) || tokenIsMine(1);
    if (!directedToOrFromMe) {
        return true;
    }

    if (tokens.size() < 3) {
        if (reason) *reason = QStringLiteral("directed message without payload");
        return false;
    }

    QStringList payload = tokens.mid(2);
    return isDirectedDecodePayloadValid(payload, contestDecodeExchangesEnabled(), reason);
}

void DecodiumBridge::rememberWorldMapGrid(const QString& call, const QString& grid)
{
    QString const key = worldMapCallKey(call);
    QString locator = grid.trimmed().toUpper();
    if (key.isEmpty() || !isGridTokenStrict(locator)) {
        return;
    }

    m_worldMapGridByCall.insert(key, locator.left(qMin(6, locator.size())));
}

void DecodiumBridge::loadWorldMapCall3Cache()
{
    if (m_worldMapCall3Loaded) {
        return;
    }
    m_worldMapCall3Loaded = true;

    QString const appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString const appLocalData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString const appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;
    if (!appData.isEmpty()) {
        candidates << QDir(appData).absoluteFilePath(QStringLiteral("CALL3.TXT"));
    }
    if (!appLocalData.isEmpty() && appLocalData != appData) {
        candidates << QDir(appLocalData).absoluteFilePath(QStringLiteral("CALL3.TXT"));
    }
    candidates << QDir(appDir).absoluteFilePath(QStringLiteral("CALL3.TXT"))
               << QDir(appDir).absoluteFilePath(QStringLiteral("../CALL3.TXT"))
               << QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/CALL3.TXT"))
               << QDir::current().absoluteFilePath(QStringLiteral("CALL3.TXT"))
               << QDir(QStringLiteral(CMAKE_SOURCE_DIR)).absoluteFilePath(QStringLiteral("CALL3.TXT"));

    QSet<QString> openedPaths;
    for (QString const& path : candidates) {
        if (path.isEmpty() || openedPaths.contains(path)) {
            continue;
        }
        openedPaths.insert(path);

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        int loadedFromFile = 0;
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString const line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char(';'))) {
                continue;
            }

            int const comma1 = line.indexOf(QLatin1Char(','));
            if (comma1 <= 0) {
                continue;
            }
            int comma2 = line.indexOf(QLatin1Char(','), comma1 + 1);
            if (comma2 < 0) {
                comma2 = line.size();
            }

            QString const call = normalizeWorldMapCall(line.left(comma1));
            QString const locator = line.mid(comma1 + 1, comma2 - comma1 - 1).trimmed().toUpper();
            if (!call.isEmpty() && isGridTokenStrict(locator)) {
                rememberWorldMapGrid(call, locator);
                ++loadedFromFile;
            }
        }
        if (loadedFromFile > 0) {
            bridgeLog(QStringLiteral("WorldMap CALL3 loaded %1 grids from %2")
                          .arg(loadedFromFile)
                          .arg(path));
            return;
        }
    }
}

QString DecodiumBridge::lookupWorldMapGrid(const QString& call)
{
    QString const key = worldMapCallKey(call);
    if (key.isEmpty()) {
        return {};
    }

    QString locator = m_worldMapGridByCall.value(key);
    if (locator.isEmpty()) {
        loadWorldMapCall3Cache();
        locator = m_worldMapGridByCall.value(key);
    }
    return isGridTokenStrict(locator) ? locator.left(qMin(6, locator.size())) : QString {};
}

QString DecodiumBridge::approximateWorldMapGridForCall(const QString& call)
{
    QString const mapCall = normalizeWorldMapCall(call);
    if (mapCall.isEmpty() || !m_dxccLookup) {
        return {};
    }

    if (!m_dxccLookup->isLoaded()) {
        QString loadedPath;
        reloadDxccLookup(&loadedPath);
    }
    if (!m_dxccLookup->isLoaded()) {
        return {};
    }

    QStringList lookupCandidates;
    lookupCandidates << mapCall;
    QString const baseCall = Radio::base_callsign(mapCall).trimmed().toUpper();
    if (!baseCall.isEmpty()) {
        lookupCandidates << baseCall;
    }
    int const slashPos = mapCall.indexOf(QLatin1Char('/'));
    if (slashPos > 0) {
        QString const left = mapCall.left(slashPos).trimmed().toUpper();
        QString const right = mapCall.mid(slashPos + 1).trimmed().toUpper();
        if (!left.isEmpty()) {
            lookupCandidates << left;
        }
        if (!right.isEmpty()) {
            lookupCandidates << right;
        }
    }
    lookupCandidates.removeDuplicates();

    DxccEntity entity;
    for (QString const& candidate : std::as_const(lookupCandidates)) {
        entity = m_dxccLookup->lookup(candidate);
        if (entity.isValid() && std::isfinite(entity.lat) && std::isfinite(entity.lon)) {
            break;
        }
        entity = {};
    }
    if (!entity.isValid()) {
        return {};
    }

    // cty.dat stores longitude as positive west; map/grid code uses positive east.
    QString const locator = maidenheadFromLonLat(-entity.lon, entity.lat);
    return isGridTokenStrict(locator) ? locator : QString {};
}

void DecodiumBridge::replayWorldMapEntry(const QVariantMap& entry)
{
    QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
    if (message.isEmpty()) {
        return;
    }

    QString myGrid = m_grid.trimmed().toUpper();
    if (!isGridTokenStrict(myGrid)) {
        return;
    }
    myGrid = myGrid.left(qMin(6, myGrid.size()));

    QStringList const words = normalizedMessageTokens(message);
    if (words.isEmpty()) {
        return;
    }

    QString const myCallKey = worldMapCallKey(m_callsign);
    auto isMine = [&myCallKey](QString const& call) {
        QString const key = worldMapCallKey(call);
        return !key.isEmpty() && !myCallKey.isEmpty() && key == myCallKey;
    };

    bool const isTx = entry.value(QStringLiteral("isTx")).toBool();
    bool const isCqLike = words.first() == QStringLiteral("CQ")
        || words.first() == QStringLiteral("QRZ")
        || words.first() == QStringLiteral("DE");
    bool const isEndOfQso = words.contains(QStringLiteral("73"))
        || words.contains(QStringLiteral("RR73"));

    QString entryGrid = entry.value(QStringLiteral("dxGrid")).toString().trimmed().toUpper();
    if (!isGridTokenStrict(entryGrid)) {
        entryGrid = extractDecodedGrid(message);
    }
    if (isGridTokenStrict(entryGrid)) {
        entryGrid = entryGrid.left(qMin(6, entryGrid.size()));
    } else {
        entryGrid.clear();
    }

    QStringList calls;
    calls.reserve(words.size());
    for (QString const& word : words) {
        if (word == QStringLiteral("CQ") || word == QStringLiteral("QRZ") || word == QStringLiteral("DE")
            || word == QStringLiteral("DX") || word == QStringLiteral("TEST")) {
            continue;
        }
        if (looksLikeCallsignToken(word)) {
            QString const call = normalizeWorldMapCall(word);
            if (!call.isEmpty()) {
                calls << call;
            }
        }
    }

    QString entryCall = normalizeWorldMapCall(entry.value(QStringLiteral("fromCall")).toString());
    if (entryCall.isEmpty()) {
        entryCall = normalizeWorldMapCall(entry.value(QStringLiteral("dxCallsign")).toString());
    }
    if (entryCall.isEmpty() && !calls.isEmpty()) {
        entryCall = isCqLike ? calls.first() : calls.last();
    }

    if (!entryGrid.isEmpty()) {
        if (isCqLike && !calls.isEmpty()) {
            rememberWorldMapGrid(calls.first(), entryGrid);
        } else if (calls.size() >= 2) {
            // Standard messages are TO FROM GRID: the locator belongs to FROM.
            rememberWorldMapGrid(calls.at(1), entryGrid);
        } else if (!entryCall.isEmpty()) {
            rememberWorldMapGrid(entryCall, entryGrid);
        }
    }

    auto lookupDxGridFor = [this](QString const& call) {
        QString locator = lookupWorldMapGrid(call);
        if (locator.isEmpty()
            && worldMapCallKey(call) == worldMapCallKey(m_dxCall)
            && isGridTokenStrict(m_dxGrid.trimmed().toUpper())) {
            locator = m_dxGrid.trimmed().toUpper().left(qMin(6, m_dxGrid.trimmed().size()));
        }
        return locator;
    };

    static constexpr int roleIncomingToMe = 1;
    static constexpr int roleOutgoingFromMe = 2;
    static constexpr int roleBandOnly = 3;

    auto emitApproximateContact = [this, &myGrid](QString const& call, int role) {
        QString const mapCall = normalizeWorldMapCall(call);
        QString const approxGrid = approximateWorldMapGridForCall(mapCall);
        if (mapCall.isEmpty() || !isGridTokenStrict(approxGrid)) {
            return false;
        }

        if (role == roleOutgoingFromMe) {
            emit worldMapContactAdded(mapCall, myGrid, approxGrid, role);
        } else if (role == roleIncomingToMe) {
            emit worldMapContactAdded(mapCall, approxGrid, myGrid, role);
        } else {
            emit worldMapContactAdded(mapCall, approxGrid, approxGrid, roleBandOnly);
        }
        return true;
    };

    QString remoteCall;
    QString remoteGrid;
    int role = roleBandOnly;

    if (isTx) {
        if (isCqLike) {
            return;
        }
        for (QString const& call : std::as_const(calls)) {
            if (!isMine(call)) {
                remoteCall = call;
                break;
            }
        }
        if (remoteCall.isEmpty()) {
            remoteCall = normalizeWorldMapCall(m_dxCall);
        }
        remoteGrid = lookupDxGridFor(remoteCall);
        if (!remoteCall.isEmpty() && isGridTokenStrict(remoteGrid)) {
            emit worldMapContactAdded(remoteCall, myGrid, remoteGrid, roleOutgoingFromMe);
        } else {
            emitApproximateContact(remoteCall, roleOutgoingFromMe);
        }
        return;
    }

    bool const outgoingFromMe = !isCqLike
        && calls.size() >= 2
        && isMine(calls.at(1))
        && !isMine(calls.at(0));
    bool const incomingToMe = !isCqLike
        && calls.size() >= 2
        && isMine(calls.at(0))
        && !isMine(calls.at(1));

    if (outgoingFromMe) {
        remoteCall = calls.at(0);
        remoteGrid = lookupDxGridFor(remoteCall);
        role = roleOutgoingFromMe;
        if (isEndOfQso && !remoteCall.isEmpty()) {
            emit worldMapContactDowngraded(remoteCall);
            role = roleBandOnly;
        }
        if (isGridTokenStrict(remoteGrid)) {
            emit worldMapContactAdded(remoteCall,
                                      role == roleOutgoingFromMe ? myGrid : remoteGrid,
                                      remoteGrid,
                                      role);
        } else {
            emitApproximateContact(remoteCall, role);
        }
        return;
    }

    if (incomingToMe) {
        remoteCall = calls.at(1);
        remoteGrid = !entryGrid.isEmpty() ? entryGrid : lookupDxGridFor(remoteCall);
        role = roleIncomingToMe;
        if (isEndOfQso && !remoteCall.isEmpty()) {
            emit worldMapContactDowngraded(remoteCall);
            role = roleBandOnly;
        }
        if (isGridTokenStrict(remoteGrid)) {
            emit worldMapContactAdded(remoteCall,
                                      remoteGrid,
                                      role == roleIncomingToMe ? myGrid : remoteGrid,
                                      role);
            return;
        }
        emitApproximateContact(remoteCall, role);
        return;
    }

    if (isCqLike && !calls.isEmpty()) {
        remoteCall = calls.first();
        remoteGrid = !entryGrid.isEmpty() ? entryGrid : lookupDxGridFor(remoteCall);
    } else if (!entryCall.isEmpty() && !isMine(entryCall)) {
        remoteCall = entryCall;
        remoteGrid = !entryGrid.isEmpty() ? entryGrid : lookupDxGridFor(remoteCall);
    } else if (calls.size() >= 2 && !isMine(calls.at(1))) {
        remoteCall = calls.at(1);
        remoteGrid = !entryGrid.isEmpty() ? entryGrid : lookupDxGridFor(remoteCall);
    } else if (!calls.isEmpty() && !isMine(calls.first())) {
        remoteCall = calls.first();
        remoteGrid = !entryGrid.isEmpty() ? entryGrid : lookupDxGridFor(remoteCall);
    }

    if (isEndOfQso && !remoteCall.isEmpty()) {
        emit worldMapContactDowngraded(remoteCall);
    }
    if (remoteCall.isEmpty()) {
        return;
    }
    if (isGridTokenStrict(remoteGrid)) {
        emit worldMapContactAdded(remoteCall, remoteGrid, remoteGrid, roleBandOnly);
        return;
    }
    if (getSetting(QStringLiteral("MapAllMessages"), false).toBool()) {
        emitApproximateContact(remoteCall, roleBandOnly);
    }
}

void DecodiumBridge::emitCurrentWorldMapQsoPath()
{
    QString myGrid = m_grid.trimmed().toUpper();
    if (!isGridTokenStrict(myGrid)) {
        return;
    }
    myGrid = myGrid.left(qMin(6, myGrid.size()));

    QString remoteCall = normalizeWorldMapCall(m_dxCall);
    if (remoteCall.isEmpty()) {
        remoteCall = normalizeWorldMapCall(inferredPartnerForAutolog());
    }
    if (remoteCall.isEmpty() || worldMapCallKey(remoteCall) == worldMapCallKey(m_callsign)) {
        return;
    }

    QString remoteGrid = m_dxGrid.trimmed().toUpper();
    if (isGridTokenStrict(remoteGrid)) {
        remoteGrid = remoteGrid.left(qMin(6, remoteGrid.size()));
        rememberWorldMapGrid(remoteCall, remoteGrid);
    } else {
        remoteGrid = lookupWorldMapGrid(remoteCall);
    }
    if (!isGridTokenStrict(remoteGrid)) {
        remoteGrid = approximateWorldMapGridForCall(remoteCall);
    }
    if (!isGridTokenStrict(remoteGrid)) {
        return;
    }

    static constexpr int roleOutgoingFromMe = 2;
    emit worldMapContactAdded(remoteCall, myGrid, remoteGrid, roleOutgoingFromMe);
}

void DecodiumBridge::replayWorldMapFeed()
{
    QSet<QString> seen;
    auto replayList = [this, &seen](QVariantList const& entries) {
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            QString key = decodeMirrorEntryKey(entry);
            if (key.isEmpty()) {
                key = entry.value(QStringLiteral("message")).toString().trimmed();
            }
            if (!key.isEmpty() && seen.contains(key)) {
                continue;
            }
            if (!key.isEmpty()) {
                seen.insert(key);
            }
            replayWorldMapEntry(entry);
        }
    };

    replayList(m_decodeList);
    replayList(m_rxDecodeList);
    emitCurrentWorldMapQsoPath();
}

void DecodiumBridge::processMapContactClick(const QString& call, const QString& grid)
{
    QString const mapCall = normalizeWorldMapCall(call);
    if (mapCall.isEmpty()) {
        return;
    }

    QString mapGrid = grid.trimmed().toUpper();
    if (isGridTokenStrict(mapGrid)) {
        mapGrid = mapGrid.left(qMin(6, mapGrid.size()));
        rememberWorldMapGrid(mapCall, mapGrid);
    } else {
        mapGrid = lookupWorldMapGrid(mapCall);
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    int const dblIntervalMs = qMax(180, QApplication::doubleClickInterval());
    bool const isDoubleClick = !m_mapLastClickCall.isEmpty()
        && m_mapLastClickCall == mapCall
        && m_mapLastClickMs > 0
        && (nowMs - m_mapLastClickMs) >= 0
        && (nowMs - m_mapLastClickMs) <= (dblIntervalMs + 120);
    m_mapLastClickCall = mapCall;
    m_mapLastClickMs = nowMs;

    if (m_autoCqRepeat) {
        setAutoCqRepeat(false);
    }
    clearCallerQueue();
    clearAutoCqPartnerLock();
    clearManualTxHold(QStringLiteral("map-contact-click"));

    bool const newQso = worldMapCallKey(mapCall) != worldMapCallKey(m_dxCall);
    setDxCall(mapCall);
    if (isGridTokenStrict(mapGrid)) {
        setDxGrid(mapGrid);
    }

    m_nTx73 = 0;
    m_txRetryCount = 0;
    m_lastNtx = -1;
    m_lastCqPidx = -1;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;
    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    m_quickPeerSignaled = false;
    m_qsoLogged = false;
    m_lastTransmittedMessage.clear();
    m_qsoStartedOn = QDateTime::currentDateTimeUtc();
    clearPendingAutoLogSnapshot();
    if (newQso) {
        clearAutoCqPartnerLock();
    }

    genStdMsgs(mapCall, mapGrid);
    advanceQsoState(1);
    updateAutoCqPartnerLock();

    bool const singleClickStartsTx = getSetting(QStringLiteral("MapSingleClickTX"), false).toBool();
    bool const startTxNow = singleClickStartsTx ? !isDoubleClick : isDoubleClick;
    if (startTxNow) {
        setTxEnabled(true);
        m_mapLastClickCall.clear();
        m_mapLastClickMs = 0;
    }

    emit statusMessage(isGridTokenStrict(mapGrid)
                           ? tr("Map selection: %1 %2").arg(mapCall, mapGrid)
                           : tr("Map selection: %1").arg(mapCall));
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

    // DXCC lookup: usa il callsign di DESTRA nel messaggio (il secondo call)
    QString rightCall = extractRightCallsign(msg);
    if (rightCall.isEmpty()) rightCall = fromCall;

    QString const myBase = normalizedBaseCall(m_callsign);
    auto sameBaseCall = [&myBase](QString const& call) {
        QString const base = normalizedBaseCall(call);
        return !myBase.isEmpty() && !base.isEmpty() && base == myBase;
    };

    QString firstCall;
    QStringList const tokens = normalizedMessageTokens(msg);
    for (QString const& token : tokens) {
        if (token == QStringLiteral("CQ")
            || token == QStringLiteral("DX")
            || token == QStringLiteral("QRZ")
            || token == QStringLiteral("DE")
            || token == QStringLiteral("TEST")) {
            continue;
        }
        if (looksLikeCallsignToken(token)) {
            firstCall = token;
            break;
        }
    }
    bool const localTxEntry = entry.value(QStringLiteral("isTx")).toBool()
        || entry.value(QStringLiteral("db")).toString().trimmed().compare(QStringLiteral("TX"), Qt::CaseInsensitive) == 0;
    bool const selfEntry = localTxEntry || sameBaseCall(fromCall) || sameBaseCall(firstCall);

    QString dxCountry;
    QString dxContinent;
    QString dxPrefix;
    bool dxIsNewCountry = false;
    if (m_dxccLookup && m_dxccLookup->isLoaded() && !rightCall.isEmpty()) {
        DxccEntity ent = m_dxccLookup->lookup(rightCall);
        if (ent.isValid()) {
            dxCountry = ent.name;
            dxContinent = ent.continent;
            dxPrefix = ent.prefix;
        }
    }

    QString dxGridExtracted = extractDecodedGrid(msg);
    double distKm = -1.0;
    double bearing = -1.0;
    if (!selfEntry && !dxGridExtracted.isEmpty() && !m_grid.isEmpty()) {
        bearing = calcBearing(m_grid, dxGridExtracted);
        distKm = calcDistance(m_grid, dxGridExtracted);
    }

    entry["fromCall"] = fromCall;
    entry["isB4"] = isB4;
    entry["isLotw"] = isLotw;
    entry["dxCountry"] = dxCountry;
    entry["dxCallsign"] = rightCall;
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
    if (shouldIgnoreDecodeCallbacks()) {
        bridgeLog("onFt8DecodeReady: ignored during shutdown");
        return;
    }

    if (m_ft8EarlyDecodeSerials.remove(serial) > 0) {
        bridgeLog("onFt8DecodeReady: hidden FT8 early stage serial=" + QString::number(serial) +
                  " rows=" + QString::number(rows.size()));
        return;
    }

    if (isTimeSyncDecodeMode(m_mode)) {
        QString const serialMode = m_decodeModeBySerial.value(serial);
        if (serialMode.isEmpty()) {
            bridgeLog("onFt8DecodeReady: ignored stale time-sync callback serial=" + QString::number(serial) +
                      " currentMode=" + m_mode);
            return;
        }
        if (serialMode != m_mode) {
            bridgeLog("onFt8DecodeReady: ignored stale " + serialMode +
                      " callback after mode change to " + m_mode +
                      " serial=" + QString::number(serial));
            m_decodeStartMsBySerial.remove(serial);
            m_decodeModeBySerial.remove(serial);
            m_decodeUtcTokenBySerial.remove(serial);
            return;
        }
    }

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

    QString const decodeMode = m_decodeModeBySerial.value(serial);
    QString const forcedUtcToken = m_decodeUtcTokenBySerial.value(serial);
    bool const trackTimeSync = isTimeSyncDecodeMode(decodeMode);
    bool const legacyUiMirrorActive = usingLegacyBackendForTx();
    QVector<double> dtSamples;
    bool changed = false;
    int parseFailures = 0;
    int guardrailFiltered = 0;
    int semanticFiltered = 0;
    int uiFiltered = 0;
    int duplicatesSkipped = 0;
    int accepted = 0;
    for (const auto& row : rows) {
        // parseFt8Row returns: [time, snr, dt, df, message, aptype, quality, freq]
        QStringList f = parseFt8Row(row);
        if (f.size() < 8) {
            bridgeLog("  parseFt8Row FAILED for: '" + row + "'");
            ++parseFailures;
            continue;
        }

        // FDR: Shannon False Decode Reduction — 4 livelli di filtro
        {
            int snrVal = f[1].trimmed().toInt();
            float qualVal = f[6].trimmed().toFloat();
            if (!isValidDecode(f[4], snrVal, f[5], qualVal)) {
                bridgeLog("FDR: filtered decode: " + f[4]);
                ++guardrailFiltered;
                continue;  // salta questo decode
            }
        }

        QString semanticRejectReason;
        if (!shouldAcceptDecodedMessage(f[4], &semanticRejectReason)) {
            bridgeLog(QStringLiteral("semantic decode filter: %1 mode=%2 msg='%3'")
                          .arg(semanticRejectReason, decodeMode.isEmpty() ? m_mode : decodeMode, f[4]));
            ++semanticFiltered;
            continue;
        }

        if (trackTimeSync) {
            bool snrOk = false;
            bool dtOk = false;
            int const snrVal = f[1].trimmed().toInt(&snrOk);
            double const dtVal = f[2].trimmed().toDouble(&dtOk);
            if (snrOk && dtOk && shouldTrackDtSample(snrVal, dtVal, f[4], decodeMode)) {
                dtSamples.append(dtVal);
            }
        }

        // Calcola proprietà derivate
        QString msg     = f[4];
        bool isCQ       = msg.startsWith("CQ ", Qt::CaseInsensitive) || msg == "CQ";
        QString const myCallUpper = m_callsign.trimmed().toUpper();
        QString const myBaseUpper = normalizedBaseCall(myCallUpper);
        bool isMyCall   = messageContainsCallToken(msg, myCallUpper, myBaseUpper);
        QString fromCall = extractDecodedCallsign(msg, isCQ);

        QString const entryTime = (trackTimeSync && !forcedUtcToken.isEmpty()) ? forcedUtcToken : f[0];
        QVariantMap entry;
        entry["time"]    = entryTime;
        entry["db"]      = f[1];
        entry["dt"]      = f[2];
        entry["freq"]    = f[7];   // frequenza assoluta audio Hz
        entry["message"] = msg;
        entry["aptype"]  = f[5];
        entry["quality"] = f[6];
        entry["mode"]    = decodeMode.isEmpty() ? m_mode : decodeMode;
        entry["isTx"]    = false;
        entry["isCQ"]    = isCQ;
        entry["isMyCall"] = isMyCall;
        entry["fromCall"] = fromCall;
        entry["decodeSessionId"] = static_cast<qulonglong>(m_decodeSessionId);
        enrichDecodeEntry(entry);
        maybeEnqueueMamCallerFromDecode(f);

        bool const filteredByCqOnly = m_filterCqOnly && !isCQ;
        bool const filteredByMyCallOnly = m_filterMyCallOnly && !isMyCall;
        if (filteredByCqOnly || filteredByMyCallOnly) {
            if (!legacyUiMirrorActive && filteredByCqOnly && !filteredByMyCallOnly) {
                appendRxDecodeEntry(entry);
            }
            ++uiFiltered;
            continue;
        }

        // Dedup early before side effects: FT4 early and final decodes can overlap.
        if (!legacyUiMirrorActive) {
            bool isDupe = false;
            int listSize = m_decodeList.size();
            QString const dedupKey = decodeDedupKey(entry.value("time").toString(),
                                                    entry.value("freq").toString(),
                                                    msg);
            for (int di = qMax(0, listSize - 300); di < listSize; ++di) {
                QVariantMap prev = m_decodeList[di].toMap();
                if (prev.value("isTx").toBool()) {
                    continue;
                }
                if (prev.value("decodeSessionId").toULongLong() != m_decodeSessionId) {
                    continue;
                }
                if (decodeDedupKey(prev.value("time").toString(),
                                   prev.value("freq").toString(),
                                   prev.value("message").toString()) == dedupKey) {
                    isDupe = true;
                    break;
                }
            }
            if (isDupe) {
                ++duplicatesSkipped;
                continue;
            }
        }

        // B9 — Feed ActiveStationsModel: extract callsign from CQ messages
        if (isCQ && m_activeStations) {
            QString dxGridExtracted = entry.value("dxGrid").toString();
            if (!fromCall.isEmpty()) {
                int freqHz = f[7].toInt();
                QString utc = entryTime;
                int snr = f[1].toInt();
                m_activeStations->addStation(fromCall, freqHz, snr, dxGridExtracted, utc);
            }
        }

        // In modalita' legacy embedded il pannello sinistro deve seguire solo il
        // mirror del backend legacy, non un mix con il decoder moderno.
        if (!legacyUiMirrorActive) {
            QString allPath = logAllTxtPath();
            QDir().mkpath(QFileInfo(allPath).absolutePath());
            QFile allFile(allPath);
            if (allFile.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream ts(&allFile);
                // Formato Shannon: yyMMdd_hhmmss  freq_MHz  Rx  MODE  SNR  DT  DF  Message
                QString utcNow = approxUtcDateTimeForDisplayToken(entryTime).toString("yyMMdd_hhmmss");
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

        if (!legacyUiMirrorActive) {
            m_decodeList.append(QVariant(entry));
            appendRxDecodeEntry(entry);
            appendLegacyAllTxtDecodeLine(entry);
            changed = true;
        }
        ++accepted;

        // UDP: invia decode a programmi esterni (JTAlert, GridTracker…)
        udpSendDecode(true, row, serial);

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
    if (changed) {
        normalizeDecodeEntriesForDisplay(m_decodeList, 1500, m_mode);
    }

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
            if (!shouldAcceptDecodedMessage(msgText)) continue;
            if (messageContainsCallToken(msgText, m_callsign.trimmed().toUpper(),
                                         normalizedBaseCall(m_callsign.trimmed().toUpper())))
                autoSequenceStep(f);
        }
    }

    bridgeLog(QStringLiteral("onFt8DecodeReady summary: raw=%1 accepted=%2 parse_fail=%3 guardrail=%4 semantic=%5 ui_filtered=%6 dupes=%7")
                  .arg(rows.size())
                  .arg(accepted)
                  .arg(parseFailures)
                  .arg(guardrailFiltered)
                  .arg(semanticFiltered)
                  .arg(uiFiltered)
                  .arg(duplicatesSkipped));
    if (changed) emit decodeListChanged();
    if (legacyUiMirrorActive) {
        syncLegacyBackendDecodeList();
    }
    finalizeTimeSyncDecodeCycle(serial, decodeMode, dtSamples);
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
    if (shouldIgnoreDecodeCallbacks()) {
        bridgeLog("onFt2AsyncDecodeReady: ignored during shutdown");
        return;
    }
    // Se l'utente ha cambiato modo mentre il decoder FT2 era ancora in volo,
    // il worker emette comunque il risultato via Qt::QueuedConnection.
    // Senza questo guard finiremmo per iniettare spot marker '~' (FT2) nella
    // decodeList e broadcastarli su WebSocket anche dopo lo switch a FT8/FT4.
    if (m_mode != QStringLiteral("FT2")) {
        bridgeLog("onFt2AsyncDecodeReady: ignored, mode changed to " + m_mode);
        return;
    }

    bridgeLog("onFt2AsyncDecodeReady: rows=" + QString::number(rows.size()));
    if (!rows.isEmpty()) bridgeLog("  async_raw[0]='" + rows[0] + "'");
    int const decodePeriodMs = periodMsForMode(QStringLiteral("FT2"));
    qint64 const currentSlot = decodePeriodMs > 0
        ? (QDateTime::currentMSecsSinceEpoch() / decodePeriodMs)
        : -1;
    QString const forcedUtcToken = utcDisplayTokenForSlotStart(currentSlot, decodePeriodMs);

    if (m_dxccLookup && !m_dxccLookup->isLoaded()) {
        QString loadedPath;
        if (reloadDxccLookup(&loadedPath)) {
            bridgeLog("DXCC: caricato on-demand da " + loadedPath);
            refreshDecodeListDxcc();
        }
    }

    // Costruisci set dei decode già presenti in questa sessione di monitor.
    // Usa la chiave quantizzata per FT2 async: lo stage7 DSP ri-emette lo stesso
    // messaggio ogni 100ms con piccolo jitter di freq; con quantizzazione a 20Hz
    // il primo decode vince e gli successivi vengono scartati come duplicati.
    QSet<QString> existing;
    for (const auto& v : m_decodeList) {
        QVariantMap const prev = v.toMap();
        if (prev.value("isTx").toBool()) {
            continue;
        }
        if (prev.value("decodeSessionId").toULongLong() != m_decodeSessionId) {
            continue;
        }
        QString const key = decodeDedupKeyFt2Async(prev.value("time").toString(),
                                                   prev.value("freq").toString(),
                                                   prev.value("message").toString());
        if (!key.isEmpty()) {
            existing.insert(key);
        }
    }

    bool changed = false;
    int accepted = 0;
    int duplicatesSkipped = 0;
    int uiFiltered = 0;
    int semanticFiltered = 0;
    int parseFailures = 0;
    int bestSnr = -99;
    for (const auto& row : rows) {
        QStringList f = parseFt8Row(row);
        if (f.size() < 8) {
            bridgeLog("  async parseFt8Row FAILED: '" + row + "'");
            ++parseFailures;
            continue;
        }
        QString msg = f[4];
        QString semanticRejectReason;
        if (!shouldAcceptDecodedMessage(msg, &semanticRejectReason)) {
            bridgeLog(QStringLiteral("semantic decode filter FT2 async: %1 msg='%2'")
                          .arg(semanticRejectReason, msg));
            ++semanticFiltered;
            continue;
        }
        // Aggiorna il miglior SNR (per S-meter del AsyncModeWidget)
        bool snrOk = false;
        int snrVal = f[1].toInt(&snrOk);
        if (snrOk && snrVal > bestSnr) bestSnr = snrVal;

        bool isCQ     = msg.startsWith("CQ ", Qt::CaseInsensitive) || msg == "CQ";
        QString const myCallUpper = m_callsign.trimmed().toUpper();
        QString const myBaseUpper = normalizedBaseCall(myCallUpper);
        bool isMyCall = messageContainsCallToken(msg, myCallUpper, myBaseUpper);

        QVariantMap entry;
        entry["time"]    = !forcedUtcToken.isEmpty() ? forcedUtcToken : f[0];
        entry["db"]      = f[1];
        entry["dt"]      = f[2];
        entry["freq"]    = f[7];
        entry["message"] = msg;
        entry["aptype"]  = f[5];
        entry["quality"] = f[6];
        entry["mode"]    = QStringLiteral("FT2");
        entry["isTx"]    = false;
        entry["isCQ"]    = isCQ;
        entry["isMyCall"] = isMyCall;
        entry["fromCall"] = extractDecodedCallsign(msg, isCQ);
        entry["decodeSessionId"] = static_cast<qulonglong>(m_decodeSessionId);
        enrichDecodeEntry(entry);
        maybeEnqueueMamCallerFromDecode(f);

        bool const filteredByCqOnly = m_filterCqOnly && !isCQ;
        bool const filteredByMyCallOnly = m_filterMyCallOnly && !isMyCall;
        if (filteredByCqOnly || filteredByMyCallOnly) {
            if (filteredByCqOnly && !filteredByMyCallOnly) {
                appendRxDecodeEntry(entry);
            }
            ++uiFiltered;
            continue;
        }

        QString const dedupKey = decodeDedupKeyFt2Async(entry.value("time").toString(),
                                                        entry.value("freq").toString(),
                                                        msg);
        if (existing.contains(dedupKey)) {
            ++duplicatesSkipped;
            continue;
        }
        existing.insert(dedupKey);

        m_decodeList.append(QVariant(entry));
        appendLegacyAllTxtDecodeLine(entry);
        changed = true;
        ++accepted;
    }
    if (changed) {
        normalizeDecodeEntriesForDisplay(m_decodeList, 1500, m_mode);
        emit decodeListChanged();
    }
    if (bestSnr > -99) setAsyncSnrDb(bestSnr);
    bridgeLog(QStringLiteral("onFt2AsyncDecodeReady summary: raw=%1 accepted=%2 parse_fail=%3 semantic=%4 ui_filtered=%5 dupes=%6")
                  .arg(rows.size())
                  .arg(accepted)
                  .arg(parseFailures)
                  .arg(semanticFiltered)
                  .arg(uiFiltered)
                  .arg(duplicatesSkipped));

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
            if (!shouldAcceptDecodedMessage(f[4])) continue;
            if (messageContainsCallToken(f[4], m_callsign.trimmed().toUpper(),
                                         normalizedBaseCall(m_callsign.trimmed().toUpper()))) {
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
    // Snapshot atomico unico: il callback audio (writer) può incrementare
    // m_asyncAudioPos in qualsiasi momento. Leggiamolo una sola volta per
    // garantire coerenza tra il check di soglia, il calcolo di start, e
    // la copia dei 45000 campioni.
    // uint64_t: wrap-around well-defined; aritmetica unsigned per evitare
    // indici negativi nel modulo (che produrrebbero OOB).
    uint64_t const pos = m_asyncAudioPos.load(std::memory_order_acquire);
    if (pos < 45000) return;  // non abbastanza audio ancora

    decodium::ft2::AsyncDecodeRequest req;
    req.audio.resize(45000);
    uint64_t const start = (pos - 45000) % ASYNC_BUF_SIZE;
    for (int i = 0; i < 45000; ++i)
        req.audio[i] = m_asyncAudio[(start + i) % ASYNC_BUF_SIZE];

    int const nfqso = qBound(m_nfa, m_rxFrequency, m_nfb);
    req.nqsoprogress = legacyDecodeQsoProgress();
    req.nfqso = nfqso;
    req.nfa   = m_nfa;
    req.nfb   = m_nfb;
    int const asyncDepth = effectiveDecodeDepth();
    req.ndepth   = (m_transmitting || m_tuning) ? qMin(asyncDepth, 2) : asyncDepth;
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
    if (shouldIgnoreDecodeCallbacks()) {
        bridgeLog("onWsprDecodeReady: ignored during shutdown");
        return;
    }

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
    if (shouldIgnoreDecodeCallbacks()) {
        bridgeLog("onLegacyJtDecodeReady: ignored during shutdown");
        return;
    }

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
        QString semanticRejectReason;
        if (!shouldAcceptDecodedMessage(msg, &semanticRejectReason)) {
            bridgeLog(QStringLiteral("semantic decode filter legacy-jt: %1 msg='%2'")
                          .arg(semanticRejectReason, msg));
            continue;
        }
        bool isCQ     = msg.startsWith("CQ ", Qt::CaseInsensitive) || msg == "CQ";
        QString const myCallUpper = m_callsign.trimmed().toUpper();
        QString const myBaseUpper = normalizedBaseCall(myCallUpper);
        bool isMyCall = messageContainsCallToken(msg, myCallUpper, myBaseUpper);
        QString fromCall = extractDecodedCallsign(msg, isCQ);

        QVariantMap entry;
        entry["time"]       = f[0];
        entry["db"]         = f[1];
        entry["dt"]         = f[2];
        entry["freq"]       = f[7];
        entry["message"]    = msg;
        entry["aptype"]     = f[5];
        entry["quality"]    = f[6];
        entry["mode"]       = m_mode;
        entry["isTx"]       = false;
        entry["isCQ"]       = isCQ;
        entry["isMyCall"]   = isMyCall;
        entry["fromCall"]   = fromCall;
        entry["decodeSessionId"] = static_cast<qulonglong>(m_decodeSessionId);
        enrichDecodeEntry(entry);

        bool const filteredByCqOnly = m_filterCqOnly && !isCQ;
        bool const filteredByMyCallOnly = m_filterMyCallOnly && !isMyCall;
        if (filteredByCqOnly || filteredByMyCallOnly) {
            if (filteredByCqOnly && !filteredByMyCallOnly) {
                appendRxDecodeEntry(entry);
            }
            continue;
        }

        // Deduplicazione limitata a stesso slot/frequenza/sessione.
        { bool isDupe = false; int ls = m_decodeList.size();
          QString const dedupKey = decodeDedupKey(entry.value("time").toString(),
                                                  entry.value("freq").toString(),
                                                  msg);
          for (int di = qMax(0,ls-5); di < ls; ++di) {
            QVariantMap p = m_decodeList[di].toMap();
            if (p.value("isTx").toBool()) continue;
            if (p.value("decodeSessionId").toULongLong() != m_decodeSessionId) continue;
            if (decodeDedupKey(p.value("time").toString(),
                               p.value("freq").toString(),
                               p.value("message").toString()) == dedupKey) { isDupe=true; break; }
          } if (isDupe) continue; }
        m_decodeList.append(QVariant(entry));
        appendRxDecodeEntry(entry);
        changed = true;

        // B8 — Alert sounds
        if (m_alertManager) {
            if (isCQ && m_alertOnCq)     m_alertManager->playAlert("cq");
            if (isMyCall && m_alertOnMyCall) m_alertManager->playAlert("mycall");
        }
    }
    if (changed) {
        normalizeDecodeEntriesForDisplay(m_decodeList, 1500, m_mode);
        emit decodeListChanged();
    }
}

void DecodiumBridge::onFst4DecodeReady(quint64 serial, QStringList rows)
{
    if (shouldIgnoreDecodeCallbacks()) {
        bridgeLog("onFst4DecodeReady: ignored during shutdown");
        return;
    }

    bridgeLog("onFst4DecodeReady: serial=" + QString::number(serial) +
              " rows=" + QString::number(rows.size()) + " mode=" + m_mode);
    // FST4 rows have the same format as FT8 (backtick marker), parseFt8Row works
    onFt8DecodeReady(serial, rows);
}

void DecodiumBridge::onPeriodTimer()
{
    // Progress bar e watchdog usano il counter di tick (cadenza UI invariata).
    // La decisione di alimentare il decoder è invece ancorata al boundary UTC
    // calcolato da QDateTime::currentMSecsSinceEpoch() / periodMs, così il
    // drift del QTimer (±5–50ms/tick su Windows con event loop carico) non
    // sfasa più le finestre passate al decoder.
    m_periodTicks++;
    qint64 const nowMs     = QDateTime::currentMSecsSinceEpoch();
    QString const modeSnapshot = m_mode;
    bool   const timeSyncMode = isTimeSyncDecodeMode(modeSnapshot);
    int    const periodMs  = periodMsForMode(modeSnapshot);
    qint64 const utcSlot   = (periodMs > 0) ? (nowMs / periodMs) : 0;
    int    const msInSlot  = (periodMs > 0) ? (int)(nowMs % periodMs) : 0;
    // Progress ancorato all'UTC: l'utente vede la barra sincrona al vero
    // boundary, non al counter che può driftare.
    m_periodProgress = (periodMs > 0) ? (msInSlot * 100 / periodMs) : 0;
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
            // Off means off: do not kill a valid FT8/FT4/FT2 QSO mid-sequence.
            watchdogFired = false;
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

    maybeDispatchFt8EarlyDecode(utcSlot, msInSlot, periodMs);
    maybeDispatchFt4EarlyDecode(utcSlot, msInSlot, periodMs);

    // Boundary detection basata su UTC slot: FT8/FT4/FT2 devono alimentare il
    // decoder solo quando cambia davvero lo slot UTC. Su Windows il QTimer puo'
    // scattare pochi ms prima del boundary: usare anche m_periodTicksMax per
    // questi modi produceva un decode completo e subito dopo un frammento
    // da ~3000 campioni. Il counter resta fallback solo per modi non sync.
    bool const utcBoundaryCrossed =
        (m_lastPeriodSlot >= 0) && (utcSlot != m_lastPeriodSlot);
    bool const tickCounterExpired = !timeSyncMode && (m_periodTicks >= m_periodTicksMax);
    if (m_lastPeriodSlot < 0) {
        // Prima chiamata: inizializza lo slot e lascia che il prossimo
        // boundary sia quello di transizione vera.
        m_lastPeriodSlot = utcSlot;
    }
    if (utcBoundaryCrossed || tickCounterExpired) {
        qint64 const completedUtcSlot =
            (periodMs > 0)
                ? ((utcBoundaryCrossed && m_lastPeriodSlot >= 0)
                       ? m_lastPeriodSlot
                       : qMax<qint64>(0, utcSlot - 1))
                : -1;
        m_lastPeriodSlot = utcSlot;
        m_periodTicks = 0;
        m_periodProgress = 0;
        emit periodProgressChanged();
        if (timeSyncMode) {
            static constexpr qint64 kTimeSyncDecodeSettleMs = 1750;
            dispatchTimeSyncDecodeWhenReady(completedUtcSlot, modeSnapshot, m_periodTimerSessionId,
                                            nowMs + kTimeSyncDecodeSettleMs);
        } else {
            feedAudioToDecoder(completedUtcSlot);
        }
        // Al boundary UTC: un click manuale tardivo in FT8/FT4/FT2 sync
        // viene armato qui invece di partire a meta' slot.
        bool const startedDeferredManualTx = tryStartDeferredManualSyncTx();
        // Al boundary del periodo: verifica se avviare TX automatico
        if (!startedDeferredManualTx) {
            checkAndStartPeriodicTx();
        }
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
    if (usingLegacyBackendForTx() && !useModernSpectrumFeedWithLegacy() && !m_legacyPcmSpectrumFeed) return;
    if (!m_monitoring) return;
    if (m_transmitting || m_tuning) return;

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_lastPanadapterFrameMs <= 0) {
        m_lastPanadapterFrameMs = nowMs;
    }

    // Copia i campioni recenti nel ring buffer waterfall (non viene consumato dal decoder).
    // Lock perché il callback del sink può appendere a m_audioBuffer in parallelo.
    if (!m_legacyPcmSpectrumFeed) {
        QMutexLocker locker(&m_audioBufferMutex);
        int avail = m_audioBuffer.size();
        if (avail > 0) {
            int toCopy = qMin(avail, (int)WF_RING_SIZE);
            int srcStart = avail - toCopy;
            for (int i = 0; i < toCopy; ++i) {
                m_wfRing[m_wfRingPos % WF_RING_SIZE] = m_audioBuffer[srcStart + i];
                ++m_wfRingPos;
            }
        }
    }

    // Usa il ring buffer waterfall per la FFT (sempre pieno, indipendente dal decoder)
    int wfAvail = qMin(m_wfRingPos, (int)WF_RING_SIZE);
    if (wfAvail < 512) {
        if (nowMs - m_lastPanadapterFrameMs > 6000
            && nowMs - m_lastSpectrumRecoveryMs > 12000) {
            m_lastSpectrumRecoveryMs = nowMs;
            restartAudioCaptureFromWatchdog(QStringLiteral("waterfall stalled: no spectrum samples"));
        }
        return;
    }

    // ── Alta risoluzione per PanadapterItem ──────────────────────────────────
    bool highResAvailable = false;
    {
        // Leggi dal ring buffer waterfall — sempre disponibile, mai interrotto
        int fftLen = PANADAPTER_FFT_SIZE;
        int usable = qMin(wfAvail, fftLen);
        if (usable >= 512) {
            highResAvailable = true;
            // Visual-only workload: keep high-res panadapter updates slower
            // than the audio/decoder path so Windows/Linux never lose decode
            // throughput to repeated FFT rendering.
            qint64 const minPanadapterIntervalMs = m_legacyPcmSpectrumFeed ? 125 : 66;
            if (m_lastPanadapterFrameMs > 0
                && nowMs - m_lastPanadapterFrameMs < minPanadapterIntervalMs) {
                return;
            }
            m_lastPanadapterFrameMs = nowMs;

            m_spectrumBuf.resize(fftLen);
            m_spectrumBuf.fill(0);
            for (int i = 0; i < usable; ++i) {
                int pos = (m_wfRingPos - usable + i + WF_RING_SIZE * 2) % WF_RING_SIZE;
                m_spectrumBuf[i] = m_wfRing[pos];
            }

            float minDb = 0.f, maxDb = 0.f;
            QVector<float> hq = computePanadapter(minDb, maxDb);
            if (!hq.isEmpty()) {
                // Smooth con l'ultimo frame per transizioni uniformi
                if (m_lastPanadapterData.size() == hq.size()) {
                    float alpha = (usable >= fftLen) ? 1.0f : 0.7f;
                    for (int i = 0; i < hq.size(); ++i)
                        hq[i] = alpha * hq[i] + (1.0f - alpha) * m_lastPanadapterData[i];
                }
                m_lastPanadapterData = hq;
                m_lastPanMinDb = minDb;
                m_lastPanMaxDb = maxDb;
                float freqPerBin = (float)SAMPLE_RATE / PANADAPTER_FFT_SIZE;
                m_lastPanFreqMin = (int)(m_nfa / freqPerBin) * freqPerBin;
                m_lastPanFreqMax = (int)(m_nfb / freqPerBin) * freqPerBin;
                emit panadapterDataReady(hq, minDb, maxDb, m_lastPanFreqMin, m_lastPanFreqMax);
            }
        }
    }

    if (highResAvailable) {
        return;
    }

    // ── Legacy 512-bin fallback ──────────────────────────────────────────────
    {
        // Only used before the panadapter has enough samples for a high-res
        // frame. Do not interleave it with high-res frames: alternating 512-bin
        // and 4096-bin images makes the line graph flicker at fast intervals.
        m_spectrumBuf.resize(SPECTRUM_FFT_SIZE);
        for (int i = 0; i < SPECTRUM_FFT_SIZE; ++i) {
            int pos = (m_wfRingPos - SPECTRUM_FFT_SIZE + i + WF_RING_SIZE * 2) % WF_RING_SIZE;
            m_spectrumBuf[i] = m_wfRing[pos];
        }
        QVector<float> spectrum = computeSpectrum();
        if (!spectrum.isEmpty()) {
            m_lastPanadapterFrameMs = nowMs;
            emit spectrumDataReady(spectrum);
        }
    }
}

void DecodiumBridge::onLegacyAudioSamples(QByteArray const& pcmSamples)
{
    if (!m_legacyPcmSpectrumFeed || pcmSamples.isEmpty()) {
        return;
    }

    int const sampleCount = pcmSamples.size() / static_cast<int>(sizeof(qint16));
    if (sampleCount <= 0) {
        return;
    }

    auto const* samples = reinterpret_cast<qint16 const*>(pcmSamples.constData());
    for (int i = 0; i < sampleCount; ++i) {
        m_wfRing[m_wfRingPos % WF_RING_SIZE] = samples[i];
        ++m_wfRingPos;
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

    if ((useModernSpectrumFeedWithLegacy() || m_legacyPcmSpectrumFeed)
        && !m_lastPanadapterData.isEmpty())
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

    m_lastPanadapterFrameMs = QDateTime::currentMSecsSinceEpoch();
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

bool DecodiumBridge::usingTciAudioInput() const
{
    return m_catBackend == QStringLiteral("tci")
        && m_hamlibCat
        && m_hamlibCat->tciAudioEnabled();
}

bool DecodiumBridge::startTciTxAudioStream(QVector<float> const& wave, QString const& mode,
                                           unsigned symbolsLength, double framesPerSymbol,
                                           double frequency, double toneSpacing,
                                           bool synchronize, double periodSeconds)
{
    if (!m_hamlibCat || !usingTciAudioInput() || !m_hamlibCat->connected()) {
        return false;
    }

    if (toneSpacing < 0.0) {
        if (wave.isEmpty()) {
            bridgeLog(QStringLiteral("TCI TX audio stream refused: empty precomputed wave mode=%1").arg(mode));
            return false;
        }
        m_hamlibCat->setRigTune(false);
        storeBridgePrecomputedTxWave(mode, wave);
    } else {
        itone[0] = 0;
    }

    m_hamlibCat->startRigTxAudio(mode,
                                 symbolsLength,
                                 framesPerSymbol,
                                 frequency,
                                 toneSpacing,
                                 synchronize,
                                 false,
                                 99.0,
                                 qMax(0.1, periodSeconds));
    bridgeLog(QStringLiteral("TCI TX audio stream armed: mode=%1 freq=%2 toneSpacing=%3 samples=%4")
                  .arg(mode)
                  .arg(frequency, 0, 'f', 1)
                  .arg(toneSpacing, 0, 'f', 1)
                  .arg(wave.size()));
    return true;
}

bool DecodiumBridge::startTciTuneAudioStream(double frequency)
{
    if (!m_hamlibCat || !usingTciAudioInput() || !m_hamlibCat->connected()) {
        return false;
    }

    itone[0] = 0;
    m_hamlibCat->setRigTune(true);
    m_hamlibCat->startRigTxAudio(QStringLiteral("FT8"),
                                 79,
                                 1920.0,
                                 frequency,
                                 0.0,
                                 false,
                                 false,
                                 99.0,
                                 15.0);
    bridgeLog(QStringLiteral("TCI TUNE audio stream armed: freq=%1").arg(frequency, 0, 'f', 1));
    return true;
}

void DecodiumBridge::stopTciTxAudioStream(bool quick)
{
    if (!m_hamlibCat || !usingTciAudioInput()) {
        return;
    }

    m_hamlibCat->stopRigTxAudio(quick);
    m_hamlibCat->setRigTune(false);
    bridgeLog(QStringLiteral("TCI TX audio stream stopped: quick=%1").arg(quick ? 1 : 0));
}

void DecodiumBridge::startTciAudioCapture()
{
    if (!m_hamlibCat || !usingTciAudioInput()) {
        return;
    }

    double const periodSeconds = qMax(0.1, periodMsForMode(m_mode) / 1000.0);
    m_tciAudioCaptureActive = true;
    m_rxAudioSuspendedForTx = false;
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = 0;
    m_lastTciAudioLogMs = 0;

    m_hamlibCat->setRigAudio(true, periodSeconds, 6912 / 2);
    emit statusMessage(QStringLiteral("Audio RX via TCI"));
    bridgeLog(QStringLiteral("TCI audio capture started: period=%1 mode=%2")
                  .arg(periodSeconds, 0, 'f', 3)
                  .arg(m_mode));
}

void DecodiumBridge::stopTciAudioCapture()
{
    if (m_hamlibCat && m_tciAudioCaptureActive) {
        m_hamlibCat->setRigAudio(false, qMax(0.1, periodMsForMode(m_mode) / 1000.0), 6912 / 2);
    }
    if (m_tciAudioCaptureActive) {
        bridgeLog(QStringLiteral("TCI audio capture stopped"));
    }
    m_tciAudioCaptureActive = false;
    m_rxAudioSuspendedForTx = false;
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = 0;
}

void DecodiumBridge::onTciPcmSamplesReady(const QVector<short>& samples)
{
    if (samples.isEmpty()
        || !m_monitoring
        || !m_tciAudioCaptureActive
        || !usingTciAudioInput()
        || m_transmitting
        || m_tuning) {
        return;
    }

    double sumSq = 0.0;
    int minSample = 32767;
    int maxSample = -32768;
    int peakAbs = 0;
    int clippedSamples = 0;
    for (short sample : samples) {
        int const value = sample;
        int const absSample = value < 0 ? -value : value;
        sumSq += static_cast<double>(value) * static_cast<double>(value);
        if (value < minSample) minSample = value;
        if (value > maxSample) maxSample = value;
        if (absSample > peakAbs) peakAbs = absSample;
        if (absSample >= 32700) ++clippedSamples;
    }

    {
        QMutexLocker locker(&m_audioBufferMutex);
        m_audioBuffer += samples;
    }

    for (short sample : samples) {
        uint64_t w = m_asyncAudioPos.load(std::memory_order_acquire);
        m_asyncAudio[w % ASYNC_BUF_SIZE] = sample;
        m_asyncAudioPos.compare_exchange_strong(
            w, w + 1,
            std::memory_order_release,
            std::memory_order_relaxed);
        if (m_wavManager) {
            m_wavManager->feedSample(sample);
        }
    }
    m_driftExpectedFrames.fetch_add(samples.size(), std::memory_order_relaxed);

    double const rms = std::sqrt(sumSq / samples.size()) / 32768.0;
    double const peak = static_cast<double>(peakAbs) / 32768.0;
    int const dynamicRange = maxSample - minSample;
    m_audioLevel = rms;
    emit audioLevelChanged();
    handleAudioHealth(rms, peak, dynamicRange, clippedSamples, samples.size());

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastTciAudioLogMs > 5000) {
        m_lastTciAudioLogMs = nowMs;
        bridgeLog(QStringLiteral("TCI audio samples: chunk=%1 buffered=%2 rms=%3 peak=%4 range=%5 clipped=%6")
                      .arg(samples.size())
                      .arg(m_audioBuffer.size())
                      .arg(rms, 0, 'g', 4)
                      .arg(peak, 0, 'g', 4)
                      .arg(dynamicRange)
                      .arg(clippedSamples));
    }
}

void DecodiumBridge::startAudioCapture()
{
    bridgeLog("startAudioCapture() called");
    if (usingTciAudioInput()) {
        bridgeLog(QStringLiteral("startAudioCapture: using TCI audio stream"));
        startTciAudioCapture();
        return;
    }

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
        m_audioSink = new DecodiumAudioSink(m_audioBuffer, 4, this, &m_audioBufferMutex);
        // Ring buffer per il path async FT2: ogni campione decimato va anche nel buffer circolare
        // + feed campioni al WavManager per registrazione audio
        m_audioSink->setSampleCallback([this](short s) {
            // Writer single-thread (callback audio). Pubblichiamo il campione
            // prima dell'incremento di posizione con release semantics così che
            // onAsyncDecodeTimer (acquire load) veda dati validi nel range.
            // uint64_t: wrap-around well-defined dal memory model C++.
            //
            // Il commit usa compare_exchange per eliminare la race con il reset
            // m_asyncAudioPos.store(0) eseguito dal main thread al cambio modo.
            // Senza CAS, il writer potrebbe leggere w=X, il main resettare a 0,
            // e il writer committare X+1 — annullando il reset.  Con CAS, se il
            // reset avviene fra load e commit il CAS fallisce e il campione
            // viene scartato (perdita accettabile durante il boundary di modo).
            uint64_t w = m_asyncAudioPos.load(std::memory_order_acquire);
            m_asyncAudio[w % ASYNC_BUF_SIZE] = s;
            m_asyncAudioPos.compare_exchange_strong(
                w, w + 1,
                std::memory_order_release,
                std::memory_order_relaxed);
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
        connect(m_audioSink, &DecodiumAudioSink::audioHealthChanged,
                this, [this](double rms, double peak, int dynamicRange, int clippedSamples, int samples) {
            handleAudioHealth(rms, peak, dynamicRange, clippedSamples, samples);
        }, Qt::QueuedConnection);
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

    // AudioDevice::Channel is intentionally mapped to the QML combobox index:
    // 0=Mono, 1=Left, 2=Right, 3=Both.
    AudioDevice::Channel channel =
        static_cast<AudioDevice::Channel>(qBound(0, m_audioInputChannel, 3));

    // downSampleFactor: SoundInput requests sample rate = 12000 * factor from
    // the driver.  Use 1 if the device natively supports 12 kHz; use 4 for
    // the common 48 kHz USB codec (e.g. Kenwood TS-590S USB Audio CODEC).
    // framesPerBuffer: 4096 frames per QAudioInput notification period.
    const unsigned downSampleFactor = 4; // assumes 48 kHz source
    bridgeLog("SoundInput::start device=" + selectedDevice.description() +
              " channel=" + QString::number((int)channel) +
              " dsf=" + QString::number(downSampleFactor));
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = QDateTime::currentMSecsSinceEpoch() + 5000;
    m_soundInput->start(selectedDevice, 4096, m_audioSink, downSampleFactor, channel);
    m_soundInput->setInputGain(rxInputGainFromLevel(m_rxInputLevel));

    emit statusMessage("Audio capture avviato: " + selectedDevice.description());

    // Auto-match: se l'output TX non è configurato, cerca un device di output
    // che appartiene alla stessa scheda audio dell'input (es. "USB Audio CODEC")
    if (m_audioOutputDevice.isEmpty()) {
        // Estrai la parte identificativa dal nome input (es. "USB Audio CODEC" da "Microphone (USB Audio CODEC )")
        QString inputDesc = selectedDevice.description();
        // Cerca tra parentesi: "Microphone (USB Audio CODEC )" → "USB Audio CODEC"
        int paren1 = inputDesc.indexOf('(');
        int paren2 = inputDesc.lastIndexOf(')');
        QString deviceId;
        if (paren1 >= 0 && paren2 > paren1)
            deviceId = inputDesc.mid(paren1 + 1, paren2 - paren1 - 1).trimmed();

        if (!deviceId.isEmpty()) {
            for (const QAudioDevice& d : QMediaDevices::audioOutputs()) {
                if (d.description().contains(deviceId, Qt::CaseInsensitive)) {
                    setAudioOutputDevice(d.description());
                    bridgeLog("auto-matched TX output device: " + d.description() +
                              " (from input: " + inputDesc + ")");
                    break;
                }
            }
        }
    }

    bridgeLog("startAudioCapture() done");
}

void DecodiumBridge::handleAudioHealth(double rms,
                                       double peak,
                                       int dynamicRange,
                                       int clippedSamples,
                                       int samples)
{
    const bool captureActive = (m_soundInput != nullptr) || m_tciAudioCaptureActive;
    if (!m_monitoring || !captureActive || samples <= 0) {
        m_audioUnhealthyStartMs = 0;
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now < m_audioWatchdogIgnoreUntilMs
        || m_transmitting
        || m_tuning
        || m_rxAudioSuspendedForTx) {
        m_audioUnhealthyStartMs = 0;
        return;
    }

    constexpr int kFlatDynamicRange = 16;
    constexpr double kSilentRms = 0.00003;
    constexpr double kSilentPeak = 0.0001;
    constexpr int kClippedPercent = 35;
    constexpr double kClippedRms = 0.30;
    constexpr double kSquareMinRms = 0.08;
    constexpr double kSquareMinPeak = 0.08;
    constexpr double kSquareMaxCrestFactor = 1.12;
    constexpr qint64 kUnhealthyMs = 8000;
    constexpr qint64 kRestartCooldownMs = 45000;
    constexpr qint64 kLogCooldownMs = 10000;

    const double crestFactor = rms > 1e-9 ? peak / rms : 999.0;
    const bool flatBlock =
        dynamicRange <= kFlatDynamicRange
        || (rms <= kSilentRms && peak <= kSilentPeak);
    const bool clippedBlock =
        samples >= 64
        && clippedSamples * 100 >= samples * kClippedPercent
        && rms >= kClippedRms;
    const bool squareLikeBlock =
        samples >= 64
        && rms >= kSquareMinRms
        && peak >= kSquareMinPeak
        && crestFactor <= kSquareMaxCrestFactor;

    if (!flatBlock && !clippedBlock && !squareLikeBlock) {
        m_audioUnhealthyStartMs = 0;
        return;
    }

    if (m_audioUnhealthyStartMs == 0) {
        m_audioUnhealthyStartMs = now;
    }

    const qint64 unhealthyForMs = now - m_audioUnhealthyStartMs;
    if (unhealthyForMs < kUnhealthyMs) {
        return;
    }

    const QString kind =
        squareLikeBlock ? QStringLiteral("square-like")
                        : clippedBlock ? QStringLiteral("clipped")
                                       : QStringLiteral("flat");
    const QString reason =
        QStringLiteral("%1 RX audio rms=%2 peak=%3 crest=%4 range=%5 clipped=%6/%7 unhealthy_ms=%8")
            .arg(kind)
            .arg(rms, 0, 'g', 4)
            .arg(peak, 0, 'g', 4)
            .arg(crestFactor, 0, 'g', 4)
            .arg(dynamicRange)
            .arg(clippedSamples)
            .arg(samples)
            .arg(unhealthyForMs);

    if (now - m_lastAudioWatchdogRestartMs < kRestartCooldownMs) {
        if (now - m_lastAudioWatchdogLogMs >= kLogCooldownMs) {
            bridgeLog(QStringLiteral("Audio watchdog: unhealthy RX audio in cooldown (%1)").arg(reason));
            m_lastAudioWatchdogLogMs = now;
        }
        return;
    }

    restartAudioCaptureFromWatchdog(reason);
}

void DecodiumBridge::restartAudioCaptureForModeChange(const QString& previousMode)
{
    if (!m_monitoring || m_transmitting || m_tuning) {
        return;
    }

    qint64 const now = QDateTime::currentMSecsSinceEpoch();
    bool const keepExistingQtCapture = !m_tciAudioCaptureActive && m_soundInput;

    if (keepExistingQtCapture) {
        bridgeLog(QStringLiteral("setMode: preserving active QAudioSource for mode change %1 -> %2")
                      .arg(previousMode, m_mode));
    } else {
        bridgeLog(QStringLiteral("setMode: restarting RX capture for mode change %1 -> %2")
                      .arg(previousMode, m_mode));
        stopAudioCapture();
    }
    m_audioWatchdogIgnoreUntilMs = now + 5000;
    m_audioUnhealthyStartMs = 0;
    resetRxPeriodAccumulation(true);
    resetTimeSyncDecodeMetrics();
    m_spectrumBuf.clear();
    m_wfRingPos = 0;
    m_lastPanadapterData.clear();
    m_lastPanadapterFrameMs = QDateTime::currentMSecsSinceEpoch();
    m_lastSpectrumRecoveryMs = 0;
    m_driftFrameCount = 0;
    m_driftExpectedFrames = 0;
    m_driftClock.restart();
    m_asyncDecodePending = false;
    if (m_mode == QStringLiteral("FT2")) {
        m_asyncAudioPos.store(0, std::memory_order_release);
    }

    if (keepExistingQtCapture) {
        if (m_soundInput) {
            m_soundInput->setInputGain(rxInputGainFromLevel(m_rxInputLevel));
        }
    } else {
        startAudioCapture();
    }
}

void DecodiumBridge::scheduleMonitorRecovery(const QString& reason,
                                             quint64 sessionId,
                                             bool monitorShouldStayActive)
{
    if (!monitorShouldStayActive) {
        return;
    }

    constexpr int kRetryDelaysMs[] = {350, 1000, 2500, 5000, 10000};
    for (int delayMs : kRetryDelaysMs) {
        QTimer::singleShot(delayMs, this, [this, reason, sessionId, delayMs]() {
            if (sessionId != m_periodTimerSessionId || !m_monitorRequested) {
                return;
            }
            if (m_transmitting || m_tuning) {
                if (delayMs == 10000) {
                    bridgeLog(QStringLiteral("%1: monitor recovery deferred while TX/Tune is active")
                                  .arg(reason));
                }
                return;
            }

            if (!m_monitoring) {
                bridgeLog(QStringLiteral("%1: monitor dropped, rearming RX").arg(reason));
                startRx();
                return;
            }

            if (usingLegacyBackendForTx()
                && legacyBackendAvailable()
                && !m_legacyBackend->monitoring()) {
                bridgeLog(QStringLiteral("%1: legacy monitor dropped, rearming backend").arg(reason));
                m_legacyBackend->setMonitoring(true);
                syncLegacyBackendState();
            }
        });
    }
}

void DecodiumBridge::scheduleModeChangeMonitorRecovery(const QString& previousMode,
                                                       const QString& requestedMode,
                                                       quint64 sessionId,
                                                       bool monitorWasActive)
{
    QString const reason = QStringLiteral("setMode %1 -> %2").arg(previousMode, requestedMode);
    scheduleMonitorRecovery(reason, sessionId, monitorWasActive);
}

void DecodiumBridge::restartAudioCaptureFromWatchdog(const QString& reason)
{
    if (!m_monitoring || m_transmitting || m_tuning) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_lastAudioWatchdogRestartMs = now;
    m_lastAudioWatchdogLogMs = now;
    m_audioUnhealthyStartMs = 0;

    bridgeLog(QStringLiteral("Audio watchdog: restarting RX capture (%1)").arg(reason));
    emit statusMessage(QStringLiteral("Audio RX riavviato: watchdog audio"));

    stopAudioCapture();
    m_audioWatchdogIgnoreUntilMs = now + 5000;
    resetRxPeriodAccumulation(true);
    m_spectrumBuf.clear();
    m_wfRingPos = 0;
    m_lastPanadapterData.clear();
    m_lastPanadapterFrameMs = now;
    m_lastSpectrumRecoveryMs = now;
    m_driftFrameCount = 0;
    m_driftExpectedFrames = 0;
    m_driftClock.restart();
    m_asyncDecodePending = false;
    if (m_mode == QStringLiteral("FT2")) {
        m_asyncAudioPos.store(0, std::memory_order_release);
    }

    startAudioCapture();
}

void DecodiumBridge::stopAudioCapture()
{
    if (m_tciAudioCaptureActive) {
        stopTciAudioCapture();
        return;
    }

    if (m_soundInput) {
        m_soundInput->stop();
    }
    m_rxAudioSuspendedForTx = false;
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = 0;
}

void DecodiumBridge::teardownAudioCapture()
{
    if (m_soundInput) {
        bridgeLog(QStringLiteral("teardownAudioCapture: stopping and destroying SoundInput"));
        QObject::disconnect(m_soundInput, nullptr, this, nullptr);
        m_soundInput->stop();
        delete m_soundInput;
        m_soundInput = nullptr;
    }
    m_rxAudioSuspendedForTx = false;
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = 0;

    if (m_audioSink) {
        bridgeLog(QStringLiteral("teardownAudioCapture: destroying audio sink"));
        if (m_audioSink->isOpen()) {
            m_audioSink->close();
        }
        delete m_audioSink;
        m_audioSink = nullptr;
    }
}

void DecodiumBridge::dispatchTimeSyncDecodeWhenReady(qint64 completedUtcSlot,
                                                     const QString& modeSnapshot,
                                                     quint64 sessionId,
                                                     qint64 deadlineMs)
{
    if (!m_monitoring
        || sessionId != m_periodTimerSessionId
        || m_mode != modeSnapshot
        || !isTimeSyncDecodeMode(modeSnapshot)) {
        return;
    }

    int const targetSamples = targetDecodeSamplesForMode(modeSnapshot);
    if (!m_pendingTimeSyncDecodeActive
        || m_pendingTimeSyncDecodeSlot != completedUtcSlot
        || m_pendingTimeSyncDecodeMode != modeSnapshot) {
        m_pendingTimeSyncDecodeAudio.clear();
        m_pendingTimeSyncDecodeSlot = completedUtcSlot;
        m_pendingTimeSyncDecodeMode = modeSnapshot;
        m_pendingTimeSyncDecodeActive = true;

        QMutexLocker locker(&m_audioBufferMutex);
        if (targetSamples > 0 && m_audioBuffer.size() > targetSamples) {
            m_pendingTimeSyncDecodeAudio = m_audioBuffer.mid(0, targetSamples);
            m_audioBuffer = m_audioBuffer.mid(targetSamples);
            m_audioBuffer.reserve(targetSamples);
        } else {
            m_pendingTimeSyncDecodeAudio = m_audioBuffer;
            m_audioBuffer.clear();
            m_audioBuffer.reserve(targetSamples > 0 ? targetSamples : m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
        }
    }

    int currentSlotSamples = 0;
    {
        QMutexLocker locker(&m_audioBufferMutex);
        currentSlotSamples = m_audioBuffer.size();
    }
    int const pendingSamples = m_pendingTimeSyncDecodeAudio.size();
    int const combinedSamples = pendingSamples + qMax(0, currentSlotSamples);
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (targetSamples > 0 && combinedSamples < targetSamples && nowMs < deadlineMs) {
        int const missingSamples = targetSamples - combinedSamples;
        int const waitMs = qBound(20, (missingSamples * 1000 + SAMPLE_RATE - 1) / SAMPLE_RATE, 125);
        QTimer::singleShot(waitMs, this, [this, completedUtcSlot, modeSnapshot, sessionId, deadlineMs]() {
            dispatchTimeSyncDecodeWhenReady(completedUtcSlot, modeSnapshot, sessionId, deadlineMs);
        });
        return;
    }

    QVector<short> finalAudio = m_pendingTimeSyncDecodeAudio;
    int borrowedSamples = 0;
    if (targetSamples > 0 && finalAudio.size() < targetSamples) {
        int const needed = targetSamples - finalAudio.size();
        QMutexLocker locker(&m_audioBufferMutex);
        borrowedSamples = qMin(needed, m_audioBuffer.size());
        if (borrowedSamples > 0) {
            finalAudio += m_audioBuffer.mid(0, borrowedSamples);
        }
    }
    if (targetSamples > 0 && finalAudio.size() > targetSamples) {
        finalAudio.resize(targetSamples);
    }

    if (targetSamples > 0 && finalAudio.size() < targetSamples) {
        bridgeLog(QStringLiteral("time-sync decode settling timeout: mode=%1 samples=%2 target=%3 borrowed=%4 slot=%5")
                      .arg(modeSnapshot)
                      .arg(finalAudio.size())
                      .arg(targetSamples)
                      .arg(borrowedSamples)
                      .arg(completedUtcSlot));
    } else if (targetSamples > 0) {
        bridgeLog(QStringLiteral("time-sync decode dispatch: mode=%1 samples=%2 target=%3 borrowed=%4 slot=%5")
                      .arg(modeSnapshot)
                      .arg(finalAudio.size())
                      .arg(targetSamples)
                      .arg(borrowedSamples)
                      .arg(completedUtcSlot));
    }

    m_pendingTimeSyncDecodeAudio.clear();
    m_pendingTimeSyncDecodeSlot = -1;
    m_pendingTimeSyncDecodeMode.clear();
    m_pendingTimeSyncDecodeActive = false;
    m_forcedDecodeAudioSnapshot = finalAudio;
    m_forcedDecodeAudioSnapshotActive = true;
    feedAudioToDecoder(completedUtcSlot);
}

void DecodiumBridge::queueFt8DecodeRequest(const QVector<short>& audioSnapshot, quint64 serial,
                                           int nutc, qint64 slotIndexForUtc, int decodeDepth,
                                           int decodeQsoProgress, int cqHint, int nzhsym,
                                           bool ft8ApEnabled, bool suppressUiRows)
{
    if (!m_ft8Worker) {
        return;
    }

    decodium::ft8::DecodeRequest req;
    req.serial = serial;
    req.audio = audioSnapshot;
    req.nutc = nutc;
    req.nqsoprogress = decodeQsoProgress;
    req.nfqso = qBound(m_nfa, m_rxFrequency, m_nfb);
    req.nftx = qBound(m_nfa, m_txFrequency, m_nfb);
    req.nfa = m_nfa;
    req.nfb = m_nfb;
    req.nzhsym = qBound(41, nzhsym, 50);
    bool const txAudioActive = m_transmitting || m_tuning;
    req.ndepth = txAudioActive ? qMin(decodeDepth, 2) : decodeDepth;
    req.ncontest = m_ncontest;
    req.emedelay = 0.0f;
    req.nagain = 0;
    req.lft8apon = (ft8ApEnabled && !txAudioActive) ? 1 : 0;
    req.lmultift8 = 1;
    req.lapcqonly = cqHint;
    if (m_frequency < 30000000.0) {
        req.napwid = 5;
    } else if (m_frequency < 100000000.0) {
        req.napwid = 15;
    } else {
        req.napwid = 50;
    }
    req.ldiskdat = 0;
    req.mycall = m_callsign.toLocal8Bit();
    req.hiscall = m_dxCall.toLocal8Bit();
    req.hisgrid = m_dxGrid.toLocal8Bit();

    if (suppressUiRows) {
        m_ft8EarlyDecodeSerials.insert(serial);
        bridgeLog("FT8 early predecode: serial=" + QString::number(serial) +
                  " nzhsym=" + QString::number(req.nzhsym) +
                  " samples=" + QString::number(audioSnapshot.size()) +
                  " slot=" + QString::number(slotIndexForUtc) +
                  " depth=" + QString::number(req.ndepth) +
                  " ft8ap=" + QString::number(req.lft8apon));
    }

    QMetaObject::invokeMethod(m_ft8Worker, [this, req]() {
        m_ft8Worker->decode(req);
    }, Qt::QueuedConnection);
}

void DecodiumBridge::maybeDispatchFt8EarlyDecode(qint64 utcSlot, int msInSlot, int periodMs)
{
    if (m_mode != QStringLiteral("FT8")
        || periodMs != 15000
        || !m_monitoring
        || usingLegacyBackendForTx()
        || m_transmitting
        || m_tuning
        || m_rxAudioSuspendedForTx) {
        return;
    }

    if (m_ft8EarlyDecodeSlot != utcSlot) {
        m_ft8EarlyDecodeSlot = utcSlot;
        m_ft8EarlyDecode41Sent = false;
        m_ft8EarlyDecode47Sent = false;
    }

    static constexpr int kFt8Early41Ms = 12750;
    static constexpr int kFt8Early47Ms = 14250;

    int nzhsym = 0;
    bool mark41 = false;
    bool mark47 = false;
    if (!m_ft8EarlyDecode41Sent && msInSlot >= kFt8Early41Ms) {
        nzhsym = 41;
        mark41 = true;
    } else if (m_ft8EarlyDecode41Sent && !m_ft8EarlyDecode47Sent && msInSlot >= kFt8Early47Ms) {
        nzhsym = 47;
        mark47 = true;
    }

    if (nzhsym == 0) {
        return;
    }

    int const decodeDepth = effectiveDecodeDepth();
    if (decodeDepth <= 1) {
        if (mark41) m_ft8EarlyDecode41Sent = true;
        if (mark47) m_ft8EarlyDecode47Sent = true;
        return;
    }

    QVector<short> audioSnapshot;
    {
        QMutexLocker locker(&m_audioBufferMutex);
        audioSnapshot = m_audioBuffer;
    }

    int const minSamples = (nzhsym == 41) ? 120000 : 132000;
    if (audioSnapshot.size() < minSamples) {
        bridgeLog("FT8 early predecode skipped: nzhsym=" + QString::number(nzhsym) +
                  " samples=" + QString::number(audioSnapshot.size()) +
                  " min=" + QString::number(minSamples));
        return;
    }

    quint64 const serial = ++m_decodeSerial;
    int nutc = utcTokenForSlotStart(utcSlot, periodMs);
    QString utcToken = utcDisplayTokenForSlotStart(utcSlot, periodMs);
    if (nutc <= 0) {
        utcToken = QDateTime::currentDateTimeUtc().toString(QStringLiteral("hhmmss"));
        nutc = utcToken.toInt();
    }

    m_decodeModeBySerial.insert(serial, QStringLiteral("FT8"));
    m_decodeUtcTokenBySerial.insert(serial, utcToken);
    bridgeLog("FT8 early visible decode: serial=" + QString::number(serial) +
              " nzhsym=" + QString::number(nzhsym) +
              " samples=" + QString::number(audioSnapshot.size()) +
              " slot=" + QString::number(utcSlot) +
              " depth=" + QString::number(decodeDepth));

    queueFt8DecodeRequest(audioSnapshot, serial, nutc, utcSlot, decodeDepth,
                          legacyDecodeQsoProgress(), legacyDecodeCqHint(),
                          nzhsym, m_ft8ApEnabled, false);
    if (mark41) m_ft8EarlyDecode41Sent = true;
    if (mark47) m_ft8EarlyDecode47Sent = true;
}

void DecodiumBridge::queueFt4DecodeRequest(const QVector<short>& audioSnapshot, quint64 serial,
                                           int nutc, qint64 slotIndexForUtc, int decodeDepth,
                                           int decodeQsoProgress, int cqHint)
{
    if (!m_ft4Worker) {
        return;
    }

    decodium::ft4::DecodeRequest req;
    req.serial = serial;
    req.audio = audioSnapshot;
    req.nutc = nutc;
    req.nqsoprogress = decodeQsoProgress;
    req.nfqso = qBound(m_nfa, m_rxFrequency, m_nfb);
    req.nfa = m_nfa;
    req.nfb = m_nfb;
    req.ndepth = (m_transmitting || m_tuning) ? qMin(decodeDepth, 2) : decodeDepth;
    req.ncontest = m_ncontest;
    req.lapcqonly = cqHint;
    req.mycall = m_callsign.toLocal8Bit();
    req.hiscall = m_dxCall.toLocal8Bit();

    bridgeLog("FT4 decode dispatch: serial=" + QString::number(serial) +
              " samples=" + QString::number(audioSnapshot.size()) +
              " slot=" + QString::number(slotIndexForUtc) +
              " depth=" + QString::number(decodeDepth));

    QMetaObject::invokeMethod(m_ft4Worker, [this, req]() {
        m_ft4Worker->decode(req);
    }, Qt::QueuedConnection);
}

void DecodiumBridge::maybeDispatchFt4EarlyDecode(qint64 utcSlot, int msInSlot, int periodMs)
{
    if (m_mode != QStringLiteral("FT4")
        || periodMs != 7500
        || !m_monitoring
        || usingLegacyBackendForTx()
        || m_transmitting
        || m_tuning
        || m_rxAudioSuspendedForTx) {
        return;
    }

    if (m_ft4EarlyDecodeSlot != utcSlot) {
        m_ft4EarlyDecodeSlot = utcSlot;
        m_ft4EarlyDecodeSent = false;
    }
    if (m_ft4EarlyDecodeSent || msInSlot < 6250) {
        return;
    }

    QVector<short> audioSnapshot;
    {
        QMutexLocker locker(&m_audioBufferMutex);
        audioSnapshot = m_audioBuffer;
    }

    int const minSamples = minimumDecodeSamplesForMode(QStringLiteral("FT4"));
    if (audioSnapshot.size() < minSamples) {
        bridgeLog("FT4 early decode skipped: samples=" + QString::number(audioSnapshot.size()) +
                  " min=" + QString::number(minSamples));
        return;
    }

    quint64 const serial = ++m_decodeSerial;
    int nutc = utcTokenForSlotStart(utcSlot, periodMs);
    QString utcToken = utcDisplayTokenForSlotStart(utcSlot, periodMs);
    if (nutc <= 0) {
        utcToken = QDateTime::currentDateTimeUtc().toString(QStringLiteral("hhmmss"));
        nutc = utcToken.toInt();
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    m_decodeStartMsBySerial.insert(serial, nowMs);
    m_decodeModeBySerial.insert(serial, QStringLiteral("FT4"));
    m_decodeUtcTokenBySerial.insert(serial, utcToken);

    queueFt4DecodeRequest(audioSnapshot, serial, nutc, utcSlot, effectiveDecodeDepth(),
                          legacyDecodeQsoProgress(), legacyDecodeCqHint());
    m_ft4EarlyDecodeSent = true;
}

void DecodiumBridge::feedAudioToDecoder(qint64 completedUtcSlot)
{
    QString const modeSnapshot = m_mode;
    int const decodeDepth = effectiveDecodeDepth();

    // Snapshot atomico del buffer audio sotto lock: il callback del sink
    // (DecodiumAudioSink::writeData) può eseguire append() in parallelo da
    // un thread del backend audio. Sfruttiamo il copy-on-write di QVector:
    // l'assegnazione `audioSnapshot = m_audioBuffer` incrementa solo il
    // refcount; `clear()` poi alloca un nuovo storage vuoto per m_audioBuffer
    // mentre `audioSnapshot` continua a possedere i dati del periodo appena
    // chiuso. Tutto il dispatch ai worker usa la snapshot locale, senza tenere
    // il lock per tutto il routing dei mode.
    QVector<short> audioSnapshot;
    QString emptyAudioReason;
    int carriedSamples = 0;
    bool usedForcedAudioSnapshot = false;
    int const targetSamples = isTimeSyncDecodeMode(modeSnapshot)
                                  ? targetDecodeSamplesForMode(modeSnapshot)
                                  : 0;
    if (m_forcedDecodeAudioSnapshotActive) {
        audioSnapshot = m_forcedDecodeAudioSnapshot;
        m_forcedDecodeAudioSnapshot.clear();
        m_forcedDecodeAudioSnapshotActive = false;
        usedForcedAudioSnapshot = true;
        if (audioSnapshot.isEmpty()) {
            QString const reason =
                QStringLiteral("Buffer audio vuoto (forced snapshot mode=%1 monitor=%2 sink=%3 rxSuspended=%4 driftExpected=%5)")
                    .arg(modeSnapshot)
                    .arg(m_monitoring ? "on" : "off")
                    .arg(m_audioSink ? "alive" : "null")
                    .arg(m_rxAudioSuspendedForTx ? "yes" : "no")
                    .arg(static_cast<qint64>(m_driftExpectedFrames.load(std::memory_order_relaxed)));
            emptyAudioReason = reason;
        }
    } else {
        QMutexLocker locker(&m_audioBufferMutex);
        if (m_audioBuffer.isEmpty()) {
            // Diagnostica dettagliata: questo path in passato era silenzioso
            // e mascherava i casi in cui il sink audio era fermo / scollegato.
            // Emettiamo mode, stato monitor, presenza del sink e se la cattura
            // era sospesa per TX — così il log basta a capire la causa.
            QString const reason =
                QStringLiteral("Buffer audio vuoto (mode=%1 monitor=%2 sink=%3 rxSuspended=%4 driftExpected=%5)")
                    .arg(modeSnapshot)
                    .arg(m_monitoring ? "on" : "off")
                    .arg(m_audioSink ? "alive" : "null")
                    .arg(m_rxAudioSuspendedForTx ? "yes" : "no")
                    .arg(static_cast<qint64>(m_driftExpectedFrames.load(std::memory_order_relaxed)));
            emptyAudioReason = reason;
        } else {
            if (targetSamples > 0 && m_audioBuffer.size() > targetSamples) {
                audioSnapshot = m_audioBuffer.mid(0, targetSamples);
                QVector<short> carry = m_audioBuffer.mid(targetSamples);
                carriedSamples = carry.size();
                m_audioBuffer = carry;
                m_audioBuffer.reserve(targetSamples);
            } else {
                audioSnapshot = m_audioBuffer;
                m_audioBuffer.clear();
                m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
            }
        }
    }

    if (!emptyAudioReason.isEmpty()) {
        bridgeLog("feedAudioToDecoder: " + emptyAudioReason);
        emit statusMessage(emptyAudioReason);
        if (modeSnapshot == QStringLiteral("FT8")) {
            resetEarlyDecodeSchedule();
        }
        if (m_monitoring
            && m_soundInput
            && !m_transmitting
            && !m_tuning
            && !m_rxAudioSuspendedForTx
            && QDateTime::currentMSecsSinceEpoch() >= m_audioWatchdogIgnoreUntilMs) {
            restartAudioCaptureFromWatchdog(emptyAudioReason);
        }
        return;
    }

    int const minSamples = minimumDecodeSamplesForMode(modeSnapshot);
    if (minSamples > 0 && audioSnapshot.size() < minSamples) {
        bridgeLog("feedAudioToDecoder: skip short " + modeSnapshot +
                  " window samples=" + QString::number(audioSnapshot.size()) +
                  " min=" + QString::number(minSamples));
        if (modeSnapshot == QStringLiteral("FT8")) {
            resetEarlyDecodeSchedule();
        }
        return;
    }

    m_decoding = true;
    emit decodingChanged();

    quint64 serial = ++m_decodeSerial;
    int const decodePeriodMs = periodMsForMode(modeSnapshot);
    qint64 slotIndexForUtc = completedUtcSlot;
    if (slotIndexForUtc < 0 && decodePeriodMs > 0) {
        slotIndexForUtc = QDateTime::currentMSecsSinceEpoch() / decodePeriodMs;
    }

    int nutc = utcTokenForSlotStart(slotIndexForUtc, decodePeriodMs);
    QString utcToken = utcDisplayTokenForSlotStart(slotIndexForUtc, decodePeriodMs);
    if (nutc <= 0) {
        QString const utcFmt = (decodePeriodMs > 0 && decodePeriodMs < 60000)
                                   ? QStringLiteral("hhmmss")
                                   : QStringLiteral("hhmm");
        utcToken = QDateTime::currentDateTimeUtc().toString(utcFmt);
        nutc = utcToken.toInt();
    }
    int const nfqso = qBound(m_nfa, m_rxFrequency, m_nfb);
    int const nftx = qBound(m_nfa, m_txFrequency, m_nfb);
    int const decodeQsoProgress = legacyDecodeQsoProgress();
    int const cqHint = legacyDecodeCqHint();

    if (isTimeSyncDecodeMode(modeSnapshot)) {
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        m_decodeStartMsBySerial.insert(serial, nowMs);
        m_decodeModeBySerial.insert(serial, modeSnapshot);
        m_decodeUtcTokenBySerial.insert(serial, utcToken);

        for (auto it = m_decodeStartMsBySerial.begin(); it != m_decodeStartMsBySerial.end(); ) {
            if (nowMs - it.value() > 60000) {
                m_decodeModeBySerial.remove(it.key());
                m_decodeUtcTokenBySerial.remove(it.key());
                it = m_decodeStartMsBySerial.erase(it);
            } else {
                ++it;
            }
        }
    }

    bridgeLog("feedAudioToDecoder: mode=" + modeSnapshot +
              " samples=" + QString::number(audioSnapshot.size()) +
              (usedForcedAudioSnapshot ? " forced=1" : QString()) +
              (carriedSamples > 0 ? " carried=" + QString::number(carriedSamples) : QString()) +
              " nutc=" + QString::number(nutc) +
              " slot=" + QString::number(slotIndexForUtc) +
              " nfqso=" + QString::number(nfqso) +
              " nftx=" + QString::number(nftx) +
              " depth=" + QString::number(decodeDepth) +
              " qso=" + QString::number(decodeQsoProgress) +
              " cqhint=" + QString::number(cqHint) +
              " ft8ap=" + QString::number(m_ft8ApEnabled ? 1 : 0));

    if (modeSnapshot == "FT8") {
        int const fastDepth = qMin(decodeDepth, 2);
        bool const txAudioActive = m_transmitting || m_tuning;
        bool const runDeepFollowup = !txAudioActive && (decodeDepth > fastDepth || m_ft8ApEnabled);
        bridgeLog("FT8 final fast pass: serial=" + QString::number(serial) +
                  " depth=" + QString::number(fastDepth) +
                  " ft8ap=0" +
                  (runDeepFollowup ? " followup=1" : " followup=0"));
        queueFt8DecodeRequest(audioSnapshot, serial, nutc, slotIndexForUtc, fastDepth,
                              decodeQsoProgress, cqHint, 50, false, false);

        if (runDeepFollowup) {
            quint64 deepSerial = ++m_decodeSerial;
            if (isTimeSyncDecodeMode(modeSnapshot)) {
                qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
                m_decodeStartMsBySerial.insert(deepSerial, nowMs);
                m_decodeModeBySerial.insert(deepSerial, modeSnapshot);
                m_decodeUtcTokenBySerial.insert(deepSerial, utcToken);
            }
            bridgeLog("FT8 final deep followup: serial=" + QString::number(deepSerial) +
                      " depth=" + QString::number(decodeDepth) +
                      " ft8ap=" + QString::number(m_ft8ApEnabled ? 1 : 0));
            queueFt8DecodeRequest(audioSnapshot, deepSerial, nutc, slotIndexForUtc, decodeDepth,
                                  decodeQsoProgress, cqHint, 50, m_ft8ApEnabled, false);
        }

    } else if (modeSnapshot == "FT2") {
        decodium::ft2::DecodeRequest req;
        req.serial = serial; req.audio = audioSnapshot;
        req.nutc = nutc; req.nqsoprogress = decodeQsoProgress;
        req.nfqso = nfqso;
        req.nfa = m_nfa; req.nfb = m_nfb;
        req.ndepth = decodeDepth; req.ncontest = m_ncontest;
        req.mycall = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        // markLatestDecodeSerial() DEVE essere chiamato prima di decode(),
        // altrimenti il controllo seriale in FT2DecodeWorker::decode() scarta sempre la richiesta.
        m_ft2Worker->markLatestDecodeSerial(serial);
        QMetaObject::invokeMethod(m_ft2Worker, [this, req]() {
            m_ft2Worker->decode(req);
        }, Qt::QueuedConnection);

    } else if (modeSnapshot == "FT4") {
        decodium::ft4::DecodeRequest req;
        req.serial = serial; req.audio = audioSnapshot;
        req.nutc = nutc; req.nqsoprogress = decodeQsoProgress;
        req.nfqso = nfqso;
        req.nfa = m_nfa; req.nfb = m_nfb;
        req.ndepth = decodeDepth; req.ncontest = m_ncontest;
        req.lapcqonly = cqHint;
        req.mycall = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        QMetaObject::invokeMethod(m_ft4Worker, [this, req]() {
            m_ft4Worker->decode(req);
        }, Qt::QueuedConnection);

    } else if (modeSnapshot == "Q65") {
        decodium::q65::DecodeRequest req;
        req.serial  = serial; req.audio = audioSnapshot;
        req.nutc    = nutc;   req.nfqso = nfqso;
        req.nfa     = m_nfa;  req.nfb   = m_nfb;
        req.ndepth  = decodeDepth; req.ncontest = m_ncontest;
        req.mycall  = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        QMetaObject::invokeMethod(m_q65Worker, [this, req]() {
            m_q65Worker->decode(req);
        }, Qt::QueuedConnection);

    } else if (modeSnapshot == "MSK144") {
        decodium::msk144::DecodeRequest req;
        req.serial  = serial; req.audio = audioSnapshot;
        req.nutc    = nutc;
        req.rxfreq  = m_rxFrequency;
        req.mycall  = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        QMetaObject::invokeMethod(m_mskWorker, [this, req]() {
            m_mskWorker->decode(req);
        }, Qt::QueuedConnection);

    } else if (modeSnapshot == "WSPR") {
        decodium::wspr::DecodeRequest req;
        req.serial = serial;
        req.arguments = QStringList()
            << "-f" << QString::number(m_frequency / 1e6, 'f', 6)
            << "-c" << m_callsign
            << "-g" << m_grid;
        QMetaObject::invokeMethod(m_wsprWorker, [this, req]() {
            m_wsprWorker->decode(req);
        }, Qt::QueuedConnection);

    } else if (modeSnapshot == "JT65" || modeSnapshot == "JT9" || modeSnapshot == "JT4") {
        decodium::legacyjt::DecodeRequest req;
        req.serial  = serial;
        req.mode    = modeSnapshot;
        req.audio   = audioSnapshot;
        req.nutc    = nutc;
        req.nfqso   = nfqso;
        req.nfa     = m_nfa;
        req.nfb     = m_nfb;
        req.ndepth  = decodeDepth;
        req.mycall  = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        req.hisgrid = m_dxGrid.toLocal8Bit();
        req.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toLocal8Bit();
        QMetaObject::invokeMethod(m_legacyJtWorker, [this, req]() {
            m_legacyJtWorker->decode(req);
        }, Qt::QueuedConnection);

    } else if (modeSnapshot.startsWith("FST4")) {
        decodium::fst4::DecodeRequest req;
        req.serial  = serial;
        req.mode    = modeSnapshot;
        req.audio   = audioSnapshot;
        req.nutc    = nutc;
        req.nfa     = m_nfa;
        req.nfb     = m_nfb;
        req.nfqso   = nfqso;
        req.ndepth  = decodeDepth;
        req.lapcqonly = cqHint;
        req.mycall  = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        req.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toLocal8Bit();
        // Estrai periodo in secondi dal nome della modalità: FST4-30 → 30
        {
            int dashPos = modeSnapshot.indexOf('-');
            req.ntrperiod = (dashPos >= 0) ? modeSnapshot.mid(dashPos + 1).toInt() : 15;
        }
        req.iwspr = modeSnapshot.startsWith("FST4W") ? 1 : 0;
        QMetaObject::invokeMethod(m_fst4Worker, [this, req]() {
            m_fst4Worker->decode(req);
        }, Qt::QueuedConnection);

    } else {
        // Fallback a FT8
        decodium::ft8::DecodeRequest req;
        req.serial = serial; req.audio = audioSnapshot;
        req.nutc = nutc; req.nqsoprogress = decodeQsoProgress;
        req.nfqso = nfqso; req.nftx = nftx;
        req.nfa = m_nfa; req.nfb = m_nfb;
        req.nzhsym = 50;
        req.ndepth = decodeDepth;
        req.emedelay = 0.0f;
        req.ncontest = m_ncontest;
        req.nagain = 0;
        req.lft8apon = m_ft8ApEnabled ? 1 : 0;
        req.lmultift8 = 1;
        req.lapcqonly = cqHint;
        if (m_frequency < 30000000.0) {
            req.napwid = 5;
        } else if (m_frequency < 100000000.0) {
            req.napwid = 15;
        } else {
            req.napwid = 50;
        }
        req.ldiskdat = 0;
        req.mycall = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        req.hisgrid = m_dxGrid.toLocal8Bit();
        QMetaObject::invokeMethod(m_ft8Worker, [this, req]() {
            m_ft8Worker->decode(req);
        }, Qt::QueuedConnection);
    }

    // m_audioBuffer è già stato clear()+reserve() sotto lock all'inizio della
    // funzione: i nuovi campioni del prossimo periodo si stanno già accumulando.
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
    // FFTW plan (cached per performance). Lo stato statico sotto è condiviso
    // tra tutti i chiamanti: serializziamo via mutex per evitare data race se
    // computePanadapter() è invocata da più thread (es. spectrum timer + property
    // binding QML). Il lock copre l'intero ciclo allocazione/finestratura/execute
    // perché fftIn/fftOut sono shared workspace dello stesso piano.
    static QMutex          fftMutex;
    static float*          fftIn  = nullptr;
    static fftwf_complex*  fftOut = nullptr;
    static fftwf_plan      fftPlan = nullptr;
    static int             lastN   = 0;
    QMutexLocker           fftLock (&fftMutex);
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
    if (m_manualTxHold) {
        return;
    }

    removeActiveMamCallerFromQueue(QStringLiteral("process-next"));

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
        m_autoCqCycleCount = 0;
        m_logAfterOwn73 = false;
        m_ft2DeferredLogPending = false;
        m_quickPeerSignaled = false;
        m_qsoLogged = false;
        m_asyncLastTxEndMs = 0;
        m_lastTransmittedMessage.clear();
        m_qsoStartedOn = QDateTime::currentDateTimeUtc();
        setReportSent(QString::number(snr));
        m_reportReceived = QStringLiteral("-10");
        emit reportReceivedChanged();

        setDxCall(nextCall);
        setDxGrid(QString());
        if (rxFreq > 0) {
            setRxFrequency(rxFreq);
        }
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
    requestRigFrequencyFromBridge(freqHz, QStringLiteral("qsy"));
    emit statusMessage(QString("QSY → %1 MHz (%2)")
        .arg(freqHz / 1e6, 0, 'f', 6)
        .arg(newMode));
}

// ============================================================
// B6 — cty.dat: scarica se mancante o vecchio di >30 giorni
// ============================================================

void DecodiumBridge::checkCtyDatUpdate()
{
    if (m_ctyDatUpdating) {
        bridgeLog("cty.dat update requested while another update is already running");
        emit statusMessage("Download cty.dat già in corso...");
        return;
    }

    bridgeLog("cty.dat update requested");

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
        bridgeLog("cty.dat already up to date: " + existing);
        emit statusMessage("cty.dat è aggiornato.");
        return;
    }

    m_ctyDatUpdating = true;
    emit ctyDatUpdatingChanged();
    bridgeLog(existing.isEmpty()
              ? "cty.dat missing, starting download"
              : "cty.dat stale, starting download from " + existing);
    emit statusMessage("Download cty.dat in corso...");

    QString destPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cty.dat";
    QDir().mkpath(QFileInfo(destPath).absolutePath());
    bridgeLog("cty.dat destination: " + destPath);

    QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    QNetworkReply* reply = nam->get(QNetworkRequest(
        QUrl("https://www.country-files.com/bigcty/cty.dat")));

    connect(reply, &QNetworkReply::finished, this, [this, reply, destPath, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        m_ctyDatUpdating = false;
        emit ctyDatUpdatingChanged();

        if (reply->error() != QNetworkReply::NoError) {
            bridgeLog("cty.dat download failed: " + reply->errorString());
            emit errorMessage("Download cty.dat fallito: " + reply->errorString());
            return;
        }
        QFile f(destPath);
        if (f.open(QIODevice::WriteOnly)) {
            auto const payload = reply->readAll();
            f.write(payload);
            f.close();
            bridgeLog(QStringLiteral("cty.dat downloaded (%1 bytes)").arg(payload.size()));
            QString loadedPath;
            if (reloadDxccLookup(&loadedPath)) {
                refreshDecodeListDxcc();
                bridgeLog("cty.dat updated and reloaded from " + loadedPath);
                emit statusMessage("cty.dat aggiornato e caricato: " + loadedPath);
            } else {
                bridgeLog("cty.dat downloaded but DXCC reload failed");
                emit errorMessage("cty.dat scaricato ma il caricamento DXCC è fallito.");
            }
        } else {
            bridgeLog("cty.dat save failed: " + f.errorString());
            emit errorMessage("Impossibile salvare cty.dat: " + f.errorString());
        }
    });
}

void DecodiumBridge::downloadCall3Txt()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }
    if (dataDir.isEmpty()) {
        emit errorMessage(QStringLiteral("Download CALL3.TXT fallito: cartella dati utente non disponibile."));
        return;
    }

    QString const path = QDir(dataDir).absoluteFilePath(QStringLiteral("CALL3.TXT"));
    QString parentError;
    if (!ensureParentDirectoryForFile(path, &parentError)) {
        emit errorMessage(QStringLiteral("Impossibile salvare CALL3.TXT: %1").arg(parentError));
        return;
    }

    bridgeLog("downloadCall3Txt: starting download to " + path);
    emit statusMessage("Download CALL3.TXT in corso...");
    auto* mgr = new QNetworkAccessManager(this);

    QNetworkRequest req(QUrl(QStringLiteral("https://sourceforge.net/projects/wsjt-x-improved/files/Additional%20Files/CALL3.TXT/download")));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader("User-Agent", "Decodium/4 CALL3 updater");

    auto* reply = mgr->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, mgr, path]() {
        bool statusOk = false;
        int const statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(&statusOk);

        auto cleanup = [reply, mgr]() {
            reply->deleteLater();
            mgr->deleteLater();
        };

        if (reply->error() != QNetworkReply::NoError || (statusOk && statusCode >= 400)) {
            QString reason = reply->errorString();
            if (statusOk && statusCode >= 400) {
                reason = QStringLiteral("HTTP %1").arg(statusCode);
            }
            bridgeLog("downloadCall3Txt failed: " + reason);
            emit errorMessage(QStringLiteral("Download CALL3.TXT fallito: %1").arg(reason));
            cleanup();
            return;
        }

        QByteArray const data = reply->readAll();
        QString validationError;
        if (!isProbablyCall3TxtPayload(data, &validationError)) {
            bridgeLog("downloadCall3Txt invalid payload: " + validationError);
            emit errorMessage(QStringLiteral("Download CALL3.TXT non valido: %1").arg(validationError));
            cleanup();
            return;
        }

        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            bridgeLog("downloadCall3Txt save failed: " + f.errorString());
            emit errorMessage(QStringLiteral("Impossibile salvare CALL3.TXT in %1: %2")
                                  .arg(QDir::toNativeSeparators(path), f.errorString()));
            cleanup();
            return;
        }

        qint64 const written = f.write(data);
        if (written != data.size()) {
            QString const reason = f.errorString();
            f.close();
            bridgeLog(QStringLiteral("downloadCall3Txt incomplete write: %1/%2 bytes")
                          .arg(written).arg(data.size()));
            emit errorMessage(QStringLiteral("Impossibile salvare CALL3.TXT: scrittura incompleta (%1/%2 byte). %3")
                                  .arg(written).arg(data.size()).arg(reason));
            cleanup();
            return;
        }

        f.close();
        if (f.error() != QFile::NoError) {
            bridgeLog("downloadCall3Txt close failed: " + f.errorString());
            emit errorMessage(QStringLiteral("Impossibile salvare CALL3.TXT: %1").arg(f.errorString()));
            cleanup();
            return;
        }

        m_worldMapCall3Loaded = false;
        loadWorldMapCall3Cache();

        bridgeLog(QStringLiteral("CALL3.TXT downloaded (%1 bytes) to %2")
                      .arg(data.size()).arg(path));
        emit statusMessage(QStringLiteral("CALL3.TXT aggiornato e caricato (%1 KB)")
                               .arg(data.size() / 1024));
        cleanup();
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

    // Carica i campioni nel buffer audio e avvia la decodifica.
    // QVector<short>::resize() vuole il NUMERO DI ELEMENTI, non byte: in
    // precedenza si moltiplicava per sizeof(short) raddoppiando inutilmente
    // la dimensione e lasciando metà buffer a zero. Lock perché la callback
    // del sink condivide m_audioBuffer (anche se il flusso WAV tipicamente
    // disabilita la cattura, manteniamo l'invariante).
    {
        QMutexLocker locker(&m_audioBufferMutex);
        m_audioBuffer.resize(samples.size());
        memcpy(m_audioBuffer.data(), samples.constData(),
               static_cast<size_t>(samples.size()) * sizeof(short));
    }

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

    QString const targetPath = expandedLocalFilePath(filename);
    QString parentError;
    if (!ensureParentDirectoryForFile(targetPath, &parentError)) {
        emit errorMessage(parentError);
        return false;
    }

    QFile out(targetPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorMessage("Impossibile scrivere: " + targetPath);
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
    emit statusMessage(QString("Cabrillo esportato: %1 QSO → %2").arg(qsoCount).arg(targetPath));
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

static QString bridgeAdifField(const QString& tag, const QString& value)
{
    return QStringLiteral("<%1:%2>%3 ")
        .arg(tag)
        .arg(value.toUtf8().size())
        .arg(value);
}

static bool bridgeAdifModeUsesMfskSubmode(QString const& mode)
{
    QString const normalized = mode.trimmed().toUpper();
    return normalized == QStringLiteral("FT2")
        || normalized == QStringLiteral("FT4")
        || normalized == QStringLiteral("FST4")
        || normalized == QStringLiteral("Q65");
}

static QString bridgeAdifRecordText(const QString& dxCall, const QString& dxGrid,
                                    double freqHz, const QString& mode,
                                    const QDateTime& timeOnUtc,
                                    const QDateTime& timeOffUtc,
                                    const QString& rstSent, const QString& rstRcvd,
                                    const QString& myCall, const QString& myGrid,
                                    const QString& comments,
                                    const QString& propMode,
                                    const QString& satellite,
                                    const QString& satMode,
                                    const QString& freqRx)
{
    double const freqMhz = freqHz / 1e6;
    QString const normalizedMode = mode.trimmed().toUpper();
    QDateTime const effectiveTimeOnUtc = timeOnUtc.isValid() ? timeOnUtc : QDateTime::currentDateTimeUtc();
    QDateTime effectiveTimeOffUtc = timeOffUtc.isValid() ? timeOffUtc : effectiveTimeOnUtc;
    if (effectiveTimeOffUtc < effectiveTimeOnUtc) {
        effectiveTimeOffUtc = effectiveTimeOnUtc;
    }
    QString const qsoDate = effectiveTimeOnUtc.toString(QStringLiteral("yyyyMMdd"));
    QString const qsoTime = effectiveTimeOnUtc.toString(QStringLiteral("HHmmss"));
    QString const qsoDateOff = effectiveTimeOffUtc.toString(QStringLiteral("yyyyMMdd"));
    QString const qsoTimeOff = effectiveTimeOffUtc.toString(QStringLiteral("HHmmss"));

    QString record = bridgeAdifField(QStringLiteral("CALL"), dxCall)
                   + bridgeAdifField(QStringLiteral("GRIDSQUARE"), dxGrid);
    if (bridgeAdifModeUsesMfskSubmode(normalizedMode)) {
        record += bridgeAdifField(QStringLiteral("MODE"), QStringLiteral("MFSK"))
                + bridgeAdifField(QStringLiteral("SUBMODE"), normalizedMode);
    } else {
        record += bridgeAdifField(QStringLiteral("MODE"), normalizedMode);
    }
    record += bridgeAdifField(QStringLiteral("RST_SENT"), rstSent.isEmpty() ? QStringLiteral("-10") : rstSent)
            + bridgeAdifField(QStringLiteral("RST_RCVD"), rstRcvd.isEmpty() ? QStringLiteral("-10") : rstRcvd)
            + bridgeAdifField(QStringLiteral("QSO_DATE"), qsoDate)
            + bridgeAdifField(QStringLiteral("TIME_ON"), qsoTime)
            + bridgeAdifField(QStringLiteral("QSO_DATE_OFF"), qsoDateOff)
            + bridgeAdifField(QStringLiteral("TIME_OFF"), qsoTimeOff)
            + bridgeAdifField(QStringLiteral("BAND"), bandFromFreqHz(freqHz))
            + bridgeAdifField(QStringLiteral("FREQ"), QString::number(freqMhz, 'f', 6))
            + bridgeAdifField(QStringLiteral("STATION_CALLSIGN"), myCall);
    if (!myGrid.trimmed().isEmpty()) {
        record += bridgeAdifField(QStringLiteral("MY_GRIDSQUARE"), myGrid);
    }
    QString const cleanComments = comments.trimmed();
    if (!cleanComments.isEmpty()) {
        record += bridgeAdifField(QStringLiteral("COMMENT"), cleanComments);
    }

    QString const cleanPropMode = propMode.trimmed();
    QString const cleanSatellite = satellite.trimmed();
    QString const cleanSatMode = satMode.trimmed();
    QString const cleanFreqRx = freqRx.trimmed();
    if (!cleanPropMode.isEmpty()) {
        record += bridgeAdifField(QStringLiteral("prop_mode"), cleanPropMode);
    }
    if (!cleanSatellite.isEmpty()) {
        record += bridgeAdifField(QStringLiteral("sat_name"), cleanSatellite);
    }
    if (!cleanSatMode.isEmpty()) {
        record += bridgeAdifField(QStringLiteral("sat_mode"), cleanSatMode);
    }
    if (!cleanFreqRx.isEmpty()) {
        record += bridgeAdifField(QStringLiteral("freq_rx"), cleanFreqRx);
    }
    return record;
}

void DecodiumBridge::appendAdifRecord(const QString& dxCall, const QString& dxGrid,
                                       double freqHz, const QString& mode,
                                       const QDateTime& timeOnUtc,
                                       const QDateTime& timeOffUtc,
                                       const QString& rstSent, const QString& rstRcvd,
                                       const QString& comments,
                                       const QString& propMode,
                                       const QString& satellite,
                                       const QString& satMode,
                                       const QString& freqRx)
{
    QString const logPath = ensureAdifLogPath();
    QDir().mkpath(QFileInfo(logPath).absolutePath());

    QFile f(logPath);
    bool newFile = !f.exists();
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;

    QTextStream ts(&f);
    if (newFile) ts << "Decodium3 ADIF Log\n<EOH>\n";

    ts << bridgeAdifRecordText(dxCall, dxGrid, freqHz, mode, timeOnUtc, timeOffUtc,
                               rstSent, rstRcvd, m_callsign, m_grid,
                               comments, propMode, satellite, satMode, freqRx)
       << "<EOR>\n";

    m_workedCalls.insert(dxCall.toUpper());
    if (m_qsoCountCache >= 0) {
        ++m_qsoCountCache;
    }
    invalidateQsoSearchCache();
    warmLogCacheAsync();
    emit qsoCountChanged();
    emit workedCountChanged();
}

bool DecodiumBridge::exportAdif(const QString& filename)
{
    if (filename.isEmpty()) return false;
    QString const adifPath = effectiveAdifLogPath();
    QFileInfo const adifInfo {adifPath};
    QFileInfo const outInfo {filename};
    QString const cleanAdifPath = QDir::cleanPath(adifInfo.absoluteFilePath());
    QString const cleanOutPath = QDir::cleanPath(outInfo.absoluteFilePath());
    if (adifInfo.exists() && adifInfo.isFile() && cleanAdifPath == cleanOutPath) {
        emit statusMessage(QString("ADIF esportato: %1").arg(filename));
        return true;
    }
    if (adifInfo.exists() && adifInfo.isFile()) {
        QFile::remove(filename);
        if (QFile::copy(adifPath, filename)) {
            emit statusMessage(QString("ADIF esportato: %1").arg(filename));
            return true;
        }
    }

    QString logPath = logAllTxtPath();
    QFile src(logPath);
    if (!src.open(QIODevice::ReadOnly | QIODevice::Text)) {
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
    invalidateQsoSearchCache();
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
int         DecodiumBridge::qsoCount()         const
{
    if (m_qsoCountCache < 0) {
        QElapsedTimer timer;
        timer.start();
        m_qsoCountCache = countAdifRecordsLightweight(effectiveAdifLogPath());
        if (timer.elapsed() > 100) {
            bridgeLog(QStringLiteral("qsoCount lightweight count: qsos=%1 elapsed=%2ms")
                          .arg(m_qsoCountCache)
                          .arg(timer.elapsed()));
        }
    }
    return m_qsoCountCache;
}

void DecodiumBridge::invalidateQsoSearchCache()
{
    m_qsoSearchCacheGeneration.fetch_add(1, std::memory_order_relaxed);
    QMutexLocker locker(&m_qsoSearchCacheMutex);
    m_qsoSearchCacheReady = false;
    m_qsoSearchCachePath.clear();
    m_qsoSearchCacheModified = QDateTime();
    m_qsoSearchCacheSize = -1;
    m_qsoSearchCacheRows.clear();
    m_qsoSearchCacheStats.clear();
}

void DecodiumBridge::warmLogCacheAsync()
{
    {
        QMutexLocker locker(&m_qsoSearchCacheMutex);
        if (m_qsoSearchWarmupInProgress) {
            return;
        }
        m_qsoSearchWarmupInProgress = true;
    }

    QString const path = effectiveAdifLogPath();
    QString const grid = m_grid;
    quint64 const generation = m_qsoSearchCacheGeneration.load(std::memory_order_relaxed);
    QPointer<DecodiumBridge> guard(this);

    QThread* worker = QThread::create([guard, path, grid, generation]() {
        QsoLogSnapshot const snapshot = buildQsoLogSnapshot(path, grid);
        if (!guard) {
            return;
        }

        QMetaObject::invokeMethod(guard.data(), [guard, snapshot, generation]() {
            if (!guard) {
                return;
            }

            bool restartNeeded = false;
            {
                QMutexLocker locker(&guard->m_qsoSearchCacheMutex);
                if (generation == guard->m_qsoSearchCacheGeneration.load(std::memory_order_relaxed)) {
                    guard->m_qsoSearchCachePath = snapshot.path;
                    guard->m_qsoSearchCacheModified = snapshot.modified;
                    guard->m_qsoSearchCacheSize = snapshot.size;
                    guard->m_qsoSearchCacheRows = snapshot.rows;
                    guard->m_qsoSearchCacheStats = snapshot.stats;
                    guard->m_qsoSearchCacheReady = true;
                } else {
                    restartNeeded = true;
                }
                guard->m_qsoSearchWarmupInProgress = false;
            }

            if (restartNeeded) {
                QTimer::singleShot(0, guard.data(), [guard]() {
                    if (guard) {
                        guard->warmLogCacheAsync();
                    }
                });
                return;
            }
            emit guard->qsoLogCacheChanged();
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start(QThread::LowestPriority);
}

QVariantList DecodiumBridge::searchQsos(const QString& search,
                                        const QString& band,
                                        const QString& mode,
                                        const QString& fromDate,
                                        const QString& toDate) const
{
    QElapsedTimer timer;
    timer.start();
    QString const path = effectiveAdifLogPath();
    QFileInfo const info(path);
    QDateTime const modified = info.exists() ? info.lastModified() : QDateTime();
    qint64 const size = info.exists() ? info.size() : 0;
    QVariantList sourceRows;
    bool cacheHit = false;

    {
        QMutexLocker locker(&m_qsoSearchCacheMutex);
        cacheHit = m_qsoSearchCacheReady
            && m_qsoSearchCachePath == path
            && m_qsoSearchCacheModified == modified
            && m_qsoSearchCacheSize == size;
        if (cacheHit) {
            sourceRows = m_qsoSearchCacheRows;
        }
    }

    if (!cacheHit) {
        {
            QMutexLocker locker(&m_qsoSearchCacheMutex);
            if (m_qsoSearchWarmupInProgress) {
                return {};
            }
        }

        QsoLogSnapshot const snapshot = buildQsoLogSnapshot(path, m_grid);
        {
            QMutexLocker locker(&m_qsoSearchCacheMutex);
            m_qsoSearchCachePath = snapshot.path;
            m_qsoSearchCacheModified = snapshot.modified;
            m_qsoSearchCacheSize = snapshot.size;
            m_qsoSearchCacheRows = snapshot.rows;
            m_qsoSearchCacheStats = snapshot.stats;
            m_qsoSearchCacheReady = true;
            sourceRows = m_qsoSearchCacheRows;
        }
    }

    QString const needle = search.trimmed().toUpper();
    QString const bandFilter = band.trimmed().toUpper();
    QString const modeFilter = mode.trimmed().toUpper();
    QString const fromFilter = fromDate.trimmed();
    QString const toFilter = toDate.trimmed();

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

    if (needle.isEmpty() && bandFilter.isEmpty() && modeFilter.isEmpty()
        && fromFilter.isEmpty() && toFilter.isEmpty()) {
        return sourceRows;
    }

    QVariantList results;
    for (QVariant const& rowValue : sourceRows) {
        QVariantMap const row = rowValue.toMap();
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

        results.append(row);
    }
    if (timer.elapsed() > 250) {
        bridgeLog(QStringLiteral("searchQsos: cacheHit=%1 rows=%2 results=%3 elapsed=%4ms")
                      .arg(cacheHit)
                      .arg(sourceRows.size())
                      .arg(results.size())
                      .arg(timer.elapsed()));
    }
    return results;
}

QVariantMap DecodiumBridge::getQsoStats() const
{
    QString const path = effectiveAdifLogPath();
    QFileInfo const info(path);
    QDateTime const modified = info.exists() ? info.lastModified() : QDateTime();
    qint64 const size = info.exists() ? info.size() : 0;

    {
        QMutexLocker locker(&m_qsoSearchCacheMutex);
        if (m_qsoSearchCacheReady
            && m_qsoSearchCachePath == path
            && m_qsoSearchCacheModified == modified
            && m_qsoSearchCacheSize == size) {
            return m_qsoSearchCacheStats;
        }
    }

    {
        QMutexLocker locker(&m_qsoSearchCacheMutex);
        if (m_qsoSearchWarmupInProgress) {
            return {};
        }
    }

    QVariantList const rows = searchQsos(QString {}, QString {}, QString {}, QString {}, QString {});
    return qsoStatsFromRows(rows);
}

int DecodiumBridge::importFromAdif(const QString& filename)
{
    QElapsedTimer timer;
    timer.start();
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
    m_qsoCountCache = dest.records.size();
    invalidateQsoSearchCache();
    warmLogCacheAsync();
    emit qsoCountChanged();
    emit workedCountChanged();
    emit statusMessage(QStringLiteral("ADIF importato: %1 QSO").arg(imported));
    bridgeLog(QStringLiteral("importFromAdif: source=%1 imported=%2 total=%3 elapsed=%4ms")
                  .arg(filename)
                  .arg(imported)
                  .arg(m_qsoCountCache)
                  .arg(timer.elapsed()));
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
    m_qsoCountCache = doc.records.size();
    invalidateQsoSearchCache();
    warmLogCacheAsync();
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
    m_qsoCountCache = doc.records.size();
    invalidateQsoSearchCache();
    warmLogCacheAsync();
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
