// Port di jtty_mdecode.f90 (con jtty_peakup, subtract_jtty, ana64a, twkfreq,
// jtty_payload_correlators e rjtty_sub). L'ordine delle operazioni e dei
// confronti e' quello del Fortran: anche i pareggi (maxloc prende il primo
// massimo scorrendo le frequenze dentro il tempo) decidono quale segnale
// viene provato per primo e quindi che cosa resta dopo la sottrazione.
//
// Due scelte diverse, entrambe senza effetto sui risultati:
//  - la superficie di ricerca del sincronismo si calcola con la FFT piena a
//    8192 punti (il ramo "full_fft_available" del Fortran), non con la
//    scorciatoia di Bluestein, che da' gli stessi valori a meno
//    dell'arrotondamento;
//  - i tempi sono in singola precisione come nel Fortran (i confronti con
//    le tolleranze cadono sugli stessi arrotondamenti), ma contati da
//    un'origine che si sposta in avanti ogni cinque minuti: cosi' il
//    ricevitore resta acceso per ore senza che i secondi perdano i
//    millisecondi che le tolleranze (3 ms) richiedono. Il Fortran invece
//    ricomincia da capo a ogni periodo di 180 s e taglia i messaggi a cavallo.

#include "JttyDecoder.hpp"

#include "JttyCodec.hpp"
#include "JttyTbcc.hpp"
#include "JttyWave.hpp"

#include "Detector/FftCompat.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>

namespace decodium
{
namespace jtty
{

namespace
{

using cf = std::complex<float>;

constexpr int NSPS = 384;
constexpr int NSS = NSPS / 2;                 // campioni per simbolo a 6 kHz
constexpr int NFFT = 8192;
constexpr int NH2 = NFFT / 2;
constexpr int NFRAME_SYM = 59;
constexpr int NSYNC_SYM = 13;
constexpr int NCHAN_SYM = 46;
constexpr int NFRAME6 = NFRAME_SYM * NSS;     // 11328
constexpr int NTSTEP = NFRAME6 / 4;           // 2832
constexpr int NTGRID = NTSTEP / 12;           // 236
constexpr int NCHUNK = kChunkSamples;         // 28320
constexpr int NCHUNK6 = NCHUNK / 2;           // 14160
constexpr int NANA = 32768;
constexpr int NPSYNC = NSYNC_SYM * NSS;       // 2496
constexpr float FSAMPLE = 6000.0f;
constexpr float TWOPI = 6.283185307179586f;
constexpr int MAXCAND = 100;

constexpr int MAX_ACTIVE_MESSAGES = 30;
constexpr int MAX_RECENT_FRAMES = MAX_ACTIVE_MESSAGES * kMaxFrames;
constexpr int MAX_CONTINUATION_GAP = 3;
constexpr int MAX_RETRO_STEPS = 3;
constexpr int MAX_SUBTRACTED = 16;
constexpr float FRAME_HISTORY_TIME_TOLERANCE = 0.05f;
constexpr float FRAME_HISTORY_FREQ_TOLERANCE = 3.0f;
constexpr float NEAR_SIMULTANEOUS_FREQ_TOLERANCE = 12.0f;
constexpr float CONTINUATION_TIME_TOLERANCE = 0.1f;

int nint (double x)
{
  return static_cast<int> (std::lround (x));
}

float db (float x)
{
  return x > 1.259e-10f ? 10.0f * std::log10 (x) : -99.0f;
}

std::string blank80 ()
{
  return std::string (kMessageLength, ' ');
}

struct Decode
{
  float f1 {0.0f};
  float xdt {0.0f};
  float tsync {0.0f};
  float snrdb {0.0f};
  std::string decoded = blank80 ();
  bool trailing_sep {false};
  bool is_last_frame {false};
};

struct Assembly
{
  std::int64_t message_id {0};
  float f1 {0.0f};
  float tsync {0.0f};
  float start_tsync {0.0f};
  int k {0};
  std::string decoded = blank80 ();
  bool trailing_sep {false};
};

struct Fingerprint
{
  float f1 {0.0f};
  float tsync {0.0f};
};

struct PendingUpdate
{
  std::int64_t message_id {0};
  float f1 {0.0f};
  float start_tsync {0.0f};
  std::string decoded = blank80 ();
  bool complete {false};
};

class Plan
{
public:
  Plan (int n, cf* in, cf* out, int sign)
  {
    m_plan = decodium::fft_compat::plan_dft_1d (n, reinterpret_cast<fftwf_complex*> (in),
                                                reinterpret_cast<fftwf_complex*> (out), sign,
                                                FFTW_ESTIMATE);
  }
  ~Plan () { decodium::fft_compat::destroy_plan (m_plan); }
  Plan (Plan const&) = delete;
  Plan& operator= (Plan const&) = delete;
  void run () const { fftwf_execute (m_plan); }

private:
  fftwf_plan m_plan {nullptr};
};

// twkfreq con a(2)=a(3)=0: il fasore avanza prima di moltiplicare, quindi il
// primo campione e' gia' ruotato di un passo.
void twkfreq (cf const* c3, cf* c4, int npts, float fsample, float a1)
{
  float const twopi = 6.283185307f;
  cf w {1.0f, 0.0f};
  float const dphi = a1 * (twopi / fsample);
  cf const wstep {std::cos (dphi), std::sin (dphi)};
  for (int i = 0; i < npts; ++i)
    {
      w = w * wstep;
      c4[i] = w * c3[i];
    }
}

}

struct Receiver::Impl
{
  // Parametri
  int nfa {200};
  int nfb {2800};
  float f0 {1500.0f};
  float ftol {50.0f};
  float smin {4.6f};

  // Stato del modulo jtty_mdec
  int ndecodes {0};
  std::vector<Assembly> active;          // al piu' MAX_ACTIVE_MESSAGES
  std::vector<Fingerprint> recent;       // al piu' MAX_RECENT_FRAMES
  std::vector<PendingUpdate> pending;
  std::int64_t next_message_id {1};

  bool interferer_pending {false};
  float interferer_f1 {0.0f};
  float interferer_tsync {0.0f};
  int interferer_payload[34] {};
  int nsubtracted {0};
  float subtracted_f1[MAX_SUBTRACTED] {};
  float subtracted_tsync[MAX_SUBTRACTED] {};
  int subtracted_payload[MAX_SUBTRACTED][34] {};

  // Tavole e buffer salvati
  std::vector<cf> csync;                 // 13*NSS
  std::vector<cf> ctones;                // [i*NSS + j]
  std::vector<float> ref_real, ref_imag; // correlatore: [tone*NSS + sample]
  std::vector<cf> c0, c1;
  std::vector<float> s0;                 // [t*(NH2+1) + bin]
  std::vector<char> mask0;
  std::vector<cf> sync_in, sync_out;
  std::vector<cf> sub_cw, sub_camp, sub_cfilt;
  std::unique_ptr<Plan> plan_sync, plan_ana_fwd, plan_ana_inv, plan_sub_fwd, plan_sub_inv,
      plan_cw;
  TbccDecoder tbcc;

  // Flusso
  std::vector<std::int16_t> buffer;
  std::int64_t base {1};                 // indice 1-based di buffer[0]
  std::int64_t total {0};                // campioni ricevuti
  std::int64_t istart {1};
  bool primed {false};
  // Origine dei tempi in float: campioni gia' scontati e i loro secondi.
  std::int64_t epoch {0};
  double epoch_seconds {0.0};

  std::function<void (std::string const&)> debug;

  Impl ()
  {
    csync.resize (NPSYNC);
    {
      float const twopi = 8.0f * std::atan (1.0f);
      float const dt = 1.0f / FSAMPLE;
      float const baud = FSAMPLE / static_cast<float> (NSS);
      float phi = 0.0f;
      int k = 0;
      for (int i = 0; i < NSYNC_SYM; ++i)
        {
          float const dphi = twopi * baud * static_cast<float> (kSync13[i]) * dt;
          for (int j = 0; j < NSS; ++j)
            {
              csync[static_cast<std::size_t> (k++)] = cf {std::cos (phi), std::sin (phi)};
              phi = phi + dphi;
            }
        }
    }
    ctones.resize (4 * NSS);
    for (int i = 0; i < 4; ++i)
      {
        float phi = 0.0f;
        float const dphi = static_cast<float> (i) * TWOPI / static_cast<float> (NSS);
        for (int j = 0; j < NSS; ++j)
          {
            ctones[static_cast<std::size_t> (i * NSS + j)] = cf {std::cos (phi), std::sin (phi)};
            phi = phi + dphi;
          }
      }
    ref_real.resize (4 * NSS);
    ref_imag.resize (4 * NSS);
    for (int tone = 0; tone < 4; ++tone)
      {
        float phase = 0.0f;
        float const step = 6.2831853071795864769f * static_cast<float> (tone) / static_cast<float> (NSS);
        for (int s = 0; s < NSS; ++s)
          {
            ref_real[static_cast<std::size_t> (tone * NSS + s)] = std::cos (phase);
            ref_imag[static_cast<std::size_t> (tone * NSS + s)] = std::sin (phase);
            phase = phase + step;
          }
      }
    c0.assign (NANA, cf {});
    c1.assign (NCHUNK6, cf {});
    s0.assign (static_cast<std::size_t> ((NH2 + 1) * (NTGRID + 1)), 0.0f);
    mask0.assign (s0.size (), 0);
    sync_in.assign (NFFT, cf {});
    sync_out.assign (NFFT, cf {});
    sub_cw.assign (NANA, cf {});
    sub_camp.assign (NANA, cf {});
    sub_cfilt.assign (NANA, cf {});
    plan_sync.reset (new Plan (NFFT, sync_in.data (), sync_out.data (), FFTW_FORWARD));
    plan_ana_fwd.reset (new Plan (NANA, c0.data (), c0.data (), FFTW_FORWARD));
    plan_ana_inv.reset (new Plan (NANA / 2, c0.data (), c0.data (), FFTW_BACKWARD));
    plan_sub_fwd.reset (new Plan (NANA, sub_cfilt.data (), sub_cfilt.data (), FFTW_FORWARD));
    plan_sub_inv.reset (new Plan (NANA, sub_cfilt.data (), sub_cfilt.data (), FFTW_BACKWARD));
    plan_cw.reset (new Plan (NANA, sub_cw.data (), sub_cw.data (), FFTW_FORWARD));

    // Il filtro passa-basso della sottrazione: finestra cos^2 su due simboli,
    // spostata circolarmente come fa cshift(cw, nfilt/2+1).
    {
      int const nfilt = 2 * NSS;
      float const pi = 4.0f * std::atan (1.0f);
      std::vector<float> window (static_cast<std::size_t> (nfilt + 1));
      float sumw = 0.0f;
      for (int j = -nfilt / 2; j <= nfilt / 2; ++j)
        {
          float const w = std::pow (std::cos (pi * static_cast<float> (j) / static_cast<float> (nfilt)), 2.0f);
          window[static_cast<std::size_t> (j + nfilt / 2)] = w;
          sumw += w;
        }
      std::fill (sub_cw.begin (), sub_cw.end (), cf {});
      int const shift = nfilt / 2 + 1;
      for (int k = 0; k <= nfilt; ++k)
        {
          int const dest = ((k - shift) % NANA + NANA) % NANA;
          sub_cw[static_cast<std::size_t> (dest)] = cf {window[static_cast<std::size_t> (k)] / sumw, 0.0f};
        }
      plan_cw->run ();
      float const fac = 1.0f / static_cast<float> (NANA);
      for (auto& v : sub_cw) v *= fac;
    }
  }

  float& S0 (int bin, int t) { return s0[static_cast<std::size_t> (t * (NH2 + 1) + bin)]; }
  char& MASK (int bin, int t) { return mask0[static_cast<std::size_t> (t * (NH2 + 1) + bin)]; }

  // ------------------------------------------------------------ assemblaggio

  static bool same_frame (float f1a, float ta, float f1b, float tb)
  {
    return std::abs (f1a - f1b) < FRAME_HISTORY_FREQ_TOLERANCE
        && std::abs (ta - tb) < FRAME_HISTORY_TIME_TOLERANCE;
  }

  static bool same_recent_frame (float f1a, float ta, float f1b, float tb)
  {
    return std::abs (f1a - f1b) < NEAR_SIMULTANEOUS_FREQ_TOLERANCE
        && std::abs (ta - tb) < FRAME_HISTORY_TIME_TOLERANCE;
  }

  void reset_decode_search_state ()
  {
    active.clear ();
    recent.clear ();
  }

  bool is_recent_frame (Decode const& c) const
  {
    for (auto const& r : recent)
      if (same_recent_frame (c.f1, c.tsync, r.f1, r.tsync)) return true;
    return false;
  }

  void remember_recent_frame (Decode const& c)
  {
    // Una storia piena non deve mai impedire una decodifica valida.
    if (static_cast<int> (recent.size ()) >= MAX_RECENT_FRAMES) recent.erase (recent.begin ());
    recent.push_back ({c.f1, c.tsync});
  }

  void queue_message_update (Assembly const& m, bool complete)
  {
    for (auto& p : pending)
      if (p.message_id == m.message_id)
        {
          p.f1 = m.f1;
          p.decoded = m.decoded;
          p.complete = complete;
          return;
        }
    PendingUpdate p;
    p.message_id = m.message_id;
    p.f1 = m.f1;
    p.start_tsync = m.start_tsync;
    p.decoded = m.decoded;
    p.complete = complete;
    pending.push_back (p);
  }

  void remove_active_message (int index)
  {
    int const n = static_cast<int> (active.size ());
    if (index < 1 || index > n) return;
    if (index < n) active[static_cast<std::size_t> (index - 1)] = active.back ();
    active.pop_back ();
  }

  bool start_message (Decode const& c, Assembly& m)
  {
    m = Assembly {};
    if (!c.is_last_frame && static_cast<int> (active.size ()) >= MAX_ACTIVE_MESSAGES) return false;
    remember_recent_frame (c);
    m.message_id = next_message_id;
    m.f1 = c.f1;
    m.tsync = c.tsync;
    m.start_tsync = c.tsync;
    m.decoded = c.decoded;
    if (m.decoded.compare (0, 4, "599 ") == 0) m.decoded = fixed ("~" + rtrim (m.decoded), kMessageLength);
    m.k = len_trim (m.decoded);
    m.trailing_sep = c.trailing_sep;
    ++next_message_id;
    queue_message_update (m, c.is_last_frame);
    if (!c.is_last_frame) active.push_back (m);
    return true;
  }

  bool append_active_message (int index, Decode const& c, int gap, Assembly& m)
  {
    m = Assembly {};
    if (index < 1 || index > static_cast<int> (active.size ())) return false;
    remember_recent_frame (c);
    Assembly& a = active[static_cast<std::size_t> (index - 1)];
    int const k = a.k;
    int const n = len_trim (c.decoded);
    if (gap > 1)
      {
        int nstart = 1;
        if (n >= 1 && c.decoded[0] == '~') nstart = 2;
        int const kz = std::min (k + 5 + (n - nstart + 1), kMessageLength);
        int const nchar = std::max (kz - k - 5, 0);
        a.decoded = fixed (rtrim (a.decoded) + "~~~~~"
                           + c.decoded.substr (static_cast<std::size_t> (nstart - 1),
                                               static_cast<std::size_t> (nchar)),
                           kMessageLength);
        a.k = k + 5 + nchar;
      }
    else
      {
        int const kz = std::min (k + n, kMessageLength);
        std::string const piece = c.decoded.substr (0, static_cast<std::size_t> (std::max (0, kz - k)));
        if (a.trailing_sep) a.decoded = fixed (rtrim (a.decoded) + " " + piece, kMessageLength);
        else a.decoded = fixed (rtrim (a.decoded) + piece, kMessageLength);
        a.k = kz;
      }
    a.trailing_sep = c.trailing_sep;
    a.f1 = c.f1;
    a.tsync = c.tsync;
    queue_message_update (a, c.is_last_frame);
    m = a;
    if (c.is_last_frame) remove_active_message (index);
    return true;
  }

  void prune_receive_state (float forward_tsync, float frame_period)
  {
    float const oldest = forward_tsync - static_cast<float> (MAX_RETRO_STEPS) * frame_period / 4.0f;
    // I candidati retroattivi vanno indietro nel tempo: solo il fronte in
    // avanti puo' far scadere la storia.
    std::vector<Fingerprint> kept;
    kept.reserve (recent.size ());
    for (auto const& r : recent)
      if (!(r.tsync < oldest - FRAME_HISTORY_TIME_TOLERANCE)) kept.push_back (r);
    recent.swap (kept);

    int i = 1;
    while (i <= static_cast<int> (active.size ()))
      {
        Assembly const& a = active[static_cast<std::size_t> (i - 1)];
        if (oldest - a.tsync > static_cast<float> (MAX_CONTINUATION_GAP) * frame_period
                                   + CONTINUATION_TIME_TOLERANCE)
          {
            queue_message_update (a, false);
            remove_active_message (i);
          }
        else
          ++i;
      }
  }

  static void classify_active_candidate (Assembly const& e, Decode const& c, float frame_period,
                                         bool& match, bool& is_window_dupe, int& gap)
  {
    float const df1 = c.f1 - e.f1;
    float const dtsync = c.tsync - e.tsync;
    match = false;
    gap = 1;
    int const nfp = nint (dtsync / frame_period);
    float const fp_resid = std::abs (dtsync - frame_period * static_cast<float> (nfp));
    if (nfp >= 1 && nfp <= MAX_CONTINUATION_GAP && fp_resid < CONTINUATION_TIME_TOLERANCE)
      {
        float const df_tol = 10.0f + 3.0f * static_cast<float> (nfp - 1);
        match = std::abs (df1) < df_tol;
        if (match) gap = nfp;
      }
    // I ripassi retroattivi rivedono solo le tre finestre precedenti.
    is_window_dupe = false;
    if (!match)
      {
        float const qstep = frame_period / 4.0f;
        int const nstep = nint (dtsync / qstep);
        float const resid = std::abs (dtsync - qstep * static_cast<float> (nstep));
        if (std::abs (nstep) <= MAX_RETRO_STEPS && std::abs (df1) < 10.0f && resid < 0.003f
            && !((std::abs (nstep) % 4) == 0 && nstep != 0))
          {
            match = true;
            is_window_dupe = true;
          }
      }
  }

  // ----------------------------------------------------------------- DSP

  // ana64a + c0(nchunk6:)=0: segnale analitico a 6 kHz della finestra.
  void ana64a (std::int16_t const* iwave)
  {
    float const fac = 2.0f / (32767.0f * static_cast<float> (NANA));
    for (int i = 0; i < NCHUNK; ++i) c0[static_cast<std::size_t> (i)] = cf {fac * static_cast<float> (iwave[i]), 0.0f};
    for (int i = NCHUNK; i < NANA; ++i) c0[static_cast<std::size_t> (i)] = cf {};
    plan_ana_fwd->run ();
    int const nfft2 = NANA / 2;
    for (int i = nfft2 / 2 + 1; i <= nfft2 - 1; ++i) c0[static_cast<std::size_t> (i)] = cf {};
    c0[0] = 0.5f * c0[0];
    plan_ana_inv->run ();
    for (int i = NCHUNK6; i < NANA; ++i) c0[static_cast<std::size_t> (i)] = cf {};
  }

  void subtract (int const tones[NFRAME_SYM], float f1, float xdt)
  {
    int const nframe = NFRAME_SYM * NSS;
    std::vector<cf> const cref = generate_complex_wave (tones, NFRAME_SYM, NSS, 2.0f, FSAMPLE, f1);
    int const nstart = nint (xdt * FSAMPLE);
    std::fill (sub_camp.begin (), sub_camp.end (), cf {});
    for (int i = 1; i <= nframe; ++i)
      {
        int const j = nstart + i - 1;
        if (j >= 0 && j < NCHUNK6)
          sub_camp[static_cast<std::size_t> (i - 1)] = c0[static_cast<std::size_t> (j)]
              * std::conj (cref[static_cast<std::size_t> (i - 1)]);
      }
    std::copy (sub_camp.begin (), sub_camp.end (), sub_cfilt.begin ());
    plan_sub_fwd->run ();
    for (int i = 0; i < NANA; ++i) sub_cfilt[static_cast<std::size_t> (i)] *= sub_cw[static_cast<std::size_t> (i)];
    plan_sub_inv->run ();
    for (int i = 1; i <= nframe; ++i)
      {
        int const j = nstart + i - 1;
        if (j >= 0 && j < NCHUNK6)
          c0[static_cast<std::size_t> (j)] -= sub_cfilt[static_cast<std::size_t> (i - 1)]
              * cref[static_cast<std::size_t> (i - 1)];
      }
  }

  void peakup (float xdt0, float fc, float& xdt, float& f1, float& snr)
  {
    constexpr int hop = 4;
    float const PI = 3.141592653589793f;
    cf qstep[13];
    for (int i = 0; i <= 12; ++i)
      {
        int const is = i * NSS;
        qstep[i] = csync[static_cast<std::size_t> (is + hop)] * std::conj (csync[static_cast<std::size_t> (is)]);
      }
    float const fs = FSAMPLE;
    float const dt = 1.0f / fs;
    int const ia = std::max (0, nint ((xdt0 - 0.004f) / dt));
    int const ib = std::min (NCHUNK6 - NPSYNC, nint ((xdt0 + 0.004f) / dt));

    float pmax = 0.0f, fpk = 0.0f, xdtpk = 0.0f;
    cf zcur[13], zbest[13];
    std::fill (c1.begin (), c1.end (), cf {});
    for (int idf = -5; idf <= 5; ++idf)
      {
        float const a1 = -fc + 0.5f * static_cast<float> (idf);
        if (ib + NPSYNC > 0) twkfreq (c0.data (), c1.data (), ib + NPSYNC, fs, a1);
        if (ia <= ib)
          for (int i = 0; i <= 12; ++i)
            {
              int const is = i * NSS;
              cf z {};
              for (int n = 0; n < NSS; ++n)
                z += std::conj (csync[static_cast<std::size_t> (is + n)]) * c1[static_cast<std::size_t> (ia + is + n)];
              zcur[i] = z;
            }
        for (int i0 = ia; i0 <= ib; i0 += hop)
          {
            float p = 0.0f;
            for (int i = 0; i <= 12; ++i) p = p + zcur[i].real () * zcur[i].real () + zcur[i].imag () * zcur[i].imag ();
            if (p > pmax)
              {
                pmax = p;
                fpk = -a1;
                xdtpk = static_cast<float> (i0) * dt;
                std::copy (zcur, zcur + 13, zbest);
              }
            if (i0 + hop <= ib)
              for (int i = 0; i <= 12; ++i)
                {
                  int const is = i * NSS;
                  cf z {};
                  for (int r = 0; r < hop; ++r)
                    z += std::conj (csync[static_cast<std::size_t> (is + r)]) * c1[static_cast<std::size_t> (i0 + is + r)];
                  zcur[i] = qstep[i] * (zcur[i] - z);
                  z = cf {};
                  for (int r = 0; r < hop; ++r)
                    z += std::conj (csync[static_cast<std::size_t> (is + NSS - hop + r)])
                        * c1[static_cast<std::size_t> (i0 + is + NSS + r)];
                  zcur[i] += z;
                }
          }
      }

    // Secondo passo: la rampa di fase residua fra i 13 simboli affina f1.
    if (pmax > 0.0f)
      {
        float const tsym = static_cast<float> (NSS) / fs;
        float phase[13], uw[13];
        for (int i = 0; i <= 12; ++i) phase[i] = std::atan2 (zbest[i].imag (), zbest[i].real ());
        uw[0] = phase[0];
        for (int i = 1; i <= 12; ++i)
          {
            float dphi = phase[i] - phase[i - 1];
            while (dphi > PI) dphi -= TWOPI;
            while (dphi < -PI) dphi += TWOPI;
            uw[i] = uw[i - 1] + dphi;
          }
        float const xm = 6.0f;
        float sum = 0.0f;
        for (float u : uw) sum += u;
        float const ym = sum / 13.0f;
        float sxy = 0.0f, sxx = 0.0f;
        for (int i = 0; i <= 12; ++i)
          {
            sxy += (static_cast<float> (i) - xm) * (uw[i] - ym);
            sxx += (static_cast<float> (i) - xm) * (static_cast<float> (i) - xm);
          }
        float const slope = sxy / sxx;
        float const intercept = ym - slope * xm;
        float resid_rms = 0.0f;
        for (int i = 0; i <= 12; ++i)
          {
            float const resid = uw[i] - (slope * static_cast<float> (i) + intercept);
            resid_rms += resid * resid;
          }
        resid_rms = std::sqrt (resid_rms / 13.0f);
        float const dfhz = slope / (TWOPI * tsym);
        if (resid_rms < 1.0f && std::abs (dfhz) <= 0.5f)
          {
            cf ztot {};
            for (int i = 0; i <= 12; ++i)
              ztot += zbest[i] * cf {std::cos (-slope * static_cast<float> (i)), std::sin (-slope * static_cast<float> (i))};
            fpk = fpk + dfhz;
            pmax = ztot.real () * ztot.real () + ztot.imag () * ztot.imag ();
          }
      }
    f1 = fpk;
    xdt = xdtpk;
    snr = pmax;
  }

  void correlate_payload (int payload_start, SymbolCorrelations& corr, SymbolCorrelations& half)
  {
    for (auto& s : corr) s.fill (cf {});
    for (auto& s : half) s.fill (cf {});
    int const size = NCHUNK6;
    if (payload_start >= size) return;
    int const available = std::min (NCHAN_SYM, (size - payload_start) / NSS);
    int const half_length = NSS / 2;
    for (int symbol = 1; symbol <= available; ++symbol)
      {
        int const first = payload_start + (symbol - 1) * NSS;
        float full_r[4] = {}, full_i[4] = {};
        double energy[4] = {};
        for (int segment = 0; segment <= 1; ++segment)
          {
            float half_r[4] = {}, half_i[4] = {};
            for (int offset = segment * half_length; offset <= (segment + 1) * half_length - 1; ++offset)
              {
                cf const s = c1[static_cast<std::size_t> (first + offset)];
                float const sr = s.real (), si = s.imag ();
                for (int t = 0; t < 4; ++t)
                  {
                    float const rr = ref_real[static_cast<std::size_t> (t * NSS + offset)] * sr;
                    float const ii = ref_imag[static_cast<std::size_t> (t * NSS + offset)] * si;
                    float const ri = ref_real[static_cast<std::size_t> (t * NSS + offset)] * si;
                    float const ir = ref_imag[static_cast<std::size_t> (t * NSS + offset)] * sr;
                    full_r[t] = full_r[t] + rr + ii;
                    full_i[t] = full_i[t] + ri - ir;
                    half_r[t] = half_r[t] + rr + ii;
                    half_i[t] = half_i[t] + ri - ir;
                  }
              }
            for (int t = 0; t < 4; ++t)
              energy[t] = energy[t] + static_cast<double> (half_r[t] * half_r[t] + half_i[t] * half_i[t]);
          }
        for (int t = 0; t < 4; ++t)
          {
            corr[static_cast<std::size_t> (symbol - 1)][static_cast<std::size_t> (t)] = cf {full_r[t], full_i[t]};
            half[static_cast<std::size_t> (symbol - 1)][static_cast<std::size_t> (t)] =
                cf {static_cast<float> (std::sqrt (std::max (0.0, energy[t]))), 0.0f};
          }
      }
  }

  // ------------------------------------------------------------- mdecode

  void mdecode (std::int16_t const* iwave, std::int64_t istart_abs, std::int64_t istart0)
  {
    bool const use_interferer = interferer_pending;
    float const use_interferer_f1 = interferer_f1;
    float const use_interferer_tsync = interferer_tsync;
    int use_interferer_payload[34];
    std::copy (interferer_payload, interferer_payload + 34, use_interferer_payload);
    interferer_pending = false;
    nsubtracted = 0;

    int nsync = 0;
    if (istart_abs == istart0 && !use_interferer)
      {
        ndecodes = 0;
        reset_decode_search_state ();
      }
    {
      long long sum = 0;
      for (int i = 0; i < NCHUNK; ++i) sum += std::abs (static_cast<int> (iwave[i]));
      if (sum == 0) return;
    }

    // (istart-1)/12000.0 in singola precisione, contato dall'origine corrente.
    float const window_start = static_cast<float> (istart_abs - epoch - 1) / 12000.0f;
    float const dt = 1.0f / FSAMPLE;
    float const df2 = FSAMPLE / static_cast<float> (NFFT);

    ana64a (iwave);

    int tone_symbols_full[NFRAME_SYM];
    int tone_symbols_chk[NCHAN_SYM];
    if (use_interferer)
      {
        std::copy (kSync13, kSync13 + NSYNC_SYM, tone_symbols_full);
        tbcc_encode (use_interferer_payload, tone_symbols_chk);
        std::copy (tone_symbols_chk, tone_symbols_chk + NCHAN_SYM, tone_symbols_full + NSYNC_SYM);
        subtract (tone_symbols_full, use_interferer_f1,
                  use_interferer_tsync - window_start);
      }

    int const nfz = nint (10.0f / df2);                  // 14
    int const ntz = nint (0.016f * 6000.0f / 12.0f);     // 8

    float fc = 0.0f, fwid = 0.0f;
    int ja = 0, jb = 0;
    bool usable = false;
    int ichan = 0;
    int ipass = 0;

    auto channel_window = [&] () {
      bool constrain;
      if (ichan == 0)
        {
          fc = f0;
          fwid = ftol;
          constrain = false;
        }
      else
        {
          fc = ichan == 2 ? 1650.0f : 1350.0f;
          fwid = 150.0f;
          constrain = true;
        }
      int const first_bin = 3, last_bin = NH2 - 2;
      ja = first_bin;
      jb = last_bin;
      usable = false;
      if (df2 <= 0.0f || fwid < 0.0f) return;
      if (constrain)
        {
          if (nfa > nfb) return;
          fc = std::max (static_cast<float> (nfa), std::min (fc, static_cast<float> (nfb)));
        }
      ja = std::max (first_bin, static_cast<int> ((fc - fwid) / df2));
      jb = std::min (last_bin, static_cast<int> ((fc + fwid) / df2));
      if (constrain)
        {
          ja = std::max (ja, static_cast<int> (std::ceil (static_cast<float> (nfa) / df2)));
          jb = std::min (jb, static_cast<int> (std::floor (static_cast<float> (nfb) / df2)));
        }
      usable = ja <= jb;
    };

    int const nchan = 2;
    int first_sync_bin = NH2 - 2;
    int last_sync_bin = 3;
    for (ichan = 0; ichan <= nchan; ++ichan)
      {
        channel_window ();
        if (!usable) continue;
        first_sync_bin = std::min (first_sync_bin, ja);
        last_sync_bin = std::max (last_sync_bin, jb);
      }

    int const nc = 2;
    int ncand = 0;
    bool any_subtracted = false;
    bool s0_valid = false;
    int nstep_search = 0;
    std::vector<Decode> cand (MAXCAND + 1);    // 1-based come il Fortran
    int n_ch0_ok = 0;
    int ja_ch0_ok[16], jb_ch0_ok[16];
    float f1_ch0_ok[16];
    float tsync_ch0_ok[16];
    float pt = 0.0f, pa = 0.0f, pn = 0.0f, snrdb = 0.0f;
    float pow[4][NCHAN_SYM];
    int irxsync[NSYNC_SYM] = {};
    int irxchan[NCHAN_SYM] = {};
    bool match = false;
    bool channel_decoded = false;

    auto build_s0 = [&] () {
      int istep = 0;
      for (int i0 = 0; i0 <= NTSTEP; i0 += 12)
        {
          for (int i = 0; i < NPSYNC; ++i)
            sync_in[static_cast<std::size_t> (i)] = std::conj (csync[static_cast<std::size_t> (i)]) * c0[static_cast<std::size_t> (i0 + i)];
          std::fill (sync_in.begin () + NPSYNC, sync_in.end (), cf {});
          plan_sync->run ();
          auto P = [&] (int j) {
            cf const z = sync_out[static_cast<std::size_t> (j)];
            return z.real () * z.real () + z.imag () * z.imag ();
          };
          if (first_sync_bin <= last_sync_bin)
            {
              float p0 = P (first_sync_bin - 2);
              float p1 = P (first_sync_bin - 1);
              float p2 = P (first_sync_bin);
              float p3 = P (first_sync_bin + 1);
              for (int j = first_sync_bin; j <= last_sync_bin; ++j)
                {
                  float const p4 = P (j + 2);
                  S0 (j, istep) = p0 + 2 * p1 + 3 * p2 + 2 * p3 + p4;
                  p0 = p1;
                  p1 = p2;
                  p2 = p3;
                  p3 = p4;
                }
            }
          ++istep;
        }
      nstep_search = istep - 1;
      s0_valid = true;
    };

    auto record_ch0_success = [&] () {
      if (ichan != 0) return;
      if (n_ch0_ok >= 16) return;
      Decode const& c = cand[static_cast<std::size_t> (ncand)];
      ja_ch0_ok[n_ch0_ok] = nint (c.f1 / df2) - nfz;
      jb_ch0_ok[n_ch0_ok] = nint (c.f1 / df2) + nfz;
      f1_ch0_ok[n_ch0_ok] = c.f1;
      tsync_ch0_ok[n_ch0_ok] = c.tsync;
      ++n_ch0_ok;
    };

    auto decode_and_merge = [&] (int ic_label) -> bool {
      Decode& cur = cand[static_cast<std::size_t> (ncand)];
      int const payload_start = nint (cur.xdt / dt) + NSYNC_SYM * NSS;
      SymbolCorrelations zsym, zhalf;
      correlate_payload (payload_start, zsym, zhalf);
      int final_payload[34];
      bool const success_dec = tbcc.decode (zsym, zhalf, final_payload);
      for (int j = 0; j < NCHAN_SYM; ++j)
        {
          int best = 0;
          for (int i = 0; i < 4; ++i)
            {
              cf const z = zsym[static_cast<std::size_t> (j)][static_cast<std::size_t> (i)];
              float const a = std::abs (z);
              pow[i][j] = a * a;
            }
          for (int i = 1; i < 4; ++i)
            if (pow[i][j] > pow[best][j]) best = i;
          irxchan[j] = best;
        }
      cur.decoded = blank80 ();
      if (!success_dec) return false;

      Frame const frame = payload_to_frame (final_payload);
      UnpackResult const u = unpack_frames (&frame, 1);
      cur.decoded = u.message;
      cur.trailing_sep = u.trailing_sep;
      cur.is_last_frame = u.is_last_frame;
      if (!u.source_valid) return false;

      ++ndecodes;
      tbcc_encode (final_payload, tone_symbols_chk);
      int nsymerrs = 13 - nsync;
      for (int j = 0; j < NCHAN_SYM; ++j)
        {
          int const is = tone_symbols_chk[j];
          if (is != irxchan[j]) ++nsymerrs;
          pt = pt + pow[is][j];
          pa = pa + (pow[0][j] + pow[1][j] + pow[2][j] + pow[3][j]);
        }
      pn = (pa - pt) / 3.0f;
      if (pn > 0.0f)
        {
          snrdb = db (pt / pn);
          cur.snrdb = snrdb;
        }
      cur.tsync = window_start + cur.xdt;

      // Doppioni
      bool dupe = false;
      for (int i = 1; i <= ncand - 1; ++i)
        if (rtrim (cand[static_cast<std::size_t> (i)].decoded) == rtrim (cur.decoded)
            && std::abs (cand[static_cast<std::size_t> (i)].tsync - cur.tsync) < 0.032f)
          dupe = true;
      // Un candidato dei canali 1/2 uguale a un frame gia' decodificato dal
      // canale 0 e' rumore della stima del sincronismo, non un altro segnale.
      if (ichan != 0)
        for (int i = 0; i < n_ch0_ok; ++i)
          if (same_frame (cur.f1, cur.tsync, f1_ch0_ok[i], tsync_ch0_ok[i])) dupe = true;
      if (dupe) return true;

      // Sottrazione: il segnale ricostruito (sincronismo + codice corretto)
      // esce da c0, e il passo successivo cerca sul residuo.
      std::copy (kSync13, kSync13 + NSYNC_SYM, tone_symbols_full);
      std::copy (tone_symbols_chk, tone_symbols_chk + NCHAN_SYM, tone_symbols_full + NSYNC_SYM);
      subtract (tone_symbols_full, cur.f1, cur.xdt);
      any_subtracted = true;
      s0_valid = false;
      if (nsubtracted < MAX_SUBTRACTED)
        {
          subtracted_f1[nsubtracted] = cur.f1;
          subtracted_tsync[nsubtracted] = cur.tsync;
          std::copy (final_payload, final_payload + 34, subtracted_payload[nsubtracted]);
          ++nsubtracted;
        }

      Decode const dec = cur;
      match = false;
      bool is_pure_dupe = is_recent_frame (dec);
      int iactive = 0;
      int best_gap = 1;
      if (!is_pure_dupe && !active.empty ())
        {
          bool have_win = false;
          int best_cont = 0;
          float best_df = 1.0e30f;
          for (int i = 1; i <= static_cast<int> (active.size ()); ++i)
            {
              bool is_window_dupe;
              int gap;
              classify_active_candidate (active[static_cast<std::size_t> (i - 1)], dec,
                                         static_cast<float> (NFRAME6) / 6000.0f, match, is_window_dupe, gap);
              if (!match) continue;
              if (is_window_dupe)
                {
                  if (!have_win) iactive = i;
                  have_win = true;
                  continue;
                }
              float const dfabs = std::abs (dec.f1 - active[static_cast<std::size_t> (i - 1)].f1);
              if (dfabs < best_df)
                {
                  best_df = dfabs;
                  best_cont = i;
                  best_gap = gap;
                }
            }
          if (have_win)
            {
              match = true;
              is_pure_dupe = true;
            }
          else if (best_cont > 0)
            {
              match = true;
              iactive = best_cont;
            }
          else
            match = false;
        }

      std::string msg;
      Assembly accepted_message;
      if (!is_pure_dupe && match)
        {
          if (!append_active_message (iactive, dec, best_gap, accepted_message)) return true;
          msg = accepted_message.decoded;
          if (dec.is_last_frame) iactive = 0;
        }
      else if (!is_pure_dupe)
        {
          if (!start_message (dec, accepted_message)) return true;
          msg = accepted_message.decoded;
          iactive = dec.is_last_frame ? 0 : static_cast<int> (active.size ());
        }
      else
        msg = dec.decoded;
      msg = display_message_text (msg);
      if (debug)
        {
          char line[256];
          std::snprintf (line, sizeof line, "%4d%4d%4d%4d%4d%4d%3s%3s%7.1f%7.3f%9.3f%5d%4d%4d  %s", ichan,
                         ipass, ic_label, ndecodes, iactive, static_cast<int> (active.size ()),
                         match ? "T" : "F", use_interferer ? "T" : "F", dec.f1, dec.xdt,
                         dec.tsync, nint (dec.snrdb - 20.0f), nsync, nsymerrs,
                         rtrim (msg).c_str ());
          debug (line);
        }
      return true;
    };

    auto process_channel = [&] () {
      channel_window ();
      if (!usable) return;
      int nc0 = nc;
      // Una banda QSO larga ha bisogno di piu' candidati.
      if (ichan == 0) nc0 = std::max (2, std::min (8, nint (fwid / (static_cast<float> (nfz) * df2))));
      float fbest = 0.0f, xdtbest = 0.0f;
      channel_decoded = false;

      if (ichan != 0 && n_ch0_ok > 0)
        for (int i = 0; i < n_ch0_ok; ++i)
          {
            int const lo = std::max (ja, ja_ch0_ok[i]);
            int const hi = std::min (jb, jb_ch0_ok[i]);
            if (lo <= hi)
              for (int t = 0; t <= nstep_search; ++t)
                for (int j = lo; j <= hi; ++j) S0 (j, t) = 0.0f;
          }

      if (ichan == 0)
        for (int t = 0; t <= nstep_search; ++t)
          for (int j = ja; j <= jb; ++j) MASK (j, t) = 1;

      for (int ic = 1; ic <= nc0; ++ic)
        {
          int nsloc1 = 0, nsloc2 = 0;
          {
            bool found = false;
            float best = 0.0f;
            for (int t = 0; t <= nstep_search; ++t)
              for (int j = ja; j <= jb; ++j)
                {
                  if (ichan == 0 && !MASK (j, t)) continue;
                  float const v = S0 (j, t);
                  if (!found || v > best)
                    {
                      found = true;
                      best = v;
                      nsloc1 = j - ja + 1;
                      nsloc2 = t + 1;
                    }
                }
          }
          int const jlo = std::max (ja, nsloc1 - nfz + ja);
          int const jhi = std::min (jb, nsloc1 + nfz + ja);
          int const tlo = std::max (0, nsloc2 - ntz);
          int const thi = std::min (nstep_search, nsloc2 + ntz);
          for (int t = tlo; t <= thi; ++t)
            for (int j = jlo; j <= jhi; ++j)
              {
                if (ichan == 0) MASK (j, t) = 0;
                else S0 (j, t) = 0.0f;
              }
          fbest = static_cast<float> (nsloc1 - 1 + ja) * df2;
          xdtbest = static_cast<float> (nsloc2 - 1) * dt * 12.0f;

          if (ichan == 0)
            {
              float xdt1, f11, snr0;
              peakup (xdtbest, fbest, xdt1, f11, snr0);
              xdtbest = xdt1;
              fbest = f11;
            }

          if (ncand >= MAXCAND) break;
          ++ncand;
          Decode& c = cand[static_cast<std::size_t> (ncand)];
          c.xdt = xdtbest;
          c.f1 = fbest;

          twkfreq (c0.data (), c1.data (), NCHUNK6, 6000.0f, -c.f1);

          pt = 0.0f;
          pa = 0.0f;
          for (auto& row : pow) std::fill (row, row + NCHAN_SYM, 0.0f);
          for (int j = 1; j <= NSYNC_SYM; ++j)
            {
              int const i0 = nint (c.xdt / dt) + (j - 1) * NSS;
              if (i0 + NSS > NCHUNK6) break;
              for (int i = 0; i < 4; ++i)
                {
                  cf z {};
                  for (int n = 0; n < NSS; ++n)
                    z += std::conj (ctones[static_cast<std::size_t> (i * NSS + n)]) * c1[static_cast<std::size_t> (i0 + n)];
                  pow[i][j - 1] = (z * std::conj (z)).real ();
                }
              int iloc = 0;
              for (int i = 1; i < 4; ++i)
                if (pow[i][j - 1] > pow[iloc][j - 1]) iloc = i;
              irxsync[j - 1] = iloc;
              pt = pt + pow[kSync13[j - 1]][j - 1];
              pa = pa + (pow[0][j - 1] + pow[1][j - 1] + pow[2][j - 1] + pow[3][j - 1]);
            }
          snrdb = -99.9f;
          pn = (pa - pt) / 3.0f;
          if (pn > 0.0f) snrdb = db (pt / pn);
          nsync = 0;
          for (int j = 0; j < NSYNC_SYM; ++j)
            if (kSync13[j] == irxsync[j]) ++nsync;
          c.snrdb = snrdb;

          if (ichan == 0 && (nsync <= 6 || snrdb < smin)) continue;
          if (ichan != 0 && (nsync <= 8 || snrdb < 5.0f)) continue;

          bool const decoded_ok = decode_and_merge (ic);
          if (decoded_ok) record_ch0_success ();
          if (decoded_ok) channel_decoded = true;
        }

      if (!channel_decoded)
        {
          // Sincronismo "appiccicoso": se un messaggio attivo aspettava il
          // frame successivo proprio adesso, si riprova la decodifica li'.
          for (int ir = 1; ir <= static_cast<int> (active.size ()); ++ir)
            {
              Assembly const& a = active[static_cast<std::size_t> (ir - 1)];
              if (a.f1 < fc - fwid || a.f1 > fc + fwid) continue;
              if (std::abs ((window_start - a.tsync) - static_cast<float> (NFRAME6) / 6000.0f) > 0.1f) continue;
              float const xdt_retry = a.tsync + static_cast<float> (NFRAME6) / 6000.0f - window_start;
              if (xdt_retry < 0.0f) continue;
              if (ncand >= MAXCAND) break;
              ++ncand;
              Decode& c = cand[static_cast<std::size_t> (ncand)];
              c.xdt = xdt_retry;
              c.f1 = a.f1;
              twkfreq (c0.data (), c1.data (), NCHUNK6, 6000.0f, -c.f1);
              nsync = -1;
              bool const decoded_ok = decode_and_merge (-1);
              if (decoded_ok) record_ch0_success ();
              if (decoded_ok) channel_decoded = true;
              break;
            }
        }
    };

    // Fase A: il canale 0 ha il primo diritto su ogni segnale.
    n_ch0_ok = 0;
    for (ipass = 1; ipass <= 2; ++ipass)
      {
        if (ipass == 2 && !any_subtracted) break;
        if (!s0_valid) build_s0 ();
        ichan = 0;
        process_channel ();
      }

    // Fase B: i canali 1 e 2 sulla stessa superficie, se non e' cambiata.
    any_subtracted = false;
    for (ipass = 1; ipass <= 2; ++ipass)
      {
        if (ipass == 2 && !any_subtracted) break;
        if (!s0_valid) build_s0 ();
        for (ichan = 1; ichan <= nchan; ++ichan) process_channel ();
        s0_valid = false;
      }
  }

  // jtty_mdecode_step: il passo in avanti e i ripassi retroattivi.
  void mdecode_step (std::int16_t const* iwave, std::int64_t first_index, std::int64_t istart_abs,
                     std::int64_t istart0)
  {
    int const nframe = 59 * NSPS;
    int const step = nframe / 4;
    prune_receive_state (static_cast<float> (istart_abs - epoch - 1) / 12000.0f,
                         static_cast<float> (nframe) / 12000.0f);
    interferer_pending = false;
    mdecode (iwave + (istart_abs - first_index), istart_abs, istart0);

    int const n_local = nsubtracted;
    float f1_local[MAX_SUBTRACTED];
    float tsync_local[MAX_SUBTRACTED];
    int payload_local[MAX_SUBTRACTED][34];
    for (int i = 0; i < n_local; ++i)
      {
        f1_local[i] = subtracted_f1[i];
        tsync_local[i] = subtracted_tsync[i];
        std::copy (subtracted_payload[i], subtracted_payload[i] + 34, payload_local[i]);
      }
    for (int i = 0; i < n_local; ++i)
      for (int k = 1; k <= MAX_RETRO_STEPS; ++k)
        {
          std::int64_t const istart_prev = istart_abs - static_cast<std::int64_t> (k) * step;
          if (istart_prev < 1 || istart_prev < first_index) continue;
          interferer_pending = true;
          interferer_f1 = f1_local[i];
          interferer_tsync = tsync_local[i];
          std::copy (payload_local[i], payload_local[i] + 34, interferer_payload);
          mdecode (iwave + (istart_prev - first_index), istart_prev, istart0);
        }
  }

  void reset ()
  {
    buffer.clear ();
    base = 1;
    total = 0;
    istart = 1;
    primed = false;
    epoch = 0;
    epoch_seconds = 0.0;
    ndecodes = 0;
    active.clear ();
    recent.clear ();
    pending.clear ();
    interferer_pending = false;
    nsubtracted = 0;
  }

  // Sposta l'origine dei tempi quando i secondi diventano grandi: la storia
  // che resta (messaggi attivi, frame recenti, aggiornamenti in coda) viene
  // riportata alla nuova origine.
  void rebase_if_needed ()
  {
    std::int64_t const rel = istart - epoch;
    if (rel < 300 * static_cast<std::int64_t> (kSampleRate)) return;
    std::int64_t const shift = rel - 60 * static_cast<std::int64_t> (kSampleRate);
    float const ds = static_cast<float> (static_cast<double> (shift) / kSampleRate);
    for (auto& a : active)
      {
        a.tsync -= ds;
        a.start_tsync -= ds;
      }
    for (auto& r : recent) r.tsync -= ds;
    for (auto& p : pending) p.start_tsync -= ds;
    epoch += shift;
    epoch_seconds += static_cast<double> (shift) / kSampleRate;
  }

  // rjtty_core, per un flusso che non finisce mai.
  void add_samples (std::int16_t const* samples, int count)
  {
    if (count <= 0) return;
    buffer.insert (buffer.end (), samples, samples + count);
    total += count;
    if (!primed)
      {
        // Il Fortran, alla prima chiamata dopo un azzeramento, fissa solo
        // l'inizio e aspetta il giro dopo.
        primed = true;
        istart = 1;
        reset_decode_search_state ();
        return;
      }
    int const nframe = 59 * NSPS;
    int const step = nframe / 4;
    while (istart + NCHUNK - 1 <= total)
      {
        mdecode_step (buffer.data (), base, istart, 1);
        istart += step;
      }
    rebase_if_needed ();
    // Si tengono i campioni che servono ai ripassi retroattivi.
    std::int64_t const keep_from = istart - static_cast<std::int64_t> (MAX_RETRO_STEPS) * step;
    if (keep_from - base > 8 * static_cast<std::int64_t> (kSampleRate))
      {
        std::int64_t const drop = keep_from - base;
        buffer.erase (buffer.begin (), buffer.begin () + drop);
        base += drop;
      }
  }
};

Receiver::Receiver () : m_ {new Impl} {}
Receiver::~Receiver () = default;

void Receiver::set_parameters (int nfa, int nfb, float f0, float ftol, float smin)
{
  m_->nfa = nfa;
  m_->nfb = nfb;
  m_->f0 = f0;
  m_->ftol = ftol;
  m_->smin = smin;
}

void Receiver::reset () { m_->reset (); }

void Receiver::add_samples (std::int16_t const* samples, int count) { m_->add_samples (samples, count); }

std::vector<MessageUpdate> Receiver::take_updates ()
{
  std::vector<MessageUpdate> out;
  out.reserve (m_->pending.size ());
  for (auto const& p : m_->pending)
    {
      MessageUpdate u;
      u.message_id = p.message_id;
      u.frequency = p.f1;
      u.start_seconds = m_->epoch_seconds + static_cast<double> (p.start_tsync);
      u.text = rtrim (display_message_text (p.decoded));
      u.complete = p.complete;
      out.push_back (u);
    }
  m_->pending.clear ();
  return out;
}

std::int64_t Receiver::samples_received () const { return m_->total; }

void Receiver::set_debug_sink (std::function<void (std::string const&)> sink) { m_->debug = std::move (sink); }

void Receiver::decode_buffer (std::int16_t const* samples, int count)
{
  int const nframe = 59 * NSPS;
  int const step = nframe / 4;
  std::int64_t istart = 1;
  while (istart + NCHUNK - 1 <= count)
    {
      m_->mdecode_step (samples, 1, istart, 1);
      m_->pending.clear ();
      istart += step;
    }
}

}
}
