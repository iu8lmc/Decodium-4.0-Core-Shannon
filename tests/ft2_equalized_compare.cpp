#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QtEndian>

extern "C"
{
  void ftx_getcandidates2_c (float const* dd, float fa, float fb, float syncmin, float nfqso,
                             int maxcand, float* savg, float* candidate, int* ncand,
                             float* sbase);
  void ftx_sync2d_c (std::complex<float> const* cd0, int np, int i0,
                     std::complex<float> const* ctwk, int itwk, float* sync);
  void ftx_ft2_downsample_c (float const* dd, int* newdata, float f0, std::complex<float>* c);
  void ftx_twkfreq1_c (std::complex<float> const* ca, int const* npts, float const* fsample,
                       float const* a, std::complex<float>* cb);
  void ftx_ft2_bitmetrics_c (std::complex<float> const* cd, float* bitmetrics, int* badsync);
  void ftx_ft2_channel_est_c (std::complex<float> const* cd, std::complex<float>* cd_eq,
                              float* ch_snr);
  void ftx_ft2_bitmetrics_diag_c (std::complex<float> const* cd,
                                  float* bitmetrics_final,
                                  float* bitmetrics_base,
                                  float* bmet_eq_raw,
                                  float* bmet_eq,
                                  std::complex<float>* cd_eq,
                                  float* ch_snr,
                                  int* badsync,
                                  int* use_cheq,
                                  float* snr_min,
                                  float* snr_max,
                                  float* snr_mean,
                                  float* fading_depth,
                                  float* noise_var,
                                  float* noise_var_eq);
}

namespace
{
  using Complex = std::complex<float>;

  constexpr int kFt2SampleCount {45000};
  constexpr int kFt2NDown {9};
  constexpr int kFt2Nsps {288};
  constexpr int kFt2Nss {kFt2Nsps / kFt2NDown};
  constexpr int kFt2Nn {103};
  constexpr int kFt2Rows {2 * kFt2Nn};
  constexpr int kFt2NSeg {kFt2Nn * kFt2Nss};
  constexpr int kFt2NdMax {kFt2SampleCount / kFt2NDown};
  constexpr int kFt2Nh1 {1152 / 2};
  constexpr int kFt2MaxCandDefault {300};
  constexpr float kFt2FsDown {12000.0f / static_cast<float> (kFt2NDown)};

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

  struct CompareResult
  {
    bool mismatch {};
    float max_diff_ref_diag_vs_plain {};
    int max_diff_ref_diag_vs_plain_index {-1};
    int badsync_ref {};
    int badsync_cpp {};
    bool use_cheq_ref {};
    bool use_cheq_cpp {};
    float snr_min_ref {};
    float snr_max_ref {};
    float snr_mean_ref {};
    float fading_depth_ref {};
    float snr_min_cpp {};
    float snr_max_cpp {};
    float snr_mean_cpp {};
    float fading_depth_cpp {};
    float noise_var_ref {};
    float noise_var_eq_ref {};
    float noise_var_cpp {};
    float noise_var_eq_cpp {};
    float max_diff_final {};
    float max_diff_base {};
    float max_diff_eq_raw {};
    float max_diff_eq {};
    float max_diff_ref_vs_base {};
    float max_diff_cpp_blend {};
    float max_abs_eq_metric {};
    float max_diff_ch_snr {};
    float max_diff_cd_eq {};
    float max_diff_eq_pwr {};
    float max_diff_eq_raw_direct {};
    int max_diff_final_index {-1};
    int max_diff_base_index {-1};
    int max_diff_eq_raw_index {-1};
    int max_diff_eq_index {-1};
    float eq_raw_ref_at_max {};
    float eq_raw_cpp_at_max {};
    float eq_ref_at_max {};
    float eq_cpp_at_max {};
    int max_diff_ref_vs_base_index {-1};
    int max_diff_cpp_blend_index {-1};
    int max_abs_eq_metric_index {-1};
    int max_diff_ch_snr_index {-1};
    int max_diff_cd_eq_index {-1};
    int max_diff_eq_pwr_index {-1};
    int max_diff_eq_raw_direct_index {-1};
    float final_ref_at_max {};
    float final_cpp_at_max {};
    float base_ref_at_max {};
    float base_cpp_at_max {};
  };

  struct MetricLocation
  {
    int row {-1};
    int col {-1};
  };

  [[noreturn]] void fail (QString const& message)
  {
    throw std::runtime_error {message.toStdString ()};
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

  MetricLocation decode_metric_index (int index, int rows)
  {
    MetricLocation loc;
    if (index < 0)
      {
        return loc;
      }
    loc.col = index / rows;
    loc.row = index % rows;
    return loc;
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
    wav.padded = sample_count < kFt2SampleCount;
    wav.truncated = sample_count > kFt2SampleCount;
    wav.samples.assign (static_cast<size_t> (kFt2SampleCount), 0);

    int const copy_count = std::min (sample_count, kFt2SampleCount);
    uchar const* input = reinterpret_cast<uchar const*> (sample_bytes.constData ());
    for (int i = 0; i < copy_count; ++i)
      {
        wav.samples[static_cast<size_t> (i)] =
            qFromLittleEndian<qint16> (input + i * static_cast<int> (sizeof (qint16)));
      }
    return wav;
  }

  void normalize_complex_buffer (Complex* data, int count)
  {
    float sum2 = 0.0f;
    for (int i = 0; i < count; ++i)
      {
        sum2 += std::norm (data[i]);
      }
    sum2 /= static_cast<float> (count);
    if (sum2 <= 0.0f)
      {
        return;
      }
    float const scale = 1.0f / std::sqrt (sum2);
    for (int i = 0; i < count; ++i)
      {
      data[i] *= scale;
    }
  }

  void compute_ft2_tone_powers_direct (Complex const* cd_eq, float* pwr_eq)
  {
    constexpr int kNss = kFt2Nss;
    constexpr float kTwoPi = 6.28318530717958647692f;
    for (int symbol = 0; symbol < kFt2Nn; ++symbol)
      {
        Complex bins[4] {};
        for (int n = 0; n < kNss; ++n)
          {
            Complex const sample = cd_eq[symbol * kNss + n];
            for (int bin = 0; bin < 4; ++bin)
              {
                float const angle = -kTwoPi * static_cast<float> (bin * n) / static_cast<float> (kNss);
                bins[bin] += sample * Complex {std::cos (angle), std::sin (angle)};
              }
          }
        for (int bin = 0; bin < 4; ++bin)
          {
            pwr_eq[symbol * 4 + bin] = std::norm (bins[bin]);
          }
      }
  }

  void compute_ft2_eq_raw_direct (float const* pwr_eq,
                                  float const* ch_snr,
                                  float* bmet_eq_raw)
  {
    constexpr int kRows = kFt2Rows;
    std::fill (bmet_eq_raw, bmet_eq_raw + kRows, 0.0f);

    static int const icos4a[4] = {0, 1, 3, 2};
    static int const icos4b[4] = {1, 0, 2, 3};
    static int const icos4c[4] = {2, 3, 1, 0};
    static int const icos4d[4] = {3, 2, 0, 1};
    static int const graymap[4] = {0, 1, 3, 2};

    int noise_sum_eq = 0;
    int nnoise_eq = 0;
    for (int k = 0; k < 4; ++k)
      {
        for (int itone = 0; itone < 4; ++itone)
          {
            if (itone != icos4a[k]) { noise_sum_eq += static_cast<int> (pwr_eq[(k + 0) * 4 + itone]); ++nnoise_eq; }
            if (itone != icos4b[k]) { noise_sum_eq += static_cast<int> (pwr_eq[(k + 33) * 4 + itone]); ++nnoise_eq; }
            if (itone != icos4c[k]) { noise_sum_eq += static_cast<int> (pwr_eq[(k + 66) * 4 + itone]); ++nnoise_eq; }
            if (itone != icos4d[k]) { noise_sum_eq += static_cast<int> (pwr_eq[(k + 99) * 4 + itone]); ++nnoise_eq; }
          }
      }
    int noise_var_eq = 0;
    if (nnoise_eq > 0)
      {
        noise_var_eq = static_cast<int> (static_cast<float> (noise_sum_eq)
                                         / std::max (1.0f, static_cast<float> (nnoise_eq)));
      }
    if (static_cast<float> (noise_var_eq) < 1.0e-10f)
      {
        noise_var_eq = static_cast<int> (1.0e-10f);
      }
    float beta_eq = std::numeric_limits<float>::infinity ();
    if (noise_var_eq != 0)
      {
        beta_eq = 0.5f / static_cast<float> (noise_var_eq);
      }
    beta_eq = std::max (0.01f, std::min (50.0f, beta_eq));

    float snr_min = ch_snr[0];
    float snr_max = ch_snr[0];
    float snr_mean = 0.0f;
    for (int i = 0; i < kFt2Nn; ++i)
      {
        snr_min = std::min (snr_min, ch_snr[i]);
        snr_max = std::max (snr_max, ch_snr[i]);
        snr_mean += ch_snr[i];
      }
    snr_mean /= static_cast<float> (kFt2Nn);

    for (int ks = 0; ks < kFt2Nn; ++ks)
      {
        float sp_eq[4] = {
          pwr_eq[ks * 4 + graymap[0]],
          pwr_eq[ks * 4 + graymap[1]],
          pwr_eq[ks * 4 + graymap[2]],
          pwr_eq[ks * 4 + graymap[3]],
        };

        float snr_weight = 1.0f;
        if (snr_mean > 1.0e-10f)
          {
            snr_weight = std::sqrt (ch_snr[ks] / snr_mean);
            snr_weight = std::max (0.1f, std::min (3.0f, snr_weight));
          }

        int const ipt = ks * 2;
        for (int ib = 0; ib < 2; ++ib)
          {
            float maxp1 = -1.0e30f;
            float maxp0 = -1.0e30f;
            for (int i = 0; i < 4; ++i)
              {
                bool const set = (ib == 0) ? (i >= 2) : ((i & 1) != 0);
                float const pval = beta_eq * sp_eq[i];
                if (set) maxp1 = std::max (maxp1, pval);
                else maxp0 = std::max (maxp0, pval);
              }
            float lse1 = 0.0f;
            float lse0 = 0.0f;
            for (int i = 0; i < 4; ++i)
              {
                bool const set = (ib == 0) ? (i >= 2) : ((i & 1) != 0);
                float const pval = beta_eq * sp_eq[i];
                if (set) lse1 += std::exp (pval - maxp1);
                else lse0 += std::exp (pval - maxp0);
              }
            bmet_eq_raw[ipt + ib] =
                ((maxp1 + std::log (std::max (lse1, 1.0e-30f))) -
                 (maxp0 + std::log (std::max (lse0, 1.0e-30f)))) * snr_weight;
          }
      }
  }

  void extract_ft2_window (std::array<Complex, kFt2NSeg>& cd,
                           std::array<Complex, kFt2NdMax> const& cb,
                           int istart)
  {
    std::fill (cd.begin (), cd.end (), Complex {});
    if (istart >= 0)
      {
        int const it = std::min (kFt2NdMax - 1, istart + kFt2NSeg - 1);
        int const np = it - istart + 1;
        if (np > 0)
          {
            std::copy_n (cb.begin () + istart, np, cd.begin ());
          }
      }
    else
      {
        int const start = -istart;
        int const count = kFt2NSeg + 2 * istart;
        if (start >= 0 && count > 0 && start + count <= kFt2NSeg)
          {
            std::copy_n (cb.begin (), count, cd.begin () + start);
          }
      }
  }

  int idf_index (int idf)
  {
    return idf + 16;
  }

  std::array<std::array<Complex, 2 * kFt2Nss>, 33> make_tweaks ()
  {
    std::array<std::array<Complex, 2 * kFt2Nss>, 33> tweaks {};
    std::array<Complex, 2 * kFt2Nss> ones {};
    std::fill (ones.begin (), ones.end (), Complex {1.0f, 0.0f});
    int const npts = static_cast<int> (ones.size ());
    float const fsample = kFt2FsDown / 2.0f;

    for (int idf = -16; idf <= 16; ++idf)
      {
        std::array<float, 5> a {};
        a[0] = static_cast<float> (idf);
        ftx_twkfreq1_c (ones.data (), &npts, &fsample, a.data (),
                        tweaks[static_cast<size_t> (idf_index (idf))].data ());
      }
    return tweaks;
  }

  float max_abs_diff (float const* left, float const* right, int count, int* index_out)
  {
    float max_diff = 0.0f;
    int max_index = -1;
    for (int i = 0; i < count; ++i)
      {
        float const diff = std::fabs (left[i] - right[i]);
        if (diff > max_diff)
          {
            max_diff = diff;
            max_index = i;
          }
      }
    if (index_out)
      {
        *index_out = max_index;
      }
    return max_diff;
  }

  float max_abs_diff (Complex const* left, Complex const* right, int count, int* index_out)
  {
    float max_diff = 0.0f;
    int max_index = -1;
    for (int i = 0; i < count; ++i)
      {
        float const diff = std::abs (left[i] - right[i]);
        if (diff > max_diff)
          {
            max_diff = diff;
            max_index = i;
          }
      }
    if (index_out)
      {
        *index_out = max_index;
      }
    return max_diff;
  }

  void compute_reference_stats (float const* ch_snr,
                                float* snr_min_out,
                                float* snr_max_out,
                                float* snr_mean_out,
                                float* fading_depth_out,
                                bool* use_cheq_out)
  {
    float snr_min = ch_snr[0];
    float snr_max = ch_snr[0];
    float snr_mean = 0.0f;
    for (int i = 0; i < kFt2Nn; ++i)
      {
        snr_min = std::min (snr_min, ch_snr[i]);
        snr_max = std::max (snr_max, ch_snr[i]);
        snr_mean += ch_snr[i];
      }
    snr_mean /= static_cast<float> (kFt2Nn);
    float const fading_depth =
        snr_min > 1.0e-10f ? 10.0f * std::log10 (snr_max / snr_min) : 30.0f;
    if (snr_min_out) *snr_min_out = snr_min;
    if (snr_max_out) *snr_max_out = snr_max;
    if (snr_mean_out) *snr_mean_out = snr_mean;
    if (fading_depth_out) *fading_depth_out = fading_depth;
    if (use_cheq_out) *use_cheq_out = fading_depth > 2.0f;
  }

  CompareResult compare_cd (std::array<Complex, kFt2NSeg> const& cd, float tolerance)
  {
    CompareResult result;

    std::array<float, kFt2Rows * 3> bitmetrics_plain {};
    std::array<float, kFt2Rows * 3> bitmetrics_cpp {};
    std::array<float, kFt2Rows * 3> bitmetrics_base_cpp {};
    std::array<float, kFt2Rows> bmet_eq_cpp {};
    std::array<float, kFt2Rows> bmet_eq_raw_cpp {};
    std::array<Complex, kFt2NSeg> cd_eq_direct {};
    std::array<Complex, kFt2NSeg> cd_eq_cpp {};
    std::array<float, kFt2Nn> ch_snr_direct {};
    std::array<float, kFt2Nn> ch_snr_cpp {};
    std::array<float, kFt2Nn * 4> pwr_eq_direct {};
    std::array<float, kFt2Rows> bmet_eq_raw_direct {};

    int badsync_plain = 0;
    int badsync_cpp = 0;
    int use_cheq_cpp = 0;
    float snr_min_cpp = 0.0f;
    float snr_max_cpp = 0.0f;
    float snr_mean_cpp = 0.0f;
    float fading_depth_cpp = 0.0f;
    float noise_var_cpp = 0.0f;
    float noise_var_eq_cpp = 0.0f;

    ftx_ft2_bitmetrics_c (cd.data (), bitmetrics_plain.data (), &badsync_plain);
    ftx_ft2_channel_est_c (cd.data (), cd_eq_direct.data (), ch_snr_direct.data ());
    compute_ft2_tone_powers_direct (cd_eq_direct.data (), pwr_eq_direct.data ());
    compute_ft2_eq_raw_direct (pwr_eq_direct.data (), ch_snr_direct.data (), bmet_eq_raw_direct.data ());
    ftx_ft2_bitmetrics_diag_c (cd.data (),
                               bitmetrics_cpp.data (),
                               bitmetrics_base_cpp.data (),
                               bmet_eq_raw_cpp.data (),
                               bmet_eq_cpp.data (),
                               cd_eq_cpp.data (),
                               ch_snr_cpp.data (),
                               &badsync_cpp,
                               &use_cheq_cpp,
                               &snr_min_cpp,
                               &snr_max_cpp,
                               &snr_mean_cpp,
                               &fading_depth_cpp,
                               &noise_var_cpp,
                               &noise_var_eq_cpp);

    result.badsync_ref = badsync_plain;
    result.badsync_cpp = badsync_cpp;
    result.use_cheq_cpp = use_cheq_cpp != 0;
    result.snr_min_cpp = snr_min_cpp;
    result.snr_max_cpp = snr_max_cpp;
    result.snr_mean_cpp = snr_mean_cpp;
    result.fading_depth_cpp = fading_depth_cpp;
    result.noise_var_cpp = noise_var_cpp;
    result.noise_var_eq_cpp = noise_var_eq_cpp;

    {
      bool use_cheq_from_ch_est = false;
      compute_reference_stats (ch_snr_direct.data (),
                               &result.snr_min_ref,
                               &result.snr_max_ref,
                               &result.snr_mean_ref,
                               &result.fading_depth_ref,
                               &use_cheq_from_ch_est);
      result.use_cheq_ref = use_cheq_from_ch_est;
    }

    result.max_diff_ref_diag_vs_plain = max_abs_diff (bitmetrics_cpp.data (),
                                                      bitmetrics_plain.data (),
                                                      kFt2Rows * 3,
                                                      &result.max_diff_ref_diag_vs_plain_index);
    result.max_diff_final = max_abs_diff (bitmetrics_plain.data (), bitmetrics_cpp.data (),
                                          kFt2Rows * 3, &result.max_diff_final_index);
    result.max_diff_base = max_abs_diff (bitmetrics_plain.data (), bitmetrics_base_cpp.data (),
                                         kFt2Rows * 3, &result.max_diff_base_index);
    result.max_diff_eq_raw = max_abs_diff (bmet_eq_raw_direct.data (), bmet_eq_raw_cpp.data (),
                                           kFt2Rows, &result.max_diff_eq_raw_index);
    result.max_diff_eq = 0.0f;
    result.max_diff_eq_index = -1;
    if (result.max_diff_eq_raw_index >= 0)
      {
        int const i = result.max_diff_eq_raw_index;
        result.eq_raw_ref_at_max = bmet_eq_raw_direct[static_cast<size_t> (i)];
        result.eq_raw_cpp_at_max = bmet_eq_raw_cpp[static_cast<size_t> (i)];
      }
    if (result.max_diff_final_index >= 0)
      {
        int const i = result.max_diff_final_index;
        result.final_ref_at_max = bitmetrics_plain[static_cast<size_t> (i)];
        result.final_cpp_at_max = bitmetrics_cpp[static_cast<size_t> (i)];
      }
    if (result.max_diff_base_index >= 0)
      {
        int const i = result.max_diff_base_index;
        result.base_ref_at_max = bitmetrics_plain[static_cast<size_t> (i)];
        result.base_cpp_at_max = bitmetrics_base_cpp[static_cast<size_t> (i)];
      }
    result.max_diff_ref_vs_base = max_abs_diff (bitmetrics_plain.data (), bitmetrics_base_cpp.data (),
                                                kFt2Rows * 3, &result.max_diff_ref_vs_base_index);
    result.max_diff_cpp_blend = max_abs_diff (bitmetrics_base_cpp.data (), bitmetrics_cpp.data (),
                                              kFt2Rows * 3, &result.max_diff_cpp_blend_index);
    result.max_abs_eq_metric = 0.0f;
    result.max_abs_eq_metric_index = -1;
    for (int i = 0; i < kFt2Rows; ++i)
      {
        float const magnitude = std::fabs (bmet_eq_cpp[static_cast<size_t> (i)]);
        if (magnitude > result.max_abs_eq_metric)
          {
            result.max_abs_eq_metric = magnitude;
            result.max_abs_eq_metric_index = i;
          }
      }
    result.max_diff_ch_snr = max_abs_diff (ch_snr_direct.data (), ch_snr_cpp.data (),
                                           kFt2Nn, &result.max_diff_ch_snr_index);
    result.max_diff_cd_eq = max_abs_diff (cd_eq_direct.data (), cd_eq_cpp.data (),
                                          kFt2NSeg, &result.max_diff_cd_eq_index);
    result.max_diff_eq_pwr = 0.0f;
    result.max_diff_eq_pwr_index = -1;
    result.max_diff_eq_raw_direct = result.max_diff_eq_raw;
    result.max_diff_eq_raw_direct_index = result.max_diff_eq_raw_index;

    result.mismatch =
        (result.badsync_ref != result.badsync_cpp)
        || (result.use_cheq_ref != result.use_cheq_cpp)
        || (std::fabs (result.snr_min_ref - result.snr_min_cpp) > tolerance)
        || (std::fabs (result.snr_max_ref - result.snr_max_cpp) > tolerance)
        || (std::fabs (result.snr_mean_ref - result.snr_mean_cpp) > tolerance)
        || (std::fabs (result.fading_depth_ref - result.fading_depth_cpp) > tolerance)
        || (result.max_diff_final > tolerance)
        || (result.max_diff_eq_raw > tolerance)
        || (result.max_diff_ch_snr > tolerance)
        || (result.max_diff_cd_eq > tolerance);

    return result;
  }

  QString describe_result (CompareResult const& result)
  {
    MetricLocation const final_loc = decode_metric_index (result.max_diff_final_index, kFt2Rows);
    MetricLocation const base_loc = decode_metric_index (result.max_diff_base_index, kFt2Rows);
    return QStringLiteral (
               "badsync ref/cpp=%1/%2 use_cheq ref/cpp=%3/%4 "
               "fade ref/cpp=%5/%6 snr[min,max,mean] ref=(%7,%8,%9) cpp=(%10,%11,%12) "
               "noise ref/cpp=(%13,%14)/(%15,%16) refdiag-plain=%17@%18 maxdiff final=%19@%20 "
               "[r%21,c%22]=(%23/%24) base=%25@%26[r%27,c%28]=(%29/%30) "
               "eq_raw=%31@%32(%33/%34) eq=%35@%36(%37/%38) "
               "ref-vs-base=%39@%40 base-vs-final=%41@%42 eq_abs=%43@%44 "
               "ch_snr=%45@%46 cd_eq=%47@%48 eq_pwr=%49@%50 eq_raw_direct=%51@%52")
        .arg (result.badsync_ref)
        .arg (result.badsync_cpp)
        .arg (result.use_cheq_ref ? 1 : 0)
        .arg (result.use_cheq_cpp ? 1 : 0)
        .arg (result.fading_depth_ref, 0, 'f', 6)
        .arg (result.fading_depth_cpp, 0, 'f', 6)
        .arg (result.snr_min_ref, 0, 'f', 6)
        .arg (result.snr_max_ref, 0, 'f', 6)
        .arg (result.snr_mean_ref, 0, 'f', 6)
        .arg (result.snr_min_cpp, 0, 'f', 6)
        .arg (result.snr_max_cpp, 0, 'f', 6)
        .arg (result.snr_mean_cpp, 0, 'f', 6)
        .arg (result.noise_var_ref, 0, 'f', 6)
        .arg (result.noise_var_eq_ref, 0, 'f', 6)
        .arg (result.noise_var_cpp, 0, 'f', 6)
        .arg (result.noise_var_eq_cpp, 0, 'f', 6)
        .arg (result.max_diff_ref_diag_vs_plain, 0, 'g', 9)
        .arg (result.max_diff_ref_diag_vs_plain_index)
        .arg (result.max_diff_final, 0, 'g', 9)
        .arg (result.max_diff_final_index)
        .arg (final_loc.row >= 0 ? final_loc.row + 1 : -1)
        .arg (final_loc.col >= 0 ? final_loc.col + 1 : -1)
        .arg (result.final_ref_at_max, 0, 'g', 9)
        .arg (result.final_cpp_at_max, 0, 'g', 9)
        .arg (result.max_diff_base, 0, 'g', 9)
        .arg (result.max_diff_base_index)
        .arg (base_loc.row >= 0 ? base_loc.row + 1 : -1)
        .arg (base_loc.col >= 0 ? base_loc.col + 1 : -1)
        .arg (result.base_ref_at_max, 0, 'g', 9)
        .arg (result.base_cpp_at_max, 0, 'g', 9)
        .arg (result.max_diff_eq_raw, 0, 'g', 9)
        .arg (result.max_diff_eq_raw_index)
        .arg (result.eq_raw_ref_at_max, 0, 'g', 9)
        .arg (result.eq_raw_cpp_at_max, 0, 'g', 9)
        .arg (result.max_diff_eq, 0, 'g', 9)
        .arg (result.max_diff_eq_index)
        .arg (result.eq_ref_at_max, 0, 'g', 9)
        .arg (result.eq_cpp_at_max, 0, 'g', 9)
        .arg (result.max_diff_ref_vs_base, 0, 'g', 9)
        .arg (result.max_diff_ref_vs_base_index)
        .arg (result.max_diff_cpp_blend, 0, 'g', 9)
        .arg (result.max_diff_cpp_blend_index)
        .arg (result.max_abs_eq_metric, 0, 'g', 9)
        .arg (result.max_abs_eq_metric_index)
        .arg (result.max_diff_ch_snr, 0, 'g', 9)
        .arg (result.max_diff_ch_snr_index)
        .arg (result.max_diff_cd_eq, 0, 'g', 9)
        .arg (result.max_diff_cd_eq_index)
        .arg (result.max_diff_eq_pwr, 0, 'g', 9)
        .arg (result.max_diff_eq_pwr_index)
        .arg (result.max_diff_eq_raw_direct, 0, 'g', 9)
        .arg (result.max_diff_eq_raw_direct_index);
  }
}

int main (int argc, char* argv[])
{
  QCoreApplication app {argc, argv};
  QCoreApplication::setApplicationName (QStringLiteral ("ft2_equalized_compare"));

  try
    {
      QCommandLineParser parser;
      parser.setApplicationDescription (
          QStringLiteral ("Scan FT2 candidate/segment windows and compare Fortran vs C++ equalized-branch diagnostics."));
      parser.addHelpOption ();

      QCommandLineOption fa_option (
          QStringList {QStringLiteral ("fa")},
          QStringLiteral ("Lower search frequency in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("200"));
      QCommandLineOption fb_option (
          QStringList {QStringLiteral ("fb")},
          QStringLiteral ("Upper search frequency in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("3000"));
      QCommandLineOption nfqso_option (
          QStringList {QStringLiteral ("nfqso")},
          QStringLiteral ("Nominal QSO frequency in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("1000"));
      QCommandLineOption syncmin_option (
          QStringList {QStringLiteral ("syncmin")},
          QStringLiteral ("Candidate search sync threshold."),
          QStringLiteral ("value"),
          QStringLiteral ("0.60"));
      QCommandLineOption maxcand_option (
          QStringList {QStringLiteral ("maxcand")},
          QStringLiteral ("Maximum number of FT2 candidates to scan."),
          QStringLiteral ("n"),
          QString::number (kFt2MaxCandDefault));
      QCommandLineOption candidate_option (
          QStringList {QStringLiteral ("candidate")},
          QStringLiteral ("Only inspect one 1-based candidate index."),
          QStringLiteral ("n"));
      QCommandLineOption segment_option (
          QStringList {QStringLiteral ("segment")},
          QStringLiteral ("Only inspect one segment index (1..3)."),
          QStringLiteral ("n"));
      QCommandLineOption tolerance_option (
          QStringList {QStringLiteral ("tolerance")},
          QStringLiteral ("Absolute tolerance for Fortran-vs-C++ comparisons."),
          QStringLiteral ("value"),
          QStringLiteral ("1e-4"));
      QCommandLineOption show_all_option (
          QStringList {QStringLiteral ("show-all")},
          QStringLiteral ("Print every scanned candidate segment, not only mismatches."));

      parser.addOption (fa_option);
      parser.addOption (fb_option);
      parser.addOption (nfqso_option);
      parser.addOption (syncmin_option);
      parser.addOption (maxcand_option);
      parser.addOption (candidate_option);
      parser.addOption (segment_option);
      parser.addOption (tolerance_option);
      parser.addOption (show_all_option);
      parser.addPositionalArgument (
          QStringLiteral ("wav"),
          QStringLiteral ("Path to a 12000 Hz mono 16-bit FT2 WAV file."));
      parser.process (app);

      QStringList const positional = parser.positionalArguments ();
      if (positional.size () != 1)
        {
          fail (QStringLiteral ("expected exactly one FT2 WAV path"));
        }

      QString const wav_path = QFileInfo {positional.front ()}.absoluteFilePath ();
      int const fa = parse_int_option (parser, fa_option, QStringLiteral ("fa"));
      int const fb = parse_int_option (parser, fb_option, QStringLiteral ("fb"));
      int const nfqso = parse_int_option (parser, nfqso_option, QStringLiteral ("nfqso"));
      float const syncmin = parse_float_option (parser, syncmin_option, QStringLiteral ("syncmin"));
      int const maxcand = parse_int_option (parser, maxcand_option, QStringLiteral ("maxcand"));
      int const candidate_filter =
          parser.isSet (candidate_option)
              ? parse_int_option (parser, candidate_option, QStringLiteral ("candidate"))
              : -1;
      int const segment_filter =
          parser.isSet (segment_option)
              ? parse_int_option (parser, segment_option, QStringLiteral ("segment"))
              : -1;
      float const tolerance = parse_float_option (parser, tolerance_option, QStringLiteral ("tolerance"));
      bool const show_all = parser.isSet (show_all_option);

      WavData const wav = read_wav_file (wav_path);
      std::vector<float> dd (wav.samples.begin (), wav.samples.end ());

      auto const tweaks = make_tweaks ();
      std::array<float, kFt2Nh1> savg {};
      std::array<float, kFt2Nh1> sbase {};
      std::vector<float> candidate (static_cast<size_t> (2 * maxcand), 0.0f);
      std::array<Complex, kFt2NdMax> cd2 {};
      std::array<Complex, kFt2NdMax> cb {};
      std::array<Complex, kFt2NSeg> cd {};

      int ncand = 0;
      ftx_getcandidates2_c (dd.data (), static_cast<float> (fa), static_cast<float> (fb),
                            syncmin, static_cast<float> (nfqso), maxcand,
                            savg.data (), candidate.data (), &ncand, sbase.data ());

      std::cout << "file: " << wav_path.toStdString () << "\n";
      std::cout << "samples: source=" << wav.source_samples
                << " compare=" << wav.samples.size ()
                << " sample_rate=" << wav.sample_rate
                << " channels=" << wav.channels
                << " bits=" << wav.bits_per_sample << "\n";
      std::cout << "scan params: fa=" << fa
                << " fb=" << fb
                << " nfqso=" << nfqso
                << " syncmin=" << syncmin
                << " maxcand=" << maxcand
                << " tolerance=" << tolerance << "\n";
      std::cout << "candidates: " << ncand << "\n\n";

      int newdata = 1;
      int compared_segments = 0;
      int mismatched_segments = 0;

      for (int icand = 0; icand < ncand; ++icand)
        {
          if (candidate_filter > 0 && candidate_filter != icand + 1)
            {
              continue;
            }
          float const f0 = candidate[static_cast<size_t> (icand * 2)];
          float const snr_est = candidate[static_cast<size_t> (icand * 2 + 1)] - 1.0f;

          ftx_ft2_downsample_c (dd.data (), &newdata, f0, cd2.data ());
          newdata = 0;
          normalize_complex_buffer (cd2.data (), kFt2NdMax);

          for (int iseg = 1; iseg <= 3; ++iseg)
            {
              if (segment_filter > 0 && segment_filter != iseg)
                {
                  continue;
                }
              int ibest = -1;
              int idfbest = 0;
              float smax = -99.0f;

              for (int isync = 1; isync <= 2; ++isync)
                {
                  int idfmin = -12;
                  int idfmax = 12;
                  int idfstp = 3;
                  int ibmin = -688;
                  int ibmax = 2024;
                  int ibstp = 4;

                  if (isync == 1)
                    {
                      if (iseg == 1)
                        {
                          ibmin = 216;
                          ibmax = 1120;
                        }
                      else if (iseg == 2)
                        {
                          ibmin = 1120;
                          ibmax = 2024;
                        }
                      else
                        {
                          ibmin = -688;
                          ibmax = 216;
                        }
                    }
                  else
                    {
                      idfmin = idfbest - 4;
                      idfmax = idfbest + 4;
                      idfstp = 1;
                      ibmin = ibest - 5;
                      ibmax = ibest + 5;
                      ibstp = 1;
                    }

                  ibest = -1;
                  idfbest = 0;
                  smax = -99.0f;
                  for (int idf = idfmin; idf <= idfmax; idf += idfstp)
                    {
                      for (int istart = ibmin; istart <= ibmax; istart += ibstp)
                        {
                          float sync = 0.0f;
                          ftx_sync2d_c (cd2.data (), kFt2NdMax, istart,
                                        tweaks[static_cast<size_t> (idf_index (idf))].data (), 1,
                                        &sync);
                          if (sync > smax)
                            {
                              smax = sync;
                              ibest = istart;
                              idfbest = idf;
                            }
                        }
                    }
                }

              float const f1 = f0 + static_cast<float> (idfbest);
              if (f1 <= 10.0f || f1 >= 4990.0f)
                {
                  continue;
                }

              ftx_ft2_downsample_c (dd.data (), &newdata, f1, cb.data ());
              normalize_complex_buffer (cb.data (), kFt2NdMax);
              extract_ft2_window (cd, cb, ibest);

              CompareResult const result = compare_cd (cd, tolerance);
              ++compared_segments;
              if (result.mismatch)
                {
                  ++mismatched_segments;
                }

              if (show_all || result.mismatch)
                {
                  std::cout << "cand=" << (icand + 1)
                            << " seg=" << iseg
                            << " f0=" << f0
                            << " f1=" << f1
                            << " ibest=" << ibest
                            << " idfbest=" << idfbest
                            << " smax=" << smax
                            << " snr_est=" << snr_est
                            << "\n  " << describe_result (result).toStdString ()
                            << "\n\n";
                }
            }
        }

      std::cout << "summary:\n";
      std::cout << "  compared_segments=" << compared_segments << "\n";
      std::cout << "  mismatched_segments=" << mismatched_segments << "\n";
      return mismatched_segments == 0 ? 0 : 1;
    }
  catch (std::exception const& error)
    {
      std::cerr << error.what () << '\n';
      return 1;
    }
}
