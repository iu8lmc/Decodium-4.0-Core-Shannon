#pragma once

#include <QStringList>
#include <QVector>

namespace decodium {
namespace mamtx {

// The MAM payload is short lived: it is valid only while the MAM sequencer is
// actively driving a transmission. Keeping this decision separate from the
// bridge makes it impossible for a manual re-arm to inherit an old composite.
inline bool multiStreamSequencerIsActive(bool multiStreamEnabled,
                                         bool modeSupported,
                                         bool txEnabled,
                                         bool autoCqOrMultiAnswer,
                                         bool bridgeAudioPathAvailable,
                                         bool manualTxHold)
{
    return multiStreamEnabled
        && modeSupported
        && txEnabled
        && autoCqOrMultiAnswer
        && bridgeAudioPathAvailable
        && !manualTxHold;
}

inline bool hasValidPayload(const QStringList& messages,
                            const QVector<int>& frequencies)
{
    return !messages.isEmpty() && messages.size() == frequencies.size();
}

inline bool multiStreamPayloadIsActive(bool sequencerActive,
                                       const QStringList& messages,
                                       const QVector<int>& frequencies)
{
    return sequencerActive && hasValidPayload(messages, frequencies);
}

inline bool clearPendingPayload(QStringList& messages, QVector<int>& frequencies)
{
    const bool hadPayload = !messages.isEmpty() || !frequencies.isEmpty();
    messages.clear();
    frequencies.clear();
    return hadPayload;
}

inline bool cacheMatchesPayloadKind(bool cachedMultiStream,
                                    bool requestedMultiStream)
{
    return cachedMultiStream == requestedMultiStream;
}

} // namespace mamtx
} // namespace decodium
