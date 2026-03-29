#include "Modulator.hpp"
#include <limits>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <qmath.h>
#include <QDateTime>
#include <QReadLocker>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
#include <QRandomGenerator>
#endif
#include <QDebug>
#include "widgets/itoneAndicw.h"  //w3sz tci
#include "widgets/FoxWaveGuard.hpp"
//#include "widgets/mainwindow.h" // TODO: G4WJS - break this dependency //w3sz tci
#include "Audio/soundout.h"
#include "commons.h"

#include "moc_Modulator.cpp"

extern float gran();		// Noise generator (for tests only)

#define RAMP_INCREMENT 64  // MUST be an integral factor of 2^16

#if defined (WSJT_SOFT_KEYING)
# define SOFT_KEYING WSJT_SOFT_KEYING
#else
# define SOFT_KEYING 1
#endif

double constexpr Modulator::m_twoPi;

namespace
{
void append_modulator_debug (QString const& line)
{
  auto const dir = QStandardPaths::writableLocation (QStandardPaths::AppLocalDataLocation);
  if (dir.isEmpty ()) return;

  QFile outputFile {dir + QLatin1String ("/debug.txt")};
  if (!outputFile.open (QIODevice::ReadWrite | QIODevice::Append))
    {
      return;
    }

  QTextStream outStream (&outputFile);
  outStream << QDateTime::currentDateTimeUtc ().toString ("HH:mm:ss.zzz ") << line << Qt::endl;
}

double vector_peak (QVector<float> const& wave)
{
  double peak = 0.0;
  for (float sample : wave)
    {
      peak = qMax (peak, std::abs (static_cast<double> (sample)));
    }
  return peak;
}

QString precomputed_debug_tag (QString const& mode)
{
  if (mode == QLatin1String ("FT2")) return QStringLiteral ("ft2");
  if (mode == QLatin1String ("FT4")) return QStringLiteral ("ft4");
  return mode.toLower ();
}
}

//    float wpm=20.0;
//    unsigned m_nspd=1.2*48000.0/wpm;
//    m_nspd=3072;                           //18.75 WPM

Modulator::Modulator (unsigned frameRate, double periodLengthInSeconds,
                      QObject * parent)
  : AudioDevice {parent}
  , m_quickClose {false}
  , m_phi {0.0}
  , m_toneSpacing {0.0}
  , m_fSpread {0.0}
  , m_period {periodLengthInSeconds}
  , m_frameRate {frameRate}
  , m_state {Idle}
  , m_tuning {false}
  , m_cwLevel {false}
  , m_j0 {-1}
  , m_toneFrequency0 {1500.0}
{
}

void Modulator::setSymbolTables (QVector<int> const& itone_values, QVector<int> const& icw_values)
{
  int const n_tones = qMin (itone_values.size (), static_cast<int> (m_itone.size ()));
  for (int i = 0; i < n_tones; ++i)
    {
      m_itone[static_cast<size_t> (i)] = itone_values.at (i);
    }
  for (int i = n_tones; i < static_cast<int> (m_itone.size ()); ++i)
    {
      m_itone[static_cast<size_t> (i)] = 0;
    }

  int const n_cw = qMin (icw_values.size (), static_cast<int> (m_icw.size ()));
  for (int i = 0; i < n_cw; ++i)
    {
      m_icw[static_cast<size_t> (i)] = icw_values.at (i);
    }
  for (int i = n_cw; i < static_cast<int> (m_icw.size ()); ++i)
    {
      m_icw[static_cast<size_t> (i)] = 0;
    }
}

void Modulator::setPrecomputedWave (QString const& mode, QVector<float> const& wave)
{
  m_pendingPrecomputedWaveMode = mode;
  m_pendingPrecomputedWave = wave;
}

void Modulator::start (QString mode, unsigned symbolsLength, double framesPerSymbol,
                       double frequency, double toneSpacing,
                       SoundOutput * stream, Channel channel,
                       bool synchronize, bool fastMode, double dBSNR, double TRperiod)
{
  // qDebug () << "mode:" << mode << "symbolsLength:" << symbolsLength << "framesPerSymbol:" << framesPerSymbol << "frequency:" << frequency << "toneSpacing:" << toneSpacing << "channel:" << channel << "synchronize:" << synchronize << "fastMode:" << fastMode << "dBSNR:" << dBSNR << "TRperiod:" << TRperiod;
  Q_ASSERT (stream);
  // FT2 stability guard: keep encoder/modulator contract fixed even if
  // a stale/incorrect queued start slips through.
  bool const ft2_precomputed_wave = (mode == "FT2" && !m_tuning && toneSpacing < 0.0);
  bool const ft4_precomputed_wave = (mode == "FT4" && !m_tuning && toneSpacing < 0.0);
  m_modeName = mode;
  m_ft2PrecomputedWave = ft2_precomputed_wave;
  m_ft4PrecomputedWave = ft4_precomputed_wave;
  m_loggedWaveChunk = false;
  m_waveFramesAccum = 0;
  m_waveNonZeroAccum = 0;
  m_waveAbsAccum = 0;
  m_wavePeakAccum = 0;
  if (mode == "FT2")
    {
      symbolsLength = 105;
      framesPerSymbol = 288.0;
      toneSpacing = -2.0;
    }
// Time according to this computer which becomes our base time
  qint64 ms0 = QDateTime::currentMSecsSinceEpoch() % 86400000;
  unsigned mstr = ms0 % int(1000.0*m_period); // ms into the nominal Tx start time

  if(m_state != Idle) stop();
  m_quickClose = false;
  m_symbolsLength = symbolsLength;
  m_isym0 = std::numeric_limits<unsigned>::max (); // big number
  m_frequency0 = 0.;
  m_phi = 0.;
  m_addNoise = dBSNR < 0.;
  m_nsps = framesPerSymbol;
  m_frequency = frequency;
  m_amp = std::numeric_limits<qint16>::max ();
  m_toneSpacing = toneSpacing;
  m_bFastMode=fastMode;
  m_TRperiod=TRperiod;
  m_waveSnapshot.clear ();
  if (!m_tuning && m_toneSpacing < 0.0)
    {
      qint64 requiredSamples = static_cast<qint64> (std::ceil (symbolsLength * 4.0 * framesPerSymbol)) + 2;
      if (m_bFastMode && m_TRperiod > 0.0)
        {
          requiredSamples = qMax (requiredSamples, static_cast<qint64> (std::ceil (m_TRperiod * 48000.0 - 24000.0)) + 2);
        }
      int const kFoxWaveSampleCount = static_cast<int> (sizeof (foxcom_.wave) / sizeof (foxcom_.wave[0]));
      if (!m_pendingPrecomputedWave.isEmpty () && m_pendingPrecomputedWaveMode == mode)
        {
          int const copySamples = static_cast<int> (
              qBound<qint64> (1, qMin<qint64> (requiredSamples, m_pendingPrecomputedWave.size ()),
                              m_pendingPrecomputedWave.size ()));
          m_waveSnapshot = m_pendingPrecomputedWave.mid (0, copySamples);
        }
      else
        {
          QReadLocker wave_lock {&fox_wave_lock ()};
          auto const& cachedWave = precomputed_tx_wave_buffer ();
          auto const& cachedMode = precomputed_tx_wave_mode ();
          if (!cachedWave.isEmpty () && cachedMode == mode)
            {
              int const copySamples = static_cast<int> (qBound<qint64> (1, qMin<qint64> (requiredSamples, cachedWave.size ()), cachedWave.size ()));
              m_waveSnapshot = cachedWave.mid (0, copySamples);
            }
          else
            {
              int const copySamples = static_cast<int> (qBound<qint64> (1, requiredSamples, kFoxWaveSampleCount));
              m_waveSnapshot.resize (copySamples);
              std::memcpy (m_waveSnapshot.data (), foxcom_.wave, static_cast<size_t> (copySamples) * sizeof (float));
            }
        }
      m_pendingPrecomputedWave.clear ();
      m_pendingPrecomputedWaveMode.clear ();
    }
  m_icmin=4294967295;
  m_icmax=0;
  unsigned delay_ms=1000;
  if((mode=="FT8" and m_nsps==1920) or (mode=="FST4" and m_nsps==720)) delay_ms=500;  //FT8, FST4-15
  if((mode=="FT8" and m_nsps==1024)) delay_ms=400;            //SuperFox Qary Polar Code transmission
  if(mode=="Q65" and m_nsps<=3600) delay_ms=500;              //Q65-15 and Q65-30
  if(mode=="FT4") delay_ms=300;                               //FT4
  if(mode=="FT2") delay_ms=300;                               //FT2 needs a less aggressive audio start on some rigs/macOS setups

// noise generator parameters
  if (m_addNoise) {
    m_snr = qPow (10.0, 0.05 * (dBSNR - 6.0));
    m_fac = 3000.0;
    if (m_snr > 1.0) m_fac = 3000.0 / m_snr;
  }

  m_silentFrames = 0;
  m_ic=0;
  if (!m_tuning && !m_bFastMode)
    {
      // calculate number of silent frames to send, so that audio will
      // start at the nominal time "delay_ms" into the Tx sequence.
      if (synchronize)
        {
          if(delay_ms > mstr) m_silentFrames = (delay_ms - mstr) * m_frameRate / 1000;

          // FT2 is short enough that some rigs/macOS audio paths can key PTT
          // before the data audio becomes usable. Hold the payload back a bit
          // longer to guarantee a small lead-in after PTT.
          if (ft2_precomputed_wave || ft4_precomputed_wave)
            {
              unsigned constexpr kFtxLeadInMs = 450;
              m_silentFrames = qMax<qint64> (m_silentFrames, (kFtxLeadInMs * m_frameRate) / 1000);
            }

          // adjust for late starts (only when synchronized to period)
          //
          // FT2 is unusually sensitive here because its precomputed waveform is
          // much shorter than the slot length. If we skip deep into the wave to
          // "catch up", the radio can key with little or no payload audio left.
          // Keep the nominal early-start alignment, but when FT2 starts late,
          // transmit the full waveform from sample 0 instead of trimming it away.
          if(!m_silentFrames && mstr >= delay_ms && !ft2_precomputed_wave && !ft4_precomputed_wave)
            {
              m_ic = (mstr - delay_ms) * m_frameRate / 1000;
            }
        }
    }
  if(mode=="Echo") m_ic=0;

  if (m_ft2PrecomputedWave || m_ft4PrecomputedWave)
    {
      auto const tag = precomputed_debug_tag (m_modeName);
      append_modulator_debug (QString {"%1modStart snapPeak:%2 samples:%3 silent:%4 mstr:%5 period:%6"}
                                  .arg (tag)
                                  .arg (vector_peak (m_waveSnapshot), 0, 'f', 6)
                                  .arg (m_waveSnapshot.size ())
                                  .arg (m_silentFrames)
                                  .arg (mstr)
                                  .arg (m_period, 0, 'f', 2));
    }

  initialize (QIODevice::ReadOnly, channel);
  Q_EMIT stateChanged ((m_state = (synchronize && m_silentFrames) ?
                        Synchronizing : Active));

//  qDebug() << "delay_ms:" << delay_ms << "mstr:" << mstr << "m_silentFrames:"
//           << m_silentFrames << "m_ic:" << m_ic << "m_state:" << m_state << synchronize;

  m_stream = stream;
  if (m_stream)
    {
      m_stream->restart (this);
    }
  else
    {
      qDebug () << "Modulator::start: no audio output stream assigned";
    }
}

void Modulator::tune (bool newState)
{
  m_tuning = newState;
  if (!m_tuning) stop (true);
}

void Modulator::stop (bool quick)
{
  m_quickClose = quick;
  close ();
}

void Modulator::close ()
{
  m_waveSnapshot.clear ();
  if (m_stream)
    {
      if (m_quickClose)
        {
          m_stream->reset ();
        }
      else
        {
          m_stream->stop ();
        }
    }
  if (m_state != Idle)
    {
      Q_EMIT stateChanged ((m_state = Idle));
    }
  AudioDevice::close ();
//  qDebug() << "zz" << m_icmin << m_icmax << m_icmax/48000.0;
}

qint64 Modulator::readData (char * data, qint64 maxSize)
{
  double toneFrequency=1500.0;
  if(m_nsps==6) {
    toneFrequency=1000.0;
    m_frequency=1000.0;
    m_frequency0=1000.0;
  }
  if(maxSize==0) return 0;
  Q_ASSERT (!(maxSize % qint64 (bytesPerFrame ()))); // no torn frames
  Q_ASSERT (isOpen ());

  qint64 numFrames (maxSize / bytesPerFrame ());
  qint16 * samples (reinterpret_cast<qint16 *> (data));
  qint16 * end (samples + numFrames * (bytesPerFrame () / sizeof (qint16)));
  qint64 framesGenerated (0);

//  if(m_ic==0) qDebug() << "aa" << 0.001*(QDateTime::currentMSecsSinceEpoch() % qint64(1000*m_TRperiod))
//                       << m_state << m_TRperiod << m_silentFrames << m_ic << foxcom_.wave[m_ic];

  switch (m_state)
    {
    case Synchronizing:
      {
        if (m_silentFrames)	{  // send silence up to end of start delay
          framesGenerated = qMin (m_silentFrames, numFrames);
          do
            {
              samples = load (0, samples); // silence
            } while (--m_silentFrames && samples != end);
          if (!m_silentFrames)
            {
              Q_EMIT stateChanged ((m_state = Active));
            }
        }

        m_cwLevel = false;
        m_ramp = 0;		// prepare for CW wave shaping
      }
      // fall through

    case Active:
      {
        unsigned int isym=0;
        qint64 waveFramesGenerated = 0;
        int waveNonZero = 0;
        qint16 wavePeak = 0;

        if(!m_tuning) isym=m_ic/(4.0*m_nsps);            // Actual fsample=48000
        bool slowCwId=((isym >= m_symbolsLength) && (m_icw[0] > 0)) && (!m_bFastMode);
        if(m_TRperiod==3.0) slowCwId=false;
        bool fastCwId=false;
        static bool bCwId=false;
        qint64 ms = QDateTime::currentMSecsSinceEpoch();
        float tsec=0.001*(ms % int(1000*m_TRperiod));
        if(m_bFastMode and (m_icw[0]>0) and (tsec > (m_TRperiod-5.0))) fastCwId=true;
        if(!m_bFastMode) m_nspd=2560;                 // 22.5 WPM


        if(slowCwId or fastCwId) {     // Transmit CW ID?
          m_dphi = m_twoPi*m_frequency/m_frameRate;
          if(m_bFastMode and !bCwId) {
            m_frequency=1500;          // Set params for CW ID
            m_dphi = m_twoPi*m_frequency/m_frameRate;
            m_symbolsLength=126;
            m_nsps=4096.0*12000.0/11025.0;
            m_ic=2246949;
            m_nspd=2560;               // 22.5 WPM
            if(m_icw[0]*m_nspd/48000.0 > 4.0) m_nspd=4.0*48000.0/m_icw[0];  //Faster CW for long calls
          }
          bCwId=true;
          unsigned ic0 = m_symbolsLength * 4 * m_nsps;
          unsigned j(0);

          while (samples != end) {
            j = (m_ic - ic0)/m_nspd + 1; // symbol of this sample
            bool level {bool (m_icw[j])};
            m_phi += m_dphi;
            if (m_phi > m_twoPi) m_phi -= m_twoPi;
            qint16 sample=0;
            float amp=32767.0;
            float x=0;
            if(m_ramp!=0) {
              x=qSin(float(m_phi));
              if(SOFT_KEYING) {
                amp=qAbs(qint32(m_ramp));
                if(amp>32767.0) amp=32767.0;
              }
              sample=round(amp*x);
            }
            if(m_bFastMode) {
              sample=0;
              if(level) sample=32767.0*x;
            }
            if (int (j) <= m_icw[0] && j < NUM_CW_SYMBOLS) { // stop condition
              samples = load (postProcessSample (sample), samples);
              ++framesGenerated;
              ++m_ic;
            } else {
              Q_EMIT stateChanged ((m_state = Idle));
              return framesGenerated * bytesPerFrame ();
            }

            // adjust ramp
            if ((m_ramp != 0 && m_ramp != std::numeric_limits<qint16>::min ()) || level != m_cwLevel) {
              // either ramp has terminated at max/min or direction has changed
              m_ramp += RAMP_INCREMENT; // ramp
            }
            m_cwLevel = level;
          }
          return framesGenerated * bytesPerFrame ();
        } else {
          bCwId=false;
        } //End of code for CW ID

        double const baud (12000.0 / m_nsps);
        // fade out parameters (no fade out for tuning)
        unsigned int i0,i1;
        if(m_tuning) {
          i1 = i0 = (m_bFastMode ? 999999 : 9999) * m_nsps;
        } else {
          i0=(m_symbolsLength - 0.017) * 4.0 * m_nsps;
          i1= m_symbolsLength * 4.0 * m_nsps;
          // Precomputed wave (FT2/FT4/Fox): trust the copied snapshot length
          // rather than re-deriving duration from mode-specific symbol counts.
          if (m_toneSpacing < 0 && m_itone[0] < 100) {
            if (!m_waveSnapshot.isEmpty ())
              {
                i1 = static_cast<unsigned int> (m_waveSnapshot.size () - 1);
              }
            i0 = i1;  // wave has built-in ramp, disable internal fade-out
          }
        }
        if(m_bFastMode and !m_tuning) {
          i1=m_TRperiod*48000.0 - 24000.0;
          i0=i1-816;
        }

        qint16 sample;

        while (samples != end && m_ic <= i1) {
          isym=0;
          if(!m_tuning and m_TRperiod!=3.0) isym=m_ic/(4.0*m_nsps);   //Actual fsample=48000
          if(m_bFastMode) isym=isym%m_symbolsLength;
          if (isym != m_isym0 || m_frequency != m_frequency0) {
            if(m_itone[0]>=100) {
              m_toneFrequency0=m_itone[0];
            } else {
              if(m_toneSpacing==0.0) {
                m_toneFrequency0=m_frequency + m_itone[isym]*baud;
              } else {
                m_toneFrequency0=m_frequency + m_itone[isym]*m_toneSpacing;
              }
            }
            m_dphi = m_twoPi * m_toneFrequency0 / m_frameRate;
            m_isym0 = isym;
            m_frequency0 = m_frequency;         //???
          }

          int j=m_ic/480;
          if(m_fSpread>0.0 and j!=m_j0) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            float x1=QRandomGenerator::global ()->generateDouble ();
            float x2=QRandomGenerator::global ()->generateDouble ();
#else
            float x1=static_cast<float>(std::rand())/static_cast<float>(RAND_MAX);
            float x2=static_cast<float>(std::rand())/static_cast<float>(RAND_MAX);
#endif
            toneFrequency = m_toneFrequency0 + 0.5*m_fSpread*(x1+x2-1.0);
            m_dphi = m_twoPi * toneFrequency / m_frameRate;
            m_j0=j;
          }

          m_phi += m_dphi;
          if (m_phi > m_twoPi) m_phi -= m_twoPi;
          if (m_ic > i0) m_amp = 0.98 * m_amp;
          if (m_ic > i1) m_amp = 0.0;

          sample=qRound(m_amp*qSin(m_phi));

//Here's where we transmit from a precomputed wave[] array:
          if(!m_tuning and (m_toneSpacing < 0) and (m_itone[0]<100)) {
            m_amp=32767.0;
            float wave_sample = 0.0f;
            if (m_ic < static_cast<unsigned> (m_waveSnapshot.size ()))
              {
                wave_sample = m_waveSnapshot[static_cast<int> (m_ic)];
              }
            sample=qRound(m_amp * wave_sample);
            ++waveFramesGenerated;
            if (sample != 0) ++waveNonZero;
            wavePeak = qMax<qint16> (wavePeak, static_cast<qint16> (std::abs (sample)));
            ++m_waveFramesAccum;
            if (sample != 0) ++m_waveNonZeroAccum;
            m_waveAbsAccum += static_cast<quint64> (std::abs (sample));
            m_wavePeakAccum = qMax<qint16> (m_wavePeakAccum, static_cast<qint16> (std::abs (sample)));
            m_icmin=qMin(m_ic,m_icmin);
            m_icmax=qMax(m_ic,m_icmax);
          }
/*
          if((m_ic<1000 or (4*m_symbolsLength*m_nsps - m_ic) < 1000) and (m_ic%10)==0) {
            qDebug() << "cc" << QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz") << m_ic << sample;
          }
*/
          samples = load(postProcessSample(sample), samples);
          ++framesGenerated;
          ++m_ic;
        }

        if(m_TRperiod==3 and m_ic >i1) {
          if (m_ft2PrecomputedWave || m_ft4PrecomputedWave)
            {
              quint64 const avgAbs = m_waveFramesAccum ? (m_waveAbsAccum / m_waveFramesAccum) : 0;
              auto const tag = precomputed_debug_tag (m_modeName);
              append_modulator_debug (QString {"%1modDone frames:%2 nonzero:%3 avgAbs:%4 peak:%5"}
                                          .arg (tag)
                                          .arg (m_waveFramesAccum)
                                          .arg (m_waveNonZeroAccum)
                                          .arg (avgAbs)
                                          .arg (m_wavePeakAccum));
            }
          Q_EMIT stateChanged ((m_state = Idle));
          return framesGenerated * bytesPerFrame ();
        }

        if ((m_ft2PrecomputedWave || m_ft4PrecomputedWave) && !m_loggedWaveChunk && waveFramesGenerated > 0)
          {
            auto const tag = precomputed_debug_tag (m_modeName);
            append_modulator_debug (QString {"%1modChunk frames:%2 nonzero:%3 peak:%4 ic:%5 i1:%6"}
                                        .arg (tag)
                                        .arg (waveFramesGenerated)
                                        .arg (waveNonZero)
                                        .arg (wavePeak)
                                        .arg (m_ic)
                                        .arg (i1));
            m_loggedWaveChunk = true;
          }

        // Precomputed wave finished (FT2/Fox) — clean exit after ramp-down
        if (!m_tuning && m_toneSpacing < 0 && m_itone[0] < 100 && m_ic > i1) {
          if (m_ft2PrecomputedWave || m_ft4PrecomputedWave)
            {
              quint64 const avgAbs = m_waveFramesAccum ? (m_waveAbsAccum / m_waveFramesAccum) : 0;
              auto const tag = precomputed_debug_tag (m_modeName);
              append_modulator_debug (QString {"%1modDone frames:%2 nonzero:%3 avgAbs:%4 peak:%5"}
                                          .arg (tag)
                                          .arg (m_waveFramesAccum)
                                          .arg (m_waveNonZeroAccum)
                                          .arg (avgAbs)
                                          .arg (m_wavePeakAccum));
            }
          Q_EMIT stateChanged ((m_state = Idle));
          return framesGenerated * bytesPerFrame ();
        }

//        qDebug() << "dd" << QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz")
//                 << tsec << m_ic << i1;

        if (m_amp == 0.0) { // TODO G4WJS: compare double with zero might not be wise
          if (m_icw[0] == 0) {
            // no CW ID to send
            if (m_ft2PrecomputedWave || m_ft4PrecomputedWave)
              {
                quint64 const avgAbs = m_waveFramesAccum ? (m_waveAbsAccum / m_waveFramesAccum) : 0;
                auto const tag = precomputed_debug_tag (m_modeName);
                append_modulator_debug (QString {"%1modDone frames:%2 nonzero:%3 avgAbs:%4 peak:%5"}
                                            .arg (tag)
                                            .arg (m_waveFramesAccum)
                                            .arg (m_waveNonZeroAccum)
                                            .arg (avgAbs)
                                            .arg (m_wavePeakAccum));
              }
            Q_EMIT stateChanged ((m_state = Idle));
            return framesGenerated * bytesPerFrame ();
          }
          m_phi = 0.0;
        }

        m_frequency0 = m_frequency;
// done for this chunk - continue on next call

//        qDebug() << "Mod B" << m_ic << i1 << 0.001*(QDateTime::currentMSecsSinceEpoch() % (1000*m_TRperiod));

        while (samples != end)  // pad block with silence
          {
            samples = load (0, samples);
            ++framesGenerated;
          }
//        if(tsec<0.5) qDebug() << "ee" << QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz")
//                 << tsec << m_ic << i1 << m_icmin << m_icmax;
        return framesGenerated * bytesPerFrame ();
      }
      // fall through

    case Idle:
      break;
    }

  Q_ASSERT (Idle == m_state);
  return 0;
}

qint16 Modulator::postProcessSample (qint16 sample) const
{
  if (m_addNoise) {  // Test frame, we'll add noise
    qint32 s = m_fac * (gran () + sample * m_snr / 32768.0);
    if (s > std::numeric_limits<qint16>::max ()) {
      s = std::numeric_limits<qint16>::max ();
    }
    if (s < std::numeric_limits<qint16>::min ()) {
      s = std::numeric_limits<qint16>::min ();
    }
    sample = s;
  }
  return sample;
}
