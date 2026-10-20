// Decodium — il manipolatore CW su una porta seriale.
//
// Il CW via CAT va bene finche' il CAT e' nostro. Ma chi opera con Decodium
// aperto la porta della radio ce l'ha gia' occupata: il CAT passa dal ponte di
// Decodium, e quel ponte il manipolatore non lo sa fare. Allora il CW lo
// facciamo qui, come si e' sempre fatto: si alza e si abbassa un piedino
// (DTR o RTS) di un'altra porta seriale, quella attaccata al circuitino di
// manipolazione, e la radio va in CW senza che nessuno debba mollare il CAT.
//
// I tempi contano: a 30 parole al minuto un punto dura 40 millisecondi, e i
// timer di Windows a quella scala ballano di brutto. Per questo la manipolazione
// gira in un thread suo, che dorme a colpi corti e chiude l'attesa contando i
// microsecondi — cosi' la spaziatura resta regolare anche mentre l'interfaccia
// fa altro.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include <memory>

class QThread;
class QSerialPort;

namespace decodium::cw {

class CwKeyerWorker;

class CwKeyer : public QObject {
    Q_OBJECT

public:
    explicit CwKeyer(QObject* parent = nullptr);
    ~CwKeyer() override;

    // La porta ("COM4") e il piedino ("DTR" o "RTS"). Torna false se la porta
    // non si apre: il motivo arriva con failed().
    bool open(const QString& port, const QString& line);
    void close();
    bool isOpen() const { return m_open; }
    QString portName() const { return m_port; }

    // Manda il testo in CW. Se ne sta gia' mandando uno, questo si accoda.
    void send(const QString& text, int wpm);
    // Tutto fermo, subito: il piedino torna giu' e la coda si svuota.
    void stop();
    bool sending() const { return m_sending; }

    // Le porte seriali di questo computer, per scegliere.
    static QStringList ports();
    // La traduzione in punti e linee, per i test e per chi vuole guardarla.
    static QString morseOf(QChar c);
    // Quanto dura questo messaggio a questa velocita', in millisecondi: serve
    // a sapere se una macro ci sta nel giro prima che il corrispondente
    // ricominci a chiamare.
    static int millisFor(const QString& text, int wpm);

signals:
    // Mandato per davvero, carattere per carattere: l'interfaccia lo mostra
    // mentre esce, non dopo.
    void charSent(const QString& character);
    void textSent(const QString& text);
    void finished();
    void failed(const QString& why);

private:
    QThread* m_thread{nullptr};
    CwKeyerWorker* m_worker{nullptr};
    QString m_port;
    bool m_open{false};
    bool m_sending{false};
    // Ogni invio ha un numero nuovo. Ferma invalida tutti quelli precedenti,
    // senza che un invio appena arrivato possa riattivarli per errore.
    std::atomic<quint64> m_generation{0};
};

} // namespace decodium::cw
