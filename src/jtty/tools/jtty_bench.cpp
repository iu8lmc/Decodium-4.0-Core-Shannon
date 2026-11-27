// Banco del port JTTY: stessi comandi e stesse uscite dei programmi Fortran
// di WSJT-X (sjtty, rjtty), cosi' le due implementazioni si confrontano riga
// per riga sugli stessi file WAV.
//
//   jtty_bench pack  [--exchange-profile=unknown|field-day|rtty-roundup] "messaggio"
//   jtty_bench tones [--exchange-profile=...] "messaggio"
//   jtty_bench wave  "messaggio" f0 file.wav        (segnale pulito, 12 kHz)
//   jtty_bench decode smin f0 ftol file.wav [...]   (come rjtty smin 1 384 f0 ftol)
//   jtty_bench stream f0 ftol file.wav [...]        (ricevitore a flusso, blocchi da 0,1 s)

#include "JttyCodec.hpp"
#include "JttyDecoder.hpp"
#include "JttyTbcc.hpp"
#include "JttyWave.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace decodium::jtty;

namespace
{

bool read_wav (char const* path, std::vector<std::int16_t>& samples)
{
  std::ifstream f (path, std::ios::binary);
  if (!f) return false;
  std::vector<char> data ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char> ());
  if (data.size () < 44) return false;
  // Si cerca il blocco "data" invece di assumere un'intestazione di 44 byte.
  std::size_t pos = 12;
  while (pos + 8 <= data.size ())
    {
      std::uint32_t size;
      std::memcpy (&size, &data[pos + 4], 4);
      if (std::memcmp (&data[pos], "data", 4) == 0)
        {
          std::size_t const n = std::min<std::size_t> (size, data.size () - pos - 8) / 2;
          samples.resize (n);
          std::memcpy (samples.data (), &data[pos + 8], n * 2);
          return true;
        }
      pos += 8 + size + (size & 1);
    }
  return false;
}

bool write_wav (char const* path, std::vector<std::int16_t> const& samples)
{
  std::ofstream f (path, std::ios::binary);
  if (!f) return false;
  std::uint32_t const bytes = static_cast<std::uint32_t> (samples.size () * 2);
  auto u32 = [&] (std::uint32_t v) { f.write (reinterpret_cast<char const*> (&v), 4); };
  auto u16 = [&] (std::uint16_t v) { f.write (reinterpret_cast<char const*> (&v), 2); };
  f.write ("RIFF", 4);
  u32 (36 + bytes);
  f.write ("WAVEfmt ", 8);
  u32 (16);
  u16 (1);
  u16 (1);
  u32 (12000);
  u32 (24000);
  u16 (2);
  u16 (16);
  f.write ("data", 4);
  u32 (bytes);
  f.write (reinterpret_cast<char const*> (samples.data ()), bytes);
  return true;
}

int parse_profile (int& argi, int argc, char** argv)
{
  int profile = EXCHANGE_UNKNOWN;
  if (argi < argc && std::strncmp (argv[argi], "--exchange-profile=", 19) == 0)
    {
      std::string const v = argv[argi] + 19;
      if (v == "unknown") profile = EXCHANGE_UNKNOWN;
      else if (v == "field-day") profile = EXCHANGE_FIELD_DAY;
      else if (v == "rtty-roundup") profile = EXCHANGE_RTTY;
      else
        {
          std::fprintf (stderr, "Invalid exchange profile\n");
          std::exit (1);
        }
      ++argi;
    }
  return profile;
}

}

int main (int argc, char** argv)
{
  if (argc < 2)
    {
      std::fprintf (stderr, "usage: jtty_bench pack|tones|wave|decode|stream ...\n");
      return 2;
    }
  std::string const cmd = argv[1];
  int argi = 2;

  if (cmd == "pack" || cmd == "tones")
    {
      int const profile = parse_profile (argi, argc, argv);
      if (argi >= argc) return 2;
      std::vector<Frame> frames;
      std::string canonical;
      int const nframes = pack_message (argv[argi], profile, frames, &canonical);
      if (nframes < 0)
        {
          std::printf ("Message exceeds JTTY encoding limits after exchange normalization\n");
          return 1;
        }
      UnpackResult const u = unpack_frames (frames.data (), nframes);
      std::string shown = rtrim (u.message);
      for (auto& c : shown)
        if (c == '~') c = ' ';
      std::printf ("Message after pack/unpack : %s\n", shown.c_str ());
      std::printf ("%4d frames\n", nframes);
      for (Frame f : frames)
        {
          int payload[34];
          frame_to_payload (f, payload);
          for (int b : payload) std::printf ("%d", b);
          std::printf ("\n");
        }
      if (cmd == "tones")
        {
          std::vector<int> const tones = tones_for_frames (frames);
          for (std::size_t i = 0; i < tones.size (); ++i)
            std::printf ("%2d%s", tones[i], (i % 30 == 29 || i + 1 == tones.size ()) ? "\n" : "");
        }
      return 0;
    }

  if (cmd == "packlines")
    {
      // Stesso formato di fpack.f90: "profilo|messaggio" per riga.
      char buf[512];
      while (std::fgets (buf, sizeof buf, stdin))
        {
          std::string line = buf;
          while (!line.empty () && (line.back () == '\n' || line.back () == '\r')) line.pop_back ();
          std::size_t const bar = line.find ('|');
          if (bar == std::string::npos) continue;
          int const profile = std::atoi (line.substr (0, bar).c_str ());
          std::vector<Frame> frames;
          std::string canonical;
          int const nframes = pack_message (line.substr (bar + 1), profile, frames, &canonical);
          std::printf ("N%3d C[%s]\n", nframes, canonical.c_str ());
          if (nframes > 0)
            {
              for (Frame f : frames)
                {
                  int payload[34];
                  frame_to_payload (f, payload);
                  for (int b : payload) std::printf ("%d", b);
                  std::printf ("\n");
                }
              UnpackResult const u = unpack_frames (frames.data (), nframes);
              std::printf ("U[%s] %s %s\n", rtrim (u.message).c_str (), u.is_last_frame ? "T" : "F",
                           u.source_valid ? "T" : "F");
            }
        }
      return 0;
    }

  if (cmd == "wave")
    {
      if (argc < 5) return 2;
      std::vector<Frame> frames;
      if (pack_message (argv[2], EXCHANGE_UNKNOWN, frames) <= 0) return 1;
      std::vector<int> const tones = tones_for_frames (frames);
      std::vector<float> const wave = generate_wave (tones, kNsps12k, kBt, 12000.0f,
                                                     static_cast<float> (std::atof (argv[3])));
      std::vector<std::int16_t> pcm (wave.size () + 12000, 0);
      for (std::size_t i = 0; i < wave.size (); ++i)
        pcm[i + 6000] = static_cast<std::int16_t> (std::lround (wave[i] * 10000.0f));
      return write_wav (argv[4], pcm) ? 0 : 1;
    }

  if (cmd == "decode")
    {
      if (argc < 6) return 2;
      float const smin = static_cast<float> (std::atof (argv[2]));
      float const f0 = static_cast<float> (std::atof (argv[3]));
      float const ftol = static_cast<float> (std::atof (argv[4]));
      Receiver rx;
      rx.set_parameters (200, 2800, f0, ftol, smin);
      rx.set_debug_sink ([] (std::string const& line) { std::printf ("%s\n", line.c_str ()); });
      for (int i = 5; i < argc; ++i)
        {
          std::vector<std::int16_t> samples;
          if (!read_wav (argv[i], samples))
            {
              std::fprintf (stderr, "cannot read %s\n", argv[i]);
              continue;
            }
          std::string const name = argv[i];
          std::printf ("%s\n", name.substr (name.size () > 17 ? name.size () - 17 : 0).c_str ());
          rx.decode_buffer (samples.data (), static_cast<int> (samples.size ()));
        }
      return 0;
    }

  if (cmd == "stream")
    {
      if (argc < 5) return 2;
      float const f0 = static_cast<float> (std::atof (argv[2]));
      float const ftol = static_cast<float> (std::atof (argv[3]));
      for (int i = 4; i < argc; ++i)
        {
          std::vector<std::int16_t> samples;
          if (!read_wav (argv[i], samples)) continue;
          Receiver rx;
          rx.set_parameters (200, 2800, f0, ftol);
          std::printf ("%s\n", argv[i]);
          for (std::size_t off = 0; off < samples.size (); off += 1200)
            {
              int const n = static_cast<int> (std::min<std::size_t> (1200, samples.size () - off));
              rx.add_samples (samples.data () + off, n);
              for (auto const& u : rx.take_updates ())
                std::printf ("  id=%lld f=%7.1f t=%8.3f %s |%s|\n", static_cast<long long> (u.message_id),
                             u.frequency, u.start_seconds, u.complete ? "EOM" : "...", u.text.c_str ());
            }
          // Una coda di silenzio per chiudere l'ultima finestra.
          std::vector<std::int16_t> tail (kChunkSamples + kStepSamples, 0);
          rx.add_samples (tail.data (), static_cast<int> (tail.size ()));
          for (auto const& u : rx.take_updates ())
            std::printf ("  id=%lld f=%7.1f t=%8.3f %s |%s|\n", static_cast<long long> (u.message_id),
                         u.frequency, u.start_seconds, u.complete ? "EOM" : "...", u.text.c_str ());
        }
      return 0;
    }

  std::fprintf (stderr, "unknown command %s\n", cmd.c_str ());
  return 2;
}
