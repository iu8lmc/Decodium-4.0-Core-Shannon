#include "DecodiumBridge.h"
#include <utility>

QVariantMap DecodiumBridge::digiPileupStatus() const
{
    QVariantMap m;
    m.insert(QStringLiteral("mode"), m_mode);
    m.insert(QStringLiteral("elasticRunning"), foxElasticActive());
    m.insert(QStringLiteral("gapHz"), mamMinStreamGapHz());
    m.insert(QStringLiteral("lastCycleMs"), m_digiLastCycleMs);
    m.insert(QStringLiteral("lastStreams"), m_digiLastStreams);
    m.insert(QStringLiteral("lastDeferred"), m_digiLastDeferred);
    m.insert(QStringLiteral("activeSlots"), m_mamSlots.size());
    m.insert(QStringLiteral("queued"), m_callerQueue.size());
    return m;
}

// Distanza minima fra due stream MAM nello stesso periodo, per modo. Due
// portanti piu' vicine della larghezza del segnale si disturbano a vicenda in
// ricezione. Misurato il 23/10/2026 (banco tools/autoimprove/multistream_scan.py,
// 2 e 3 stream a pari ampiezza, rumore bianco, SNR per stream vicino alla
// soglia): FT2 (segnale largo ~167 Hz) si decodifica al 98-100% a 170 Hz, al
// 48-92% a 125 Hz, al 15-65% a 85 Hz, <10% a 40 Hz. FT4 (~83 Hz): a 3 dB sopra la
// soglia 100% da 85 Hz in su, 90-93% a 60 Hz, 17-67% a 40 Hz. FT8 resta a 50/60 Hz.
int DecodiumBridge::mamMinStreamGapHz() const
{
    if (m_mode == QStringLiteral("FT2")) return 170;
    if (m_mode == QStringLiteral("FT4")) return 85;
    return 50;
}

// Passo con cui si cercano frequenze libere: un po' sopra la distanza minima.
int DecodiumBridge::mamStreamStepHz() const
{
    if (m_mode == QStringLiteral("FT2")) return 180;
    if (m_mode == QStringLiteral("FT4")) return 90;
    return 60;
}

// 1.0.569+ - sceglie fino a `count` frequenze audio per i CQ paralleli. Parte
// dalla frequenza TX dell'operatore e sale a passi di 60 Hz (la spaziatura gia'
// usata dal de-dup degli slot: un segnale FT8 occupa 50 Hz), poi ripiega verso
// il basso. Salta tutto cio' che dista meno di 50 Hz da una frequenza gia' in
// uso, cosi' i CQ non si sovrappongono tra loro ne' ai QSO in corso.
QVector<int> DecodiumBridge::mamPlanCqFrequencies(int count, const QVector<int>& used) const
{
    QVector<int> out;
    if (count <= 0) {
        return out;
    }
    int const spacingHz = mamStreamStepHz();
    int const minGapHz = mamMinStreamGapHz();
    constexpr int maxSteps = 64;
    int const base = qBound(200, m_txFrequency > 0 ? m_txFrequency : m_rxFrequency, 4000);
    QVector<int> taken = used;
    auto tryPlace = [&](int f) {
        if (f < 200 || f > 4000) {
            return;
        }
        for (int t : std::as_const(taken)) {
            if (qAbs(t - f) < minGapHz) {
                return;
            }
        }
        taken.append(f);
        out.append(f);
    };
    for (int step = 0; out.size() < count && step <= maxSteps; ++step) {
        tryPlace(base + step * spacingHz);
    }
    for (int step = 1; out.size() < count && step <= maxSteps; ++step) {
        tryPlace(base - step * spacingHz);
    }
    return out;
}

// FASE 3 - snapshot read-only degli slot QSO multi-stream attivi per la UI.
// Un QVariantMap per slot; NON muta stato. Emesso via mamActiveSlotsChanged()
// dai punti che modificano m_mamSlots.
