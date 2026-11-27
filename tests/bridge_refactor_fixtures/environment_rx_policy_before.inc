bool truthyEnvironmentFlag(char const* name)
{
    QString const value = qEnvironmentVariable(name).trimmed().toLower();
    return value == QStringLiteral("1")
        || value == QStringLiteral("true")
        || value == QStringLiteral("yes")
        || value == QStringLiteral("on");
}

bool catSuppressedByEnvironment()
{
    return truthyEnvironmentFlag("DECODIUM_DISABLE_CAT")
        || truthyEnvironmentFlag("DECODIUM_RX_RECORD_DISABLE_CAT");
}

bool bridgeOwnsLegacyJtRxWhenCatSuppressed(QString const& mode)
{
    if (!catSuppressedByEnvironment()) {
        return false;
    }
    QString const normalized = mode.trimmed().toUpper();
    return normalized == QStringLiteral("JT4")
        || normalized == QStringLiteral("JT9")
        || normalized == QStringLiteral("JT65");
}

bool bridgeOwnsFst4Audio(QString const& mode)
{
    QString const normalized = mode.trimmed().toUpper();
    return normalized == QStringLiteral("FST4")
        || normalized == QStringLiteral("FST4W")
        || normalized.startsWith(QStringLiteral("FST4-"))
        || normalized.startsWith(QStringLiteral("FST4W-"));
}

}

bool decodiumBridgeCatSuppressedForTx() { return catSuppressedByEnvironment(); }
bool decodiumBridgeOwnsLegacyJtRxWhenCatSuppressed(QString const& mode) { return bridgeOwnsLegacyJtRxWhenCatSuppressed(mode); }
bool decodiumBridgeOwnsFst4Audio(QString const& mode) { return bridgeOwnsFst4Audio(mode); }
bool decodiumBridgeTruthyEnvironmentFlag(const char* name) { return truthyEnvironmentFlag(name); }
