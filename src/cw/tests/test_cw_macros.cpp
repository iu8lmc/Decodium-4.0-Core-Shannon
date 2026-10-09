#include "CwMacros.h"

#include <QTest>

using namespace decodium::cw;

class TestCwMacros : public QObject {
    Q_OBJECT
private slots:
    void fillsTheHoles()
    {
        Context c;
        c.myCall = "iu8lmc";
        c.call = "dl1abc";
        c.nr = "042";
        c.exch = "JN71";
        c.name = "Hans";
        QCOMPARE(expand("{call} 5NN {NR}", c), QString("DL1ABC 5NN 042"));
        QCOMPARE(expand("CQ TEST {MYCALL} {MYCALL} TEST", c), QString("CQ TEST IU8LMC IU8LMC TEST"));
        QCOMPARE(expand("{RST} {EXCH} {NAME}", c), QString("599 JN71 HANS"));
    }

    void leftoversNeverReachTheAir()
    {
        Context c;
        QCOMPARE(expand("{CALL} TU {FOO} {#}", c), QString("TU"));
        QVERIFY(!expand("{MYCALL}", c).contains('{'));
    }

    void everythingIsUpperCaseAndTidy()
    {
        Context c;
        QCOMPARE(expand("  qrz   de  ", c), QString("QRZ DE"));
    }

    void defaultsAreTheTwelveKeys()
    {
        const auto m = defaultMacros();
        QCOMPARE(m.size(), 12);
        QVERIFY(m.first().label.startsWith("F1"));
        QVERIFY(m.last().label.startsWith("F12"));
    }

    void jsonRoundTrip()
    {
        QList<Macro> m {{"F1 CQ", "CQ {MYCALL}"}, {"F2 ?", "?"}};
        const QList<Macro> back = macrosFromJson(macrosToJson(m));
        QCOMPARE(back.size(), 2);
        QCOMPARE(back[0].label, QString("F1 CQ"));
        QCOMPARE(back[1].text, QString("?"));
    }

    void emptyOrBrokenJsonGivesTheDefaults()
    {
        QCOMPARE(macrosFromJson(QString()).size(), 12);
        QCOMPARE(macrosFromJson("non json").size(), 12);
        QCOMPARE(macrosFromJson("[]").size(), 12);
    }

    void neverMoreThanTheMaximum()
    {
        QList<Macro> many;
        for (int i = 0; i < 40; ++i) many.append({QString("F%1").arg(i), "X"});
        QCOMPARE(macrosFromJson(macrosToJson(many)).size(), kMaxMacros);
    }
};

QTEST_GUILESS_MAIN(TestCwMacros)
#include "test_cw_macros.moc"
