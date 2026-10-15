#include <QtTest>

#include "src/rtl/RtlSdrCapabilities.h"

class TestRtlSdrCapabilities final : public QObject
{
    Q_OBJECT

private slots:
    void detectsBlogV4Identities();
    void acceptsOnlyTheDirectSamplingHfRange();
    void resolvesHardwareBeforeFrequency();
};

void TestRtlSdrCapabilities::detectsBlogV4Identities()
{
    using decodium::rtl_sdr::isRtlSdrBlogV4Identity;

    QVERIFY(isRtlSdrBlogV4Identity(QStringLiteral("0: RTLSDRBlog Blog V4 — 00000001")));
    QVERIFY(isRtlSdrBlogV4Identity(QStringLiteral("RTL-SDR Blog V4L")));
    QVERIFY(!isRtlSdrBlogV4Identity(QStringLiteral("RTLSDRBlog Blog V3 — 00000001")));
    QVERIFY(!isRtlSdrBlogV4Identity(QStringLiteral("Generic RTL2832U")));
}

void TestRtlSdrCapabilities::acceptsOnlyTheDirectSamplingHfRange()
{
    using decodium::rtl_sdr::isDirectSamplingFrequency;

    QVERIFY(!isDirectSamplingFrequency(499999));
    QVERIFY(isDirectSamplingFrequency(500000));
    QVERIFY(isDirectSamplingFrequency(7100000));
    QVERIFY(isDirectSamplingFrequency(24000000));
    QVERIFY(!isDirectSamplingFrequency(24000001));
    QVERIFY(!isDirectSamplingFrequency(144100000));
}

void TestRtlSdrCapabilities::resolvesHardwareBeforeFrequency()
{
    using decodium::rtl_sdr::DirectSamplingBlockReason;
    using decodium::rtl_sdr::directSamplingBlockReason;

    QCOMPARE(directSamplingBlockReason(QStringLiteral("RTLSDRBlog Blog V4"), 7100000),
             DirectSamplingBlockReason::BlogV4UsesUpconverter);
    QCOMPARE(directSamplingBlockReason(QStringLiteral("RTLSDRBlog Blog V3"), 144100000),
             DirectSamplingBlockReason::OutsideHfRange);
    QCOMPARE(directSamplingBlockReason(QStringLiteral("RTLSDRBlog Blog V3"), 7100000),
             DirectSamplingBlockReason::None);
}

QTEST_MAIN(TestRtlSdrCapabilities)
#include "test_rtlsdr_capabilities.moc"
