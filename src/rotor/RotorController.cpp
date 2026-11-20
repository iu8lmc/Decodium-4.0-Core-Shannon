#include "RotorController.h"

#include <QDateTime>
#include <QMutexLocker>

#include <algorithm>
#include <cmath>

namespace decodium::rotor {

namespace {
QVariant round1(const std::optional<double>& v)
{
    if (!v)
        return QVariant();
    return std::round(*v * 10.0) / 10.0;
}
}  // namespace

class RotorController::Loop : public QThread {
public:
    explicit Loop(RotorController* owner) : m_owner(owner) {}

protected:
    void run() override { m_owner->runLoop(); }

private:
    RotorController* m_owner;
};

RotorController::RotorController(const Config& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
{
    m_clock.start();
    m_epochStartMs = QDateTime::currentMSecsSinceEpoch();
    if (const Model* model = modelByKey(m_config.model))
        m_model = *model;
}

RotorController::~RotorController()
{
    shutdown();
}

void RotorController::start()
{
    if (m_thread)
        return;
    m_running = true;
    m_thread = new Loop(this);
    m_thread->start();
}

void RotorController::shutdown()
{
    m_running = false;
    if (m_thread) {
        m_thread->wait(5000);
        delete m_thread;
        m_thread = nullptr;
    }
    m_link.reset();
}

// ── comandi ────────────────────────────────────────────────────────────────

QVariantMap RotorController::goTo(std::optional<double> az, std::optional<double> el)
{
    std::optional<double> appliedAz;
    std::optional<double> appliedEl;
    {
        QMutexLocker locker(&m_lock);
        if (az) {
            if (m_model && !m_model->hasAz())
                throw RotorError(QStringLiteral("the configured control box has no azimuth"));
            appliedAz = m_config.clampAz(*az >= 360.0 ? std::fmod(*az, 360.0) : *az);
        }
        if (el) {
            if (m_model && !m_model->hasEl())
                throw RotorError(QStringLiteral("the configured control box has no elevation"));
            appliedEl = m_config.clampEl(*el);
        }
    }
    if (!appliedAz && !appliedEl)
        throw RotorError(QStringLiteral("no axis given"));

    enqueue([this, appliedAz, appliedEl] {
        for (const QString& name : {QStringLiteral("az"), QStringLiteral("el")}) {
            const std::optional<double>& degrees = (name == QLatin1String("az")) ? appliedAz : appliedEl;
            const char id = axisId(name);
            if (!degrees || id == 0 || !m_link)
                continue;
            const QByteArray frame = gotoFrame(id, *degrees, multiplier());
            if (frame.isEmpty())
                continue;
            m_link->send(frame);
            QMutexLocker locker(&m_lock);
            AxisState& state = (name == QLatin1String("az")) ? m_az : m_el;
            state.target = *degrees;
            state.lastChangeMs = nowMs();
        }
    });

    QVariantMap applied;
    if (appliedAz)
        applied.insert(QStringLiteral("az"), *appliedAz);
    if (appliedEl)
        applied.insert(QStringLiteral("el"), *appliedEl);
    return applied;
}

void RotorController::halt(const QString& axis, bool fast)
{
    enqueue([this, axis, fast] {
        const QStringList names = (axis == QLatin1String("all"))
            ? QStringList{QStringLiteral("az"), QStringLiteral("el")}
            : QStringList{axis};
        for (const QString& name : names) {
            const char id = axisId(name);
            if (id == 0 || !m_link)
                continue;
            m_link->send(stopFrame(id, fast, multiplier()));
            QMutexLocker locker(&m_lock);
            (name == QLatin1String("az") ? m_az : m_el).target.reset();
        }
    });
}

QVariantMap RotorController::park()
{
    std::optional<double> az;
    std::optional<double> el;
    {
        QMutexLocker locker(&m_lock);
        if (!m_model || m_model->hasAz())
            az = m_config.parkAz;
        if (m_model && m_model->hasEl())
            el = m_config.parkEl;
    }
    return goTo(az, el);
}

void RotorController::clientAttached()
{
    QMutexLocker locker(&m_lock);
    ++m_clients;
}

void RotorController::clientDetached()
{
    bool orphaned;
    bool moving;
    bool stopOnLoss;
    {
        QMutexLocker locker(&m_lock);
        m_clients = std::max(0, m_clients - 1);
        orphaned = (m_clients == 0);
        moving = m_az.target.has_value() || m_el.target.has_value();
        stopOnLoss = m_config.stopOnClientLoss;
    }
    // Nessun client durante un movimento: stop di sicurezza.
    if (orphaned && moving && stopOnLoss)
        halt();
}

// ── puntamento assistito e memorie ─────────────────────────────────────────

Bearing RotorController::bearingTo(const QString& locator) const
{
    QString mine;
    {
        QMutexLocker locker(&m_lock);
        mine = m_locator;
    }
    Bearing b;
    if (!bearingToLocator(mine, locator, &b))
        throw RotorError(QStringLiteral("invalid locator: %1").arg(locator));
    return b;
}

QVariantMap RotorController::goToLocator(const QString& locator, bool longPath)
{
    const Bearing bearing = bearingTo(locator);
    const double degrees = longPath ? bearing.longPath : bearing.shortPath;
    QVariantMap applied = goTo(std::round(degrees * 10.0) / 10.0, std::nullopt);
    applied.insert(QStringLiteral("bearing"), bearing.toMap());
    applied.insert(QStringLiteral("locator"), locator.toUpper());
    return applied;
}

QVariantList RotorController::presets() const
{
    QMutexLocker locker(&m_lock);
    return presetsToVariant(m_config.presets);
}

QVariantList RotorController::savePreset(const QString& name, double az, std::optional<double> el)
{
    const QString clean = name.trimmed();
    if (clean.isEmpty())
        throw RotorError(QStringLiteral("a memory needs a name"));
    Preset p;
    p.name = clean;
    p.az = std::round(az * 10.0) / 10.0;
    if (el)
        p.el = std::round(*el * 10.0) / 10.0;
    QMutexLocker locker(&m_lock);
    m_config.presets.removeIf([&clean](const Preset& x) { return x.name == clean; });
    m_config.presets.append(p);
    std::sort(m_config.presets.begin(), m_config.presets.end(), [](const Preset& a, const Preset& b) {
        return a.name.toLower() < b.name.toLower();
    });
    return presetsToVariant(m_config.presets);
}

QVariantList RotorController::deletePreset(const QString& name)
{
    QMutexLocker locker(&m_lock);
    m_config.presets.removeIf([&name](const Preset& x) { return x.name == name; });
    return presetsToVariant(m_config.presets);
}

QVariantMap RotorController::recallPreset(const QString& name)
{
    std::optional<Preset> found;
    {
        QMutexLocker locker(&m_lock);
        for (const Preset& p : m_config.presets) {
            if (p.name == name) {
                found = p;
                break;
            }
        }
    }
    if (!found)
        throw RotorError(QStringLiteral("unknown memory: %1").arg(name));
    return goTo(found->az, found->el);
}

// ── stazione e configurazione ──────────────────────────────────────────────

void RotorController::setStation(const QString& callsign, const QString& locator)
{
    QMutexLocker locker(&m_lock);
    m_callsign = callsign;
    m_locator = locator;
}

QString RotorController::locator() const
{
    QMutexLocker locker(&m_lock);
    return m_locator;
}

Config RotorController::config() const
{
    QMutexLocker locker(&m_lock);
    return m_config;
}

void RotorController::applyConfig(const Config& config)
{
    QMutexLocker locker(&m_lock);
    m_config = config;
}

// ── diagnostica ────────────────────────────────────────────────────────────

void RotorController::recordFrame(const QString& direction, const QByteArray& frame)
{
    QByteArray printable = frame;
    printable.replace(QByteArray(1, kStx), "<STX>");
    printable.replace(QByteArray(1, kCr), "<CR>");
    QVariantMap entry;
    entry.insert(QStringLiteral("ts"), QDateTime::currentMSecsSinceEpoch() / 1000.0);
    entry.insert(QStringLiteral("dir"), direction);
    entry.insert(QStringLiteral("frame"), QString::fromLatin1(printable));
    entry.insert(QStringLiteral("hex"), QString::fromLatin1(frame.toHex(' ')));
    QMutexLocker locker(&m_lock);
    if (direction == QLatin1String("tx"))
        ++m_txFrames;
    else
        ++m_rxFrames;
    m_traffic.append(entry);
    while (m_traffic.size() > m_config.trafficSize)
        m_traffic.removeFirst();
}

QVariantList RotorController::recentTraffic(int limit) const
{
    QMutexLocker locker(&m_lock);
    QVariantList out;
    const int from = std::max(0, static_cast<int>(m_traffic.size()) - std::max(0, limit));
    for (int i = from; i < m_traffic.size(); ++i)
        out.append(m_traffic.at(i));
    return out;
}

QVariantList RotorController::recentHistory(int limit) const
{
    QMutexLocker locker(&m_lock);
    QVariantList out;
    const int from = std::max(0, static_cast<int>(m_history.size()) - std::max(0, limit));
    for (int i = from; i < m_history.size(); ++i)
        out.append(m_history.at(i));
    return out;
}

void RotorController::recordHistory()
{
    QMutexLocker locker(&m_lock);
    if (!m_az.position && !m_el.position)
        return;
    QVariantMap entry;
    entry.insert(QStringLiteral("ts"), QDateTime::currentMSecsSinceEpoch() / 1000.0);
    entry.insert(QStringLiteral("az"), round1(m_az.position));
    entry.insert(QStringLiteral("el"), round1(m_el.position));
    m_history.append(entry);
    while (m_history.size() > m_config.historySize)
        m_history.removeFirst();
}

QVariantMap RotorController::snapshot() const
{
    QMutexLocker locker(&m_lock);
    QVariantMap s;
    s.insert(QStringLiteral("type"), QStringLiteral("state"));
    s.insert(QStringLiteral("connected"), m_connected);
    s.insert(QStringLiteral("port"), m_config.simulate ? QStringLiteral("SIMULATED") : m_config.port);
    s.insert(QStringLiteral("model"), m_model ? m_model->key : QStringLiteral("auto"));
    s.insert(QStringLiteral("model_label"), m_model ? m_model->label : QStringLiteral("detecting"));
    s.insert(QStringLiteral("has_az"), m_model ? m_model->hasAz() : true);
    s.insert(QStringLiteral("has_el"), m_model ? m_model->hasEl() : false);
    s.insert(QStringLiteral("az"), round1(m_az.position));
    s.insert(QStringLiteral("az_target"), round1(m_az.target));
    s.insert(QStringLiteral("az_moving"), m_az.moving);
    s.insert(QStringLiteral("el"), round1(m_el.position));
    s.insert(QStringLiteral("el_target"), round1(m_el.target));
    s.insert(QStringLiteral("el_moving"), m_el.moving);
    s.insert(QStringLiteral("moving"), m_az.moving || m_el.moving);
    s.insert(QStringLiteral("error"), m_error.isEmpty() ? QVariant() : QVariant(m_error));
    s.insert(QStringLiteral("locator"), m_locator);
    s.insert(QStringLiteral("callsign"), m_callsign);
    s.insert(QStringLiteral("beamwidth"), m_config.beamwidth);
    s.insert(QStringLiteral("clients"), m_clients);
    s.insert(QStringLiteral("tx_frames"), m_txFrames);
    s.insert(QStringLiteral("rx_frames"), m_rxFrames);
    s.insert(QStringLiteral("errors"), m_errors);
    s.insert(QStringLiteral("reconnects"), m_reconnects);
    s.insert(QStringLiteral("uptime"), (QDateTime::currentMSecsSinceEpoch() - m_epochStartMs) / 1000.0);
    s.insert(QStringLiteral("ts"), QDateTime::currentMSecsSinceEpoch() / 1000.0);
    return s;
}

// ── interni ────────────────────────────────────────────────────────────────

void RotorController::enqueue(std::function<void()> action)
{
    QMutexLocker locker(&m_lock);
    m_commands.append(std::move(action));
}

int RotorController::multiplier() const
{
    QMutexLocker locker(&m_lock);
    return m_model ? m_model->multiplier : 1;
}

char RotorController::axisId(const QString& name) const
{
    QMutexLocker locker(&m_lock);
    if (!m_model)
        return 0;
    return name == QLatin1String("az") ? m_model->azId : m_model->elId;
}

// Interroga gli ID noti e deduce il control box collegato. La presenza dell'ID
// 'B' identifica un Combi-Track (angoli per dieci); altrimenti e' un control
// box D.
const Model* RotorController::detectModel()
{
    bool found[3] = {false, false, false};
    const char ids[3] = {kIdAzimuth, kIdElevation, kIdSecondUnit};
    for (int i = 0; i < 3; ++i) {
        Reply reply;
        found[i] = m_link->transact(queryPosition(ids[i]), 1, &reply);
    }
    const bool hasA = found[0], hasE = found[1], hasB = found[2];
    if (hasB)
        return modelByKey(QStringLiteral("combi"));
    if (hasA && hasE)
        return modelByKey(QStringLiteral("d_azel"));
    if (hasE)
        return modelByKey(QStringLiteral("d_el"));
    return modelByKey(QStringLiteral("d_az"));   // ripiego: l'azimut e' la piu' diffusa
}

bool RotorController::openLink()
{
    try {
        Config cfg;
        {
            QMutexLocker locker(&m_lock);
            cfg = m_config;
        }
        std::unique_ptr<Transport> transport;
        if (cfg.simulate) {
            const Model* wanted = modelByKey(cfg.model);
            transport = std::make_unique<SimulatedTransport>(wanted ? wanted->key : QStringLiteral("d_azel"));
        } else {
            if (cfg.port.isEmpty())
                throw TransportError(QStringLiteral("no serial port selected"));
            transport = std::make_unique<SerialTransport>(cfg.port, cfg.baudrate, cfg.timeout);
        }
        m_link = std::make_unique<Link>(std::move(transport), cfg.retries,
                                        [this](const QString& d, const QByteArray& f) { recordFrame(d, f); });
        m_link->open();
        bool needDetect;
        {
            QMutexLocker locker(&m_lock);
            needDetect = !m_model.has_value();
        }
        if (needDetect) {
            if (const Model* model = detectModel()) {
                QMutexLocker locker(&m_lock);
                m_model = *model;
            }
        }
        Model model;
        {
            QMutexLocker locker(&m_lock);
            model = *m_model;
        }
        for (char id : {model.azId, model.elId}) {
            if (id != 0)
                m_link->send(disableCpm(id));
        }
        QMutexLocker locker(&m_lock);
        m_connected = true;
        m_error.clear();
        return true;
    } catch (const TransportError& e) {
        QMutexLocker locker(&m_lock);
        m_connected = false;
        m_error = QString::fromUtf8(e.what());
        return false;
    }
}

void RotorController::pollAxis(const QString& name, AxisState& state)
{
    const char id = axisId(name);
    if (id == 0)
        return;
    Reply reply;
    if (!m_link->transact(queryPosition(id), multiplier(), &reply)) {
        QMutexLocker locker(&m_lock);
        ++m_errors;
        return;
    }
    bool stopFast = false;
    {
        QMutexLocker locker(&m_lock);
        const std::optional<double> previous = state.position;
        state.position = reply.value;
        if (!previous || std::abs(reply.value - *previous) > 0.2)
            state.lastChangeMs = nowMs();

        if (!state.target) {
            state.moving = reply.isMoving();
            return;
        }
        const bool reached = std::abs(reply.value - *state.target) <= m_config.tolerance;
        const bool stalled = (nowMs() - state.lastChangeMs) > static_cast<qint64>(m_config.stallTimeout * 1000.0);
        if (reached) {
            state.target.reset();
            state.moving = false;
        } else if (stalled) {
            state.target.reset();
            state.moving = false;
            m_error = QStringLiteral("%1: no movement within %2 s, safety stop")
                          .arg(name).arg(m_config.stallTimeout, 0, 'f', 0);
            stopFast = true;
        } else {
            state.moving = true;
        }
    }
    if (stopFast)
        m_link->send(stopFrame(id, true, multiplier()));
}

void RotorController::runLoop()
{
    double backoff = 1.0;
    auto pause = [this](double seconds) {
        qint64 remaining = static_cast<qint64>(seconds * 1000.0);
        while (remaining > 0 && m_running) {
            const qint64 slice = std::min<qint64>(remaining, 50);
            QThread::msleep(static_cast<unsigned long>(slice));
            remaining -= slice;
        }
    };
    while (m_running) {
        bool connected;
        {
            QMutexLocker locker(&m_lock);
            connected = m_connected;
        }
        if (!m_link || !connected) {
            if (!openLink()) {
                emit stateUpdated();
                pause(backoff);
                backoff = std::min(backoff * 2.0, 10.0);
                continue;
            }
            backoff = 1.0;
        }
        try {
            QList<std::function<void()>> pending;
            {
                QMutexLocker locker(&m_lock);
                pending.swap(m_commands);
            }
            for (const auto& action : pending)
                action();
            pollAxis(QStringLiteral("az"), m_az);
            pollAxis(QStringLiteral("el"), m_el);
        } catch (const TransportError& e) {
            {
                QMutexLocker locker(&m_lock);
                m_connected = false;
                m_error = QString::fromUtf8(e.what());
                ++m_reconnects;
            }
            if (m_link)
                m_link->close();
        }
        recordHistory();
        emit stateUpdated();
        double poll;
        {
            QMutexLocker locker(&m_lock);
            poll = m_config.pollInterval;
        }
        pause(poll);
    }
    if (m_link)
        m_link->close();
}

}  // namespace decodium::rotor
