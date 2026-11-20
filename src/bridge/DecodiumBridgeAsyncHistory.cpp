#include "DecodiumBridge.h"
#include "AdifDocumentInternal.h"
#include "CallsignIntelligenceService.h"
#include "DxccLookup.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>
#include <QTimer>
#include <memory>
#include <utility>

using namespace decodium::adif_document;
void decodiumBridgeAsyncHistoryLog(const QString& message);
void rebuildWorkedCallsFromDocument(QSet<QString>&, QList<ParsedAdifRecord> const&);

void DecodiumBridge::loadDxccLookupAsync()
{
    if (m_shuttingDown || !m_dxccLookup || m_dxccLookup->isLoaded()
        || m_dxccLookupLoading || m_dxccLookupLoadAttempted) {
        return;
    }

    m_dxccLookupLoading = true;
    m_dxccLookupLoadAttempted = true;
    quint64 const serial = ++m_dxccLookupLoadSerial;
    QStringList const candidates = ctyDatSearchPaths();
    QPointer<DecodiumBridge> self(this);
    auto* task = QRunnable::create([self, candidates, serial]() {
        QElapsedTimer elapsed;
        elapsed.start();
        auto lookup = std::make_shared<DxccLookup>();
        QString loadedPath;
        for (QString const& path : candidates) {
            if (QFile::exists(path) && lookup->loadCtyDat(path)) {
                loadedPath = path;
                break;
            }
        }
        qint64 const elapsedMs = elapsed.elapsed();
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, serial, lookup = std::move(lookup),
                                         loadedPath, elapsedMs]() mutable {
            if (!self || serial != self->m_dxccLookupLoadSerial) {
                return;
            }
            self->m_dxccLookupLoading = false;
            if (!loadedPath.isEmpty() && lookup->isLoaded()) {
                *self->m_dxccLookup = std::move(*lookup);
                decodiumBridgeAsyncHistoryLog(QStringLiteral("DXCC async loaded %1 entities from %2 in %3 ms")
                              .arg(self->m_dxccLookup->entityCount())
                              .arg(loadedPath)
                              .arg(elapsedMs));
                if (self->m_callsignIntelligence)
                    self->m_callsignIntelligence->notifyDxccDataChanged();
                emit self->localDataStateChanged();
                self->refreshDecodeListDxcc();
            } else {
                decodiumBridgeAsyncHistoryLog(QStringLiteral("DXCC async load finished without cty.dat in %1 ms")
                              .arg(elapsedMs));
            }
            self->loadWorkedBeforeHistoryAsync();
        }, Qt::QueuedConnection);
    });
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task, -1);
}

void DecodiumBridge::loadWorkedBeforeHistoryAsync()
{
    if (m_shuttingDown || m_workedHistoryLoading || m_workedHistoryLoaded) {
        return;
    }
    if (!m_dxccLookupLoadAttempted) {
        loadDxccLookupAsync();
        return;
    }
    if (m_dxccLookupLoading) {
        return;
    }

    QString const path = ensureAdifLogPath();
    m_workedHistoryLoading = true;
    QPointer<DecodiumBridge> self(this);
    auto* task = QRunnable::create([self, path]() {
        QElapsedTimer elapsed;
        elapsed.start();
        ParsedAdifDocument document = loadAdifDocument(path);
        QFileInfo const parsedInfo(path);
        qint64 const parsedSize = parsedInfo.exists() ? parsedInfo.size() : -1;
        qint64 const parsedModifiedMs = parsedInfo.exists()
            ? parsedInfo.lastModified().toMSecsSinceEpoch()
            : -1;
        qint64 const elapsedMs = elapsed.elapsed();
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, path, document = std::move(document),
                                         parsedSize, parsedModifiedMs, elapsedMs]() mutable {
            if (!self) {
                return;
            }
            self->m_workedHistoryLoading = false;
            QString const activePath = self->effectiveAdifLogPath();
            if (QDir::cleanPath(activePath) != QDir::cleanPath(path)) {
                decodiumBridgeAsyncHistoryLog(QStringLiteral("Worked-before async preload profile changed; retrying old=%1 new=%2")
                              .arg(path, activePath));
                QTimer::singleShot(100, self.data(), [self]() {
                    if (self) {
                        self->loadWorkedBeforeHistoryAsync();
                    }
                });
                return;
            }
            QFileInfo const currentInfo(path);
            qint64 const currentSize = currentInfo.exists() ? currentInfo.size() : -1;
            qint64 const currentModifiedMs = currentInfo.exists()
                ? currentInfo.lastModified().toMSecsSinceEpoch()
                : -1;
            if (currentSize != parsedSize || currentModifiedMs != parsedModifiedMs) {
                decodiumBridgeAsyncHistoryLog(QStringLiteral("Worked-before async preload changed during read; retrying path=%1")
                              .arg(path));
                QTimer::singleShot(100, self.data(), [self]() {
                    if (self) {
                        self->loadWorkedBeforeHistoryAsync();
                    }
                });
                return;
            }

            rebuildWorkedCallsFromDocument(self->m_workedCalls, document.records);
            self->rebuildWorkedSetsFromAdifRecords(document.records);
            self->m_qsoCountCache = document.records.size();
            self->m_workedHistoryLoaded = true;
            self->invalidateQsoSearchCache();
            self->refreshDecodeListDxcc();
            emit self->qsoCountChanged();
            emit self->workedCountChanged();
            decodiumBridgeAsyncHistoryLog(QStringLiteral("Worked-before async preload: %1 QSO, %2 DXCC, %3 zone CQ, %4 grid in %5 ms")
                          .arg(self->m_qsoCountCache)
                          .arg(self->m_worked.dxccEver.size())
                          .arg(self->m_worked.cqZoneEver.size())
                          .arg(self->m_worked.gridEver.size())
                          .arg(elapsedMs));
        }, Qt::QueuedConnection);
    });
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task, -1);
}

