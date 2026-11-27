// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/sstv/fax/SstvRadiofax.h"
#include "src/sstv/fax/SstvRadiofaxAutoRx.h"
#include "src/sstv/fax/SstvRadiofaxRxSession.h"
#include "src/sstv/fax/SstvRadiofaxPhasing.h"
#include "src/sstv/fax/SstvRadiofaxStop.h"
#include "src/sstv/dsp/SstvFrequencyDemodulator.h"
#include <QtTest/QtTest>
#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace decodium::sstv;

class TestSstvRadiofax final : public QObject {
    Q_OBJECT
private slots:
    void aptStartDetectsIocAndRejectsOtherPatternsAB()
    {
        constexpr std::uint32_t rate = 12000U;
        for (const auto ioc : {SstvFaxIoc::Ioc576, SstvFaxIoc::Ioc288}) {
            const unsigned expected = ioc == SstvFaxIoc::Ioc576 ? 300U : 675U;
            const unsigned otherIoc = ioc == SstvFaxIoc::Ioc576 ? 675U : 300U;
            const unsigned tolerance = ioc == SstvFaxIoc::Ioc576 ? 3U : 7U;
            for (const unsigned modulation : {expected - tolerance, expected, expected + tolerance, otherIoc, 450U}) {
                SstvRadiofaxAptStartDetector whole(rate, ioc), duplicate(rate, ioc);
                SstvFrequencyObservation obs;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                for (std::uint64_t sample = 0; sample < 3U * rate; ++sample) {
                    obs.centreSample = sample;
                    obs.correctedFrequencyHz = (sample * modulation * 2U / rate) % 2U ? 2300.0 : 1500.0;
                    whole.consume(obs);
                    duplicate.consume(obs);
                    if (sample % 7U == 0U) duplicate.consume(obs);
                }
                const bool expectedSignal = modulation >= expected - tolerance && modulation <= expected + tolerance;
                QCOMPARE(whole.lastAptWhiteEdge().has_value(), expectedSignal);
                QCOMPARE(duplicate.lastAptWhiteEdge(), whole.lastAptWhiteEdge());
                if (whole.lastAptWhiteEdge()) QVERIFY(*whole.lastAptWhiteEdge() > 2U * rate);
            }
        }
    }

    void wmoTimedPhasingPreservesPhasingLikeFirstRowsAB()
    {
        constexpr std::uint32_t rate = 12000U;
        for (const auto ioc : {SstvFaxIoc::Ioc576, SstvFaxIoc::Ioc288}) {
         for (const std::uint16_t lpm : {60U, 90U, 120U, 240U}) {
          for (const std::uint64_t scale : {997U, 1000U, 1003U}) {
           const std::uint64_t period = (std::uint64_t(rate) * 60U / lpm) * scale / 1000U;
           for (const auto pattern : {SstvFaxPhasePattern::Symmetric, SstvFaxPhasePattern::WhiteFivePercent}) {
            SstvFaxConfig config;
            config.ioc = ioc;
            config.width = 32U;
            config.height = 8U;
            config.linesPerMinute = lpm;
            config.phasing = pattern;
            config.wmoTimedPhasing = true;
            SstvRadiofaxAutoRx timed(config);
            config.wmoTimedPhasing = false;
            SstvRadiofaxAutoRx heuristic(config);
            constexpr std::uint64_t aptEnd = 5U * rate;
            const std::uint64_t imageStart = aptEnd + std::uint64_t(lpm / 2U) * period;
            const unsigned apt = ioc == SstvFaxIoc::Ioc576 ? 300U : 675U;
            std::vector<SstvFrequencyObservation> batch;
            auto deliver = [&] {
                timed.consume(batch);
                heuristic.consume(batch);
                batch.clear();
            };
            for (std::uint64_t sample = 0; sample < imageStart + 11U * period;) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                if (sample < aptEnd)
                    obs.correctedFrequencyHz = (sample * apt * 2U / rate) % 2U ? 2300.0 : 1500.0;
                else if (sample < imageStart + 3U * period)
                    obs.correctedFrequencyHz = ((sample - aptEnd) % period
                        < period / (pattern == SstvFaxPhasePattern::Symmetric ? 2U : 20U)) ? 2300.0 : 1500.0;
                else obs.correctedFrequencyHz = 1900.0;
                batch.push_back(obs);
                if (batch.size() == 4096U) deliver();
                sample += sample < aptEnd ? 1U : 6U;
            }
            if (!batch.empty()) deliver();
            QVERIFY(timed.imageStartSample());
            QVERIFY(timed.usedWmoTimedPhasing());
            QVERIFY(!heuristic.usedWmoTimedPhasing());
            QVERIFY(heuristic.imageStartSample());
            QVERIFY2(std::llabs(qint64(*timed.imageStartSample()) - qint64(imageStart)) < 20,
                     "timed receiver did not preserve the first image line");
            QVERIFY(*heuristic.imageStartSample() >= imageStart + 2U * period);
            timed.finish();
            const auto image = timed.snapshot();
            QVERIFY(image);
            QVERIFY(image->isScanlineComplete(0));
            QVERIFY(image->pixel(pattern == SstvFaxPhasePattern::Symmetric ? 4 : 0, 0).red > 240);
            QVERIFY(image->pixel(28, 0).red < 15);
           }
          }
         }
        }
    }

    void wmoTimedPhasingFallsBackWithoutIocStartAB()
    {
        for (const auto ioc : {SstvFaxIoc::Ioc576, SstvFaxIoc::Ioc288}) {
            SstvFaxConfig config;
            config.ioc = ioc;
            config.width = 32U;
            config.height = 4U;
            config.linesPerMinute = 240U;
            config.phasing = SstvFaxPhasePattern::Symmetric;
            config.wmoTimedPhasing = true;
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> batch;
            constexpr std::uint64_t period = 3000U;
            constexpr std::uint64_t imageStart = 18U * period;
            for (std::uint64_t sample = 0; sample < imageStart + 7U * period; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                obs.correctedFrequencyHz = sample < imageStart
                    ? (sample % period < period / 2U ? 2300.0 : 1500.0) : 1900.0;
                batch.push_back(obs);
                if (batch.size() == 4096U) { receiver.consume(batch); batch.clear(); }
            }
            receiver.consume(batch);
            QVERIFY(receiver.imageStartSample());
            QVERIFY(!receiver.usedWmoTimedPhasing());
            QVERIFY(std::llabs(qint64(*receiver.imageStartSample()) - qint64(imageStart)) < 20);
        }
    }

    void wmoTimedPhasingIgnoresEarlyFalseImageAB()
    {
        constexpr std::uint32_t rate = 12000U;
        constexpr std::uint64_t period = 3000U;
        constexpr std::uint64_t aptEnd = 5U * rate;
        constexpr std::uint64_t imageStart = aptEnd + 120U * period;
        for (const auto ioc : {SstvFaxIoc::Ioc576, SstvFaxIoc::Ioc288}) {
            SstvFaxConfig config;
            config.ioc = ioc;
            config.width = 32U;
            config.height = 4U;
            config.linesPerMinute = 240U;
            config.phasing = SstvFaxPhasePattern::Symmetric;
            config.wmoTimedPhasing = true;
            SstvRadiofaxAutoRx receiver(config);
            const unsigned apt = ioc == SstvFaxIoc::Ioc576 ? 300U : 675U;
            std::vector<SstvFrequencyObservation> batch;
            for (std::uint64_t sample = 0; sample < imageStart + 7U * period;) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                if (sample < aptEnd)
                    obs.correctedFrequencyHz = (sample * apt * 2U / rate) % 2U ? 2300.0 : 1500.0;
                else if (sample < imageStart) {
                    const auto relative = sample - aptEnd;
                    const bool noise = relative / period == 60U;
                    obs.correctedFrequencyHz = noise || relative % period < period / 2U ? 2300.0 : 1500.0;
                } else obs.correctedFrequencyHz = 1900.0;
                batch.push_back(obs);
                if (batch.size() == 4096U) { receiver.consume(batch); batch.clear(); }
                sample += sample < aptEnd ? 1U : 6U;
            }
            receiver.consume(batch);
            QVERIFY(receiver.imageStartSample());
            QVERIFY(receiver.usedWmoTimedPhasing());
            QVERIFY(std::llabs(qint64(*receiver.imageStartSample()) - qint64(imageStart)) < 20);
        }
    }

    void wmoTimedPhasingDoesNotInventImageAfterSignalLossAB()
    {
        constexpr std::uint32_t rate = 12000U;
        constexpr std::uint64_t period = 3000U;
        constexpr std::uint64_t aptEnd = 5U * rate;
        constexpr std::uint64_t loss = aptEnd + 60U * period;
        for (const auto ioc : {SstvFaxIoc::Ioc576, SstvFaxIoc::Ioc288}) {
            SstvFaxConfig config;
            config.ioc = ioc;
            config.width = 32U;
            config.height = 4U;
            config.linesPerMinute = 240U;
            config.wmoTimedPhasing = true;
            SstvRadiofaxAutoRx receiver(config);
            const unsigned apt = ioc == SstvFaxIoc::Ioc576 ? 300U : 675U;
            std::vector<SstvFrequencyObservation> batch;
            for (std::uint64_t sample = 0; sample < aptEnd + 130U * period;) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = sample < loss ? SstvFrequencyStatus::Valid : SstvFrequencyStatus::LowSignal;
                obs.confidence = sample < loss ? 1.0 : 0.0;
                obs.correctedFrequencyHz = sample < aptEnd
                    ? ((sample * apt * 2U / rate) % 2U ? 2300.0 : 1500.0)
                    : ((sample - aptEnd) % period < period / 20U ? 2300.0 : 1500.0);
                batch.push_back(obs);
                if (batch.size() == 4096U) { receiver.consume(batch); batch.clear(); }
                sample += sample < aptEnd ? 1U : 6U;
            }
            receiver.consume(batch);
            QVERIFY(!receiver.imageStartSample());
            QVERIFY(!receiver.snapshot());
            QVERIFY(!receiver.usedWmoTimedPhasing());
            QCOMPARE(receiver.state(), SstvRadiofaxAutoRxState::SearchingPhasing);
        }
    }
    void autoReceiverCalibratesPhasingLevelsAB()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        for (const double scale : {0.99, 1.01}) {
            for (const double phasingConfidence : {0.4, 1.0}) {
                SstvRadiofaxAutoRx receiver(config);
                std::vector<SstvFrequencyObservation> batch;
                for (std::uint64_t sample = 0; sample < 23U * 6000U; sample += 6U) {
                    SstvFrequencyObservation obs;
                    obs.centreSample = sample;
                    obs.status = SstvFrequencyStatus::Valid;
                    obs.confidence = sample < 20U * 6000U ? phasingConfidence : 1.0;
                    obs.correctedFrequencyHz = (sample < 20U * 6000U
                        ? (sample % 6000U < 300U ? 2300.0 : 1500.0) : 1900.0) * scale;
                    batch.push_back(obs);
                    if (batch.size() == 4096U) { receiver.consume(batch); batch.clear(); }
                }
                receiver.consume(batch);
                receiver.finish();
                const auto image = receiver.snapshot();
                QVERIFY(image);
                QVERIFY(image->isScanlineComplete(0));
                const auto levels = receiver.imageLevels();
                QVERIFY(levels);
                if (phasingConfidence < 0.7) {
                    QCOMPARE(levels->blackHz, 1500.0);
                    QCOMPARE(levels->whiteHz, 2300.0);
                    QVERIFY(std::abs(int(image->pixel(10, 0).red) - 128) >= 5);
                    continue;
                }
                QVERIFY(std::abs(levels->blackHz - 1500.0 * scale) <= 1.0);
                QVERIFY(std::abs(levels->whiteHz - 2300.0 * scale) <= 1.0);
                QVERIFY2(std::abs(int(image->pixel(10, 0).red) - 128) <= 1,
                         qPrintable(QStringLiteral("scale=%1 pixel=%2").arg(scale).arg(image->pixel(10, 0).red)));
            }
        }
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
            SstvRadiofaxRxSession(config, 0U, 0.0, std::nullopt, {1551.0, 2300.0}));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
            SstvRadiofaxRxSession(config, 0U, 0.0, std::nullopt, {1500.0, 2341.0}));
    }
    void phasingClockTracksIndependentRateOffsets()
    {
        constexpr std::uint32_t rate = 12'000U;
        for (const auto lpm : {60U, 90U, 120U, 240U}) {
            for (const auto pattern : {SstvFaxPhasePattern::WhiteFivePercent,
                                       SstvFaxPhasePattern::Symmetric}) {
                for (const double scale : {0.99, 0.999, 1.001, 1.01}) {
                    const double period = double(rate) * 60.0 / lpm * scale;
                    SstvRadiofaxClockTracker tracker(rate,
                        {static_cast<std::uint16_t>(lpm), pattern, 0U});
                    SstvFrequencyObservation obs;
                    obs.status = SstvFrequencyStatus::Valid;
                    obs.confidence = 1.0;
                    const double fraction = pattern == SstvFaxPhasePattern::Symmetric ? 0.5 : 0.05;
                    const auto end = static_cast<std::uint64_t>(std::ceil(period * 18.0));
                    for (std::uint64_t sample = 0; sample < end; ++sample) {
                        obs.centreSample = sample;
                        obs.correctedFrequencyHz = std::fmod(double(sample), period) < period * fraction
                            ? 2300.0 : 1500.0;
                        tracker.consume(obs);
                    }
                    const auto measured = tracker.estimate();
                    QVERIFY(measured.has_value());
                    const double actual = double(measured->samples) / measured->lines;
                    QVERIFY2(std::abs(actual - period) < 0.2,
                             qPrintable(QStringLiteral("lpm=%1 measured=%2 expected=%3")
                                 .arg(lpm).arg(actual, 0, 'f', 5).arg(period, 0, 'f', 5)));
                }
            }
        }
    }

    void phasingStartWaitsForClockAtSmallOffsetAB()
    {
        constexpr std::uint32_t rate = 12'000U;
        for (const std::uint64_t period : {2991U, 3009U}) {
            SstvRadiofaxImageStartDetector detector(rate,
                {240U, SstvFaxPhasePattern::WhiteFivePercent, 0U, period});
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0; sample < period * 16U; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                obs.correctedFrequencyHz = sample % period < period / 20U ? 2300.0 : 1500.0;
                observations.push_back(obs);
                if (observations.size() == 4096U) {
                    QVERIFY(!detector.consume(observations));
                    observations.clear();
                }
            }
            QVERIFY(!detector.consume(observations));
            QVERIFY(detector.valid());
        }
    }

    void phasingLockUsesMedianOfRecentPeriodsAB()
    {
        for (const std::uint64_t base : {2991U, 3009U}) {
            const auto transition = base < 3000U ? 2U * base + 50U : 2U * base - 50U;
            const std::vector<std::uint64_t> edges {0U, base, transition,
                                                     transition + base, transition + 2U * base};
            SstvRadiofaxPhasingDetector detector(12'000U);
            std::optional<SstvRadiofaxPhaseLock> lock;
            for (std::uint64_t sample = 0; sample <= edges.back(); ++sample) {
                const auto after = std::upper_bound(edges.begin(), edges.end(), sample);
                const auto edge = *(after - 1);
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                obs.correctedFrequencyHz = sample - edge < 150U ? 2300.0 : 1500.0;
                if (const auto value = detector.consume(&obs, 1U)) { lock = value; break; }
            }
            QVERIFY(lock);
            QCOMPARE(lock->linesPerMinute, std::uint16_t(240U));
            QCOMPARE(lock->detectedPeriodSamples, base);
        }
    }

    void phasingClockRejectsInsufficientAndIrregularPulses()
    {
        SstvRadiofaxClockTracker tracker(12'000U,
            {120U, SstvFaxPhasePattern::WhiteFivePercent, 0U});
        SstvFrequencyObservation obs;
        obs.status = SstvFrequencyStatus::Valid;
        obs.confidence = 1.0;
        for (std::uint64_t sample = 0; sample < 12U * 6000U; ++sample) {
            obs.centreSample = sample;
            // Sustained malformed periods must invalidate a plausible clock.
            const auto within = sample < 9U * 6000U ? sample % 6000U
                : (sample - 9U * 6000U) % 6100U;
            obs.correctedFrequencyHz = within < 300U ? 2300.0 : 1500.0;
            tracker.consume(obs);
            if (sample == 4U * 6000U) QVERIFY(!tracker.estimate());
        }
        QVERIFY(!tracker.estimate());
        QVERIFY(!tracker.valid());
        obs.centreSample = 0U;
        tracker.consume(obs);
        QVERIFY(!tracker.estimate());
    }

    void provisionalPhasingClockRequiresConsecutiveEdgesAB()
    {
        constexpr std::uint32_t rate = 12'000U;
        for (const std::uint64_t period : {2991U, 3009U}) {
            for (const bool missing : {false, true}) {
                SstvRadiofaxClockTracker tracker(rate,
                    {240U, SstvFaxPhasePattern::WhiteFivePercent, 0U});
                SstvFrequencyObservation obs;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                for (std::uint64_t sample = 0; sample < period * 5U; sample += 3U) {
                    obs.centreSample = sample;
                    const auto line = sample / period;
                    const bool white = sample % period < period / 20U && !(missing && line == 2U);
                    obs.correctedFrequencyHz = white ? 2300.0 : 1500.0;
                    tracker.consume(obs);
                    if (sample < period * 2U) QVERIFY(!tracker.provisionalEstimate());
                }
                QVERIFY(tracker.valid());
                const auto estimate = tracker.provisionalEstimate();
                if (missing) QVERIFY(!estimate);
                else {
                    QVERIFY(estimate);
                    QVERIFY(std::abs(double(estimate->samples) / estimate->lines - double(period)) < 1.0);
                    QVERIFY(!tracker.estimate());
                }
            }
        }
    }

    void provisionalTwoEdgesRequirePriorReferenceAB()
    {
        for (const std::uint64_t period : {2991U, 3009U}) {
            for (const bool reference : {false, true}) {
                for (const bool missing : {false, true}) {
                    SstvRadiofaxClockTracker tracker(12000U,
                        {240U, SstvFaxPhasePattern::WhiteFivePercent, 0U});
                    if (reference) tracker.setPhaseReference(0U, {period, 1U});
                    SstvFrequencyObservation obs;
                    obs.status = SstvFrequencyStatus::Valid;
                    obs.confidence = 1.0;
                    for (std::uint64_t sample = 0; sample < period * 2U; sample += 3U) {
                        obs.centreSample = sample;
                        const auto line = sample / period;
                        obs.correctedFrequencyHz = (sample % period < period / 20U
                            && !(missing && line == 1U)) ? 2300.0 : 1500.0;
                        tracker.consume(obs);
                        if (sample < period) QVERIFY(!tracker.provisionalEstimate());
                    }
                    const auto estimate = tracker.provisionalEstimate();
                    if (reference && !missing) {
                        QVERIFY(estimate);
                        QVERIFY(std::abs(double(estimate->samples) / estimate->lines - double(period)) < 1.0);
                        QVERIFY(!tracker.estimate());
                    } else QVERIFY(!estimate);
                }
            }
        }
    }

    void phasingClockSurvivesSingleFalsePulseAB()
    {
        constexpr std::uint32_t rate = 12'000U;
        for (const std::uint64_t glitchStart : {2000U, 3500U}) {
            SstvRadiofaxClockTracker tracker(rate,
                {120U, SstvFaxPhasePattern::WhiteFivePercent, 0U});
            SstvFrequencyObservation obs;
            obs.status = SstvFrequencyStatus::Valid;
            obs.confidence = 1.0;
            bool checkedIrregular = false;
            for (std::uint64_t sample = 0; sample < 20U * 6000U; sample += 6U) {
                obs.centreSample = sample;
                const auto within = sample % 6000U;
                const bool falsePulse = sample / 6000U == 11U
                    && within >= glitchStart && within < glitchStart + 300U;
                obs.correctedFrequencyHz = within < 300U || falsePulse ? 2300.0 : 1500.0;
                tracker.consume(obs);
                if (sample / 6000U == 11U && within >= glitchStart + 300U
                    && within < glitchStart + 306U) {
                    QVERIFY(!tracker.provisionalEstimate());
                    checkedIrregular = true;
                }
            }
            QVERIFY(checkedIrregular);
            QVERIFY(tracker.valid());
            QCOMPARE(tracker.ignoredIrregularPulses(), 1U);
            const auto estimate = tracker.estimate();
            QVERIFY(estimate);
            QVERIFY(std::abs(double(estimate->samples) / estimate->lines - 6000.0) < 1.0);
        }
    }

    void phasingClockRejectsEarlyFalsePulseAB()
    {
        constexpr std::uint32_t rate = 12'000U;
        for (const std::uint64_t glitchStart : {2000U, 3500U}) {
            SstvRadiofaxClockTracker tracker(rate,
                {120U, SstvFaxPhasePattern::WhiteFivePercent, 0U});
            SstvFrequencyObservation obs;
            obs.status = SstvFrequencyStatus::Valid;
            obs.confidence = 1.0;
            for (std::uint64_t sample = 0; sample < 8U * 6000U; sample += 6U) {
                obs.centreSample = sample;
                const auto within = sample % 6000U;
                const bool falsePulse = sample / 6000U == 2U
                    && within >= glitchStart && within < glitchStart + 300U;
                obs.correctedFrequencyHz = within < 300U || falsePulse ? 2300.0 : 1500.0;
                tracker.consume(obs);
            }
            QVERIFY(!tracker.valid());
            QVERIFY(!tracker.estimate());
            QCOMPARE(tracker.ignoredIrregularPulses(), 0U);
        }
    }

    void phasingClockRejectsTwoFalsePulsesAB()
    {
        constexpr std::uint32_t rate = 12'000U;
        for (const std::uint64_t glitchStart : {1200U, 1800U}) {
            SstvRadiofaxClockTracker tracker(rate,
                {120U, SstvFaxPhasePattern::WhiteFivePercent, 0U});
            SstvFrequencyObservation obs;
            obs.status = SstvFrequencyStatus::Valid;
            obs.confidence = 1.0;
            for (std::uint64_t sample = 0; sample < 20U * 6000U; sample += 6U) {
                obs.centreSample = sample;
                const auto within = sample % 6000U;
                const bool first = within >= glitchStart && within < glitchStart + 300U;
                const bool second = within >= glitchStart + 1800U && within < glitchStart + 2100U;
                const bool falsePulse = sample / 6000U == 11U && (first || second);
                obs.correctedFrequencyHz = within < 300U || falsePulse ? 2300.0 : 1500.0;
                tracker.consume(obs);
            }
            QVERIFY(!tracker.valid());
            QVERIFY(!tracker.estimate());
            QCOMPARE(tracker.ignoredIrregularPulses(), 1U);
        }
    }

    void phasingClockReferenceRejectsPersistentFalsePulsesAB()
    {
        for (const std::uint64_t period : {2991U, 3009U}) {
            for (const bool wrongPhase : {false, true}) {
                SstvRadiofaxClockTracker tracker(12'000U,
                    {240U, SstvFaxPhasePattern::WhiteFivePercent, period});
                tracker.setPhaseReference(wrongPhase ? period - period / 4U : period,
                                          {period, 1U});
                SstvFrequencyObservation obs;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                for (std::uint64_t sample = period; sample < period * 40U; sample += 6U) {
                    obs.centreSample = sample;
                    const auto within = sample % period;
                    const bool pulse = within < period / 20U
                        || (within >= period * 2U / 5U && within < period * 9U / 20U)
                        || (within >= period * 16U / 25U && within < period * 69U / 100U);
                    obs.correctedFrequencyHz = pulse ? 2300.0 : 1500.0;
                    tracker.consume(obs);
                }
                if (wrongPhase) QVERIFY(!tracker.estimate());
                else {
                    const auto estimate = tracker.estimate();
                    QVERIFY(estimate);
                    QVERIFY(std::abs(double(estimate->samples) / estimate->lines - double(period)) < 1.0);
                    QVERIFY(tracker.ignoredIrregularPulses() > 20U);
                }
            }
        }
    }

    void phasingClockReferenceExpiresWithoutExpectedPulsesAB()
    {
        for (const std::uint64_t period : {2991U, 3009U}) {
            SstvRadiofaxClockTracker tracker(12'000U,
                {240U, SstvFaxPhasePattern::WhiteFivePercent, period});
            tracker.setPhaseReference(period, {period, 1U});
            SstvFrequencyObservation obs;
            obs.status = SstvFrequencyStatus::Valid;
            obs.confidence = 1.0;
            for (std::uint64_t sample = period; sample < period * 25U; sample += 6U) {
                obs.centreSample = sample;
                obs.correctedFrequencyHz = sample < period * 16U && sample % period < period / 20U
                    ? 2300.0 : 1500.0;
                tracker.consume(obs);
                if (sample >= period * 14U && sample < period * 14U + 6U)
                    QVERIFY(tracker.estimate());
            }
            QVERIFY(!tracker.valid());
            QVERIFY(!tracker.estimate());
        }
    }

    void phasingClockReferenceRejectsChangedRateAB()
    {
        for (const bool reverse : {false, true}) {
            const std::uint64_t initial = reverse ? 3009U : 2991U;
            const std::uint64_t changed = reverse ? 2991U : 3009U;
            SstvRadiofaxClockTracker tracker(12'000U,
                {240U, SstvFaxPhasePattern::WhiteFivePercent, initial});
            tracker.setPhaseReference(initial, {initial, 1U});
            SstvFrequencyObservation obs;
            obs.status = SstvFrequencyStatus::Valid;
            obs.confidence = 1.0;
            const auto changeAt = initial * 20U;
            for (std::uint64_t sample = initial; sample < changeAt + changed * 60U; sample += 6U) {
                obs.centreSample = sample;
                const auto within = sample < changeAt ? sample % initial : (sample - changeAt) % changed;
                const auto period = sample < changeAt ? initial : changed;
                obs.correctedFrequencyHz = within < period / 20U ? 2300.0 : 1500.0;
                tracker.consume(obs);
                if (sample >= initial * 14U && sample < initial * 14U + 6U)
                    QVERIFY(tracker.estimate());
            }
            QVERIFY(!tracker.valid());
            QVERIFY(!tracker.estimate());
        }
    }

    void autoReceiverReacquiresClockAfterTwoFalsePulsesAB()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        config.linesPerMinute = 240U;
        config.centeredPhasingWithWhiteEndLine = true;
        for (const std::uint64_t period : {2991U, 3009U}) {
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> observations;
            const auto end = period * 124U;
            for (std::uint64_t sample = 0; sample < end; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                const auto line = sample / period;
                const auto within = sample % period;
                const bool phasingWhite = within < period / 40U
                    || within >= period - period / 40U;
                const bool glitch = line == 18U
                    && ((within >= period * 2U / 5U && within < period * 9U / 20U)
                        || (within >= period * 16U / 25U && within < period * 69U / 100U));
                obs.correctedFrequencyHz = line < 120U ? (phasingWhite || glitch ? 2300.0 : 1500.0)
                    : (line == 120U ? 2300.0 : 1900.0);
                observations.push_back(obs);
                if (observations.size() == 4096U) { receiver.consume(observations); observations.clear(); }
            }
            receiver.consume(observations);
            QVERIFY(receiver.imageStartSample());
            QVERIFY2(std::abs(double(*receiver.imageStartSample()) - double(period * 121U)) < 60.0,
                     "receiver acquired image during phasing after two false pulses");
            const auto clock = receiver.imageClock();
            QVERIFY(clock);
            QVERIFY(std::abs(double(clock->samples) / clock->lines - double(period)) < 1.0);
            QCOMPARE(receiver.phasingClockRestarts(), 1U);
            QCOMPARE(receiver.ignoredIrregularPhasingPulses(), 1U);
        }
    }

    void autoReceiverStartsImageDuringClockReacquisitionAB()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        config.linesPerMinute = 240U;
        config.centeredPhasingWithWhiteEndLine = true;
        for (const std::uint64_t period : {2991U, 3009U}) {
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> observations;
            const auto end = period * 23U;
            for (std::uint64_t sample = 0; sample < end; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                const auto line = sample / period;
                const auto within = sample % period;
                const bool phasingWhite = within < period / 40U
                    || within >= period - period / 40U;
                const bool glitch = line == 18U
                    && ((within >= period * 2U / 5U && within < period * 9U / 20U)
                        || (within >= period * 16U / 25U && within < period * 69U / 100U));
                obs.correctedFrequencyHz = line < 19U ? (phasingWhite || glitch ? 2300.0 : 1500.0)
                    : (line == 19U ? 2300.0 : 1900.0);
                observations.push_back(obs);
                if (observations.size() == 4096U) { receiver.consume(observations); observations.clear(); }
            }
            receiver.consume(observations);
            QVERIFY(receiver.imageStartSample());
            QVERIFY2(std::abs(double(*receiver.imageStartSample()) - double(period * 20U)) < 60.0,
                     "receiver acquired image during phasing after two false pulses");
            QVERIFY(receiver.imageClock());
            QVERIFY(receiver.imageClockProvisional());
            QVERIFY(std::abs(double(receiver.imageClock()->samples) / receiver.imageClock()->lines - double(period)) < 1.0);
            QVERIFY(receiver.phasingClockRestarts() >= 1U);
        }
    }

    void autoReceiverMarksUnconfirmedImmediateClockChangeAB()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        config.linesPerMinute = 240U;
        config.centeredPhasingWithWhiteEndLine = true;
        for (const bool reverse : {false, true}) {
            const std::uint64_t initial = reverse ? 3009U : 2991U;
            const std::uint64_t changed = reverse ? 2991U : 3009U;
            const auto endPhasing = initial * 19U;
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0; sample < endPhasing + changed * 4U; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                const auto line = sample / initial;
                const auto within = sample % initial;
                const bool white = within < initial / 40U || within >= initial - initial / 40U;
                const bool glitch = line == 18U && sample < endPhasing
                    && ((within >= initial * 2U / 5U && within < initial * 9U / 20U)
                        || (within >= initial * 16U / 25U && within < initial * 69U / 100U));
                obs.correctedFrequencyHz = sample < endPhasing ? (white || glitch ? 2300.0 : 1500.0)
                    : (sample < endPhasing + changed ? 2300.0 : 1900.0);
                observations.push_back(obs);
                if (observations.size() == 4096U) { receiver.consume(observations); observations.clear(); }
            }
            receiver.consume(observations);
            QVERIFY(receiver.imageStartSample());
            QVERIFY(receiver.imageClock());
            QVERIFY(receiver.imageClockProvisional());
            const auto used = double(receiver.imageClock()->samples) / receiver.imageClock()->lines;
            QVERIFY(std::abs(used - double(initial)) < 1.0);
            QVERIFY(std::abs(used - double(changed)) > 15.0);
        }
    }

    void autoReceiverAcceptsNoisyWhiteEndLineAB()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        config.linesPerMinute = 240U;
        config.centeredPhasingWithWhiteEndLine = true;
        for (const std::uint64_t period : {2991U, 3009U}) {
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> observations;
            const auto end = period * 23U;
            for (std::uint64_t sample = 0; sample < end; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                const auto line = sample / period;
                const auto within = sample % period;
                const bool phasingWhite = within < period / 40U
                    || within >= period - period / 40U;
                const bool glitch = line == 18U
                    && ((within >= period * 2U / 5U && within < period * 9U / 20U)
                        || (within >= period * 16U / 25U && within < period * 69U / 100U));
                obs.correctedFrequencyHz = line < 19U ? (phasingWhite || glitch ? 2300.0 : 1500.0)
                    : (line == 19U ? (within >= period / 3U && within < period * 8U / 15U ? 1500.0 : 2300.0) : 1900.0);
                observations.push_back(obs);
                if (observations.size() == 4096U) { receiver.consume(observations); observations.clear(); }
            }
            receiver.consume(observations);
            QVERIFY(receiver.imageStartSample());
            QVERIFY2(std::abs(double(*receiver.imageStartSample()) - double(period * 20U)) < 60.0,
                     "receiver acquired image during phasing after two false pulses");
            QVERIFY(receiver.imageClock());
            QVERIFY(receiver.imageClockProvisional());
            QVERIFY(std::abs(double(receiver.imageClock()->samples) / receiver.imageClock()->lines - double(period)) < 1.0);
            QVERIFY(receiver.phasingClockRestarts() >= 1U);
        }
    }

    void autoReceiverDoesNotUseExpiredClockAB()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        config.linesPerMinute = 240U;
        config.centeredPhasingWithWhiteEndLine = true;
        for (const std::uint64_t period : {2991U, 3009U}) {
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> batch;
            for (std::uint64_t sample = 0; sample < period * 34U; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                const auto line = sample / period;
                const auto within = sample % period;
                const bool white = within < period / 40U || within >= period - period / 40U;
                // An extended black interval prevents a new estimate before
                // the end line. It must not keep the old raster clock alive.
                obs.correctedFrequencyHz = line < 18U ? (white ? 2300.0 : 1500.0)
                    : (line < 30U ? 1500.0 : (line == 30U ? 2300.0 : 1900.0));
                batch.push_back(obs);
                if (batch.size() == 4096U) { receiver.consume(batch); batch.clear(); }
            }
            receiver.consume(batch);
            QVERIFY(receiver.imageStartSample());
            QVERIFY(!receiver.imageClock());
            QVERIFY(!receiver.imageClockProvisional());
        }
    }

    void autoReceiverBoundsRepeatedClockRestartsAB()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        config.linesPerMinute = 240U;
        config.centeredPhasingWithWhiteEndLine = true;
        for (const std::uint64_t period : {2991U, 3009U}) {
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0; sample < period * 124U; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                const auto line = sample / period;
                const auto within = sample % period;
                const bool phasingWhite = within < period / 40U
                    || within >= period - period / 40U;
                const bool disturbedLine = line == 18U || line == 36U
                    || line == 54U || line == 72U;
                const bool glitch = disturbedLine
                    && ((within >= period * 2U / 5U && within < period * 9U / 20U)
                        || (within >= period * 16U / 25U && within < period * 69U / 100U));
                obs.correctedFrequencyHz = line < 120U ? (phasingWhite || glitch ? 2300.0 : 1500.0)
                    : (line == 120U ? 2300.0 : 1900.0);
                observations.push_back(obs);
                if (observations.size() == 4096U) { receiver.consume(observations); observations.clear(); }
            }
            receiver.consume(observations);
            QCOMPARE(receiver.phasingClockRestarts(), 1U);
            QVERIFY(receiver.phasingClockTrackerValid());
            QVERIFY(receiver.imageClock());
            QVERIFY(std::abs(double(receiver.imageClock()->samples) / receiver.imageClock()->lines - double(period)) < 1.0);
            QVERIFY(receiver.imageStartSample());
            QVERIFY2(std::abs(double(*receiver.imageStartSample()) - double(period * 121U)) < 60.0,
                     "receiver acquired image during phasing after exhausting clock retries");
        }
    }

    void autoReceiverReacquiresAfterCloselySpacedDisturbancesAB()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        config.linesPerMinute = 240U;
        config.centeredPhasingWithWhiteEndLine = true;
        for (const std::uint64_t period : {2991U, 3009U}) {
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0; sample < period * 124U; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                const auto line = sample / period;
                const auto within = sample % period;
                const bool phasingWhite = within < period / 40U
                    || within >= period - period / 40U;
                const bool disturbedLine = line == 18U || line == 20U
                    || line == 22U || line == 24U;
                const bool glitch = disturbedLine
                    && ((within >= period * 2U / 5U && within < period * 9U / 20U)
                        || (within >= period * 16U / 25U && within < period * 69U / 100U));
                obs.correctedFrequencyHz = line < 120U ? (phasingWhite || glitch ? 2300.0 : 1500.0)
                    : (line == 120U ? 2300.0 : 1900.0);
                observations.push_back(obs);
                if (observations.size() == 4096U) { receiver.consume(observations); observations.clear(); }
            }
            receiver.consume(observations);
            QVERIFY(receiver.phasingClockTrackerValid());
            QVERIFY(receiver.imageClock());
            QVERIFY(std::abs(double(receiver.imageClock()->samples) / receiver.imageClock()->lines - double(period)) < 1.0);
            QVERIFY(receiver.imageStartSample());
            QVERIFY2(std::abs(double(*receiver.imageStartSample()) - double(period * 121U)) < 60.0,
                     "receiver acquired image during phasing after exhausting clock retries");
        }
    }

    void autoReceiverTracksClockDuringPersistentDisturbanceAB()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        config.linesPerMinute = 240U;
        config.centeredPhasingWithWhiteEndLine = true;
        for (const std::uint64_t period : {2991U, 3009U}) {
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0; sample < period * 124U; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                const auto line = sample / period;
                const auto within = sample % period;
                const bool phasingWhite = within < period / 40U
                    || within >= period - period / 40U;
                const bool disturbedLine = line >= 18U && line < 120U;
                const bool glitch = disturbedLine
                    && ((within >= period * 2U / 5U && within < period * 9U / 20U)
                        || (within >= period * 16U / 25U && within < period * 69U / 100U));
                obs.correctedFrequencyHz = line < 120U ? (phasingWhite || glitch ? 2300.0 : 1500.0)
                    : (line == 120U ? 2300.0 : 1900.0);
                observations.push_back(obs);
                if (observations.size() == 4096U) { receiver.consume(observations); observations.clear(); }
            }
            receiver.consume(observations);
            QVERIFY(receiver.imageStartSample());
            QVERIFY(receiver.imageClock());
            QVERIFY(std::abs(double(receiver.imageClock()->samples) / receiver.imageClock()->lines - double(period)) < 1.0);
            QCOMPARE(receiver.phasingClockRestarts(), 1U);
            QVERIFY2(std::abs(double(*receiver.imageStartSample()) - double(period * 121U)) < 60.0,
                     qPrintable(QStringLiteral("early start at %1, expected %2")
                                    .arg(*receiver.imageStartSample()).arg(period * 121U)));
        }
    }

    void autoReceiverReacquiresChangedClockAB()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        config.linesPerMinute = 240U;
        config.centeredPhasingWithWhiteEndLine = true;
        for (const bool reverse : {false, true}) {
            const std::uint64_t initial = reverse ? 3009U : 2991U;
            const std::uint64_t changed = reverse ? 2991U : 3009U;
            const auto changeAt = initial * 30U;
            const auto imageWhite = changeAt + changed * 90U;
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0; sample < imageWhite + changed * 4U; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                const auto within = sample < changeAt ? sample % initial : (sample - changeAt) % changed;
                const auto period = sample < changeAt ? initial : changed;
                const bool white = within < period / 40U || within >= period - period / 40U;
                const bool glitch = sample / initial == 18U && sample < changeAt
                    && ((within >= initial * 2U / 5U && within < initial * 9U / 20U)
                        || (within >= initial * 16U / 25U && within < initial * 69U / 100U));
                obs.correctedFrequencyHz = sample < imageWhite ? (white || glitch ? 2300.0 : 1500.0)
                    : (sample < imageWhite + changed ? 2300.0 : 1900.0);
                observations.push_back(obs);
                if (observations.size() == 4096U) { receiver.consume(observations); observations.clear(); }
            }
            receiver.consume(observations);
            QVERIFY(receiver.imageStartSample());
            QVERIFY2(std::abs(double(*receiver.imageStartSample()) - double(imageWhite + changed)) < 80.0,
                     qPrintable(QStringLiteral("start %1 expected %2").arg(*receiver.imageStartSample()).arg(imageWhite + changed)));
            QVERIFY(receiver.imageClock());
            QVERIFY(!receiver.imageClockProvisional());
            QVERIFY(std::abs(double(receiver.imageClock()->samples) / receiver.imageClock()->lines - double(changed)) < 1.0);
        }
    }

    void autoReceiverRejectsBroadFalseWhitePhasingAB()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        config.linesPerMinute = 240U;
        config.centeredPhasingWithWhiteEndLine = true;
        for (const bool reverse : {false, true}) for (const std::uint64_t whiteEndTwentieths : {13U, 18U}) {
            const std::uint64_t initial = reverse ? 3009U : 2991U;
            const std::uint64_t changed = reverse ? 2991U : 3009U;
            const auto changeAt = initial * 30U;
            const auto imageWhite = changeAt + changed * 90U;
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0; sample < imageWhite + changed * 4U; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                const auto within = sample < changeAt ? sample % initial : (sample - changeAt) % changed;
                const auto period = sample < changeAt ? initial : changed;
                const bool white = within < period / 40U || within >= period - period / 40U;
                const bool glitch = (sample / initial == 18U && sample < changeAt
                    && ((within >= initial * 2U / 5U && within < initial * 9U / 20U)
                        || (within >= initial * 16U / 25U && within < initial * 69U / 100U)))
                    || (sample >= changeAt + changed * 10U && sample < changeAt + changed * 18U
                        && within >= changed / 4U && within < changed * whiteEndTwentieths / 20U);
                obs.correctedFrequencyHz = sample < imageWhite ? (white || glitch ? 2300.0 : 1500.0)
                    : (sample < imageWhite + changed ? 2300.0 : 1900.0);
                observations.push_back(obs);
                if (observations.size() == 4096U) { receiver.consume(observations); observations.clear(); }
            }
            receiver.consume(observations);
            QVERIFY(receiver.imageStartSample());
            QVERIFY2(std::abs(double(*receiver.imageStartSample()) - double(imageWhite + changed)) < 80.0,
                     qPrintable(QStringLiteral("start %1 expected %2 reverse=%3 end=%4").arg(*receiver.imageStartSample()).arg(imageWhite + changed).arg(reverse).arg(whiteEndTwentieths)));
            QVERIFY(receiver.imageClock());
        }
    }

    void autoReceiverReacquiresChangedClockWithNoisyEndAB()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        config.linesPerMinute = 240U;
        config.centeredPhasingWithWhiteEndLine = true;
        for (const bool reverse : {false, true}) {
            const std::uint64_t initial = reverse ? 3009U : 2991U;
            const std::uint64_t changed = reverse ? 2991U : 3009U;
            const auto changeAt = initial * 30U;
            const auto imageWhite = changeAt + changed * 90U;
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0; sample < imageWhite + changed * 4U; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                const auto within = sample < changeAt ? sample % initial : (sample - changeAt) % changed;
                const auto period = sample < changeAt ? initial : changed;
                const bool white = within < period / 40U || within >= period - period / 40U;
                const bool glitch = sample / initial == 18U && sample < changeAt
                    && ((within >= initial * 2U / 5U && within < initial * 9U / 20U)
                        || (within >= initial * 16U / 25U && within < initial * 69U / 100U));
                obs.correctedFrequencyHz = sample < imageWhite ? (white || glitch ? 2300.0 : 1500.0)
                    : (sample < imageWhite + changed ? ((sample - imageWhite >= changed / 3U && sample - imageWhite < changed * 8U / 15U) ? 1500.0 : 2300.0) : 1900.0);
                observations.push_back(obs);
                if (observations.size() == 4096U) { receiver.consume(observations); observations.clear(); }
            }
            receiver.consume(observations);
            QVERIFY(receiver.imageStartSample());
            QVERIFY(receiver.imageClock());
            QVERIFY2(std::abs(double(*receiver.imageStartSample()) - double(imageWhite + changed)) < 80.0,
                     qPrintable(QStringLiteral("start %1 expected %2").arg(*receiver.imageStartSample()).arg(imageWhite + changed)));
        }
    }

    void autoReceiverStartsBeforePhasingLikeImageAB()
    {
        // Image content can repeat a broad white stripe or resemble the
        // phasing pulse. Neither is evidence that phasing has continued.
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        config.linesPerMinute = 240U;
        config.centeredPhasingWithWhiteEndLine = true;
        for (const bool reverse : {false, true}) for (const bool narrow : {false, true}) {
            const std::uint64_t initial = reverse ? 3009U : 2991U;
            const std::uint64_t changed = reverse ? 2991U : 3009U;
            const auto changeAt = initial * 30U;
            const auto imageWhite = changeAt + changed * 90U;
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0; sample < imageWhite + changed * 5U; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                const auto within = sample < changeAt ? sample % initial : (sample - changeAt) % changed;
                const auto period = sample < changeAt ? initial : changed;
                const bool phaseWhite = within < period / 40U || within >= period - period / 40U;
                const bool imageStripe = narrow ? phaseWhite
                    : (within >= period / 4U && within < period * 9U / 10U) || phaseWhite;
                obs.correctedFrequencyHz = sample < imageWhite ? (phaseWhite ? 2300.0 : 1500.0)
                    : (sample < imageWhite + changed ? 2300.0 : (imageStripe ? 2300.0 : 1500.0));
                observations.push_back(obs);
                if (observations.size() == 4096U) { receiver.consume(observations); observations.clear(); }
            }
            receiver.consume(observations);
            QVERIFY(receiver.imageStartSample());
            QVERIFY2(std::abs(double(*receiver.imageStartSample()) - double(imageWhite + changed)) < 80.0,
                     qPrintable(QStringLiteral("start %1 expected %2 reverse=%3 narrow=%4")
                                    .arg(*receiver.imageStartSample()).arg(imageWhite + changed).arg(reverse).arg(narrow)));
        }
    }

    void autoReceiverRejectsEarlyBroadWhitePhasingAB()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 3U;
        config.linesPerMinute = 240U;
        config.centeredPhasingWithWhiteEndLine = true;
        for (const std::uint64_t period : {2991U, 3009U}) {
            const auto end = period * 20U;
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0; sample < end + period * 5U; sample += 6U) {
                const auto within = sample % period;
                const bool phaseWhite = within < period / 40U || within >= period - period / 40U;
                const bool interference = sample >= period * 10U && sample < period * 18U
                    && within >= period / 4U && within < period * 9U / 10U;
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                obs.correctedFrequencyHz = sample < end ? (phaseWhite || interference ? 2300.0 : 1500.0)
                    : (sample < end + period ? 2300.0 : 1900.0);
                observations.push_back(obs);
                if (observations.size() == 4096U) { receiver.consume(observations); observations.clear(); }
            }
            receiver.consume(observations);
            QVERIFY(receiver.imageStartSample());
            QVERIFY2(std::abs(double(*receiver.imageStartSample()) - double(end + period)) < 80.0,
                     qPrintable(QStringLiteral("start %1 expected %2 period=%3")
                                    .arg(*receiver.imageStartSample()).arg(end + period).arg(period)));
        }
    }

    void measuredClockMapsObservedPixelsAtBothClockOffsets()
    {
        SstvFaxConfig config;
        config.width = 128U;
        config.height = 150U;
        for (const auto clock : {SstvRadiofaxClockEstimate {599'940U, 100U},
                                 SstvRadiofaxClockEstimate {600'060U, 100U}}) {
            SstvRadiofaxRxSession corrected(config, 0U, 0.0, clock);
            SstvRadiofaxRxSession nominal(config, 0U);
            std::vector<SstvFrequencyObservation> observations;
            const auto end = 120U * clock.samples / clock.lines;
            for (std::uint64_t sample = 0; sample < end; sample += 6U) {
                const auto line = ((sample + 1U) * clock.lines - 1U) / clock.samples;
                const auto begin = line * clock.samples / clock.lines;
                const auto next = (line + 1U) * clock.samples / clock.lines;
                const auto pixel = (sample - begin) * config.width / (next - begin);
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                obs.correctedFrequencyHz = ((pixel + line) % 2U) ? 2300.0 : 1500.0;
                observations.push_back(obs);
                if (observations.size() == SstvRadiofaxRxSession::MaximumObservationsPerConsume) {
                    corrected.consume(observations);
                    nominal.consume(observations);
                    observations.clear();
                }
            }
            corrected.consume(observations);
            nominal.consume(observations);
            corrected.finish();
            nominal.finish();
            const auto image = corrected.snapshot();
            QCOMPARE(image.completedPixels, std::size_t(120U * config.width));
            for (std::uint32_t y = 0; y < 120U; ++y) {
                QVERIFY(image.isScanlineComplete(y));
                for (std::uint32_t x = 0; x < config.width; ++x)
                    QCOMPARE(image.pixel(x, y).red, std::uint8_t((x + y) % 2U ? 255U : 0U));
            }
            if (clock.samples < 600'000U)
                QVERIFY(nominal.snapshot().completedPixels < image.completedPixels);
        }
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvRadiofaxRxSession(config, 0U, 0.0,
            SstvRadiofaxClockEstimate {1U, 1U}));
    }

    void autoReceiverTracksPhasingClockForBothRateOffsets()
    {
        SstvFaxConfig config;
        config.width = 128U;
        config.height = 150U;
        for (const std::uint64_t periodNumerator : {594'000U, 599'400U, 600'600U, 606'000U}) {
            constexpr std::uint64_t denominator = 100U;
            const auto boundary = [=](std::uint64_t line) {
                return line * periodNumerator / denominator;
            };
            const auto imageStart = boundary(20U);
            const auto imageEnd = boundary(140U);
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> batch;
            for (std::uint64_t sample = 0; sample < imageEnd + 12'000U; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                if (sample < imageStart) {
                    const auto line = ((sample + 1U) * denominator - 1U) / periodNumerator;
                    const auto within = sample - boundary(line);
                    const auto length = boundary(line + 1U) - boundary(line);
                    obs.correctedFrequencyHz = within * 20U < length ? 2300.0 : 1500.0;
                } else {
                    obs.correctedFrequencyHz = 1900.0;
                }
                batch.push_back(obs);
                if (batch.size() == 4096U) {
                    receiver.consume(batch);
                    batch.clear();
                }
            }
            receiver.consume(batch);
            QVERIFY(receiver.imageStartSample());
            QVERIFY2(std::llabs(static_cast<long long>(*receiver.imageStartSample())
                                - static_cast<long long>(imageStart)) < 10LL,
                     qPrintable(QString::number(static_cast<long long>(*receiver.imageStartSample())
                                                - static_cast<long long>(imageStart))));
            QVERIFY(receiver.stopAtAudioSample(imageEnd));
            QVERIFY(receiver.stopWasAudioTone());
            const auto image = receiver.snapshot();
            QVERIFY(image);
            QCOMPARE(image->completedPixels, std::size_t(120U * config.width));
            QCOMPARE(receiver.state(), SstvRadiofaxAutoRxState::Partial);
            QVERIFY(!image->isComplete());
            QCOMPARE(image->coveredComponents, std::size_t(120U * config.width * 3U));
            for (std::uint32_t row = 0; row < config.height; ++row) {
                QCOMPARE(image->isScanlineComplete(row), row < 120U);
                for (std::uint32_t column = 0; column < config.width; ++column)
                    QCOMPARE(image->coverageMask(column, row), std::uint8_t(row < 120U ? 0x07U : 0U));
            }
        }
    }

    void autoReceiverTracksWmoDeadSectorClockAB()
    {
        SstvFaxConfig config;
        config.width = 128U;
        config.height = 150U;
        config.wmoDeadSector = true;
        for (const std::uint64_t numerator : {596'400U, 603'600U}) {
            for (const double stopHz : {440.0, 445.0, 447.3, 450.0, 452.7, 455.0, 460.0}) {
                constexpr std::uint64_t denominator = 100U;
                const auto boundary = [=](std::uint64_t line) { return line * numerator / denominator; };
                const auto imageStart = boundary(20U);
                const auto imageEnd = imageStart + boundary(120U);
                SstvRadiofaxAutoRx receiver(config);
                std::vector<SstvFrequencyObservation> batch;
                for (std::uint64_t sample = 0U; sample < imageEnd + 48'000U; sample += 6U) {
                    SstvFrequencyObservation obs;
                    obs.centreSample = sample;
                    obs.status = SstvFrequencyStatus::Valid;
                    obs.confidence = 1.0;
                    if (sample < imageStart) {
                        const auto line = ((sample + 1U) * denominator - 1U) / numerator;
                        const auto within = sample - boundary(line);
                        const auto length = boundary(line + 1U) - boundary(line);
                        obs.correctedFrequencyHz = within * 20U < length ? 2300.0 : 1500.0;
                    } else if (sample >= imageEnd) {
                        obs.correctedFrequencyHz = std::fmod(double(sample - imageEnd) * stopHz * 2.0
                                                             / config.sampleRate, 2.0) >= 1.0 ? 2300.0 : 1500.0;
                    } else {
                        const auto offset = sample - imageStart;
                        const auto line = ((offset + 1U) * denominator - 1U) / numerator;
                        const auto within = offset - boundary(line);
                        const auto length = boundary(line + 1U) - boundary(line);
                        const auto dead = sstvRadiofaxDeadSectorSamples(length);
                        if (within < dead) obs.correctedFrequencyHz = 2300.0;
                        else if (line < 10U) obs.correctedFrequencyHz = 1900.0;
                        else {
                            const auto pixel = (within - dead) * config.width / (length - dead);
                            obs.correctedFrequencyHz = (pixel + line) % 2U ? 2300.0 : 1500.0;
                        }
                    }
                    batch.push_back(obs);
                    if (batch.size() == 4096U) {
                        receiver.consume(batch);
                        batch.clear();
                    }
                }
                receiver.consume(batch);
                QVERIFY(receiver.imageStartSample());
                QVERIFY(std::llabs(static_cast<long long>(*receiver.imageStartSample())
                                   - static_cast<long long>(imageStart)) < 10LL);
                QVERIFY(receiver.imageClock());
                QVERIFY(std::abs(double(receiver.imageClock()->samples) / receiver.imageClock()->lines
                                 - double(numerator) / denominator) < 0.2);
                QVERIFY(receiver.stopSample());
                QVERIFY2(std::llabs(static_cast<long long>(*receiver.stopSample())
                                   - static_cast<long long>(imageEnd)) < 10LL,
                         qPrintable(QStringLiteral("stopHz=%1 delta=%2").arg(stopHz)
                             .arg(qint64(*receiver.stopSample()) - qint64(imageEnd))));
                QVERIFY(!receiver.stopWasAudioTone());
                const auto image = receiver.snapshot();
                QVERIFY(image);
                QCOMPARE(image->completedPixels, std::size_t(120U * config.width));
                QCOMPARE(receiver.state(), SstvRadiofaxAutoRxState::Partial);
                QVERIFY(!image->isComplete());
                QCOMPARE(image->coveredComponents, std::size_t(120U * config.width * 3U));
                for (std::uint32_t row = 0; row < config.height; ++row) {
                    QCOMPARE(image->isScanlineComplete(row), row < 120U);
                    for (std::uint32_t column = 0; column < config.width; ++column)
                        QCOMPARE(image->coverageMask(column, row), std::uint8_t(row < 120U ? 0x07U : 0U));
                }
                for (std::uint32_t y = 10U; y < 120U; ++y)
                    for (std::uint32_t x = 0U; x < config.width; ++x)
                        QCOMPARE(image->pixel(x, y).red, std::uint8_t((x + y) % 2U ? 255U : 0U));
            }
        }
    }

    void autoReceiverTracksLongPhasingAB()
    {
        SstvFaxConfig config;
        config.width = 128U;
        config.height = 4U;
        for (const auto lpm : {60U, 90U, 120U, 240U})
        for (const auto pattern : {SstvFaxPhasePattern::WhiteFivePercent,
                                   SstvFaxPhasePattern::Symmetric})
        for (const std::uint64_t scale : {990U, 997U, 1003U, 1010U}) {
            config.linesPerMinute = static_cast<std::uint16_t>(lpm);
            config.phasing = pattern;
            const std::uint64_t period = std::uint64_t(config.sampleRate) * 60U * scale / (lpm * 1000U);
            constexpr std::uint64_t phaseLines = 180U;
            const auto expectedStart = period * phaseLines;
            SstvRadiofaxAutoRx receiver(config);
            std::vector<SstvFrequencyObservation> batch;
            for (std::uint64_t sample = 0; sample < expectedStart + 3U * period; sample += 6U) {
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                const auto whiteLength = pattern == SstvFaxPhasePattern::Symmetric
                    ? period / 2U : period / 20U;
                obs.correctedFrequencyHz = sample < expectedStart
                    ? (sample % period < whiteLength ? 2300.0 : 1500.0)
                    : 1900.0;
                batch.push_back(obs);
                if (batch.size() == 4096U) {
                    receiver.consume(batch);
                    batch.clear();
                }
            }
            receiver.consume(batch);
            QVERIFY(receiver.imageStartSample());
            QVERIFY2(std::llabs(static_cast<long long>(*receiver.imageStartSample())
                                - static_cast<long long>(expectedStart)) < 20LL,
                     qPrintable(QStringLiteral("lpm=%1 pattern=%2 scale=%3 delta=%4")
                         .arg(lpm).arg(int(pattern)).arg(scale)
                         .arg(static_cast<long long>(*receiver.imageStartSample())
                              - static_cast<long long>(expectedStart))));
        }
    }

    void autoReceiverFallsBackWhenClockCannotResolveRaster()
    {
        SstvFaxConfig config;
        config.linesPerMinute = 240U;
        config.width = 3000U;
        config.height = 3U;
        SstvRadiofaxAutoRx receiver(config);
        std::vector<SstvFrequencyObservation> batch;
        constexpr std::uint64_t period = 2997U;
        for (std::uint64_t sample = 0; sample < period * 22U; sample += 2U) {
            SstvFrequencyObservation obs;
            obs.centreSample = sample;
            obs.status = SstvFrequencyStatus::Valid;
            obs.confidence = 1.0;
            obs.correctedFrequencyHz = sample < period * 20U
                ? (sample % period < period / 20U ? 2300.0 : 1500.0) : 1900.0;
            batch.push_back(obs);
            if (batch.size() == 4096U) {
                receiver.consume(batch);
                batch.clear();
            }
        }
        receiver.consume(batch);
        QVERIFY(receiver.imageStartSample());
        QVERIFY(receiver.state() != SstvRadiofaxAutoRxState::Failed);
    }

    void centeredTransmitPhasingAB()
    {
        for (auto rate : {12'000U, 48'000U}) {
            for (auto lpm : {120U, 240U}) {
                SstvFaxConfig base;
                base.sampleRate=rate; base.linesPerMinute=static_cast<std::uint16_t>(lpm);
                base.width=128; base.height=2;
                auto aligned=base; aligned.centeredPhasingWithWhiteEndLine=true;
                SstvRadiofaxMapper normal(base), centered(aligned);
                const auto phaseStart=std::uint64_t(rate)*5U;
                const auto line=std::uint64_t(rate)*60U/lpm;
                QCOMPARE(centered.imageStartSample(),normal.imageStartSample()+line);
                QVERIFY(centered.positionAtSample(phaseStart).white);
                QVERIFY(centered.positionAtSample(phaseStart+line/100U).white);
                QVERIFY(!centered.positionAtSample(phaseStart+line/2U).white);
                QVERIFY(centered.positionAtSample(phaseStart+line-line/100U).white);
                QVERIFY(!normal.positionAtSample(phaseStart+line-line/100U).white);
                QVERIFY(centered.positionAtSample(centered.imageStartSample()-line/2U).white);
                QCOMPARE(centered.positionAtSample(centered.imageStartSample()).region,SstvFaxRegion::Image);
            }
        }
    }
    void wmoDeadSectorMapsActiveRasterAB()
    {
        for (const unsigned rpm : {120U, 240U}) {
            SstvFaxConfig cfg;
            cfg.linesPerMinute = static_cast<std::uint16_t>(rpm);
            cfg.sampleRate = rpm == 120U ? 11'025U : 12'000U;
            cfg.width = 8U; cfg.height = 2U; cfg.wmoDeadSector = true;
            SstvRadiofaxMapper mapper(cfg);
            for (std::uint32_t line = 0; line < cfg.height; ++line) {
                const auto start = mapper.lineStartSample(line);
                const auto lineSamples = mapper.lineStartSample(line + 1U) - start;
                const auto dead = lineSamples * 45U / 1000U;
                QCOMPARE(mapper.positionAtSample(start).region, SstvFaxRegion::DeadSector);
                QVERIFY(mapper.positionAtSample(start).white);
                QCOMPARE(mapper.positionAtSample(start + dead - 1U).region, SstvFaxRegion::DeadSector);
                QCOMPARE(mapper.positionAtSample(start + dead).region, SstvFaxRegion::Image);
                QCOMPARE(mapper.positionAtSample(start + dead).pixel, std::uint32_t(0));
                QCOMPARE(mapper.positionAtSample(start + lineSamples - 1U).pixel, cfg.width - 1U);
            }
            SstvRadiofaxRxSession onlyDead(cfg, mapper.imageStartSample());
            for (std::uint32_t line = 0; line < cfg.height; ++line) {
                const auto start = mapper.lineStartSample(line);
                const auto length = mapper.lineStartSample(line + 1U) - start;
                const auto dead = length * 45U / 1000U;
                for (std::uint64_t i = 0; i < dead; ++i) {
                    SstvFrequencyObservation obs;
                    obs.status = SstvFrequencyStatus::Valid;
                    obs.confidence = 1.0;
                    obs.centreSample = start + i;
                    obs.correctedFrequencyHz = 2300.0;
                    onlyDead.consume(&obs, 1U);
                }
            }
            QCOMPARE(onlyDead.finish(), SstvRadiofaxRxState::Partial);
            QCOMPARE(onlyDead.snapshot().completedPixels, std::size_t(0));
            SstvRadiofaxRxSession rx(cfg, mapper.imageStartSample());
            for (std::uint32_t line = 0; line < cfg.height; ++line) {
                const auto start = mapper.lineStartSample(line);
                const auto lineSamples = mapper.lineStartSample(line + 1U) - start;
                const auto dead = lineSamples * 45U / 1000U;
                for (std::uint64_t i = 0; i < lineSamples; ++i) {
                    SstvFrequencyObservation obs;
                    obs.status = SstvFrequencyStatus::Valid;
                    obs.confidence = 1.0;
                    obs.centreSample = start + i;
                    const auto x = i < dead ? 0U : (i - dead) * cfg.width / (lineSamples - dead);
                    obs.correctedFrequencyHz = i < dead ? (line == 0U ? 2300.0 : 1500.0)
                                                        : (x < 4U ? 1500.0 : 2300.0);
                    rx.consume(&obs, 1U);
                }
            }
            auto invalid = cfg;
            invalid.centeredPhasingWithWhiteEndLine = true;
            bool rejected = false;
            try { static_cast<void>(SstvRadiofaxMapper(invalid)); }
            catch (const std::invalid_argument&) { rejected = true; }
            QVERIFY(rejected);
            invalid = cfg;
            const auto shortestLine = cfg.sampleRate * 60U / rpm;
            invalid.width = shortestLine - shortestLine * 45U / 1000U + 1U;
            rejected = false;
            try { static_cast<void>(SstvRadiofaxMapper(invalid)); }
            catch (const std::invalid_argument&) { rejected = true; }
            QVERIFY(rejected);
            QCOMPARE(rx.finish(), SstvRadiofaxRxState::Complete);
            const auto image = rx.snapshot();
            for (std::uint32_t y = 0; y < cfg.height; ++y) {
                for (std::uint32_t x = 0; x < cfg.width; ++x)
                    QCOMPARE(image.pixels[y * cfg.width + x].red, std::uint8_t(x < 4U ? 0 : 255));
            }
        }
    }

    void measuredClockWithWmoDeadSectorAB()
    {
        SstvFaxConfig cfg;
        cfg.width = 128U;
        cfg.height = 120U;
        cfg.wmoDeadSector = true;
        for (const auto clock : {SstvRadiofaxClockEstimate {596'400U, 100U},
                                 SstvRadiofaxClockEstimate {603'600U, 100U}}) {
            SstvRadiofaxRxSession corrected(cfg, 0U, 0.0, clock);
            SstvRadiofaxRxSession nominal(cfg, 0U);
            std::vector<SstvFrequencyObservation> batch;
            const auto end = cfg.height * clock.samples / clock.lines;
            for (std::uint64_t sample = 0U; sample < end; sample += 6U) {
                const auto line = ((sample + 1U) * clock.lines - 1U) / clock.samples;
                const auto start = line * clock.samples / clock.lines;
                const auto next = (line + 1U) * clock.samples / clock.lines;
                const auto dead = sstvRadiofaxDeadSectorSamples(next - start);
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                if (sample - start < dead) {
                    obs.correctedFrequencyHz = 2300.0;
                } else {
                    const auto pixel = (sample - start - dead) * cfg.width / (next - start - dead);
                    obs.correctedFrequencyHz = ((pixel + line) % 2U) ? 2300.0 : 1500.0;
                }
                batch.push_back(obs);
                if (batch.size() == SstvRadiofaxRxSession::MaximumObservationsPerConsume) {
                    corrected.consume(batch);
                    nominal.consume(batch);
                    batch.clear();
                }
            }
            corrected.consume(batch);
            nominal.consume(batch);
            QCOMPARE(corrected.finish(), SstvRadiofaxRxState::Complete);
            nominal.finish();
            const auto actual = corrected.snapshot();
            const auto uncorrected = nominal.snapshot();
            QCOMPARE(actual.completedPixels, std::size_t(cfg.width * cfg.height));
            std::size_t wrongNominal = 0U;
            for (std::uint32_t y = 0; y < cfg.height; ++y) {
                for (std::uint32_t x = 0; x < cfg.width; ++x) {
                    const auto expected = std::uint8_t((x + y) % 2U ? 255U : 0U);
                    QCOMPARE(actual.pixel(x, y).red, expected);
                    if (uncorrected.pixel(x, y).red != expected) ++wrongNominal;
                }
            }
            QVERIFY(wrongNominal > cfg.width);
        }
    }

    void pcmLoopbackIsChunkInvariantAndPreservesRaster()
    {
        for (const auto ioc : {SstvFaxIoc::Ioc288, SstvFaxIoc::Ioc576}) {
            for (const auto rpm : {60U, 90U, 120U, 240U}) {
                for (const auto phase : {SstvFaxPhasePattern::Symmetric,
                                         SstvFaxPhasePattern::WhiteFivePercent}) {
                    SstvFaxConfig cfg;
                    cfg.ioc = ioc;
                    cfg.linesPerMinute = static_cast<std::uint16_t>(rpm);
                    cfg.phasing = phase;
                    cfg.width = 32U;
                    cfg.height = 4U;
                    std::vector<std::uint8_t> pixels(cfg.width * cfg.height);
                    for (std::uint32_t y = 0; y < cfg.height; ++y) {
                        for (std::uint32_t x = 0; x < cfg.width; ++x) {
                            pixels[y * cfg.width + x] = static_cast<std::uint8_t>((x * 7U + y * 43U) % 256U);
                        }
                    }
                    SstvRadiofaxEncoder a(cfg, pixels), b(cfg, pixels);
                    SstvRadiofaxRxSession receiverA(cfg, a.mapper().imageStartSample());
                    SstvRadiofaxRxSession receiverB(cfg, a.mapper().imageStartSample());
                    SstvRadiofaxAutoRx pipelineA(cfg), pipelineB(cfg);
                    SstvRadiofaxPhasingDetector phaseA(cfg.sampleRate);
                    SstvRadiofaxPhasingDetector phaseB(cfg.sampleRate);
                    std::unique_ptr<SstvRadiofaxImageStartDetector> startA;
                    std::unique_ptr<SstvRadiofaxImageStartDetector> startB;
                    std::optional<std::uint64_t> imageStartA;
                    std::optional<std::uint64_t> imageStartB;
                    std::unique_ptr<SstvRadiofaxRxSession> automaticReceiver;
                    std::vector<SstvFrequencyObservation> replay;
                    SstvFrequencyDemodulatorConfig demodConfig = SstvFrequencyDemodulatorConfig::sstvDefaults();
                    demodConfig.averagingSamples = 6U;
                    demodConfig.hopSamples = 3U;
                    SstvFrequencyDemodulator demod(demodConfig);
                    std::vector<std::int16_t> first(4096U), second(4096U);
                    std::vector<float> floating(4096U);
                    std::vector<double> sums(pixels.size(), 0.0);
                    std::vector<unsigned> counts(pixels.size(), 0U);
                    while (!a.complete()) {
                        const auto count = a.pullPcm16(first.data(), first.size());
                        std::size_t offset = 0U;
                        while (offset < count) {
                            const auto request = std::min<std::size_t>(127U, count - offset);
                            QCOMPARE(b.pullPcm16(second.data() + offset, request), request);
                            offset += request;
                        }
                        QVERIFY(std::equal(first.begin(), first.begin() + count, second.begin()));
                        for (std::size_t i = 0; i < count; ++i) floating[i] = first[i] / 32768.0F;
                        const auto observations = demod.consume(floating.data(), count);
                        pipelineA.consume(observations);
                        QVERIFY(pipelineA.bufferedObservations() <= pipelineA.maximumBufferedObservations());
                        const auto phaseLockA = phaseA.consume(observations);
                        if (phaseLockA && !startA) {
                            startA = std::make_unique<SstvRadiofaxImageStartDetector>(cfg.sampleRate, *phaseLockA);
                        }
                        if (startA && !automaticReceiver) {
                            replay.insert(replay.end(), observations.begin(), observations.end());
                            imageStartA = startA->consume(observations);
                            if (imageStartA) {
                                automaticReceiver = std::make_unique<SstvRadiofaxRxSession>(cfg, *imageStartA);
                                for (std::size_t i = 0; i < replay.size(); i += 8192U) {
                                    automaticReceiver->consume(replay.data() + i,
                                        std::min<std::size_t>(8192U, replay.size() - i));
                                }
                                replay.clear();
                            }
                        } else if (automaticReceiver) {
                            automaticReceiver->consume(observations);
                        }
                        receiverA.consume(observations);
                        for (std::size_t i = 0; i < observations.size(); i += 17U) {
                            const auto partSize = std::min<std::size_t>(17U, observations.size() - i);
                            pipelineB.consume(observations.data() + i, partSize);
                            const auto phaseLockB = phaseB.consume(observations.data() + i, partSize);
                            if (phaseLockB && !startB) {
                                startB = std::make_unique<SstvRadiofaxImageStartDetector>(cfg.sampleRate, *phaseLockB);
                            }
                            if (startB) imageStartB = startB->consume(observations.data() + i, partSize);
                            receiverB.consume(observations.data() + i,
                                              std::min<std::size_t>(17U, observations.size() - i));
                        }
                        for (const auto& observation : observations) {
                            if (!observation.valid()) continue;
                            const auto pos = a.mapper().positionAtSample(observation.centreSample);
                            if (pos.region != SstvFaxRegion::Image) continue;
                            const auto start = a.mapper().lineStartSample(pos.line);
                            const auto end = a.mapper().lineStartSample(pos.line + 1U);
                            const auto within = observation.centreSample - start;
                            const auto pixelPhase = (within * cfg.width) % (end - start);
                            if (pixelPhase < (end - start) / 3U || pixelPhase > (end - start) * 2U / 3U) continue;
                            const std::size_t index = pos.line * cfg.width + pos.pixel;
                            sums[index] += observation.correctedFrequencyHz;
                            ++counts[index];
                        }
                    }
                    QVERIFY(b.complete());
                    const auto pipelineStateA = pipelineA.finish();
                    const auto pipelineStateB = pipelineB.finish();
                    QVERIFY(pipelineStateA == SstvRadiofaxAutoRxState::Complete
                            || pipelineStateA == SstvRadiofaxAutoRxState::Partial);
                    QCOMPARE(pipelineStateA, pipelineStateB);
                    QVERIFY(pipelineA.phasing().has_value());
                    QVERIFY(pipelineA.imageStartSample().has_value());
                    QCOMPARE(pipelineA.imageStartSample(), pipelineB.imageStartSample());
                    QVERIFY(pipelineA.snapshot().has_value());
                    QVERIFY(pipelineB.snapshot().has_value());
                    QCOMPARE(pipelineA.snapshot()->pixels, pipelineB.snapshot()->pixels);
                    const auto lockA = phaseA.consume(nullptr, 0U);
                    const auto lockB = phaseB.consume(nullptr, 0U);
                    QVERIFY(lockA.has_value());
                    QVERIFY(lockB.has_value());
                    QCOMPARE(lockA->linesPerMinute, cfg.linesPerMinute);
                    QCOMPARE(lockA->pattern, cfg.phasing);
                    QCOMPARE(lockA->latestWhiteEdgeSample, lockB->latestWhiteEdgeSample);
                    QCOMPARE(a.mapper().positionAtSample(lockA->latestWhiteEdgeSample).region,
                             SstvFaxRegion::Phasing);
                    QVERIFY(imageStartA.has_value());
                    QVERIFY(imageStartB.has_value());
                    QCOMPARE(*imageStartA, *imageStartB);
                    const auto expectedStart = a.mapper().imageStartSample();
                    const auto errorSamples = *imageStartA > expectedStart
                        ? *imageStartA - expectedStart : expectedStart - *imageStartA;
                    QVERIFY2(errorSamples <= 20U, qPrintable(QStringLiteral("start error: %1 samples").arg(errorSamples)));
                    double error = 0.0;
                    for (std::size_t i = 0; i < pixels.size(); ++i) {
                        QVERIFY(counts[i] > 0U);
                        error += std::abs(static_cast<int>(SstvRadiofaxMapper::valueForFrequency(sums[i] / counts[i]))
                                          - static_cast<int>(pixels[i]));
                    }
                    QVERIFY2(error / pixels.size() < 8.0, qPrintable(QString::number(error / pixels.size())));
                    QCOMPARE(receiverA.finish(), SstvRadiofaxRxState::Complete);
                    QCOMPARE(receiverB.finish(), SstvRadiofaxRxState::Complete);
                    QVERIFY(automaticReceiver != nullptr);
                    const auto autoState = automaticReceiver->finish();
                    QVERIFY(autoState == SstvRadiofaxRxState::Complete
                            || autoState == SstvRadiofaxRxState::Partial);
                    const auto firstImage = receiverA.snapshot();
                    const auto secondImage = receiverB.snapshot();
                    const auto automaticImage = automaticReceiver->snapshot();
                    const auto pipelineImage = *pipelineA.snapshot();
                    QCOMPARE(firstImage.pixels, secondImage.pixels);
                    QVERIFY(!receiverA.takeDirtyEvents().empty());
                    double imageError = 0.0;
                    double automaticError = 0.0;
                    double pipelineError = 0.0;
                    for (std::size_t i = 0; i < pixels.size(); ++i) {
                        imageError += std::abs(static_cast<int>(firstImage.pixels[i].red)
                                               - static_cast<int>(pixels[i]));
                        QCOMPARE(firstImage.pixels[i].red, firstImage.pixels[i].green);
                        QCOMPARE(firstImage.pixels[i].green, firstImage.pixels[i].blue);
                        automaticError += std::abs(static_cast<int>(automaticImage.pixels[i].red)
                                                   - static_cast<int>(pixels[i]));
                        pipelineError += std::abs(static_cast<int>(pipelineImage.pixels[i].red)
                                                  - static_cast<int>(pixels[i]));
                    }
                    QVERIFY2(imageError / pixels.size() < 35.0,
                             qPrintable(QString::number(imageError / pixels.size())));
                    QVERIFY(automaticImage.coverage() > 0.98);
                    QVERIFY(pipelineImage.coverage() > 0.98);
                    QVERIFY2(pipelineError / pixels.size() < 35.0,
                             qPrintable(QString::number(pipelineError / pixels.size())));
                    QVERIFY2(automaticError / pixels.size() < 35.0,
                             qPrintable(QString::number(automaticError / pixels.size())));
                }
            }
        }
    }

    void stopDetectorRecognises450HzAndRejectsOtherPatterns()
    {
        constexpr std::uint32_t rate = 12'000U;
        for (const double frequency : {0.0, 300.0, 438.0, 439.0, 440.0, 440.5,
                                       447.3, 450.0, 452.7, 459.5, 460.0, 461.0, 462.0, 675.0}) {
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0; sample < 3U * rate; sample += 3U) {
                SstvFrequencyObservation obs;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                obs.centreSample = sample;
                obs.correctedFrequencyHz = frequency == 0.0 ? 1900.0
                    : (std::fmod(double(sample) * frequency * 2.0 / rate, 2.0) >= 1.0 ? 2300.0 : 1500.0);
                observations.push_back(obs);
            }
            SstvRadiofaxStopDetector a(rate), b(rate);
            std::optional<std::uint64_t> da, db;
            for (std::size_t i = 0; i < observations.size(); i += 193U) {
                const auto n = std::min(std::size_t {193U}, observations.size() - i);
                const auto result = a.consume(observations.data() + i, n);
                if (result) da = result;
            }
            for (const auto& obs : observations) {
                const auto result = b.consume(&obs, 1U);
                if (result) db = result;
            }
            QVERIFY2(da.has_value() == (frequency >= 440.0 && frequency <= 460.0),
                     qPrintable(QStringLiteral("frequency=%1 detection=%2")
                                    .arg(frequency).arg(da.has_value())));
            QCOMPARE(da, db);
        }

        // One-second windows at different frequencies must not count as a
        // sustained match in the same frequency bin.
        SstvRadiofaxStopDetector changing(rate);
        for (std::uint64_t sample = 0U; sample < 4U * rate; sample += 3U) {
            const double frequency = (sample / rate) % 2U == 0U ? 444.0 : 456.0;
            SstvFrequencyObservation obs;
            obs.status = SstvFrequencyStatus::Valid;
            obs.confidence = 1.0;
            obs.centreSample = sample;
            obs.correctedFrequencyHz = std::fmod(double(sample) * frequency * 2.0 / rate, 2.0) >= 1.0
                ? 2300.0 : 1500.0;
            QVERIFY(!changing.consume(&obs, 1U));
        }

        SstvFaxConfig config;
        config.width = 16U; config.height = 1U; config.linesPerMinute = 240U;
        SstvRadiofaxEncoder encoder(config, std::vector<std::uint8_t>(16U, 128U));
        SstvFrequencyDemodulatorConfig dc = SstvFrequencyDemodulatorConfig::sstvDefaults();
        dc.averagingSamples = 3U; dc.hopSamples = 1U;
        SstvFrequencyDemodulator demod(dc);
        SstvRadiofaxStopDetector detector(config.sampleRate);
        std::vector<std::int16_t> pcm(4096U);
        std::vector<float> input(4096U);
        std::optional<std::uint64_t> detected;
        while (!encoder.complete()) {
            const auto count = encoder.pullPcm16(pcm.data(), pcm.size());
            for (std::size_t i = 0; i < count; ++i) input[i] = float(pcm[i]) / 32768.0F;
            const auto observations = demod.consume(input.data(), count);
            const auto result = detector.consume(observations.data(), observations.size());
            if (result) { detected = result; break; }
        }
        QVERIFY2(detected.has_value(), "PCM stop signal was not detected");
        QVERIFY(*detected >= encoder.mapper().imageEndSample() - config.sampleRate);
        QVERIFY(*detected <= encoder.mapper().imageEndSample() + config.sampleRate);
    }

    void frequencyStopWithDropoutsAndNoiseAB()
    {
        constexpr std::uint32_t rate = 12'000U;
        for (const unsigned variant : {0U, 1U}) {
            const double frequency = variant == 0U ? 447.3 : 452.7;
            const double offset = variant == 0U ? -75.0 : 75.0;
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0U; sample <= 3U * rate; sample += 6U) {
                const auto i = sample / 6U;
                SstvFrequencyObservation obs;
                obs.centreSample = sample;
                obs.status = i % 5U == variant ? SstvFrequencyStatus::LowSignal : SstvFrequencyStatus::Valid;
                obs.confidence = obs.valid() ? 0.8 : 0.0;
                const auto high = std::fmod(double(sample) * frequency * 2.0 / rate, 2.0) >= 1.0;
                const double noise = double(int((i * 37U + variant * 11U) % 61U) - 30);
                obs.correctedFrequencyHz = (high ? 2300.0 : 1500.0) + offset + noise;
                observations.push_back(obs);
            }
            SstvRadiofaxStopDetector whole(rate, offset), fragmented(rate, offset), insufficient(rate, offset);
            SstvRadiofaxStopDetector impossibleConfidence(rate, offset);
            std::optional<std::uint64_t> a, b, negative, impossible;
            for (std::size_t i = 0U; i < observations.size(); i += 997U) {
                const auto n = std::min(std::size_t {997U}, observations.size() - i);
                if (auto found = whole.consume(observations.data() + i, n)) a = found;
            }
            for (std::size_t i = 0U; i < observations.size(); ++i) {
                if (auto found = fragmented.consume(&observations[i], 1U)) b = found;
                auto weak = observations[i];
                if (i % 2U == 0U) weak.confidence = 0.0;
                if (auto found = insufficient.consume(&weak, 1U)) negative = found;
                auto invalid = observations[i];
                invalid.confidence = 1.01;
                if (auto found = impossibleConfidence.consume(&invalid, 1U)) impossible = found;
            }
            QVERIFY(a);
            QCOMPARE(a, b);
            QVERIFY(!negative);
            QVERIFY(!impossible);
        }
    }

    void directAudioStopIsChunkInvariantAndRejectsOtherTones()
    {
        constexpr std::uint32_t rate = 12'000U;
        for (const unsigned frequency : {300U, 400U, 430U, 435U, 437U, 450U, 463U, 465U, 470U, 500U, 675U, 1500U, 2300U}) {
            for (const std::size_t prefix : {241U, 1427U}) {
                std::vector<float> pcm(prefix + 2U * rate, 0.0F);
                for (std::size_t i = prefix; i < pcm.size(); ++i)
                    pcm[i] = 0.25F * std::sin(2.0 * std::acos(-1.0)
                        * double(frequency) * double(i - prefix) / rate);
                SstvRadiofaxAudioStopDetector a(rate), b(rate);
                std::optional<std::uint64_t> foundA, foundB;
                for (std::size_t i = 0; i < pcm.size(); i += 193U) {
                    const auto n = std::min(std::size_t {193U}, pcm.size() - i);
                    if (auto value = a.consume(pcm.data() + i, n)) foundA = value;
                }
                for (float sample : pcm)
                    if (auto value = b.consume(&sample, 1U)) foundB = value;
                QCOMPARE(foundA.has_value(), frequency >= 430U && frequency <= 470U);
                QCOMPARE(foundA, foundB);
                if (foundA) {
                    QVERIFY(*foundA + rate / 10U >= prefix);
                    QVERIFY(*foundA <= prefix + rate / 10U);
                }
            }
        }
    }

    void directAudioStopRequiresPersistentFrequencyAndResetsWhenDisabled()
    {
        constexpr std::uint32_t rate = 12'000U;
        constexpr std::size_t window = rate / 10U;
        std::vector<float> samples(2U * rate);
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const double frequency = (i / window) % 2U ? 470.0 : 430.0;
            samples[i] = 0.25F * std::sin(2.0 * std::acos(-1.0)
                * frequency * double(i) / rate);
        }
        SstvRadiofaxAudioStopDetector detector(rate);
        QVERIFY(!detector.consume(samples.data(), samples.size()));
        for (std::size_t i = 0; i < samples.size(); ++i)
            samples[i] = 0.25F * std::sin(2.0 * std::acos(-1.0) * 437.0 * double(i) / rate);
        detector.reset();
        QVERIFY(!detector.consume(samples.data(), rate / 2U));
        QVERIFY(!detector.consume(samples.data(), 1U, false));
        QVERIFY(!detector.consume(samples.data(), rate / 2U));
        QVERIFY(detector.consume(samples.data() + rate / 2U, rate).has_value());
    }

    void directAudioStopIgnoresPreImageToneAcrossChunkSizes()
    {
        constexpr std::uint32_t rate = 12'000U;
        constexpr std::uint64_t imageStart = rate * 8U / 5U;
        constexpr std::uint64_t trueStop = rate * 2U;
        std::vector<float> samples(rate * 7U / 2U, 0.0F);
        for (std::uint64_t i = rate / 5U; i < rate * 7U / 5U; ++i)
            samples[i] = 0.25F * std::sin(2.0 * std::acos(-1.0) * 450.0 * double(i) / rate);
        for (std::uint64_t i = trueStop; i < samples.size(); ++i)
            samples[i] = 0.25F * std::sin(2.0 * std::acos(-1.0) * 450.0 * double(i) / rate);
        for (const std::size_t block : {193U, 16000U}) {
            SstvRadiofaxAudioStopDetector detector(rate);
            std::optional<std::uint64_t> detection;
            for (std::size_t i = 0; i < samples.size(); i += block) {
                const auto count = std::min(block, samples.size() - i);
                if (const auto result = detector.consume(samples.data() + i, count, true, imageStart))
                    detection = result;
            }
            QVERIFY(detection.has_value());
            QCOMPARE(*detection, trueStop);
        }
        // If the image boundary becomes known after partial tone windows,
        // those earlier windows must not contribute to the new detection.
        SstvRadiofaxAudioStopDetector detector(rate);
        std::vector<float> tone(rate * 2U);
        for (std::size_t i = 0; i < tone.size(); ++i)
            tone[i] = 0.25F * std::sin(2.0 * std::acos(-1.0) * 450.0 * double(i) / rate);
        QVERIFY(!detector.consume(tone.data(), rate / 2U));
        const auto updatedBoundary = detector.consume(tone.data() + rate / 2U,
                                                        rate, true, rate / 4U);
        QVERIFY(updatedBoundary.has_value());
        QCOMPARE(*updatedBoundary, std::uint64_t(rate / 2U));
    }

    void directAudioStopRearmsAfterDetectionWhenDisabled()
    {
        constexpr std::uint32_t rate = 12'000U;
        std::vector<float> tone(2U * rate), silence(rate / 2U, 0.0F);
        for (std::size_t i = 0; i < tone.size(); ++i)
            tone[i] = 0.25F * std::sin(2.0 * std::acos(-1.0) * 450.0 * double(i) / rate);
        for (const std::size_t chunk : {193U, 16000U}) {
            SstvRadiofaxAudioStopDetector detector(rate);
            auto feed = [&](const std::vector<float>& pcm, bool enabled) {
                std::optional<std::uint64_t> result;
                for (std::size_t i = 0; i < pcm.size(); i += chunk) {
                    const auto count = std::min(chunk, pcm.size() - i);
                    if (auto found = detector.consume(pcm.data() + i, count, enabled)) result = found;
                }
                return result;
            };
            const auto first = feed(tone, true);
            QVERIFY(first.has_value());
            QVERIFY(!feed(silence, false).has_value());
            const auto second = feed(tone, true);
            QVERIFY(second.has_value());
            QCOMPARE(*second, std::uint64_t(tone.size() + silence.size()));
        }
    }

    void centeredPhasingUsesExplicitRasterAlignment()
    {
        SstvFaxConfig config;
        config.width = 32U; config.height = 4U; config.linesPerMinute = 120U;
        const std::uint64_t lineSamples = config.sampleRate / 2U;
        for (unsigned variant = 0; variant < 2; ++variant) {
            auto alignedConfig = config;
            alignedConfig.centeredPhasingWithWhiteEndLine = true;
            SstvRadiofaxAutoRx normal(config), aligned(alignedConfig);
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0; sample < 15U * lineSamples; sample += 3U) {
                const auto line = sample / lineSamples;
                const auto within = sample % lineSamples;
                SstvFrequencyObservation obs;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                obs.centreSample = sample;
                if (line < 10U)
                    obs.correctedFrequencyHz = (within < lineSamples / 40U
                        || within >= lineSamples * 39U / 40U) ? 2300.0 : 1500.0;
                else if (line == 10U)
                    obs.correctedFrequencyHz = 2300.0;
                else
                    obs.correctedFrequencyHz = variant ? 1750.0 : 2050.0;
                observations.push_back(obs);
            }
            for (std::size_t i = 0; i < observations.size(); i += 193U) {
                const auto count = std::min(std::size_t {193U}, observations.size() - i);
                normal.consume(observations.data() + i, count);
                aligned.consume(observations.data() + i, count);
            }
            QVERIFY(normal.imageStartSample().has_value());
            QVERIFY(aligned.imageStartSample().has_value());
            QCOMPARE(*aligned.imageStartSample() - *normal.imageStartSample(),
                     lineSamples + lineSamples / 40U);
            QVERIFY(std::llabs(static_cast<long long>(*aligned.imageStartSample())
                - static_cast<long long>(11U * lineSamples)) <= 3LL);
        }
    }

    void autoReceiverStopsEarlyAtEndSignalWithoutClaimingFullHeight()
    {
        for (const std::uint16_t speed : {60U, 90U, 120U, 240U}) {
        SstvFaxConfig tx;
        tx.width = 32U; tx.height = 4U; tx.linesPerMinute = speed;
        SstvFaxConfig rx = tx; rx.height = 20U;
        std::vector<std::uint8_t> pixels(tx.width * tx.height);
        for (std::size_t i = 0; i < pixels.size(); ++i)
            pixels[i] = std::uint8_t(((i % tx.width) * 7U + (i / tx.width) * 43U) % 256U);
        SstvRadiofaxEncoder encoder(tx, pixels);
        SstvFrequencyDemodulatorConfig dc = SstvFrequencyDemodulatorConfig::sstvDefaults();
        dc.averagingSamples = 3U; dc.hopSamples = 1U;
        SstvFrequencyDemodulator demod(dc);
        SstvRadiofaxAutoRx a(rx), b(rx);
        std::vector<std::int16_t> pcm(4096U);
        std::vector<float> input(4096U);
        while (!encoder.complete() && !a.stopSample()) {
            const auto count = encoder.pullPcm16(pcm.data(), pcm.size());
            for (std::size_t i = 0; i < count; ++i) input[i] = float(pcm[i]) / 32768.0F;
            const auto obs = demod.consume(input.data(), count);
            a.consume(obs);
            for (std::size_t i = 0; i < obs.size(); i += 17U)
                b.consume(obs.data() + i, std::min(std::size_t {17U}, obs.size() - i));
        }
        QVERIFY(a.stopSample().has_value());
        QVERIFY(!a.stopWasAudioTone());
        QCOMPARE(a.stopWasAudioTone(), b.stopWasAudioTone());
        QCOMPARE(a.stopSample(), b.stopSample());
        QCOMPARE(a.state(), SstvRadiofaxAutoRxState::Partial);
        QCOMPARE(a.state(), b.state());
        const auto ia = a.snapshot(); const auto ib = b.snapshot();
        QVERIFY(ia && ib);
        QCOMPARE(ia->pixels, ib->pixels);
        QCOMPARE(ia->channelCoverage, ib->channelCoverage);
        std::size_t extraPixels = 0;
        for (std::size_t i = pixels.size(); i < ia->channelCoverage.size(); ++i)
            if (ia->channelCoverage[i] != 0U) ++extraPixels;
        QCOMPARE(extraPixels, std::size_t {0U});
        double error = 0;
        for (std::size_t i = 0; i < pixels.size(); ++i)
            error += std::abs(int(ia->pixels[i].red) - int(pixels[i]));
        QVERIFY(error / pixels.size() < 25.0);
        const auto delta = qint64(*a.stopSample()) - qint64(encoder.mapper().imageEndSample());
        QVERIFY2(std::abs(delta) < qint64(tx.sampleRate), qPrintable(QString::number(delta)));
        }
    }

    void phasingRejectsConstantOutOfBandAndIrregularSignals()
    {
        SstvRadiofaxPhasingDetector detector(12'000U, 75.0);
        std::vector<SstvFrequencyObservation> observations;
        for (std::uint64_t i = 0; i < 100; ++i) {
            SstvFrequencyObservation observation;
            observation.status = SstvFrequencyStatus::Valid;
            observation.centreSample = i * 1000U;
            observation.confidence = 1.0;
            observation.correctedFrequencyHz = 1975.0;
            observations.push_back(observation);
        }
        QVERIFY(!detector.consume(observations).has_value());
        detector.reset();
        for (std::size_t i = 0; i < observations.size(); ++i) {
            observations[i].correctedFrequencyHz = i % 2U ? 3000.0 : 500.0;
        }
        QVERIFY(!detector.consume(observations).has_value());
        detector.reset();
        for (std::size_t i = 0; i < observations.size(); ++i) {
            observations[i].correctedFrequencyHz = i % 2U ? 2375.0 : 1575.0;
        }
        QVERIFY(!detector.consume(observations).has_value());
        auto regression = observations.back();
        regression.centreSample--;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, detector.consume(&regression, 1U));
        detector.reset();
        QVERIFY(!detector.consume(nullptr, 0U).has_value());
    }

    void phasingAcceptsConfiguredFrequencyOffsetAndCanReset()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 1U;
        const SstvRadiofaxMapper mapper(config);
        SstvRadiofaxPhasingDetector detector(config.sampleRate, 75.0);
        std::optional<SstvRadiofaxPhaseLock> lock;
        for (std::uint64_t sample = 5U * config.sampleRate;
             sample < mapper.imageStartSample() && !lock; sample += 12U) {
            SstvFrequencyObservation observation;
            observation.status = SstvFrequencyStatus::Valid;
            observation.centreSample = sample;
            observation.confidence = 1.0;
            observation.correctedFrequencyHz =
                (mapper.positionAtSample(sample).white ? 2300.0 : 1500.0) + 75.0;
            lock = detector.consume(&observation, 1U);
        }
        QVERIFY(lock.has_value());
        QCOMPARE(lock->linesPerMinute, std::uint16_t {120U});
        QCOMPARE(lock->pattern, SstvFaxPhasePattern::WhiteFivePercent);
        detector.reset();
        QVERIFY(!detector.consume(nullptr, 0U).has_value());
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, detector.consume(nullptr, 1U));
        std::vector<SstvFrequencyObservation> tooMany(
            SstvRadiofaxPhasingDetector::MaximumObservationsPerConsume + 1U);
        QVERIFY_THROWS_EXCEPTION(std::length_error, detector.consume(tooMany));
    }

    void imageStartCandidateDoesNotInventAnAnchorAcrossGaps()
    {
        const SstvRadiofaxPhaseLock lock {120U, SstvFaxPhasePattern::WhiteFivePercent, 0U};
        SstvRadiofaxImageStartDetector detector(12'000U, lock);
        for (std::uint64_t sample = 0; sample < 24'000U; sample += 12U) {
            SstvFrequencyObservation obs;
            obs.status = SstvFrequencyStatus::Valid;
            obs.confidence = 1.0;
            obs.centreSample = sample;
            obs.correctedFrequencyHz = sample % 6000U < 300U ? 2300.0 : 1500.0;
            QVERIFY(!detector.consume(&obs, 1U).has_value());
        }
        QVERIFY(detector.valid());
        SstvFrequencyObservation gap;
        gap.status = SstvFrequencyStatus::Valid;
        gap.confidence = 1.0;
        gap.correctedFrequencyHz = 1500.0;
        gap.centreSample = 36'000U;
        QVERIFY(!detector.consume(&gap, 1U).has_value());
        QVERIFY(!detector.valid());
        gap.centreSample += 12U;
        QVERIFY(!detector.consume(&gap, 1U).has_value());
    }

    void automaticReceiverBoundsReplayAndClosesOnInterruption()
    {
        SstvFaxConfig config;
        config.width = 32U;
        config.height = 4U;
        SstvRadiofaxAutoRx pipeline(config);
        SstvFrequencyObservation obs;
        obs.status = SstvFrequencyStatus::Valid;
        obs.confidence = 1.0;
        for (std::uint64_t sample = 0; sample < 25'000U; sample += 12U) {
            obs.centreSample = sample;
            obs.correctedFrequencyHz = sample % 6000U < 300U ? 2300.0 : 1500.0;
            pipeline.consume(&obs, 1U);
        }
        QCOMPARE(pipeline.state(), SstvRadiofaxAutoRxState::SearchingImage);
        SstvRadiofaxAutoRx interrupted(config);
        interrupted.notifyDiscontinuity();
        QCOMPARE(interrupted.state(), SstvRadiofaxAutoRxState::Partial);
        QVERIFY(!interrupted.snapshot().has_value());
        SstvRadiofaxAutoRx cancelled(config);
        cancelled.cancel();
        QCOMPARE(cancelled.state(), SstvRadiofaxAutoRxState::Cancelled);
        obs.centreSample = 25'000U;
        std::vector<SstvFrequencyObservation> duplicates(8192U, obs);
        for (int i = 0; i < 6 && pipeline.state() != SstvRadiofaxAutoRxState::Failed; ++i) {
            pipeline.consume(duplicates);
            QVERIFY(pipeline.bufferedObservations() <= pipeline.maximumBufferedObservations());
        }
        QCOMPARE(pipeline.state(), SstvRadiofaxAutoRxState::Failed);
        QCOMPARE(pipeline.bufferedObservations(), std::size_t {0U});
    }

    void receiverKeepsMissingPixelsPartialAndRejectsRegressions()
    {
        SstvFaxConfig config;
        config.width = 16U;
        config.height = 2U;
        constexpr std::uint64_t anchor = 1'000U;
        SstvRadiofaxRxSession receiver(config, anchor);
        SstvFrequencyObservation observation;
        observation.status = SstvFrequencyStatus::Valid;
        observation.centreSample = anchor + 10U;
        observation.correctedFrequencyHz = 1900.0;
        observation.confidence = 1.0;
        QCOMPARE(receiver.consume(&observation, 1U), std::size_t {1U});
        auto regression = observation;
        regression.centreSample--;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, receiver.consume(&regression, 1U));
        QCOMPARE(receiver.finish(), SstvRadiofaxRxState::Partial);
        const auto snapshot = receiver.snapshot();
        QVERIFY(snapshot.coverage() > 0.0);
        QVERIFY(snapshot.coverage() < 1.0);
        QCOMPARE(snapshot.pixel(0U, 0U).red, std::uint8_t {128U});
        SstvRadiofaxRxSession interrupted(config, anchor);
        QCOMPARE(interrupted.consume(&observation, 1U), std::size_t {1U});
        QCOMPARE(interrupted.notifyDiscontinuity(), SstvRadiofaxRxState::Partial);
        QCOMPARE(interrupted.consume(&observation, 1U), std::size_t {0U});
        QVERIFY(interrupted.snapshot().coverage() > 0.0);
        SstvRadiofaxRxSession cancelled(config, anchor);
        cancelled.cancel();
        QCOMPARE(cancelled.state(), SstvRadiofaxRxState::Cancelled);
        QCOMPARE(cancelled.consume(&observation, 1U), std::size_t {0U});
        QVERIFY(cancelled.snapshot().cancelled);
    }

    void wmoControlSignalsAndAandBProfiles()
    {
        for (const auto ioc : {SstvFaxIoc::Ioc288, SstvFaxIoc::Ioc576}) {
            for (const auto rpm : {60U, 90U, 120U, 240U}) {
                for (const auto phase : {SstvFaxPhasePattern::Symmetric,
                                         SstvFaxPhasePattern::WhiteFivePercent}) {
                    SstvFaxConfig cfg;
                    cfg.ioc = ioc;
                    cfg.linesPerMinute = static_cast<std::uint16_t>(rpm);
                    cfg.sampleRate = 12'000U;
                    cfg.width = 256U;
                    cfg.height = 5U;
                    cfg.phasing = phase;
                    const SstvRadiofaxMapper fax(cfg);
                    QCOMPARE(fax.imageStartSample(), std::uint64_t {420'000});
                    QCOMPARE(fax.lineStartSample(5U),
                             420'000U + 5U * 12'000U * 60U / rpm);
                    QCOMPARE(fax.imageEndSample(), fax.lineStartSample(5U));
                    QCOMPARE(fax.totalSamples() - fax.imageEndSample(), std::uint64_t {180'000});
                    QCOMPARE(fax.positionAtSample(0).region, SstvFaxRegion::Start);
                    const auto startCycles = ioc == SstvFaxIoc::Ioc576 ? 300U : 675U;
                    unsigned changes = 0U;
                    bool previous = fax.positionAtSample(0).white;
                    for (unsigned sample = 1; sample < cfg.sampleRate; ++sample) {
                        const bool next = fax.positionAtSample(sample).white;
                        changes += next != previous;
                        previous = next;
                    }
                    QCOMPARE(changes, 2U * startCycles - 1U);
                    const auto phasingStart = std::uint64_t {60'000};
                    const auto lineSamples = std::uint64_t {720'000} / rpm;
                    QVERIFY(fax.positionAtSample(phasingStart).white);
                    QVERIFY(!fax.positionAtSample(phasingStart + lineSamples * 3U / 4U).white);
                    QCOMPARE(fax.positionAtSample(phasingStart + lineSamples / 4U).white,
                             phase == SstvFaxPhasePattern::Symmetric);
                    for (std::uint32_t line = 0; line < cfg.height; ++line) {
                        const auto first = fax.lineStartSample(line);
                        const auto end = fax.lineStartSample(line + 1U);
                        QCOMPARE(fax.positionAtSample(first).line, line);
                        QCOMPARE(fax.positionAtSample(first).pixel, 0U);
                        QCOMPARE(fax.positionAtSample(end - 1U).line, line);
                        QCOMPARE(fax.positionAtSample(end - 1U).pixel, cfg.width - 1U);
                    }
                    QCOMPARE(fax.positionAtSample(fax.imageEndSample()).region,
                             SstvFaxRegion::StopAlternating);
                    changes = 0U;
                    previous = fax.positionAtSample(fax.imageEndSample()).white;
                    for (unsigned sample = 1; sample < cfg.sampleRate; ++sample) {
                        const bool next = fax.positionAtSample(fax.imageEndSample() + sample).white;
                        changes += next != previous;
                        previous = next;
                    }
                    QCOMPARE(changes, 899U);
                    QCOMPARE(fax.positionAtSample(fax.imageEndSample() + 60'000U).region,
                             SstvFaxRegion::StopBlack);
                    QCOMPARE(fax.positionAtSample(fax.totalSamples()).region,
                             SstvFaxRegion::Complete);
                }
            }
        }
    }

    void cumulativeNinetyRpmNeverDriftsOrSkipsRows()
    {
        SstvFaxConfig cfg;
        cfg.linesPerMinute = 90U;
        cfg.sampleRate = 8'000U;
        cfg.width = 1'000U;
        cfg.height = 8'192U;
        const SstvRadiofaxMapper fax(cfg);
        for (std::uint32_t y = 0; y < cfg.height; ++y) {
            const auto start = fax.lineStartSample(y);
            QCOMPARE(start - fax.imageStartSample(),
                     static_cast<std::uint64_t>(y) * 480'000U / 90U);
            QCOMPARE(fax.positionAtSample(start).line, y);
            QCOMPARE(fax.positionAtSample(fax.lineStartSample(y + 1U) - 1U).line, y);
        }
        QVERIFY_THROWS_EXCEPTION(std::out_of_range, fax.lineStartSample(cfg.height + 1U));
    }

    void grayFrequencyMappingAndBounds()
    {
        QCOMPARE(SstvRadiofaxMapper::frequencyForValue(0U), 1500.0);
        QCOMPARE(SstvRadiofaxMapper::frequencyForValue(255U), 2300.0);
        for (unsigned v = 0; v <= 255; ++v) {
            QCOMPARE(SstvRadiofaxMapper::valueForFrequency(
                         SstvRadiofaxMapper::frequencyForValue(static_cast<std::uint8_t>(v))),
                     static_cast<std::uint8_t>(v));
        }
        SstvFaxConfig cfg;
        cfg.linesPerMinute = 123U;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvRadiofaxMapper {cfg});
        cfg.linesPerMinute = 240U;
        cfg.sampleRate = 8'000U;
        cfg.width = 4'096U;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvRadiofaxMapper {cfg});
    }
};
QTEST_GUILESS_MAIN(TestSstvRadiofax)
#include "test_sstv_radiofax.moc"
