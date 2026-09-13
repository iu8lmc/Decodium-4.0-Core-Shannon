// DecoRTTY — registro dei collegamenti.
//
// Un decodificatore senza log è uno strumento da banco, non una stazione: i
// collegamenti vanno annotati mentre si fanno, non ricostruiti a memoria dopo.
// Il formato di uscita è ADIF, che è quello che LoTW, eQSL, Club Log e ogni
// programma di log sanno leggere.
#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QVector>

namespace decortty::app {

struct QsoRecord {
    QDateTime start;          // UTC, sempre: un log in ora locale è inutilizzabile
    QDateTime end;
    QString   call;
    QString   rstSent;
    QString   rstReceived;
    QString   name;
    QString   qth;
    QString   exchangeSent;
    QString   exchangeReceived;
    double    frequencyMhz{0.0};
    QString   mode{QStringLiteral("RTTY")};
    QString   comment;
};

class QsoLog : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString logPath READ logPath NOTIFY logPathChanged)

public:
    enum Roles {
        StartRole = Qt::UserRole + 1,
        CallRole,
        RstSentRole,
        RstReceivedRole,
        NameRole,
        FrequencyRole,
        ModeRole,
        DurationRole,
    };

    explicit QsoLog(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString logPath() const { return m_path; }

    // Registra un collegamento. Ritorna false quando manca il nominativo, che è
    // l'unico campo senza il quale la riga non ha senso.
    Q_INVOKABLE bool addQso(const QString& call, const QString& rstSent,
                            const QString& rstReceived, const QString& name,
                            const QString& qth, double frequencyMhz,
                            const QString& exchangeSent = {},
                            const QString& exchangeReceived = {},
                            const QString& comment = {});

    Q_INVOKABLE void removeQso(int row);
    Q_INVOKABLE void clear();

    // Quante volte questo nominativo è già nel log: un duplicato in contest
    // vale zero punti, e saperlo prima di rispondere fa risparmiare un minuto.
    Q_INVOKABLE int timesWorked(const QString& call) const;
    Q_INVOKABLE QString lastWorked(const QString& call) const;

    // Scrive tutto il log in ADIF. Senza percorso usa quello predefinito
    // accanto alle impostazioni.
    Q_INVOKABLE bool exportAdif(const QString& path = {}) const;
    // Carica un ADIF esistente, per riprendere un log iniziato altrove.
    Q_INVOKABLE bool importAdif(const QString& path);

    // Il log viene riscritto a ogni collegamento: un programma che si chiude
    // male non deve poter portare via la serata di lavoro.
    void setAutoSavePath(const QString& path);

signals:
    void countChanged();
    void logPathChanged();
    void errorOccurred(const QString& message);
    void qsoLogged(const QString& call);

private:
    static QString adifField(const QString& name, const QString& value);
    void autoSave();

    QVector<QsoRecord> m_records;
    QString            m_path;
};

} // namespace decortty::app
