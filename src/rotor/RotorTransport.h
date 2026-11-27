// Trasporto seriale verso il control box, piu' un control box simulato.
//
// Il trasporto sa solo scrivere frame e leggerne uno di risposta; la logica del
// rotore (limiti, target, watchdog) sta in RotorController. Tutto qui e'
// bloccante e va usato da un solo thread: quello del rotore.
#pragma once

#include "RotorConfig.h"
#include "RotorProtocol.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QSerialPort>
#include <QString>

#include <functional>
#include <memory>
#include <stdexcept>

namespace decodium::rotor {

// La porta non si apre o la comunicazione e' caduta.
struct TransportError : std::runtime_error {
    explicit TransportError(const QString& what) : std::runtime_error(what.toStdString()) {}
};

class Transport {
public:
    virtual ~Transport() = default;
    virtual void open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual void write(const QByteArray& frame) = 0;
    // Un frame STX..CR, o un array vuoto se non ne arriva uno entro il tempo.
    virtual QByteArray readFrame() = 0;
};

// Porta seriale reale: 9600 8N1, nessun handshake.
class SerialTransport : public Transport {
public:
    SerialTransport(const QString& port, int baudrate = 9600, double timeoutSeconds = 3.0);
    ~SerialTransport() override;
    void open() override;
    void close() override;
    bool isOpen() const override;
    void write(const QByteArray& frame) override;
    QByteArray readFrame() override;

private:
    QString m_portName;
    int m_baud;
    int m_timeoutMs;
    std::unique_ptr<QSerialPort> m_port;
};

// Control box virtuale: permette di provare tutto senza rotore. Emula assi che
// ruotano a velocita' costante verso il target e risponde con gli stessi frame
// del control box vero, solo agli ID che possiede, perche' l'autorilevamento
// possa distinguere i modelli.
class SimulatedTransport : public Transport {
public:
    static constexpr double kSpeedDegPerSecond = 4.0;   // ~90 s per un giro, come un PST-61D

    explicit SimulatedTransport(const QString& modelKey = QStringLiteral("d_azel"),
                                double az = 0.0, double el = 0.0);
    void open() override { m_open = true; }
    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }
    void write(const QByteArray& frame) override;
    QByteArray readFrame() override;

private:
    void advance();

    Model m_model;
    QHash<char, double> m_position;
    QHash<char, double> m_target;
    QElapsedTimer m_clock;
    qint64 m_lastMs {0};
    QByteArray m_pending;
    QMutex m_lock;
    bool m_open {false};
};

// Transazione comando/risposta con ripetizioni.
class Link {
public:
    using FrameHook = std::function<void(const QString& direction, const QByteArray& frame)>;

    Link(std::unique_ptr<Transport> transport, int retries, FrameHook onFrame = {});
    bool isOpen() const { return m_transport && m_transport->isOpen(); }
    void open() { m_transport->open(); }
    void close() { m_transport->close(); }

    // Manda un comando senza attendere risposta (goto, stop, CPM).
    void send(const QByteArray& frame);
    // Manda un comando e decodifica la risposta. False se il control box tace
    // o risponde con frame sporchi per tutti i tentativi.
    bool transact(const QByteArray& frame, int multiplier, Reply* reply);

private:
    void notify(const QString& direction, const QByteArray& frame);

    std::unique_ptr<Transport> m_transport;
    int m_retries;
    FrameHook m_onFrame;
};

}  // namespace decodium::rotor
