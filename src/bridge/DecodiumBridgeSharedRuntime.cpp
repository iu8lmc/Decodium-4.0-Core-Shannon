// SPDX-License-Identifier: GPL-3.0-or-later
#include "FtDecodeThreadBudget.hpp"
#include "BridgeConfigConstants.h"
#include "DecodiumBridgeTxPolicy.h"
#include "FtxMessageHelpers.h"
#include <QByteArray>
#include <QTimeZone>

namespace {

constexpr int kMaxFtDecodeThreads {decodium::decode::kMaxFtDecodeThreads};

QTimeZone decodiumUtcTimeZone()
{
    return QTimeZone(QByteArrayLiteral("UTC"));
}

bool decodiumPskDebugEnabled()
{
    static const bool enabled = []() {
        QByteArray value = qgetenv("DECODIUM_PSK_DEBUG").trimmed().toLower();
        return value == "1" || value == "true" || value == "yes" || value == "on";
    }();
    return enabled;
}

}

bool decodiumBridgePskDebugEnabled() { return decodiumPskDebugEnabled(); }


int decodiumBridgeMaxFtDecodeThreads() { return kMaxFtDecodeThreads; }

QTimeZone decodiumBridgeUtcTimeZone()
{
    return decodiumUtcTimeZone();
}


int decodiumBridgeSpecialOpHound() { return kSpecialOpHound; }

int decodiumBridgeLatestD3SyncStartMs(const QString& mode, int period, bool immediate) { return latestD3CompatibleSyncTxStartMs(mode, period, immediate); }

bool decodiumBridgeIsFtx77MessageMode(QString const& mode)
{
    return isFtx77MessageMode(mode);
}

