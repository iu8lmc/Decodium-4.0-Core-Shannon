#include "link/RadioHub.h"

#include "app/BandPlan.h"

#include <QVariantMap>

#include <algorithm>

namespace decortty::link {

RadioHub::RadioHub(QObject* parent)
    : QObject(parent)
    , m_idleStatus(tr("Disconnected"))
{
}

RadioHub::~RadioHub()
{
    releaseLink();
}

// ── state, forwarded from whichever link is live ────────────────────────────

bool    RadioHub::connected() const  { return m_link && m_link->isConnected(); }
QString RadioHub::sharedWith() const
{
    return m_link ? m_link->sharedWith() : QString();
}

QString RadioHub::statusText() const { return m_link ? m_link->statusText() : m_idleStatus; }
QString RadioHub::radioName() const  { return m_link ? m_link->radioName() : QString(); }
double  RadioHub::frequencyMhz() const { return m_link ? m_link->frequencyMhz() : 0.0; }
QString RadioHub::mode() const       { return m_link ? m_link->mode() : QString(); }
bool    RadioHub::transmitting() const { return m_link && m_link->isTransmitting(); }
bool    RadioHub::canTransmit() const  { return m_link && m_link->canTransmit(); }
bool    RadioHub::requiresFullScaleTransmitAudio() const
{
    return m_link && m_link->requiresFullScaleTransmitAudio();
}
bool    RadioHub::canControl() const   { return m_link && m_link->canControl(); }
int     RadioHub::currentBand() const  { return app::bandAt(frequencyMhz()); }

QVariantList RadioHub::bands() const
{
    QVariantList list;
    for (const auto& band : app::bandPlan()) {
        QVariantMap entry;
        entry[QStringLiteral("name")] = band.name;
        entry[QStringLiteral("mhz")]  = band.rttyMhz;
        // Gli estremi servono al righello, che deve sapere dove colorare.
        entry[QStringLiteral("low")]  = band.lowMhz;
        entry[QStringLiteral("high")] = band.highMhz;
        list.append(entry);
    }
    return list;
}

QStringList RadioHub::modes() const
{
    // RTTY-U per primo: e' il modo dell'RTTY vero, quello in cui l'apparato
    // stringe il filtro attorno ai due toni, ed e' li' che si copia meglio.
    //
    // Attenzione a cosa comporta in trasmissione, perche' i due gruppi non sono
    // intercambiabili: nei modi RTTY l'apparato aspetta il tasto FSK e genera i
    // toni per conto suo, quindi l'audio AFSK che manderemmo dalla scheda non
    // esce. Chi vuole trasmettere da qui deve stare su DIGU o DIGL, dove il
    // filtro e' piu' largo ma l'audio modula davvero. Il suggerimento di ogni
    // pulsante lo dice, invece di lasciarlo scoprire premendo il tasto e non
    // sentendo nulla in aria.
    return { QStringLiteral("RTTY-U"), QStringLiteral("RTTY-L"),
             QStringLiteral("DIGU"),   QStringLiteral("DIGL"),
             QStringLiteral("USB"),    QStringLiteral("LSB") };
}
int     RadioHub::signalStrengthDbm() const { return m_link ? m_link->signalStrengthDbm() : -140; }

// ── link lifecycle ──────────────────────────────────────────────────────────

void RadioHub::releaseLink()
{
    if (!m_link)
        return;
    m_link->disconnectRadio();
    m_link->deleteLater();
    m_link = nullptr;
    m_decodium = nullptr;
}

void RadioHub::adopt(RadioLink* link)
{
    releaseLink();
    m_link = link;

    // Everything the link reports becomes ours. Re-emitting rather than letting
    // QML bind to the link directly is what keeps the interface from having to
    // rebind every time the operator changes radio.
    connect(link, &RadioLink::connectionChanged,  this, &RadioHub::connectionChanged);
    connect(link, &RadioLink::statusTextChanged,  this, &RadioHub::statusTextChanged);
    connect(link, &RadioLink::sliceChanged,       this, &RadioHub::sliceChanged);
    connect(link, &RadioLink::transmittingChanged, this, &RadioHub::transmittingChanged);
    connect(link, &RadioLink::metersChanged,      this, &RadioHub::metersChanged);
    connect(link, &RadioLink::errorOccurred,      this, &RadioHub::errorOccurred);
    connect(link, &RadioLink::audioReady,         this, &RadioHub::audioReady);

    emit connectionChanged();
    emit statusTextChanged();
}

void RadioHub::collegaADecodium(DecodiumLink::Ganci ganci)
{
    auto* link = new DecodiumLink(std::move(ganci), this);
    adopt(link);
    m_decodium = link;
    emit connectionChanged();
    emit sliceChanged();
}

void RadioHub::consegnaAudioDecodium(const std::vector<float>& campioni, int frames)
{
    // Solo quando la radio in uso e' quella di Decodium: con una scheda audio
    // collegata i campioni arrivano da li', e sovrapporre le due sorgenti
    // darebbe al decodificatore due segnali mescolati invece di uno.
    if (m_decodium && m_link == m_decodium)
        m_decodium->consegnaAudio(campioni, frames);
}

// ── control, forwarded ──────────────────────────────────────────────────────

void RadioHub::tuneToBand(int index)
{
    const auto& plan = app::bandPlan();
    if (index < 0 || index >= static_cast<int>(plan.size()) || !m_link)
        return;
    if (index == currentBand())
        return;
    m_link->setFrequencyMhz(plan[index].rttyMhz);
}

void RadioHub::setFrequencyMhz(double mhz)
{
    if (m_link) m_link->setFrequencyMhz(mhz);
}

void RadioHub::setMode(const QString& mode)
{
    if (m_link) m_link->setMode(mode);
}

void RadioHub::setFilter(int lowHz, int highHz)
{
    if (m_link) m_link->setFilter(lowHz, highHz);
}

void RadioHub::applyRttyProfile(int markHz, int shiftHz)
{
    if (m_link) m_link->applyRttyProfile(markHz, shiftHz);
}

void RadioHub::setTransmit(bool on)
{
    if (!m_link)
        return;
    if (on && !m_link->canTransmit()) {
        emit errorOccurred(tr("Transmit is unavailable on this connection"));
        return;
    }
    m_link->setTransmit(on);
}

int RadioHub::sendTransmitAudio(const float* samples, int count)
{
    return m_link ? m_link->sendTransmitAudio(samples, count) : 0;
}

} // namespace decortty::link
