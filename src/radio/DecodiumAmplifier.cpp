#include "DecodiumAmplifier.h"

#include <QDebug>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QElapsedTimer>
#include <QStringList>
#include <QTimer>
#include <QtGlobal>

namespace {

// Richiesta di stato: tre byte di sincronismo, lunghezza, comando "Get
// Status", somma di controllo (di un byte solo, e' il byte stesso).
const char kRequest[] = { char(0x55), char(0x55), char(0x55),
                          char(0x01), char(0x90), char(0x90) };

constexpr int kSilenceMs = 5000;   // oltre il quale l'apparato e' considerato muto
constexpr int kMaxBuffer = 8192;

} // namespace


DecodiumAmplifier::DecodiumAmplifier(QObject* parent)
    : QObject(parent)
{
}

DecodiumAmplifier::~DecodiumAmplifier()
{
    configure(false, m_port, m_baud, m_passive, m_pollMs);
}

QList<DecodiumAmplifier::Trovato>
DecodiumAmplifier::cerca(const QStringList& daEscludere, int attesaMs)
{
    QList<Trovato> trovati;

    // Le velocita' da provare, dalla piu' probabile. La guida dice che
    // l'apparato si adatta da se' fino a 115200: si parte da li' e si scende,
    // cosi' nel caso normale si risponde alla prima.
    static const QList<int> velocita = {115200, 57600, 38400, 19200, 9600};

    QStringList escluse;
    for (const QString& p : daEscludere)
        escluse << p.trimmed().toUpper();

    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        QString const nome = info.portName();
        if (escluse.contains(nome.toUpper())) {
            qInfo().noquote() << "[AMP] ricerca: salto" << nome
                              << "(la sta usando il CAT)";
            continue;
        }
        // Se la porta e' in mano a un altro programma l'apertura fallisce da
        // se', qualche riga piu' sotto: non serve chiederlo prima. (In Qt 6
        // QSerialPortInfo::isBusy() non c'e' piu'.)

        for (int baud : velocita) {
            QSerialPort porta;
            porta.setPort(info);
            porta.setBaudRate(baud);
            porta.setDataBits(QSerialPort::Data8);
            porta.setParity(QSerialPort::NoParity);
            porta.setStopBits(QSerialPort::OneStop);
            porta.setFlowControl(QSerialPort::NoFlowControl);
            if (!porta.open(QIODevice::ReadWrite))
                break;   // se non si apre a una velocita' non si aprira' alle altre

            porta.write(statusRequest());
            porta.flush();

            // Si aspetta finche' arriva una trama intera o scade il tempo. Un
            // solo waitForReadyRead non basta: la risposta e' di ~75 byte e su
            // una seriale lenta arriva a pezzi.
            QByteArray risposta;
            QElapsedTimer orologio;
            orologio.start();
            while (orologio.elapsed() < attesaMs) {
                if (porta.waitForReadyRead(qMax(20, attesaMs / 4)))
                    risposta.append(porta.readAll());
                Reading const r = parseSpeStatus(risposta);
                if (r.valid) {
                    Trovato t;
                    t.porta = nome;
                    t.modello = r.model;
                    t.baud = baud;
                    t.descrizione = info.description();
                    trovati.append(t);
                    qInfo().noquote() << "[AMP] trovato" << r.model << "su" << nome
                                      << "a" << baud << "baud";
                    break;
                }
            }
            porta.close();
            if (!trovati.isEmpty() && trovati.last().porta == nome)
                break;   // trovato su questa porta: le altre velocita' non servono
        }
    }

    if (trovati.isEmpty())
        qInfo().noquote() << "[AMP] ricerca: nessun amplificatore ha risposto";
    return trovati;
}

QByteArray DecodiumAmplifier::statusRequest()
{
    return QByteArray(kRequest, int(sizeof kRequest));
}

bool DecodiumAmplifier::connected() const
{
    return m_serial && m_serial->isOpen();
}

QString DecodiumAmplifier::status() const
{
    if (!m_enabled)
        return QStringLiteral("off");
    if (!connected()) {
        // Nessuna porta indicata non e' un errore: e' una configurazione da
        // finire. Chiamarlo "errore" manda a cercare un guasto che non c'e'.
        if (m_port.trimmed().isEmpty())
            return QStringLiteral("noport");
        // Su Windows la seriale la tiene un solo programma alla volta, anche
        // in sola lettura. E' il caso tipico: il software del costruttore e'
        // aperto sulla stessa porta.
        if (m_lastErrorCode == int(QSerialPort::PermissionError))
            return QStringLiteral("busy");
        return m_lastError.isEmpty() ? QStringLiteral("error") : m_lastError;
    }
    return m_responding ? QStringLiteral("reading") : QStringLiteral("silent");
}

// ------------------------------------------------------------------- analisi
DecodiumAmplifier::Reading DecodiumAmplifier::parseSpeStatus(const QByteArray& frame)
{
    Reading r;
    int const i = frame.indexOf(QByteArray("\xAA\xAA\xAA", 3));
    if (i < 0 || frame.size() < i + 4)
        return r;

    int const n = quint8(frame.at(i + 3));
    if (frame.size() < i + 4 + n)
        return r;
    QByteArray const body = frame.mid(i + 4, n);

    // La somma di controllo non e' pignoleria: su una linea disturbata una
    // trama mutila darebbe letture assurde proprio mentre si trasmette a
    // piena potenza, che e' il momento peggiore per fidarsi di un numero.
    int sum = 0;
    for (unsigned char c : body)
        sum += c;
    if (frame.size() >= i + 6 + n) {
        int const c0 = quint8(frame.at(i + 4 + n));
        int const c1 = quint8(frame.at(i + 5 + n));
        if (c0 != (sum % 256) || c1 != ((sum / 256) % 256))
            return r;
    }

    QStringList const f = QString::fromLatin1(body).split(QLatin1Char(','));
    if (f.size() < 12)
        return r;

    bool ok = false;
    r.model = f.at(0).trimmed();
    r.transmitting = f.at(2).trimmed().toUpper() == QLatin1String("T");

    double const w = f.at(9).trimmed().toDouble(&ok);
    if (!ok)
        return r;
    r.watts = w;

    r.swrAtu = qMax(1.0, f.at(10).trimmed().toDouble());
    double const ant = f.at(11).trimmed().toDouble();
    r.swr = ant >= 1.0 ? ant : r.swrAtu;

    if (f.size() > 12) r.voltage = f.at(12).trimmed().toDouble();
    if (f.size() > 13) r.current = f.at(13).trimmed().toDouble();
    if (f.size() > 14) r.temperature = f.at(14).trimmed().toInt();
    if (f.size() > 17) r.warning = f.at(17).trimmed();
    if (f.size() > 18) r.alarm = f.at(18).trimmed();

    r.valid = true;
    return r;
}

QList<DecodiumAmplifier::Reading> DecodiumAmplifier::harvest(QByteArray& buffer)
{
    QList<Reading> out;
    // Sulla linea passano anche le richieste del software del costruttore:
    // si cerca il sincronismo e si scarta tutto il resto.
    for (;;) {
        int const i = buffer.indexOf(QByteArray("\xAA\xAA\xAA", 3));
        if (i < 0 || buffer.size() < i + 4)
            break;
        int const n = quint8(buffer.at(i + 3));
        int const end = i + 6 + n;
        if (buffer.size() < end)
            break;
        Reading const r = parseSpeStatus(buffer.mid(i, end - i));
        // La trama termina con CR LF: consumarli evita di lasciare code nel
        // buffer, che a lungo andare confondono la ricerca del sincronismo.
        int consumed = end;
        while (consumed < buffer.size()
               && (buffer.at(consumed) == 0x0D || buffer.at(consumed) == 0x0A))
            ++consumed;
        buffer.remove(0, consumed);
        if (r.valid)
            out.append(r);
    }
    if (buffer.size() > kMaxBuffer)
        buffer = buffer.right(1024);
    return out;
}

// ------------------------------------------------------------ configurazione
bool DecodiumAmplifier::configure(bool enabled, const QString& port, int baud,
                                  bool passive, int pollMs)
{
    bool const changed = enabled != m_enabled || port != m_port
                         || baud != m_baud || passive != m_passive
                         || pollMs != m_pollMs;

    m_enabled = enabled;
    m_port = port;
    m_baud = baud > 0 ? baud : 9600;
    m_passive = passive;
    m_pollMs = qBound(100, pollMs, 5000);

    if (m_serial) {
        m_serial->close();
        m_serial->deleteLater();
        m_serial = nullptr;
    }
    if (m_pollTimer) { m_pollTimer->stop(); m_pollTimer->deleteLater(); m_pollTimer = nullptr; }
    if (m_silenceTimer) { m_silenceTimer->stop(); m_silenceTimer->deleteLater(); m_silenceTimer = nullptr; }
    m_buffer.clear();
    m_responding = false;
    m_last = Reading{};
    m_lastError.clear();
    m_lastErrorCode = int(QSerialPort::NoError);

    if (!m_enabled || m_port.isEmpty()) {
        if (changed) emit configChanged();
        emit connectedChanged();
        emit telemetryChanged();
        if (m_enabled)
            qInfo().noquote() << "[AMP] nessuna porta indicata";
        else
            qInfo().noquote() << "[AMP] lettura amplificatore disattivata";
        return true;
    }

    m_serial = new QSerialPort(this);
    m_serial->setPortName(m_port);
    m_serial->setBaudRate(m_baud);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);
    connect(m_serial, &QSerialPort::readyRead, this, &DecodiumAmplifier::onReadyRead);

    // In ascolto passivo si apre in SOLA LETTURA: e' una garanzia verificabile
    // che sulla linea non finisca un nostro byte mentre il software del
    // costruttore sta dialogando con l'apparato.
    QIODevice::OpenMode const mode = m_passive ? QIODevice::ReadOnly : QIODevice::ReadWrite;
    if (!m_serial->open(mode)) {
        m_lastError = m_serial->errorString();
        m_lastErrorCode = int(m_serial->error());
        qWarning().noquote() << "[AMP] apertura di" << m_port << "fallita:" << m_lastError;
        m_serial->deleteLater();
        m_serial = nullptr;
        if (changed) emit configChanged();
        emit connectedChanged();
        return false;
    }

    m_silenceTimer = new QTimer(this);
    m_silenceTimer->setSingleShot(true);
    m_silenceTimer->setInterval(kSilenceMs);
    connect(m_silenceTimer, &QTimer::timeout, this, &DecodiumAmplifier::onSilence);
    m_silenceTimer->start();

    if (!m_passive) {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setInterval(m_pollMs);
        connect(m_pollTimer, &QTimer::timeout, this, &DecodiumAmplifier::onPoll);
        m_pollTimer->start();
        onPoll();
    }

    qInfo().noquote() << "[AMP] aperto" << m_port << "a" << m_baud << "baud,"
                      << (m_passive ? "ascolto passivo" : "interrogazione ogni")
                      << (m_passive ? QString() : QString::number(m_pollMs) + " ms");
    if (changed) emit configChanged();
    emit connectedChanged();
    return true;
}

// ------------------------------------------------------------------- lettura
void DecodiumAmplifier::onPoll()
{
    if (m_serial && m_serial->isOpen() && !m_passive)
        m_serial->write(statusRequest());
}

void DecodiumAmplifier::onReadyRead()
{
    if (!m_serial)
        return;
    m_buffer.append(m_serial->readAll());
    QList<Reading> const letture = harvest(m_buffer);
    if (letture.isEmpty())
        return;
    applyReading(letture.last());
}

void DecodiumAmplifier::applyReading(const Reading& r)
{
    m_last = r;
    if (!m_responding) {
        m_responding = true;
        qInfo().noquote() << "[AMP] risponde:" << r.model
                          << (r.transmitting ? "TX" : "RX")
                          << r.watts << "W ROS" << r.swr;
        emit connectedChanged();
    }
    if (m_silenceTimer)
        m_silenceTimer->start();
    emit telemetryChanged();
}

void DecodiumAmplifier::onSilence()
{
    if (!m_responding && !m_passive) {
        qWarning().noquote() << "[AMP]" << m_port
                             << "non risponde. Porta giusta? Velocita' giusta?";
    } else if (!m_responding) {
        qWarning().noquote() << "[AMP] nessuna trama sulla linea. Il software"
                                " dell'amplificatore sta interrogando l'apparato?";
    }
    if (m_responding) {
        m_responding = false;
        m_last = Reading{};
        emit telemetryChanged();
        emit connectedChanged();
    }
    if (m_silenceTimer)
        m_silenceTimer->start();
}
