// 1.0.234 (Sprint 3 / Phase 2 performance roadmap):
// StreamingListModel<T> -- template base class per stream high-frequency
// di entries che alimentano ListView QML.
//
// Use cases pianificati (Phase 6 migration):
//   - Decode list (FT8/FT2 burst end-of-cycle, 15-20 entries)
//   - DX cluster spots (reconnect storm, 100+ entries/s)
//   - CAT/Hamlib trace (debug overlay)
//
// Caratteristiche:
//   - Coalescing flush via QTimer (default 100ms): un singolo segnale
//     rowsInserted per batch, anche se enqueue() viene chiamato molte
//     volte in rapida successione (esempio: end of T/R cycle).
//   - Thread-safe enqueue: protetto da QMutex, puo' essere chiamato da
//     worker thread (decoder, network, audio callback).
//   - FIFO row cap: setMaxRows(n) > 0 fa droppare le entries piu' vecchie
//     in modo che il modello non cresca indefinitamente (vedi regressione
//     decode list cap 1.0.155 / 1.0.206 in MEMORY.md).
//   - forceFlush() sincrono dal thread GUI per smaltire la coda subito,
//     utile a fine ciclo T/R o prima di operazioni che leggono il count.
//
// Concurrency model:
//   - m_pending protetto da QMutex (enqueue da qualsiasi thread).
//   - m_rows toccato SOLO dal thread che possiede il modello (GUI thread
//     per i model esposti a QML), via flushPending() invocato dal timer
//     che vive sul thread del modello (parent della classe).
//   - flushPending fa swap-and-clear di m_pending sotto lock, poi rilascia
//     il lock prima di chiamare beginInsertRows() per minimizzare la
//     finestra di contesa.
//
// Sottoclassi:
//   - DEVONO override roleNames() + data(index, role).
//   - Possono usare entryAt(row) (protected) per leggere la entry alla riga.
//
// Stato: classe pronta per uso, ma NON ancora integrata. La migrazione di
// m_decodeList in DecodiumBridge resta pendente -- il bridge attuale
// (QVariantList m_decodeList + emit throttle 500ms + shift-diff) funziona
// bene per uso singolo-utente e la migrazione tocca 5 ListView QML +
// DecodeWindow.qml panel: out of scope per Sprint 3. Vedi roadmap Phase 6.
#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QTimer>
#include <QVariant>
#include <QVector>
#include <utility>

template <typename Entry>
class StreamingListModel : public QAbstractListModel
{
public:
    explicit StreamingListModel (QObject *parent = nullptr)
        : QAbstractListModel (parent)
        , m_flushTimer (new QTimer (this))
    {
        m_flushTimer->setInterval (m_flushIntervalMs);
        m_flushTimer->setSingleShot (false);
        // Lambda evita la necessita' di Q_OBJECT / moc nel template:
        // QObject::connect con functor target-aware funziona con QTimer
        // anche per template class che non puo' avere slot.
        QObject::connect (m_flushTimer, &QTimer::timeout,
                          this, [this] () { flushPending (); });
        m_flushTimer->start ();
    }

    // Thread-safe enqueue. Non emette segnali sul thread chiamante:
    // il flush successivo (timer o forceFlush) si occupa di beginInsertRows.
    void enqueue (Entry e)
    {
        QMutexLocker lock (&m_mutex);
        m_pending.append (std::move (e));
    }

    // Force flush sincrono. Deve essere chiamato dal thread che possiede
    // il modello (GUI thread per i model esposti a QML).
    Q_INVOKABLE void forceFlush ()
    {
        flushPending ();
    }

    void setMaxRows (int n) { m_maxRows = n > 0 ? n : 0; }
    int maxRows () const { return m_maxRows; }

    void setFlushIntervalMs (int ms)
    {
        m_flushIntervalMs = ms > 0 ? ms : 100;
        m_flushTimer->setInterval (m_flushIntervalMs);
    }
    int flushIntervalMs () const { return m_flushIntervalMs; }

    int rowCount (const QModelIndex &parent = QModelIndex ()) const override
    {
        if (parent.isValid ()) return 0;
        return m_rows.size ();
    }

    // Sottoclassi MUST override:
    QVariant data (const QModelIndex &index, int role) const override = 0;
    QHash<int, QByteArray> roleNames () const override = 0;

    // Numero di entries in attesa di flush (utile per i test / debug).
    int pendingCount () const
    {
        QMutexLocker lock (&m_mutex);
        return m_pending.size ();
    }

protected:
    const Entry &entryAt (int row) const { return m_rows.at (row); }

private:
    void flushPending ()
    {
        QVector<Entry> local;
        {
            QMutexLocker lock (&m_mutex);
            if (m_pending.isEmpty ()) return;
            local.swap (m_pending);
        }
        const int first = m_rows.size ();
        const int last  = first + local.size () - 1;
        beginInsertRows (QModelIndex (), first, last);
        m_rows.append (local);
        endInsertRows ();
        capRows ();
    }

    void capRows ()
    {
        if (m_maxRows <= 0 || m_rows.size () <= m_maxRows) return;
        const int toRemove = m_rows.size () - m_maxRows;
        beginRemoveRows (QModelIndex (), 0, toRemove - 1);
        m_rows.remove (0, toRemove);
        endRemoveRows ();
    }

    mutable QMutex m_mutex;
    QVector<Entry> m_pending;
    QVector<Entry> m_rows;
    QTimer *m_flushTimer = nullptr;
    int m_flushIntervalMs = 100;
    int m_maxRows = 0; // 0 = unlimited
};
