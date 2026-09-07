// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvTxAudioDevice.h"

#include "../analog/SstvAvt.h"
#include "../analog/SstvMartinM1.h"
#include "../analog/SstvMmsstvExtended.h"
#include "../analog/SstvPd.h"
#include "../analog/SstvRobot.h"
#include "../analog/SstvScottie.h"
#include "../analog/SstvSequentialRgb.h"
#include "../core/SstvFskIdCodec.h"
#include "../tx/SstvTxStateMachine.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class QImage;

namespace decodium::sstv {

enum class SstvTxCoordinatorMode : std::uint8_t
{
    MartinM1,
    MartinM2,
    MartinM3,
    MartinM4,
    ScottieS1,
    ScottieS2,
    ScottieS3,
    ScottieS4,
    ScottieDx,
    RobotColour12,
    RobotColour24,
    RobotColour36,
    RobotColour72,
    RobotBw8,
    RobotBw12,
    RobotBw24,
    RobotBw36,
    WraaseSc2_60,
    WraaseSc2_120,
    WraaseSc2_180,
    PasokonP3,
    PasokonP5,
    PasokonP7,
    Pd50,
    Pd90,
    Pd120,
    Pd160,
    Pd180,
    Pd240,
    Pd290,
    Avt24,
    Avt90,
    Avt94,
    Mp73,
    Mp115,
    Mp140,
    Mp175,
    Mr73,
    Mr90,
    Mr115,
    Mr140,
    Mr175,
    Ml180,
    Ml240,
    Ml280,
    Ml320,
    Mp73Narrow,
    Mp110Narrow,
    Mp140Narrow,
    Mc110Narrow,
    Mc140Narrow,
    Mc180Narrow,
};

enum class SstvTxAudioDetachReason : std::uint8_t
{
    Completed,
    Cancelled,
    Error,
    Shutdown,
};

struct SstvTxFskIdPlan final
{
    std::string text;
    SstvFskIdCodec::TextPolicy textPolicy {
        SstvFskIdCodec::TextPolicy::Callsign};
    SstvFskIdCodec::InputHandling inputHandling {
        SstvFskIdCodec::InputHandling::Strict};
};

struct SstvTxSourceBuilderConfig final
{
    SstvTxCoordinatorMode mode {SstvTxCoordinatorMode::MartinM1};
    std::uint32_t sampleRate {48'000U};
    std::int32_t clockErrorPpm {0};
    double level {1.0};
    double headroom {kDefaultSstvTxHeadroom};
    std::optional<SstvTxFskIdPlan> fskId;
};

struct SstvTxBuiltSource final
{
    std::unique_ptr<SstvPcm16Source> source;
    std::string mode;
    std::uint32_t sampleRate {0U};
    std::uint32_t width {0U};
    std::uint32_t height {0U};
    std::uint64_t headerFrames {0U};
    std::uint64_t imageEndFrame {0U};
    std::uint64_t totalFrames {0U};
    std::uint64_t fskIdFrames {0U};
    double headroom {kDefaultSstvTxHeadroom};
    bool fskIdPlanned {false};
};

// Shared bounded source builder for live SoundOutput and atomic WAV export.
// It owns one RGB frame through the native encoder and at most the bounded FSK
// symbol plan; it never renders a complete PCM waveform.  The QImage overload
// performs strict selected-mode RGB8 geometry validation/normalization.
class SstvTxSourceBuilder final
{
public:
    SstvTxSourceBuilder() = delete;

    static const char* modeId(SstvTxCoordinatorMode mode) noexcept;
    static std::optional<SstvTxCoordinatorMode> modeFromId(
        std::string_view modeId) noexcept;
    static std::vector<SstvRgbPixel> pixelsFromImage(const QImage& image);
    static std::vector<SstvRgbPixel> pixelsFromImage(
        const QImage& image,
        SstvTxCoordinatorMode mode);
    static SstvTxBuiltSource build(
        const std::vector<SstvRgbPixel>& pixels,
        const SstvTxSourceBuilderConfig& config);
    static SstvTxBuiltSource build(
        const QImage& image,
        const SstvTxSourceBuilderConfig& config);
};

struct SstvTxCoordinatorRequest final
{
    SstvTxCoordinatorMode mode {SstvTxCoordinatorMode::MartinM1};
    std::vector<SstvRgbPixel> pixels;
    unsigned channelCount {1U};
    SstvTxChannelRoute channelRoute {SstvTxChannelRoute::Both};
    std::optional<SstvTxFskIdPlan> fskId;
};

// A protocol encoder outside the analog SSTV mode builders can submit its
// bounded pull source through the same preflight, CAT/PTT, SoundOutput lease,
// progress and shutdown authority.  Phase boundaries are relative to the
// protocol source before any live-only VOX envelope is added.  The source is
// consumed only after preflight succeeds.
struct SstvTxPreparedAudioRequest final
{
    std::unique_ptr<SstvPcm16Source> source;
    std::string mode;
    std::uint32_t width {1U};
    std::uint32_t height {1U};
    unsigned channelCount {1U};
    SstvTxChannelRoute channelRoute {SstvTxChannelRoute::Both};
    std::uint64_t headerEndFrame {0U};
    std::uint64_t imageEndFrame {0U};
    double headroom {kDefaultSstvTxHeadroom};
};

// This snapshot is supplied by Decodium immediately before a job is
// accepted.  The coordinator deliberately does not own a CAT backend,
// SoundOutput, sequencer, or global TX flag; the existing bridge remains the
// single authority for those resources.
struct SstvTxCoordinatorPreflight final
{
    bool audioOutputReady {false};
    bool pttPathReady {false};
    bool weakSignalSequencerActive {false};
    bool transmitAlreadyActive {false};

    // CAT/DTR/RTS/remote paths set this true.  VOX/audio-activity policies set
    // it false, but still provide requestPttOn and an explicit confirmation.
    bool pttReleaseRequired {true};
    std::string detail;
};

struct SstvTxCoordinatorConfig final
{
    std::uint32_t sampleRate {48'000U};
    std::uint64_t pttLeadDelayMs {100U};
    std::uint64_t pttTailDelayMs {250U};
    // VOX cannot be confirmed by CAT feedback.  Its policy-approved PTT
    // confirmation therefore starts a real, bounded audio envelope: a tone
    // before the SSTV header gives the radio time to key, and a tone after the
    // protocol payload keeps the gate open while the final samples drain.
    // These samples are live-output only; protocol WAV export remains clean.
    std::uint64_t voxPreKeyMs {750U};
    std::uint64_t voxHangMs {500U};
    double voxToneFrequencyHz {1'900.0};
    double voxToneLevel {0.5};
    // Audio-only VOX normally carries a pre-key/hang envelope so a real
    // transmitter opens reliably.  The bounded lab loopback can disable that
    // envelope explicitly to keep the 1900 Hz keying tone out of RX VIS/DSP.
    bool voxEnvelopeEnabled {true};
    // A dispatched OFF command is not proof that RF has stopped.  Retry the
    // existing Decodium PTT adapter at this bounded cadence until its feedback
    // confirms release; the release barrier remains closed in the meantime.
    std::uint64_t pttReleaseRetryMs {500U};
    SstvTxPolicy stateMachinePolicy;
};

// Runtime-adjustable subset of the coordinator policy.  Applying it is
// permitted only with no active lifecycle or retained audio lease, so one TX
// session can never change its lead/tail or VOX frame plan underneath the
// SoundOutput tracker.
struct SstvTxTimingConfig final
{
    std::uint64_t pttLeadDelayMs {100U};
    std::uint64_t pttTailDelayMs {250U};
    std::uint64_t pttReleaseRetryMs {500U};
    std::uint64_t voxPreKeyMs {750U};
    std::uint64_t voxHangMs {500U};
    double voxToneFrequencyHz {1'900.0};
    double voxToneLevel {0.5};
};

struct SstvTxAudioPlan final
{
    std::uint64_t sessionId {0U};
    std::string mode;
    std::uint32_t sampleRate {0U};
    unsigned channelCount {0U};
    SstvTxChannelRoute channelRoute {SstvTxChannelRoute::Both};
    // All phase boundaries are absolute frames from the live audio stream.
    // protocolStartFrame/protocolEndFrame exclude the VOX envelope; the
    // header and image boundaries include any leading VOX frames.
    std::uint64_t protocolStartFrame {0U};
    std::uint64_t headerFrames {0U};
    std::uint64_t imageEndFrame {0U};
    std::uint64_t protocolEndFrame {0U};
    std::uint64_t totalFrames {0U};
    std::uint64_t fskIdFrames {0U};
    std::uint64_t voxPreKeyFrames {0U};
    std::uint64_t voxHangFrames {0U};
    double headroom {kDefaultSstvTxHeadroom};
    bool fskIdPlanned {false};
    bool voxEnvelopeEnabled {false};
};

struct SstvTxPlaybackProgress final
{
    // Frames confirmed as played/on-air by the existing output path.  Source
    // frames merely pulled into an OS buffer must not be reported here.
    std::uint64_t playedFrames {0U};
    bool playbackComplete {false};
    bool failed {false};
    std::string detail;
};

struct SstvTxCoordinatorMetrics final
{
    std::uint64_t startCalls {0U};
    std::uint64_t jobsAccepted {0U};
    std::uint64_t preflightRejections {0U};
    std::uint64_t staleCallbacks {0U};
    std::uint64_t playbackUpdates {0U};
    std::uint64_t audioStartAttempts {0U};
    std::uint64_t audioDetachAttempts {0U};
    std::uint64_t pttOnAttempts {0U};
    std::uint64_t pttOffAttempts {0U};
    std::uint64_t hookFailures {0U};
};

struct SstvTxCoordinatorSnapshot final
{
    SstvTxMetrics stateMachine;
    SstvTxCoordinatorMetrics coordinator;
    SstvTxAudioPlan audioPlan;
    SstvTxErrorCode lastOperationError {SstvTxErrorCode::None};
    std::string lastOperationDetail;
    std::uint64_t playedFrames {0U};
    std::uint64_t protocolPlayedFrames {0U};
    double progress {0.0};
    double pcmPeak {0.0};
    double headroom {kDefaultSstvTxHeadroom};
    std::uint64_t clippedFrames {0U};
    bool audioStartAttempted {false};
    bool audioAttached {false};
    bool audioDetachPending {false};
    bool audioLeaseRetained {false};
    bool pttOnAttempted {false};
    bool pttOffAttempted {false};
    bool pttReleased {false};
    bool fskIdPlanned {false};
    bool fskIdOnAir {false};
    bool fskIdCompleted {false};
};

struct SstvTxCoordinatorResult final
{
    bool accepted {false};
    SstvTxErrorCode error {SstvTxErrorCode::None};
    std::string detail;
    std::uint64_t sessionId {0U};
};

struct SstvTxCoordinatorHooks final
{
    std::function<SstvTxCoordinatorPreflight()> queryPreflight;

    // These hooks adapt to Decodium's existing PTT transition policy.  A
    // successful requestPttOn only means that the request was dispatched;
    // notifyPttConfirmed remains mandatory before audio can start.
    std::function<bool(std::uint64_t sessionId)> requestPttOn;
    std::function<bool(std::uint64_t sessionId)> requestPttOff;

    // startAudio must pass device.get() to the existing SoundOutput.  The
    // coordinator keeps the shared lease.  detachAudio returns true only once
    // SoundOutput has synchronously stopped pulling that device.  If it
    // returns false, the lease is retained until notifyAudioDetached().
    std::function<bool(
        const std::shared_ptr<SstvTxAudioDevice>& device,
        const SstvTxAudioPlan& plan)> startAudio;
    std::function<bool(
        const std::shared_ptr<SstvTxAudioDevice>& device,
        SstvTxAudioDetachReason reason)> detachAudio;

    // Optional owner-thread observer.  Exceptions are contained and never
    // alter the TX lifecycle.
    std::function<void(const SstvTxCoordinatorSnapshot&)> stateChanged;
};

// Owner-thread coordinator for native SSTV TX.  All public lifecycle methods
// and hook callbacks must be marshalled to one monotonically-clocked owner
// thread.  Only SstvTxAudioDevice::cancel() crosses into the SoundOutput pull
// path, using its documented concurrent cancellation contract.
class SstvTxCoordinator final
{
public:
    explicit SstvTxCoordinator(SstvTxCoordinatorConfig config,
                               SstvTxCoordinatorHooks hooks);
    ~SstvTxCoordinator();

    SstvTxCoordinator(const SstvTxCoordinator&) = delete;
    SstvTxCoordinator& operator=(const SstvTxCoordinator&) = delete;

    SstvTxCoordinatorResult enable(std::uint64_t nowMs);
    bool updateTimingConfig(const SstvTxTimingConfig& timing);
    SstvTxCoordinatorResult start(
        std::uint64_t nowMs,
        const SstvTxCoordinatorRequest& request);
    SstvTxCoordinatorResult startPrepared(
        std::uint64_t nowMs,
        SstvTxPreparedAudioRequest request);

    bool notifyPttConfirmed(std::uint64_t nowMs,
                            std::uint64_t sessionId);
    bool notifyPlayback(std::uint64_t nowMs,
                        std::uint64_t sessionId,
                        const SstvTxPlaybackProgress& progress);
    bool notifyAudioError(std::uint64_t nowMs,
                          std::uint64_t sessionId,
                          std::string detail);
    bool notifyAudioUnderrun(std::uint64_t nowMs,
                             std::uint64_t sessionId,
                             std::string detail);
    bool notifyAudioDetached(std::uint64_t nowMs,
                             std::uint64_t sessionId);
    bool notifyPttReleased(std::uint64_t nowMs,
                           std::uint64_t sessionId);

    bool cancel(std::uint64_t nowMs);
    bool tick(std::uint64_t nowMs);
    bool shutdown(std::uint64_t nowMs);

    SstvTxCoordinatorSnapshot snapshot() const;
    const SstvTxStateMachine& stateMachine() const noexcept;

    static const char* modeId(SstvTxCoordinatorMode mode) noexcept;

private:
    struct BuiltAudio final
    {
        std::shared_ptr<SstvTxAudioDevice> device;
        SstvTxAudioPlan plan;
    };

    static void validateConfig(const SstvTxCoordinatorConfig& config,
                               const SstvTxCoordinatorHooks& hooks);
    static void saturatingAdd(std::uint64_t& value) noexcept;

    SstvTxCoordinatorResult rejectPreflight(
        SstvTxErrorCode error,
        std::string detail);
    SstvTxCoordinatorResult rejectStart(
        SstvTxErrorCode error,
        std::string detail,
        std::string_view mode);
    SstvTxCoordinatorResult acceptedResult() const;
    BuiltAudio buildAudio(const SstvTxCoordinatorRequest& request,
                          bool voxAudioActivation) const;
    BuiltAudio buildPreparedAudio(SstvTxPreparedAudioRequest& request,
                                  bool voxAudioActivation) const;
    bool validateRequest(const SstvTxCoordinatorRequest& request,
                         SstvTxErrorCode& error,
                         std::string& detail) const;
    bool validatePreparedRequest(
        const SstvTxPreparedAudioRequest& request,
        SstvTxErrorCode& error,
        std::string& detail) const;
    SstvTxCoordinatorResult startValidated(
        std::uint64_t nowMs,
        std::uint32_t width,
        std::uint32_t height,
        std::string diagnosticMode,
        const std::function<BuiltAudio(bool)>& build);
    bool dispatch(std::uint64_t nowMs, const SstvTxEvent& event);
    bool sessionMatches(std::uint64_t sessionId) noexcept;
    bool startAudio(std::uint64_t nowMs);
    bool detachAudio(std::uint64_t nowMs,
                     SstvTxAudioDetachReason reason);
    void releaseAudioLease() noexcept;
    void requestPttRelease(std::uint64_t nowMs);
    bool fail(std::uint64_t nowMs,
              SstvTxErrorCode error,
              std::string detail,
              SstvTxAudioDetachReason detachReason);
    void recordFailureDiagnostic(std::uint64_t nowMs,
                                 SstvTxErrorCode error) noexcept;
    std::uint64_t diagnosticDurationMs(std::uint64_t nowMs) const noexcept;
    void clearForNewSession(BuiltAudio audio);
    void publishState() noexcept;
    std::string boundedDetail(std::string detail) const;
    void emergencyShutdown() noexcept;

    SstvTxCoordinatorConfig config_;
    SstvTxCoordinatorHooks hooks_;
    SstvTxStateMachine stateMachine_;
    SstvTxCoordinatorMetrics metrics_;
    SstvTxAudioPlan audioPlan_;
    std::shared_ptr<SstvTxAudioDevice> audioDevice_;
    SstvTxErrorCode lastOperationError_ {SstvTxErrorCode::None};
    std::string lastOperationDetail_;
    std::uint64_t activeSessionId_ {0U};
    std::uint64_t pttConfirmedAtMs_ {0U};
    std::uint64_t pttOffLastAttemptAtMs_ {0U};
    std::uint64_t playedFrames_ {0U};
    std::uint64_t diagnosticSessionStartedAtMs_ {0U};
    double lastPcmPeak_ {0.0};
    std::uint64_t lastClippedFrames_ {0U};
    bool pttReleaseRequired_ {true};
    bool pttOnAttempted_ {false};
    bool pttOffAttempted_ {false};
    bool pttReleased_ {false};
    bool audioStartAttempted_ {false};
    bool audioAttached_ {false};
    bool audioDetachAttempted_ {false};
    bool audioDetachPending_ {false};
    bool playbackComplete_ {false};
    bool fskIdStarted_ {false};
    bool fskIdCompleted_ {false};
    bool diagnosticFailureRecorded_ {false};
    bool destroying_ {false};
};

} // namespace decodium::sstv
