// test_tx_pipeline.cpp
//
// Integration test for the Decodium TX pipeline.
// For each mode (FT8, FT4, FT2) the test simulates a full QSO by:
//   1) encoding each message via decodium::txmsg::encodeFtN
//   2) generating a baseband waveform via decodium::txwave::generateFtNWave
//   3) packing the waveform into a mode-sized PCM frame (12000 Hz mono, s16)
//   4) feeding the frame to the Fortran decoder wrapper used by the RX path
//   5) verifying that the decoder returns a row whose decoded text matches
//      the original message.
//
// A pass/fail line is printed per message; a summary line is printed per mode
// and at the end of the run. Exit code is 0 when every message roundtrips.
//
// Runtime is deliberately self-contained: no WAV files, no audio devices,
// no CAT. The goal is to validate the C++/Fortran TX+RX chain in isolation,
// independent of audio routing and transceiver configuration.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <QByteArray>
#include <QCoreApplication>
#include <QMutexLocker>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QVector>

#include "Detector/FortranRuntimeGuard.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"

extern "C"
{
  // FT8 stage4 async decoder (same entry point used by FT8DecodeWorker).
  void ftx_ft8_async_decode_stage4_c (short const* iwave, int* nqsoprogress, int* nfqso, int* nftx,
                                      int* nutc, int* nfa, int* nfb, int* nzhsym, int* ndepth,
                                      float* emedelay, int* ncontest, int* nagain,
                                      int* lft8apon, int* ltry_a8, int* lapcqonly, int* napwid,
                                      char const* mycall, char const* hiscall,
                                      char const* hisgrid, int* ldiskdat, float* syncs, int* snrs,
                                      float* dts, float* freqs, int* naps, float* quals,
                                      signed char* bits77, char* decodeds, int* nout);
  void ftx_ft8_stage4_reset_c ();

  // FT4 async decoder.
  void ftx_ft4_decode_c (short const* iwave, int* nqsoprogress, int* nfqso, int* nfa, int* nfb,
                         int* ndepth, int* lapcqonly, int* ncontest,
                         char const* mycall, char const* hiscall,
                         float syncs[], int snrs[], float dts[], float freqs[],
                         int naps[], float quals[], signed char bits77[],
                         char decodeds[], int* nout,
                         size_t, size_t, size_t);

  // FT2 async decoder.
  void ft2_async_decode_ (short iwave[], int* nqsoprogress, int* nfqso, int* nfa, int* nfb,
                          int* ndepth, int* ncontest, char mycall[], char hiscall[],
                          int snrs[], float dts[], float freqs[], int naps[], float quals[],
                          signed char bits77[], char decodeds[], int* nout,
                          size_t, size_t, size_t);
  void ftx_ft2_stage7_clravg_c ();
}

namespace
{
  // Mode constants (matching Modulator + Detector configuration).
  constexpr int kSampleRate {12000};
  constexpr float kCarrierHz {1500.0f};

  // FT8: 79 tones × 1920 samples/symbol = 151680 + 28320 silence = 180000
  constexpr int kFt8Nsps {1920};
  constexpr int kFt8Samples {180000};
  constexpr float kFt8Bt {2.0f};

  // FT4: 105 tones × 576 samples = 60480 + 12096 silence = 72576
  constexpr int kFt4Nsps {576};
  constexpr int kFt4Samples {72576};

  // FT2: 103 tones × 288 samples = 29664 + silence = 45000
  constexpr int kFt2Nsps {288};
  constexpr int kFt2Samples {45000};

  constexpr int kMaxLines {200};
  constexpr int kBitsPerMessage {77};
  constexpr int kDecodedChars {37};

  // QSO message sequence between IW8XOU (op) and K1ABC (partner).
  // The test checks that each transmitted message roundtrips through
  // encode -> waveform -> decoder.
  struct QsoMessage
  {
    QString text;
    QString direction; // "TX" = we transmit, "RX" = partner transmits
  };

  QList<QsoMessage> make_qso ()
  {
    return {
      {QStringLiteral ("CQ IW8XOU JN71"),         QStringLiteral ("TX")},
      {QStringLiteral ("IW8XOU K1ABC FN42"),      QStringLiteral ("RX")},
      {QStringLiteral ("K1ABC IW8XOU -10"),       QStringLiteral ("TX")},
      {QStringLiteral ("IW8XOU K1ABC R-08"),      QStringLiteral ("RX")},
      {QStringLiteral ("K1ABC IW8XOU RR73"),      QStringLiteral ("TX")},
      {QStringLiteral ("IW8XOU K1ABC 73"),        QStringLiteral ("RX")},
    };
  }

  QByteArray to_fortran_field (QByteArray value, int width)
  {
    value = value.left (width);
    if (value.size () < width)
      {
        value.append (QByteArray (width - value.size (), ' '));
      }
    return value;
  }

  QByteArray trim_fortran_field (char const* data, int width)
  {
    QByteArray field {data, width};
    while (!field.isEmpty () && (field.back () == ' ' || field.back () == '\0'))
      {
        field.chop (1);
      }
    return field;
  }

  QString canonical (QString const& msg)
  {
    return msg.simplified ().toUpper ();
  }

  std::vector<qint16> to_pcm_frame (QVector<float> const& wave, int frame_samples,
                                    int offset_samples, float gain)
  {
    std::vector<qint16> pcm (static_cast<size_t> (frame_samples), 0);
    int const wave_samples = static_cast<int> (wave.size ());
    int const end = std::min (frame_samples, offset_samples + wave_samples);
    for (int i = offset_samples; i < end; ++i)
      {
        float const s = gain * wave[i - offset_samples];
        float const clipped = std::max (-1.0f, std::min (1.0f, s));
        pcm[static_cast<size_t> (i)] =
            static_cast<qint16> (std::lround (static_cast<double> (clipped) * 32767.0));
      }
    return pcm;
  }

  struct DecodeRow
  {
    QString text;
    float freq {};
    int snr {};
    float dt {};
  };

  // Run the FT8 stage4 decoder once over a prepared PCM frame. Returns all
  // rows emitted. `nzhsym=50` corresponds to a completed 15s slot.
  QList<DecodeRow> decode_ft8 (std::vector<qint16> const& samples)
  {
    std::array<int, kMaxLines> snrs {};
    std::array<float, kMaxLines> syncs {};
    std::array<float, kMaxLines> dts {};
    std::array<float, kMaxLines> freqs {};
    std::array<int, kMaxLines> naps {};
    std::array<float, kMaxLines> quals {};
    std::array<signed char, kBitsPerMessage * kMaxLines> bits77 {};
    std::array<char, kMaxLines * kDecodedChars> decodeds {};
    int nout = 0;

    int nqsoprogress = 0, nfqso = 1500, nftx = 0, nutc = 0, nfa = 200, nfb = 3000;
    int nzhsym = 50, ndepth = 3, ncontest = 0, nagain = 0;
    int lft8apon = 0, ltry_a8 = 0, lapcqonly = 0, napwid = 50, ldiskdat = 1;
    float emedelay = 0.0f;
    QByteArray my = to_fortran_field ("IW8XOU", 12);
    QByteArray his = to_fortran_field ("K1ABC", 12);
    QByteArray hisgrid = to_fortran_field ("", 6);

    QMutexLocker locker {&decodium::fortran::runtime_mutex ()};
    ftx_ft8_stage4_reset_c ();
    ftx_ft8_async_decode_stage4_c (samples.data (), &nqsoprogress, &nfqso, &nftx, &nutc,
                                   &nfa, &nfb, &nzhsym, &ndepth, &emedelay,
                                   &ncontest, &nagain, &lft8apon, &ltry_a8,
                                   &lapcqonly, &napwid, my.constData (),
                                   his.constData (), hisgrid.constData (), &ldiskdat,
                                   syncs.data (), snrs.data (), dts.data (), freqs.data (),
                                   naps.data (), quals.data (), bits77.data (),
                                   decodeds.data (), &nout);
    ftx_ft8_stage4_reset_c ();
    locker.unlock ();

    QList<DecodeRow> rows;
    for (int i = 0; i < std::max (0, nout) && i < kMaxLines; ++i)
      {
        DecodeRow r;
        r.text = QString::fromLatin1 (
            trim_fortran_field (decodeds.data () + i * kDecodedChars, kDecodedChars));
        r.freq = freqs[i];
        r.snr = snrs[i];
        r.dt = dts[i];
        rows.append (r);
      }
    return rows;
  }

  QList<DecodeRow> decode_ft4 (std::vector<qint16> const& samples)
  {
    std::array<int, kMaxLines> snrs {};
    std::array<float, kMaxLines> syncs {};
    std::array<float, kMaxLines> dts {};
    std::array<float, kMaxLines> freqs {};
    std::array<int, kMaxLines> naps {};
    std::array<float, kMaxLines> quals {};
    std::array<signed char, kBitsPerMessage * kMaxLines> bits77 {};
    std::array<char, kMaxLines * kDecodedChars> decodeds {};
    int nout = 0;

    int nqsoprogress = 0, nfqso = 1500, nfa = 200, nfb = 3000;
    int ndepth = 3, lapcqonly = 0, ncontest = 0;
    QByteArray my = to_fortran_field ("IW8XOU", 12);
    QByteArray his = to_fortran_field ("K1ABC", 12);

    QMutexLocker locker {&decodium::fortran::runtime_mutex ()};
    ftx_ft4_decode_c (samples.data (), &nqsoprogress, &nfqso, &nfa, &nfb,
                      &ndepth, &lapcqonly, &ncontest,
                      my.constData (), his.constData (),
                      syncs.data (), snrs.data (), dts.data (), freqs.data (),
                      naps.data (), quals.data (), bits77.data (),
                      decodeds.data (), &nout,
                      static_cast<size_t> (my.size ()),
                      static_cast<size_t> (his.size ()),
                      static_cast<size_t> (kDecodedChars));
    locker.unlock ();

    QList<DecodeRow> rows;
    for (int i = 0; i < std::max (0, nout) && i < kMaxLines; ++i)
      {
        DecodeRow r;
        r.text = QString::fromLatin1 (
            trim_fortran_field (decodeds.data () + i * kDecodedChars, kDecodedChars));
        r.freq = freqs[i];
        r.snr = snrs[i];
        r.dt = dts[i];
        rows.append (r);
      }
    return rows;
  }

  QList<DecodeRow> decode_ft2 (std::vector<qint16>& samples)
  {
    std::array<int, kMaxLines> snrs {};
    std::array<float, kMaxLines> dts {};
    std::array<float, kMaxLines> freqs {};
    std::array<int, kMaxLines> naps {};
    std::array<float, kMaxLines> quals {};
    std::array<signed char, kBitsPerMessage * kMaxLines> bits77 {};
    std::array<char, kMaxLines * kDecodedChars> decodeds {};
    int nout = 0;

    int nqsoprogress = 0, nfqso = 1500, nfa = 200, nfb = 3000;
    int ndepth = 3, ncontest = 0;
    QByteArray my = to_fortran_field ("IW8XOU", 12);
    QByteArray his = to_fortran_field ("K1ABC", 12);

    QMutexLocker locker {&decodium::fortran::runtime_mutex ()};
    ftx_ft2_stage7_clravg_c ();
    ft2_async_decode_ (samples.data (), &nqsoprogress, &nfqso, &nfa, &nfb,
                       &ndepth, &ncontest, my.data (), his.data (),
                       snrs.data (), dts.data (), freqs.data (),
                       naps.data (), quals.data (), bits77.data (),
                       decodeds.data (), &nout,
                       static_cast<size_t> (my.size ()),
                       static_cast<size_t> (his.size ()),
                       static_cast<size_t> (kDecodedChars));
    locker.unlock ();

    QList<DecodeRow> rows;
    for (int i = 0; i < std::max (0, nout) && i < kMaxLines; ++i)
      {
        DecodeRow r;
        r.text = QString::fromLatin1 (
            trim_fortran_field (decodeds.data () + i * kDecodedChars, kDecodedChars));
        r.freq = freqs[i];
        r.snr = snrs[i];
        r.dt = dts[i];
        rows.append (r);
      }
    return rows;
  }

  enum class Mode { Ft8, Ft4, Ft2 };

  struct ModeResult
  {
    QString name;
    int total {0};
    int passed {0};
  };

  // Run a single-message roundtrip: encode -> waveform -> PCM -> decode -> match.
  bool run_roundtrip (QTextStream& out, Mode mode, QString const& message)
  {
    decodium::txmsg::EncodedMessage enc;
    QVector<float> wave;
    int frame_samples = 0;
    int nsps = 0;
    QString mode_tag;

    switch (mode)
      {
      case Mode::Ft8:
        enc = decodium::txmsg::encodeFt8 (message);
        nsps = kFt8Nsps;
        frame_samples = kFt8Samples;
        mode_tag = QStringLiteral ("FT8");
        break;
      case Mode::Ft4:
        enc = decodium::txmsg::encodeFt4 (message);
        nsps = kFt4Nsps;
        frame_samples = kFt4Samples;
        mode_tag = QStringLiteral ("FT4");
        break;
      case Mode::Ft2:
        enc = decodium::txmsg::encodeFt2 (message);
        nsps = kFt2Nsps;
        frame_samples = kFt2Samples;
        mode_tag = QStringLiteral ("FT2");
        break;
      }

    if (!enc.ok || enc.tones.isEmpty ())
      {
        out << "  [FAIL] " << mode_tag << " encode failed for \"" << message << "\"\n";
        return false;
      }

    switch (mode)
      {
      case Mode::Ft8:
        wave = decodium::txwave::generateFt8Wave (enc.tones.constData (), enc.tones.size (),
                                                  nsps, kFt8Bt,
                                                  static_cast<float> (kSampleRate), kCarrierHz);
        break;
      case Mode::Ft4:
        wave = decodium::txwave::generateFt4Wave (enc.tones.constData (), enc.tones.size (),
                                                  nsps, static_cast<float> (kSampleRate),
                                                  kCarrierHz);
        break;
      case Mode::Ft2:
        wave = decodium::txwave::generateFt2Wave (enc.tones.constData (), enc.tones.size (),
                                                  nsps, static_cast<float> (kSampleRate),
                                                  kCarrierHz);
        break;
      }

    if (wave.isEmpty ())
      {
        out << "  [FAIL] " << mode_tag << " waveform generation failed for \""
            << message << "\"\n";
        return false;
      }

    // Offset: 0.5 s for FT8 (time-sync window), smaller for FT4/FT2 since
    // slots are shorter. Keeps tx audio comfortably inside the frame.
    int const offset_samples = (mode == Mode::Ft8) ? 6000 : 3000;
    std::vector<qint16> pcm = to_pcm_frame (wave, frame_samples, offset_samples, 0.85f);

    QList<DecodeRow> rows;
    switch (mode)
      {
      case Mode::Ft8: rows = decode_ft8 (pcm); break;
      case Mode::Ft4: rows = decode_ft4 (pcm); break;
      case Mode::Ft2: rows = decode_ft2 (pcm); break;
      }

    QString const want = canonical (QString::fromLatin1 (enc.msgsent).simplified ());
    for (DecodeRow const& r : rows)
      {
        QString const got = canonical (r.text);
        if (got == want)
          {
            out << "  [ OK ] " << mode_tag << " \"" << message
                << "\" -> decoded \"" << r.text.trimmed ()
                << "\" (snr=" << r.snr << " dt=" << r.dt
                << " f=" << qRound (r.freq) << ")\n";
            return true;
          }
      }

    out << "  [FAIL] " << mode_tag << " \"" << message << "\" not recovered. "
        << "encoded as \"" << QString::fromLatin1 (enc.msgsent).trimmed ()
        << "\", decoder returned " << rows.size () << " row(s):\n";
    for (DecodeRow const& r : rows)
      {
        out << "         - \"" << r.text.trimmed ()
            << "\" (snr=" << r.snr << " dt=" << r.dt
            << " f=" << qRound (r.freq) << ")\n";
      }
    return false;
  }

  ModeResult run_mode (QTextStream& out, Mode mode, QList<QsoMessage> const& qso)
  {
    ModeResult result;
    switch (mode)
      {
      case Mode::Ft8: result.name = QStringLiteral ("FT8"); break;
      case Mode::Ft4: result.name = QStringLiteral ("FT4"); break;
      case Mode::Ft2: result.name = QStringLiteral ("FT2"); break;
      }

    out << "=== " << result.name << " ===\n";
    for (QsoMessage const& m : qso)
      {
        out << "[" << m.direction << "] encoding \"" << m.text << "\"\n";
        ++result.total;
        if (run_roundtrip (out, mode, m.text))
          {
            ++result.passed;
          }
      }
    out << result.name << " summary: " << result.passed << "/" << result.total << " passed\n\n";
    return result;
  }
}

int main (int argc, char* argv[])
{
  QCoreApplication app {argc, argv};
  QCoreApplication::setApplicationName (QStringLiteral ("test_tx_pipeline"));

  try
    {
      QTextStream out {stdout};
      out << "Decodium TX pipeline roundtrip test\n";
      out << "  sample_rate=" << kSampleRate << " Hz, carrier=" << kCarrierHz << " Hz\n\n";

      QList<QsoMessage> const qso = make_qso ();

      QList<ModeResult> results;
      results.append (run_mode (out, Mode::Ft8, qso));
      results.append (run_mode (out, Mode::Ft4, qso));
      results.append (run_mode (out, Mode::Ft2, qso));

      int total = 0, passed = 0;
      for (ModeResult const& r : results)
        {
          total += r.total;
          passed += r.passed;
        }
      out << "=== TOTAL: " << passed << "/" << total << " messages roundtripped ===\n";
      return (passed == total) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  catch (std::exception const& e)
    {
      std::cerr << "test_tx_pipeline: " << e.what () << '\n';
      return EXIT_FAILURE;
    }
}
