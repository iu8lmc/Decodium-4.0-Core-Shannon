#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QVariantList>

class QNetworkAccessManager;
class QNetworkReply;

class DecodiumPropagationManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY propagationChanged)
    Q_PROPERTY(bool updating READ updating NOTIFY updatingChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString updated READ updated NOTIFY propagationChanged)
    Q_PROPERTY(QString solarFlux READ solarFlux NOTIFY propagationChanged)
    Q_PROPERTY(QString aIndex READ aIndex NOTIFY propagationChanged)
    Q_PROPERTY(QString kIndex READ kIndex NOTIFY propagationChanged)
    Q_PROPERTY(QString xRay READ xRay NOTIFY propagationChanged)
    Q_PROPERTY(QString sunspots READ sunspots NOTIFY propagationChanged)
    Q_PROPERTY(QString solarWind READ solarWind NOTIFY propagationChanged)
    Q_PROPERTY(QString muf READ muf NOTIFY propagationChanged)
    Q_PROPERTY(QString geomagneticField READ geomagneticField NOTIFY propagationChanged)
    Q_PROPERTY(QString signalNoise READ signalNoise NOTIFY propagationChanged)
    Q_PROPERTY(QVariantList hfConditions READ hfConditions NOTIFY propagationChanged)
    Q_PROPERTY(QVariantList vhfConditions READ vhfConditions NOTIFY propagationChanged)
    Q_PROPERTY(QString sourceUrl READ sourceUrl CONSTANT)
    Q_PROPERTY(QString sourcePageUrl READ sourcePageUrl CONSTANT)

public:
    explicit DecodiumPropagationManager(QObject * parent = nullptr);
    ~DecodiumPropagationManager() override = default;

    bool available() const { return m_available; }
    bool updating() const { return m_updating; }
    QString statusText() const { return m_statusText; }
    QString updated() const { return m_updated; }
    QString solarFlux() const { return m_solarFlux; }
    QString aIndex() const { return m_aIndex; }
    QString kIndex() const { return m_kIndex; }
    QString xRay() const { return m_xRay; }
    QString sunspots() const { return m_sunspots; }
    QString solarWind() const { return m_solarWind; }
    QString muf() const { return m_muf; }
    QString geomagneticField() const { return m_geomagneticField; }
    QString signalNoise() const { return m_signalNoise; }
    QVariantList hfConditions() const { return m_hfConditions; }
    QVariantList vhfConditions() const { return m_vhfConditions; }
    QString sourceUrl() const;
    QString sourcePageUrl() const;

    Q_INVOKABLE void refresh();

signals:
    void propagationChanged();
    void updatingChanged();
    void statusTextChanged();

private slots:
    void onNetworkFinished(QNetworkReply * reply);

private:
    struct BandCondition
    {
        QString name;
        QString time;
        QString value;
    };

    struct VhfCondition
    {
        QString name;
        QString location;
        QString value;
    };

    struct SolarSnapshot
    {
        QString updated;
        QString solarflux;
        QString aindex;
        QString kindex;
        QString xray;
        QString sunspots;
        QString solarwind;
        QString muf;
        QString geomagfield;
        QString signalnoise;
        QVector<BandCondition> bands;
        QVector<VhfCondition> vhf;
    };

    static QString normalizeText(QString const& text);
    static QString conditionColor(QString const& text);
    static QString displayBandName(QString const& band);
    static QString displayVhfName(QString const& name, QString const& location);
    static QByteArray readReplyPayloadLimited(QNetworkReply * reply, qint64 maxBytes, QString * error = nullptr);
    static QVariantList buildHfConditions(SolarSnapshot const& snapshot);
    static QVariantList buildVhfConditions(SolarSnapshot const& snapshot);

    bool parseSolarXml(QByteArray const& xml, SolarSnapshot * out, QString * error) const;
    void applySnapshot(SolarSnapshot const& snapshot);
    void setStatusText(QString const& text);

    QTimer m_refreshTimer;
    QNetworkAccessManager * m_network {nullptr};
    bool m_requestInFlight {false};
    bool m_available {false};
    bool m_updating {false};
    QString m_statusText;
    QString m_updated;
    QString m_solarFlux;
    QString m_aIndex;
    QString m_kIndex;
    QString m_xRay;
    QString m_sunspots;
    QString m_solarWind;
    QString m_muf;
    QString m_geomagneticField;
    QString m_signalNoise;
    QVariantList m_hfConditions;
    QVariantList m_vhfConditions;
};
