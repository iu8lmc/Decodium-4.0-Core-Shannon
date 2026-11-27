#include "DecodiumBridge.h"

bool decodiumBridgeDecodeTelemetryOnly(QString const&);

decodium::decode_ui::WorkedFilterOptions DecodiumBridge::workedFilterOptions() const
{
    decodium::decode_ui::WorkedFilterOptions options;
    if (m_filtersBypassed) {
        return options;
    }
    options.hideWorkedBand =
        getSetting(QStringLiteral("FiltersHideWorkedBand"), false).toBool();
    options.hideWorkedToday =
        getSetting(QStringLiteral("FiltersHideWorkedToday"), false).toBool();
    options.hideWorkedEver =
        getSetting(QStringLiteral("FiltersHideWorkedEver"), false).toBool();
    options.todayIncludesYesterday =
        getSetting(QStringLiteral("FiltersWorkedTodayIncludesYesterday"), false).toBool();
    return options;
}

bool DecodiumBridge::shouldDisplayEntryForBandActivity(QVariantMap const& entry) const
{
    return shouldDisplayEntryForBandActivity(entry, workedFilterOptions());
}

bool DecodiumBridge::shouldDisplayEntryForBandActivity(
    QVariantMap const& entry,
    decodium::decode_ui::WorkedFilterOptions const& workedFilters) const
{
    if (entry.isEmpty()) return false;
    // 1.0.145: prima cosa, filter ghost decode se attivo. Le entries TX sono
    // certe; MyCall resta passante solo quando non mostra segnali strutturali
    // di falso positivo.
    if (m_hideGhostDecodes && looksLikeGhostDecode(entry)) return false;
    bool const isTxEntry = entry.value(QStringLiteral("isTx")).toBool()
        || entry.value(QStringLiteral("db")).toString().trimmed().compare(QStringLiteral("TX"), Qt::CaseInsensitive) == 0;
    bool const preserveWorkedCq = !m_filtersBypassed && m_filterCqOnly;
    if (!isTxEntry
        && decodium::decode_ui::isHiddenByWorkedFilters(
            entry, workedFilters, preserveWorkedCq)) {
        return false;
    }
    if (!m_hideTelemetryOnlyDecodes) return true;
    QString const msg = entry.value(QStringLiteral("displayMessage")).toString();
    QString const fallback = entry.value(QStringLiteral("message")).toString();
    return !decodiumBridgeDecodeTelemetryOnly(msg.isEmpty() ? fallback : msg);
}

