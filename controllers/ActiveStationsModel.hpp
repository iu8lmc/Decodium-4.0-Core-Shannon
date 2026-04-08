#ifndef ACTIVESTATIONSMODEL_HPP
#define ACTIVESTATIONSMODEL_HPP

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>
#include <QTimer>
#include <QVector>

struct StationEntry {
    QString callsign;
    int frequency = 0;
    int snr = 0;
    QString grid;
    QString lastUtc;
    int age = 0;          // periods since last heard
    bool isWanted = false;
    bool isNewDxcc = false;
    bool isNewGrid = false;
    bool isLotwUser = false;
    int priority = 0;     // scoring: newDXCC=3, newGrid=2, wanted=1, b4=0
};

class ActiveStationsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int expiryPeriods READ expiryPeriods WRITE setExpiryPeriods NOTIFY expiryPeriodsChanged)
    Q_PROPERTY(bool filterCqOnly READ filterCqOnly WRITE setFilterCqOnly NOTIFY filterCqOnlyChanged)
    Q_PROPERTY(bool filterWantedOnly READ filterWantedOnly WRITE setFilterWantedOnly NOTIFY filterWantedOnlyChanged)

public:
    enum Roles {
        CallsignRole = Qt::UserRole + 1,
        FrequencyRole,
        SnrRole,
        GridRole,
        LastUtcRole,
        AgeRole,
        IsWantedRole,
        IsNewDxccRole,
        IsNewGridRole,
        IsLotwUserRole,
        PriorityRole
    };

    explicit ActiveStationsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_stations.size(); }
    int expiryPeriods() const { return m_expiryPeriods; }
    void setExpiryPeriods(int v);
    bool filterCqOnly() const { return m_filterCqOnly; }
    void setFilterCqOnly(bool v);
    bool filterWantedOnly() const { return m_filterWantedOnly; }
    void setFilterWantedOnly(bool v);

    Q_INVOKABLE void addStation(const QString &callsign, int freq, int snr,
                                 const QString &grid, const QString &utc);
    Q_INVOKABLE void ageAllStations();
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString callsignAt(int row) const;
    Q_INVOKABLE int frequencyAt(int row) const;

    // Scoring hooks (called from main.cpp wiring)
    void setWantedCallsigns(const QStringList &calls);
    void markNewDxcc(const QString &callsign, bool isNew);
    void markNewGrid(const QString &callsign, bool isNew);
    void markLotwUser(const QString &callsign, bool isLotw);

signals:
    void countChanged();
    void expiryPeriodsChanged();
    void filterCqOnlyChanged();
    void filterWantedOnlyChanged();

private:
    int findStation(const QString &callsign) const;
    void computePriority(StationEntry &entry);

    QVector<StationEntry> m_stations;
    int m_expiryPeriods = 10;
    bool m_filterCqOnly = false;
    bool m_filterWantedOnly = false;
    QStringList m_wantedCallsigns;
};

#endif // ACTIVESTATIONSMODEL_HPP
