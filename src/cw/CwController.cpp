#include "CwController.h"

#include <algorithm>
#include <cmath>
#include <QDateTime>
#include <QDebug>
#include <QPointer>
#include <QRegularExpression>

namespace decodium::cw {

namespace {
constexpr int kMinWpm = 5;
constexpr int kMaxWpm = 60;
constexpr int kKeyerRetryIntervalMs = 250;
constexpr int kKeyerRetryLimit = 120;
const char kGroup[] = "CW";

// A serial keyer and a K1EL WinKeyer are local devices opened through
// QSerialPort.  rigctld endpoints are commonly written as host:port, which
// QSerialPort otherwise tries to open as a (non-existent) device name and
// leaves the operator with the misleading "keyer is not open" message.
bool looksLikeTcpEndpoint(const QString& candidate)
{
    const QString value = candidate.trimmed();
    if (value.isEmpty())
        return false;

    static const QRegularExpression uriScheme(
        QStringLiteral(R"(^(?:tcp|rigctld|http|https|ws|wss)://)"),
        QRegularExpression::CaseInsensitiveOption);
    if (uriScheme.match(value).hasMatch())
        return true;

    // IPv6 endpoints use the conventional [address]:port spelling.
    static const QRegularExpression ipv6Endpoint(
        QStringLiteral(R"(^\[[0-9A-Fa-f:.]+\]:\d{1,5}$)"));
    if (ipv6Endpoint.match(value).hasMatch())
        return true;

    // Covers localhost:4533, 127.0.0.1:4533 and DNS names.  A real serial
    // device such as COM5, \\.\COM12 or /dev/ttyUSB0 cannot match this form.
    static const QRegularExpression hostPort(
        QStringLiteral(R"(^(?:(?:\d{1,3}\.){3}\d{1,3}|[A-Za-z0-9][A-Za-z0-9.-]*):\d{1,5}$)"));
    return hostPort.match(value).hasMatch();
}
}  // namespace

CwController::CwController(QObject* parent)
    : QObject(parent)
    , m_macros(defaultMacros())
{
    m_audioDone.setSingleShot(true);
    m_keyTimer.setSingleShot(true);
    m_keyerRetryTimer.setSingleShot(true);
    connect(&m_keyerRetryTimer, &QTimer::timeout, this, [this] {
        qInfo().noquote() << "[CW] queued keyer reopen attempt"
                          << m_keyerRetryAttempts << "of" << kKeyerRetryLimit;
        applyBackend();
    });
    connect(&m_keyTimer, &QTimer::timeout, this, [this] {
        if (m_keyAllSent)
            finishRemoteKey(false);
        else
            pumpRemoteKey();
    });
    connect(&m_audioDone, &QTimer::timeout, this, [this] {
        // Per il keyer CAT il PTT alto e' lo stato atteso durante l'invio:
        // non passare da canTransmit(), che in TX risulta correttamente falso.
        if (m_rigActive) {
            if (m_rigWaiting) {
                // No completion from the CAT worker: stop before releasing PTT.
                if (m_hooks.stopRigMorse) m_hooks.stopRigMorse();
                finishRigKey();
                emit message(tr("The CAT CW keyer did not confirm the send command"), QStringLiteral("warning"));
            } else if (!m_rigQueuedText.isEmpty()) {
                sendNextRigChunk();
            } else {
                finishRigKey();
            }
            return;
        }
        // The bridge may still be draining its audio device after the Morse
        // timeline ends. Poll its real transmit gate instead of imposing a
        // long fixed delay or declaring the controller idle too early.
        if (m_hooks.canTransmit && !m_hooks.canTransmit()) {
            m_audioDone.start(25);
            return;
        }
        setSending(false, -1);
    });

    connect(&m_keyer, &CwKeyer::finished, this, [this] { setSending(false, -1); });
    connect(&m_keyer, &CwKeyer::failed, this, [this](const QString& why) {
        setSending(false, -1);
        emit message(why, QStringLiteral("warning"));
        emit txChanged();
    });
    connect(&m_winKeyer, &WinKeyer::busyChanged, this, [this](bool busy) {
        if (!busy)
            setSending(false, -1);
    });
    connect(&m_winKeyer, &WinKeyer::failed, this, [this](const QString& why) {
        setSending(false, -1);
        emit message(why, QStringLiteral("warning"));
        emit txChanged();
    });
    connect(&m_winKeyer, &WinKeyer::versionReceived, this, [this](int) { emit txChanged(); });
}

CwController::~CwController()
{
    // Il piedino torna giu' e la coda si svuota: non si lascia mai una radio
    // con il tasto abbassato.
    m_keyer.stop();
    m_winKeyer.stop();
}

void CwController::start(QSettings* settings)
{
    m_settings = settings;
    if (m_settings) {
        m_settings->beginGroup(QLatin1String(kGroup));
        m_backend = m_settings->value(QStringLiteral("backend"), m_backend).toString();
        m_keyerPort = m_settings->value(QStringLiteral("keyerPort")).toString();
        m_keyerLine = m_settings->value(QStringLiteral("keyerLine"), m_keyerLine).toString();
        m_winKeyerPort = m_settings->value(QStringLiteral("winKeyerPort")).toString();
        m_wpm = std::clamp(m_settings->value(QStringLiteral("wpm"), m_wpm).toInt(), kMinWpm, kMaxWpm);
        m_decoderToneLock = m_settings->value(QStringLiteral("toneLock"), 0).toInt();
        m_decoderSpeedLock = m_settings->value(QStringLiteral("speedLock"), 0).toInt();
        m_decoderOn = m_settings->value(QStringLiteral("decoderOn"), true).toBool();
        m_tuningLowerSideband = m_settings->value(QStringLiteral("tuningLowerSideband"), false).toBool();
        m_remoteKey = m_settings->value(QStringLiteral("remoteKey"), true).toBool();
        m_toneHz = std::clamp(m_settings->value(QStringLiteral("toneHz"), 700).toInt(), 400, 1000);
        m_macros = macrosFromJson(m_settings->value(QStringLiteral("macros")).toString());
        m_settings->endGroup();
    }
    if (m_backend != QLatin1String("serial") && m_backend != QLatin1String("winkeyer"))
        m_backend = QStringLiteral("audio");

    // Never retry a saved rigctld address as a local COM/tty device.  Clear
    // it from the persisted CW setup as well, so every launch is safe and the
    // user can choose a real device once without fighting the old value.
    bool removedNetworkEndpoint = false;
    if (rejectNetworkKeyerPort(m_keyerPort, tr("serial keyer"))) {
        m_keyerPort.clear();
        removedNetworkEndpoint = true;
    }
    if (rejectNetworkKeyerPort(m_winKeyerPort, tr("WinKeyer"))) {
        m_winKeyerPort.clear();
        removedNetworkEndpoint = true;
    }
    if (removedNetworkEndpoint)
        save();

    m_decoder.setTone(m_decoderToneLock);
    m_decoder.setSpeed(m_decoderSpeedLock);
    applyBackend();
    emit decoderChanged();
    emit macrosChanged();
    emit txChanged();
}

void CwController::save()
{
    if (!m_settings)
        return;
    m_settings->beginGroup(QLatin1String(kGroup));
    m_settings->setValue(QStringLiteral("backend"), m_backend);
    m_settings->setValue(QStringLiteral("keyerPort"), m_keyerPort);
    m_settings->setValue(QStringLiteral("keyerLine"), m_keyerLine);
    m_settings->setValue(QStringLiteral("winKeyerPort"), m_winKeyerPort);
    m_settings->setValue(QStringLiteral("wpm"), m_wpm);
    m_settings->setValue(QStringLiteral("toneLock"), m_decoderToneLock);
    m_settings->setValue(QStringLiteral("speedLock"), m_decoderSpeedLock);
    m_settings->setValue(QStringLiteral("decoderOn"), m_decoderOn);
    m_settings->setValue(QStringLiteral("tuningLowerSideband"), m_tuningLowerSideband);
    m_settings->setValue(QStringLiteral("remoteKey"), m_remoteKey);
    m_settings->setValue(QStringLiteral("toneHz"), m_toneHz);
    m_settings->setValue(QStringLiteral("macros"), macrosToJson(m_macros));
    m_settings->endGroup();
}

// ── decodificatore ─────────────────────────────────────────────────────────

void CwController::setDecoderOn(bool on)
{
    if (m_decoderOn == on)
        return;
    if (!on) {
        // L'ultima lettera sta ancora nel decodificatore: la finestra di
        // analisi e' lunga tre secondi, e spegnendo si chiuderebbe con una
        // lettera in meno.
        const QString last = m_decoder.flush();
        if (!last.isEmpty())
            m_decoderText += last;
        m_scope.clear();
        emit decoderScopeChanged();
    }
    m_decoderOn = on;
    save();
    emit decoderChanged();
}

void CwController::setDecoderToneLock(int hz)
{
    const int clean = (hz <= 0) ? 0 : std::clamp(hz, 100, 3000);
    if (m_decoderToneLock == clean)
        return;
    m_decoderToneLock = clean;
    m_decoder.setTone(clean);
    m_decoder.reset();
    save();
    emit decoderChanged();
}

void CwController::setDecoderSpeedLock(int wpm)
{
    const int clean = (wpm <= 0) ? 0 : std::clamp(wpm, kMinWpm, kMaxWpm);
    if (m_decoderSpeedLock == clean)
        return;
    m_decoderSpeedLock = clean;
    m_decoder.setSpeed(clean);
    m_decoder.reset();
    save();
    emit decoderChanged();
}

void CwController::setTuningLowerSideband(bool lower)
{
    if (m_tuningLowerSideband == lower) return;
    m_tuningLowerSideband = lower;
    save();
    emit txChanged();
}

void CwController::invalidateTuningSignal()
{
    m_decoder.reset();
    m_scope.clear();
    m_lastRxClock.invalidate();
    m_retuneClock.restart();
    emit decoderScopeChanged();
}

bool CwController::tuneTo(double hz)
{
    if (m_sending || !std::isfinite(hz) || hz < 1000 || hz > 1e12
        || !m_hooks.tuneFrequency || !m_hooks.tuneFrequency(std::round(hz))) {
        emit message(tr("Cannot tune: check CAT connection and stop transmitting."), QStringLiteral("warning"));
        return false;
    }
    invalidateTuningSignal();
    return true;
}

bool CwController::tuneBy(int hz)
{
    return m_hooks.frequency && hz >= -10000 && hz <= 10000
        && tuneTo(m_hooks.frequency() + hz);
}

bool CwController::centerSignal()
{
    // Use the measured pitch, never a fixed decoder filter or the previous station.
    const auto& scope = m_decoder.scope();
    if (!m_decoderOn || m_decoderToneLock != 0 || !m_lastRxClock.isValid()
        || m_lastRxClock.elapsed() > 750 || !scope.reading
        || !std::isfinite(scope.pitch) || scope.pitch < 200 || scope.pitch > 1200
        || (m_retuneClock.isValid() && m_retuneClock.elapsed() < 1500)) {
        emit message(tr("Wait for a CW signal with Decoder tone set to Auto."), QStringLiteral("warning"));
        return false;
    }
    const int error = qRound(scope.pitch) - m_toneHz;
    if (std::abs(error) <= 5) {
        emit message(tr("CW is already centered."), QStringLiteral("info"));
        return true;
    }
    // CW polarity is radio-dependent: the operator selects CW-U / CW-L.
    if (!tuneBy(m_tuningLowerSideband ? -error : error)) return false;
    emit message(tr("CW centered at %1 Hz.").arg(m_toneHz), QStringLiteral("info"));
    return true;
}

void CwController::clearDecoder()
{
    m_decoderText.clear();
    m_decoder.reset();
    emit decoderChanged();
}

void CwController::feedRxAudio(const QVector<short>& samples, int sampleRate)
{
    if (!m_decoderOn || samples.isEmpty())
        return;
    m_decoderLogSamples += static_cast<quint64>(samples.size());
    if (m_decoder.sampleRate() != sampleRate) {
        m_decoder.setSampleRate(sampleRate);
        m_decoder.reset();
    }
    // Discard queued pre-QSY audio while the CAT/remote stream settles.
    if (m_retuneClock.isValid() && m_retuneClock.elapsed() < 1000) return;
    m_lastRxClock.restart();
    const QString text = m_decoder.feed(reinterpret_cast<const qint16*>(samples.constData()),
                                        static_cast<int>(samples.size()));
    if (!m_decoderLogClock.isValid() || m_decoderLogClock.elapsed() >= 2000) {
        m_decoderLogClock.restart();
        const auto& scope = m_decoder.scope();
        qInfo().noquote() << QStringLiteral("[CW-DECODER] samples=%1 tone_hz=%2 wpm=%3 cost=%4 reading=%5 text_chars=%6")
                             .arg(m_decoderLogSamples)
                             .arg(scope.pitch, 0, 'f', 1)
                             .arg(scope.speed, 0, 'f', 1)
                             .arg(scope.cost, 0, 'f', 4)
                             .arg(scope.reading ? 1 : 0)
                             .arg(text.size());
        m_decoderLogSamples = 0;
    }
    if (!text.isEmpty()) {
        m_decoderText += text;
        // Non si tiene una giornata di CW in memoria: gli ultimi 4000
        // caratteri bastano e avanzano.
        if (m_decoderText.size() > 4000)
            m_decoderText = m_decoderText.right(3000);
    }
    emit decoderChanged();
    publishScope();
}

void CwController::publishScope(bool force)
{
    // Il disegno non ha bisogno di tutti i fotogrammi: a 15 al secondo scorre
    // gia' liscio, e l'interfaccia non si carica per niente.
    if (!force && m_scopeClock.isValid() && m_scopeClock.elapsed() < 66)
        return;
    m_scopeClock.restart();

    const CwDecoder::Scope& scope = m_decoder.scope();
    // Una colonna per punto del grafico basta e avanza: si tiene il massimo di
    // ogni gruppo, cosi' anche il punto piu' corto resta visibile.
    constexpr int kPoints = 300;
    QVariantList signal;
    const qsizetype n = scope.signal.size();
    if (n > 0) {
        const int points = static_cast<int>(std::min<qsizetype>(n, kPoints));
        signal.reserve(points);
        for (int i = 0; i < points; ++i) {
            const qsizetype from = n * i / points;
            const qsizetype to = std::max(from + 1, n * (i + 1) / points);
            float peak = 0;
            for (qsizetype j = from; j < to; ++j)
                peak = std::max(peak, scope.signal.at(j));
            signal.append(peak);
        }
    }
    m_scope = QVariantMap{
        {QStringLiteral("signal"), signal},
        {QStringLiteral("level"), scope.level},
        {QStringLiteral("pitch"), scope.pitch},
        {QStringLiteral("wpm"), scope.speed},
        {QStringLiteral("cost"), scope.cost},
        {QStringLiteral("reading"), scope.reading},
    };
    emit decoderScopeChanged();
}

// ── trasmissione ───────────────────────────────────────────────────────────

void CwController::setTxBackend(const QString& backend)
{
    QString b = backend.toLower();
    if (b != QLatin1String("serial") && b != QLatin1String("winkeyer"))
        b = QStringLiteral("audio");
    if (m_backend == b)
        return;
    stop();
    m_backend = b;
    if (m_backend == QLatin1String("audio"))
        clearTxSetupError();
    applyBackend();
    save();
    emit txChanged();
}

void CwController::setKeyerPort(const QString& port)
{
    const QString clean = port.trimmed();
    if (rejectNetworkKeyerPort(clean, tr("serial keyer")))
        return;
    if (m_keyerPort == clean)
        return;
    m_keyerPort = clean;
    clearTxSetupError();
    applyBackend();
    save();
    emit txChanged();
}

void CwController::setKeyerLine(const QString& line)
{
    const QString l = line.toUpper() == QLatin1String("RTS") ? QStringLiteral("RTS") : QStringLiteral("DTR");
    if (m_keyerLine == l)
        return;
    m_keyerLine = l;
    applyBackend();
    save();
    emit txChanged();
}

void CwController::setWinKeyerPort(const QString& port)
{
    const QString clean = port.trimmed();
    if (rejectNetworkKeyerPort(clean, tr("WinKeyer")))
        return;
    if (m_winKeyerPort == clean)
        return;
    m_winKeyerPort = clean;
    clearTxSetupError();
    applyBackend();
    save();
    emit txChanged();
}

QString CwController::effectiveBackend() const
{
    if (!remoteRadio()) {
        if (m_backend == QLatin1String("audio")
            && m_hooks.rigKeyerAvailable && m_hooks.rigKeyerAvailable())
            return QStringLiteral("rigkey");
        return m_backend;
    }
    if (m_remoteKey && m_hooks.remoteKeySupported && m_hooks.remoteKeySupported())
        return QStringLiteral("remotekey");
    return QStringLiteral("audio");
}

void CwController::setRemoteKey(bool on)
{
    if (m_remoteKey == on)
        return;
    m_remoteKey = on;
    save();
    emit txChanged();
}

void CwController::setToneHz(int hz)
{
    const int clean = std::clamp(hz, 400, 1000);
    if (m_toneHz == clean)
        return;
    m_toneHz = clean;
    save();
    emit txChanged();
}

void CwController::setWpm(int wpm)
{
    const int clean = std::clamp(wpm, kMinWpm, kMaxWpm);
    if (m_wpm == clean)
        return;
    m_wpm = clean;
    if (m_winKeyer.isOpen())
        m_winKeyer.setSpeed(clean);
    save();
    emit txChanged();
}

// Apre solo il manipolatore locale che verra' davvero usato: due strade aperte
// insieme vorrebbero dire due mani sullo stesso tasto.  In particolare non si
// deve lasciare una COM aperta quando il CAT nativo, una radio remota o l'audio
// hanno gia' scelto un'altra strada.
void CwController::applyBackend()
{
    m_keyerRetryTimer.stop();
    m_keyer.close();
    m_winKeyer.close();

    const QString active = effectiveBackend();
    if (active != QLatin1String("serial") && active != QLatin1String("winkeyer")) {
        m_keyerRetryAttempts = 0;
        // The selected local keyer is not the active transmit path (for
        // example a supported CAT keyer took over).  Do not keep a stale
        // port-conflict warning in front of a now-working transmit path.
        clearTxSetupError();
        return;
    }
    if (active == QLatin1String("serial") && !m_keyerPort.isEmpty()) {
        if (rejectNetworkKeyerPort(m_keyerPort, tr("serial keyer")))
            return;
        if (rejectReservedKeyerPort(m_keyerPort, tr("serial keyer"))) {
            scheduleKeyerReopen(tr("waiting for the owning CAT/DecoPort component"));
            return;
        }
        if (!m_keyer.open(m_keyerPort, m_keyerLine)) {
            emit message(tr("The keyer port %1 did not open").arg(m_keyerPort), QStringLiteral("warning"));
            scheduleKeyerReopen(tr("serial keyer open failed"));
        } else {
            m_keyerRetryAttempts = 0;
            clearTxSetupError();
        }
    } else if (active == QLatin1String("winkeyer") && !m_winKeyerPort.isEmpty()) {
        if (rejectNetworkKeyerPort(m_winKeyerPort, tr("WinKeyer")))
            return;
        if (rejectReservedKeyerPort(m_winKeyerPort, tr("WinKeyer"))) {
            scheduleKeyerReopen(tr("waiting for the owning CAT/DecoPort component"));
            return;
        }
        if (!m_winKeyer.open(m_winKeyerPort)) {
            emit message(tr("The WinKeyer on %1 did not open").arg(m_winKeyerPort), QStringLiteral("warning"));
            scheduleKeyerReopen(tr("WinKeyer open failed"));
        } else {
            m_keyerRetryAttempts = 0;
            m_winKeyer.setSpeed(m_wpm);
            clearTxSetupError();
        }
    }
}

void CwController::scheduleKeyerReopen(const QString& reason)
{
    if (m_keyerRetryAttempts >= kKeyerRetryLimit || m_keyerRetryTimer.isActive())
        return;
    ++m_keyerRetryAttempts;
    qInfo().noquote() << "[CW] keyer reopen queued"
                      << "attempt=" << m_keyerRetryAttempts << "/" << kKeyerRetryLimit
                      << "reason=" << reason;
    m_keyerRetryTimer.start(kKeyerRetryIntervalMs);
}

bool CwController::rejectNetworkKeyerPort(const QString& port, const QString& keyerName)
{
    const QString endpoint = port.trimmed();
    if (!looksLikeTcpEndpoint(endpoint))
        return false;

    m_txSetupError = tr("%1 is a TCP CAT/rigctld address, not a local %2 port. "
                        "Configure it under CAT and select a real COM or tty device here.")
                         .arg(endpoint, keyerName);
    qWarning().noquote() << "[CW] rejected TCP endpoint for" << keyerName << ':' << endpoint;
    emit message(m_txSetupError, QStringLiteral("warning"));
    emit txChanged();
    return true;
}

bool CwController::rejectReservedKeyerPort(const QString& port, const QString& keyerName)
{
    if (!m_hooks.serialPortReservation)
        return false;

    const QString owner = m_hooks.serialPortReservation(port).trimmed();
    if (owner.isEmpty())
        return false;

    const QString error = tr("%1 cannot be used for the %2 because it is already reserved by %3. "
                             "A serial CW keyer needs a different local COM or tty port. "
                             "Select Audio tone, the radio CAT keyer, or a separate keyer port.")
                              .arg(port.trimmed(), keyerName, owner);
    if (m_txSetupError != error) {
        m_txSetupError = error;
        qWarning().noquote() << "[CW] rejected reserved serial port for" << keyerName << ':' << port
                             << "owner=" << owner;
        emit message(m_txSetupError, QStringLiteral("warning"));
        emit txChanged();
    }
    return true;
}

void CwController::clearTxSetupError()
{
    if (m_txSetupError.isEmpty())
        return;
    m_txSetupError.clear();
}

bool CwController::canSend() const
{
    const QString backend = effectiveBackend();
    if (backend == QLatin1String("serial"))
        return m_keyer.isOpen();
    if (backend == QLatin1String("winkeyer"))
        return m_winKeyer.isOpen();
    if (backend == QLatin1String("remotekey"))
        return true;     // effectiveBackend() l'ha scelto perche' la strada c'e'
    if (backend == QLatin1String("rigkey"))
        return m_rigActive || !m_hooks.canTransmit || m_hooks.canTransmit();
    return m_hooks.sendAudio && (!m_hooks.canTransmit || m_hooks.canTransmit());
}

void CwController::refreshTransmitState()
{
    const QString active = effectiveBackend();
    const bool wantsSerial = active == QLatin1String("serial");
    const bool wantsWinKeyer = active == QLatin1String("winkeyer");
    if ((wantsSerial != m_keyer.isOpen()) || (wantsWinKeyer != m_winKeyer.isOpen()))
        applyBackend();
    emit txChanged();
}

void CwController::refreshKeyerSetup()
{
    if (m_backend == QLatin1String("serial") || m_backend == QLatin1String("winkeyer"))
        applyBackend();
    emit txChanged();
}

QStringList CwController::availablePorts() const
{
    return CwKeyer::ports();
}

void CwController::testKeyer()
{
    sendText(QStringLiteral("V"), {});
}

Context CwController::contextFrom(const QVariantMap& map) const
{
    Context c;
    c.myCall = m_hooks.myCall ? m_hooks.myCall() : QString();
    c.call = map.contains(QStringLiteral("call")) ? map.value(QStringLiteral("call")).toString()
                                                  : (m_hooks.hisCall ? m_hooks.hisCall() : QString());
    c.rst = map.value(QStringLiteral("rst"), QStringLiteral("599")).toString();
    c.nr = map.value(QStringLiteral("nr")).toString();
    c.exch = map.value(QStringLiteral("exch")).toString();
    c.name = map.value(QStringLiteral("name")).toString();
    return c;
}

QString CwController::expandText(const QString& text, const QVariantMap& context) const
{
    return expand(text, contextFrom(context));
}

void CwController::sendMacro(int index, const QVariantMap& context)
{
    if (index < 0 || index >= m_macros.size())
        return;
    const QString ready = expand(m_macros.at(index).text, contextFrom(context));
    if (ready.isEmpty())
        return;
    if (!canSend()) {
        emit message(tr("CW transmit is not ready"), QStringLiteral("warning"));
        return;
    }
    setSending(true, index);
    sendExpanded(ready);
}

void CwController::sendText(const QString& text, const QVariantMap& context)
{
    const QString ready = expand(text, contextFrom(context));
    if (ready.isEmpty())
        return;
    if (!canSend()) {
        emit message(tr("CW transmit is not ready"), QStringLiteral("warning"));
        return;
    }
    setSending(true, -1);
    sendExpanded(ready);
}

bool CwController::sendTypedText(const QString& text)
{
    QString ready = text.toUpper();
    ready.replace(QRegularExpression(QStringLiteral("\\s")), QStringLiteral(" "));
    if (ready.trimmed().isEmpty() || !canSend() || m_sending)
        return false;
    setSending(true, -1);
    sendExpanded(ready);
    return m_sending;
}

void CwController::sendExpanded(const QString& ready)
{
    // Il tempo che il messaggio ci mette: serve a sapere quando l'audio finisce
    // (non lo dice nessuno) e a non lasciare un tasto acceso per sempre.
    const int ms = CwKeyer::millisFor(ready, m_wpm);
    const QString backend = effectiveBackend();
    if (m_rigActive || backend == QLatin1String("rigkey")) {
        if (m_rigActive) {
            // Another macro must not overwrite the message memory in flight.
            m_rigQueuedText += QLatin1Char(' ') + ready;
            return;
        }
        m_rigQueuedText = ready;
        m_rigActive = true;
        if (m_hooks.rigPtt) m_hooks.rigPtt(true);
        sendNextRigChunk();
        return;
    }
    if (backend == QLatin1String("remotekey")) {
        startRemoteKey(ready);
        return;
    }
    if (backend == QLatin1String("winkeyer")) {
        m_winKeyer.send(ready, m_wpm);
        return;
    }
    if (backend == QLatin1String("serial")) {
        m_keyer.send(ready, m_wpm);
        return;
    }
    if (!m_hooks.sendAudio || !m_hooks.sendAudio(ready, m_wpm)) {
        setSending(false, -1);
        emit message(tr("The CW audio did not start"), QStringLiteral("warning"));
        return;
    }
    // Begin checking as soon as the Morse timeline has played. The audio
    // backend's PTT/audio tail is variable, so the timer callback waits on the
    // real canTransmit hook in short intervals instead of adding a fixed 800 ms
    // pause after every short typed chunk.
    m_audioDone.start(ms + 100);
}

// ── CW a tasto verso la radio remota ───────────────────────────────────────
//
// Si alza il PTT, si aspetta che arrivi, e poi la traccia del messaggio parte
// a pezzi. Il gateway mette in coda gli eventi e li suona al ritmo che portano:
// per questo ogni pezzo deve arrivare prima che finisca il precedente, e per
// questo si tiene davanti mezzo secondo di traccia gia' consegnata. Il prezzo
// e' che un Stop non ferma subito: smette di mandare e abbassa il tasto, ma
// quel mezzo secondo gia' in coda il gateway lo suona.

void CwController::sendNextRigChunk()
{
    const int limit = m_hooks.rigMorseTextLimit ? m_hooks.rigMorseTextLimit() : 0;
    const int count = limit > 0 ? qMin(limit, int(m_rigQueuedText.size())) : int(m_rigQueuedText.size());
    const QString chunk = m_rigQueuedText.left(count);
    m_rigQueuedText.remove(0, count);
    const int duration = CwKeyer::millisFor(chunk, m_wpm);
    const int tail = qMax(100, 3 * (1200 / m_wpm));
    const quint64 generation = ++m_rigGeneration;
    m_rigWaiting = true;
    m_audioDone.start(10000);
    QPointer<CwController> self(this);
    auto completed = [self, generation, duration, tail](const QString& error) {
        if (!self || !self->m_rigActive || generation != self->m_rigGeneration) return;
        self->m_rigWaiting = false;
        if (!error.isEmpty()) {
            self->finishRigKey();
            emit self->message(error, QStringLiteral("warning"));
        } else {
            self->m_audioDone.start(duration + tail);
        }
    };
    if (m_hooks.sendRigMorseAsync) {
        if (!m_hooks.sendRigMorseAsync(chunk, m_wpm, completed))
            completed(tr("The radio CAT keyer did not accept the CW text"));
    } else {
        const bool accepted = m_hooks.sendRigMorse && m_hooks.sendRigMorse(chunk, m_wpm);
        completed(accepted ? QString {} : tr("The radio CAT keyer did not accept the CW text"));
    }
}

void CwController::finishRigKey()
{
    ++m_rigGeneration; // invalidate acknowledgements from an earlier send
    m_rigActive = false;
    m_rigWaiting = false;
    m_rigQueuedText.clear();
    m_audioDone.stop();
    if (m_hooks.rigPtt) m_hooks.rigPtt(false);
    setSending(false, -1);
}

void CwController::startRemoteKey(const QString& text)
{
    m_keyQueue = timelineFor(text, m_wpm);
    if (m_keyQueue.isEmpty()) {
        setSending(false, -1);
        return;
    }
    m_keyActive = true;
    m_keyAllSent = false;
    m_keyPlayheadStarted = false;
    m_keySentMs = 0;
    if (m_hooks.remotePtt)
        m_hooks.remotePtt(true);
    // Il PTT viaggia come comando: gli si da' il tempo di arrivare.
    const int lead = m_hooks.remoteLeadMs ? m_hooks.remoteLeadMs() : 250;
    m_keyTimer.start(qMax(0, lead));
}

void CwController::pumpRemoteKey()
{
    if (!m_keyActive)
        return;
    if (!m_keyPlayheadStarted) {
        m_keyPlayheadStarted = true;
        m_keyClock.restart();
    }
    const int lead = m_hooks.remoteLeadMs ? m_hooks.remoteLeadMs() : 250;
    const int lookahead = qMax(500, lead * 2);
    const int elapsed = static_cast<int>(m_keyClock.elapsed());
    while (!m_keyQueue.isEmpty() && m_keySentMs - elapsed < lookahead) {
        const QVector<KeyEvent> piece = takeWindow(m_keyQueue, lookahead);
        if (piece.isEmpty() || !m_hooks.sendKey || !m_hooks.sendKey(piece, m_toneHz)) {
            emit message(tr("The CW key did not reach the remote radio"), QStringLiteral("warning"));
            finishRemoteKey(true);
            return;
        }
        m_keySentMs += totalMs(piece);
    }
    if (m_keyQueue.isEmpty()) {
        // Tutto consegnato: si aspetta che il gateway finisca di suonarlo, piu'
        // una coda perche' l'ultimo "su" sia davvero arrivato prima del PTT.
        m_keyAllSent = true;
        m_keyTimer.start(qMax(0, m_keySentMs - elapsed) + 300);
    } else {
        m_keyTimer.start(qMax(10, m_keySentMs - elapsed - lookahead / 2));
    }
}

void CwController::finishRemoteKey(bool immediate)
{
    if (!m_keyActive)
        return;
    m_keyActive = false;
    m_keyAllSent = false;
    m_keyTimer.stop();
    if (immediate && m_hooks.sendKey)
        m_hooks.sendKey({{0, false}}, m_toneHz);        // il tasto su, per non lasciare la nota accesa
    m_keyQueue.clear();
    if (m_hooks.remotePtt)
        m_hooks.remotePtt(false);
    setSending(false, -1);
}

void CwController::stop()
{
    if (m_rigActive) {
        if (m_hooks.stopRigMorse) m_hooks.stopRigMorse();
        // Release the path that started TX, even after CAT/backend changes.
        finishRigKey();
    }
    finishRemoteKey(true);
    m_audioDone.stop();
    m_keyer.stop();
    m_winKeyer.stop();
    if (m_hooks.abortAudio)
        m_hooks.abortAudio();
    setSending(false, -1);
}

void CwController::setSending(bool on, int macroIndex)
{
    if (m_sending == on && m_activeMacro == macroIndex)
        return;
    m_sending = on;
    m_activeMacro = on ? macroIndex : -1;
    emit sendingChanged();
}

// ── macro ──────────────────────────────────────────────────────────────────

QVariantList CwController::macros() const
{
    QVariantList out;
    for (const Macro& m : m_macros)
        out << QVariantMap{{QStringLiteral("label"), m.label}, {QStringLiteral("text"), m.text}};
    return out;
}

void CwController::setMacro(int index, const QString& label, const QString& text)
{
    if (index < 0 || index >= m_macros.size())
        return;
    m_macros[index] = {label, text};
    save();
    emit macrosChanged();
}

void CwController::addMacro()
{
    if (m_macros.size() >= kMaxMacros)
        return;
    m_macros.append({QStringLiteral("F%1").arg(m_macros.size() + 1), QString()});
    save();
    emit macrosChanged();
}

void CwController::removeMacro(int index)
{
    // Almeno un tasto resta sempre: una tastiera senza tasti non si puo'
    // nemmeno riempire di nuovo.
    if (index < 0 || index >= m_macros.size() || m_macros.size() <= 1)
        return;
    m_macros.removeAt(index);
    save();
    emit macrosChanged();
}

void CwController::resetMacros()
{
    m_macros = defaultMacros();
    save();
    emit macrosChanged();
}

}  // namespace decodium::cw
