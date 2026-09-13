// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvRxControlPolicy.h"

#include "../dsp/SstvSlantEstimator.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>

namespace decodium::sstv {

// Only protocol tones with a known nominal frequency are eligible for
// automatic AFC.  ImageData exists explicitly so integration code cannot
// accidentally treat luminance/chroma samples as a reference carrier.
enum class SstvAfcEvidenceRole : std::uint8_t
{
    Leader,
    HeaderBreak,
    VisControl,
    TrustedLineSync,
    ImageData,
    Untrusted,
};

struct SstvAfcEvidence final
{
    SstvAfcEvidenceRole role {SstvAfcEvidenceRole::Untrusted};
    std::uint64_t sequence {0U};
    std::uint64_t centreSample {0U};
    double measuredRawFrequencyHz {0.0};
    double nominalFrequencyHz {0.0};
    double confidence {0.0};
    bool trusted {false};
};

enum class SstvAfcUpdateStatus : std::uint8_t
{
    Accepted,
    Disabled,
    ManualMode,
    LuminanceRejected,
    UntrustedReference,
    LowConfidence,
    OutOfRange,
    InvalidInput,
    NonMonotonic,
};

struct SstvAfcSnapshot final
{
    SstvRxAfcMode mode {SstvRxAfcMode::Automatic};
    double measuredOffsetHz {0.0};
    double correctionHz {0.0};
    double confidence {0.0};
    std::uint64_t acceptedReferences {0U};
    std::uint64_t rejectedReferences {0U};
    std::uint64_t rejectedImageObservations {0U};
    std::uint64_t nonMonotonicReferences {0U};
    std::size_t retainedReferences {0U};
};

struct SstvAfcUpdate final
{
    SstvAfcUpdateStatus status {SstvAfcUpdateStatus::InvalidInput};
    bool correctionChanged {false};
    SstvAfcSnapshot snapshot;
};

// Worker-confined robust AFC policy.  Its output correction is the single
// value that must be installed in SstvFrequencyDemodulator.  Mode decoders
// then consume correctedFrequencyHz with their own frequencyOffsetHz set to
// zero; this invariant prevents double correction.
class SstvAfcController final
{
public:
    struct Config final
    {
        double maximumCorrectionHz {150.0};
        double minimumReferenceConfidence {0.55};
        double smoothing {0.25};
        double maximumTrackingStepHz {12.0};
        std::size_t referenceWindow {9U};
    };

    SstvAfcController();
    explicit SstvAfcController(Config config);

    void configure(SstvRxAfcMode mode, double manualCorrectionHz);
    SstvAfcUpdate consume(SstvAfcEvidence evidence);
    void reset() noexcept;
    SstvAfcSnapshot snapshot() const noexcept;
    const Config& config() const noexcept;

private:
    struct Reference final
    {
        double offsetHz {0.0};
        double confidence {0.0};
    };

    static Config validate(Config config);
    static bool roleIsEligible(SstvAfcEvidenceRole role) noexcept;
    static double clamp(double value, double minimum, double maximum) noexcept;
    double robustOffset() const;
    double aggregateConfidence() const noexcept;
    SstvAfcUpdate reject(SstvAfcUpdateStatus status) noexcept;

    Config m_config;
    SstvAfcSnapshot m_snapshot;
    std::deque<Reference> m_references;
    std::uint64_t m_lastSequence {0U};
    std::uint64_t m_lastCentreSample {0U};
    bool m_haveTimeline {false};
};

struct SstvSlantControllerSnapshot final
{
    SstvRxSlantMode mode {SstvRxSlantMode::Automatic};
    bool configured {false};
    bool estimateValid {false};
    double measuredClockErrorPpm {0.0};
    // This value is passed to a freshly constructed mode mapper/decoder or a
    // retained-audio re-decode.  It is the measured source clock error, not an
    // additional resampling ratio applied to an already corrected image.
    double appliedClockErrorPpm {0.0};
    double confidence {0.0};
    std::uint64_t observedSyncs {0U};
    std::uint64_t rejectedSyncs {0U};
};

class SstvSlantController final
{
public:
    struct Config final
    {
        std::size_t windowLines {32U};
        std::size_t minimumLines {4U};
        double minimumConfidence {0.30};
        double outlierToleranceSamples {8.0};
        double warningClockErrorPpm {300.0};
        double maximumClockErrorPpm {5'000.0};
    };

    SstvSlantController();
    explicit SstvSlantController(Config config);

    // Reconfiguring the nominal period starts a new mode/session estimate.
    void configure(std::uint64_t nominalLinePeriodSamples,
                   SstvRxSlantMode mode,
                   double manualClockErrorPpm);
    SstvSlantControllerSnapshot observe(const SstvSlantObservation& observation);
    void notifyDiscontinuity();
    void reset();
    SstvSlantControllerSnapshot snapshot() const noexcept;
    const Config& config() const noexcept;

private:
    static Config validate(Config config);
    static bool finiteBounded(double value, double bound) noexcept;
    void updateFromEstimate(const SstvSlantEstimate& estimate) noexcept;

    Config m_config;
    SstvSlantControllerSnapshot m_snapshot;
    std::unique_ptr<SstvSlantEstimator> m_estimator;
    std::uint64_t m_nominalLinePeriodSamples {0U};
    double m_manualClockErrorPpm {0.0};
};

} // namespace decodium::sstv
