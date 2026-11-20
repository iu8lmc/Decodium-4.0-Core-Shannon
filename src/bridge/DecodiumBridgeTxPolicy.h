#pragma once
#include <QString>
int estimatedSyncPayloadMs(const QString&);
int syncTxStartBudgetMs(const QString&);
int minimumUsefulSyncPayloadMs(const QString&);
int ft2AsyncTxStartSafetyMs(bool);
int latestFt2AsyncTxStartMs(int, bool);
int decodiumBridgeLatestFt2AsyncTxStartMs(int, bool);
int latestD3CompatibleSyncTxStartMs(const QString&, int, bool = false);
int txSyncLeadInMsForMode(const QString&);
int decodiumBridgeTxSyncLeadInMsForMode(const QString&);
int autoTxDecodeGraceMs(const QString&, bool = false);
