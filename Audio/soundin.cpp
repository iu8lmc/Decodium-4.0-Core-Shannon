#include "soundin.h"

#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <utility>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QElapsedTimer>
#include <QSysInfo>
#include <QDebug>
#include <QThread>
#include <QTimer>

#include "Logger.hpp"

#include "moc_soundin.cpp"

namespace
{
constexpr qint64 kSoundInputTraceDefaultThresholdMs = 20;

QString audioStateName(QAudio::State state)
{
  switch (state)
    {
    case QAudio::ActiveState: return QStringLiteral("ActiveState");
    case QAudio::SuspendedState: return QStringLiteral("SuspendedState");
    case QAudio::StoppedState: return QStringLiteral("StoppedState");
    case QAudio::IdleState: return QStringLiteral("IdleState");
    }
  return QStringLiteral("UnknownState(%1)").arg(static_cast<int>(state));
}

QString audioErrorName(QAudio::Error error)
{
  switch (error)
    {
    case QAudio::NoError: return QStringLiteral("NoError");
    case QAudio::OpenError: return QStringLiteral("OpenError");
    case QAudio::IOError: return QStringLiteral("IOError");
    case QAudio::UnderrunError: return QStringLiteral("UnderrunError");
    case QAudio::FatalError: return QStringLiteral("FatalError");
    }
  return QStringLiteral("UnknownError(%1)").arg(static_cast<int>(error));
}

QString inputChannelName(int channel)
{
  return AudioDevice::toString(static_cast<AudioDevice::Channel>(qBound(0, channel, 3)));
}

QString inputFormatSummary(QAudioFormat const& format)
{
  if (!format.isValid())
    {
      return QStringLiteral("invalid-format");
    }
  return QStringLiteral("%1 Hz, %2 ch, sample=%3, bytes/frame=%4")
      .arg(format.sampleRate())
      .arg(format.channelCount())
      .arg(static_cast<int>(format.sampleFormat()))
      .arg(format.bytesPerFrame());
}

void soundInputTimelineLog(QString const& label,
                           qint64 elapsedMs,
                           QString const& details = {})
{
  QString msg = QStringLiteral("[AUDIO-TL] %1 elapsed_ms=%2")
      .arg(label)
      .arg(elapsedMs);
  if (!details.trimmed().isEmpty())
    {
      msg += QLatin1Char(' ');
      msg += details.trimmed();
    }
  qInfo().noquote() << msg;
}

bool shouldDispatchToSoundInputThread(QObject const* object)
{
  QThread *ownerThread = object ? object->thread() : nullptr;
  return ownerThread
      && ownerThread->isRunning()
      && ownerThread != QThread::currentThread();
}

class SoundInputTraceScope
{
public:
  SoundInputTraceScope(QString label,
                       QString details = {},
                       qint64 thresholdMs = kSoundInputTraceDefaultThresholdMs)
    : m_label(std::move(label)),
      m_details(std::move(details)),
      m_thresholdMs(thresholdMs)
  {
    m_timer.start();
  }

  ~SoundInputTraceScope()
  {
    qint64 const elapsedMs = m_timer.elapsed();
    if (elapsedMs >= m_thresholdMs)
      {
        soundInputTimelineLog(m_label, elapsedMs, m_details);
      }
  }

  void addDetail(QString const& detail)
  {
    QString const trimmed = detail.trimmed();
    if (trimmed.isEmpty())
      {
        return;
      }
    if (!m_details.isEmpty())
      {
        m_details += QLatin1Char(' ');
      }
    m_details += trimmed;
  }

private:
  QElapsedTimer m_timer;
  QString m_label;
  QString m_details;
  qint64 m_thresholdMs;
};

QAudioFormat makeInputFormat(QAudioDevice const& device,
                             unsigned downSampleFactor,
                             AudioDevice::Channel channel,
                             bool *usingStereoForMono = nullptr)
{
  if (usingStereoForMono)
    {
      *usingStereoForMono = false;
    }

  QAudioFormat format;
  format.setChannelCount (AudioDevice::Mono == channel ? 1 : 2);
  format.setSampleRate (12000 * downSampleFactor);
  format.setSampleFormat (QAudioFormat::Int16);

  if (channel == AudioDevice::Mono)
    {
      QAudioFormat stereoFormat {format};
      stereoFormat.setChannelCount (2);
      if (device.isFormatSupported (stereoFormat))
        {
          // Keep the hardware stream stereo and let AudioDevice build the
          // mono signal. This avoids Qt6/CoreAudio/driver-specific mono
          // channel mapping, which otherwise changes the reported SNR across
          // the audio passband compared with Decodium3.
          format = stereoFormat;
          if (usingStereoForMono)
            {
              *usingStereoForMono = true;
            }
        }
    }

  return format;
}

bool isReusableInputStream(QAudioSource const* stream)
{
  if (!stream || stream->error() != QAudio::NoError)
    {
      return false;
    }

  QAudio::State const state = stream->state();
  return state == QAudio::ActiveState
      || state == QAudio::IdleState
      || state == QAudio::SuspendedState;
}

QString audioDeviceIdForKey(QAudioDevice const& device)
{
  QByteArray const id = device.id();
  return id.isEmpty() ? QString() : QString::fromLatin1(id.toHex());
}

#if defined(Q_OS_MACOS)
QString osStatusName(OSStatus status)
{
  char text[5] {};
  auto const bigEndian = static_cast<quint32> (status);
  text[0] = static_cast<char> ((bigEndian >> 24) & 0xff);
  text[1] = static_cast<char> ((bigEndian >> 16) & 0xff);
  text[2] = static_cast<char> ((bigEndian >> 8) & 0xff);
  text[3] = static_cast<char> (bigEndian & 0xff);
  bool printable = true;
  for (int i = 0; i < 4; ++i)
    {
      if (text[i] < 32 || text[i] > 126)
        {
          printable = false;
          break;
        }
    }
  return printable
      ? QStringLiteral("'%1' (%2)").arg(QString::fromLatin1(text, 4)).arg(status)
      : QString::number(status);
}

bool isVirtualMacAudioInput (QAudioDevice const& device)
{
  QString const identity = (device.description () + QLatin1Char (' ')
                            + QString::fromLatin1 (device.id ())).toCaseFolded ();
  return identity.contains (QStringLiteral ("blackhole"))
      || identity.contains (QStringLiteral ("soundflower"))
      || identity.contains (QStringLiteral ("loopback"))
      || identity.contains (QStringLiteral ("vb-cable"))
      || identity.contains (QStringLiteral ("virtual audio"));
}
#endif
}

bool SoundInput::isActiveFor (QAudioDevice const& device,
                              unsigned downSampleFactor,
                              AudioDevice::Channel channel) const
{
  if (shouldDispatchToSoundInputThread(this))
    {
      bool result = false;
      auto *self = const_cast<SoundInput *> (this);
      QAudioDevice deviceCopy {device};
      QMetaObject::invokeMethod (self, [self, deviceCopy, downSampleFactor, channel, &result] {
        result = self->isActiveFor (deviceCopy, downSampleFactor, channel);
      }, Qt::BlockingQueuedConnection);
      return result;
    }

  QAudioFormat const format = makeInputFormat(device, downSampleFactor, channel);
  QString const deviceId = audioDeviceIdForKey(device);
  QString const startKey = QStringLiteral("%1|%2|%3|%4|%5")
      .arg(device.description())
      .arg(deviceId)
      .arg(format.sampleRate())
      .arg(format.channelCount())
      .arg(static_cast<int>(channel));
  qint64 const now_ms = QDateTime::currentMSecsSinceEpoch ();
#if defined(Q_OS_MACOS)
  if (m_audioQueue
      && m_sink
      && m_deviceDescription == device.description()
      && m_deviceId == deviceId
      && m_sampleRate == format.sampleRate()
      && m_channelCount == format.channelCount()
      && m_channelSelector == static_cast<int>(channel))
    {
      return true;
    }
#endif
  return m_stream
      && (isReusableInputStream(m_stream.data())
          || (m_stream->error () == QAudio::NoError
              && m_currentStartKey == startKey
              && m_currentStartRequestedMs > 0
              && now_ms - m_currentStartRequestedMs < 5000))
      && m_deviceDescription == device.description()
      && m_deviceId == deviceId
      && m_sampleRate == format.sampleRate()
      && m_channelCount == format.channelCount()
      && m_channelSelector == static_cast<int>(channel);
}

#if defined(Q_OS_MACOS)
void SoundInput::audioQueueInputCallback (void * userData,
                                          AudioQueueRef queue,
                                          AudioQueueBufferRef buffer,
                                          AudioTimeStamp const *,
                                          UInt32 packetCount,
                                          AudioStreamPacketDescription const *)
{
  auto * self = static_cast<SoundInput *> (userData);
  if (!self || !buffer)
    {
      return;
    }

  Q_UNUSED (packetCount);
  if (buffer->mAudioData && buffer->mAudioDataByteSize > 0)
    {
      AudioDevice * sink = self->m_sink.data ();
      if (sink && sink->isOpen ())
        {
          sink->write (static_cast<char const *> (buffer->mAudioData),
                       static_cast<qint64> (buffer->mAudioDataByteSize));
        }
    }

  if (self->m_audioQueue == queue)
    {
      AudioQueueEnqueueBuffer (queue, buffer, 0, nullptr);
  }
}

void SoundInput::pullAudioData ()
{
  if (!m_usingPullAudio || !m_pullDevice || !m_sink || !m_sink->isOpen ())
    {
      return;
    }

  int const frameBytes = m_pullBytesPerFrame > 0
      ? m_pullBytesPerFrame
      : (m_stream ? m_stream->format ().bytesPerFrame () : 0);
  if (frameBytes <= 0)
    {
      return;
    }

  qint64 const available = m_pullDevice->bytesAvailable ();
  if (available > 0)
    {
      qint64 const maxRead = qMin<qint64> (available, 65536);
      m_pullReadBuffer.resize (static_cast<int> (maxRead));
      qint64 const read = m_pullDevice->read (m_pullReadBuffer.data (), maxRead);
      if (read > 0)
        {
          m_pullPendingData.append (m_pullReadBuffer.constData (), static_cast<int> (read));
          ++m_pullReadCalls;

          qint64 const completeBytes = m_pullPendingData.size ()
              - (m_pullPendingData.size () % frameBytes);
          if (completeBytes > 0)
            {
              qint64 const written = m_sink->write (m_pullPendingData.constData (), completeBytes);
              if (written > 0)
                {
                  qint64 const consumed = qMin (written, completeBytes);
                  // The legacy detector consumes signed 16-bit PCM. Keep a
                  // cheap signal sanity check here so a live callback that
                  // carries only zeros is immediately distinguishable from
                  // useful BlackHole audio.
                  if (qEnvironmentVariableIsSet ("DECODIUM_LEGACY_AUDIO_TRACE"))
                    {
                      qint64 const sampleCount = consumed / static_cast<qint64> (sizeof (qint16));
                      auto const *samples = reinterpret_cast<qint16 const *> (m_pullPendingData.constData ());
                      for (qint64 i = 0; i < sampleCount; ++i)
                        {
                          qint32 const value = samples[i];
                          qint32 const absolute = value < 0 ? -value : value;
                          m_pullSignalSamples += 1;
                          m_pullSignalEnergy += static_cast<double> (value) * static_cast<double> (value);
                          if (value != 0)
                            {
                              m_pullSignalNonZero += 1;
                            }
                          if (absolute > m_pullSignalPeak)
                            {
                              m_pullSignalPeak = absolute;
                            }
                        }
                    }
                  m_pullPendingData.remove (0, static_cast<int> (consumed));
                  m_pullReadFrames += static_cast<quint64> (consumed / frameBytes);
                }
            }
        }
    }

  if (qEnvironmentVariableIsSet ("DECODIUM_LEGACY_AUDIO_TRACE"))
    {
      qint64 const nowMs = QDateTime::currentMSecsSinceEpoch ();
      if (m_pullLastTraceMs < 0 || nowMs - m_pullLastTraceMs >= 1000)
        {
          qInfo ().nospace ()
              << "[AUDIO-PULL] dev=" << m_deviceDescription
              << " available=" << available
              << " readCalls=" << m_pullReadCalls
              << " frames=" << m_pullReadFrames
              << " peak16=" << m_pullSignalPeak
              << " rms16=" << (m_pullSignalSamples > 0
                                  ? std::sqrt (m_pullSignalEnergy / static_cast<double> (m_pullSignalSamples))
                                  : 0.0)
              << " nonzeroPct=" << (m_pullSignalSamples > 0
                                      ? (100.0 * static_cast<double> (m_pullSignalNonZero)
                                         / static_cast<double> (m_pullSignalSamples))
                                      : 0.0)
              << " pendingBytes=" << m_pullPendingData.size ()
              << " state=" << (m_stream ? static_cast<int> (m_stream->state ()) : -1)
              << " error=" << (m_stream ? static_cast<int> (m_stream->error ()) : -1);
          m_pullReadCalls = 0;
          m_pullReadFrames = 0;
          m_pullSignalSamples = 0;
          m_pullSignalNonZero = 0;
          m_pullSignalPeak = 0;
          m_pullSignalEnergy = 0.0;
          m_pullLastTraceMs = nowMs;
        }
    }
}

void SoundInput::stopPullAudio ()
{
  m_pullTimer.stop ();
  m_usingPullAudio = false;
  m_pullDevice.clear ();
  m_pullPendingData.clear ();
  m_pullReadBuffer.clear ();
  m_pullBytesPerFrame = 0;
  m_pullReadCalls = 0;
  m_pullReadFrames = 0;
  m_pullSignalSamples = 0;
  m_pullSignalNonZero = 0;
  m_pullSignalPeak = 0;
  m_pullSignalEnergy = 0.0;
  m_pullLastTraceMs = -1;
}

bool SoundInput::startNativeMacInput (QAudioDevice const& device,
                                      QAudioFormat const& format,
                                      int framesPerBuffer,
                                      AudioDevice * sink,
                                      AudioDevice::Channel channel,
                                      QString *failureReason)
{
  stopNativeMacInput ();

  auto fail = [failureReason] (QString const& reason) {
    if (failureReason)
      {
        *failureReason = reason;
      }
    return false;
  };

  if (!sink)
    {
      return fail (QStringLiteral ("missing sink"));
    }
  if (format.sampleFormat () != QAudioFormat::Int16
      || format.sampleRate () <= 0
      || format.channelCount () <= 0)
    {
      Q_EMIT error (tr ("Native macOS AudioQueue input requires PCM Int16 format, got %1")
                    .arg (inputFormatSummary (format)));
      return fail (QStringLiteral ("unsupported native input format %1")
                   .arg (inputFormatSummary (format)));
    }

  if (!sink->initialize (QIODevice::WriteOnly, channel, format.channelCount ()))
    {
      Q_EMIT error (tr ("Native macOS AudioQueue sink initialization failed: input device=\"%1\", requested=%2, selected-channel=%3")
                    .arg (device.description (),
                          inputFormatSummary (format),
                          inputChannelName (static_cast<int> (channel))));
      return fail (QStringLiteral ("sink initialization failed"));
    }

  AudioStreamBasicDescription asbd {};
  asbd.mSampleRate = format.sampleRate ();
  asbd.mFormatID = kAudioFormatLinearPCM;
  asbd.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
  asbd.mBytesPerPacket = static_cast<UInt32> (format.bytesPerFrame ());
  asbd.mFramesPerPacket = 1;
  asbd.mBytesPerFrame = static_cast<UInt32> (format.bytesPerFrame ());
  asbd.mChannelsPerFrame = static_cast<UInt32> (format.channelCount ());
  asbd.mBitsPerChannel = 16;

  AudioQueueRef queue {nullptr};
  OSStatus status = AudioQueueNewInput (&asbd,
                                        &SoundInput::audioQueueInputCallback,
                                        this,
                                        nullptr,
                                        kCFRunLoopCommonModes,
                                        0,
                                        &queue);
  if (status != noErr || !queue)
    {
      return fail (QStringLiteral ("AudioQueueNewInput failed for \"%1\": %2")
                   .arg (device.description (), osStatusName (status)));
    }

  QByteArray const deviceId = device.id ();
  if (!deviceId.isEmpty ())
    {
      CFStringRef uid = CFStringCreateWithBytes (kCFAllocatorDefault,
                                                 reinterpret_cast<UInt8 const *> (deviceId.constData ()),
                                                 deviceId.size (),
                                                 kCFStringEncodingUTF8,
                                                 false);
      if (!uid)
        {
          AudioQueueDispose (queue, true);
          return fail (QStringLiteral ("cannot convert CoreAudio input UID for \"%1\"")
                       .arg (device.description ()));
        }

      status = AudioQueueSetProperty (queue,
                                      kAudioQueueProperty_CurrentDevice,
                                      &uid,
                                      sizeof (uid));
      CFRelease (uid);
      if (status != noErr)
        {
          AudioQueueDispose (queue, true);
          return fail (QStringLiteral ("AudioQueue current-device select failed for \"%1\": %2")
                       .arg (device.description (), osStatusName (status)));
        }
    }

  m_audioQueue = queue;
  m_nativeInputFormat = format;
  m_audioQueueBuffers.clear ();

  int const requestedFrames = framesPerBuffer > 0 ? framesPerBuffer : 2048;
  // The generic buffer request is intentionally large on Windows to survive
  // UI/decode stalls.  Using that same value as the native AudioQueue callback
  // quantum makes macOS deliver fresh spectrum samples only every ~171 ms at
  // 48 kHz.  Keep four queued buffers, but make each callback about 20 ms so
  // the GPU panadapter receives continuously advancing PCM.
  int const defaultNativeFrames = qBound (512, format.sampleRate () / 50, 2048);
  bool nativeFramesOverrideOk = false;
  int const nativeFramesOverride =
      qEnvironmentVariableIntValue ("DECODIUM_MAC_AUDIO_QUEUE_FRAMES",
                                    &nativeFramesOverrideOk);
  int const nativeFrameLimit = nativeFramesOverrideOk && nativeFramesOverride > 0
      ? qBound (512, nativeFramesOverride, 8192)
      : defaultNativeFrames;
  int const bufferFrames = qBound (512,
                                   qMin (requestedFrames, nativeFrameLimit),
                                   8192);
  UInt32 const bufferBytes = static_cast<UInt32> (format.bytesForFrames (bufferFrames));
  for (int i = 0; i < 4; ++i)
    {
      AudioQueueBufferRef bufferRef {nullptr};
      status = AudioQueueAllocateBuffer (m_audioQueue, bufferBytes, &bufferRef);
      if (status != noErr || !bufferRef)
        {
              Q_EMIT error (tr ("Native macOS AudioQueue buffer allocation failed for \"%1\": %2")
                            .arg (device.description (), osStatusName (status)));
              stopNativeMacInput ();
              return fail (QStringLiteral ("buffer allocation failed: %1")
                           .arg (osStatusName (status)));
            }
      m_audioQueueBuffers.append (bufferRef);
      status = AudioQueueEnqueueBuffer (m_audioQueue, bufferRef, 0, nullptr);
      if (status != noErr)
        {
              Q_EMIT error (tr ("Native macOS AudioQueue buffer enqueue failed for \"%1\": %2")
                            .arg (device.description (), osStatusName (status)));
              stopNativeMacInput ();
              return fail (QStringLiteral ("buffer enqueue failed: %1")
                           .arg (osStatusName (status)));
            }
    }

  sink->setInputGainLinear (m_inputGain);
  status = AudioQueueStart (m_audioQueue, nullptr);
  if (status != noErr)
    {
      Q_EMIT error (tr ("Native macOS AudioQueue input start failed for \"%1\": %2")
                    .arg (device.description (), osStatusName (status)));
      stopNativeMacInput ();
      return fail (QStringLiteral ("AudioQueueStart failed: %1")
                   .arg (osStatusName (status)));
    }

  qInfo () << "SoundInput: native macOS AudioQueue input active for"
           << device.description()
           << "id=" << QString::fromUtf8 (deviceId)
           << "format=" << inputFormatSummary (format)
           << "requestedFrames=" << requestedFrames
           << "bufferFrames=" << bufferFrames
           << "callbackMs="
           << QString::number (1000.0 * bufferFrames / format.sampleRate (),
                               'f', 2);
  cummulative_lost_usec_ = std::numeric_limits<qint64>::min ();
  emitStatusIfChanged (tr ("Receiving"), QAudio::ActiveState);
  return true;
}

void SoundInput::stopNativeMacInput ()
{
  if (!m_audioQueue)
    {
      m_audioQueueBuffers.clear ();
      m_nativeInputFormat = QAudioFormat {};
      return;
    }

  AudioQueueRef queue = m_audioQueue;
  m_audioQueue = nullptr;
  AudioQueueStop (queue, true);
  AudioQueueDispose (queue, true);
  m_audioQueueBuffers.clear ();
  m_nativeInputFormat = QAudioFormat {};
}
#endif

void SoundInput::setInputGain (float gain)
{
  if (shouldDispatchToSoundInputThread(this))
    {
      QPointer<SoundInput> guard {this};
      QMetaObject::invokeMethod (this, [guard, gain] {
        if (guard)
          {
            guard->setInputGain (gain);
          }
      }, Qt::QueuedConnection);
      return;
    }

  m_inputGain = qMax (0.0f, gain);
  if (m_sink) m_sink->setInputGainLinear (m_inputGain);
  if (m_stream) m_stream->setVolume (1.0f);
}

float SoundInput::inputGain () const
{
  if (shouldDispatchToSoundInputThread(this))
    {
      float result = 1.0f;
      auto *self = const_cast<SoundInput *> (this);
      QMetaObject::invokeMethod (self, [self, &result] {
        result = self->inputGain ();
      }, Qt::BlockingQueuedConnection);
      return result;
    }

  return m_inputGain;
}

bool SoundInput::checkStream ()
{
  bool result (false);
  if (m_stream)
    {
      QString const context = QStringLiteral("device=\"%1\", requested=%2, selected-channel=%3, state=%4, qt-error=%5")
          .arg(m_deviceDescription.isEmpty() ? QStringLiteral("<default input>") : m_deviceDescription,
               inputFormatSummary(m_stream->format()),
               inputChannelName(m_channelSelector),
               audioStateName(m_stream->state()),
               audioErrorName(m_stream->error()));
      switch (m_stream->error ())
        {
        case QAudio::OpenError:
          Q_EMIT error (tr ("Audio RX input open error: Qt could not open the selected input device. %1").arg(context));
          break;

        case QAudio::IOError:
          {
            QString const message = tr ("Audio RX input read error: Qt reported an I/O failure while reading samples. %1").arg(context);
#if defined(Q_OS_WIN)
            // A WASAPI USB endpoint can enter IOError after suspend/resume
            // even though the device is still present and can be reopened.
            // Let the bridge recover it without blocking the UI with a modal
            // error. Other platforms retain their established behaviour.
            Q_EMIT recoverableError (message);
#else
            Q_EMIT error (message);
#endif
          }
          break;

        case QAudio::FatalError:
          Q_EMIT error (tr ("Audio RX input fatal error: the selected input device is not usable now. %1").arg(context));
          break;

        case QAudio::UnderrunError:
          // Soft-fail on underrun, but do not silently ignore it.
          qWarning () << "SoundInput underrun detected on" << QSysInfo::prettyProductName ();
          Q_EMIT status (tr ("Audio RX input underrun: capture fell behind but will continue. %1").arg(context));
          if (m_stream)
            {
              m_stream->reset ();
            }
          result = true;
          break;

        case QAudio::NoError:
          result = true;
          break;
        }
    }
  return result;
}

void SoundInput::emitStatusIfChanged (QString const& message, QAudio::State state)
{
  if (m_haveReportedState_
      && m_lastReportedState == state
      && m_lastStatusMessage == message)
    {
      return;
    }

  m_lastReportedState = state;
  m_lastStatusMessage = message;
  m_haveReportedState_ = true;
  Q_EMIT status (message);
}

void SoundInput::start(QAudioDevice const& device, int framesPerBuffer, AudioDevice * sink
                       , unsigned downSampleFactor, AudioDevice::Channel channel)
{
  Q_ASSERT (sink);

  if (shouldDispatchToSoundInputThread(this))
    {
      QPointer<SoundInput> guard {this};
      QPointer<AudioDevice> sinkGuard {sink};
      QAudioDevice deviceCopy {device};
      QMetaObject::invokeMethod (this, [guard, deviceCopy, framesPerBuffer, sinkGuard,
                                        downSampleFactor, channel] {
        if (guard && sinkGuard)
          {
            guard->start (deviceCopy, framesPerBuffer, sinkGuard.data(), downSampleFactor, channel);
          }
      }, Qt::QueuedConnection);
      return;
    }

#if defined(Q_OS_LINUX)
  // A pending recovery must never reopen an old device after a manual start,
  // a mode change, or a newer watchdog attempt has superseded it.
  ++m_deferredRestartGeneration;
#endif
  m_sink = sink;

  bool usingStereoForMono = false;
  QAudioFormat const format = makeInputFormat(device, downSampleFactor, channel, &usingStereoForMono);
  SoundInputTraceScope trace(QStringLiteral("sound_input_start"),
                             QStringLiteral("dev=[%1] frames=%2 dsf=%3 channel=%4 format=[%5]")
                                 .arg(device.description())
                                 .arg(framesPerBuffer)
                                 .arg(downSampleFactor)
                                 .arg(static_cast<int>(channel))
                                 .arg(inputFormatSummary(format)));
  QString const deviceId = audioDeviceIdForKey(device);
  QString const currentStartKey = QStringLiteral("%1|%2|%3|%4|%5")
      .arg(device.description())
      .arg(deviceId)
      .arg(format.sampleRate())
      .arg(format.channelCount())
      .arg(static_cast<int>(channel));
  qint64 const now_ms = QDateTime::currentMSecsSinceEpoch ();

  if (m_startInProgress)
    {
      bool const shouldLogPending =
          currentStartKey != m_lastDuplicateStartKey
          || m_lastDuplicateStartLogMs < 0
          || now_ms - m_lastDuplicateStartLogMs >= 30000;
      if (shouldLogPending)
        {
          qWarning() << "SoundInput: start skipped, previous start still pending for"
                     << m_deviceDescription
                     << "new=" << device.description();
          m_lastDuplicateStartKey = currentStartKey;
          m_lastDuplicateStartLogMs = now_ms;
        }
      return;
    }

  bool const sameActiveInput =
#if defined(Q_OS_MACOS)
      (m_audioQueue
       && m_sink == sink
       && m_deviceDescription == device.description()
       && m_deviceId == deviceId
       && m_sampleRate == format.sampleRate()
       && m_channelCount == format.channelCount()
       && m_channelSelector == static_cast<int>(channel))
      ||
#endif
      (m_stream
      && (isReusableInputStream(m_stream.data())
          || (m_stream->error () == QAudio::NoError
              && m_currentStartKey == currentStartKey
              && m_currentStartSink == sink
              && m_currentStartRequestedMs > 0
              && now_ms - m_currentStartRequestedMs < 5000))
      && m_sink == sink
      && m_deviceDescription == device.description()
      && m_deviceId == deviceId
      && m_sampleRate == format.sampleRate()
      && m_channelCount == format.channelCount()
      && m_channelSelector == static_cast<int>(channel));
  if (sameActiveInput)
    {
      bool const keyChanged = currentStartKey != m_lastDuplicateStartKey;
      bool const shouldLogDuplicate =
          keyChanged
          || m_lastDuplicateStartLogMs < 0
          || now_ms - m_lastDuplicateStartLogMs >= 60000;
      if (shouldLogDuplicate)
        {
          QString suffix;
          if (m_suppressedDuplicateStartLogs > 0)
            {
              suffix = QStringLiteral("suppressed=%1").arg(m_suppressedDuplicateStartLogs);
            }
          qDebug() << "SoundInput: start skipped, stream already active for"
                   << m_deviceDescription
                   << "rate=" << m_sampleRate
                   << "channels=" << m_channelCount
                   << suffix;
          m_lastDuplicateStartKey = currentStartKey;
          m_lastDuplicateStartLogMs = now_ms;
          m_suppressedDuplicateStartLogs = 0;
        }
      else
        {
          ++m_suppressedDuplicateStartLogs;
        }
      cummulative_lost_usec_ = std::numeric_limits<qint64>::min ();
      m_sink->setInputGainLinear (m_inputGain);
      return;
    }

  m_startInProgress = true;
  stop ();
  m_startInProgress = true;
  m_deviceDescription = device.description();
  m_deviceId = deviceId;
  m_sampleRate = format.sampleRate();
  m_channelCount = format.channelCount();
  m_channelSelector = static_cast<int>(channel);
  m_currentStartKey = currentStartKey;
  m_currentStartRequestedMs = QDateTime::currentMSecsSinceEpoch ();
  m_currentStartSink = sink;
  m_lastDuplicateStartKey.clear ();
  m_lastDuplicateStartLogMs = -1;
  m_suppressedDuplicateStartLogs = 0;

  if (usingStereoForMono)
    {
      qDebug() << "SoundInput: using stereo capture with internal mono downmix";
    }
  qDebug() << "SoundInput::start ch=" << format.channelCount() << "rate=" << format.sampleRate() << "dev=" << device.description();
  if (!device.isFormatSupported (format))
    {
      qDebug() << "SoundInput: FORMAT NOT SUPPORTED";
      Q_EMIT error (tr ("Audio RX format unsupported: device=\"%1\" does not accept requested=%2; preferred=%3; selected-channel=%4")
                    .arg(device.description(),
                         inputFormatSummary(format),
                         inputFormatSummary(device.preferredFormat()),
                         inputChannelName(static_cast<int>(channel))));
      m_startInProgress = false;
      return;
    }

#if defined(Q_OS_MACOS)
  QString nativeFailureReason;
  bool const virtualMacInput = isVirtualMacAudioInput (device);
  // BlackHole and similar virtual devices can expose a QAudioSource pull stream
  // that stays alive but returns silence. Prefer the native callback path on
  // macOS and keep the pull path as an explicit diagnostic fallback.
  bool const forceNativeVirtual =
      qEnvironmentVariableIntValue ("DECODIUM_MAC_NATIVE_AUDIO_QUEUE") != 0;
  bool const forcePullVirtual =
      qEnvironmentVariableIntValue ("DECODIUM_MAC_QAUDIO_PULL_FOR_VIRTUAL") != 0;
  bool const tryNativeInput = !forcePullVirtual || !virtualMacInput || forceNativeVirtual;
  if (tryNativeInput)
    {
      qDebug() << "SoundInput: trying native macOS AudioQueue input"
               << device.description()
               << "virtual=" << virtualMacInput
               << "forced=" << forceNativeVirtual
               << "pullOverride=" << forcePullVirtual;
      if (startNativeMacInput (device, format, framesPerBuffer, sink, channel,
                               &nativeFailureReason))
        {
          m_startInProgress = false;
          return;
        }
      qWarning() << "SoundInput: native macOS AudioQueue input unavailable for"
                 << device.description()
                 << "-" << nativeFailureReason
                 << "; falling back to QAudioSource pull mode";
    }
  else
    {
      nativeFailureReason = QStringLiteral ("virtual macOS input forced to QAudioSource pull mode");
      qInfo() << "SoundInput: using QAudioSource pull mode for virtual macOS input"
              << device.description()
              << "because DECODIUM_MAC_QAUDIO_PULL_FOR_VIRTUAL is set";
    }
#endif
    {
      m_stream.reset (new QAudioSource {device, format});
    }
  qDebug() << "SoundInput: QAudioSource created, error=" << (int)m_stream->error();
  if (!checkStream ())
    {
      m_startInProgress = false;
      return;
    }

  m_sink->setInputGainLinear (m_inputGain);
  m_stream->setVolume (1.0f);
  m_expectedSuspend_ = false;
  m_haveReportedState_ = false;
  m_lastStatusMessage.clear ();
  m_lastDebugStateLogMs = -1;
  m_suppressedDebugStateLogs = 0;

  connect (m_stream.data(), &QAudioSource::stateChanged, this, &SoundInput::handleStateChanged);
  // Note: QAudioSource::notify() was removed in Qt6; no periodic notification needed.

  if (framesPerBuffer > 0)
    {
      m_stream->setBufferSize (m_stream->format ().bytesForFrames (framesPerBuffer));
    }
  if (m_sink->initialize (QIODevice::WriteOnly, channel, format.channelCount ()))
    {
#if defined(Q_OS_MACOS)
      // Qt's macOS push path can report ActiveState while never invoking the
      // destination QIODevice for virtual devices. Pulling from the source
      // explicitly keeps capture on the SoundInput event loop and makes the
      // samples visible to the detector without reopening CoreAudio.
      m_pullDevice = m_stream->start ();
      if (!m_pullDevice)
        {
          Q_EMIT error (tr ("Audio RX pull stream could not be started: input device=\"%1\"")
                        .arg (m_deviceDescription));
          m_startInProgress = false;
          return;
        }
      m_pullBytesPerFrame = format.bytesPerFrame ();
      m_pullPendingData.clear ();
      m_pullReadBuffer.clear ();
      m_pullReadCalls = 0;
      m_pullReadFrames = 0;
      m_pullSignalSamples = 0;
      m_pullSignalNonZero = 0;
      m_pullSignalPeak = 0;
      m_pullSignalEnergy = 0.0;
      m_pullLastTraceMs = QDateTime::currentMSecsSinceEpoch ();
      m_usingPullAudio = true;
      m_pullTimer.start (5);
#else
      m_stream->start (sink);
#endif
      checkStream ();
      cummulative_lost_usec_ = -1;
      trace.addDetail(QStringLiteral("state=%1 error=%2 path=%3")
                          .arg(audioStateName(m_stream->state()),
                               audioErrorName(m_stream->error()),
#if defined(Q_OS_MACOS)
                               QStringLiteral("pull")));
#else
                               QStringLiteral("push")));
#endif
    }
  else
    {
      Q_EMIT error (tr ("Audio RX sink initialization failed: input device=\"%1\", requested=%2, selected-channel=%3")
                    .arg(m_deviceDescription,
                         inputFormatSummary(format),
                         inputChannelName(m_channelSelector)));
    }
  m_startInProgress = false;
}

void SoundInput::restart(QAudioDevice const& device, int framesPerBuffer, AudioDevice * sink
                         , unsigned downSampleFactor, AudioDevice::Channel channel)
{
  Q_ASSERT (sink);

  if (shouldDispatchToSoundInputThread(this))
    {
      QPointer<SoundInput> guard {this};
      QPointer<AudioDevice> sinkGuard {sink};
      QAudioDevice deviceCopy {device};
      QMetaObject::invokeMethod (this, [guard, deviceCopy, framesPerBuffer, sinkGuard,
                                        downSampleFactor, channel] {
        if (guard && sinkGuard)
          {
            guard->restart (deviceCopy, framesPerBuffer, sinkGuard.data(), downSampleFactor, channel);
          }
      }, Qt::QueuedConnection);
      return;
    }

#if defined(Q_OS_MACOS)
  qDebug() << "SoundInput: forced restart for" << device.description();
  stop ();

  QPointer<SoundInput> guard {this};
  QPointer<AudioDevice> sinkGuard {sink};
  QAudioDevice deviceCopy {device};
  auto delayedStart = [guard, deviceCopy, framesPerBuffer, sinkGuard,
                       downSampleFactor, channel] {
    if (guard && sinkGuard)
      {
        guard->start (deviceCopy, framesPerBuffer, sinkGuard.data(), downSampleFactor, channel);
      }
  };
  QTimer::singleShot (850, this, delayedStart);
#else
#if defined(Q_OS_LINUX)
  // Qt 6.11's PipeWire backend can leave its notifier bound to a dead socket
  // after a service restart. Calling reset(), start(), or resume() on that
  // terminal source immediately enters an "Invalid socket" notifier loop.
  // Retire it on its owning thread and let the backend settle before creating
  // a replacement. The delay also keeps posted QtMultimedia events away from
  // a same-turn replacement source, which was the trigger for the earlier
  // pure-virtual abort. This is deliberately Linux-only: Windows and macOS
  // retain their established capture and timing paths.
  bool const terminalStream = m_stream
      && (m_stream->state() == QAudio::StoppedState
          || m_stream->error() != QAudio::NoError);
  if (terminalStream)
    {
      qWarning() << "SoundInput: deferring PipeWire recovery for terminal source"
                 << device.description()
                 << "state=" << static_cast<int>(m_stream->state())
                 << "error=" << static_cast<int>(m_stream->error());

      // Do not use stop() here.  Its normal Linux teardown uses deleteLater(),
      // which leaves a StoppedState PipeWire source registered with the event
      // dispatcher until the next deferred-delete pass.  After a PipeWire
      // service restart that source can start emitting Invalid socket notifier
      // events before the deferred delete is reached, starving this thread and
      // preventing the delayed recovery from ever running.
      //
      // restart() is already executing on SoundInput's owning thread, not from
      // the QAudioSource stateChanged callback.  It is therefore safe to tear
      // down this terminal source synchronously.  The replacement remains
      // delayed below so QtMultimedia/PipeWire has time to settle and no new
      // source is created in the same event turn.
      QAudioSource *terminalSource = m_stream.take();
      QObject::disconnect(terminalSource, nullptr, this, nullptr);
      qInfo() << "SoundInput: destroying terminal PipeWire source before recovery"
              << device.description();
      delete terminalSource;

      constexpr int kPipeWireRecoveryDelayMs = 2000;
      quint64 const recoveryGeneration = ++m_deferredRestartGeneration;
      QPointer<SoundInput> guard {this};
      QPointer<AudioDevice> sinkGuard {sink};
      QAudioDevice deviceCopy {device};
      QTimer::singleShot(kPipeWireRecoveryDelayMs, this,
                         [guard, deviceCopy, framesPerBuffer, sinkGuard,
                          downSampleFactor, channel, recoveryGeneration] {
        if (!guard
            || !sinkGuard
            || guard->m_deferredRestartGeneration != recoveryGeneration)
          {
            return;
          }

        qInfo() << "SoundInput: reopening PipeWire source after recovery delay for"
                << deviceCopy.description();
        guard->start(deviceCopy, framesPerBuffer, sinkGuard.data(), downSampleFactor, channel);
      });
      return;
    }
#endif

  // PipeWire may still have a QtMultimedia event queued after QAudioSource::stop().
  // Destroying the source and recreating it in the same recovery turn can leave
  // that event targeting a retired backend object (QTBUG-style pure-virtual abort).
  // Reuse an equivalent source first; the bridge escalates to a full reopen only
  // when no PCM callback arrives after this non-destructive recovery attempt.
  QAudioFormat const format = makeInputFormat(device, downSampleFactor, channel);
  QString const deviceId = audioDeviceIdForKey(device);
  bool const sameStream =
      m_stream
      && m_sink == sink
      && m_deviceDescription == device.description()
      && m_deviceId == deviceId
      && m_sampleRate == format.sampleRate()
      && m_channelCount == format.channelCount()
      && m_channelSelector == static_cast<int>(channel);

  if (!sameStream)
    {
      qWarning() << "SoundInput: watchdog recovery needs a new source for"
                 << device.description();
      start (device, framesPerBuffer, sink, downSampleFactor, channel);
      return;
    }

#if defined(Q_OS_LINUX)
  qDebug() << "SoundInput: non-destructive PipeWire recovery for" << device.description();
#else
  qDebug() << "SoundInput: non-destructive audio input recovery for" << device.description();
#endif
  m_sink = sink;
  m_sink->setInputGainLinear (m_inputGain);
  m_expectedSuspend_ = false;
  m_stream->reset ();

  QPointer<SoundInput> guard {this};
  QPointer<AudioDevice> sinkGuard {sink};
  QTimer::singleShot (120, this, [guard, sinkGuard] {
    if (!guard || !sinkGuard || !guard->m_stream)
      {
        return;
      }

    QAudio::State const state = guard->m_stream->state ();
    if (state == QAudio::SuspendedState)
      {
        guard->m_stream->resume ();
      }
    else if (state != QAudio::ActiveState)
      {
        guard->m_stream->start (sinkGuard.data ());
      }
    guard->checkStream ();
  });
#endif
}

void SoundInput::suspend ()
{
  if (shouldDispatchToSoundInputThread(this))
    {
      QPointer<SoundInput> guard {this};
      QMetaObject::invokeMethod (this, [guard] {
        if (guard)
          {
            guard->suspend ();
          }
      }, Qt::QueuedConnection);
      return;
    }

  if (m_stream)
    {
      if (m_stream->state () == QAudio::ActiveState
          || m_stream->state () == QAudio::IdleState)
        {
          m_expectedSuspend_ = true;
          m_stream->suspend ();
        }
      checkStream ();
    }
#if defined(Q_OS_MACOS)
  else if (m_audioQueue)
    {
      AudioQueuePause (m_audioQueue);
      m_expectedSuspend_ = true;
    }
#endif
}

void SoundInput::resume ()
{
  if (shouldDispatchToSoundInputThread(this))
    {
      QPointer<SoundInput> guard {this};
      QMetaObject::invokeMethod (this, [guard] {
        if (guard)
          {
            guard->resume ();
          }
      }, Qt::QueuedConnection);
      return;
    }

  if (m_sink)
    {
      m_sink->reset ();
    }

#if defined(Q_OS_MACOS)
  if (m_audioQueue)
    {
      AudioQueueStart (m_audioQueue, nullptr);
      m_expectedSuspend_ = false;
      cummulative_lost_usec_ = std::numeric_limits<qint64>::min ();
      return;
    }
#endif

  if (m_stream)
    {
      if (m_stream->state () == QAudio::ActiveState)
        {
          cummulative_lost_usec_ = std::numeric_limits<qint64>::min ();
          return;
        }
      if (m_stream->state () == QAudio::SuspendedState)
        {
          m_stream->resume ();
        }
      else if (m_stream->state () == QAudio::StoppedState)
        {
          m_stream->start (m_sink);
        }
      m_expectedSuspend_ = false;
      checkStream ();
      cummulative_lost_usec_ = std::numeric_limits<qint64>::min ();
    }
}

void SoundInput::handleStateChanged (QAudio::State newState)
{
  QAudio::Error const initialError = m_stream ? m_stream->error () : QAudio::NoError;
  SoundInputTraceScope trace(QStringLiteral("sound_input_state_changed"),
                             QStringLiteral("state=%1 error=%2 dev=[%3]")
                                 .arg(audioStateName(newState),
                                      audioErrorName(initialError),
                                      m_deviceDescription),
                             15);
  auto *stream = qobject_cast<QAudioSource *> (sender ());
  if (stream && stream != m_stream.data ())
    {
      return;
    }

  QAudio::Error const streamError = m_stream ? m_stream->error () : QAudio::NoError;
  bool const benignActiveIdle =
      streamError == QAudio::NoError
      && (newState == QAudio::ActiveState || newState == QAudio::IdleState);
  qint64 const now_ms = QDateTime::currentMSecsSinceEpoch ();
  bool const shouldLogState =
      !benignActiveIdle
      || m_lastDebugStateLogMs < 0
      || now_ms - m_lastDebugStateLogMs >= 30000;
  if (shouldLogState)
    {
      QString suffix = m_stream ? " err=" + QString::number((int)streamError) : " no_stream";
      if (m_suppressedDebugStateLogs > 0)
        {
          suffix += " suppressed=" + QString::number (m_suppressedDebugStateLogs);
        }
      qDebug() << "SoundInput: handleStateChanged state=" << (int)newState << suffix;
      m_lastDebugStateLogMs = now_ms;
      m_suppressedDebugStateLogs = 0;
    }
  else
    {
      ++m_suppressedDebugStateLogs;
    }
  switch (newState)
    {
    case QAudio::IdleState:
      if (streamError == QAudio::NoError)
        {
          // QAudioSource on some devices spuriously bounces through IdleState
          // even while capture is healthy. Treat that as a benign transient
          // and keep the stream logically "Receiving" to avoid false alarms.
          reset (false);
          return;
        }
      emitStatusIfChanged (tr ("Idle"), newState);
      break;

    case QAudio::ActiveState:
      reset (false);
      emitStatusIfChanged (tr ("Receiving"), newState);
      break;

    case QAudio::SuspendedState:
      emitStatusIfChanged (m_expectedSuspend_ ? tr ("Receiving") : tr ("Suspended"), newState);
      break;

    case QAudio::StoppedState:
      if (!checkStream ())
        {
          emitStatusIfChanged (tr ("Audio RX input stopped with error: device=\"%1\", state=%2")
                               .arg(m_deviceDescription.isEmpty() ? QStringLiteral("<default input>") : m_deviceDescription,
                                    audioStateName(newState)),
                               newState);
        }
      else
        {
          emitStatusIfChanged (tr ("Stopped"), newState);
        }
      break;
    }
}

void SoundInput::retireCurrentStream ()
{
#if defined(Q_OS_MACOS)
  stopPullAudio ();
#endif
  auto *stream = m_stream.take ();
  if (!stream)
    {
      return;
    }

  QObject::disconnect (stream, nullptr, this, nullptr);
#if defined(Q_OS_MACOS)
  // Qt 6.11's CoreAudio backend can still have a device-disconnect future
  // queued when the app asks QAudioSource to stop. Calling stop() in the same
  // turn can make Qt run stopAudioUnit() twice and crash inside
  // QFutureInterfaceBase::cancel(). Suspend immediately, then reset/delete after
  // CoreAudio's pending listener work has had time to settle.
  if (stream->state () != QAudio::StoppedState)
    {
      stream->suspend ();
    }

  QPointer<QAudioSource> guard {stream};
  QTimer::singleShot (750, stream, [guard] {
    if (!guard)
      {
        return;
      }
    if (guard->state () != QAudio::StoppedState)
      {
        // stop() drains QtMultimedia's source ringbuffer into the QIODevice.
        // During shutdown/restart the sink can already be closing, so discard
        // buffered input instead of writing stale samples into a retiring sink.
        guard->reset ();
      }
  });
  QTimer::singleShot (5000, stream, &QObject::deleteLater);
#else
  if (stream->state () != QAudio::StoppedState)
    {
      stream->stop ();
    }

  stream->deleteLater ();
#endif
}

void SoundInput::reset (bool report_dropped_frames)
{
  if (shouldDispatchToSoundInputThread(this))
    {
      QPointer<SoundInput> guard {this};
      QMetaObject::invokeMethod (this, [guard, report_dropped_frames] {
        if (guard)
          {
            guard->reset (report_dropped_frames);
          }
      }, Qt::QueuedConnection);
      return;
    }

  constexpr qint64 dropped_audio_warning_usec {750000};
  constexpr qint64 dropped_audio_warning_interval_ms {15000};
  if (m_stream)
    {
      auto elapsed_usecs = m_stream->elapsedUSecs ();
      while (std::abs (elapsed_usecs - m_stream->processedUSecs ())
             > 24 * 60 * 60 * 500000ll) // half day
        {
          // QAudioSource::elapsedUSecs() wraps after 24 hours
          elapsed_usecs += 24 * 60 * 60 * 1000000ll;
        }
      // don't report first time as we don't yet know latency
      if (cummulative_lost_usec_ != std::numeric_limits<qint64>::min () && report_dropped_frames)
        {
          auto lost_usec = elapsed_usecs - m_stream->processedUSecs () - cummulative_lost_usec_;
          if (lost_usec < 0 || lost_usec > 10 * 1000000ll)
            {
              cummulative_lost_usec_ = std::numeric_limits<qint64>::min ();
            }
          else if (lost_usec > 5 * 48000)
            {
              qint64 const now_ms = QDateTime::currentMSecsSinceEpoch ();
              if (lost_usec >= dropped_audio_warning_usec
                  && (last_dropped_warning_ms_ < 0
                      || now_ms - last_dropped_warning_ms_ >= dropped_audio_warning_interval_ms))
                {
                  auto const lost_frames = qRound64 ((double (lost_usec)
                                                      * m_stream->format ().sampleRate ())
                                                     / 1000000.0);
                  LOG_WARN ("Detected excessive dropped audio source samples: "
                            << lost_frames
                            << " (" << std::setprecision (4) << lost_usec / 1.e6 << " S)");
                  last_dropped_warning_ms_ = now_ms;
                }
            }
        }
      cummulative_lost_usec_ = elapsed_usecs - m_stream->processedUSecs ();
    }
#if defined(Q_OS_MACOS)
  else if (m_audioQueue)
    {
      cummulative_lost_usec_ = std::numeric_limits<qint64>::min ();
    }
#endif
}

void SoundInput::stop()
{
  if (shouldDispatchToSoundInputThread(this))
    {
      QPointer<SoundInput> guard {this};
      QMetaObject::invokeMethod (this, [guard] {
        if (guard)
          {
            guard->stop ();
          }
      }, Qt::QueuedConnection);
      return;
    }

#if defined(Q_OS_LINUX)
  ++m_deferredRestartGeneration;
#endif
#if defined(Q_OS_MACOS)
  stopNativeMacInput ();
#endif
  retireCurrentStream ();
  m_deviceDescription.clear ();
  m_deviceId.clear ();
  m_sampleRate = 0;
  m_channelCount = 0;
  m_channelSelector = static_cast<int>(AudioDevice::Mono);
  m_expectedSuspend_ = false;
  m_lastStatusMessage.clear ();
  m_lastReportedState = QAudio::StoppedState;
  m_haveReportedState_ = false;
  m_lastDebugStateLogMs = -1;
  m_suppressedDebugStateLogs = 0;
  m_lastDuplicateStartKey.clear ();
  m_lastDuplicateStartLogMs = -1;
  m_suppressedDuplicateStartLogs = 0;
  m_currentStartKey.clear ();
  m_currentStartRequestedMs = -1;
  m_currentStartSink.clear ();
  m_startInProgress = false;
}

SoundInput::~SoundInput ()
{
  stop ();
}
