// HashedCallsignCache.h — 1.0.293 (fork iu8lmc), AP hashed-callsign cache Fase 0
//
// Cache band-wide degli hash28 dei callsign visti di recente in banda, per AP
// decoding FT2 (roadmap §1.1, obiettivo −3 dB sul floor — Fase 2). In Fase 0
// serve solo a misurare l'hit-rate (osservabilità), nessun effetto sul decode.
//
// THREAD-SAFETY: questa struttura è SINGLE-THREAD-CONFINED. È un membro di
// DecodiumBridge e viene toccata SOLO dal GUI thread (seed in onFt2AsyncDecodeReady,
// snapshot in onAsyncDecodeTimer). Il worker FT2 riceve solo una COPIA by-value
// (QVector<quint32>) dentro la AsyncDecodeRequest. Per questo NON serve alcun lock
// (la roadmap proponeva QReadWriteLock: inutile e dannoso qui).
#pragma once

#include <array>
#include <QVector>
#include <QtGlobal>

class HashedCallsignCache
{
public:
    static constexpr int    RING_SIZE = 1024;
    static constexpr qint64 TTL_MS    = 30LL * 60 * 1000;  // 30 minuti

    // Inserisce/aggiorna una call. Idempotente: rivedere la stessa call ne
    // rinfresca il timestamp (TTL refresh) — una call attiva resta "calda".
    void add(quint32 hash28, qint64 utcMs) noexcept
    {
        for (auto& e : m_ring) {
            if (e.utcMs > 0 && e.hash28 == hash28) { e.utcMs = utcMs; return; }
        }
        m_ring[m_writeIdx] = Entry{hash28, utcMs};
        m_writeIdx = (m_writeIdx + 1) % RING_SIZE;
    }

    // Vero se hash28 è in cache e non scaduto (TTL).
    bool containsHash28(quint32 hash28, qint64 nowMs) const noexcept
    {
        for (const auto& e : m_ring) {
            if (e.utcMs > 0 && e.hash28 == hash28 && (nowMs - e.utcMs) <= TTL_MS)
                return true;
        }
        return false;
    }

    // Snapshot dei soli hash non scaduti (per passaggio by-value al worker).
    QVector<quint32> snapshotValid(qint64 nowMs) const
    {
        QVector<quint32> out;
        for (const auto& e : m_ring) {
            if (e.utcMs > 0 && (nowMs - e.utcMs) <= TTL_MS)
                out.append(e.hash28);
        }
        return out;
    }

    int validCount(qint64 nowMs) const noexcept
    {
        int n = 0;
        for (const auto& e : m_ring)
            if (e.utcMs > 0 && (nowMs - e.utcMs) <= TTL_MS) ++n;
        return n;
    }

private:
    struct Entry { quint32 hash28 {0}; qint64 utcMs {0}; };
    std::array<Entry, RING_SIZE> m_ring {};
    int m_writeIdx {0};
};
