#ifndef DECODIUM_FT2LINK_WAVEFORM_HPP
#define DECODIUM_FT2LINK_WAVEFORM_HPP

#include "lib/ft2link/FT2LinkFrame.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace decodium
{
namespace ft2link
{

struct W500WaveformConfig
{
  double sampleRate {12000.0};
  double centerFrequencyHz {1500.0};
  double symbolRate {125.0};
  double toneSpacingHz {125.0};
  double gain {0.80};
};

struct W500DecodeMetrics
{
  std::size_t sampleOffset {0};
  std::size_t symbolOffset {0};
  std::size_t symbolCount {0};
  std::size_t packetBytes {0};
  double quality {0.0};
  double estimatedCenterFrequencyHz {0.0};
  double estimatedFrequencyOffsetHz {0.0};
};

struct NarrowWaveformConfig
{
  double sampleRate {12000.0};
  double centerFrequencyHz {1500.0};
  double toneSpacingHz {125.0};
  double symbolRate {125.0};
  double gain {0.75};
  int bitRepetition {3};
};

struct NarrowDecodeMetrics
{
  std::size_t sampleOffset {0};
  std::size_t symbolOffset {0};
  std::size_t symbolCount {0};
  std::size_t packetBytes {0};
  double quality {0.0};
  double estimatedCenterFrequencyHz {0.0};
  double estimatedFrequencyOffsetHz {0.0};
  int bitRepetition {3};
  double rawBitRate {0.0};
  double payloadBitRate {0.0};
};

enum class W2300RateMode : std::uint8_t
{
  Fast = 0,
  Robust = 1,
  Weak = 2,
  Deep = 3,
  Ultra = 4
};

struct W2300WaveformConfig
{
  double sampleRate {12000.0};
  double centerFrequencyHz {1500.0};
  double symbolRate {600.0};
  double subcarrierSpacingHz {600.0};
  double gain {0.55};
  W2300RateMode rateMode {W2300RateMode::Fast};
  // 0 means unlimited. Live RX sets a small budget so a false W2300
  // acquisition cannot hold the decode worker during shutdown.
  int maxDecodeMillis {0};
};

struct W2300DecodeMetrics
{
  std::size_t sampleOffset {0};
  std::size_t symbolOffset {0};
  std::size_t symbolCount {0};
  std::size_t packetBytes {0};
  double quality {0.0};
  double estimatedCenterFrequencyHz {0.0};
  double estimatedFrequencyOffsetHz {0.0};
  W2300RateMode rateMode {W2300RateMode::Fast};
  int repetitionFactor {1};
  double rawBitRate {0.0};
  double payloadBitRate {0.0};
};

std::vector<std::uint8_t> w500PacketToSymbols (std::vector<std::uint8_t> const& packet,
                                               std::string* error = nullptr);
bool w500SymbolsToPacket (std::vector<std::uint8_t> const& symbols,
                          std::vector<std::uint8_t>* packet,
                          std::string* error = nullptr);

std::vector<float> generateW500Waveform (std::vector<std::uint8_t> const& packet,
                                         W500WaveformConfig const& config = W500WaveformConfig {},
                                         std::string* error = nullptr);
bool decodeW500Waveform (std::vector<float> const& wave,
                         std::vector<std::uint8_t>* packet,
                         W500WaveformConfig const& config = W500WaveformConfig {},
                         std::string* error = nullptr);
bool decodeW500WaveformWithMetrics (std::vector<float> const& wave,
                                    std::vector<std::uint8_t>* packet,
                                    W500DecodeMetrics* metrics,
                                    W500WaveformConfig const& config = W500WaveformConfig {},
                                    std::string* error = nullptr);

std::vector<float> generateW500FrameWaveform (Frame const& frame,
                                              W500WaveformConfig const& config = W500WaveformConfig {},
                                              std::string* error = nullptr);
bool decodeW500FrameWaveform (std::vector<float> const& wave,
                              Frame* frame,
                              W500WaveformConfig const& config = W500WaveformConfig {},
                              std::string* error = nullptr);
bool decodeW500FrameWaveformWithMetrics (std::vector<float> const& wave,
                                         Frame* frame,
                                         W500DecodeMetrics* metrics,
                                         W500WaveformConfig const& config = W500WaveformConfig {},
                                         std::string* error = nullptr);

std::vector<float> generateNarrowFrameWaveform (
    Frame const& frame,
    NarrowWaveformConfig const& config = NarrowWaveformConfig {},
    std::string* error = nullptr);
bool decodeNarrowFrameWaveform (
    std::vector<float> const& wave,
    Frame* frame,
    NarrowWaveformConfig const& config = NarrowWaveformConfig {},
    std::string* error = nullptr);
bool decodeNarrowFrameWaveformWithMetrics (
    std::vector<float> const& wave,
    Frame* frame,
    NarrowDecodeMetrics* metrics,
    NarrowWaveformConfig const& config = NarrowWaveformConfig {},
    std::string* error = nullptr);

std::vector<std::uint8_t> w2300PacketToSymbols (std::vector<std::uint8_t> const& packet,
                                                std::string* error = nullptr);
std::vector<std::uint8_t> w2300PacketToSymbols (std::vector<std::uint8_t> const& packet,
                                                W2300RateMode mode,
                                                std::string* error = nullptr);
bool w2300SymbolsToPacket (std::vector<std::uint8_t> const& symbols,
                           std::vector<std::uint8_t>* packet,
                           std::string* error = nullptr);
char const* w2300RateModeName (W2300RateMode mode);
int w2300RateModeRepetitionFactor (W2300RateMode mode);
W2300RateMode recommendedW2300RateMode (W2300DecodeMetrics const& metrics,
                                        unsigned retryCount = 0);

class W2300RateController
{
public:
  explicit W2300RateController (W2300RateMode initialMode = W2300RateMode::Fast);

  W2300RateMode currentMode () const;
  void reset (W2300RateMode mode = W2300RateMode::Fast);
  void observe (W2300DecodeMetrics const& metrics, unsigned retryCount = 0);
  W2300WaveformConfig configForAttempt (
      int attemptCount,
      W2300WaveformConfig const& base = W2300WaveformConfig {}) const;

private:
  W2300RateMode m_currentMode {W2300RateMode::Fast};
};

std::vector<float> generateW2300Waveform (std::vector<std::uint8_t> const& packet,
                                          W2300WaveformConfig const& config = W2300WaveformConfig {},
                                          std::string* error = nullptr);
bool decodeW2300Waveform (std::vector<float> const& wave,
                          std::vector<std::uint8_t>* packet,
                          W2300WaveformConfig const& config = W2300WaveformConfig {},
                          std::string* error = nullptr);
bool decodeW2300WaveformWithMetrics (std::vector<float> const& wave,
                                     std::vector<std::uint8_t>* packet,
                                     W2300DecodeMetrics* metrics,
                                     W2300WaveformConfig const& config = W2300WaveformConfig {},
                                     std::string* error = nullptr);

std::vector<float> generateW2300FrameWaveform (Frame const& frame,
                                               W2300WaveformConfig const& config = W2300WaveformConfig {},
                                               std::string* error = nullptr);
bool decodeW2300FrameWaveform (std::vector<float> const& wave,
                               Frame* frame,
                               W2300WaveformConfig const& config = W2300WaveformConfig {},
                               std::string* error = nullptr);
bool decodeW2300FrameWaveformWithMetrics (std::vector<float> const& wave,
                                          Frame* frame,
                                          W2300DecodeMetrics* metrics,
                                          W2300WaveformConfig const& config = W2300WaveformConfig {},
                                          std::string* error = nullptr);

}
}

#endif
