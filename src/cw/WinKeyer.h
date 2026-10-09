// Decodium — il manipolatore K1EL WinKeyer (WK2, WK3, WKUSB, e i cloni che ne
// parlano il protocollo).
//
// Il WinKeyer fa da se' i tempi del CW: gli si manda il testo in ASCII e lui lo
// batte, con la velocita' che gli si da'. Porta seriale a 1200 baud, 8N2. Si
// apre con "host open" (0x00 0x02), che risponde con la versione; si chiude con
// 0x00 0x03. 0x02 n imposta la velocita', 0x0A svuota il buffer e ferma tutto.
// Il byte di stato (0xC0 | bit) dice quando ha finito.
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QSerialPort;

namespace decodium::cw {

namespace winkeyer {
// Il testo come lo vuole il WinKeyer: maiuscolo, solo i caratteri che conosce.
QByteArray textBytes(const QString& text);
QByteArray hostOpen();
QByteArray hostClose();
QByteArray speed(int wpm);
QByteArray clearBuffer();
} // namespace winkeyer

class WinKeyer : public QObject {
    Q_OBJECT

public:
    explicit WinKeyer(QObject* parent = nullptr);
    ~WinKeyer() override;

    bool open(const QString& port);
    void close();
    bool isOpen() const;
    int version() const { return m_version; }
    bool busy() const { return m_busy; }

    void send(const QString& text, int wpm);
    void setSpeed(int wpm);
    void stop();

signals:
    void failed(const QString& message);
    void versionReceived(int version);
    void busyChanged(bool busy);

private:
    void onReadyRead();

    QSerialPort* m_port{nullptr};
    int m_version{0};
    int m_wpm{0};
    bool m_busy{false};
};

} // namespace decodium::cw
