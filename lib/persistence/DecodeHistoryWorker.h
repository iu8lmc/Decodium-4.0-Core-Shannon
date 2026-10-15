// 1.0.238 (Phase 5.2 perf roadmap):
// DecodeHistoryWorker -- write-behind worker thread per persistere la decode
// history in SQLite senza bloccare il thread GUI.
//
// SCOPE:
//   - Vive su un QThread dedicato (moveToThread dal main).
//   - Possiede una QSqlDatabase named connection "decode_history_worker"
//     SEPARATA da QSqlDatabase::defaultConnection (usata dal main thread per
//     altri scopi -- regola Qt: una QSqlDatabase non e' thread-safe).
//   - Riceve QVariantMap di decode tramite slot enqueueDecode(), invocato
//     dal bridge via QMetaObject::invokeMethod(... QueuedConnection ...).
//   - Bufferizza fino a 200 entry in m_buffer; un QTimer interno (500 ms)
//     scarica il buffer in una singola transaction batch sul DB.
//   - Backpressure: oltre 200 pendenti, FIFO-drop della entry piu' vecchia
//     (le decode "vecchie" sono meno preziose della stabilita' UI). Log
//     warning ogni 100 drop cumulati.
//   - Prepared INSERT statement preparato una volta in init() e riusato per
//     ogni flush (bindValue + exec dentro la transaction).
//
// Concurrency model:
//   - m_buffer/m_dropCount toccati SOLO dal thread worker (post-moveToThread).
//   - enqueueDecode e' uno slot: l'invocazione cross-thread e' Queued (event
//     loop), quindi serializzata sul thread worker. Niente mutex necessario.
//   - NO Qt::DirectConnection cross-thread (lezione 1.0.236).
//
// Schema target (creato in main_qml.cpp 1.0.237 Phase 5.1):
//   decodes(id, ts_utc, band, freq_hz, mode, submode, callsign_dx,
//           callsign_de, grid, snr_db, dt_s, df_hz, message, confidence,
//           session_id)
//   sessions(id, started_utc, ended_utc, operator, station, decodium_ver)

#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <memory>

class QTimer;

class DecodeHistoryWorker : public QObject
{
    Q_OBJECT
public:
    explicit DecodeHistoryWorker(QString dbPath,
                                 qint64 sessionId,
                                 QObject* parent = nullptr);
    ~DecodeHistoryWorker() override;

    // Diagnostica: lettura non protetta da mutex, va invocata solo per debug.
    int  pendingCount()  const { return m_buffer.size(); }
    int  dropCount()     const { return m_dropCount;     }
    qint64 sessionId()   const { return m_sessionId;     }

    static constexpr int kMaxBuffer = 200;   // backpressure cap
    static constexpr int kFlushMs   = 500;   // batch flush cadence

public slots:
    // Chiamato da DecodiumBridge via QueuedConnection (cross-thread).
    // L'event loop del thread worker serializza le invocazioni.
    void enqueueDecode(QVariantMap entry);

    // Inizializza la connessione SQLite + prepared statement. Chiamato dopo
    // moveToThread() via QMetaObject::invokeMethod(... QueuedConnection ...)
    // o tramite QThread::started signal, in modo che il QSqlDatabase venga
    // creato/aperto sul thread corretto.
    void initialize();

    // Forza flush del buffer e chiude la connessione. Chiamato in shutdown
    // prima di quit() del thread.
    void shutdown();

signals:
    void decodesPersisted(int count, qint64 sessionId);

private slots:
    void flushBuffer();

private:
    QString          m_dbPath;
    qint64           m_sessionId   {-1};
    QString          m_connName    {QStringLiteral("decode_history_worker")};
    QSqlDatabase     m_db;
    QSqlQuery        m_insertStmt;
    QTimer*          m_flushTimer  {nullptr};
    QVector<QVariantMap> m_buffer;
    int              m_dropCount   {0};
    int              m_lastDropWarnAt {0};
    bool             m_initialized {false};
};
