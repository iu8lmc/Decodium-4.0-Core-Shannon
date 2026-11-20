#include "DecodiumBridge.h"
#include "AdifDocumentInternal.h"
#include "AdifExportSanitizer.h"
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <algorithm>

using namespace decodium::adif_document;
extern void rebuildWorkedCallsFromDocument(QSet<QString>&, QList<ParsedAdifRecord> const&);
extern QString exportRecordFingerprint(ParsedAdifRecord const&);
extern void setAdifModeFields(QMap<QString, QString>&, QString const&);

bool DecodiumBridge::exportSelectedToAdif(const QString& filename, const QVariantList& selection)
{
    if (filename.isEmpty() || selection.isEmpty()) return false;
    QString const sourcePath = effectiveAdifLogPath();
    // Never allow a partial export to replace the active log (including symlinks).
    QFileInfo const target(filename), source(sourcePath);
    if (target.absoluteFilePath() == source.absoluteFilePath()
        || (target.exists() && target.canonicalFilePath() == source.canonicalFilePath())) {
        emit errorMessage(tr("Choose a different file: the active logbook cannot be overwritten."));
        return false;
    }
    ParsedAdifDocument const document = loadAdifDocument(sourcePath);
    ParsedAdifDocument subset;
    subset.header = document.header;
    QSet<int> seen;
    for (auto const& value : selection) {
        auto const row = value.toMap();
        bool validIndex = false;
        int const index = row.value(QStringLiteral("exportIndex")).toInt(&validIndex);
        if (!document.loaded || !validIndex || index < 0 || index >= document.records.size()
            || row.value(QStringLiteral("exportLogPath")).toString() != sourcePath
            || row.value(QStringLiteral("exportFingerprint")).toString()
                != exportRecordFingerprint(document.records.at(index))) {
            emit errorMessage(tr("The logbook changed. Refresh it and select the QSOs again."));
            return false;
        }
        if (!seen.contains(index)) {
            seen.insert(index);
            subset.records.append(document.records.at(index));
        }
    }
    if (!writeAdifDocument(filename, subset)) {
        emit errorMessage(tr("Unable to export the selected QSOs."));
        return false;
    }
    return true;
}

bool DecodiumBridge::exportToAdif(const QString& filename)
{
    if (filename.isEmpty()) {
        return false;
    }

    QFile source(effectiveAdifLogPath());
    if (!source.open(QIODevice::ReadOnly)) {
        emit errorMessage(QStringLiteral("Impossibile esportare ADIF: %1").arg(filename));
        return false;
    }

    // Export through a save file so a legacy log containing
    // <MY_IOTA:4>NONE is cleaned in the exported copy without mutating the
    // active logbook.  TQSL requires an unset MY_IOTA field to be omitted.
    QByteArray const data = decodium::adif::sanitizeExport(source.readAll());
    source.close();

    QSaveFile destination(filename);
    if (!destination.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit errorMessage(QStringLiteral("Impossibile esportare ADIF: %1")
                              .arg(destination.errorString()));
        return false;
    }
    if (destination.write(data) != data.size() || !destination.commit()) {
        emit errorMessage(QStringLiteral("Impossibile esportare ADIF: %1")
                              .arg(destination.errorString()));
        return false;
    }
    return true;
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
    rebuildWorkedSetsFromAdifRecords(doc.records);
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
    rebuildWorkedSetsFromAdifRecords(doc.records);
    m_qsoCountCache = doc.records.size();
    invalidateQsoSearchCache();
    warmLogCacheAsync();
    emit qsoCountChanged();
    emit workedCountChanged();
    return true;
}

