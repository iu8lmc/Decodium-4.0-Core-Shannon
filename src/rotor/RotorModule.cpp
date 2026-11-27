#include "RotorModule.h"

#include <QDir>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QSerialPortInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpSocket>

#include <algorithm>
#include <cmath>

namespace decodium::rotor {

namespace {

constexpr int kUiRefreshMs = 100;
constexpr int kDiagnosticsRefreshMs = 400;

QVariantMap idleState()
{
    return {{QStringLiteral("type"), QStringLiteral("state")},
            {QStringLiteral("connected"), false},
            {QStringLiteral("model_label"), QString()},
            {QStringLiteral("has_el"), false},
            {QStringLiteral("moving"), false},
            {QStringLiteral("clients"), 0}};
}

QString firstLocalAddress()
{
    for (const QHostAddress& a : QNetworkInterface::allAddresses()) {
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback()
            && !a.isLinkLocal())
            return a.toString();
    }
    return QStringLiteral("127.0.0.1");
}


}  // namespace

RotorModule::RotorModule(QObject* parent)
    : QObject(parent)
    , m_state(idleState())
{
    m_ticker.setInterval(kUiRefreshMs);
    connect(&m_ticker, &QTimer::timeout, this, &RotorModule::refreshState);
    m_diagnostics.setInterval(kDiagnosticsRefreshMs);
    connect(&m_diagnostics, &QTimer::timeout, this, &RotorModule::refreshDiagnostics);
}

RotorModule::~RotorModule()
{
    shutdown();
}

void RotorModule::start(QSettings* settings)
{
    m_settings = settings;
    if (m_settings)
        m_config.load(*m_settings);
    m_spotBook.configure(m_config.spotTtl, m_config.spotLimit);
    m_presets = presetsToVariant(m_config.presets);
    refreshEndpoints();
    m_ticker.start();
    m_diagnostics.start();
    applyRuntime();
    emit settingsChanged();
    emit presetsChanged();
}

void RotorModule::shutdown()
{
    m_ticker.stop();
    m_diagnostics.stop();
    stopNetwork();
    stopControl();
}

// ── accensione e spegnimento ───────────────────────────────────────────────

void RotorModule::applyRuntime()
{
    stopNetwork();
    stopControl();
    if (!m_config.enabled) {
        m_state = idleState();
        emit stateChanged();
        return;
    }
    startControl();
    if (m_config.networkEnabled)
        startNetwork();
    refreshEndpoints();
    emit settingsChanged();
}

void RotorModule::startControl()
{
    m_controller = std::make_unique<RotorController>(m_config);
    m_controller->setStation(m_callsign, m_locator);
    m_controller->start();
    m_state = m_controller->snapshot();
}

void RotorModule::stopControl()
{
    if (!m_controller)
        return;
    // Mai lasciare il rotore in movimento senza nessuno al comando.
    if (m_config.stopOnClientLoss)
        m_controller->halt();
    m_controller->shutdown();
    m_controller.reset();
}

void RotorModule::startNetwork()
{
    if (!m_controller)
        return;
    ServerDelegate delegate;
    delegate.configPayload = [this] { return configPayload(); };
    delegate.configApply = [this](const QVariantMap& values) { return configApply(values); };
    delegate.spotsPayload = [this] { return spotsPayload(); };

    const QHostAddress bind(m_config.bind);
    const QHostAddress address = bind.isNull() ? QHostAddress(QHostAddress::AnyIPv4) : bind;
    QString error;

    m_rotctld = std::make_unique<RotctldServer>(m_controller.get());
    if (!m_rotctld->listen(address, static_cast<quint16>(m_config.rotctldPort), &error))
        emit notified(tr("rotctld port %1: %2").arg(m_config.rotctldPort).arg(error), true);

    m_ws = std::make_unique<WsServer>(m_controller.get(), delegate);
    if (!m_ws->listen(address, static_cast<quint16>(m_config.wsPort), m_config.token, &error))
        emit notified(tr("WebSocket port %1: %2").arg(m_config.wsPort).arg(error), true);

    m_tiles = std::make_unique<TileCache>(tileDirectory());
    m_tiles->configure(m_config.tileUrl, m_config.tileCacheDays, m_config.tileCacheMb);
    m_http = std::make_unique<HttpServer>(m_controller.get(), m_tiles.get(), delegate);
    if (!m_http->listen(address, static_cast<quint16>(m_config.httpPort), m_config.token,
                        m_config.wsPort, m_config.rotctldPort, &error))
        emit notified(tr("HTTP port %1: %2").arg(m_config.httpPort).arg(error), true);
}

void RotorModule::stopNetwork()
{
    if (m_http) m_http->close();
    if (m_ws) m_ws->close();
    if (m_rotctld) m_rotctld->close();
    m_http.reset();
    m_ws.reset();
    m_rotctld.reset();
    m_tiles.reset();
}

QString RotorModule::tileDirectory() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return QDir(base.isEmpty() ? QDir::tempPath() : base).absoluteFilePath(QStringLiteral("rotor-tiles"));
}

// Il tile server della mappa e' sempre quello locale: la mappa chiede i riquadri
// al nostro HTTP, che li tiene su disco. Senza server di rete la mappa resta
// vuota (solo il quadrante).
QString RotorModule::tileEndpoint() const
{
    if (!m_http || !m_http->isListening())
        return {};
    return QStringLiteral("http://127.0.0.1:%1/tiles/").arg(m_http->serverPort());
}

// ── da Decodium ────────────────────────────────────────────────────────────

void RotorModule::setStation(const QString& callsign, const QString& locator)
{
    const QString call = callsign.trimmed().toUpper();
    const QString grid = locator.trimmed().toUpper();
    if (call == m_callsign && grid == m_locator)
        return;
    m_callsign = call;
    m_locator = grid;
    m_spotBook.setStation(m_callsign, m_locator);
    if (m_controller)
        m_controller->setStation(m_callsign, m_locator);
    emit settingsChanged();
    emit bearingChanged();
}

void RotorModule::noteDecode(const QString& message, int snr, bool hasSnr, const QString& mode, qint64 dialHz)
{
    if (!m_config.enabled || !m_config.spotsEnabled)
        return;
    QString call, grid;
    if (!SpotBook::stationFromMessage(message, &call, &grid))
        return;
    m_spotBook.dialFrequency = dialHz;
    m_spotBook.note(call, grid, QStringLiteral("decodium"),
                    hasSnr ? std::optional<int>(snr) : std::nullopt, mode, dialHz);
}

void RotorModule::setWorkingCall(const QString& call)
{
    const QString c = call.trimmed().toUpper();
    if (m_spotBook.workingCall == c)
        return;
    m_spotBook.workingCall = c;
    emit spotsChanged();
}

// ── aggiornamento periodico ────────────────────────────────────────────────

void RotorModule::refreshState()
{
    const QVariantMap state = m_controller ? m_controller->snapshot() : idleState();
    // ts e uptime cambiano a ogni giro: si confronta il resto.
    auto significant = [](QVariantMap m) {
        m.remove(QStringLiteral("ts"));
        m.remove(QStringLiteral("uptime"));
        return m;
    };
    const bool changed = significant(state) != significant(m_state);
    const bool uptimeTick = state.value(QStringLiteral("uptime")).toInt() != m_state.value(QStringLiteral("uptime")).toInt();
    if (changed || uptimeTick) {
        updateSense(m_state, state);
        m_state = state;
        emit stateChanged();
    }
}

void RotorModule::updateSense(const QVariantMap& previous, const QVariantMap& current)
{
    if (!current.value(QStringLiteral("moving")).toBool()) {
        m_sense = 0;
        return;
    }
    const QVariant before = previous.value(QStringLiteral("az"));
    const QVariant now = current.value(QStringLiteral("az"));
    if (!before.isNull() && !now.isNull()) {
        const double delta = std::fmod(now.toDouble() - before.toDouble() + 540.0, 360.0) - 180.0;
        if (std::fabs(delta) >= 0.05) {
            m_sense = delta > 0 ? 1 : -1;
            return;
        }
    }
    const QVariant target = current.value(QStringLiteral("az_target"));
    if (m_sense == 0 && !target.isNull() && !now.isNull()) {
        const double gap = std::fmod(target.toDouble() - now.toDouble() + 540.0, 360.0) - 180.0;
        m_sense = gap >= 0 ? 1 : -1;
    }
}

void RotorModule::refreshDiagnostics()
{
    if (m_controller) {
        const QVariantList traffic = m_controller->recentTraffic(120);
        if (traffic != m_traffic) {
            m_traffic = traffic;
            emit trafficChanged();
        }
        const QVariantList history = m_controller->recentHistory(240);
        if (history != m_history) {
            m_history = history;
            emit historyChanged();
        }
    }
    const QVariantList before = m_endpoints;
    refreshEndpoints();
    if (before != m_endpoints)
        emit settingsChanged();
    refreshSpots();
}

void RotorModule::refreshSpots()
{
    // Gli spot scadono anche in silenzio: ogni tanto si ripassa comunque,
    // altrimenti una stazione sparita resterebbe sulla mappa.
    ++m_spotTicks;
    const int version = m_spotBook.version();
    if (version == m_spotVersion && m_spotTicks < 25)
        return;
    m_spotTicks = 0;
    m_spotVersion = version;
    const QVariantList fresh = m_spotBook.entries(60);
    if (fresh == m_spots)
        return;
    m_spots = fresh;
    emit spotsChanged();
}

void RotorModule::refreshEndpoints()
{
    const QString host = firstLocalAddress();
    m_endpoints = {
        QVariantMap{{QStringLiteral("role"), tr("app")},
                    {QStringLiteral("address"), QStringLiteral("ws://%1:%2").arg(host).arg(m_config.wsPort)},
                    {QStringLiteral("active"), m_ws && m_ws->isListening()}},
        QVariantMap{{QStringLiteral("role"), tr("web")},
                    {QStringLiteral("address"), QStringLiteral("http://%1:%2").arg(host).arg(m_config.httpPort)},
                    {QStringLiteral("active"), m_http && m_http->isListening()}},
        QVariantMap{{QStringLiteral("role"), tr("rotctld")},
                    {QStringLiteral("address"), QStringLiteral("%1:%2").arg(host).arg(m_config.rotctldPort)},
                    {QStringLiteral("active"), m_rotctld && m_rotctld->isListening()}},
    };
}

// ── proprieta derivate ─────────────────────────────────────────────────────

QStringList RotorModule::serialPorts() const
{
    QStringList names;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts())
        names << info.portName();
    names.sort();
    return names;
}

void RotorModule::refreshSerialPorts()
{
    emit settingsChanged();
}

QVariantList RotorModule::modelChoices() const
{
    QVariantList list;
    list.append(QVariantMap{{QStringLiteral("key"), QStringLiteral("auto")}, {QStringLiteral("label"), tr("Auto-detect")}});
    for (const Model& m : models())
        list.append(QVariantMap{{QStringLiteral("key"), m.key}, {QStringLiteral("label"), m.label}});
    return list;
}

bool RotorModule::hasElevation() const
{
    return m_state.value(QStringLiteral("has_el")).toBool() && !m_state.value(QStringLiteral("el")).isNull();
}

double RotorModule::azimuthTarget() const
{
    const QVariant t = m_state.value(QStringLiteral("az_target"));
    return t.isNull() ? -1.0 : t.toDouble();
}

double RotorModule::elevationTarget() const
{
    const QVariant t = m_state.value(QStringLiteral("el_target"));
    return t.isNull() ? -1.0 : t.toDouble();
}

QString RotorModule::uptimeText() const
{
    const int seconds = m_state.value(QStringLiteral("uptime")).toInt();
    return QStringLiteral("%1:%2:%3").arg(seconds / 3600)
        .arg((seconds % 3600) / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

double RotorModule::myLatitude() const
{
    Position p;
    return locatorToPosition(m_locator, &p) ? p.latitude : 0.0;
}

double RotorModule::myLongitude() const
{
    Position p;
    return locatorToPosition(m_locator, &p) ? p.longitude : 0.0;
}

// ── comandi ────────────────────────────────────────────────────────────────

bool RotorModule::guard(const std::function<void()>& action)
{
    if (!m_controller) {
        emit notified(tr("Rotator is off: enable it in the Settings tab."), true);
        return false;
    }
    try {
        action();
    } catch (const RotorError& e) {
        emit notified(e.message(), true);
        return false;
    }
    return true;
}

void RotorModule::gotoAzimuth(double degrees)
{
    guard([&] { m_controller->goTo(degrees, std::nullopt); });
}

void RotorModule::gotoPosition(double az, double el)
{
    if (az < 0 && el < 0) {
        emit notified(tr("Enter at least one angle."), true);
        return;
    }
    guard([&] {
        m_controller->goTo(az < 0 ? std::nullopt : std::optional<double>(az),
                           el < 0 ? std::nullopt : std::optional<double>(el));
    });
}

void RotorModule::jog(double delta)
{
    if (m_state.value(QStringLiteral("az")).isNull())
        return;
    guard([&] { m_controller->goTo(std::fmod(azimuth() + delta + 720.0, 360.0), std::nullopt); });
}

void RotorModule::stop(bool fast)
{
    if (!m_controller)
        return;
    m_controller->halt(QStringLiteral("all"), fast);
    emit notified(tr("Stop sent to the control box."), false);
}

void RotorModule::park()
{
    guard([&] { m_controller->park(); });
}

void RotorModule::computeBearing(const QString& locator)
{
    Position origin;
    Position target;
    m_bearingValid = false;
    if (locatorToPosition(m_locator, &origin) && locatorToPosition(locator, &target)) {
        m_bearing = bearingBetween(origin, target).toMap();
        m_bearingValid = true;
    }
    emit bearingChanged();
}

void RotorModule::gotoLocator(const QString& locator, bool longPath)
{
    QVariantMap applied;
    if (!guard([&] { applied = m_controller->goToLocator(locator, longPath); }))
        return;
    emit notified(tr("Towards %1: %2°").arg(locator.toUpper())
                      .arg(applied.value(QStringLiteral("az")).toDouble(), 0, 'f', 1), false);
}

// ── memorie ────────────────────────────────────────────────────────────────

void RotorModule::persist()
{
    if (!m_settings)
        return;
    m_config.save(*m_settings);
    m_settings->sync();
}

void RotorModule::savePresetHere(const QString& name)
{
    std::optional<double> el;
    if (m_state.value(QStringLiteral("has_el")).toBool() && !m_state.value(QStringLiteral("el")).isNull())
        el = elevation();
    if (!guard([&] { m_presets = m_controller->savePreset(name, azimuth(), el); }))
        return;
    m_config.presets = m_controller->config().presets;
    persist();
    emit presetsChanged();
}

void RotorModule::deletePreset(const QString& name)
{
    if (!guard([&] { m_presets = m_controller->deletePreset(name); }))
        return;
    m_config.presets = m_controller->config().presets;
    persist();
    emit presetsChanged();
}

void RotorModule::recallPreset(const QString& name)
{
    guard([&] { m_controller->recallPreset(name); });
}

// ── stazioni sentite ───────────────────────────────────────────────────────

void RotorModule::pointAtSpot(const QString& call)
{
    const QVariantMap spot = m_spotBook.find(call);
    if (spot.isEmpty()) {
        emit notified(tr("Station no longer in the list."), true);
        return;
    }
    if (!guard([&] { m_controller->goTo(spot.value(QStringLiteral("az")).toDouble(), std::nullopt); }))
        return;
    emit notified(tr("Towards %1: %2° at %3 km").arg(spot.value(QStringLiteral("call")).toString())
                      .arg(spot.value(QStringLiteral("az")).toDouble(), 0, 'f', 1)
                      .arg(spot.value(QStringLiteral("km")).toLongLong()), false);
}

void RotorModule::savePresetFromSpot(const QString& call)
{
    const QVariantMap spot = m_spotBook.find(call);
    if (spot.isEmpty())
        return;
    if (!guard([&] {
            m_presets = m_controller->savePreset(spot.value(QStringLiteral("call")).toString(),
                                                 spot.value(QStringLiteral("az")).toDouble(), std::nullopt);
        }))
        return;
    m_config.presets = m_controller->config().presets;
    persist();
    emit presetsChanged();
}

void RotorModule::clearSpots()
{
    m_spotBook.clear();
    refreshSpots();
}

// ── impostazioni ───────────────────────────────────────────────────────────

void RotorModule::setSetting(const QString& field, const QVariant& value)
{
    bool restart = false;
    if (field == QLatin1String("enabled")) { m_config.enabled = value.toBool(); restart = true; }
    else if (field == QLatin1String("simulate")) { m_config.simulate = value.toBool(); restart = true; }
    else if (field == QLatin1String("network_enabled")) { m_config.networkEnabled = value.toBool(); restart = true; }
    else if (field == QLatin1String("port")) { m_config.port = value.toString(); restart = true; }
    else if (field == QLatin1String("model")) { m_config.model = value.toString(); restart = true; }
    else if (field == QLatin1String("token")) { m_config.token = value.toString().trimmed(); restart = true; }
    else if (field == QLatin1String("beamwidth")) m_config.beamwidth = std::clamp(value.toDouble(), 5.0, 180.0);
    else if (field == QLatin1String("park_az")) m_config.parkAz = value.toDouble();
    else if (field == QLatin1String("park_el")) m_config.parkEl = value.toDouble();
    else if (field == QLatin1String("stop_on_client_loss")) m_config.stopOnClientLoss = value.toBool();
    else if (field == QLatin1String("dark_theme")) m_config.darkTheme = value.toBool();
    else if (field == QLatin1String("spots_enabled")) m_config.spotsEnabled = value.toBool();
    else return;

    if (m_controller && !restart)
        m_controller->applyConfig(m_config);
    persist();
    if (restart)
        applyRuntime();
    emit settingsChanged();
}

void RotorModule::setLimit(const QString& field, double value)
{
    if (field == QLatin1String("az_min")) m_config.limits.azMin = value;
    else if (field == QLatin1String("az_max")) m_config.limits.azMax = value;
    else if (field == QLatin1String("el_min")) m_config.limits.elMin = value;
    else if (field == QLatin1String("el_max")) m_config.limits.elMax = value;
    else return;
    if (m_controller)
        m_controller->applyConfig(m_config);
    persist();
    emit settingsChanged();
}

// Quello che le app possono leggere e cambiare dalla rete.
QVariantMap RotorModule::configPayload() const
{
    return {
        {QStringLiteral("beamwidth"), m_config.beamwidth},
        {QStringLiteral("park_az"), m_config.parkAz},
        {QStringLiteral("park_el"), m_config.parkEl},
        {QStringLiteral("stop_on_client_loss"), m_config.stopOnClientLoss},
        {QStringLiteral("az_min"), m_config.limits.azMin},
        {QStringLiteral("az_max"), m_config.limits.azMax},
        {QStringLiteral("el_min"), m_config.limits.elMin},
        {QStringLiteral("el_max"), m_config.limits.elMax},
        {QStringLiteral("callsign"), m_callsign},
        {QStringLiteral("my_locator"), m_locator},
    };
}

QVariantMap RotorModule::configApply(const QVariantMap& values)
{
    static const QStringList settable {QStringLiteral("beamwidth"), QStringLiteral("park_az"),
                                       QStringLiteral("park_el"), QStringLiteral("stop_on_client_loss")};
    static const QStringList limits {QStringLiteral("az_min"), QStringLiteral("az_max"),
                                     QStringLiteral("el_min"), QStringLiteral("el_max")};
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (settable.contains(it.key()))
            setSetting(it.key(), it.value());
        else if (limits.contains(it.key()))
            setLimit(it.key(), it.value().toDouble());
    }
    return configPayload();
}

QVariantMap RotorModule::spotsPayload()
{
    return {{QStringLiteral("spots"), m_spotBook.entries(120)},
            {QStringLiteral("working"), m_spotBook.workingCall}};
}

}  // namespace decodium::rotor
