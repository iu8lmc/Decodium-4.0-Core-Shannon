#include "DecodiumBridge.h"

extern void decodiumBridgeLogbookLog(const QString&);

bool DecodiumBridge::worldMapFeedEnabled() const
{
    return m_worldMapDisplayed;
}

bool DecodiumBridge::worldMapConsumerReady() const
{
    return !m_worldMapReadyConsumers.isEmpty();
}

void DecodiumBridge::setWorldMapConsumerReady(QObject* consumer, bool ready)
{
    if (!consumer) {
        return;
    }

    if (!ready) {
        if (m_worldMapReadyConsumers.remove(consumer) > 0
            && m_worldMapReadyConsumers.isEmpty()
            && worldMapFeedEnabled()) {
            // Contacts emitted while no QML item exists are intentionally
            // replayed once a docked/detached map becomes available again.
            m_worldMapFullReplayDeferred = true;
        }
        return;
    }

    if (m_worldMapReadyConsumers.contains(consumer)) {
        return;
    }
    m_worldMapReadyConsumers.insert(consumer);

    connect(consumer, &QObject::destroyed, this, [this](QObject* destroyed) {
        if (m_worldMapReadyConsumers.remove(destroyed) > 0
            && m_worldMapReadyConsumers.isEmpty()
            && worldMapFeedEnabled()) {
            m_worldMapFullReplayDeferred = true;
        }
    });

    if (worldMapFeedEnabled()) {
        m_worldMapFullReplayDeferred = true;
        decodiumBridgeLogbookLog(QStringLiteral("LiveMap: consumer ready; scheduling authoritative replay"));
        scheduleDeferredWorldMapFeedFlush(0);
    }
}

