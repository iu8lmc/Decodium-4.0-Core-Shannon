// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../src/sstv/digital/HamDrmController.h"
#include "../../src/sstv/digital/HamDrmMotCodec.h"
#include "../../src/sstv/digital/HamDrmProfileRegistry.h"
#include "../../src/sstv/digital/waveform/HamDrmWaveformAdapters.h"
#include "../../src/sstv/digital/waveform/HamDrmWaveformCodec.h"

#include <QCoreApplication>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QUrl>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace decodium::sstv;
using namespace decodium::sstv::hamdrm;
using namespace decodium::sstv::hamdrm::waveform;

namespace {

class TestFailure final : public std::runtime_error
{
public:
    explicit TestFailure(const std::string& detail)
        : std::runtime_error(detail)
    {
    }
};

void require(bool condition, const std::string& detail)
{
    if (!condition) {
        throw TestFailure(detail);
    }
}

const HamDrmProfile& shortProfile()
{
    for (const auto& profile : HamDrmProfileRegistry::all()) {
        if (profile.robustness == HamDrmRobustness::A
            && profile.occupiedBandwidth == HamDrmOccupiedBandwidth::Hz2300
            && profile.constellation == HamDrmConstellation::Qam4
            && profile.protection == HamDrmProtection::High
            && profile.interleaver == HamDrmInterleaver::Short) {
            return profile;
        }
    }
    throw TestFailure("short HAMDRM test profile is missing");
}

std::vector<std::uint8_t> makeGroup()
{
    HamDrmMotDataGroup group;
    group.kind = HamDrmMotGroupKind::Body;
    group.lastSegment = true;
    group.transportId = 0x3434U;
    group.payload.resize(37U);
    for (std::size_t index = 0U; index < group.payload.size(); ++index) {
        group.payload[index] = static_cast<std::uint8_t>(
            index * 17U + 3U);
    }
    const auto encoded = encodeHamDrmMotDataGroup(group);
    require(encoded.ok(), "test MOT group did not encode");
    return *encoded.value;
}

QUrl writeJpegFixture(const QString& path)
{
    // The controller now requires a bounded, decodable Gallery snapshot
    // before it permits waveform TX.  Use a real JPEG rather than a
    // header-only structural fixture, which belongs in the rejection tests.
    QImage image(12, 9, QImage::Format_RGB32);
    if (image.isNull()) {
        return {};
    }
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixelColor(x, y, QColor::fromRgb(
                (x * 31) % 256, (y * 43) % 256,
                ((x + y) * 17) % 256));
        }
    }
    if (!image.save(path, "JPG", 90)) {
        return {};
    }
    return QUrl::fromLocalFile(path);
}

std::vector<std::int16_t> transmitPcm12(
    const HamDrmProfile& profile,
    const std::vector<std::uint8_t>& group)
{
    HamDrmWaveformConfig config;
    config.robustness = profile.robustness;
    config.occupiedBandwidth = profile.occupiedBandwidth;
    config.constellation = profile.constellation;
    config.protection = profile.protection;
    config.interleaver = profile.interleaver;
    HamDrmWaveformTransmitter transmitter(config);
    require(transmitter.enqueueMotDataGroup(group).ok()
                && transmitter.finish().ok(),
            "test waveform transmitter setup failed");
    std::vector<std::int16_t> pcm;
    while (!transmitter.done()) {
        const std::vector<double> chunk = transmitter.pullPcm(503U);
        require(!chunk.empty(), "test waveform transmitter stalled");
        for (const double sample : chunk) {
            pcm.push_back(static_cast<std::int16_t>(std::lround(
                std::clamp(sample, -1.0, 1.0)
                * static_cast<double>(
                    std::numeric_limits<std::int16_t>::max()))));
        }
    }
    return pcm;
}

class RxSink final : public HamDrmWaveformRxSink
{
public:
    void hamDrmRxProgress(std::uint64_t, double value) override
    {
        const std::lock_guard<std::mutex> lock(mutex);
        progress = value;
    }

    void hamDrmRxMotGroup(std::uint64_t sessionId,
                          QByteArray encodedGroup) override
    {
        const std::lock_guard<std::mutex> lock(mutex);
        groupSession = sessionId;
        group = std::move(encodedGroup);
        changed.notify_all();
    }

    void hamDrmRxFinished(std::uint64_t sessionId,
                          HamDrmStatus status) override
    {
        const std::lock_guard<std::mutex> lock(mutex);
        finishSession = sessionId;
        finishStatus = std::move(status);
        finished = true;
        changed.notify_all();
    }

    bool waitForGroup(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock, timeout,
                                [this]() { return !group.isEmpty(); });
    }

    bool waitForFinish(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock, timeout,
                                [this]() { return finished; });
    }

    std::mutex mutex;
    std::condition_variable changed;
    QByteArray group;
    HamDrmStatus finishStatus;
    std::uint64_t groupSession {0U};
    std::uint64_t finishSession {0U};
    double progress {0.0};
    bool finished {false};
};

class BlockingFinishRxSink final : public HamDrmWaveformRxSink
{
public:
    void hamDrmRxProgress(std::uint64_t, double) override {}
    void hamDrmRxMotGroup(std::uint64_t, QByteArray) override {}

    void hamDrmRxFinished(std::uint64_t sessionId,
                          HamDrmStatus status) override
    {
        std::unique_lock<std::mutex> lock(mutex);
        finishSession = sessionId;
        finishStatus = std::move(status);
        entered = true;
        changed.notify_all();
        changed.wait(lock, [this]() { return releaseRequested; });
        returned = true;
        changed.notify_all();
    }

    bool waitUntilEntered(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock, timeout,
                                [this]() { return entered; });
    }

    void release()
    {
        const std::lock_guard<std::mutex> lock(mutex);
        releaseRequested = true;
        changed.notify_all();
    }

    std::mutex mutex;
    std::condition_variable changed;
    HamDrmStatus finishStatus;
    std::uint64_t finishSession {0U};
    bool entered {false};
    bool releaseRequested {false};
    bool returned {false};
};

class TxSink final : public HamDrmWaveformTxSink
{
public:
    void hamDrmTxProgress(std::uint64_t sessionId, double value) override
    {
        progressSession = sessionId;
        progress = value;
    }

    void hamDrmTxFinished(std::uint64_t sessionId,
                          HamDrmStatus status) override
    {
        finishSession = sessionId;
        finishStatus = std::move(status);
        finished = true;
    }

    HamDrmStatus finishStatus;
    std::uint64_t progressSession {0U};
    std::uint64_t finishSession {0U};
    double progress {0.0};
    bool finished {false};
};

HamDrmEncodedObject makeObject()
{
    HamDrmMotObjectMetadata metadata;
    metadata.transportId = 0x4545U;
    metadata.filename = "adapter.jpg";
    std::vector<std::uint8_t> body(113U, 0x5aU);
    const auto encoded = encodeHamDrmObject(metadata, body, 47U);
    require(encoded.ok(), "test MOT object did not encode");
    return std::move(*encoded.value);
}

class FixedSource final : public SstvPcm16Source
{
public:
    explicit FixedSource(std::uint64_t samples)
        : total(samples)
    {
    }

    std::uint32_t sampleRate() const noexcept override { return 48'000U; }
    std::uint64_t totalSamples() const noexcept override { return total; }
    std::uint64_t producedSamples() const noexcept override { return produced; }
    bool complete() const noexcept override { return produced == total; }
    bool cancelled() const noexcept override { return cancelledFlag.load(); }

    std::size_t pullPcm16(std::int16_t* output,
                          std::size_t capacity) override
    {
        if (capacity != 0U && output == nullptr) {
            throw std::invalid_argument("null fixed-source output");
        }
        const std::size_t count = static_cast<std::size_t>(
            std::min<std::uint64_t>(capacity, total - produced));
        std::fill_n(output, count, static_cast<std::int16_t>(123));
        produced += count;
        return count;
    }

    void cancel() noexcept override { cancelledFlag.store(true); }
    void reset() override
    {
        produced = 0U;
        cancelledFlag.store(false);
    }

private:
    std::uint64_t total {0U};
    std::uint64_t produced {0U};
    std::atomic_bool cancelledFlag {false};
};

void testAdapterConfigurationAndChunkHardLimits()
{
    HamDrmNativeAdapterLimits unbounded;
    unbounded.maximumTxQueuedBytes =
        std::numeric_limits<std::size_t>::max();
    bool rejectedUnboundedConfiguration = false;
    try {
        HamDrmNativeRxBackend backend({}, unbounded);
    } catch (const std::invalid_argument&) {
        rejectedUnboundedConfiguration = true;
    }
    require(rejectedUnboundedConfiguration,
            "RX adapter accepted a configuration above its hard limits");

    HamDrmNativeAdapterLimits tiny;
    tiny.maximumRxQueuedChunks = 1U;
    tiny.maximumRxQueuedSamples = 4U;
    tiny.maximumRxChunkSamples = 8U;
    HamDrmNativeRxHooks hooks;
    hooks.activateSharedAudioTap = []() {
        return HamDrmStatus::success();
    };
    hooks.deactivateSharedAudioTap = []() {};
    HamDrmNativeRxBackend backend(std::move(hooks), tiny);
    RxSink sink;
    require(backend.start(shortProfile(), 10U, sink).ok(),
            "bounded RX adapter did not start");
    const std::array<std::int16_t, 5U> oversized {{0, 0, 0, 0, 0}};
    require(!backend.submitPcm16(oversized.data(), oversized.size(), 12'000U),
            "RX adapter accepted a chunk above its total queue capacity");
    require(sink.waitForFinish(std::chrono::seconds(5))
                && sink.finishSession == 10U
                && sink.finishStatus.code == HamDrmErrorCode::InvalidArgument,
            "RX adapter did not fail closed before queuing an oversized chunk");
    backend.shutdown();
}

void testRxAdapterLoopbackAndBounds()
{
    std::atomic<unsigned> activations {0U};
    std::atomic<unsigned> deactivations {0U};
    HamDrmNativeRxHooks hooks;
    hooks.activateSharedAudioTap = [&activations]() {
        ++activations;
        return HamDrmStatus::success();
    };
    hooks.deactivateSharedAudioTap = [&deactivations]() {
        ++deactivations;
    };
    HamDrmNativeRxBackend backend(std::move(hooks));
    require(backend.capability().completeBackend,
            "RX adapter did not advertise its connected backend");

    RxSink sink;
    const auto& profile = shortProfile();
    const auto group = makeGroup();
    const auto pcm = transmitPcm12(profile, group);
    require(backend.start(profile, 11U, sink).ok(),
            "RX adapter did not start");
    for (std::size_t offset = 0U; offset < pcm.size();) {
        const std::size_t count = std::min<std::size_t>(677U,
                                                        pcm.size() - offset);
        require(backend.submitPcm16(pcm.data() + offset, count, 12'000U),
                "RX adapter rejected bounded PCM");
        offset += count;
    }
    const std::vector<std::int16_t> acquisitionTail(2'048U, 0);
    require(backend.submitPcm16(acquisitionTail.data(),
                                acquisitionTail.size(), 12'000U),
            "RX adapter rejected the acquisition tail");
    require(sink.waitForGroup(std::chrono::seconds(5)),
            "RX adapter did not deliver the decoded MOT group");
    require(sink.groupSession == 11U
                && static_cast<std::size_t>(sink.group.size()) == group.size()
                && std::memcmp(sink.group.constData(), group.data(),
                               group.size()) == 0,
            "RX adapter delivered different MOT bytes");
    backend.resetAudioStream();
    backend.cancel(11U);
    require(!backend.active() && activations.load() == 1U
                && deactivations.load() == 1U,
            "RX adapter cancellation did not detach the shared tap");
    require(backend.statistics().streamResets == 1U,
            "RX adapter did not record the stream reset");

    RxSink invalidSink;
    require(backend.start(profile, 12U, invalidSink).ok(),
            "RX adapter did not restart");
    const std::int16_t invalidPcm = 0;
    require(!backend.submitPcm16(&invalidPcm, 1U, 12'345U),
            "RX adapter accepted an unsupported sample rate");
    require(invalidSink.waitForFinish(std::chrono::seconds(5))
                && invalidSink.finishSession == 12U
                && invalidSink.finishStatus.code
                    == HamDrmErrorCode::InvalidArgument,
            "RX adapter did not fail closed on invalid PCM");

    BlockingFinishRxSink blockingSink;
    require(backend.start(profile, 13U, blockingSink).ok(),
            "RX adapter did not restart for cancellation ordering");
    require(!backend.submitPcm16(&invalidPcm, 1U, 12'345U)
                && blockingSink.waitUntilEntered(std::chrono::seconds(5)),
            "RX adapter did not enter its terminal callback");
    std::atomic_bool cancelReturned {false};
    std::thread cancellation([&backend, &cancelReturned]() {
        backend.cancel(13U);
        cancelReturned.store(true, std::memory_order_release);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    require(!cancelReturned.load(std::memory_order_acquire),
            "RX cancel returned while a backend callback was still active");
    blockingSink.release();
    cancellation.join();
    require(cancelReturned.load(std::memory_order_acquire)
                && blockingSink.returned
                && blockingSink.finishSession == 13U
                && blockingSink.finishStatus.code
                    == HamDrmErrorCode::InvalidArgument,
            "RX cancel did not synchronously drain its terminal callback");
    backend.shutdown();
}

void testTxAdapterPreparedSourceAndCompletion()
{
    std::unique_ptr<SstvPcm16Source> captured;
    std::string capturedMode;
    std::uint64_t nextCoordinatorSession = 71U;
    std::uint64_t cancelledSession = 0U;
    HamDrmNativeTxHooks hooks;
    hooks.configuredCallsign = []() { return std::string("9H1TEST"); };
    hooks.startPreparedAudio = [&captured, &capturedMode,
                                &nextCoordinatorSession](
        std::unique_ptr<SstvPcm16Source> source,
        std::string mode) {
            captured = std::move(source);
            capturedMode = std::move(mode);
            return SstvTxCoordinatorResult {
                true, SstvTxErrorCode::None, {}, nextCoordinatorSession++};
        };
    hooks.cancelAudio = [&cancelledSession](std::uint64_t sessionId) {
        cancelledSession = sessionId;
        return true;
    };
    HamDrmNativeTxBackend backend(std::move(hooks));
    require(backend.capability().completeBackend,
            "TX adapter did not advertise its connected backend");

    TxSink sink;
    require(backend.start(shortProfile(), makeObject(), 21U, sink).ok(),
            "TX adapter rejected a bounded MOT object");
    require(captured && captured->sampleRate() == 48'000U
                && captured->totalSamples()
                    % kHamDrmNativePcmSamplesPerFrame == 0U
                && capturedMode.find("HAMDRM ") == 0U,
            "TX adapter did not build native 48 kHz prepared PCM");
    std::array<std::int16_t, 1'777U> pcm {};
    std::uint64_t drained = 0U;
    const auto drainDeadline = std::chrono::steady_clock::now()
        + std::chrono::seconds(10);
    while (!captured->complete()) {
        const std::size_t count = captured->pullPcm16(pcm.data(), pcm.size());
        if (count == 0U && !captured->complete()) {
            require(std::chrono::steady_clock::now() < drainDeadline,
                    "TX prepared PCM worker missed its bounded drain deadline");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        drained += count;
    }
    require(drained == captured->totalSamples(),
            "TX prepared PCM source violated its declared length");
    captured->reset();
    require(captured->producedSamples() == 0U && !captured->complete()
                && !captured->cancelled()
                && captured->pullPcm16(pcm.data(), pcm.size()) == pcm.size(),
            "TX prepared PCM source did not rebuild its bounded prebuffer");

    SstvTxCoordinatorSnapshot progress;
    progress.stateMachine.currentSessionId = 71U;
    progress.stateMachine.state = SstvTxState::TransmittingImage;
    progress.progress = 0.625;
    backend.coordinatorStateChanged(progress);
    require(sink.progressSession == 21U && sink.progress == 0.625,
            "TX adapter did not forward played-audio progress");
    progress.stateMachine.state = SstvTxState::Completed;
    progress.progress = 1.0;
    backend.coordinatorStateChanged(progress);
    require(sink.finished && sink.finishSession == 21U
                && sink.finishStatus.ok() && !backend.active(),
            "TX adapter did not forward coordinated completion");

    TxSink cancelledSink;
    require(backend.start(shortProfile(), makeObject(),
                          22U, cancelledSink).ok(),
            "TX adapter did not accept a second job");
    backend.cancel(22U);
    require(cancelledSession == 72U && !backend.active()
                && !cancelledSink.finished,
            "TX adapter cancellation did not detach synchronously");

    bool invalidStarted = false;
    HamDrmNativeTxHooks invalidHooks;
    invalidHooks.configuredCallsign = []() { return std::string {}; };
    invalidHooks.startPreparedAudio = [&invalidStarted](
        std::unique_ptr<SstvPcm16Source>, std::string) {
            invalidStarted = true;
            return SstvTxCoordinatorResult {
                true, SstvTxErrorCode::None, {}, 73U};
        };
    invalidHooks.cancelAudio = [](std::uint64_t) { return true; };
    HamDrmNativeTxBackend invalidBackend(std::move(invalidHooks));
    TxSink invalidSink;
    const HamDrmStatus invalid = invalidBackend.start(
        shortProfile(), makeObject(), 23U, invalidSink);
    require(invalid.code == HamDrmErrorCode::InvalidArgument
                && !invalidStarted && !invalidBackend.active(),
            "TX adapter did not fail closed on a missing FAC callsign");
}

void testPreparedCoordinatorUsesSharedAuthority()
{
    std::shared_ptr<SstvTxAudioDevice> attached;
    SstvTxAudioPlan attachedPlan;
    unsigned pttOn = 0U;
    unsigned pttOff = 0U;
    unsigned detaches = 0U;
    SstvTxCoordinatorHooks hooks;
    hooks.queryPreflight = []() {
        SstvTxCoordinatorPreflight result;
        result.audioOutputReady = true;
        result.pttPathReady = true;
        result.pttReleaseRequired = true;
        return result;
    };
    hooks.requestPttOn = [&pttOn](std::uint64_t) {
        ++pttOn;
        return true;
    };
    hooks.requestPttOff = [&pttOff](std::uint64_t) {
        ++pttOff;
        return true;
    };
    hooks.startAudio = [&attached, &attachedPlan](
        const std::shared_ptr<SstvTxAudioDevice>& device,
        const SstvTxAudioPlan& plan) {
            attached = device;
            attachedPlan = plan;
            return true;
        };
    hooks.detachAudio = [&detaches](
        const std::shared_ptr<SstvTxAudioDevice>&,
        SstvTxAudioDetachReason) {
            ++detaches;
            return true;
        };
    SstvTxCoordinatorConfig config;
    config.pttLeadDelayMs = 0U;
    config.pttTailDelayMs = 0U;
    SstvTxCoordinator coordinator(config, std::move(hooks));
    require(coordinator.enable(0U).accepted,
            "prepared-source coordinator did not enable");
    SstvTxPreparedAudioRequest request;
    request.source = std::make_unique<FixedSource>(4'800U);
    request.mode = "HAMDRM-test";
    request.headerEndFrame = 0U;
    request.imageEndFrame = 4'800U;
    const auto started = coordinator.startPrepared(0U, std::move(request));
    require(started.accepted && started.sessionId != 0U && pttOn == 1U,
            "prepared source bypassed or failed shared PTT preflight");
    require(coordinator.notifyPttConfirmed(0U, started.sessionId)
                && attached && attachedPlan.protocolEndFrame == 4'800U,
            "prepared source did not reach shared SoundOutput hook");

    std::vector<char> bytes(static_cast<std::size_t>(attached->size()));
    qint64 read = 0;
    while (read < attached->size()) {
        const qint64 count = attached->read(
            bytes.data() + read, attached->size() - read);
        require(count > 0, "prepared coordinator audio device stalled");
        read += count;
    }
    SstvTxPlaybackProgress playback;
    playback.playedFrames = attachedPlan.totalFrames;
    playback.playbackComplete = true;
    require(coordinator.notifyPlayback(1U, started.sessionId, playback),
            "prepared coordinator rejected drained playback");
    require(coordinator.tick(1U) && pttOff == 1U
                && coordinator.notifyPttReleased(1U, started.sessionId),
            "prepared coordinator did not enforce the PTT release barrier");
    require(coordinator.snapshot().stateMachine.state
                == SstvTxState::Completed
                && detaches == 1U,
            "prepared coordinator did not complete through shared authority");
}

void testControllerUsesNativeAdapters()
{
    HamDrmNativeRxHooks rxHooks;
    rxHooks.activateSharedAudioTap = []() {
        return HamDrmStatus::success();
    };
    rxHooks.deactivateSharedAudioTap = []() {};
    auto rx = std::make_shared<HamDrmNativeRxBackend>(
        std::move(rxHooks));

    std::unique_ptr<SstvPcm16Source> captured;
    HamDrmNativeTxHooks txHooks;
    txHooks.configuredCallsign = []() { return std::string("9H1TEST"); };
    txHooks.startPreparedAudio = [&captured](
        std::unique_ptr<SstvPcm16Source> source, std::string) {
            captured = std::move(source);
            return SstvTxCoordinatorResult {
                true, SstvTxErrorCode::None, {}, 88U};
        };
    txHooks.cancelAudio = [](std::uint64_t) { return true; };
    auto tx = std::make_shared<HamDrmNativeTxBackend>(
        std::move(txHooks));

    HamDrmControllerBackends backends;
    backends.waveformRx = rx;
    backends.waveformTx = tx;
    HamDrmController controller({}, std::move(backends));
    require(controller.waveformRxAvailable()
                && controller.waveformTxAvailable()
                && controller.capabilities().value(
                    QStringLiteral("waveformRx")).toBool()
                && controller.capabilities().value(
                    QStringLiteral("waveformTx")).toBool(),
            "controller did not expose the real native capabilities");
    require(controller.startRx() && controller.cancelRx(),
            "controller did not drive the native RX adapter lifecycle");

    QTemporaryDir temporary;
    require(temporary.isValid(),
            "controller adapter fixture directory is invalid");
    const QUrl image = writeJpegFixture(
        temporary.filePath(QStringLiteral("native-adapter.jpg")));
    require(image.isValid() && controller.startTx(image),
            "controller did not start native MOT-to-PCM TX");
    require(captured && captured->sampleRate() == 48'000U
                && controller.txState()
                    == HamDrmController::OperationState::Active,
            "controller did not hand native PCM to the shared TX authority");
    SstvTxCoordinatorSnapshot progress;
    progress.stateMachine.currentSessionId = 88U;
    progress.stateMachine.state = SstvTxState::TransmittingImage;
    progress.progress = 0.5;
    tx->coordinatorStateChanged(progress);
    require(controller.txProgress() == 0.5,
            "controller did not observe shared-audio TX progress");
    progress.stateMachine.state = SstvTxState::Completed;
    progress.progress = 1.0;
    tx->coordinatorStateChanged(progress);
    require(controller.txState()
                == HamDrmController::OperationState::Completed
                && controller.txProgress() == 1.0 && !controller.busy(),
            "controller did not wait for coordinated TX completion");
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    const std::array<std::pair<const char*, void (*)()>, 5U> tests {{
        {"adapter configuration/chunk hard limits",
         testAdapterConfigurationAndChunkHardLimits},
        {"RX adapter loopback/bounds", testRxAdapterLoopbackAndBounds},
        {"TX adapter source/completion", testTxAdapterPreparedSourceAndCompletion},
        {"prepared coordinator authority", testPreparedCoordinatorUsesSharedAuthority},
        {"controller native adapters", testControllerUsesNativeAdapters},
    }};
    try {
        for (const auto& test : tests) {
            test.second();
            std::cout << "PASS: " << test.first << '\n';
        }
    } catch (const std::exception& exception) {
        std::cerr << "FAIL: " << exception.what() << '\n';
        return 1;
    }
    return 0;
}
