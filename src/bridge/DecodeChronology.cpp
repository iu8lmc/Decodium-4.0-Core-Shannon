#include "DecodeChronology.h"
#include "DecodeUtcTime.h"
#include "DecodeMessageNormalization.h"
#include <QHash>
using decodium::decode_message::canonicalDecodeMessage;
#include <algorithm>
#include <limits>
#include <utility>

static qint64 decodeDisplaySortMsecsFromTime(QString const& normalizedTime, QDateTime const& nowUtc)
{
    int const decodeSeconds = secondsFromUtcDisplayToken(normalizedTime);
    if (decodeSeconds < 0) {
        return std::numeric_limits<qint64>::max();
    }

    QDate decodedDate = nowUtc.date();
    int const nowSeconds = nowUtc.time().msecsSinceStartOfDay() / 1000;
    int const deltaSeconds = decodeSeconds - nowSeconds;
    if (deltaSeconds > 60) {
        decodedDate = decodedDate.addDays(-1);
    } else if (deltaSeconds < -86340) {
        decodedDate = decodedDate.addDays(1);
    }

    QTime decodedTime(0, 0, 0);
    decodedTime = decodedTime.addSecs(decodeSeconds);
    return QDateTime(decodedDate, decodedTime, Qt::UTC).toMSecsSinceEpoch();
}

static qint64 decodeDisplaySortMsecs(QVariantMap const& entry, QDateTime const& nowUtc)
{
    // 1.0.139: preferisci entry["timestamp"] ms epoch (sempre popolato in FT2
    // async, dove entry["time"] è vuoto — vedi commento riga ~18734 e fix
    // dedup 1.0.136). Se manca, fallback al parsing del campo time. Senza
    // questa preferenza i decode FT2 async finivano tutti con sortMsecs =
    // INT_MAX e venivano mescolati a entries con time valido (es. quelle
    // mirrorate da ALL.TXT legacy) → ordinamento sporadico, vecchie entries
    // potevano apparire "fresche" in cima alla list visibile.
    QString const normalizedTime = normalizeUtcDisplayToken(entry.value(QStringLiteral("time")).toString());
    if (entry.value(QStringLiteral("isTx")).toBool()) {
        qint64 const txVisualSortMs = decodeDisplaySortMsecsFromTime(normalizedTime, nowUtc);
        if (txVisualSortMs != std::numeric_limits<qint64>::max()) {
            return txVisualSortMs;
        }
    }

    qulonglong const tsMs = entry.value(QStringLiteral("timestamp")).toULongLong();
    if (tsMs > 0) {
        return static_cast<qint64>(tsMs);
    }

    return decodeDisplaySortMsecsFromTime(normalizedTime, nowUtc);
}

qint64 decodiumBridgeDecodeDisplaySortMsecs(QVariantMap const& entry, QDateTime const& now)
{
    return decodeDisplaySortMsecs(entry, now);
}

void sortDecodeEntriesChronologically(QVariantList& entries)
{
    struct IndexedEntry
    {
        QVariantMap entry;
        int originalIndex {0};
        qint64 sortMsecs {0};
        int freq {0};
        QString message;
        bool freqValid {false};
    };

    QVector<IndexedEntry> indexed;
    QDateTime const sortReferenceUtc = QDateTime::currentDateTimeUtc();
    indexed.reserve(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        QVariantMap entry = entries.at(i).toMap();
        bool freqOk = false;
        int const freq = entry.value(QStringLiteral("freq")).toString().trimmed().toInt(&freqOk);
        indexed.push_back(IndexedEntry {
            entry,
            i,
            decodeDisplaySortMsecs(entry, sortReferenceUtc),
            freq,
            entry.value(QStringLiteral("message")).toString().trimmed(),
            freqOk
        });
    }

    std::stable_sort(indexed.begin(), indexed.end(), [] (IndexedEntry const& lhs, IndexedEntry const& rhs) {
        bool const lhsValid = lhs.sortMsecs != std::numeric_limits<qint64>::max();
        bool const rhsValid = rhs.sortMsecs != std::numeric_limits<qint64>::max();
        if (lhsValid != rhsValid) {
            return lhsValid;
        }
        if (lhs.sortMsecs != rhs.sortMsecs) {
            return lhs.sortMsecs < rhs.sortMsecs;
        }
        if (lhs.freqValid && rhs.freqValid && lhs.freq != rhs.freq) {
            return lhs.freq < rhs.freq;
        }
        int const msgCompare = QString::compare(lhs.message, rhs.message, Qt::CaseInsensitive);
        if (msgCompare != 0) {
            return msgCompare < 0;
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
    QString const message = canonicalDecodeMessage(entry.value(QStringLiteral("message")).toString());
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

void decodiumBridgeNormalizeDecodeEntries(QVariantList& entries, int maxEntries, QString const& mode)
{
    normalizeDecodeEntriesForDisplay(entries, maxEntries, mode);
}

