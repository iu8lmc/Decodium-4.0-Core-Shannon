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

bool SoundInput::checkStream ()
{
  bool result (false);
  if (m_stream)
    {
      switch (m_stream->error ())
        {
        case QAudio::OpenError:
          Q_EMIT error (tr ("An error opening the audio input device has occurred."));
          break;

        case QAudio::IOError:
          Q_EMIT error (tr ("An error occurred during read from the audio input device."));
          break;

        case QAudio::FatalError:
          Q_EMIT error (tr ("Non-recoverable error, audio input device not usable at this time."));
          break;

        case QAudio::UnderrunError:
          // Soft-fail on underrun, but do not silently ignore it.
          qWarning () << "SoundInput underrun detected on" << QSysInfo::prettyProductName ();
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

  // Qt6: QAudioFormat setup — no codec/sampleType/sampleSize/byteOrder
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
          qDebug() << "SoundInput: using stereo capture for mono input selection";
        }
    }

  if (m_stream
      && m_stream->state () == QAudio::ActiveState
      && m_sink == sink
      && m_deviceDescription == device.description()
      && m_sampleRate == format.sampleRate()
      && m_channelCount == format.channelCount()
      && m_channelSelector == static_cast<int>(channel))
    {
      qDebug() << "SoundInput: start skipped, stream already active for"
               << m_deviceDescription
               << "rate=" << m_sampleRate
               << "channels=" << m_channelCount;
      cummulative_lost_usec_ = std::numeric_limits<qint64>::min ();
      m_sink->setInputGainLinear (m_inputGain);
      return;
    }

  stop ();

  qDebug() << "SoundInput::start ch=" << format.channelCount() << "rate=" << format.sampleRate() << "dev=" << device.description();
  if (!device.isFormatSupported (format))
    {
      qDebug() << "SoundInput: FORMAT NOT SUPPORTED";
      Q_EMIT error (tr ("Requested input audio format is not supported on device."));
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
  m_deviceDescription = device.description();
  m_sampleRate = format.sampleRate();
  m_channelCount = format.channelCount();
  m_channelSelector = static_cast<int>(channel);
  m_expectedSuspend_ = false;
  m_haveReportedState_ = false;
  m_lastStatusMessage.clear ();

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
      Q_EMIT error (tr ("Failed to initialize audio sink device"));
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
  qDebug() << "SoundInput: handleStateChanged state=" << (int)newState
           << (m_stream ? " err=" + QString::number((int)streamError) : " no_stream");
  switch (newState)
    {
    case QAudio::IdleState:
      if (streamError == QAudio::NoError)
        {
          // QAudioSource on some devices spuriously bounces through IdleState
          // even while capture is healthy. Treat that as a benign transient
          // and keep the stream logically "Receiving" to avoid false alarms.
          reset (false);
          qDebug () << "SoundInput: benign idle transition ignored";
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
          emitStatusIfChanged (tr ("Error"), newState);
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
  if (stream->state () != QAudio::StoppedState)
    {
      stream->stop ();
    }

#if defined(Q_OS_MACOS)
  // QtMultimedia/CoreAudio can still have queued disconnect-listener work after
  // stop(). Keep the QAudioSource alive briefly so those callbacks cannot outlive
  // the object they belong to.
  QTimer::singleShot (1500, stream, &QObject::deleteLater);
#else
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
}

SoundInput::~SoundInput ()
{
  stop ();
}
