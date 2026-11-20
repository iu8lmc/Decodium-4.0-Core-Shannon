#include "lib/ft2link/FT2LinkOffline.hpp"

#include "lib/ft2link/FT2LinkSession.hpp"

#include <algorithm>

namespace decodium
{
namespace ft2link
{
namespace
{
bool shouldDropFirstAttempt (std::vector<std::uint16_t> const& sequences,
                             std::uint16_t sequence,
                             int attempt)
{
  return attempt == 1
      && std::find (sequences.begin (), sequences.end (), sequence) != sequences.end ();
}

std::vector<float> padWave (std::vector<float> const& wave,
                            std::size_t leadingSamples,
                            std::size_t trailingSamples)
{
  std::vector<float> out (leadingSamples, 0.0f);
  out.insert (out.end (), wave.begin (), wave.end ());
  out.insert (out.end (), trailingSamples, 0.0f);
  return out;
}

void addDeterministicNoise (std::vector<float>& wave, float amplitude)
{
  if (amplitude <= 0.0f)
    {
      return;
    }

  std::uint32_t state = 0xd4f72300u;
  for (float& sample : wave)
    {
      state = state * 1664525u + 1013904223u;
      float const unit = float ((state >> 8) & 0xffffu) / 32767.5f - 1.0f;
      sample += amplitude * unit;
    }
}
}

W2300OfflinePipelineResult runW2300OfflinePipeline (
    std::vector<std::uint8_t> const& message,
    std::uint16_t sessionId,
    W2300OfflinePipelineOptions const& options)
{
  W2300OfflinePipelineResult result;

  OutboundTransfer tx {Profile::Wide2300, sessionId, message};
  tx.setWindowSize (options.windowSize);
  tx.setRetryMs (options.retryMs);
  tx.setMaxAttempts (options.maxAttempts);

  InboundTransfer rx {Profile::Wide2300, sessionId};
  W2300RateController rateController {options.initialRateMode};

  std::uint64_t now = 0;
  for (std::size_t iteration = 0;
       iteration < options.maxIterations && !tx.complete () && !tx.failed ();
       ++iteration, now += options.retryMs)
    {
      std::vector<Frame> const frames = tx.framesToSend (now);
      for (Frame const& frame : frames)
        {
          int const attempt = tx.attemptsForSequence (frame.sequence);
          W2300WaveformConfig txConfig = rateController.configForAttempt (
              attempt, options.txConfig);

          W2300OfflineBurstTrace trace;
          trace.sequence = frame.sequence;
          trace.attempt = attempt;
          trace.rateMode = txConfig.rateMode;

          if (shouldDropFirstAttempt (options.dropFirstAttemptSequences,
                                      frame.sequence,
                                      attempt))
            {
              trace.dropped = true;
              result.bursts.push_back (trace);
              continue;
            }

          std::string error;
          std::vector<float> wave = generateW2300FrameWaveform (frame, txConfig, &error);
          if (wave.empty ())
            {
              result.error = error.empty () ? "failed to generate W2300 waveform" : error;
              result.failed = true;
              result.bursts.push_back (trace);
              return result;
            }
          wave = padWave (wave, options.leadingSamples, options.trailingSamples);
          addDeterministicNoise (wave, options.noiseAmplitude);

          Frame decoded;
          W2300DecodeMetrics metrics;
          if (decodeW2300FrameWaveformWithMetrics (
                  wave, &decoded, &metrics, options.rxConfig, &error))
            {
              trace.decoded = true;
              trace.metrics = metrics;
              rx.receive (decoded);
              tx.handleAckFrame (rx.makeAckFrame ());
              rateController.observe (metrics);
            }
          else
            {
              result.error = error;
            }
          result.bursts.push_back (trace);
        }
    }

  result.complete = tx.complete () && rx.complete ();
  result.failed = tx.failed () || !result.complete;
  if (rx.complete ())
    {
      result.receivedMessage = rx.message ();
    }
  if (!result.complete && result.error.empty ())
    {
      result.error = "W2300 offline pipeline did not complete";
    }
  return result;
}

}
}
