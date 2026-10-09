// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

namespace decodium::sstv {

enum class SstvTxState : std::uint8_t
{
    Disabled,
    Idle,
    PreparingImage,
    Encoding,
    Ready,
    RequestingPtt,
    WaitingForPtt,
    TransmittingHeader,
    TransmittingImage,
    TransmittingFskId,
    TailDelay,
    ReleasingPtt,
    Completed,
    Cancelled,
    Error,
};

enum class SstvTxCause : std::uint8_t
{
    None,
    Enabled,
    Disabled,
    Reset,
    AutoReset,
    PreparationStarted,
    ImagePrepared,
    EncodingComplete,
    TransmissionRequested,
    PttRequestDispatched,
    PttConfirmed,
    LeadDelayElapsed,
    HeaderComplete,
    ImageComplete,
    FskIdComplete,
    TailDelayElapsed,
    PttReleased,
    Completed,
    Cancelled,
    Failure,
    Timeout,
    ClockRegression,
    UnexpectedEvent,
    InvalidInput,
};

enum class SstvTxErrorCode : std::uint8_t
{
    None,
    InvalidImage,
    UnsupportedMode,
    EncodingFailure,
    TxNotPermitted,
    TxBusy,
    PttDispatchFailure,
    PttTimeout,
    AudioDeviceLoss,
    AudioUnderrun,
    RadioDisconnected,
    WatchdogExpired,
    Shutdown,
    ClockRegression,
    InternalFailure,
};

struct SstvTxTimeoutPolicy final
{
    std::uint64_t preparingImageMs {30'000U};
    std::uint64_t encodingMs {30'000U};
    std::uint64_t requestingPttMs {5'000U};
    std::uint64_t waitingForPttMs {10'000U};
    std::uint64_t transmittingHeaderMs {5'000U};
    std::uint64_t transmittingImageSlackMs {15'000U};
    std::uint64_t transmittingFskIdMs {30'000U};
    std::uint64_t tailDelayMs {10'000U};
    std::uint64_t releasingPttMs {10'000U};
};

struct SstvTxPolicy final
{
    SstvTxTimeoutPolicy timeouts;
    std::uint64_t maximumEncodedSamples {2'073'600'000U};
    std::uint32_t minimumSampleRate {8'000U};
    std::uint32_t maximumSampleRate {384'000U};
    std::uint32_t maximumImageDimension {8'192U};
    std::size_t maximumModeCharacters {64U};
    std::size_t maximumErrorCharacters {512U};
};

struct SstvTxEnable final {};
struct SstvTxDisable final {};
struct SstvTxReset final {};
struct SstvTxTick final {};

struct SstvTxPrepare final
{
    std::string mode;
    std::uint32_t width {0U};
    std::uint32_t height {0U};
};

struct SstvTxImagePrepared final {};

struct SstvTxEncodingComplete final
{
    std::uint64_t totalSamples {0U};
    std::uint32_t sampleRate {0U};
    bool fskIdEnabled {false};
};

struct SstvTxRequest final {};

// releaseRequired is true after a CAT/DTR/RTS/remote PTT request may have
// reached a radio.  From that instant every terminal path is forced through
// ReleasingPtt until SstvTxPttReleased is observed.  VOX/audio-only policies
// set it false but still use the same explicit coordinator sequence.
struct SstvTxPttRequestDispatched final
{
    bool releaseRequired {true};
};

struct SstvTxPttConfirmed final {};
struct SstvTxLeadElapsed final {};
struct SstvTxHeaderComplete final {};
struct SstvTxImageComplete final {};
struct SstvTxFskIdComplete final {};
struct SstvTxTailElapsed final {};
struct SstvTxPttReleased final {};
struct SstvTxCancel final {};

struct SstvTxFailure final
{
    SstvTxErrorCode code {SstvTxErrorCode::InternalFailure};
    std::string detail;
};

using SstvTxEvent = std::variant<SstvTxEnable,
                                 SstvTxDisable,
                                 SstvTxReset,
                                 SstvTxTick,
                                 SstvTxPrepare,
                                 SstvTxImagePrepared,
                                 SstvTxEncodingComplete,
                                 SstvTxRequest,
                                 SstvTxPttRequestDispatched,
                                 SstvTxPttConfirmed,
                                 SstvTxLeadElapsed,
                                 SstvTxHeaderComplete,
                                 SstvTxImageComplete,
                                 SstvTxFskIdComplete,
                                 SstvTxTailElapsed,
                                 SstvTxPttReleased,
                                 SstvTxCancel,
                                 SstvTxFailure>;

struct SstvTxMetrics final
{
    SstvTxState state {SstvTxState::Disabled};
    SstvTxCause lastCause {SstvTxCause::None};
    SstvTxErrorCode lastErrorCode {SstvTxErrorCode::None};
    std::string lastErrorDetail;
    std::string mode;
    std::uint64_t currentSessionId {0U};
    std::uint64_t stateEnteredAtMs {0U};
    std::uint64_t lastEventAtMs {0U};
    std::uint64_t encodedSamples {0U};
    std::uint64_t encodedDurationMs {0U};
    std::uint32_t sampleRate {0U};
    std::uint32_t imageWidth {0U};
    std::uint32_t imageHeight {0U};
    std::uint64_t sessionsStarted {0U};
    std::uint64_t sessionsCompleted {0U};
    std::uint64_t sessionsCancelled {0U};
    std::uint64_t sessionsFailed {0U};
    std::uint64_t pttRequestsDispatched {0U};
    std::uint64_t pttConfirmations {0U};
    std::uint64_t pttReleaseRequests {0U};
    std::uint64_t pttReleases {0U};
    std::uint64_t watchdogExpiries {0U};
    std::uint64_t rejectedEvents {0U};
    bool fskIdEnabled {false};
    bool pttRequestDispatched {false};
    bool pttConfirmed {false};
    bool releaseRequired {false};
    bool audioStarted {false};
};

struct SstvTxTransition final
{
    SstvTxState before {SstvTxState::Disabled};
    SstvTxState after {SstvTxState::Disabled};
    SstvTxCause cause {SstvTxCause::None};
    bool accepted {false};
    bool stateChanged {false};
    bool releaseRequired {false};
    bool automaticReset {false};
    std::uint64_t sessionId {0U};
};

// Deterministic, side-effect-free TX lifecycle authority.  It never keys a
// radio or opens audio.  The Decodium bridge/coordinator performs those
// effects only after an accepted transition and feeds the resulting events
// back here.  This separation makes the PTT fail-safe paths exhaustively
// testable without hardware.
class SstvTxStateMachine final
{
public:
    explicit SstvTxStateMachine(SstvTxPolicy policy = {});

    SstvTxTransition dispatch(std::uint64_t nowMs,
                              const SstvTxEvent& event);

    SstvTxState state() const noexcept;
    const SstvTxPolicy& policy() const noexcept;
    const SstvTxMetrics& metrics() const noexcept;
    bool enabled() const noexcept;
    bool active() const noexcept;
    bool releaseRequired() const noexcept;
    bool invariantsHold() const noexcept;

    static bool isTerminal(SstvTxState state) noexcept;
    static const char* stateName(SstvTxState state) noexcept;
    static const char* causeName(SstvTxCause cause) noexcept;

private:
    struct DeferredTerminal final
    {
        SstvTxState state {SstvTxState::Completed};
        SstvTxCause cause {SstvTxCause::Completed};
        SstvTxErrorCode error {SstvTxErrorCode::None};
        std::string detail;
        bool valid {false};
    };

    SstvTxTransition result(SstvTxState before,
                            SstvTxCause cause,
                            bool accepted,
                            bool automaticReset = false) const noexcept;
    SstvTxTransition reject(SstvTxState before,
                            SstvTxCause cause) noexcept;
    void transitionTo(SstvTxState state,
                      SstvTxCause cause,
                      std::uint64_t nowMs) noexcept;
    void clearSession() noexcept;
    void resetToIdle(std::uint64_t nowMs,
                     SstvTxCause cause) noexcept;
    void beginSession(const SstvTxPrepare& prepare,
                      std::uint64_t nowMs);
    void enterTerminal(SstvTxState state,
                       SstvTxCause cause,
                       SstvTxErrorCode error,
                       std::string detail,
                       std::uint64_t nowMs);
    void requestTerminal(SstvTxState state,
                         SstvTxCause cause,
                         SstvTxErrorCode error,
                         std::string detail,
                         std::uint64_t nowMs);
    bool expire(std::uint64_t nowMs);
    std::uint64_t timeoutForState() const noexcept;
    bool validPrepare(const SstvTxPrepare& prepare) const noexcept;
    bool validEncoding(const SstvTxEncodingComplete& encoded) const noexcept;
    std::string boundedDetail(const std::string& detail) const;
    static void validatePolicy(const SstvTxPolicy& policy);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;

    SstvTxPolicy policy_;
    SstvTxMetrics metrics_;
    DeferredTerminal deferred_;
    std::uint64_t nextSessionId_ {0U};
    bool enabled_ {false};
    bool hasClock_ {false};
};

} // namespace decodium::sstv
