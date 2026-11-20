#include "DecodiumBridge.h"
#include "DxccLookup.h"
#include "CallsignIntelligenceService.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRunnable>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThreadPool>

void decodiumBridgeLocalUpdateLog(const QString&);
bool decodiumBridgeEnsureParentDirectoryForFile(const QString&, QString*);

static QStringList ctyDatDownloadUrls()
{
    return {
        QStringLiteral("https://www.country-files.com/bigcty/cty.dat"),
        QStringLiteral("https://www.country-files.com/cty/cty.dat")
    };
}

static bool ctyDatPayloadLooksValid(const QByteArray& payload, QString* reason = nullptr)
{
    auto fail = [reason](const QString& text) {
        if (reason)
            *reason = text;
        return false;
    };

    QByteArray const trimmed = payload.left(4096).trimmed();
    QByteArray const lower = trimmed.toLower();
    if (payload.size() < 20000)
        return fail(QStringLiteral("file troppo piccolo (%1 byte)").arg(payload.size()));
    if (lower.startsWith("<!doctype") || lower.startsWith("<html") || lower.contains("<html"))
        return fail(QStringLiteral("il server ha restituito HTML invece di cty.dat"));

    const QString head = QString::fromLatin1(payload.left(16384));
    const QStringList lines = head.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    int countryRows = 0;
    for (const QString& line : lines) {
        const QString row = line.trimmed();
        if (row.count(QLatin1Char(':')) >= 7 && row.endsWith(QLatin1Char(':')))
            ++countryRows;
        if (countryRows >= 2)
            return true;
    }

    return fail(QStringLiteral("formato cty.dat non riconosciuto"));
}

static QNetworkRequest makeCtyDatRequest(const QUrl& url)
{
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader("User-Agent",
                     "Decodium/4.0 cty.dat updater (+https://github.com/elisir80/Decodium-4.0-Core-Shannon)");
    req.setRawHeader("Accept", "text/plain, application/octet-stream, */*");
    req.setRawHeader("Accept-Language", "en-US,en;q=0.8");
    req.setRawHeader("Cache-Control", "no-cache");
    return req;
}

static bool issueNextCtyDatRequest(QNetworkAccessManager* nam)
{
    const QStringList urls = nam->property("ctyUrls").toStringList();
    int index = nam->property("ctyUrlIndex").toInt();
    if (index >= urls.size())
        return false;

    const QUrl url(urls.at(index));
    nam->setProperty("ctyUrlIndex", index + 1);
    decodiumBridgeLocalUpdateLog(QStringLiteral("cty.dat download attempt %1/%2: %3")
              .arg(index + 1)
              .arg(urls.size())
              .arg(url.toString()));
    nam->get(makeCtyDatRequest(url));
    return true;
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


void DecodiumBridge::checkCtyDatUpdate(bool forceDownload)
{
    if (offlineMode()) {
        emit statusMessage(QStringLiteral("Offline: cty.dat update disabled; local data remains available"));
        return;
    }
    if (m_ctyDatUpdating) {
        decodiumBridgeLocalUpdateLog("cty.dat update requested while another update is already running");
        emit statusMessage(tr("cty.dat download already under way..."));
        return;
    }

    m_ctyDatLastError.clear();
    emit localDataStateChanged();
    decodiumBridgeLocalUpdateLog("cty.dat update requested");

    // Cerca cty.dat nelle posizioni standard
    QStringList searchPaths = ctyDatSearchPaths();

    QString existing;
    for (const auto& p : searchPaths) {
        if (QFile::exists(p)) { existing = p; break; }
    }

    bool needsUpdate = forceDownload || existing.isEmpty();
    if (!needsUpdate && !existing.isEmpty()) {
        QFileInfo fi(existing);
        needsUpdate = fi.lastModified().daysTo(QDateTime::currentDateTime()) > 30;
    }

    if (!needsUpdate) {
        decodiumBridgeLocalUpdateLog("cty.dat already up to date: " + existing);
        emit statusMessage("cty.dat è aggiornato.");
        return;
    }

    m_ctyDatUpdating = true;
    emit ctyDatUpdatingChanged();
    emit localDataStateChanged();
    decodiumBridgeLocalUpdateLog(existing.isEmpty()
              ? "cty.dat missing, starting download"
              : "cty.dat stale, starting download from " + existing);
    emit statusMessage("Download cty.dat in corso...");

    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty())
        dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dataDir.isEmpty()) {
        m_ctyDatLastError = QStringLiteral("cartella dati utente non disponibile");
        m_ctyDatUpdating = false;
        emit ctyDatUpdatingChanged();
        emit localDataStateChanged();
        emit errorMessage(QStringLiteral("Download cty.dat fallito: cartella dati utente non disponibile."));
        return;
    }

    QString destPath = QDir(dataDir).absoluteFilePath(QStringLiteral("cty.dat"));
    QDir().mkpath(QFileInfo(destPath).absolutePath());
    decodiumBridgeLocalUpdateLog("cty.dat destination: " + destPath);

    QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    nam->setProperty("ctyUrls", ctyDatDownloadUrls());
    nam->setProperty("ctyUrlIndex", 0);
    nam->setProperty("ctyLastError", QString());

    connect(nam, &QNetworkAccessManager::finished, this, [this, destPath, nam](QNetworkReply* reply) {
        const QUrl sourceUrl = reply->url();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->error() == QNetworkReply::NoError && reply->isReadable()
            ? reply->readAll() : QByteArray();

        QString failureReason;
        if (reply->error() != QNetworkReply::NoError) {
            failureReason = statusCode > 0
                ? QStringLiteral("%1 (%2, HTTP %3)")
                      .arg(reply->errorString(), sourceUrl.toString())
                      .arg(statusCode)
                : QStringLiteral("%1 (%2)").arg(reply->errorString(), sourceUrl.toString());
        } else if (statusCode >= 400) {
            failureReason = QStringLiteral("HTTP %1 da %2").arg(statusCode).arg(sourceUrl.toString());
        } else {
            QString validationError;
            if (!ctyDatPayloadLooksValid(payload, &validationError)) {
                failureReason = QStringLiteral("%1 da %2")
                    .arg(validationError, sourceUrl.toString());
            }
        }

        reply->deleteLater();

        if (!failureReason.isEmpty()) {
            decodiumBridgeLocalUpdateLog("cty.dat download attempt failed: " + failureReason);
            nam->setProperty("ctyLastError", failureReason);
            if (issueNextCtyDatRequest(nam))
                return;

            nam->deleteLater();
            m_ctyDatLastError = failureReason;
            m_ctyDatUpdating = false;
            emit ctyDatUpdatingChanged();
            emit localDataStateChanged();
            emit errorMessage("Download cty.dat fallito: " + failureReason);
            return;
        }

        QSaveFile f(destPath);
        if (f.open(QIODevice::WriteOnly)) {
            const qint64 written = f.write(payload);
            bool commitOk = false;
            if (written == payload.size())
                commitOk = f.commit();
            else
                f.cancelWriting();
            const QString writeError = f.errorString();

            if (commitOk) {
                decodiumBridgeLocalUpdateLog(QStringLiteral("cty.dat downloaded from %1 (%2 bytes)")
                          .arg(sourceUrl.toString())
                          .arg(payload.size()));
                reloadDxccLookupAsync([this, nam](bool loaded, const QString& loadedPath) {
                    if (loaded) {
                        m_ctyDatLastError.clear();
                        m_workedHistoryLoaded = false;
                        loadWorkedBeforeHistoryAsync();
                        decodiumBridgeLocalUpdateLog("cty.dat updated and reloaded from " + loadedPath);
                        emit statusMessage(tr("cty.dat updated and loaded: %1").arg(loadedPath));
                    } else {
                        m_ctyDatLastError = QStringLiteral("caricamento DXCC fallito");
                        decodiumBridgeLocalUpdateLog("cty.dat downloaded but DXCC reload failed");
                        emit errorMessage("cty.dat scaricato ma il caricamento DXCC è fallito.");
                    }
                    nam->deleteLater();
                    m_ctyDatUpdating = false;
                    emit ctyDatUpdatingChanged();
                    emit localDataStateChanged();
                });
                return;
            } else {
                decodiumBridgeLocalUpdateLog(QStringLiteral("cty.dat save incomplete: wrote %1/%2 bytes, %3")
                          .arg(written)
                          .arg(payload.size())
                          .arg(writeError));
                m_ctyDatLastError = QStringLiteral("scrittura incompleta (%1/%2 byte). %3")
                                    .arg(written)
                                    .arg(payload.size())
                                    .arg(writeError);
                emit errorMessage(QStringLiteral("Impossibile salvare cty.dat: scrittura incompleta (%1/%2 byte). %3")
                                  .arg(written)
                                  .arg(payload.size())
                                  .arg(writeError));
            }
        } else {
            decodiumBridgeLocalUpdateLog("cty.dat save failed: " + f.errorString());
            m_ctyDatLastError = f.errorString();
            emit errorMessage("Impossibile salvare cty.dat: " + f.errorString());
        }
        nam->deleteLater();
        m_ctyDatUpdating = false;
        emit ctyDatUpdatingChanged();
        emit localDataStateChanged();
    });

    if (!issueNextCtyDatRequest(nam)) {
        nam->deleteLater();
        m_ctyDatLastError = QStringLiteral("nessun URL configurato");
        m_ctyDatUpdating = false;
        emit ctyDatUpdatingChanged();
        emit localDataStateChanged();
        emit errorMessage(QStringLiteral("Download cty.dat fallito: nessun URL configurato."));
    }
}

void DecodiumBridge::downloadCall3Txt()
{
    if (offlineMode()) {
        emit statusMessage(QStringLiteral("Offline: CALL3.TXT update disabled; local data remains available"));
        return;
    }
    if (m_call3TxtUpdating) {
        decodiumBridgeLocalUpdateLog("downloadCall3Txt: request ignored because an update is already running");
        emit statusMessage(QStringLiteral("Download CALL3.TXT già in corso..."));
        return;
    }

    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }
    if (dataDir.isEmpty()) {
        m_call3TxtLastError = QStringLiteral("cartella dati utente non disponibile");
        emit errorMessage(QStringLiteral("Download CALL3.TXT fallito: cartella dati utente non disponibile."));
        return;
    }

    QString const path = QDir(dataDir).absoluteFilePath(QStringLiteral("CALL3.TXT"));
    QString parentError;
    if (!decodiumBridgeEnsureParentDirectoryForFile(path, &parentError)) {
        m_call3TxtLastError = parentError;
        emit errorMessage(QStringLiteral("Impossibile salvare CALL3.TXT: %1").arg(parentError));
        return;
    }

    m_call3TxtLastError.clear();
    m_call3TxtUpdating = true;
    emit call3TxtUpdatingChanged();
    emit localDataStateChanged();

    decodiumBridgeLocalUpdateLog("downloadCall3Txt: starting download to " + path);
    emit statusMessage("Download CALL3.TXT in corso...");
    auto* mgr = new QNetworkAccessManager(this);

    QNetworkRequest req(QUrl(QStringLiteral("https://sourceforge.net/projects/wsjt-x-improved/files/Additional%20Files/CALL3.TXT/download")));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader("User-Agent", "Decodium/4.0 CALL3 updater (+https://github.com/elisir80/Decodium-4.0-Core-Shannon)");
    req.setRawHeader("Accept", "text/plain, application/octet-stream, */*");
    req.setRawHeader("Accept-Language", "en-US,en;q=0.8");
    req.setRawHeader("Cache-Control", "no-cache");

    auto* reply = mgr->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, mgr, path]() {
        bool statusOk = false;
        int const statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(&statusOk);

        auto cleanup = [this, reply, mgr]() {
            reply->deleteLater();
            mgr->deleteLater();
            m_call3TxtUpdating = false;
            emit call3TxtUpdatingChanged();
            emit localDataStateChanged();
        };

        if (reply->error() != QNetworkReply::NoError || (statusOk && statusCode >= 400)) {
            QString reason = reply->errorString();
            if (statusOk && statusCode >= 400) {
                reason = QStringLiteral("HTTP %1").arg(statusCode);
            }
            m_call3TxtLastError = reason;
            decodiumBridgeLocalUpdateLog("downloadCall3Txt failed: " + reason);
            emit errorMessage(QStringLiteral("Download CALL3.TXT fallito: %1").arg(reason));
            cleanup();
            return;
        }

        QByteArray const data = reply->error() == QNetworkReply::NoError && reply->isReadable()
            ? reply->readAll() : QByteArray();
        QString validationError;
        if (!isProbablyCall3TxtPayload(data, &validationError)) {
            m_call3TxtLastError = validationError;
            decodiumBridgeLocalUpdateLog("downloadCall3Txt invalid payload: " + validationError);
            emit errorMessage(QStringLiteral("Download CALL3.TXT non valido: %1").arg(validationError));
            cleanup();
            return;
        }

        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            m_call3TxtLastError = f.errorString();
            decodiumBridgeLocalUpdateLog("downloadCall3Txt save failed: " + f.errorString());
            emit errorMessage(QStringLiteral("Impossibile salvare CALL3.TXT in %1: %2")
                                  .arg(QDir::toNativeSeparators(path), f.errorString()));
            cleanup();
            return;
        }

        qint64 const written = f.write(data);
        if (written != data.size()) {
            QString const reason = f.errorString();
            f.cancelWriting();
            m_call3TxtLastError = QStringLiteral("scrittura incompleta (%1/%2 byte). %3")
                                  .arg(written).arg(data.size()).arg(reason);
            decodiumBridgeLocalUpdateLog(QStringLiteral("downloadCall3Txt incomplete write: %1/%2 bytes")
                          .arg(written).arg(data.size()));
            emit errorMessage(QStringLiteral("Impossibile salvare CALL3.TXT: scrittura incompleta (%1/%2 byte). %3")
                                  .arg(written).arg(data.size()).arg(reason));
            cleanup();
            return;
        }

        if (!f.commit()) {
            m_call3TxtLastError = f.errorString();
            decodiumBridgeLocalUpdateLog("downloadCall3Txt commit failed: " + f.errorString());
            emit errorMessage(QStringLiteral("Impossibile salvare CALL3.TXT: %1").arg(f.errorString()));
            cleanup();
            return;
        }

        m_worldMapCall3Loaded = false;
        m_worldMapCall3Loading = false;
        m_worldMapCall3RecordCount = 0;
        loadWorldMapCall3Cache();
        emit localDataStateChanged();

        decodiumBridgeLocalUpdateLog(QStringLiteral("CALL3.TXT downloaded (%1 bytes) to %2")
                      .arg(data.size()).arg(path));
        emit statusMessage(QStringLiteral("CALL3.TXT aggiornato; caricamento mappa in background (%1 KB)")
                               .arg(data.size() / 1024));
        cleanup();
    });
}

void DecodiumBridge::reloadDxccLookupAsync(const std::function<void(bool, const QString&)>& completion)
{
    if (!m_dxccLookup) {
        if (completion)
            completion(false, {});
        return;
    }

    ++m_dxccLookupLoadSerial;
    m_dxccLookupLoading = true;
    m_dxccLookupLoadAttempted = true;
    const quint64 serial = m_dxccLookupLoadSerial;
    const QStringList candidates = ctyDatSearchPaths();
    QPointer<DecodiumBridge> self(this);
    auto* task = QRunnable::create([self, candidates, serial, completion]() {
        auto lookup = std::make_shared<DxccLookup>();
        QString loadedPath;
        for (const QString& path : candidates) {
            if (QFile::exists(path) && lookup->loadCtyDat(path)) {
                loadedPath = path;
                break;
            }
        }
        if (!self)
            return;

        QMetaObject::invokeMethod(self, [self, serial, lookup = std::move(lookup), loadedPath, completion]() mutable {
            if (!self || serial != self->m_dxccLookupLoadSerial)
                return;

            self->m_dxccLookupLoading = false;
            const bool loaded = !loadedPath.isEmpty() && lookup->isLoaded();
            if (loaded) {
                *self->m_dxccLookup = std::move(*lookup);
                if (self->m_callsignIntelligence)
                    self->m_callsignIntelligence->notifyDxccDataChanged();
                self->refreshDecodeListDxcc();
            }
            if (completion)
                completion(loaded, loadedPath);
        }, Qt::QueuedConnection);
    });
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task, -1);
}

