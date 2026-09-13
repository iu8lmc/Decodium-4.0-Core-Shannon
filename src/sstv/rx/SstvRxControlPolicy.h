// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace decodium::sstv {

enum class SstvRxModeControl : std::uint8_t
{
    Automatic,
    Manual,
};

enum class SstvRxAfcMode : std::uint8_t
{
    Off,
    Automatic,
    Manual,
};

enum class SstvRxSlantMode : std::uint8_t
{
    Off,
    Automatic,
    Manual,
};

// User-owned RX controls.  This object deliberately contains only bounded
// scalar/string state: it is safe to copy at an SSTV worker chunk boundary and
// never carries audio, images, paths, or QObject ownership across threads.
struct SstvRxControlSettings final
{
    SstvRxModeControl modeControl {SstvRxModeControl::Automatic};
    std::string manualMode;
    bool modeLockEnabled {false};
    std::string lockedMode;
    bool receiveWithoutVis {false};
    bool timingFallbackEnabled {true};

    SstvRxAfcMode afcMode {SstvRxAfcMode::Automatic};
    double manualFrequencyCorrectionHz {0.0};
    SstvRxSlantMode slantMode {SstvRxSlantMode::Automatic};
    double manualClockErrorPpm {0.0};

    std::uint32_t replayRetentionSeconds {180U};
    bool retainRawAudio {false};
    bool diagnosticScopeEnabled {false};
};

struct SstvRxRedecodeParameters final
{
    // Empty means use automatic VIS/timing detection on the retained audio.
    std::string mode;
    SstvRxAfcMode afcMode {SstvRxAfcMode::Automatic};
    double frequencyCorrectionHz {0.0};
    SstvRxSlantMode slantMode {SstvRxSlantMode::Automatic};
    double clockErrorPpm {0.0};
};

struct SstvRxControlSnapshot final
{
    SstvRxControlSettings settings;
    std::uint64_t revision {0U};
    std::uint64_t afcResetSerial {0U};
    std::uint64_t slantResetSerial {0U};
    std::uint64_t redecodeSerial {0U};
    std::optional<SstvRxRedecodeParameters> redecode;
};

// Thread-safe owner/worker mailbox.  UI/Bridge setters only validate and copy
// bounded control state.  DSP remains on the existing native SSTV worker,
// which snapshots this mailbox at chunk boundaries and remembers the command
// serials it has consumed.
class SstvRxControlPolicy final
{
public:
    static constexpr std::size_t MaximumModeCharacters = 64U;
    static constexpr double MaximumFrequencyCorrectionHz = 150.0;
    static constexpr double MaximumAbsoluteClockErrorPpm = 5'000.0;
    static constexpr std::uint32_t MinimumReplayRetentionSeconds = 5U;
    static constexpr std::uint32_t MaximumReplayRetentionSeconds = 600U;

    SstvRxControlPolicy();
    explicit SstvRxControlPolicy(SstvRxControlSettings settings);

    SstvRxControlPolicy(const SstvRxControlPolicy&) = delete;
    SstvRxControlPolicy& operator=(const SstvRxControlPolicy&) = delete;

    SstvRxControlSnapshot snapshot() const;
    bool replace(SstvRxControlSettings settings);

    bool setModeControl(SstvRxModeControl control, std::string manualMode);
    bool setModeLock(bool enabled, std::string lockedMode);
    bool setReceiveWithoutVis(bool enabled);
    bool setTimingFallbackEnabled(bool enabled);
    bool setAfc(SstvRxAfcMode mode, double manualCorrectionHz);
    bool setSlant(SstvRxSlantMode mode, double manualClockErrorPpm);
    bool setReplayRetentionSeconds(std::uint32_t seconds);
    bool setRetainRawAudio(bool enabled);
    bool setDiagnosticScopeEnabled(bool enabled);

    void requestAfcReset() noexcept;
    void requestSlantReset() noexcept;
    bool requestRedecode(SstvRxRedecodeParameters parameters);

    static bool settingsAreValid(const SstvRxControlSettings& settings)
        noexcept;
    static bool redecodeParametersAreValid(
        const SstvRxRedecodeParameters& parameters) noexcept;
    static bool modeNameIsValid(const std::string& mode) noexcept;

private:
    static bool afcModeIsValid(SstvRxAfcMode mode) noexcept;
    static bool slantModeIsValid(SstvRxSlantMode mode) noexcept;
    static bool modeControlIsValid(SstvRxModeControl control) noexcept;
    static void increment(std::uint64_t& serial) noexcept;

    mutable std::mutex m_mutex;
    SstvRxControlSnapshot m_snapshot;
};

} // namespace decodium::sstv
