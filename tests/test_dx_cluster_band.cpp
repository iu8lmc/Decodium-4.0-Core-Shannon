#include <QtTest>

#include "DecodiumDxCluster.h"

class TestDxClusterBand final : public QObject
{
    Q_OBJECT

private slots:
    void mapsKnownClusterFrequenciesToBands()
    {
        QCOMPARE(DecodiumDxCluster::bandLabelFromFrequencyKhz(21075.1), QStringLiteral("15M"));
        QCOMPARE(DecodiumDxCluster::bandLabelFromFrequencyKhz(24915.0), QStringLiteral("12M"));
        QCOMPARE(DecodiumDxCluster::bandLabelFromFrequencyKhz(40680.0), QStringLiteral("8M"));
        QCOMPARE(DecodiumDxCluster::bandLabelFromFrequencyKhz(50313.0), QStringLiteral("6M"));
        QCOMPARE(DecodiumDxCluster::bandLabelFromFrequencyKhz(70154.0), QStringLiteral("4M"));
        QCOMPARE(DecodiumDxCluster::bandLabelFromFrequencyKhz(144175.1), QStringLiteral("2M"));
        QCOMPARE(DecodiumDxCluster::bandLabelFromFrequencyKhz(432174.0), QStringLiteral("70CM"));
    }

    void rejectsGapsInsteadOfMislabelingThem()
    {
        QCOMPARE(DecodiumDxCluster::bandLabelFromFrequencyKhz(69950.0), QStringLiteral("UNK"));
        QCOMPARE(DecodiumDxCluster::bandLabelFromFrequencyKhz(71050.0), QStringLiteral("UNK"));
        QCOMPARE(DecodiumDxCluster::bandLabelFromFrequencyKhz(130000.0), QStringLiteral("UNK"));
    }
};

QTEST_MAIN(TestDxClusterBand)

#include "test_dx_cluster_band.moc"
