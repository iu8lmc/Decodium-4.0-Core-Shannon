// Confronto bit per bit delle metriche di bit FT8/FT4/FT2 fra due build di
// Detector/FtxBitmetrics.cpp.
//
// Le due build vengono caricate come librerie separate, ricevono gli stessi
// segnali casuali (livelli su cinque decadi, SNR da -12 a +28 dB, fading lento,
// un vettore su dieci di solo rumore) e le uscite sono confrontate con memcmp.
// Serve a verificare che una modifica di prestazioni non cambi alcun valore.
// Non fa parte della build: si compila a mano.
//
//   FL="-O3 -DNDEBUG -std=gnu++17 -fcx-limited-range -fPIC -shared -I<sorgenti>"
//   g++ $FL -o libprima.so <FtxBitmetrics.cpp di riferimento> -lfftw3f
//   g++ $FL -o libdopo.so  Detector/FtxBitmetrics.cpp         -lfftw3f
//   g++ -O2 -std=gnu++17 -o ftx_bitmetrics_compare tests/tools/ftx_bitmetrics_compare.cpp -ldl
//   ./ftx_bitmetrics_compare ./libprima.so ./libdopo.so 40000 [seme]
//
// Esce con 0 se tutte le uscite coincidono, 1 altrimenti. I tempi stampati
// sono indicativi: le due librerie sono chiamate a turno sugli stessi dati.
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include <dlfcn.h>

using Complex = std::complex<float>;
using clk = std::chrono::steady_clock;

struct Api
{
  void (*ft4) (Complex const*, float*, int*);
  void (*ft2) (Complex const*, float*, int*);
  void (*ft2_accum) (Complex const*, float const*, int, float*);
  void (*ft2_diag) (Complex const*, float*, float*, float*, float*, Complex*, float*, int*, int*,
                    float*, float*, float*, float*, float*, float*);
  void (*ft8) (Complex const*, int, int, int, float, int, int, Complex const*, Complex*, float*,
               int*, float*, float*, float*, float*, float*);
};

static Api load (char const* path)
{
  void* h = dlopen (path, RTLD_NOW | RTLD_LOCAL);
  if (!h) { std::fprintf (stderr, "dlopen %s: %s\n", path, dlerror ()); std::exit (2); }
  Api a {};
  a.ft4 = reinterpret_cast<decltype (a.ft4)> (dlsym (h, "ftx_ft4_bitmetrics_c"));
  a.ft2 = reinterpret_cast<decltype (a.ft2)> (dlsym (h, "ftx_ft2_bitmetrics_c"));
  a.ft2_accum = reinterpret_cast<decltype (a.ft2_accum)> (dlsym (h, "ftx_ft2_bitmetrics_accum_c"));
  a.ft2_diag = reinterpret_cast<decltype (a.ft2_diag)> (dlsym (h, "ftx_ft2_bitmetrics_diag_c"));
  a.ft8 = reinterpret_cast<decltype (a.ft8)> (dlsym (h, "ftx_ft8_bitmetrics_capture_c"));
  if (!a.ft4 || !a.ft2 || !a.ft2_accum || !a.ft2_diag || !a.ft8)
    { std::fprintf (stderr, "missing symbol in %s\n", path); std::exit (2); }
  return a;
}

struct Stat
{
  long vectors {0}, differing {0}, values {0}, values_diff {0}, skipped {0};
  double max_abs {0.0}, max_rel {0.0};
  double t_old {0.0}, t_new {0.0};
  void compare (float const* a, float const* b, size_t n)
  {
    values += static_cast<long> (n);
    bool any = false;
    for (size_t i = 0; i < n; ++i)
      {
        if (std::memcmp (&a[i], &b[i], sizeof (float)) != 0)
          {
            any = true; ++values_diff;
            double const d = std::fabs (static_cast<double> (a[i]) - static_cast<double> (b[i]));
            max_abs = std::max (max_abs, d);
            double const m = std::max (std::fabs (static_cast<double> (a[i])), 1e-30);
            max_rel = std::max (max_rel, d / m);
          }
      }
    if (any) ++differing;
  }
  void print (char const* name) const
  {
    std::printf ("%-34s vettori=%-7ld saltati=%-6ld diversi=%-6ld valori=%-10ld valori_diversi=%-8ld"
                 " max_abs=%.3g max_rel=%.3g  t_prima=%.1f us  t_dopo=%.1f us  (%.2fx)\n",
                 name, vectors, skipped, differing, values, values_diff, max_abs, max_rel,
                 vectors ? 1e6 * t_old / vectors : 0.0, vectors ? 1e6 * t_new / vectors : 0.0,
                 t_new > 0 ? t_old / t_new : 0.0);
  }
};

static double secs (clk::time_point a) { return std::chrono::duration<double> (clk::now () - a).count (); }

// Un burst a 4 o 8 toni: simboli da 32 campioni, Costas nelle posizioni giuste,
// ampiezza, fading lento e rumore casuali.
static void make_burst (std::mt19937& rng, int tones, int nsym, Complex* out, int total, int start,
                        bool with_signal)
{
  std::normal_distribution<float> gauss (0.0f, 1.0f);
  std::uniform_real_distribution<float> uni (0.0f, 1.0f);
  // scala complessiva su molte decadi: i decoder ricevono livelli molto diversi
  float const scale = std::pow (10.0f, -2.0f + 5.0f * uni (rng));
  float const snr_db = -12.0f + 40.0f * uni (rng);
  float const amp = with_signal ? std::pow (10.0f, snr_db / 20.0f) : 0.0f;
  float const fade_depth = uni (rng) < 0.5f ? 0.0f : uni (rng);
  float const fade_rate = 0.5f + 3.0f * uni (rng);
  float const fade_phase = 6.2831853f * uni (rng);
  float const dphi_err = 0.02f * (uni (rng) - 0.5f);          // piccolo errore di frequenza
  float phase = 6.2831853f * uni (rng);
  static int const icos7[7] = {3, 1, 4, 0, 6, 5, 2};
  static int const icos4[4][4] = {{0, 1, 3, 2}, {1, 0, 2, 3}, {2, 3, 1, 0}, {3, 2, 0, 1}};
  for (int i = 0; i < total; ++i) out[i] = scale * Complex (gauss (rng), gauss (rng));
  for (int k = 0; k < nsym; ++k)
    {
      int tone = static_cast<int> (rng () % static_cast<unsigned> (tones));
      if (tones == 8)
        {
          if (k < 7) tone = icos7[k]; else if (k >= 36 && k < 43) tone = icos7[k - 36];
          else if (k >= 72) tone = icos7[k - 72];
        }
      else
        {
          if (k < 4) tone = icos4[0][k]; else if (k >= 33 && k < 37) tone = icos4[1][k - 33];
          else if (k >= 66 && k < 70) tone = icos4[2][k - 66]; else if (k >= 99) tone = icos4[3][k - 99];
        }
      for (int n = 0; n < 32; ++n)
        {
          int const idx = start + k * 32 + n;
          float const t = static_cast<float> (k * 32 + n) / static_cast<float> (nsym * 32);
          float const fade = 1.0f - fade_depth * 0.5f * (1.0f + std::sin (6.2831853f * fade_rate * t + fade_phase));
          phase += 6.2831853f * static_cast<float> (tone) / 32.0f + dphi_err;
          if (phase > 6.2831853f) phase -= 6.2831853f;
          if (idx >= 0 && idx < total)
            out[idx] += scale * amp * fade * Complex (std::cos (phase), std::sin (phase));
        }
    }
}

int main (int argc, char** argv)
{
  if (argc < 4) { std::fprintf (stderr, "usage: ftx_bitmetrics_compare before.so after.so vectors [seed]\n"); return 2; }
  Api const a = load (argv[1]);
  Api const b = load (argv[2]);
  long const n = std::atol (argv[3]);
  std::mt19937 rng (argc > 4 ? static_cast<unsigned> (std::atol (argv[4])) : 20261116u);

  constexpr int NN4 = 103, rows = 206;
  bool all_equal = true;

  { // FT4
    Stat st; std::vector<Complex> cd (NN4 * 32);
    std::vector<float> x (rows * 3), y (rows * 3);
    for (long v = 0; v < n; ++v)
      {
        make_burst (rng, 4, NN4, cd.data (), NN4 * 32, 0, v % 10 != 0);
        int bx = 0, by = 0;
        auto t = clk::now (); a.ft4 (cd.data (), x.data (), &bx); st.t_old += secs (t);
        t = clk::now (); b.ft4 (cd.data (), y.data (), &by); st.t_new += secs (t);
        if (bx != by) { ++st.differing; continue; }
        if (bx != 0) { ++st.skipped; continue; }
        ++st.vectors; st.compare (x.data (), y.data (), x.size ());
      }
    st.print ("FT4 ftx_ft4_bitmetrics_c"); all_equal = all_equal && st.differing == 0;
  }
  { // FT2 (diag: tutte le uscite intermedie)
    Stat st; std::vector<Complex> cd (NN4 * 32), eqx (NN4 * 32), eqy (NN4 * 32);
    std::vector<float> x (rows * 3), y (rows * 3), basex (rows * 3), basey (rows * 3);
    std::vector<float> rawx (rows), rawy (rows), bex (rows), bey (rows), snx (NN4), sny (NN4);
    for (long v = 0; v < n; ++v)
      {
        make_burst (rng, 4, NN4, cd.data (), NN4 * 32, 0, v % 10 != 0);
        int bx = 0, by = 0, ux = 0, uy = 0; float fx[6] {}, fy[6] {};
        auto t = clk::now ();
        a.ft2_diag (cd.data (), x.data (), basex.data (), rawx.data (), bex.data (), eqx.data (), snx.data (),
                    &bx, &ux, &fx[0], &fx[1], &fx[2], &fx[3], &fx[4], &fx[5]);
        st.t_old += secs (t); t = clk::now ();
        b.ft2_diag (cd.data (), y.data (), basey.data (), rawy.data (), bey.data (), eqy.data (), sny.data (),
                    &by, &uy, &fy[0], &fy[1], &fy[2], &fy[3], &fy[4], &fy[5]);
        st.t_new += secs (t);
        if (bx != by || ux != uy) { ++st.differing; continue; }
        if (bx != 0) { ++st.skipped; continue; }
        ++st.vectors;
        st.compare (x.data (), y.data (), x.size ());
        st.compare (basex.data (), basey.data (), basex.size ());
        st.compare (bex.data (), bey.data (), bex.size ());
        st.compare (fx, fy, 6);
      }
    st.print ("FT2 ftx_ft2_bitmetrics_diag_c"); all_equal = all_equal && st.differing == 0;
  }
  { // FT2 semplice (quello usato dal decoder)
    Stat st; std::vector<Complex> cd (NN4 * 32);
    std::vector<float> x (rows * 3), y (rows * 3);
    for (long v = 0; v < n; ++v)
      {
        make_burst (rng, 4, NN4, cd.data (), NN4 * 32, 0, v % 10 != 0);
        int bx = 0, by = 0;
        auto t = clk::now (); a.ft2 (cd.data (), x.data (), &bx); st.t_old += secs (t);
        t = clk::now (); b.ft2 (cd.data (), y.data (), &by); st.t_new += secs (t);
        if (bx != by) { ++st.differing; continue; }
        if (bx != 0) { ++st.skipped; continue; }
        ++st.vectors; st.compare (x.data (), y.data (), x.size ());
      }
    st.print ("FT2 ftx_ft2_bitmetrics_c"); all_equal = all_equal && st.differing == 0;
  }
  { // FT2 accumulo su piu' slot
    Stat st; std::vector<float> x (rows * 3), y (rows * 3);
    std::normal_distribution<float> gauss (0.0f, 1.0f);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);
    for (long v = 0; v < n; ++v)
      {
        int const nslot = 1 + static_cast<int> (rng () % 4u);
        std::vector<Complex> cs (static_cast<size_t> (nslot) * NN4 * 4);
        std::vector<float> beta (static_cast<size_t> (nslot));
        for (int s = 0; s < nslot; ++s)
          {
            float const scale = std::pow (10.0f, -1.0f + 3.0f * uni (rng));
            float const amp = std::pow (10.0f, (-12.0f + 36.0f * uni (rng)) / 20.0f) * 5.6f;
            beta[static_cast<size_t> (s)] = std::min (50.0f, std::max (0.01f, 0.5f / (32.0f * scale * scale * (0.5f + uni (rng)))));
            for (int k = 0; k < NN4; ++k)
              {
                int const tone = static_cast<int> (rng () % 4u);
                Complex const ph = std::polar (1.0f, 6.2831853f * uni (rng));
                for (int j = 0; j < 4; ++j)
                  cs[static_cast<size_t> (s) * NN4 * 4 + static_cast<size_t> (k * 4 + j)] =
                      scale * (5.6f * Complex (gauss (rng), gauss (rng)) + (j == tone ? amp * 5.6f * ph : Complex {}));
              }
          }
        auto t = clk::now (); a.ft2_accum (cs.data (), beta.data (), nslot, x.data ()); st.t_old += secs (t);
        t = clk::now (); b.ft2_accum (cs.data (), beta.data (), nslot, y.data ()); st.t_new += secs (t);
        ++st.vectors; st.compare (x.data (), y.data (), x.size ());
      }
    st.print ("FT2 ftx_ft2_bitmetrics_accum_c"); all_equal = all_equal && st.differing == 0;
  }
  { // FT8: tutte le metriche, con e senza deep/equalizzazione/storico
    constexpr int np2 = 2812, size = 3200, NN8 = 79;
    std::vector<Complex> cd (size), hist (NN8 * 8), csx (NN8 * 8), csy (NN8 * 8);
    std::normal_distribution<float> gauss (0.0f, 1.0f);
    for (int imetric = 1; imetric <= 4; ++imetric)
      for (int deep = 0; deep <= 1; ++deep)
        for (int eq = 0; eq <= 1; ++eq)
          for (int use_hist = 0; use_hist <= 1; ++use_hist)
            {
              Stat st;
              float sx[8 * NN8], sy[8 * NN8], lx[5][174], ly[5][174];
              long const m = std::max (1L, n / 8);
              for (long v = 0; v < m; ++v)
                {
                  int const ibest = static_cast<int> (rng () % 280u);
                  make_burst (rng, 8, NN8, cd.data (), size, ibest, v % 10 != 0);
                  for (auto& h : hist) h = Complex (std::fabs (gauss (rng)) * 3.0f, 0.0f);
                  int nx = 0, ny = 0;
                  auto t = clk::now ();
                  a.ft8 (cd.data (), np2, ibest, imetric, 3.2f, deep, eq, use_hist ? hist.data () : nullptr,
                         use_hist ? nullptr : csx.data (), sx, &nx, lx[0], lx[1], lx[2], lx[3], lx[4]);
                  st.t_old += secs (t); t = clk::now ();
                  b.ft8 (cd.data (), np2, ibest, imetric, 3.2f, deep, eq, use_hist ? hist.data () : nullptr,
                         use_hist ? nullptr : csy.data (), sy, &ny, ly[0], ly[1], ly[2], ly[3], ly[4]);
                  st.t_new += secs (t);
                  ++st.vectors;
                  if (nx != ny) { ++st.differing; continue; }
                  st.compare (sx, sy, 8 * NN8);
                  for (int q = 0; q < 5; ++q) st.compare (lx[q], ly[q], 174);
                  if (!use_hist) st.compare (reinterpret_cast<float*> (csx.data ()), reinterpret_cast<float*> (csy.data ()), 2 * 8 * NN8);
                }
              char name[80];
              std::snprintf (name, sizeof name, "FT8 metrica=%d deep=%d eq=%d storico=%d", imetric, deep, eq, use_hist);
              st.print (name); all_equal = all_equal && st.differing == 0;
            }
  }
  std::printf ("%s\n", all_equal ? "TUTTO IDENTICO" : "DIFFERENZE TROVATE");
  return all_equal ? 0 : 1;
}
