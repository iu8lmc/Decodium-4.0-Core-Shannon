#pragma once

#include <QString>

namespace decodium {
inline bool autoCqPausePending(qint64 now, qint64 genericUntil, qint64 burstUntil)
{
    return now < qMax(genericUntil, burstUntil);
}

// Completion-only accounting. Replies, aborted playback and watchdog ticks
// must never consume the generic CQ budget.
inline bool completeAutoCq(bool pureCq, bool error, int maximum, int& completed)
{
    if (!pureCq || error) return false;
    ++completed;
    return maximum > 0 && completed >= maximum;
}

// Classify the scheduled operation, not the first word of its payload.
// TX6 can contain a special-event call such as TEST instead of CQ/QRZ.
inline bool isAutoCqCall(bool enabled, int txNumber, bool calling,
                         bool partnerActive, const QString& payload)
{
    return enabled && txNumber == 6 && calling && !partnerActive
        && !payload.trimmed().isEmpty();
}
}
