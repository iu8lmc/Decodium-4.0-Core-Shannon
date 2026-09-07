#include "src/services/DxccLookup.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class DxccLookupTest final : public QObject
{
    Q_OBJECT

private slots:
    void bundledIndonesiaPrefixes()
    {
        const QString path = QFINDTESTDATA("../resources/runtime/cty.dat");
        QVERIFY2(!path.isEmpty(), "Bundled resources/runtime/cty.dat not found");

        DxccLookup lookup;
        QVERIFY(lookup.loadCtyDat(path));
        QCOMPARE(lookup.lookup(QStringLiteral("8B8FTDM")).name,
                 QStringLiteral("Indonesia"));
        QCOMPARE(lookup.lookup(QStringLiteral("8D8DADA")).name,
                 QStringLiteral("Indonesia"));
        QCOMPARE(lookup.lookup(QStringLiteral("8A3B")).name,
                 QStringLiteral("Indonesia"));
        QCOMPARE(lookup.lookup(QStringLiteral("VU33IN")).name,
                 QStringLiteral("India"));
    }

    void kg4UsesLengthSensitiveAssignment()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = directory.filePath(QStringLiteral("cty.dat"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(
            "United States: 05: 08: NA: 37.60: 91.87: 5.0: K:\n"
            "    K;\n"
            "Guantanamo Bay: 08: 11: NA: 20.00: 75.00: 5.0: KG4:\n"
            "    KG4,=KG4AC;\n");
        file.close();

        DxccLookup lookup;
        QVERIFY(lookup.loadCtyDat(path));

        // Generic one- and three-character suffixes are United States.
        QCOMPARE(lookup.lookup(QStringLiteral("KG4A")).name,
                 QStringLiteral("United States"));
        QCOMPARE(lookup.lookup(QStringLiteral("KG4ABC")).name,
                 QStringLiteral("United States"));

        // A generic two-character suffix remains Guantanamo Bay.
        QCOMPARE(lookup.lookup(QStringLiteral("KG4AB")).name,
                 QStringLiteral("Guantanamo Bay"));

        // Exact cty.dat exceptions remain authoritative.
        QCOMPARE(lookup.lookup(QStringLiteral("KG4AC")).name,
                 QStringLiteral("Guantanamo Bay"));
    }

    void indonesiaAndNumericPortableSuffixes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = directory.filePath(QStringLiteral("cty.dat"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(
            "Indonesia: 28: 54: OC: -7.30: -109.88: -7.0: YB:\n"
            "    8A,8B,8D,YB;\n"
            "India: 22: 41: AS: 22.50: -77.58: -5.5: VU:\n"
            "    VU;\n"
            "Italy: 15: 28: EU: 42.82: -12.58: -1.0: I:\n"
            "    I,IK,IZ;\n");
        file.close();

        DxccLookup lookup;
        QVERIFY(lookup.loadCtyDat(path));

        QCOMPARE(lookup.lookup(QStringLiteral("8B8FTDM")).name,
                 QStringLiteral("Indonesia"));
        QCOMPARE(lookup.lookup(QStringLiteral("8D8DADA")).name,
                 QStringLiteral("Indonesia"));
        QCOMPARE(lookup.lookup(QStringLiteral("8A3B")).name,
                 QStringLiteral("Indonesia"));
        QCOMPARE(lookup.lookup(QStringLiteral("VU33IN")).name,
                 QStringLiteral("India"));
        QCOMPARE(lookup.lookup(QStringLiteral("IZ1ABC/0")).name,
                 QStringLiteral("Italy"));
        QCOMPARE(lookup.lookup(QStringLiteral("IZ1ABC/1")).name,
                 QStringLiteral("Italy"));
    }
};

QTEST_MAIN(DxccLookupTest)
#include "test_dxcc_lookup.moc"
