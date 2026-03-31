#include "soundout.h"

#include <QDateTime>
#include <QAudioDeviceInfo>
#include <QAudioOutput>
#include <QSysInfo>
#include <qmath.h>
#include <QDebug>

#include "Logger.hpp"
#include "Audio/AudioDevice.hpp"

#include "moc_soundout.cpp"

bool SoundOutput::checkStream () const
{
  bool result {false};

  Q_ASSERT_X (m_stream, "SoundOutput", "programming error");
  if (m_stream) {
    switch (m_stream->error ())
      {
      case QAudio::OpenError:
        Q_EMIT error (tr ("An error opening the audio output device has occurred."));
        break;

      case QAudio::IOError:
        Q_EMIT error (tr ("An error occurred during write to the audio output device."));
        break;

      case QAudio::UnderrunError:
        // Keep underruns non-fatal and report status. This behavior
        // proved more stable on macOS with repeated short FT2 cycles.
        Q_EMIT status (tr ("Audio output underrun"));
        result = true;
        break;

      case QAudio::FatalError:
        Q_EMIT error (tr ("Non-recoverable error, audio output device not usable at this time."));
        break;

      case QAudio::NoError:
        result = true;
        break;
      }
  }
  return result;
}

void SoundOutput::setFormat (QAudioDeviceInfo const& device, unsigned channels, int frames_buffered)
{
  Q_ASSERT (0 < channels && channels < 3);
  m_device = device;
  m_channels = channels;
  m_framesBuffered = frames_buffered;
}

void SoundOutput::restart (QIODevice * source)
{
  if (!m_device.isNull ())
    {
      QAudioFormat format (m_device.preferredFormat ());
      //  qDebug () << "Preferred audio output format:" << format;
      format.setChannelCount (m_channels);
      format.setCodec ("audio/pcm");
      format.setSampleRate (48000);
      format.setSampleType (QAudioFormat::SignedInt);
      format.setSampleSize (16);
      format.setByteOrder (QAudioFormat::Endian (QSysInfo::ByteOrder));
      if (!format.isValid ())
        {
          Q_EMIT error (tr ("Requested output audio format is not valid."));
        }
      else if (!m_device.isFormatSupported (format))
        {
          Q_EMIT error (tr ("Requested output audio format is not supported on device."));
        }
      else
        {
          // qDebug () << "Selected audio output format:" << format;
          m_stream.reset (new QAudioOutput (m_device, format));
          checkStream ();
          m_stream->setVolume (m_volume);
          m_stream->setNotifyInterval(1000);
          error_ = false;

          connect (m_stream.data(), &QAudioOutput::stateChanged, this, &SoundOutput::handleStateChanged);
          connect (m_stream.data(), &QAudioOutput::notify, [this] () {checkStream ();});

          //      qDebug() << "A" << m_volume << m_stream->notifyInterval();
        }
    }
  if (!m_stream)
    {
      if (!error_)
        {
          error_ = true;        // only signal error once
          Q_EMIT error (tr ("No audio output device configured."));
        }
      return;
    }
  else
    {
      error_ = false;
    }

  // we have to set this before every start on the stream because the
  // Windows implementation seems to forget the buffer size after a
  // stop.
  //qDebug () << "SoundOut default buffer size (bytes):" << m_stream->bufferSize () << "period size:" << m_stream->periodSize ();
  if (m_framesBuffered > 0)
    {
      m_stream->setBufferSize (m_stream->format().bytesForFrames (m_framesBuffered));
    }
  else
    {
#if defined(Q_OS_LINUX)
      // Linux FT2/FT4 benefit from a much smaller TX queue; large Qt/PulseAudio
      // buffers push the payload deep into the transmit window.
      m_stream->setBufferSize (m_stream->format().bytesForFrames (1024));
#else
      // Default: 16384 frames (~341ms @ 48kHz) — prevents underrun
      // on Windows/macOS where Qt's automatic buffer sizing can be too small.
      m_stream->setBufferSize (m_stream->format().bytesForFrames (16384));
#endif
    }
#if defined(Q_OS_LINUX)
  m_stream->setCategory ("game");
#else
  m_stream->setCategory ("production");
#endif
  m_stream->start (source);
//  LOG_DEBUG ("Selected buffer size (bytes): " << m_stream->bufferSize () << " period size: " << m_stream->periodSize ());
}

void SoundOutput::suspend ()
{
  if (m_stream && QAudio::ActiveState == m_stream->state ())
    {
      m_stream->suspend ();
      checkStream ();
    }
}

void SoundOutput::resume ()
{
  if (m_stream && QAudio::SuspendedState == m_stream->state ())
    {
      m_stream->resume ();
      checkStream ();
    }
}

void SoundOutput::reset ()
{
  if (m_stream)
    {
      m_stream->reset ();
      checkStream ();
    }
}

void SoundOutput::stop ()
{
  if (m_stream)
    {
#if defined(__APPLE__)
      bool const sequoia_or_newer =
          QOperatingSystemVersion::current () >= QOperatingSystemVersion (QOperatingSystemVersion::MacOS, 15);
      // On macOS 15+, QAudioOutput::reset() is observed to destabilize output.
      if (!sequoia_or_newer)
        {
          m_stream->reset ();
        }
#else
      m_stream->reset ();
#endif
      m_stream->stop ();
    }
#if defined(__APPLE__)
  if (QOperatingSystemVersion::current () < QOperatingSystemVersion (QOperatingSystemVersion::MacOS, 15))
    {
      m_stream.reset ();
    }
#endif
}

qreal SoundOutput::attenuation () const
{
  return -(20. * qLn (m_volume) / qLn (10.));
}

void SoundOutput::setAttenuation (qreal a)
{
  Q_ASSERT (0. <= a && a <= 999.);
  m_volume = qPow(10.0, -a/20.0);
  //  qDebug () << "SoundOut: attn = " << a << ", vol = " << m_volume;
  if (m_stream)
    {
      m_stream->setVolume (m_volume);
    }
}

void SoundOutput::resetAttenuation ()
{
  m_volume = 1.;
  if (m_stream)
    {
      m_stream->setVolume (m_volume);
    }
}

void SoundOutput::handleStateChanged (QAudio::State newState)
{
  switch (newState)
    {
    case QAudio::IdleState:
      Q_EMIT status (tr ("Idle"));
      break;

    case QAudio::ActiveState:
      Q_EMIT status (tr ("Sending"));
      break;

    case QAudio::SuspendedState:
      Q_EMIT status (tr ("Suspended"));
      break;

#if QT_VERSION >= QT_VERSION_CHECK (5, 10, 0)
    case QAudio::InterruptedState:
      Q_EMIT status (tr ("Interrupted"));
      break;
#endif

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
