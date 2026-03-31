#include <QByteArray>
#include <QString>
#include <QVector>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

#include "Detector/FtxFst4Ldpc.hpp"
#include "Modulator/FtxMessageEncoder.hpp"

extern "C" float gran_ ();

namespace
{
constexpr int kCodewordBits = 240;
constexpr int kMessageBits = 101;
constexpr int kPayloadBits = 77;
constexpr int kPayloadTones = 120;

constexpr std::array<unsigned char, 77> kFt2Ft4Rvec {{
  0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0,
  1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1,
  0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1
}};

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

std::array<float, kCodewordBits> make_llr_awgn (std::array<signed char, kCodewordBits> const& codeword,
                                                float sigma, float sigma_override, int& nerr_out)
{
  std::array<float, kCodewordBits> rxdata {};
  nerr_out = 0;
  for (int i = 0; i < kCodewordBits; ++i)
    {
      float const symbol = codeword[static_cast<size_t> (i)] != 0 ? 1.0f : -1.0f;
      rxdata[static_cast<size_t> (i)] = symbol + sigma * gran_ ();
      if (rxdata[static_cast<size_t> (i)] * symbol < 0.0f)
        {
          ++nerr_out;
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

std::string trim_right (QByteArray const& data)
{
  QByteArray copy = data;
  while (!copy.isEmpty () && (copy.back () == ' ' || copy.back () == '\0'))
    {
      copy.chop (1);
    }
  return copy.toStdString ();
}

void print_bits (std::array<signed char, kMessageBits> const& bits)
{
  for (int i = 0; i < kPayloadBits; ++i)
    {
      std::cout << int (bits[static_cast<size_t> (i)]);
    }
  std::cout << ' ';
  for (int i = kPayloadBits; i < kMessageBits; ++i)
    {
      std::cout << int (bits[static_cast<size_t> (i)]);
    }
  std::cout << '\n';
}

void print_codeword (std::array<signed char, kCodewordBits> const& bits)
{
  for (int i = 0; i < 77; ++i)
    {
      std::cout << int (bits[static_cast<size_t> (i)]);
    }
  std::cout << ' ';
  for (int i = 77; i < 101; ++i)
    {
      std::cout << int (bits[static_cast<size_t> (i)]);
    }
  std::cout << ' ';
  for (int i = 101; i < kCodewordBits; ++i)
    {
      std::cout << int (bits[static_cast<size_t> (i)]);
    }
  std::cout << '\n';
}

void print_usage ()
{
  std::cout << "Usage: ldpcsim240_101 niter ndeep #trials s K [msg]\n"
            << "e.g.   ldpcsim240_101 20 5 1000 0.85 91 \"K9AN K1JT FN20\"\n"
            << "s<0 uses sigma derived from SNR. niter is kept for CLI compatibility.\n";
}
}

int main (int argc, char* argv[])
{
  if (argc != 6 && argc != 7)
    {
      print_usage ();
      return 0;
    }

  int const max_iterations = QString::fromLocal8Bit (argv[1]).toInt ();
  int const norder = QString::fromLocal8Bit (argv[2]).toInt ();
  int const ntrials = QString::fromLocal8Bit (argv[3]).toInt ();
  float const s = QString::fromLocal8Bit (argv[4]).toFloat ();
  int const keff = QString::fromLocal8Bit (argv[5]).toInt ();
  QString const message = argc == 7 ? QString::fromLocal8Bit (argv[6])
                                    : QStringLiteral ("K9AN K1JT FN20");

  auto const encoded = decodium::txmsg::encodeFst4WithHint (message, false, false);
  if (!encoded.ok || encoded.tones.size () != 160 || encoded.msgbits.size () != 101
      || (encoded.i3 == 0 && encoded.n3 == 6))
    {
      std::cerr << "Failed to encode standard FST4 message.\n";
      return 1;
    }

  std::array<signed char, kMessageBits> message_bits {};
  for (int i = 0; i < kMessageBits; ++i)
    {
      message_bits[static_cast<size_t> (i)] = encoded.msgbits.at (i) != 0 ? 1 : 0;
    }
  std::array<signed char, kCodewordBits> const codeword =
      codeword_from_payload_tones (extract_payload_tones (encoded.tones));

  float const rate = static_cast<float> (keff) / static_cast<float> (kCodewordBits);
  std::cout << "code rate: " << rate << '\n'
            << "niter    : " << max_iterations << '\n'
            << "norder   : " << norder << '\n'
            << "s        : " << s << '\n'
            << "K        : " << keff << '\n'
            << "message  : " << trim_right (encoded.msgsent) << '\n'
            << "message with crc24\n";
  print_bits (message_bits);
  std::cout << "codeword\n";
  print_codeword (codeword);
  std::cout << "Eb/N0    Es/N0   ngood  nundetected   sigma   symbol error rate\n";

  bool first_decode_printed = false;
  for (int idb = 8; idb >= -3; --idb)
    {
      float const db = 0.5f * idb - 1.0f;
      float const sigma = 1.0f / std::sqrt (2.0f * rate * std::pow (10.0f, db / 10.0f));
      int ngood = 0;
      int nue = 0;
      int nberr = 0;

      for (int itrial = 0; itrial < ntrials; ++itrial)
        {
          int nerr = 0;
          std::array<float, kCodewordBits> const llr = make_llr_awgn (codeword, sigma, s, nerr);
          nberr += nerr;

          std::array<signed char, kCodewordBits> apmask {};
          std::array<signed char, kMessageBits> decoded_message {};
          std::array<signed char, kCodewordBits> decoded_codeword {};
          int ntype = 0;
          int nharderror = -1;
          float dmin = 0.0f;
          bool const have_codeword = decodium::fst4::decode240_101_native (
              llr, keff, 2, norder, apmask,
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
              if (!first_decode_printed)
                {
                  std::array<unsigned char, kPayloadBits> raw77 {};
                  for (int i = 0; i < kPayloadBits; ++i)
                    {
                      raw77[static_cast<size_t> (i)] =
                          static_cast<unsigned char> ((decoded_message[static_cast<size_t> (i)] + kFt2Ft4Rvec[static_cast<size_t> (i)]) & 0x1u);
                    }
                  QByteArray raw_bits (kPayloadBits, '\0');
                  for (int i = 0; i < kPayloadBits; ++i)
                    {
                      raw_bits[i] = static_cast<char> (raw77[static_cast<size_t> (i)]);
                    }
                  auto const decoded = decodium::txmsg::decode77 (raw_bits, 1, 0);
                  if (decoded.ok)
                    {
                      std::cout << "Decoded message: " << trim_right (decoded.msgsent) << '\n';
                    }
                  first_decode_printed = true;
                }
            }
          else
            {
              ++nue;
            }
        }

      float const esn0 = db + 10.0f * std::log10 (rate);
      float const pberr = ntrials > 0 ? static_cast<float> (nberr) / (ntrials * kCodewordBits) : 0.0f;
      float const sigma_print = s < 0.0f ? sigma : s;
      std::cout << std::fixed << std::setprecision (1)
                << std::setw (4) << db << "    "
                << std::setw (5) << esn0 << ' '
                << std::setw (8) << ngood << ' '
                << std::setw (11) << nue << "        "
                << std::setw (5) << std::setprecision (2) << sigma_print
                << "    "
                << std::scientific << std::setprecision (3) << pberr
                << std::defaultfloat << '\n';
    }

  return 0;
}
