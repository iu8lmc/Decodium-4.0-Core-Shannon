#include "Q65DopplerTracker.h"

#include <QRegularExpression>
#include <QTimeZone>
#include <algorithm>
#include <cmath>

extern "C" bool decodium_moon_doppler_c(int, int, int, double, double,
                                       char const*, double*, double*);

namespace {
bool lunarState(QDateTime const& time, QString const& grid, double frequency,
                double& oneWay, double& elevation)
{
    QDateTime const utc = time.toUTC();
    double const hours = utc.time().msecsSinceStartOfDay() / 3600000.0;
    return decodium_moon_doppler_c(utc.date().year(), utc.date().month(), utc.date().day(),
                                  hours, frequency, grid.trimmed().toUpper().toLatin1().constData(),
                                  &oneWay, &elevation);
}
}

bool Q65DopplerTracker::validGrid(QString const& grid)
{
    // A four-character square is too coarse for automatic EME correction.
    static QRegularExpression const pattern(QStringLiteral("^[A-R]{2}[0-9]{2}[A-X]{2}([0-9]{2})?$"));
    return pattern.match(grid.trimmed().toUpper()).hasMatch();
}

QPair<double, double> Q65DopplerTracker::offsets(int method, double own, double dx)
{
    if (method == FullDoppler) return {own + dx, -(own + dx)};
    if (method == OwnEcho) return {2.0 * own, 0.0};
    return {own, -own};
}

QDateTime Q65DopplerTracker::txMidpoint(QDateTime const& utc, int periodMs)
{
    qint64 const period = qMax(15000, periodMs);
    qint64 const time = utc.toMSecsSinceEpoch() + 2000; // cover pre-key lead-in
    return QDateTime::fromMSecsSinceEpoch(time - time % period + period / 2, QTimeZone("UTC"));
}

void Q65DopplerTracker::setEnabled(bool value)
{
    if (value && (!m_ready || m_busy)) return;
    if (m_enabled == value) return;
    m_enabled = value;
    emit stateChanged();
    emit configurationChanged();
}

void Q65DopplerTracker::setMethod(int value)
{
    if (value < ConstantOnMoon || value > OwnEcho || value == m_method || m_busy) return;
    m_method = value;
    emit stateChanged();
    emit configurationChanged();
}

void Q65DopplerTracker::remember(double hz, qint64 now)
{
    if (hz <= 0) return;
    m_reports.erase(std::remove_if(m_reports.begin(), m_reports.end(),
                                  [now](Report const& r) { return r.expires < now; }), m_reports.end());
    m_reports.push_back({hz, now + 15000});
}

void Q65DopplerTracker::disconnected()
{
    m_enabled = m_applied = m_ready = false;
    m_rxDialHz = m_txDialHz = 0;
    m_reports.clear();
}

bool Q65DopplerTracker::consumeFrequencyReport(double hz, qint64 now)
{
    for (Report const& r : m_reports)
        if (r.expires >= now && std::abs(hz - r.hz) <= 2.0) return true;
    if (m_applied) {
        if (std::abs(hz - m_rxDialHz) <= 2.0 || std::abs(hz - m_txDialHz) <= 2.0)
            return true;
        // Do not retune against the operator. The normal CAT path adopts the
        // manually selected frequency and re-establishes ordinary split.
        m_enabled = m_applied = false;
        m_reports.clear();
        m_snapshot[QStringLiteral("status")] = tr("Doppler stopped: manual radio tuning");
        emit stateChanged();
    }
    return false;
}

Q65DopplerTracker::Action Q65DopplerTracker::update(Context const& c, QDateTime const& utc)
{
    Action action;
    m_busy = c.transmitting;
    QString reason;
    QString const mode = c.mode.trimmed().toUpper();
    if (!c.connected) { disconnected(); reason = tr("Connect a CAT radio"); }
    else if (!c.supported) reason = tr("EME tracking requires the Hamlib backend");
    else if (mode != QStringLiteral("Q65") && !mode.startsWith(QStringLiteral("Q65-"))) reason = tr("Select Q65");
    else if (!c.split) reason = tr("Set CAT Split to Rig or Fake It");
    else if (!std::isfinite(c.nominalHz) || c.nominalHz < 21000000.0) reason = tr("Select an EME frequency at or above 21 MHz");
    else if (!validGrid(c.myGrid)) reason = tr("Enter your station locator (6 or 8 characters)");
    else if (m_method == FullDoppler && !validGrid(c.dxGrid)) reason = tr("Enter the DX locator (6 or 8 characters)");
    else if (c.conflict) reason = tr("Another radio/audio controller is active");
    else if (!c.monitoring && !c.transmitting) reason = tr("Start monitoring");

    double own = 0, dx = 0, ownTx = 0, dxTx = 0, elevation = 0, unused = 0;
    if (reason.isEmpty()) {
        bool good = utc.isValid() && lunarState(utc, c.myGrid, c.nominalHz, own, elevation)
                    && lunarState(txMidpoint(utc, c.periodMs), c.myGrid, c.nominalHz, ownTx, unused);
        if (m_method == FullDoppler)
            good = good && lunarState(utc, c.dxGrid, c.nominalHz, dx, unused)
                        && lunarState(txMidpoint(utc, c.periodMs), c.dxGrid, c.nominalHz, dxTx, unused);
        if (!good) reason = tr("Lunar calculation unavailable");
    }
    m_ready = reason.isEmpty();
    if (!m_ready || (m_applied && c.nominalHz != m_nominalHz)) m_enabled = false;

    if (!m_busy) {
        if (m_enabled) {
            auto const rx = offsets(m_method, own, dx);
            auto const tx = offsets(m_method, ownTx, dxTx);
            double const rxHz = c.nominalHz + std::round(rx.first);
            double const txHz = c.nominalHz + std::round(tx.second);
            action.tune = !m_applied || rxHz != m_rxDialHz || txHz != m_txDialHz;
            action.rxHz = rxHz;
            action.txHz = txHz;
            if (action.tune) {
                remember(c.nominalHz, utc.toMSecsSinceEpoch());
                remember(m_rxDialHz, utc.toMSecsSinceEpoch());
                remember(m_txDialHz, utc.toMSecsSinceEpoch());
                remember(rxHz, utc.toMSecsSinceEpoch());
                remember(txHz, utc.toMSecsSinceEpoch());
            }
            m_applied = true;
            m_nominalHz = c.nominalHz;
            m_rxDialHz = rxHz;
            m_txDialHz = txHz;
        } else if (m_applied && c.connected) {
            action = {true, true, c.nominalHz, 0};
            remember(m_rxDialHz, utc.toMSecsSinceEpoch());
            remember(m_txDialHz, utc.toMSecsSinceEpoch());
            remember(c.nominalHz, utc.toMSecsSinceEpoch());
            m_applied = false;
            m_rxDialHz = m_txDialHz = 0;
        }
    }
    m_snapshot = {{QStringLiteral("ready"), m_ready}, {QStringLiteral("busy"), m_busy},
                  {QStringLiteral("active"), m_applied}, {QStringLiteral("nominalHz"), c.nominalHz},
                  {QStringLiteral("selfDopplerHz"), 2.0 * own}, {QStringLiteral("dxDopplerHz"), own + dx},
                  {QStringLiteral("moonElevation"), elevation},
                  {QStringLiteral("rxCorrectionHz"), m_applied ? m_rxDialHz - m_nominalHz : 0},
                  {QStringLiteral("txCorrectionHz"), m_applied ? m_txDialHz - m_nominalHz : 0},
                  {QStringLiteral("status"), m_busy && m_applied
                       ? (m_enabled ? tr("TX: correction held at the period midpoint") : tr("Stopping Doppler after TX"))
                       : (!reason.isEmpty() ? reason : (m_enabled ? tr("Doppler tracking active") : tr("Doppler tracking off")))}};
    emit stateChanged();
    return action;
}
