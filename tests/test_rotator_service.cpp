#include <QtTest>

#include <QHostAddress>
#include <QUdpSocket>

#include "src/services/RotatorService.h"

class TestRotatorService final : public QObject
{
    Q_OBJECT

private slots:
    void sendsPstAzimuthAndElevationAndReadsFeedback()
    {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));

        RotatorService service;
        service.setHost(QStringLiteral("127.0.0.1"));
        service.setPort(receiver.localPort());
        service.setEnabled(true);
        QVERIFY(service.commandTarget(123.4, 45.6, true));
        QVERIFY(receiver.waitForReadyRead(1000));

        QByteArray command;
        command.resize(static_cast<int>(receiver.pendingDatagramSize()));
        receiver.readDatagram(command.data(), command.size());
        QCOMPARE(command,
                 QByteArray("<PST><AZIMUTH>123.4</AZIMUTH><ELEVATION>45.6</ELEVATION></PST>"));

        service.pollFeedback();
        QVERIFY(receiver.waitForReadyRead(1000));
        QByteArray query;
        query.resize(static_cast<int>(receiver.pendingDatagramSize()));
        QHostAddress senderAddress;
        quint16 senderPort = 0;
        receiver.readDatagram(query.data(), query.size(), &senderAddress, &senderPort);
        QVERIFY(query == QByteArray("<PST>AZ?</PST>")
                || query == QByteArray("<PST>EL?</PST>"));

        QUdpSocket feedback;
        QVERIFY(feedback.writeDatagram(QByteArray("AZ:222.2\r\n"),
                                       QHostAddress::LocalHost,
                                       static_cast<quint16>(receiver.localPort() + 1)) > 0);
        QVERIFY(feedback.writeDatagram(QByteArray("EL:33.3\r\n"),
                                       QHostAddress::LocalHost,
                                       static_cast<quint16>(receiver.localPort() + 1)) > 0);
        QTest::qWait(100);
        QTRY_VERIFY_WITH_TIMEOUT(service.feedbackAvailable(), 2000);
        QCOMPARE(service.currentAzimuth(), 222.2);
        QCOMPARE(service.currentElevation(), 33.3);
    }

    void catRotatorUsesCommonTagsAndSafetyLimits()
    {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));

        RotatorService service;
        service.setProtocol(QStringLiteral("CatRotator"));
        service.setHost(QStringLiteral("127.0.0.1"));
        service.setPort(receiver.localPort());
        service.setEnabled(true);
        QVERIFY(service.commandTarget(90.0, 12.0, true));
        QVERIFY(receiver.waitForReadyRead(1000));
        QByteArray command;
        command.resize(static_cast<int>(receiver.pendingDatagramSize()));
        receiver.readDatagram(command.data(), command.size());
        QCOMPARE(command,
                 QByteArray("<AZIMUTH>90.0</AZIMUTH><ELEVATION>12.0</ELEVATION>"));
        QVERIFY(!service.feedbackAvailable());

        service.setMaxElevation(10.0);
        QVERIFY(!service.commandTarget(90.0, 12.0, true));
        QVERIFY(service.status().contains(QStringLiteral("safety"), Qt::CaseInsensitive));
        QVERIFY(!receiver.waitForReadyRead(100));
    }

    void trackingRepeatsWithoutBlocking()
    {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));

        RotatorService service;
        service.setHost(QStringLiteral("127.0.0.1"));
        service.setPort(receiver.localPort());
        service.setEnabled(true);
        service.setTrackingIntervalMs(250);
        QVERIFY(service.trackTarget(20.0, 5.0, true));
        int datagrams = 0;
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 1200) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            while (receiver.hasPendingDatagrams()) {
                receiver.readDatagram(nullptr, 0);
                ++datagrams;
            }
            QTest::qWait(50);
        }
        service.stopTracking();
        QVERIFY(datagrams >= 3);
    }
};

QTEST_GUILESS_MAIN(TestRotatorService)

#include "test_rotator_service.moc"
