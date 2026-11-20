#include "DecodiumBridge.h"
#include <QCoreApplication>
#include <QMediaDevices>
#include <QTimer>

void DecodiumBridge::scheduleAudioDeviceRefresh(int delayMs, bool verboseLog)
{
    if (m_shuttingDown || QCoreApplication::closingDown()) {
        return;
    }

    m_pendingAudioDeviceRefreshVerbose =
        m_pendingAudioDeviceRefreshVerbose || verboseLog;
    int const boundedDelayMs = qMax(0, delayMs);
    if (m_audioDeviceRefreshTimer) {
        m_audioDeviceRefreshTimer->start(boundedDelayMs);
        return;
    }

    QTimer::singleShot(boundedDelayMs, this, [this]() {
        bool const verbose = m_pendingAudioDeviceRefreshVerbose;
        m_pendingAudioDeviceRefreshVerbose = false;
        refreshAudioDeviceCache(QStringLiteral("single-shot device refresh"), verbose, true);
    });
}

QList<QAudioDevice> DecodiumBridge::cachedAudioInputs(const QString& reason,
                                                      bool refreshIfStale)
{
    if ((!m_audioDeviceCacheValid || m_audioDeviceCacheDirty) && refreshIfStale) {
        refreshAudioDeviceCache(reason, false, true);
    } else if (!m_audioDeviceCacheValid || m_audioDeviceCacheDirty) {
        scheduleAudioDeviceRefresh(750, false);
    }
    return m_cachedAudioInputs;
}

QList<QAudioDevice> DecodiumBridge::cachedAudioOutputs(const QString& reason,
                                                       bool refreshIfStale)
{
    if ((!m_audioDeviceCacheValid || m_audioDeviceCacheDirty) && refreshIfStale) {
        refreshAudioDeviceCache(reason, false, true);
    } else if (!m_audioDeviceCacheValid || m_audioDeviceCacheDirty) {
        scheduleAudioDeviceRefresh(750, false);
    }
    return m_cachedAudioOutputs;
}

QAudioDevice DecodiumBridge::cachedDefaultAudioInput(const QString& reason,
                                                     bool refreshIfStale)
{
    if ((!m_audioDeviceCacheValid || m_audioDeviceCacheDirty) && refreshIfStale) {
        refreshAudioDeviceCache(reason, false, true);
    } else if (!m_audioDeviceCacheValid || m_audioDeviceCacheDirty) {
        scheduleAudioDeviceRefresh(750, false);
    }
    if (!m_cachedDefaultAudioInput.description().trimmed().isEmpty()) {
        return m_cachedDefaultAudioInput;
    }
    return QMediaDevices::defaultAudioInput();
}

QAudioDevice DecodiumBridge::cachedDefaultAudioOutput(const QString& reason,
                                                      bool refreshIfStale)
{
    if ((!m_audioDeviceCacheValid || m_audioDeviceCacheDirty) && refreshIfStale) {
        refreshAudioDeviceCache(reason, false, true);
    } else if (!m_audioDeviceCacheValid || m_audioDeviceCacheDirty) {
        scheduleAudioDeviceRefresh(750, false);
    }
    if (!m_cachedDefaultAudioOutput.description().trimmed().isEmpty()) {
        return m_cachedDefaultAudioOutput;
    }
    return QMediaDevices::defaultAudioOutput();
}

void DecodiumBridge::enumerateAudioDevices()
{
    refreshAudioDeviceCache(QStringLiteral("enumerateAudioDevices"), true, true);
}

