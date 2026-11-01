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
                    QCOMPARE(pipelineImage.pixels, automaticImage.pixels);
                    QCOMPARE(firstImage.pixels, secondImage.pixels);
                    QVERIFY(!receiverA.takeDirtyEvents().empty());
                    double imageError = 0.0;
                    double automaticError = 0.0;
                    for (std::size_t i = 0; i < pixels.size(); ++i) {
                        imageError += std::abs(static_cast<int>(firstImage.pixels[i].red)
                                               - static_cast<int>(pixels[i]));
                        QCOMPARE(firstImage.pixels[i].red, firstImage.pixels[i].green);
                        QCOMPARE(firstImage.pixels[i].green, firstImage.pixels[i].blue);
                        automaticError += std::abs(static_cast<int>(automaticImage.pixels[i].red)
                                                   - static_cast<int>(pixels[i]));
                    }
                    QVERIFY2(imageError / pixels.size() < 35.0,
                             qPrintable(QString::number(imageError / pixels.size())));
                    QVERIFY(automaticImage.coverage() > 0.98);
                    QVERIFY2(automaticError / pixels.size() < 35.0,
                             qPrintable(QString::number(automaticError / pixels.size())));
                }
            }
        }
    }

    void stopDetectorRecognises450HzAndRejectsOtherPatterns()
    {
        constexpr std::uint32_t rate = 12'000U;
        for (const unsigned frequency : {0U, 300U, 450U, 675U}) {
            std::vector<SstvFrequencyObservation> observations;
            for (std::uint64_t sample = 0; sample < 3U * rate; sample += 3U) {
                SstvFrequencyObservation obs;
                obs.status = SstvFrequencyStatus::Valid;
                obs.confidence = 1.0;
                obs.centreSample = sample;
                obs.correctedFrequencyHz = frequency == 0U ? 1900.0
                    : ((sample * frequency * 2U / rate) % 2U ? 2300.0 : 1500.0);
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
            QCOMPARE(da.has_value(), frequency == 450U);
            QCOMPARE(da, db);
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
