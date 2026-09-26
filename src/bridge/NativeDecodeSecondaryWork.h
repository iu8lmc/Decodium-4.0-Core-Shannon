#pragma once

#include <QString>
#include <QVariantMap>

namespace decodium {

// Work for an already validated, deduplicated native decode. Presentation
// filters control the displays, not whether a received call can raise an alert.
struct NativeDecodeSecondaryWork {
    QVariantMap entry;
    QString rawRow;
    quint64 serial {0};
    QString key;
    bool publishPsk {false};
    bool updateActiveStation {false};
    bool updateWorldMap {false};
    bool sendUdp {false};
    bool playAlert {false};
    bool reportDecodeTiming {false};

    void route(bool visible, bool deepInTxListOnly)
    {
        bool const resolved = !entry.value(QStringLiteral("hasUnresolvedPeer")).toBool();
        publishPsk = !deepInTxListOnly && resolved;
        updateActiveStation = visible && publishPsk && entry.value(QStringLiteral("isCQ")).toBool();
        updateWorldMap = visible;
        sendUdp = visible && !deepInTxListOnly;
        playAlert = !deepInTxListOnly;
        reportDecodeTiming = visible;
    }

    bool hasWork() const
    {
        return publishPsk || updateActiveStation || updateWorldMap
            || sendUdp || playAlert || reportDecodeTiming;
    }
};

} // namespace decodium
