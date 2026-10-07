#include "CwController.h"

#include <algorithm>

namespace decodium::cw {

namespace {
constexpr int kMinWpm = 5;
constexpr int kMaxWpm = 60;
const char kGroup[] = "CW";
}  // namespace

CwController::CwController(QObject* parent)
    : QObject(parent)
    , m_macros(defaultMacros())
{
    m_audioDone.setSingleShot(true);
    connect(&m_audioDone, &QTimer::timeout, this, [this] { setSending(false, -1); });

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
        m_macros = macrosFromJson(m_settings->value(QStringLiteral("macros")).toString());
        m_settings->endGroup();
    }
    if (m_backend != QLatin1String("serial") && m_backend != QLatin1String("winkeyer"))
        m_backend = QStringLiteral("audio");
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
    if (m_decoder.sampleRate() != sampleRate) {
        m_decoder.setSampleRate(sampleRate);
        m_decoder.reset();
    }
    const QString text = m_decoder.feed(reinterpret_cast<const qint16*>(samples.constData()),
                                        static_cast<int>(samples.size()));
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
    applyBackend();
    save();
    emit txChanged();
}

void CwController::setKeyerPort(const QString& port)
{
    if (m_keyerPort == port)
        return;
    m_keyerPort = port;
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
    if (m_winKeyerPort == port)
        return;
    m_winKeyerPort = port;
    applyBackend();
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

// Apre solo il manipolatore scelto: due strade aperte insieme vorrebbero dire
// due mani sullo stesso tasto.
void CwController::applyBackend()
{
    m_keyer.close();
    m_winKeyer.close();
    if (m_backend == QLatin1String("serial") && !m_keyerPort.isEmpty()) {
        if (!m_keyer.open(m_keyerPort, m_keyerLine))
            emit message(tr("The keyer port %1 did not open").arg(m_keyerPort), QStringLiteral("warning"));
    } else if (m_backend == QLatin1String("winkeyer") && !m_winKeyerPort.isEmpty()) {
        if (!m_winKeyer.open(m_winKeyerPort))
            emit message(tr("The WinKeyer on %1 did not open").arg(m_winKeyerPort), QStringLiteral("warning"));
        else
            m_winKeyer.setSpeed(m_wpm);
    }
}

bool CwController::canSend() const
{
    if (m_backend == QLatin1String("serial"))
        return m_keyer.isOpen();
    if (m_backend == QLatin1String("winkeyer"))
        return m_winKeyer.isOpen();
    return m_hooks.sendAudio && (!m_hooks.canTransmit || m_hooks.canTransmit());
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

void CwController::sendExpanded(const QString& ready)
{
    // Il tempo che il messaggio ci mette: serve a sapere quando l'audio finisce
    // (non lo dice nessuno) e a non lasciare un tasto acceso per sempre.
    const int ms = CwKeyer::millisFor(ready, m_wpm);
    if (m_backend == QLatin1String("winkeyer")) {
        m_winKeyer.send(ready, m_wpm);
        return;
    }
    if (m_backend == QLatin1String("serial")) {
        m_keyer.send(ready, m_wpm);
        return;
    }
    if (!m_hooks.sendAudio || !m_hooks.sendAudio(ready, m_wpm)) {
        setSending(false, -1);
        emit message(tr("The CW audio did not start"), QStringLiteral("warning"));
        return;
    }
    m_audioDone.start(ms + 800);
}

void CwController::stop()
{
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
