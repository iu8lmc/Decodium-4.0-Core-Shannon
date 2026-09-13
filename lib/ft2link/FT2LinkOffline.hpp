#ifndef DECODIUM_FT2LINK_OFFLINE_HPP
#define DECODIUM_FT2LINK_OFFLINE_HPP

#include "lib/ft2link/FT2LinkFrame.hpp"
#include "lib/ft2link/FT2LinkWaveform.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace decodium
{
namespace ft2link
{

struct W2300OfflinePipelineOptions
{
  std::size_t windowSize {1};
  std::uint64_t retryMs {1000};
  int maxAttempts {5};
  std::size_t maxIterations {64};
  W2300RateMode initialRateMode {W2300RateMode::Fast};
  W2300WaveformConfig txConfig;
  W2300WaveformConfig rxConfig;
  std::size_t leadingSamples {0};
  std::size_t trailingSamples {0};
  float noiseAmplitude {0.0f};
  std::vector<std::uint16_t> dropFirstAttemptSequences;
};

struct W2300OfflineBurstTrace
{
  std::uint16_t sequence {0};
  int attempt {0};
  W2300RateMode rateMode {W2300RateMode::Fast};
  bool dropped {false};
  bool decoded {false};
  W2300DecodeMetrics metrics;
};

struct W2300OfflinePipelineResult
{
  bool complete {false};
  bool failed {false};
  std::vector<std::uint8_t> receivedMessage;
  std::vector<W2300OfflineBurstTrace> bursts;
  std::string error;
};

W2300OfflinePipelineResult runW2300OfflinePipeline (
    std::vector<std::uint8_t> const& message,
    std::uint16_t sessionId,
    W2300OfflinePipelineOptions const& options = W2300OfflinePipelineOptions {});

}
}

#endif
