// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#if DECODIUM_HAS_SSTV
#include "Audio/soundout.h"
#include "Modulator/Modulator.hpp"
#include "src/sstv/integration/SstvStudioController.h"
#include "src/sstv/integration/SstvTxCoordinator.h"
#endif
#include <QCoreApplication>
#include <cmath>

bool DecodiumBridge::sstvTxCanStart() const
{
    return sstvTxUnavailableReason().isEmpty();
}

QString DecodiumBridge::sstvTxUnavailableReason() const
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvStudioController) {
        return tr("SSTV Transmit Studio is not available");
    }
    if (m_sstvStudioController->busy()) {
        return tr("Wait for the current image operation to finish");
    }
    if (!m_sstvStudioController->preparedReady()) {
        return m_sstvStudioController->sourceReady()
            ? tr("Press Preview to prepare the image for transmission")
            : tr("Load an image, then press Preview");
    }
    const QStringList blockers = sstvTxPreflightBlockers();
    return blockers.isEmpty() ? QString()
        : tr("SSTV TX is not ready: %1").arg(blockers.join(QStringLiteral("; ")));
#else
    return tr("SSTV support is not available in this build");
#endif
}

QString DecodiumBridge::sstvTxPreflightRejection() const
{
    QStringList const blockers = sstvTxPreflightBlockers();
    if (blockers.isEmpty()) {
        // Il preflight e' passato fra il controllo e questo messaggio: raro,
        // ma dire "non pronto: nulla" sarebbe peggio che dirlo genericamente.
        return tr("SSTV TX preflight is not ready; stop any other TX and verify audio/CAT or VOX");
    }
    return tr("SSTV TX preflight is not ready: %1")
        .arg(blockers.join(QStringLiteral("; ")));
}

// 1.0.588: il preflight ha venticinque condizioni e ne riportava una sola,
// sempre la stessa: "non pronto, verifica audio/CAT o VOX". Chi la leggeva non
// poteva sapere quale delle venticinque fosse, e le piu' comuni - il TX di un
// altro modo ancora armato, il sink audio non ancora rilasciato - con audio,
// CAT e VOX non c'entrano nulla. Ora le condizioni stanno scritte una volta
// sola, ognuna col suo nome, e di qui vengono sia la risposta booleana sia il
// messaggio all'operatore.
QStringList DecodiumBridge::sstvTxPreflightBlockers() const
{
    QStringList blockers;
#if DECODIUM_HAS_SSTV
    auto const blocks = [&blockers](bool condition, char const* label) {
        if (condition) {
            blockers << QString::fromLatin1(label);
        }
    };

    blocks(!m_sstvTxCoordinator, "SSTV TX coordinator missing");
    blocks(!m_soundOutput, "audio output not initialised");
    blocks(m_sstvTxShuttingDown || m_shuttingDown
           || QCoreApplication::closingDown(), "application is shutting down");
    blocks(m_sstvOwnsBridgeTx, "an SSTV transmission already owns the TX path");
    blocks(m_transmitting || m_ssbTxActive, "another mode is transmitting");
    blocks(m_tuning, "the tune tone is active");
    blocks(m_txEnabled, "Enable TX is armed for another mode");
    blocks(m_autoCqRepeat, "Auto CQ is running");
    blocks(m_deferredManualSyncTx || m_txRequested,
           "a transmission is already requested");
    blocks(m_pttPending || m_pttConfirmed, "PTT is already engaged");
    blocks(m_bridgeAudioLegacyTxActive, "the legacy TX audio path is active");
    blocks(m_ft2LinkTxActive, "FT2-Link is transmitting");
    blocks(m_cwTxActive, "CW transmission is active");
    blocks(m_manualTxHold, "manual TX hold is engaged");
    blocks(m_bridgeAudioTuneActive || m_legacyBridgeAudioTxStartPending,
           "the TX audio path is starting up");
    blocks(m_txPlaybackReleasePending || m_txAudioRestartPending
           || m_txPlaybackHoldUntilMs != 0 || m_txPlaybackHardDeadlineMs != 0,
           "the previous transmission has not released the audio device yet");
    blocks(m_txAudioSink || m_txPcmBuffer,
           "the TX audio sink is still allocated");
    blocks(m_modulator && m_modulator->isActive(), "the modulator is running");
    blocks(!sstvTxUsesVoxPtt() && sstvTxPttActive(),
           "the radio is already keyed");
    blocks(m_decoPortUseRemote, "DecoPort is driving a remote radio");
    blocks(usingTciAudioInput(), "audio is coming from TCI");

    blocks(m_sstvTxOutputPinned
           && (m_sstvTxOutputDevice.isNull()
               || (m_sstvTxOutputChannels != 1U
                   && m_sstvTxOutputChannels != 2U)),
           "the pinned SSTV output device is no longer valid");

    if (m_sstvTxCoordinator) {
        const decodium::sstv::SstvTxCoordinatorSnapshot snapshot
            = m_sstvTxCoordinator->snapshot();
        blocks(snapshot.stateMachine.state
                   == decodium::sstv::SstvTxState::Disabled,
               "the SSTV TX state machine is disabled");
        blocks(snapshot.stateMachine.releaseRequired,
               "the previous SSTV transmission still needs releasing");
        blocks(snapshot.audioLeaseRetained,
               "the SSTV audio lease is still held");
    }

    blocks(!sstvTxUsesVoxPtt() && !sstvTxCanControlPtt()
               && !sstvTxAudioOnlyAllowed(),
           "no way to key the radio: no CAT PTT, no VOX");
#endif
    return blockers;
}

bool DecodiumBridge::sstvTxGlobalPreflightReady() const
{
#if DECODIUM_HAS_SSTV
    if (!sstvTxPreflightBlockers().isEmpty()) {
        return false;
    }
    // La cache dei dispositivi audio non entra nel preflight della UI: un
    // cambio di dispositivo la invalida di proposito, e il percorso di avvio
    // vero risolve l'uscita subito prima di fissarla. Pretenderla fresca qui
    // faceva dire "non pronto" a un instradamento VOX perfettamente valido
    // finche' non capitava un aggiornamento audio per altri motivi.
    return true;
#else
    return false;
#endif
}

bool DecodiumBridge::sstvTxAudioOnlyAllowed() const
{
#if DECODIUM_HAS_SSTV
    // A disconnected CAT/PTT path is safe for a local BlackHole/WAV test:
    // audio can still be rendered, but no radio can be keyed. Remote and TCI
    // routes remain excluded because they have their own ownership model.
    return !m_decoPortUseRemote
        && !usingTciAudioInput()
        && !sstvTxUsesVoxPtt()
        && !sstvTxCanControlPtt()
        && !sstvTxPttActive();
#else
    return false;
#endif
}

QVariantMap DecodiumBridge::sstvTxDiagnostics() const
{
    QVariantMap result;
#if DECODIUM_HAS_SSTV
    if (!m_sstvTxCoordinator) {
        return result;
    }
    const decodium::sstv::SstvTxCoordinatorSnapshot snapshot
        = m_sstvTxCoordinator->snapshot();
    const double headroomDb = snapshot.headroom > 0.0
        ? 20.0 * std::log10(snapshot.headroom) : 0.0;
    result.insert(QStringLiteral("peak"), snapshot.pcmPeak);
    result.insert(QStringLiteral("pcmPeak"), snapshot.pcmPeak);
    result.insert(QStringLiteral("headroom"), snapshot.headroom);
    result.insert(QStringLiteral("headroomDb"), headroomDb);
    result.insert(QStringLiteral("clippedFrames"),
                  static_cast<qulonglong>(snapshot.clippedFrames));
    result.insert(QStringLiteral("clipping"),
                  snapshot.clippedFrames != 0U);
    QStringList const blockers = sstvTxPreflightBlockers();
    result.insert(QStringLiteral("preflightReady"), blockers.isEmpty());
    result.insert(QStringLiteral("preflightBlockers"), blockers);
#endif
    return result;
}

bool DecodiumBridge::sstvTxActive() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvOwnsBridgeTx
        || (m_sstvTxCoordinator
            && m_sstvTxCoordinator->stateMachine().active());
#else
    return false;
#endif
}

QString DecodiumBridge::sstvTxState() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvTxState;
#else
    return QStringLiteral("Disabled");
#endif
}

double DecodiumBridge::sstvTxProgress() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvTxProgress;
#else
    return 0.0;
#endif
}

QString DecodiumBridge::sstvTxError() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvTxError;
#else
    return {};
#endif
}

