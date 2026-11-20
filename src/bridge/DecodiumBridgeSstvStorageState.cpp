// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvRxAudioJobController.h"
#include "src/sstv/integration/SstvWavReplayController.h"
#endif
#include <QThread>

bool DecodiumBridge::sstvRxAudioJobBusy() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxAudioJobController
        && m_sstvRxAudioJobController->busy();
#else
    return false;
#endif
}

QString DecodiumBridge::sstvRxAudioJobState() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxAudioJobController
        ? m_sstvRxAudioJobController->stateName()
        : QStringLiteral("Unavailable");
#else
    return QStringLiteral("Unavailable");
#endif
}

QString DecodiumBridge::sstvRxAudioJobError() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxAudioJobController
        ? m_sstvRxAudioJobController->lastError() : QString {};
#else
    return {};
#endif
}

QString DecodiumBridge::sstvRxRawAudioPath() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxRawAudioPath;
#else
    return {};
#endif
}

bool DecodiumBridge::sstvStorageReady() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvStorageReady;
#else
    return false;
#endif
}

bool DecodiumBridge::sstvRxAutoSaveEnabled() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxAutoSaveEnabled;
#else
    return false;
#endif
}

void DecodiumBridge::setSstvRxAutoSaveEnabled(bool enabled)
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (enabled && !m_sstvStorageReady) {
        setSstvRxSaveStatus(
            QStringLiteral("error"),
            tr("SSTV storage is not ready; automatic save remains disabled"));
        return;
    }
    if (m_sstvRxAutoSaveEnabled == enabled) {
        return;
    }
    m_sstvRxAutoSaveEnabled = enabled;
    setSetting(QStringLiteral("SSTV/RxAutoSaveEnabled"), enabled);
    emit sstvRxAutoSaveChanged();
    if (enabled) {
        maybeAutoSaveSstvRxImage();
    }
#else
    Q_UNUSED(enabled)
#endif
}

QString DecodiumBridge::sstvRxSaveState() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxSaveState;
#else
    return QStringLiteral("idle");
#endif
}

QString DecodiumBridge::sstvRxSaveError() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxSaveError;
#else
    return {};
#endif
}

bool DecodiumBridge::sstvWavReplayActive() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvWavReplayController
        && m_sstvWavReplayController->active();
#else
    return false;
#endif
}

QString DecodiumBridge::sstvWavReplayState() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvWavReplayController
        ? m_sstvWavReplayController->stateName()
        : QStringLiteral("Unavailable");
#else
    return QStringLiteral("Unavailable");
#endif
}

double DecodiumBridge::sstvWavReplayProgress() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvWavReplayController
        ? m_sstvWavReplayController->progress() : 0.0;
#else
    return 0.0;
#endif
}

QString DecodiumBridge::sstvWavReplayFileName() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvWavReplayController
        ? m_sstvWavReplayController->fileName() : QString {};
#else
    return {};
#endif
}

QString DecodiumBridge::sstvWavReplayError() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvWavReplayController
        ? m_sstvWavReplayController->lastError() : QString {};
#else
    return {};
#endif
}

