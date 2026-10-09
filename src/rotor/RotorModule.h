// Il rotore come lo vede la finestra QML: proprieta e comandi su un solo oggetto
// ("rotor" nel contesto QML). Possiede il controllo, i tre server di rete e il
// libro delle stazioni sentite; tutto resta spento finche l'operatore non lo
// accende dalle impostazioni.
#pragma once

#include "RotorConfig.h"
#include "RotorController.h"
#include "RotorServers.h"
#include "RotorSpots.h"

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <memory>

class QSettings;

namespace decodium::rotor {

class RotorModule : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool enabled READ enabled NOTIFY settingsChanged)
    Q_PROPERTY(bool simulate READ simulate NOTIFY settingsChanged)
    Q_PROPERTY(bool networkEnabled READ networkEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QString serialPort READ serialPort NOTIFY settingsChanged)
    Q_PROPERTY(QStringList serialPorts READ serialPorts NOTIFY settingsChanged)
    Q_PROPERTY(QString modelKey READ modelKey NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList modelChoices READ modelChoices CONSTANT)
    Q_PROPERTY(QString token READ token NOTIFY settingsChanged)

    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(bool hasPosition READ hasPosition NOTIFY stateChanged)
    Q_PROPERTY(bool hasElevation READ hasElevation NOTIFY stateChanged)
    Q_PROPERTY(double azimuth READ azimuth NOTIFY stateChanged)
    Q_PROPERTY(double azimuthTarget READ azimuthTarget NOTIFY stateChanged)
    Q_PROPERTY(double elevation READ elevation NOTIFY stateChanged)
    Q_PROPERTY(double elevationTarget READ elevationTarget NOTIFY stateChanged)
    Q_PROPERTY(bool moving READ moving NOTIFY stateChanged)
    Q_PROPERTY(int rotationSense READ rotationSense NOTIFY stateChanged)
    Q_PROPERTY(int clients READ clients NOTIFY stateChanged)
    Q_PROPERTY(QString modelLabel READ modelLabel NOTIFY stateChanged)
    Q_PROPERTY(QString port READ port NOTIFY stateChanged)
    Q_PROPERTY(int baudrate READ baudrate NOTIFY settingsChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(int txFrames READ txFrames NOTIFY stateChanged)
    Q_PROPERTY(int rxFrames READ rxFrames NOTIFY stateChanged)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY stateChanged)
    Q_PROPERTY(int reconnects READ reconnects NOTIFY stateChanged)
    Q_PROPERTY(QString uptimeText READ uptimeText NOTIFY stateChanged)
    Q_PROPERTY(QVariantList endpoints READ endpoints NOTIFY settingsChanged)
    Q_PROPERTY(bool tokenRequired READ tokenRequired NOTIFY settingsChanged)

    Q_PROPERTY(QString callsign READ callsign NOTIFY settingsChanged)
    Q_PROPERTY(QString locator READ locator NOTIFY settingsChanged)
    Q_PROPERTY(QString decodiumStation READ decodiumStation NOTIFY settingsChanged)
    Q_PROPERTY(double myLatitude READ myLatitude NOTIFY settingsChanged)
    Q_PROPERTY(double myLongitude READ myLongitude NOTIFY settingsChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme NOTIFY settingsChanged)
    Q_PROPERTY(double beamwidth READ beamwidth NOTIFY settingsChanged)
    Q_PROPERTY(double parkAz READ parkAz NOTIFY settingsChanged)
    Q_PROPERTY(double azMin READ azMin NOTIFY settingsChanged)
    Q_PROPERTY(double azMax READ azMax NOTIFY settingsChanged)
    Q_PROPERTY(bool stopOnClientLoss READ stopOnClientLoss NOTIFY settingsChanged)
    Q_PROPERTY(QString wsEndpoint READ wsEndpoint NOTIFY settingsChanged)
    Q_PROPERTY(QString httpEndpoint READ httpEndpoint NOTIFY settingsChanged)
    Q_PROPERTY(QString rotctldEndpoint READ rotctldEndpoint NOTIFY settingsChanged)

    Q_PROPERTY(QVariantList presets READ presets NOTIFY presetsChanged)
    Q_PROPERTY(QVariantList traffic READ traffic NOTIFY trafficChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QVariantList spots READ spots NOTIFY spotsChanged)
    Q_PROPERTY(int spotCount READ spotCount NOTIFY spotsChanged)
    Q_PROPERTY(bool spotsEnabled READ spotsEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QString workingCall READ workingCall NOTIFY spotsChanged)
    Q_PROPERTY(QString tileEndpoint READ tileEndpoint NOTIFY settingsChanged)
    Q_PROPERTY(QString tileAttribution READ tileAttribution CONSTANT)

    Q_PROPERTY(bool bearingValid READ bearingValid NOTIFY bearingChanged)
    Q_PROPERTY(double shortPath READ shortPath NOTIFY bearingChanged)
    Q_PROPERTY(double longPath READ longPath NOTIFY bearingChanged)
    Q_PROPERTY(double distanceKm READ distanceKm NOTIFY bearingChanged)
    Q_PROPERTY(double bearingLatitude READ bearingLatitude NOTIFY bearingChanged)
    Q_PROPERTY(double bearingLongitude READ bearingLongitude NOTIFY bearingChanged)

public:
    explicit RotorModule(QObject* parent = nullptr);
    ~RotorModule() override;

    // Legge la configurazione e, se abilitato, avvia controllo e server.
    void start(QSettings* settings);
    void shutdown();

    // ── da Decodium ──
    void setStation(const QString& callsign, const QString& locator);
    void noteDecode(const QString& message, int snr, bool hasSnr, const QString& mode, qint64 dialHz);
    void setWorkingCall(const QString& call);

    // ── proprieta ──
    bool enabled() const { return m_config.enabled; }
    bool simulate() const { return m_config.simulate; }
    bool networkEnabled() const { return m_config.networkEnabled; }
    QString serialPort() const { return m_config.port; }
    QStringList serialPorts() const;
    QString modelKey() const { return m_config.model; }
    QVariantList modelChoices() const;
    QString token() const { return m_config.token; }

    bool connected() const { return m_state.value(QStringLiteral("connected")).toBool(); }
    bool hasPosition() const { return !m_state.value(QStringLiteral("az")).isNull(); }
    bool hasElevation() const;
    double azimuth() const { return m_state.value(QStringLiteral("az")).toDouble(); }
    double azimuthTarget() const;
    double elevation() const { return m_state.value(QStringLiteral("el")).toDouble(); }
    double elevationTarget() const;
    bool moving() const { return m_state.value(QStringLiteral("moving")).toBool(); }
    int rotationSense() const { return m_sense; }
    int clients() const { return m_state.value(QStringLiteral("clients")).toInt(); }
    QString modelLabel() const { return m_state.value(QStringLiteral("model_label")).toString(); }
    QString port() const { return m_state.value(QStringLiteral("port")).toString(); }
    int baudrate() const { return m_config.baudrate; }
    QString errorText() const { return m_state.value(QStringLiteral("error")).toString(); }
    int txFrames() const { return m_state.value(QStringLiteral("tx_frames")).toInt(); }
    int rxFrames() const { return m_state.value(QStringLiteral("rx_frames")).toInt(); }
    int errorCount() const { return m_state.value(QStringLiteral("errors")).toInt(); }
    int reconnects() const { return m_state.value(QStringLiteral("reconnects")).toInt(); }
    QString uptimeText() const;
    QVariantList endpoints() const { return m_endpoints; }
    bool tokenRequired() const { return !m_config.token.isEmpty(); }

    QString callsign() const { return m_callsign; }
    QString locator() const { return m_locator; }
    QString decodiumStation() const { return (m_callsign + QLatin1Char(' ') + m_locator).trimmed(); }
    double myLatitude() const;
    double myLongitude() const;
    bool darkTheme() const { return m_config.darkTheme; }
    double beamwidth() const { return m_config.beamwidth; }
    double parkAz() const { return m_config.parkAz; }
    double azMin() const { return m_config.limits.azMin; }
    double azMax() const { return m_config.limits.azMax; }
    bool stopOnClientLoss() const { return m_config.stopOnClientLoss; }
    QString wsEndpoint() const { return QStringLiteral(":%1").arg(m_config.wsPort); }
    QString httpEndpoint() const { return QStringLiteral(":%1").arg(m_config.httpPort); }
    QString rotctldEndpoint() const { return QStringLiteral(":%1").arg(m_config.rotctldPort); }

    QVariantList presets() const { return m_presets; }
    QVariantList traffic() const { return m_traffic; }
    QVariantList history() const { return m_history; }
    QVariantList spots() const { return m_spots; }
    int spotCount() const { return m_spots.size(); }
    bool spotsEnabled() const { return m_config.spotsEnabled; }
    QString workingCall() const { return m_spotBook.workingCall; }
    QString tileEndpoint() const;
    QString tileAttribution() const { return m_config.tileAttribution; }

    bool bearingValid() const { return m_bearingValid; }
    double shortPath() const { return m_bearing.value(QStringLiteral("short_path")).toDouble(); }
    double longPath() const { return m_bearing.value(QStringLiteral("long_path")).toDouble(); }
    double distanceKm() const { return m_bearing.value(QStringLiteral("distance_km")).toDouble(); }
    double bearingLatitude() const { return m_bearing.value(QStringLiteral("lat")).toDouble(); }
    double bearingLongitude() const { return m_bearing.value(QStringLiteral("lon")).toDouble(); }

public slots:
    // comandi
    void gotoAzimuth(double degrees);
    void gotoPosition(double az, double el);   // negativo = asse non indicato
    void jog(double delta);
    void stop(bool fast = false);
    void park();
    void computeBearing(const QString& locator);
    void gotoLocator(const QString& locator, bool longPath);
    // memorie
    void savePresetHere(const QString& name);
    void deletePreset(const QString& name);
    void recallPreset(const QString& name);
    // stazioni sentite
    void pointAtSpot(const QString& call);
    void savePresetFromSpot(const QString& call);
    void clearSpots();
    // impostazioni
    void setSetting(const QString& field, const QVariant& value);
    void setLimit(const QString& field, double value);
    void refreshSerialPorts();

signals:
    void stateChanged();
    void settingsChanged();
    void presetsChanged();
    void trafficChanged();
    void historyChanged();
    void spotsChanged();
    void bearingChanged();
    void notified(const QString& message, bool isError);

private:
    void applyRuntime();            // accende o spegne controllo e server secondo la configurazione
    void startControl();
    void stopControl();
    void startNetwork();
    void stopNetwork();
    void persist();
    void refreshState();
    void refreshDiagnostics();
    void refreshSpots();
    void refreshEndpoints();
    void updateSense(const QVariantMap& previous, const QVariantMap& current);
    bool guard(const std::function<void()>& action);
    QVariantMap configPayload() const;
    QVariantMap configApply(const QVariantMap& values);
    QVariantMap spotsPayload();
    QString tileDirectory() const;

    QSettings* m_settings {nullptr};
    Config m_config;
    QString m_callsign;
    QString m_locator;
    std::unique_ptr<RotorController> m_controller;
    std::unique_ptr<RotctldServer> m_rotctld;
    std::unique_ptr<WsServer> m_ws;
    std::unique_ptr<HttpServer> m_http;
    std::unique_ptr<TileCache> m_tiles;
    SpotBook m_spotBook;

    QVariantMap m_state;
    QVariantList m_presets;
    QVariantList m_traffic;
    QVariantList m_history;
    QVariantList m_spots;
    QVariantList m_endpoints;
    QVariantMap m_bearing;
    bool m_bearingValid {false};
    int m_sense {0};
    int m_spotVersion {-1};
    int m_spotTicks {0};
    QTimer m_ticker;
    QTimer m_diagnostics;
};

}  // namespace decodium::rotor
