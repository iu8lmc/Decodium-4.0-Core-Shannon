#include <QtTest>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include "Network/AdifUdpPayload.hpp"

class TestAdifUdpPayload : public QObject {
    Q_OBJECT
private slots:
    void preservesOtherFields() {
        QByteArray input("<BAND:3>20M <band_rx:4:S>70CM <RST_SENT:3>-10 <RST_RCVD:3>-12 <CALL:6>IZ1JIZ <EOR>");
        QByteArray expected("<BAND:3>20m <band_rx:4:S>70cm <RST_SENT:3>-10 <RST_RCVD:3>-12 <CALL:6>IZ1JIZ <EOR>");
        QCOMPARE(decodium::adif::udpPayload(input), expected);
        QCOMPARE(decodium::adif::udpPayload(expected), expected);
        QCOMPARE(decodium::adif::udpPayload("<COMMENT:12><BAND:3>20M! <BAND:3>40M"),
                 QByteArray("<COMMENT:12><BAND:3>20M! <BAND:3>40m"));
        QCOMPARE(decodium::adif::udpPayload("<BAND:99>20M"), QByteArray("<BAND:99>20M"));
    }
    void threeLoopbackDestinations() {
        QUdpSocket receivers[3];
        QUdpSocket sender;
        QByteArray const input("<BAND:3>20M <RST_SENT:3>-10 <RST_RCVD:3>-12");
        QByteArray const expected("<BAND:3>20m <RST_SENT:3>-10 <RST_RCVD:3>-12 <eor>");
        for (auto& receiver : receivers) {
            QVERIFY(receiver.bind(QHostAddress(QHostAddress::LocalHost), quint16(0)));
            auto payload = decodium::adif::udpPayload(input) + " <eor>";
            QCOMPARE(sender.writeDatagram(payload, QHostAddress::LocalHost, receiver.localPort()), qint64(payload.size()));
        }
        for (auto& receiver : receivers) {
            QTRY_VERIFY(receiver.hasPendingDatagrams());
            QCOMPARE(receiver.receiveDatagram().data(), expected);
        }
    }
};
QTEST_GUILESS_MAIN(TestAdifUdpPayload)
#include "test_adif_udp_payload.moc"
