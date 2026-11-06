#include "lib/ft2link/FT2LinkAudio.hpp"

#include "lib/ft2link/FT2LinkSession.hpp"

#include <algorithm>
#include <cmath>

namespace decodium
{
namespace ft2link
{
namespace
{
void setError (std::string* error, char const* message)
{
  if (error)
    {
      *error = message;
    }
}

int samplesPerSymbol (W2300WaveformConfig const& config)
{
  if (config.sampleRate <= 0.0 || config.symbolRate <= 0.0)
    {
      return 0;
    }
  double const exact = config.sampleRate / config.symbolRate;
  int const rounded = static_cast<int> (std::lround (exact));
  if (rounded <= 0 || std::fabs (exact - double (rounded)) > 1.0e-6)
    {
      return 0;
    }
  return rounded;
}

int samplesPerSymbol (W500WaveformConfig const& config)
{
  if (config.sampleRate <= 0.0 || config.symbolRate <= 0.0)
    {
      return 0;
    }
  double const exact = config.sampleRate / config.symbolRate;
  int const rounded = static_cast<int> (std::lround (exact));
  if (rounded <= 0 || std::fabs (exact - double (rounded)) > 1.0e-6)
    {
      return 0;
    }
  return rounded;
}

bool shouldDropFirstAttempt (std::vector<std::uint16_t> const& sequences,
                             std::uint16_t sequence,
                             int attempt)
{
  return attempt == 1
      && std::find (sequences.begin (), sequences.end (), sequence) != sequences.end ();
}

std::size_t samplesForMs (std::uint64_t ms, double sampleRate)
{
  if (sampleRate <= 0.0)
    {
      return 0;
    }
  return static_cast<std::size_t> (std::ceil (double (ms) * sampleRate / 1000.0));
}

std::uint64_t msForSamples (std::size_t samples, double sampleRate)
{
  if (sampleRate <= 0.0)
    {
      return 0;
    }
  return static_cast<std::uint64_t> (std::ceil (double (samples) * 1000.0 / sampleRate));
}

bool windowContains (W2300ChannelBusyWindow const& window, std::uint64_t nowMs)
{
  return window.startMs <= nowMs && nowMs < window.endMs;
}

bool windowsContain (std::vector<W2300ChannelBusyWindow> const& windows,
                     std::uint64_t nowMs,
                     std::uint64_t* busyUntilMs)
{
  bool busy = false;
  std::uint64_t until = nowMs;
  for (W2300ChannelBusyWindow const& window : windows)
    {
      if (windowContains (window, nowMs))
        {
          busy = true;
          until = std::max (until, window.endMs);
        }
    }
  if (busyUntilMs)
    {
      *busyUntilMs = until;
    }
  return busy;
}

bool channelBusyAt (W2300AudioPipelineOptions const& options,
                    std::uint64_t nowMs,
                    std::uint64_t localTxBusyUntilMs,
                    W2300ChannelDeferralTrace* trace)
{
  W2300ChannelDeferralTrace result;
  result.atMs = nowMs;
  result.resumeMs = nowMs;

  std::uint64_t busyUntil = nowMs;
  std::uint64_t externalUntil = nowMs;
  if (windowsContain (options.externalBusyWindows, nowMs, &externalUntil))
    {
      result.externalBusy = true;
      busyUntil = std::max (busyUntil, externalUntil);
    }

  std::uint64_t rxUntil = nowMs;
  if (windowsContain (options.rxBusyWindows, nowMs, &rxUntil))
    {
      result.rxBusy = true;
      busyUntil = std::max (busyUntil, rxUntil);
    }

  if (nowMs < localTxBusyUntilMs)
    {
      result.txBusy = true;
      busyUntil = std::max (busyUntil, localTxBusyUntilMs);
    }

  bool const busy = result.externalBusy || result.rxBusy || result.txBusy;
  if (busy)
    {
      result.resumeMs = busyUntil + options.busyBackoffMs;
    }
  if (trace)
    {
      *trace = result;
    }
  return busy;
}

bool channelBusyAt (WideAudioPipelineOptions const& options,
                    std::uint64_t nowMs,
                    std::uint64_t localTxBusyUntilMs,
                    W2300ChannelDeferralTrace* trace)
{
  W2300ChannelDeferralTrace result;
  result.atMs = nowMs;
  result.resumeMs = nowMs;

  std::uint64_t busyUntil = nowMs;
  std::uint64_t externalUntil = nowMs;
  if (windowsContain (options.externalBusyWindows, nowMs, &externalUntil))
    {
      result.externalBusy = true;
      busyUntil = std::max (busyUntil, externalUntil);
    }

  std::uint64_t rxUntil = nowMs;
  if (windowsContain (options.rxBusyWindows, nowMs, &rxUntil))
    {
      result.rxBusy = true;
      busyUntil = std::max (busyUntil, rxUntil);
    }

  if (nowMs < localTxBusyUntilMs)
    {
      result.txBusy = true;
      busyUntil = std::max (busyUntil, localTxBusyUntilMs);
    }

  bool const busy = result.externalBusy || result.rxBusy || result.txBusy;
  if (busy)
    {
      result.resumeMs = busyUntil + options.busyBackoffMs;
    }
  if (trace)
    {
      *trace = result;
    }
  return busy;
}

std::vector<float> sliceSamples (std::vector<float> const& samples,
                                 std::size_t start,
                                 std::size_t count)
{
  std::size_t const end = std::min (samples.size (), start + count);
  return std::vector<float> (
      samples.begin () + static_cast<std::vector<float>::difference_type> (start),
      samples.begin () + static_cast<std::vector<float>::difference_type> (end));
}

void addDeterministicNoise (std::vector<float>& samples, float amplitude)
{
  if (amplitude <= 0.0f)
    {
      return;
    }

  std::uint32_t state = 0xd4a2300u;
  for (float& sample : samples)
    {
      state = state * 1664525u + 1013904223u;
      float const unit = float ((state >> 8) & 0xffffu) / 32767.5f - 1.0f;
      sample += amplitude * unit;
    }
}

bool feedToRx (W2300RxAudioBuffer* rxAudio,
               std::vector<float> const& samples,
               std::size_t chunkSamples,
               Frame* decoded,
               W2300DecodeMetrics* metrics,
               std::string* error)
{
  if (!rxAudio || !decoded || !metrics)
    {
      setError (error, "missing W2300 audio pipeline output");
      return false;
    }

  if (chunkSamples == 0)
    {
      rxAudio->append (samples);
      return rxAudio->decodeNext (decoded, metrics, error);
    }

  for (std::size_t offset = 0; offset < samples.size (); offset += chunkSamples)
    {
      std::size_t const count = std::min (chunkSamples, samples.size () - offset);
      rxAudio->append (sliceSamples (samples, offset, count));
    }

  return rxAudio->decodeNext (decoded, metrics, error);
}

bool feedToRx (W500RxAudioBuffer* rxAudio,
               std::vector<float> const& samples,
               std::size_t chunkSamples,
               Frame* decoded,
               W500DecodeMetrics* metrics,
               std::string* error)
{
  if (!rxAudio || !decoded || !metrics)
    {
      setError (error, "missing W500 audio pipeline output");
      return false;
    }

  if (chunkSamples == 0)
    {
      rxAudio->append (samples);
      return rxAudio->decodeNext (decoded, metrics, error);
    }

  for (std::size_t offset = 0; offset < samples.size (); offset += chunkSamples)
    {
      std::size_t const count = std::min (chunkSamples, samples.size () - offset);
      rxAudio->append (sliceSamples (samples, offset, count));
    }

  return rxAudio->decodeNext (decoded, metrics, error);
}

AudioAckTrace makeAckTrace (Frame const& ack, W500AudioBurstTrace const& burst)
{
  AudioAckTrace trace;
  trace.profile = Profile::Wide500;
  trace.sourceSequence = burst.sequence;
  trace.sourceAttempt = burst.attempt;
  trace.ackBase = ack.ackBase;
  trace.ackBitmap = ack.ackBitmap;
  trace.transmitStartMs = burst.transmitStartMs;
  trace.transmitEndMs = burst.transmitEndMs;
  trace.startSample = burst.startSample;
  trace.sampleCount = burst.sampleCount;
  trace.dropped = burst.dropped;
  trace.decoded = burst.decoded;
  trace.w500Metrics = burst.metrics;
  return trace;
}

AudioAckTrace makeAckTrace (Frame const& ack, W2300AudioBurstTrace const& burst)
{
  AudioAckTrace trace;
  trace.profile = Profile::Wide2300;
  trace.sourceSequence = burst.sequence;
  trace.sourceAttempt = burst.attempt;
  trace.ackBase = ack.ackBase;
  trace.ackBitmap = ack.ackBitmap;
  trace.w2300RateMode = burst.rateMode;
  trace.transmitStartMs = burst.transmitStartMs;
  trace.transmitEndMs = burst.transmitEndMs;
  trace.startSample = burst.startSample;
  trace.sampleCount = burst.sampleCount;
  trace.dropped = burst.dropped;
  trace.decoded = burst.decoded;
  trace.w2300Metrics = burst.metrics;
  return trace;
}

bool appendAndDecodeW2300Ack (Frame const& ack,
                              std::uint16_t sourceSequence,
                              int sourceAttempt,
                              std::uint64_t ackStartMs,
                              W2300WaveformConfig const& config,
                              std::size_t guardBeforeSamples,
                              W2300TxAudioBuffer* ackAudio,
                              W2300RxAudioBuffer* ackRxAudio,
                              AudioAckTrace* trace,
                              Frame* decodedAck,
                              std::size_t chunkSamples,
                              float noiseAmplitude,
                              bool dropAck,
                              std::string* error)
{
  if (!ackAudio || !ackRxAudio || !trace || !decodedAck)
    {
      setError (error, "missing W2300 ACK audio output");
      return false;
    }

  std::size_t const targetSamples = samplesForMs (ackStartMs, config.sampleRate);
  std::size_t const desiredStart = targetSamples > guardBeforeSamples
      ? targetSamples - guardBeforeSamples
      : 0u;
  if (desiredStart > ackAudio->nextSampleIndex ())
    {
      ackAudio->appendSilence (desiredStart - ackAudio->nextSampleIndex ());
    }

  W2300AudioBurstTrace burst;
  if (!ackAudio->appendFrame (ack, 1, config, &burst, error))
    {
      return false;
    }
  if (dropAck)
    {
      burst.dropped = true;
      *trace = makeAckTrace (ack, burst);
      trace->sourceSequence = sourceSequence;
      trace->sourceAttempt = sourceAttempt;
      return true;
    }

  std::vector<float> channelSamples = sliceSamples (
      ackAudio->samples (), burst.startSample, burst.sampleCount);
  addDeterministicNoise (channelSamples, noiseAmplitude);

  W2300DecodeMetrics metrics;
  std::string decodeError;
  if (feedToRx (ackRxAudio,
                channelSamples,
                chunkSamples,
                decodedAck,
                &metrics,
                &decodeError))
    {
      burst.decoded = true;
      burst.metrics = metrics;
    }
  else
    {
      setError (error, decodeError.c_str ());
    }

  *trace = makeAckTrace (ack, burst);
  trace->sourceSequence = sourceSequence;
  trace->sourceAttempt = sourceAttempt;
  return true;
}

bool appendAndDecodeW500Ack (Frame const& ack,
                             std::uint16_t sourceSequence,
                             int sourceAttempt,
                             std::uint64_t ackStartMs,
                             W500WaveformConfig const& config,
                             std::size_t guardBeforeSamples,
                             W500TxAudioBuffer* ackAudio,
                             W500RxAudioBuffer* ackRxAudio,
                             AudioAckTrace* trace,
                             Frame* decodedAck,
                             std::size_t chunkSamples,
                             float noiseAmplitude,
                             bool dropAck,
                             std::string* error)
{
  if (!ackAudio || !ackRxAudio || !trace || !decodedAck)
    {
      setError (error, "missing W500 ACK audio output");
      return false;
    }

  std::size_t const targetSamples = samplesForMs (ackStartMs, config.sampleRate);
  std::size_t const desiredStart = targetSamples > guardBeforeSamples
      ? targetSamples - guardBeforeSamples
      : 0u;
  if (desiredStart > ackAudio->nextSampleIndex ())
    {
      ackAudio->appendSilence (desiredStart - ackAudio->nextSampleIndex ());
    }

  W500AudioBurstTrace burst;
  if (!ackAudio->appendFrame (ack, 1, config, &burst, error))
    {
      return false;
    }
  if (dropAck)
    {
      burst.dropped = true;
      *trace = makeAckTrace (ack, burst);
      trace->sourceSequence = sourceSequence;
      trace->sourceAttempt = sourceAttempt;
      return true;
    }

  std::vector<float> channelSamples = sliceSamples (
      ackAudio->samples (), burst.startSample, burst.sampleCount);
  addDeterministicNoise (channelSamples, noiseAmplitude);

  W500DecodeMetrics metrics;
  std::string decodeError;
  if (feedToRx (ackRxAudio,
                channelSamples,
                chunkSamples,
                decodedAck,
                &metrics,
                &decodeError))
    {
      burst.decoded = true;
      burst.metrics = metrics;
    }
  else
    {
      setError (error, decodeError.c_str ());
    }

  *trace = makeAckTrace (ack, burst);
  trace->sourceSequence = sourceSequence;
  trace->sourceAttempt = sourceAttempt;
  return true;
}

void addThroughputBurst (AudioThroughputMetrics* metrics,
                         std::uint64_t startMs,
                         std::uint64_t endMs,
                         int attempt,
                         bool dropped,
                         bool decoded)
{
  if (!metrics)
    {
      return;
    }

  if (metrics->burstCount == 0u)
    {
      metrics->firstTransmitMs = startMs;
    }
  std::uint64_t const duration = endMs > startMs ? endMs - startMs : 0u;
  metrics->lastTransmitMs = std::max (metrics->lastTransmitMs, endMs);
  metrics->dataTransmitMs += duration;
  metrics->activeTransmitMs += duration;
  metrics->burstCount += 1u;
  metrics->decodedBurstCount += decoded ? 1u : 0u;
  metrics->droppedBurstCount += dropped ? 1u : 0u;
  metrics->retryBurstCount += attempt > 1 ? 1u : 0u;
}

void addThroughputAck (AudioThroughputMetrics* metrics,
                       AudioAckTrace const& ack)
{
  if (!metrics)
    {
      return;
    }

  if (metrics->burstCount == 0u && metrics->ackBurstCount == 0u)
    {
      metrics->firstTransmitMs = ack.transmitStartMs;
    }
  std::uint64_t const duration = ack.transmitEndMs > ack.transmitStartMs
      ? ack.transmitEndMs - ack.transmitStartMs
      : 0u;
  metrics->lastTransmitMs = std::max (metrics->lastTransmitMs, ack.transmitEndMs);
  metrics->ackTransmitMs += duration;
  metrics->activeTransmitMs += duration;
  metrics->ackBurstCount += 1u;
  metrics->decodedAckBurstCount += ack.decoded ? 1u : 0u;
  metrics->droppedAckBurstCount += ack.dropped ? 1u : 0u;
}

void finishThroughputMetrics (AudioThroughputMetrics* metrics,
                              std::size_t totalSamples,
                              double sampleRate)
{
  if (!metrics)
    {
      return;
    }

  metrics->sessionDurationMs = std::max (
      metrics->lastTransmitMs, msForSamples (totalSamples, sampleRate));
  if (metrics->sessionDurationMs > 0u)
    {
      metrics->effectivePayloadBytesPerSecond =
          double (metrics->payloadBytes) * 1000.0
          / double (metrics->sessionDurationMs);
      metrics->effectivePayloadBitsPerSecond =
          metrics->effectivePayloadBytesPerSecond * 8.0;
    }
  if (metrics->activeTransmitMs > 0u)
    {
      metrics->activePayloadBytesPerSecond =
          double (metrics->payloadBytes) * 1000.0
          / double (metrics->activeTransmitMs);
      metrics->activePayloadBitsPerSecond =
          metrics->activePayloadBytesPerSecond * 8.0;
    }
  if (metrics->sessionDurationMs > 0u)
    {
      metrics->channelUtilization =
          std::min (1.0,
                    double (metrics->activeTransmitMs)
                    / double (metrics->sessionDurationMs));
    }
}

AudioThroughputMetrics makeThroughputMetrics (
    Profile profile,
    std::size_t payloadBytes,
    std::vector<W2300AudioBurstTrace> const& bursts,
    std::vector<AudioAckTrace> const& ackBursts,
    std::size_t totalSamples,
    double sampleRate)
{
  AudioThroughputMetrics metrics;
  metrics.profile = profile;
  metrics.payloadBytes = payloadBytes;
  for (W2300AudioBurstTrace const& burst : bursts)
    {
      addThroughputBurst (&metrics,
                          burst.transmitStartMs,
                          burst.transmitEndMs,
                          burst.attempt,
                          burst.dropped,
                          burst.decoded);
    }
  for (AudioAckTrace const& ack : ackBursts)
    {
      addThroughputAck (&metrics, ack);
    }
  finishThroughputMetrics (&metrics, totalSamples, sampleRate);
  return metrics;
}

AudioThroughputMetrics makeThroughputMetrics (
    Profile profile,
    std::size_t payloadBytes,
    std::vector<WideAudioBurstTrace> const& bursts,
    std::vector<AudioAckTrace> const& ackBursts,
    std::size_t totalSamples,
    double sampleRate)
{
  AudioThroughputMetrics metrics;
  metrics.profile = profile;
  metrics.payloadBytes = payloadBytes;
  for (WideAudioBurstTrace const& burst : bursts)
    {
      addThroughputBurst (&metrics,
                          burst.transmitStartMs,
                          burst.transmitEndMs,
                          burst.attempt,
                          burst.dropped,
                          burst.decoded);
    }
  for (AudioAckTrace const& ack : ackBursts)
    {
      addThroughputAck (&metrics, ack);
    }
  finishThroughputMetrics (&metrics, totalSamples, sampleRate);
  return metrics;
}

std::vector<Frame> makeWideFrames (Profile profile,
                                   FrameType frameType,
                                   std::uint16_t sessionId,
                                   std::vector<std::uint8_t> const& message)
{
  std::size_t const capacity = std::max<std::size_t> (
      1u, profilePayloadCapacity (profile));
  std::size_t const frameCount = std::max<std::size_t> (
      1u, (message.size () + capacity - 1u) / capacity);

  std::vector<Frame> frames;
  frames.reserve (frameCount);
  for (std::size_t index = 0; index < frameCount; ++index)
    {
      std::size_t const begin = index * capacity;
      std::size_t const end = std::min (message.size (), begin + capacity);

      Frame frame;
      frame.type = frameType;
      frame.profile = profile;
      frame.sessionId = sessionId;
      frame.sequence = static_cast<std::uint16_t> (index);
      if (begin < end)
        {
          frame.payload.assign (
              message.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (begin),
              message.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (end));
        }
      if (index + 1u == frameCount)
        {
          frame.flags = static_cast<std::uint8_t> (
              frame.flags | FlagEndOfMessage);
        }
      frames.push_back (frame);
    }
  return frames;
}
}

W500TxAudioBuffer::W500TxAudioBuffer (std::size_t guardBeforeSamples,
                                      std::size_t guardAfterSamples,
                                      std::size_t interBurstGapSamples)
  : m_guardBeforeSamples {guardBeforeSamples}
  , m_guardAfterSamples {guardAfterSamples}
  , m_interBurstGapSamples {interBurstGapSamples}
{
}

bool W500TxAudioBuffer::appendFrame (Frame const& frame,
                                     int attempt,
                                     W500WaveformConfig const& config,
                                     W500AudioBurstTrace* trace,
                                     std::string* error)
{
  if (!trace)
    {
      setError (error, "missing W500 audio trace output");
      return false;
    }

  if (!m_samples.empty () && m_interBurstGapSamples > 0)
    {
      m_samples.insert (m_samples.end (), m_interBurstGapSamples, 0.0f);
    }

  std::size_t const start = m_samples.size ();
  if (m_guardBeforeSamples > 0)
    {
      m_samples.insert (m_samples.end (), m_guardBeforeSamples, 0.0f);
    }

  std::vector<float> const wave = generateW500FrameWaveform (frame, config, error);
  if (wave.empty ())
    {
      return false;
    }
  m_samples.insert (m_samples.end (), wave.begin (), wave.end ());

  if (m_guardAfterSamples > 0)
    {
      m_samples.insert (m_samples.end (), m_guardAfterSamples, 0.0f);
    }

  trace->sequence = frame.sequence;
  trace->attempt = attempt;
  trace->startSample = start;
  trace->sampleCount = m_samples.size () - start;
  trace->transmitStartMs = msForSamples (trace->startSample, config.sampleRate);
  trace->transmitEndMs = msForSamples (trace->startSample + trace->sampleCount,
                                       config.sampleRate);
  trace->dropped = false;
  trace->decoded = false;
  trace->metrics = W500DecodeMetrics {};
  return true;
}

std::vector<float> const& W500TxAudioBuffer::samples () const
{
  return m_samples;
}

std::size_t W500TxAudioBuffer::nextSampleIndex () const
{
  return m_samples.size ();
}

void W500TxAudioBuffer::appendSilence (std::size_t samples)
{
  if (samples > 0)
    {
      m_samples.insert (m_samples.end (), samples, 0.0f);
    }
}

void W500TxAudioBuffer::clear ()
{
  m_samples.clear ();
}

W500RxAudioBuffer::W500RxAudioBuffer (W500WaveformConfig const& config)
  : m_config {config}
{
}

void W500RxAudioBuffer::append (std::vector<float> const& samples)
{
  m_buffer.insert (m_buffer.end (), samples.begin (), samples.end ());
}

bool W500RxAudioBuffer::decodeNext (Frame* frame,
                                    W500DecodeMetrics* metrics,
                                    std::string* error)
{
  if (!frame || !metrics)
    {
      setError (error, "missing W500 RX decode output");
      return false;
    }

  W500DecodeMetrics decodedMetrics;
  Frame decodedFrame;
  if (!decodeW500FrameWaveformWithMetrics (
          m_buffer, &decodedFrame, &decodedMetrics, m_config, error))
    {
      return false;
    }

  int const nsps = samplesPerSymbol (m_config);
  if (nsps <= 0)
    {
      setError (error, "invalid W500 RX sample rate");
      return false;
    }
  std::size_t const consumed = std::min (
      m_buffer.size (),
      decodedMetrics.sampleOffset
          + decodedMetrics.symbolCount * static_cast<std::size_t> (nsps));
  m_buffer.erase (
      m_buffer.begin (),
      m_buffer.begin () + static_cast<std::vector<float>::difference_type> (consumed));

  *frame = decodedFrame;
  *metrics = decodedMetrics;
  return true;
}

std::size_t W500RxAudioBuffer::bufferedSamples () const
{
  return m_buffer.size ();
}

void W500RxAudioBuffer::clear ()
{
  m_buffer.clear ();
}

void W500RxAudioBuffer::dropToLastSamples (std::size_t maxSamples)
{
  if (m_buffer.size () > maxSamples)
    {
      m_buffer.erase (
          m_buffer.begin (),
          m_buffer.begin ()
              + static_cast<std::vector<float>::difference_type> (
                  m_buffer.size () - maxSamples));
    }
}

W2300TxAudioBuffer::W2300TxAudioBuffer (std::size_t guardBeforeSamples,
                                        std::size_t guardAfterSamples,
                                        std::size_t interBurstGapSamples)
  : m_guardBeforeSamples {guardBeforeSamples}
  , m_guardAfterSamples {guardAfterSamples}
  , m_interBurstGapSamples {interBurstGapSamples}
{
}

bool W2300TxAudioBuffer::appendFrame (Frame const& frame,
                                      int attempt,
                                      W2300WaveformConfig const& config,
                                      W2300AudioBurstTrace* trace,
                                      std::string* error)
{
  if (!trace)
    {
      setError (error, "missing W2300 audio trace output");
      return false;
    }

  if (!m_samples.empty () && m_interBurstGapSamples > 0)
    {
      m_samples.insert (m_samples.end (), m_interBurstGapSamples, 0.0f);
    }

  std::size_t const start = m_samples.size ();
  if (m_guardBeforeSamples > 0)
    {
      m_samples.insert (m_samples.end (), m_guardBeforeSamples, 0.0f);
    }

  std::vector<float> const wave = generateW2300FrameWaveform (frame, config, error);
  if (wave.empty ())
    {
      return false;
    }
  m_samples.insert (m_samples.end (), wave.begin (), wave.end ());

  if (m_guardAfterSamples > 0)
    {
      m_samples.insert (m_samples.end (), m_guardAfterSamples, 0.0f);
    }

  trace->sequence = frame.sequence;
  trace->attempt = attempt;
  trace->rateMode = config.rateMode;
  trace->startSample = start;
  trace->sampleCount = m_samples.size () - start;
  trace->transmitStartMs = msForSamples (trace->startSample, config.sampleRate);
  trace->transmitEndMs = msForSamples (trace->startSample + trace->sampleCount,
                                       config.sampleRate);
  trace->dropped = false;
  trace->decoded = false;
  trace->metrics = W2300DecodeMetrics {};
  return true;
}

std::vector<float> const& W2300TxAudioBuffer::samples () const
{
  return m_samples;
}

std::size_t W2300TxAudioBuffer::nextSampleIndex () const
{
  return m_samples.size ();
}

void W2300TxAudioBuffer::appendSilence (std::size_t samples)
{
  if (samples > 0)
    {
      m_samples.insert (m_samples.end (), samples, 0.0f);
    }
}

void W2300TxAudioBuffer::clear ()
{
  m_samples.clear ();
}

W2300RxAudioBuffer::W2300RxAudioBuffer (W2300WaveformConfig const& config)
  : m_config {config}
{
}

void W2300RxAudioBuffer::append (std::vector<float> const& samples)
{
  m_buffer.insert (m_buffer.end (), samples.begin (), samples.end ());
}

bool W2300RxAudioBuffer::decodeNext (Frame* frame,
                                     W2300DecodeMetrics* metrics,
                                     std::string* error)
{
  if (!frame || !metrics)
    {
      setError (error, "missing W2300 RX decode output");
      return false;
    }

  int const nsps = samplesPerSymbol (m_config);
  if (nsps <= 0)
    {
      setError (error, "invalid W2300 RX sample rate");
      return false;
    }

  // Runtime TX/RX paths include a short silent guard before wide bursts.  The
  // W2300 acquisition is intentionally bounded for CPU reasons, so strip only
  // clear leading silence.  Keep the resulting start aligned to the decoder's
  // symbol phase search; cutting into the first symbol makes clean guarded
  // bursts look like "burst not found".
  constexpr float kLeadingSilencePeak = 0.000001f;
  std::size_t firstEnergy = 0u;
  while (firstEnergy < m_buffer.size ()
         && std::fabs (m_buffer[firstEnergy]) < kLeadingSilencePeak)
    {
      ++firstEnergy;
    }
  std::size_t const nspsSize = static_cast<std::size_t> (nsps);
  std::size_t const drop = firstEnergy > nspsSize
      ? firstEnergy - (firstEnergy % nspsSize)
      : 0u;
  if (drop > 0u)
    {
      m_buffer.erase (
          m_buffer.begin (),
          m_buffer.begin ()
              + static_cast<std::vector<float>::difference_type> (drop));
    }

  W2300DecodeMetrics decodedMetrics;
  Frame decodedFrame;
  if (!decodeW2300FrameWaveformWithMetrics (
          m_buffer, &decodedFrame, &decodedMetrics, m_config, error))
    {
      return false;
    }

  std::size_t const consumed = std::min (
      m_buffer.size (),
      decodedMetrics.sampleOffset
          + decodedMetrics.symbolCount * static_cast<std::size_t> (nsps));
  m_buffer.erase (
      m_buffer.begin (),
      m_buffer.begin () + static_cast<std::vector<float>::difference_type> (consumed));

  *frame = decodedFrame;
  *metrics = decodedMetrics;
  return true;
}

std::size_t W2300RxAudioBuffer::bufferedSamples () const
{
  return m_buffer.size ();
}

void W2300RxAudioBuffer::clear ()
{
  m_buffer.clear ();
}

void W2300RxAudioBuffer::dropToLastSamples (std::size_t maxSamples)
{
  if (m_buffer.size () > maxSamples)
    {
      m_buffer.erase (
          m_buffer.begin (),
          m_buffer.begin ()
              + static_cast<std::vector<float>::difference_type> (
                  m_buffer.size () - maxSamples));
    }
}

W2300AudioPipelineResult runW2300AudioPipeline (
    std::vector<std::uint8_t> const& message,
    std::uint16_t sessionId,
    W2300AudioPipelineOptions const& options)
{
  W2300AudioPipelineResult result;

  W2300RateMode initialRateMode = options.initialRateMode;
  if (options.performHandshake)
    {
      result.handshakeAttempted = true;

      Frame const hello = makeHelloFrame (sessionId, options.initiatorCapabilities);
      result.handshakeFrames.push_back (hello);

      Frame helloAck;
      NegotiatedLink responderNegotiated;
      std::string error;
      bool const answered = answerHelloFrame (hello,
                                              options.responderCapabilities,
                                              &helloAck,
                                              &responderNegotiated,
                                              &error);
      result.handshakeFrames.push_back (helloAck);
      result.negotiatedLink = responderNegotiated;

      LinkCapabilities parsedResponder;
      NegotiatedLink initiatorNegotiated;
      if (!parseHelloAckFrame (helloAck, &parsedResponder, &initiatorNegotiated, &error))
        {
          result.failed = true;
          result.error = error.empty ()
              ? "failed to parse FT2-Link HELLO_ACK"
              : error;
          return result;
        }

      result.negotiatedLink = initiatorNegotiated;
      result.handshakeAccepted = answered && initiatorNegotiated.accepted;
      if (!result.handshakeAccepted)
        {
          result.failed = true;
          result.error = error.empty ()
              ? "FT2-Link handshake rejected"
              : error;
          return result;
        }

      if (initiatorNegotiated.profile != Profile::Wide2300)
        {
          result.failed = true;
          result.error = "W2300 audio pipeline requires a negotiated W2300 profile";
          return result;
        }

      initialRateMode = initiatorNegotiated.w2300RateMode;
    }

  OutboundTransfer tx {Profile::Wide2300, sessionId, message};
  tx.setWindowSize (options.windowSize);
  tx.setRetryMs (options.retryMs);
  tx.setMaxAttempts (options.maxAttempts);

  InboundTransfer rx {Profile::Wide2300, sessionId};
  W2300RateController rateController {initialRateMode};
  W2300TxAudioBuffer txAudio {
    options.guardBeforeSamples,
    options.guardAfterSamples,
    options.interBurstGapSamples
  };
  W2300TxAudioBuffer ackAudio {
    options.guardBeforeSamples,
    options.guardAfterSamples,
    options.interBurstGapSamples
  };
  W2300RxAudioBuffer rxAudio {options.rxConfig};
  W2300RxAudioBuffer ackRxAudio {options.rxConfig};

  std::uint64_t now = 0;
  std::uint64_t localTxBusyUntilMs = 0;
  for (std::size_t iteration = 0;
       iteration < options.maxIterations && !tx.complete () && !tx.failed ();
       ++iteration)
    {
      if (options.listenBeforeTransmit)
        {
          W2300ChannelDeferralTrace deferral;
          if (channelBusyAt (options, now, localTxBusyUntilMs, &deferral))
            {
              result.deferrals.push_back (deferral);
              std::size_t const targetSamples = samplesForMs (
                  deferral.resumeMs, options.txConfig.sampleRate);
              if (targetSamples > txAudio.nextSampleIndex ())
                {
                  txAudio.appendSilence (targetSamples - txAudio.nextSampleIndex ());
                }
              now = std::max<std::uint64_t> (now + 1u, deferral.resumeMs);
              continue;
            }
        }

      std::vector<Frame> const frames = tx.framesToSend (now);
      if (frames.empty ())
        {
          now += std::max<std::uint64_t> (1u, options.retryMs);
          continue;
        }

      for (Frame const& frame : frames)
        {
          if (options.listenBeforeTransmit)
            {
              W2300ChannelDeferralTrace deferral;
              if (channelBusyAt (options, now, localTxBusyUntilMs, &deferral))
                {
                  result.deferrals.push_back (deferral);
                  std::size_t const targetSamples = samplesForMs (
                      deferral.resumeMs, options.txConfig.sampleRate);
                  if (targetSamples > txAudio.nextSampleIndex ())
                    {
                      txAudio.appendSilence (targetSamples - txAudio.nextSampleIndex ());
                    }
                  now = std::max<std::uint64_t> (now + 1u, deferral.resumeMs);
                  break;
                }
            }

          int const attempt = tx.attemptsForSequence (frame.sequence);
          W2300WaveformConfig const txConfig = rateController.configForAttempt (
              attempt, options.txConfig);

          std::size_t const targetSamples = samplesForMs (now, txConfig.sampleRate);
          if (targetSamples > txAudio.nextSampleIndex ())
            {
              txAudio.appendSilence (targetSamples - txAudio.nextSampleIndex ());
            }

          W2300AudioBurstTrace trace;
          std::string error;
          if (!txAudio.appendFrame (frame, attempt, txConfig, &trace, &error))
            {
              result.error = error.empty () ? "failed to append W2300 audio frame" : error;
              result.failed = true;
              return result;
            }

          if (shouldDropFirstAttempt (options.dropFirstAttemptSequences,
                                      frame.sequence,
                                      attempt))
            {
              trace.dropped = true;
              result.bursts.push_back (trace);
              localTxBusyUntilMs = std::max (localTxBusyUntilMs, trace.transmitEndMs);
              now = trace.transmitEndMs;
              continue;
            }

          std::vector<float> channelSamples = sliceSamples (
              txAudio.samples (), trace.startSample, trace.sampleCount);
          addDeterministicNoise (channelSamples, options.noiseAmplitude);

          Frame decoded;
          W2300DecodeMetrics metrics;
          std::uint64_t nextAvailableMs = trace.transmitEndMs;
          if (feedToRx (&rxAudio,
                        channelSamples,
                        options.rxChunkSamples,
                        &decoded,
                        &metrics,
                        &error))
            {
              trace.decoded = true;
              trace.metrics = metrics;
              rx.receive (decoded);
              Frame const ack = rx.makeAckFrame ();
              if (options.modelAckAudio)
                {
                  W2300WaveformConfig ackConfig = options.txConfig;
                  ackConfig.rateMode = options.ackRateMode;
                  AudioAckTrace ackTrace;
                  Frame decodedAck;
                  std::string ackError;
                  bool const dropAck = shouldDropFirstAttempt (
                      options.dropFirstAckForSequences, decoded.sequence, attempt);
                  if (!appendAndDecodeW2300Ack (
                          ack,
                          decoded.sequence,
                          attempt,
                          trace.transmitEndMs + options.dataToAckTurnaroundMs,
                          ackConfig,
                          options.guardBeforeSamples,
                          &ackAudio,
                          &ackRxAudio,
                          &ackTrace,
                          &decodedAck,
                          options.rxChunkSamples,
                          options.noiseAmplitude,
                          dropAck,
                          &ackError))
                    {
                      result.error = ackError.empty ()
                          ? "failed to append W2300 ACK audio frame"
                          : ackError;
                      result.failed = true;
                      return result;
                    }
                  if (ackTrace.decoded)
                    {
                      tx.handleAckFrame (decodedAck);
                    }
                  else
                    {
                      result.error = ackError;
                    }
                  result.ackBursts.push_back (ackTrace);
                  nextAvailableMs = ackTrace.transmitEndMs
                      + options.ackToDataTurnaroundMs;
                }
              else
                {
                  tx.handleAckFrame (ack);
                }
              rateController.observe (metrics);
            }
          else
            {
              result.error = error;
            }
          result.bursts.push_back (trace);
          localTxBusyUntilMs = std::max (localTxBusyUntilMs, nextAvailableMs);
          now = nextAvailableMs;
        }
    }

  result.totalSamples = std::max (txAudio.nextSampleIndex (), ackAudio.nextSampleIndex ());
  result.complete = tx.complete () && rx.complete ();
  result.failed = tx.failed () || !result.complete;
  if (rx.complete ())
    {
      result.receivedMessage = rx.message ();
    }
  if (!result.complete && result.error.empty ())
    {
      result.error = "W2300 audio pipeline did not complete";
    }
  result.throughput = makeThroughputMetrics (Profile::Wide2300,
                                             message.size (),
                                             result.bursts,
                                             result.ackBursts,
                                             result.totalSamples,
                                             options.txConfig.sampleRate);
  return result;
}

namespace
{
WideAudioBurstTrace makeWideTrace (W500AudioBurstTrace const& burst)
{
  WideAudioBurstTrace trace;
  trace.profile = Profile::Wide500;
  trace.sequence = burst.sequence;
  trace.attempt = burst.attempt;
  trace.transmitStartMs = burst.transmitStartMs;
  trace.transmitEndMs = burst.transmitEndMs;
  trace.startSample = burst.startSample;
  trace.sampleCount = burst.sampleCount;
  trace.dropped = burst.dropped;
  trace.decoded = burst.decoded;
  trace.w500Metrics = burst.metrics;
  return trace;
}

WideAudioBurstTrace makeWideTrace (W2300AudioBurstTrace const& burst)
{
  WideAudioBurstTrace trace;
  trace.profile = Profile::Wide2300;
  trace.sequence = burst.sequence;
  trace.attempt = burst.attempt;
  trace.w2300RateMode = burst.rateMode;
  trace.transmitStartMs = burst.transmitStartMs;
  trace.transmitEndMs = burst.transmitEndMs;
  trace.startSample = burst.startSample;
  trace.sampleCount = burst.sampleCount;
  trace.dropped = burst.dropped;
  trace.decoded = burst.decoded;
  trace.w2300Metrics = burst.metrics;
  return trace;
}

void mergeW2300Result (W2300AudioPipelineResult const& source,
                       WideAudioPipelineResult* target)
{
  if (!target)
    {
      return;
    }

  target->complete = source.complete;
  target->failed = source.failed;
  target->receivedMessage = source.receivedMessage;
  target->ackBursts = source.ackBursts;
  target->deferrals = source.deferrals;
  target->throughput = source.throughput;
  target->totalSamples = source.totalSamples;
  target->error = source.error;
  target->bursts.reserve (target->bursts.size () + source.bursts.size ());
  for (W2300AudioBurstTrace const& burst : source.bursts)
    {
      target->bursts.push_back (makeWideTrace (burst));
    }
}
}

WideAudioPipelineResult runWideAudioPipeline (
    std::vector<std::uint8_t> const& message,
    std::uint16_t sessionId,
    WideAudioPipelineOptions const& options)
{
  WideAudioPipelineResult result;

  Profile selectedProfile = options.profile;
  W2300RateMode selectedW2300RateMode = options.initialW2300RateMode;
  if (options.performHandshake)
    {
      result.handshakeAttempted = true;

      Frame const hello = makeHelloFrame (sessionId, options.initiatorCapabilities);
      result.handshakeFrames.push_back (hello);

      Frame helloAck;
      NegotiatedLink responderNegotiated;
      std::string error;
      bool const answered = answerHelloFrame (hello,
                                              options.responderCapabilities,
                                              &helloAck,
                                              &responderNegotiated,
                                              &error);
      result.handshakeFrames.push_back (helloAck);
      result.negotiatedLink = responderNegotiated;

      LinkCapabilities parsedResponder;
      NegotiatedLink initiatorNegotiated;
      if (!parseHelloAckFrame (helloAck, &parsedResponder, &initiatorNegotiated, &error))
        {
          result.failed = true;
          result.error = error.empty ()
              ? "failed to parse FT2-Link HELLO_ACK"
              : error;
          return result;
        }

      result.negotiatedLink = initiatorNegotiated;
      result.handshakeAccepted = answered && initiatorNegotiated.accepted;
      if (!result.handshakeAccepted)
        {
          result.failed = true;
          result.error = error.empty ()
              ? "FT2-Link handshake rejected"
              : error;
          return result;
        }

      selectedProfile = initiatorNegotiated.profile;
      selectedW2300RateMode = initiatorNegotiated.w2300RateMode;
    }
  else
    {
      result.negotiatedLink.accepted = true;
      result.negotiatedLink.profile = selectedProfile;
      result.negotiatedLink.w2300RateMode = selectedW2300RateMode;
    }

  if (selectedProfile == Profile::Wide2300)
    {
      W2300AudioPipelineOptions w2300Options;
      w2300Options.windowSize = options.windowSize;
      w2300Options.retryMs = options.retryMs;
      w2300Options.maxAttempts = options.maxAttempts;
      w2300Options.maxIterations = options.maxIterations;
      w2300Options.initialRateMode = selectedW2300RateMode;
      w2300Options.txConfig = options.w2300TxConfig;
      w2300Options.rxConfig = options.w2300RxConfig;
      w2300Options.guardBeforeSamples = options.guardBeforeSamples;
      w2300Options.guardAfterSamples = options.guardAfterSamples;
      w2300Options.interBurstGapSamples = options.interBurstGapSamples;
      w2300Options.rxChunkSamples = options.rxChunkSamples;
      w2300Options.noiseAmplitude = options.noiseAmplitude;
      w2300Options.listenBeforeTransmit = options.listenBeforeTransmit;
      w2300Options.busyBackoffMs = options.busyBackoffMs;
      w2300Options.externalBusyWindows = options.externalBusyWindows;
      w2300Options.rxBusyWindows = options.rxBusyWindows;
      w2300Options.dropFirstAttemptSequences = options.dropFirstAttemptSequences;
      w2300Options.performHandshake = false;
      w2300Options.modelAckAudio = options.modelAckAudio;
      w2300Options.dataToAckTurnaroundMs = options.dataToAckTurnaroundMs;
      w2300Options.ackToDataTurnaroundMs = options.ackToDataTurnaroundMs;
      w2300Options.ackRateMode = options.w2300AckRateMode;
      w2300Options.dropFirstAckForSequences = options.dropFirstAckForSequences;

      W2300AudioPipelineResult const w2300Result = runW2300AudioPipeline (
          message, sessionId, w2300Options);
      mergeW2300Result (w2300Result, &result);
      return result;
    }

  if (selectedProfile != Profile::Wide500)
    {
      result.failed = true;
      result.error = "wide audio pipeline requires a W500 or W2300 profile";
      return result;
    }

  OutboundTransfer tx {Profile::Wide500, sessionId, message};
  tx.setWindowSize (options.windowSize);
  tx.setRetryMs (options.retryMs);
  tx.setMaxAttempts (options.maxAttempts);

  InboundTransfer rx {Profile::Wide500, sessionId};
  W500TxAudioBuffer txAudio {
    options.guardBeforeSamples,
    options.guardAfterSamples,
    options.interBurstGapSamples
  };
  W500TxAudioBuffer ackAudio {
    options.guardBeforeSamples,
    options.guardAfterSamples,
    options.interBurstGapSamples
  };
  W500RxAudioBuffer rxAudio {options.w500RxConfig};
  W500RxAudioBuffer ackRxAudio {options.w500RxConfig};

  std::uint64_t now = 0;
  std::uint64_t localTxBusyUntilMs = 0;
  for (std::size_t iteration = 0;
       iteration < options.maxIterations && !tx.complete () && !tx.failed ();
       ++iteration)
    {
      if (options.listenBeforeTransmit)
        {
          W2300ChannelDeferralTrace deferral;
          if (channelBusyAt (options, now, localTxBusyUntilMs, &deferral))
            {
              result.deferrals.push_back (deferral);
              std::size_t const targetSamples = samplesForMs (
                  deferral.resumeMs, options.w500TxConfig.sampleRate);
              if (targetSamples > txAudio.nextSampleIndex ())
                {
                  txAudio.appendSilence (targetSamples - txAudio.nextSampleIndex ());
                }
              now = std::max<std::uint64_t> (now + 1u, deferral.resumeMs);
              continue;
            }
        }

      std::vector<Frame> const frames = tx.framesToSend (now);
      if (frames.empty ())
        {
          now += std::max<std::uint64_t> (1u, options.retryMs);
          continue;
        }

      for (Frame const& frame : frames)
        {
          if (options.listenBeforeTransmit)
            {
              W2300ChannelDeferralTrace deferral;
              if (channelBusyAt (options, now, localTxBusyUntilMs, &deferral))
                {
                  result.deferrals.push_back (deferral);
                  std::size_t const targetSamples = samplesForMs (
                      deferral.resumeMs, options.w500TxConfig.sampleRate);
                  if (targetSamples > txAudio.nextSampleIndex ())
                    {
                      txAudio.appendSilence (targetSamples - txAudio.nextSampleIndex ());
                    }
                  now = std::max<std::uint64_t> (now + 1u, deferral.resumeMs);
                  break;
                }
            }

          int const attempt = tx.attemptsForSequence (frame.sequence);
          std::size_t const targetSamples = samplesForMs (
              now, options.w500TxConfig.sampleRate);
          if (targetSamples > txAudio.nextSampleIndex ())
            {
              txAudio.appendSilence (targetSamples - txAudio.nextSampleIndex ());
            }

          W500AudioBurstTrace burst;
          std::string error;
          if (!txAudio.appendFrame (
                  frame, attempt, options.w500TxConfig, &burst, &error))
            {
              result.error = error.empty () ? "failed to append W500 audio frame" : error;
              result.failed = true;
              return result;
            }

          if (shouldDropFirstAttempt (options.dropFirstAttemptSequences,
                                      frame.sequence,
                                      attempt))
            {
              burst.dropped = true;
              result.bursts.push_back (makeWideTrace (burst));
              localTxBusyUntilMs = std::max (localTxBusyUntilMs, burst.transmitEndMs);
              now = burst.transmitEndMs;
              continue;
            }

          std::vector<float> channelSamples = sliceSamples (
              txAudio.samples (), burst.startSample, burst.sampleCount);
          addDeterministicNoise (channelSamples, options.noiseAmplitude);

          Frame decoded;
          W500DecodeMetrics metrics;
          std::uint64_t nextAvailableMs = burst.transmitEndMs;
          if (feedToRx (&rxAudio,
                        channelSamples,
                        options.rxChunkSamples,
                        &decoded,
                        &metrics,
                        &error))
            {
              burst.decoded = true;
              burst.metrics = metrics;
              rx.receive (decoded);
              Frame const ack = rx.makeAckFrame ();
              if (options.modelAckAudio)
                {
                  AudioAckTrace ackTrace;
                  Frame decodedAck;
                  std::string ackError;
                  bool const dropAck = shouldDropFirstAttempt (
                      options.dropFirstAckForSequences, decoded.sequence, attempt);
                  if (!appendAndDecodeW500Ack (
                          ack,
                          decoded.sequence,
                          attempt,
                          burst.transmitEndMs + options.dataToAckTurnaroundMs,
                          options.w500TxConfig,
                          options.guardBeforeSamples,
                          &ackAudio,
                          &ackRxAudio,
                          &ackTrace,
                          &decodedAck,
                          options.rxChunkSamples,
                          options.noiseAmplitude,
                          dropAck,
                          &ackError))
                    {
                      result.error = ackError.empty ()
                          ? "failed to append W500 ACK audio frame"
                          : ackError;
                      result.failed = true;
                      return result;
                    }
                  if (ackTrace.decoded)
                    {
                      tx.handleAckFrame (decodedAck);
                    }
                  else
                    {
                      result.error = ackError;
                    }
                  result.ackBursts.push_back (ackTrace);
                  nextAvailableMs = ackTrace.transmitEndMs
                      + options.ackToDataTurnaroundMs;
                }
              else
                {
                  tx.handleAckFrame (ack);
                }
            }
          else
            {
              result.error = error;
            }

          result.bursts.push_back (makeWideTrace (burst));
          localTxBusyUntilMs = std::max (localTxBusyUntilMs, nextAvailableMs);
          now = nextAvailableMs;
        }
    }

  result.totalSamples = std::max (txAudio.nextSampleIndex (), ackAudio.nextSampleIndex ());
  result.complete = tx.complete () && rx.complete ();
  result.failed = tx.failed () || !result.complete;
  if (rx.complete ())
    {
      result.receivedMessage = rx.message ();
    }
  if (!result.complete && result.error.empty ())
    {
      result.error = "W500 audio pipeline did not complete";
    }
  result.throughput = makeThroughputMetrics (Profile::Wide500,
                                             message.size (),
                                             result.bursts,
                                             result.ackBursts,
                                             result.totalSamples,
                                             options.w500TxConfig.sampleRate);
  return result;
}

WideTxAudioPlan buildWideTxAudioPlan (
    std::vector<std::uint8_t> const& message,
    std::uint16_t sessionId,
    WideTxAudioPlanOptions const& options)
{
  return buildWideTxAudioPlanForFrames (
      makeWideFrames (options.profile, options.frameType, sessionId, message),
      options);
}

WideTxAudioPlan buildWideTxAudioPlanForFrames (
    std::vector<Frame> const& frames,
    WideTxAudioPlanOptions const& options)
{
  WideTxAudioPlan plan;
  plan.profile = options.profile;
  plan.w2300RateMode = options.w2300RateMode;
  plan.sampleRate = options.sampleRate;

  if (frames.empty ())
    {
      plan.error = "FT2-Link TX audio plan frame window is empty";
      return plan;
    }
  if (options.sampleRate <= 0.0)
    {
      plan.error = "FT2-Link TX audio plan sample rate is invalid";
      return plan;
    }
  if (options.profile != Profile::Wide500
      && options.profile != Profile::Wide2300)
    {
      plan.error = "FT2-Link TX audio plan requires W500 or W2300";
      return plan;
    }

  plan.frames = frames;
  std::size_t payloadBytes = 0u;
  for (Frame const& frame : plan.frames)
    {
      if (frame.profile != options.profile)
        {
          plan.error = "FT2-Link TX audio plan frame profile mismatch";
          return plan;
        }
      payloadBytes += frame.payload.size ();
    }
  std::vector<AudioAckTrace> noAckBursts;

  if (options.profile == Profile::Wide2300)
    {
      W2300WaveformConfig config = options.w2300TxConfig;
      config.sampleRate = options.sampleRate;
      config.rateMode = options.w2300RateMode;

      W2300TxAudioBuffer txAudio {
        options.guardBeforeSamples,
        options.guardAfterSamples,
        options.interBurstGapSamples
      };

      for (Frame const& frame : plan.frames)
        {
          W2300AudioBurstTrace burst;
          if (!txAudio.appendFrame (frame, 1, config, &burst, &plan.error))
            {
              return plan;
            }
          plan.bursts.push_back (makeWideTrace (burst));
        }

      plan.samples = txAudio.samples ();
    }
  else
    {
      W500WaveformConfig config = options.w500TxConfig;
      config.sampleRate = options.sampleRate;

      W500TxAudioBuffer txAudio {
        options.guardBeforeSamples,
        options.guardAfterSamples,
        options.interBurstGapSamples
      };

      for (Frame const& frame : plan.frames)
        {
          W500AudioBurstTrace burst;
          if (!txAudio.appendFrame (frame, 1, config, &burst, &plan.error))
            {
              return plan;
            }
          plan.bursts.push_back (makeWideTrace (burst));
        }

      plan.samples = txAudio.samples ();
    }

  plan.totalSamples = plan.samples.size ();
  plan.throughput = makeThroughputMetrics (options.profile,
                                           payloadBytes,
                                           plan.bursts,
                                           noAckBursts,
                                           plan.totalSamples,
                                           options.sampleRate);
  plan.ok = !plan.samples.empty () && !plan.bursts.empty ();
  if (!plan.ok && plan.error.empty ())
    {
      plan.error = "FT2-Link TX audio plan produced no audio";
    }
  return plan;
}

}
}
