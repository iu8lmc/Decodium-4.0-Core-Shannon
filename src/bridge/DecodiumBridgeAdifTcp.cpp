#include "DecodiumBridge.h"
#include "UdpTargetHelpers.h"
#include <QTcpSocket>
#include <QTimer>

void decodiumBridgeAdifTcpLog(const QString&);

void DecodiumBridge::tcpSendLoggedAdifQso(const QString& dxCall, const QByteArray& adifRecord)
{
    if (!getSetting(QStringLiteral("ADIFTcpEnabled"), false).toBool()) {
        return;
    }

    QString const serverName = getSetting(QStringLiteral("ADIFTcpServer"), QStringLiteral("127.0.0.1")).toString().trimmed();
    quint16 const port = udpPortFromSettingValue(getSetting(QStringLiteral("ADIFTcpPort"), 52001), 52001);
    if (serverName.isEmpty() || adifRecord.trimmed().isEmpty()) {
        decodiumBridgeAdifTcpLog(QStringLiteral("ADIF TCP skipped: missing server or empty ADIF record"));
        return;
    }

    auto* socket = new QTcpSocket(this);
    auto* timeout = new QTimer(socket);
    timeout->setSingleShot(true);
    QByteArray const payload = adifRecord + " <EOR>\n";

    socket->setProperty("adifTcpFailed", false);

    connect(timeout, &QTimer::timeout, socket, [socket, serverName, port]() {
        socket->setProperty("adifTcpFailed", true);
        decodiumBridgeAdifTcpLog(QStringLiteral("ADIF TCP timeout: target=%1:%2")
                      .arg(serverName, QString::number(port)));
        socket->abort();
    });

    connect(socket, &QTcpSocket::connected, socket, [socket, payload]() {
        qint64 const written = socket->write(payload);
        if (written < 0) {
            socket->setProperty("adifTcpFailed", true);
            decodiumBridgeAdifTcpLog(QStringLiteral("ADIF TCP write failed: %1").arg(socket->errorString()));
            socket->abort();
            return;
        }
        socket->flush();
    });

    connect(socket, &QTcpSocket::bytesWritten, socket, [socket](qint64) {
        if (socket->bytesToWrite() == 0) {
            socket->disconnectFromHost();
        }
    });

    connect(socket, &QTcpSocket::disconnected, socket, [socket, dxCall, serverName, port, payload]() {
        if (!socket->property("adifTcpFailed").toBool()) {
            decodiumBridgeAdifTcpLog(QStringLiteral("ADIF TCP sent: call=%1 target=%2:%3 bytes=%4")
                          .arg(dxCall, serverName, QString::number(port), QString::number(payload.size())));
        }
        socket->deleteLater();
    });

    connect(socket, &QTcpSocket::errorOccurred, socket,
            [socket, dxCall, serverName, port](QAbstractSocket::SocketError) {
        if (socket->property("adifTcpFailed").toBool()) {
            return;
        }
        socket->setProperty("adifTcpFailed", true);
        decodiumBridgeAdifTcpLog(QStringLiteral("ADIF TCP failed: call=%1 target=%2:%3 error=%4")
                      .arg(dxCall, serverName, QString::number(port), socket->errorString()));
        socket->abort();
    });

    timeout->start(3000);
    socket->connectToHost(serverName, port);
}
