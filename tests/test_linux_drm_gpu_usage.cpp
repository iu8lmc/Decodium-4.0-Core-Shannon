#include <QtTest>

#include "src/bridge/LinuxDrmGpuUsage.h"

using decodium::gpu_usage::LinuxDrmFdInfo;
using decodium::gpu_usage::primaryLinuxDrmPciDevice;

class TestLinuxDrmGpuUsage final : public QObject
{
    Q_OBJECT

private:
    static QString i915Snapshot(quint64 renderNs)
    {
        return QStringLiteral("drm-driver:\ti915\n"
                              "drm-client-id:\t46\n"
                              "drm-pdev:\t0000:00:02.0\n"
                              "drm-engine-render:\t%1 ns\n"
                              "drm-engine-copy:\t0 ns\n")
            .arg(renderNs);
    }

private slots:
    void primaryDevicePrefersActiveGpu()
    {
        QList<LinuxDrmFdInfo> const descriptors {
            { QStringLiteral("40"), QStringLiteral("drm-pdev:\t0000:00:02.0\n"
                                                    "drm-active-system0:\t0\n") },
            { QStringLiteral("41"), QStringLiteral("drm-pdev:\t0000:01:00.0\n"
                                                    "drm-active-vram0:\t8192 KiB\n") }
        };

        QCOMPARE(primaryLinuxDrmPciDevice(descriptors), QStringLiteral("0000:01:00.0"));
    }

    void primaryDeviceFallsBackToDescriptorCount()
    {
        QList<LinuxDrmFdInfo> const descriptors {
            { QStringLiteral("40"), QStringLiteral("drm-pdev:\t0000:00:02.0\n") },
            { QStringLiteral("41"), QStringLiteral("drm-pdev:\t0000:00:02.0\n") },
            { QStringLiteral("42"), QStringLiteral("drm-pdev:\t0000:01:00.0\n") }
        };

        QCOMPARE(primaryLinuxDrmPciDevice(descriptors), QStringLiteral("0000:00:02.0"));
    }

    void duplicateDescriptorsAreCountedOnce()
    {
        QList<LinuxDrmFdInfo> const descriptors {
            { QStringLiteral("46"), i915Snapshot(8114572232ULL) },
            { QStringLiteral("47"), i915Snapshot(8114572232ULL) },
            { QStringLiteral("51"), i915Snapshot(8114572232ULL) },
            { QStringLiteral("52"), i915Snapshot(8114572232ULL) }
        };

        quint64 totalNs = 0;
        QVERIFY(decodium::gpu_usage::aggregateLinuxDrmEngineTimeNs(descriptors, &totalNs));
        QCOMPARE(totalNs, 8114572232ULL);
    }

    void distinctClientsAreAggregated()
    {
        QList<LinuxDrmFdInfo> const descriptors {
            { QStringLiteral("46"), i915Snapshot(1000000000ULL) },
            { QStringLiteral("53"), QStringLiteral("drm-driver:\ti915\n"
                                                    "drm-client-id:\t47\n"
                                                    "drm-pdev:\t0000:00:02.0\n"
                                                    "drm-engine-copy:\t2000000000 ns\n") }
        };

        quint64 totalNs = 0;
        QVERIFY(decodium::gpu_usage::aggregateLinuxDrmEngineTimeNs(descriptors, &totalNs));
        QCOMPARE(totalNs, 3000000000ULL);
    }

    void highestDuplicateEngineCounterWins()
    {
        QList<LinuxDrmFdInfo> const descriptors {
            { QStringLiteral("46"), i915Snapshot(1000000000ULL) },
            { QStringLiteral("47"), i915Snapshot(1500000000ULL) }
        };

        quint64 totalNs = 0;
        QVERIFY(decodium::gpu_usage::aggregateLinuxDrmEngineTimeNs(descriptors, &totalNs));
        QCOMPARE(totalNs, 1500000000ULL);
    }

    void unchangedCounterBecomesStaleAfterThreshold()
    {
        int unchangedSamples = 0;
        unchangedSamples = decodium::gpu_usage::nextLinuxDrmUnchangedSampleCount(
            1000ULL, 1000ULL, unchangedSamples);
        QVERIFY(!decodium::gpu_usage::linuxDrmCounterIsStale(unchangedSamples));
        unchangedSamples = decodium::gpu_usage::nextLinuxDrmUnchangedSampleCount(
            1000ULL, 1000ULL, unchangedSamples);
        QVERIFY(!decodium::gpu_usage::linuxDrmCounterIsStale(unchangedSamples));
        unchangedSamples = decodium::gpu_usage::nextLinuxDrmUnchangedSampleCount(
            1000ULL, 1000ULL, unchangedSamples);
        QVERIFY(decodium::gpu_usage::linuxDrmCounterIsStale(unchangedSamples));
    }

    void advancingCounterClearsStaleState()
    {
        int const unchangedSamples = decodium::gpu_usage::nextLinuxDrmUnchangedSampleCount(
            1000ULL, 1001ULL, 9);
        QCOMPARE(unchangedSamples, 0);
        QVERIFY(!decodium::gpu_usage::linuxDrmCounterIsStale(unchangedSamples));
    }

    void xeCycleCountersCollapseDuplicateDescriptors()
    {
        QString const snapshot = QStringLiteral("drm-driver:\txe\n"
                                                "drm-client-id:\t3\n"
                                                "drm-pdev:\t0000:03:00.0\n"
                                                "drm-cycles-rcs:\t25\n"
                                                "drm-total-cycles-rcs:\t100\n");
        QList<LinuxDrmFdInfo> const descriptors {
            { QStringLiteral("46"), snapshot },
            { QStringLiteral("47"), snapshot }
        };

        decodium::gpu_usage::LinuxDrmCycleSample sample;
        QVERIFY(decodium::gpu_usage::aggregateLinuxDrmCycleSample(descriptors, &sample));
        QCOMPARE(sample.busyCycles, 25ULL);
        QCOMPARE(sample.totalCycles, 100ULL);
    }

    void xeCycleCountersAggregateClientsWithoutDuplicatingClock()
    {
        QList<LinuxDrmFdInfo> const descriptors {
            { QStringLiteral("46"), QStringLiteral("drm-driver:\txe\n"
                                                    "drm-client-id:\t3\n"
                                                    "drm-pdev:\t0000:03:00.0\n"
                                                    "drm-cycles-rcs:\t20\n"
                                                    "drm-total-cycles-rcs:\t100\n") },
            { QStringLiteral("53"), QStringLiteral("drm-driver:\txe\n"
                                                    "drm-client-id:\t4\n"
                                                    "drm-pdev:\t0000:03:00.0\n"
                                                    "drm-cycles-rcs:\t30\n"
                                                    "drm-total-cycles-rcs:\t100\n") }
        };

        decodium::gpu_usage::LinuxDrmCycleSample sample;
        QVERIFY(decodium::gpu_usage::aggregateLinuxDrmCycleSample(descriptors, &sample));
        QCOMPARE(sample.busyCycles, 50ULL);
        QCOMPARE(sample.totalCycles, 100ULL);
    }

    void xeCycleCountersHonourEngineCapacity()
    {
        QList<LinuxDrmFdInfo> const descriptors {
            { QStringLiteral("46"), QStringLiteral("drm-driver:\txe\n"
                                                    "drm-client-id:\t3\n"
                                                    "drm-pdev:\t0000:03:00.0\n"
                                                    "drm-cycles-ccs:\t100\n"
                                                    "drm-total-cycles-ccs:\t100\n"
                                                    "drm-engine-capacity-ccs:\t4\n") }
        };

        decodium::gpu_usage::LinuxDrmCycleSample sample;
        QVERIFY(decodium::gpu_usage::aggregateLinuxDrmCycleSample(descriptors, &sample));
        QCOMPARE(sample.busyCycles, 100ULL);
        QCOMPARE(sample.totalCycles, 400ULL);
    }
};

QTEST_MAIN(TestLinuxDrmGpuUsage)
#include "test_linux_drm_gpu_usage.moc"
