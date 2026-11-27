#include "DecodiumBridge.h"
#include <QDateTime>
#include <algorithm>

void decodiumBridgeAudioLevelLog(const QString&);
QString decodiumBridgeDecodeRxClearEntryKey(QVariantMap const&);
void decodiumBridgeCoalesceRxPaneTxRows(QVariantList&, QString const&);
qint64 decodiumBridgeDecodeDisplaySortMsecs(QVariantMap const&, QDateTime const&);

bool DecodiumBridge::entryBelongsToCurrentQso(QVariantMap const& entry,
                                              bool ghostAlreadyAccepted) const
{
    if (entry.isEmpty()) return false;
    // Keep the rebuilt native model aligned with appendRxDecodeEntry().
    // Otherwise a manual double-click on an older CQ/third-party row changes
    // dxCall and TX messages, but Signal RX does not show that selected
    // partner until a new decode arrives.
    // Il filtro Band Activity ha gia validato il ghost gate prima di arrivare
    // qui; non ripetere regex, lookup DXCC e parsing del messaggio.
    return shouldMirrorToRxPane(entry, ghostAlreadyAccepted);
}

void DecodiumBridge::injectPeriodSeparators(QVariantList& filtered) const
{
    if (!m_decodeShowPeriodSeparator || filtered.size() <= 1) {
        if (qEnvironmentVariableIsSet("DECODIUM_DECODE_MODEL_DEBUG")) {
            decodiumBridgeAudioLevelLog(QStringLiteral("injectPeriodSeparators skipped: enabled=%1 size=%2")
                .arg(m_decodeShowPeriodSeparator).arg(filtered.size()));
        }
        return;
    }
    QVariantList withSep;
    withSep.reserve(filtered.size() * 11 / 10 + 1);
    QString prevPeriod;
    qint64 prevTs = 0;
    int sepCount = 0;
    for (QVariant const& v : filtered) {
        QVariantMap const it = v.toMap();
        QString const t = it.value(QStringLiteral("time")).toString();
        qint64 const ts = static_cast<qint64>(it.value(QStringLiteral("timestamp")).toULongLong());
        bool newPeriod = false;
        if (!t.isEmpty()) {
            if (!prevPeriod.isEmpty() && t != prevPeriod) newPeriod = true;
        } else {
            if (prevTs > 0 && ts > 0 && (ts - prevTs) > 1500) newPeriod = true;
        }
        if (newPeriod) {
            QVariantMap sep;
            // 1.0.149: stable key per il diff in DecodeListModel::decodeMatchKey
            // — usare anche il ts del separator (cambia per ogni period FT2).
            sep.insert(QStringLiteral("isSeparator"), true);
            sep.insert(QStringLiteral("time"), t);
            sep.insert(QStringLiteral("timestamp"), QVariant::fromValue(static_cast<qulonglong>(ts)));
            withSep.append(sep);
            ++sepCount;
        }
        if (!t.isEmpty()) prevPeriod = t;
        if (ts > 0) prevTs = ts;
        withSep.append(it);
    }
    if (qEnvironmentVariableIsSet("DECODIUM_DECODE_MODEL_DEBUG")) {
        decodiumBridgeAudioLevelLog(QStringLiteral("injectPeriodSeparators inserted=%1 rows=%2 -> %3")
            .arg(sepCount).arg(filtered.size()).arg(withSep.size()));
    }
    filtered = withSep;
}

QVariantList DecodiumBridge::filterEntriesForBandActivity(QVariantList const& source) const
{
    QVariantList filtered;
    filtered.reserve(source.size());
    decodium::decode_ui::WorkedFilterOptions const workedFilters = workedFilterOptions();
    for (QVariant const& v : source) {
        QVariantMap const e = v.toMap();
        if (shouldDisplayEntryForBandActivity(e, workedFilters)) {
            filtered.append(e);
        }
    }
    if (m_decodeNewestFirst) std::reverse(filtered.begin(), filtered.end());
    injectPeriodSeparators(filtered);
    return filtered;
}

QVariantList DecodiumBridge::filterEntriesForRxDecode(QVariantList const& source) const
{
    QVariantList filtered;
    filtered.reserve(source.size());
    bool const showTxMessagesInRx =
        getSetting(QStringLiteral("TXMessagesToRX"), true).toBool();
    decodium::decode_ui::WorkedFilterOptions const workedFilters = workedFilterOptions();
    for (QVariant const& v : source) {
        QVariantMap const e = v.toMap();
        if (!shouldDisplayEntryForBandActivity(e, workedFilters)) continue;
        if (e.value(QStringLiteral("isTx")).toBool() && !showTxMessagesInRx) continue;
        QString const key = decodiumBridgeDecodeRxClearEntryKey(e);
        if (!key.isEmpty() && m_clearedRxDecodeKeys.contains(key)) continue;
        if (e.value(QStringLiteral("forceRxPane")).toBool()
            || entryBelongsToCurrentQso(e, true)) {
            filtered.append(e);
        }
    }
    decodiumBridgeCoalesceRxPaneTxRows(filtered, m_mode);
    // 1.0.434: riferimento di sort DETERMINISTICO derivato dai dati (max
    // timestamp delle entry) invece di QDateTime::currentDateTimeUtc(): con
    // "now" il riferimento cambiava ad ogni rebuild -> le righe TX/senza-timestamp
    // potevano riordinarsi tra un rebuild e l'altro (churn "slot machine").
    // Stesso input -> stesso ordine. Fallback a now se nessuna entry ha timestamp.
    qint64 maxEntryTsMs = 0;
    for (QVariant const& fv : filtered) {
        qint64 const ts = static_cast<qint64>(fv.toMap().value(QStringLiteral("timestamp")).toULongLong());
        if (ts > maxEntryTsMs) maxEntryTsMs = ts;
    }
    QDateTime const sortReferenceUtc = (maxEntryTsMs > 0)
        ? QDateTime::fromMSecsSinceEpoch(maxEntryTsMs, Qt::UTC)
        : QDateTime::currentDateTimeUtc();
    std::stable_sort(filtered.begin(), filtered.end(),
        [&sortReferenceUtc](QVariant const& a, QVariant const& b) {
            qint64 const ta = decodiumBridgeDecodeDisplaySortMsecs(a.toMap(), sortReferenceUtc);
            qint64 const tb = decodiumBridgeDecodeDisplaySortMsecs(b.toMap(), sortReferenceUtc);
            if (ta != tb) return ta < tb;
            return false;
        });
    injectPeriodSeparators(filtered);
    return filtered;
}

