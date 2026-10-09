#include "RotorController.h"
#include "RotorGeo.h"
#include "RotorProtocol.h"
#include "RotorServers.h"
#include "RotorSpots.h"

#include <QtTest>

using namespace decodium::rotor;

class TestRotor : public QObject {
    Q_OBJECT
private slots:
    void protocolFrames()
    {
        const QByteArray goFrame = gotoFrame('A', 123.0, 1);
        QCOMPARE(goFrame.left(1), QByteArray(1, '\x02'));
        QCOMPARE(goFrame.right(1), QByteArray(1, '\r'));
        QVERIFY(goFrame.contains("123"));
        QVERIFY(stopFrame('A', false, 1).contains("997"));
        QVERIFY(stopFrame('A', true, 1).contains("999"));
        QVERIFY(stopFrame('A', false, 10).contains("9777"));
        QVERIFY(stopFrame('A', true, 10).contains("9999"));
        QCOMPARE(models().size(), 4);
        QVERIFY(modelByKey(QStringLiteral("auto")) == nullptr);
    }

    void geo()
    {
        Position p;
        QVERIFY(locatorToPosition(QStringLiteral("JN70"), &p));
        QVERIFY(std::fabs(p.latitude - 40.5) < 0.6);
        QVERIFY(!isValidLocator(QStringLiteral("ZZ99")));
        QVERIFY(isValidLocator(QStringLiteral("JN70ab")));
        Bearing b;
        QVERIFY(bearingToLocator(QStringLiteral("JN70"), QStringLiteral("JN70"), &b));
        QVERIFY(b.distanceKm < 1.0);
        QVERIFY(bearingToLocator(QStringLiteral("JN70"), QStringLiteral("FN31"), &b));
        QVERIFY(b.shortPath > 270.0 && b.shortPath < 330.0);
        QVERIFY(std::fabs(std::fmod(b.shortPath + 180.0, 360.0) - b.longPath) < 0.01);
        QVERIFY(b.distanceKm > 5000 && b.distanceKm < 8000);
    }

    void spotsFromMessages()
    {
        QString call, grid;
        QVERIFY(SpotBook::stationFromMessage(QStringLiteral("CQ DX EA8ABC IL18"), &call, &grid));
        QCOMPARE(call, QStringLiteral("EA8ABC"));
        QCOMPARE(grid, QStringLiteral("IL18"));
        QVERIFY(SpotBook::stationFromMessage(QStringLiteral("IU8LMC EA8ABC RR73"), &call, &grid));
        QCOMPARE(call, QStringLiteral("EA8ABC"));
        QVERIFY(grid.isEmpty());
        QVERIFY(!SpotBook::stationFromMessage(QStringLiteral("CQ"), &call, &grid));

        SpotBook book;
        book.setStation(QStringLiteral("IU8LMC"), QStringLiteral("JN70"));
        book.note(QStringLiteral("EA8ABC"), QStringLiteral("IL18"), QStringLiteral("decodium"), -12, QStringLiteral("FT8"), 14074000);
        book.note(QStringLiteral("IU8LMC"), QStringLiteral("JN70"), QStringLiteral("decodium"));
        book.note(QStringLiteral("NOGRID"), QString(), QStringLiteral("decodium"));
        QCOMPARE(book.count(), 1);
        const QVariantMap s = book.find(QStringLiteral("ea8abc"));
        QVERIFY(!s.isEmpty());
        QVERIFY(s.value(QStringLiteral("km")).toInt() > 3000);
    }

    void simulatedRotorReachesTarget()
    {
        Config cfg;
        cfg.simulate = true;
        cfg.model = QStringLiteral("d_azel");
        cfg.pollInterval = 0.05;
        RotorController rotor(cfg);
        rotor.start();
        QTRY_VERIFY_WITH_TIMEOUT(rotor.snapshot().value(QStringLiteral("connected")).toBool(), 5000);
        const QVariantMap applied = rotor.goTo(10.0, std::nullopt);
        QVERIFY(std::fabs(applied.value(QStringLiteral("az")).toDouble() - 10.0) < 0.01);
        QTRY_VERIFY_WITH_TIMEOUT(std::fabs(rotor.snapshot().value(QStringLiteral("az")).toDouble() - 10.0) < 1.5, 8000);
        QVERIFY_EXCEPTION_THROWN(rotor.goToLocator(QStringLiteral("not a locator")), RotorError);
        rotor.shutdown();
    }

    void rotctldDialect()
    {
        Config cfg;
        cfg.simulate = true;
        cfg.pollInterval = 0.05;
        RotorController rotor(cfg);
        rotor.start();
        QTRY_VERIFY_WITH_TIMEOUT(rotor.snapshot().value(QStringLiteral("connected")).toBool(), 5000);
        RotctldServer server(&rotor);
        const auto pos = server.execute(QStringLiteral("p"));
        QVERIFY(pos.has_value());
        QCOMPARE(pos->count('\n'), 2);
        QCOMPARE(*server.execute(QStringLiteral("S")), QByteArray("RPRT 0\n"));
        QCOMPARE(*server.execute(QStringLiteral("P abc")), QByteArray("RPRT -1\n"));
        QCOMPARE(*server.execute(QStringLiteral("zzz")), QByteArray("RPRT -8\n"));
        QVERIFY(server.execute(QStringLiteral("dump_state"))->endsWith("done\n"));
        QVERIFY(!server.execute(QStringLiteral("q")).has_value());
        rotor.shutdown();
    }
};

QTEST_MAIN(TestRotor)
#include "test_rotor.moc"
