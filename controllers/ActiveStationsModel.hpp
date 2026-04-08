#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class ActiveStationsModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool filterCqOnly READ filterCqOnly WRITE setFilterCqOnly NOTIFY filterCqOnlyChanged)
    Q_PROPERTY(bool filterWantedOnly READ filterWantedOnly WRITE setFilterWantedOnly NOTIFY filterWantedOnlyChanged)

public:
    explicit ActiveStationsModel(QObject* parent = nullptr);

    enum Roles {
        CallsignRole = Qt::UserRole + 1,
        FrequencyRole,
        SnrRole,
        GridRole,
        LastUtcRole,
        AgeRole,
        IsNewDxccRole,
        IsNewGridRole,
        IsWantedRole,
        IsLotwUserRole
    };
    Q_ENUM(Roles)

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

    bool filterCqOnly() const;
    void setFilterCqOnly(bool value);

    bool filterWantedOnly() const;
    void setFilterWantedOnly(bool value);

    Q_INVOKABLE void clear();
    Q_INVOKABLE QString callsignAt(int index) const;
    Q_INVOKABLE int frequencyAt(int index) const;

    void addStation(const QString& callsign, int frequency, int snr, const QString& grid, const QString& utc);
    void ageAllStations();

signals:
    void countChanged();
    void filterCqOnlyChanged();
    void filterWantedOnlyChanged();

private:
    struct StationEntry {
        QString callsign;
        int frequency {0};
        int snr {0};
        QString grid;
        QString lastUtc;
        int age {0};
        bool isNewDxcc {false};
        bool isNewGrid {false};
        bool isWanted {false};
        bool isLotwUser {false};
        bool isCq {true};
    };

    bool matchesFilters(const StationEntry& station) const;
    int visibleCount() const;
    const StationEntry* stationAtVisibleIndex(int index) const;
    StationEntry* findStation(const QString& callsign);
    void resetModelAndNotifyCount(int previousCount);

    QVector<StationEntry> m_stations;
    bool m_filterCqOnly {false};
    bool m_filterWantedOnly {false};
};
