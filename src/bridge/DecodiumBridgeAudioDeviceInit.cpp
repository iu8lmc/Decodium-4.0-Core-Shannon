#include "DecodiumBridge.h"
#include <QMediaDevices>
#include <QTimer>

void decodiumBridgeAudioDeviceInitLog(const QString&);
#define bridgeLog decodiumBridgeAudioDeviceInitLog

void DecodiumBridge::initialiseAudioDeviceNotifications()
{
    m_audioDeviceRefreshTimer = new QTimer(this);
    m_audioDeviceRefreshTimer->setSingleShot(true);
    connect(m_audioDeviceRefreshTimer, &QTimer::timeout, this, [this]() {
        bool const verboseLog = m_pendingAudioDeviceRefreshVerbose;
        m_pendingAudioDeviceRefreshVerbose = false;
        if (!verboseLog && (m_monitoring || m_transmitting || m_tuning)) {
            m_audioDeviceCacheDirty = true;
            bridgeLog(QStringLiteral("audio device refresh deferred: active radio path monitoring=%1 tx=%2 tune=%3")
                          .arg(m_monitoring ? 1 : 0)
                          .arg(m_transmitting ? 1 : 0)
                          .arg(m_tuning ? 1 : 0));
            m_audioDeviceRefreshTimer->start(30000);
            return;
        }
        refreshAudioDeviceCache(QStringLiteral("debounced device refresh"), verboseLog, true);
    });
    m_audioBufferReleaseTimer = new QTimer(this);
    m_audioBufferReleaseTimer->setSingleShot(true);
    connect(m_audioBufferReleaseTimer, &QTimer::timeout,
            this, &DecodiumBridge::releaseIdleAudioBuffers);
    auto queueAudioDeviceCacheRefresh = [this]() {
        m_audioDeviceCacheDirty = true;
        m_cachedTxOutputDeviceValid = false;
        scheduleAudioDeviceRefresh(750, false);
    };
    connect(m_mediaDevices, &QMediaDevices::audioInputsChanged,
            this, queueAudioDeviceCacheRefresh);
    connect(m_mediaDevices, &QMediaDevices::audioOutputsChanged,
            this, queueAudioDeviceCacheRefresh);

}
