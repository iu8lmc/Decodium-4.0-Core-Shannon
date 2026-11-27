// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvRxCorrectionController.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace decodium::sstv {

SstvAfcController::SstvAfcController()
    : SstvAfcController(Config {})
{
}

SstvAfcController::SstvAfcController(Config config)
    : m_config(validate(config))
{
}

void SstvAfcController::configure(SstvRxAfcMode mode,
                                  double manualCorrectionHz)
{
    if (mode != SstvRxAfcMode::Off
        && mode != SstvRxAfcMode::Automatic
        && mode != SstvRxAfcMode::Manual) {
        throw std::invalid_argument("invalid SSTV AFC mode");
    }
    if (!std::isfinite(manualCorrectionHz)
        || std::abs(manualCorrectionHz) > m_config.maximumCorrectionHz) {
        throw std::invalid_argument("invalid SSTV manual AFC correction");
    }

    const bool modeChanged = m_snapshot.mode != mode;
    m_snapshot.mode = mode;
    if (mode == SstvRxAfcMode::Off) {
        m_snapshot.correctionHz = 0.0;
        m_snapshot.confidence = 0.0;
    } else if (mode == SstvRxAfcMode::Manual) {
        m_snapshot.correctionHz = manualCorrectionHz;
        m_snapshot.measuredOffsetHz = manualCorrectionHz;
        m_snapshot.confidence = 1.0;
    } else if (modeChanged) {
        m_snapshot.correctionHz = 0.0;
        m_snapshot.measuredOffsetHz = 0.0;
        m_snapshot.confidence = 0.0;
    }
    if (modeChanged) {
        m_references.clear();
        m_snapshot.retainedReferences = 0U;
        m_haveTimeline = false;
    }
}

SstvAfcUpdate SstvAfcController::consume(SstvAfcEvidence evidence)
{
    if (!std::isfinite(evidence.measuredRawFrequencyHz)
        || !std::isfinite(evidence.nominalFrequencyHz)
        || !std::isfinite(evidence.confidence)
        || evidence.measuredRawFrequencyHz <= 0.0
        || evidence.nominalFrequencyHz <= 0.0
        || evidence.confidence < 0.0 || evidence.confidence > 1.0) {
        return reject(SstvAfcUpdateStatus::InvalidInput);
    }
    if (m_haveTimeline
        && (evidence.sequence <= m_lastSequence
            || evidence.centreSample <= m_lastCentreSample)) {
        if (m_snapshot.nonMonotonicReferences
            != std::numeric_limits<std::uint64_t>::max()) {
            ++m_snapshot.nonMonotonicReferences;
        }
        return reject(SstvAfcUpdateStatus::NonMonotonic);
    }
    m_lastSequence = evidence.sequence;
    m_lastCentreSample = evidence.centreSample;
    m_haveTimeline = true;

    if (m_snapshot.mode == SstvRxAfcMode::Off) {
        return reject(SstvAfcUpdateStatus::Disabled);
    }
    if (m_snapshot.mode == SstvRxAfcMode::Manual) {
        return reject(SstvAfcUpdateStatus::ManualMode);
    }
    if (evidence.role == SstvAfcEvidenceRole::ImageData) {
        if (m_snapshot.rejectedImageObservations
            != std::numeric_limits<std::uint64_t>::max()) {
            ++m_snapshot.rejectedImageObservations;
        }
        return reject(SstvAfcUpdateStatus::LuminanceRejected);
    }
    if (!roleIsEligible(evidence.role) || !evidence.trusted) {
        return reject(SstvAfcUpdateStatus::UntrustedReference);
    }
    if (evidence.confidence < m_config.minimumReferenceConfidence) {
        return reject(SstvAfcUpdateStatus::LowConfidence);
    }

    const double offset = evidence.measuredRawFrequencyHz
        - evidence.nominalFrequencyHz;
    if (!std::isfinite(offset)
        || std::abs(offset) > m_config.maximumCorrectionHz) {
        return reject(SstvAfcUpdateStatus::OutOfRange);
    }

    m_references.push_back(Reference {offset, evidence.confidence});
    while (m_references.size() > m_config.referenceWindow) {
        m_references.pop_front();
    }
    const double target = robustOffset();
    const double previous = m_snapshot.correctionHz;
    if (m_snapshot.acceptedReferences == 0U) {
        // A qualified leader/header is a direct acquisition measurement.  Snap
        // once so a full +/-100 Hz mistune is corrected before image pixels.
        m_snapshot.correctionHz = target;
    } else {
        double delta = m_config.smoothing
            * (target - m_snapshot.correctionHz);
        delta = clamp(delta,
                      -m_config.maximumTrackingStepHz,
                      m_config.maximumTrackingStepHz);
        m_snapshot.correctionHz = clamp(
            m_snapshot.correctionHz + delta,
            -m_config.maximumCorrectionHz,
            m_config.maximumCorrectionHz);
    }
    m_snapshot.measuredOffsetHz = target;
    m_snapshot.confidence = aggregateConfidence();
    if (m_snapshot.acceptedReferences
        != std::numeric_limits<std::uint64_t>::max()) {
        ++m_snapshot.acceptedReferences;
    }
    m_snapshot.retainedReferences = m_references.size();

    SstvAfcUpdate update;
    update.status = SstvAfcUpdateStatus::Accepted;
    update.correctionChanged = std::abs(previous - m_snapshot.correctionHz)
        > 1.0e-9;
    update.snapshot = m_snapshot;
    return update;
}

void SstvAfcController::reset() noexcept
{
    const SstvRxAfcMode mode = m_snapshot.mode;
    m_snapshot = {};
    m_snapshot.mode = mode;
    m_references.clear();
    m_lastSequence = 0U;
    m_lastCentreSample = 0U;
    m_haveTimeline = false;
}

SstvAfcSnapshot SstvAfcController::snapshot() const noexcept
{
    return m_snapshot;
}

const SstvAfcController::Config& SstvAfcController::config() const noexcept
{
    return m_config;
}

SstvAfcController::Config SstvAfcController::validate(Config config)
{
    if (!std::isfinite(config.maximumCorrectionHz)
        || config.maximumCorrectionHz <= 0.0
        || config.maximumCorrectionHz > 500.0
        || !std::isfinite(config.minimumReferenceConfidence)
        || config.minimumReferenceConfidence < 0.0
        || config.minimumReferenceConfidence > 1.0
        || !std::isfinite(config.smoothing) || config.smoothing <= 0.0
        || config.smoothing > 1.0
        || !std::isfinite(config.maximumTrackingStepHz)
        || config.maximumTrackingStepHz <= 0.0
        || config.maximumTrackingStepHz > config.maximumCorrectionHz
        || config.referenceWindow < 3U || config.referenceWindow > 31U) {
        throw std::invalid_argument("invalid SSTV AFC controller config");
    }
    return config;
}

bool SstvAfcController::roleIsEligible(SstvAfcEvidenceRole role) noexcept
{
    return role == SstvAfcEvidenceRole::Leader
        || role == SstvAfcEvidenceRole::HeaderBreak
        || role == SstvAfcEvidenceRole::VisControl
        || role == SstvAfcEvidenceRole::TrustedLineSync;
}

double SstvAfcController::clamp(double value,
                                double minimum,
                                double maximum) noexcept
{
    return std::max(minimum, std::min(maximum, value));
}

double SstvAfcController::robustOffset() const
{
    std::vector<double> values;
    values.reserve(m_references.size());
    for (const Reference& reference : m_references) {
        values.push_back(reference.offsetHz);
    }
    const std::size_t middle = values.size() / 2U;
    std::nth_element(values.begin(),
                     values.begin() + static_cast<std::ptrdiff_t>(middle),
                     values.end());
    double median = values[middle];
    if ((values.size() % 2U) == 0U) {
        const auto lower = std::max_element(
            values.begin(),
            values.begin() + static_cast<std::ptrdiff_t>(middle));
        median = (*lower + median) * 0.5;
    }
    return median;
}

double SstvAfcController::aggregateConfidence() const noexcept
{
    double sum = 0.0;
    for (const Reference& reference : m_references) {
        sum += reference.confidence;
    }
    return m_references.empty()
        ? 0.0
        : clamp(sum / static_cast<double>(m_references.size()), 0.0, 1.0);
}

SstvAfcUpdate SstvAfcController::reject(SstvAfcUpdateStatus status) noexcept
{
    if (m_snapshot.rejectedReferences
        != std::numeric_limits<std::uint64_t>::max()) {
        ++m_snapshot.rejectedReferences;
    }
    return SstvAfcUpdate {status, false, m_snapshot};
}

SstvSlantController::SstvSlantController()
    : SstvSlantController(Config {})
{
}

SstvSlantController::SstvSlantController(Config config)
    : m_config(validate(config))
{
}

void SstvSlantController::configure(
    std::uint64_t nominalLinePeriodSamples,
    SstvRxSlantMode mode,
    double manualClockErrorPpm)
{
    if (nominalLinePeriodSamples == 0U
        || nominalLinePeriodSamples
            > SstvSlantEstimator::MaximumLinePeriodSamples
        || (mode != SstvRxSlantMode::Off
            && mode != SstvRxSlantMode::Automatic
            && mode != SstvRxSlantMode::Manual)
        || !finiteBounded(manualClockErrorPpm,
                          m_config.maximumClockErrorPpm)) {
        throw std::invalid_argument("invalid SSTV slant controller settings");
    }

    const bool rebuild = !m_estimator
        || nominalLinePeriodSamples != m_nominalLinePeriodSamples;
    m_nominalLinePeriodSamples = nominalLinePeriodSamples;
    m_manualClockErrorPpm = manualClockErrorPpm;
    m_snapshot.mode = mode;
    m_snapshot.configured = true;
    if (rebuild) {
        SstvSlantEstimatorConfig estimatorConfig;
        estimatorConfig.nominalLinePeriodSamples = nominalLinePeriodSamples;
        estimatorConfig.windowLines = m_config.windowLines;
        estimatorConfig.minimumLines = m_config.minimumLines;
        estimatorConfig.minimumConfidence = m_config.minimumConfidence;
        estimatorConfig.outlierToleranceSamples
            = m_config.outlierToleranceSamples;
        estimatorConfig.warningClockErrorPpm
            = m_config.warningClockErrorPpm;
        estimatorConfig.maximumClockErrorPpm
            = m_config.maximumClockErrorPpm;
        m_estimator = std::make_unique<SstvSlantEstimator>(estimatorConfig);
        m_snapshot.estimateValid = false;
        m_snapshot.measuredClockErrorPpm = 0.0;
        m_snapshot.confidence = 0.0;
    }
    if (mode == SstvRxSlantMode::Off) {
        m_snapshot.appliedClockErrorPpm = 0.0;
    } else if (mode == SstvRxSlantMode::Manual) {
        m_snapshot.appliedClockErrorPpm = manualClockErrorPpm;
    } else {
        m_snapshot.appliedClockErrorPpm = m_snapshot.estimateValid
            ? m_snapshot.measuredClockErrorPpm : 0.0;
    }
}

SstvSlantControllerSnapshot SstvSlantController::observe(
    const SstvSlantObservation& observation)
{
    if (!m_estimator) {
        if (m_snapshot.rejectedSyncs
            != std::numeric_limits<std::uint64_t>::max()) {
            ++m_snapshot.rejectedSyncs;
        }
        return m_snapshot;
    }
    const SstvSlantUpdate update = m_estimator->observe(observation);
    if (update.accepted) {
        if (m_snapshot.observedSyncs
            != std::numeric_limits<std::uint64_t>::max()) {
            ++m_snapshot.observedSyncs;
        }
    } else if (m_snapshot.rejectedSyncs
               != std::numeric_limits<std::uint64_t>::max()) {
        ++m_snapshot.rejectedSyncs;
    }
    updateFromEstimate(update.estimate);
    return m_snapshot;
}

void SstvSlantController::notifyDiscontinuity()
{
    if (m_estimator) {
        m_estimator->notifyDiscontinuity();
    }
    m_snapshot.estimateValid = false;
    m_snapshot.confidence = 0.0;
    if (m_snapshot.mode == SstvRxSlantMode::Automatic) {
        m_snapshot.appliedClockErrorPpm = 0.0;
    }
}

void SstvSlantController::reset()
{
    if (m_estimator) {
        m_estimator->reset();
    }
    const SstvRxSlantMode mode = m_snapshot.mode;
    const bool configured = m_snapshot.configured;
    m_snapshot = {};
    m_snapshot.mode = mode;
    m_snapshot.configured = configured;
    if (mode == SstvRxSlantMode::Manual) {
        m_snapshot.appliedClockErrorPpm = m_manualClockErrorPpm;
    }
}

SstvSlantControllerSnapshot SstvSlantController::snapshot() const noexcept
{
    return m_snapshot;
}

const SstvSlantController::Config& SstvSlantController::config() const noexcept
{
    return m_config;
}

SstvSlantController::Config SstvSlantController::validate(Config config)
{
    if (config.windowLines < 4U
        || config.windowLines > SstvSlantEstimator::MaximumWindowLines
        || config.minimumLines < 2U
        || config.minimumLines > config.windowLines
        || !std::isfinite(config.minimumConfidence)
        || config.minimumConfidence < 0.0
        || config.minimumConfidence > 1.0
        || !std::isfinite(config.outlierToleranceSamples)
        || config.outlierToleranceSamples <= 0.0
        || !std::isfinite(config.warningClockErrorPpm)
        || config.warningClockErrorPpm < 0.0
        || !std::isfinite(config.maximumClockErrorPpm)
        || config.maximumClockErrorPpm <= 0.0
        || config.maximumClockErrorPpm
            > SstvSlantEstimator::MaximumAllowedClockErrorPpm
        || config.warningClockErrorPpm > config.maximumClockErrorPpm) {
        throw std::invalid_argument("invalid SSTV slant controller config");
    }
    return config;
}

bool SstvSlantController::finiteBounded(double value, double bound) noexcept
{
    return std::isfinite(value) && std::abs(value) <= bound;
}

void SstvSlantController::updateFromEstimate(
    const SstvSlantEstimate& estimate) noexcept
{
    m_snapshot.estimateValid = estimate.valid;
    m_snapshot.measuredClockErrorPpm = estimate.valid
        ? estimate.clockErrorPpm : 0.0;
    m_snapshot.confidence = estimate.valid ? estimate.confidence : 0.0;
    if (m_snapshot.mode == SstvRxSlantMode::Off) {
        m_snapshot.appliedClockErrorPpm = 0.0;
    } else if (m_snapshot.mode == SstvRxSlantMode::Manual) {
        m_snapshot.appliedClockErrorPpm = m_manualClockErrorPpm;
    } else {
        m_snapshot.appliedClockErrorPpm = estimate.valid
            ? estimate.clockErrorPpm : 0.0;
    }
}

} // namespace decodium::sstv
