#include "CwKeyer.h"
#include "CwSerialPorts.h"

#include <QElapsedTimer>
#include <QSerialPort>
#include <QThread>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>

namespace decodium::cw {

namespace {

// La tavola del codice Morse. Quella che manca da qui non si manda: meglio un
// carattere saltato che un carattere sbagliato in aria.
struct Letter {
    char c;
    const char* code;
};

constexpr Letter kTable[] = {
    {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},   {'E', "."},
    {'F', "..-."},  {'G', "--."},   {'H', "...."},  {'I', ".."},    {'J', ".---"},
    {'K', "-.-"},   {'L', ".-.."},  {'M', "--"},    {'N', "-."},    {'O', "---"},
    {'P', ".--."},  {'Q', "--.-"},  {'R', ".-."},   {'S', "..."},   {'T', "-"},
    {'U', "..-"},   {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},  {'Y', "-.--"},
    {'Z', "--.."},
    {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"}, {'4', "....-"},
    {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."},
    {'/', "-..-."}, {'?', "..--.."},{',', "--..--"},{'.', ".-.-.-"},{'=', "-...-"},
    {'+', ".-.-."}, {'-', "-....-"},{'@', ".--.-."},{':', "---..."},{'\'', ".----."},
};

// Aspetta il tempo giusto: si dorme a colpi corti — l'attesa lunga su Windows
// sbaglia di dieci millisecondi e piu' — e gli ultimi due si contano fermi qui,
// che e' l'unico modo di avere una spaziatura che non traballa.
void waitFor(qint64 microseconds, const std::atomic<quint64>& cancelledThrough, quint64 generation)
{
    QElapsedTimer clock;
    clock.start();
    while (cancelledThrough.load() < generation) {
        const qint64 left = microseconds - clock.nsecsElapsed() / 1000;
        if (left <= 0)
            return;
        if (left > 2000)
            QThread::usleep(static_cast<unsigned long>(qMin<qint64>(left - 2000, 5000)));
        else
            QThread::yieldCurrentThread();
    }
}

} // namespace

// ── Il lavoratore: vive nel suo thread e tiene in mano la porta ─────────────

class CwKeyerWorker : public QObject {
    Q_OBJECT

public:
    ~CwKeyerWorker() override { closePort(); }

public slots:
    void openPort(const QString& name, const QString& line)
    {
        closePort();
        m_line = line.trimmed().toUpper() == QLatin1String("RTS") ? Line::Rts : Line::Dtr;
        m_port = new QSerialPort(name);
        // Niente handshake: il piedino lo comandiamo noi, e se lo comanda anche
        // il driver si accavallano.
        m_port->setBaudRate(QSerialPort::Baud9600);
        m_port->setFlowControl(QSerialPort::NoFlowControl);
        if (!m_port->open(QIODevice::ReadWrite)) {
            const QString why = m_port->errorString();
            delete m_port;
            m_port = nullptr;
            emit failed(tr("Cannot open %1: %2").arg(name, why));
            emit opened(false);
            return;
        }
        key(false);
        emit opened(true);
    }

    void closePort()
    {
        if (!m_port)
            return;
        key(false);
        m_port->close();
        delete m_port;
        m_port = nullptr;
    }

    void enqueue(const QString& text, int wpm, quint64 generation)
    {
        {
            QMutexLocker lock(&m_mutex);
            m_queue.enqueue({text, qBound(5, wpm, 60), generation});
        }
        QMetaObject::invokeMethod(this, "drain", Qt::QueuedConnection);
    }

    void cancelThrough(quint64 generation)
    {
        quint64 current = m_cancelledThrough.load();
        while (current < generation
               && !m_cancelledThrough.compare_exchange_weak(current, generation)) {
        }
        {
            QMutexLocker lock(&m_mutex);
            m_queue.clear();
        }
        // Se non sta battendo nulla il worker deve comunque togliere il
        // piedino. Il numero evita che questo evento vecchio abbassi la linea
        // nel mezzo di una macro nuova arrivata subito dopo Ferma.
        QMetaObject::invokeMethod(this, "releaseKey", Qt::QueuedConnection,
                                  Q_ARG(quint64, generation));
    }

    void releaseKey(quint64 generation)
    {
        if (m_activeGeneration.load() <= generation)
            key(false);
    }

    Q_INVOKABLE void drain()
    {
        if (m_busy)
            return;
        m_busy = true;
        forever {
            Job next;
            {
                QMutexLocker lock(&m_mutex);
                if (m_queue.isEmpty())
                    break;
                next = m_queue.dequeue();
            }
            if (cancelled(next.generation))
                continue;
            if (sendNow(next.text, next.wpm, next.generation))
                emit textSent(next.text);
        }
        m_busy = false;
        emit finished();
    }

signals:
    void opened(bool ok);
    void charSent(const QString& character);
    void textSent(const QString& text);
    void finished();
    void failed(const QString& why);

private:
    enum class Line { Dtr, Rts };

    struct Job {
        QString text;
        int wpm{24};
        quint64 generation{0};
    };

    bool cancelled(quint64 generation) const
    {
        return m_cancelledThrough.load() >= generation;
    }

    void key(bool down)
    {
        if (!m_port)
            return;
        if (m_line == Line::Rts)
            m_port->setRequestToSend(down);
        else
            m_port->setDataTerminalReady(down);
    }

    bool sendNow(const QString& text, int wpm, quint64 generation)
    {
        if (!m_port || cancelled(generation))
            return false;
        m_activeGeneration.store(generation);
        bool complete = true;
        // Il punto: 1200 diviso le parole al minuto, come dice PARIS.
        const qint64 dot = 1200'000 / wpm;     // microsecondi
        for (const QChar raw : text.toUpper()) {
            if (cancelled(generation)) {
                complete = false;
                break;
            }
            if (raw == QLatin1Char(' ')) {
                // Fra due parole sette punti; tre sono gia' passati con
                // l'ultima lettera.
                waitFor(dot * 4, m_cancelledThrough, generation);
                if (cancelled(generation)) {
                    complete = false;
                    break;
                }
                emit charSent(QStringLiteral(" "));
                continue;
            }
            const QString code = CwKeyer::morseOf(raw);
            if (code.isEmpty())
                continue;
            for (qsizetype i = 0; i < code.size(); ++i) {
                if (cancelled(generation)) {
                    complete = false;
                    break;
                }
                key(true);
                waitFor(code.at(i) == QLatin1Char('-') ? dot * 3 : dot, m_cancelledThrough, generation);
                key(false);
                if (cancelled(generation)) {
                    complete = false;
                    break;
                }
                // Fra un elemento e l'altro un punto di silenzio.
                if (i + 1 < code.size())
                    waitFor(dot, m_cancelledThrough, generation);
            }
            if (!complete)
                break;
            emit charSent(QString(raw));
            // Fra due lettere tre punti: uno l'ha gia' fatto l'elemento.
            waitFor(dot * 2, m_cancelledThrough, generation);
            if (cancelled(generation)) {
                complete = false;
                break;
            }
        }
        key(false);
        if (m_activeGeneration.load() == generation)
            m_activeGeneration.store(0);
        return complete && !cancelled(generation);
    }

    QSerialPort* m_port{nullptr};
    Line m_line{Line::Dtr};
    QQueue<Job> m_queue;
    QMutex m_mutex;
    std::atomic<quint64> m_cancelledThrough{0};
    std::atomic<quint64> m_activeGeneration{0};
    bool m_busy{false};
};

// ── La faccia pubblica ──────────────────────────────────────────────────────

CwKeyer::CwKeyer(QObject* parent)
    : QObject(parent)
    , m_thread(new QThread(this))
    , m_worker(new CwKeyerWorker)
{
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &CwKeyerWorker::charSent, this, &CwKeyer::charSent);
    connect(m_worker, &CwKeyerWorker::textSent, this, &CwKeyer::textSent);
    connect(m_worker, &CwKeyerWorker::failed, this, &CwKeyer::failed);
    connect(m_worker, &CwKeyerWorker::finished, this, [this] {
        m_sending = false;
        emit finished();
    });
    connect(m_worker, &CwKeyerWorker::opened, this, [this](bool ok) { m_open = ok; });
    // La manipolazione vuole precedenza: un ritardo qui si sente in aria.
    m_thread->start(QThread::TimeCriticalPriority);
}

CwKeyer::~CwKeyer()
{
    stop();
    // Non aspettare con una chiamata bloccante il worker: se l'uscita arriva
    // mentre sta manipolando un carattere, quella attesa puo' tenere vivo il
    // processo anche dopo la chiusura della finestra. stop() invalida in modo
    // atomico il messaggio in corso, quindi il worker esce subito dall'attesa
    // Morse e processa queste due richieste nell'ordine.
    if (!m_thread->isRunning())
        return;
    QMetaObject::invokeMethod(m_worker, "closePort", Qt::QueuedConnection);
    m_thread->quit();
    if (!m_thread->wait(1500)) {
        // Ultima rete di sicurezza: un driver seriale difettoso non deve mai
        // rendere impossibile terminare Decodium.
        m_thread->terminate();
        m_thread->wait(500);
    }
}

bool CwKeyer::open(const QString& port, const QString& line)
{
    if (port.trimmed().isEmpty()) {
        emit failed(tr("No serial port chosen for the CW keyer"));
        return false;
    }
    m_port = port.trimmed();
    m_open = false;
    QMetaObject::invokeMethod(m_worker, "openPort", Qt::BlockingQueuedConnection,
                              Q_ARG(QString, m_port), Q_ARG(QString, line));
    return m_open;
}

void CwKeyer::close()
{
    stop();
    QMetaObject::invokeMethod(m_worker, "closePort", Qt::BlockingQueuedConnection);
    m_open = false;
    m_port.clear();
}

void CwKeyer::send(const QString& text, int wpm)
{
    if (!m_open || text.trimmed().isEmpty())
        return;
    m_sending = true;
    const quint64 generation = m_generation.fetch_add(1) + 1;
    m_worker->enqueue(text, wpm, generation);
}

void CwKeyer::stop()
{
    const quint64 generation = m_generation.fetch_add(1) + 1;
    m_worker->cancelThrough(generation);
    m_sending = false;
}

QStringList CwKeyer::ports()
{
    return availableSerialPorts();
}

int CwKeyer::millisFor(const QString& text, int wpm)
{
    const int dot = 1200 / qBound(5, wpm, 60);
    int total = 0;
    bool first = true;
    for (const QChar c : text.toUpper()) {
        if (c == QLatin1Char(' ')) {
            total += dot * 7;          // sette punti fra due parole
            first = true;
            continue;
        }
        const QString code = morseOf(c);
        if (code.isEmpty())
            continue;
        if (!first)
            total += dot * 3;          // tre punti fra una lettera e l'altra
        first = false;
        for (qsizetype i = 0; i < code.size(); ++i) {
            total += code.at(i) == QLatin1Char('-') ? dot * 3 : dot;
            if (i + 1 < code.size())
                total += dot;          // un punto fra gli elementi
        }
    }
    return total;
}

QString CwKeyer::morseOf(QChar c)
{
    const char upper = c.toUpper().toLatin1();
    for (const Letter& letter : kTable) {
        if (letter.c == upper)
            return QString::fromLatin1(letter.code);
    }
    return {};
}

} // namespace decodium::cw

#include "CwKeyer.moc"
