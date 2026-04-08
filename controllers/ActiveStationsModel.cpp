#include "ActiveStationsModel.hpp"
#include <algorithm>

ActiveStationsModel::ActiveStationsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ActiveStationsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_stations.size();
}

QVariant ActiveStationsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_stations.size())
        return {};

    const auto &s = m_stations.at(index.row());
    switch (role) {
    case CallsignRole:    return s.callsign;
    case FrequencyRole:   return s.frequency;
    case SnrRole:         return s.snr;
    case GridRole:        return s.grid;
    case LastUtcRole:     return s.lastUtc;
    case AgeRole:         return s.age;
    case IsWantedRole:    return s.isWanted;
    case IsNewDxccRole:   return s.isNewDxcc;
    case IsNewGridRole:   return s.isNewGrid;
    case IsLotwUserRole:  return s.isLotwUser;
    case PriorityRole:    return s.priority;
    }
    return {};
}

QHash<int, QByteArray> ActiveStationsModel::roleNames() const
{
    return {
        {CallsignRole,   "callsign"},
        {FrequencyRole,  "frequency"},
        {SnrRole,        "snr"},
        {GridRole,       "grid"},
        {LastUtcRole,    "lastUtc"},
        {AgeRole,        "age"},
        {IsWantedRole,   "isWanted"},
        {IsNewDxccRole,  "isNewDxcc"},
        {IsNewGridRole,  "isNewGrid"},
        {IsLotwUserRole, "isLotwUser"},
        {PriorityRole,   "priority"},
    };
}

void ActiveStationsModel::setExpiryPeriods(int v)
{
    if (m_expiryPeriods != v) {
        m_expiryPeriods = v;
        emit expiryPeriodsChanged();
    }
}

void ActiveStationsModel::setFilterCqOnly(bool v)
{
    if (m_filterCqOnly != v) {
        m_filterCqOnly = v;
        emit filterCqOnlyChanged();
    }
}

void ActiveStationsModel::setFilterWantedOnly(bool v)
{
    if (m_filterWantedOnly != v) {
        m_filterWantedOnly = v;
        emit filterWantedOnlyChanged();
    }
}

void ActiveStationsModel::addStation(const QString &callsign, int freq, int snr,
                                      const QString &grid, const QString &utc)
{
    int idx = findStation(callsign);
    if (idx >= 0) {
        // Update existing
        auto &s = m_stations[idx];
        s.frequency = freq;
        s.snr = snr;
        if (!grid.isEmpty()) s.grid = grid;
        s.lastUtc = utc;
        s.age = 0;
        s.isWanted = m_wantedCallsigns.contains(callsign.toUpper());
        computePriority(s);
        emit dataChanged(index(idx, 0), index(idx, 0));
    } else {
        // Insert new at top (sorted by priority later)
        StationEntry entry;
        entry.callsign = callsign.toUpper();
        entry.frequency = freq;
        entry.snr = snr;
        entry.grid = grid;
        entry.lastUtc = utc;
        entry.age = 0;
        entry.isWanted = m_wantedCallsigns.contains(entry.callsign);
        computePriority(entry);

        beginInsertRows(QModelIndex(), 0, 0);
        m_stations.prepend(entry);
        endInsertRows();
        emit countChanged();
    }
}

void ActiveStationsModel::ageAllStations()
{
    for (int i = m_stations.size() - 1; i >= 0; --i) {
        m_stations[i].age++;
        if (m_stations[i].age > m_expiryPeriods) {
            beginRemoveRows(QModelIndex(), i, i);
            m_stations.removeAt(i);
            endRemoveRows();
            emit countChanged();
        } else {
            emit dataChanged(index(i, 0), index(i, 0), {AgeRole});
        }
    }
}

void ActiveStationsModel::clear()
{
    if (m_stations.isEmpty()) return;
    beginResetModel();
    m_stations.clear();
    endResetModel();
    emit countChanged();
}

QString ActiveStationsModel::callsignAt(int row) const
{
    if (row < 0 || row >= m_stations.size()) return {};
    return m_stations.at(row).callsign;
}

int ActiveStationsModel::frequencyAt(int row) const
{
    if (row < 0 || row >= m_stations.size()) return 0;
    return m_stations.at(row).frequency;
}

void ActiveStationsModel::setWantedCallsigns(const QStringList &calls)
{
    m_wantedCallsigns = calls;
    for (int i = 0; i < m_stations.size(); ++i) {
        bool wanted = m_wantedCallsigns.contains(m_stations[i].callsign);
        if (m_stations[i].isWanted != wanted) {
            m_stations[i].isWanted = wanted;
            computePriority(m_stations[i]);
            emit dataChanged(index(i, 0), index(i, 0), {IsWantedRole, PriorityRole});
        }
    }
}

void ActiveStationsModel::markNewDxcc(const QString &callsign, bool isNew)
{
    int idx = findStation(callsign.toUpper());
    if (idx >= 0 && m_stations[idx].isNewDxcc != isNew) {
        m_stations[idx].isNewDxcc = isNew;
        computePriority(m_stations[idx]);
        emit dataChanged(index(idx, 0), index(idx, 0), {IsNewDxccRole, PriorityRole});
    }
}

void ActiveStationsModel::markNewGrid(const QString &callsign, bool isNew)
{
    int idx = findStation(callsign.toUpper());
    if (idx >= 0 && m_stations[idx].isNewGrid != isNew) {
        m_stations[idx].isNewGrid = isNew;
        computePriority(m_stations[idx]);
        emit dataChanged(index(idx, 0), index(idx, 0), {IsNewGridRole, PriorityRole});
    }
}

void ActiveStationsModel::markLotwUser(const QString &callsign, bool isLotw)
{
    int idx = findStation(callsign.toUpper());
    if (idx >= 0 && m_stations[idx].isLotwUser != isLotw) {
        m_stations[idx].isLotwUser = isLotw;
        emit dataChanged(index(idx, 0), index(idx, 0), {IsLotwUserRole});
    }
}

int ActiveStationsModel::findStation(const QString &callsign) const
{
    QString upper = callsign.toUpper();
    for (int i = 0; i < m_stations.size(); ++i) {
        if (m_stations.at(i).callsign == upper)
            return i;
    }
    return -1;
}

void ActiveStationsModel::computePriority(StationEntry &entry)
{
    entry.priority = 0;
    if (entry.isNewDxcc)  entry.priority += 3;
    if (entry.isNewGrid)  entry.priority += 2;
    if (entry.isWanted)   entry.priority += 1;
}
