#pragma once

#include <QtGlobal>
#include <QVector>

namespace decodium::seq {

// Facts about one station waiting in the caller queue (AutoCQ / MAM). Every
// field is plain data so the policy can be tested without the bridge.
struct CallerScoreInput {
    int    snrDb {-10};          // SNR of the caller's last decode
    bool   snrKnown {false};     // false: the queue entry carried no SNR
    qint64 waitMs {0};           // time spent in the queue
    bool   newDxccEver {false};  // entity never worked on any band
    bool   newDxccBand {false};  // entity not worked on the current band
    bool   workedBand {false};   // this call already worked on the current band
    bool   workedEver {false};   // this call already worked on any band
    bool   pinned {false};       // chosen by the operator (click): always ahead of automatic callers
};

struct CallerScoreWeights {
    double snrMax        {30.0};   // points for a strong signal (SNR >= snrHigh)
    int    snrLow        {-24};    // at or below: 0 points
    int    snrHigh       {10};     // at or above: snrMax points
    int    snrUnknownDb  {-10};    // assumed when the entry has no SNR
    double waitMax       {20.0};   // points for waiting waitFullMs or more
    qint64 waitFullMs    {90000};
    double newDxccEver   {40.0};
    double newDxccBand   {25.0};   // only when not already new on any band
    double workedBand    {-35.0};
    double workedEver    {-12.0};  // only when not already worked on the band
    double pinned        {1000.0}; // operator's own pick: nothing else counts, queue order decides
};

inline double callerScore(const CallerScoreInput& in,
                          const CallerScoreWeights& w = CallerScoreWeights {})
{
    // A station the operator picked by hand is never outranked by an automatic caller, and
    // several picks keep the order in which they were made (equal score = queue order).
    if (in.pinned) return w.pinned;

    int const snr = in.snrKnown ? in.snrDb : w.snrUnknownDb;
    int const clamped = qBound(w.snrLow, snr, w.snrHigh);
    double score = w.snrMax * double(clamped - w.snrLow) / double(w.snrHigh - w.snrLow);

    qint64 const wait = qBound<qint64>(0, in.waitMs, w.waitFullMs);
    score += w.waitMax * double(wait) / double(w.waitFullMs);

    if (in.newDxccEver)      score += w.newDxccEver;
    else if (in.newDxccBand) score += w.newDxccBand;

    if (in.workedBand)       score += w.workedBand;
    else if (in.workedEver)  score += w.workedEver;

    return score;
}

// Index of the best candidate; the earliest one wins a tie, so with equal
// scores the queue keeps its arrival order. Returns -1 for an empty list.
inline int pickBestCaller(const QVector<CallerScoreInput>& candidates,
                          const CallerScoreWeights& w = CallerScoreWeights {})
{
    int best = -1;
    double bestScore = 0.0;
    for (int i = 0; i < candidates.size(); ++i) {
        double const s = callerScore(candidates.at(i), w);
        if (best < 0 || s > bestScore + 1e-9) {
            best = i;
            bestScore = s;
        }
    }
    return best;
}

// Stable ranking: indices ordered by descending score, ties keep queue order.
inline QVector<int> rankCallers(const QVector<CallerScoreInput>& candidates,
                                const CallerScoreWeights& w = CallerScoreWeights {})
{
    QVector<double> scores;
    scores.reserve(candidates.size());
    QVector<int> order;
    order.reserve(candidates.size());
    for (int i = 0; i < candidates.size(); ++i) {
        scores.append(callerScore(candidates.at(i), w));
        order.append(i);
    }
    // Insertion sort: the queue is capped at 20 entries and it must be stable.
    for (int i = 1; i < order.size(); ++i) {
        int const cur = order.at(i);
        int j = i - 1;
        while (j >= 0 && scores.at(order.at(j)) + 1e-9 < scores.at(cur)) {
            order[j + 1] = order.at(j);
            --j;
        }
        order[j + 1] = cur;
    }
    return order;
}

} // namespace decodium::seq
