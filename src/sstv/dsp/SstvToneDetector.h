// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace decodium::sstv
{

enum class SstvToneStatus : std::uint8_t
{
  Detected,
  LowSignal,
  Ambiguous,
  InvalidInput
};

struct SstvToneDetectorConfig
{
  double sampleRateHz {12'000.0};
  std::size_t windowSamples {264};
  std::size_t hopSamples {132};
  std::vector<double> nominalFrequenciesHz {
      1'100.0, 1'200.0, 1'300.0, 1'500.0,
      1'900.0, 2'100.0, 2'300.0
  };

  double maximumOffsetHz {100.0};
  double searchStepHz {5.0};
  double minimumRms {0.002};
  double minimumSnrDb {6.0};
  double minimumDominanceDb {2.0};
  double minimumConfidence {0.35};
  double offsetSmoothing {0.25};

  static SstvToneDetectorConfig sstvDefaults (double sampleRateHz);
};

struct SstvToneObservation
{
  SstvToneStatus status {SstvToneStatus::InvalidInput};
  std::uint64_t sequence {0};
  std::uint64_t startSample {0};
  std::uint64_t centreSample {0};

  double nominalFrequencyHz {0.0};
  double detectedFrequencyHz {0.0};
  double frequencyOffsetHz {0.0};
  double commonOffsetHz {0.0};
  double rms {0.0};
  double tonePower {0.0};
  double noisePower {0.0};
  double snrDb {0.0};
  double dominanceDb {0.0};
  double confidence {0.0};

  bool valid () const noexcept
  {
    return status == SstvToneStatus::Detected;
  }
};

struct SstvToneDetectorMetrics
{
  std::uint64_t samplesConsumed {0};
  std::uint64_t windowsAnalysed {0};
  std::uint64_t detections {0};
  std::uint64_t lowSignalWindows {0};
  std::uint64_t ambiguousWindows {0};
  std::uint64_t invalidWindows {0};
  std::size_t bufferedSamples {0};
  std::size_t peakBufferedSamples {0};
};

// A bounded, streaming filter bank for the discrete tones used by analog
// SSTV, VIS and FSK ID.  It classifies tone windows; it is not an image
// demodulator and does not by itself establish support for any SSTV mode.
class SstvToneDetector final
{
public:
  // Public hostile-input bounds.  A call exceeding any of these limits is
  // rejected transactionally before samples are read or output is allocated.
  static constexpr std::size_t MaximumSamplesPerConsume = 1'048'576;
  static constexpr std::size_t MaximumObservationsPerConsume = 4'096;
  static constexpr std::size_t MaximumWindowSamples = 65'536;
  static constexpr std::size_t MaximumNominalFrequencies = 64;
  static constexpr std::size_t MaximumSearchPointsPerWindow = 4'096;
  static constexpr std::uint64_t MaximumWorkUnitsPerWindow = 4'194'304;
  static constexpr std::uint64_t MaximumWorkUnitsPerConsume = 134'217'728;
  static constexpr double MinimumSampleRateHz = 6'000.0;
  static constexpr double MaximumSampleRateHz = 384'000.0;
  static constexpr double MaximumConfiguredOffsetHz = 1'000.0;

  explicit SstvToneDetector (SstvToneDetectorConfig config);

  std::vector<SstvToneObservation> consume (float const* samples,
                                            std::size_t count);
  std::vector<SstvToneObservation> consume (
      std::vector<float> const& samples);

  // Clears stream overlap and counters.  A caller may retain an externally
  // established radio offset across a discontinuity only by opting in.
  void reset (bool preserveCommonOffset = false) noexcept;

  // Drops overlap while retaining an absolute stream coordinate.  This is
  // used at an exact protocol boundary (for example, the first sample after
  // a completed image) so a following leader cannot share an FFT window with
  // the previous frame.  It is deliberately distinct from reset(), whose
  // public contract starts a new zero-based stream.
  void resetAtStreamSample (std::uint64_t nextSample,
                            bool preserveCommonOffset = false) noexcept;

  void seedCommonOffset (double offsetHz);
  void clearCommonOffset () noexcept;
  bool hasCommonOffset () const noexcept;
  std::optional<double> commonOffsetHz () const noexcept;

  SstvToneDetectorConfig const& config () const noexcept;
  SstvToneDetectorMetrics const& metrics () const noexcept;
  std::size_t bufferedSampleCount () const noexcept;
  std::size_t maximumBufferedSampleCount () const noexcept;

  static std::vector<double> defaultSstvFrequencies ();

private:
  SstvToneObservation analyseWindow ();
  static double spectralEnergy (std::vector<double> const& windowed,
                                double sampleRateHz,
                                double frequencyHz) noexcept;
  void updateOffset (double offsetHz) noexcept;
  static void validateConfig (SstvToneDetectorConfig const& config);

  SstvToneDetectorConfig config_;
  SstvToneDetectorMetrics metrics_;
  std::vector<float> buffer_;
  std::uint64_t bufferStartSample_ {0};
  std::uint64_t observationSequence_ {0};
  std::size_t searchPointUpperBound_ {0};
  std::uint64_t workUnitsPerWindow_ {0};
  std::optional<double> commonOffsetHz_;
};

} // namespace decodium::sstv
