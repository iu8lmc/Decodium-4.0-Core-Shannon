// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "src/sstv/rx/SstvRxControlPolicy.h"
#include "src/sstv/rx/SstvRxCorrectionController.h"
#include "src/sstv/rx/SstvRxRetainedAudio.h"
#include "src/sstv/rx/SstvTimingFallbackDetector.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace decodium::sstv;
using namespace std::chrono_literals;

SstvModeSpec fallbackMode(std::string id,
                          std::int64_t linePicoseconds,
                          std::int64_t syncPicoseconds)
{
    SstvModeSpec mode;
    mode.id = std::move(id);
    mode.longName = mode.id + " long";
    mode.shortName = mode.id;
    mode.family = "test";
    mode.classification = ModeClassification::AnalogSstv;
    mode.rxStatus = CapabilityStatus::Implemented;
    mode.fallbackSignature.nominalLineDuration
        = Picoseconds {linePicoseconds};
    mode.fallbackSignature.nominalSyncDuration
        = Picoseconds {syncPicoseconds};
    mode.fallbackSignature.syncFrequencyHz = 1'200U;
    mode.fallbackSignature.discriminator = "test timing";
    mode.timing.tolerancePpm = 3'000U;
    return mode;
}

SstvAudioChunk audioChunk(std::uint64_t sequence,
                          std::chrono::nanoseconds start,
                          std::size_t count,
                          float value)
{
    SstvAudioChunk chunk;
    chunk.source = {SstvAudioSourceKind::LocalSoundCard, 7U};
    chunk.sampleRate = 12'000U;
    chunk.startTime = start;
    chunk.sequence = sequence;
    chunk.samples.assign(count, value);
    return chunk;
}

} // namespace

class TestSstvRxControls final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void policyRejectsUnboundedAndInvalidControls()
    {
        SstvRxControlPolicy policy;
        const auto initial = policy.snapshot();
        QCOMPARE(initial.settings.replayRetentionSeconds, 180U);
        QCOMPARE(initial.settings.afcMode, SstvRxAfcMode::Automatic);
        QCOMPARE(initial.settings.slantMode, SstvRxSlantMode::Automatic);

        QVERIFY(!policy.setModeControl(SstvRxModeControl::Manual, {}));
        QVERIFY(!policy.setModeControl(SstvRxModeControl::Manual,
                                       std::string(65U, 'x')));
        QVERIFY(policy.setModeControl(SstvRxModeControl::Manual,
                                      "martin-m1"));
        QVERIFY(!policy.setModeLock(true, "bad mode"));
        QVERIFY(policy.setModeLock(true, "martin-m1"));
        QVERIFY(!policy.setAfc(SstvRxAfcMode::Manual, 150.01));
        QVERIFY(!policy.setAfc(SstvRxAfcMode::Manual,
                              std::numeric_limits<double>::quiet_NaN()));
        QVERIFY(policy.setAfc(SstvRxAfcMode::Manual, -100.0));
        QVERIFY(!policy.setSlant(SstvRxSlantMode::Manual, 5'000.1));
        QVERIFY(policy.setSlant(SstvRxSlantMode::Manual, 300.0));
        QVERIFY(!policy.setReplayRetentionSeconds(4U));
        QVERIFY(!policy.setReplayRetentionSeconds(601U));
        QVERIFY(policy.setReplayRetentionSeconds(600U));

        SstvRxRedecodeParameters redecode;
        redecode.mode = "scottie-s1";
        redecode.afcMode = SstvRxAfcMode::Manual;
        redecode.frequencyCorrectionHz = 100.0;
        redecode.slantMode = SstvRxSlantMode::Manual;
        redecode.clockErrorPpm = -300.0;
        QVERIFY(policy.requestRedecode(redecode));
        policy.requestAfcReset();
        policy.requestSlantReset();
        const auto snapshot = policy.snapshot();
        QVERIFY(snapshot.revision > initial.revision);
        QCOMPARE(snapshot.afcResetSerial, std::uint64_t {1U});
        QCOMPARE(snapshot.slantResetSerial, std::uint64_t {1U});
        QCOMPARE(snapshot.redecodeSerial, std::uint64_t {1U});
        QVERIFY(snapshot.redecode.has_value());
        QCOMPARE(snapshot.redecode->mode, std::string("scottie-s1"));
    }

    void policyMailboxIsRaceFreeAndBounded()
    {
        SstvRxControlPolicy policy;
        std::atomic_bool start {false};
        std::vector<std::thread> writers;
        for (int worker = 0; worker < 4; ++worker) {
            writers.emplace_back([&policy, &start, worker] {
                while (!start.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                for (int index = 0; index < 2'000; ++index) {
                    const double correction = ((index + worker) % 2 == 0)
                        ? 100.0 : -100.0;
                    QVERIFY(policy.setAfc(SstvRxAfcMode::Manual,
                                          correction));
                    QVERIFY(policy.setReplayRetentionSeconds(
                        static_cast<std::uint32_t>(5 + index % 596)));
                    if ((index % 41) == 0) {
                        policy.requestAfcReset();
                    }
                    const auto snapshot = policy.snapshot();
                    QVERIFY(SstvRxControlPolicy::settingsAreValid(
                        snapshot.settings));
                }
            });
        }
        start.store(true, std::memory_order_release);
        for (auto& writer : writers) {
            writer.join();
        }
        QVERIFY(SstvRxControlPolicy::settingsAreValid(
            policy.snapshot().settings));
    }

    void afcAcquiresPlusAndMinusOneHundredHz()
    {
        for (const double offset : {-100.0, 100.0}) {
            SstvAfcController controller;
            controller.configure(SstvRxAfcMode::Automatic, 0.0);
            const auto update = controller.consume(SstvAfcEvidence {
                SstvAfcEvidenceRole::Leader,
                1U,
                1'000U,
                1'900.0 + offset,
                1'900.0,
                0.95,
                true});
            QCOMPARE(update.status, SstvAfcUpdateStatus::Accepted);
            QVERIFY(update.correctionChanged);
            QVERIFY(std::abs(update.snapshot.correctionHz - offset)
                    < 1.0e-9);
            QVERIFY(update.snapshot.confidence >= 0.95);
        }
    }

    void afcNeverChasesImageLuminance()
    {
        SstvAfcController controller;
        controller.configure(SstvRxAfcMode::Automatic, 0.0);
        QVERIFY(controller.consume(SstvAfcEvidence {
            SstvAfcEvidenceRole::Leader, 1U, 1'000U,
            1'980.0, 1'900.0, 0.95, true}).correctionChanged);
        const double acquired = controller.snapshot().correctionHz;
        for (std::uint64_t index = 2U; index < 200U; ++index) {
            const auto rejected = controller.consume(SstvAfcEvidence {
                SstvAfcEvidenceRole::ImageData,
                index,
                1'000U + index * 6U,
                (index % 2U) == 0U ? 1'500.0 : 2'300.0,
                1'900.0,
                1.0,
                true});
            QCOMPARE(rejected.status,
                     SstvAfcUpdateStatus::LuminanceRejected);
        }
        QCOMPARE(controller.snapshot().correctionHz, acquired);
        QCOMPARE(controller.snapshot().rejectedImageObservations,
                 std::uint64_t {198U});
    }

    void afcManualOffAndTrustedSyncAreExplicit()
    {
        SstvAfcController controller;
        controller.configure(SstvRxAfcMode::Manual, -77.0);
        QCOMPARE(controller.snapshot().correctionHz, -77.0);
        QCOMPARE(controller.consume(SstvAfcEvidence {
                     SstvAfcEvidenceRole::TrustedLineSync, 1U, 100U,
                     1'300.0, 1'200.0, 1.0, true}).status,
                 SstvAfcUpdateStatus::ManualMode);
        QCOMPARE(controller.snapshot().correctionHz, -77.0);

        controller.configure(SstvRxAfcMode::Off, 0.0);
        QCOMPARE(controller.snapshot().correctionHz, 0.0);
        QCOMPARE(controller.consume(SstvAfcEvidence {
                     SstvAfcEvidenceRole::Leader, 2U, 200U,
                     2'000.0, 1'900.0, 1.0, true}).status,
                 SstvAfcUpdateStatus::Disabled);

        controller.configure(SstvRxAfcMode::Automatic, 0.0);
        QCOMPARE(controller.consume(SstvAfcEvidence {
                     SstvAfcEvidenceRole::TrustedLineSync, 3U, 300U,
                     1'300.0, 1'200.0, 1.0, false}).status,
                 SstvAfcUpdateStatus::UntrustedReference);
        QCOMPARE(controller.consume(SstvAfcEvidence {
                     SstvAfcEvidenceRole::TrustedLineSync, 4U, 400U,
                     1'300.0, 1'200.0, 1.0, true}).status,
                 SstvAfcUpdateStatus::Accepted);
        QCOMPARE(controller.snapshot().correctionHz, 100.0);
    }

    void automaticSlantMeasuresAndAppliesPlusMinusThreeHundredPpm()
    {
        constexpr std::uint64_t nominalPeriod = 12'000U;
        for (const double ppm : {-300.0, 300.0}) {
            SstvSlantController controller;
            controller.configure(nominalPeriod,
                                 SstvRxSlantMode::Automatic,
                                 0.0);
            const double actualPeriod = static_cast<double>(nominalPeriod)
                * (1.0 + ppm / 1'000'000.0);
            for (std::uint64_t line = 0U; line < 32U; ++line) {
                const auto start = static_cast<std::uint64_t>(std::llround(
                    500.0 + static_cast<double>(line) * actualPeriod));
                controller.observe(SstvSlantObservation {
                    line, start, 0.98, false});
            }
            const auto snapshot = controller.snapshot();
            QVERIFY(snapshot.estimateValid);
            QVERIFY(std::abs(snapshot.measuredClockErrorPpm - ppm) < 15.0);
            QCOMPARE(snapshot.appliedClockErrorPpm,
                     snapshot.measuredClockErrorPpm);
            QVERIFY(snapshot.confidence > 0.80);
        }
    }

    void manualSlantIsAppliedWithoutDestroyingTheEstimate()
    {
        SstvSlantController controller;
        controller.configure(12'000U, SstvRxSlantMode::Manual, -275.0);
        for (std::uint64_t line = 0U; line < 8U; ++line) {
            controller.observe(SstvSlantObservation {
                line, 100U + line * 12'000U, 0.95, false});
        }
        const auto snapshot = controller.snapshot();
        QVERIFY(snapshot.estimateValid);
        QVERIFY(std::abs(snapshot.measuredClockErrorPpm) < 1.0);
        QCOMPARE(snapshot.appliedClockErrorPpm, -275.0);
    }

    void fallbackSelectsUniqueModeAndRecoversInitialPulses()
    {
        const SstvModeRegistry registry({
            fallbackMode("mode-a", 500'000'000'000LL, 10'000'000'000LL),
            fallbackMode("mode-b", 550'000'000'000LL, 10'000'000'000LL),
        });
        SstvTimingFallbackDetector detector(registry);
        SstvFallbackResult result;
        for (std::uint64_t line = 0U; line < 6U; ++line) {
            result = detector.consume(SstvFallbackSyncPulse {
                1'000U + line * 6'002U,
                1'120U + line * 6'002U,
                1'203.0,
                0.95,
                false});
        }
        QCOMPARE(result.status, SstvFallbackStatus::Unique);
        QVERIFY(result.selectedMode.has_value());
        QCOMPARE(*result.selectedMode, std::string("mode-a"));
        QCOMPARE(result.retainedPulses.size(), std::size_t {6U});
        QVERIFY(std::abs(result.observedLinePeriodSamples - 6'002.0)
                < 1.0e-9);
    }

    void fallbackConflictsFailClosedAndLockCanDisambiguate()
    {
        const SstvModeRegistry registry({
            fallbackMode("mode-a", 500'000'000'000LL, 10'000'000'000LL),
            fallbackMode("mode-c", 500'000'000'000LL, 10'000'000'000LL),
        });
        SstvTimingFallbackDetector detector(registry);
        SstvFallbackResult result;
        for (std::uint64_t line = 0U; line < 4U; ++line) {
            result = detector.consume(SstvFallbackSyncPulse {
                100U + line * 6'000U,
                220U + line * 6'000U,
                1'200.0,
                0.95,
                false});
        }
        QCOMPARE(result.status, SstvFallbackStatus::Ambiguous);
        QVERIFY(!result.selectedMode.has_value());
        QCOMPARE(result.candidates.size(), std::size_t {2U});

        detector.reset();
        QVERIFY(detector.setLockedMode(std::string("mode-c")));
        for (std::uint64_t line = 0U; line < 4U; ++line) {
            result = detector.consume(SstvFallbackSyncPulse {
                100U + line * 6'000U,
                220U + line * 6'000U,
                1'200.0,
                0.95,
                false});
        }
        QCOMPARE(result.status, SstvFallbackStatus::Unique);
        QCOMPARE(*result.selectedMode, std::string("mode-c"));
    }

    void retainedAudioIsHardBoundedAndSnapshotsOneAcquisition()
    {
        SstvRxRetainedAudio retained(SstvRxRetainedAudio::Config {
            12'000U, 5U, 4U});
        const SstvAudioSource source {
            SstvAudioSourceKind::LocalSoundCard, 7U};
        QVERIFY(retained.append(audioChunk(1U, 0s, 36'000U, 0.1F)));
        QVERIFY(retained.beginAcquisition(42U, source, 0s));
        QVERIFY(retained.append(audioChunk(2U, 3s, 36'000U, 0.2F)));
        QVERIFY(retained.closeAcquisition(42U, 5s, true,
                                          "martin-m1", "IU8LMC",
                                          100.0, 300.0));

        QCOMPARE(retained.capacitySamples(), std::size_t {60'000U});
        QCOMPARE(retained.retainedSamples(), std::size_t {60'000U});
        const auto snapshot = retained.snapshotAcquisition(42U);
        QVERIFY(snapshot.has_value());
        QCOMPARE(snapshot->acquisitionId, std::uint64_t {42U});
        QCOMPARE(snapshot->mode, std::string("martin-m1"));
        QCOMPARE(snapshot->fskId, std::string("IU8LMC"));
        QCOMPARE(snapshot->frequencyCorrectionHz, 100.0);
        QCOMPARE(snapshot->slantCorrectionPpm, 300.0);
        QVERIFY(snapshot->sampleCount <= retained.capacitySamples());
        QVERIFY(snapshot->truncatedAtStart);
        QVERIFY(snapshot->acquisitionClosed);
        QVERIFY(snapshot->acquisitionComplete);
        QVERIFY(!snapshot->chunks.empty());
    }

    void retainedAudioRejectsBadMetadataAndDescriptorOverflow()
    {
        SstvRxRetainedAudio retained(SstvRxRetainedAudio::Config {
            12'000U, 5U, 2U});
        const SstvAudioSource source {SstvAudioSourceKind::Replay, 1U};
        QVERIFY(!retained.beginAcquisition(0U, source, 0ns));
        QVERIFY(retained.beginAcquisition(1U, source, 0ns));
        QVERIFY(retained.beginAcquisition(2U, source, 1s));
        QVERIFY(!retained.beginAcquisition(3U, source, 2s));
        QVERIFY(!retained.closeAcquisition(
            1U, 3s, false, std::string(65U, 'x'), {}, 0.0, 0.0));
        QVERIFY(retained.closeAcquisition(
            1U, 3s, false, "martin-m1", {}, 0.0, 0.0));
        QVERIFY(retained.beginAcquisition(3U, source, 4s));
        QCOMPARE(retained.acquisitions().size(), std::size_t {2U});
    }
};

QTEST_APPLESS_MAIN(TestSstvRxControls)

#include "test_sstv_rx_controls.moc"
