#include "DecodiumBridge.h"
#include "AdifDocumentInternal.h"
#include "MapIntelligenceService.h"
#include "CallsignIntelligenceService.h"
#include <QFileInfo>
#include <QFutureWatcher>
#include <QPointer>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

using namespace decodium::adif_document;
extern void decodiumBridgeLogbookLog(const QString&);

void DecodiumBridge::importConfirmedAdifIntoLogbookAsync(const QString& provider, const QString& path)
{
    if (m_shuttingDown || !m_callsignIntelligence) {
        return;
    }
    if (m_confirmedAdifImportWatcher) {
        m_callsignIntelligence->completeConfirmedAdifImport(
            provider, false, 0, 0, 0, QStringLiteral("un'altra sincronizzazione conferme è già in corso"));
        return;
    }

    const QString sourcePath = path;
    const QString destinationPath = ensureAdifLogPath();
    m_confirmedAdifImportWatcher = new QFutureWatcher<QVariantMap>(this);
    auto* watcher = m_confirmedAdifImportWatcher;
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, provider]() {
        const QVariantMap result = watcher->result();
        if (m_confirmedAdifImportWatcher == watcher) {
            m_confirmedAdifImportWatcher = nullptr;
        }
        watcher->deleteLater();

        const bool ok = result.value(QStringLiteral("ok")).toBool();
        if (ok) {
            const QVariantList newWorkedRecords = result.value(QStringLiteral("newWorkedRecords")).toList();
            for (const QVariant& value : newWorkedRecords) {
                const QVariantMap record = value.toMap();
                const QString call = record.value(QStringLiteral("call")).toString().trimmed().toUpper();
                if (call.isEmpty()) {
                    continue;
                }
                m_workedCalls.insert(call);
                appendWorkedQso(call,
                                record.value(QStringLiteral("grid")).toString(),
                                record.value(QStringLiteral("freq")).toULongLong(),
                                record.value(QStringLiteral("mode")).toString(),
                                record.value(QStringLiteral("date")).toString());
            }
            m_qsoCountCache = result.value(QStringLiteral("total")).toInt();
            invalidateQsoSearchCache();
            warmLogCacheAsync();
            emit qsoCountChanged();
            emit workedCountChanged();
            emit qsoLogCacheChanged();
            if (m_mapIntelligenceService) {
                m_mapIntelligenceService->reloadFromAdif(effectiveAdifLogPath());
            }
            m_callsignIntelligence->completeConfirmedAdifImport(
                provider,
                true,
                result.value(QStringLiteral("imported")).toInt(),
                result.value(QStringLiteral("updated")).toInt(),
                result.value(QStringLiteral("sourceCount")).toInt());
        } else {
            const QString error = result.value(QStringLiteral("error"),
                                                QStringLiteral("sincronizzazione conferme non riuscita")).toString();
            m_callsignIntelligence->completeConfirmedAdifImport(provider, false, 0, 0, 0, error);
        }
    });
    emit statusMessage(QStringLiteral("Sincronizzazione %1 nel logbook in background...").arg(provider));
    watcher->setFuture(QtConcurrent::run([sourcePath, destinationPath, provider]() {
        return importConfirmedAdifDocumentInBackground(sourcePath, destinationPath, provider);
    }));
}

void DecodiumBridge::setAdifImportProgress(int progress, const QString& status)
{
    const int boundedProgress = qBound(0, progress, 100);
    if (m_adifImportProgress == boundedProgress && m_adifImportStatus == status) {
        return;
    }
    m_adifImportProgress = boundedProgress;
    m_adifImportStatus = status;
    emit adifImportProgressChanged();
}

bool DecodiumBridge::importFromAdifAsync(const QString& filename)
{
    if (m_shuttingDown) {
        return false;
    }
    if (m_adifImportWatcher || m_adifImportInProgress) {
        const QString message = QStringLiteral("Un'importazione ADIF è già in corso.");
        setAdifImportProgress(m_adifImportProgress, message);
        emit statusMessage(message);
        return false;
    }

    QString sourcePath = filename.trimmed();
    const QUrl sourceUrl(sourcePath);
    if (sourceUrl.isLocalFile()) {
        sourcePath = sourceUrl.toLocalFile();
    }
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        const QString message = QStringLiteral("Impossibile importare ADIF: file sorgente non trovato.");
        setAdifImportProgress(0, message);
        emit errorMessage(message);
        return false;
    }

    const QString destinationPath = ensureAdifLogPath();
    if (destinationPath.isEmpty()) {
        const QString message = QStringLiteral("Impossibile importare ADIF: logbook attivo non disponibile.");
        setAdifImportProgress(0, message);
        emit errorMessage(message);
        return false;
    }

    m_adifImportInProgress = true;
    setAdifImportProgress(0, QStringLiteral("Preparazione importazione ADIF..."));
    emit statusMessage(QStringLiteral("Importazione ADIF in background..."));

    m_adifImportWatcher = new QFutureWatcher<QVariantMap>(this);
    QFutureWatcher<QVariantMap>* const watcher = m_adifImportWatcher;
    QPointer<DecodiumBridge> guard(this);

    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher]() {
        const QVariantMap result = watcher->result();
        if (m_adifImportWatcher == watcher) {
            m_adifImportWatcher = nullptr;
        }
        watcher->deleteLater();

        const bool success = result.value(QStringLiteral("ok")).toBool();
        const int imported = result.value(QStringLiteral("imported")).toInt();
        const int skipped = result.value(QStringLiteral("skipped")).toInt();
        const int total = result.value(QStringLiteral("total")).toInt();
        const QString backupPath = result.value(QStringLiteral("backupPath")).toString();
        QString message;

        m_adifImportInProgress = false;
        if (success) {
            const QStringList workedCalls = result.value(QStringLiteral("workedCalls")).toStringList();
            m_workedCalls.clear();
            for (const QString& call : workedCalls) {
                m_workedCalls.insert(call);
            }
            m_qsoCountCache = total;
            invalidateQsoSearchCache();
            warmLogCacheAsync();
            emit qsoCountChanged();
            emit workedCountChanged();
            if (m_mapIntelligenceService && imported > 0) {
                m_mapIntelligenceService->reloadFromAdif(effectiveAdifLogPath());
            }

            message = QStringLiteral("Importazione ADIF completata: %1 nuovi QSO, %2 ignorati, totale %3.")
                          .arg(imported)
                          .arg(skipped)
                          .arg(total);
            if (!backupPath.isEmpty()) {
                message += QStringLiteral(" Backup creato: %1").arg(backupPath);
            }
            setAdifImportProgress(100, message);
            emit statusMessage(message);
            decodiumBridgeLogbookLog(QStringLiteral("importFromAdif async: source=%1 imported=%2 skipped=%3 total=%4 backup=%5")
                          .arg(result.value(QStringLiteral("sourcePath")).toString())
                          .arg(imported)
                          .arg(skipped)
                          .arg(total)
                          .arg(backupPath));
        } else {
            message = result.value(QStringLiteral("error"),
                                   QStringLiteral("Importazione ADIF non riuscita.")).toString();
            setAdifImportProgress(0, message);
            emit errorMessage(message);
        }

        emit adifImportFinished(success, imported, skipped, total, backupPath, message);
    });

    watcher->setFuture(QtConcurrent::run([guard, sourcePath, destinationPath]() {
        const AdifImportProgressCallback reportProgress = [guard](int progress, const QString& status) {
            if (!guard) {
                return;
            }
            QMetaObject::invokeMethod(guard.data(), [guard, progress, status]() {
                if (guard && guard->m_adifImportInProgress) {
                    guard->setAdifImportProgress(progress, status);
                }
            }, Qt::QueuedConnection);
        };
        QVariantMap result = importAdifDocumentInBackground(sourcePath, destinationPath, reportProgress);
        result.insert(QStringLiteral("sourcePath"), sourcePath);
        return result;
    }));
    return true;
}

int DecodiumBridge::importFromAdif(const QString& filename)
{
    // Compatibility wrapper: imports are now asynchronous. A positive return
    // value means that the job was accepted, not that all records are done.
    return importFromAdifAsync(filename) ? 1 : 0;
}

