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

void SoundInput::start(QAudioDevice const& device, int framesPerBuffer, AudioDevice * sink
                       , unsigned downSampleFactor, AudioDevice::Channel channel)
{
  Q_ASSERT (sink);

  stop ();

  m_sink = sink;

  // Qt6: QAudioFormat setup — no codec/sampleType/sampleSize/byteOrder
  QAudioFormat format;
  format.setChannelCount (AudioDevice::Mono == channel ? 1 : 2);
  format.setSampleRate (12000 * downSampleFactor);
  format.setSampleFormat (QAudioFormat::Int16);

  // Se il device preferisce stereo, usiamo stereo anche se è stato richiesto mono.
  // WASAPI su Windows spesso non funziona in mono su dispositivi USB audio.
  {
    QAudioFormat preferred = device.preferredFormat();
    if (preferred.channelCount() > 1 && format.channelCount() == 1) {
        qDebug() << "SoundInput: device prefers" << preferred.channelCount() << "channels — upgrading from mono to stereo";
        format.setChannelCount(preferred.channelCount());
        // Con stereo bisogna usare Left (non Mono) altrimenti bytesPerFrame()=2
        // e store() leggerebbe male i frame stereo da 4 byte.
        channel = AudioDevice::Left;
    }
  }

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

  connect (m_stream.data(), &QAudioSource::stateChanged, this, &SoundInput::handleStateChanged);
  // Note: QAudioSource::notify() was removed in Qt6; no periodic notification needed.

  if (framesPerBuffer > 0)
    {
      m_stream->setBufferSize (m_stream->format ().bytesForFrames (framesPerBuffer));
    }
  if (m_sink->initialize (QIODevice::WriteOnly, channel))
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
      m_stream->stop();
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
      m_stream->start (m_sink);
      checkStream ();
    }
}

void SoundInput::handleStateChanged (QAudio::State newState)
{
  qDebug() << "SoundInput: handleStateChanged state=" << (int)newState
           << (m_stream ? " err=" + QString::number((int)m_stream->error()) : " no_stream");
  switch (newState)
    {
    case QAudio::IdleState:
      Q_EMIT status (tr ("Idle"));
      break;

    case QAudio::ActiveState:
      reset (false);
      Q_EMIT status (tr ("Receiving"));
      break;

    case QAudio::SuspendedState:
      Q_EMIT status (tr ("Suspended"));
      break;

    case QAudio::StoppedState:
      if (!checkStream ())
        {
          Q_EMIT status (tr ("Error"));
        }
      else
        {
          Q_EMIT status (tr ("Stopped"));
        }
      break;
    }
}

void SoundInput::reset (bool report_dropped_frames)
{
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
          if (std::abs (lost_usec) > 5 * 48000)
            {
              LOG_ERROR ("Detected excessive dropped audio source samples: "
                        << m_stream->format ().framesForDuration (lost_usec)
                         << " (" << std::setprecision (4) << lost_usec / 1.e6 << " S)");
            }
        }
      cummulative_lost_usec_ = elapsed_usecs - m_stream->processedUSecs ();
    }
}

void SoundInput::stop()
{
  if (m_stream)
    {
      m_stream->stop ();
    }
  m_stream.reset ();
}

SoundInput::~SoundInput ()
{
  stop ();
}
