#include "DecodeMessageNormalization.h"
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

using decodium::decode_message::canonicalDecodeMessage;
QString decodiumBridgeNormalizedVisualTxTimeToken(const QString&, const QString&);

static QString decodeMirrorEntryKey(QVariantMap const& entry)
{
    QString const time = entry.value(QStringLiteral("isTx")).toBool()
        ? decodiumBridgeNormalizedVisualTxTimeToken(entry.value(QStringLiteral("time")).toString(),
                                      entry.value(QStringLiteral("mode")).toString())
        : entry.value(QStringLiteral("time")).toString();
    QString const message = canonicalDecodeMessage(entry.value(QStringLiteral("message")).toString());
    return time + "|" +
           entry.value("freq").toString() + "|" +
           message + "|" +
           (entry.value("isTx").toBool() ? QStringLiteral("1") : QStringLiteral("0"));
}

QString decodiumBridgeMirrorEntryKey(const QVariantMap& entry)
{
    return decodeMirrorEntryKey(entry);
}

static QString decodeMirrorSemanticKey(QVariantMap const& entry)
{
    return entry.value(QStringLiteral("mode")).toString().trimmed().toUpper()
        + QLatin1Char('|')
        + entry.value(QStringLiteral("freq")).toString().trimmed()
        + QLatin1Char('|')
        + canonicalDecodeMessage(entry.value(QStringLiteral("message")).toString())
        + QLatin1Char('|')
        + (entry.value(QStringLiteral("isTx")).toBool()
               ? QStringLiteral("1") : QStringLiteral("0"));
}
QString decodiumBridgeMirrorSemanticKey(QVariantMap const& entry) { return decodeMirrorSemanticKey(entry); }


static QString decodeRxClearEntryKey(QVariantMap const& entry)
{
    QString key = decodeMirrorEntryKey(entry);
    QString const time = entry.value(QStringLiteral("time")).toString().trimmed();
    if (time.isEmpty()) {
        qulonglong const timestamp =
            entry.value(QStringLiteral("timestamp")).toULongLong();
        if (timestamp > 0) {
            key += QStringLiteral("|ts=") + QString::number(timestamp);
        }
    }
    return key;
}

QString decodiumBridgeDecodeRxClearEntryKey(QVariantMap const& entry)
{
    return decodeRxClearEntryKey(entry);
}

static QVariantList decodeEntriesNotInPrevious(QVariantList const& entries,
                                                QVariantList const& previousEntries)
{
    QSet<QString> previousKeys;
    previousKeys.reserve(previousEntries.size());
    for (QVariant const& value : previousEntries) {
        QString const key = decodeMirrorEntryKey(value.toMap());
        if (!key.isEmpty()) {
            previousKeys.insert(key);
        }
    }

    QVariantList added;
    added.reserve(qMax(0, entries.size() - previousEntries.size()));
    for (QVariant const& value : entries) {
        QVariantMap const entry = value.toMap();
        QString const key = decodeMirrorEntryKey(entry);
        if (key.isEmpty() || !previousKeys.contains(key)) {
            added.append(entry);
        }
    }
    return added;
}

QVariantList decodiumBridgeDecodeEntriesNotInPrevious(const QVariantList& entries, const QVariantList& previous) { return decodeEntriesNotInPrevious(entries, previous); }

QString decodeMirrorLooseKey(QVariantMap const& entry)
{
    return entry.value("db").toString().trimmed() + "|" +
           entry.value("dt").toString().trimmed() + "|" +
           entry.value("freq").toString().trimmed() + "|" +
           canonicalDecodeMessage(entry.value("message").toString()) + "|" +
           (entry.value("isTx").toBool() ? QStringLiteral("1") : QStringLiteral("0"));
}

