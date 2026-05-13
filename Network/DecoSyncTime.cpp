// -*- Mode: C++ -*-
#include "DecoSyncTime.hpp"
#include "NtpClient.hpp"
#include "HttpsTimeSource.hpp"
#include "DecoSyncSelfCal.hpp"

#include <QDateTime>

#include <algorithm>
#include <cmath>

DecoSyncTime::DecoSyncTime(QObject *parent)
    : QObject(parent)
    , m_ntp(new NtpClient(this))
    , m_https(new HttpsTimeSource(this))
    , m_selfCal(new DecoSyncSelfCal(this))
{
    // Fase 4: self-cal dai decode amateur. Quando una finestra di periodo
    // chiude con N+ sample, ingeriamo l'offset stimato come measurement
    // Kalman con varianza alta (decoder dt accuracy ~150ms).
    connect(m_selfCal, &DecoSyncSelfCal::offsetEstimateUpdated, this,
        [this](double offsetMs, double varianceMs2, int sampleCount) {
            Q_UNUSED(sampleCount)
            // Self-cal misura skew NETTO rispetto al correct time, NON
            // additivo al Kalman offset corrente. Convertiamo in
            // "measurement assoluto": m_kalman vede z = offsetMs.
            m_kalman.update(offsetMs, std::max(1.0, varianceMs2));
            recomputeCombinedOffset(QStringLiteral("selfcal"));
        });
    // Fase 2+3: NTP UDP, HTTPS Date e Kalman filter girano IN PARALLELO.
    // I sample da NTP e HTTPS vengono ingeriti nel Kalman che produce
    // l'offset finale predicted con compensazione drift PC.
    connect(m_ntp, &NtpClient::offsetUpdated, this, [this](double offsetMs) {
        // NTP RTT non e' direttamente esposto, ma il NtpClient gia' fa
        // sue filtering median+cluster; stimiamo varianza bassa.
        // Tipico residual jitter ~5 ms su sample filtrato.
        ingestKalmanSample(offsetMs, /*rttMs*/ 10.0, "ntp");
        recomputeCombinedOffset(QStringLiteral("ntp offset"));
    });
    connect(m_ntp, &NtpClient::syncStatusChanged, this,
        [this](bool synced, QString const& statusText) {
            Q_UNUSED(synced)
            recomputeCombinedOffset(QStringLiteral("ntp sync ") + statusText);
        });
    connect(m_ntp, &NtpClient::errorOccurred, this, &DecoSyncTime::errorOccurred);

    connect(m_https, &HttpsTimeSource::offsetUpdated, this,
        [this](double offsetMs, double rttMs, int) {
            // HTTPS Date risoluzione 1s + RTT half-asym error.
            // Varianza tipica ~ (rtt/2)^2 + (500ms)^2 (risoluzione + jitter)
            ingestKalmanSample(offsetMs, rttMs, "https");
            recomputeCombinedOffset(QStringLiteral("https offset"));
        });
    connect(m_https, &HttpsTimeSource::syncStatusChanged, this,
        [this](bool, QString const& statusText) {
            recomputeCombinedOffset(QStringLiteral("https sync ") + statusText);
        });
    connect(m_https, &HttpsTimeSource::errorOccurred, this,
        &DecoSyncTime::errorOccurred);

    // Kalman tick 1 Hz: anche senza nuovi sample, propaga lo stato cosi'
    // che l'offset cresca/decresca seguendo il drift stimato.
    // 1.0.172 — Kalman tick 2s invece di 1s. Per CPU su PC vecchi: il
    // predict step ogni 2s e' piu' che sufficiente per la stabilita'
    // dell'offset (drift PC tipico < 10ms/min = sub-pixel).
    m_kalmanTickTimer.setInterval(2000);
    connect(&m_kalmanTickTimer, &QTimer::timeout, this, &DecoSyncTime::onKalmanTick);
    m_kalmanLastTickMs = QDateTime::currentMSecsSinceEpoch();
    m_kalmanTickTimer.start();

    // Default ENABLED; il consumer (DecodiumBridge) chiama setEnabled con
    // il valore della QSettings; default ON su nuova install.
    m_ntp->setEnabled(true);
    m_https->setEnabled(true);
    m_selfCal->setEnabled(true);
}

DecoSyncTime::~DecoSyncTime() = default;

bool DecoSyncTime::synced() const
{
    // Fase 3: Kalman lock e' sufficient — significa che abbiamo gia' ricevuto
    // almeno un sample buono e stiamo propagando lo stato. Anche se NTP/HTTPS
    // perdono sync temporaneamente (rete down), il sync logico si mantiene
    // finche' il drift estimato resta dentro tolerance.
    if (m_kalman.hasLock()) return true;
    if (m_ntp && m_ntp->isSynced()) return true;
    if (m_https && m_https->isSynced()) return true;
    return false;
}

double DecoSyncTime::offsetMs() const
{
    // Fase 3: se il Kalman ha lock, e' la source autoritativa (combina NTP
    // + HTTPS + drift PC compensato). Altrimenti fallback a NTP/HTTPS raw.
    if (m_kalman.hasLock()) {
        return m_kalman.offsetMs();
    }
    if (m_ntp && m_ntp->isSynced()) {
        return m_ntp->offsetMs();
    }
    if (m_https && m_https->isSynced()) {
        return m_https->offsetMs();
    }
    return 0.0;
}

int DecoSyncTime::lastServerCount() const
{
    int total = 0;
    if (m_ntp) total += m_ntp->lastServerCount();
    if (m_https) total += m_https->lastEndpointCount();
    return total;
}

bool   DecoSyncTime::httpsSynced()    const { return m_https && m_https->isSynced(); }
double DecoSyncTime::httpsOffsetMs()  const { return m_https ? m_https->offsetMs() : 0.0; }
int    DecoSyncTime::httpsLastCount() const { return m_https ? m_https->lastEndpointCount() : 0; }

void DecoSyncTime::setEnabled(bool on)
{
    if (m_enabled == on) return;
    m_enabled = on;
    if (m_ntp) m_ntp->setEnabled(on);
    if (m_https) m_https->setEnabled(on);
    if (m_selfCal) m_selfCal->setEnabled(on);
    emit enabledChanged();
    recomputeCombinedOffset(on ? QStringLiteral("enabled") : QStringLiteral("disabled"));
}

void DecoSyncTime::reportDecodeDt(double dtSec, int snrDb)
{
    if (m_selfCal) m_selfCal->addDecodeDt(dtSec, snrDb);
}

void DecoSyncTime::setSelfCalPeriodMs(int ms)
{
    if (m_selfCal) m_selfCal->setPeriodMs(ms);
}

int DecoSyncTime::selfCalPendingCount() const
{
    return m_selfCal ? m_selfCal->pendingSampleCount() : 0;
}

double DecoSyncTime::selfCalLastOffsetMs() const
{
    return m_selfCal ? m_selfCal->lastEmittedOffsetMs() : 0.0;
}

void DecoSyncTime::setCustomServer(QString const& server)
{
    if (m_ntp) m_ntp->setCustomServer(server);
}

void DecoSyncTime::setRefreshInterval(int ms)
{
    if (m_ntp) m_ntp->setRefreshInterval(ms);
}

void DecoSyncTime::setMaxRtt(double ms)
{
    if (m_ntp) m_ntp->setMaxRtt(ms);
}

void DecoSyncTime::setInitialOffset(double offsetMs)
{
    if (m_ntp) m_ntp->setInitialOffset(offsetMs);
}

qint64 DecoSyncTime::wallclockMs() const
{
    qint64 const systemMs = QDateTime::currentMSecsSinceEpoch();
    if (m_enabled && synced()) {
        double const off = offsetMs();
        if (std::isfinite(off)) {
            return systemMs + static_cast<qint64>(off);
        }
    }
    return systemMs;
}

void DecoSyncTime::syncNow()
{
    if (m_ntp) m_ntp->syncNow();
    if (m_https) m_https->syncNow();
}

void DecoSyncTime::ingestKalmanSample(double offsetMs, double rttMs, char const* source)
{
    Q_UNUSED(source)
    // Varianza measurement: combinazione di RTT/2 (half-roundtrip uncertainty)
    // e jitter floor. NTP: ~10 ms RTT effettivo dopo cluster filtering;
    // HTTPS: ~rtt reale + 500ms (header risoluzione 1s ~ +/- 500ms).
    double const halfRtt = std::max(1.0, rttMs / 2.0);
    double measurementVar = halfRtt * halfRtt;
    // Su HTTPS aggiungiamo la quantizzazione del Date header (1s = +/- 500ms).
    // Identifichiamo via il magnitude di rttMs: NTP filtered passa varianza
    // direttamente (rttMs=10 default), HTTPS passa il vero rtt (>=20ms).
    if (rttMs > 50.0) {
        measurementVar += 250000.0;  // (500ms)^2
    }
    m_kalman.update(offsetMs, measurementVar);
}

void DecoSyncTime::onKalmanTick()
{
    qint64 const now = QDateTime::currentMSecsSinceEpoch();
    if (m_kalmanLastTickMs == 0) {
        m_kalmanLastTickMs = now;
        return;
    }
    double const dtSec = static_cast<double>(now - m_kalmanLastTickMs) / 1000.0;
    m_kalmanLastTickMs = now;
    if (dtSec <= 0.0 || dtSec > 60.0) return;  // skip ticks anomali
    m_kalman.predict(dtSec);
    // Tick propaga lo stato; se Kalman ha lock, lo stato cambia secondo il
    // drift, quindi notifichiamo il consumer cosi' l'offset effettivo
    // visibile riflette il drift compensation in real time.
    if (m_kalman.hasLock()) {
        recomputeCombinedOffset(QStringLiteral("kalman tick"));
    }
}

void DecoSyncTime::recomputeCombinedOffset(QString const& reason)
{
    Q_UNUSED(reason)
    bool const nowSynced = synced();
    double const nowOffset = offsetMs();

    bool const syncedChanged = (nowSynced != m_lastEmittedSynced);
    // 1.0.172 — soglia emit alzata da 0.5 a 5 ms. Riduce signal flooding
    // verso il QML su PC vecchi (1 emit ogni 2s di tick × cambi minimi
    // del Kalman → 20-40 signal/min inutili). Sotto i 5 ms l'offset
    // visibile e' invariato per l'utente FT8/FT4.
    bool const offsetChanged =
        std::abs(nowOffset - m_lastEmittedOffsetMs) >= 5.0
        || (syncedChanged && nowSynced);

    if (syncedChanged) {
        m_lastEmittedSynced = nowSynced;
        QString const status = nowSynced
            ? QStringLiteral("synced (combined ntp+https)")
            : QStringLiteral("not synced");
        emit this->syncedChanged(nowSynced, status);
    }
    if (offsetChanged) {
        m_lastEmittedOffsetMs = nowOffset;
        emit offsetUpdated(nowOffset);
    }
}
