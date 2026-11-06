#include "CwCatText.h"
#include <QTest>

using namespace decodium::cw;

class TestCwCatText : public QObject {
    Q_OBJECT
private slots:
    void limitsOnlyChangeTheIcomPath()
    {
        QCOMPARE(catMorseTextLimit("Icom", false), 30);
        QCOMPARE(catMorseTextLimit(" icom ", false), 30);
        QCOMPARE(catMorseTextLimit("Yaesu", true), 49);
        QCOMPARE(catMorseTextLimit("Yaesu", false), 0);
        QCOMPARE(catMorseTextLimit("Kenwood", false), 0);
        QCOMPARE(catMorseTextLimit("Hamlib", false), 0); // NET rigctl
        QCOMPARE(catMorseTextLimit({}, false), 0);
    }

    void singleLettersAndDigitsAreLiteralMessages()
    {
        QCOMPARE(ft991MorsePayload("v"), QStringLiteral("V}"));
        QCOMPARE(ft991MorsePayload("2"), QStringLiteral("2}"));
        QCOMPARE(ft991MorsePayload("V}"), QStringLiteral("V}"));
        QCOMPARE(ft991MorsePayload(" K "), QStringLiteral(" K }"));
    }
    void memoryLimitReservesTheEndMarker()
    {
        const QString text(49, QLatin1Char('E'));
        QCOMPARE(ft991MorsePayload(text).size(), 50);
        QVERIFY(ft991MorsePayload(text).endsWith('}'));
        QVERIFY(ft991MorsePayload(QString(50, QLatin1Char('E'))).isEmpty());
    }
    void invalidCatDelimitersAreRejectedBeforeTransmission()
    {
        QVERIFY(ft991MorsePayload(" ").isEmpty());
        QVERIFY(ft991MorsePayload("V;TX1;").isEmpty());
        QVERIFY(ft991MorsePayload("V}E").isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestCwCatText)
#include "test_cw_cat_text.moc"
