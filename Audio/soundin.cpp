#include "soundin.h"

#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QMediaDevices>
#include <QSysInfo>
#include <QDebug>
#include <QTimer>

#include "Logger.hpp"

#include "moc_soundin.cpp"

namespace
{
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
          // Capture stereo and let AudioDevice select the requested hardware
          // channel. This avoids Qt/OS mono downmixing.
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
  return state == QAudio::ActiveState || state == QAudio::IdleState;
}
}

bool SoundInput::isActiveFor (QAudioDevice const& device,
                              unsigned downSampleFactor,
                              AudioDevice::Channel channel) const
{
  QAudioFormat const format = makeInputFormat(device, downSampleFactor, channel);
  return m_stream
      && isReusableInputStream(m_stream.data())
      && m_deviceDescription == device.description()
      && m_sampleRate == format.sampleRate()
      && m_channelCount == format.channelCount()
      && m_channelSelector == static_cast<int>(channel);
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
          Q_EMIT error (tr ("Audio RX input read error: Qt reported an I/O failure while reading samples. %1").arg(context));
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

  m_sink = sink;

  bool usingStereoForMono = false;
  QAudioFormat const format = makeInputFormat(device, downSampleFactor, channel, &usingStereoForMono);

  if (m_stream
      && isReusableInputStream(m_stream.data())
      && m_sink == sink
      && m_deviceDescription == device.description()
      && m_sampleRate == format.sampleRate()
      && m_channelCount == format.channelCount()
      && m_channelSelector == static_cast<int>(channel))
    {
      qint64 const now_ms = QDateTime::currentMSecsSinceEpoch ();
      QString const duplicateKey = QStringLiteral("%1|%2|%3|%4")
          .arg(m_deviceDescription)
          .arg(m_sampleRate)
          .arg(m_channelCount)
          .arg(m_channelSelector);
      bool const keyChanged = duplicateKey != m_lastDuplicateStartKey;
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
          m_lastDuplicateStartKey = duplicateKey;
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

  stop ();
  m_deviceDescription = device.description();
  m_sampleRate = format.sampleRate();
  m_channelCount = format.channelCount();
  m_channelSelector = static_cast<int>(channel);
  m_lastDuplicateStartKey.clear ();
  m_lastDuplicateStartLogMs = -1;
  m_suppressedDuplicateStartLogs = 0;

  if (usingStereoForMono)
    {
      qDebug() << "SoundInput: using stereo capture for mono input selection";
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
      return;
    }

  m_stream.reset (new QAudioSource {device, format});
  qDebug() << "SoundInput: QAudioSource created, error=" << (int)m_stream->error();
  if (!checkStream ())
    {
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
      m_stream->start (sink);
      checkStream ();
      cummulative_lost_usec_ = -1;
    }
  else
    {
      Q_EMIT error (tr ("Audio RX sink initialization failed: input device=\"%1\", requested=%2, selected-channel=%3")
                    .arg(m_deviceDescription,
                         inputFormatSummary(format),
                         inputChannelName(m_channelSelector)));
    }
}

void SoundInput::suspend ()
{
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
}

void SoundInput::resume ()
{
  if (m_sink)
    {
      m_sink->reset ();
    }

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
}

void SoundInput::stop()
{
  retireCurrentStream ();
  m_deviceDescription.clear ();
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
}

SoundInput::~SoundInput ()
{
  stop ();
}
