#include "AdifDocumentInternal.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QVariantList>

namespace decodium::adif_document {
static bool copyFileAtomically(QString const& sourcePath, QString const& destinationPath,
                               QString* errorMessage)
{
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = source.errorString();
        }
        return false;
    }

    QSaveFile destination(destinationPath);
    if (!destination.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = destination.errorString();
        }
        return false;
    }

    QByteArray buffer;
    buffer.resize(1024 * 1024);
    while (!source.atEnd()) {
        const qint64 read = source.read(buffer.data(), buffer.size());
        if (read < 0) {
            if (errorMessage) {
                *errorMessage = source.errorString();
            }
            destination.cancelWriting();
            return false;
        }
        if (read == 0) {
            break;
        }
        if (destination.write(buffer.constData(), read) != read) {
            if (errorMessage) {
                *errorMessage = destination.errorString();
            }
            destination.cancelWriting();
            return false;
        }
    }

    if (!destination.commit()) {
        if (errorMessage) {
            *errorMessage = destination.errorString();
        }
        return false;
    }
    return true;
}

static QString uniqueAdifBackupPath(QString const& logbookPath)
{
    QFileInfo const info(logbookPath);
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString base = info.absolutePath() + QDir::separator()
        + info.completeBaseName() + QStringLiteral(".backup.") + stamp;

    QString candidate = base + QStringLiteral(".adi");
    for (int suffix = 1; QFile::exists(candidate); ++suffix) {
        candidate = base + QStringLiteral(".%1.adi").arg(suffix);
    }
    return candidate;
}

QVariantMap importAdifDocumentInBackground(QString const& sourcePath,
                                                   QString const& destinationPath,
                                                   AdifImportProgressCallback const& reportProgress)
{
    QVariantMap result;
    const auto report = [&reportProgress](int progress, QString const& status) {
        if (reportProgress) {
            reportProgress(qBound(0, progress, 100), status);
        }
    };

    const QString sourceFilePath = QFileInfo(sourcePath).absoluteFilePath();
    const QString destinationFilePath = QFileInfo(destinationPath).absoluteFilePath();
    if (sourceFilePath == destinationFilePath) {
        result.insert(QStringLiteral("error"), QStringLiteral("il file sorgente è già il logbook attivo"));
        return result;
    }

    QFileInfo const sourceBefore(sourceFilePath);
    if (!sourceBefore.exists() || !sourceBefore.isFile()) {
        result.insert(QStringLiteral("error"), QStringLiteral("file ADIF sorgente non trovato"));
        return result;
    }

    QFileInfo const destinationBefore(destinationFilePath);
    const bool destinationExisted = destinationBefore.exists();
    const qint64 destinationSize = destinationExisted ? destinationBefore.size() : -1;
    const QDateTime destinationModified = destinationExisted ? destinationBefore.lastModified() : QDateTime();

    report(3, QStringLiteral("Apertura del file ADIF sorgente..."));
    ParsedAdifDocument source = loadAdifDocument(sourceFilePath);
    if (!source.loaded) {
        result.insert(QStringLiteral("error"), QStringLiteral("impossibile leggere il file ADIF sorgente"));
        return result;
    }
    const int sourceCount = source.records.size();
    result.insert(QStringLiteral("sourceCount"), sourceCount);
    if (sourceCount <= 0) {
        result.insert(QStringLiteral("error"), QStringLiteral("il file ADIF non contiene QSO validi"));
        return result;
    }
    report(20, QStringLiteral("File sorgente letto: %1 QSO...").arg(sourceCount));

    ParsedAdifDocument destination = loadAdifDocument(destinationFilePath);
    if (destinationExisted && !destination.loaded) {
        result.insert(QStringLiteral("error"), QStringLiteral("impossibile leggere il logbook ADIF attivo"));
        return result;
    }
    report(35, QStringLiteral("Logbook attivo letto: %1 QSO...").arg(destination.records.size()));

    QSet<QString> existingKeys;
    existingKeys.reserve(destination.records.size() + source.records.size());
    for (ParsedAdifRecord const& record : destination.records) {
        const QString key = adifRecordDedupeKey(record.fields);
        if (!key.isEmpty()) {
            existingKeys.insert(key);
        }
    }

    int imported = 0;
    for (int index = 0; index < source.records.size(); ++index) {
        ParsedAdifRecord normalizedRecord = normalizeImportedAdifRecord(source.records.at(index));
        const QString key = adifRecordDedupeKey(normalizedRecord.fields);
        if (!key.isEmpty() && !existingKeys.contains(key)) {
            destination.records.append(normalizedRecord);
            existingKeys.insert(key);
            ++imported;
        }

        if ((index & 0x1FF) == 0 || index + 1 == source.records.size()) {
            const int mergeProgress = 38 + ((index + 1) * 37 / qMax(1, source.records.size()));
            report(mergeProgress,
                   QStringLiteral("Analisi QSO: %1/%2...").arg(index + 1).arg(source.records.size()));
        }
    }

    const QFileInfo sourceAfter(sourceFilePath);
    const QFileInfo destinationAfterMerge(destinationFilePath);
    const bool sourceChanged = sourceAfter.exists() != sourceBefore.exists()
        || (sourceAfter.exists()
            && (sourceAfter.size() != sourceBefore.size()
                || sourceAfter.lastModified() != sourceBefore.lastModified()));
    const bool destinationChanged = destinationAfterMerge.exists() != destinationExisted
        || (destinationAfterMerge.exists()
            && (destinationAfterMerge.size() != destinationSize
                || destinationAfterMerge.lastModified() != destinationModified));
    if (sourceChanged) {
        result.insert(QStringLiteral("error"), QStringLiteral("il file sorgente è cambiato durante l'importazione; riprovare"));
        return result;
    }
    if (destinationChanged) {
        result.insert(QStringLiteral("error"), QStringLiteral("il logbook è cambiato durante l'importazione; riprovare"));
        return result;
    }

    QString backupPath;
    if (imported > 0) {
        if (destinationExisted) {
            backupPath = uniqueAdifBackupPath(destinationFilePath);
            report(82, QStringLiteral("Creazione del backup prima della scrittura..."));
            QString backupError;
            if (!copyFileAtomically(destinationFilePath, backupPath, &backupError)) {
                result.insert(QStringLiteral("error"),
                              QStringLiteral("backup del logbook non riuscito: %1").arg(backupError));
                return result;
            }
            result.insert(QStringLiteral("backupPath"), backupPath);
        }

        report(91, QStringLiteral("Salvataggio del logbook aggiornato..."));
        if (!writeAdifDocument(destinationFilePath, destination)) {
            result.insert(QStringLiteral("error"),
                          backupPath.isEmpty()
                              ? QStringLiteral("impossibile salvare il logbook ADIF aggiornato")
                              : QStringLiteral("impossibile salvare il logbook ADIF aggiornato; backup disponibile in %1")
                                    .arg(backupPath));
            return result;
        }
    }

    QStringList workedCalls;
    QSet<QString> workedCallSet;
    workedCallSet.reserve(destination.records.size());
    for (ParsedAdifRecord const& record : destination.records) {
        const QString call = record.fields.value(QStringLiteral("CALL")).trimmed().toUpper();
        if (!call.isEmpty() && !workedCallSet.contains(call)) {
            workedCallSet.insert(call);
            workedCalls.append(call);
        }
    }

    const int skipped = qMax(0, sourceCount - imported);
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("imported"), imported);
    result.insert(QStringLiteral("skipped"), skipped);
    result.insert(QStringLiteral("total"), destination.records.size());
    result.insert(QStringLiteral("backupPath"), backupPath);
    result.insert(QStringLiteral("workedCalls"), workedCalls);
    report(100, imported > 0
        ? QStringLiteral("Importazione completata: %1 nuovi QSO.").arg(imported)
        : QStringLiteral("Importazione completata: nessun QSO nuovo."));
    return result;
}

QVariantMap importConfirmedAdifDocumentInBackground(QString const& sourcePath,
                                                           QString const& destinationPath,
                                                           QString const& provider)
{
    QVariantMap result;
    QFileInfo const destinationBefore(destinationPath);
    const bool destinationExisted = destinationBefore.exists();
    const qint64 destinationSize = destinationExisted ? destinationBefore.size() : -1;
    const QDateTime destinationModified = destinationExisted ? destinationBefore.lastModified() : QDateTime();

    ParsedAdifDocument source = loadAdifDocument(sourcePath);
    if (!source.loaded) {
        const QString providerLabel = provider == QStringLiteral("lotw_confirmed")
            ? QStringLiteral("LoTW")
            : provider == QStringLiteral("qrz_confirmed")
              ? QStringLiteral("QRZ")
              : QStringLiteral("eQSL InBox");
        result.insert(QStringLiteral("error"),
                      QStringLiteral("il file %1 non contiene ADI valido").arg(providerLabel));
        return result;
    }
    const int sourceCount = source.records.size();

    QFileInfo const destinationInfo(destinationPath);
    ParsedAdifDocument destination = loadAdifDocument(destinationPath);
    if (destinationInfo.exists() && !destination.loaded) {
        result.insert(QStringLiteral("error"), QStringLiteral("impossibile leggere il logbook ADIF attivo"));
        return result;
    }

    QHash<QString, int> existingIndexes;
    for (int i = 0; i < destination.records.size(); ++i) {
        const QString key = adifRecordDedupeKey(destination.records.at(i).fields);
        if (!key.isEmpty() && !existingIndexes.contains(key)) {
            existingIndexes.insert(key, i);
        }
    }

    QVariantList newWorkedRecords;
    int imported = 0;
    int updated = 0;
    bool changed = false;
    for (ParsedAdifRecord incoming : source.records) {
        incoming = normalizeImportedAdifRecord(incoming);
        const QString call = incoming.fields.value(QStringLiteral("CALL")).trimmed().toUpper();
        const QString key = adifRecordDedupeKey(incoming.fields);
        if (call.isEmpty() || key.isEmpty()) {
            continue;
        }

        QStringList confirmationFields;
        if (provider == QStringLiteral("lotw_confirmed")) {
            // LoTW returns matched records with QSL_RCVD=Y.  Keep the
            // provider-specific fields as well, because award calculations
            // may need the LoTW match timestamp and credit information.
            incoming.fields.insert(QStringLiteral("QSL_RCVD"), QStringLiteral("Y"));
            incoming.fields.insert(QStringLiteral("LOTW_QSL_RCVD"), QStringLiteral("Y"));
            const QString rxQsl = incoming.fields.value(QStringLiteral("APP_LOTW_RXQSL")).trimmed();
            if (incoming.fields.value(QStringLiteral("QSLRDATE")).trimmed().isEmpty() && rxQsl.size() >= 10) {
                incoming.fields.insert(QStringLiteral("QSLRDATE"), rxQsl.left(10).remove(QLatin1Char('-')));
            }
            confirmationFields = {
                QStringLiteral("QSL_RCVD"),
                QStringLiteral("LOTW_QSL_RCVD"),
                QStringLiteral("QSLRDATE"),
                QStringLiteral("APP_LOTW_RXQSL"),
                QStringLiteral("APP_LOTW_RXQSO"),
                QStringLiteral("APP_LOTW_2xQSL"),
                QStringLiteral("APP_LOTW_QSLMODE"),
                QStringLiteral("CREDIT_GRANTED"),
                QStringLiteral("APP_LOTW_CREDIT_GRANTED"),
                QStringLiteral("CREDIT_SUBMITTED"),
                QStringLiteral("APP_LOTW_CREDIT_SUBMITTED")
            };
        } else if (provider == QStringLiteral("qrz_confirmed")) {
            // QRZ FETCH is filtered to confirmed contacts.  Keep the QRZ
            // provenance fields and also set the standard ADIF confirmation
            // flag consumed by awards and logbook filters.
            incoming.fields.insert(QStringLiteral("QSL_RCVD"), QStringLiteral("Y"));
            const QString qrzDate = incoming.fields.value(QStringLiteral("APP_QRZLOG_QSLDATE")).trimmed();
            if (!qrzDate.isEmpty() && incoming.fields.value(QStringLiteral("QSLRDATE")).trimmed().isEmpty()) {
                incoming.fields.insert(QStringLiteral("QSLRDATE"), qrzDate);
            }
            confirmationFields = {
                QStringLiteral("QSL_RCVD"),
                QStringLiteral("QSLRDATE"),
                QStringLiteral("APP_QRZLOG_STATUS"),
                QStringLiteral("APP_QRZLOG_QSLDATE")
            };
        } else {
            // An eQSL InBox record is itself proof that an eQSL was received.
            // Force the standard ADIF confirmation field even when an older
            // eQSL export omitted it; awards consume this field from the active log.
            incoming.fields.insert(QStringLiteral("EQSL_QSL_RCVD"), QStringLiteral("Y"));
            confirmationFields = {
                QStringLiteral("EQSL_QSL_RCVD"),
                QStringLiteral("EQSL_QSLRDATE"),
                QStringLiteral("APP_EQSL_AG")
            };
        }

        const auto existing = existingIndexes.constFind(key);
        if (existing == existingIndexes.constEnd()) {
            destination.records.append(incoming);
            existingIndexes.insert(key, destination.records.size() - 1);
            ++imported;
            changed = true;

            QVariantMap worked;
            worked.insert(QStringLiteral("call"), call);
            worked.insert(QStringLiteral("grid"), incoming.fields.value(QStringLiteral("GRIDSQUARE")));
            worked.insert(QStringLiteral("freq"), static_cast<qulonglong>(adifFrequencyHzFromFields(incoming.fields)));
            worked.insert(QStringLiteral("mode"), normalizeAdifModeForDisplay(incoming.fields));
            worked.insert(QStringLiteral("date"), incoming.fields.value(QStringLiteral("QSO_DATE")));
            newWorkedRecords.append(worked);
            continue;
        }

        ParsedAdifRecord& current = destination.records[existing.value()];
        bool recordChanged = false;
        for (const QString& field : confirmationFields) {
            const QString incomingValue = incoming.fields.value(field).trimmed();
            if (!incomingValue.isEmpty() && current.fields.value(field) != incomingValue) {
                current.fields.insert(field, incomingValue);
                recordChanged = true;
            }
        }
        ++updated;
        changed = changed || recordChanged;
    }

    // Do not overwrite a logbook that changed while the background merge was
    // running (for example because a QSO was logged in the meantime).
    QFileInfo const destinationAfter(destinationPath);
    const bool changedDuringImport = destinationAfter.exists() != destinationExisted
        || (destinationAfter.exists()
            && (destinationAfter.size() != destinationSize
                || destinationAfter.lastModified() != destinationModified));
    if (changedDuringImport) {
        result.insert(QStringLiteral("error"), QStringLiteral("il logbook è cambiato durante la sincronizzazione; riprovare"));
        return result;
    }

    if (changed && !writeAdifDocument(destinationPath, destination)) {
        result.insert(QStringLiteral("error"), QStringLiteral("impossibile salvare il logbook ADIF aggiornato"));
        return result;
    }

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("imported"), imported);
    result.insert(QStringLiteral("updated"), updated);
    result.insert(QStringLiteral("sourceCount"), sourceCount);
    result.insert(QStringLiteral("total"), destination.records.size());
    result.insert(QStringLiteral("newWorkedRecords"), newWorkedRecords);
    return result;
}

} // namespace decodium::adif_document
