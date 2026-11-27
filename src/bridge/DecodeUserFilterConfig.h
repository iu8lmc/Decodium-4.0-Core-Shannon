#pragma once
#include <QStringList>
#include <QVariantMap>

struct DecodeUserFilterConfig {
    bool waitPounceOnly {false};
    bool callingOnly {false};
    bool blacklistEnabled {false};
    bool whitelistEnabled {false};
    bool alwaysPassEnabled {false};
    QStringList blacklistTerms;
    QStringList whitelistTerms;
    QStringList passTerms;
    QStringList territoryTerms;
};

bool shouldAcceptDecodeEntryByUserFilters(const QVariantMap&, const DecodeUserFilterConfig&, bool, QString*);
