#include "Detector.hpp"
#include "Fil4Filter.hpp"
#include <algorithm>
#include <QDateTime>
#include <QtAlgorithms>
#include <QDebug>
#include <QtGlobal>
#include <math.h>
#include "commons.h"

#include "moc_Detector.cpp"

extern dec_data_t dec_data;

namespace
{
short scale_and_clamp_sample (short sample, float ratio)
{
  return static_cast<short> (qBound (-32768,
                                     qRound (static_cast<float> (sample) * ratio),
                                     32767));
}
}

Detector::Detector (unsigned frameRate, double periodLengthInSeconds,
                    unsigned downSampleFactor, QObject * parent)
  : AudioDevice (parent)
  , m_period (periodLengthInSeconds)
  , m_downSampleFactor (downSampleFactor)
  , m_samplesPerFFT {max_buffer_size}
  , m_buffer ((downSampleFactor > 1) ?
              new short [max_buffer_size * downSampleFactor] : nullptr)
  , m_bufferPos (0)
{
  (void)frameRate;
  clear ();
}

void Detector::setBlockSize (unsigned n)
{
  m_samplesPerFFT = n;
}

bool Detector::reset ()
{
  clear ();
  // don't call base class reset because it calls seek(0) which causes
  // a warning
  return isOpen ();
}

void Detector::clear ()
{
  // set index to roughly where we are in time (1ms resolution)
  // qint64 now (QDateTime::currentMSecsSinceEpoch ());
  // unsigned msInPeriod ((now % 86400000LL) % (m_period * 1000));
  // dec_data.params.kin = qMin ((msInPeriod * m_frameRate) / 1000, static_cast<unsigned> (sizeof (dec_data.d2) / sizeof (dec_data.d2[0])));
  dec_data.params.kin = 0;
  m_bufferPos = 0;

  // fill buffer with zeros (G4WJS commented out because it might cause decoder hangs)
  // qFill (dec_data.d2, dec_data.d2 + sizeof (dec_data.d2) / sizeof (dec_data.d2[0]), 0);
}

void Detector::applyInputGainLinear (float gain)
{
  float const bounded_gain = qMax (0.0f, gain);
  float const previous_gain = inputGainLinear ();
  if (qAbs (bounded_gain - previous_gain) < 0.0005f)
    {
      setInputGainLinear (bounded_gain);
      return;
    }

  int constexpr kMaxKin = NTMAX * RX_SAMPLE_RATE;
  int const kin = qBound (0, dec_data.params.kin, kMaxKin);

  if (bounded_gain <= 0.0001f)
    {
      std::fill (dec_data.d2, dec_data.d2 + kin, 0);
      if (m_buffer && m_bufferPos)
        {
          std::fill (m_buffer.data (), m_buffer.data () + m_bufferPos, 0);
        }
    }
  else if (previous_gain > 0.0001f)
    {
      float const ratio = bounded_gain / previous_gain;
      for (int i = 0; i < kin; ++i)
        {
          dec_data.d2[i] = scale_and_clamp_sample (dec_data.d2[i], ratio);
        }
      if (m_buffer && m_bufferPos)
        {
          for (unsigned i = 0; i < m_bufferPos; ++i)
            {
              m_buffer[i] = scale_and_clamp_sample (m_buffer[i], ratio);
            }
        }
    }

  setInputGainLinear (bounded_gain);
}

qint64 Detector::writeData (char const * data, qint64 maxSize)
{
  constexpr int kMaxKin = NTMAX * RX_SAMPLE_RATE;
  static unsigned mstr0=999999;
  qint64 ms0 = QDateTime::currentMSecsSinceEpoch() % 86400000;
  unsigned mstr = ms0 % int(1000.0*m_period); // ms into the nominal Tx start time
  if(mstr < mstr0) {              //When mstr has wrapped around to 0, restart the buffer
    dec_data.params.kin = 0;
    m_bufferPos = 0;
  }
  mstr0=mstr;

  if (dec_data.params.kin < 0 || dec_data.params.kin > kMaxKin)
    {
      qWarning () << "Detector: clamping out-of-range kin:" << dec_data.params.kin;
      dec_data.params.kin = qBound (0, dec_data.params.kin, kMaxKin);
    }
  Q_ASSERT (dec_data.params.kin >= 0);

  // no torn frames
  Q_ASSERT (!(maxSize % static_cast<qint64> (bytesPerFrame ())));
  // these are in terms of input frames (not down sampled)
  size_t framesAcceptable ((sizeof (dec_data.d2) /
                            sizeof (dec_data.d2[0]) - dec_data.params.kin) * m_downSampleFactor);
  size_t framesAccepted (qMin (static_cast<size_t> (maxSize /
                                                    bytesPerFrame ()), framesAcceptable));

  // Soundcard drift computation disabled per product requirement.

  if (framesAccepted < static_cast<size_t> (maxSize / bytesPerFrame ())) {
    qDebug () << "dropped " << maxSize / bytesPerFrame () - framesAccepted
                << " frames of data on the floor!"
                << dec_data.params.kin << mstr;
    }

    for (unsigned remaining = framesAccepted; remaining; ) {
      size_t numFramesProcessed (qMin (m_samplesPerFFT *
                                       m_downSampleFactor - m_bufferPos, remaining));

      if(m_downSampleFactor > 1) {
        store (&data[(framesAccepted - remaining) * bytesPerFrame ()],
               numFramesProcessed, &m_buffer[m_bufferPos]);
        m_bufferPos += numFramesProcessed;

        if(m_bufferPos==m_samplesPerFFT*m_downSampleFactor) {
          qint32 framesToProcess (m_samplesPerFFT * m_downSampleFactor);
          qint32 framesAfterDownSample (m_samplesPerFFT);
          int const boundedKin = qBound (0, dec_data.params.kin, kMaxKin);
          if(m_downSampleFactor > 1 &&
             boundedKin <= (kMaxKin - framesAfterDownSample)) {
            fil4_cpp(m_buffer.get(), framesToProcess, &dec_data.d2[boundedKin],
                framesAfterDownSample);
            dec_data.params.kin = boundedKin + framesAfterDownSample;
          } else {
            // qDebug() << "framesToProcess     = " << framesToProcess;
            // qDebug() << "dec_data.params.kin = " << dec_data.params.kin;
            // qDebug() << "secondInPeriod      = " << secondInPeriod();
            // qDebug() << "framesAfterDownSample" << framesAfterDownSample;
          }
          Q_EMIT framesWritten (dec_data.params.kin);
          m_bufferPos = 0;
        }

      } else {
        store (&data[(framesAccepted - remaining) * bytesPerFrame ()],
               numFramesProcessed, &dec_data.d2[dec_data.params.kin]);
        m_bufferPos += numFramesProcessed;
        dec_data.params.kin += numFramesProcessed;
        if (m_bufferPos == static_cast<unsigned> (m_samplesPerFFT)) {
          Q_EMIT framesWritten (dec_data.params.kin);
          m_bufferPos = 0;
        }
      }
      remaining -= numFramesProcessed;
    }

    // we drop any data past the end of the buffer on the floor until
    // the next period starts
    return maxSize;
}
