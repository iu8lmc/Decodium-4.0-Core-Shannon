#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutexLocker>
#include <QSet>
#include <QStringList>
#include <QTextStream>

#include "Detector/FtxApStorico.hpp"
#include <cstdlib>
#include <iostream>
#include <QtEndian>

#include <fftw3.h>

#include "Detector/FortranRuntimeGuard.hpp"

extern "C"
{
  int ftx_ft8_ap_storico_tentativi_c ();
  int ftx_ft8_ap_msg_tentativi_c ();
  void ftx_ft8_async_decode_stage4_c (short const* iwave, int* nqsoprogress, int* nfqso, int* nftx,
                                      int* nutc, int* nfa, int* nfb, int* nzhsym, int* ndepth,
                                      float* emedelay, int* ncontest, int* nagain,
                                      int* lft8apon, int* ltry_a8, int* lapcqonly, int* napwid,
                                      char const* mycall, char const* hiscall,
                                      char const* hisgrid, int* ldiskdat, float* syncs, int* snrs,
                                      float* dts, float* freqs, int* naps, float* quals,
                                      signed char* bits77, char* decodeds, int* nout);
  void ftx_ft8_cpp_dsp_rollout_stage_override_c (int stage);
  void ftx_ft8_cpp_dsp_rollout_stage_reset_c ();
  void ftx_ft8_stage4_reset_c ();
  void ftx_ft8_stage4_set_deadline_ms_c (long long deadline_ms);
  void ftx_ft8_stage4_set_ldpc_osd_c (int maxosd, int norder);
  void ftx_ft8_stage4_set_ldpc_max_iter_c (int max_iter);
  void ftx_ft8_stage4_set_decode_options_c (int low_thresholds, int subpass,
                                            int cycles, int rx_freq_sensitivity,
                                            int candidate_thin);
  void ftx_ft8_stage4_set_supplemental_c (int supplemental);
  void ftx_ft8_stage4_set_superfox_options_c (int enabled, int ntol_hz);
  void ftx_ft8_stage4_seed_known_cq_c (char const* call, char const* grid,
                                       float freq, float dt, int nutc);
  void ftx_ft8_stage4_seed_known_cq_call_c (char const* call,
                                            float freq, float dt, int nutc);
  void ftx_ft8_a7_save_c (int jseq, float dt, float freq, char const* msg37);
  void ftx_ft8_downsample_c (float const* dd, int* newdat, float f0, fftwf_complex* c1);
  void ftx_ft8_a7_search_initial_c (std::complex<float> const* cd0, int np2, float fs2,
                                    float xdt_in, int* ibest_out, float* delfbest_out);
  void ftx_ft8_a7_refine_search_c (std::complex<float> const* cd0, int np2, float fs2,
                                   int ibest_in, int* ibest_out, float* sync_out,
                                   float* xdt_out);
  void ftx_ft8_bitmetrics_deep_c (std::complex<float> const* cd0, int np2, int ibest,
                                  int imetric, float scale, float* s8_out, int* nsync_out,
                                  float* llra, float* llrb, float* llrc, float* llrd,
                                  float* llre);
  void ftx_ft8_bitmetrics_deep_equalized_c (std::complex<float> const* cd0, int np2,
                                            int ibest, int imetric, float scale,
                                            float* s8_out, int* nsync_out,
                                            float* llra, float* llrb,
                                            float* llrc, float* llrd, float* llre);
  void ftx_prepare_ft8_ap_c (char const mycall[12], char const hiscall[12], int ncontest,
                             int* apsym, int* aph10);
  int ftx_ft8_prepare_decode_pass_c (int ipass, int nQSOProgress, int lapcqonly,
                                     int ncontest, int nfqso, int nftx, float f1,
                                     int napwid, int const* apsym, int const* aph10,
                                     float const* llra, float const* llrb,
                                     float const* llrc, float const* llrd,
                                     float const* llre, float* llrz,
                                     int* apmask, int* iaptype_out);
  void ftx_decode174_91_c (float const* llr_in, int Keff, int maxosd, int norder,
                           signed char const* apmask_in, signed char* message91_out,
                           signed char* cw_out, int* ntype_out, int* nharderror_out,
                           float* dmin_out);
  void ftx_osd174_91_c (float const* llr_in, int Keff, signed char const* apmask_in,
                        int norder, signed char* message91_out, signed char* cw_out,
                        int* nharderror_out, float* dmin_out);
  void ftx_ldpc174_91_metrics_c (signed char const* cw, float const* llr,
                                 int* nharderrors_out, float* dmin_out);
  int ftx_encode_ft8_candidate_c (char const* message37, char* msgsent_out,
                                  int* itone_out, signed char* codeword_out);
  void ftx_subtract_ft8_c (float* dd0, int const* itone, float f0, float dt, int lrefinedt);
  int legacy_pack77_unpack77bits_c (signed char const* message77, int received,
                                    char* msg37, int* quirky);
  void ftx_ft8_stage4_seed_hash_call_c (char const* call);
  void ftx_ft8_stage4_apply_hash_seed_cache_c ();
  void ftx_ft8_prepare_pass_c (int ndepth, int ipass, int ndecodes,
                               float* syncmin, int* imetric,
                               int* lsubtract, int* run_pass);
  void ftx_sync8_search_stage4_c (float const* dd, int npts, float nfa, float nfb,
                                  float syncmin, float nfqso, int maxcand, int ipass,
                                  int candidate_thin, float* candidate, int* ncand,
                                  float* sbase);
}

namespace
{
  constexpr int kFt8SampleCount {180000};
  constexpr int kFt8MaxLines {200};
  constexpr int kBitsPerMessage {77};
  constexpr int kDecodedChars {37};

  long long steady_clock_ms ()
  {
    using namespace std::chrono;
    return duration_cast<milliseconds> (steady_clock::now ().time_since_epoch ()).count ();
  }

  struct WavData
  {
    int sample_rate {};
    int channels {};
    int bits_per_sample {};
    int source_samples {};
    bool padded {};
    bool truncated {};
    std::vector<short> samples;
  };

  struct DecodeLine
  {
    int snr {};
    float dt {};
    float freq {};
    int nap {};
    float qual {};
    QString decoded;
  };

  struct DecodeResult
  {
    int stage {};
    std::vector<DecodeLine> lines;
  };

  [[noreturn]] void fail (QString const& message)
  {
    throw std::runtime_error {message.toStdString ()};
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

  QString decode_key (DecodeLine const& line)
  {
    return QStringLiteral ("%1|%2")
        .arg (qRound (line.freq), 5)
        .arg (line.decoded.trimmed ());
  }

  QString message_key (DecodeLine const& line)
  {
    return line.decoded.trimmed ();
  }

  int infer_nutc_from_path (QString const& path, int fallback)
  {
    QString const stem = QFileInfo {path}.completeBaseName ();
    for (int pos = stem.size () - 6; pos >= 0; --pos)
      {
        QString const token = stem.mid (pos, 6);
        bool digits = true;
        for (QChar const ch : token)
          {
            if (!ch.isDigit ())
              {
                digits = false;
                break;
              }
          }
        if (!digits)
          {
            continue;
          }
        int const hh = token.mid (0, 2).toInt ();
        int const mm = token.mid (2, 2).toInt ();
        int const ss = token.mid (4, 2).toInt ();
        if (hh >= 0 && hh < 24 && mm >= 0 && mm < 60 && ss >= 0 && ss < 60)
          {
            return token.toInt ();
          }
      }
    return fallback;
  }

  QString describe_line (DecodeLine const& line)
  {
    return QStringLiteral ("snr=%1 dt=%2 freq=%3 nap=%4 qual=%5 decoded=\"%6\"")
        .arg (line.snr, 4)
        .arg (line.dt, 0, 'f', 1)
        .arg (qRound (line.freq), 5)
        .arg (line.nap, 2)
        .arg (line.qual, 0, 'f', 3)
        .arg (line.decoded.trimmed ());
  }

  void dump_candidates (QTextStream& out, std::vector<short> const& samples,
                        int nfa, int nfb, int nfqso, int nftx, int ndepth,
                        int ncontest, int lft8apon, int lapcqonly, int napwid,
                        int lowThresholds, int subpass, int cycles,
                        int rxFreqSensitivity, int candidateThin,
                        int windowHz, QString const& probeMessage,
                        QByteArray const& mycall, QByteArray const& hiscall,
                        bool probeOsd5, bool probeMetricsOnly,
                        int dumpPassFilter, int dumpMaxCandidates)
  {
    constexpr int kMaxCandidates {960};
    constexpr int kSbaseSize {1920};
    constexpr int kNp2 {2812};
    constexpr float kFs2 {200.0f};
    constexpr float kScale {2.83f};
    std::array<float, kFt8SampleCount> dd {};
    for (size_t i = 0; i < dd.size () && i < samples.size (); ++i)
      {
        dd[i] = static_cast<float> (samples[i]);
      }

    bool const probe = !probeMessage.trimmed ().isEmpty ();
    std::array<char, kDecodedChars> probeMsgSent {};
    std::array<int, 79> probeTones {};
    std::array<signed char, 174> probeCodeword {};
    if (probe)
      {
        QByteArray const probeField = to_fortran_field (probeMessage.toLatin1 (), kDecodedChars);
        if (ftx_encode_ft8_candidate_c (probeField.constData (), probeMsgSent.data (),
                                        probeTones.data (), probeCodeword.data ()) == 0)
          {
            fail (QStringLiteral ("cannot encode probe message \"%1\"").arg (probeMessage));
          }
        out << "Probe message: " << trim_fortran_field (probeMsgSent.data (), kDecodedChars)
            << '\n';
      }

    QByteArray const mycall12 = to_fortran_field (mycall, 12);
    QByteArray const hiscall12 = to_fortran_field (hiscall, 12);
    std::array<int, 58> apsym {};
    std::array<int, 10> aph10 {};
    ftx_prepare_ft8_ap_c (mycall12.constData (), hiscall12.constData (), ncontest,
                          apsym.data (), aph10.data ());

    int npass = 5;
    if (ndepth <= 2)
      {
        npass = 3;
      }
    if (ndepth == 1)
      {
        npass = 2;
      }
    if (ndepth >= 3)
      {
        if (cycles >= 2)
          {
            npass = std::max (npass, 6);
          }
        if (cycles >= 3)
          {
            npass = std::max (npass, 9);
          }
        if (subpass != 0)
          {
            npass = std::max (npass, 8);
          }
      }

    out << "\nCandidate dump around " << nfqso << " Hz +/- " << windowHz << " Hz\n";
    auto ddCycleBase = dd;
    for (int ipass = 1; ipass <= npass; ++ipass)
      {
        if (dumpPassFilter > 0 && ipass != dumpPassFilter)
          {
            continue;
          }
        auto passDd = dd;
        if (ndepth >= 4 && (ipass == 4 || ipass == 7))
          {
            passDd = ddCycleBase;
            if (ipass == 7)
              {
                for (int i = static_cast<int> (passDd.size ()) - 1; i >= 1; --i)
                  {
                    passDd[static_cast<size_t> (i)] =
                        0.5f * (ddCycleBase[static_cast<size_t> (i - 1)]
                                + ddCycleBase[static_cast<size_t> (i)]);
                  }
              }
            else
              {
                for (int i = 0; i + 1 < static_cast<int> (passDd.size ()); ++i)
                  {
                    passDd[static_cast<size_t> (i)] =
                        0.5f * (ddCycleBase[static_cast<size_t> (i)]
                                + ddCycleBase[static_cast<size_t> (i + 1)]);
                  }
              }
          }
        float syncmin = 0.0f;
        int imetric = 0;
        int lsubtract = 0;
        int runPass = 0;
        ftx_ft8_prepare_pass_c (ndepth, ipass, 0, &syncmin, &imetric, &lsubtract,
                                &runPass);
        if (lowThresholds != 0)
          {
            float thresholdScale = subpass != 0 ? 0.82f : 0.90f;
            if (rxFreqSensitivity >= 3)
              {
                thresholdScale *= 0.95f;
              }
            if (ipass >= 4)
              {
                thresholdScale *= subpass != 0 ? 0.90f : 0.95f;
              }
            syncmin *= thresholdScale;
          }
        if (runPass == 0)
          {
            continue;
          }

        std::array<float, 4 * kMaxCandidates> candidates {};
        std::array<float, kSbaseSize> sbase {};
        int ncand = 0;
        ftx_sync8_search_stage4_c (passDd.data (), static_cast<int> (passDd.size ()),
                                   static_cast<float> (nfa), static_cast<float> (nfb),
                                   syncmin, static_cast<float> (nfqso), kMaxCandidates,
                                   ipass, qBound (1, candidateThin, 100),
                                   candidates.data (), &ncand, sbase.data ());

        out << "pass " << ipass << ": syncmin=" << syncmin
            << " imetric=" << imetric
            << " lsubtract=" << lsubtract
            << " ncand=" << ncand << '\n';
        int printed = 0;
        for (int i = 0; i < ncand; ++i)
          {
            if (dumpMaxCandidates > 0 && printed >= dumpMaxCandidates)
              {
                break;
              }
            float const freq = candidates[static_cast<size_t> (i * 4 + 0)];
            if (std::fabs (freq - static_cast<float> (nfqso)) > static_cast<float> (windowHz))
              {
                continue;
              }
            out << "  cand " << i
                << " f=" << freq
                << " dt=" << candidates[static_cast<size_t> (i * 4 + 1)]
                << " sync=" << candidates[static_cast<size_t> (i * 4 + 2)]
                << " cq=" << candidates[static_cast<size_t> (i * 4 + 3)]
                << '\n';
            if (probe)
              {
                std::array<std::complex<float>, 3200> cd0 {};
                int newdat = 1;
                float refinedFreq = freq;
                float refinedDt = candidates[static_cast<size_t> (i * 4 + 1)];
                ftx_ft8_downsample_c (passDd.data (), &newdat, refinedFreq,
                                      reinterpret_cast<fftwf_complex*> (cd0.data ()));
                int ibest = 0;
                float delfbest = 0.0f;
                ftx_ft8_a7_search_initial_c (cd0.data (), kNp2, kFs2, refinedDt,
                                             &ibest, &delfbest);
                refinedFreq += delfbest;
                int secondNewdat = 0;
                ftx_ft8_downsample_c (passDd.data (), &secondNewdat, refinedFreq,
                                      reinterpret_cast<fftwf_complex*> (cd0.data ()));
                float refinedSync = 0.0f;
                ftx_ft8_a7_refine_search_c (cd0.data (), kNp2, kFs2, ibest,
                                            &ibest, &refinedSync, &refinedDt);

                auto probe_metrics = [&] (char const* prefix, bool equalized) {
                  std::array<float, 8 * 79> s8 {};
                  std::array<float, 174> llra {};
                  std::array<float, 174> llrb {};
                  std::array<float, 174> llrc {};
                  std::array<float, 174> llrd {};
                  std::array<float, 174> llre {};
                  int nsync = 0;
                  if (equalized)
                    {
                      ftx_ft8_bitmetrics_deep_equalized_c (
                          cd0.data (), kNp2, ibest, imetric, kScale, s8.data (), &nsync,
                          llra.data (), llrb.data (), llrc.data (), llrd.data (), llre.data ());
                    }
                  else
                    {
                      ftx_ft8_bitmetrics_deep_c (
                          cd0.data (), kNp2, ibest, imetric, kScale, s8.data (), &nsync,
                          llra.data (), llrb.data (), llrc.data (), llrd.data (), llre.data ());
                    }

                  auto measure = [&] (char const* label, float const* llr) {
                    int nhard = -1;
                    float dmin = 0.0f;
                    ftx_ldpc174_91_metrics_c (probeCodeword.data (), llr, &nhard, &dmin);
                    out << "    " << prefix << ' ' << label
                        << " nhard=" << nhard
                        << " dmin=" << dmin << '\n';
                  };
                  out << "    " << prefix
                      << " refined_f=" << refinedFreq
                      << " refined_dt=" << refinedDt
                      << " sync=" << refinedSync
                      << " nsync=" << nsync
                      << " ibest=" << ibest << '\n';
                  measure ("llra", llra.data ());
                  measure ("llrb", llrb.data ());
                  measure ("llrc", llrc.data ());
                  measure ("llrd", llrd.data ());
                  measure ("llre", llre.data ());

                  auto unpack_message91 = [] (signed char const* message91) {
                    std::array<signed char, kBitsPerMessage> message77 {};
                    std::copy_n (message91, kBitsPerMessage, message77.begin ());
                    std::array<char, kDecodedChars> text {};
                    int quirky = 0;
                    if (legacy_pack77_unpack77bits_c (message77.data (), 1, text.data (),
                                                      &quirky) == 0)
                      {
                        return QByteArrayLiteral ("<unpack failed>");
                      }
                    return trim_fortran_field (text.data (), kDecodedChars);
                  };

                  int const passLimit = lft8apon != 0 ? 8 : 5;
                  for (int decodePass = 1; decodePass <= passLimit; ++decodePass)
                    {
                      std::array<float, 174> llrz {};
                      std::array<int, 174> apmaskInt {};
                      std::array<signed char, 174> apmask {};
                      int iaptype = 0;
                      if (ftx_ft8_prepare_decode_pass_c (
                              decodePass, 0, lapcqonly, ncontest, nfqso, nftx, refinedFreq,
                              napwid, apsym.data (), aph10.data (), llra.data (), llrb.data (),
                              llrc.data (), llrd.data (), llre.data (), llrz.data (),
                              apmaskInt.data (), &iaptype) == 0)
                        {
                          continue;
                        }
                      std::transform (apmaskInt.begin (), apmaskInt.end (), apmask.begin (),
                                      [] (int value) {
                                        return static_cast<signed char> (value != 0);
                                      });
                      int nhard = -1;
                      float dmin = 0.0f;
                      ftx_ldpc174_91_metrics_c (probeCodeword.data (), llrz.data (),
                                                &nhard, &dmin);
                      out << "    " << prefix << " pass" << decodePass
                          << " iap=" << iaptype
                          << " exp_nhard=" << nhard
                          << " exp_dmin=" << dmin << '\n';
                      if (probeMetricsOnly)
                        {
                          continue;
                        }

                      for (int order : {3, 4, 5})
                        {
                          if (order == 5 && !probeOsd5)
                            {
                              continue;
                            }
                          std::array<signed char, 91> osdMessage {};
                          std::array<signed char, 174> osdCw {};
                          int osdHard = -1;
                          float osdDmin = 0.0f;
                          ftx_osd174_91_c (llrz.data (), 91, apmask.data (), order,
                                           osdMessage.data (), osdCw.data (), &osdHard,
                                           &osdDmin);
                          int osdVsExpected = 0;
                          for (int bit = 0; bit < 174; ++bit)
                            {
                              if (osdCw[static_cast<size_t> (bit)]
                                  != probeCodeword[static_cast<size_t> (bit)])
                                {
                                  ++osdVsExpected;
                                }
                            }
                          out << "      osd order=" << order
                              << " nhard=" << osdHard
                              << " dmin=" << osdDmin
                              << " cw_delta=" << osdVsExpected
                              << " text=\"" << unpack_message91 (osdMessage.data ()) << "\"\n";
                        }

                      for (float scaleFactor : {0.5f, 1.0f, 1.5f, 2.0f, 3.0f})
                        {
                          std::array<float, 174> scaledLlr {};
                          for (int bit = 0; bit < 174; ++bit)
                            {
                              scaledLlr[static_cast<size_t> (bit)] =
                                  llrz[static_cast<size_t> (bit)] * scaleFactor;
                            }
                          std::array<signed char, 91> message91 {};
                          std::array<signed char, 174> decodedCw {};
                          int ntype = 0;
                          int decodeHard = -1;
                          float decodeDmin = 0.0f;
                          ftx_decode174_91_c (scaledLlr.data (), 91, 3, 4, apmask.data (),
                                              message91.data (), decodedCw.data (), &ntype,
                                              &decodeHard, &decodeDmin);
                          int decodedVsExpected = 0;
                          for (int bit = 0; bit < 174; ++bit)
                            {
                              if (decodedCw[static_cast<size_t> (bit)]
                                  != probeCodeword[static_cast<size_t> (bit)])
                                {
                                  ++decodedVsExpected;
                                }
                            }
                          out << "      decode scale=" << scaleFactor
                              << " ntype=" << ntype
                              << " nhard=" << decodeHard
                              << " dmin=" << decodeDmin
                              << " cw_delta=" << decodedVsExpected
                              << " text=\"" << unpack_message91 (message91.data ()) << "\"\n";
                        }
                    }
                };

                probe_metrics ("base", false);
                probe_metrics ("eq", true);
              }
            ++printed;
          }
        if (printed == 0)
          {
            out << "  (no candidates in window)\n";
          }
      }
  }

  QList<int> parse_stages (QString const& raw)
  {
    QList<int> stages;
    QSet<int> seen;
    for (QString part : raw.split (QLatin1Char {','}, Qt::SkipEmptyParts))
      {
        part = part.trimmed ();
        bool ok = false;
        int const stage = part.toInt (&ok);
        if (!ok || stage < 0 || stage > 4)
          {
            fail (QStringLiteral ("invalid FT8 stage \"%1\"; expected values in 0..4").arg (part));
          }
        if (!seen.contains (stage))
          {
            seen.insert (stage);
            stages.append (stage);
          }
      }
    if (stages.isEmpty ())
      {
        fail (QStringLiteral ("no FT8 stages selected"));
      }
    return stages;
  }

  int parse_int_option (QCommandLineParser const& parser, QCommandLineOption const& option,
                        QString const& name)
  {
    bool ok = false;
    int const value = parser.value (option).toInt (&ok);
    if (!ok)
      {
        fail (QStringLiteral ("invalid --%1 value").arg (name));
      }
    return value;
  }

  float parse_float_option (QCommandLineParser const& parser, QCommandLineOption const& option,
                            QString const& name)
  {
    bool ok = false;
    float const value = parser.value (option).toFloat (&ok);
    if (!ok)
      {
        fail (QStringLiteral ("invalid --%1 value").arg (name));
      }
    return value;
  }

  void seed_known_cq_entry (QString const& raw)
  {
    QString normalized = raw.simplified ().toUpper ();
    normalized.replace (QLatin1Char (','), QLatin1Char (' '));
    normalized.replace (QLatin1Char (':'), QLatin1Char (' '));
    QStringList const parts = normalized.split (QLatin1Char (' '), Qt::SkipEmptyParts);
    if (parts.size () < 3 || parts.size () > 5)
      {
        fail (QStringLiteral ("invalid --seed-known-cq value \"%1\"; expected CALL GRID FREQ [DT] [NUTC]")
                  .arg (raw));
      }

    bool ok_freq = false;
    float const freq = parts.at (2).toFloat (&ok_freq);
    if (!ok_freq || freq <= 0.0f)
      {
        fail (QStringLiteral ("invalid --seed-known-cq frequency in \"%1\"").arg (raw));
      }

    float dt = 0.0f;
    int nutc = -1;
    if (parts.size () >= 4)
      {
        bool ok_value = false;
        int const value = parts.at (3).toInt (&ok_value);
        if (ok_value && parts.size () == 4)
          {
            nutc = value;
          }
        else
          {
            dt = parts.at (3).toFloat (&ok_value);
            if (!ok_value)
              {
                fail (QStringLiteral ("invalid --seed-known-cq DT in \"%1\"").arg (raw));
              }
          }
      }
    if (parts.size () >= 5)
      {
        bool ok_time = false;
        nutc = parts.at (4).toInt (&ok_time);
        if (!ok_time)
          {
            fail (QStringLiteral ("invalid --seed-known-cq NUTC in \"%1\"").arg (raw));
          }
      }

    QByteArray const call = to_fortran_field (parts.at (0).toLatin1 (), 12);
    QByteArray const grid = to_fortran_field (parts.at (1).toLatin1 (), 4);
    ftx_ft8_stage4_seed_known_cq_c (call.constData (), grid.constData (), freq, dt, nutc);
  }

  void seed_known_cq_call_entry (QString const& raw)
  {
    QString normalized = raw.simplified ().toUpper ();
    normalized.replace (QLatin1Char (','), QLatin1Char (' '));
    normalized.replace (QLatin1Char (':'), QLatin1Char (' '));
    QStringList const parts = normalized.split (QLatin1Char (' '), Qt::SkipEmptyParts);
    if (parts.size () < 2 || parts.size () > 4)
      {
        fail (QStringLiteral ("invalid --seed-known-cq-call value \"%1\"; expected CALL FREQ [DT] [NUTC]")
                  .arg (raw));
      }

    bool ok_freq = false;
    float const freq = parts.at (1).toFloat (&ok_freq);
    if (!ok_freq || freq <= 0.0f)
      {
        fail (QStringLiteral ("invalid --seed-known-cq-call frequency in \"%1\"").arg (raw));
      }

    float dt = 0.0f;
    int nutc = -1;
    if (parts.size () >= 3)
      {
        bool ok_value = false;
        int const value = parts.at (2).toInt (&ok_value);
        if (ok_value && parts.size () == 3)
          {
            nutc = value;
          }
        else
          {
            dt = parts.at (2).toFloat (&ok_value);
            if (!ok_value)
              {
                fail (QStringLiteral ("invalid --seed-known-cq-call DT in \"%1\"").arg (raw));
              }
          }
      }
    if (parts.size () >= 4)
      {
        bool ok_time = false;
        nutc = parts.at (3).toInt (&ok_time);
        if (!ok_time)
          {
            fail (QStringLiteral ("invalid --seed-known-cq-call NUTC in \"%1\"").arg (raw));
          }
      }

    QByteArray const call = to_fortran_field (parts.at (0).toLatin1 (), 12);
    ftx_ft8_stage4_seed_known_cq_call_c (call.constData (), freq, dt, nutc);
  }

  void seed_hash_call_entry (QString const& raw)
  {
    QString call = raw.simplified ().toUpper ();
    if (call.startsWith (QLatin1Char ('<')) && call.endsWith (QLatin1Char ('>')) && call.size () > 2)
      {
        call = call.mid (1, call.size () - 2);
      }
    if (call.isEmpty () || call.contains (QLatin1Char (' ')))
      {
        fail (QStringLiteral ("invalid --seed-hash-call value \"%1\"; expected one CALL")
                  .arg (raw));
      }

    QByteArray const call13 = to_fortran_field (call.toLatin1 (), 13);
    ftx_ft8_stage4_seed_hash_call_c (call13.constData ());
  }

  int sequence_index_for_utc (int nutc)
  {
    return std::abs ((nutc / 5) % 2);
  }

  void seed_a7_hint_entry (QString const& raw)
  {
    QStringList const parts = raw.split (QLatin1Char ('|'));
    if (parts.size () < 4 || parts.size () > 5)
      {
        fail (QStringLiteral ("invalid --seed-a7-hint value \"%1\"; expected MESSAGE|FREQ|DT|NUTC[|HITS]")
                  .arg (raw));
      }

    bool ok_freq = false;
    bool ok_dt = false;
    bool ok_nutc = false;
    float const freq = parts.at (1).trimmed ().toFloat (&ok_freq);
    float const dt = parts.at (2).trimmed ().toFloat (&ok_dt);
    int const nutc = parts.at (3).trimmed ().toInt (&ok_nutc);
    if (!ok_freq || freq <= 0.0f)
      {
        fail (QStringLiteral ("invalid --seed-a7-hint frequency in \"%1\"").arg (raw));
      }
    if (!ok_dt)
      {
        fail (QStringLiteral ("invalid --seed-a7-hint DT in \"%1\"").arg (raw));
      }
    if (!ok_nutc)
      {
        fail (QStringLiteral ("invalid --seed-a7-hint NUTC in \"%1\"").arg (raw));
      }

    int hits = 1;
    if (parts.size () == 5)
      {
        bool ok_hits = false;
        hits = parts.at (4).trimmed ().toInt (&ok_hits);
        if (!ok_hits || hits < 1)
          {
            fail (QStringLiteral ("invalid --seed-a7-hint HITS in \"%1\"").arg (raw));
          }
      }

    QByteArray const message = to_fortran_field (parts.at (0).trimmed ().toUpper ().toLatin1 (),
                                                 kDecodedChars);
    int const jseq = sequence_index_for_utc (nutc);
    for (int hit = 0; hit < hits; ++hit)
      {
        ftx_ft8_a7_save_c (jseq, dt, freq, message.constData ());
      }
  }

  WavData read_wav_file (QString const& file_name)
  {
    QFile file {file_name};
    if (!file.open (QIODevice::ReadOnly))
      {
        fail (QStringLiteral ("cannot open WAV file \"%1\": %2")
                  .arg (file_name, file.errorString ()));
      }

    QByteArray const blob = file.readAll ();
    if (blob.size () < 12)
      {
        fail (QStringLiteral ("WAV file \"%1\" is too short").arg (file_name));
      }
    if (blob.mid (0, 4) != "RIFF" || blob.mid (8, 4) != "WAVE")
      {
        fail (QStringLiteral ("WAV file \"%1\" is not RIFF/WAVE").arg (file_name));
      }

    bool have_fmt = false;
    bool have_data = false;
    quint16 audio_format = 0;
    quint16 channels = 0;
    quint32 sample_rate = 0;
    quint16 bits_per_sample = 0;
    QByteArray sample_bytes;

    int pos = 12;
    while (pos + 8 <= blob.size ())
      {
        QByteArray const chunk_id = blob.mid (pos, 4);
        quint32 const chunk_size = qFromLittleEndian<quint32> (
            reinterpret_cast<uchar const*> (blob.constData () + pos + 4));
        pos += 8;
        if (pos + static_cast<int> (chunk_size) > blob.size ())
          {
            fail (QStringLiteral ("WAV file \"%1\" has a truncated %2 chunk")
                      .arg (file_name, QString::fromLatin1 (chunk_id)));
          }

        if (chunk_id == "fmt ")
          {
            if (chunk_size < 16)
              {
                fail (QStringLiteral ("WAV file \"%1\" has an invalid fmt chunk").arg (file_name));
              }
            uchar const* fmt = reinterpret_cast<uchar const*> (blob.constData () + pos);
            audio_format = qFromLittleEndian<quint16> (fmt);
            channels = qFromLittleEndian<quint16> (fmt + 2);
            sample_rate = qFromLittleEndian<quint32> (fmt + 4);
            bits_per_sample = qFromLittleEndian<quint16> (fmt + 14);
            have_fmt = true;
          }
        else if (chunk_id == "data")
          {
            sample_bytes = blob.mid (pos, static_cast<int> (chunk_size));
            have_data = true;
          }

        pos += static_cast<int> ((chunk_size + 1u) & ~1u);
      }

    if (!have_fmt || !have_data)
      {
        fail (QStringLiteral ("WAV file \"%1\" is missing fmt or data chunks").arg (file_name));
      }
    if (audio_format != 1u)
      {
        fail (QStringLiteral ("WAV file \"%1\" is not PCM (format=%2)")
                  .arg (file_name)
                  .arg (audio_format));
      }
    if (channels != 1u)
      {
        fail (QStringLiteral ("WAV file \"%1\" must be mono; got %2 channels")
                  .arg (file_name)
                  .arg (channels));
      }
    if (sample_rate != 12000u)
      {
        fail (QStringLiteral ("WAV file \"%1\" must be 12000 Hz; got %2 Hz")
                  .arg (file_name)
                  .arg (sample_rate));
      }
    if (bits_per_sample != 16u)
      {
        fail (QStringLiteral ("WAV file \"%1\" must be 16-bit PCM; got %2 bits")
                  .arg (file_name)
                  .arg (bits_per_sample));
      }

    int const sample_count = sample_bytes.size () / static_cast<int> (sizeof (qint16));
    WavData wav;
    wav.sample_rate = static_cast<int> (sample_rate);
    wav.channels = static_cast<int> (channels);
    wav.bits_per_sample = static_cast<int> (bits_per_sample);
    wav.source_samples = sample_count;
    wav.padded = sample_count < kFt8SampleCount;
    wav.truncated = sample_count > kFt8SampleCount;
    wav.samples.assign (static_cast<size_t> (kFt8SampleCount), 0);

    int const copy_count = std::min (sample_count, kFt8SampleCount);
    uchar const* input = reinterpret_cast<uchar const*> (sample_bytes.constData ());
    for (int i = 0; i < copy_count; ++i)
      {
        wav.samples[static_cast<size_t> (i)] =
            qFromLittleEndian<qint16> (input + i * static_cast<int> (sizeof (qint16)));
      }
    return wav;
  }

  DecodeResult run_decode (std::vector<short> const& samples, int stage, int nqsoprogress,
                           int nfqso, int nftx, int nutc, int nfa, int nfb, int nzhsym,
                           int ndepth, float emedelay, int ncontest, int nagain,
                           int lft8apon, int lapcqonly, int napwid, int ldiskdat,
                           int supplemental, int maxosd, int norder,
                           int maxIter, int maxDecodeMs,
                           int lowThresholds, int subpass, int cycles,
                           int rxFreqSensitivity, int candidateThin,
                           bool skipEarlyPasses,
                           QByteArray const& mycall, QByteArray const& hiscall,
                           QByteArray const& hisgrid,
                           bool resetStage4Before = true,
                           bool resetStage4After = true,
                           int superfoxEnabled = 0,
                           int sfTolHz = 50)
  {
    std::vector<short> iwave = samples;
    std::array<int, kFt8MaxLines> snrs {};
    std::array<float, kFt8MaxLines> syncs {};
    std::array<float, kFt8MaxLines> dts {};
    std::array<float, kFt8MaxLines> freqs {};
    std::array<int, kFt8MaxLines> naps {};
    std::array<float, kFt8MaxLines> quals {};
    std::array<signed char, kBitsPerMessage * kFt8MaxLines> bits77 {};
    std::array<char, kFt8MaxLines * kDecodedChars> decodeds {};
    int nout = 0;
    QByteArray mycall_field = to_fortran_field (mycall, 12);
    QByteArray hiscall_field = to_fortran_field (hiscall, 12);
    QByteArray hisgrid_field = to_fortran_field (hisgrid, 6);
    DecodeResult result;
    result.stage = stage;
    result.lines.reserve (static_cast<size_t> (kFt8MaxLines));
    QSet<QString> seen_keys;

    auto collect_step_lines = [&] (int step_nout) {
      for (int i = 0; i < std::max (0, step_nout) && i < kFt8MaxLines; ++i)
        {
          DecodeLine line;
          line.snr = snrs[i];
          line.dt = dts[i];
          line.freq = freqs[i];
          line.nap = naps[i];
          line.qual = quals[i];
          line.decoded = QString::fromLatin1 (
              trim_fortran_field (decodeds.data () + i * kDecodedChars, kDecodedChars));
          QString const key = decode_key (line);
          if (seen_keys.contains (key))
            {
              continue;
            }
          seen_keys.insert (key);
          result.lines.push_back (line);
        }
    };

    auto invoke_decode = [&] (int nzhsym_step) {
      ftx_ft8_stage4_set_deadline_ms_c (
          maxDecodeMs > 0 ? steady_clock_ms () + maxDecodeMs : 0);
      ftx_ft8_stage4_set_superfox_options_c (superfoxEnabled, sfTolHz);

      int step_nqsoprogress = nqsoprogress;
      int step_nfqso = nfqso;
      int step_nftx = nftx;
      int step_nutc = nutc;
      int step_nfa = nfa;
      int step_nfb = nfb;
      int step_nzhsym = nzhsym_step;
      int step_ndepth = ndepth;
      float step_emedelay = emedelay;
      int step_ncontest = ncontest;
      int step_nagain = nagain;
      int step_lft8apon = lft8apon;
      int step_ltry_a8 = nzhsym_step == 41 ? 1 : 0;
      int step_lapcqonly = lapcqonly;
      int step_napwid = napwid;
      int step_ldiskdat = ldiskdat;
      int step_nout = 0;

      ftx_ft8_async_decode_stage4_c (iwave.data (), &step_nqsoprogress, &step_nfqso,
                                     &step_nftx, &step_nutc, &step_nfa, &step_nfb,
                                     &step_nzhsym, &step_ndepth, &step_emedelay,
                                     &step_ncontest, &step_nagain, &step_lft8apon,
                                     &step_ltry_a8, &step_lapcqonly, &step_napwid,
                                     mycall_field.constData (),
                                     hiscall_field.constData (), hisgrid_field.constData (),
                                     &step_ldiskdat, syncs.data (), snrs.data (), dts.data (),
                                     freqs.data (), naps.data (), quals.data (),
                                     bits77.data (), decodeds.data (), &step_nout);
      collect_step_lines (step_nout);
      return step_nout;
    };

    QMutexLocker locker {&decodium::fortran::runtime_mutex ()};
    if (resetStage4Before)
      {
        ftx_ft8_stage4_reset_c ();
      }
    ftx_ft8_stage4_apply_hash_seed_cache_c ();
    ftx_ft8_stage4_set_decode_options_c (lowThresholds, subpass, cycles,
                                         rxFreqSensitivity, candidateThin);
    ftx_ft8_stage4_set_supplemental_c (supplemental);
    ftx_ft8_stage4_set_ldpc_osd_c (maxosd, norder);
    ftx_ft8_stage4_set_ldpc_max_iter_c (maxIter);
    if (nzhsym >= 50 && !skipEarlyPasses)
      {
        invoke_decode (41);
        invoke_decode (47);
      }
    nout = invoke_decode (nzhsym);
    ftx_ft8_stage4_set_supplemental_c (0);
    ftx_ft8_stage4_set_ldpc_osd_c (-1, 0);
    ftx_ft8_stage4_set_ldpc_max_iter_c (30);
    ftx_ft8_stage4_set_decode_options_c (0, 0, 1, 1, 100);
    ftx_ft8_stage4_set_deadline_ms_c (0);
    if (resetStage4After)
      {
        ftx_ft8_stage4_reset_c ();
      }
    locker.unlock ();

    if (result.lines.empty () && nout > 0)
      {
        collect_step_lines (nout);
      }
    return result;
  }

  void apply_pre_subtractions (std::vector<short>& samples, QStringList const& entries,
                               QTextStream& out)
  {
    if (entries.isEmpty ())
      {
        return;
      }

    std::array<float, kFt8SampleCount> dd {};
    for (size_t i = 0; i < dd.size () && i < samples.size (); ++i)
      {
        dd[i] = static_cast<float> (samples[i]);
      }

    for (QString const& raw : entries)
      {
        QStringList const parts = raw.split (QLatin1Char ('|'));
        if (parts.size () < 3 || parts.size () > 4)
          {
            fail (QStringLiteral ("invalid --pre-subtract value \"%1\"; expected MESSAGE|FREQ|DT[|REFINE]")
                      .arg (raw));
          }

        bool ok_freq = false;
        bool ok_dt = false;
        float const freq = parts.at (1).trimmed ().toFloat (&ok_freq);
        float const callback_dt = parts.at (2).trimmed ().toFloat (&ok_dt);
        if (!ok_freq || !ok_dt)
          {
            fail (QStringLiteral ("invalid --pre-subtract frequency/dt in \"%1\"").arg (raw));
          }

        int refine = 1;
        if (parts.size () == 4)
          {
            bool ok_refine = false;
            refine = parts.at (3).trimmed ().toInt (&ok_refine);
            if (!ok_refine)
              {
                fail (QStringLiteral ("invalid --pre-subtract refine in \"%1\"").arg (raw));
              }
          }

        QByteArray const message = to_fortran_field (parts.at (0).trimmed ().toLatin1 (),
                                                     kDecodedChars);
        std::array<char, kDecodedChars> msgsent {};
        std::array<int, 79> itone {};
        std::array<signed char, 174> codeword {};
        if (ftx_encode_ft8_candidate_c (message.constData (), msgsent.data (),
                                        itone.data (), codeword.data ()) == 0)
          {
            fail (QStringLiteral ("cannot encode --pre-subtract message \"%1\"")
                      .arg (parts.at (0).trimmed ()));
          }

        ftx_subtract_ft8_c (dd.data (), itone.data (), freq, callback_dt + 0.5f, refine);
        out << "pre-subtracted: " << trim_fortran_field (msgsent.data (), kDecodedChars)
            << " freq=" << freq
            << " dt=" << callback_dt
            << " refine=" << refine << '\n';
      }

    for (size_t i = 0; i < samples.size () && i < dd.size (); ++i)
      {
        long const rounded = std::lround (dd[i]);
        samples[i] = static_cast<short> (std::clamp<long> (rounded, -32768L, 32767L));
      }
  }

  QHash<QString, QString> line_map (DecodeResult const& result)
  {
    QHash<QString, QString> map;
    for (DecodeLine const& line : result.lines)
      {
        QString const key = decode_key (line);
        if (!map.contains (key))
          {
            map.insert (key, describe_line (line));
          }
      }
    return map;
  }

  QHash<QString, QString> message_map (DecodeResult const& result)
  {
    QHash<QString, QString> map;
    for (DecodeLine const& line : result.lines)
      {
        QString const key = message_key (line);
        if (!map.contains (key))
          {
            map.insert (key, describe_line (line));
          }
      }
    return map;
  }

  void print_result (QTextStream& out, DecodeResult const& result)
  {
    out << "stage " << result.stage << ": nout=" << result.lines.size () << '\n';
    if (result.lines.empty ())
      {
        out << "  (no decodes)\n";
        return;
      }
    for (DecodeLine const& line : result.lines)
      {
        out << "  - " << describe_line (line) << '\n';
      }
  }

  void print_comparison (QTextStream& out, DecodeResult const& lhs, DecodeResult const& rhs)
  {
    QHash<QString, QString> const left_map = line_map (lhs);
    QHash<QString, QString> const right_map = line_map (rhs);
    QSet<QString> const left_keys = QSet<QString> (left_map.keyBegin (), left_map.keyEnd ());
    QSet<QString> const right_keys = QSet<QString> (right_map.keyBegin (), right_map.keyEnd ());
    QSet<QString> const common = left_keys & right_keys;
    QSet<QString> const only_left = left_keys - right_keys;
    QSet<QString> const only_right = right_keys - left_keys;
    QStringList only_left_sorted = QStringList (only_left.values ());
    QStringList only_right_sorted = QStringList (only_right.values ());
    std::sort (only_left_sorted.begin (), only_left_sorted.end ());
    std::sort (only_right_sorted.begin (), only_right_sorted.end ());

    out << "stage " << lhs.stage << " vs stage " << rhs.stage
        << ": common=" << common.size ()
        << " only_" << lhs.stage << '=' << only_left.size ()
        << " only_" << rhs.stage << '=' << only_right.size ()
        << '\n';

    for (QString const& key : only_left_sorted)
      {
        out << "  missing in stage " << rhs.stage << ": " << left_map.value (key) << '\n';
      }
    for (QString const& key : only_right_sorted)
      {
        out << "  extra in stage " << rhs.stage << ": " << right_map.value (key) << '\n';
      }

    QHash<QString, QString> const left_message_map = message_map (lhs);
    QHash<QString, QString> const right_message_map = message_map (rhs);
    QSet<QString> const left_message_keys = QSet<QString> (left_message_map.keyBegin (),
                                                           left_message_map.keyEnd ());
    QSet<QString> const right_message_keys = QSet<QString> (right_message_map.keyBegin (),
                                                            right_message_map.keyEnd ());
    QSet<QString> const common_messages = left_message_keys & right_message_keys;
    QSet<QString> const only_left_messages = left_message_keys - right_message_keys;
    QSet<QString> const only_right_messages = right_message_keys - left_message_keys;
    QStringList only_left_messages_sorted = QStringList (only_left_messages.values ());
    QStringList only_right_messages_sorted = QStringList (only_right_messages.values ());
    std::sort (only_left_messages_sorted.begin (), only_left_messages_sorted.end ());
    std::sort (only_right_messages_sorted.begin (), only_right_messages_sorted.end ());

    out << "message parity stage " << lhs.stage << " vs stage " << rhs.stage
        << ": common=" << common_messages.size ()
        << " only_" << lhs.stage << '=' << only_left_messages.size ()
        << " only_" << rhs.stage << '=' << only_right_messages.size ()
        << '\n';

    for (QString const& key : only_left_messages_sorted)
      {
        out << "  message missing in stage " << rhs.stage << ": "
            << left_message_map.value (key) << '\n';
      }
    for (QString const& key : only_right_messages_sorted)
      {
        out << "  message extra in stage " << rhs.stage << ": "
            << right_message_map.value (key) << '\n';
      }
  }
}

int main (int argc, char * argv[])
{
  try
    {
      QCoreApplication app {argc, argv};

      // Innesco di prova per l'a priori storico. Il banco processa un file per
      // invocazione, quindi l'elenco delle stazioni sentite parte vuoto e il
      // tipo 7 non avrebbe niente da provare: la misura direbbe "nessuna
      // differenza" e sarebbe una misura sbagliata, non un risultato negativo.
      //
      // Sta QUI e non accanto alla preparazione dell'a priori del banco:
      // quella e' in un ramo che per questa invocazione non viene mai eseguito
      // -- verificato con una stampa incondizionata che non compariva mai.
      // L'a priori vero lo prepara il decodificatore al suo interno.
      // Innesco per il tipo 8: il MESSAGGIO INTERO che si suppone sentito
      // due slot fa a quella frequenza. Come per il tipo 7, il banco processa
      // un file per invocazione e senza innesco l'elenco sarebbe vuoto: la
      // misura direbbe "nessuna differenza" per il motivo sbagliato.
      //
      // I bit si ricavano codificando il messaggio, cioe' esattamente come
      // farebbe una decodifica riuscita allo slot precedente.
      if (char const* seed = std::getenv ("DECODIUM_AP_MSG_SEED"))
        {
          QByteArray const raw {seed};
          int const sep = raw.lastIndexOf (':');
          if (sep > 0)
            {
              QByteArray const testo = raw.left (sep);
              float const hz = raw.mid (sep + 1).toFloat ();
              QByteArray const campo = to_fortran_field (testo, kDecodedChars);
              std::array<char, kDecodedChars> inviato {};
              std::array<int, 79> toni {};
              std::array<signed char, 174> parola {};
              if (ftx_encode_ft8_candidate_c (campo.constData (), inviato.data (),
                                              toni.data (), parola.data ()) != 0)
                {
                  // i primi 77 bit della parola di codice sono il messaggio
                  decodium::apstorico::registra_messaggio (hz, parola.data ());
                }
            }
        }

      if (char const* seed = std::getenv ("DECODIUM_AP_STORICO_SEED"))
        {
          QByteArray const raw {seed};
          int const sep = raw.indexOf (':');
          if (sep > 0)
            {
              QByteArray const call = raw.left (sep);
              decodium::apstorico::registra (
                  decodium::apstorico::ciclo_corrente (),
                  raw.mid (sep + 1).toFloat (), call.constData ());
            }
        }
      QCoreApplication::setApplicationName (QStringLiteral ("ft8_stage_compare"));
      QCoreApplication::setApplicationVersion (QStringLiteral ("1.0"));

      QCommandLineParser parser;
      parser.setApplicationDescription (
          QStringLiteral ("Compare FT8 decode outputs across DSP rollout stages on the same WAV file."));
      parser.addHelpOption ();
      parser.addVersionOption ();

      QCommandLineOption const stages_option {
          QStringList {QStringLiteral ("s"), QStringLiteral ("stages")},
          QStringLiteral ("Comma-separated FT8 stages to compare."),
          QStringLiteral ("stages"),
          QStringLiteral ("0,3")
      };
      QCommandLineOption const nqsoprogress_option {
          QStringLiteral ("nqsoprogress"),
          QStringLiteral ("QSO progress hint."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const nfqso_option {
          QStringLiteral ("nfqso"),
          QStringLiteral ("Expected QSO audio frequency in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("1000")
      };
      QCommandLineOption const nftx_option {
          QStringLiteral ("nftx"),
          QStringLiteral ("Transmit audio frequency hint in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("0")
      };
      QCommandLineOption const nutc_option {
          QStringLiteral ("nutc"),
          QStringLiteral ("UTC time hint as HHMM or HHMMSS integer."),
          QStringLiteral ("time"),
          QStringLiteral ("0")
      };
      QCommandLineOption const nfa_option {
          QStringLiteral ("nfa"),
          QStringLiteral ("Low decode limit in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("200")
      };
      QCommandLineOption const nfb_option {
          QStringLiteral ("nfb"),
          QStringLiteral ("High decode limit in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("4000")
      };
      QCommandLineOption const nzhsym_option {
          QStringLiteral ("nzhsym"),
          QStringLiteral ("FT8 half-symbol count (41..50)."),
          QStringLiteral ("count"),
          QStringLiteral ("50")
      };
      QCommandLineOption const depth_option {
          QStringLiteral ("depth"),
          QStringLiteral ("FT8 decode depth."),
          QStringLiteral ("depth"),
          QStringLiteral ("3")
      };
      QCommandLineOption const emedelay_option {
          QStringLiteral ("emedelay"),
          QStringLiteral ("EME delay hint."),
          QStringLiteral ("seconds"),
          QStringLiteral ("0")
      };
      QCommandLineOption const ncontest_option {
          QStringLiteral ("ncontest"),
          QStringLiteral ("Contest mode hint."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const superfox_option {
          QStringLiteral ("superfox"),
          QStringLiteral ("Enable SuperFox QPC decode on even sequences (0/1)."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const sf_tol_option {
          QStringLiteral ("sf-tol"),
          QStringLiteral ("SuperFox frequency search tolerance in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("50")
      };
      QCommandLineOption const nagain_option {
          QStringLiteral ("nagain"),
          QStringLiteral ("Again flag (0/1)."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const lft8apon_option {
          QStringLiteral ("lft8apon"),
          QStringLiteral ("FT8 AP enabled flag (0/1)."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const lapcqonly_option {
          QStringLiteral ("lapcqonly"),
          QStringLiteral ("CQ-only AP flag (0/1)."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const napwid_option {
          QStringLiteral ("napwid"),
          QStringLiteral ("AP frequency window in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("50")
      };
      QCommandLineOption const ldiskdat_option {
          QStringLiteral ("ldiskdat"),
          QStringLiteral ("Disk-data flag (0/1)."),
          QStringLiteral ("value"),
          QStringLiteral ("1")
      };
      QCommandLineOption const supplemental_option {
          QStringLiteral ("supplemental"),
          QStringLiteral ("Enable FT8 supplemental candidate search flag (0/1)."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const maxosd_option {
          QStringLiteral ("maxosd"),
          QStringLiteral ("LDPC max OSD override (-1 for decoder default)."),
          QStringLiteral ("value"),
          QStringLiteral ("-1")
      };
      QCommandLineOption const norder_option {
          QStringLiteral ("norder"),
          QStringLiteral ("LDPC OSD order override (0 for decoder default)."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const max_iter_option {
          QStringLiteral ("max-iter"),
          QStringLiteral ("LDPC BP max iterations."),
          QStringLiteral ("value"),
          QStringLiteral ("30")
      };
      QCommandLineOption const max_ms_option {
          QStringLiteral ("max-ms"),
          QStringLiteral ("Stage4 decode deadline in milliseconds; 0 disables the deadline."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const low_thresholds_option {
          QStringLiteral ("low-thresholds"),
          QStringLiteral ("Enable FT8 low-threshold candidate/decode gates (0/1)."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const subpass_option {
          QStringLiteral ("subpass"),
          QStringLiteral ("Enable FT8 low-threshold subpass mode (0/1)."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const cycles_option {
          QStringLiteral ("cycles"),
          QStringLiteral ("FT8 decode cycles (1..3)."),
          QStringLiteral ("value"),
          QStringLiteral ("1")
      };
      QCommandLineOption const rxfsens_option {
          QStringLiteral ("rxfsens"),
          QStringLiteral ("FT8 RX-frequency sensitivity (1..3)."),
          QStringLiteral ("value"),
          QStringLiteral ("1")
      };
      QCommandLineOption const candthin_option {
          QStringLiteral ("candthin"),
          QStringLiteral ("FT8 candidate thinning percentage (1..100)."),
          QStringLiteral ("value"),
          QStringLiteral ("100")
      };
      QCommandLineOption const no_early_option {
          QStringLiteral ("no-early"),
          QStringLiteral ("Skip synthetic 41/47 early passes before the requested stage.")
      };
      QCommandLineOption const dump_candidates_option {
          QStringLiteral ("dump-candidates"),
          QStringLiteral ("Print sync candidates before decode.")
      };
      QCommandLineOption const dump_only_option {
          QStringLiteral ("dump-only"),
          QStringLiteral ("Exit after candidate dump/probe.")
      };
      QCommandLineOption const dump_center_option {
          QStringLiteral ("dump-center"),
          QStringLiteral ("Candidate dump center in Hz; defaults to --nfqso."),
          QStringLiteral ("hz"),
          QString {}
      };
      QCommandLineOption const dump_window_option {
          QStringLiteral ("dump-window"),
          QStringLiteral ("Candidate dump half-window in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("30")
      };
      QCommandLineOption const dump_pass_option {
          QStringLiteral ("dump-pass"),
          QStringLiteral ("Only dump/probe one sync pass."),
          QStringLiteral ("pass"),
          QStringLiteral ("0")
      };
      QCommandLineOption const dump_max_candidates_option {
          QStringLiteral ("dump-max-candidates"),
          QStringLiteral ("Maximum matching candidates to dump/probe per pass; 0 means unlimited."),
          QStringLiteral ("count"),
          QStringLiteral ("0")
      };
      QCommandLineOption const probe_message_option {
          QStringLiteral ("probe-message"),
          QStringLiteral ("Measure candidate LLRs against an expected FT8 message."),
          QStringLiteral ("message"),
          QString {}
      };
      QCommandLineOption const probe_osd5_option {
          QStringLiteral ("probe-osd5"),
          QStringLiteral ("Include slow OSD order 5 in --probe-message diagnostics.")
      };
      QCommandLineOption const probe_metrics_only_option {
          QStringLiteral ("probe-metrics-only"),
          QStringLiteral ("Skip OSD/BP decode attempts in --probe-message diagnostics.")
      };
      QCommandLineOption const mycall_option {
          QStringLiteral ("mycall"),
          QStringLiteral ("Optional mycall override."),
          QStringLiteral ("call"),
          QString {}
      };
      QCommandLineOption const hiscall_option {
          QStringLiteral ("hiscall"),
          QStringLiteral ("Optional hiscall override."),
          QStringLiteral ("call"),
          QString {}
      };
      QCommandLineOption const hisgrid_option {
          QStringLiteral ("hisgrid"),
          QStringLiteral ("Optional hisgrid override."),
          QStringLiteral ("grid"),
          QString {}
      };
      QCommandLineOption const encode_message_option {
          QStringLiteral ("encode-message"),
          QStringLiteral ("Print FT8 packed bits and tones for a message, then exit."),
          QStringLiteral ("message"),
          QString {}
      };
      QCommandLineOption const seed_known_cq_option {
          QStringLiteral ("seed-known-cq"),
          QStringLiteral ("Seed the known CQ AP cache as: CALL GRID FREQ [NUTC]."),
          QStringLiteral ("entry"),
          QString {}
      };
      QCommandLineOption const seed_known_cq_call_option {
          QStringLiteral ("seed-known-cq-call"),
          QStringLiteral ("Seed the known CQ call-only cache as: CALL FREQ [DT] [NUTC]."),
          QStringLiteral ("entry"),
          QString {}
      };
      QCommandLineOption const seed_hash_call_option {
          QStringLiteral ("seed-hash-call"),
          QStringLiteral ("Seed the pack77 hash call context with one callsign."),
          QStringLiteral ("call"),
          QString {}
      };
      QCommandLineOption const seed_a7_hint_option {
          QStringLiteral ("seed-a7-hint"),
          QStringLiteral ("Seed the A7 repeated-hint cache as MESSAGE|FREQ|DT|NUTC[|HITS]."),
          QStringLiteral ("entry"),
          QString {}
      };
      QCommandLineOption const pre_subtract_option {
          QStringLiteral ("pre-subtract"),
          QStringLiteral ("Subtract a known FT8 signal before decode as MESSAGE|FREQ|DT[|REFINE]."),
          QStringLiteral ("entry"),
          QString {}
      };

      parser.addOption (stages_option);
      parser.addOption (nqsoprogress_option);
      parser.addOption (nfqso_option);
      parser.addOption (nftx_option);
      parser.addOption (nutc_option);
      parser.addOption (nfa_option);
      parser.addOption (nfb_option);
      parser.addOption (nzhsym_option);
      parser.addOption (depth_option);
      parser.addOption (emedelay_option);
      parser.addOption (ncontest_option);
      parser.addOption (superfox_option);
      parser.addOption (sf_tol_option);
      parser.addOption (nagain_option);
      parser.addOption (lft8apon_option);
      parser.addOption (lapcqonly_option);
      parser.addOption (napwid_option);
      parser.addOption (ldiskdat_option);
      parser.addOption (supplemental_option);
      parser.addOption (maxosd_option);
      parser.addOption (norder_option);
      parser.addOption (max_iter_option);
      parser.addOption (max_ms_option);
      parser.addOption (low_thresholds_option);
      parser.addOption (subpass_option);
      parser.addOption (cycles_option);
      parser.addOption (rxfsens_option);
      parser.addOption (candthin_option);
      parser.addOption (no_early_option);
      parser.addOption (dump_candidates_option);
      parser.addOption (dump_only_option);
      parser.addOption (dump_center_option);
      parser.addOption (dump_window_option);
      parser.addOption (dump_pass_option);
      parser.addOption (dump_max_candidates_option);
      parser.addOption (probe_message_option);
      parser.addOption (probe_osd5_option);
      parser.addOption (probe_metrics_only_option);
      parser.addOption (mycall_option);
      parser.addOption (hiscall_option);
      parser.addOption (hisgrid_option);
      parser.addOption (encode_message_option);
      parser.addOption (seed_known_cq_option);
      parser.addOption (seed_known_cq_call_option);
      parser.addOption (seed_hash_call_option);
      parser.addOption (seed_a7_hint_option);
      parser.addOption (pre_subtract_option);
      parser.addPositionalArgument (QStringLiteral ("wav-file"),
                                    QStringLiteral ("Path to a 12000 Hz mono 16-bit FT8 WAV file."),
                                    QStringLiteral ("wav-file [wav-file ...]"));
      parser.process (app);

      if (parser.isSet (encode_message_option))
        {
          QByteArray const message = to_fortran_field (parser.value (encode_message_option).toLatin1 (), 37);
          std::array<char, 37> msgsent {};
          std::array<int, 79> itone {};
          std::array<signed char, 174> codeword {};
          QTextStream out {stdout};
          if (ftx_encode_ft8_candidate_c (message.constData (), msgsent.data (),
                                          itone.data (), codeword.data ()) == 0)
            {
              out << "encode failed\n";
              return 1;
            }
          out << "msgsent: " << trim_fortran_field (msgsent.data (), 37) << '\n';
          out << "bits77: ";
          for (int i = 0; i < 77; ++i)
            {
              out << int (codeword[static_cast<size_t> (i)]);
            }
          out << '\n';
          out << "itone: ";
          for (int i = 0; i < 79; ++i)
            {
              if (i != 0) out << ',';
              out << itone[static_cast<size_t> (i)];
            }
          out << '\n';
          return 0;
        }

      QStringList const positional = parser.positionalArguments ();
      if (positional.isEmpty ())
        {
          parser.showHelp (2);
        }

      QList<int> const stages = parse_stages (parser.value (stages_option));
      int const nqsoprogress = parse_int_option (parser, nqsoprogress_option, QStringLiteral ("nqsoprogress"));
      int const nfqso = parse_int_option (parser, nfqso_option, QStringLiteral ("nfqso"));
      int const nftx = parse_int_option (parser, nftx_option, QStringLiteral ("nftx"));
      int const nutc = parse_int_option (parser, nutc_option, QStringLiteral ("nutc"));
      int const nfa = parse_int_option (parser, nfa_option, QStringLiteral ("nfa"));
      int const nfb = parse_int_option (parser, nfb_option, QStringLiteral ("nfb"));
      int const nzhsym = parse_int_option (parser, nzhsym_option, QStringLiteral ("nzhsym"));
      int const depth = parse_int_option (parser, depth_option, QStringLiteral ("depth"));
      float const emedelay = parse_float_option (parser, emedelay_option, QStringLiteral ("emedelay"));
      int const ncontest = parse_int_option (parser, ncontest_option, QStringLiteral ("ncontest"));
      int const superfoxEnabled = parse_int_option (parser, superfox_option, QStringLiteral ("superfox"));
      int const sfTolHz = parse_int_option (parser, sf_tol_option, QStringLiteral ("sf-tol"));
      int const nagain = parse_int_option (parser, nagain_option, QStringLiteral ("nagain"));
      int const lft8apon = parse_int_option (parser, lft8apon_option, QStringLiteral ("lft8apon"));
      int const lapcqonly = parse_int_option (parser, lapcqonly_option, QStringLiteral ("lapcqonly"));
      int const napwid = parse_int_option (parser, napwid_option, QStringLiteral ("napwid"));
      int const ldiskdat = parse_int_option (parser, ldiskdat_option, QStringLiteral ("ldiskdat"));
      int const supplemental = parse_int_option (parser, supplemental_option, QStringLiteral ("supplemental"));
      int const maxosd = parse_int_option (parser, maxosd_option, QStringLiteral ("maxosd"));
      int const norder = parse_int_option (parser, norder_option, QStringLiteral ("norder"));
      int const maxIter = parse_int_option (parser, max_iter_option, QStringLiteral ("max-iter"));
      int const maxDecodeMs = parse_int_option (parser, max_ms_option, QStringLiteral ("max-ms"));
      int const lowThresholds = parse_int_option (parser, low_thresholds_option,
                                                  QStringLiteral ("low-thresholds"));
      int const subpass = parse_int_option (parser, subpass_option, QStringLiteral ("subpass"));
      int const cycles = parse_int_option (parser, cycles_option, QStringLiteral ("cycles"));
      int const rxFreqSensitivity = parse_int_option (parser, rxfsens_option,
                                                      QStringLiteral ("rxfsens"));
      int const candidateThin = parse_int_option (parser, candthin_option,
                                                  QStringLiteral ("candthin"));
      bool const skipEarlyPasses = parser.isSet (no_early_option);
      bool const dumpCandidates = parser.isSet (dump_candidates_option);
      bool const dumpOnly = parser.isSet (dump_only_option);
      int const dumpCenter = parser.isSet (dump_center_option)
          ? parse_int_option (parser, dump_center_option, QStringLiteral ("dump-center"))
          : nfqso;
      int const dumpWindowHz = parse_int_option (parser, dump_window_option,
                                                 QStringLiteral ("dump-window"));
      int const dumpPassFilter = parse_int_option (parser, dump_pass_option,
                                                   QStringLiteral ("dump-pass"));
      int const dumpMaxCandidates = parse_int_option (parser, dump_max_candidates_option,
                                                      QStringLiteral ("dump-max-candidates"));

      QTextStream out {stdout};
      QStringList const knownCqSeeds = parser.values (seed_known_cq_option);
      for (QString const& seed : knownCqSeeds)
        {
          seed_known_cq_entry (seed);
          out << "seeded known CQ: " << seed << '\n';
        }
      QStringList const knownCqCallSeeds = parser.values (seed_known_cq_call_option);
      for (QString const& seed : knownCqCallSeeds)
        {
          seed_known_cq_call_entry (seed);
          out << "seeded known CQ call: " << seed << '\n';
        }
      QStringList const hashSeeds = parser.values (seed_hash_call_option);
      for (QString const& seed : hashSeeds)
        {
          seed_hash_call_entry (seed);
          out << "seeded hash call: " << seed << '\n';
        }
      QStringList const a7Seeds = parser.values (seed_a7_hint_option);
      for (QString const& seed : a7Seeds)
        {
          seed_a7_hint_entry (seed);
          out << "seeded A7 hint: " << seed << '\n';
        }
      for (int fileIndex = 0; fileIndex < positional.size (); ++fileIndex)
        {
      QString const wav_path = QFileInfo {positional.at (fileIndex)}.absoluteFilePath ();
      int const effective_nutc = nutc != 0 ? nutc : infer_nutc_from_path (wav_path, nutc);
      ftx_ft8_stage4_apply_hash_seed_cache_c ();
      WavData wav = read_wav_file (wav_path);

      out << "file: " << wav_path << '\n';
      out << "samples: source=" << wav.source_samples
          << " compare=" << wav.samples.size ()
          << " sample_rate=" << wav.sample_rate
          << " channels=" << wav.channels
          << " bits=" << wav.bits_per_sample
          << '\n';
      if (wav.padded)
        {
          out << "note: source WAV is shorter than FT8 compare buffer; zero-padding tail samples\n";
        }
      if (wav.truncated)
        {
          out << "note: source WAV is longer than FT8 compare buffer; truncating to first "
              << kFt8SampleCount << " samples\n";
        }
      apply_pre_subtractions (wav.samples, parser.values (pre_subtract_option), out);
      out << "decoder params: nqsoprogress=" << nqsoprogress
          << " nfqso=" << nfqso
          << " nftx=" << nftx
          << " nutc=" << effective_nutc
          << " nfa=" << nfa
          << " nfb=" << nfb
          << " nzhsym=" << nzhsym
          << " depth=" << depth
          << " emedelay=" << emedelay
          << " ncontest=" << ncontest
          << " nagain=" << nagain
          << " lft8apon=" << lft8apon
          << " lapcqonly=" << lapcqonly
          << " napwid=" << napwid
          << " ldiskdat=" << ldiskdat
          << " supplemental=" << supplemental
          << " maxosd=" << maxosd
          << " norder=" << norder
          << " maxIter=" << maxIter
          << " maxMs=" << maxDecodeMs
          << " lowThresholds=" << lowThresholds
          << " subpass=" << subpass
          << " cycles=" << cycles
          << " rxfsens=" << rxFreqSensitivity
          << " candthin=" << candidateThin
          << " noEarly=" << (skipEarlyPasses ? 1 : 0)
          << '\n';

      if (dumpCandidates)
        {
          dump_candidates (out, wav.samples, nfa, nfb, dumpCenter, nftx, depth,
                           ncontest, lft8apon, lapcqonly, napwid,
                           lowThresholds, subpass, cycles, rxFreqSensitivity,
                           candidateThin, qMax (1, dumpWindowHz),
                           parser.value (probe_message_option),
                           parser.value (mycall_option).toLatin1 (),
                           parser.value (hiscall_option).toLatin1 (),
                           parser.isSet (probe_osd5_option),
                           parser.isSet (probe_metrics_only_option),
                           dumpPassFilter, dumpMaxCandidates);
          if (dumpOnly)
            {
              return EXIT_SUCCESS;
            }
        }

      std::vector<DecodeResult> results;
      results.reserve (static_cast<size_t> (stages.size ()));
      bool const sequenceState = positional.size () > 1 && stages.size () == 1;
      for (int stage : stages)
        {
          bool const resetBefore = !(sequenceState && fileIndex > 0)
              && !(fileIndex == 0 && !a7Seeds.isEmpty ());
          bool const resetAfter = !(sequenceState && fileIndex + 1 < positional.size ());
          results.push_back (run_decode (wav.samples, stage, nqsoprogress, nfqso, nftx, effective_nutc,
                                         nfa, nfb, nzhsym, depth, emedelay, ncontest,
                                         nagain, lft8apon, lapcqonly, napwid, ldiskdat,
                                         supplemental, maxosd, norder,
                                         maxIter, maxDecodeMs,
                                         lowThresholds, subpass, cycles,
                                         rxFreqSensitivity, candidateThin,
                                         skipEarlyPasses,
                                         parser.value (mycall_option).toLatin1 (),
                                         parser.value (hiscall_option).toLatin1 (),
                                         parser.value (hisgrid_option).toLatin1 (),
                                         resetBefore, resetAfter,
                                         superfoxEnabled, sfTolHz));
        }

      out << '\n';
      for (DecodeResult const& result : results)
        {
          print_result (out, result);
          out << '\n';
        }

      for (size_t i = 1; i < results.size (); ++i)
        {
          print_comparison (out, results.front (), results[i]);
        }

        }

      // Diagnostica della fase 1: quante stazioni sono finite nell'elenco
      // delle sentite di recente. Serve a verificare che il collegamento
      // funzioni, prima di costruirci sopra le ipotesi.
      if (std::getenv ("DECODIUM_AP_STORICO_DIAG"))
        {
          std::cout << "ap_storico: " << decodium::apstorico::quante ()
                    << " voci, ciclo " << decodium::apstorico::ciclo_corrente ()
                    << ", tipo7 " << ftx_ft8_ap_storico_tentativi_c ()
                    << ", tipo8 " << ftx_ft8_ap_msg_tentativi_c ()
                    << ", messaggi " << decodium::apstorico::quanti_messaggi ()
                    << std::endl;
        }

      return EXIT_SUCCESS;
    }
  catch (std::exception const& ex)
    {
      std::cerr << ex.what () << '\n';
      return EXIT_FAILURE;
    }
}
