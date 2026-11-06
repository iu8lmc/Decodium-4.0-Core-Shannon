// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include "DecodiumSerialPortRegistry.h"

class QIODevice;
class QSerialPort;
class QTcpSocket;

// I parametri di una linea seriale. Per le radio vengono dalla libreria, cioe'
// da Hamlib: velocita', bit, parita' e handshake non si indovinano.
struct DecoPortLineSettings {
    int     baud {9600};
    int     dataBits {8};
    int     stopBits {1};
    QChar   parity {QLatin1Char('N')};
    QString handshake {QStringLiteral("none")};   // none, hardware, xonxoff
    // Le linee di controllo. -1 lascia stare: su molte radio RTS o DTR alzati
    // significano trasmissione, e una linea toccata per sbaglio manda in aria.
    int     rts {-1};
    int     dtr {-1};
};

// Una linea seriale, vera o in rete.
//
//   "COM5", "/dev/ttyUSB0"   una porta del sistema
//   "CNCB0"                  l'estremo nascosto di una coppia virtuale
//   "tcp:host:porta"         un server seriale in rete (ser2net, RemoteRig,
//                            un simulatore): stesso flusso di byte, senza
//                            velocita' ne' linee di controllo
//
// Chi la usa legge e scrive su device() e non sa quale delle tre sia.
class DecoPortSerialLine : public QObject
{
    Q_OBJECT

public:
    explicit DecoPortSerialLine(QObject* parent = nullptr);
    ~DecoPortSerialLine() override;

    static bool isNetworkSpec(const QString& spec);

    // Per una porta di rete aspetta la connessione fino a `connectTimeoutMs`.
    bool open(const QString& spec, const DecoPortLineSettings& settings,
              int connectTimeoutMs = 3000,
              const QString& owner = QString());
    void close();
    bool isOpen() const;
    bool isNetwork() const { return m_tcp != nullptr; }

    QIODevice* device() const;
    QString spec() const { return m_spec; }
    QString errorString() const { return m_error; }

    // DSR della porta, cioe' il DTR di chi sta dall'altra parte di una coppia
    // virtuale: dice se un programma ha la porta aperta. -1 se non si sa.
    int dsr() const;
    // DCD della porta. Sulle coppie che crea DecoPort e' collegato all'apertura
    // dell'altra parte (com0com dcd=ropen): dice se un programma ha la porta
    // aperta anche quando quel programma non alza mai DTR. -1 se non si sa.
    int dcd() const;

signals:
    void readyRead();
    // La porta e' sparita (USB staccata) o la connessione e' caduta.
    void lost(const QString& why);

private:
    QString      m_spec;
    QString      m_error;
    QSerialPort* m_serial {nullptr};
    QTcpSocket*  m_tcp {nullptr};
    decodium::ports::SerialPortLease m_serialLease;
};
