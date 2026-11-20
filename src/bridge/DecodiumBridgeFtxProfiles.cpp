#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include <QSettings>

extern void decodiumBridgeFtxSettingsLog(const QString& message);

// 1.0.174 — FT2 Weak-Signal Pack master flag setter.
// Ghost filter rilassato (-24 dB), retry cap esteso, same-step wait
// adattivo. Persistito su QSettings; il valore viene riletto da loadSettings()
// al prossimo avvio.
void DecodiumBridge::setFt2Conservative(bool v)
{
    if (m_ft2Conservative == v) return;
    m_ft2Conservative = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2Conservative"), v);
    emit ft2ConservativeChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Conservative mode %1").arg(v ? "ON" : "OFF"));
}

// 1.0.384 — Profili pronti: applica in blocco un set coerente di toggle FT2/decode.
// I valori non esplicitati dall'utente sono inferiti per l'intento del profilo (vedi matrice
// nelle note di release). Usa i setter esistenti (persistono + emettono) sotto guard
// m_applyingReadyProfile, cosi' l'auto-clear di activeReadyProfile non scatta durante l'apply.
void DecodiumBridge::applyReadyProfile(const QString& id)
{
    struct ReadyProfile {
        bool conservative, fullDecodeAutoCq, quickGiveUpStrong, adaptiveDecode, apHashCache,
             skipFineSlot, mamMultiStream, partnerMemory, tx2Resend, smoothFlow;
        int  callerRetries, mamStreams;
    };
    ReadyProfile p;
    QString normId = id;
    if (id == QLatin1String("weak")) {
        //         cons   full   strong adapt  ap     skip   mam    pmem   tx2    smooth  retr streams
        p = ReadyProfile{ true,  true,  true,  false, true,  false, false, true,  true,  true,   7,   2 };
    } else if (id == QLatin1String("contest")) {
        p = ReadyProfile{ false, true,  true,  true,  true,  true,  true,  true,  true,  true,   3,   2 };
    } else if (id == QLatin1String("cpu")) {
        p = ReadyProfile{ false, false, false, true,  false, false, false, false, true,  true,  10,   2 };
    } else {
        normId = QStringLiteral("balanced");
        p = ReadyProfile{ true,  true,  true,  true,  true,  false, false, true,  true,  true,   5,   2 };
    }

    m_applyingReadyProfile = true;
    setFt2Conservative(p.conservative);
    setFt2FullDecodeInAutoCq(p.fullDecodeAutoCq);
    setFt2QuickGiveUpStrong(p.quickGiveUpStrong);
    setFt2AdaptiveDecode(p.adaptiveDecode);
    setFt2ApHashCache(p.apHashCache);
    setFt2AsyncSkipRedundantSyncDecode(p.skipFineSlot);
    if (p.mamMultiStream) setMamMaxStreams(p.mamStreams);
    setMamMultiStream(p.mamMultiStream);
    setFt2PartnerMemoryEnabled(p.partnerMemory);
    setFt2Tx2ResendOnStall(p.tx2Resend);
    setSmoothDecodeFlow(p.smoothFlow);
    setMaxCallerRetries(p.callerRetries);
    m_applyingReadyProfile = false;

    if (m_activeReadyProfile != normId) {
        m_activeReadyProfile = normId;
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        decodium::beginActiveSettingsProfile(settings);
        settings.setValue(QStringLiteral("ActiveReadyProfile"), m_activeReadyProfile);
        emit activeReadyProfileChanged();
    }
    decodiumBridgeFtxSettingsLog(QStringLiteral("[Profile] applicato profilo pronto '%1'").arg(normId));
}

// 1.0.384 — svuota activeReadyProfile quando l'utente modifica a mano un toggle gestito
// da un profilo (non durante l'apply del profilo stesso, grazie alla guard).
void DecodiumBridge::clearActiveReadyProfileOnManualChange()
{
    if (m_applyingReadyProfile || m_activeReadyProfile.isEmpty()) return;
    m_activeReadyProfile.clear();
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("ActiveReadyProfile"), m_activeReadyProfile);
    emit activeReadyProfileChanged();
}

void DecodiumBridge::setAutoCqMaxCycles(int v)
{
    v = qBound(0, v, 999);
    if (m_autoCqMaxCycles == v) return;
    m_autoCqMaxCycles = v;
    applyAutoCqBurstCadenceToLegacyBackend();
    emit autoCqMaxCyclesChanged();
    saveSettingsAsync();
}

void DecodiumBridge::setAutoCqPauseSec(int v)
{
    v = qBound(0, v, 300);
    if (m_autoCqPauseSec == v) return;
    m_autoCqPauseSec = v;
    m_autoCqGenericListenUntilMs = 0;
    applyAutoCqBurstCadenceToLegacyBackend();
    emit autoCqPauseSecChanged();
    saveSettingsAsync();
}

void DecodiumBridge::setAutoCqBurstCalls(int v)
{
    int const clamped = qBound(0, v, 999);
    if (m_autoCqBurstCalls == clamped) {
        return;
    }

    m_autoCqBurstCalls = clamped;
    resetAutoCqBurstCadence(QStringLiteral("calls-per-burst changed"));
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("AutoCqBurstCalls"), clamped);
    applyAutoCqBurstCadenceToLegacyBackend();
    emit autoCqBurstCallsChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("AutoCQ burst: calls per burst = %1").arg(clamped));
}

void DecodiumBridge::setAutoCqListenCycles(int v)
{
    int const clamped = qBound(0, v, 999);
    if (m_autoCqListenCycles == clamped) {
        return;
    }

    m_autoCqListenCycles = clamped;
    resetAutoCqBurstCadence(QStringLiteral("listening cycles changed"));
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("AutoCqListenCycles"), clamped);
    applyAutoCqBurstCadenceToLegacyBackend();
    emit autoCqListenCyclesChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("AutoCQ burst: listening cycles = %1").arg(clamped));
}


void DecodiumBridge::initialiseReadyProfileConnections()
{
    // 1.0.384 — appena l'utente cambia a mano uno dei toggle gestiti da un profilo pronto,
    // l'etichetta del profilo attivo torna a "personalizzato" (guard durante l'apply).
    {
        const QList<void (DecodiumBridge::*)()> profileToggleSignals = {
            &DecodiumBridge::ft2ConservativeChanged,
            &DecodiumBridge::ft2FullDecodeInAutoCqChanged,
            &DecodiumBridge::ft2QuickGiveUpStrongChanged,
            &DecodiumBridge::ft2AdaptiveDecodeChanged,
            &DecodiumBridge::ft2ApHashCacheChanged,
            &DecodiumBridge::ft2AsyncSkipRedundantSyncDecodeChanged,
            &DecodiumBridge::mamMultiStreamChanged,
            &DecodiumBridge::mamMaxStreamsChanged,
            &DecodiumBridge::ft2PartnerMemoryEnabledChanged,
            &DecodiumBridge::ft2Tx2ResendOnStallChanged,
            &DecodiumBridge::smoothDecodeFlowChanged,
            &DecodiumBridge::maxCallerRetriesChanged,
        };
        for (auto sig : profileToggleSignals)
            connect(this, sig, this, &DecodiumBridge::clearActiveReadyProfileOnManualChange);
    }
}
