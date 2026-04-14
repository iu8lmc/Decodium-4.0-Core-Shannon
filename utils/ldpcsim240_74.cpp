#include <QByteArray>
#include <QString>
#include <QVector>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "Detector/FtxFst4Ldpc.hpp"
#include "Modulator/FtxMessageEncoder.hpp"

extern "C" float gran_ ();

namespace
{
constexpr int kCodewordBits = 240;
constexpr int kMessageBits = 74;
constexpr int kPayloadTones = 120;
constexpr int kSymbols = 120;
constexpr int kGrayMap[4] = {0, 1, 3, 2};

std::array<int, kPayloadTones> extract_payload_tones (QVector<int> const& tones)
{
  std::array<int, kPayloadTones> payload {};
  auto copy_block = [&tones, &payload] (int tone_start, int payload_start) {
    for (int i = 0; i < 30; ++i)
      {
        payload[static_cast<size_t> (payload_start + i)] = tones.at (tone_start + i);
      }
  };
  copy_block (8, 0);
  copy_block (46, 30);
  copy_block (84, 60);
  copy_block (122, 90);
  return payload;
}

std::array<signed char, kCodewordBits> codeword_from_payload_tones (std::array<int, kPayloadTones> const& tones)
{
  std::array<signed char, kCodewordBits> codeword {};
  for (int i = 0; i < kPayloadTones; ++i)
    {
      int msb = 0;
      int lsb = 0;
      switch (tones[static_cast<size_t> (i)])
        {
        case 0: msb = 0; lsb = 0; break;
        case 1: msb = 0; lsb = 1; break;
        case 3: msb = 1; lsb = 0; break;
        case 2: msb = 1; lsb = 1; break;
        default: break;
        }
      codeword[static_cast<size_t> (2 * i)] = static_cast<signed char> (msb);
      codeword[static_cast<size_t> (2 * i + 1)] = static_cast<signed char> (lsb);
    }
  return codeword;
}

void normalize_metric_column (float* data, int n)
{
  float mean = 0.0f;
  float mean2 = 0.0f;
  for (int i = 0; i < n; ++i)
    {
      mean += data[i];
      mean2 += data[i] * data[i];
    }
  mean /= n;
  mean2 /= n;
  float const variance = mean2 - mean * mean;
  float sigma = 0.0f;
  if (variance > 0.0f)
    {
      sigma = std::sqrt (variance);
    }
  else if (mean2 > 0.0f)
    {
      sigma = std::sqrt (mean2);
    }
  if (!(sigma > 0.0f))
    {
      return;
    }
  for (int i = 0; i < n; ++i)
    {
      data[i] /= sigma;
    }
}

float max_metric_for_bit (float const* values, int count, int bit_index, int bit_value)
{
  float best = -1.0e30f;
  for (int i = 0; i < count; ++i)
    {
      if (((i >> bit_index) & 1) == bit_value)
        {
          best = std::max (best, values[static_cast<size_t> (i)]);
        }
    }
  return best;
}

std::array<float, kCodewordBits> make_bpsk_llr (std::array<signed char, kCodewordBits> const& codeword,
                                                float sigma, float sigma_override, int& nberr_out)
{
  std::array<float, kCodewordBits> rxdata {};
  nberr_out = 0;
  for (int i = 0; i < kCodewordBits; ++i)
    {
      float const symbol = codeword[static_cast<size_t> (i)] != 0 ? 1.0f : -1.0f;
      rxdata[static_cast<size_t> (i)] = symbol + sigma * gran_ ();
      if (rxdata[static_cast<size_t> (i)] * symbol < 0.0f)
        {
          ++nberr_out;
        }
    }

  float mean = 0.0f;
  float mean2 = 0.0f;
  for (float sample : rxdata)
    {
      mean += sample;
      mean2 += sample * sample;
    }
  mean /= kCodewordBits;
  mean2 /= kCodewordBits;
  float const variance = std::max (0.0f, mean2 - mean * mean);
  float const scale = std::sqrt (variance);
  if (scale > 0.0f)
    {
      for (float& sample : rxdata)
        {
          sample /= scale;
        }
    }

  float const ss = sigma_override < 0.0f ? sigma : sigma_override;
  std::array<float, kCodewordBits> llr {};
  float const denom = ss * ss;
  for (int i = 0; i < kCodewordBits; ++i)
    {
      llr[static_cast<size_t> (i)] = denom > 0.0f ? 2.0f * rxdata[static_cast<size_t> (i)] / denom : 0.0f;
    }
  return llr;
}

std::array<float, kCodewordBits> make_4fsk_llr (std::array<int, kPayloadTones> const& itone,
                                                float sigma, int channeltype, int& nsymerr_out)
{
  using Complex = std::complex<float>;
  std::array<float, kCodewordBits * 4> bitmetrics {};
  std::array<float, kCodewordBits> llr {};
  std::array<Complex, 4 * 8> c1 {};
  std::array<Complex, 16 * 4> c2 {};
  std::array<Complex, 256 * 2> c4 {};
  std::vector<float> s2 (65536, 0.0f);
  std::array<Complex, 4 * kSymbols> cs {};
  nsymerr_out = 0;

  auto cs_at = [&cs] (int tone, int symbol) -> Complex& {
    return cs[static_cast<size_t> (tone + 4 * symbol)];
  };
  auto bitmetric_at = [&bitmetrics] (int row0, int block0) -> float& {
    return bitmetrics[static_cast<size_t> (row0 + kCodewordBits * block0)];
  };

  for (int i = 0; i < kSymbols; ++i)
    {
      for (int j = 0; j < 4; ++j)
        {
          float amplitude = 0.0f;
          if (j == itone[static_cast<size_t> (i)])
            {
              amplitude = 1.0f;
              if (channeltype == 1)
                {
                  float const xi = gran_ () * gran_ () + gran_ () * gran_ ();
                  amplitude = std::sqrt (xi / 2.0f);
                }
            }
          cs_at (j, i) = Complex {amplitude + sigma * gran_ (), sigma * gran_ ()};
        }
      int best_index = 0;
      float best_value = -1.0f;
      for (int j = 0; j < 4; ++j)
        {
          float const value = std::abs (cs_at (j, i));
          if (value > best_value)
            {
              best_value = value;
              best_index = j;
            }
        }
      if (best_index != itone[static_cast<size_t> (i)])
        {
          ++nsymerr_out;
        }
    }

  for (int k = 0; k < kSymbols; k += 8)
    {
      for (int m = 0; m < 8; ++m)
        {
          float tones[4] {};
          for (int is = 0; is < 4; ++is)
            {
              c1[static_cast<size_t> (is + 4 * m)] = cs_at (kGrayMap[is], k + m);
              tones[is] = std::abs (c1[static_cast<size_t> (is + 4 * m)]);
            }
          int const ipt = 2 * k + 2 * m;
          for (int ib = 0; ib < 2; ++ib)
            {
              bitmetric_at (ipt + ib, 0) = max_metric_for_bit (tones, 4, 1 - ib, 1)
                                         - max_metric_for_bit (tones, 4, 1 - ib, 0);
            }
        }

      for (int m = 0; m < 4; ++m)
        {
          float tones[16] {};
          for (int i = 0; i < 4; ++i)
            {
              for (int j = 0; j < 4; ++j)
                {
                  int const is = i * 4 + j;
                  c2[static_cast<size_t> (is + 16 * m)] = c1[static_cast<size_t> (i + 4 * (2 * m))]
                                                        + c1[static_cast<size_t> (j + 4 * (2 * m + 1))];
                  tones[is] = std::norm (c2[static_cast<size_t> (is + 16 * m)]);
                }
            }
          int const ipt = 2 * k + 4 * m;
          for (int ib = 0; ib < 4; ++ib)
            {
              bitmetric_at (ipt + ib, 1) = max_metric_for_bit (tones, 16, 3 - ib, 1)
                                         - max_metric_for_bit (tones, 16, 3 - ib, 0);
            }
        }

      for (int m = 0; m < 2; ++m)
        {
          std::array<float, 256> tones {};
          for (int i = 0; i < 16; ++i)
            {
              for (int j = 0; j < 16; ++j)
                {
                  int const is = i * 16 + j;
                  c4[static_cast<size_t> (is + 256 * m)] = c2[static_cast<size_t> (i + 16 * (2 * m))]
                                                         + c2[static_cast<size_t> (j + 16 * (2 * m + 1))];
                  tones[static_cast<size_t> (is)] = std::abs (c4[static_cast<size_t> (is + 256 * m)]);
                }
            }
          int const ipt = 2 * k + 8 * m;
          for (int ib = 0; ib < 8; ++ib)
            {
              bitmetric_at (ipt + ib, 2) = max_metric_for_bit (tones.data (), 256, 7 - ib, 1)
                                         - max_metric_for_bit (tones.data (), 256, 7 - ib, 0);
            }
        }

      for (int i = 0; i < 256; ++i)
        {
          for (int j = 0; j < 256; ++j)
            {
              int const is = i * 256 + j;
              s2[static_cast<size_t> (is)] =
                  std::abs (c4[static_cast<size_t> (i)] + c4[static_cast<size_t> (j + 256)]);
            }
        }
      int const ipt = 2 * k;
      for (int ib = 0; ib < 16; ++ib)
        {
          bitmetric_at (ipt + ib, 3) = max_metric_for_bit (s2.data (), 65536, 15 - ib, 1)
                                     - max_metric_for_bit (s2.data (), 65536, 15 - ib, 0);
        }
    }

  normalize_metric_column (bitmetrics.data () + kCodewordBits * 0, kCodewordBits);
  normalize_metric_column (bitmetrics.data () + kCodewordBits * 1, kCodewordBits);
  normalize_metric_column (bitmetrics.data () + kCodewordBits * 2, kCodewordBits);
  normalize_metric_column (bitmetrics.data () + kCodewordBits * 3, kCodewordBits);

  constexpr float scale = 2.83f;
  for (int i = 0; i < kCodewordBits; ++i)
    {
      llr[static_cast<size_t> (i)] = scale * bitmetrics[static_cast<size_t> (i)];
    }
  return llr;
}

std::string trim_right (QByteArray const& data)
{
  QByteArray copy = data;
  while (!copy.isEmpty () && (copy.back () == ' ' || copy.back () == '\0'))
    {
      copy.chop (1);
    }
  return copy.toStdString ();
}

void print_usage ()
{
  std::cout << "Usage: ldpcsim240_74 maxosd norder #trials s Keff modtype channel [msg]\n"
            << "e.g.   ldpcsim240_74 2 4 1000 0.85 50 1 0\n"
            << "modtype 0=BPSK, 1=4FSK. channel 0=AWGN, 1=Rayleigh (4FSK only).\n";
}
}

int main (int argc, char* argv[])
{
  if (argc != 8 && argc != 9)
    {
      print_usage ();
      return 0;
    }

  int const maxosd = QString::fromLocal8Bit (argv[1]).toInt ();
  int const norder = QString::fromLocal8Bit (argv[2]).toInt ();
  int const ntrials = QString::fromLocal8Bit (argv[3]).toInt ();
  float const s = QString::fromLocal8Bit (argv[4]).toFloat ();
  int const keff = QString::fromLocal8Bit (argv[5]).toInt ();
  int const modtype = QString::fromLocal8Bit (argv[6]).toInt ();
  int const channeltype = QString::fromLocal8Bit (argv[7]).toInt ();
  QString const message = argc == 9 ? QString::fromLocal8Bit (argv[8])
                                    : QStringLiteral ("K1ABC FN42 33");

  auto const encoded = decodium::txmsg::encodeFst4WithHint (message, true, false);
  if (!encoded.ok || encoded.tones.size () != 160 || encoded.msgbits.size () < 74
      || !(encoded.i3 == 0 && encoded.n3 == 6))
    {
      std::cerr << "Failed to encode FST4W message.\n";
      return 1;
    }

  std::array<signed char, kMessageBits> message_bits {};
  for (int i = 0; i < kMessageBits; ++i)
    {
      message_bits[static_cast<size_t> (i)] = encoded.msgbits.at (i) != 0 ? 1 : 0;
    }
  std::array<int, kPayloadTones> const itone = extract_payload_tones (encoded.tones);
  std::array<signed char, kCodewordBits> const codeword = codeword_from_payload_tones (itone);

  float const rate = static_cast<float> (keff) / static_cast<float> (kCodewordBits);
  std::cout << "code rate: " << rate << '\n'
            << "maxosd   : " << maxosd << '\n'
            << "norder   : " << norder << '\n'
            << "s        : " << s << '\n'
            << "K        : " << keff << '\n'
            << "modtype  : " << (modtype == 0 ? "coherent BPSK" : "noncoherent 4FSK") << '\n'
            << "channel  : " << (channeltype == 0 ? "AWGN" : "Rayleigh") << '\n'
            << "message  : " << trim_right (encoded.msgsent) << '\n'
            << "Eb/N0    Es/N0   ngood  nundetected   symbol error rate\n";

  for (int idb = 24; idb >= -8; --idb)
    {
      float const db = 0.5f * idb - 1.0f;
      int const iq = modtype == 0 ? 1 : 2;
      float const sigma = 1.0f / std::sqrt (2.0f * rate * iq * std::pow (10.0f, db / 10.0f));
      int ngood = 0;
      int nue = 0;
      int nsymerr = 0;

      for (int itrial = 0; itrial < ntrials; ++itrial)
        {
          std::array<float, kCodewordBits> llr {};
          if (modtype == 0)
            {
              int trial_berr = 0;
              llr = make_bpsk_llr (codeword, sigma, s, trial_berr);
            }
          else
            {
              int trial_symerr = 0;
              llr = make_4fsk_llr (itone, sigma, channeltype, trial_symerr);
              nsymerr += trial_symerr;
            }

          std::array<signed char, kCodewordBits> apmask {};
          std::array<signed char, kMessageBits> decoded_message {};
          std::array<signed char, kCodewordBits> decoded_codeword {};
          int ntype = 0;
          int nharderror = -1;
          float dmin = 0.0f;
          bool const have_codeword = decodium::fst4::decode240_74_native (
              llr, keff, maxosd, norder, apmask,
              decoded_message, decoded_codeword, ntype, nharderror, dmin);
          if (!have_codeword)
            {
              continue;
            }

          int diff = 0;
          for (int i = 0; i < kCodewordBits; ++i)
            {
              diff += decoded_codeword[static_cast<size_t> (i)] != codeword[static_cast<size_t> (i)] ? 1 : 0;
            }
          if (diff == 0)
            {
              ++ngood;
            }
          else
            {
              ++nue;
            }
        }

      float const esn0 = db + 10.0f * std::log10 (rate * iq);
      float const pserr = ntrials > 0 ? static_cast<float> (nsymerr) / (ntrials * kSymbols) : 0.0f;
      std::cout << std::fixed << std::setprecision (1)
                << std::setw (4) << db << "    "
                << std::setw (5) << esn0 << ' '
                << std::setw (8) << ngood << ' '
                << std::setw (11) << nue << "        "
                << std::scientific << std::setprecision (3) << pserr
                << std::defaultfloat << '\n';
    }

  return 0;
}
