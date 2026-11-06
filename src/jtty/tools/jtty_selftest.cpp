// Autotest del port JTTY: vettori d'oro generati dal riferimento Fortran di
// WSJT-X 3.2.0-rc1 (lib/jtty, pack_jtty) e un giro completo trasmissione ->
// rumore -> ricevitore. Esce con 0 se tutto torna.

#include "JttyCodec.hpp"
#include "JttyDecoder.hpp"
#include "JttyWave.hpp"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace decodium::jtty;

namespace
{

struct Golden
{
  int profile;
  char const* message;
  int nframes;
  char const* canonical;
  std::vector<char const*> frames;
};

std::vector<Golden> const kGolden = {
  {0, "CQ K1ABC CQ", 1, "CQ K1ABC CQ", {"0000100110111101111000110101000001"}},
  {0, "WB9XYZ TU CQ KA1ABC CQ", 2, "WB9XYZ TU CQ KA1ABC CQ", {"1110011100111000011110111010110000", "1001010111000110010100100001000001"}},
  {0, "WB9XYZ 599 123", 2, "WB9XYZ 599 123", {"1110011100111000011110111010010000", "1011100000000001111011000000001001"}},
  {0, "599 MA", 1, "599 MA", {"1001100000001100100010000000011001"}},
  {0, "599 FN42", 1, "599 FN42", {"0001101010000110011000000001001001"}},
  {0, "1D EMA", 1, "1D EMA", {"0010000010110001011000000000101001"}},
  {0, "599 05", 2, "599 05", {"0011100000001001010111000000001000", "0000000001011001001001001001001101"}},
  {2, "599 05", 1, "599 005", {"1000000000000000000101000000001001"}},
  {2, "K1ABC 599 001", 2, "K1ABC 599 001", {"0000100110111101111000110101010000", "1000000000000000000001000000001001"}},
  {1, "1D EMA", 1, "1D EMA", {"0010000010110001011000000000101001"}},
  {0, "TU NOW JA6DEF 599 102", 2, "TU NOW JA6DEF 599 102", {"1000111100011100111100110111010100", "1011100000000001100110000000001001"}},
  {0, "JA6DEF AGN?", 1, "JA6DEF AGN?", {"1000111100011100111100110111000101"}},
  {0, "QSL TU", 1, "QSL TU", {"0000000101100000000000000001001001"}},
  {0, "hello world", 3, "HELLO WORLD", {"0100010011100101010101010110001100", "1001001000000110000110110101011100", "0011011001001001001001001001001101"}},
  {0, "THE QUICK BROWN FOX", 4, "THE QUICK BROWN FOX", {"0111010100010011101001000110101100", "0111100100100011000101001001001100", "0010110110110110001000000101111100", "1001000011110110001000011001001101"}},
  {0, "IU8LMC/P", 2, "IU8LMC/P", {"0100100111100010000101010101101100", "0011001010000110011001001001001101"}},
  {0, "599 131072", 2, "599 131072", {"0001010010010010011001000000011100", "0000110000010000000001110000101101"}},
  {2, "599 0123", 1, "599 123", {"1000000000000001111011000000001001"}},
};

int failures = 0;

void check (bool ok, std::string const& what)
{
  if (!ok)
    {
      ++failures;
      std::printf ("FAIL %s\n", what.c_str ());
    }
}

// Un messaggio a 1500 Hz in rumore gaussiano, SNR riferito a 2500 Hz come in sjtty.
bool round_trip (std::string const& text, float snr_db, unsigned seed)
{
  std::vector<Frame> frames;
  std::string canonical;
  if (pack_message (text, EXCHANGE_UNKNOWN, frames, &canonical) <= 0) return false;
  std::vector<float> const wave = generate_wave (tones_for_frames (frames), kNsps12k, kBt, 12000.0f, 1500.0f);
  float const sig = std::sqrt (2.0f * 2500.0f / 6000.0f) * std::pow (10.0f, 0.05f * snr_db);
  std::mt19937 rng (seed);
  std::normal_distribution<float> noise (0.0f, 1.0f);
  std::vector<std::int16_t> pcm (12000 + wave.size () + 3 * kChunkSamples);
  for (std::size_t i = 0; i < pcm.size (); ++i)
    {
      float x = noise (rng);
      if (i >= 12000 && i - 12000 < wave.size ()) x += sig * wave[i - 12000];
      pcm[i] = static_cast<std::int16_t> (std::lround (std::max (-32767.0f, std::min (32767.0f, 100.0f * x))));
    }
  Receiver rx;
  rx.set_parameters (200, 2800, 1500.0f, 50.0f);
  bool found = false;
  for (std::size_t off = 0; off < pcm.size (); off += 1200)
    {
      int const n = static_cast<int> (std::min<std::size_t> (1200, pcm.size () - off));
      rx.add_samples (pcm.data () + off, n);
      for (auto const& u : rx.take_updates ())
        if (u.complete && u.text == canonical && std::abs (u.frequency - 1500.0f) < 5.0f
            && std::abs (u.start_seconds - 1.0) < 0.05)
          found = true;
    }
  return found;
}

}

int main ()
{
  for (auto const& g : kGolden)
    {
      std::vector<Frame> frames;
      std::string canonical;
      int const n = pack_message (g.message, g.profile, frames, &canonical);
      check (n == g.nframes, std::string ("frame count: ") + g.message);
      check (canonical == g.canonical, std::string ("canonical text: ") + g.message);
      for (int i = 0; i < n && i < static_cast<int> (g.frames.size ()); ++i)
        {
          int payload[kPayloadBits];
          frame_to_payload (frames[static_cast<std::size_t> (i)], payload);
          std::string bits;
          for (int b : payload) bits.push_back (static_cast<char> ('0' + b));
          check (bits == g.frames[static_cast<std::size_t> (i)], std::string ("frame bits: ") + g.message);
        }
    }
  check (round_trip ("CQ IU8LMC CQ", -12.0f, 1), "round trip CQ at -12 dB");
  check (round_trip ("TU NOW JA6DEF 599 102", -12.0f, 2), "round trip exchange at -12 dB");
  check (round_trip ("THE QUICK BROWN FOX 73", -10.0f, 3), "round trip text at -10 dB");
  std::printf ("%s: %d failure(s)\n", failures ? "JTTY selftest FAILED" : "JTTY selftest ok", failures);
  return failures ? 1 : 0;
}
