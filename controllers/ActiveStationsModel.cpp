#include "controllers/ActiveStationsModel.hpp"

#include <algorithm>

ActiveStationsModel::ActiveStationsModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ActiveStationsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return visibleCount();
}

QVariant ActiveStationsModel::data(const QModelIndex& index, int role) const
{
    const StationEntry* station = stationAtVisibleIndex(index.row());
    if (!station) {
        return {};
    }

    switch (role) {
    case CallsignRole:
        return station->callsign;
    case FrequencyRole:
        return station->frequency;
    case SnrRole:
        return station->snr;
    case GridRole:
        return station->grid;
    case LastUtcRole:
        return station->lastUtc;
    case AgeRole:
        return station->age;
    case IsNewDxccRole:
        return station->isNewDxcc;
    case IsNewGridRole:
        return station->isNewGrid;
    case IsWantedRole:
        return station->isWanted;
    case IsLotwUserRole:
        return station->isLotwUser;
    default:
        return {};
    }
}

QHash<int, QByteArray> ActiveStationsModel::roleNames() const
{
    return {
        {CallsignRole, "callsign"},
        {FrequencyRole, "frequency"},
        {SnrRole, "snr"},
        {GridRole, "grid"},
        {LastUtcRole, "lastUtc"},
        {AgeRole, "age"},
        {IsNewDxccRole, "isNewDxcc"},
        {IsNewGridRole, "isNewGrid"},
        {IsWantedRole, "isWanted"},
        {IsLotwUserRole, "isLotwUser"},
    };
}

int ActiveStationsModel::count() const
{
    return visibleCount();
}

bool ActiveStationsModel::filterCqOnly() const
{
    return m_filterCqOnly;
}

void ActiveStationsModel::setFilterCqOnly(bool value)
{
    if (m_filterCqOnly == value) {
        return;
    }

    const int previousCount = count();
    m_filterCqOnly = value;
    resetModelAndNotifyCount(previousCount);
    emit filterCqOnlyChanged();
}

bool ActiveStationsModel::filterWantedOnly() const
{
    return m_filterWantedOnly;
}

void ActiveStationsModel::setFilterWantedOnly(bool value)
{
    if (m_filterWantedOnly == value) {
        return;
    }

    const int previousCount = count();
    m_filterWantedOnly = value;
    resetModelAndNotifyCount(previousCount);
    emit filterWantedOnlyChanged();
}

void ActiveStationsModel::clear()
{
    if (m_stations.isEmpty()) {
        return;
    }

    const int previousCount = count();
    beginResetModel();
    m_stations.clear();
    endResetModel();
    if (previousCount != 0) {
        emit countChanged();
    }
}

QString ActiveStationsModel::callsignAt(int index) const
{
    const StationEntry* station = stationAtVisibleIndex(index);
    return station ? station->callsign : QString{};
}

int ActiveStationsModel::frequencyAt(int index) const
{
    const StationEntry* station = stationAtVisibleIndex(index);
    return station ? station->frequency : 0;
}

void ActiveStationsModel::addStation(const QString& callsign, int frequency, int snr, const QString& grid, const QString& utc)
{
    if (callsign.isEmpty()) {
        return;
    }

    const int previousCount = count();
    beginResetModel();

    if (StationEntry* existing = findStation(callsign)) {
        existing->frequency = frequency;
        existing->snr = snr;
        existing->grid = grid;
        existing->lastUtc = utc;
        existing->age = 0;
    } else {
        StationEntry station;
        station.callsign = callsign;
        station.frequency = frequency;
        station.snr = snr;
        station.grid = grid;
        station.lastUtc = utc;
        m_stations.prepend(station);
    }

    endResetModel();
    if (previousCount != count()) {
        emit countChanged();
    }
}

void ActiveStationsModel::ageAllStations()
{
    if (m_stations.isEmpty()) {
        return;
    }

    const int previousCount = count();
    beginResetModel();

    for (StationEntry& station : m_stations) {
        ++station.age;
    }

    m_stations.erase(
        std::remove_if(m_stations.begin(), m_stations.end(), [](const StationEntry& station) {
            return station.age > 10;
        }),
        m_stations.end());

    endResetModel();
    if (previousCount != count()) {
        emit countChanged();
    }
}

bool ActiveStationsModel::matchesFilters(const StationEntry& station) const
{
    if (m_filterCqOnly && !station.isCq) {
        return false;
    }
    if (m_filterWantedOnly && !station.isWanted) {
        return false;
    }
    return true;
}

int ActiveStationsModel::visibleCount() const
{
    int total = 0;
    for (const StationEntry& station : m_stations) {
        if (matchesFilters(station)) {
            ++total;
        }
    }
    return total;
}

const ActiveStationsModel::StationEntry* ActiveStationsModel::stationAtVisibleIndex(int index) const
{
    if (index < 0) {
        return nullptr;
    }

    int visibleIndex = 0;
    for (const StationEntry& station : m_stations) {
        if (!matchesFilters(station)) {
            continue;
        }
        if (visibleIndex == index) {
            return &station;
        }
        ++visibleIndex;
    }

    return nullptr;
}

ActiveStationsModel::StationEntry* ActiveStationsModel::findStation(const QString& callsign)
{
    for (StationEntry& station : m_stations) {
        if (station.callsign.compare(callsign, Qt::CaseInsensitive) == 0) {
            return &station;
        }
    }
    return nullptr;
}

void ActiveStationsModel::resetModelAndNotifyCount(int previousCount)
{
    beginResetModel();
    endResetModel();
    if (previousCount != count()) {
        emit countChanged();
    }
}
