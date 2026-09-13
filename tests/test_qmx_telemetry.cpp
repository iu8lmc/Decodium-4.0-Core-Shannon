#include <limits>

#include <QtTest>

#include "Transceiver/QmxTelemetry.hpp"

class TestQmxTelemetry final : public QObject
{
  Q_OBJECT

private slots:
  void parsesPowerInTenthsOfAWatt ()
  {
    unsigned int milliwatts = 0;
    QVERIFY (decodium::qmx_telemetry::parse_power_milliwatts (QByteArrayLiteral ("PC19;"), &milliwatts));
    QCOMPARE (milliwatts, 1900U);

    QVERIFY (decodium::qmx_telemetry::parse_power_milliwatts (QByteArrayLiteral ("  PC00;\r\n"), &milliwatts));
    QCOMPARE (milliwatts, 0U);
  }

  void parsesSwrInHundredths ()
  {
    unsigned int hundredths = 0;
    QVERIFY (decodium::qmx_telemetry::parse_swr_hundredths (QByteArrayLiteral ("SW121;"), &hundredths));
    QCOMPARE (hundredths, 121U);
  }

  void rejectsEmptyReceiveModeAndMalformedReplies ()
  {
    unsigned int value = 77;
    QVERIFY (!decodium::qmx_telemetry::parse_power_milliwatts (QByteArrayLiteral ("PC;"), &value));
    QCOMPARE (value, 77U);
    QVERIFY (!decodium::qmx_telemetry::parse_swr_hundredths (QByteArrayLiteral ("SW;"), &value));
    QVERIFY (!decodium::qmx_telemetry::parse_power_milliwatts (QByteArrayLiteral ("PC1.9;"), &value));
    QVERIFY (!decodium::qmx_telemetry::parse_power_milliwatts (QByteArrayLiteral ("SW19;"), &value));
    QVERIFY (!decodium::qmx_telemetry::parse_swr_hundredths (QByteArrayLiteral ("SW121"), &value));
    QVERIFY (!decodium::qmx_telemetry::parse_swr_hundredths (QByteArrayLiteral ("SW121;extra"), &value));
    QVERIFY (!decodium::qmx_telemetry::parse_swr_hundredths (QByteArrayLiteral ("SW121;"), nullptr));
  }

  void rejectsOverflow ()
  {
    unsigned int value = 0;
    QByteArray const tooLarge = QByteArrayLiteral ("PC")
        + QByteArray::number (std::numeric_limits<unsigned int>::max ()) + QByteArrayLiteral (";");
    QVERIFY (!decodium::qmx_telemetry::parse_power_milliwatts (tooLarge, &value));
  }

  void ignoresSettlingSampleAndConfirmsPersistentHighSWR ()
  {
    using namespace decodium::qmx_telemetry;
    SwrSafetyFilter filter;
    filter.reset ();

    auto result = filter.process (900U, 250U, true);
    QCOMPARE (result.decision, SwrFilterDecision::SettlingIgnored);
    QCOMPARE (result.samples, 0U);
    QVERIFY (!result.stop_eligible);

    result = filter.process (360U, 250U);
    QCOMPARE (result.decision, SwrFilterDecision::Collecting);
    QVERIFY (!result.stop_eligible);
    result = filter.process (370U, 250U);
    QCOMPARE (result.decision, SwrFilterDecision::Collecting);
    QVERIFY (!result.stop_eligible);
    result = filter.process (380U, 250U);
    QCOMPARE (result.filtered_hundredths, 370U);
    QCOMPARE (result.published_hundredths, 370U);
    QCOMPARE (result.consecutive_high, 3U);
    QCOMPARE (result.decision, SwrFilterDecision::HighConfirmed);
    QVERIFY (result.stop_eligible);
  }

  void isolatedSpikeDoesNotStopTransmission ()
  {
    using namespace decodium::qmx_telemetry;
    SwrSafetyFilter filter;

    filter.process (125U, 250U);
    filter.process (900U, 250U);
    auto const result = filter.process (130U, 250U);

    QCOMPARE (result.filtered_hundredths, 130U);
    QCOMPARE (result.published_hundredths, 130U);
    QCOMPARE (result.decision, SwrFilterDecision::Safe);
    QVERIFY (!result.stop_eligible);
  }

  void requiresConsecutiveHighReadingsAfterMedianSettles ()
  {
    using namespace decodium::qmx_telemetry;
    SwrSafetyFilter filter;

    filter.process (400U, 250U);
    filter.process (410U, 250U);
    auto result = filter.process (120U, 250U);
    QCOMPARE (result.filtered_hundredths, 400U);
    QCOMPARE (result.published_hundredths, 250U);
    QCOMPARE (result.consecutive_high, 0U);
    QCOMPARE (result.decision, SwrFilterDecision::HighPending);
    QVERIFY (!result.stop_eligible);

    result = filter.process (420U, 250U);
    QCOMPARE (result.consecutive_high, 1U);
    QVERIFY (!result.stop_eligible);
    result = filter.process (430U, 250U);
    QCOMPARE (result.consecutive_high, 2U);
    QCOMPARE (result.decision, SwrFilterDecision::HighConfirmed);
    QVERIFY (result.stop_eligible);
  }

  void resetPreventsPreviousTransmissionFromBlockingNextOne ()
  {
    using namespace decodium::qmx_telemetry;
    SwrSafetyFilter filter;

    filter.process (500U, 250U);
    filter.process (510U, 250U);
    QVERIFY (filter.process (520U, 250U).stop_eligible);

    filter.reset ();
    auto result = filter.process (520U, 250U, true);
    QCOMPARE (result.samples, 0U);
    QCOMPARE (result.published_hundredths, 0U);
    QVERIFY (!result.stop_eligible);
    result = filter.process (120U, 250U);
    QCOMPARE (result.samples, 1U);
    QVERIFY (!result.stop_eligible);
  }

  void invalidReadingBreaksHighPersistenceWithoutLosingMedianHistory ()
  {
    using namespace decodium::qmx_telemetry;
    SwrSafetyFilter filter;

    filter.process (400U, 250U);
    filter.process (410U, 250U);
    auto result = filter.process (0U, 250U);
    QCOMPARE (result.decision, SwrFilterDecision::InvalidSample);
    QCOMPARE (result.samples, 2U);
    QCOMPARE (result.consecutive_high, 0U);
    QVERIFY (!result.stop_eligible);

    result = filter.process (420U, 250U);
    QCOMPARE (result.filtered_hundredths, 410U);
    QCOMPARE (result.consecutive_high, 1U);
    QCOMPARE (result.decision, SwrFilterDecision::HighPending);
    QVERIFY (!result.stop_eligible);

    result = filter.process (430U, 250U);
    QCOMPARE (result.consecutive_high, 2U);
    QCOMPARE (result.decision, SwrFilterDecision::HighConfirmed);
    QVERIFY (result.stop_eligible);
  }

  void qmxBurstTimingProvidesThreePostSettlingSamplesBy700ms ()
  {
    using namespace decodium::qmx_telemetry;
    QCOMPARE (swr_poll_delays_ms.front (), 120);
    QVERIFY (ignore_scheduled_swr_sample (120));
    QVERIFY (!ignore_scheduled_swr_sample (350));
    QCOMPARE (swr_poll_delays_ms[1], 350);
    QCOMPARE (swr_poll_delays_ms[2], 525);
    QCOMPARE (swr_poll_delays_ms[3], 700);
  }
};

QTEST_MAIN (TestQmxTelemetry)
#include "test_qmx_telemetry.moc"
