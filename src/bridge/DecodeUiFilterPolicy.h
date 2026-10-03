#pragma once

#include <QString>
#include <QVariantMap>

namespace decodium {
namespace decode_ui {

inline bool isWorkedCq(QVariantMap const& entry)
{
    if (!entry.value(QStringLiteral("isCQ")).toBool()) {
        return false;
    }

    return entry.value(QStringLiteral("isB4")).toBool()
        || entry.value(QStringLiteral("dxIsWorked")).toBool()
        || entry.value(QStringLiteral("dxIsWorkedBand")).toBool();
}

// The worked-station filters the user can switch on in Settings > Filters.
// Kept in one struct so that adding a filter does not mean threading another
// boolean through every display pipeline.
struct WorkedFilterOptions
{
    bool hideWorkedBand {false};   // same call already worked on this band
    bool hideWorkedToday {false};  // same call worked today (UTC QSO_DATE)
    bool hideWorkedEver {false};   // call present anywhere in the log
    // 1.0.584: matches the WSJT-X / JTDX quick filter that widens "today" to
    // "today and yesterday" (UTC).  Only meaningful with hideWorkedToday.
    bool todayIncludesYesterday {false};
};

inline bool isHiddenByWorkedFilters(QVariantMap const& entry,
                                    WorkedFilterOptions const& options,
                                    bool preserveWorkedCq)
{
    // "CQ Only" is a presentation mode: a CQ must remain available to the
    // delegate so that the B4 colour and optional strikeout can be rendered.
    // Explicit worked-station hiding keeps its usual behaviour in other modes.
    if (preserveWorkedCq && isWorkedCq(entry)) {
        return false;
    }

    if (options.hideWorkedBand
        && entry.value(QStringLiteral("dxIsWorkedBand")).toBool()) {
        return true;
    }
    if (options.hideWorkedEver
        && entry.value(QStringLiteral("dxIsWorkedEver")).toBool()) {
        return true;
    }
    if (options.hideWorkedToday) {
        if (entry.value(QStringLiteral("dxIsWorkedToday")).toBool()) {
            return true;
        }
        if (options.todayIncludesYesterday
            && entry.value(QStringLiteral("dxIsWorkedYesterday")).toBool()) {
            return true;
        }
    }
    return false;
}

} // namespace decode_ui
} // namespace decodium
