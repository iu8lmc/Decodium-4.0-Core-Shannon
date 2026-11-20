#include "DecodiumBridge.h"
#include "lib/persistence/DecodeHistoryWorker.h"
#include "revision_utils.hpp"
#include "wsjtx_config.h"

#include <QDateTime>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>

void decodiumBridgeDecodePersistenceLog(const QString&);
bool decodiumBridgeIsStreamingKeyboardMode(QString const&);
QString decodiumBridgeMirrorEntryKey(const QVariantMap&);
bool decodiumBridgeUnresolvedGridOnlyDecode(const QString&, QString*);
bool decodiumBridgeFst4wUnresolvedHashDecode(QString const&, QString const&);

void DecodiumBridge::appendDecodeMapToList(QVariantMap const& entry)
{
#if DECODIUM_HAS_SSTV
    // Last-resort boundary for every decode producer. SSTV keeps the visual
    // PCM path alive, but no dashboard-mode result may enter history/UI while
    // its exclusive workspace is open.
    if (m_sstvWorkspaceActive) {
        return;
    }
#endif
    // A worker callback queued immediately before entering RTTY must not add
    // an FT8/FT4/JT line after the RTTY window is visible.  RTTY's own modem
    // explicitly tags its rows as RTTY, so its history remains intact.
    if (decodiumBridgeIsStreamingKeyboardMode(m_mode)
        && entry.value(QStringLiteral("mode")).toString().trimmed()
               .compare(m_mode.trimmed(), Qt::CaseInsensitive) != 0) {
        return;
    }
    // Telemetria stazione+meteo (tipo FT8/FT4 0.5, opt-in): il testo
    // decodificato di un messaggio tipo-5 e' gia' oggi una stringa esadecimale
    // a 18 caratteri (unpack77_cpp). Se porta la nostra firma, avvisa la UI
    // con i campi interpretati — la voce grezza resta comunque in lista.
    if (getSetting(QStringLiteral("ShowTelemetryPopup"), false).toBool()) {
        QString const msgText = entry.value(QStringLiteral("message")).toString().trimmed().toUpper();
        decodium::telemetry::StationTelemetryFields telemetryFields;
        if (msgText.size() == 18
            && decodium::telemetry::decodeStationTelemetryHex(msgText, telemetryFields)) {
            qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
            bool const correlated = !m_lastLoggedQsoCall.isEmpty()
                && (nowMs - m_lastLoggedQsoTimestampMs) < 60000;
            QVariantMap fields;
            fields[QStringLiteral("grid4")] = telemetryFields.grid4;
            fields[QStringLiteral("tempC")] = telemetryFields.tempC == decodium::telemetry::kTempUnknown
                ? QVariant() : QVariant(telemetryFields.tempC);
            fields[QStringLiteral("windSpeedKmh")] = telemetryFields.windSpeedKmh;
            fields[QStringLiteral("windDirLabel")] = decodium::telemetry::windDirIndex16ToLabel(
                telemetryFields.windDirDeg16);
            fields[QStringLiteral("sky")] = decodium::telemetry::stationSkyConditionNames().value(
                static_cast<int>(telemetryFields.sky), QStringLiteral("?"));
            fields[QStringLiteral("powerWatts")] = telemetryFields.powerWatts;
            fields[QStringLiteral("radioModel")] = decodium::telemetry::stationRadioModelNames().value(
                telemetryFields.radioModelId, QStringLiteral("?"));
            fields[QStringLiteral("antennaType")] = decodium::telemetry::stationAntennaTypeNames().value(
                telemetryFields.antennaTypeId, QStringLiteral("?"));
            fields[QStringLiteral("likelyCall")] = correlated ? m_lastLoggedQsoCall : QString();
            emit stationTelemetryDecoded(fields);
        }
    }

    m_decodeList.append(QVariant(entry));
    rememberDecodeDedupEntry(entry);
    trimDecodeListsIfNeeded();
    noteDecodeCommitted();  // 1.0.233 DevOverlay counter (no-op se overlay off)
    // 1.0.238 (Phase 5.2 perf roadmap): write-behind SQLite persistence.
    // Spedisce la entry al worker thread tramite slot QueuedConnection: il
    // costo lato GUI thread e' solo l'allocazione di un QMetaCallEvent
    // (alcune centinaia di ns). NO blocco I/O.
    persistDecodeHistoryEntry(entry);
}

bool DecodiumBridge::persistDecodeHistoryEntry(QVariantMap const& entry)
{
    if (!m_persistenceWorker) {
        return false;
    }
    if (entry.value(QStringLiteral("isTx")).toBool()) {
        return false;
    }
    QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
    if (message.isEmpty()) {
        return false;
    }

    QString const mode = entry.value(QStringLiteral("mode"), m_mode).toString().trimmed().toUpper();
    QString const mirrorKey = decodiumBridgeMirrorEntryKey(entry);
    if (!mirrorKey.isEmpty()) {
        QString const persistKey =
            QString::number(m_currentSessionId)
            + QLatin1Char('|')
            + mode
            + QLatin1Char('|')
            + QString::number(static_cast<qint64>(m_frequency))
            + QLatin1Char('|')
            + mirrorKey;
        if (m_decodeHistoryPersistedKeys.contains(persistKey)) {
            return false;
        }
        if (m_decodeHistoryPersistedKeys.size() > 50000) {
            m_decodeHistoryPersistedKeys.clear();
        }
        m_decodeHistoryPersistedKeys.insert(persistKey);
    }

    enqueuePersistDecode(entry);
    return true;
}

// 1.0.238 (Phase 5.2 perf roadmap): bootstrap del write-behind worker per la
// decode history. Sequenza:
//   1. Apre la connessione SQLite default (gia' creata in main_qml.cpp:
//      ensureLegacySqliteDatabase) e inserisce una nuova riga in `sessions`.
//   2. Memorizza il rowid in m_currentSessionId.
//   3. Crea il QThread + DecodeHistoryWorker, moveToThread() e starta.
//   4. Connette QThread::started -> worker::initialize (init SQLite sul
//      thread giusto), QThread::finished -> worker::deleteLater.
// Chiamato dal costruttore del bridge subito dopo che gli altri member sono
// stati inizializzati.
void DecodiumBridge::startPersistenceWorker()
{
    if (m_persistenceWorker || m_persistenceThread) return;

    QString const dbPath = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation))
        .absoluteFilePath(QStringLiteral("db.sqlite"));

    // 1. INSERT in sessions usando la connessione default (main thread).
    {
        QSqlDatabase db = QSqlDatabase::database(QSqlDatabase::defaultConnection);
        if (!db.isValid() || !db.isOpen()) {
            decodiumBridgeDecodePersistenceLog(QStringLiteral("[Phase5.2] sessions: default DB connection "
                                     "unavailable, persistence disabled"));
            return;
        }
        // 1.0.270 (Phase 5.3+) — Retention: cancella decode > 30 giorni PRIMA di
        // creare la nuova sessione. Cosi' la nuova sessione parte sempre su un DB
        // potato. Eseguito sulla connessione default (sincrono), prima del worker.
        pruneDecodeHistoryOlderThan(30);

        QSqlQuery q(db);
        // 1.0.268 (Phase 5.3) — popola anche operator + station, prima erano NULL.
        if (!q.prepare(QStringLiteral(
                "INSERT INTO sessions (started_utc, decodium_ver, operator, station) "
                "VALUES (:ts, :ver, :op, :sta)"))) {
            decodiumBridgeDecodePersistenceLog(QStringLiteral("[Phase5.2] sessions INSERT prepare failed: %1")
                          .arg(q.lastError().text()));
            return;
        }
        q.bindValue(QStringLiteral(":ts"),
                    QDateTime::currentMSecsSinceEpoch());
        q.bindValue(QStringLiteral(":ver"),
                    QString::fromLatin1(FORK_RELEASE_VERSION));
        q.bindValue(QStringLiteral(":op"),
                    m_callsign.trimmed().toUpper());
        q.bindValue(QStringLiteral(":sta"),
                    QSysInfo::machineHostName());
        if (!q.exec()) {
            decodiumBridgeDecodePersistenceLog(QStringLiteral("[Phase5.2] sessions INSERT failed: %1")
                          .arg(q.lastError().text()));
            return;
        }
        m_currentSessionId = q.lastInsertId().toLongLong();
    }

    // 2/3. Spin-up del worker thread.
    m_persistenceThread = new QThread(this);
    m_persistenceThread->setObjectName(QStringLiteral("DecodeHistoryWorker"));
    m_persistenceWorker = new DecodeHistoryWorker(dbPath, m_currentSessionId);
    m_persistenceWorker->moveToThread(m_persistenceThread);

    connect(m_persistenceThread, &QThread::started,
            m_persistenceWorker, &DecodeHistoryWorker::initialize);
    connect(m_persistenceThread, &QThread::finished,
            m_persistenceWorker, &QObject::deleteLater);
    connect(m_persistenceWorker, &DecodeHistoryWorker::decodesPersisted,
            this, [](int count, qint64 sessionId) {
                // 1.0.238: log su counter mod 200 per evitare flood.
                static int s_total = 0;
                s_total += count;
                if ((s_total / 200) != ((s_total - count) / 200)) {
                    decodiumBridgeDecodePersistenceLog(QStringLiteral("[Phase5.2] persisted %1 decodes "
                                             "(session=%2)").arg(s_total)
                                  .arg(sessionId));
                }
            });

    m_persistenceThread->start();
    decodiumBridgeDecodePersistenceLog(QStringLiteral("[Phase5.2] decode history worker started, "
                             "session_id=%1").arg(m_currentSessionId));
}

void DecodiumBridge::enqueuePersistDecode(QVariantMap const& entry)
{
    if (!m_persistenceWorker) return;
    // 1.0.242: arricchisce entry con dialFreqHz + bandLabel correnti del
    // bridge. entry["freq"] da decoder e' audio offset Hz, non RF assoluta;
    // il worker calcola RF = dialFreqHz + audioOffsetHz. bandLabel viene da
    // BandManager (es "20m", "FT8 14.074") quando disponibile.
    QVariantMap enriched = entry;
    QString const message = enriched.value(QStringLiteral("message")).toString().trimmed();
    QString const mode = enriched.value(QStringLiteral("mode"), m_mode).toString().trimmed();
    if (enriched.value(QStringLiteral("partialDecode")).toBool()
        || enriched.value(QStringLiteral("unresolvedHash")).toBool()
        || enriched.value(QStringLiteral("hasUnresolvedGridOnly")).toBool()
        || decodiumBridgeUnresolvedGridOnlyDecode(message, nullptr)
        || decodiumBridgeFst4wUnresolvedHashDecode(mode, message)) {
        enriched.insert(QStringLiteral("partialDecode"), true);
        enriched.insert(QStringLiteral("unresolvedHash"), true);
        enriched.insert(QStringLiteral("fromCall"), QString());
        enriched.insert(QStringLiteral("dxCallsign"), QString());
        enriched.insert(QStringLiteral("dxCountry"), QString());
        enriched.insert(QStringLiteral("dxGrid"), QString());
        enriched.insert(QStringLiteral("dxDistance"), -1.0);
        enriched.insert(QStringLiteral("dxBearing"), -1.0);
    }
    enriched.insert(QStringLiteral("dialFreqHz"),
                    static_cast<qint64>(m_frequency));
    if (m_bandManager) {
        QString const band = m_bandManager->currentBandLambda().trimmed();
        if (!band.isEmpty()) {
            enriched.insert(QStringLiteral("bandLabel"), band);
        }
    }
    // QueuedConnection: l'event va in coda sul thread del worker. Cost lato
    // GUI = alloc QMetaCallEvent + push in QObjectPrivate::postEvent (~µs).
    QMetaObject::invokeMethod(m_persistenceWorker, "enqueueDecode",
                              Qt::QueuedConnection,
                              Q_ARG(QVariantMap, enriched));
}

void DecodiumBridge::stopPersistenceWorker()
{
    if (!m_persistenceThread) return;
    // 1.0.268 (Phase 5.3) — chiude la sessione corrente con ended_utc.
    // Eseguito prima dello shutdown del worker per stare sicuri che il flush
    // del buffer abbia terminato (worker usa connessione separata).
    if (m_currentSessionId > 0) {
        QSqlDatabase db = QSqlDatabase::database(QSqlDatabase::defaultConnection);
        if (db.isValid() && db.isOpen()) {
            QSqlQuery q(db);
            if (q.prepare(QStringLiteral(
                    "UPDATE sessions SET ended_utc = :ts WHERE id = :id"))) {
                q.bindValue(QStringLiteral(":ts"),
                            QDateTime::currentMSecsSinceEpoch());
                q.bindValue(QStringLiteral(":id"), m_currentSessionId);
                if (!q.exec()) {
                    decodiumBridgeDecodePersistenceLog(QStringLiteral("[Phase5.3] sessions UPDATE ended_utc failed: %1")
                                  .arg(q.lastError().text()));
                }
            }
        }
    }
    if (m_persistenceWorker) {
        // Forza flush + chiusura DB sul thread worker via Queued: l'event
        // loop processa il metodo prima di tornare a quit().
        QMetaObject::invokeMethod(m_persistenceWorker, "shutdown",
                                  Qt::BlockingQueuedConnection);
    }
    m_persistenceThread->quit();
    if (!m_persistenceThread->wait(5000)) {
        qWarning("~DecodiumBridge: persistence thread did not finish in 5s, "
                 "terminating");
        m_persistenceThread->terminate();
        m_persistenceThread->wait(2000);
    }
    // worker deletato via deleteLater on QThread::finished signal; thread
    // ha this come parent quindi distrutto col bridge.
    m_persistenceWorker = nullptr;
}

