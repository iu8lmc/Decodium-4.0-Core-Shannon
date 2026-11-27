#include "DecodiumBridge.h"
#include "DecodiumTransceiverManager.h"
#include "DecodiumOmniRigManager.h"
#include "DecodiumCat4OmManager.h"
#include <QThreadPool>

QThreadPool& decodiumBridgeSettingsWriteThreadPool();
bool decodiumBridgeIsHamlibFamilyBackend(QString const&);
#define settingsWriteThreadPool decodiumBridgeSettingsWriteThreadPool
#define isHamlibFamilyBackend decodiumBridgeIsHamlibFamilyBackend

void DecodiumBridge::shutdown()
{
    m_settingsShutdownInProgress = true;
    beginDecodeCallbackShutdown();
    halt();
    // The generic halt path routes an active native SSTV transmission through
    // its coordinator.  Complete its in-process fail-safe teardown, including
    // one final PTT-OFF dispatch, before any CAT manager is disconnected.
    shutdownSstvTx();
    stopRx();
    teardownAudioCapture();
    if (m_catBackend == "cat4om" && m_cat4OmCat->connected())
        m_cat4OmCat->disconnectRig();
    else if (m_catBackend == "omnirig" && m_omniRigCat->connected())
        m_omniRigCat->disconnectRig();
    else if (isHamlibFamilyBackend(m_catBackend) && m_hamlibCat->connected())
        m_hamlibCat->disconnectRig();
    // Window/layout snapshots are flushed by the serial settings writer.  The
    // close path waits for the final queued snapshot before saveSettings()
    // writes the rest of the application state.
    settingsWriteThreadPool().waitForDone(5000);
    // No queued completion is guaranteed once the GUI event loop exits.
    // Also persist CAT/DX-cluster managers, skipped by the asynchronous path.
    saveSettings();
}
