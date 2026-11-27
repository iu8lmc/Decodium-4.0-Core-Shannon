static int txSyncLeadInMsForMode(const QString& mode)
{
    QString const normalized = mode.trimmed().toUpper();
    if (normalized == QStringLiteral("FT2LINK")) {
        return 300;
    }
    if (normalized == QStringLiteral("FT8") || normalized == QStringLiteral("FT4")) {
        return 500;
    }
    if (normalized == QStringLiteral("Q65") || normalized.startsWith(QStringLiteral("Q65-"))) {
        return 1000;
    }
    if (normalized == QStringLiteral("JT4")) {
        return 1000;
    }
    if (normalized == QStringLiteral("FST4")
        || normalized == QStringLiteral("FST4W")
        || normalized.startsWith(QStringLiteral("FST4-"))
        || normalized.startsWith(QStringLiteral("FST4W-"))) {
        return fst4PeriodSecondsForMode(normalized, 0.0) == 15 ? 500 : 1000;
    }
    if (normalized == QStringLiteral("FT2")) {
#if defined(Q_OS_LINUX)
        return 0;
#else
        return 300;
#endif
    }
    return 0;
}

int decodiumBridgeTxSyncLeadInMsForMode(const QString& mode)
{
    return txSyncLeadInMsForMode(mode);
}

static int autoTxDecodeGraceMs(const QString& mode, bool ft8FastSequence = false)
{
    QString const normalized = mode.trimmed().toUpper();
    // Grace = quanto aspettare al boundary il decode dello slot appena finito prima
    // di committare il TX auto. Deve superare (settle 650ms + decode + advanceQsoState)
    // ~0.95-1.0s in FT8, altrimenti la fallback committa CQ con lo stato vecchio e la
    // risposta al partner viene persa (QSO non si chiude).
    // 1.0.317 — opt-in ft8FastSequence: grace FT8 ridotta a 400ms per reattività stile
    // WSJT-X/JTDX. Sotto pressione CPU il qMax(900,...) in effectiveAutoTxDecodeGraceMs
    // sovrascrive automaticamente (sicurezza > reattività su PC carichi).
    if (normalized == QStringLiteral("FT8"))
        return ft8FastSequence ? 400 : 1200;
    if (normalized == QStringLiteral("FT4"))
        return 700;
    if (normalized == QStringLiteral("FT2"))
        return 250;
    return 500;
}

