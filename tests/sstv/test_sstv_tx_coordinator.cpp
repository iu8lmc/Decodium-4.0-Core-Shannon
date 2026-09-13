// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../src/sstv/integration/SstvTxCoordinator.h"
#include "../../src/sstv/integration/SstvTxSources.h"
#include "../../src/sstv/diagnostics/SstvDiagnosticLogging.h"
#include "../../src/sstv/tx/SstvToneGenerator.h"

#include <QtTest/QtTest>

#include <QImage>
#include <QJsonDocument>
#include <QSet>

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace decodium::sstv;

namespace {

struct FakeHooks final
{
    SstvTxCoordinatorPreflight preflight {
        true, true, false, false, true, {}};
    bool pttOnResult {true};
    bool pttOffResult {true};
    bool audioStartResult {true};
    bool audioDetachResult {true};
    bool throwPreflight {false};
    bool throwPttOn {false};
    bool throwAudioStart {false};
    bool throwObserver {false};
    std::uint64_t pttOnCalls {0U};
    std::uint64_t pttOffCalls {0U};
    std::uint64_t audioStartCalls {0U};
    std::uint64_t audioDetachCalls {0U};
    std::uint64_t preflightCalls {0U};
    std::uint64_t lastSessionId {0U};
    SstvTxAudioDetachReason lastDetachReason {
        SstvTxAudioDetachReason::Completed};
    SstvTxAudioPlan lastAudioPlan;
    std::weak_ptr<SstvTxAudioDevice> audioDevice;
    std::vector<std::string> callOrder;
    std::vector<SstvTxCoordinatorSnapshot> observations;

    SstvTxCoordinatorHooks make()
    {
        SstvTxCoordinatorHooks hooks;
        hooks.queryPreflight = [this] {
            ++preflightCalls;
            callOrder.emplace_back("preflight");
            if (throwPreflight) {
                throw std::runtime_error("injected preflight failure");
            }
            return preflight;
        };
        hooks.requestPttOn = [this](std::uint64_t sessionId) {
            ++pttOnCalls;
            lastSessionId = sessionId;
            callOrder.emplace_back("ptt-on");
            if (throwPttOn) {
                throw std::runtime_error("injected PTT-on failure");
            }
            return pttOnResult;
        };
        hooks.requestPttOff = [this](std::uint64_t sessionId) {
            ++pttOffCalls;
            lastSessionId = sessionId;
            callOrder.emplace_back("ptt-off");
            return pttOffResult;
        };
        hooks.startAudio = [this](
                               const std::shared_ptr<SstvTxAudioDevice>& device,
                               const SstvTxAudioPlan& plan) {
            ++audioStartCalls;
            audioDevice = device;
            lastAudioPlan = plan;
            callOrder.emplace_back("audio-start");
            if (throwAudioStart) {
                throw std::runtime_error("injected audio start failure");
            }
            return audioStartResult;
        };
        hooks.detachAudio = [this](
                                const std::shared_ptr<SstvTxAudioDevice>&,
                                SstvTxAudioDetachReason reason) {
            ++audioDetachCalls;
            lastDetachReason = reason;
            callOrder.emplace_back("audio-detach");
            return audioDetachResult;
        };
        hooks.stateChanged = [this](
                                 const SstvTxCoordinatorSnapshot& snapshot) {
            observations.push_back(snapshot);
            if (throwObserver) {
                throw std::runtime_error("injected observer failure");
            }
        };
        return hooks;
    }
};

SstvTxCoordinatorConfig fastConfig()
{
    SstvTxCoordinatorConfig config;
    config.pttLeadDelayMs = 10U;
    config.pttTailDelayMs = 20U;
    config.pttReleaseRetryMs = 25U;
    config.voxPreKeyMs = 5U;
    config.voxHangMs = 7U;
    config.stateMachinePolicy.timeouts.requestingPttMs = 100U;
    config.stateMachinePolicy.timeouts.waitingForPttMs = 100U;
    config.stateMachinePolicy.timeouts.transmittingHeaderMs = 1'000U;
    config.stateMachinePolicy.timeouts.transmittingImageSlackMs = 1'000U;
    config.stateMachinePolicy.timeouts.transmittingFskIdMs = 5'000U;
    config.stateMachinePolicy.timeouts.tailDelayMs = 100U;
    config.stateMachinePolicy.timeouts.releasingPttMs = 100U;
    return config;
}

SstvTxCoordinatorRequest requestFor(SstvTxCoordinatorMode mode)
{
    SstvTxCoordinatorRequest request;
    request.mode = mode;
    std::size_t pixelCount = 0U;
    switch (mode) {
    case SstvTxCoordinatorMode::MartinM1:
        pixelCount = SstvMartinM1Encoder::pixelCount(SstvMartinMode::M1);
        break;
    case SstvTxCoordinatorMode::MartinM2:
        pixelCount = SstvMartinM1Encoder::pixelCount(SstvMartinMode::M2);
        break;
    case SstvTxCoordinatorMode::MartinM3:
        pixelCount = SstvMartinM1Encoder::pixelCount(SstvMartinMode::M3);
        break;
    case SstvTxCoordinatorMode::MartinM4:
        pixelCount = SstvMartinM1Encoder::pixelCount(SstvMartinMode::M4);
        break;
    case SstvTxCoordinatorMode::ScottieS3:
        pixelCount = SstvScottieEncoder::pixelCount(SstvScottieMode::S3);
        break;
    case SstvTxCoordinatorMode::ScottieS4:
        pixelCount = SstvScottieEncoder::pixelCount(SstvScottieMode::S4);
        break;
    case SstvTxCoordinatorMode::ScottieS1:
    case SstvTxCoordinatorMode::ScottieS2:
    case SstvTxCoordinatorMode::ScottieDx:
        pixelCount = SstvMartinM1Encoder::PixelCount;
        break;
    case SstvTxCoordinatorMode::RobotColour12:
        pixelCount = SstvRobotEncoder::pixelCount(SstvRobotMode::Colour12);
        break;
    case SstvTxCoordinatorMode::RobotColour24:
        pixelCount = SstvRobotEncoder::pixelCount(SstvRobotMode::Colour24);
        break;
    case SstvTxCoordinatorMode::RobotColour36:
        pixelCount = SstvRobotEncoder::pixelCount(SstvRobotMode::Colour36);
        break;
    case SstvTxCoordinatorMode::RobotColour72:
        pixelCount = SstvRobotEncoder::pixelCount(SstvRobotMode::Colour72);
        break;
    case SstvTxCoordinatorMode::RobotBw8:
        pixelCount = SstvRobotEncoder::pixelCount(SstvRobotMode::Bw8);
        break;
    case SstvTxCoordinatorMode::RobotBw12:
        pixelCount = SstvRobotEncoder::pixelCount(SstvRobotMode::Bw12);
        break;
    case SstvTxCoordinatorMode::RobotBw24:
        pixelCount = SstvRobotEncoder::pixelCount(SstvRobotMode::Bw24);
        break;
    case SstvTxCoordinatorMode::RobotBw36:
        pixelCount = SstvRobotEncoder::pixelCount(SstvRobotMode::Bw36);
        break;
    case SstvTxCoordinatorMode::WraaseSc2_60:
        pixelCount = SstvSequentialRgbEncoder::pixelCount(
            SstvSequentialRgbMode::WraaseSc2_60);
        break;
    case SstvTxCoordinatorMode::WraaseSc2_120:
        pixelCount = SstvSequentialRgbEncoder::pixelCount(
            SstvSequentialRgbMode::WraaseSc2_120);
        break;
    case SstvTxCoordinatorMode::WraaseSc2_180:
        pixelCount = SstvSequentialRgbEncoder::pixelCount(
            SstvSequentialRgbMode::WraaseSc2_180);
        break;
    case SstvTxCoordinatorMode::PasokonP3:
        pixelCount = SstvSequentialRgbEncoder::pixelCount(
            SstvSequentialRgbMode::PasokonP3);
        break;
    case SstvTxCoordinatorMode::PasokonP5:
        pixelCount = SstvSequentialRgbEncoder::pixelCount(
            SstvSequentialRgbMode::PasokonP5);
        break;
    case SstvTxCoordinatorMode::PasokonP7:
        pixelCount = SstvSequentialRgbEncoder::pixelCount(
            SstvSequentialRgbMode::PasokonP7);
        break;
    case SstvTxCoordinatorMode::Pd50:
        pixelCount = SstvPdEncoder::pixelCount(SstvPdMode::Pd50);
        break;
    case SstvTxCoordinatorMode::Pd90:
        pixelCount = SstvPdEncoder::pixelCount(SstvPdMode::Pd90);
        break;
    case SstvTxCoordinatorMode::Pd120:
        pixelCount = SstvPdEncoder::pixelCount(SstvPdMode::Pd120);
        break;
    case SstvTxCoordinatorMode::Pd160:
        pixelCount = SstvPdEncoder::pixelCount(SstvPdMode::Pd160);
        break;
    case SstvTxCoordinatorMode::Pd180:
        pixelCount = SstvPdEncoder::pixelCount(SstvPdMode::Pd180);
        break;
    case SstvTxCoordinatorMode::Pd240:
        pixelCount = SstvPdEncoder::pixelCount(SstvPdMode::Pd240);
        break;
    case SstvTxCoordinatorMode::Pd290:
        pixelCount = SstvPdEncoder::pixelCount(SstvPdMode::Pd290);
        break;
    case SstvTxCoordinatorMode::Avt24:
        pixelCount = SstvAvtEncoder::pixelCount(SstvAvtMode::Avt24);
        break;
    case SstvTxCoordinatorMode::Avt90:
        pixelCount = SstvAvtEncoder::pixelCount(SstvAvtMode::Avt90);
        break;
    case SstvTxCoordinatorMode::Avt94:
        pixelCount = SstvAvtEncoder::pixelCount(SstvAvtMode::Avt94);
        break;
    case SstvTxCoordinatorMode::Mp73:
        pixelCount = SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Mp73);
        break;
    case SstvTxCoordinatorMode::Mp115:
        pixelCount = SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Mp115);
        break;
    case SstvTxCoordinatorMode::Mp140:
        pixelCount = SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Mp140);
        break;
    case SstvTxCoordinatorMode::Mp175:
        pixelCount = SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Mp175);
        break;
    case SstvTxCoordinatorMode::Mr73:
        pixelCount = SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Mr73);
        break;
    case SstvTxCoordinatorMode::Mr90:
        pixelCount = SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Mr90);
        break;
    case SstvTxCoordinatorMode::Mr115:
        pixelCount = SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Mr115);
        break;
    case SstvTxCoordinatorMode::Mr140:
        pixelCount = SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Mr140);
        break;
    case SstvTxCoordinatorMode::Mr175:
        pixelCount = SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Mr175);
        break;
    case SstvTxCoordinatorMode::Ml180:
        pixelCount = SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Ml180);
        break;
    case SstvTxCoordinatorMode::Ml240:
        pixelCount = SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Ml240);
        break;
    case SstvTxCoordinatorMode::Ml280:
        pixelCount = SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Ml280);
        break;
    case SstvTxCoordinatorMode::Ml320:
        pixelCount = SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Ml320);
        break;
    case SstvTxCoordinatorMode::Mp73Narrow:
        pixelCount = SstvMmsstvEncoder::pixelCount(
            SstvMmsstvMode::Mp73Narrow);
        break;
    case SstvTxCoordinatorMode::Mp110Narrow:
        pixelCount = SstvMmsstvEncoder::pixelCount(
            SstvMmsstvMode::Mp110Narrow);
        break;
    case SstvTxCoordinatorMode::Mp140Narrow:
        pixelCount = SstvMmsstvEncoder::pixelCount(
            SstvMmsstvMode::Mp140Narrow);
        break;
    case SstvTxCoordinatorMode::Mc110Narrow:
        pixelCount = SstvMmsstvEncoder::pixelCount(
            SstvMmsstvMode::Mc110Narrow);
        break;
    case SstvTxCoordinatorMode::Mc140Narrow:
        pixelCount = SstvMmsstvEncoder::pixelCount(
            SstvMmsstvMode::Mc140Narrow);
        break;
    case SstvTxCoordinatorMode::Mc180Narrow:
        pixelCount = SstvMmsstvEncoder::pixelCount(
            SstvMmsstvMode::Mc180Narrow);
        break;
    }
    request.pixels.assign(
        pixelCount,
        SstvRgbPixel {17U, 91U, 203U});
    return request;
}

std::unique_ptr<SstvTxCoordinator> enabledCoordinator(
    FakeHooks& fake,
    SstvTxCoordinatorConfig config = fastConfig())
{
    auto coordinator = std::make_unique<SstvTxCoordinator>(
        std::move(config), fake.make());
    const SstvTxCoordinatorResult enabled = coordinator->enable(0U);
    Q_ASSERT(enabled.accepted);
    return coordinator;
}

bool observedState(const FakeHooks& fake, SstvTxState state)
{
    for (const SstvTxCoordinatorSnapshot& snapshot : fake.observations) {
        if (snapshot.stateMachine.state == state) {
            return true;
        }
    }
    return false;
}

} // namespace

class TestSstvTxCoordinator final : public QObject
{
    Q_OBJECT

private slots:
    void configurationRequiresExplicitExistingPathHooks();
    void sharedSourceBuilderNormalizesQImageAndComposesFsk();
    void martinFamilySourceBuilderUsesSelectedSpecs();
    void robotFamilySourceBuilderUsesSelectedSpecs();
    void sequentialRgbFamilySourceBuilderUsesSelectedSpecs();
    void pdFamilySourceBuilderUsesSelectedSpecs();
    void avtFamilySourceBuilderUsesProtectedHeaderAndSelectedSpecs();
    void mmsstvExtendedFamilySourceBuilderUsesSelectedSpecs();
    void preflightRejectsInvalidBusyAndUnavailableJobs();
    void allNativeModeFactoriesRemainLazyAndBounded();
    void confirmedPttLeadAudioFskTailAndReleaseAreOrdered();
    void cancellationDuringHeaderImageAndFskReleasesExactlyOnce();
    void calibrationPreparedPathSharesPttLifecycleAndMetrics();
    void voxCompletionUsesNoPttOffHook();
    void cancelShutdownRetainDeviceUntilDetachAndReleaseOnce();
    void audioLeaseSurvivesConcurrentPullUntilDetachAcknowledgement();
    void hookFailuresFailClosedWithoutDuplicatePttOff();
    void pttReleaseRetriesUntilFeedbackConfirms();
    void watchdogAndClockRegressionReleaseImmediately();
    void sessionScopedSinkUnderrunFailsClosed();
    void staleAndMaliciousPlaybackCallbacksFailClosed();
};

void TestSstvTxCoordinator::configurationRequiresExplicitExistingPathHooks()
{
    FakeHooks fake;
    SstvTxCoordinatorHooks hooks = fake.make();
    hooks.requestPttOff = {};
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxCoordinator(fastConfig(), std::move(hooks)));

    SstvTxCoordinatorConfig invalidLead = fastConfig();
    invalidLead.pttLeadDelayMs =
        invalidLead.stateMachinePolicy.timeouts.waitingForPttMs;
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxCoordinator(invalidLead, fake.make()));

    SstvTxCoordinatorConfig invalidTail = fastConfig();
    invalidTail.pttTailDelayMs =
        invalidTail.stateMachinePolicy.timeouts.tailDelayMs;
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxCoordinator(invalidTail, fake.make()));

    SstvTxCoordinatorConfig invalidRetry = fastConfig();
    invalidRetry.pttReleaseRetryMs = 0U;
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxCoordinator(invalidRetry, fake.make()));
    invalidRetry = fastConfig();
    invalidRetry.pttReleaseRetryMs
        = invalidRetry.stateMachinePolicy.timeouts.releasingPttMs + 1U;
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxCoordinator(invalidRetry, fake.make()));

    SstvTxCoordinatorConfig invalidVox = fastConfig();
    invalidVox.voxPreKeyMs = 0U;
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxCoordinator(invalidVox, fake.make()));
    invalidVox = fastConfig();
    invalidVox.voxHangMs =
        invalidVox.stateMachinePolicy.timeouts.tailDelayMs;
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxCoordinator(invalidVox, fake.make()));
    invalidVox = fastConfig();
    invalidVox.voxToneFrequencyHz =
        static_cast<double>(invalidVox.sampleRate) / 2.0;
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxCoordinator(invalidVox, fake.make()));
    invalidVox = fastConfig();
    invalidVox.voxToneLevel =
        std::numeric_limits<double>::quiet_NaN();
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxCoordinator(invalidVox, fake.make()));
}

void TestSstvTxCoordinator::sharedSourceBuilderNormalizesQImageAndComposesFsk()
{
    QCOMPARE(SstvTxSourceBuilder::modeFromId("martin-m1"),
             std::optional<SstvTxCoordinatorMode> {
                 SstvTxCoordinatorMode::MartinM1});
    QCOMPARE(SstvTxSourceBuilder::modeFromId("martin-m2"),
             std::optional<SstvTxCoordinatorMode> {
                 SstvTxCoordinatorMode::MartinM2});
    QCOMPARE(SstvTxSourceBuilder::modeFromId("martin-m3"),
             std::optional<SstvTxCoordinatorMode> {
                 SstvTxCoordinatorMode::MartinM3});
    QCOMPARE(SstvTxSourceBuilder::modeFromId("martin-m4"),
             std::optional<SstvTxCoordinatorMode> {
                 SstvTxCoordinatorMode::MartinM4});
    QCOMPARE(SstvTxSourceBuilder::modeFromId("scottie-s1"),
             std::optional<SstvTxCoordinatorMode> {
                 SstvTxCoordinatorMode::ScottieS1});
    QCOMPARE(SstvTxSourceBuilder::modeFromId("scottie-s2"),
             std::optional<SstvTxCoordinatorMode> {
                 SstvTxCoordinatorMode::ScottieS2});
    QCOMPARE(SstvTxSourceBuilder::modeFromId("scottie-s3"),
             std::optional<SstvTxCoordinatorMode> {
                 SstvTxCoordinatorMode::ScottieS3});
    QCOMPARE(SstvTxSourceBuilder::modeFromId("scottie-s4"),
             std::optional<SstvTxCoordinatorMode> {
                 SstvTxCoordinatorMode::ScottieS4});
    QCOMPARE(SstvTxSourceBuilder::modeFromId("scottie-dx"),
             std::optional<SstvTxCoordinatorMode> {
                 SstvTxCoordinatorMode::ScottieDx});
    const struct {
        const char* id;
        SstvTxCoordinatorMode mode;
    } robotIds[] {
        {"robot-c12", SstvTxCoordinatorMode::RobotColour12},
        {"robot-c24", SstvTxCoordinatorMode::RobotColour24},
        {"robot-c36", SstvTxCoordinatorMode::RobotColour36},
        {"robot-c72", SstvTxCoordinatorMode::RobotColour72},
        {"robot-bw8", SstvTxCoordinatorMode::RobotBw8},
        {"robot-bw12", SstvTxCoordinatorMode::RobotBw12},
        {"robot-bw24", SstvTxCoordinatorMode::RobotBw24},
        {"robot-bw36", SstvTxCoordinatorMode::RobotBw36},
        {"wraase-sc2-60", SstvTxCoordinatorMode::WraaseSc2_60},
        {"wraase-sc2-120", SstvTxCoordinatorMode::WraaseSc2_120},
        {"wraase-sc2-180", SstvTxCoordinatorMode::WraaseSc2_180},
        {"pasokon-p3", SstvTxCoordinatorMode::PasokonP3},
        {"pasokon-p5", SstvTxCoordinatorMode::PasokonP5},
        {"pasokon-p7", SstvTxCoordinatorMode::PasokonP7},
        {"pd-50", SstvTxCoordinatorMode::Pd50},
        {"pd-90", SstvTxCoordinatorMode::Pd90},
        {"pd-120", SstvTxCoordinatorMode::Pd120},
        {"pd-160", SstvTxCoordinatorMode::Pd160},
        {"pd-180", SstvTxCoordinatorMode::Pd180},
        {"pd-240", SstvTxCoordinatorMode::Pd240},
        {"pd-290", SstvTxCoordinatorMode::Pd290},
        {"avt-24", SstvTxCoordinatorMode::Avt24},
        {"avt-90", SstvTxCoordinatorMode::Avt90},
        {"avt-94", SstvTxCoordinatorMode::Avt94},
        {"mp-73", SstvTxCoordinatorMode::Mp73},
        {"mp-115", SstvTxCoordinatorMode::Mp115},
        {"mp-140", SstvTxCoordinatorMode::Mp140},
        {"mp-175", SstvTxCoordinatorMode::Mp175},
        {"mr-73", SstvTxCoordinatorMode::Mr73},
        {"mr-90", SstvTxCoordinatorMode::Mr90},
        {"mr-115", SstvTxCoordinatorMode::Mr115},
        {"mr-140", SstvTxCoordinatorMode::Mr140},
        {"mr-175", SstvTxCoordinatorMode::Mr175},
        {"ml-180", SstvTxCoordinatorMode::Ml180},
        {"ml-240", SstvTxCoordinatorMode::Ml240},
        {"ml-280", SstvTxCoordinatorMode::Ml280},
        {"ml-320", SstvTxCoordinatorMode::Ml320},
        {"mp-73-narrow", SstvTxCoordinatorMode::Mp73Narrow},
        {"mp-110-narrow", SstvTxCoordinatorMode::Mp110Narrow},
        {"mp-140-narrow", SstvTxCoordinatorMode::Mp140Narrow},
        {"mc-110-narrow", SstvTxCoordinatorMode::Mc110Narrow},
        {"mc-140-narrow", SstvTxCoordinatorMode::Mc140Narrow},
        {"mc-180-narrow", SstvTxCoordinatorMode::Mc180Narrow},
    };
    for (const auto& item : robotIds) {
        QCOMPARE(SstvTxSourceBuilder::modeFromId(item.id),
                 std::optional<SstvTxCoordinatorMode> {item.mode});
        QCOMPARE(std::string(SstvTxSourceBuilder::modeId(item.mode)),
                 std::string(item.id));
    }
    QVERIFY(!SstvTxSourceBuilder::modeFromId("unknown-mode").has_value());

    QImage image(static_cast<int>(SstvMartinM1Protocol::Width),
                 static_cast<int>(SstvMartinM1Protocol::Height),
                 QImage::Format_ARGB32);
    image.fill(qRgb(12, 34, 56));
    const std::vector<SstvRgbPixel> pixels =
        SstvTxSourceBuilder::pixelsFromImage(image);
    QCOMPARE(pixels.size(), SstvMartinM1Encoder::PixelCount);
    QCOMPARE(pixels.front().red, std::uint8_t {12U});
    QCOMPARE(pixels.front().green, std::uint8_t {34U});
    QCOMPARE(pixels.front().blue, std::uint8_t {56U});
    QCOMPARE(pixels.back().red, std::uint8_t {12U});

    QImage wrong(319, 256, QImage::Format_RGB888);
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxSourceBuilder::pixelsFromImage(wrong));

    QImage halfHeight(320, 128, QImage::Format_RGB888);
    halfHeight.fill(qRgb(200, 100, 50));
    const auto halfPixels = SstvTxSourceBuilder::pixelsFromImage(
        halfHeight, SstvTxCoordinatorMode::ScottieS4);
    QCOMPARE(halfPixels.size(),
             SstvScottieEncoder::pixelCount(SstvScottieMode::S4));
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxSourceBuilder::pixelsFromImage(
            halfHeight, SstvTxCoordinatorMode::ScottieS2));

    SstvTxSourceBuilderConfig halfConfig;
    halfConfig.mode = SstvTxCoordinatorMode::ScottieS4;
    SstvTxBuiltSource halfBuilt = SstvTxSourceBuilder::build(
        halfHeight, halfConfig);
    QCOMPARE(halfBuilt.mode, std::string("scottie-s4"));
    QCOMPARE(halfBuilt.width, std::uint32_t {320U});
    QCOMPARE(halfBuilt.height, std::uint32_t {128U});
    QCOMPARE(halfBuilt.source->producedSamples(), std::uint64_t {0U});

    SstvTxSourceBuilderConfig config;
    config.mode = SstvTxCoordinatorMode::ScottieS1;
    config.fskId = SstvTxFskIdPlan {"IU8LMC",
        SstvFskIdCodec::TextPolicy::Callsign,
        SstvFskIdCodec::InputHandling::Strict};
    SstvTxBuiltSource built = SstvTxSourceBuilder::build(image, config);
    QVERIFY(built.source != nullptr);
    QCOMPARE(built.mode, std::string("scottie-s1"));
    QCOMPARE(built.width, std::uint32_t {320U});
    QCOMPARE(built.height, std::uint32_t {256U});
    QVERIFY(built.headerFrames > 0U);
    QVERIFY(built.imageEndFrame > built.headerFrames);
    QVERIFY(built.fskIdFrames > 0U);
    QCOMPARE(built.totalFrames - built.imageEndFrame,
             built.fskIdFrames);
    QCOMPARE(built.source->producedSamples(), std::uint64_t {0U});

    std::array<std::int16_t, 1'024U> scratch {};
    QCOMPARE(built.source->pullPcm16(scratch.data(), scratch.size()),
             scratch.size());
    QCOMPARE(built.source->producedSamples(),
             std::uint64_t {scratch.size()});
    QVERIFY(!built.source->complete());
    built.source->cancel();
    QVERIFY(built.source->cancelled());
    built.source->reset();
    QCOMPARE(built.source->producedSamples(), std::uint64_t {0U});
}

void TestSstvTxCoordinator::martinFamilySourceBuilderUsesSelectedSpecs()
{
    const struct {
        SstvTxCoordinatorMode coordinatorMode;
        SstvMartinMode protocolMode;
        const char* id;
        std::uint32_t height;
        std::uint64_t imageEndFramesAt12k;
    } cases[] {
        {SstvTxCoordinatorMode::MartinM1,
         SstvMartinMode::M1, "martin-m1", 256U, 1'382'402U},
        {SstvTxCoordinatorMode::MartinM2,
         SstvMartinMode::M2, "martin-m2", 256U, 707'643U},
        {SstvTxCoordinatorMode::MartinM3,
         SstvMartinMode::M3, "martin-m3", 128U, 696'661U},
        {SstvTxCoordinatorMode::MartinM4,
         SstvMartinMode::M4, "martin-m4", 128U, 359'281U},
    };

    for (const auto& item : cases) {
        const SstvMartinModeSpec spec = SstvMartinM1Protocol::spec(
            item.protocolMode);
        QCOMPARE(spec.height, item.height);
        QImage image(static_cast<int>(spec.width),
                     static_cast<int>(spec.height),
                     QImage::Format_RGB888);
        image.fill(qRgb(12, 34, 56));

        SstvTxSourceBuilderConfig config;
        config.mode = item.coordinatorMode;
        config.sampleRate = 12'000U;
        SstvTxBuiltSource built = SstvTxSourceBuilder::build(image, config);
        QCOMPARE(built.mode, std::string(item.id));
        QCOMPARE(built.width, spec.width);
        QCOMPARE(built.height, spec.height);
        QCOMPARE(built.headerFrames, std::uint64_t {10'920U});
        QCOMPARE(built.imageEndFrame, item.imageEndFramesAt12k);
        QCOMPARE(built.totalFrames, item.imageEndFramesAt12k);
        QCOMPARE(built.fskIdFrames, std::uint64_t {0U});
        QCOMPARE(built.source->sampleRate(), std::uint32_t {12'000U});
        QCOMPARE(built.source->totalSamples(), item.imageEndFramesAt12k);
        QCOMPARE(built.source->producedSamples(), std::uint64_t {0U});
    }

    QImage halfHeight(320, 128, QImage::Format_RGB888);
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxSourceBuilder::pixelsFromImage(
            halfHeight, SstvTxCoordinatorMode::MartinM2));
    QImage fullHeight(320, 256, QImage::Format_RGB888);
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxSourceBuilder::pixelsFromImage(
            fullHeight, SstvTxCoordinatorMode::MartinM3));
}

void TestSstvTxCoordinator::robotFamilySourceBuilderUsesSelectedSpecs()
{
    const struct {
        SstvTxCoordinatorMode coordinatorMode;
        SstvRobotMode protocolMode;
        std::uint64_t imageEndFramesAt12k;
    } cases[] {
        {SstvTxCoordinatorMode::RobotColour12,
         SstvRobotMode::Colour12, 154'920U},
        {SstvTxCoordinatorMode::RobotColour24,
         SstvRobotMode::Colour24, 298'920U},
        {SstvTxCoordinatorMode::RobotColour36,
         SstvRobotMode::Colour36, 442'920U},
        {SstvTxCoordinatorMode::RobotColour72,
         SstvRobotMode::Colour72, 874'920U},
        {SstvTxCoordinatorMode::RobotBw8,
         SstvRobotMode::Bw8, 105'960U},
        {SstvTxCoordinatorMode::RobotBw12,
         SstvRobotMode::Bw12, 154'920U},
        {SstvTxCoordinatorMode::RobotBw24,
         SstvRobotMode::Bw24, 313'320U},
        {SstvTxCoordinatorMode::RobotBw36,
         SstvRobotMode::Bw36, 442'920U},
    };

    for (const auto& item : cases) {
        const SstvRobotModeSpec spec = SstvRobotProtocol::spec(
            item.protocolMode);
        QImage image(static_cast<int>(spec.width),
                     static_cast<int>(spec.height),
                     QImage::Format_RGB888);
        image.fill(qRgb(220, 31, 97));

        SstvTxSourceBuilderConfig config;
        config.mode = item.coordinatorMode;
        config.sampleRate = 12'000U;
        SstvTxBuiltSource built = SstvTxSourceBuilder::build(image, config);
        QCOMPARE(built.mode, std::string(spec.stableId));
        QCOMPARE(built.width, spec.width);
        QCOMPARE(built.height, spec.height);
        QCOMPARE(built.headerFrames, std::uint64_t {10'920U});
        QCOMPARE(built.imageEndFrame, item.imageEndFramesAt12k);
        QCOMPARE(built.totalFrames, item.imageEndFramesAt12k);
        QCOMPARE(built.fskIdFrames, std::uint64_t {0U});
        QCOMPARE(built.source->sampleRate(), std::uint32_t {12'000U});
        QCOMPARE(built.source->totalSamples(), item.imageEndFramesAt12k);
        QCOMPARE(built.source->producedSamples(), std::uint64_t {0U});
    }

    QImage c12(160, 120, QImage::Format_RGB888);
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxSourceBuilder::pixelsFromImage(
            c12, SstvTxCoordinatorMode::RobotColour24));
    QImage c24(320, 120, QImage::Format_RGB888);
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxSourceBuilder::pixelsFromImage(
            c24, SstvTxCoordinatorMode::RobotColour12));
}

void TestSstvTxCoordinator::sequentialRgbFamilySourceBuilderUsesSelectedSpecs()
{
    const struct {
        SstvTxCoordinatorMode coordinatorMode;
        SstvSequentialRgbMode protocolMode;
        std::uint64_t imageEndFramesAt12k;
    } cases[] {
        {SstvTxCoordinatorMode::WraaseSc2_60,
         SstvSequentialRgbMode::WraaseSc2_60, 749'442U},
        {SstvTxCoordinatorMode::WraaseSc2_120,
         SstvSequentialRgbMode::WraaseSc2_120, 1'471'725U},
        {SstvTxCoordinatorMode::WraaseSc2_180,
         SstvSequentialRgbMode::WraaseSc2_180, 2'195'181U},
        {SstvTxCoordinatorMode::PasokonP3,
         SstvSequentialRgbMode::PasokonP3, 2'447'520U},
        {SstvTxCoordinatorMode::PasokonP5,
         SstvSequentialRgbMode::PasokonP5, 3'665'820U},
        {SstvTxCoordinatorMode::PasokonP7,
         SstvSequentialRgbMode::PasokonP7, 4'884'120U},
    };

    for (const auto& item : cases) {
        const SstvSequentialRgbModeSpec spec =
            SstvSequentialRgbProtocol::spec(item.protocolMode);
        QImage image(static_cast<int>(spec.width),
                     static_cast<int>(spec.height),
                     QImage::Format_RGB888);
        image.fill(qRgb(33, 149, 217));

        SstvTxSourceBuilderConfig config;
        config.mode = item.coordinatorMode;
        config.sampleRate = 12'000U;
        SstvTxBuiltSource built = SstvTxSourceBuilder::build(image, config);
        QCOMPARE(built.mode, std::string(spec.stableId));
        QCOMPARE(built.width, spec.width);
        QCOMPARE(built.height, spec.height);
        QCOMPARE(built.headerFrames, std::uint64_t {10'920U});
        QCOMPARE(built.imageEndFrame, item.imageEndFramesAt12k);
        QCOMPARE(built.totalFrames, item.imageEndFramesAt12k);
        QCOMPARE(built.fskIdFrames, std::uint64_t {0U});
        QCOMPARE(built.source->sampleRate(), std::uint32_t {12'000U});
        QCOMPARE(built.source->totalSamples(), item.imageEndFramesAt12k);
        QCOMPARE(built.source->producedSamples(), std::uint64_t {0U});
    }

    QImage sc2(320, 256, QImage::Format_RGB888);
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxSourceBuilder::pixelsFromImage(
            sc2, SstvTxCoordinatorMode::PasokonP3));
    QImage pasokon(640, 496, QImage::Format_RGB888);
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxSourceBuilder::pixelsFromImage(
            pasokon, SstvTxCoordinatorMode::WraaseSc2_60));
}

void TestSstvTxCoordinator::pdFamilySourceBuilderUsesSelectedSpecs()
{
    const struct {
        SstvTxCoordinatorMode coordinatorMode;
        SstvPdMode protocolMode;
        std::uint64_t imageEndFramesAt12k;
    } cases[] {
        {SstvTxCoordinatorMode::Pd50, SstvPdMode::Pd50, 607'133U},
        {SstvTxCoordinatorMode::Pd90, SstvPdMode::Pd90, 1'090'789U},
        {SstvTxCoordinatorMode::Pd120, SstvPdMode::Pd120, 1'524'156U},
        {SstvTxCoordinatorMode::Pd160, SstvPdMode::Pd160, 1'941'518U},
        {SstvTxCoordinatorMode::Pd180, SstvPdMode::Pd180, 2'255'538U},
        {SstvTxCoordinatorMode::Pd240, SstvPdMode::Pd240, 2'986'920U},
        {SstvTxCoordinatorMode::Pd290, SstvPdMode::Pd290, 3'475'106U},
    };

    for (const auto& item : cases) {
        const SstvPdModeSpec spec = SstvPdProtocol::spec(item.protocolMode);
        QImage image(static_cast<int>(spec.width),
                     static_cast<int>(spec.height),
                     QImage::Format_RGB888);
        image.fill(qRgb(83, 157, 229));

        SstvTxSourceBuilderConfig config;
        config.mode = item.coordinatorMode;
        config.sampleRate = 12'000U;
        SstvTxBuiltSource built = SstvTxSourceBuilder::build(image, config);
        QCOMPARE(built.mode, std::string(spec.stableId));
        QCOMPARE(built.width, spec.width);
        QCOMPARE(built.height, spec.height);
        QCOMPARE(built.headerFrames, std::uint64_t {10'920U});
        QCOMPARE(built.imageEndFrame, item.imageEndFramesAt12k);
        QCOMPARE(built.totalFrames, item.imageEndFramesAt12k);
        QCOMPARE(built.fskIdFrames, std::uint64_t {0U});
        QCOMPARE(built.source->sampleRate(), std::uint32_t {12'000U});
        QCOMPARE(built.source->totalSamples(), item.imageEndFramesAt12k);
        QCOMPARE(built.source->producedSamples(), std::uint64_t {0U});
        QCOMPARE(built.source->pullPcm16(nullptr, 0U), std::size_t {0U});
    }

    QImage pd50(320, 256, QImage::Format_RGB888);
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxSourceBuilder::pixelsFromImage(
            pd50, SstvTxCoordinatorMode::Pd120));
    QImage pd290(800, 616, QImage::Format_RGB888);
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxSourceBuilder::pixelsFromImage(
            pd290, SstvTxCoordinatorMode::Pd50));
}

void TestSstvTxCoordinator::
avtFamilySourceBuilderUsesProtectedHeaderAndSelectedSpecs()
{
    const struct {
        SstvTxCoordinatorMode coordinatorMode;
        SstvAvtMode protocolMode;
        std::uint64_t imageEndFramesAt12k;
    } cases[] {
        {SstvTxCoordinatorMode::Avt24,
         SstvAvtMode::Avt24, 366'510U},
        {SstvTxCoordinatorMode::Avt90,
         SstvAvtMode::Avt90, 1'176'510U},
        {SstvTxCoordinatorMode::Avt94,
         SstvAvtMode::Avt94, 1'221'510U},
    };

    for (const auto& item : cases) {
        const SstvAvtModeSpec spec = SstvAvtProtocol::spec(
            item.protocolMode);
        QImage image(static_cast<int>(spec.width),
                     static_cast<int>(spec.height),
                     QImage::Format_RGB888);
        image.fill(qRgb(29, 113, 241));

        SstvTxSourceBuilderConfig config;
        config.mode = item.coordinatorMode;
        config.sampleRate = 12'000U;
        SstvTxBuiltSource built = SstvTxSourceBuilder::build(image, config);
        QCOMPARE(built.mode, std::string(spec.stableId));
        QCOMPARE(built.width, spec.width);
        QCOMPARE(built.height, spec.height);
        QCOMPARE(built.headerFrames, std::uint64_t {96'510U});
        QCOMPARE(built.imageEndFrame, item.imageEndFramesAt12k);
        QCOMPARE(built.totalFrames, item.imageEndFramesAt12k);
        QCOMPARE(built.fskIdFrames, std::uint64_t {0U});
        QCOMPARE(built.source->sampleRate(), std::uint32_t {12'000U});
        QCOMPARE(built.source->totalSamples(), item.imageEndFramesAt12k);
        QCOMPARE(built.source->producedSamples(), std::uint64_t {0U});
        std::array<std::int16_t, 257U> scratch {};
        QCOMPARE(built.source->pullPcm16(scratch.data(), scratch.size()),
                 scratch.size());
        QCOMPARE(built.source->producedSamples(),
                 std::uint64_t {scratch.size()});
    }

    // AVT90's effective 256-column resolution does not shrink the audited
    // prepared/transmitted 320-column raster accepted by the shared builder.
    QImage effectiveOnly(256, 240, QImage::Format_RGB888);
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxSourceBuilder::pixelsFromImage(
            effectiveOnly, SstvTxCoordinatorMode::Avt90));
    QImage avt90(320, 240, QImage::Format_RGB888);
    const auto pixels = SstvTxSourceBuilder::pixelsFromImage(
        avt90, SstvTxCoordinatorMode::Avt90);
    QCOMPARE(pixels.size(), std::size_t {320U * 240U});
}

void TestSstvTxCoordinator::mmsstvExtendedFamilySourceBuilderUsesSelectedSpecs()
{
    const struct {
        SstvTxCoordinatorMode coordinatorMode;
        SstvMmsstvMode protocolMode;
        std::uint64_t imageEndFramesAt12k;
    } cases[] {
        {SstvTxCoordinatorMode::Mp73, SstvMmsstvMode::Mp73, 889'320U},
        {SstvTxCoordinatorMode::Mp115, SstvMmsstvMode::Mp115, 1'399'272U},
        {SstvTxCoordinatorMode::Mp140, SstvMmsstvMode::Mp140, 1'688'040U},
        {SstvTxCoordinatorMode::Mp175, SstvMmsstvMode::Mp175, 2'118'120U},
        {SstvTxCoordinatorMode::Mr73, SstvMmsstvMode::Mr73, 893'313U},
        {SstvTxCoordinatorMode::Mr90, SstvMmsstvMode::Mr90, 1'096'065U},
        {SstvTxCoordinatorMode::Mr115, SstvMmsstvMode::Mr115, 1'397'121U},
        {SstvTxCoordinatorMode::Mr140, SstvMmsstvMode::Mr140, 1'698'177U},
        {SstvTxCoordinatorMode::Mr175, SstvMmsstvMode::Mr175, 2'115'969U},
        {SstvTxCoordinatorMode::Ml180, SstvMmsstvMode::Ml180, 2'176'161U},
        {SstvTxCoordinatorMode::Ml240, SstvMmsstvMode::Ml240, 2'890'401U},
        {SstvTxCoordinatorMode::Ml280, SstvMmsstvMode::Ml280, 3'378'465U},
        {SstvTxCoordinatorMode::Ml320, SstvMmsstvMode::Ml320, 3'854'625U},
        {SstvTxCoordinatorMode::Mp73Narrow,
         SstvMmsstvMode::Mp73Narrow, 886'920U},
        {SstvTxCoordinatorMode::Mp110Narrow,
         SstvMmsstvMode::Mp110Narrow, 1'329'288U},
        {SstvTxCoordinatorMode::Mp140Narrow,
         SstvMmsstvMode::Mp140Narrow, 1'685'640U},
        {SstvTxCoordinatorMode::Mc110Narrow,
         SstvMmsstvMode::Mc110Narrow, 1'327'752U},
        {SstvTxCoordinatorMode::Mc140Narrow,
         SstvMmsstvMode::Mc140Narrow, 1'696'392U},
        {SstvTxCoordinatorMode::Mc180Narrow,
         SstvMmsstvMode::Mc180Narrow, 2'175'624U},
    };

    for (const auto& item : cases) {
        const SstvMmsstvModeSpec spec = SstvMmsstvProtocol::spec(
            item.protocolMode);
        QImage image(static_cast<int>(spec.width),
                     static_cast<int>(spec.height),
                     QImage::Format_RGB888);
        image.fill(qRgb(41, 137, 233));

        SstvTxSourceBuilderConfig config;
        config.mode = item.coordinatorMode;
        config.sampleRate = 12'000U;
        SstvTxBuiltSource built = SstvTxSourceBuilder::build(image, config);
        QCOMPARE(built.mode, std::string(spec.stableId));
        QCOMPARE(built.width, spec.width);
        QCOMPARE(built.height, spec.height);
        QCOMPARE(built.headerFrames,
                 spec.narrow ? std::uint64_t {11'400U}
                             : std::uint64_t {13'800U});
        QCOMPARE(built.imageEndFrame, item.imageEndFramesAt12k);
        QCOMPARE(built.totalFrames, item.imageEndFramesAt12k);
        QCOMPARE(built.fskIdFrames, std::uint64_t {0U});
        QCOMPARE(built.source->sampleRate(), std::uint32_t {12'000U});
        QCOMPARE(built.source->totalSamples(), item.imageEndFramesAt12k);
        QCOMPARE(built.source->producedSamples(), std::uint64_t {0U});
        std::array<std::int16_t, 257U> scratch {};
        QCOMPARE(built.source->pullPcm16(scratch.data(), scratch.size()),
                 scratch.size());
        QCOMPARE(built.source->producedSamples(),
                 std::uint64_t {scratch.size()});
    }

    QImage mp(320, 256, QImage::Format_RGB888);
    QImage ml(640, 496, QImage::Format_RGB888);
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxSourceBuilder::pixelsFromImage(
            mp, SstvTxCoordinatorMode::Ml180));
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        SstvTxSourceBuilder::pixelsFromImage(
            ml, SstvTxCoordinatorMode::Mc110Narrow));
}

void TestSstvTxCoordinator::preflightRejectsInvalidBusyAndUnavailableJobs()
{
    FakeHooks fake;
    auto coordinator = enabledCoordinator(fake);

    SstvTxCoordinatorRequest request = requestFor(
        SstvTxCoordinatorMode::MartinM1);
    request.pixels.pop_back();
    SstvTxCoordinatorResult result = coordinator->start(1U, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.error, SstvTxErrorCode::InvalidImage);
    QCOMPARE(fake.preflightCalls, std::uint64_t {0U});
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::Idle);

    request = requestFor(SstvTxCoordinatorMode::MartinM1);
    request.channelCount = 1U;
    request.channelRoute = SstvTxChannelRoute::Left;
    result = coordinator->start(2U, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.error, SstvTxErrorCode::AudioDeviceLoss);
    QCOMPARE(fake.preflightCalls, std::uint64_t {0U});

    request = requestFor(SstvTxCoordinatorMode::MartinM1);
    request.mode = static_cast<SstvTxCoordinatorMode>(255U);
    result = coordinator->start(3U, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.error, SstvTxErrorCode::UnsupportedMode);

    request = requestFor(SstvTxCoordinatorMode::MartinM1);
    request.fskId = SstvTxFskIdPlan {"bad!",
        SstvFskIdCodec::TextPolicy::Callsign,
        SstvFskIdCodec::InputHandling::Strict};
    result = coordinator->start(4U, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.error, SstvTxErrorCode::EncodingFailure);

    request = requestFor(SstvTxCoordinatorMode::MartinM1);
    fake.preflight.audioOutputReady = false;
    result = coordinator->start(5U, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.error, SstvTxErrorCode::AudioDeviceLoss);
    QCOMPARE(fake.pttOnCalls, std::uint64_t {0U});

    fake.preflight.audioOutputReady = true;
    fake.preflight.pttPathReady = false;
    result = coordinator->start(6U, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.error, SstvTxErrorCode::TxNotPermitted);

    fake.preflight.pttPathReady = true;
    fake.preflight.weakSignalSequencerActive = true;
    result = coordinator->start(7U, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.error, SstvTxErrorCode::TxBusy);

    fake.preflight.weakSignalSequencerActive = false;
    fake.preflight.transmitAlreadyActive = true;
    result = coordinator->start(8U, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.error, SstvTxErrorCode::TxBusy);
    QCOMPARE(fake.pttOnCalls, std::uint64_t {0U});
    QCOMPARE(fake.audioStartCalls, std::uint64_t {0U});

    fake.preflight.transmitAlreadyActive = false;
    result = coordinator->start(9U, request);
    QVERIFY(result.accepted);
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::WaitingForPtt);
    const SstvTxCoordinatorResult busy = coordinator->start(10U, request);
    QVERIFY(!busy.accepted);
    QCOMPARE(busy.error, SstvTxErrorCode::TxBusy);
    QCOMPARE(fake.pttOnCalls, std::uint64_t {1U});

    QVERIFY(coordinator->cancel(11U));
    QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
    QVERIFY(coordinator->notifyPttReleased(12U, result.sessionId));
    QVERIFY(coordinator->shutdown(13U));
}

void TestSstvTxCoordinator::allNativeModeFactoriesRemainLazyAndBounded()
{
    const struct {
        SstvTxCoordinatorMode mode;
        const char* id;
        std::uint32_t width;
        std::uint32_t height;
        std::uint64_t exactImageEndFrames;
    } cases[] {
        {SstvTxCoordinatorMode::MartinM1,
         "martin-m1", 320U, 256U, 5'529'608U},
        {SstvTxCoordinatorMode::MartinM2,
         "martin-m2", 320U, 256U, 2'830'573U},
        {SstvTxCoordinatorMode::MartinM3,
         "martin-m3", 320U, 128U, 2'786'644U},
        {SstvTxCoordinatorMode::MartinM4,
         "martin-m4", 320U, 128U, 1'437'126U},
        {SstvTxCoordinatorMode::ScottieS1,
         "scottie-s1", 320U, 256U, 0U},
        {SstvTxCoordinatorMode::ScottieS2,
         "scottie-s2", 320U, 256U, 0U},
        {SstvTxCoordinatorMode::ScottieS3,
         "scottie-s3", 320U, 128U, 0U},
        {SstvTxCoordinatorMode::ScottieS4,
         "scottie-s4", 320U, 128U, 0U},
        {SstvTxCoordinatorMode::ScottieDx,
         "scottie-dx", 320U, 256U, 0U},
        {SstvTxCoordinatorMode::RobotColour12,
         "robot-c12", 160U, 120U, 619'680U},
        {SstvTxCoordinatorMode::RobotColour24,
         "robot-c24", 320U, 120U, 1'195'680U},
        {SstvTxCoordinatorMode::RobotColour36,
         "robot-c36", 320U, 240U, 1'771'680U},
        {SstvTxCoordinatorMode::RobotColour72,
         "robot-c72", 320U, 240U, 3'499'680U},
        {SstvTxCoordinatorMode::RobotBw8,
         "robot-bw8", 160U, 120U, 423'840U},
        {SstvTxCoordinatorMode::RobotBw12,
         "robot-bw12", 160U, 120U, 619'680U},
        {SstvTxCoordinatorMode::RobotBw24,
         "robot-bw24", 320U, 240U, 1'253'280U},
        {SstvTxCoordinatorMode::RobotBw36,
         "robot-bw36", 320U, 240U, 1'771'680U},
        {SstvTxCoordinatorMode::WraaseSc2_60,
         "wraase-sc2-60", 320U, 256U, 2'997'768U},
        {SstvTxCoordinatorMode::WraaseSc2_120,
         "wraase-sc2-120", 320U, 256U, 5'886'900U},
        {SstvTxCoordinatorMode::WraaseSc2_180,
         "wraase-sc2-180", 320U, 256U, 8'780'724U},
        {SstvTxCoordinatorMode::PasokonP3,
         "pasokon-p3", 640U, 496U, 9'790'080U},
        {SstvTxCoordinatorMode::PasokonP5,
         "pasokon-p5", 640U, 496U, 14'663'280U},
        {SstvTxCoordinatorMode::PasokonP7,
         "pasokon-p7", 640U, 496U, 19'536'480U},
        {SstvTxCoordinatorMode::Pd50,
         "pd-50", 320U, 256U, 2'428'535U},
        {SstvTxCoordinatorMode::Pd90,
         "pd-90", 320U, 256U, 4'363'157U},
        {SstvTxCoordinatorMode::Pd120,
         "pd-120", 640U, 496U, 6'096'625U},
        {SstvTxCoordinatorMode::Pd160,
         "pd-160", 512U, 400U, 7'766'073U},
        {SstvTxCoordinatorMode::Pd180,
         "pd-180", 640U, 496U, 9'022'152U},
        {SstvTxCoordinatorMode::Pd240,
         "pd-240", 640U, 496U, 11'947'680U},
        {SstvTxCoordinatorMode::Pd290,
         "pd-290", 800U, 616U, 13'900'427U},
        {SstvTxCoordinatorMode::Avt24,
         "avt-24", 128U, 120U, 0U},
        {SstvTxCoordinatorMode::Avt90,
         "avt-90", 320U, 240U, 0U},
        {SstvTxCoordinatorMode::Avt94,
         "avt-94", 320U, 200U, 0U},
    };

    for (const auto& item : cases) {
        FakeHooks fake;
        auto coordinator = enabledCoordinator(fake);
        SstvTxCoordinatorRequest request = requestFor(item.mode);
        const SstvTxCoordinatorResult started = coordinator->start(1U,
                                                                   request);
        QVERIFY2(started.accepted, started.detail.c_str());
        const auto waiting = coordinator->snapshot();
        QCOMPARE(waiting.audioPlan.mode, std::string(item.id));
        QCOMPARE(waiting.stateMachine.mode, std::string(item.id));
        QCOMPARE(waiting.stateMachine.imageWidth, item.width);
        QCOMPARE(waiting.stateMachine.imageHeight, item.height);
        QCOMPARE(waiting.audioPlan.sampleRate, std::uint32_t {48'000U});
        QCOMPARE(waiting.audioPlan.channelCount, 1U);
        QVERIFY(waiting.audioPlan.headerFrames > 0U);
        QVERIFY(waiting.audioPlan.imageEndFrame
                > waiting.audioPlan.headerFrames);
        QCOMPARE(waiting.audioPlan.totalFrames,
                 waiting.audioPlan.imageEndFrame);
        QCOMPARE(waiting.audioPlan.fskIdFrames, std::uint64_t {0U});
        if (item.exactImageEndFrames != 0U) {
            QCOMPARE(waiting.audioPlan.headerFrames,
                     std::uint64_t {43'680U});
            QCOMPARE(waiting.audioPlan.imageEndFrame,
                     item.exactImageEndFrames);
        }

        QVERIFY(coordinator->notifyPttConfirmed(2U, started.sessionId));
        QVERIFY(coordinator->tick(12U));
        QCOMPARE(fake.audioStartCalls, std::uint64_t {1U});
        const std::shared_ptr<SstvTxAudioDevice> device =
            fake.audioDevice.lock();
        QVERIFY(device != nullptr);
        QCOMPARE(device->framesProducedBySource(), std::uint64_t {0U});
        QCOMPARE(device->totalFrames(), waiting.audioPlan.totalFrames);
        QVERIFY(device->totalFrames() > SstvTxAudioDevice::MaximumFramesPerPull);

        QVERIFY(coordinator->cancel(13U));
        QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});
        QCOMPARE(fake.lastDetachReason, SstvTxAudioDetachReason::Cancelled);
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QVERIFY(coordinator->notifyPttReleased(14U, started.sessionId));
    }
}

void TestSstvTxCoordinator::confirmedPttLeadAudioFskTailAndReleaseAreOrdered()
{
    FakeHooks fake;
    auto coordinator = enabledCoordinator(fake);
    SstvTxCoordinatorRequest request = requestFor(
        SstvTxCoordinatorMode::ScottieS2);
    request.channelCount = 2U;
    request.channelRoute = SstvTxChannelRoute::Right;
    request.fskId = SstvTxFskIdPlan {"IU8LMC",
        SstvFskIdCodec::TextPolicy::Callsign,
        SstvFskIdCodec::InputHandling::Strict};

    const SstvTxCoordinatorResult started = coordinator->start(1U, request);
    QVERIFY2(started.accepted, started.detail.c_str());
    QCOMPARE(fake.pttOnCalls, std::uint64_t {1U});
    QCOMPARE(fake.audioStartCalls, std::uint64_t {0U});
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::WaitingForPtt);

    QVERIFY(coordinator->notifyPttConfirmed(2U, started.sessionId));
    QVERIFY(coordinator->tick(11U));
    QCOMPARE(fake.audioStartCalls, std::uint64_t {0U});
    QVERIFY(coordinator->tick(12U));
    QCOMPARE(fake.audioStartCalls, std::uint64_t {1U});
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::TransmittingHeader);
    QCOMPARE(fake.callOrder.at(1U), std::string("ptt-on"));
    QCOMPARE(fake.callOrder.at(2U), std::string("audio-start"));

    const SstvTxAudioPlan plan = coordinator->snapshot().audioPlan;
    QVERIFY(plan.fskIdPlanned);
    QVERIFY(plan.fskIdFrames > 0U);
    QCOMPARE(plan.totalFrames - plan.imageEndFrame, plan.fskIdFrames);
    QCOMPARE(plan.channelCount, 2U);
    QCOMPARE(plan.channelRoute, SstvTxChannelRoute::Right);

    QVERIFY(coordinator->notifyPlayback(
        13U,
        started.sessionId,
        {plan.headerFrames - 1U, false, false, {}}));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::TransmittingHeader);
    QVERIFY(coordinator->notifyPlayback(
        14U,
        started.sessionId,
        {plan.headerFrames, false, false, {}}));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::TransmittingImage);
    QVERIFY(!coordinator->snapshot().fskIdOnAir);

    QVERIFY(coordinator->notifyPlayback(
        15U,
        started.sessionId,
        {plan.imageEndFrame, false, false, {}}));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::TransmittingFskId);
    QVERIFY(coordinator->snapshot().fskIdOnAir);
    QVERIFY(!coordinator->snapshot().fskIdCompleted);

    QVERIFY(coordinator->notifyPlayback(
        16U,
        started.sessionId,
        {plan.totalFrames, true, false, {}}));
    const auto tail = coordinator->snapshot();
    QCOMPARE(tail.stateMachine.state, SstvTxState::TailDelay);
    QVERIFY(!tail.fskIdOnAir);
    QVERIFY(tail.fskIdCompleted);
    QCOMPARE(tail.playedFrames, plan.totalFrames);
    QCOMPARE(tail.progress, 1.0);
    QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});
    QCOMPARE(fake.lastDetachReason, SstvTxAudioDetachReason::Completed);
    QVERIFY(fake.audioDevice.expired());

    QVERIFY(coordinator->tick(35U));
    QCOMPARE(fake.pttOffCalls, std::uint64_t {0U});
    QVERIFY(coordinator->tick(36U));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::ReleasingPtt);
    QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
    QVERIFY(coordinator->notifyPttReleased(37U, started.sessionId));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::Completed);
    QVERIFY(coordinator->snapshot().pttReleased);
    QVERIFY(observedState(fake, SstvTxState::TransmittingFskId));
    QCOMPARE(coordinator->stateMachine().metrics().sessionsCompleted,
             std::uint64_t {1U});
}

void TestSstvTxCoordinator::cancellationDuringHeaderImageAndFskReleasesExactlyOnce()
{
    enum class Phase : std::uint8_t { Header, Image, Fsk };
    const struct {
        Phase phase;
        SstvTxState expectedState;
    } cases[] {
        {Phase::Header, SstvTxState::TransmittingHeader},
        {Phase::Image, SstvTxState::TransmittingImage},
        {Phase::Fsk, SstvTxState::TransmittingFskId},
    };

    for (const auto& item : cases) {
        FakeHooks fake;
        auto coordinator = enabledCoordinator(fake);
        SstvTxCoordinatorRequest request = requestFor(
            SstvTxCoordinatorMode::ScottieS2);
        request.fskId = SstvTxFskIdPlan {
            "IU8LMC",
            SstvFskIdCodec::TextPolicy::Callsign,
            SstvFskIdCodec::InputHandling::Strict};

        const SstvTxCoordinatorResult started = coordinator->start(1U, request);
        QVERIFY2(started.accepted, started.detail.c_str());
        QVERIFY(coordinator->notifyPttConfirmed(2U, started.sessionId));
        QVERIFY(coordinator->tick(12U));

        const SstvTxAudioPlan plan = coordinator->snapshot().audioPlan;
        QVERIFY(plan.headerFrames > 0U);
        QVERIFY(plan.imageEndFrame > plan.headerFrames);
        QVERIFY(plan.protocolEndFrame > plan.imageEndFrame);
        std::uint64_t playedFrames = plan.headerFrames - 1U;
        switch (item.phase) {
        case Phase::Header:
            break;
        case Phase::Image:
            playedFrames = plan.headerFrames;
            break;
        case Phase::Fsk:
            playedFrames = plan.imageEndFrame;
            break;
        }
        QVERIFY(coordinator->notifyPlayback(
            13U, started.sessionId, {playedFrames, false, false, {}}));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 item.expectedState);

        std::shared_ptr<SstvTxAudioDevice> device = fake.audioDevice.lock();
        QVERIFY(device != nullptr);
        QVERIFY(!device->cancelled());
        QVERIFY(coordinator->cancel(14U));
        QVERIFY(device->cancelled());
        QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});
        QCOMPARE(fake.lastDetachReason, SstvTxAudioDetachReason::Cancelled);
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::ReleasingPtt);

        // Repeated cancellation and late output callbacks cannot detach the
        // source again, re-key the radio or request a duplicate PTT release.
        QVERIFY(coordinator->cancel(15U));
        QVERIFY(!coordinator->notifyPlayback(
            16U,
            started.sessionId,
            {plan.protocolEndFrame, false, false, {}}));
        QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});

        QVERIFY(coordinator->notifyPttReleased(17U, started.sessionId));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::Cancelled);
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QVERIFY(!coordinator->notifyPlayback(
            18U,
            started.sessionId,
            {plan.totalFrames, true, false, {}}));
        QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
    }
}

void TestSstvTxCoordinator::calibrationPreparedPathSharesPttLifecycleAndMetrics()
{
    SstvDiagnosticLogBuffer::instance().clear();
    FakeHooks fake;
    auto coordinator = enabledCoordinator(fake);
    SstvTxPreparedAudioRequest request;
    request.source = makeCalibrationTonePcm16Source(
        SstvCalibrationToneKind::WhiteReference,
        48'000U,
        250U);
    request.mode = "calibration-white-2300";
    request.width = 1U;
    request.height = 1U;
    request.headerEndFrame = 0U;
    request.imageEndFrame = request.source->totalSamples();
    request.headroom = kDefaultSstvTxHeadroom;

    const SstvTxCoordinatorResult started = coordinator->startPrepared(
        1U, std::move(request));
    QVERIFY2(started.accepted, started.detail.c_str());
    QCOMPARE(fake.pttOnCalls, std::uint64_t {1U});
    QCOMPARE(fake.audioStartCalls, std::uint64_t {0U});
    QVERIFY(coordinator->notifyPttConfirmed(2U, started.sessionId));
    QVERIFY(coordinator->tick(12U));
    QCOMPARE(fake.audioStartCalls, std::uint64_t {1U});

    std::shared_ptr<SstvTxAudioDevice> device = fake.audioDevice.lock();
    QVERIFY(device);
    while (!device->atEnd()) {
        const QByteArray pcm = device->read(1'007);
        QVERIFY(!pcm.isEmpty());
    }
    const SstvTxCoordinatorSnapshot active = coordinator->snapshot();
    QCOMPARE(active.audioPlan.mode,
             std::string("calibration-white-2300"));
    QCOMPARE(active.audioPlan.headerFrames, std::uint64_t {0U});
    QCOMPARE(active.audioPlan.imageEndFrame, std::uint64_t {12'000U});
    QCOMPARE(active.audioPlan.headroom, kDefaultSstvTxHeadroom);
    QVERIFY(active.pcmPeak > 0.88 && active.pcmPeak < 0.90);
    QCOMPARE(active.clippedFrames, std::uint64_t {0U});

    QVERIFY(coordinator->notifyPlayback(
        13U,
        started.sessionId,
        {active.audioPlan.totalFrames, true, false, {}}));
    device.reset();
    const SstvTxCoordinatorSnapshot detached = coordinator->snapshot();
    QVERIFY(!detached.audioLeaseRetained);
    QVERIFY(detached.pcmPeak > 0.88 && detached.pcmPeak < 0.90);
    QCOMPARE(detached.clippedFrames, std::uint64_t {0U});
    QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});
    QCOMPARE(fake.lastDetachReason, SstvTxAudioDetachReason::Completed);
    QVERIFY(coordinator->tick(34U));
    QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
    QVERIFY(coordinator->notifyPttReleased(35U, started.sessionId));

    FakeHooks cancelledFake;
    auto cancelledCoordinator = enabledCoordinator(cancelledFake);
    SstvTxPreparedAudioRequest cancelledRequest;
    cancelledRequest.source = makeCalibrationTonePcm16Source(
        SstvCalibrationToneKind::SyncReference,
        48'000U,
        250U);
    cancelledRequest.mode = "calibration-sync-1200";
    cancelledRequest.imageEndFrame = cancelledRequest.source->totalSamples();
    const auto cancelStarted = cancelledCoordinator->startPrepared(
        40U, std::move(cancelledRequest));
    QVERIFY(cancelStarted.accepted);
    QVERIFY(cancelledCoordinator->cancel(41U));
    QCOMPARE(cancelledFake.pttOffCalls, std::uint64_t {1U});
    QCOMPARE(cancelledCoordinator->snapshot().stateMachine.state,
             SstvTxState::ReleasingPtt);
    QVERIFY(cancelledCoordinator->notifyPttReleased(
        42U, cancelStarted.sessionId));
    QCOMPARE(cancelledCoordinator->snapshot().stateMachine.state,
             SstvTxState::Cancelled);

    FakeHooks rejectedFake;
    auto rejectedCoordinator = enabledCoordinator(rejectedFake);
    SstvTxPreparedAudioRequest rejectedRequest;
    rejectedRequest.mode = "calibration-black-1500";
    const auto rejected = rejectedCoordinator->startPrepared(
        50U, std::move(rejectedRequest));
    QVERIFY(!rejected.accepted);

    FakeHooks failedFake;
    auto failedCoordinator = enabledCoordinator(failedFake);
    SstvTxPreparedAudioRequest failedRequest;
    failedRequest.source = makeCalibrationTonePcm16Source(
        SstvCalibrationToneKind::LeaderReference, 48'000U, 250U);
    failedRequest.mode = "calibration-leader-1900";
    failedRequest.imageEndFrame = failedRequest.source->totalSamples();
    const auto failedStarted = failedCoordinator->startPrepared(
        60U, std::move(failedRequest));
    QVERIFY(failedStarted.accepted);
    QVERIFY(!failedCoordinator->notifyAudioError(
        61U,
        failedStarted.sessionId,
        "injected /Users/operator/secret-device failure"));
    QVERIFY(failedCoordinator->notifyPttReleased(
        62U, failedStarted.sessionId));

    const QVariantList events
        = SstvDiagnosticLogBuffer::instance().snapshot();
    QSet<QString> names;
    QVariantMap completedFields;
    for (const QVariant& value : events) {
        const QVariantMap event = value.toMap();
        if (event.value(QStringLiteral("category")).toString()
            != QStringLiteral("sstv.tx")) {
            continue;
        }
        const QString name = event.value(QStringLiteral("event")).toString();
        names.insert(name);
        if (name == QStringLiteral("tx.calibration-completed")) {
            completedFields = event.value(QStringLiteral("fields")).toMap();
        }
    }
    QVERIFY(names.contains(QStringLiteral("tx.calibration-started")));
    QVERIFY(names.contains(QStringLiteral("tx.calibration-completed")));
    QVERIFY(names.contains(
        QStringLiteral("tx.calibration-cancel-requested")));
    QVERIFY(names.contains(QStringLiteral("tx.calibration-cancelled")));
    QVERIFY(names.contains(QStringLiteral("tx.calibration-rejected")));
    QVERIFY(names.contains(QStringLiteral("tx.calibration-failed")));
    QCOMPARE(completedFields.value(QStringLiteral("modeId")).toString(),
             QStringLiteral("calibration-white-2300"));
    QCOMPARE(completedFields.value(QStringLiteral("success")).toBool(), true);
    QVERIFY(completedFields.contains(QStringLiteral("durationMs")));
    const QByteArray serialized = QJsonDocument::fromVariant(events).toJson(
        QJsonDocument::Compact);
    QVERIFY(!serialized.contains("/Users/operator"));
    QVERIFY(!serialized.contains("secret-device"));
}

void TestSstvTxCoordinator::voxCompletionUsesNoPttOffHook()
{
    FakeHooks fake;
    fake.preflight.pttReleaseRequired = false;
    const SstvTxCoordinatorConfig config = fastConfig();
    auto coordinator = enabledCoordinator(fake, config);
    SstvTxTimingConfig timing;
    timing.pttLeadDelayMs = config.pttLeadDelayMs;
    timing.pttTailDelayMs = config.pttTailDelayMs;
    timing.pttReleaseRetryMs = config.pttReleaseRetryMs;
    timing.voxPreKeyMs = 6U;
    timing.voxHangMs = 8U;
    timing.voxToneFrequencyHz = config.voxToneFrequencyHz;
    timing.voxToneLevel = config.voxToneLevel;
    QVERIFY(coordinator->updateTimingConfig(timing));
    const SstvTxCoordinatorResult started = coordinator->start(
        1U, requestFor(SstvTxCoordinatorMode::MartinM1));
    QVERIFY(started.accepted);
    QVERIFY(!coordinator->updateTimingConfig(timing));
    QCOMPARE(fake.audioStartCalls, std::uint64_t {0U});
    QVERIFY(coordinator->notifyPttConfirmed(2U, started.sessionId));
    QCOMPARE(fake.audioStartCalls, std::uint64_t {1U});
    const SstvTxAudioPlan plan = coordinator->snapshot().audioPlan;
    QVERIFY(plan.voxEnvelopeEnabled);
    QCOMPARE(plan.voxPreKeyFrames, std::uint64_t {288U});
    QCOMPARE(plan.voxHangFrames, std::uint64_t {384U});
    QCOMPARE(plan.protocolStartFrame, plan.voxPreKeyFrames);
    QCOMPARE(plan.totalFrames - plan.protocolEndFrame,
             plan.voxHangFrames);
    QCOMPARE(plan.protocolEndFrame - plan.imageEndFrame,
             std::uint64_t {0U});
    QCOMPARE(coordinator->snapshot().progress, 0.0);

    std::shared_ptr<SstvTxAudioDevice> device = fake.audioDevice.lock();
    QVERIFY(device != nullptr);
    std::array<std::int16_t, 96U> actualTone {};
    std::array<std::int16_t, 96U> expectedTone {};
    QCOMPARE(device->read(
                 reinterpret_cast<char*>(actualTone.data()),
                 static_cast<qint64>(actualTone.size()
                                     * sizeof(std::int16_t))),
             static_cast<qint64>(actualTone.size()
                                 * sizeof(std::int16_t)));
    SstvToneGenerator expectedGenerator(plan.sampleRate);
    QCOMPARE(expectedGenerator.generatePcm16(
                 1'900.0, 0.5, expectedTone.data(), expectedTone.size()),
             expectedTone.size());
    for (std::size_t index = 0U; index < actualTone.size(); ++index) {
        QCOMPARE(actualTone[index], expectedTone[index]);
    }

    QVERIFY(coordinator->notifyPlayback(
        3U,
        started.sessionId,
        {plan.protocolStartFrame, false, false, {}}));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::TransmittingHeader);
    QCOMPARE(coordinator->snapshot().progress, 0.0);
    QVERIFY(coordinator->notifyPlayback(
        4U,
        started.sessionId,
        {plan.headerFrames, false, false, {}}));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::TransmittingImage);
    QVERIFY(coordinator->snapshot().progress > 0.0);
    QVERIFY(coordinator->notifyPlayback(
        5U,
        started.sessionId,
        {plan.protocolEndFrame, false, false, {}}));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::TailDelay);
    QCOMPARE(coordinator->snapshot().progress, 1.0);
    QCOMPARE(fake.audioDetachCalls, std::uint64_t {0U});
    QVERIFY(coordinator->notifyPlayback(
        6U,
        started.sessionId,
        {plan.totalFrames, true, false, {}}));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::TailDelay);
    QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});
    QVERIFY(coordinator->tick(6U));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::Completed);
    QCOMPARE(fake.pttOnCalls, std::uint64_t {1U});
    QCOMPARE(fake.pttOffCalls, std::uint64_t {0U});
    QCOMPARE(coordinator->stateMachine().metrics().pttReleases,
             std::uint64_t {1U});
}

void TestSstvTxCoordinator::cancelShutdownRetainDeviceUntilDetachAndReleaseOnce()
{
    {
        FakeHooks fake;
        fake.audioDetachResult = false;
        auto coordinator = enabledCoordinator(fake);
        const SstvTxCoordinatorResult started = coordinator->start(
            1U, requestFor(SstvTxCoordinatorMode::ScottieS1));
        QVERIFY(started.accepted);
        QVERIFY(coordinator->notifyPttConfirmed(2U, started.sessionId));
        QVERIFY(coordinator->tick(12U));
        std::shared_ptr<SstvTxAudioDevice> device = fake.audioDevice.lock();
        QVERIFY(device != nullptr);

        QVERIFY(coordinator->cancel(13U));
        QVERIFY(device->cancelled());
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::ReleasingPtt);
        QVERIFY(coordinator->snapshot().audioDetachPending);
        QVERIFY(coordinator->snapshot().audioLeaseRetained);
        QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});

        QVERIFY(coordinator->cancel(14U));
        QVERIFY(coordinator->shutdown(15U));
        QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QVERIFY(coordinator->notifyPttReleased(16U, started.sessionId));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::Disabled);
        QVERIFY(coordinator->snapshot().audioLeaseRetained);

        device.reset();
        QVERIFY(!fake.audioDevice.expired());
        QVERIFY(coordinator->notifyAudioDetached(17U, started.sessionId));
        QVERIFY(fake.audioDevice.expired());
        QVERIFY(!coordinator->snapshot().audioLeaseRetained);
        QVERIFY(coordinator->shutdown(18U));
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
    }

    // VOX has no release barrier, so Cancelled/Disabled can be reached while
    // SoundOutput is still asynchronously detaching.  That must not drop the
    // only shared lease before the explicit detach acknowledgement.
    {
        FakeHooks fake;
        fake.preflight.pttReleaseRequired = false;
        fake.audioDetachResult = false;
        auto coordinator = enabledCoordinator(fake);
        const SstvTxCoordinatorResult started = coordinator->start(
            1U, requestFor(SstvTxCoordinatorMode::MartinM1));
        QVERIFY(started.accepted);
        QVERIFY(coordinator->notifyPttConfirmed(2U, started.sessionId));
        QVERIFY(coordinator->tick(12U));
        std::shared_ptr<SstvTxAudioDevice> device = fake.audioDevice.lock();
        QVERIFY(device != nullptr);

        QVERIFY(coordinator->cancel(13U));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::Cancelled);
        QVERIFY(coordinator->snapshot().audioDetachPending);
        QVERIFY(coordinator->snapshot().audioLeaseRetained);
        QCOMPARE(fake.pttOffCalls, std::uint64_t {0U});

        QVERIFY(coordinator->shutdown(14U));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::Disabled);
        QVERIFY(coordinator->snapshot().audioLeaseRetained);
        QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});

        device.reset();
        QVERIFY(!fake.audioDevice.expired());
        QVERIFY(coordinator->notifyAudioDetached(15U, started.sessionId));
        QVERIFY(fake.audioDevice.expired());
        QVERIFY(!coordinator->snapshot().audioLeaseRetained);
    }
}

void TestSstvTxCoordinator::audioLeaseSurvivesConcurrentPullUntilDetachAcknowledgement()
{
    FakeHooks fake;
    fake.audioDetachResult = false;
    auto coordinator = enabledCoordinator(fake);
    const SstvTxCoordinatorResult started = coordinator->start(
        1U, requestFor(SstvTxCoordinatorMode::MartinM1));
    QVERIFY(started.accepted);
    QVERIFY(coordinator->notifyPttConfirmed(2U, started.sessionId));
    QVERIFY(coordinator->tick(12U));

    std::shared_ptr<SstvTxAudioDevice> holder = fake.audioDevice.lock();
    QVERIFY(holder != nullptr);
    SstvTxAudioDevice* const pulledDevice = holder.get();
    holder.reset();

    std::atomic_bool firstRead {false};
    std::atomic_uint64_t bytesRead {0U};
    std::thread pullThread([pulledDevice, &firstRead, &bytesRead] {
        std::array<char, 8'192U> buffer {};
        for (;;) {
            const qint64 count = pulledDevice->read(
                buffer.data(), static_cast<qint64>(buffer.size()));
            firstRead.store(true, std::memory_order_release);
            if (count <= 0) {
                break;
            }
            bytesRead.fetch_add(static_cast<std::uint64_t>(count),
                                std::memory_order_relaxed);
        }
    });

    std::size_t spins = 0U;
    while (!firstRead.load(std::memory_order_acquire)
           && spins < 1'000'000U) {
        ++spins;
        std::this_thread::yield();
    }
    if (!firstRead.load(std::memory_order_acquire)) {
        pulledDevice->cancel();
        pullThread.join();
        QFAIL("SoundOutput-style pull thread did not start");
    }

    QVERIFY(coordinator->cancel(13U));
    pullThread.join();
    QVERIFY(bytesRead.load(std::memory_order_relaxed) > 0U);
    QVERIFY(!fake.audioDevice.expired());
    QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});
    QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
    QVERIFY(coordinator->notifyPttReleased(14U, started.sessionId));
    QVERIFY(coordinator->notifyAudioDetached(15U, started.sessionId));
    QVERIFY(fake.audioDevice.expired());
}

void TestSstvTxCoordinator::hookFailuresFailClosedWithoutDuplicatePttOff()
{
    {
        FakeHooks fake;
        fake.pttOnResult = false;
        auto coordinator = enabledCoordinator(fake);
        const SstvTxCoordinatorResult started = coordinator->start(
            1U, requestFor(SstvTxCoordinatorMode::MartinM1));
        QVERIFY(!started.accepted);
        QCOMPARE(started.error, SstvTxErrorCode::PttDispatchFailure);
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::ReleasingPtt);
        QCOMPARE(fake.pttOnCalls, std::uint64_t {1U});
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QVERIFY(coordinator->tick(2U));
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QVERIFY(coordinator->notifyPttReleased(3U, started.sessionId));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::Error);
    }

    {
        FakeHooks fake;
        fake.audioStartResult = false;
        auto coordinator = enabledCoordinator(fake);
        const SstvTxCoordinatorResult started = coordinator->start(
            1U, requestFor(SstvTxCoordinatorMode::ScottieS2));
        QVERIFY(started.accepted);
        QVERIFY(coordinator->notifyPttConfirmed(2U, started.sessionId));
        QVERIFY(!coordinator->tick(12U));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::ReleasingPtt);
        QCOMPARE(fake.audioStartCalls, std::uint64_t {1U});
        QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QVERIFY(coordinator->notifyPttReleased(13U, started.sessionId));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::Error);
    }

    {
        FakeHooks fake;
        fake.pttOffResult = false;
        auto coordinator = enabledCoordinator(fake);
        const SstvTxCoordinatorResult started = coordinator->start(
            1U, requestFor(SstvTxCoordinatorMode::ScottieDx));
        QVERIFY(started.accepted);
        QVERIFY(coordinator->cancel(2U));
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::ReleasingPtt);
        QCOMPARE(coordinator->snapshot().lastOperationError,
                 SstvTxErrorCode::PttDispatchFailure);
        QVERIFY(coordinator->tick(3U));
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QVERIFY(coordinator->notifyPttReleased(4U, started.sessionId));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::Error);
    }

    {
        FakeHooks fake;
        fake.throwPreflight = true;
        auto coordinator = enabledCoordinator(fake);
        const SstvTxCoordinatorResult started = coordinator->start(
            1U, requestFor(SstvTxCoordinatorMode::MartinM1));
        QVERIFY(!started.accepted);
        QCOMPARE(started.error, SstvTxErrorCode::InternalFailure);
        QCOMPARE(fake.pttOnCalls, std::uint64_t {0U});
    }

    {
        FakeHooks fake;
        fake.throwObserver = true;
        auto coordinator = enabledCoordinator(fake);
        const SstvTxCoordinatorResult started = coordinator->start(
            1U, requestFor(SstvTxCoordinatorMode::MartinM1));
        QVERIFY(started.accepted);
        QVERIFY(coordinator->snapshot().coordinator.hookFailures > 0U);
        QVERIFY(coordinator->cancel(2U));
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QVERIFY(coordinator->notifyPttReleased(3U, started.sessionId));
    }
}

void TestSstvTxCoordinator::pttReleaseRetriesUntilFeedbackConfirms()
{
    FakeHooks fake;
    auto coordinator = enabledCoordinator(fake);
    const SstvTxCoordinatorResult started = coordinator->start(
        1U, requestFor(SstvTxCoordinatorMode::MartinM1));
    QVERIFY(started.accepted);

    QVERIFY(coordinator->cancel(2U));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::ReleasingPtt);
    QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});

    QVERIFY(coordinator->tick(26U));
    QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
    QVERIFY(coordinator->tick(27U));
    QCOMPARE(fake.pttOffCalls, std::uint64_t {2U});
    QVERIFY(coordinator->tick(52U));
    QCOMPARE(fake.pttOffCalls, std::uint64_t {3U});
    QCOMPARE(coordinator->snapshot().coordinator.pttOffAttempts,
             std::uint64_t {3U});

    QVERIFY(coordinator->notifyPttReleased(53U, started.sessionId));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::Cancelled);
    QVERIFY(coordinator->tick(80U));
    QCOMPARE(fake.pttOffCalls, std::uint64_t {3U});
}

void TestSstvTxCoordinator::watchdogAndClockRegressionReleaseImmediately()
{
    {
        FakeHooks fake;
        SstvTxCoordinatorConfig config = fastConfig();
        config.stateMachinePolicy.timeouts.waitingForPttMs = 20U;
        config.stateMachinePolicy.timeouts.releasingPttMs = 30U;
        config.pttLeadDelayMs = 5U;
        auto coordinator = enabledCoordinator(fake, config);
        const SstvTxCoordinatorResult started = coordinator->start(
            1U, requestFor(SstvTxCoordinatorMode::MartinM1));
        QVERIFY(started.accepted);
        QVERIFY(coordinator->tick(21U));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::ReleasingPtt);
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QVERIFY(coordinator->notifyPttReleased(22U, started.sessionId));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::Error);
        QCOMPARE(coordinator->snapshot().stateMachine.lastErrorCode,
                 SstvTxErrorCode::PttTimeout);
    }

    {
        FakeHooks fake;
        auto coordinator = enabledCoordinator(fake);
        const SstvTxCoordinatorResult started = coordinator->start(
            10U, requestFor(SstvTxCoordinatorMode::ScottieS1));
        QVERIFY(started.accepted);
        QVERIFY(!coordinator->notifyPttConfirmed(9U, started.sessionId));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::ReleasingPtt);
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QVERIFY(coordinator->notifyPttReleased(11U, started.sessionId));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::Error);
        QCOMPARE(coordinator->snapshot().stateMachine.lastErrorCode,
                 SstvTxErrorCode::ClockRegression);
    }
}

void TestSstvTxCoordinator::sessionScopedSinkUnderrunFailsClosed()
{
    FakeHooks fake;
    auto coordinator = enabledCoordinator(fake);
    const SstvTxCoordinatorResult started = coordinator->start(
        1U, requestFor(SstvTxCoordinatorMode::MartinM1));
    QVERIFY(started.accepted);
    QVERIFY(coordinator->notifyPttConfirmed(2U, started.sessionId));
    QVERIFY(coordinator->tick(12U));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::TransmittingHeader);

    QVERIFY(!coordinator->notifyAudioUnderrun(
        13U, started.sessionId + 1U, "stale sink underrun"));
    QCOMPARE(coordinator->snapshot().stateMachine.state,
             SstvTxState::TransmittingHeader);

    // Failure notifications return false because the lifecycle operation did
    // not succeed, even though the matching callback was consumed.
    QVERIFY(!coordinator->notifyAudioUnderrun(
        14U, started.sessionId, "injected sink underrun"));
    const SstvTxCoordinatorSnapshot releasing = coordinator->snapshot();
    QCOMPARE(releasing.stateMachine.state, SstvTxState::ReleasingPtt);
    QCOMPARE(releasing.lastOperationError, SstvTxErrorCode::AudioUnderrun);
    QCOMPARE(QString::fromStdString(releasing.lastOperationDetail),
             QStringLiteral("injected sink underrun"));
    QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});
    QCOMPARE(fake.lastDetachReason, SstvTxAudioDetachReason::Error);
    QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});

    QVERIFY(coordinator->notifyPttReleased(15U, started.sessionId));
    QCOMPARE(coordinator->snapshot().stateMachine.state, SstvTxState::Error);
}

void TestSstvTxCoordinator::staleAndMaliciousPlaybackCallbacksFailClosed()
{
    {
        FakeHooks fake;
        auto coordinator = enabledCoordinator(fake);
        const SstvTxCoordinatorResult started = coordinator->start(
            1U, requestFor(SstvTxCoordinatorMode::MartinM1));
        QVERIFY(started.accepted);
        QVERIFY(!coordinator->notifyPlayback(
            2U, started.sessionId + 1U, {1U, false, false, {}}));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::WaitingForPtt);
        QVERIFY(coordinator->snapshot().coordinator.staleCallbacks > 0U);

        QVERIFY(coordinator->notifyPttConfirmed(3U, started.sessionId));
        QVERIFY(coordinator->tick(13U));
        QVERIFY(coordinator->notifyPlayback(
            14U, started.sessionId, {100U, false, false, {}}));
        QVERIFY(!coordinator->notifyPlayback(
            15U, started.sessionId, {99U, false, false, {}}));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::ReleasingPtt);
        QCOMPARE(coordinator->snapshot().lastOperationError,
                 SstvTxErrorCode::AudioUnderrun);
        QCOMPARE(fake.audioDetachCalls, std::uint64_t {1U});
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QVERIFY(coordinator->notifyPttReleased(16U, started.sessionId));
    }

    {
        FakeHooks fake;
        auto coordinator = enabledCoordinator(fake);
        const SstvTxCoordinatorResult started = coordinator->start(
            1U, requestFor(SstvTxCoordinatorMode::ScottieS2));
        QVERIFY(started.accepted);
        QVERIFY(coordinator->notifyPttConfirmed(2U, started.sessionId));
        QVERIFY(coordinator->tick(12U));
        const std::uint64_t total =
            coordinator->snapshot().audioPlan.totalFrames;
        QVERIFY(!coordinator->notifyPlayback(
            13U,
            started.sessionId,
            {total - 1U, true, false, {}}));
        QCOMPARE(coordinator->snapshot().stateMachine.state,
                 SstvTxState::ReleasingPtt);
        QCOMPARE(fake.pttOffCalls, std::uint64_t {1U});
        QVERIFY(coordinator->notifyPttReleased(14U, started.sessionId));
    }
}

QTEST_GUILESS_MAIN(TestSstvTxCoordinator)
#include "test_sstv_tx_coordinator.moc"
