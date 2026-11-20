int decodiumBridgeSpecialOpHound() { return kSpecialOpHound; }

int decodiumBridgeLatestD3SyncStartMs(const QString& mode, int period, bool immediate) { return latestD3CompatibleSyncTxStartMs(mode, period, immediate); }

bool decodiumBridgeIsFtx77MessageMode(QString const& mode)
{
    return isFtx77MessageMode(mode);
}

