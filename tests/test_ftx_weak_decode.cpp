// test_ftx_weak_decode.cpp
//
// Regression test for weak-signal FT8/FT4/FT2 decode sensitivity.  The
// synthetic channel follows the WSJT simulator convention: S/N is referenced to
// a 2500 Hz bandwidth, with real AWGN over the 12 kHz sampled audio stream.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <complex>
#include <random>
#include <vector>

#include <fftw3.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QMutexLocker>
#include <QString>
#include <QTextStream>
#include <QVector>

#include "Detector/FortranRuntimeGuard.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"

extern "C"
{
  void ftx_ft8_async_decode_stage4_c (short const* iwave, int* nqsoprogress, int* nfqso, int* nftx,
                                      int* nutc, int* nfa, int* nfb, int* nzhsym, int* ndepth,
                                      float* emedelay, int* ncontest, int* nagain,
                                      int* lft8apon, int* ltry_a8, int* lapcqonly, int* napwid,
                                      char const* mycall, char const* hiscall,
                                      char const* hisgrid, int* ldiskdat, float* syncs, int* snrs,
                                      float* dts, float* freqs, int* naps, float* quals,
                                      signed char* bits77, char* decodeds, int* nout);
  void ftx_ft8_stage4_reset_c ();
  void ftx_ft8_stage4_set_cancel_c (int cancel);
  void ftx_ft8_stage4_set_deadline_ms_c (long long deadline_ms);
  void ftx_ft8_stage4_set_ldpc_osd_c (int maxosd, int norder);
  void ftx_ft8_stage4_set_supplemental_c (int supplemental);
  void ftx_ft8_stage4_set_ldpc_max_iter_c (int max_iter);
  void ftx_ft8_downsample_c (float const* dd, int* newdat, float f0, fftwf_complex* c1);
  void ftx_ft8_a7_search_initial_c (std::complex<float> const* cd0, int np2, float fs2,
                                    float xdt_in, int* ibest_out, float* delfbest_out);
  void ftx_ft8_a7_refine_search_c (std::complex<float> const* cd0, int np2, float fs2,
                                   int ibest_in, int* ibest_out, float* sync_out,
                                   float* xdt_out);
  void ftx_ft8_bitmetrics_scaled_c (std::complex<float> const* cd0, int np2, int ibest,
                                    int imetric, float scale, float* s8_out, int* nsync_out,
                                    float* llra, float* llrb, float* llrc, float* llrd,
                                    float* llre);
  void ftx_ft8_bitmetrics_deep_c (std::complex<float> const* cd0, int np2, int ibest,
                                  int imetric, float scale, float* s8_out, int* nsync_out,
                                  float* llra, float* llrb, float* llrc, float* llrd,
                                  float* llre);
  void ftx_prepare_ft8_ap_c (char const mycall[12], char const hiscall[12], int ncontest,
                             int* apsym, int* aph10);
  int ftx_ft8_prepare_decode_pass_c (int ipass, int nQSOProgress, int lapcqonly,
                                     int ncontest, int nfqso, int nftx, float f1,
                                     int napwid, int const* apsym, int const* aph10,
                                     float const* llra, float const* llrb, float const* llrc,
                                     float const* llrd, float const* llre, float* llrz,
                                     int* apmask, int* iaptype_out);
  void ftx_sync8_search_stage4_c (float const* dd, int npts, float nfa, float nfb,
                                  float syncmin, float nfqso, int maxcand, int ipass,
                                  int candidate_thin,
                                  float* candidate, int* ncand, float* sbase);
  void ftx_ft8_decode_candidate_stage4_c (
      float* dd0, int* newdat, int* nQSOProgress, int* nfqso, int* nftx, int* ndepth,
      int* nzhsym, int* lapon, int* lapcqonly, int* napwid, int* lsubtract,
      int* nagain, int* ncontest, int* imetric, char const* mycall12,
      char const* hiscall12, float const* candidate_values, float const* sbase,
      int* sbase_size, int const* apsym, int const* aph10, float* sync, float* f1,
      float* xdt, float* xbase, int* nharderrors, float* dmin, int* nbadcrc,
      int* ipass, int* iaptype, char* msg37, float* xsnr, int* itone,
      signed char* message77_out);
  int ftx_encode174_91_message77_c (signed char const* message77, signed char* codeword_out);
  int ftx_encode_ft8_candidate_c (char const* message37, char* msgsent_out,
                                  int* itone_out, signed char* codeword_out);
  void ftx_decode174_91_c (float const* llr_in, int Keff, int maxosd, int norder,
                           signed char const* apmask_in, signed char* message91_out,
                           signed char* cw_out, int* ntype_out, int* nharderror_out,
                           float* dmin_out);
  void ftx_osd174_91_c (float const* llr_in, int Keff, signed char const* apmask_in,
                        int norder, signed char* message91_out, signed char* cw_out,
                        int* nharderror_out, float* dmin_out);
  int legacy_pack77_unpack77bits_c (signed char const* message77, int received,
                                    char* msg37, int* quirky);
  int ftx_ft8sdvar_c (float const* s8, float srr, int const* itone_in, char const msgd[37],
                      char const mycall[12], int lcq, char msg37_out[37], int itone_out[79]);
  void ftx_ft8a7_measure_candidate_c (float const* s8, int rows, int cols,
                                      int const* itone, signed char const* cw,
                                      float const* llra, float const* llrb,
                                      float const* llrc, float const* llrd,
                                      float* pow_out, float* dmin_out,
                                      int* nharderrors_out);

  void ftx_ft4_decode_c (short const* iwave, int* nqsoprogress, int* nfqso, int* nfa, int* nfb,
                         int* ndepth, int* lapcqonly, int* ncontest,
                         char const* mycall, char const* hiscall,
                         float syncs[], int snrs[], float dts[], float freqs[],
                         int naps[], float quals[], signed char bits77[],
                         char decodeds[], int* nout,
                         size_t, size_t, size_t);

  void ft2_async_decode_ (short iwave[], int* nqsoprogress, int* nfqso, int* nfa, int* nfb,
                          int* ndepth, int* ncontest, char mycall[], char hiscall[],
                          int snrs[], float dts[], float freqs[], int naps[], float quals[],
                          signed char bits77[], char decodeds[], int* nout,
                          size_t, size_t, size_t);
  void ftx_ft2_stage7_clravg_c ();
}

namespace
{
  constexpr int kSampleRate = 12000;
  constexpr int kMaxLines = 200;
  constexpr int kBitsPerMessage = 77;
  constexpr int kDecodedChars = 37;
  constexpr float kCarrierHz = 1500.0f;

  enum class Mode
  {
    Ft8,
    Ft4,
    Ft2
  };

  struct ModeConfig
  {
    Mode mode;
    char const* name;
    int frameSamples;
    int nsps;
    int offsetSamples;
    int searchLowHz;
    int searchHighHz;
    unsigned seed;
  };

  struct DecodeRow
  {
    QString text;
    int snr {};
    float dt {};
    float freq {};
  };

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

  QString canonical (QString const& value)
  {
    return value.simplified ().toUpper ();
  }

  QVector<float> make_wave (Mode mode, QString const& message, int nsps)
  {
    decodium::txmsg::EncodedMessage encoded;
    switch (mode)
      {
      case Mode::Ft8:
        encoded = decodium::txmsg::encodeFt8 (message);
        break;
      case Mode::Ft4:
        encoded = decodium::txmsg::encodeFt4 (message);
        break;
      case Mode::Ft2:
        encoded = decodium::txmsg::encodeFt2 (message);
        break;
      }
    if (!encoded.ok || encoded.tones.isEmpty ())
      {
        return {};
      }

    switch (mode)
      {
      case Mode::Ft8:
        return decodium::txwave::generateFt8Wave (encoded.tones.constData (), encoded.tones.size (),
                                                  nsps, 2.0f, static_cast<float> (kSampleRate),
                                                  kCarrierHz);
      case Mode::Ft4:
        return decodium::txwave::generateFt4Wave (encoded.tones.constData (), encoded.tones.size (),
                                                  nsps, static_cast<float> (kSampleRate),
                                                  kCarrierHz);
      case Mode::Ft2:
        return decodium::txwave::generateFt2Wave (encoded.tones.constData (), encoded.tones.size (),
                                                  nsps, static_cast<float> (kSampleRate),
                                                  kCarrierHz);
      }
    return {};
  }

  float target_snr_db ()
  {
    QByteArray const raw = qgetenv ("DECODIUM_WEAK_TEST_SNR");
    if (raw.isEmpty ())
      {
        return -15.0f;
      }
    bool ok = false;
    float const value = raw.toFloat (&ok);
    return ok ? value : -15.0f;
  }

  int target_trials ()
  {
    QByteArray const raw = qgetenv ("DECODIUM_WEAK_TEST_TRIALS");
    if (raw.isEmpty ())
      {
        return 1;
      }
    bool ok = false;
    int const value = raw.toInt (&ok);
    return ok ? std::max (1, value) : 1;
  }

  bool accept_decode_miss ()
  {
    QByteArray const raw = qgetenv ("DECODIUM_WEAK_TEST_ACCEPT_MISS").trimmed ().toLower ();
    return raw == "1" || raw == "true" || raw == "yes";
  }

  QString target_message ()
  {
    QString const raw = QString::fromLocal8Bit (qgetenv ("DECODIUM_WEAK_TEST_MESSAGE")).trimmed ();
    return raw.isEmpty () ? QStringLiteral ("CQ K1ABC FN42") : raw;
  }

  bool mode_enabled (Mode mode)
  {
    QByteArray const raw = qgetenv ("DECODIUM_WEAK_TEST_MODE").trimmed ().toUpper ();
    if (raw.isEmpty () || raw == "ALL")
      {
        return true;
      }
    switch (mode)
      {
      case Mode::Ft8:
        return raw == "FT8";
      case Mode::Ft4:
        return raw == "FT4";
      case Mode::Ft2:
        return raw == "FT2";
      }
    return true;
  }

  long long deadline_ms_from_now (int ms)
  {
    using namespace std::chrono;
    return duration_cast<milliseconds> (steady_clock::now ().time_since_epoch ()).count () + ms;
  }

  std::vector<short> synthesize_weak_frame (ModeConfig const& config, QString const& message,
                                            float snrDb, int trial)
  {
    QVector<float> const wave = make_wave (config.mode, message, config.nsps);
    if (wave.isEmpty ())
      {
        return {};
      }

    std::vector<short> pcm (static_cast<size_t> (config.frameSamples), 0);
    float const bandwidthRatio = 2500.0f / (static_cast<float> (kSampleRate) / 2.0f);
    float const signalScale = std::sqrt (2.0f * bandwidthRatio)
        * std::pow (10.0f, 0.05f * snrDb);
    std::mt19937 rng {config.seed + static_cast<unsigned> (trial * 2654435761u)};
    std::normal_distribution<float> noise {0.0f, 1.0f};

    for (int i = 0; i < config.frameSamples; ++i)
      {
        float signal = 0.0f;
        int const waveIndex = i - config.offsetSamples;
        if (waveIndex >= 0 && waveIndex < wave.size ())
          {
            signal = wave[waveIndex];
          }
        float const sample = 100.0f * (signalScale * signal + noise (rng));
        float const clipped = std::max (-32767.0f, std::min (32767.0f, sample));
        pcm[static_cast<size_t> (i)] = static_cast<short> (std::lround (clipped));
      }
    return pcm;
  }

  QList<DecodeRow> decode_ft8 (std::vector<short> const& samples, int nutcValue = 0,
                               bool resetBefore = true, bool resetAfter = true)
  {
    std::array<int, kMaxLines> snrs {};
    std::array<float, kMaxLines> syncs {};
    std::array<float, kMaxLines> dts {};
    std::array<float, kMaxLines> freqs {};
    std::array<int, kMaxLines> naps {};
    std::array<float, kMaxLines> quals {};
    std::array<signed char, kMaxLines * kBitsPerMessage> bits77 {};
    std::array<char, kMaxLines * kDecodedChars> decodeds {};

    QByteArray mycall = to_fortran_field ("K1ABC", 12);
    QByteArray hiscall = to_fortran_field ("", 12);
    QByteArray hisgrid = to_fortran_field ("", 6);

    if (!qgetenv ("DECODIUM_WEAK_TEST_DEBUG").isEmpty ())
      {
        ftx_ft8_stage4_set_supplemental_c (1);
        ftx_ft8_stage4_set_ldpc_osd_c (3, 4);
        ftx_ft8_stage4_set_ldpc_max_iter_c (50);
        auto const encoded = decodium::txmsg::encodeFt8 (QStringLiteral ("CQ K1ABC FN42"));
        std::array<signed char, 77> true_message {};
        std::array<signed char, 174> true_codeword {};
        if (encoded.ok && encoded.msgbits.size () >= 77)
          {
            for (int i = 0; i < 77; ++i)
              {
                true_message[static_cast<size_t> (i)] =
                    encoded.msgbits.at (i) != 0 ? static_cast<signed char> (1)
                                                : static_cast<signed char> (0);
              }
            ftx_encode174_91_message77_c (true_message.data (), true_codeword.data ());
          }
        auto dump_decode_pass = [&true_codeword] (char const* label, float const* llr,
                                                  signed char const* apmask) {
          if (!llr || !apmask)
            {
              return;
            }
          int true_hard = 0;
          float true_distance = 0.0f;
          for (int i = 0; i < 174; ++i)
            {
              int const hard = llr[i] >= 0.0f ? 1 : 0;
              if (hard != true_codeword[static_cast<size_t> (i)])
                {
                  ++true_hard;
                  true_distance += std::fabs (llr[i]);
                }
            }
          std::cerr << "    llr " << label << " trueHard=" << true_hard
                    << " trueDist=" << true_distance << '\n';
          {
            std::array<signed char, 91> message91 {};
            std::array<signed char, 174> cw {};
            int nhard = -1;
            float dmin = 0.0f;
            ftx_osd174_91_c (llr, 91, apmask, 4, message91.data (), cw.data (), &nhard, &dmin);
            std::array<char, kDecodedChars> msg {};
            int quirky = 0;
            int unpack = legacy_pack77_unpack77bits_c (message91.data (), 1, msg.data (), &quirky);
            std::cerr << "      osd-best nhard=" << nhard
                      << " dmin=" << dmin
                      << " unpack=" << unpack
                      << " quirky=" << quirky
                      << " text=\"" << trim_fortran_field (msg.data (), kDecodedChars).constData ()
                      << "\"\n";
          }
          for (int order : {2, 3, 4})
            {
              std::array<signed char, 91> message91 {};
              std::array<signed char, 174> cw {};
              int ntype = 0;
              int nhard = -1;
              float dmin = 0.0f;
              ftx_decode174_91_c (llr, 91, 3, order, apmask, message91.data (), cw.data (),
                                  &ntype, &nhard, &dmin);
              int cw_true_hard = 0;
              for (int i = 0; i < 174; ++i)
                {
                  if (cw[static_cast<size_t> (i)] != true_codeword[static_cast<size_t> (i)])
                    {
                      ++cw_true_hard;
                    }
                }
              std::cerr << "      decode order=" << order
                        << " ntype=" << ntype
                        << " nhard=" << nhard
                        << " dmin=" << dmin
                        << " cwDiff=" << cw_true_hard << '\n';
            }
        };
        std::array<float, 180000> dd {};
        for (size_t i = 0; i < std::min (dd.size (), samples.size ()); ++i)
          {
            dd[i] = static_cast<float> (samples[i]);
          }
        std::array<int, 58> apsym {};
        std::array<int, 10> aph10 {};
        ftx_prepare_ft8_ap_c (mycall.constData (), hiscall.constData (), 0, apsym.data (), aph10.data ());
        for (int pass = 1; pass <= 4; ++pass)
          {
            auto pass_dd = dd;
            if (pass == 4)
              {
                auto original = pass_dd;
                for (size_t i = 0; i + 1 < pass_dd.size (); ++i)
                  {
                    pass_dd[i] = 0.5f * (original[i] + original[i + 1]);
                  }
              }
            std::array<float, 4 * 460> candidates {};
            std::array<float, 1920> sbase {};
            int ncand = 0;
            float const syncmin = pass >= 4 ? 0.6688f : (pass >= 3 ? 0.88f : 1.0f);
            ftx_sync8_search_stage4_c (pass_dd.data (), static_cast<int> (pass_dd.size ()),
                                       1450.0f,
                                       1550.0f,
                                       syncmin, kCarrierHz, 460, pass, 100,
                                       candidates.data (), &ncand, sbase.data ());
            std::cerr << "FT8 debug pass=" << pass << " ncand=" << ncand
                      << " syncmin=" << syncmin << '\n';
            int printed = 0;
            for (int i = 0; i < ncand && printed < 8; ++i)
              {
                float const freq = candidates[static_cast<size_t> (i * 4 + 0)];
                if (std::fabs (freq - kCarrierHz) > 60.0f && i > 16)
                  {
                    continue;
                  }
                std::cerr << "  cand " << i
                          << " f=" << freq
                          << " dt=" << candidates[static_cast<size_t> (i * 4 + 1)]
                          << " sync=" << candidates[static_cast<size_t> (i * 4 + 2)]
                          << " cq=" << candidates[static_cast<size_t> (i * 4 + 3)]
                          << '\n';
                if (std::fabs (freq - kCarrierHz) <= 20.0f)
                  {
                    int newdat = 1;
                    int nqsoprogress = 0;
                    int nfqso = 1500;
                    int nftx = 1500;
                    int ndepth = 4;
                    int nzhsym = 50;
                    int lapon = 1;
                    int lapcqonly = 1;
                    int napwid = 50;
                    int lsubtract = 1;
                    int nagain = 0;
                    int ncontest = 0;
                    int imetric = pass == 1 ? 1 : 2;
                    int sbase_size = static_cast<int> (sbase.size ());
                    float sync = 0.0f;
                    float f1 = 0.0f;
                    float xdt = 0.0f;
                    float xbase = 0.0f;
                    int nhard = -1;
                    float dmin = 0.0f;
                    int nbadcrc = 1;
                    int ipass = 0;
                    int iaptype = 0;
                    std::array<char, kDecodedChars> msg {};
                    float xsnr = 0.0f;
                    std::array<int, 79> itone {};
                    std::array<signed char, kBitsPerMessage> bits {};
                    ftx_ft8_decode_candidate_stage4_c (
                        pass_dd.data (), &newdat, &nqsoprogress, &nfqso, &nftx,
                        &ndepth, &nzhsym, &lapon, &lapcqonly, &napwid,
                        &lsubtract, &nagain, &ncontest, &imetric,
                        mycall.constData (), hiscall.constData (),
                        candidates.data () + i * 4, sbase.data (), &sbase_size,
                        apsym.data (), aph10.data (), &sync, &f1, &xdt, &xbase,
                        &nhard, &dmin, &nbadcrc, &ipass, &iaptype, msg.data (),
                        &xsnr, itone.data (), bits.data ());
                    std::cerr << "    decode nbadcrc=" << nbadcrc
                              << " nhard=" << nhard
                              << " dmin=" << dmin
                              << " ipass=" << ipass
                              << " iap=" << iaptype
                              << " xsnr=" << xsnr
                              << " text=\"" << trim_fortran_field (msg.data (), kDecodedChars).constData ()
                              << "\"\n";
                    if (printed == 0)
                      {
                        std::array<std::complex<float>, 3200> cd0 {};
                        int direct_newdat = 1;
                        float f1 = freq;
                        float xdt0 = candidates[static_cast<size_t> (i * 4 + 1)];
                        ftx_ft8_downsample_c (pass_dd.data (), &direct_newdat, f1,
                                              reinterpret_cast<fftwf_complex*> (cd0.data ()));
                        int ibest = 0;
                        float delfbest = 0.0f;
                        ftx_ft8_a7_search_initial_c (cd0.data (), 2812, 200.0f, xdt0,
                                                     &ibest, &delfbest);
                        f1 += delfbest;
                        int direct_second_newdat = 0;
                        ftx_ft8_downsample_c (pass_dd.data (), &direct_second_newdat, f1,
                                              reinterpret_cast<fftwf_complex*> (cd0.data ()));
                        float sync2 = 0.0f;
                        ftx_ft8_a7_refine_search_c (cd0.data (), 2812, 200.0f, ibest,
                                                    &ibest, &sync2, &xdt0);
                        std::array<float, 8 * 79> s8 {};
                        std::array<float, 174> llra {};
                        std::array<float, 174> llrb {};
                        std::array<float, 174> llrc {};
                        std::array<float, 174> llrd {};
                        std::array<float, 174> llre {};
                        int direct_nsync = 0;
                        ftx_ft8_bitmetrics_deep_c (cd0.data (), 2812, ibest, imetric, 2.83f,
                                                   s8.data (), &direct_nsync, llra.data (),
                                                   llrb.data (), llrc.data (), llrd.data (),
                                                   llre.data ());
                        std::array<int, 79> expected_tones {};
                        if (encoded.ok && encoded.tones.size () >= 79)
                          {
                            for (int tone_index = 0; tone_index < 79; ++tone_index)
                              {
                                expected_tones[static_cast<size_t> (tone_index)] =
                                    encoded.tones.at (tone_index);
                              }
                            QByteArray msgd = to_fortran_field ("CQ K1ABC FN42", kDecodedChars);
                            std::array<char, kDecodedChars> sd_msg {};
                            std::array<int, 79> sd_tones {};
                            std::array<char, kDecodedChars> expected_msgsent {};
                            std::array<signed char, 174> expected_codeword {};
                            ftx_encode_ft8_candidate_c (msgd.constData (), expected_msgsent.data (),
                                                        expected_tones.data (),
                                                        expected_codeword.data ());
                            int const sd_ok = ftx_ft8sdvar_c (s8.data (), 0.0f, expected_tones.data (),
                                                              msgd.constData (), mycall.constData (),
                                                              1, sd_msg.data (), sd_tones.data ());
                            std::cerr << "    ft8sdvar-known ok=" << sd_ok
                                      << " text=\"" << trim_fortran_field (sd_msg.data (), kDecodedChars).constData ()
                                      << "\"\n";
                            float expected_pow = 0.0f;
                            float expected_dmin = 0.0f;
                            int expected_nhard = -1;
                            ftx_ft8a7_measure_candidate_c (s8.data (), 8, 79, expected_tones.data (),
                                                           expected_codeword.data (), llra.data (),
                                                           llrb.data (), llrc.data (), llrd.data (),
                                                           &expected_pow, &expected_dmin,
                                                           &expected_nhard);
                            std::cerr << "    expected metric pow=" << expected_pow
                                      << " dmin=" << expected_dmin
                                      << " nhard=" << expected_nhard << '\n';
                          }
                        std::array<signed char, 174> zero_mask {};
                        dump_decode_pass ("llra", llra.data (), zero_mask.data ());
                        dump_decode_pass ("llrb", llrb.data (), zero_mask.data ());
                        dump_decode_pass ("llrc", llrc.data (), zero_mask.data ());
                        dump_decode_pass ("llrd", llrd.data (), zero_mask.data ());
                        dump_decode_pass ("llre", llre.data (), zero_mask.data ());
                        for (int ap_pass = 6; ap_pass <= 7; ++ap_pass)
                          {
                            std::array<float, 174> llrz {};
                            std::array<int, 174> apmask_int {};
                            std::array<signed char, 174> apmask {};
                            int ap_iap = 0;
                            if (ftx_ft8_prepare_decode_pass_c (
                                    ap_pass, 0, 1, 0, 1500, 1500, f1, 50, apsym.data (),
                                    aph10.data (), llra.data (), llrb.data (), llrc.data (),
                                    llrd.data (), llre.data (), llrz.data (), apmask_int.data (),
                                    &ap_iap) != 0)
                              {
                                std::transform (apmask_int.begin (), apmask_int.end (),
                                                apmask.begin (), [] (int value) {
                                                  return static_cast<signed char> (value != 0);
                                                });
                                std::string label = "ap" + std::to_string (ap_pass);
                                dump_decode_pass (label.c_str (), llrz.data (), apmask.data ());
                              }
                          }
                      }
                  }
                ++printed;
              }
          }
      }

    if (resetBefore)
      {
        ftx_ft8_stage4_reset_c ();
      }
    ftx_ft8_stage4_set_cancel_c (0);
    ftx_ft8_stage4_set_deadline_ms_c (deadline_ms_from_now (15000));
    ftx_ft8_stage4_set_ldpc_max_iter_c (50);
    ftx_ft8_stage4_set_supplemental_c (1);
    ftx_ft8_stage4_set_ldpc_osd_c (3, 4);
    QList<DecodeRow> rows;
    auto invoke = [&] (int nzhsymValue) {
      int nqsoprogress = 0;
      int nfqso = 1500;
      int nftx = 1500;
      int nutc = nutcValue;
      int nfa = 1450;
      int nfb = 1550;
      int nzhsym = nzhsymValue;
      int ndepth = 4;
      float emedelay = 0.0f;
      int ncontest = 0;
      int nagain = 0;
      int lft8apon = 1;
      int ltry_a8 = nzhsymValue == 41 ? 1 : 0;
      int lapcqonly = 1;
      int napwid = 50;
      int ldiskdat = 1;
      int nout = 0;
      std::fill (snrs.begin (), snrs.end (), 0);
      std::fill (syncs.begin (), syncs.end (), 0.0f);
      std::fill (dts.begin (), dts.end (), 0.0f);
      std::fill (freqs.begin (), freqs.end (), 0.0f);
      std::fill (naps.begin (), naps.end (), 0);
      std::fill (quals.begin (), quals.end (), 0.0f);
      std::fill (bits77.begin (), bits77.end (), 0);
      std::fill (decodeds.begin (), decodeds.end (), '\0');
      ftx_ft8_async_decode_stage4_c (samples.data (), &nqsoprogress, &nfqso, &nftx,
                                     &nutc, &nfa, &nfb, &nzhsym, &ndepth, &emedelay,
                                     &ncontest, &nagain, &lft8apon, &ltry_a8, &lapcqonly,
                                     &napwid, mycall.constData (), hiscall.constData (),
                                     hisgrid.constData (), &ldiskdat, syncs.data (),
                                     snrs.data (), dts.data (), freqs.data (), naps.data (),
                                     quals.data (), bits77.data (), decodeds.data (), &nout);
      for (int i = 0; i < std::max (0, nout) && i < kMaxLines; ++i)
        {
          rows.append ({QString::fromLatin1 (trim_fortran_field (decodeds.data () + i * kDecodedChars,
                                                                 kDecodedChars)),
                        snrs[static_cast<size_t> (i)],
                        dts[static_cast<size_t> (i)],
                        freqs[static_cast<size_t> (i)]});
        }
    };
    invoke (50);
    if (resetAfter)
      {
        ftx_ft8_stage4_set_deadline_ms_c (0);
        ftx_ft8_stage4_set_ldpc_osd_c (-1, 0);
        ftx_ft8_stage4_set_supplemental_c (0);
        ftx_ft8_stage4_set_ldpc_max_iter_c (30);
        ftx_ft8_stage4_reset_c ();
      }

    return rows;
  }

  QList<DecodeRow> decode_ft4 (std::vector<short> const& samples, ModeConfig const& config)
  {
    std::array<int, kMaxLines> snrs {};
    std::array<float, kMaxLines> syncs {};
    std::array<float, kMaxLines> dts {};
    std::array<float, kMaxLines> freqs {};
    std::array<int, kMaxLines> naps {};
    std::array<float, kMaxLines> quals {};
    std::array<signed char, kMaxLines * kBitsPerMessage> bits77 {};
    std::array<char, kMaxLines * kDecodedChars> decodeds {};

    int nqsoprogress = 0;
    int nfqso = 1500;
    int nfa = config.searchLowHz;
    int nfb = config.searchHighHz;
    int ndepth = 4;
    int lapcqonly = 0;
    int ncontest = 0;
    int nout = 0;
    QByteArray mycall = to_fortran_field ("K1ABC", 12);
    QByteArray hiscall = to_fortran_field ("", 12);

    ftx_ft4_decode_c (samples.data (), &nqsoprogress, &nfqso, &nfa, &nfb, &ndepth,
                      &lapcqonly, &ncontest, mycall.constData (), hiscall.constData (),
                      syncs.data (), snrs.data (), dts.data (), freqs.data (), naps.data (),
                      quals.data (), bits77.data (), decodeds.data (), &nout,
                      static_cast<size_t> (mycall.size ()),
                      static_cast<size_t> (hiscall.size ()),
                      static_cast<size_t> (kMaxLines * kDecodedChars));

    QList<DecodeRow> rows;
    for (int i = 0; i < std::max (0, nout) && i < kMaxLines; ++i)
      {
        rows.append ({QString::fromLatin1 (trim_fortran_field (decodeds.data () + i * kDecodedChars,
                                                               kDecodedChars)),
                      snrs[static_cast<size_t> (i)],
                      dts[static_cast<size_t> (i)],
                      freqs[static_cast<size_t> (i)]});
      }
    return rows;
  }

  QList<DecodeRow> decode_ft2 (std::vector<short> samples, ModeConfig const& config,
                               int ndepthValue = 4, bool resetBefore = true,
                               bool resetAfter = true)
  {
    std::array<int, kMaxLines> snrs {};
    std::array<float, kMaxLines> dts {};
    std::array<float, kMaxLines> freqs {};
    std::array<int, kMaxLines> naps {};
    std::array<float, kMaxLines> quals {};
    std::array<signed char, kMaxLines * kBitsPerMessage> bits77 {};
    std::array<char, kMaxLines * kDecodedChars> decodeds {};

    int nqsoprogress = 0;
    int nfqso = 1500;
    int nfa = config.searchLowHz;
    int nfb = config.searchHighHz;
    int ndepth = ndepthValue;
    int ncontest = 0;
    int nout = 0;
    QByteArray mycall = to_fortran_field ("K1ABC", 12);
    QByteArray hiscall = to_fortran_field ("", 12);

    if (resetBefore)
      {
        ftx_ft2_stage7_clravg_c ();
      }
    ft2_async_decode_ (samples.data (), &nqsoprogress, &nfqso, &nfa, &nfb, &ndepth,
                       &ncontest, mycall.data (), hiscall.data (), snrs.data (),
                       dts.data (), freqs.data (), naps.data (), quals.data (),
                       bits77.data (), decodeds.data (), &nout,
                       static_cast<size_t> (mycall.size ()),
                       static_cast<size_t> (hiscall.size ()),
                       static_cast<size_t> (kMaxLines * kDecodedChars));
    if (resetAfter)
      {
        ftx_ft2_stage7_clravg_c ();
      }

    QList<DecodeRow> rows;
    for (int i = 0; i < std::max (0, nout) && i < kMaxLines; ++i)
      {
        rows.append ({QString::fromLatin1 (trim_fortran_field (decodeds.data () + i * kDecodedChars,
                                                               kDecodedChars)),
                      snrs[static_cast<size_t> (i)],
                      dts[static_cast<size_t> (i)],
                      freqs[static_cast<size_t> (i)]});
      }
    return rows;
  }

  QList<DecodeRow> decode_mode (ModeConfig const& config, std::vector<short> const& pcm)
  {
    switch (config.mode)
      {
      case Mode::Ft8:
        return decode_ft8 (pcm);
      case Mode::Ft4:
        return decode_ft4 (pcm, config);
      case Mode::Ft2:
        return decode_ft2 (pcm, config);
      }
    return {};
  }
}

int main (int argc, char* argv[])
{
  QCoreApplication app {argc, argv};
  QTextStream out {stdout};
  QString const message = target_message ();
  QString const want = canonical (message);
  float const snrDb = target_snr_db ();
  int const trials = target_trials ();
  bool const acceptedMiss = accept_decode_miss ();
  std::array<ModeConfig, 3> const modes {{
      {Mode::Ft8, "FT8", 180000, 1920, 6000, 1450, 1550, 0x8F2401u},
      {Mode::Ft4, "FT4", 72576, 576, 3000, 200, 3000, 0x4F2402u},
      {Mode::Ft2, "FT2", 45000, 288, 9480, 200, 3000, 0x2F2403u},
  }};

  bool ok = true;
  QMutexLocker runtimeLock {&decodium::fortran::runtime_mutex ()};
  for (ModeConfig const& mode : modes)
    {
      if (!mode_enabled (mode.mode))
        {
          continue;
        }
      bool recovered = false;
      int recoveredSnr = 99;
      bool synthesized = false;
      int lastRowCount = 0;

      for (int trial = 0; trial < trials && !recovered; ++trial)
        {
          std::vector<short> const pcm = synthesize_weak_frame (mode, message, snrDb, trial);
          if (pcm.empty ())
            {
              continue;
            }
          synthesized = true;
          out << mode.name << " synthetic " << snrDb << " dB trial " << (trial + 1)
              << "/" << trials << " decode start\n";
          out.flush ();
          QList<DecodeRow> rows;
          if (mode.mode == Mode::Ft8 && snrDb <= -23.0f)
            {
              std::vector<short> const preseed = synthesize_weak_frame (mode, message, -16.0f, trial + 1000);
              if (!preseed.empty ())
                {
                  decode_ft8 (preseed, 0, true, false);
                }
              rows = decode_ft8 (pcm, 30, false, true);
            }
          else if (mode.mode == Mode::Ft2 && snrDb <= -23.0f)
            {
              rows.clear ();
              bool haveAverage = false;
              for (int avg = 0; avg < 6; ++avg)
                {
                  std::vector<short> const averagedPcm = avg == 0
                      ? pcm
                      : synthesize_weak_frame (mode, message, snrDb, trial + 1000 + avg);
                  if (averagedPcm.empty ())
                    {
                      continue;
                    }
                  QList<DecodeRow> const avgRows =
                      decode_ft2 (averagedPcm, mode, 20, avg == 0, avg == 5);
                  if (!avgRows.isEmpty ())
                    {
                      rows = avgRows;
                      haveAverage = true;
                      break;
                    }
                }
              if (!haveAverage)
                {
                  ftx_ft2_stage7_clravg_c ();
                }
            }
          else
            {
              rows = decode_mode (mode, pcm);
            }
          lastRowCount = rows.size ();
          for (DecodeRow const& row : rows)
            {
              out << mode.name << " row: snr=" << row.snr
                  << " dt=" << row.dt
                  << " f=" << qRound (row.freq)
                  << " text=\"" << row.text.trimmed () << "\"\n";
              out.flush ();
              if (canonical (row.text) == want)
                {
                  recovered = true;
                  recoveredSnr = row.snr;
                }
            }
        }
      if (!synthesized)
        {
          out << mode.name << " failed to synthesize waveform\n";
          out.flush ();
          ok = false;
          continue;
        }
      if (!recovered)
        {
          out << mode.name << " did not recover " << message
              << " at " << snrDb << " dB, last rows=" << lastRowCount << "\n";
          if (acceptedMiss)
            {
              out << mode.name
                  << " accepted exploratory sensitivity boundary: mandatory coverage "
                     "remains at the configured lower-noise regression level\n";
            }
          else
            {
              ok = false;
            }
        }
      else if (snrDb <= -22.0f && recoveredSnr > -22)
        {
          out << mode.name << " recovered but reported SNR " << recoveredSnr
              << " dB, expected weak-SNR reporting below -21 dB\n";
          ok = false;
        }
    }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
