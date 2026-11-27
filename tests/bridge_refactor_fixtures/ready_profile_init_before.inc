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
