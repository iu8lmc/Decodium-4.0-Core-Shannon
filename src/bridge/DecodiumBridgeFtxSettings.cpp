#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include <QSettings>

extern void decodiumBridgeFtxSettingsLog(const QString& message);

// 1.0.311 — cap ripetizioni 73/RR73 in FT2 (1-8). Persistito nello store canonico Decodium3.
void DecodiumBridge::setFt2SignoffRetryCap(int v)
{
    int const clamped = qBound(1, v, 8);
    if (m_ft2SignoffRetryCap == clamped) return;
    m_ft2SignoffRetryCap = clamped;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);  // 1.0.493: senza, sotto -config il load rileggeva il valore stale del profilo
    settings.setValue(QStringLiteral("Ft2SignoffRetryCap"), clamped);
    emit ft2SignoffRetryCapChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Signoff retry cap (FT2) = %1").arg(clamped));
}

// 1.0.314 — TX immediato al click (stile 1.0.283). Default OFF = comportamento upstream
// sicuro (period-gate stretto FT2/FT8/FT4). ON = rilassa: FT2 bypassa TX1, FT8/FT4 alzano
// la finestra cliccabile da ~650ms a 2000ms (shift ancora dentro la tolleranza partner).
void DecodiumBridge::setFtxImmediateClickTx(bool v)
{
    if (m_ftxImmediateClickTx == v) return;
    m_ftxImmediateClickTx = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("FtxImmediateClickTx"), v);
    emit ftxImmediateClickTxChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Immediate-click TX (FT2/FT8/FT4) %1").arg(v ? "ON" : "OFF"));
}

// 1.0.371 - opt-in: logga il QSO chiuso da noi con RR73 (TX4) in FT2 AutoCQ async anche
// se il partner sparisce. Default OFF = comportamento attuale (one-shot disarm).
void DecodiumBridge::setFt2LogRr73OnPartnerLeft(bool v)
{
    if (m_ft2LogRr73OnPartnerLeft == v) return;
    m_ft2LogRr73OnPartnerLeft = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2LogRr73OnPartnerLeft"), v);
    emit ft2LogRr73OnPartnerLeftChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Log RR73 on partner-left (FT2 async TX4) %1").arg(v ? "ON" : "OFF"));
}

// 1.0.315 — cap ripetizioni 73/RR73 in FT4 (1-8, default 4). Persistito Decodium3.
void DecodiumBridge::setFt4SignoffRetryCap(int v)
{
    int const clamped = qBound(1, v, 8);
    if (m_ft4SignoffRetryCap == clamped) return;
    m_ft4SignoffRetryCap = clamped;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft4SignoffRetryCap"), clamped);
    emit ft4SignoffRetryCapChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Signoff retry cap (FT4) = %1").arg(clamped));
}

// 1.0.315 — cap ripetizioni 73/RR73 in FT8 (1-8, default 3). Persistito Decodium3.
void DecodiumBridge::setFt8SignoffRetryCap(int v)
{
    int const clamped = qBound(1, v, 8);
    if (m_ft8SignoffRetryCap == clamped) return;
    m_ft8SignoffRetryCap = clamped;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft8SignoffRetryCap"), clamped);
    emit ft8SignoffRetryCapChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Signoff retry cap (FT8) = %1").arg(clamped));
}

// 1.0.437 - opt-in: extra signoff retries per partner debole (FTX). Default OFF = byte-identico.
void DecodiumBridge::setFtxWeakSignoffBoost(bool v)
{
    if (m_ftxWeakSignoffBoost == v) return;
    m_ftxWeakSignoffBoost = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("FtxWeakSignoffBoost"), v);
    emit ftxWeakSignoffBoostChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Weak-partner signoff boost (FTX) %1").arg(v ? "ON" : "OFF"));
}

void DecodiumBridge::setFtxWeakSnrThreshold(int v)
{
    int const clamped = qBound(-30, v, -5);
    if (m_ftxWeakSnrThreshold == clamped) return;
    m_ftxWeakSnrThreshold = clamped;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("FtxWeakSnrThreshold"), clamped);
    emit ftxWeakSnrThresholdChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Weak-partner SNR threshold = %1 dB").arg(clamped));
}

void DecodiumBridge::setFtxWeakSignoffBonus(int v)
{
    int const clamped = qBound(1, v, 6);
    if (m_ftxWeakSignoffBonus == clamped) return;
    m_ftxWeakSignoffBonus = clamped;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("FtxWeakSignoffBonus"), clamped);
    emit ftxWeakSignoffBonusChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Weak-partner signoff bonus = +%1").arg(clamped));
}

// 1.0.446 - opt-in guard: limita i ri-aggganci RRR a un partner appena loggato (caso 9H1SR).
void DecodiumBridge::setFt2PostLogReengageGuard(bool v)
{
    if (m_ft2PostLogReengageGuard == v) return;
    m_ft2PostLogReengageGuard = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2PostLogReengageGuard"), v);
    emit ft2PostLogReengageGuardChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Post-log RRR re-engage guard %1").arg(v ? "ON" : "OFF"));
}

void DecodiumBridge::setFt2PostLogReengageMax(int v)
{
    int const clamped = qBound(0, v, 5);
    if (m_ft2PostLogReengageMax == clamped) return;
    m_ft2PostLogReengageMax = clamped;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2PostLogReengageMax"), clamped);
    emit ft2PostLogReengageMaxChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Post-log RRR re-engage max = %1").arg(clamped));
}

// 1.0.446 - P0-3 opt-in: il TX watchdog logga il QSO in chiusura invece di abbandonarlo.
void DecodiumBridge::setTxWatchdogLogOnClose(bool v)
{
    if (m_txWatchdogLogOnClose == v) return;
    m_txWatchdogLogOnClose = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("TxWatchdogLogOnClose"), v);
    emit txWatchdogLogOnCloseChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[Watchdog] Log QSO on close at timeout %1").arg(v ? "ON" : "OFF"));
}

// 1.0.326 B2 — Caller retries (1-99). 1.0.493: spostato dall'inline header e reso
// profile-aware: prima scriveva sempre nella root INI, quindi sotto istanza -config
// il load rileggeva la copia stale del profilo (spinbox "ignorato" al riavvio).
void DecodiumBridge::setMaxCallerRetries(int v)
{
    int const clamped = qBound(1, v, 99);
    if (m_maxCallerRetries == clamped) return;
    m_maxCallerRetries = clamped;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("MaxCallerRetries"), clamped);
    emit maxCallerRetriesChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Caller retries cap = %1").arg(clamped));
}

void DecodiumBridge::setCallerRetriesAlwaysHard(bool v)
{
    if (m_callerRetriesAlwaysHard == v) return;
    m_callerRetriesAlwaysHard = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("CallerRetriesAlwaysHard"), v);
    emit callerRetriesAlwaysHardChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Caller retries always hard = %1").arg(v ? "ON" : "OFF"));
}

// 1.0.447 - Fondamenta Fase 1 opt-in: censimento transizioni stato FT2 (solo-logging).
void DecodiumBridge::setFt2TransitionCensus(bool v)
{
    if (m_ft2TransitionCensus == v) return;
    m_ft2TransitionCensus = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2TransitionCensus"), v);
    emit ft2TransitionCensusChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[STATE] FT2 transition census %1").arg(v ? "ON" : "OFF"));
}

// 1.0.447 - Leva#6-A opt-in: gate smart-TX adattivi all'occupazione del canale.
void DecodiumBridge::setFt2AdaptiveTxGates(bool v)
{
    if (m_ft2AdaptiveTxGates == v) return;
    m_ft2AdaptiveTxGates = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2AdaptiveTxGates"), v);
    emit ft2AdaptiveTxGatesChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Adaptive TX gates %1").arg(v ? "ON" : "OFF"));
}

// 1.0.321 — opt-in FT2 manual one-shot disarm (Salvatore latch fix da 1.0.300).
// Default OFF su fork = TX1 ripete fino a maxCallerRetries → no "TX1 stacca su weak signal".
void DecodiumBridge::setFt2ManualOneShotEnabled(bool v)
{
    if (m_ft2ManualOneShotEnabled == v) return;
    m_ft2ManualOneShotEnabled = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2ManualOneShotEnabled"), v);
    emit ft2ManualOneShotEnabledChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] FT2 manual one-shot disarm %1").arg(v ? "ON" : "OFF"));
}

// 1.0.317 — opt-in FT8 fast sequence: grace ridotta (1200→400ms) + onFt8DecodeReady
// accetta decode tardivi entro d3CapMs invece di scartare lo slot. Pensata per "ci
// mette troppo dopo che chiamo" (feedback Pasquale). Default OFF = comportamento
// upstream conservativo (massima affidabilità decode). Persistito Decodium3.
void DecodiumBridge::setFt8FastSequence(bool v)
{
    if (m_ft8FastSequence == v) return;
    m_ft8FastSequence = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft8FastSequence"), v);
    emit ft8FastSequenceChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] FT8 fast sequence %1").arg(v ? "ON" : "OFF"));
}

// 1.0.367 — finestra TX FT2 async conservativa (default ON). ON = richiede spazio
// per il payload INTERO (~18% slot, niente frame troncati, stile Decodium 3.0).
// OFF = comportamento FIX B 1.0.353 (finestra ~76%, più reattivo ma può troncare).
// Persistito Decodium3. Vedi latestFt2AsyncTxStartMs / ft2AsyncTxStartSafetyMs.
void DecodiumBridge::setFt2ConservativeTiming(bool v)
{
    if (m_ft2ConservativeTiming == v) return;
    m_ft2ConservativeTiming = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2ConservativeTiming"), v);
    emit ft2ConservativeTimingChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] FT2 conservative TX window %1").arg(v ? "ON" : "OFF"));
}

// 1.0.289 — FT2 enhancement toggles (opt-in, default OFF = comportamento 1.0.288)
void DecodiumBridge::setFt2FullDecodeInAutoCq(bool v)
{
    if (m_ft2FullDecodeInAutoCq == v) return;
    m_ft2FullDecodeInAutoCq = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2FullDecodeInAutoCq"), v);
    emit ft2FullDecodeInAutoCqChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Full decode in AutoCQ %1").arg(v ? "ON" : "OFF"));
}

void DecodiumBridge::setCallerScoring(bool v)
{
    if (m_callerScoring == v) return;
    m_callerScoring = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("CallerScoring"), v);
    emit callerScoringChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("CallerScoring %1").arg(v ? "ON" : "OFF"));
    if (v) rankCallerQueue(QStringLiteral("enabled"));
}

void DecodiumBridge::setFt2QuickGiveUpStrong(bool v)
{
    if (m_ft2QuickGiveUpStrong == v) return;
    m_ft2QuickGiveUpStrong = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2QuickGiveUpStrong"), v);
    emit ft2QuickGiveUpStrongChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Quick give-up strong partner %1").arg(v ? "ON" : "OFF"));
}

void DecodiumBridge::setFt2AdaptiveDecode(bool v)
{
    if (m_ft2AdaptiveDecode == v) return;
    m_ft2AdaptiveDecode = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2AdaptiveDecode"), v);
    emit ft2AdaptiveDecodeChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Adaptive decode %1").arg(v ? "ON" : "OFF"));
}

void DecodiumBridge::setFt2NarrowAsyncDecode(bool v)
{
    if (m_ft2NarrowAsyncDecode == v) return;
    m_ft2NarrowAsyncDecode = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2NarrowAsyncDecode"), v);
    emit ft2NarrowAsyncDecodeChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Narrow async decode %1").arg(v ? "ON" : "OFF"));
}

void DecodiumBridge::setFt2ApHashCache(bool v)
{
    if (m_ft2ApHashCache == v) return;
    m_ft2ApHashCache = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2ApHashCache"), v);
    emit ft2ApHashCacheChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] AP hashed-callsign cache %1 (Phase 1: cache-confirmed borderline decodes; rescued rows are display-only for AutoSeq/TX)")
                  .arg(v ? "ON" : "OFF"));
}

// 1.0.355 — opt-in: salta il decode sync di fine-slot quando l'async ha GIA'
// prodotto un decode per quello slot. Riduce la contesa sul runtime_mutex/worker
// FT2 (~1.8s post-TX) che ritarda l'aggancio della risposta. Default OFF.
void DecodiumBridge::setFt2AsyncSkipRedundantSyncDecode(bool v)
{
    if (m_ft2AsyncSkipRedundantSyncDecode == v) return;
    m_ft2AsyncSkipRedundantSyncDecode = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2AsyncSkipRedundantSyncDecode"), v);
    emit ft2AsyncSkipRedundantSyncDecodeChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT2WS] Skip redundant end-slot sync decode %1").arg(v ? "ON" : "OFF"));
}

// 1.0.299 — Deep decode anche in TX (decode-list-only). Quando ON, il deep depth-4
// follow-up gira anche durante l'operazione TX/QSO (txStartPending) per recuperare le
// stazioni di terzi che il fast pass depth-2 perde, SENZA toccare il timing/decisione TX
// (il suo serial e' marcato list-only -> niente auto-seq). Default OFF (opt-in).
void DecodiumBridge::setFt8DeepDecodeInTx(bool v)
{
    if (m_ft8DeepDecodeInTx == v) return;
    m_ft8DeepDecodeInTx = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft8DeepDecodeInTx"), v);
    emit ft8DeepDecodeInTxChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT8] Deep decode in TX %1").arg(v ? "ON" : "OFF"));
}

// 1.0.304 (#9) — resume-on-reply opt-in. Alla Halt con un QSO attivo memorizza il partner;
// se quello ri-risponde (suo call + mio call) entro 2 min riprende il QSO. Default OFF:
// l'hard-stop v4 dell'Halt resta il comportamento di default (feedback tester 2026-05).
// F0 subpass-harvest: abilita l'engine subpass ft8b (lft8subpass), gia' implementato in
// FtxFt8Stage4.cpp ma hardcoded OFF nel worker. Opt-in, default OFF. F0 = solo plumbing:
// il flag arriva al worker via req.subpass. Entro il budget live ~5849ms non da' guadagno
// (A/B config E) ma e' benigno; il vero recupero deboli arrivera' col worker harvest (F1).
void DecodiumBridge::setFt8SubpassHarvest(bool v)
{
    if (m_ft8SubpassHarvest == v) return;
    m_ft8SubpassHarvest = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft8SubpassHarvest"), v);
    emit ft8SubpassHarvestChanged();
    decodiumBridgeFtxSettingsLog(QStringLiteral("[FT8] Subpass harvest %1").arg(v ? "ON" : "OFF"));
}

