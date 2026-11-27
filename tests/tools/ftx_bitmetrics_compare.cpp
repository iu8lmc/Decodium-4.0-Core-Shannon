// Confronto bit per bit delle metriche di bit FT8/FT4/FT2 fra due build di
// Detector/FtxBitmetrics.cpp.
//
// Le due build vengono caricate come librerie separate, ricevono gli stessi
// segnali casuali (livelli su cinque decadi, SNR da -12 a +28 dB, fading lento,
// un vettore su dieci di solo rumore) e le uscite sono confrontate con memcmp.
// Serve a verificare che una modifica di prestazioni non cambi alcun valore.
// Non fa parte della build: si compila a mano.
//
// Sono chiamati tutti i punti d'ingresso extern "C" del file, uno per sezione:
// un'opzione di compilazione vale per l'intera unita' e il compilatore tiene
// copie separate dello stesso calcolo (run_ft8_bitmetrics ha un clone per i
// punti d'ingresso senza cattura), quindi provarne uno non dice nulla sugli
// altri. L'ultima sezione ripassa tutto su ingressi degeneri (silenzio, un solo
// campione, portante pura, livelli sotto la precisione), dove lavorano le
// guardie sulle divisioni.
//
//   FL="-O3 -DNDEBUG -std=gnu++17 -fcx-limited-range -fPIC -shared -I<sorgenti>"
//   g++ $FL -o libprima.so <FtxBitmetrics.cpp di riferimento> -lfftw3f
//   g++ $FL -o libdopo.so  Detector/FtxBitmetrics.cpp         -lfftw3f
//   (su x86-64 con GCC il progetto compila il file anche con -fno-trapping-math:
//    aggiungerla alla riga di libdopo.so per provare la build di produzione)
//   g++ -O2 -std=gnu++17 -o ftx_bitmetrics_compare tests/tools/ftx_bitmetrics_compare.cpp -ldl
//   ./ftx_bitmetrics_compare ./libprima.so ./libdopo.so 40000 [seme]
//
// Esce con 0 se tutte le uscite coincidono, 1 altrimenti. I tempi stampati
// sono indicativi: le due librerie sono chiamate a turno sugli stessi dati.
//
// Differenze attese rispetto a origin/main prima della correzione del
// trabocco in ft2_legacy_base_noise_var / ft2_legacy_equalized_beta: le
// sezioni FT2 differiscono (e il programma esce con 1) nei vettori in cui il
// vecchio codice traboccava un int, cioe' ai livelli estremi del generatore
// (circa 80-90 vettori su 3900 per ftx_ft2_bitmetrics_c con 4000 vettori).
// Le sezioni FT8 e FT4 devono restare identiche. Il comportamento nuovo e'
// fissato da tests/test_ft2_bitmetrics_overflow.cpp.
#include <algorithm>
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
  void (*ft4_ref) (Complex const*, float*, int*);
  void (*ft2_mags) (Complex const*, float*);
  void (*ft2_coherent) (Complex const*, float*);
  void (*ft2_spectra) (Complex const*, Complex*, float*);
  void (*ft2_chest) (Complex const*, Complex*, float*);
  void (*ft8_coherent) (Complex const*, float, float*, float*, float*, float*, float*);
  void (*ft8_bicm) (Complex const*, float, float const*, float*, float*, float*, float*, float*);
  void (*ft8_a8) (Complex const*, int, float, int const*, float*, int*, float*);
  void (*ft8_plain) (Complex const*, int, int, int, float*, int*, float*, float*, float*, float*, float*);
  // scaled, deep, equalized, deep_equalized: stessa firma
  using Ft8Scaled = void (*) (Complex const*, int, int, int, float, float*, int*,
                              float*, float*, float*, float*, float*);
  Ft8Scaled ft8_scaled[4];
};

static char const* const ft8_wrapper_names[5] = {
  "ftx_ft8_bitmetrics_c", "ftx_ft8_bitmetrics_scaled_c", "ftx_ft8_bitmetrics_deep_c",
  "ftx_ft8_bitmetrics_equalized_c", "ftx_ft8_bitmetrics_deep_equalized_c"};

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
  a.ft4_ref = reinterpret_cast<decltype (a.ft4_ref)> (dlsym (h, "ftx_ft4_bitmetrics_ref_c"));
  a.ft2_mags = reinterpret_cast<decltype (a.ft2_mags)> (dlsym (h, "ftx_ft2_symbol_mags_c"));
  a.ft2_coherent = reinterpret_cast<decltype (a.ft2_coherent)> (dlsym (h, "ftx_ft2_bitmetrics_coherent_c"));
  a.ft2_spectra = reinterpret_cast<decltype (a.ft2_spectra)> (dlsym (h, "ftx_ft2_symbol_spectra_c"));
  a.ft2_chest = reinterpret_cast<decltype (a.ft2_chest)> (dlsym (h, "ftx_ft2_channel_est_c"));
  a.ft8_coherent = reinterpret_cast<decltype (a.ft8_coherent)> (dlsym (h, "ftx_ft8_bitmetrics_coherent_c"));
  a.ft8_bicm = reinterpret_cast<decltype (a.ft8_bicm)> (dlsym (h, "ftx_ft8_bitmetrics_bicm_c"));
  a.ft8_a8 = reinterpret_cast<decltype (a.ft8_a8)> (dlsym (h, "ftx_ft8_a8_score_c"));
  a.ft8_plain = reinterpret_cast<decltype (a.ft8_plain)> (dlsym (h, ft8_wrapper_names[0]));
  for (int w = 0; w < 4; ++w)
    a.ft8_scaled[w] = reinterpret_cast<Api::Ft8Scaled> (dlsym (h, ft8_wrapper_names[w + 1]));
  if (!a.ft4_ref || !a.ft2_mags || !a.ft2_coherent || !a.ft2_spectra || !a.ft2_chest || !a.ft8_coherent
      || !a.ft8_bicm || !a.ft8_a8 || !a.ft8_plain || !a.ft8_scaled[0] || !a.ft8_scaled[1]
      || !a.ft8_scaled[2] || !a.ft8_scaled[3])
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

// Riempie le due uscite con valori diversi fra loro: se una funzione non scrive
// un elemento il confronto lo segnala, invece di ritrovare il valore del giro
// precedente uguale da entrambe le parti.
static void poison (float* x, float* y, size_t n)
{
  std::fill_n (x, n, 1.0e30f);
  std::fill_n (y, n, -1.0e30f);
}

// Un burst a 4 o 8 toni: simboli da 32 campioni, Costas nelle posizioni giuste,
// ampiezza, fading lento e rumore casuali. tones_out, se dato, riceve il tono
// trasmesso in ciascun simbolo.
static void make_burst (std::mt19937& rng, int tones, int nsym, Complex* out, int total, int start,
                        bool with_signal, int* tones_out = nullptr)
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
      if (tones_out) tones_out[k] = tone;
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

// Tutte le uscite di tutti i punti d'ingresso su un ingresso dato, in fila in
// un solo vettore (gli interi convertiti in float). cd4: 103 simboli FT4/FT2,
// cd8: 3200 campioni FT8. Usata per gli ingressi degeneri.
static void run_all (Api const& a, Complex const* cd4, Complex const* cd8, int ibest, int imetric,
                     int deep, int eq, std::vector<float>& out)
{
  constexpr int NN4 = 103, rows = 206, NN8 = 79, np2 = 2812, size = 3200;
  static constexpr float unset = -7.25e-7f;   // riconoscibile: nessuna uscita lo produce per caso
  out.clear ();
  auto put = [&out] (float const* p, size_t n) { out.insert (out.end (), p, p + n); };
  auto put_c = [&put] (std::vector<Complex> const& v) { put (reinterpret_cast<float const*> (v.data ()), 2 * v.size ()); };
  auto put_v = [&put] (std::vector<float> const& v) { put (v.data (), v.size ()); };
  auto put_i = [&out] (int v) { out.push_back (static_cast<float> (v)); };
  auto reset = [] (std::vector<float>& v) { std::fill (v.begin (), v.end (), unset); };
  auto reset_c = [] (std::vector<Complex>& v) { std::fill (v.begin (), v.end (), Complex (unset, unset)); };

  std::vector<float> bm (rows * 3), base (rows * 3), raw (rows), be (rows), sn (NN4), mags (NN4 * 4), coer (rows);
  std::vector<Complex> eqd (NN4 * 32), sp (NN4 * 4);
  int bad = -1, use = -1;
  reset (bm); a.ft4 (cd4, bm.data (), &bad); put_v (bm); put_i (bad);
  reset (bm); bad = -1; a.ft4_ref (cd4, bm.data (), &bad); put_v (bm); put_i (bad);
  reset (bm); bad = -1; a.ft2 (cd4, bm.data (), &bad); put_v (bm); put_i (bad);
  float f[6]; std::fill_n (f, 6, unset);
  reset (bm); reset (base); reset (raw); reset (be); reset (sn); reset_c (eqd); bad = -1;
  a.ft2_diag (cd4, bm.data (), base.data (), raw.data (), be.data (), eqd.data (), sn.data (), &bad, &use,
              &f[0], &f[1], &f[2], &f[3], &f[4], &f[5]);
  put_v (bm); put_v (base); put_v (raw); put_v (be); put_c (eqd); put_v (sn); put_i (bad); put_i (use); put (f, 6);
  reset (mags); a.ft2_mags (cd4, mags.data ()); put_v (mags);
  reset (coer); a.ft2_coherent (cd4, coer.data ()); put_v (coer);
  float beta = unset;
  reset_c (sp); a.ft2_spectra (cd4, sp.data (), &beta); put_c (sp); put (&beta, 1);
  reset (bm); a.ft2_accum (sp.data (), &beta, 1, bm.data ()); put_v (bm);
  reset (sn); reset_c (eqd); a.ft2_chest (cd4, eqd.data (), sn.data ()); put_c (eqd); put_v (sn);

  constexpr float scale = 2.83f;
  std::vector<Complex> cs (NN8 * 8);
  std::vector<float> s8 (8 * NN8);
  float l[5][174];
  int nsync = -1;
  auto reset_l = [&l] { for (auto& row : l) std::fill_n (row, 174, unset); };
  auto put_l = [&l, &put] { for (auto const& row : l) put (row, 174); };
  reset (s8); reset_l (); reset_c (cs);
  a.ft8 (cd8, np2, ibest, imetric, scale, deep, eq, nullptr, cs.data (), s8.data (), &nsync, l[0], l[1], l[2], l[3], l[4]);
  put_v (s8); put_l (); put_c (cs); put_i (nsync);
  float la[174];
  for (int i = 0; i < 174; ++i) la[i] = static_cast<float> (i % 5 - 2);
  reset_l (); a.ft8_coherent (cs.data (), scale, l[0], l[1], l[2], l[3], l[4]); put_l ();
  reset_l (); a.ft8_bicm (cs.data (), scale, nullptr, l[0], l[1], l[2], l[3], l[4]); put_l ();
  reset_l (); a.ft8_bicm (cs.data (), scale, la, l[0], l[1], l[2], l[3], l[4]); put_l ();
  // storico: gli spettri appena catturati, come se lo stesso segnale fosse gia' stato visto
  reset (s8); reset_l (); nsync = -1;
  a.ft8 (cd8, np2, ibest, imetric, scale, deep, eq, cs.data (), nullptr, s8.data (), &nsync, l[0], l[1], l[2], l[3], l[4]);
  put_v (s8); put_l (); put_i (nsync);
  for (int w = 0; w < 5; ++w)
    {
      reset (s8); reset_l (); nsync = -1;
      if (w == 0) a.ft8_plain (cd8, np2, ibest, imetric, s8.data (), &nsync, l[0], l[1], l[2], l[3], l[4]);
      else a.ft8_scaled[w - 1] (cd8, np2, ibest, imetric, scale, s8.data (), &nsync, l[0], l[1], l[2], l[3], l[4]);
      put_v (s8); put_l (); put_i (nsync);
    }
  int itone[NN8];
  for (int k = 0; k < NN8; ++k) itone[k] = k % 8;
  float plog = unset, sigobig = unset; int nhard = -1;
  a.ft8_a8 (cd8, size, static_cast<float> (ibest - 100) / 200.0f, itone, &plog, &nhard, &sigobig);
  put (&plog, 1); put_i (nhard); put (&sigobig, 1);
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
        st.compare (rawx.data (), rawy.data (), rawx.size ());
        st.compare (snx.data (), sny.data (), snx.size ());
        st.compare (reinterpret_cast<float*> (eqx.data ()), reinterpret_cast<float*> (eqy.data ()), 2 * eqx.size ());
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
  { // FT4, punto d'ingresso di riferimento: stesso calcolo, simbolo a parte
    Stat st; std::vector<Complex> cd (NN4 * 32);
    std::vector<float> x (rows * 3), y (rows * 3);
    for (long v = 0; v < n; ++v)
      {
        make_burst (rng, 4, NN4, cd.data (), NN4 * 32, 0, v % 10 != 0);
        int bx = -1, by = -2;
        poison (x.data (), y.data (), x.size ());
        auto t = clk::now (); a.ft4_ref (cd.data (), x.data (), &bx); st.t_old += secs (t);
        t = clk::now (); b.ft4_ref (cd.data (), y.data (), &by); st.t_new += secs (t);
        if (bx != by) { ++st.differing; continue; }
        if (bx != 0) { ++st.skipped; continue; }
        ++st.vectors; st.compare (x.data (), y.data (), x.size ());
      }
    st.print ("FT4 ftx_ft4_bitmetrics_ref_c"); all_equal = all_equal && st.differing == 0;
  }
  { // FT2: moduli dei quattro toni di ogni simbolo (conferma del messaggio atteso in Stage7)
    Stat st; std::vector<Complex> cd (NN4 * 32);
    std::vector<float> x (NN4 * 4), y (NN4 * 4);
    for (long v = 0; v < n; ++v)
      {
        make_burst (rng, 4, NN4, cd.data (), NN4 * 32, 0, v % 10 != 0);
        poison (x.data (), y.data (), x.size ());
        auto t = clk::now (); a.ft2_mags (cd.data (), x.data ()); st.t_old += secs (t);
        t = clk::now (); b.ft2_mags (cd.data (), y.data ()); st.t_new += secs (t);
        ++st.vectors; st.compare (x.data (), y.data (), x.size ());
      }
    st.print ("FT2 ftx_ft2_symbol_mags_c"); all_equal = all_equal && st.differing == 0;
  }
  { // FT2: metriche coerenti, 206 righe
    Stat st; std::vector<Complex> cd (NN4 * 32);
    std::vector<float> x (rows), y (rows);
    for (long v = 0; v < n; ++v)
      {
        make_burst (rng, 4, NN4, cd.data (), NN4 * 32, 0, v % 10 != 0);
        poison (x.data (), y.data (), x.size ());
        auto t = clk::now (); a.ft2_coherent (cd.data (), x.data ()); st.t_old += secs (t);
        t = clk::now (); b.ft2_coherent (cd.data (), y.data ()); st.t_new += secs (t);
        ++st.vectors; st.compare (x.data (), y.data (), x.size ());
      }
    st.print ("FT2 ftx_ft2_bitmetrics_coherent_c"); all_equal = all_equal && st.differing == 0;
  }
  { // FT2: spettri di simbolo e beta di uno slot; poi, come Stage7, l'accumulo
    // degli slot raccolti (qui da 1 a 4, ciascuno col proprio livello e rumore)
    Stat st, st_acc;
    std::vector<Complex> cd (NN4 * 32), csx (4 * NN4 * 4), csy (4 * NN4 * 4);
    std::vector<float> x (rows * 3), y (rows * 3);
    for (long v = 0; v < n; ++v)
      {
        int const nslot = 1 + static_cast<int> (rng () % 4u);
        float betax[4], betay[4];
        for (int s = 0; s < nslot; ++s)
          {
            make_burst (rng, 4, NN4, cd.data (), NN4 * 32, 0, v % 10 != 0);
            Complex* const px = csx.data () + s * NN4 * 4;
            Complex* const py = csy.data () + s * NN4 * 4;
            poison (reinterpret_cast<float*> (px), reinterpret_cast<float*> (py), 2 * NN4 * 4);
            poison (&betax[s], &betay[s], 1);
            auto t = clk::now (); a.ft2_spectra (cd.data (), px, &betax[s]); st.t_old += secs (t);
            t = clk::now (); b.ft2_spectra (cd.data (), py, &betay[s]); st.t_new += secs (t);
            ++st.vectors;
            st.compare (reinterpret_cast<float*> (px), reinterpret_cast<float*> (py), 2 * NN4 * 4);
            st.compare (&betax[s], &betay[s], 1);
          }
        // stessi spettri alle due librerie: la differenza, se c'e', e' dell'accumulo
        poison (x.data (), y.data (), x.size ());
        auto t = clk::now (); a.ft2_accum (csx.data (), betax, nslot, x.data ()); st_acc.t_old += secs (t);
        t = clk::now (); b.ft2_accum (csx.data (), betax, nslot, y.data ()); st_acc.t_new += secs (t);
        ++st_acc.vectors; st_acc.compare (x.data (), y.data (), x.size ());
      }
    st.print ("FT2 ftx_ft2_symbol_spectra_c"); all_equal = all_equal && st.differing == 0;
    st_acc.print ("FT2 spettri veri -> accum_c"); all_equal = all_equal && st_acc.differing == 0;
  }
  { // FT2: stima di canale ed equalizzazione
    Stat st; std::vector<Complex> cd (NN4 * 32), eqx (NN4 * 32), eqy (NN4 * 32);
    std::vector<float> snx (NN4), sny (NN4);
    for (long v = 0; v < n; ++v)
      {
        make_burst (rng, 4, NN4, cd.data (), NN4 * 32, 0, v % 10 != 0);
        poison (reinterpret_cast<float*> (eqx.data ()), reinterpret_cast<float*> (eqy.data ()), 2 * eqx.size ());
        poison (snx.data (), sny.data (), snx.size ());
        auto t = clk::now (); a.ft2_chest (cd.data (), eqx.data (), snx.data ()); st.t_old += secs (t);
        t = clk::now (); b.ft2_chest (cd.data (), eqy.data (), sny.data ()); st.t_new += secs (t);
        ++st.vectors;
        st.compare (reinterpret_cast<float*> (eqx.data ()), reinterpret_cast<float*> (eqy.data ()), 2 * eqx.size ());
        st.compare (snx.data (), sny.data (), snx.size ());
      }
    st.print ("FT2 ftx_ft2_channel_est_c"); all_equal = all_equal && st.differing == 0;
  }
  { // FT8: metriche coerenti e BICM-ID sugli spettri catturati, come in Stage4.
    // Gli spettri sono quelli della prima libreria e vanno a entrambe: la
    // cattura e' gia' confrontata sopra, qui si isola la funzione in prova.
    constexpr int np2 = 2812, size = 3200, NN8 = 79;
    constexpr float scale = 2.83f;                              // kFt8BitMetricScale
    Stat st_coer, st_null, st_giro1, st_giro2;
    std::vector<Complex> cd (size), cs (NN8 * 8);
    std::normal_distribution<float> gauss (0.0f, 1.0f);
    for (long v = 0; v < n; ++v)
      {
        int const ibest = static_cast<int> (rng () % 280u);
        int const imetric = 1 + static_cast<int> (rng () % 4u);
        int const deep = static_cast<int> (rng () % 2u), eq = static_cast<int> (rng () % 2u);
        make_burst (rng, 8, NN8, cd.data (), size, ibest, v % 10 != 0);
        float s8[8 * NN8], prod[5][174], lx[5][174], ly[5][174], la[174];
        int nsync = 0;
        a.ft8 (cd.data (), np2, ibest, imetric, scale, deep, eq, nullptr, cs.data (), s8, &nsync,
               prod[0], prod[1], prod[2], prod[3], prod[4]);

        for (int q = 0; q < 5; ++q) poison (lx[q], ly[q], 174);
        auto t = clk::now (); a.ft8_coherent (cs.data (), scale, lx[0], lx[1], lx[2], lx[3], lx[4]); st_coer.t_old += secs (t);
        t = clk::now (); b.ft8_coherent (cs.data (), scale, ly[0], ly[1], ly[2], ly[3], ly[4]); st_coer.t_new += secs (t);
        ++st_coer.vectors;
        for (int q = 0; q < 5; ++q) st_coer.compare (lx[q], ly[q], 174);

        // senza a priori
        for (int q = 0; q < 5; ++q) poison (lx[q], ly[q], 174);
        t = clk::now (); a.ft8_bicm (cs.data (), scale, nullptr, lx[0], lx[1], lx[2], lx[3], lx[4]); st_null.t_old += secs (t);
        t = clk::now (); b.ft8_bicm (cs.data (), scale, nullptr, ly[0], ly[1], ly[2], ly[3], ly[4]); st_null.t_new += secs (t);
        ++st_null.vectors;
        for (int q = 0; q < 5; ++q) st_null.compare (lx[q], ly[q], 174);

        // Due giri come con DECODIUM_FT8_BICM=2. L'estrinseca vera viene dal
        // decodificatore (superldpc_extrinsic174_91_c), che qui non c'e': se ne
        // imita la forma, cioe' correlata con le metriche da cui parte, rumorosa
        // e tagliata a +-2. Un vettore su otto e' tutto a zero, che e' cio' che
        // arriva quando il decodificatore non ha posteriori da dare.
        float const* sonda = prod[0];
        for (int giro = 0; giro < 2; ++giro)
          {
            Stat& st = giro == 0 ? st_giro1 : st_giro2;
            for (int i = 0; i < 174; ++i)
              la[i] = v % 8 == 5 ? 0.0f : std::max (-2.0f, std::min (2.0f, 0.5f * sonda[i] + 1.2f * gauss (rng)));
            for (int q = 0; q < 5; ++q) poison (lx[q], ly[q], 174);
            t = clk::now (); a.ft8_bicm (cs.data (), scale, la, lx[0], lx[1], lx[2], lx[3], lx[4]); st.t_old += secs (t);
            t = clk::now (); b.ft8_bicm (cs.data (), scale, la, ly[0], ly[1], ly[2], ly[3], ly[4]); st.t_new += secs (t);
            ++st.vectors;
            for (int q = 0; q < 5; ++q) st.compare (lx[q], ly[q], 174);
            std::copy_n (lx[0], 174, prod[0]);                  // il giro dopo riparte da queste
          }
      }
    st_coer.print ("FT8 ftx_ft8_bitmetrics_coherent_c"); all_equal = all_equal && st_coer.differing == 0;
    st_null.print ("FT8 ftx_ft8_bitmetrics_bicm_c senza la"); all_equal = all_equal && st_null.differing == 0;
    st_giro1.print ("FT8 ftx_ft8_bitmetrics_bicm_c giro1"); all_equal = all_equal && st_giro1.differing == 0;
    st_giro2.print ("FT8 ftx_ft8_bitmetrics_bicm_c giro2"); all_equal = all_equal && st_giro2.differing == 0;
  }
  { // FT8: punteggio a8 di un messaggio ipotizzato. Come nel chiamante, tbest e'
    // un ritardo fra -208 e +208 campioni a 200 Hz e il segnale comincia 100
    // campioni dopo; con i ritardi piu' negativi i primi simboli cadono fuori.
    constexpr int size = 3200, NN8 = 79;
    Stat st; std::vector<Complex> cd (size);
    for (long v = 0; v < n; ++v)
      {
        int const lag = static_cast<int> (rng () % 417u) - 208;
        int itone[NN8];
        make_burst (rng, 8, NN8, cd.data (), size, lag + 100, v % 10 != 0, itone);
        // l'ipotesi e' il messaggio trasmesso; una su quattro ha qualche tono
        // sbagliato, una su otto e' un altro messaggio
        if (v % 4 == 1) for (int e = 0; e < 6; ++e) itone[rng () % 79u] = static_cast<int> (rng () % 8u);
        if (v % 8 == 3) for (int k = 7; k < 72; ++k) if (k < 36 || k > 42) itone[k] = static_cast<int> (rng () % 8u);
        float const tbest = static_cast<float> (lag) * (1.0f / 200.0f);
        float ox[2], oy[2]; int hx = -1, hy = -2;
        poison (ox, oy, 2);
        auto t = clk::now (); a.ft8_a8 (cd.data (), size, tbest, itone, &ox[0], &hx, &ox[1]); st.t_old += secs (t);
        t = clk::now (); b.ft8_a8 (cd.data (), size, tbest, itone, &oy[0], &hy, &oy[1]); st.t_new += secs (t);
        ++st.vectors;
        if (hx != hy) { ++st.differing; continue; }
        st.compare (ox, oy, 2);
      }
    st.print ("FT8 ftx_ft8_a8_score_c"); all_equal = all_equal && st.differing == 0;
  }
  { // FT8: i cinque punti d'ingresso senza cattura. Passano da una copia di
    // run_ft8_bitmetrics diversa da quella di capture_c, quindi vanno chiamati.
    // ibest esce anche dalla finestra, come nel decoder: i simboli che non ci
    // stanno restano a zero.
    constexpr int np2 = 2812, size = 3200, NN8 = 79;
    std::vector<Complex> cd (size);
    for (int w = 0; w < 5; ++w)
      {
        Stat st;
        long const m = std::max (1L, n / 4);
        for (long v = 0; v < m; ++v)
          {
            int const ibest = static_cast<int> (rng () % 440u) - 120;
            int const imetric = 1 + static_cast<int> (rng () % 4u);
            make_burst (rng, 8, NN8, cd.data (), size, ibest, v % 10 != 0);
            float sx[8 * NN8], sy[8 * NN8], lx[5][174], ly[5][174];
            int nx = -1, ny = -2;
            poison (sx, sy, 8 * NN8);
            for (int q = 0; q < 5; ++q) poison (lx[q], ly[q], 174);
            auto t = clk::now ();
            if (w == 0) a.ft8_plain (cd.data (), np2, ibest, imetric, sx, &nx, lx[0], lx[1], lx[2], lx[3], lx[4]);
            else a.ft8_scaled[w - 1] (cd.data (), np2, ibest, imetric, 2.83f, sx, &nx, lx[0], lx[1], lx[2], lx[3], lx[4]);
            st.t_old += secs (t); t = clk::now ();
            if (w == 0) b.ft8_plain (cd.data (), np2, ibest, imetric, sy, &ny, ly[0], ly[1], ly[2], ly[3], ly[4]);
            else b.ft8_scaled[w - 1] (cd.data (), np2, ibest, imetric, 2.83f, sy, &ny, ly[0], ly[1], ly[2], ly[3], ly[4]);
            st.t_new += secs (t);
            ++st.vectors;
            if (nx != ny) { ++st.differing; continue; }
            st.compare (sx, sy, 8 * NN8);
            for (int q = 0; q < 5; ++q) st.compare (lx[q], ly[q], 174);
          }
        char name[80];
        std::snprintf (name, sizeof name, "FT8 %s", ft8_wrapper_names[w]);
        st.print (name); all_equal = all_equal && st.differing == 0;
      }
  }
  { // Ingressi degeneri: tutte le uscite di tutti i punti d'ingresso (run_all)
    constexpr int size = 3200, NN8 = 79, kinds = 6;
    static char const* const names[kinds] = {"degenere: silenzio", "degenere: un solo campione",
                                             "degenere: portante pura", "degenere: segnale con buchi",
                                             "degenere: segnale x 1e-18", "degenere: segnale x 1e-22"};
    std::vector<Complex> cd4 (NN4 * 32), cd8 (size);
    std::vector<float> x, y;
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);
    for (int kind = 0; kind < kinds; ++kind)
      {
        Stat st;
        long const m = std::max (1L, n / 20);
        for (long v = 0; v < m; ++v)
          {
            int const ibest = static_cast<int> (rng () % 440u) - 120;
            int const imetric = 1 + static_cast<int> (rng () % 4u);
            int const deep = static_cast<int> (rng () % 2u), eq = static_cast<int> (rng () % 2u);
            if (kind >= 3)
              {
                make_burst (rng, 4, NN4, cd4.data (), NN4 * 32, 0, true);
                make_burst (rng, 8, NN8, cd8.data (), size, ibest, true);
              }
            else
              {
                std::fill (cd4.begin (), cd4.end (), Complex {});
                std::fill (cd8.begin (), cd8.end (), Complex {});
              }
            if (kind == 1)
              {
                Complex const value = std::polar (std::pow (10.0f, -2.0f + 5.0f * uni (rng)), 6.2831853f * uni (rng));
                cd4[rng () % cd4.size ()] = value;
                cd8[rng () % cd8.size ()] = value;
              }
            else if (kind == 2)
              {
                float const amp = std::pow (10.0f, -2.0f + 5.0f * uni (rng)), phase = 6.2831853f * uni (rng);
                int const tone = static_cast<int> (rng () % 4u);
                for (size_t i = 0; i < cd4.size (); ++i)
                  cd4[i] = std::polar (amp, phase + 6.2831853f * static_cast<float> ((tone * i) % 32) / 32.0f);
                for (size_t i = 0; i < cd8.size (); ++i)
                  cd8[i] = std::polar (amp, phase + 6.2831853f * static_cast<float> ((tone * i) % 32) / 32.0f);
              }
            else if (kind == 3)
              {
                // tratti azzerati lunghi da uno a otto simboli, come un ingresso che cade
                for (int hole = 0; hole < 6; ++hole)
                  {
                    size_t const len = 32u * (1u + rng () % 8u);
                    size_t const p4 = rng () % (cd4.size () - len), p8 = rng () % (cd8.size () - len);
                    std::fill_n (cd4.begin () + static_cast<long> (p4), len, Complex {});
                    std::fill_n (cd8.begin () + static_cast<long> (p8), len, Complex {});
                  }
              }
            else if (kind >= 4)
              {
                float const k = kind == 4 ? 1.0e-18f : 1.0e-22f;
                for (auto& c : cd4) c *= k;
                for (auto& c : cd8) c *= k;
              }
            auto t = clk::now (); run_all (a, cd4.data (), cd8.data (), ibest, imetric, deep, eq, x); st.t_old += secs (t);
            t = clk::now (); run_all (b, cd4.data (), cd8.data (), ibest, imetric, deep, eq, y); st.t_new += secs (t);
            ++st.vectors; st.compare (x.data (), y.data (), x.size ());
          }
        st.print (names[kind]); all_equal = all_equal && st.differing == 0;
      }
  }
  std::printf ("%s\n", all_equal ? "TUTTO IDENTICO" : "DIFFERENZE TROVATE");
  return all_equal ? 0 : 1;
}
