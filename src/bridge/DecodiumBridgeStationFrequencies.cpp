#include "DecodiumBridge.h"
#include "Radio.hpp"
#include "models/Bands.hpp"
#include "models/StationList.hpp"
#include <algorithm>
#include <cmath>

extern void decodiumBridgeLogbookLog(const QString&);
extern Radio::FrequencyDelta decodiumBridgeParseFrequencyOffsetHzValue(const QVariant&, bool*);

QVariantList DecodiumBridge::stationFrequencyRows() const
{
    QVariant const raw = readSettingFromLegacyIni(QStringLiteral("stations"));
    StationList::Stations stations = raw.isValid() ? raw.value<StationList::Stations>() : StationList::Stations {};

    Bands bands;
    std::stable_sort(stations.begin(), stations.end(), [&bands](StationList::Station const& lhs,
                                                                StationList::Station const& rhs) {
        int const lhsBand = bands.find(lhs.band_name_);
        int const rhsBand = bands.find(rhs.band_name_);
        if (lhsBand != rhsBand) return lhsBand < rhsBand;
        return lhs.band_name_ < rhs.band_name_;
    });

    QVariantList rows;
    rows.reserve(stations.size());
    for (int i = 0; i < stations.size(); ++i) {
        StationList::Station const& station = stations.at(i);
        QVariantMap row;
        row.insert(QStringLiteral("index"), i);
        row.insert(QStringLiteral("band"), station.band_name_);
        row.insert(QStringLiteral("offsetHz"), QVariant::fromValue<qlonglong>(station.offset_));
        row.insert(QStringLiteral("offsetMHz"),
                   QString::number(static_cast<double>(station.offset_) / 1000000.0, 'f', 6));
        row.insert(QStringLiteral("offset"), Radio::pretty_frequency_MHz_string(station.offset_) + QStringLiteral(" MHz"));
        row.insert(QStringLiteral("antenna"), station.antenna_description_);
        rows << row;
    }
    return rows;
}

bool DecodiumBridge::addStationFrequencyRow(const QString& band,
                                            const QVariant& offset,
                                            const QString& antennaDescription)
{
    QString const cleanBand = band.trimmed();
    Bands bands;
    if (bands.find(cleanBand) < 0) {
        emit errorMessage(QStringLiteral("Invalid band for station information"));
        return false;
    }
    bool offsetOk = false;
    Radio::FrequencyDelta const offsetHz = decodiumBridgeParseFrequencyOffsetHzValue(offset, &offsetOk);
    if (!offsetOk) {
        emit errorMessage(QStringLiteral("Invalid band offset"));
        return false;
    }
    QVariant const raw = readSettingFromLegacyIni(QStringLiteral("stations"));
    StationList::Stations stations = raw.isValid() ? raw.value<StationList::Stations>() : StationList::Stations {};
    bool replacedExistingBand = false;
    for (StationList::Station& station : stations) {
        if (station.band_name_.compare(cleanBand, Qt::CaseInsensitive) == 0) {
            station = StationList::Station {cleanBand, offsetHz, antennaDescription.trimmed()};
            replacedExistingBand = true;
            break;
        }
    }
    if (!replacedExistingBand) {
        stations << StationList::Station {cleanBand, offsetHz, antennaDescription.trimmed()};
    }
    syncSettingToLegacyIni(QStringLiteral("stations"), QVariant::fromValue(stations));
    StationList::Stations const persistedStations =
        readSettingFromLegacyIni(QStringLiteral("stations")).value<StationList::Stations>();
    if (persistedStations != stations) {
        decodiumBridgeLogbookLog(QStringLiteral("Station information save verification failed for band=%1 offset=%2 Hz")
                      .arg(cleanBand, QString::number(offsetHz)));
        emit errorMessage(QStringLiteral("Station information could not be saved; check that the Decodium settings file is writable"));
        return false;
    }
    emit settingValueChanged(QStringLiteral("stations"), QVariant::fromValue(stations));
    if (m_currentCatLogicalBand.compare(cleanBand, Qt::CaseInsensitive) == 0) {
        m_currentCatStationOffsetHz = offsetHz;
    }
    emit statusMessage(QStringLiteral("Station information %1 for %2: offset %3 MHz")
                           .arg(replacedExistingBand ? QStringLiteral("updated")
                                                     : QStringLiteral("added"),
                                cleanBand,
                                QString::number(static_cast<double>(offsetHz) / 1000000.0, 'f', 6)));
    return true;
}

bool DecodiumBridge::updateStationFrequencyRow(int index,
                                               const QString& band,
                                               const QVariant& offset,
                                               const QString& antennaDescription)
{
    QString const cleanBand = band.trimmed();
    Bands bands;
    if (bands.find(cleanBand) < 0) {
        emit errorMessage(QStringLiteral("Invalid band for station information"));
        return false;
    }
    bool offsetOk = false;
    Radio::FrequencyDelta const offsetHz = decodiumBridgeParseFrequencyOffsetHzValue(offset, &offsetOk);
    if (!offsetOk) {
        emit errorMessage(QStringLiteral("Invalid band offset"));
        return false;
    }
    QVariant const raw = readSettingFromLegacyIni(QStringLiteral("stations"));
    StationList::Stations stations = raw.isValid() ? raw.value<StationList::Stations>() : StationList::Stations {};
    Bands sortBands;
    std::stable_sort(stations.begin(), stations.end(), [&sortBands](StationList::Station const& lhs,
                                                                    StationList::Station const& rhs) {
        int const lhsBand = sortBands.find(lhs.band_name_);
        int const rhsBand = sortBands.find(rhs.band_name_);
        if (lhsBand != rhsBand) return lhsBand < rhsBand;
        return lhs.band_name_ < rhs.band_name_;
    });
    if (index < 0 || index >= stations.size()) {
        emit errorMessage(QStringLiteral("Invalid station information selection"));
        return false;
    }
    QString const previousBand = stations.at(index).band_name_;
    stations[index] = StationList::Station {cleanBand, offsetHz, antennaDescription.trimmed()};
    syncSettingToLegacyIni(QStringLiteral("stations"), QVariant::fromValue(stations));
    StationList::Stations const persistedStations =
        readSettingFromLegacyIni(QStringLiteral("stations")).value<StationList::Stations>();
    if (persistedStations != stations) {
        decodiumBridgeLogbookLog(QStringLiteral("Station information update verification failed for band=%1 offset=%2 Hz")
                      .arg(cleanBand, QString::number(offsetHz)));
        emit errorMessage(QStringLiteral("Station information could not be saved; check that the Decodium settings file is writable"));
        return false;
    }
    emit settingValueChanged(QStringLiteral("stations"), QVariant::fromValue(stations));
    if (m_currentCatLogicalBand.compare(previousBand, Qt::CaseInsensitive) == 0
        || m_currentCatLogicalBand.compare(cleanBand, Qt::CaseInsensitive) == 0) {
        m_currentCatStationOffsetHz = static_cast<Radio::FrequencyDelta>(
            std::llround(stationOffsetForFrequencyHz(m_frequency)));
    }
    emit statusMessage(QStringLiteral("Station information updated for %1: offset %2 MHz")
                           .arg(cleanBand,
                                QString::number(static_cast<double>(offsetHz) / 1000000.0, 'f', 6)));
    return true;
}

bool DecodiumBridge::deleteStationFrequencyRow(int index)
{
    QVariant const raw = readSettingFromLegacyIni(QStringLiteral("stations"));
    StationList::Stations stations = raw.isValid() ? raw.value<StationList::Stations>() : StationList::Stations {};
    Bands bands;
    std::stable_sort(stations.begin(), stations.end(), [&bands](StationList::Station const& lhs,
                                                                StationList::Station const& rhs) {
        int const lhsBand = bands.find(lhs.band_name_);
        int const rhsBand = bands.find(rhs.band_name_);
        if (lhsBand != rhsBand) return lhsBand < rhsBand;
        return lhs.band_name_ < rhs.band_name_;
    });
    if (index < 0 || index >= stations.size()) {
        emit errorMessage(QStringLiteral("Invalid station information selection"));
        return false;
    }
    QString const removedBand = stations.at(index).band_name_;
    stations.removeAt(index);
    syncSettingToLegacyIni(QStringLiteral("stations"), QVariant::fromValue(stations));
    StationList::Stations const persistedStations =
        readSettingFromLegacyIni(QStringLiteral("stations")).value<StationList::Stations>();
    if (persistedStations != stations) {
        decodiumBridgeLogbookLog(QStringLiteral("Station information delete verification failed for band=%1")
                      .arg(removedBand));
        emit errorMessage(QStringLiteral("Station information could not be deleted; check that the Decodium settings file is writable"));
        return false;
    }
    emit settingValueChanged(QStringLiteral("stations"), QVariant::fromValue(stations));
    if (m_currentCatLogicalBand.compare(removedBand, Qt::CaseInsensitive) == 0) {
        m_currentCatStationOffsetHz = static_cast<Radio::FrequencyDelta>(
            std::llround(stationOffsetForFrequencyHz(m_frequency)));
    }
    emit statusMessage(QStringLiteral("Station information deleted for %1").arg(removedBand));
    return true;
}

