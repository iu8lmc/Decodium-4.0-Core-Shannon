// DecoLink: gli spot e le richieste di sintonia che arrivano dal cluster di DecoLog,
// e la lista del DX Cluster che li accoglie senza doppioni.
#include <QtTest>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>

#include "DecodiumDxCluster.h"
#include "Network/DecodiumDecoLogLink.h"

class TestDecoLogClusterLink final : public QObject
{
    Q_OBJECT

private slots:
    void spotAndTuneMessages()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QTcpSocket* peer = nullptr;
        connect(&server, &QTcpServer::newConnection, this, [&] { peer = server.nextPendingConnection(); });

        DecodiumDecoLogLink link;
        QSignalSpy connected(&link, &DecodiumDecoLogLink::connectedChanged);
        QSignalSpy spots(&link, &DecodiumDecoLogLink::spotReceived);
        QSignalSpy tunes(&link, &DecodiumDecoLogLink::tuneRequested);
        link.setIdentity(QStringLiteral("test"), QStringLiteral("IU8LMC"));
        link.setPort(server.serverPort());
        link.setEnabled(true);

        QTRY_VERIFY(peer != nullptr);
        auto send = [&](const QJsonObject& o) { peer->write(QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n'); };
        send({{QStringLiteral("type"), QStringLiteral("hello")}, {QStringLiteral("app"), QStringLiteral("DecoLog")},
              {QStringLiteral("version"), QStringLiteral("0.1.0")}, {QStringLiteral("protocol"), 1}});
        QTRY_COMPARE(connected.size(), 1);

        send({{QStringLiteral("type"), QStringLiteral("spot")}, {QStringLiteral("call"), QStringLiteral("3Y0J")},
              {QStringLiteral("freqKhz"), 14025.1}, {QStringLiteral("mode"), QStringLiteral("CW")},
              {QStringLiteral("status"), QStringLiteral("NEW DXCC")}, {QStringLiteral("alert"), true}});
        send({{QStringLiteral("type"), QStringLiteral("tune")}, {QStringLiteral("call"), QStringLiteral("FT4TA")},
              {QStringLiteral("dialKhz"), 10136.0}, {QStringLiteral("audioHz"), 1500}, {QStringLiteral("mode"), QStringLiteral("FT8")}});
        QTRY_COMPARE(spots.size(), 1);
        QTRY_COMPARE(tunes.size(), 1);
        QCOMPARE(spots.at(0).at(0).toJsonObject().value(QStringLiteral("call")).toString(), QStringLiteral("3Y0J"));
        QCOMPARE(tunes.at(0).at(0).toJsonObject().value(QStringLiteral("audioHz")).toInt(), 1500);
    }

    void injectedSpotsReplaceTheSameDx()
    {
        DecodiumDxCluster cluster;
        auto spot = [](const char* call, double khz, const char* mode, const char* comment) {
            QVariantMap m;
            m[QStringLiteral("dxCall")] = QLatin1String(call);
            m[QStringLiteral("frequency")] = khz;
            m[QStringLiteral("band")] = DecodiumDxCluster::bandLabelFromFrequencyKhz(khz);
            m[QStringLiteral("mode")] = QLatin1String(mode);
            m[QStringLiteral("comment")] = QLatin1String(comment);
            return m;
        };
        cluster.injectSpot(spot("3Y0J", 14025.1, "CW", "[NEW DXCC] 18 dB"));
        cluster.injectSpot(spot("JA1XX", 14075.0, "FT8", ""));
        cluster.injectSpot(spot("3Y0J", 14025.2, "CW", "[NEW DXCC] 22 dB"));
        cluster.injectSpot(spot("3Y0J", 7025.0, "CW", ""));
        const QVariantList list = cluster.spots();
        QCOMPARE(list.size(), 3);
        QCOMPARE(list.at(1).toMap().value(QStringLiteral("comment")).toString(), QStringLiteral("[NEW DXCC] 22 dB"));
    }
};

QTEST_MAIN(TestDecoLogClusterLink)

#include "test_decolog_cluster_link.moc"
