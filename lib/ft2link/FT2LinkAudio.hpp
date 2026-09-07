#ifndef DECODIUM_FT2LINK_AUDIO_HPP
#define DECODIUM_FT2LINK_AUDIO_HPP

#include "lib/ft2link/FT2LinkHandshake.hpp"
#include "lib/ft2link/FT2LinkWaveform.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace decodium
{
namespace ft2link
{

struct W2300ChannelBusyWindow
{
  std::uint64_t startMs {0};
  std::uint64_t endMs {0};
};

struct W2300ChannelDeferralTrace
{
  std::uint64_t atMs {0};
  std::uint64_t resumeMs {0};
  bool externalBusy {false};
  bool txBusy {false};
  bool rxBusy {false};
};

struct AudioThroughputMetrics
{
  Profile profile {Profile::Narrow};
  std::size_t payloadBytes {0};
  std::size_t burstCount {0};
  std::size_t ackBurstCount {0};
  std::size_t decodedBurstCount {0};
  std::size_t decodedAckBurstCount {0};
  std::size_t droppedBurstCount {0};
  std::size_t droppedAckBurstCount {0};
  std::size_t retryBurstCount {0};
  std::uint64_t firstTransmitMs {0};
  std::uint64_t lastTransmitMs {0};
  std::uint64_t sessionDurationMs {0};
  std::uint64_t dataTransmitMs {0};
  std::uint64_t ackTransmitMs {0};
  std::uint64_t activeTransmitMs {0};
  double effectivePayloadBytesPerSecond {0.0};
  double effectivePayloadBitsPerSecond {0.0};
  double activePayloadBytesPerSecond {0.0};
  double activePayloadBitsPerSecond {0.0};
  double channelUtilization {0.0};
};

struct W2300AudioPipelineOptions
{
  std::size_t windowSize {1};
  std::uint64_t retryMs {1000};
  int maxAttempts {5};
  std::size_t maxIterations {64};
  W2300RateMode initialRateMode {W2300RateMode::Fast};
  W2300WaveformConfig txConfig;
  W2300WaveformConfig rxConfig;
  std::size_t guardBeforeSamples {120};
  std::size_t guardAfterSamples {120};
  std::size_t interBurstGapSamples {240};
  std::size_t rxChunkSamples {0};
  float noiseAmplitude {0.0f};
  bool listenBeforeTransmit {true};
  std::uint64_t busyBackoffMs {250};
  std::vector<W2300ChannelBusyWindow> externalBusyWindows;
  std::vector<W2300ChannelBusyWindow> rxBusyWindows;
  std::vector<std::uint16_t> dropFirstAttemptSequences;
  bool performHandshake {false};
  LinkCapabilities initiatorCapabilities;
  LinkCapabilities responderCapabilities;
  bool modelAckAudio {false};
  std::uint64_t dataToAckTurnaroundMs {250};
  std::uint64_t ackToDataTurnaroundMs {250};
  W2300RateMode ackRateMode {W2300RateMode::Robust};
  std::vector<std::uint16_t> dropFirstAckForSequences;
};

struct W2300AudioBurstTrace
{
  std::uint16_t sequence {0};
  int attempt {0};
  W2300RateMode rateMode {W2300RateMode::Fast};
  std::uint64_t transmitStartMs {0};
  std::uint64_t transmitEndMs {0};
  std::size_t startSample {0};
  std::size_t sampleCount {0};
  bool dropped {false};
  bool decoded {false};
  W2300DecodeMetrics metrics;
};

struct W500AudioBurstTrace
{
  std::uint16_t sequence {0};
  int attempt {0};
  std::uint64_t transmitStartMs {0};
  std::uint64_t transmitEndMs {0};
  std::size_t startSample {0};
  std::size_t sampleCount {0};
  bool dropped {false};
  bool decoded {false};
  W500DecodeMetrics metrics;
};

struct AudioAckTrace
{
  Profile profile {Profile::Narrow};
  std::uint16_t sourceSequence {0};
  int sourceAttempt {0};
  std::uint16_t ackBase {0};
  std::uint16_t ackBitmap {0};
  W2300RateMode w2300RateMode {W2300RateMode::Fast};
  std::uint64_t transmitStartMs {0};
  std::uint64_t transmitEndMs {0};
  std::size_t startSample {0};
  std::size_t sampleCount {0};
  bool dropped {false};
  bool decoded {false};
  W500DecodeMetrics w500Metrics;
  W2300DecodeMetrics w2300Metrics;
};

class W500TxAudioBuffer
{
public:
  W500TxAudioBuffer (std::size_t guardBeforeSamples = 0,
                     std::size_t guardAfterSamples = 0,
                     std::size_t interBurstGapSamples = 0);

  bool appendFrame (Frame const& frame,
                    int attempt,
                    W500WaveformConfig const& config,
                    W500AudioBurstTrace* trace,
                    std::string* error = nullptr);

  std::vector<float> const& samples () const;
  std::size_t nextSampleIndex () const;
  void appendSilence (std::size_t samples);
  void clear ();

private:
  std::size_t m_guardBeforeSamples {0};
  std::size_t m_guardAfterSamples {0};
  std::size_t m_interBurstGapSamples {0};
  std::vector<float> m_samples;
};

class W500RxAudioBuffer
{
public:
  explicit W500RxAudioBuffer (
      W500WaveformConfig const& config = W500WaveformConfig {});

  void append (std::vector<float> const& samples);
  bool decodeNext (Frame* frame,
                   W500DecodeMetrics* metrics,
                   std::string* error = nullptr);
  std::size_t bufferedSamples () const;
  void clear ();
  // 1.0.452 iu8lmc: trim finestra scorrevole PRE-decode (fix freeze FT2-Link wide).
  void dropToLastSamples (std::size_t maxSamples);

private:
  W500WaveformConfig m_config;
  std::vector<float> m_buffer;
};

class W2300TxAudioBuffer
{
public:
  W2300TxAudioBuffer (std::size_t guardBeforeSamples = 0,
                      std::size_t guardAfterSamples = 0,
                      std::size_t interBurstGapSamples = 0);

  bool appendFrame (Frame const& frame,
                    int attempt,
                    W2300WaveformConfig const& config,
                    W2300AudioBurstTrace* trace,
                    std::string* error = nullptr);

  std::vector<float> const& samples () const;
  std::size_t nextSampleIndex () const;
  void appendSilence (std::size_t samples);
  void clear ();

private:
  std::size_t m_guardBeforeSamples {0};
  std::size_t m_guardAfterSamples {0};
  std::size_t m_interBurstGapSamples {0};
  std::vector<float> m_samples;
};

class W2300RxAudioBuffer
{
public:
  explicit W2300RxAudioBuffer (
      W2300WaveformConfig const& config = W2300WaveformConfig {});

  void append (std::vector<float> const& samples);
  bool decodeNext (Frame* frame,
                   W2300DecodeMetrics* metrics,
                   std::string* error = nullptr);
  std::size_t bufferedSamples () const;
  void clear ();
  // 1.0.452 iu8lmc: trim finestra scorrevole PRE-decode (fix freeze FT2-Link wide).
  void dropToLastSamples (std::size_t maxSamples);

private:
  W2300WaveformConfig m_config;
  std::vector<float> m_buffer;
};

struct W2300AudioPipelineResult
{
  bool complete {false};
  bool failed {false};
  bool handshakeAttempted {false};
  bool handshakeAccepted {false};
  NegotiatedLink negotiatedLink;
  std::vector<Frame> handshakeFrames;
  std::vector<std::uint8_t> receivedMessage;
  std::vector<W2300AudioBurstTrace> bursts;
  std::vector<AudioAckTrace> ackBursts;
  std::vector<W2300ChannelDeferralTrace> deferrals;
  AudioThroughputMetrics throughput;
  std::size_t totalSamples {0};
  std::string error;
};

W2300AudioPipelineResult runW2300AudioPipeline (
    std::vector<std::uint8_t> const& message,
    std::uint16_t sessionId,
    W2300AudioPipelineOptions const& options = W2300AudioPipelineOptions {});

struct WideAudioPipelineOptions
{
  Profile profile {Profile::Wide2300};
  std::size_t windowSize {1};
  std::uint64_t retryMs {1000};
  int maxAttempts {5};
  std::size_t maxIterations {64};
  W2300RateMode initialW2300RateMode {W2300RateMode::Fast};
  W500WaveformConfig w500TxConfig;
  W500WaveformConfig w500RxConfig;
  W2300WaveformConfig w2300TxConfig;
  W2300WaveformConfig w2300RxConfig;
  std::size_t guardBeforeSamples {120};
  std::size_t guardAfterSamples {120};
  std::size_t interBurstGapSamples {240};
  std::size_t rxChunkSamples {0};
  float noiseAmplitude {0.0f};
  bool listenBeforeTransmit {true};
  std::uint64_t busyBackoffMs {250};
  std::vector<W2300ChannelBusyWindow> externalBusyWindows;
  std::vector<W2300ChannelBusyWindow> rxBusyWindows;
  std::vector<std::uint16_t> dropFirstAttemptSequences;
  bool performHandshake {true};
  LinkCapabilities initiatorCapabilities;
  LinkCapabilities responderCapabilities;
  bool modelAckAudio {false};
  std::uint64_t dataToAckTurnaroundMs {250};
  std::uint64_t ackToDataTurnaroundMs {250};
  W2300RateMode w2300AckRateMode {W2300RateMode::Robust};
  std::vector<std::uint16_t> dropFirstAckForSequences;
};

struct WideAudioBurstTrace
{
  Profile profile {Profile::Wide2300};
  std::uint16_t sequence {0};
  int attempt {0};
  W2300RateMode w2300RateMode {W2300RateMode::Fast};
  std::uint64_t transmitStartMs {0};
  std::uint64_t transmitEndMs {0};
  std::size_t startSample {0};
  std::size_t sampleCount {0};
  bool dropped {false};
  bool decoded {false};
  W500DecodeMetrics w500Metrics;
  W2300DecodeMetrics w2300Metrics;
};

struct WideAudioPipelineResult
{
  bool complete {false};
  bool failed {false};
  bool handshakeAttempted {false};
  bool handshakeAccepted {false};
  NegotiatedLink negotiatedLink;
  std::vector<Frame> handshakeFrames;
  std::vector<std::uint8_t> receivedMessage;
  std::vector<WideAudioBurstTrace> bursts;
  std::vector<AudioAckTrace> ackBursts;
  std::vector<W2300ChannelDeferralTrace> deferrals;
  AudioThroughputMetrics throughput;
  std::size_t totalSamples {0};
  std::string error;
};

WideAudioPipelineResult runWideAudioPipeline (
    std::vector<std::uint8_t> const& message,
    std::uint16_t sessionId,
    WideAudioPipelineOptions const& options = WideAudioPipelineOptions {});

struct WideTxAudioPlanOptions
{
  Profile profile {Profile::Wide2300};
  FrameType frameType {FrameType::Data};
  W2300RateMode w2300RateMode {W2300RateMode::Fast};
  double sampleRate {48000.0};
  W500WaveformConfig w500TxConfig;
  W2300WaveformConfig w2300TxConfig;
  std::size_t guardBeforeSamples {480};
  std::size_t guardAfterSamples {480};
  std::size_t interBurstGapSamples {960};
};

struct WideTxAudioPlan
{
  bool ok {false};
  Profile profile {Profile::Wide2300};
  W2300RateMode w2300RateMode {W2300RateMode::Fast};
  double sampleRate {48000.0};
  std::vector<float> samples;
  std::vector<Frame> frames;
  std::vector<WideAudioBurstTrace> bursts;
  AudioThroughputMetrics throughput;
  std::size_t totalSamples {0};
  std::string error;
};

WideTxAudioPlan buildWideTxAudioPlan (
    std::vector<std::uint8_t> const& message,
    std::uint16_t sessionId,
    WideTxAudioPlanOptions const& options = WideTxAudioPlanOptions {});

// Builds a burst plan from an already selected ARQ window. This lets the
// live transport retransmit only frames that are still missing.
WideTxAudioPlan buildWideTxAudioPlanForFrames (
    std::vector<Frame> const& frames,
    WideTxAudioPlanOptions const& options = WideTxAudioPlanOptions {});

}
}

#endif
