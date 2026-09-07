#include "link/DecodiumLink.h"

#include <QObject>
#include <QtGlobal>
#include <QDebug>

namespace decortty::link {

namespace {
// Ogni quarto di secondo. Il CAT di Decodium interroga la radio per conto suo
// con la sua cadenza; qui non si parla con nessun apparato, si rilegge quello
// che l'applicazione ha gia' in memoria. Quattro volte al secondo bastano
// perche' la manopola sembri viva e non costano niente.
constexpr int kIntervalloSondaggioMs = 250;

bool isQmxFamily(QString const& radioName)
{
    return radioName.contains(QStringLiteral("QMX"), Qt::CaseInsensitive);
}
} // namespace

DecodiumLink::DecodiumLink(Ganci ganci, QObject* parent)
    : RadioLink(parent)
    , m_ganci(std::move(ganci))
{
    m_stato = tr("radio di Decodium");

    m_sonda = new QTimer(this);
    m_sonda->setInterval(kIntervalloSondaggioMs);
    connect(m_sonda, &QTimer::timeout, this, &DecodiumLink::sondaggio);
    m_sonda->start();
    sondaggio();
}

DecodiumLink::~DecodiumLink() = default;

bool DecodiumLink::isConnected() const
{
    return m_ganci.connesso ? m_ganci.connesso() : false;
}

QString DecodiumLink::radioName() const
{
    if (!m_ganci.nomeRadio)
        return tr("radio di Decodium");
    QString const nome = m_ganci.nomeRadio();
    return nome.isEmpty() ? tr("radio di Decodium") : nome;
}

bool DecodiumLink::requiresFullScaleTransmitAudio() const
{
    return isQmxFamily(radioName());
}

double DecodiumLink::frequencyMhz() const
{
    if (!m_ganci.frequenzaHz)
        return 0.0;
    return m_ganci.frequenzaHz() / 1e6;
}

QString DecodiumLink::mode() const
{
    return m_ganci.modo ? m_ganci.modo() : QString();
}

bool DecodiumLink::isTransmitting() const
{
    return m_ganci.inTrasmissione ? m_ganci.inTrasmissione() : false;
}

bool DecodiumLink::canTransmit() const
{
    // Senza il gancio non si dichiara la trasmissione: meglio un pulsante
    // spento di uno che promette e non mantiene.
    return m_ganci.puoTrasmettere ? m_ganci.puoTrasmettere() : false;
}

void DecodiumLink::disconnectRadio()
{
    // Non c'e' niente da chiudere: la radio non l'abbiamo aperta noi. Staccare
    // qui il CAT dell'applicazione vorrebbe dire lasciare senza radio anche
    // FT8, che sta lavorando nella stessa finestra.
    if (m_sonda)
        m_sonda->stop();
}

void DecodiumLink::setFrequencyMhz(double mhz)
{
    if (m_ganci.impostaFrequenzaHz && mhz > 0.0)
        m_ganci.impostaFrequenzaHz(mhz * 1e6);
}

void DecodiumLink::setMode(const QString& mode)
{
    if (!m_ganci.impostaModo || mode.isEmpty())
        return;

    QString requested = mode.trimmed().toUpper();
    // Hamlib exposes QMX FSK as PKTUSB/PKTLSB (Decodium DATA-U/DATA-L), which
    // is also the Digi mode required by the QMX manual for USB-audio RTTY.
    // Translate the operator-facing RTTY labels at this radio boundary rather
    // than asking the QMX backend for unsupported RTTY/RTTYR modes.
    if (requiresFullScaleTransmitAudio()) {
        if (requested == QStringLiteral("RTTY-U")
            || requested == QStringLiteral("RTTY"))
            requested = QStringLiteral("DATA-U");
        else if (requested == QStringLiteral("RTTY-L"))
            requested = QStringLiteral("DATA-L");
    }
    m_ganci.impostaModo(requested);
}

void DecodiumLink::setFilter(int lowHz, int highHz)
{
    Q_UNUSED(lowHz)
    Q_UNUSED(highHz)
}

void DecodiumLink::applyRttyProfile(int markHz, int shiftHz)
{
    Q_UNUSED(markHz)
    Q_UNUSED(shiftHz)
    // Mette la radio in dati banda laterale superiore, che e' il modo giusto
    // per l'AFSK. Il filtro non si tocca: quello lo governa Decodium con le
    // sue impostazioni, e stringerlo da qui lo stringerebbe anche per FT8, che
    // sta decodificando sulla stessa radio.
    //
    // Cambiare modo e' un gesto vistoso su una radio condivisa, e infatti non
    // si fa da soli: succede solo quando l'operatore preme "Prepara radio".
    // Prima questa funzione era vuota, e quel pulsante non faceva niente —
    // un comando che non fa nulla e' peggio di un comando che manca.
    setMode(QStringLiteral("DATA-U"));
}

void DecodiumLink::setTransmit(bool on)
{
    if (m_ganci.impostaPtt)
        m_ganci.impostaPtt(on);
}

int DecodiumLink::sendTransmitAudio(const float* samples, int count)
{
    if (!m_ganci.mandaAudioTx || !samples || count <= 1)
        return 0;

    // Il modulatore genera a 24 kHz, l'uscita di Decodium prende 12 e da li'
    // interpola verso la scheda. Si dimezza percio' la frequenza facendo la
    // media dei campioni a due a due invece di scartarne uno: qui il segnale va
    // in aria, e quel che si butta via senza filtrare torna come banda occupata
    // dove non la si vuole.
    int const coppie = count / 2;
    m_txBuffer.resize(coppie);
    for (int i = 0; i < coppie; ++i) {
        float const medio = 0.5f * (samples[i * 2] + samples[i * 2 + 1]);
        float const scalato = qBound(-32768.0f, medio * 32767.0f, 32767.0f);
        m_txBuffer[i] = static_cast<short>(scalato);
    }
    m_ganci.mandaAudioTx(m_txBuffer);
    return coppie;
}

void DecodiumLink::consegnaAudio(const std::vector<float>& campioni, int frames)
{
    if (frames <= 0 || campioni.empty())
        return;
    emit audioReady(campioni, frames);
}

void DecodiumLink::sondaggio()
{
    // Un segnale per ogni cosa che e' cambiata, nessuno per quelle ferme: i
    // binding del QML si rivalutano a ogni emissione, e emetterli quattro
    // volte al secondo a vuoto ridisegnerebbe i pannelli per niente.
    bool const connesso = isConnected();
    if (connesso != m_ultimoConnesso) {
        m_ultimoConnesso = connesso;
        m_stato = connesso ? tr("collegato: %1").arg(radioName())
                           : tr("radio di Decodium non collegata");
        // Una riga nel log a ogni cambiamento, mai a ogni sondaggio. Quello che
        // RTTY crede della radio e quello che il CAT dell'applicazione riporta
        // devono coincidere, e quando l'interfaccia dice "CAT non connesso"
        // mentre la frequenza scorre, e' qui che si vede chi dei due ha torto.
        qInfo().noquote()
            << "[RTTY] radio:" << (connesso ? "collegata" : "non collegata")
            << "- nome" << radioName()
            << "- comandabile" << canControl()
            << "- trasmissione possibile" << canTransmit();
        emit connectionChanged();
        emit statusTextChanged();
    }

    double const hz = m_ganci.frequenzaHz ? m_ganci.frequenzaHz() : 0.0;
    QString const modo = mode();
    // Un hertz di differenza e' rumore di arrotondamento fra la lettura del CAT
    // e il valore logico; sotto quella soglia non e' successo niente.
    if (qAbs(hz - m_ultimaFrequenzaHz) >= 1.0 || modo != m_ultimoModo) {
        m_ultimaFrequenzaHz = hz;
        m_ultimoModo = modo;
        emit sliceChanged();
    }

    bool const tx = isTransmitting();
    if (tx != m_ultimaTrasmissione) {
        m_ultimaTrasmissione = tx;
        emit transmittingChanged();
    }
}

} // namespace decortty::link
