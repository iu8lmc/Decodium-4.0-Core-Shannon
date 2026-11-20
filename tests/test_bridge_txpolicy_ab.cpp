#include <QString>
#include <QStringList>
#include <iostream>
#include "src/bridge/DecodiumBridgeTxTimingMath.h"
#include "src/bridge/DecodiumBridgeTxPolicy.h"

#define txSyncLeadInMsForMode originalLeadIn
#define decodiumBridgeTxSyncLeadInMsForMode originalLeadInWrapper
#define autoTxDecodeGraceMs originalDecodeGrace
#include "bridge_refactor_fixtures/txleadingrace_before.inc"
#define estimatedSyncPayloadMs originalEstimatedPayload
#define syncTxStartBudgetMs originalStartBudget
#define minimumUsefulSyncPayloadMs originalMinimumPayload
#define ft2AsyncTxStartSafetyMs originalFt2Safety
#define latestFt2AsyncTxStartMs originalLatestFt2
#define decodiumBridgeLatestFt2AsyncTxStartMs originalLatestFt2Wrapper
#define latestD3CompatibleSyncTxStartMs originalLatestD3
#include "bridge_refactor_fixtures/txpolicy_before.inc"
#undef estimatedSyncPayloadMs
#undef syncTxStartBudgetMs
#undef minimumUsefulSyncPayloadMs
#undef ft2AsyncTxStartSafetyMs
#undef latestFt2AsyncTxStartMs
#undef decodiumBridgeLatestFt2AsyncTxStartMs
#undef latestD3CompatibleSyncTxStartMs
#undef txSyncLeadInMsForMode
#undef decodiumBridgeTxSyncLeadInMsForMode
#undef autoTxDecodeGraceMs

int main()
{
    const QStringList modes {"FT8", "FT4", "FT2", "JT4", "MSK144", "WSPR", "Q65-15A", "Q65-120D", "FST4-15", "FST4W-120", "UNKNOWN", " ft8 "};
    const int periods[] {0, 3750, 7500, 15000, 60000, 120000};
    for (const auto& mode : modes) {
        if (originalEstimatedPayload(mode) != estimatedSyncPayloadMs(mode)
            || originalStartBudget(mode) != syncTxStartBudgetMs(mode)
            || originalMinimumPayload(mode) != minimumUsefulSyncPayloadMs(mode)
            || originalLeadIn(mode) != txSyncLeadInMsForMode(mode)
            || originalLeadInWrapper(mode) != decodiumBridgeTxSyncLeadInMsForMode(mode)
            || originalDecodeGrace(mode, false) != autoTxDecodeGraceMs(mode, false)
            || originalDecodeGrace(mode, true) != autoTxDecodeGraceMs(mode, true)) return 1;
        for (int period : periods) for (bool immediate : {false, true}) {
            if (originalLatestD3(mode, period, immediate)
                != latestD3CompatibleSyncTxStartMs(mode, period, immediate)) return 2;
        }
    }
    for (int period : periods) for (bool conservative : {false, true}) {
        if (originalFt2Safety(conservative) != ft2AsyncTxStartSafetyMs(conservative)
            || originalLatestFt2(period, conservative) != latestFt2AsyncTxStartMs(period, conservative)
            || originalLatestFt2Wrapper(period, conservative) != decodiumBridgeLatestFt2AsyncTxStartMs(period, conservative)) return 3;
    }
    std::cout << "TX policy A/B OK: modes, periods, immediate clicks and FT2 conservative timing\n";
    return 0;
}
