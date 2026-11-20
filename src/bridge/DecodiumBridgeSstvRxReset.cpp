// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvRxRuntime.h"
using decodium::sstv::SstvRxControlSettings;
bool decodiumSstvRxSettingsFromMap(const QVariantMap&, SstvRxControlSettings, SstvRxControlSettings*, QString*);
void decodiumPersistSstvRxControls(const SstvRxControlSettings&);
#define sstvRxSettingsFromMap decodiumSstvRxSettingsFromMap
#define persistSstvRxControls decodiumPersistSstvRxControls
#endif

bool DecodiumBridge::resetSstvRx()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRequested || !m_sstvRxRuntime
        || sstvWavReplayActive()) {
        return false;
    }
    const bool reset = m_sstvRxRuntime->resetStream();
    if (reset) {
        refreshSstvProducerTaps();
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
    }
    return reset;
#else
    return false;
#endif
}

bool DecodiumBridge::abortSstvRxFrame()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRequested || !m_sstvRxRuntime
        || sstvWavReplayActive()) {
        return false;
    }
    const bool aborted = m_sstvRxRuntime->resetStream();
    if (aborted) {
        refreshSstvProducerTaps();
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
        emit statusMessage(tr(
            "Current SSTV frame aborted; receiver returned to search"));
    }
    return aborted;
#else
    return false;
#endif
}

void DecodiumBridge::resetSstvRxAfc()
{
#if DECODIUM_HAS_SSTV
    if (m_sstvRxRuntime) {
        m_sstvRxRuntime->resetRxAfc();
        emit sstvRxSnapshotChanged();
    }
#endif
}

void DecodiumBridge::resetSstvRxSlant()
{
#if DECODIUM_HAS_SSTV
    if (m_sstvRxRuntime) {
        m_sstvRxRuntime->resetRxSlant();
        emit sstvRxSnapshotChanged();
    }
#endif
}

bool DecodiumBridge::updateSstvRxControls(const QVariantMap& controls)
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvRxRuntime || m_sstvRedecodeActive) {
        return false;
    }
    SstvRxControlSettings parsed;
    QString error;
    if (!sstvRxSettingsFromMap(
            controls,
            m_sstvRxRuntime->rxControlSnapshot().settings,
            &parsed,
            &error)
        || !m_sstvRxRuntime->replaceRxControlSettings(parsed)) {
        emit errorMessage(error.isEmpty()
            ? tr("The SSTV RX controls were rejected") : error);
        return false;
    }

    persistSstvRxControls(parsed);
    return true;
#else
    Q_UNUSED(controls)
    return false;
#endif
}

