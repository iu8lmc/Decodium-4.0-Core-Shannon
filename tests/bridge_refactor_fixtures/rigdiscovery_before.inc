QVariantList DecodiumBridge::detectConnectedRigs() const
{
    // Delegato a un modulo a se': la rilevazione restituisce solo DATI, cosi'
    // vale per qualunque backend CAT attivo (nativo, Hamlib, OmniRig, TCI).
    QVariantList const candidates = DecodiumRigDetector::detect();
    bridgeLog(QStringLiteral("rilevamento radio: %1 candidati").arg(candidates.size()));
    for (QVariant const& entry : candidates) {
        QVariantMap const candidate = entry.toMap();
        bridgeLog(QStringLiteral("  %1 su %2 (fiducia %3): %4")
                      .arg(candidate.value(QStringLiteral("rigLabel")).toString(),
                           candidate.value(QStringLiteral("catPort")).toString(),
                           candidate.value(QStringLiteral("confidence")).toString(),
                           candidate.value(QStringLiteral("evidence")).toString()));
    }
    return candidates;
}

