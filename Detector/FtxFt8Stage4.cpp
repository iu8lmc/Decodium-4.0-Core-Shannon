// -*- Mode: C++ -*-
#include "wsjtx_config.h"
#include "commons.h"
#include "helper_functions.h"

#include <algorithm>
#include "Detector/FtxApStorico.hpp"

extern "C" void ftx_ft8_ap_msg_conta_successo_c ();
extern "C" int ftx_ft8_ap_msg_successi_c ();
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <fftw3.h>

#include "Detector/FftCompat.hpp"

#include "lib/superfox/qpc/np_qpc.h"

extern "C" uint32_t nhash2 (void const* key, uint64_t length, uint32_t initval);

namespace
{

constexpr int kFt8NMax {15 * 12000};
constexpr int kFt8Nn {79};
constexpr int kFt8Nh1 {1920};
constexpr int kFt8DefaultMaxCand {1000};
constexpr int kFt8DeepMaxCand {2400};
constexpr int kFt8MaxCand {4800};
constexpr int kFt8StrictHardErrors {36};
constexpr int kFt8MaxHardErrors {58};
constexpr int kFt8MaxEarly {200};
constexpr int kFt8A7MaxRetained {160};
constexpr int kFt8A7MaxAge {4};
constexpr int kFt8CqSignalMemory {48};
constexpr int kFt8CqSignalMaxAge {4};
constexpr float kFt8CqSignalSaveScore {1.15f};
constexpr float kFt8CqSignalRepeatScore {0.65f};
constexpr int kFt8CallGridMemory {96};
constexpr int kFt8CallGridMaxAge {4};
constexpr int kFt8KnownCallGridMemory {1024};
constexpr int kFt8KnownCqCallMemory {512};
constexpr int kFt8KnownCallGridMaxAgeSeconds {3 * 3600};
constexpr int kFt8KnownCallGridMinFastReplayHits {3};
constexpr int kFt8KnownCallGridFastReplayMaxAgeSeconds {150};
// Regolabile per misura: DECODIUM_FT8_KNOWNCQ_AGE (secondi).
inline int ft8_knowncq_fast_age ()
{
  static int const v = [] {
    char const* raw = std::getenv ("DECODIUM_FT8_KNOWNCQ_AGE");
    int const n = raw ? std::atoi (raw) : 0;
    return (n > 0 && n <= 3 * 3600) ? n : kFt8KnownCallGridFastReplayMaxAgeSeconds;
  }();
  return v;
}
constexpr float kFt8KnownCallGridFastReplayMaxFreqDelta {2.5f};
constexpr float kFt8KnownCallGridFastReplayMaxDtDelta {0.10f};
constexpr float kFt8KnownCallGridFastReplayMinCqScore {3.0f};
constexpr int kFt8KnownCallGridFastReplayMinSync {9};
constexpr int kFt8HashCallSeedMemory {8192};
constexpr int kFt8MaxLines {200};
constexpr int kFt8Bits {77};
constexpr int kFt8DecodedChars {37};
constexpr int kFt8WordChars {13};
constexpr int kFt8WordCount {19};
constexpr int kFt8SequenceCount {2};
constexpr int kFt8CarrySamples {47 * 3456};
constexpr int kFt8A7Np2 {2812};
constexpr int kFt8A7DownsampleSize {3200};
constexpr int kFt8VarDownsampleOffset {800};
constexpr int kFt8VarDownsampleSize {4801};
constexpr int kFt8A7MaxMsg {206};
constexpr float kFt8A7Fs2 {200.0f};
constexpr int kFt8A8Nsym {79};
constexpr int kFt8A8Nsps {32};
constexpr int kFt8A8Nwave {kFt8A8Nsym * kFt8A8Nsps};
constexpr int kFt8A8DphiSize {(kFt8A8Nsym + 2) * kFt8A8Nsps};
constexpr int kFt8A8MaxMsg {206};
constexpr int kFt8PhaseTableSize {65536};
constexpr float kFt8A8Bt {2.0f};
constexpr float kFt8TwoPi {6.28318530717958647692f};
constexpr float kFt8BitMetricScale {2.83f};

// Quanti thread deve usare la regione parallela dello stadio 4. Si applica con
// la clausola num_threads, che NON ridimensiona il pool OpenMP: cambiarne la
// dimensione fa morire i thread, ed e' da li' che nascevano la corruzione dello
// heap e gli stalli (vedi il commento in FT8DecodeWorker.cpp).
std::atomic<int>& ft8_thread_budget ()
{
  static std::atomic<int> budget {1};
  return budget;
}

extern "C" void ftx_ft8_set_thread_budget_c (int n)
{
  ft8_thread_budget ().store (n > 0 ? n : 1);
}

std::atomic<bool>& stage4_cancel_requested ()
{
  static std::atomic<bool> cancel {false};
  return cancel;
}

std::atomic<long long>& stage4_deadline_ms ()
{
  static std::atomic<long long> deadline {0};
  return deadline;
}

std::atomic<int>& stage4_ldpc_maxosd_override ()
{
  static std::atomic<int> value {-1};
  return value;
}

std::atomic<int>& stage4_ldpc_norder_override ()
{
  static std::atomic<int> value {0};
  return value;
}

std::atomic<bool>& stage4_supplemental_requested ()
{
  static std::atomic<bool> value {false};
  return value;
}

std::atomic<bool>& stage4_low_threshold_requested ()
{
  static std::atomic<bool> value {false};
  return value;
}

std::atomic<bool>& stage4_subpass_requested ()
{
  static std::atomic<bool> value {false};
  return value;
}

std::atomic<int>& stage4_decode_cycles ()
{
  static std::atomic<int> value {1};
  return value;
}

std::atomic<int>& stage4_rx_freq_sensitivity ()
{
  static std::atomic<int> value {1};
  return value;
}

std::atomic<int>& stage4_candidate_thin ()
{
  static std::atomic<int> value {100};
  return value;
}

std::atomic<bool>& stage4_superfox_enabled ()
{
  static std::atomic<bool> value {false};
  return value;
}

std::atomic<int>& stage4_superfox_tolerance_hz ()
{
  static std::atomic<int> value {50};
  return value;
}

std::atomic<int>& freqpart_bins_used ()
{
  static std::atomic<int> value {0};
  return value;
}

std::atomic<int>& stage4_force_fresh_slot ()
{
  static std::atomic<int> value {0};
  return value;
}

std::atomic<int>& stage4_freqpart_request ()
{
  static std::atomic<int> value {0};
  return value;
}

std::atomic<float>& stage4_syncmin_scale_request ()
{
  static std::atomic<float> value {1.0f};
  return value;
}

std::atomic<int>& stage4_decode_syncmin_request ()
{
  static std::atomic<int> value {-1};
  return value;
}

long long steady_clock_ms ()
{
  using namespace std::chrono;
  return duration_cast<milliseconds> (steady_clock::now ().time_since_epoch ()).count ();
}

bool stage4_should_cancel ()
{
  if (stage4_cancel_requested ().load (std::memory_order_relaxed))
    {
      return true;
    }

  long long const deadline = stage4_deadline_ms ().load (std::memory_order_relaxed);
  return deadline > 0 && steady_clock_ms () >= deadline;
}

long long stage4_remaining_ms ()
{
  long long const deadline = stage4_deadline_ms ().load (std::memory_order_relaxed);
  if (deadline <= 0)
    {
      return 9000000000000LL;
    }
  return deadline - steady_clock_ms ();
}

extern "C"
{
  void ftx_sync8_search_c (float const* dd, int npts, float nfa, float nfb,
                           float syncmin, float nfqso, int maxcand,
                           float* candidate, int* ncand, float* sbase);
  void ftx_sync8_search_stage4_c (float const* dd, int npts, float nfa, float nfb,
                                   float syncmin, float nfqso, int maxcand, int ipass,
                                   int candidate_thin,
                                   float* candidate, int* ncand, float* sbase);
  void ftx_subtract_ft8_c (float* dd0, int const* itone, float f0, float dt, int lrefinedt);
  void ftx_prepare_ft8_ap_c (char const mycall[12], char const hiscall[12], int ncontest,
                             int* apsym, int* aph10);
  void ftx_ft8_prepare_pass_c (int ndepth, int ipass, int ndecodes,
                               float* syncmin, int* imetric, int* lsubtract, int* run_pass);
  void ftx_ft8_plan_decode_stage_c (int ndepth, int nzhsym, int ndec_early,
                                    int nagain, int* action_out, int* refine_out);
  int ftx_ft8_should_bail_by_tseq_c (int ldiskdat, double tseq, double limit);
  void ftx_ft8_prepare_candidate_c (float sync_in, float f1_in, float xdt_in,
                                    float const* sbase, int sbase_size,
                                    float* sync_out, float* f1_out,
                                    float* xdt_out, float* xbase_out);
  void ftx_ft8_finalize_main_result_c (float xsnr, float xdt_in, float emedelay,
                                       int nharderrors, float dmin, int* nsnr_out,
                                       float* xdt_out, float* qual_out);
  int ftx_ft8_should_run_a7_c (int lft8apon, int ncontest, int nzhsym, int previous_count);
  int ftx_ft8_should_run_a8_c (int lft8apon, int ncontest, int nzhsym, int la8,
                               int hiscall_len, int hisgrid_len, int ltry_a8);
  int ftx_ft8_prepare_a7_request_c (float previous_f0, float previous_dt,
                                    char const previous_msg37[37],
                                    float const* sbase, int sbase_size,
                                    float* f1_out, float* xdt_out, float* xbase_out,
                                    char call_1_out[12], char call_2_out[12], char grid4_out[4]);
  int ftx_ft8_should_keep_a8_after_a7_c (char const decoded_msg37[37],
                                         char const hiscall12[12]);
  void ftx_ft8_finalize_a7_result_c (float xsnr, int* nsnr_out, int* iaptype_out,
                                     float* qual_out);
  void ftx_ft8_finalize_a8_result_c (float plog, float xsnr, float fbest,
                                     int* nsnr_out, int* iaptype_out,
                                     float* qual_out, float* save_freq_out);
  int ftx_ft8_select_npasses_c (int lapon, int lapcqonly, int ncontest,
                                int nzhsym, int nQSOProgress);
  void ftx_ft8_plan_pass_window_c (int requested_pass, int npasses,
                                   int* pass_first_out, int* pass_last_out);
  int ftx_ft8_ap_storico_passate_c ();
  int ftx_ft8_prepare_decode_pass_c (int ipass, int nQSOProgress, int lapcqonly, int ncontest,
                                     int nfqso, int nftx, float f1, int napwid,
                                     int const* apsym, int const* aph10,
                                     float const* llra, float const* llrb,
                                     float const* llrc, float const* llrd,
                                     float const* llre, float* llrz,
                                     int* apmask, int* iaptype_out);
  int ftx_ft8_prepare_cq_ap_pass_c (int ipass, int nQSOProgress, int lapcqonly, int ncontest,
                                    int nfqso, int nftx, float f1, int napwid,
                                    int const* apsym, int const* aph10,
                                    float const* llra, float const* llrb, float const* llrc,
                                    float* llrz, int* apmask, int* iaptype_out);
  int ftx_ft8_finalize_decode_pass_c (int nbadcrc, int pass_index, int iaptype_in,
                                      int* ipass_out, int* iaptype_out);
  int ftx_ft8_store_saved_decode_c (int ndecodes, int max_early,
                                    int nsnr, float f1, float xdt,
                                    int const* itone, int nn,
                                    int* allsnrs, float* f1_save,
                                    float* xdt_save, int* itone_save);
  void ftx_ft8_apply_saved_subtractions_c (float* dd, int const* itone_save,
                                           int nn, int ndec_early,
                                           float const* f1_save, float const* xdt_save,
                                           bool* lsubtracted,
                                           int initial_only, int refine);
  void legacy_pack77_split77_c (char const msg[37], int* nwords, int nw[19], char words[247]);
  void ftx_ft2_decode_and_emit_params_c (short const* iwave,
                                         params_block_t const* params,
                                         char const* temp_dir,
                                         int* decoded_count);
  void ftx_ft4_decode_and_emit_params_c (short const* iwave,
                                         params_block_t const* params,
                                         char const* temp_dir,
                                         int* decoded_count);
  void ftx_ft8_a7d_c (float* dd0, int* newdat, char const call_1[12], char const call_2[12],
                      char const grid4[4], float* xdt, float* f1, float* xbase,
                      int* nharderrors, float* dmin, char msg37[37], float* xsnr);
  void ftx_ft8_a8d_c (float* dd, char const mycall[12], char const dxcall[12],
                      char const dxgrid[6], float* f1a, float* xdt, float* fbest,
                      float* xsnr, float* plog, char msgbest[37]);
  void ftx_ft8_downsample_c (float const* dd, int* newdat, float f0, fftwf_complex* c1);
  void ftx_ft8var_downsample_c (float const* dd, int* newdat, float const* f0,
                                int const* nqso, fftwf_complex* c0, fftwf_complex* c2,
                                fftwf_complex* c3, int const* lhighsens,
                                int* lsubtracted, int* npos, float const* freqsub);
  void ftx_ft8_bitmetrics_scaled_c (std::complex<float> const* cd0, int np2, int ibest, int imetric,
                                    float scale, float* s8_out, int* nsync_out,
                                    float* llra, float* llrb, float* llrc, float* llrd, float* llre);
  void ftx_ft8_bitmetrics_deep_c (std::complex<float> const* cd0, int np2, int ibest, int imetric,
                                  float scale, float* s8_out, int* nsync_out,
                                  float* llra, float* llrb, float* llrc, float* llrd, float* llre);
  void ftx_ft8_bitmetrics_equalized_c (std::complex<float> const* cd0, int np2, int ibest,
                                       int imetric, float scale, float* s8_out,
                                       int* nsync_out, float* llra, float* llrb,
                                       float* llrc, float* llrd, float* llre);
  void ftx_ft8_bitmetrics_deep_equalized_c (std::complex<float> const* cd0, int np2,
                                            int ibest, int imetric, float scale,
                                            float* s8_out, int* nsync_out,
                                            float* llra, float* llrb, float* llrc,
                                            float* llrd, float* llre);
  void ftx_ft8_bitmetrics_capture_c (std::complex<float> const* cd0, int np2,
                                     int ibest, int imetric, float scale,
                                     int weak_deep, int equalize_tone_power,
                                     std::complex<float> const* history_cs,
                                     std::complex<float>* current_cs_out,
                                     float* s8_out, int* nsync_out,
                                     float* llra, float* llrb, float* llrc,
                                     float* llrd, float* llre);
  int ftx_ft8_a8_search_candidate_c (std::complex<float> const* cd,
                                     std::complex<float> const* cwave,
                                     int nzz, int nwave, float f1,
                                     float* spk_out, float* fpk_out,
                                     float* tpk_out, float* spectrum_out);
  int ftx_ft8_a8_finalize_search_c (float const* spectrum, int size, float f1,
                                    float fbest, float* xsnr_out);
  int ftx_ft8_a8_accept_score_c (int nhard, float plog, float sigobig);
  void ftx_ft8_a8_score_c (std::complex<float> const* cd, int nzz, float tbest,
                           int const* itone_best, float* plog, int* nhard,
                           float* sigobig);
  void ftx_twkfreq1_c (std::complex<float> const* ca, int const* npts, float const* fsample,
                       float const* a, std::complex<float>* cb);
  int ftx_prepare_ft8_a7_candidate_c (int imsg, char const call_1[12], char const call_2[12],
                                      char const grid4[4], char message37[37]);
  int ftx_prepare_ft8_a8_candidate_c (int imsg, char const mycall[12], char const hiscall[12],
                                      char const hisgrid[6], char message37[37]);
  int ftx_encode_ft8_candidate_c (char const* message37, char* msgsent_out,
                                  int* itone_out, signed char* codeword_out);
  void ftx_ft8a7_measure_candidate_c (float const* s8, int rows, int cols,
                                      int const* itone, signed char const* cw,
                                      float const* llra, float const* llrb,
                                      float const* llrc, float const* llrd,
                                      float* pow_out, float* dmin_out,
                                      int* nharderrors_out);
  void ftx_ft8_a7_search_initial_c (std::complex<float> const* cd0, int np2, float fs2,
                                    float xdt_in, int* ibest_out, float* delfbest_out);
  void ftx_ft8_a7_refine_search_c (std::complex<float> const* cd0, int np2, float fs2,
                                   int ibest_in, int* ibest_out, float* sync_out, float* xdt_out);
  int ftx_ft8_a7_finalize_metrics_c (float const* dmm, int count, float pbest, float xbase,
                                     float* dmin_out, float* dmin2_out, float* xsnr_out);
  void ftx_decode174_91_c (float const* llr_in, int Keff, int maxosd, int norder,
                           signed char const* apmask_in, signed char* message91_out,
                           signed char* cw_out, int* ntype_out, int* nharderror_out,
                           float* dmin_out);
  // Detector/fastldpc/: stessa firma, min-sum SIMD (AVX2/FMA su x86, NEON su
  // ARM64). Ricade da solo su ftx_decode174_91_c se il backend richiesto non
  // e' disponibile o Keff != 91.
  void fastldpc_decode174_91_c (float const* llr, int Keff, int maxosd, int norder,
                                signed char const* apmask, signed char* message91,
                                signed char* cw, int* ntype, int* nharderror, float* dmin);
  void fastldpc_set_ft8_mode_c (int on);
  void fastldpc_decode174_91_batch_c (int n, float const* llr, signed char const* apmask,
                                      int Keff, int maxosd, int norder,
                                      signed char* message91, signed char* cw,
                                      int* ntype, int* nharderror, float* dmin);
  int ftx_ft8_validate_candidate_meta_c (signed char const* message77, signed char const* cw,
                                         int nharderrors, int unpack_ok, int quirky, int ncontest);
  int ftx_ft8_compute_snr_c (float const* s8, int rows, int cols, int const* itone,
                             float xbase, int nagain, int nsync, float* xsnr_out);
  void ftx_ft8var_chkfalse8_c (char msg37[37], int const* i3_in, int const* n3_in,
                               int* nbadcrc_io, int const* iaptype_in,
                               int const* lcall1hash_in, char const mycall12[12],
                               char const hiscall12[12], char const hisgrid4[4]);
  void legacy_pack77_reset_context_c ();
  void legacy_pack77_set_context_c (char const mycall[13], char const hiscall[13]);
  void legacy_pack77_save_hash_call_c (char const c13[13], int* n10, int* n12, int* n22);
  void legacy_pack77_pack_c (char const msg0[37], int* i3, int* n3,
                             signed char c77[77], char msgsent[37],
                             bool* success, int received);
  void legacy_pack77_unpack28_c (int n28, char c13[13], bool* success);
  void legacy_pack77_unpacktext77_c (char const c71[71], char c13[13], bool* success);
  int legacy_pack77_unpack77bits_c (signed char const* message77, int received,
                                    char msgsent[37], int* quirky_out);
  int ftx_ft8_message77_to_itone_c (signed char const* message77, int* itone_out);
  int ftx_ft8sdvar_c (float const* s8, float srr, int const* itone_in,
                      char const msgd[37], char const mycall[12], int lcq,
                      char msg37_out[37], int itone_out[79]);
  void ftx_sfox_remove_ft8_c (float* dd, int npts);
  void sfox_remove_tone_ (std::complex<float>* c0, float* fsync);
  void qpc_decode2_ (std::complex<float>* c0, float* fsync, float* ftol,
                     signed char* xdec, int* ndepth, float* dth, float* damp,
                     int* crc_ok, float* snrsync, float* fbest, float* tbest, float* snr);
  void __ft8_a7_MOD_ft8_a7d (float* dd0, int* newdat, char* call_1, char* call_2,
                             char* grid4, float* xdt, float* f1, float* xbase,
                             int* nharderrors, float* dmin, char* msg37, float* xsnr,
                             size_t, size_t, size_t, size_t);
  void ft8_a8d_ (float* dd, char* mycall, char* dxcall, char* dxgrid,
                 float* f1a, float* xdt, float* fbest, float* xsnr,
                 float* plog, char* msgbest,
                 size_t, size_t, size_t, size_t);
}
namespace {

// Quale decoder LDPC usa FT8. ACCESO di default dal 28/08/2026.
// Attenzione, la differenza non e' solo di velocita': con maxosd=3 e
// norder=4 il decoder originale usa il BP ESATTO, mentre fastldpc e' sempre
// min-sum, che ne e' un'approssimazione. Il confronto appaiato sugli stessi
// wav (18 prove fra -20 e -22 dB) non ha mostrato perdite -- una sola
// discordanza, a favore di fastldpc, con p=1,00 al test dei segni -- ma non
// ha nemmeno dimostrato un guadagno: campione troppo piccolo per concludere.
// Si spegne con DECODIUM_FT8_FASTLDPC=0 senza ricompilare.
bool ft8_use_fastldpc ()
{
  static bool const on = [] {
    char const* raw = std::getenv ("DECODIUM_FT8_FASTLDPC");
    return !raw || (raw[0] != '0' && raw[0] != 0);
  }();
  return on;
}

// Decodifica a BLOCCHI delle passate FT8: acceso di default quando fastldpc e'
// attivo, si spegne con DECODIUM_FT8_BATCH=0 per tornare passata per passata.
// Forza il decoder classico per la chiamata in corso, ignorando fastldpc.
// Serve alla seconda passata di recupero: vedi il commento al suo punto d'uso.
thread_local bool g_ft8_forza_classico = false;

// Quanti recuperi col decoder classico si concedono per ciclo. Ognuno costa
// come un intero slot del decoder lento, quindi il tetto evita che una banda
// piena di candidati sterili se li mangi tutti. 0 disattiva il recupero.
int ft8_classic_rescue_budget ()
{
  static int const n = [] {
    char const* raw = std::getenv ("DECODIUM_FT8_CLASSIC_RESCUE");
    if (!raw) return 12;
    int const v = std::atoi (raw);
    return (v >= 0 && v <= 200) ? v : 12;
  }();
  return n;
}

int& ft8_classic_rescue_used ()
{
  static thread_local int used = 0;
  return used;
}

bool ft8_batch_passes ()
{
  static bool const on = [] {
    char const* raw = std::getenv ("DECODIUM_FT8_BATCH");
    return !raw || (raw[0] != '0' && raw[0] != 0);
  }();
  return on;
}

void ft8_ldpc_decode (float const* llr, int Keff, int maxosd, int norder,
                      signed char const* apmask, signed char* message91,
                      signed char* cw, int* ntype, int* nharderror, float* dmin)
{
  if (ft8_use_fastldpc () && !g_ft8_forza_classico)
    {
      // Tiene tutti i tipi di messaggio: i formati da contest che FT2 esclude
      // in FT8 esistono, e filtrarli via renderebbe il decoder cieco a quelli.
      fastldpc_set_ft8_mode_c (1);
      fastldpc_decode174_91_c (llr, Keff, maxosd, norder, apmask, message91, cw,
                               ntype, nharderror, dmin);
    }
  else
    ftx_decode174_91_c (llr, Keff, maxosd, norder, apmask, message91, cw,
                        ntype, nharderror, dmin);
}

}  // namespace


template <size_t N>
using FixedChars = std::array<char, N>;

template <size_t N>
FixedChars<N> blank_fixed ()
{
  FixedChars<N> out {};
  out.fill (' ');
  return out;
}

template <size_t N>
FixedChars<N> fixed_from_chars (char const* data)
{
  FixedChars<N> out = blank_fixed<N> ();
  if (data)
    {
      std::copy_n (data, N, out.data ());
    }
  return out;
}

template <size_t N>
FixedChars<N> fixed_from_string (std::string const& text)
{
  FixedChars<N> out = blank_fixed<N> ();
  std::copy_n (text.data (), std::min (static_cast<int>(text.size ()), static_cast<int>(N)), out.data ());
  return out;
}

template <size_t N>
std::string trim_fixed (FixedChars<N> const& value)
{
  size_t end = N;
  while (end > 0 && (value[end - 1] == ' ' || value[end - 1] == '\0'))
    {
      --end;
    }
  return std::string {value.data (), end};
}

std::string trim_block (char const* data, int width)
{
  int end = width;
  while (end > 0 && (data[end - 1] == ' ' || data[end - 1] == '\0'))
    {
      --end;
    }
  return std::string {data, data + end};
}

int trim_length (char const* data, int width)
{
  int end = width;
  while (end > 0 && (data[end - 1] == ' ' || data[end - 1] == '\0'))
    {
      --end;
    }
  return end;
}

float gfsk_pulse (float bt, float t)
{
  float const c = 0.5f * kFt8TwoPi * std::sqrt (2.0f / std::log (2.0f));
  return 0.5f * (std::erf (c * bt * (t + 0.5f)) - std::erf (c * bt * (t - 0.5f)));
}

float wrap_phase (float phase)
{
  phase = std::fmod (phase, kFt8TwoPi);
  if (phase < 0.0f)
    {
      phase += kFt8TwoPi;
    }
  return phase;
}

std::array<std::complex<float>, kFt8PhaseTableSize> const& phase_table ()
{
  static std::array<std::complex<float>, kFt8PhaseTableSize> table = [] {
    std::array<std::complex<float>, kFt8PhaseTableSize> value {};
    for (int i = 0; i < kFt8PhaseTableSize; ++i)
      {
        float const phase = static_cast<float> (i) * kFt8TwoPi
                            / static_cast<float> (kFt8PhaseTableSize);
        value[static_cast<size_t> (i)] = {
            std::cos (phase),
            std::sin (phase)
        };
      }
    return value;
  }();
  return table;
}

void generate_ft8_a8_waveform (int const* itone, float f0,
                               std::array<std::complex<float>, kFt8A8Nwave>& cwave)
{
  cwave.fill (std::complex<float> {});
  if (!itone || !std::isfinite (f0))
    {
      return;
    }

  static std::array<float, 3 * kFt8A8Nsps> const pulse = [] {
    std::array<float, 3 * kFt8A8Nsps> value {};
    for (int i = 0; i < 3 * kFt8A8Nsps; ++i)
      {
        float const tt = (static_cast<float> (i + 1) - 1.5f * static_cast<float> (kFt8A8Nsps))
                         / static_cast<float> (kFt8A8Nsps);
        value[static_cast<size_t> (i)] = gfsk_pulse (kFt8A8Bt, tt);
      }
    return value;
  }();

  std::array<float, kFt8A8DphiSize> dphi {};
  float const dphi_peak = kFt8TwoPi / static_cast<float> (kFt8A8Nsps);
  for (int j = 0; j < kFt8A8Nsym; ++j)
    {
      int const ib = j * kFt8A8Nsps;
      int const tone = std::max (0, std::min (7, itone[j]));
      for (int i = 0; i < 3 * kFt8A8Nsps; ++i)
        {
          dphi[static_cast<size_t> (ib + i)] += dphi_peak * pulse[static_cast<size_t> (i)]
                                                * static_cast<float> (tone);
        }
    }

  int const first_tone = std::max (0, std::min (7, itone[0]));
  int const last_tone = std::max (0, std::min (7, itone[kFt8A8Nsym - 1]));
  for (int i = 0; i < 2 * kFt8A8Nsps; ++i)
    {
      dphi[static_cast<size_t> (i)] += dphi_peak * static_cast<float> (first_tone)
                                       * pulse[static_cast<size_t> (kFt8A8Nsps + i)];
      dphi[static_cast<size_t> (kFt8A8Nsym * kFt8A8Nsps + i)] +=
          dphi_peak * static_cast<float> (last_tone) * pulse[static_cast<size_t> (i)];
    }

  float const carrier_step = kFt8TwoPi * f0 / kFt8A7Fs2;
  auto const& table = phase_table ();
  float phi = 0.0f;
  for (int j = kFt8A8Nsps, k = 0; j < kFt8A8Nsps + kFt8A8Nwave; ++j, ++k)
    {
      int const index = std::max (
          0,
          std::min (kFt8PhaseTableSize - 1,
                    static_cast<int> (phi * static_cast<float> (kFt8PhaseTableSize) / kFt8TwoPi)));
      cwave[static_cast<size_t> (k)] = table[static_cast<size_t> (index)];
      phi = wrap_phase (phi + dphi[static_cast<size_t> (j)] + carrier_step);
    }

  int const nramp = std::max (1, static_cast<int> (std::lround (kFt8A8Nsps / 8.0f)));
  for (int i = 0; i < nramp; ++i)
    {
      float const env = 0.5f * (1.0f - std::cos (kFt8TwoPi * static_cast<float> (i)
                                                 / (2.0f * static_cast<float> (nramp))));
      cwave[static_cast<size_t> (i)] *= env;
    }
  int const tail_start = kFt8A8Nsym * kFt8A8Nsps - nramp;
  for (int i = 0; i < nramp; ++i)
    {
      float const env = 0.5f * (1.0f + std::cos (kFt8TwoPi * static_cast<float> (i)
                                                 / (2.0f * static_cast<float> (nramp))));
      cwave[static_cast<size_t> (tail_start + i)] *= env;
    }
}

template <size_t N>
bool starts_with (FixedChars<N> const& value, char const* prefix)
{
  size_t const length = std::strlen (prefix);
  return length <= N && std::equal (prefix, prefix + length, value.begin ());
}

bool is_standard_call (char const* data, int width)
{
  auto is_digit = [] (char c) {
    return c >= '0' && c <= '9';
  };
  auto is_letter = [] (char c) {
    return c >= 'A' && c <= 'Z';
  };

  std::string const call = trim_block (data, width);
  if (call.empty () || call.size () > 11)
    {
      return false;
    }
  if (call.find ('.') != std::string::npos
      || call.find ('+') != std::string::npos
      || call.find ('-') != std::string::npos
      || call.find ('?') != std::string::npos)
    {
      return false;
    }
  if (call.size () > 7 && call.find ('/') == std::string::npos)
    {
      return false;
    }

  std::string base = call;
  size_t const slash = call.find ('/');
  if (slash != std::string::npos && slash >= 1 && slash + 1 < call.size ())
    {
      size_t const left = slash;
      size_t const right = call.size () - slash - 1;
      base = left <= right ? call.substr (slash + 1) : call.substr (0, slash);
    }
  if (base.size () > 7)
    {
      return false;
    }
  if ((base.size () < 1 || !is_letter (base[0]))
      && (base.size () < 2 || !is_letter (base[1])))
    {
      return false;
    }
  if (base[0] == 'Q' && base.rfind ("QU1RK", 0) != 0)
    {
      return false;
    }

  int digit_index = -1;
  for (size_t index = 1; index < base.size () && index <= 3; ++index)
    {
      if (is_digit (base[index]))
        {
          digit_index = static_cast<int> (index);
        }
    }
  if (digit_index < 0 || digit_index + 1 >= static_cast<int> (base.size ()))
    {
      return false;
    }

  int suffix_letters = 0;
  for (size_t i = static_cast<size_t> (digit_index + 1); i < base.size (); ++i)
    {
      if (!is_letter (base[i]))
        {
          return false;
        }
      ++suffix_letters;
    }
  return suffix_letters >= 1 && suffix_letters <= 3;
}

bool is_grid4 (std::string const& grid)
{
  auto between = [] (char value, char lo, char hi) {
    return value >= lo && value <= hi;
  };

  return grid.size () == 4
      && between (grid[0], 'A', 'R')
      && between (grid[1], 'A', 'R')
      && between (grid[2], '0', '9')
      && between (grid[3], '0', '9');
}

bool is_standard_call_word (std::string const& word)
{
  if (word.empty () || word.size () > 12)
    {
      return false;
    }

  FixedChars<12> call = blank_fixed<12> ();
  std::copy_n (word.data (), word.size (), call.data ());
  return is_standard_call (call.data (), static_cast<int> (call.size ()));
}

bool is_cq_modifier (std::string const& word)
{
  bool const alpha_modifier =
      word.size () >= 2
      && word.size () <= 4
      && std::all_of (word.begin (), word.end (), [] (char ch) {
           return ch >= 'A' && ch <= 'Z';
         });
  return word == "DX"
      || word == "NA"
      || word == "SA"
      || word == "EU"
      || word == "AF"
      || word == "AS"
      || word == "OC"
      || word == "POTA"
      || word == "SOTA"
      || word == "QRP"
      || word == "IOTA"
      || word == "FD"
      || word == "WW"
      || word == "LCT"
      || word == "TEST"
      || alpha_modifier;
}

bool is_report_token (std::string const& word)
{
  if (word == "RRR" || word == "RR73" || word == "73")
    {
      return true;
    }
  size_t offset = 0;
  if (!word.empty () && word[0] == 'R')
    {
      offset = 1;
    }
  if (word.size () != offset + 3)
    {
      return false;
    }
  return (word[offset] == '-' || word[offset] == '+')
      && word[offset + 1] >= '0' && word[offset + 1] <= '9'
      && word[offset + 2] >= '0' && word[offset + 2] <= '9';
}

bool is_standard_ft8_exchange_tail (std::string const& word)
{
  return is_grid4 (word) || is_report_token (word);
}

std::vector<std::string> split_words (std::string const& text);
bool is_packable_cq_call_message (std::string const& call);
bool is_ft8_call_word (std::string const& word);

// Nominativo strutturalmente impossibile: barra iniziale, finale o doppia.
// Nessun nominativo valido ha quella forma. E' un test di FORMA, non una
// soglia, quindi non costa nulla ai segnali deboli: su 649836 decodifiche
// d'archivio ne compaiono due in tutto ("CQ M8K0DSOHIW/" a -15 dB e
// "CQ X6NJ9B3WZY/" a -23 dB), entrambe false. Un criterio piu' severo sulla
// struttura prefisso/cifra/suffisso e' stato provato e scartato: avrebbe
// respinto il 4% dei nominativi veri (4L7T, 2E1HAT, 9A6NTK, 7Z1FF, 3B8GL...),
// che hanno la cifra dentro il prefisso.
bool has_malformed_call_word (FixedChars<kFt8DecodedChars> const& decoded)
{
  for (std::string const& word : split_words (trim_fixed (decoded)))
    {
      size_t const first = word.find ('/');
      if (first == std::string::npos)
        {
          continue;
        }
      if (first == 0
          || first + 1 >= word.size ()
          || word.find ('/', first + 1) != std::string::npos)
        {
          return true;
        }
    }
  return false;
}

bool is_strict_standard_ft8_message (FixedChars<kFt8DecodedChars> const& decoded)
{
  std::vector<std::string> const words = split_words (trim_fixed (decoded));
  if (words.size () < 2 || words.size () > 4)
    {
      return false;
    }

  if (words[0] == "CQ")
    {
      size_t callIndex = 1;
      if (words.size () >= 3 && is_cq_modifier (words[1]))
        {
          callIndex = 2;
        }
      if (callIndex >= words.size ())
        {
          return false;
        }
      bool const plain_packable_cq_call =
          callIndex == 1
          && words.size () == 2
          && is_packable_cq_call_message (words[callIndex]);
      if (!is_standard_call_word (words[callIndex]) && !plain_packable_cq_call)
        {
          return false;
        }
      return callIndex + 1 == words.size ()
          || (callIndex + 2 == words.size () && is_grid4 (words[callIndex + 1]));
    }

  if (!is_ft8_call_word (words[0]) || !is_ft8_call_word (words[1]))
    {
      return false;
    }
  if (words.size () == 2)
    {
      return true;
    }
  if (!is_standard_ft8_exchange_tail (words[2]))
    {
      return false;
    }
  return words.size () == 3
      || (words.size () == 4
          && words[3] == "TU"
          && is_report_token (words[2]));
}

bool is_directed_pair_only_message (FixedChars<kFt8DecodedChars> const& decoded)
{
  std::vector<std::string> const words = split_words (trim_fixed (decoded));
  return words.size () == 2
      && words[0] != "CQ"
      && is_ft8_call_word (words[0])
      && is_ft8_call_word (words[1]);
}

bool is_hash_call_placeholder_word (std::string const& word)
{
  if (word == "<...>")
    {
      return true;
    }
  if (word.size () <= 2 || word.front () != '<' || word.back () != '>')
    {
      return false;
  }
  std::string const inner = word.substr (1, word.size () - 2);
  return is_standard_call_word (inner)
      || is_packable_cq_call_message (inner);
}

bool is_resolved_hash_call_word (std::string const& word)
{
  if (word.size () < 3 || word.size () > 13)
    {
      return false;
    }
  if (word == "CQ" || word == "DE" || word == "QRZ" || word == "RRR"
      || word == "RR73" || word == "73" || word == "TU")
    {
      return false;
    }
  if (is_grid4 (word) || is_report_token (word))
    {
      return false;
    }

  auto valid_core = [] (std::string const& core) {
    if (core.size () < 3 || core.size () > 9)
      {
        return false;
      }
    size_t first_digit = std::string::npos;
    size_t last_digit = std::string::npos;
    for (size_t index = 0; index < core.size (); ++index)
      {
        if (core[index] >= '0' && core[index] <= '9')
          {
            if (first_digit == std::string::npos)
              {
                first_digit = index;
              }
            last_digit = index;
          }
      }
    if (first_digit == std::string::npos || first_digit < 1
        || last_digit + 1 >= core.size ())
      {
        return false;
      }
    if (first_digit > 3 || (last_digit - first_digit + 1) > 4)
      {
        return false;
      }
    bool prefix_has_letter = false;
    for (size_t index = 0; index < first_digit; ++index)
      {
        char const ch = core[index];
        if (ch < 'A' || ch > 'Z')
          {
            return false;
          }
        prefix_has_letter = true;
      }
    for (size_t index = first_digit; index <= last_digit; ++index)
      {
        char const ch = core[index];
        if (ch < '0' || ch > '9')
          {
            return false;
          }
      }
    int suffix_letters = 0;
    for (size_t index = last_digit + 1; index < core.size (); ++index)
      {
        char const ch = core[index];
        if (ch < 'A' || ch > 'Z')
          {
            return false;
          }
        ++suffix_letters;
      }
    return prefix_has_letter && suffix_letters >= 1 && suffix_letters <= 5;
  };

  size_t const slash = word.find ('/');
  if (slash == std::string::npos)
    {
      return valid_core (word);
    }
  if (slash == 0 || slash + 1 >= word.size ()
      || word.find ('/', slash + 1) != std::string::npos)
    {
      return false;
    }
  std::string const left = word.substr (0, slash);
  std::string const right = word.substr (slash + 1);
  auto valid_affix = [] (std::string const& affix) {
    if (affix.empty () || affix.size () > 4)
      {
        return false;
      }
    return std::all_of (affix.begin (), affix.end (), [] (char ch) {
      return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
    });
  };
  return (valid_core (left) && valid_affix (right))
      || (valid_core (right) && valid_affix (left));
}

bool is_standard_or_hash_call_word (std::string const& word)
{
  return is_standard_call_word (word)
      || is_hash_call_placeholder_word (word)
      || is_resolved_hash_call_word (word);
}

bool is_ft8_call_word (std::string const& word)
{
  // FT8 type-4 can carry a raw non-standard callsign (for example the
  // Indonesian special-event calls 8A81JK and 8B81JB).  Such calls are not
  // standard 28-bit calls and are therefore not covered by the standard/hash
  // predicate above, but they are valid when the encoder can round-trip them
  // as a CQ call.
  return is_standard_or_hash_call_word (word)
      || is_packable_cq_call_message (word);
}

bool ft8_word_has_valid_punctuation (std::string const& word)
{
  if (word.find (';') != std::string::npos
      || word.find (':') != std::string::npos
      || word.find (',') != std::string::npos)
    {
      return false;
    }
  if (word.find ('.') != std::string::npos && word != "<...>")
    {
      return false;
    }
  bool const has_report_sign =
      word.find ('-') != std::string::npos || word.find ('+') != std::string::npos;
  return !has_report_sign || is_report_token (word);
}

bool is_plausible_ft8_message_for_emit (FixedChars<kFt8DecodedChars> const& decoded)
{
  std::vector<std::string> const words = split_words (trim_fixed (decoded));
  if (words.size () < 2 || words.size () > 4)
    {
      return false;
    }
  for (std::string const& word : words)
    {
      if (!ft8_word_has_valid_punctuation (word))
        {
          return false;
        }
    }

  if (words[0] == "CQ")
    {
      size_t call_index = 1;
      if (words.size () >= 3 && is_cq_modifier (words[1]))
        {
          call_index = 2;
        }
      if (call_index >= words.size ()
          || !is_ft8_call_word (words[call_index]))
        {
          return false;
        }
      return call_index + 1 == words.size ()
          || (call_index + 2 == words.size () && is_grid4 (words[call_index + 1]));
    }

  if (!is_ft8_call_word (words[0])
      || !is_ft8_call_word (words[1]))
    {
      return false;
    }
  if (words.size () == 2)
    {
      return true;
    }
  if (words.size () == 3)
    {
      return is_grid4 (words[2]) || is_report_token (words[2]);
    }
  return words.size () == 4
      && is_report_token (words[2])
      && words[3] == "TU";
}

FixedChars<kFt8DecodedChars> normalize_resolved_hash_call_tokens (
    FixedChars<kFt8DecodedChars> const& decoded)
{
  std::string const trimmed = trim_fixed (decoded);
  std::vector<std::string> words = split_words (trimmed);
  if (words.empty ())
    {
      return decoded;
    }

  bool changed = false;
  for (std::string& word : words)
    {
      if (word.size () <= 2 || word.front () != '<' || word.back () != '>')
        {
          continue;
        }
      std::string const inner = word.substr (1, word.size () - 2);
      bool has_letter = false;
      bool has_digit = false;
      bool call_alphabet = true;
      for (char const ch : inner)
        {
          has_letter = has_letter || (ch >= 'A' && ch <= 'Z');
          has_digit = has_digit || (ch >= '0' && ch <= '9');
          call_alphabet = call_alphabet
              && ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '/');
        }
      bool const resolved_call_like =
          inner.size () >= 3
          && inner.size () <= 13
          && has_letter
          && has_digit
          && call_alphabet
          && !is_grid4 (inner)
          && !is_report_token (inner);
      if (inner == "..." || !resolved_call_like)
        {
          continue;
        }
      word = inner;
      changed = true;
    }
  if (!changed)
    {
      return decoded;
    }

  std::string normalized;
  for (std::string const& word : words)
    {
      if (!normalized.empty ())
        {
          normalized.push_back (' ');
        }
      normalized += word;
    }
  return fixed_from_string<kFt8DecodedChars> (normalized);
}

bool has_unresolved_hash_placeholder (FixedChars<kFt8DecodedChars> const& decoded)
{
  return trim_fixed (decoded).find ("<...>") != std::string::npos;
}

std::vector<std::string> split_words (std::string const& text)
{
  std::istringstream stream {text};
  std::vector<std::string> words;
  std::string word;
  while (stream >> word)
    {
      words.push_back (word);
    }
  return words;
}

std::mutex& pack77_hash_seed_mutex ()
{
  static std::mutex mutex;
  return mutex;
}

std::vector<std::string>& pack77_hash_seed_cache ()
{
  static std::vector<std::string> cache;
  return cache;
}

std::vector<std::string>& pack77_hash_external_seed_cache ()
{
  static std::vector<std::string> cache;
  return cache;
}

bool debug_known_cq_replay ()
{
  static bool const enabled = std::getenv ("DECODIUM_DEBUG_KNOWN_CQ") != nullptr;
  return enabled;
}

bool debug_ft8_focus_replay ()
{
  static bool const enabled = std::getenv ("DECODIUM_DEBUG_FT8_FOCUS") != nullptr;
  return enabled;
}

struct Ft8ExpectedTarget
{
  int nutc {};
  float freq {};
  float dt {};
  std::string message;
};

std::string trim_ascii_string (std::string text)
{
  while (!text.empty ()
         && std::isspace (static_cast<unsigned char> (text.front ())) != 0)
    {
      text.erase (text.begin ());
    }
  while (!text.empty ()
         && std::isspace (static_cast<unsigned char> (text.back ())) != 0)
    {
      text.pop_back ();
    }
  return text;
}

std::string normalize_expected_target_text (std::string text)
{
  text = trim_ascii_string (text);
  std::string out;
  bool pending_space = false;
  for (char ch : text)
    {
      unsigned char const uch = static_cast<unsigned char> (ch);
      if (std::isspace (uch) != 0)
        {
          pending_space = !out.empty ();
          continue;
        }
      if (pending_space)
        {
          out.push_back (' ');
          pending_space = false;
        }
      out.push_back (static_cast<char> (std::toupper (uch)));
    }
  return out;
}

std::vector<Ft8ExpectedTarget> parse_ft8_expected_targets ()
{
  std::vector<Ft8ExpectedTarget> targets;
  char const* env = std::getenv ("DECODIUM_FT8_EXPECTED_TARGETS");
  if (!env || *env == '\0')
    {
      env = std::getenv ("DECODIUM_FT8_TARGET_TRACE");
    }
  if (!env || *env == '\0')
    {
      return targets;
    }

  std::istringstream entries {env};
  std::string entry;
  while (std::getline (entries, entry, ';'))
    {
      entry = trim_ascii_string (entry);
      if (entry.empty ())
        {
          continue;
        }
      size_t const p1 = entry.find (',');
      size_t const p2 = p1 == std::string::npos ? std::string::npos : entry.find (',', p1 + 1);
      size_t const p3 = p2 == std::string::npos ? std::string::npos : entry.find (',', p2 + 1);
      if (p1 == std::string::npos || p2 == std::string::npos || p3 == std::string::npos)
        {
          continue;
        }

      Ft8ExpectedTarget target;
      target.nutc = std::atoi (trim_ascii_string (entry.substr (0, p1)).c_str ());
      target.freq = static_cast<float> (std::atof (trim_ascii_string (entry.substr (p1 + 1, p2 - p1 - 1)).c_str ()));
      target.dt = static_cast<float> (std::atof (trim_ascii_string (entry.substr (p2 + 1, p3 - p2 - 1)).c_str ()));
      target.message = normalize_expected_target_text (entry.substr (p3 + 1));
      if (target.nutc > 0 && target.freq > 0.0f && !target.message.empty ())
        {
          targets.push_back (target);
        }
    }
  return targets;
}

std::vector<Ft8ExpectedTarget> const& ft8_expected_targets ()
{
  static std::vector<Ft8ExpectedTarget> const targets = parse_ft8_expected_targets ();
  return targets;
}

float ft8_expected_target_freq_window ()
{
  static float const value = [] {
    char const* env = std::getenv ("DECODIUM_FT8_TARGET_FREQ_WINDOW");
    float const parsed = env ? static_cast<float> (std::atof (env)) : 4.0f;
    return (parsed > 0.1f && parsed <= 25.0f) ? parsed : 4.0f;
  }();
  return value;
}

float ft8_expected_target_dt_window ()
{
  static float const value = [] {
    char const* env = std::getenv ("DECODIUM_FT8_TARGET_DT_WINDOW");
    float const parsed = env ? static_cast<float> (std::atof (env)) : 0.65f;
    return (parsed > 0.05f && parsed <= 3.0f) ? parsed : 0.65f;
  }();
  return value;
}

bool ft8_expected_target_matches_signal (Ft8ExpectedTarget const& target, int nutc,
                                         float freq, float callback_dt)
{
  return target.nutc == nutc
      && std::fabs (freq - target.freq) <= ft8_expected_target_freq_window ()
      && std::fabs (callback_dt - target.dt) <= ft8_expected_target_dt_window ();
}

Ft8ExpectedTarget const* find_ft8_expected_target (int nutc, float freq,
                                                   float callback_dt)
{
  for (Ft8ExpectedTarget const& target : ft8_expected_targets ())
    {
      if (ft8_expected_target_matches_signal (target, nutc, freq, callback_dt))
        {
          return &target;
        }
    }
  return nullptr;
}

std::mutex& ft8_expected_target_trace_mutex ()
{
  static std::mutex mutex;
  return mutex;
}

template <typename Writer>
void trace_ft8_expected_target (Ft8ExpectedTarget const& target, char const* stage,
                                Writer&& writer)
{
  std::lock_guard<std::mutex> lock {ft8_expected_target_trace_mutex ()};
  std::cerr << "[FT8TARGET] stage=" << stage
            << " utc=" << target.nutc
            << " target_freq=" << target.freq
            << " target_dt=" << target.dt
            << " target_msg=\"" << target.message << '"';
  writer (std::cerr);
  std::cerr << '\n';
}

bool ft8_expected_message_matches (Ft8ExpectedTarget const& target,
                                   std::string const& decoded)
{
  return normalize_expected_target_text (decoded) == target.message;
}

void trace_ft8_expected_candidate_list (int nutc, int ipass, int ifa, int ifb,
                                        float syncmin, int imetric, int lsubtract,
                                        int run_pass, float const* candidate,
                                        int ncand, bool shifted_pass,
                                        bool fp_isolate, int fp_bins)
{
  if (!candidate || ft8_expected_targets ().empty ())
    {
      return;
    }

  for (Ft8ExpectedTarget const& target : ft8_expected_targets ())
    {
      if (target.nutc != nutc)
        {
          continue;
        }

      int best_index = -1;
      int in_window = 0;
      float best_score = 1.0e30f;
      float best_freq = 0.0f;
      float best_raw_dt = 0.0f;
      float best_callback_dt = 0.0f;
      float best_sync = 0.0f;
      float best_cq = 0.0f;
      float const freq_window = ft8_expected_target_freq_window ();
      float const dt_window = ft8_expected_target_dt_window ();
      for (int index = 0; index < ncand; ++index)
        {
          float const freq = candidate[static_cast<size_t> (index * 4)];
          float const raw_dt = candidate[static_cast<size_t> (index * 4 + 1)];
          float const callback_dt = raw_dt - 0.5f;
          float const df = std::fabs (freq - target.freq);
          float const ddt = std::fabs (callback_dt - target.dt);
          if (df <= freq_window && ddt <= dt_window)
            {
              ++in_window;
            }
          float const score = df / std::max (freq_window, 0.1f)
                              + ddt / std::max (dt_window, 0.05f);
          if (score < best_score)
            {
              best_score = score;
              best_index = index;
              best_freq = freq;
              best_raw_dt = raw_dt;
              best_callback_dt = callback_dt;
              best_sync = candidate[static_cast<size_t> (index * 4 + 2)];
              best_cq = candidate[static_cast<size_t> (index * 4 + 3)];
            }
        }

      trace_ft8_expected_target (target, "candidate-list",
                                 [&] (std::ostream& out) {
        out << " pass=" << ipass
            << " ifa=" << ifa
            << " ifb=" << ifb
            << " syncmin=" << syncmin
            << " imetric=" << imetric
            << " lsubtract=" << lsubtract
            << " run_pass=" << run_pass
            << " ncand=" << ncand
            << " in_window=" << in_window
            << " shifted=" << (shifted_pass ? 1 : 0)
            << " fp_isolate=" << (fp_isolate ? 1 : 0)
            << " fp_bins=" << fp_bins;
        if (best_index >= 0)
          {
            out << " best_index=" << best_index
                << " best_freq=" << best_freq
                << " best_dt_raw=" << best_raw_dt
                << " best_dt_cb=" << best_callback_dt
                << " best_df=" << (best_freq - target.freq)
                << " best_ddt=" << (best_callback_dt - target.dt)
                << " best_sync=" << best_sync
                << " best_cq=" << best_cq;
          }
        else
          {
            out << " best_index=-1";
          }
      });
    }
}

int ft8_freqpart_bins ()
{
  static int const env_bins = [] {
    char const* v = std::getenv ("DECODIUM_FT8_FREQPART_BINS");
    return v ? std::max (0, std::min (32, std::atoi (v))) : 0;
  }();
  return std::max (env_bins, stage4_freqpart_request ().load (std::memory_order_relaxed));
}

bool ft8_freqpart_isolate ()
{
  static bool const env_enabled = std::getenv ("DECODIUM_FT8_FREQPART_ISOLATE") != nullptr;
  return env_enabled || stage4_freqpart_request ().load (std::memory_order_relaxed) > 0;
}

float ft8_syncmin_scale_override ()
{
  static float const env_scale = [] {
    char const* v = std::getenv ("DECODIUM_FT8_SYNCMIN_SCALE");
    if (!v) return -1.0f;
    float const s = static_cast<float> (std::atof (v));
    return (s > 0.0f && s <= 2.0f) ? s : -1.0f;
  }();
  if (env_scale > 0.0f) return env_scale;
  return stage4_syncmin_scale_request ().load (std::memory_order_relaxed);
}

int ft8_decode_syncmin_override ()
{
  static int const env_ov = [] {
    char const* v = std::getenv ("DECODIUM_FT8_DECODE_SYNCMIN");
    return v ? std::max (0, std::min (10, std::atoi (v))) : -2;
  }();
  if (env_ov != -2) return env_ov;
  return stage4_decode_syncmin_request ().load (std::memory_order_relaxed);
}

std::string sanitize_pack77_hash_call_seed (std::string word)
{
  while (!word.empty () && (word.front () == '<' || word.front () == ';' || word.front () == ','))
    {
      word.erase (word.begin ());
    }
  while (!word.empty () && (word.back () == '>' || word.back () == ';' || word.back () == ','))
    {
      word.pop_back ();
    }
  return word;
}

bool looks_like_pack77_hash_call_seed (std::string const& call)
{
  if (call.size () < 3 || call.size () > 13 || call == "...")
    {
      return false;
    }
  if (call == "CQ" || call == "DE" || call == "QRZ" || call == "RRR"
      || call == "RR73" || call == "73" || call == "TU")
    {
      return false;
    }
  if (is_grid4 (call) || is_report_token (call))
    {
      return false;
    }

  bool has_letter = false;
  bool has_digit = false;
  for (char const ch : call)
    {
      if (ch >= 'A' && ch <= 'Z')
        {
          has_letter = true;
          continue;
        }
      if (ch >= '0' && ch <= '9')
        {
          has_digit = true;
          continue;
        }
      if (ch == '/')
        {
          continue;
        }
      return false;
    }
  return has_letter && has_digit;
}

void apply_pack77_hash_call_seed (std::string const& call)
{
  FixedChars<13> call13 = fixed_from_string<13> (call);
  legacy_pack77_save_hash_call_c (call13.data (), nullptr, nullptr, nullptr);
}

void remember_pack77_hash_call_seed (std::string const& call)
{
  std::lock_guard<std::mutex> lock {pack77_hash_seed_mutex ()};
  auto& cache = pack77_hash_seed_cache ();
  cache.erase (std::remove (cache.begin (), cache.end (), call), cache.end ());
  cache.push_back (call);
  if (cache.size () > kFt8HashCallSeedMemory)
    {
      cache.erase (cache.begin (),
                   cache.begin () + static_cast<std::ptrdiff_t> (cache.size ()
                                                                 - kFt8HashCallSeedMemory));
    }
}

void remember_pack77_hash_external_seed (std::string const& call)
{
  std::lock_guard<std::mutex> lock {pack77_hash_seed_mutex ()};
  auto& cache = pack77_hash_external_seed_cache ();
  cache.erase (std::remove (cache.begin (), cache.end (), call), cache.end ());
  cache.push_back (call);
  if (cache.size () > kFt8HashCallSeedMemory)
    {
      cache.erase (cache.begin (),
                   cache.begin () + static_cast<std::ptrdiff_t> (cache.size ()
                                                                 - kFt8HashCallSeedMemory));
    }
}

void seed_pack77_hash_call (std::string const& raw_call, bool remember, bool external_seed = false)
{
  std::string const call = sanitize_pack77_hash_call_seed (raw_call);
  if (!looks_like_pack77_hash_call_seed (call))
    {
      return;
    }

  apply_pack77_hash_call_seed (call);
  if (remember)
    {
      remember_pack77_hash_call_seed (call);
      if (external_seed)
        {
          remember_pack77_hash_external_seed (call);
        }
    }
}

void seed_pack77_hashes_from_message (FixedChars<kFt8DecodedChars> const& decoded)
{
  for (std::string const& word : split_words (trim_fixed (decoded)))
    {
      seed_pack77_hash_call (word, true);
    }
}

void apply_pack77_hash_seed_cache ()
{
  std::vector<std::string> seeds;
  std::vector<std::string> external_seeds;
  {
    std::lock_guard<std::mutex> lock {pack77_hash_seed_mutex ()};
    seeds = pack77_hash_seed_cache ();
    external_seeds = pack77_hash_external_seed_cache ();
  }
  for (std::string const& call : seeds)
    {
      apply_pack77_hash_call_seed (call);
    }
  for (std::string const& call : external_seeds)
    {
      apply_pack77_hash_call_seed (call);
    }
}

void apply_pack77_hash_external_seed_cache ()
{
  std::vector<std::string> external_seeds;
  {
    std::lock_guard<std::mutex> lock {pack77_hash_seed_mutex ()};
    external_seeds = pack77_hash_external_seed_cache ();
  }
  for (std::string const& call : external_seeds)
    {
      apply_pack77_hash_call_seed (call);
    }
}

std::string format_ft8_stdout_line (int nutc, float sync, int snr, float dt, float freq,
                                    std::string const& decoded, int nap, float qual)
{
  (void) sync;
  std::string decoded_copy = decoded;
  if (decoded_copy.size () < kFt8DecodedChars)
    {
      decoded_copy.append (kFt8DecodedChars - decoded_copy.size (), ' ');
    }
  else if (decoded_copy.size () > kFt8DecodedChars)
    {
      decoded_copy.resize (kFt8DecodedChars);
    }

  std::string annot {"  "};
  if (nap != 0)
    {
      annot[0] = 'a';
      annot[1] = static_cast<char> ('0' + std::max (0, std::min (9, nap)));
      if (qual < 0.17f && !decoded_copy.empty ())
        {
          decoded_copy.back () = '?';
        }
    }

  std::ostringstream line;
  line << std::setfill ('0') << std::setw (6) << nutc
       << std::setfill (' ') << std::setw (4) << snr
       << std::fixed << std::setprecision (1) << std::setw (5) << dt
       << std::setw (5) << static_cast<int> (std::lround (freq))
       << " ~ " << ' ' << decoded_copy << ' ' << annot;
  return line.str ();
}

std::string format_ft8_decoded_file_line (int nutc, float sync, int snr, float dt, float freq,
                                          std::string const& decoded, int nap, float qual)
{
  std::string decoded_copy = decoded;
  if (decoded_copy.size () < kFt8DecodedChars)
    {
      decoded_copy.append (kFt8DecodedChars - decoded_copy.size (), ' ');
    }
  else if (decoded_copy.size () > kFt8DecodedChars)
    {
      decoded_copy.resize (kFt8DecodedChars);
    }
  if (nap != 0 && qual < 0.17f && !decoded_copy.empty ())
    {
      decoded_copy.back () = '?';
    }

  std::ostringstream line;
  line << std::setfill ('0') << std::setw (6) << nutc
       << std::setfill (' ') << std::setw (4) << static_cast<int> (std::lround (sync))
       << std::setw (5) << snr
       << std::fixed << std::setprecision (1) << std::setw (6) << dt
       << std::fixed << std::setprecision (0) << std::setw (8) << freq
       << std::setw (4) << 0
       << "   " << decoded_copy << " FT8";
  return line.str ();
}

constexpr int kSuperFoxPackedSymbols {50};
constexpr int kSuperFoxMaxDecodeLines {16};
constexpr int kSuperFoxNqu1Rks {203514677};
constexpr int kSuperFoxNFilt {8000};
constexpr int kSuperFoxToneFrameSamples {50 * 3456};
constexpr int kSuperFoxQpcRows {128};
constexpr int kSuperFoxQpcCols {128};
constexpr int kSuperFoxDemodSymbols {151};
constexpr int kSuperFoxDemodNsps {1024};
constexpr int kSuperFoxDemodSpectrumCols {kSuperFoxDemodSymbols + 1};
constexpr int kSuperFoxQpcSearchCount {100};
constexpr int kSuperFoxSyncSamples {9 * 12000};
constexpr int kSuperFoxSyncDown {16};
constexpr int kSuperFoxSyncNz {kSuperFoxSyncSamples / kSuperFoxSyncDown};
constexpr int kSuperFoxSyncSymbols {24};
constexpr std::array<int, kSuperFoxSyncSymbols> kSuperFoxIsync {
    1, 2, 4, 7, 11, 16, 22, 29, 37, 39, 42, 43,
    45, 48, 52, 57, 63, 70, 78, 80, 83, 84, 86, 89};
constexpr std::array<int, 8> kSuperFoxMaxDither {
    20, 50, 100, 200, 500, 1000, 2000, 5000};
constexpr std::array<int, kSuperFoxQpcSearchCount> kSuperFoxIdf {
    0,  0, -1,  0, -1,  1,  0, -1,  1, -2,  0, -1,  1, -2,  2,
    0, -1,  1, -2,  2, -3,  0, -1,  1, -2,  2, -3,  3,  0, -1,
    1, -2,  2, -3,  3, -4,  0, -1,  1, -2,  2, -3,  3, -4,  4,
    0, -1,  1, -2,  2, -3,  3, -4,  4, -5, -1,  1, -2,  2, -3,
    3, -4,  4, -5,  1, -2,  2, -3,  3, -4,  4, -5, -2,  2, -3,
    3, -4,  4, -5,  2, -3,  3, -4,  4, -5, -3,  3, -4,  4, -5,
    3, -4,  4, -5, -4,  4, -5,  4, -5, -5};
constexpr std::array<int, kSuperFoxQpcSearchCount> kSuperFoxIdt {
    0, -1,  0,  1, -1,  0, -2,  1, -1,  0,  2, -2,  1, -1,  0,
   -3,  2, -2,  1, -1,  0,  3, -3,  2, -2,  1, -1,  0, -4,  3,
   -3,  2, -2,  1, -1,  0,  4, -4,  3, -3,  2, -2,  1, -1,  0,
   -5,  4, -4,  3, -3,  2, -2,  1, -1,  0, -5,  4, -4,  3, -3,
    2, -2,  1, -1, -5,  4, -4,  3, -3,  2, -2,  1, -5,  4, -4,
    3, -3,  2, -2, -5,  4, -4,  3, -3,  2, -5,  4, -4,  3, -3,
   -5,  4, -4,  3, -5,  4, -4, -5,  4, -5};
constexpr char kSuperFoxBase38[] {" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/"};

struct SuperFoxComplexFft
{
  SuperFoxComplexFft ()
    : data (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * kFt8NMax))),
      forward (nullptr),
      inverse (nullptr)
  {
    if (data)
      {
        forward = decodium::fft_compat::plan_dft_1d (kFt8NMax, data, data, FFTW_FORWARD, FFTW_ESTIMATE);
        inverse = decodium::fft_compat::plan_dft_1d (kFt8NMax, data, data, FFTW_BACKWARD, FFTW_ESTIMATE);
      }
  }

  ~SuperFoxComplexFft ()
  {
    if (forward)
      {
        decodium::fft_compat::destroy_plan (forward);
      }
    if (inverse)
      {
        decodium::fft_compat::destroy_plan (inverse);
      }
    if (data)
      {
        fftwf_free (data);
      }
  }

  bool valid () const
  {
    return data && forward && inverse;
  }

  std::complex<float>* values ()
  {
    return reinterpret_cast<std::complex<float>*> (data);
  }

  fftwf_complex* data;
  fftwf_plan forward;
  fftwf_plan inverse;
};

SuperFoxComplexFft& superfox_fft ()
{
  static thread_local SuperFoxComplexFft fft;
  return fft;
}

struct SuperFoxSyncFft
{
  SuperFoxSyncFft ()
    : data (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * kSuperFoxSyncSamples))),
      forward (nullptr)
  {
    if (data)
      {
        forward = decodium::fft_compat::plan_dft_1d (kSuperFoxSyncSamples, data, data, FFTW_FORWARD, FFTW_ESTIMATE);
      }
  }

  ~SuperFoxSyncFft ()
  {
    if (forward)
      {
        decodium::fft_compat::destroy_plan (forward);
      }
    if (data)
      {
        fftwf_free (data);
      }
  }

  bool valid () const
  {
    return data && forward;
  }

  std::complex<float>* values ()
  {
    return reinterpret_cast<std::complex<float>*> (data);
  }

  fftwf_complex* data;
  fftwf_plan forward;
};

SuperFoxSyncFft& superfox_sync_fft ()
{
  static SuperFoxSyncFft fft;
  return fft;
}

struct SuperFoxDemodFft
{
  SuperFoxDemodFft ()
    : data (reinterpret_cast<fftwf_complex*> (
          fftwf_malloc (sizeof (fftwf_complex) * kSuperFoxDemodNsps))),
      forward (nullptr)
  {
    if (data)
      {
        forward = decodium::fft_compat::plan_dft_1d (kSuperFoxDemodNsps, data, data, FFTW_FORWARD, FFTW_ESTIMATE);
      }
  }

  ~SuperFoxDemodFft ()
  {
    if (forward)
      {
        decodium::fft_compat::destroy_plan (forward);
      }
    if (data)
      {
        fftwf_free (data);
      }
  }

  bool valid () const
  {
    return data && forward;
  }

  std::complex<float>* values ()
  {
    return reinterpret_cast<std::complex<float>*> (data);
  }

  fftwf_complex* data;
  fftwf_plan forward;
};

SuperFoxDemodFft& superfox_demod_fft ()
{
  static SuperFoxDemodFft fft;
  return fft;
}

std::vector<std::complex<float>> const& superfox_smoothing_window ()
{
  static std::vector<std::complex<float>> cwindow = [] {
    std::vector<std::complex<float>> filter (static_cast<size_t> (kFt8NMax));
    SuperFoxComplexFft& fft = superfox_fft ();
    if (!fft.valid ())
      {
        return filter;
      }

    std::complex<float>* scratch = fft.values ();
    std::fill_n (scratch, kFt8NMax, std::complex<float> {});

    float sumw = 0.0f;
    std::array<float, kSuperFoxNFilt + 1> window {};
    for (int j = -kSuperFoxNFilt / 2; j <= kSuperFoxNFilt / 2; ++j)
      {
        float const value = std::pow (std::cos ((0.5f * kFt8TwoPi) * static_cast<float> (j)
                                                / static_cast<float> (kSuperFoxNFilt)), 2.0f);
        window[static_cast<size_t> (j + kSuperFoxNFilt / 2)] = value;
        sumw += value;
      }

    int const shift = kSuperFoxNFilt / 2 + 1;
    for (int src = 0; src <= kSuperFoxNFilt; ++src)
      {
        int dst = src - shift;
        if (dst < 0)
          {
            dst += kFt8NMax;
          }
        scratch[static_cast<size_t> (dst)] =
            std::complex<float> {window[static_cast<size_t> (src)] / sumw, 0.0f};
      }

    fftwf_execute (fft.forward);

    float const fac = 1.0f / static_cast<float> (kFt8NMax);
    for (int i = 0; i < kFt8NMax; ++i)
      {
        filter[static_cast<size_t> (i)] = scratch[static_cast<size_t> (i)] * fac;
      }
    return filter;
  }();
  return cwindow;
}

bool superfox_analytic_signal (float const* dd, int npts, std::complex<float>* c0)
{
  if (!dd || !c0 || npts <= 0)
    {
      return false;
    }

  std::vector<std::complex<float>> spectrum (static_cast<size_t> (npts));
  float const scale = 2.0f / (32767.0f * static_cast<float> (npts));
  for (int i = 0; i < npts; ++i)
    {
      spectrum[static_cast<size_t> (i)] = std::complex<float> {scale * dd[i], 0.0f};
    }

  auto* fft_data = reinterpret_cast<fftwf_complex*> (spectrum.data ());
  fftwf_plan const forward =
      decodium::fft_compat::plan_dft_1d (npts, fft_data, fft_data, FFTW_FORWARD, FFTW_ESTIMATE);
  fftwf_plan const inverse =
      decodium::fft_compat::plan_dft_1d (npts, fft_data, fft_data, FFTW_BACKWARD, FFTW_ESTIMATE);
  if (!forward || !inverse)
    {
      if (forward)
        {
          decodium::fft_compat::destroy_plan (forward);
        }
      if (inverse)
        {
          decodium::fft_compat::destroy_plan (inverse);
        }
      return false;
    }

  fftwf_execute (forward);
  for (int i = npts / 2 + 1; i < npts; ++i)
    {
      spectrum[static_cast<size_t> (i)] = std::complex<float> {};
    }
  spectrum[0] *= 0.5f;
  fftwf_execute (inverse);

  for (int i = 0; i < npts; ++i)
    {
      c0[static_cast<size_t> (i)] = spectrum[static_cast<size_t> (i)];
    }

  decodium::fft_compat::destroy_plan (forward);
  decodium::fft_compat::destroy_plan (inverse);
  return true;
}

bool superfox_remove_tone (std::complex<float>* c0, float fsync)
{
  if (!c0)
    {
      return false;
    }

  if (fsync > 1400.0f)
    {
      return true;
    }

  SuperFoxComplexFft& fft = superfox_fft ();
  if (!fft.valid ())
    {
      return false;
    }

  std::vector<std::complex<float>> const& cwindow = superfox_smoothing_window ();
  if (cwindow.empty ())
    {
      return false;
    }

  constexpr float kSampleRate = 12000.0f;
  float const baud = kSampleRate / 1024.0f;
  float const df = kSampleRate / static_cast<float> (kFt8NMax);
  float const fac = 1.0f / static_cast<float> (kFt8NMax);
  int const nbaud = static_cast<int> (std::lround (baud / df));

  std::vector<std::complex<float>> cref (static_cast<size_t> (kFt8NMax));
  std::vector<float> s (static_cast<size_t> (kFt8NMax / 4));
  std::complex<float>* cfilt = fft.values ();

  for (int i = 0; i < kFt8NMax; ++i)
    {
      cfilt[static_cast<size_t> (i)] = c0[static_cast<size_t> (i)] * fac;
    }
  fftwf_execute (fft.forward);

  for (int i = 1; i <= kFt8NMax / 4; ++i)
    {
      s[static_cast<size_t> (i - 1)] = std::norm (cfilt[static_cast<size_t> (i - 1)]);
    }

  int ia = static_cast<int> (std::lround ((fsync - 50.0f) / df));
  int ib = static_cast<int> (std::lround ((fsync + 1550.0f) / df));
  ia = std::max (1, ia);
  ib = std::min (kFt8NMax / 4, ib);
  if (ia > ib)
    {
      return true;
    }

  int i0 = ia;
  float peak = s[static_cast<size_t> (ia - 1)];
  for (int i = ia + 1; i <= ib; ++i)
    {
      float const value = s[static_cast<size_t> (i - 1)];
      if (value > peak)
        {
          peak = value;
          i0 = i;
        }
    }

  ia = std::max (1, i0 - nbaud);
  ib = std::min (kFt8NMax / 4, i0 + nbaud);

  float s0 = 0.0f;
  float s1 = 0.0f;
  for (int i = ia; i <= ib; ++i)
    {
      float const value = s[static_cast<size_t> (i - 1)];
      s0 += value;
      s1 += static_cast<float> (i - i0) * value;
    }
  if (s0 <= 0.0f)
    {
      return true;
    }

  float const delta = s1 / s0;
  i0 = static_cast<int> (std::lround (static_cast<float> (i0) + delta));
  float const f2 = static_cast<float> (i0) * df;

  ia = std::max (1, i0 - nbaud);
  ib = std::min (kFt8NMax / 4, i0 + nbaud);

  float s2 = 0.0f;
  for (int i = ia; i <= ib; ++i)
    {
      float const delta_i = static_cast<float> (i - i0);
      s2 += s[static_cast<size_t> (i - 1)] * delta_i * delta_i;
    }

  float const sigma = std::sqrt (s2 / s0) * df;
  if (sigma > 2.5f)
    {
      return true;
    }

  float const dt = 1.0f / kSampleRate;
  for (int i = 0; i < kFt8NMax; ++i)
    {
      float const arg = kFt8TwoPi * f2 * static_cast<float> (i + 1) * dt;
      cref[static_cast<size_t> (i)] = std::complex<float> {std::cos (arg), std::sin (arg)};
      cfilt[static_cast<size_t> (i)] = c0[static_cast<size_t> (i)] * std::conj (cref[static_cast<size_t> (i)]);
    }

  fftwf_execute (fft.forward);
  for (int i = 0; i < kFt8NMax; ++i)
    {
      cfilt[static_cast<size_t> (i)] *= cwindow[static_cast<size_t> (i)];
    }
  fftwf_execute (fft.inverse);

  int const nframe = std::min (kSuperFoxToneFrameSamples, kFt8NMax);
  for (int i = 0; i < nframe; ++i)
    {
      std::complex<float> const tone = cfilt[static_cast<size_t> (i)] * cref[static_cast<size_t> (i)];
      c0[static_cast<size_t> (i)] -= tone;
    }

  return true;
}

float db_compat (float x)
{
  if (x <= 1.259e-10f)
    {
      return -99.0f;
    }
  return 10.0f * std::log10 (x);
}

void smooth_121 (float* values, int count)
{
  if (!values || count <= 2)
    {
      return;
    }

  float x0 = values[0];
  for (int i = 1; i < count - 1; ++i)
    {
      float const x1 = values[i];
      values[i] = 0.5f * values[i] + 0.25f * (x0 + values[i + 1]);
      x0 = x1;
    }
}

float superfox_percentile (float const* values, int count, int percentile)
{
  if (!values || count <= 0 || percentile < 0 || percentile > 100)
    {
      return 1.0f;
    }

  std::vector<float> sorted (values, values + count);
  int const one_based = std::max (1, std::min (count,
                                               static_cast<int> (std::lround (
                                                   static_cast<double> (count) * 0.01
                                                   * static_cast<double> (percentile)))));
  std::nth_element (sorted.begin (), sorted.begin () + (one_based - 1), sorted.end ());
  return sorted[static_cast<size_t> (one_based - 1)];
}

void superfox_twkfreq2 (std::complex<float> const* c3, std::complex<float>* c4,
                        int npts, float fsample, float fshift)
{
  if (!c3 || !c4 || npts <= 0 || fsample <= 0.0f)
    {
      return;
    }

  std::complex<float> w {1.0f, 0.0f};
  float const dphi = fshift * kFt8TwoPi / fsample;
  std::complex<float> const wstep {std::cos (dphi), std::sin (dphi)};
  for (int i = 0; i < npts; ++i)
    {
      w *= wstep;
      c4[static_cast<size_t> (i)] = w * c3[static_cast<size_t> (i)];
    }
}

void smooth_121a (float* values, int count, float a, float b)
{
  if (!values || count <= 2)
    {
      return;
    }

  float const fac = 1.0f / (a + 2.0f * b);
  float x0 = values[0];
  for (int i = 1; i < count - 1; ++i)
    {
      float const x1 = values[i];
      values[i] = fac * (a * values[i] + b * (x0 + values[i + 1]));
      x0 = x1;
    }
}

bool superfox_demod (std::complex<float> const* crcvd, float f, float t,
                     float* s2_out, float* s3_out)
{
  if (!crcvd || !s2_out || !s3_out)
    {
      return false;
    }

  SuperFoxDemodFft& fft = superfox_demod_fft ();
  if (!fft.valid ())
    {
      return false;
    }

  std::fill_n (s2_out, kSuperFoxQpcRows * kSuperFoxDemodSpectrumCols, 0.0f);
  std::fill_n (s3_out, kSuperFoxQpcRows * kSuperFoxQpcCols, 0.0f);

  int const j0 = static_cast<int> (std::lround (12000.0f * (t + 0.5f)));
  float const df = 12000.0f / static_cast<float> (kSuperFoxDemodNsps);
  int const i0 = static_cast<int> (std::lround (f / df)) - kSuperFoxQpcRows / 2;
  if (i0 < 0 || i0 + kSuperFoxQpcRows > kSuperFoxDemodNsps)
    {
      return false;
    }

  std::complex<float>* spectrum = fft.values ();
  int k2 = 0;
  for (int n = 1; n <= kSuperFoxDemodSymbols; ++n)
    {
      int const jb = n * kSuperFoxDemodNsps + j0;
      int const ja = jb - kSuperFoxDemodNsps + 1;
      ++k2;
      if (ja < 1 || jb > kFt8NMax)
        {
          continue;
        }

      std::copy_n (crcvd + (ja - 1), kSuperFoxDemodNsps, spectrum);
      fftwf_execute (fft.forward);
      for (int row = 0; row < kSuperFoxQpcRows; ++row)
        {
          s2_out[row + kSuperFoxQpcRows * k2] =
              std::norm (spectrum[static_cast<size_t> (i0 + row)]);
        }
    }

  float const base2 = superfox_percentile (s2_out, kSuperFoxQpcRows * kSuperFoxDemodSymbols, 50);
  if (base2 > 0.0f)
    {
      float const inv = 1.0f / base2;
      for (int i = 0; i < kSuperFoxQpcRows * kSuperFoxDemodSpectrumCols; ++i)
        {
          s2_out[i] *= inv;
        }
    }

  std::array<int, kSuperFoxQpcRows> hist1 {};
  std::array<int, kSuperFoxQpcRows> hist2 {};
  for (int col = 0; col <= kSuperFoxDemodSymbols; ++col)
    {
      int best_row = 1;
      float best = s2_out[1 + kSuperFoxQpcRows * col];
      for (int row = 2; row < kSuperFoxQpcRows; ++row)
        {
          float const value = s2_out[row + kSuperFoxQpcRows * col];
          if (value > best)
            {
              best = value;
              best_row = row;
            }
        }
      ++hist1[static_cast<size_t> (best_row - 1)];
    }

  hist1[0] = 0;
  for (int i = 0; i <= 123; ++i)
    {
      hist2[static_cast<size_t> (i)] = hist1[static_cast<size_t> (i)]
                                       + hist1[static_cast<size_t> (i + 1)]
                                       + hist1[static_cast<size_t> (i + 2)]
                                       + hist1[static_cast<size_t> (i + 3)];
    }

  int i1 = 0;
  int m1 = hist1[0];
  for (int i = 1; i < kSuperFoxQpcRows; ++i)
    {
      if (hist1[static_cast<size_t> (i)] > m1)
        {
          m1 = hist1[static_cast<size_t> (i)];
          i1 = i;
        }
    }
  (void) i1;

  int i2 = 0;
  int m2 = hist2[0];
  for (int i = 1; i < kSuperFoxQpcRows; ++i)
    {
      if (hist2[static_cast<size_t> (i)] > m2)
        {
          m2 = hist2[static_cast<size_t> (i)];
          i2 = i;
        }
    }

  if (m1 > 12)
    {
      for (int row = 0; row < kSuperFoxQpcRows; ++row)
        {
          if (hist1[static_cast<size_t> (row)] > 12)
            {
              for (int col = 0; col <= kSuperFoxDemodSymbols; ++col)
                {
                  s2_out[row + kSuperFoxQpcRows * col] = 1.0f;
                }
            }
        }
    }

  if (m2 > 20)
    {
      if (i2 >= 1)
        {
          --i2;
        }
      if (i2 > 120)
        {
          i2 = 120;
        }
      for (int row = i2; row <= i2 + 7; ++row)
        {
          for (int col = 0; col <= kSuperFoxDemodSymbols; ++col)
            {
              s2_out[row + kSuperFoxQpcRows * col] = 1.0f;
            }
        }
    }

  int k3 = 0;
  for (int n = 1; n <= kSuperFoxDemodSymbols; ++n)
    {
      if (std::find (kSuperFoxIsync.begin (), kSuperFoxIsync.end (), n) != kSuperFoxIsync.end ())
        {
          continue;
        }
      ++k3;
      for (int row = 0; row < kSuperFoxQpcRows; ++row)
        {
          s3_out[row + kSuperFoxQpcRows * k3] = s2_out[row + kSuperFoxQpcRows * n];
        }
    }

  float const base3 = superfox_percentile (s3_out, kSuperFoxQpcRows * kSuperFoxQpcCols, 50);
  if (base3 > 0.0f)
    {
      float const inv = 1.0f / base3;
      for (int i = 0; i < kSuperFoxQpcRows * kSuperFoxQpcCols; ++i)
        {
          s3_out[i] *= inv;
        }
    }

  return true;
}

struct SuperFoxQpcDecodeResult
{
  std::array<unsigned char, kSuperFoxPackedSymbols> xdec {};
  bool crc_ok {false};
  float snrsync {0.0f};
  float fbest {0.0f};
  float tbest {0.0f};
  float snr {0.0f};
};

bool superfox_qpc_sync (std::complex<float> const* crcvd0, float fsample, float fsync, float ftol,
                        float& f2, float& t2, float& snrsync);
void superfox_qpc_likelihoods (float const* s3, int rows, int cols, float EsNo, float No,
                               float* py_out);
float superfox_qpc_snr (float const* s3, int rows, int cols, unsigned char const* y);

bool superfox_qpc_decode2 (std::complex<float> const* c0, float fsync, float ftol,
                           int ndepth, float dth, float damp,
                           SuperFoxQpcDecodeResult& result)
{
  if (!c0)
    {
      return false;
    }

  float f2 = 0.0f;
  float t2 = 0.0f;
  if (!superfox_qpc_sync (c0, 12000.0f, fsync, ftol, f2, t2, result.snrsync))
    {
      return false;
    }

  float const baud = 12000.0f / 1024.0f;
  float const f00 = 1500.0f + f2;
  float const t00 = t2;
  result.fbest = f00;
  result.tbest = t00;
  result.crc_ok = false;
  result.snr = 0.0f;
  result.xdec.fill (0);

  int maxd = 1;
  if (ndepth > 0)
    {
      int const depth_index = std::max (0, std::min (ndepth - 1,
                                                     static_cast<int> (kSuperFoxMaxDither.size ()) - 1));
      maxd = kSuperFoxMaxDither[static_cast<size_t> (depth_index)];
    }

  int maxft = kSuperFoxQpcSearchCount;
  if (result.snrsync < 4.0f || ndepth <= 0)
    {
      maxft = 1;
    }

  std::vector<std::complex<float>> shifted (static_cast<size_t> (kFt8NMax));
  std::vector<float> py (static_cast<size_t> (kSuperFoxQpcRows * kSuperFoxQpcCols));
  std::vector<float> py0 (static_cast<size_t> (kSuperFoxQpcRows * kSuperFoxQpcCols));
  std::vector<float> pyd (static_cast<size_t> (kSuperFoxQpcRows * kSuperFoxQpcCols));
  std::vector<float> s2 (static_cast<size_t> (kSuperFoxQpcRows * kSuperFoxDemodSpectrumCols));
  std::vector<float> s3 (static_cast<size_t> (kSuperFoxQpcRows * kSuperFoxQpcCols));
  std::array<unsigned char, kSuperFoxPackedSymbols> xdec {};
  std::array<unsigned char, kSuperFoxQpcCols> ydec {};

  constexpr std::array<int, 33> kSeed {
      321278106,  -658879006,  1239150429,  -941466001, -698554454,
      1136210962,  1633585627,  1261915021, -1134191465, -487888229,
      2131958895, -1429290834, -1802468092,  1801346659, 1966248904,
      402671397, -1961400750, -1567227835,  1895670987, -286583128,
      -595933665, -1699285543,  1518291336,  1338407128,  838354404,
      -2081343776, -1449416716,  1236537391,  -133197638,  337355509,
      -460640480,  1592689606,          0};
  std::array<uint32_t, kSeed.size ()> seed_data {};
  for (size_t i = 0; i < kSeed.size (); ++i)
    {
      seed_data[i] = static_cast<uint32_t> (kSeed[i]);
    }

  constexpr uint32_t kMask21 = (1u << 21) - 1u;
  for (int idith = 1; idith <= maxft; ++idith)
    {
      if (idith >= 2)
        {
          maxd = 1;
        }

      float const deltaf = static_cast<float> (kSuperFoxIdf[static_cast<size_t> (idith - 1)]) * 0.5f;
      float const deltat = static_cast<float> (kSuperFoxIdt[static_cast<size_t> (idith - 1)])
                           * 8.0f / 1024.0f;
      float const f = f00 + deltaf;
      float const t = t00 + deltat;
      float const fshift = 1500.0f - (f + baud);
      superfox_twkfreq2 (c0, shifted.data (), kFt8NMax, 12000.0f, fshift);

      float const a = 1.0f;
      for (int kk = 1; kk <= 4; ++kk)
        {
          float b = 0.0f;
          if (kk == 2)
            {
              b = 0.4f;
            }
          else if (kk == 3)
            {
              b = 0.5f;
            }
          else if (kk == 4)
            {
              b = 0.6f;
            }

          if (!superfox_demod (shifted.data (), 1500.0f, t, s2.data (), s3.data ()))
            {
              return false;
            }

          if (b > 0.0f)
            {
              for (int col = 0; col < kSuperFoxQpcCols; ++col)
                {
                  smooth_121a (s3.data () + kSuperFoxQpcRows * col, kSuperFoxQpcRows, a, b);
                }
            }

          float const base3 = superfox_percentile (s3.data (), kSuperFoxQpcRows * kSuperFoxQpcCols, 50);
          if (base3 > 0.0f)
            {
              float const inv = 1.0f / base3;
              for (float& value : s3)
                {
                  value *= inv;
                }
            }

          constexpr float kEsNoDec = 3.16f;
          constexpr float kNo = 1.0f;
          py0 = s3;
          superfox_qpc_likelihoods (s3.data (), kSuperFoxQpcRows, kSuperFoxQpcCols, kEsNoDec, kNo,
                                    py.data ());

          std::seed_seq seed_seq (seed_data.begin (), seed_data.end ());
          std::mt19937 engine (seed_seq);
          std::uniform_real_distribution<float> uniform (0.0f, 1.0f);
          for (int kkk = 1; kkk <= maxd; ++kkk)
            {
              if (kkk == 1)
                {
                  pyd = py0;
                }
              else
                {
                  std::fill (pyd.begin (), pyd.end (), 0.0f);
                  if (kkk > 2)
                    {
                      for (float& value : pyd)
                        {
                          value = 2.0f * (uniform (engine) - 0.5f);
                        }
                    }
                  for (size_t i = 0; i < pyd.size (); ++i)
                    {
                      if (py[i] > dth)
                        {
                          pyd[i] = 0.0f;
                        }
                    }
                  for (size_t i = 0; i < pyd.size (); ++i)
                    {
                      pyd[i] = py[i] * (1.0f + damp * pyd[i]);
                    }
                }

              for (int col = 0; col < kSuperFoxQpcCols; ++col)
                {
                  float ss = 0.0f;
                  for (int row = 0; row < kSuperFoxQpcRows; ++row)
                    {
                      ss += pyd[static_cast<size_t> (row + kSuperFoxQpcRows * col)];
                    }
                  if (ss > 0.0f)
                    {
                      float const inv = 1.0f / ss;
                      for (int row = 0; row < kSuperFoxQpcRows; ++row)
                        {
                          pyd[static_cast<size_t> (row + kSuperFoxQpcRows * col)] *= inv;
                        }
                    }
                  else
                    {
                      for (int row = 0; row < kSuperFoxQpcRows; ++row)
                        {
                          pyd[static_cast<size_t> (row + kSuperFoxQpcRows * col)] = 0.0f;
                        }
                    }
                }

              qpc_decode (xdec.data (), ydec.data (), pyd.data ());
              std::reverse (xdec.begin (), xdec.end ());

              uint32_t const crc_chk = nhash2 (xdec.data (), 47, 571u) & kMask21;
              uint32_t const crc_sent =
                  (static_cast<uint32_t> (xdec[47]) << 14)
                  | (static_cast<uint32_t> (xdec[48]) << 7)
                  | static_cast<uint32_t> (xdec[49]);
              result.crc_ok = crc_chk == crc_sent;
              if (result.crc_ok)
                {
                  result.snr = superfox_qpc_snr (s3.data (), kSuperFoxQpcRows,
                                                 kSuperFoxQpcCols, ydec.data ());
                  result.xdec = xdec;
                  result.fbest = f;
                  result.tbest = t;
                  if (result.snr < -16.5f)
                    {
                      result.crc_ok = false;
                    }
                  return true;
                }
            }
        }
    }

  return true;
}

bool superfox_qpc_sync (std::complex<float> const* crcvd0, float fsample, float fsync, float ftol,
                        float& f2, float& t2, float& snrsync)
{
  if (!crcvd0)
    {
      return false;
    }

  SuperFoxSyncFft& fft = superfox_sync_fft ();
  if (!fft.valid ())
    {
      return false;
    }

  constexpr int kSpectrumBins = kSuperFoxSyncSamples / 4;
  float const baud = 12000.0f / 1024.0f;
  float const df2 = fsample / static_cast<float> (kSuperFoxSyncSamples);
  float const fac = 1.0f / static_cast<float> (kSuperFoxSyncSamples);
  std::complex<float>* c0 = fft.values ();
  for (int i = 0; i < kSuperFoxSyncSamples; ++i)
    {
      c0[static_cast<size_t> (i)] = crcvd0[static_cast<size_t> (i)] * fac;
    }
  fftwf_execute (fft.forward);

  std::vector<float> s (static_cast<size_t> (kSpectrumBins));
  for (int i = 1; i <= kSpectrumBins; ++i)
    {
      s[static_cast<size_t> (i - 1)] = std::norm (c0[static_cast<size_t> (i)]);
    }

  for (int i = 0; i < 4; ++i)
    {
      smooth_121 (s.data (), kSpectrumBins);
    }

  int ia = static_cast<int> (std::lround ((fsync - ftol) / df2));
  int ib = static_cast<int> (std::lround ((fsync + ftol) / df2));
  ia = std::max (1, ia);
  ib = std::min (kSpectrumBins, ib);
  if (ia > ib)
    {
      return false;
    }

  int i0 = ia;
  float peak = s[static_cast<size_t> (ia - 1)];
  for (int i = ia + 1; i <= ib; ++i)
    {
      float const value = s[static_cast<size_t> (i - 1)];
      if (value > peak)
        {
          peak = value;
          i0 = i;
        }
    }
  f2 = df2 * static_cast<float> (i0) - 750.0f;

  ia = static_cast<int> (std::lround (static_cast<float> (i0) - baud / df2));
  ib = static_cast<int> (std::lround (static_cast<float> (i0) + baud / df2));
  ia = std::max (1, ia);
  ib = std::min (kSpectrumBins, ib);

  float s1 = 0.0f;
  float s0 = 0.0f;
  for (int i = ia; i <= ib; ++i)
    {
      float const value = s[static_cast<size_t> (i - 1)];
      s0 += value;
      s1 += static_cast<float> (i - i0) * value;
    }
  if (s0 <= 0.0f)
    {
      return false;
    }

  float const delta = s1 / s0;
  i0 = static_cast<int> (std::lround (static_cast<float> (i0) + delta));
  f2 = static_cast<float> (i0) * df2 - 750.0f;

  std::vector<std::complex<float>> c1 (static_cast<size_t> (kSuperFoxSyncNz), std::complex<float> {});
  ia = static_cast<int> (std::lround (static_cast<float> (i0) - baud / df2));
  ib = static_cast<int> (std::lround (static_cast<float> (i0) + baud / df2));
  ia = std::max (0, ia);
  ib = std::min (kSuperFoxSyncSamples - 1, ib);
  for (int i = ia; i <= ib; ++i)
    {
      int const j = i - i0;
      int const dst = j >= 0 ? j : j + kSuperFoxSyncNz;
      if (dst >= 0 && dst < kSuperFoxSyncNz)
        {
          c1[static_cast<size_t> (dst)] = c0[static_cast<size_t> (i)];
        }
    }

  fftwf_plan inverse = decodium::fft_compat::plan_dft_1d (kSuperFoxSyncNz,
                                          reinterpret_cast<fftwf_complex*> (c1.data ()),
                                          reinterpret_cast<fftwf_complex*> (c1.data ()),
                                          FFTW_BACKWARD, FFTW_ESTIMATE);
  if (!inverse)
    {
      return false;
    }
  fftwf_execute (inverse);
  decodium::fft_compat::destroy_plan (inverse);

  std::vector<std::complex<float>> c1sum (static_cast<size_t> (kSuperFoxSyncNz));
  c1sum[0] = c1[0];
  for (int i = 1; i < kSuperFoxSyncNz; ++i)
    {
      c1sum[static_cast<size_t> (i)] = c1sum[static_cast<size_t> (i - 1)] + c1[static_cast<size_t> (i)];
    }

  int const nspsd = 1024 / kSuperFoxSyncDown;
  float const dt = static_cast<float> (kSuperFoxSyncDown) / 12000.0f;
  int const lagmax = static_cast<int> (1.5f / dt);
  int const nominal_start = static_cast<int> (std::lround (0.5f * fsample / kSuperFoxSyncDown));
  std::vector<float> p (static_cast<size_t> (2 * lagmax + 1));
  float pmax = 0.0f;
  int lagpk = 0;
  for (int lag = -lagmax; lag <= lagmax; ++lag)
    {
      float sp = 0.0f;
      for (int j = 0; j < kSuperFoxSyncSymbols; ++j)
        {
          int const i1 = nominal_start + (kSuperFoxIsync[static_cast<size_t> (j)] - 1) * nspsd + lag;
          int const i2 = i1 + nspsd;
          if (i1 < 0 || i1 > kSuperFoxSyncNz - 1 || i2 < 0 || i2 > kSuperFoxSyncNz - 1)
            {
              continue;
            }
          std::complex<float> const z = c1sum[static_cast<size_t> (i2)] - c1sum[static_cast<size_t> (i1)];
          sp += std::norm (z);
        }
      if (sp > pmax)
        {
          pmax = sp;
          lagpk = lag;
        }
      p[static_cast<size_t> (lag + lagmax)] = sp;
    }

  t2 = static_cast<float> (lagpk) * dt;
  float sp = 0.0f;
  float sq = 0.0f;
  int nsum = 0;
  float const tsym = 1024.0f / 12000.0f;
  for (int lag = -lagmax; lag <= lagmax; ++lag)
    {
      float const t = static_cast<float> (lag - lagpk) * dt;
      if (std::fabs (t) < tsym)
        {
          continue;
        }
      float const value = p[static_cast<size_t> (lag + lagmax)];
      ++nsum;
      sp += value;
      sq += value * value;
    }
  if (nsum <= 0)
    {
      return false;
    }

  float const ave = sp / static_cast<float> (nsum);
  float const variance = std::max (0.0f, sq / static_cast<float> (nsum) - ave * ave);
  float const rms = std::sqrt (variance);
  snrsync = rms > 0.0f ? (pmax - ave) / rms : 0.0f;
  return true;
}

void superfox_qpc_likelihoods (float const* s3, int rows, int cols, float EsNo, float No,
                               float* py_out)
{
  float const norm = (EsNo / (EsNo + 1.0f)) / No;
  for (int col = 0; col < cols; ++col)
    {
      float normpwrmax = 0.0f;
      for (int row = 0; row < rows; ++row)
        {
          int const index = row + rows * col;
          float const normpwr = norm * s3[index];
          py_out[index] = normpwr;
          normpwrmax = std::max (normpwrmax, normpwr);
        }
      float pynorm = 0.0f;
      for (int row = 0; row < rows; ++row)
        {
          int const index = row + rows * col;
          py_out[index] = std::exp (py_out[index] - normpwrmax);
          pynorm += py_out[index];
        }
      if (pynorm > 0.0f)
        {
          for (int row = 0; row < rows; ++row)
            {
              int const index = row + rows * col;
              py_out[index] /= pynorm;
            }
        }
    }
}

float superfox_qpc_snr (float const* s3, int rows, int cols, unsigned char const* y)
{
  float p = 0.0f;
  for (int col = 1; col < cols; ++col)
    {
      int const row = y[col];
      if (row >= 0 && row < rows)
        {
          p += s3[row + rows * col];
        }
    }
  return db_compat (p / 127.0f) - db_compat (127.0f) - 4.0f;
}

std::string format_superfox_stdout_line (int nutc, int snr, float dt, float freq,
                                         std::string const& decoded)
{
  std::ostringstream line;
  line << std::setfill ('0') << std::setw (6) << nutc
       << std::setfill (' ') << std::setw (4) << snr
       << std::fixed << std::setprecision (1) << std::setw (5) << dt
       << std::setw (5) << static_cast<int> (std::lround (freq))
       << " ~  " << decoded;
  return line.str ();
}

std::uint64_t superfox_read_bits (std::array<unsigned char, kSuperFoxPackedSymbols> const& xdec,
                                  int bit_offset, int bit_count)
{
  std::uint64_t value = 0;
  for (int i = 0; i < bit_count; ++i)
    {
      int const bit_index = bit_offset + i;
      int const symbol = bit_index / 7;
      int const shift = 6 - (bit_index % 7);
      value = (value << 1)
              | ((static_cast<std::uint64_t> (xdec[static_cast<size_t> (symbol)]) >> shift) & 1u);
    }
  return value;
}

std::string superfox_unpack_text71 (std::array<unsigned char, kSuperFoxPackedSymbols> const& xdec,
                                    int bit_offset)
{
  std::array<char, 71> bits {};
  for (int i = 0; i < 71; ++i)
    {
      bits[static_cast<size_t> (i)] =
          superfox_read_bits (xdec, bit_offset + i, 1) != 0 ? '1' : '0';
    }

  std::array<char, 13> unpacked {};
  bool success = false;
  legacy_pack77_unpacktext77_c (bits.data (), unpacked.data (), &success);
  return success ? trim_block (unpacked.data (), static_cast<int> (unpacked.size ())) : std::string {};
}

std::string superfox_unpack_call28 (int n28)
{
  std::array<char, 13> unpacked {};
  bool success = false;
  legacy_pack77_unpack28_c (n28, unpacked.data (), &success);
  return success ? trim_block (unpacked.data (), static_cast<int> (unpacked.size ())) : std::string {};
}

std::string superfox_unpack_grid15 (int n15)
{
  constexpr int kGridBase = 180 * 180;
  if (n15 < 0)
    {
      return {};
    }
  if (n15 < 32400)
    {
      int const dlat = (n15 % 180) - 90;
      int const dlong = (n15 / 180) * 2 - 180 + 2;
      int const nlong = static_cast<int> (60.0 * (180.0 - static_cast<double> (dlong)) / 5.0);
      int const long_a = nlong / 240;
      int const long_b = (nlong - 240 * long_a) / 24;
      int const nlat = static_cast<int> (60.0 * (static_cast<double> (dlat) + 90.0) / 2.5);
      int const lat_a = nlat / 240;
      int const lat_b = (nlat - 240 * lat_a) / 24;
      std::string grid (4, ' ');
      grid[0] = static_cast<char> ('A' + long_a);
      grid[1] = static_cast<char> ('A' + lat_a);
      grid[2] = static_cast<char> ('0' + long_b);
      grid[3] = static_cast<char> ('0' + lat_b);
      if (grid.rfind ("KA", 0) == 0 || grid.rfind ("LA", 0) == 0)
        {
          bool const prefixed = grid[0] == 'L';
          int value = std::stoi (grid.substr (2, 2)) - 50;
          std::ostringstream report;
          if (prefixed)
            {
              report << 'R';
            }
          if (value >= 0)
            {
              report << '+';
            }
          report << std::setw (2) << std::setfill ('0') << std::abs (value);
          return report.str ();
        }
      return grid;
    }

  int value = n15 - kGridBase - 1;
  std::ostringstream report;
  if (value >= 1 && value <= 30)
    {
      report << '-' << std::setw (2) << std::setfill ('0') << value;
      return report.str ();
    }
  if (value >= 31 && value <= 60)
    {
      report << "R-" << std::setw (2) << std::setfill ('0') << (value - 30);
      return report.str ();
    }
  if (value == 61)
    {
      return "RO";
    }
  if (value == 62)
    {
      return "RRR";
    }
  if (value == 63)
    {
      return "73";
    }
  return {};
}

std::string superfox_decode_compound_call (std::uint64_t n58)
{
  std::array<char, 13> foxcall {};
  foxcall.fill (' ');
  for (int i = 10; i >= 0; --i)
    {
      std::uint64_t const index = (n58 % 38u);
      foxcall[static_cast<size_t> (i)] = kSuperFoxBase38[index];
      n58 /= 38u;
    }
  return trim_block (foxcall.data (), static_cast<int> (foxcall.size ()));
}

std::string superfox_trim_period_padding (std::string text)
{
  for (int i = static_cast<int> (text.size ()) - 1; i >= 0; --i)
    {
      if (text[static_cast<size_t> (i)] != '.')
        {
          break;
        }
      text[static_cast<size_t> (i)] = ' ';
    }
  while (!text.empty () && text.back () == ' ')
    {
      text.pop_back ();
    }
  return text;
}

std::string superfox_format_report (int raw)
{
  if (raw == 31)
    {
      return "RR73";
    }

  int const value = raw - 18;
  std::ostringstream report;
  report << (value >= 0 ? '+' : '-')
         << std::setw (2) << std::setfill ('0') << std::abs (value);
  return report.str ();
}

std::vector<std::string> superfox_unpack_lines (
    std::array<unsigned char, kSuperFoxPackedSymbols> const& xdec, bool use_otp)
{
  std::vector<std::string> lines;
  lines.reserve (kSuperFoxMaxDecodeLines);

  int const i3 = static_cast<int> (superfox_read_bits (xdec, 326, 3));
  std::string foxcall = superfox_unpack_call28 (static_cast<int> (superfox_read_bits (xdec, 0, 28)));
  int ncq = 0;

  if (i3 == 2)
    {
      std::string free_text = superfox_unpack_text71 (xdec, 160);
      free_text += superfox_unpack_text71 (xdec, 231);
      lines.push_back (superfox_trim_period_padding (free_text));
    }
  else if (i3 == 3)
    {
      foxcall = superfox_decode_compound_call (superfox_read_bits (xdec, 0, 58));
      std::string const grid4 = superfox_unpack_grid15 (static_cast<int> (superfox_read_bits (xdec, 58, 15)));
      lines.push_back ("CQ " + foxcall + (grid4.empty () ? std::string {} : " " + grid4));

      std::uint64_t const n32 = superfox_read_bits (xdec, 73, 32);
      if (static_cast<int> (n32) != kSuperFoxNqu1Rks)
        {
          std::string free_text = superfox_unpack_text71 (xdec, 73);
          free_text += superfox_unpack_text71 (xdec, 144);
          free_text = superfox_trim_period_padding (free_text);
          if (!free_text.empty ())
            {
              lines.push_back (free_text);
            }
        }
    }

  if (i3 != 3)
    {
      int report_offset = (i3 == 2) ? 140 : 280;
      int report_count = 4;
      std::array<std::string, 5> reports {};
      for (int i = 0; i < report_count; ++i)
        {
          reports[static_cast<size_t> (i)] =
              superfox_format_report (static_cast<int> (superfox_read_bits (xdec, report_offset + 5 * i, 5)));
        }

      int const max_calls = (i3 == 2 || i3 == 3) ? 4 : 9;
      for (int i = 0; i < max_calls; ++i)
        {
          int const n28 = static_cast<int> (superfox_read_bits (xdec, 28 * i, 28));
          if (n28 == 0 || n28 == kSuperFoxNqu1Rks)
            {
              continue;
            }

          std::string const hound = superfox_unpack_call28 (n28);
          if (hound.empty ())
            {
              continue;
            }

          std::string message = hound + " " + foxcall;
          bool const is_cq = message.rfind ("CQ ", 0) == 0;
          if (is_cq)
            {
              ++ncq;
            }
          else if (i3 == 2)
            {
              message += " " + reports[static_cast<size_t> (i)];
            }
          else if (i < 5)
            {
              message += " RR73";
            }
          else
            {
              message += " " + reports[static_cast<size_t> (i - 5)];
            }

          if (ncq <= 1 || !is_cq)
            {
              lines.push_back (message);
            }
        }

      if (superfox_read_bits (xdec, 305, 1) != 0 && ncq < 1)
        {
          lines.push_back ("CQ " + foxcall);
        }
    }

  if (use_otp)
    {
      int const signature = static_cast<int> (superfox_read_bits (xdec, 306, 20));
      std::ostringstream verify;
      verify << "$VERIFY$ " << foxcall << ' '
             << std::setfill ('0') << std::setw (6) << signature;
      lines.push_back (verify.str ());
    }

  return lines;
}

bool superfox_decode_lines_from_wave (short const* iwave, int nfqso, int ntol,
                                      std::vector<std::string>& lines_out,
                                      int& nsnr_out, float& freq_out, float& dt_out)
{
  lines_out.clear ();
  nsnr_out = 0;
  freq_out = 0.0f;
  dt_out = 0.0f;

  if (!iwave)
    {
      return false;
    }

  // std::vector, non std::array: kFt8NMax=180000 renderebbe questi due
  // buffer ~2.16MB su stack, oltre lo stack di default di un thread
  // Windows -- crash reale verificato (STATUS_STACK_OVERFLOW) col tool
  // standalone utils/sfrx.cpp prima di questo fix, 6 settembre 2026.
  std::vector<float> dd (kFt8NMax);
  for (int i = 0; i < kFt8NMax; ++i)
    {
      dd[static_cast<size_t> (i)] = static_cast<float> (iwave[i]);
    }
  ftx_sfox_remove_ft8_c (dd.data (), kFt8NMax);

  std::vector<std::complex<float>> c0 (kFt8NMax);
  if (!superfox_analytic_signal (dd.data (), kFt8NMax, c0.data ()))
    {
      return false;
    }

  float fsync = static_cast<float> (nfqso);
  if (!superfox_remove_tone (c0.data (), fsync))
    {
      return false;
    }

  SuperFoxQpcDecodeResult decode;
  if (!superfox_qpc_decode2 (c0.data (), fsync, static_cast<float> (ntol),
                             3, 0.5f, 1.0f, decode)
      || !decode.crc_ok)
    {
      return false;
    }

  std::array<unsigned char, kSuperFoxPackedSymbols> payload {};
  for (int i = 0; i < kSuperFoxPackedSymbols; ++i)
    {
      payload[static_cast<size_t> (i)] = decode.xdec[static_cast<size_t> (i)];
    }

  lines_out = superfox_unpack_lines (payload, true);
  nsnr_out = static_cast<int> (std::lround (decode.snr));
  freq_out = decode.fbest - 750.0f;
  dt_out = decode.tbest;
  return !lines_out.empty ();
}

void copy_superfox_lines (std::vector<std::string> const& lines, char* lines_out,
                          int line_stride, int max_lines)
{
  if (!lines_out || line_stride <= 0 || max_lines <= 0)
    {
      return;
    }

  int const count = std::min (max_lines, static_cast<int> (lines.size ()));
  for (int i = 0; i < count; ++i)
    {
      char* destination = lines_out + i * line_stride;
      std::fill_n (destination, line_stride, ' ');
      std::string const& line = lines[static_cast<size_t> (i)];
      std::copy_n (line.data (), std::min (line_stride, static_cast<int> (line.size ())), destination);
    }
}

struct Ft8FoxEntry
{
  FixedChars<12> callsign {blank_fixed<12> ()};
  FixedChars<4> grid {blank_fixed<4> ()};
  int snr {0};
  int freq {0};
  int n30 {0};
};

struct Ft8EmitState
{
  std::vector<Ft8FoxEntry> fox_entries;
  int n30z {0};
  int nwrap {0};

  void reset ()
  {
    fox_entries.clear ();
    n30z = 0;
    nwrap = 0;
  }
};

Ft8EmitState& ft8_emit_state ()
{
  static Ft8EmitState state;
  return state;
}

int update_ft8_n30_state (Ft8EmitState& state, int nutc)
{
  int const n30 = (3600 * (nutc / 10000)
                   + 60 * ((nutc / 100) % 100)
                   + nutc % 100) / 30;
  if (n30 < state.n30z)
    {
      state.nwrap += 2880;
    }
  state.n30z = n30;
  return n30 + state.nwrap;
}

bool should_collect_fox_entry (std::string const& decoded, std::string const& mycall,
                               bool superfox, int freq, Ft8FoxEntry& entry)
{
  std::vector<std::string> const words = split_words (decoded);
  if (words.size () < 2)
    {
      return false;
    }

  std::string const& c1 = words[0];
  std::string const& c2 = words[1];
  std::string const g2 = words.size () >= 3 ? words[2] : std::string {};
  bool b0 = c1 == mycall;
  if (c1 == "DE" && c2.find ('/') != std::string::npos && c2.find ('/') >= 1)
    {
      b0 = true;
    }
  if (c1.size () != mycall.size ())
    {
      if (c1.find (mycall) != std::string::npos || mycall.find (c1) != std::string::npos)
        {
          b0 = true;
        }
    }

  bool const b1 = is_grid4 (g2);
  bool const b2 = g2.empty ();
  if (!(b0 && (b1 || b2) && (freq >= 1000 || superfox)))
    {
      return false;
    }

  entry.callsign = fixed_from_string<12> (c2);
  entry.grid = fixed_from_string<4> (b1 ? g2 : std::string {});
  entry.freq = freq;
  return true;
}

int grid_distance_km (FixedChars<6> const& mygrid, FixedChars<4> const& hisgrid4)
{
  std::string const hisgrid_trimmed = trim_fixed (hisgrid4);
  if (!is_grid4 (hisgrid_trimmed))
    {
      return 9999;
    }

  FixedChars<6> hisgrid = blank_fixed<6> ();
  std::copy_n (hisgrid4.begin (), 4, hisgrid.begin ());
  QString const my_grid = QString::fromLatin1 (mygrid.data (), static_cast<int> (mygrid.size ()));
  QString const his_grid = QString::fromLatin1 (hisgrid.data (), static_cast<int> (hisgrid.size ()));
  return geo_distance (my_grid, his_grid, 0.0).km;
}

void write_houndcallers_file (std::string const& path, Ft8EmitState& state,
                              FixedChars<6> const& mygrid, int current_n30)
{
  std::ofstream file {path, std::ios::out | std::ios::trunc};
  if (!file.is_open ())
    {
      return;
    }

  std::vector<Ft8FoxEntry> retained;
  retained.reserve (state.fox_entries.size ());
  for (Ft8FoxEntry const& entry : state.fox_entries)
    {
      int const age = std::min (99, (current_n30 - entry.n30 + 288000) % 2880);
      if (age > 4)
        {
          continue;
        }

      retained.push_back (entry);
      int const dkm = grid_distance_km (mygrid, entry.grid);
      file << std::left << std::setw (12) << trim_fixed (entry.callsign)
           << ' '
           << std::setw (4) << trim_fixed (entry.grid)
           << std::right << std::setw (5) << entry.snr
           << std::setw (6) << entry.freq
           << std::setw (7) << dkm
           << std::setw (3) << age
           << '\n';
    }
  file.flush ();
  state.fox_entries = std::move (retained);
}

void copy_audio_to_float (short const* source, std::array<float, kFt8NMax>& dest)
{
  for (int i = 0; i < kFt8NMax; ++i)
    {
      dest[static_cast<size_t> (i)] = static_cast<float> (source[i]);
    }
}

void restore_carried_audio (std::array<float, kFt8NMax>& dest,
                            std::array<float, kFt8NMax> const& carried,
                            short const* source, int carried_samples)
{
  copy_audio_to_float (source, dest);
  int const ncopy = std::clamp (carried_samples, 0, kFt8CarrySamples);
  std::copy_n (carried.begin (), ncopy, dest.begin ());
}

double current_sequence_seconds ()
{
  using namespace std::chrono;

  system_clock::time_point const now = system_clock::now ();
  auto const millis = duration_cast<milliseconds> (now.time_since_epoch ());
  std::time_t const tt = system_clock::to_time_t (now);
  std::tm local_tm {};
#if defined(_WIN32)
  localtime_s (&local_tm, &tt);
#else
  localtime_r (&tt, &local_tm);
#endif
  double const sec = static_cast<double> (local_tm.tm_sec) + (millis.count () % 1000) / 1000.0;
  double tseq = std::fmod (sec, 15.0);
  if (tseq < 10.0)
    {
      tseq += 15.0;
    }
  return tseq;
}

struct AsyncCollector
{
  float* syncs {};
  int* snrs {};
  float* dts {};
  float* freqs {};
  int* naps {};
  float* quals {};
  signed char* bits77 {};
  char* decodeds {};
  int* nout {};
  int count {0};

  void reset ()
  {
    if (syncs) std::fill_n (syncs, kFt8MaxLines, 0.0f);
    if (snrs) std::fill_n (snrs, kFt8MaxLines, 0);
    if (dts) std::fill_n (dts, kFt8MaxLines, 0.0f);
    if (freqs) std::fill_n (freqs, kFt8MaxLines, 0.0f);
    if (naps) std::fill_n (naps, kFt8MaxLines, 0);
    if (quals) std::fill_n (quals, kFt8MaxLines, 0.0f);
    if (bits77) std::fill_n (bits77, kFt8MaxLines * kFt8Bits, static_cast<signed char> (0));
    if (decodeds) std::fill_n (decodeds, kFt8MaxLines * kFt8DecodedChars, ' ');
    count = 0;
    if (nout) *nout = 0;
    // Una invocazione del decodificatore = uno slot. L'elenco delle stazioni
    // sentite misura la propria memoria in questa scala.
    decodium::apstorico::avanza_ciclo ();
  }

  void append (float sync, int snr, float dt, float freq,
               FixedChars<kFt8DecodedChars> const& decoded, int nap, float qual,
               signed char const* message77)
  {
    (void) sync;
    FixedChars<kFt8DecodedChars> const normalized_decoded =
        normalize_resolved_hash_call_tokens (decoded);
    if (!is_plausible_ft8_message_for_emit (normalized_decoded))
      {
        return;
      }
    if (decodeds)
      {
        for (int i = 0; i < count; ++i)
          {
            if (std::equal (normalized_decoded.begin (), normalized_decoded.end (),
                            decodeds + i * kFt8DecodedChars))
              {
                return;
              }
          }
      }
    if (count >= kFt8MaxLines)
      {
        return;
      }

    int const index = count++;
    if (syncs)
      {
        syncs[index] = sync;
      }
    snrs[index] = snr;
    dts[index] = dt;
    freqs[index] = freq;
    naps[index] = nap;
    quals[index] = qual;
    std::copy (normalized_decoded.begin (), normalized_decoded.end (),
               decodeds + index * kFt8DecodedChars);
    if (message77)
      {
        std::copy_n (message77, kFt8Bits, bits77 + index * kFt8Bits);
      }
    if (nout)
      {
        *nout = count;
      }

    if (nap == 8)
      {
        ftx_ft8_ap_msg_conta_successo_c ();
        // A differenza di FT2 (DECODIUM_FT2_AP_MSG_LOG), qui non c'era modo
        // di controllare a occhio i successi della decodifica predittiva:
        // solo il contatore aggregato ftx_ft8_ap_msg_successi_c(). Questa
        // riga stampa il testo decodificato per ogni successo, cosi' si puo'
        // giudicare la plausibilita' (continuazione di QSO sensata) invece
        // di fidarsi solo del tasso di falsi misurato in laboratorio.
        if (std::getenv ("DECODIUM_FT8_AP_MSG_LOG"))
          {
            std::fprintf (stderr, "[APMSG-FT8] f=%.1f dt=%.2f snr=%d testo=\"%.*s\"\n",
                          static_cast<double> (freq), static_cast<double> (dt), snr,
                          static_cast<int> (normalized_decoded.size ()),
                          normalized_decoded.data ());
            std::fflush (stderr);
          }
      }

    // La stazione appena letta diventa un'ipotesi a priori per i cicli
    // successivi, su questa frequenza. Si registra QUI e non piu' a monte
    // perche' questo e' l'unico punto in cui una decodifica e' definitivamente
    // accettata: prima ci sono ancora il controllo di plausibilita' e quello
    // sui doppioni.
    {
      // L'inizializzazione a {} azzera tutto, quindi l'ultimo carattere e'
      // gia' il terminatore che al campo a lunghezza fissa manca.
      char msg[kFt8DecodedChars + 1] {};
      std::copy (normalized_decoded.begin (), normalized_decoded.end (), msg);
      decodium::apstorico::registra_da_messaggio (
          decodium::apstorico::ciclo_corrente (), freq, msg);
    }

    // E i 77 bit interi, che sono l'ipotesi forte: in FT8 chi chiama li ripete
    // identici finche' non gli risponde qualcuno, quindi due slot dopo il
    // decodificatore puo' VERIFICARLI invece di indovinare. Si registrano solo
    // se i bit ci sono davvero: un messaggio senza bit non e' un'ipotesi.
    if (message77)
      {
        decodium::apstorico::registra_messaggio (freq, message77);
    }
  }

  void resolve_hash_placeholders ()
  {
    if (!decodeds || !bits77 || count <= 0)
      {
        return;
      }

    for (int i = 0; i < count; ++i)
      {
        FixedChars<kFt8DecodedChars> decoded =
            fixed_from_chars<kFt8DecodedChars> (decodeds + i * kFt8DecodedChars);
        decoded = normalize_resolved_hash_call_tokens (decoded);
        seed_pack77_hashes_from_message (decoded);
      }
    apply_pack77_hash_seed_cache ();

    for (int iteration = 0; iteration < 2; ++iteration)
      {
        int changed = 0;
        for (int i = 0; i < count; ++i)
          {
            FixedChars<kFt8DecodedChars> current =
                fixed_from_chars<kFt8DecodedChars> (decodeds + i * kFt8DecodedChars);
            if (!has_unresolved_hash_placeholder (current))
              {
                continue;
              }

            signed char const* bits = bits77 + i * kFt8Bits;
            bool has_bits = false;
            for (int bit = 0; bit < kFt8Bits; ++bit)
              {
                if (bits[bit] != 0)
                  {
                    has_bits = true;
                    break;
                  }
              }
            if (!has_bits)
              {
                continue;
              }

            FixedChars<kFt8DecodedChars> unpacked =
                blank_fixed<kFt8DecodedChars> ();
            int quirky = 0;
            if (legacy_pack77_unpack77bits_c (bits, 1, unpacked.data (), &quirky) == 0)
              {
                continue;
              }
            FixedChars<kFt8DecodedChars> const normalized_unpacked =
                normalize_resolved_hash_call_tokens (unpacked);
            if (has_unresolved_hash_placeholder (unpacked)
                || !is_plausible_ft8_message_for_emit (unpacked))
              {
                continue;
              }

            bool duplicate = false;
            for (int other = 0; other < count; ++other)
              {
                if (other == i)
                  {
                    continue;
                  }
                if (std::equal (unpacked.begin (), unpacked.end (),
                                decodeds + other * kFt8DecodedChars))
                  {
                    duplicate = true;
                    break;
                  }
              }
            if (duplicate)
              {
                continue;
              }

            std::copy (unpacked.begin (), unpacked.end (),
                       decodeds + i * kFt8DecodedChars);
            seed_pack77_hashes_from_message (normalized_unpacked);
            ++changed;
          }

        if (changed == 0)
          {
            break;
          }
        apply_pack77_hash_seed_cache ();
      }
  }
};

struct Ft8A7Entry
{
  float dt {0.0f};
  float freq {0.0f};
  int age {0};
  int hits {1};
  FixedChars<kFt8DecodedChars> message {blank_fixed<kFt8DecodedChars> ()};
  FixedChars<kFt8DecodedChars> replay_message {blank_fixed<kFt8DecodedChars> ()};
};

struct Ft8A7Slot
{
  int previous_count {0};
  int current_count {0};
  std::array<Ft8A7Entry, kFt8MaxEarly> previous {};
  std::array<Ft8A7Entry, kFt8MaxEarly> current {};
};

struct Ft8A7HistoryState
{
  std::array<Ft8A7Slot, kFt8SequenceCount> a7slots {};
  int nutc0 {-1};

  void reset ()
  {
    a7slots = {};
    nutc0 = -1;
  }
};

struct Ft8CqSignalEntry
{
  bool valid {false};
  int nutc {-1};
  float dt {0.0f};
  float freq {0.0f};
  float score {0.0f};
  int hits {0};
  std::array<std::complex<float>, kFt8Nn * 8> cs {};
};

struct Ft8CqSignalHistoryState
{
  std::array<std::array<Ft8CqSignalEntry, kFt8CqSignalMemory>, kFt8SequenceCount> entries {};

  void reset ()
  {
    entries = {};
  }
};

struct Ft8CallGridEntry
{
  bool valid {false};
  int nutc {-1};
  float dt {0.0f};
  float freq {0.0f};
  int hits {0};
  FixedChars<12> call {blank_fixed<12> ()};
  FixedChars<4> grid {blank_fixed<4> ()};
};

struct Ft8CallGridHistoryState
{
  std::array<std::array<Ft8CallGridEntry, kFt8CallGridMemory>, kFt8SequenceCount> entries {};

  void reset ()
  {
    entries = {};
  }
};

struct Ft8KnownCallGridEntry
{
  bool valid {false};
  int nutc {-1};
  float freq {0.0f};
  float dt {0.0f};
  int hits {0};
  FixedChars<12> call {blank_fixed<12> ()};
  FixedChars<4> grid {blank_fixed<4> ()};
};

struct Ft8KnownCallGridState
{
  std::array<Ft8KnownCallGridEntry, kFt8KnownCallGridMemory> entries {};
};

struct Ft8KnownCqCallEntry
{
  bool valid {false};
  int nutc {-1};
  float freq {0.0f};
  float dt {0.0f};
  int hits {0};
  FixedChars<12> call {blank_fixed<12> ()};
};

struct Ft8KnownCqCallState
{
  std::array<Ft8KnownCqCallEntry, kFt8KnownCqCallMemory> entries {};
};

struct Ft8Stage4State
{
  std::array<float, kFt8NMax> dd {};
  std::array<float, kFt8NMax> dd1 {};
  std::array<FixedChars<kFt8DecodedChars>, kFt8MaxEarly> allmessages {};
  std::array<int, kFt8MaxEarly> allsnrs {};
  std::array<int, kFt8Nn * kFt8MaxEarly> itone_save {};
  std::array<float, kFt8MaxEarly> f1_save {};
  std::array<float, kFt8MaxEarly> xdt_save {};
  std::array<bool, kFt8MaxEarly> lsubtracted {};
  std::array<float, kFt8Nh1> early_sbase {};
  std::array<Ft8A7Slot, kFt8SequenceCount> a7 {};
  int nutc0 {-1};
  int early_nutc {-1};
  int ndec_early {0};
  int early_audio_samples {0};
  bool early_sbase_valid {false};

  void resetEarlySlotState ()
  {
    allmessages.fill (blank_fixed<kFt8DecodedChars> ());
    allsnrs.fill (0);
    itone_save.fill (0);
    f1_save.fill (0.0f);
    xdt_save.fill (0.0f);
    lsubtracted.fill (false);
    early_sbase.fill (0.0f);
    early_sbase_valid = false;
    early_audio_samples = 0;
    ndec_early = 0;
  }

  void reset ()
  {
    dd.fill (0.0f);
    dd1.fill (0.0f);
    resetEarlySlotState ();
    a7 = {};
    nutc0 = -1;
    early_nutc = -1;
  }
};

struct Ft8Request
{
  short const* iwave {};
  int nqsoprogress {};
  int nfqso {};
  int nftx {};
  int nutc {};
  int nfa {};
  int nfb {};
  int nzhsym {};
  int ndepth {};
  float emedelay {};
  int ncontest {};
  int nagain {};
  int lft8apon {};
  int ltry_a8 {};
  int lapcqonly {};
  int napwid {};
  int ldiskdat {};
  int ncandthin {100};
  int nft8cycles {1};
  int nft8rxfsens {1};
  bool lft8lowth {false};
  bool lft8subpass {false};
  bool supplemental {false};
  FixedChars<12> mycall {blank_fixed<12> ()};
  FixedChars<12> hiscall {blank_fixed<12> ()};
  FixedChars<6> hisgrid {blank_fixed<6> ()};
};

Ft8Stage4State& stage4_state ()
{
  static Ft8Stage4State state;
  return state;
}

Ft8A7HistoryState& global_a7_history ()
{
  static Ft8A7HistoryState state;
  return state;
}

Ft8CqSignalHistoryState& cq_signal_history ()
{
  static Ft8CqSignalHistoryState state;
  return state;
}

Ft8CallGridHistoryState& call_grid_history ()
{
  static Ft8CallGridHistoryState state;
  return state;
}

Ft8KnownCallGridState*& known_call_grid_override ()
{
  thread_local Ft8KnownCallGridState* pointer = nullptr;
  return pointer;
}

Ft8KnownCallGridState& known_call_grid_history ()
{
  if (Ft8KnownCallGridState* const pointer = known_call_grid_override ())
    {
      return *pointer;
    }
  static Ft8KnownCallGridState state;
  return state;
}

Ft8KnownCqCallState& known_cq_call_history ()
{
  static Ft8KnownCqCallState state;
  return state;
}

void save_known_call_grid (Ft8KnownCallGridState& state, std::string const& call,
                           std::string const& grid, float freq, float dt, int nutc);
void save_known_cq_call (Ft8KnownCqCallState& state, std::string const& call,
                         float freq, float dt, int nutc);
int ft8_utc_delta_seconds (int newer, int older);

int sequence_index_for_utc (int nutc)
{
  return std::abs ((nutc / 5) % 2);
}

bool a7_entries_match (Ft8A7Entry const& lhs, Ft8A7Entry const& rhs)
{
  FixedChars<kFt8DecodedChars> const& lhs_message =
      trim_fixed (lhs.replay_message).empty () ? lhs.message : lhs.replay_message;
  FixedChars<kFt8DecodedChars> const& rhs_message =
      trim_fixed (rhs.replay_message).empty () ? rhs.message : rhs.replay_message;
  return std::fabs (lhs.freq - rhs.freq) <= 3.0f
         && std::equal (lhs_message.begin (), lhs_message.end (), rhs_message.begin ());
}

void append_a7_retained_entry (std::array<Ft8A7Entry, kFt8MaxEarly>& retained,
                               int& retained_count, Ft8A7Entry entry)
{
  if (entry.freq <= -98.0f)
    {
      return;
    }

  for (int i = 0; i < retained_count; ++i)
    {
      if (a7_entries_match (retained[static_cast<size_t> (i)], entry))
        {
          entry.hits = std::min (retained[static_cast<size_t> (i)].hits + entry.hits, 16);
          retained[static_cast<size_t> (i)] = entry;
          return;
        }
    }

  if (retained_count >= kFt8A7MaxRetained || retained_count >= kFt8MaxEarly)
    {
      return;
    }

  retained[static_cast<size_t> (retained_count)] = entry;
  ++retained_count;
}

void prepare_a7_tables (std::array<Ft8A7Slot, kFt8SequenceCount>& a7slots, int& nutc0,
                        int nutc, int nzhsym, int jseq)
{
  if (nzhsym != 41 && nutc == nutc0)
    {
      return;
    }

  Ft8A7Slot& slot = a7slots[static_cast<size_t> (jseq)];
  std::array<Ft8A7Entry, kFt8MaxEarly> retained {};
  int retained_count = 0;

  for (int i = 0; i < slot.current_count && i < kFt8MaxEarly; ++i)
    {
      Ft8A7Entry entry = slot.current[static_cast<size_t> (i)];
      entry.age = 0;
      append_a7_retained_entry (retained, retained_count, entry);
    }

  for (int i = 0; i < slot.previous_count && i < kFt8MaxEarly; ++i)
    {
      Ft8A7Entry entry = slot.previous[static_cast<size_t> (i)];
      if (entry.age >= kFt8A7MaxAge)
        {
          continue;
        }
      ++entry.age;
      append_a7_retained_entry (retained, retained_count, entry);
    }

  slot.previous = retained;
  slot.previous_count = retained_count;
  slot.current_count = 0;
  nutc0 = nutc;
}

void save_a7_entry (std::array<Ft8A7Slot, kFt8SequenceCount>& a7slots, int jseq, float dt, float freq,
                    FixedChars<kFt8DecodedChars> const& decoded, int seed_hits = 1)
{
  std::string const decoded_trimmed = trim_fixed (decoded);
  if (decoded_trimmed.find ('/') != std::string::npos
      || decoded_trimmed.find ('<') != std::string::npos)
    {
      return;
    }

  int nwords = 0;
  std::array<int, kFt8WordCount> nw {};
  std::array<char, kFt8WordCount * kFt8WordChars> words {};
  legacy_pack77_split77_c (decoded.data (), &nwords, nw.data (), words.data ());
  if (nwords < 1)
    {
      return;
    }

  auto word_at = [&] (int index) {
    return trim_block (words.data () + index * kFt8WordChars, kFt8WordChars);
  };

  std::string const word1 = word_at (0);
  if (word1.rfind ("CQ_", 0) == 0)
    {
      return;
    }

  Ft8A7Slot& slot = a7slots[static_cast<size_t> (jseq)];
  if (slot.current_count >= kFt8MaxEarly)
    {
      return;
    }

  std::string const word2 = nwords >= 2 ? word_at (1) : std::string {};
  std::string saved = word1 + (word2.empty () ? std::string {} : " " + word2);
  if (word1 == "CQ" && nwords >= 3 && nw[1] <= 2)
    {
      saved = "CQ " + word2 + " " + word_at (2);
    }

  std::string const last_word = word_at (std::max (0, nwords - 1));
  if (is_grid4 (last_word))
    {
      saved += " " + last_word;
    }

  Ft8A7Entry entry;
  entry.dt = dt;
  entry.freq = freq;
  entry.age = 0;
  entry.hits = std::max (1, seed_hits);
  entry.message = fixed_from_string<kFt8DecodedChars> (saved);
  entry.replay_message = is_strict_standard_ft8_message (decoded)
      ? decoded
      : entry.message;
  slot.current[static_cast<size_t> (slot.current_count)] = entry;
  ++slot.current_count;

  int saved_words = 0;
  std::array<int, kFt8WordCount> saved_nw {};
  std::array<char, kFt8WordCount * kFt8WordChars> saved_split {};
  legacy_pack77_split77_c (entry.message.data (), &saved_words, saved_nw.data (), saved_split.data ());
  if (saved_words < 2)
    {
      return;
    }
  if (word1 == "CQ")
    {
      return;
    }
  std::string const peer = trim_block (saved_split.data () + kFt8WordChars, kFt8WordChars);

  for (int i = 0; i < slot.previous_count && i < kFt8MaxEarly; ++i)
    {
      Ft8A7Entry& previous = slot.previous[static_cast<size_t> (i)];
      if (previous.freq <= -98.0f)
        {
          continue;
        }
      std::string const previous_trimmed = trim_fixed (previous.message);
      std::string const needle = " " + peer;
      if (std::fabs (freq - previous.freq) <= 3.0f
          && previous_trimmed.find (needle) != std::string::npos
          && previous_trimmed.find (needle) >= 2)
        {
          previous.freq = -98.0f;
        }
    }
}

bool messages_equal (FixedChars<kFt8DecodedChars> const& lhs,
                     FixedChars<kFt8DecodedChars> const& rhs)
{
  return std::equal (lhs.begin (), lhs.end (), rhs.begin ());
}

FixedChars<13> widen_call_for_pack77 (FixedChars<12> const& value)
{
  FixedChars<13> out = blank_fixed<13> ();
  std::copy_n (value.begin (), 12, out.begin ());
  return out;
}

bool ft8sd_hint_is_cq (FixedChars<kFt8DecodedChars> const& message)
{
  return starts_with (message, "CQ ")
      || starts_with (message, "DE ")
      || starts_with (message, "QRZ ");
}

FixedChars<kFt8DecodedChars> ft8_a7_replay_message (Ft8A7Entry const& hint)
{
  return trim_fixed (hint.replay_message).empty () ? hint.message : hint.replay_message;
}

bool ft8_message_has_word (FixedChars<kFt8DecodedChars> const& message, std::string const& word)
{
  if (word.empty ())
    {
      return false;
    }
  std::vector<std::string> const words = split_words (trim_fixed (message));
  return std::find (words.begin (), words.end (), word) != words.end ();
}

bool ft8_hint_matches_context (Ft8A7Entry const& hint, Ft8Request const& request)
{
  FixedChars<kFt8DecodedChars> const message = ft8_a7_replay_message (hint);
  if (hint.freq <= -98.0f || hint.age > 3 || !is_strict_standard_ft8_message (message))
    {
      return false;
    }

  std::string const dxcall = trim_fixed (request.hiscall);
  if (!dxcall.empty ())
    {
      // When the operator or auto-seq already has a current partner, never use
      // a retained FT8 hint from another callsign.  This keeps the very-deep
      // path from reviving stale stations.
      if (!ft8_message_has_word (message, dxcall))
        {
          return false;
        }
      std::string const mycall = trim_fixed (request.mycall);
      return ft8sd_hint_is_cq (message)
          || mycall.empty ()
          || ft8_message_has_word (message, mycall);
    }

  // Without a current partner, replay directed messages only while they are
  // fresh unless the same complete message has repeated in this A7 bucket.
  // Repetition is tracked per frequency/message, so older hints still need to
  // match the current signal position before they can be used.
  return ft8sd_hint_is_cq (message) || hint.age <= 1 || hint.hits >= 2;
}

bool ft8_live_advanced_history_enabled (Ft8Request const& request)
{
  return request.nzhsym >= 50
         && request.ndepth >= 3
         && !request.supplemental
         && request.lft8apon != 0
         && request.lft8lowth
         && request.lft8subpass
         && request.nft8cycles >= 3
         && request.ncontest == 0
         && trim_fixed (request.hiscall).empty ();
}

bool ft8_history_replay_enabled (Ft8Request const& request)
{
  return (request.ndepth >= 4 && request.supplemental)
         || ft8_live_advanced_history_enabled (request);
}

bool ft8_lightweight_repeated_hint_enabled (Ft8Request const& request)
{
  return request.nzhsym >= 50
         && request.ndepth >= 3
         && !request.supplemental
         && request.ncontest == 0
         && trim_fixed (request.hiscall).empty ();
}

bool ft8_repeated_hint_replay_enabled (Ft8Request const& request)
{
  return ft8_history_replay_enabled (request)
         || ft8_lightweight_repeated_hint_enabled (request);
}

bool ft8_known_cq_replay_enabled (Ft8Request const& request)
{
  return ft8_history_replay_enabled (request)
         || (request.nzhsym >= 50
             && request.ndepth >= 3
             && !request.supplemental
             && request.lft8apon != 0
             && request.ncontest == 0
             && trim_fixed (request.hiscall).empty ());
}

bool ft8_lightweight_known_cq_grid_enabled (Ft8Request const& request)
{
  return request.nzhsym >= 50
         && request.ndepth >= 3
         && !request.supplemental
         && request.ncontest == 0
         && trim_fixed (request.hiscall).empty ();
}

bool ft8_targeted_low_subpass_enabled (Ft8Request const& request)
{
  return request.nzhsym >= 50
         && request.ndepth >= 3
         && !request.supplemental
         && request.lft8apon != 0
         && request.ncontest == 0
         && trim_fixed (request.hiscall).empty ();
}

Ft8Request ft8_targeted_low_subpass_request (Ft8Request request)
{
  request.lft8lowth = true;
  request.lft8subpass = true;
  request.nft8cycles = std::max (request.nft8cycles, 3);
  return request;
}

Ft8A7Entry const* find_ft8_repeated_hint (Ft8A7Slot const* hints, Ft8Request const& request,
                                          float f1, float callback_dt,
                                          float freq_tolerance = 7.0f,
                                          float dt_tolerance = 0.55f)
{
  if (!hints || !ft8_repeated_hint_replay_enabled (request))
    {
      return nullptr;
    }

  bool const live_history = ft8_live_advanced_history_enabled (request);
  if (live_history)
    {
      freq_tolerance = std::min (freq_tolerance, 4.0f);
      dt_tolerance = std::min (dt_tolerance, 0.28f);
    }

  Ft8A7Entry const* best = nullptr;
  float best_score = 1.0e30f;
  int const previous_limit = std::min (hints->previous_count, kFt8MaxEarly);
  for (int index = 0; index < previous_limit; ++index)
    {
      Ft8A7Entry const& hint = hints->previous[static_cast<size_t> (index)];
      if (!ft8_hint_matches_context (hint, request))
        {
          continue;
        }
      FixedChars<kFt8DecodedChars> const hint_message = ft8_a7_replay_message (hint);

      float const freq_delta = std::fabs (hint.freq - f1);
      float const dt_delta = std::fabs (hint.dt - callback_dt);
      bool const no_dx_context = trim_fixed (request.hiscall).empty ();
      bool const directed_replay = no_dx_context && !ft8sd_hint_is_cq (hint_message);
      float const max_freq_delta = directed_replay ? std::min (freq_tolerance, 3.0f) : freq_tolerance;
      float const max_dt_delta = directed_replay ? std::min (dt_tolerance, 0.22f) : dt_tolerance;
      if (freq_delta > max_freq_delta || dt_delta > max_dt_delta)
        {
          continue;
        }

      float const score = freq_delta + 6.0f * dt_delta + static_cast<float> (hint.age);
      if (score < best_score)
        {
          best = &hint;
          best_score = score;
        }
    }
  return best;
}

void append_ft8_hint_candidates (Ft8A7Slot const* hints, Ft8Request const& request,
                                 int ifa, int ifb, float* candidate,
                                 int& ncand)
{
  if (!hints || !candidate || !ft8_repeated_hint_replay_enabled (request))
    {
      return;
    }

  bool const live_history = ft8_live_advanced_history_enabled (request);
  int const append_limit = live_history ? 24 : 32;
  std::array<float, 4 * 32> hinted_candidates {};
  int appended = 0;
  int const previous_limit = std::min (hints->previous_count, kFt8MaxEarly);
  for (int index = 0; index < previous_limit && appended < append_limit; ++index)
    {
      Ft8A7Entry const& hint = hints->previous[static_cast<size_t> (index)];
      if (!ft8_hint_matches_context (hint, request))
        {
          continue;
        }
      FixedChars<kFt8DecodedChars> const hint_message = ft8_a7_replay_message (hint);
      if (hint.freq < static_cast<float> (ifa) - 4.0f
          || hint.freq > static_cast<float> (ifb) + 4.0f)
        {
          continue;
        }

      bool duplicate = false;
      bool const directed_grid_hint = !ft8sd_hint_is_cq (hint_message)
          && [&] {
               std::vector<std::string> const words =
                   split_words (trim_fixed (hint_message));
               return words.size () == 3 && is_grid4 (words[2]);
             } ();
      for (int i = 0; i < ncand; ++i)
        {
          float const freq = candidate[static_cast<size_t> (i * 4 + 0)];
          float const dt = candidate[static_cast<size_t> (i * 4 + 1)] - 0.5f;
          if (std::fabs (freq - hint.freq) <= 2.0f && std::fabs (dt - hint.dt) <= 0.20f)
            {
              float const existing_cq =
                  candidate[static_cast<size_t> (i * 4 + 3)];
              duplicate = !(directed_grid_hint && existing_cq >= 1.5f);
              break;
            }
        }
      if (duplicate)
        {
          continue;
        }

      hinted_candidates[static_cast<size_t> (appended * 4 + 0)] = hint.freq;
      hinted_candidates[static_cast<size_t> (appended * 4 + 1)] = hint.dt + 0.5f;
      hinted_candidates[static_cast<size_t> (appended * 4 + 2)] = 0.55f;
      hinted_candidates[static_cast<size_t> (appended * 4 + 3)] =
          ft8sd_hint_is_cq (hint_message) ? 2.0f : 0.0f;
      ++appended;
    }
  if (appended <= 0)
    {
      return;
    }

  int const keep = std::min (ncand, kFt8MaxCand - appended);
  for (int i = keep - 1; i >= 0; --i)
    {
      for (int j = 0; j < 4; ++j)
        {
          candidate[static_cast<size_t> ((i + appended) * 4 + j)] =
              candidate[static_cast<size_t> (i * 4 + j)];
        }
    }
  for (int i = 0; i < appended; ++i)
    {
      for (int j = 0; j < 4; ++j)
        {
          candidate[static_cast<size_t> (i * 4 + j)] =
              hinted_candidates[static_cast<size_t> (i * 4 + j)];
        }
    }
  ncand = keep + appended;
}

[[maybe_unused]] void prepend_live_cq_companion_candidates (Ft8Request const& request, int ipass,
                                                           int ifa, int ifb, float* candidate,
                                                           int& ncand)
{
  if (!candidate
      || request.nzhsym < 50
      || request.ndepth < 3
      || request.supplemental
      || request.lft8apon == 0
      || request.ncontest != 0
      || !trim_fixed (request.hiscall).empty ()
      || ipass > 2)
    {
      return;
    }

  constexpr int kMaxCompanions = 16;
  struct CompanionCandidate
  {
    std::array<float, 4> values {};
    float score {1.0e30f};
  };
  std::array<CompanionCandidate, 64> companion_pool {};
  int pool_count = 0;
  int const scan_limit = std::min (ncand, 96);

  (void) ifa;
  (void) ifb;
  auto duplicate_pool = [&] (float freq, float dt) {
    for (int index = 0; index < pool_count; ++index)
      {
        float const existing_freq =
            companion_pool[static_cast<size_t> (index)].values[0];
        float const existing_dt =
            companion_pool[static_cast<size_t> (index)].values[1];
        if (std::fabs (existing_freq - freq) <= 0.75f
            && std::fabs (existing_dt - dt) <= 0.08f)
          {
            return true;
          }
    }
    return false;
  };

  for (int index = 0; index < scan_limit; ++index)
    {
      float const freq = candidate[static_cast<size_t> (index * 4 + 0)];
      float const dt = candidate[static_cast<size_t> (index * 4 + 1)];
      float const sync = candidate[static_cast<size_t> (index * 4 + 2)];
      float const cq = candidate[static_cast<size_t> (index * 4 + 3)];
      if (cq < 1.75f || sync < 3.0f)
        {
          continue;
        }

      for (int other = index + 1; other < scan_limit; ++other)
        {
          float const other_freq = candidate[static_cast<size_t> (other * 4 + 0)];
          float const other_dt = candidate[static_cast<size_t> (other * 4 + 1)];
          float const other_sync = candidate[static_cast<size_t> (other * 4 + 2)];
          float const other_cq = candidate[static_cast<size_t> (other * 4 + 3)];
          float const delta = std::fabs (other_freq - freq);
          if (other_cq < 1.75f
              || other_sync < 1.0f
              || std::fabs (other_dt - dt) > 0.08f
              || delta < 2.6f
              || delta > 3.7f)
            {
              continue;
            }
          int const promote = other_freq >= freq ? other : index;
          float const promote_freq = candidate[static_cast<size_t> (promote * 4 + 0)];
          float const promote_dt = candidate[static_cast<size_t> (promote * 4 + 1)];
          if (duplicate_pool (promote_freq, promote_dt))
            {
              continue;
            }
          float const pair_sync = std::min (sync, other_sync);
          float const pair_cq = std::min (cq, other_cq);
          CompanionCandidate entry;
          for (int field = 0; field < 4; ++field)
            {
              entry.values[static_cast<size_t> (field)] =
                  candidate[static_cast<size_t> (promote * 4 + field)];
            }
          entry.score = std::fabs (delta - 3.125f)
                        - 0.035f * std::min (pair_sync, 12.0f)
                        - 0.10f * std::min (pair_cq, 3.0f)
                        + 0.001f * static_cast<float> (std::min (index, other));
          if (pool_count < static_cast<int> (companion_pool.size ()))
            {
              companion_pool[static_cast<size_t> (pool_count++)] = entry;
            }
          else
            {
              int worst = 0;
              for (int item = 1; item < pool_count; ++item)
                {
                  if (companion_pool[static_cast<size_t> (item)].score
                      > companion_pool[static_cast<size_t> (worst)].score)
                    {
                      worst = item;
                    }
                }
              if (entry.score < companion_pool[static_cast<size_t> (worst)].score)
                {
                  companion_pool[static_cast<size_t> (worst)] = entry;
                }
            }
        }
    }

  if (pool_count <= 0)
    {
      return;
    }
  std::sort (companion_pool.begin (), companion_pool.begin () + pool_count,
             [] (CompanionCandidate const& lhs, CompanionCandidate const& rhs) {
               return lhs.score < rhs.score;
             });
  int const companion_count = std::min (pool_count, kMaxCompanions);
  if (companion_count <= 0)
    {
      return;
    }
  if (debug_ft8_focus_replay ())
    {
      std::cerr << "[FT8COMP] utc=" << request.nutc
                << " pass=" << ipass
                << " count=" << companion_count
                << " first_freq=" << companion_pool[0].values[0]
                << " first_dt=" << companion_pool[0].values[1]
                << " ncand_before=" << ncand
                << '\n';
    }

  int const keep = std::min (ncand, kFt8MaxCand - companion_count);
  for (int index = keep - 1; index >= 0; --index)
    {
      for (int field = 0; field < 4; ++field)
        {
          candidate[static_cast<size_t> ((index + companion_count) * 4 + field)] =
              candidate[static_cast<size_t> (index * 4 + field)];
        }
    }
  for (int index = 0; index < companion_count; ++index)
    {
      for (int field = 0; field < 4; ++field)
        {
          candidate[static_cast<size_t> (index * 4 + field)] =
              companion_pool[static_cast<size_t> (index)].values[static_cast<size_t> (field)];
        }
    }
  ncand = keep + companion_count;
}

void append_known_cq_candidates (Ft8KnownCallGridState const& state,
                                 Ft8Request const& request, int ifa, int ifb,
                                 float* candidate, int& ncand)
{
  if (debug_known_cq_replay ())
    {
      int valid_entries = 0;
      for (Ft8KnownCallGridEntry const& entry : state.entries)
        {
          if (entry.valid)
            {
              if (valid_entries < 12)
                {
                  std::cerr << "[KNOWNCQ] entry call=" << trim_fixed (entry.call)
                            << " grid=" << trim_fixed (entry.grid)
                            << " freq=" << entry.freq
                            << " dt=" << entry.dt
                            << " nutc=" << entry.nutc
                            << " hits=" << entry.hits << '\n';
                }
              ++valid_entries;
            }
        }
      std::cerr << "[KNOWNCQ] append-enter valid=" << valid_entries
                << " ndepth=" << request.ndepth
                << " nzhsym=" << request.nzhsym
                << " supplemental=" << (request.supplemental ? 1 : 0)
                << " lft8apon=" << request.lft8apon
                << " low=" << (request.lft8lowth ? 1 : 0)
                << " subpass=" << (request.lft8subpass ? 1 : 0)
                << " cycles=" << request.nft8cycles
                << " hiscall='" << trim_fixed (request.hiscall) << "'"
                << " nutc=" << request.nutc
                << " ifa=" << ifa
                << " ifb=" << ifb
                << " ncand=" << ncand << '\n';
    }
  if (!candidate
      || !ft8_known_cq_replay_enabled (request)
      || request.lft8apon == 0
      || !trim_fixed (request.hiscall).empty ()
      || request.nutc <= 0)
    {
      return;
    }

  struct CandidateHint
  {
    float freq {0.0f};
    float dt {0.0f};
    float score {0.0f};
  };

  auto local_support_score = [&] (Ft8KnownCallGridEntry const& entry) {
    float best_score = 1.0e30f;
    for (int index = 0; index < ncand; ++index)
      {
        float const candidate_freq = candidate[static_cast<size_t> (index * 4 + 0)];
        float const candidate_dt =
            candidate[static_cast<size_t> (index * 4 + 1)] - 0.5f;
        float const candidate_sync = candidate[static_cast<size_t> (index * 4 + 2)];
        float const candidate_cq = candidate[static_cast<size_t> (index * 4 + 3)];
        float const freq_delta = std::fabs (candidate_freq - entry.freq);
        float const dt_delta = std::fabs (candidate_dt - entry.dt);
        // Soglia di evidenza per riproporre un nominativo gia' sentito.
        // Sotto i -23 dB il sync sta spesso sotto 0,32 e il replay non parte
        // affatto: e' qui che JTDX, con Aggressive=1, continua a vedere
        // stazioni che noi perdiamo. Abbassarla e' mirato, non e' una
        // ricerca cieca: vale solo entro 5,5 Hz e 0,35 s da dove quella
        // stessa stazione era stata decodificata poco prima.
        static float const sync_floor = [] {
          char const* raw = std::getenv ("DECODIUM_FT8_KNOWNCQ_SYNC");
          float const v = raw ? static_cast<float> (std::atof (raw)) : 0.0f;
          return (v > 0.0f && v <= 5.0f) ? v : 0.32f;
        }();
        bool const has_signal_evidence =
            candidate_sync >= sync_floor || candidate_cq >= 1.0f;
        if (!has_signal_evidence || freq_delta > 5.5f || dt_delta > 0.35f)
          {
            continue;
          }

        float const support =
            freq_delta + 10.0f * dt_delta
            - 0.08f * std::min (candidate_sync, 10.0f)
            - 0.25f * std::min (candidate_cq, 3.0f);
        best_score = std::min (best_score, support);
      }
    return best_score;
  };

  std::array<CandidateHint, 96> hints {};
  int hint_count = 0;
  for (Ft8KnownCallGridEntry const& entry : state.entries)
    {
      if (!entry.valid || entry.freq <= 0.0f || entry.nutc <= 0)
        {
          if (debug_known_cq_replay () && entry.valid)
            {
              std::cerr << "[KNOWNCQ] skip invalid-fields call=" << trim_fixed (entry.call)
                        << " grid=" << trim_fixed (entry.grid)
                        << " freq=" << entry.freq
                        << " nutc=" << entry.nutc << '\n';
            }
          continue;
        }
      int const age_seconds = ft8_utc_delta_seconds (request.nutc, entry.nutc);
      if (age_seconds <= 0 || age_seconds > kFt8KnownCallGridMaxAgeSeconds)
        {
          if (debug_known_cq_replay ())
            {
              std::cerr << "[KNOWNCQ] skip age call=" << trim_fixed (entry.call)
                        << " grid=" << trim_fixed (entry.grid)
                        << " age=" << age_seconds
                        << " req_nutc=" << request.nutc
                        << " entry_nutc=" << entry.nutc << '\n';
            }
          continue;
        }
      if (age_seconds > ft8_knowncq_fast_age ())
        {
          continue;
        }
      if (entry.freq < static_cast<float> (ifa) - 3.0f
          || entry.freq > static_cast<float> (ifb) + 3.0f
          || entry.dt < -2.5f
          || entry.dt > 2.5f)
        {
          if (debug_known_cq_replay ())
            {
              std::cerr << "[KNOWNCQ] skip window call=" << trim_fixed (entry.call)
                        << " grid=" << trim_fixed (entry.grid)
                        << " freq=" << entry.freq
                        << " dt=" << entry.dt
                        << " ifa=" << ifa
                        << " ifb=" << ifb << '\n';
            }
          continue;
        }

      float const support_score = local_support_score (entry);
      if (support_score >= 1.0e20f)
        {
          if (debug_known_cq_replay ())
            {
              std::cerr << "[KNOWNCQ] skip unsupported call=" << trim_fixed (entry.call)
                        << " grid=" << trim_fixed (entry.grid)
                        << " freq=" << entry.freq
                        << " dt=" << entry.dt << '\n';
            }
          continue;
        }

      CandidateHint hint;
      hint.freq = entry.freq;
      hint.dt = entry.dt;
      hint.score = support_score
                   + 0.0005f * static_cast<float> (age_seconds)
                   - 0.10f * static_cast<float> (std::min (entry.hits, 20));
      if (debug_known_cq_replay ())
        {
          std::cerr << "[KNOWNCQ] append call=" << trim_fixed (entry.call)
                    << " grid=" << trim_fixed (entry.grid)
                    << " freq=" << entry.freq
                    << " dt=" << entry.dt
                    << " age=" << age_seconds
                    << " hits=" << entry.hits
                    << " support=" << support_score << '\n';
        }

      int insert_at = hint_count;
      if (hint_count >= static_cast<int> (hints.size ()))
        {
          int worst = 0;
          for (int index = 1; index < hint_count; ++index)
            {
              if (hints[static_cast<size_t> (index)].score
                  > hints[static_cast<size_t> (worst)].score)
                {
                  worst = index;
                }
            }
          if (hint.score >= hints[static_cast<size_t> (worst)].score)
            {
              continue;
            }
          insert_at = worst;
        }
      else
        {
          ++hint_count;
        }
      hints[static_cast<size_t> (insert_at)] = hint;
    }

  if (hint_count <= 0)
    {
      return;
    }

  std::sort (hints.begin (), hints.begin () + hint_count,
             [] (CandidateHint const& lhs, CandidateHint const& rhs) {
               return lhs.score < rhs.score;
             });

  int const append_limit = ft8_live_advanced_history_enabled (request) ? 24 : 16;
  int const appended = std::min ({hint_count, append_limit, kFt8MaxCand - ncand});
  if (appended <= 0)
    {
      return;
    }

  for (int i = 0; i < appended; ++i)
    {
      CandidateHint const& hint = hints[static_cast<size_t> (i)];
      int const index = ncand + i;
      candidate[static_cast<size_t> (index * 4 + 0)] = hint.freq;
      candidate[static_cast<size_t> (index * 4 + 1)] = hint.dt + 0.5f;
      candidate[static_cast<size_t> (index * 4 + 2)] = 0.55f;
      candidate[static_cast<size_t> (index * 4 + 3)] = 3.0f;
    }
  ncand += appended;
}

bool ft8sd1_replay_accepts (float const* s8, std::array<int, kFt8Nn> const& expected_tones)
{
  if (!s8)
    {
      return false;
    }

  std::array<std::array<float, 8>, 58> symbol_power {};
  std::array<int, 58> detected {};
  for (int index = 0; index < 58; ++index)
    {
      int const symbol = index < 29 ? index + 7 : index + 14;
      int best_tone = 0;
      float best_power = s8[8 * symbol];
      for (int tone = 0; tone < 8; ++tone)
        {
          float const value = s8[tone + 8 * symbol];
          symbol_power[static_cast<size_t> (index)][static_cast<size_t> (tone)] = value;
          if (value > best_power)
            {
              best_tone = tone;
              best_power = value;
            }
        }
      detected[static_cast<size_t> (index)] = best_tone;
      symbol_power[static_cast<size_t> (index)][static_cast<size_t> (best_tone)] = 0.0f;
    }

  std::array<bool, 58> matched {};
  int match_count = 0;
  int crc_match_count = 0;
  for (int index = 0; index < 58; ++index)
    {
      int const symbol = index < 29 ? index + 7 : index + 14;
      if (expected_tones[static_cast<size_t> (symbol)] == detected[static_cast<size_t> (index)])
        {
          matched[static_cast<size_t> (index)] = true;
          ++match_count;
          if (index >= 25)
            {
              ++crc_match_count;
            }
        }
    }
  if (match_count > 29 && crc_match_count > 10)
    {
      return true;
    }

  if (match_count < 22)
    {
      return false;
    }

  int second_match_count = match_count;
  int second_crc_match_count = crc_match_count;
  for (int index = 0; index < 58; ++index)
    {
      if (matched[static_cast<size_t> (index)])
        {
          continue;
        }
      int const symbol = index < 29 ? index + 7 : index + 14;
      int second_tone = 0;
      float second_power = symbol_power[static_cast<size_t> (index)][0];
      for (int tone = 1; tone < 8; ++tone)
        {
          float const value = symbol_power[static_cast<size_t> (index)][static_cast<size_t> (tone)];
          if (value > second_power)
            {
              second_tone = tone;
              second_power = value;
            }
        }
      if (expected_tones[static_cast<size_t> (symbol)] == second_tone)
        {
          ++second_match_count;
          if (index >= 25)
            {
              ++second_crc_match_count;
            }
        }
    }

  return second_match_count > 41 && second_crc_match_count > 19;
}

bool try_ft8sd_known_message (FixedChars<kFt8DecodedChars> const& hint_message,
                              Ft8Request const& request, float const* s8,
                              int nsync, float xbase,
                              FixedChars<kFt8DecodedChars>& msg37,
                              float& xsnr, std::array<int, kFt8Nn>& itone,
                              std::array<signed char, kFt8Bits>& message77,
                              float const* llra, float const* llrb,
                              float const* llrc, float const* llrd,
                              int hard_limit_override = -1,
                              float dmin_limit_override = -1.0f,
                              bool allow_metrics_fallback = true)
{
  std::array<int, kFt8Nn> expected_tones {};
  std::array<signed char, 174> expected_codeword {};
  FixedChars<kFt8DecodedChars> msgsent = blank_fixed<kFt8DecodedChars> ();
  if (ftx_encode_ft8_candidate_c (hint_message.data (), msgsent.data (),
                                  expected_tones.data (),
                                  expected_codeword.data ()) == 0)
    {
      return false;
    }

  FixedChars<kFt8DecodedChars> sd_message = blank_fixed<kFt8DecodedChars> ();
  std::array<int, kFt8Nn> sd_tones {};
  int const lcq = ft8sd_hint_is_cq (hint_message) ? 1 : 0;
  auto accept_expected_hint_by_metrics = [&] {
    if (ft8sd1_replay_accepts (s8, expected_tones))
      {
        sd_message = hint_message;
        sd_tones = expected_tones;
        return true;
      }

    if (!allow_metrics_fallback)
      {
        return false;
      }

    float hint_pow = 0.0f;
    float hint_dmin = 1.0e30f;
    int hint_nhard = 174;
    if (nsync < 1 || !llra || !llrb || !llrc || !llrd)
      {
        return false;
      }
    ftx_ft8a7_measure_candidate_c (s8, 8, kFt8Nn, expected_tones.data (),
                                   expected_codeword.data (), llra, llrb,
                                   llrc, llrd, &hint_pow, &hint_dmin,
                                   &hint_nhard);
    int const hard_limit = hard_limit_override >= 0
        ? hard_limit_override
        : (nsync >= 5 ? 80 : 84);
    float const dmin_limit = dmin_limit_override >= 0.0f
        ? dmin_limit_override
        : (nsync >= 5 ? 185.0f : 205.0f);
    if (hint_nhard > hard_limit || hint_dmin > dmin_limit || hint_pow <= 0.0f)
      {
        if (debug_known_cq_replay ())
          {
            std::cerr << "[KNOWNCQ] reject metrics hint=" << trim_fixed (hint_message)
                      << " nsync=" << nsync
                      << " nhard=" << hint_nhard
                      << " hard_limit=" << hard_limit
                      << " dmin=" << hint_dmin
                      << " dmin_limit=" << dmin_limit
                      << " pow=" << hint_pow << '\n';
          }
        return false;
      }
    if (debug_known_cq_replay ())
      {
        std::cerr << "[KNOWNCQ] accept metrics hint=" << trim_fixed (hint_message)
                  << " nsync=" << nsync
                  << " nhard=" << hint_nhard
                  << " hard_limit=" << hard_limit
                  << " dmin=" << hint_dmin
                  << " dmin_limit=" << dmin_limit
                  << " pow=" << hint_pow << '\n';
      }
    sd_message = hint_message;
    sd_tones = expected_tones;
    return true;
  };

  bool const sdvar_ok =
      ftx_ft8sdvar_c (s8, 0.0f, expected_tones.data (), hint_message.data (),
                      request.mycall.data (), lcq, sd_message.data (),
                      sd_tones.data ()) != 0;
  if (!sdvar_ok || !messages_equal (sd_message, hint_message))
    {
      if (debug_known_cq_replay ())
        {
          std::cerr << "[KNOWNCQ] sdvar fallback hint=" << trim_fixed (hint_message)
                    << " ok=" << (sdvar_ok ? 1 : 0)
                    << " got=" << trim_fixed (sd_message) << '\n';
        }
      if (!accept_expected_hint_by_metrics ())
        {
          return false;
        }
    }
  if (!is_strict_standard_ft8_message (sd_message))
    {
      return false;
    }

  int i3 = -1;
  int n3 = -1;
  bool pack_ok = false;
  FixedChars<kFt8DecodedChars> packed_msgsent = blank_fixed<kFt8DecodedChars> ();
  std::array<signed char, kFt8Bits> packed_bits {};
  legacy_pack77_pack_c (sd_message.data (), &i3, &n3, packed_bits.data (),
                        packed_msgsent.data (), &pack_ok, 0);
  if (!pack_ok)
    {
      return false;
    }

  float hinted_snr = 0.0f;
  if (ftx_ft8_compute_snr_c (s8, 8, kFt8Nn, sd_tones.data (), xbase,
                             request.nagain, nsync, &hinted_snr) == 0)
    {
      hinted_snr = -24.0f;
    }

  msg37 = sd_message;
  xsnr = hinted_snr;
  itone = sd_tones;
  message77 = packed_bits;
  return true;
}

bool try_ft8sd_repeated_hint (Ft8A7Slot const* hints, Ft8Request const& request,
                              float const* s8, int nsync, float f1, float xdt,
                              float xbase, FixedChars<kFt8DecodedChars>& msg37,
                              float& xsnr, std::array<int, kFt8Nn>& itone,
                              std::array<signed char, kFt8Bits>& message77,
                              float const* llra, float const* llrb,
                              float const* llrc, float const* llrd)
{
  if (!hints || !s8 || !ft8_repeated_hint_replay_enabled (request))
    {
      return false;
    }

  float const callback_dt = xdt - 0.5f;
  Ft8A7Entry const* matched_hint = find_ft8_repeated_hint (hints, request, f1, callback_dt);
  if (!matched_hint)
    {
      return false;
    }

  Ft8A7Entry const& hint = *matched_hint;
  FixedChars<kFt8DecodedChars> const hint_message = ft8_a7_replay_message (hint);
  float const freq_delta = std::fabs (hint.freq - f1);
  float const dt_delta = std::fabs (hint.dt - callback_dt);
  bool const directed_replay =
      trim_fixed (request.hiscall).empty () && !ft8sd_hint_is_cq (hint_message);
  bool const repeated_directed_metrics =
      ft8_lightweight_repeated_hint_enabled (request)
      && directed_replay
      && hint.hits >= 2
      && hint.age <= 3
      && nsync >= 8
      && freq_delta <= 1.5f
      && dt_delta <= 0.18f;
  bool const allow_metrics_fallback =
      ft8_history_replay_enabled (request) || repeated_directed_metrics;
  int const hard_limit = repeated_directed_metrics ? 58 : -1;
  float const dmin_limit = repeated_directed_metrics ? 170.0f : -1.0f;
  bool const replay_ok =
      try_ft8sd_known_message (hint_message, request, s8, nsync, xbase,
                               msg37, xsnr, itone, message77,
                               llra, llrb, llrc, llrd, hard_limit, dmin_limit,
                               allow_metrics_fallback);
  if (!replay_ok)
    {
      return false;
    }
  if (is_directed_pair_only_message (hint_message)
      && is_directed_pair_only_message (msg37)
      && trim_fixed (hint_message) == trim_fixed (msg37))
    {
      return false;
    }
  return true;
}

struct Ft8TerminalPairHint
{
  FixedChars<12> call_1 {blank_fixed<12> ()};
  FixedChars<12> call_2 {blank_fixed<12> ()};
};

bool append_terminal_pair_hint (std::vector<Ft8TerminalPairHint>& hints,
                                std::string const& call_1,
                                std::string const& call_2)
{
  if (call_1.empty () || call_2.empty () || call_1 == call_2
      || !is_standard_call_word (call_1)
      || !is_standard_call_word (call_2))
    {
      return false;
    }
  for (Ft8TerminalPairHint const& hint : hints)
    {
      if (trim_fixed (hint.call_1) == call_1
          && trim_fixed (hint.call_2) == call_2)
        {
          return false;
        }
    }
  Ft8TerminalPairHint hint;
  hint.call_1 = fixed_from_string<12> (call_1);
  hint.call_2 = fixed_from_string<12> (call_2);
  hints.push_back (hint);
  return true;
}

bool terminal_pair_context_message (Ft8A7Entry const& entry,
                                    std::string const& source_call_1,
                                    std::string const& source_call_2)
{
  std::vector<std::string> const words =
      split_words (trim_fixed (ft8_a7_replay_message (entry)));
  if (words.size () != 3
      || words[0] != source_call_1
      || words[1] != source_call_2)
    {
      return false;
    }
  bool const r_report =
      words[2].size () == 4
      && words[2][0] == 'R'
      && (words[2][1] == '-' || words[2][1] == '+');
  return is_grid4 (words[2])
         || (is_report_token (words[2]) && !r_report && words[2] != "73");
}

bool terminal_pair_has_prior_context (Ft8Stage4State const& state,
                                      std::string const& source_call_1,
                                      std::string const& source_call_2)
{
  for (Ft8A7Slot const& slot : state.a7)
    {
      int const current_limit = std::min (slot.current_count, kFt8MaxEarly);
      for (int index = 0; index < current_limit; ++index)
        {
          if (terminal_pair_context_message (
                  slot.current[static_cast<size_t> (index)],
                  source_call_1, source_call_2))
            {
              return true;
            }
        }
      int const previous_limit = std::min (slot.previous_count, kFt8MaxEarly);
      for (int index = 0; index < previous_limit; ++index)
        {
          if (terminal_pair_context_message (
                  slot.previous[static_cast<size_t> (index)],
                  source_call_1, source_call_2))
            {
              return true;
            }
        }
    }
  return false;
}

void append_reverse_terminal_hints_from_entry (
    std::vector<Ft8TerminalPairHint>& hints, Ft8Stage4State const& state,
    Ft8A7Entry const& entry)
{
  if (entry.freq <= -98.0f)
    {
      return;
    }
  std::vector<std::string> const words =
      split_words (trim_fixed (ft8_a7_replay_message (entry)));
  bool const is_r_report =
      words.size () == 3
      && words[2].size () == 4
      && words[2][0] == 'R'
      && (words[2][1] == '-' || words[2][1] == '+')
      && std::isdigit (static_cast<unsigned char> (words[2][2]))
      && std::isdigit (static_cast<unsigned char> (words[2][3]));
  if (words.size () != 3
      || words[0] == "CQ"
      || !is_r_report
      || words[0].find ('<') != std::string::npos
      || words[1].find ('<') != std::string::npos)
    {
      return;
    }
  if (!terminal_pair_has_prior_context (state, words[0], words[1]))
    {
      return;
    }
  append_terminal_pair_hint (hints, words[1], words[0]);
}

void append_reverse_terminal_hints_from_slot (
    std::vector<Ft8TerminalPairHint>& hints, Ft8Stage4State const& state,
    Ft8A7Slot const& slot)
{
  int const current_limit = std::min (slot.current_count, kFt8MaxEarly);
  for (int index = current_limit - 1; index >= 0; --index)
    {
      append_reverse_terminal_hints_from_entry (
          hints, state, slot.current[static_cast<size_t> (index)]);
      if (hints.size () >= 12)
        {
          return;
        }
    }
}

bool try_reverse_terminal_pair_rescue (
    Ft8Request const& request, int jseq, float const* s8, int nsync,
    float xbase, FixedChars<kFt8DecodedChars>& msg37, float& xsnr,
    std::array<int, kFt8Nn>& itone,
    std::array<signed char, kFt8Bits>& message77,
    float const* llra, float const* llrb, float const* llrc, float const* llrd)
{
  if (!s8 || !llra || !llrb || !llrc || !llrd
      || jseq < 0 || jseq >= kFt8SequenceCount
      || request.nzhsym < 50
      || request.ndepth < 3
      || request.supplemental
      || request.lft8apon == 0
      || request.ncontest != 0
      || nsync < 10
      || stage4_remaining_ms () < 3500
      || !trim_fixed (request.hiscall).empty ())
    {
      return false;
    }

  std::vector<Ft8TerminalPairHint> hints;
  Ft8Stage4State& state = stage4_state ();
  append_reverse_terminal_hints_from_slot (
      hints, state, state.a7[static_cast<size_t> (1 - jseq)]);
  if (hints.empty ())
    {
      return false;
    }

  FixedChars<kFt8DecodedChars> best_message = blank_fixed<kFt8DecodedChars> ();
  int best_hard = 174;
  float best_dmin = 1.0e30f;
  int second_hard = 174;
  float second_dmin = 1.0e30f;
  FixedChars<4> blank_grid = blank_fixed<4> ();

  auto remember_candidate = [&] (FixedChars<kFt8DecodedChars> const& candidate_message,
                                 int hard, float metric) {
    if (hard < best_hard || (hard == best_hard && metric < best_dmin))
      {
        second_hard = best_hard;
        second_dmin = best_dmin;
        best_hard = hard;
        best_dmin = metric;
        best_message = candidate_message;
        return;
      }
    if (hard < second_hard || (hard == second_hard && metric < second_dmin))
      {
        second_hard = hard;
        second_dmin = metric;
      }
  };

  for (Ft8TerminalPairHint const& hint : hints)
    {
      for (int const imsg : {3})
        {
          FixedChars<kFt8DecodedChars> candidate_message =
              blank_fixed<kFt8DecodedChars> ();
          if (ftx_prepare_ft8_a7_candidate_c (imsg, hint.call_1.data (),
                                              hint.call_2.data (),
                                              blank_grid.data (),
                                              candidate_message.data ()) == 0
              || !is_strict_standard_ft8_message (candidate_message))
            {
              continue;
            }

          FixedChars<kFt8DecodedChars> msgsent = blank_fixed<kFt8DecodedChars> ();
          std::array<int, kFt8Nn> expected_tones {};
          std::array<signed char, 174> expected_codeword {};
          if (ftx_encode_ft8_candidate_c (candidate_message.data (), msgsent.data (),
                                          expected_tones.data (),
                                          expected_codeword.data ()) == 0)
            {
              continue;
            }

          float power = 0.0f;
          float metric = 1.0e30f;
          int hard = 174;
          ftx_ft8a7_measure_candidate_c (s8, 8, kFt8Nn,
                                         expected_tones.data (),
                                         expected_codeword.data (),
                                         llra, llrb, llrc, llrd,
                                         &power, &metric, &hard);
          if (power <= 0.0f)
            {
              continue;
            }
          remember_candidate (candidate_message, hard, metric);
        }
    }

  if (trim_fixed (best_message).empty ()
      || best_hard > 35
      || best_dmin > 70.0f
      || (second_hard < 174
          && best_hard + 4 > second_hard
          && best_dmin + 16.0f > second_dmin))
    {
      return false;
    }

  return try_ft8sd_known_message (best_message, request, s8, nsync, xbase,
                                  msg37, xsnr, itone, message77,
                                  llra, llrb, llrc, llrd,
                                  35, 70.0f, true);
}

bool try_ft8_a7_report_variant_hint (
    Ft8Request const& request, FixedChars<kFt8DecodedChars> const& hint_message,
    FixedChars<12> const& call_1, FixedChars<12> const& call_2,
    FixedChars<4> const& grid4, float const* s8, int nsync, float xbase,
    FixedChars<kFt8DecodedChars>& msg37, float& xsnr,
    std::array<int, kFt8Nn>& itone,
    std::array<signed char, kFt8Bits>& message77,
    float const* llra, float const* llrb, float const* llrc, float const* llrd)
{
  if (!s8 || !llra || !llrb || !llrc || !llrd)
    {
      return false;
    }
  if (nsync < 18)
    {
      return false;
    }

  std::vector<std::string> const hint_words = split_words (trim_fixed (hint_message));
  if (hint_words.size () != 3
      || hint_words[0] == "CQ"
      || hint_words[0].find ('<') != std::string::npos
      || hint_words[1].find ('<') != std::string::npos
      || !is_standard_call_word (hint_words[0])
      || !is_standard_call_word (hint_words[1])
      || !is_report_token (hint_words[2])
      || hint_words[2] == "73"
      || hint_words[2] == "RRR"
      || hint_words[2] == "RR73")
    {
      return false;
    }

  std::array<float, kFt8A7MaxMsg> dmm;
  dmm.fill (1.0e30f);
  FixedChars<kFt8DecodedChars> best_message = blank_fixed<kFt8DecodedChars> ();
  std::array<int, kFt8Nn> best_tones {};
  int best_hard = 174;
  float best_power = 0.0f;
  float best_dmin = 1.0e30f;

  for (int imsg = 7; imsg <= kFt8A7MaxMsg; ++imsg)
    {
      if (stage4_should_cancel ())
        {
          return false;
        }

      FixedChars<kFt8DecodedChars> candidate_message =
          blank_fixed<kFt8DecodedChars> ();
      if (ftx_prepare_ft8_a7_candidate_c (imsg, call_1.data (),
                                          call_2.data (), grid4.data (),
                                          candidate_message.data ()) == 0
          || !is_strict_standard_ft8_message (candidate_message)
          || messages_equal (candidate_message, hint_message))
        {
          continue;
        }

      std::vector<std::string> const candidate_words =
          split_words (trim_fixed (candidate_message));
      if (candidate_words.size () != 3
          || !is_report_token (candidate_words[2])
          || candidate_words[2] == "73"
          || candidate_words[2] == "RRR"
          || candidate_words[2] == "RR73")
        {
          continue;
        }

      FixedChars<kFt8DecodedChars> msgsent = blank_fixed<kFt8DecodedChars> ();
      std::array<int, kFt8Nn> candidate_tones {};
      std::array<signed char, 174> candidate_codeword {};
      if (ftx_encode_ft8_candidate_c (candidate_message.data (), msgsent.data (),
                                      candidate_tones.data (),
                                      candidate_codeword.data ()) == 0)
        {
          continue;
        }

      float power = 0.0f;
      float metric = 1.0e30f;
      int hard = 174;
      ftx_ft8a7_measure_candidate_c (s8, 8, kFt8Nn,
                                     candidate_tones.data (),
                                     candidate_codeword.data (),
                                     llra, llrb, llrc, llrd,
                                     &power, &metric, &hard);
      dmm[static_cast<size_t> (imsg - 1)] = metric;
      if (power <= 0.0f)
        {
          continue;
        }
      if (metric < best_dmin)
        {
          best_dmin = metric;
          best_power = power;
          best_hard = hard;
          best_message = msgsent;
          best_tones = candidate_tones;
        }
    }

  if (trim_fixed (best_message).empty ())
    {
      return false;
    }

  float accepted_dmin = 1.0e30f;
  float second_dmin = 1.0e30f;
  float candidate_snr = -25.0f;
  int const accept =
      ftx_ft8_a7_finalize_metrics_c (dmm.data (), kFt8A7MaxMsg, best_power,
                                     xbase, &accepted_dmin, &second_dmin,
                                     &candidate_snr);
  bool const strong_metric_fallback =
      best_hard <= 44
      && accepted_dmin <= 110.0f;
  if (best_hard > 44 || (accept == 0 && !strong_metric_fallback))
    {
      if (debug_known_cq_replay ())
        {
          std::cerr << "[KNOWNCQ] reject report variant hint="
                    << trim_fixed (hint_message)
                    << " best=" << trim_fixed (best_message)
                    << " nsync=" << nsync
                    << " nhard=" << best_hard
                    << " dmin=" << accepted_dmin
                    << " dmin2=" << second_dmin
                    << " accept=" << accept << '\n';
        }
      return false;
    }

  float snr_estimate = candidate_snr;
  ftx_ft8_compute_snr_c (s8, 8, kFt8Nn, best_tones.data (), xbase,
                         request.nagain, nsync, &snr_estimate);
  if (debug_known_cq_replay ())
    {
      std::cerr << "[KNOWNCQ] accept report variant hint="
                << trim_fixed (hint_message)
                << " best=" << trim_fixed (best_message)
                << " nsync=" << nsync
                << " nhard=" << best_hard
                << " dmin=" << accepted_dmin
                << " dmin2=" << second_dmin
                << " snr=" << snr_estimate << '\n';
    }
  return try_ft8sd_known_message (best_message, request, s8, nsync, xbase,
                                  msg37, xsnr, itone, message77,
                                  llra, llrb, llrc, llrd,
                                  45, 125.0f, true);
}

bool try_ft8_a7_fast_repeated_hint (Ft8Stage4State& state,
                                    Ft8Request const& request,
                                    Ft8A7Entry const& hint,
                                    int& newdat_a7,
                                    std::array<float, kFt8Nh1> const& sbase,
                                    FixedChars<kFt8DecodedChars>& msg37,
                                    float& xsnr, float& xdt, float& f1,
                                    std::array<int, kFt8Nn>& itone,
                                    std::array<signed char, kFt8Bits>& message77,
                                    bool allow_single_hit,
                                    bool allow_relaxed_single_hit_exact = false)
{
  if (request.nzhsym < 50
      || request.ndepth < 3
      || request.lft8apon == 0
      || request.ncontest != 0
      || !trim_fixed (request.hiscall).empty ()
      || (hint.hits < 2 && !allow_single_hit)
      || !ft8_hint_matches_context (hint, request))
    {
      return false;
    }

  FixedChars<kFt8DecodedChars> const hint_message = ft8_a7_replay_message (hint);
  bool const directed_replay = !ft8sd_hint_is_cq (hint_message);
  if (!is_strict_standard_ft8_message (hint_message)
      || is_directed_pair_only_message (hint_message))
    {
      return false;
    }

  float xbase = 0.0f;
  FixedChars<12> call_1 = blank_fixed<12> ();
  FixedChars<12> call_2 = blank_fixed<12> ();
  FixedChars<4> grid4 = blank_fixed<4> ();
  int const request_status =
      ftx_ft8_prepare_a7_request_c (hint.freq, hint.dt, hint.message.data (),
                                    sbase.data (), kFt8Nh1, &f1, &xdt, &xbase,
                                    call_1.data (), call_2.data (), grid4.data ());
  if (request_status != 1)
    {
      return false;
    }

  std::array<std::complex<float>, kFt8A7DownsampleSize> cd0 {};
  ftx_ft8_downsample_c (state.dd.data (), &newdat_a7, f1,
                        reinterpret_cast<fftwf_complex*> (cd0.data ()));
  if (stage4_should_cancel ())
    {
      return false;
    }

  int ibest = 0;
  float delfbest = 0.0f;
  ftx_ft8_a7_search_initial_c (cd0.data (), kFt8A7Np2, kFt8A7Fs2,
                               xdt, &ibest, &delfbest);
  if (std::fabs (delfbest) > 0.0f)
    {
      std::array<float, 5> tweak {};
      tweak[0] = -delfbest;
      int const npts = kFt8A7Np2;
      float const fsample = kFt8A7Fs2;
      std::array<std::complex<float>, kFt8A7DownsampleSize> adjusted {};
      ftx_twkfreq1_c (cd0.data (), &npts, &fsample, tweak.data (), adjusted.data ());
      cd0 = adjusted;
    }
  f1 += delfbest;

  int second_pass_newdat = 0;
  ftx_ft8_downsample_c (state.dd.data (), &second_pass_newdat, f1,
                        reinterpret_cast<fftwf_complex*> (cd0.data ()));
  if (stage4_should_cancel ())
    {
      return false;
    }

  float sync = 0.0f;
  ftx_ft8_a7_refine_search_c (cd0.data (), kFt8A7Np2, kFt8A7Fs2,
                              ibest, &ibest, &sync, &xdt);

  std::array<float, 8 * kFt8Nn> s8 {};
  std::array<float, 174> llra {};
  std::array<float, 174> llrb {};
  std::array<float, 174> llrc {};
  std::array<float, 174> llrd {};
  std::array<float, 174> llre {};
  int nsync = 0;
  ftx_ft8_bitmetrics_scaled_c (cd0.data (), kFt8A7Np2, ibest, 1,
                               kFt8BitMetricScale, s8.data (), &nsync,
                               llra.data (), llrb.data (), llrc.data (),
                               llrd.data (), llre.data ());
  if (debug_ft8_focus_replay ())
    {
      std::cerr << "[FT8A7FAST] utc=" << request.nutc
                << " hint=" << trim_fixed (hint_message)
                << " directed=" << (directed_replay ? 1 : 0)
                << " nsync=" << nsync
                << " sync=" << sync
                << " f1=" << f1
                << " xdt=" << xdt
                << " hits=" << hint.hits
                << '\n';
    }

  std::vector<std::string> const hint_words = split_words (trim_fixed (hint_message));
  bool const exact_report_tail =
      hint_words.size () == 3
      && hint_words[0].find ('<') == std::string::npos
      && hint_words[1].find ('<') == std::string::npos
      && is_report_token (hint_words[2])
      && hint_words[2] != "73"
      && hint_words[2] != "RRR"
      && hint_words[2] != "RR73";
  bool const exact_recent_report_tail =
      directed_replay
      && allow_single_hit
      && hint.hits >= 2
      && hint.age <= 1
      && exact_report_tail;
  int const directed_nsync_limit = exact_recent_report_tail ? 6 : 7;
  if (directed_replay && nsync < directed_nsync_limit)
    {
      return false;
    }
  bool const relaxed_repeated_report =
      exact_recent_report_tail
      && nsync >= directed_nsync_limit
      && exact_report_tail;
  int const hard_limit = directed_replay
      ? (relaxed_repeated_report ? 86 : 58)
      : -1;
  float const dmin_limit = directed_replay
      ? (relaxed_repeated_report ? 185.0f : 170.0f)
      : -1.0f;
  bool const single_hit_direct =
      directed_replay && hint.hits < 2 && allow_single_hit;
  if (!single_hit_direct
      && try_ft8sd_known_message (hint_message, request, s8.data (), nsync, xbase,
                                  msg37, xsnr, itone, message77,
                                  llra.data (), llrb.data (), llrc.data (),
                                  llrd.data (), hard_limit, dmin_limit, true))
    {
      return true;
    }

  if (single_hit_direct && allow_relaxed_single_hit_exact)
    {
      bool const report_tail =
          exact_report_tail;
      if (report_tail
          && try_ft8sd_known_message (hint_message, request, s8.data (), nsync,
                                      xbase, msg37, xsnr, itone, message77,
                                      llra.data (), llrb.data (), llrc.data (),
                                      llrd.data (), hard_limit, dmin_limit, true))
        {
          return true;
        }
    }

  if (!directed_replay)
    {
      return false;
    }

  if (try_ft8_a7_report_variant_hint (request, hint_message, call_1, call_2,
                                      grid4, s8.data (), nsync, xbase,
                                      msg37, xsnr, itone, message77,
                                      llra.data (), llrb.data (),
                                      llrc.data (), llrd.data ()))
    {
      return true;
    }

  for (int const terminal_imsg : {4, 3, 2})
    {
      FixedChars<kFt8DecodedChars> terminal_message =
          blank_fixed<kFt8DecodedChars> ();
      if (ftx_prepare_ft8_a7_candidate_c (terminal_imsg, call_1.data (),
                                          call_2.data (), grid4.data (),
                                          terminal_message.data ()) == 0)
        {
          continue;
        }
      if (!is_strict_standard_ft8_message (terminal_message)
          || messages_equal (terminal_message, hint_message))
        {
          continue;
        }
      int const terminal_hard_limit = directed_replay ? 58 : hard_limit;
      float const terminal_dmin_limit = directed_replay ? 170.0f : dmin_limit;
      if (try_ft8sd_known_message (terminal_message, request, s8.data (), nsync,
                                   xbase, msg37, xsnr, itone, message77,
                                   llra.data (), llrb.data (), llrc.data (),
                                   llrd.data (), terminal_hard_limit,
                                   terminal_dmin_limit, true))
        {
          return true;
        }
    }
  return false;
}

Ft8CallGridEntry const* find_call_grid_cq_history (Ft8CallGridHistoryState const& state,
                                                   Ft8Request const& request,
                                                   int jseq, float freq,
                                                   float callback_dt,
                                                   float sync, int nsync,
                                                   float cq_score);

FixedChars<kFt8DecodedChars> call_grid_cq_message (Ft8CallGridEntry const& entry);

bool try_call_grid_cq_replay (Ft8CallGridHistoryState const* history,
                              Ft8Request const& request, float const* s8,
                              int nsync, float sync, float f1, float xdt,
                              float xbase, float cq_score,
                              FixedChars<kFt8DecodedChars>& msg37,
                              float& xsnr, std::array<int, kFt8Nn>& itone,
                              std::array<signed char, kFt8Bits>& message77,
                              float const* llra, float const* llrb,
                              float const* llrc, float const* llrd,
                              int jseq)
{
  if (!history || !s8)
    {
      return false;
    }

  Ft8CallGridEntry const* entry =
      find_call_grid_cq_history (*history, request, jseq, f1, xdt - 0.5f,
                                 sync, nsync, cq_score);
  if (!entry)
    {
      return false;
    }

  FixedChars<kFt8DecodedChars> const cq_message = call_grid_cq_message (*entry);
  return try_ft8sd_known_message (cq_message, request, s8, nsync, xbase,
                                  msg37, xsnr, itone, message77,
                                  llra, llrb, llrc, llrd,
                                  nsync >= 5 ? 78 : 82,
                                  nsync >= 5 ? 180.0f : 200.0f);
}

int ft8_candidate_sync_threshold (int imetric, Ft8Request const& request)
{
  int const ndepth = request.ndepth;
  int syncmin = 6;
  if (imetric >= 2)
    {
      syncmin = 7;
    }
  if (ndepth >= 4)
    {
      syncmin = std::min (syncmin, 5);
    }
  else if (ndepth >= 3)
    {
      syncmin = std::min (syncmin, 6);
    }
  if (ndepth <= 2)
    {
      syncmin = 8;
    }
  if (ndepth == 1)
    {
      syncmin = 7;
    }
  if (ndepth >= 4 && request.supplemental)
    {
      syncmin = std::min (syncmin, 4);
    }
  if (request.lft8lowth)
    {
      syncmin = std::min (syncmin, request.lft8subpass ? 4 : 5);
    }
  // La soglia di aggancio NON si abbassa. Provato il 28/08/2026 e ritirato:
  // il decoder vettorizzato lascia il tempo per accettare candidati piu'
  // deboli, ma quei candidati non contengono nulla da decodificare.
  //
  // Su tre finestre di sette minuti di traffico reale la soglia 3 sembrava
  // valere +10% di decodifiche e ottanta nominativi nuovi, tutti autentici e
  // ripetuti fra i cicli. Era un'illusione da finestre diverse: rimisurato
  // sugli STESSI 19 slot registrati dall'aria, soglia 6, 3 e 1 danno lo
  // stesso identico esito -- 275 messaggi distinti e 486 decodifiche -- con
  // il tempo che sale del 15% scendendo. La banda cambia abbastanza fra due
  // finestre da simulare un guadagno del 10% che non esiste.
  //
  // Chi vuole riprovare ha DECODIUM_FT8_DECODE_SYNCMIN qui sotto, ma la
  // misura va fatta appaiata su registrazioni, non su finestre consecutive.
  {
    int const ov = ft8_decode_syncmin_override ();
    if (ov >= 0) syncmin = std::min (syncmin, ov);
  }
  return syncmin;
}

int ft8_candidate_budget (Ft8Request const& request)
{
  if (request.ndepth >= 4 && request.supplemental)
    {
      return kFt8MaxCand;
    }
  if (request.ndepth >= 4)
    {
      return 3200;
    }
  if (request.ndepth >= 3)
    {
      return kFt8DeepMaxCand;
    }
  return kFt8DefaultMaxCand;
}

int ft8_main_candidate_budget (Ft8Request const& request, int ifa, int ifb, int ipass)
{
  int const budget = ft8_candidate_budget (request);
  bool const advanced_profile =
      request.lft8lowth || request.lft8subpass || request.nft8cycles > 1
      || request.nft8rxfsens > 1;
  if (request.ndepth < 4)
    {
      int const span = std::max (0, ifb - ifa);
      if (advanced_profile && span >= 3500)
        {
          int cap = 2400;
          if (ipass >= 3)
            {
              cap = 1800;
            }
          if (ipass >= 4)
            {
              cap = 1300;
            }
          if (request.lft8subpass && ipass >= 6)
            {
              cap = 950;
            }
          return std::min (budget, cap);
        }
      return budget;
    }

  int const span = std::max (0, ifb - ifa);
  if (span >= 3500)
    {
      int cap = request.supplemental ? 1200 : 1050;
      if (ipass >= 3)
        {
          cap = std::min (cap, request.supplemental ? 950 : 850);
        }
      if (ipass >= 4)
        {
          cap = std::min (cap, request.supplemental ? 800 : 700);
        }
      if (request.lft8lowth || request.lft8subpass || request.nft8rxfsens >= 2)
        {
          cap += 200;
        }
      return std::min (budget, cap);
    }
  if (span >= 2500)
    {
      return std::min (budget, request.supplemental ? 2600 : 2200);
    }
  return budget;
}

int select_ft8_focus_frequency (Ft8Stage4State const& state, Ft8Request const& request,
                                int ifa, int ifb, int ndecodes,
                                std::array<int, 8> const& blocked,
                                int blocked_count, bool overlap_only)
{
  if (request.nfqso != 0 || request.nzhsym < 50 || request.ndepth < 3)
    {
      return 0;
    }

  std::vector<float> candidate (static_cast<size_t> (4 * kFt8MaxCand), 0.0f);
  std::array<float, kFt8Nh1> sbase {};
  float syncmin = 0.0f;
  int imetric = 0;
  int lsubtract = 0;
  int run_pass = 0;
  ftx_ft8_prepare_pass_c (3, 2, ndecodes, &syncmin, &imetric, &lsubtract, &run_pass);
  (void) imetric;
  (void) lsubtract;
  if (run_pass == 0)
    {
      return 0;
    }

  int ncand = 0;
  ftx_sync8_search_stage4_c (state.dd.data (), kFt8NMax,
                             static_cast<float> (ifa), static_cast<float> (ifb),
                             syncmin, 0.0f, ft8_candidate_budget (request), 2,
                             request.ncandthin, candidate.data (), &ncand, sbase.data ());

  auto already_decoded_near = [&] (float freq) {
    int const limit = std::min (ndecodes, kFt8MaxEarly);
    for (int index = 0; index < limit; ++index)
      {
        if (std::fabs (freq - state.f1_save[static_cast<size_t> (index)]) <= 3.0f)
          {
            return true;
          }
      }
    return false;
  };
  auto decoded_neighbor = [&] (float freq, float min_delta, float max_delta, bool cq_only) {
    int const limit = std::min (ndecodes, kFt8MaxEarly);
    for (int index = 0; index < limit; ++index)
      {
        if (cq_only && !ft8sd_hint_is_cq (state.allmessages[static_cast<size_t> (index)]))
          {
            continue;
          }
        float const delta = std::fabs (freq - state.f1_save[static_cast<size_t> (index)]);
        if (delta >= min_delta && delta <= max_delta)
          {
            return true;
          }
      }
    return false;
  };
  auto blocked_near = [&] (float freq) {
    int const limit = std::min (blocked_count, static_cast<int> (blocked.size ()));
    for (int index = 0; index < limit; ++index)
      {
        if (std::abs (static_cast<int> (std::lround (freq)) - blocked[static_cast<size_t> (index)]) <= 20)
          {
            return true;
          }
      }
    return false;
  };

  for (int icand = 0; icand < ncand; ++icand)
    {
      float const freq = candidate[static_cast<size_t> (icand * 4)];
      if (freq < static_cast<float> (ifa) || freq > static_cast<float> (ifb))
        {
          continue;
        }
      if (freq < 100.0f || already_decoded_near (freq) || blocked_near (freq))
        {
          continue;
        }
      if (overlap_only && !decoded_neighbor (freq, 3.0f, 16.0f, true))
        {
          continue;
        }
      return std::max (0, static_cast<int> (std::lround (freq)));
    }

  return 0;
}

int select_ft8_close_cq_focus_frequency (Ft8Stage4State const& audio_state,
                                         Ft8Stage4State const& decode_state,
                                         Ft8Request const& request,
                                         int ifa, int ifb, int ndecodes,
                                         std::array<int, 8> const& blocked,
                                         int blocked_count)
{
  if (request.nzhsym < 50 || request.ndepth < 3 || ndecodes < 2)
    {
      return 0;
    }

  constexpr int kMaxCloseCqRefs = 16;
  std::array<float, kMaxCloseCqRefs> cq_freqs {};
  std::array<float, kMaxCloseCqRefs> cq_dts {};
  int cq_count = 0;
  int const limit = std::min (ndecodes, kFt8MaxEarly);
  for (int index = 0; index < limit && cq_count < kMaxCloseCqRefs; ++index)
    {
      if (!ft8sd_hint_is_cq (decode_state.allmessages[static_cast<size_t> (index)]))
        {
          continue;
        }
      float const freq = decode_state.f1_save[static_cast<size_t> (index)];
      if (freq < static_cast<float> (ifa) || freq > static_cast<float> (ifb))
        {
          continue;
        }
      cq_freqs[static_cast<size_t> (cq_count)] = freq;
      cq_dts[static_cast<size_t> (cq_count)] =
          decode_state.xdt_save[static_cast<size_t> (index)] - 0.5f;
      ++cq_count;
    }
  if (cq_count == 0)
    {
      return 0;
    }

  auto already_decoded_near = [&] (float freq, float tolerance) {
    for (int index = 0; index < limit; ++index)
      {
        if (std::fabs (freq - decode_state.f1_save[static_cast<size_t> (index)]) <= tolerance)
          {
            return true;
          }
      }
    return false;
  };
  auto blocked_near = [&] (float freq) {
    int const limit = std::min (blocked_count, static_cast<int> (blocked.size ()));
    for (int index = 0; index < limit; ++index)
      {
        if (std::abs (static_cast<int> (std::lround (freq)) - blocked[static_cast<size_t> (index)]) <= 10)
          {
            return true;
          }
      }
    return false;
  };

  std::vector<float> candidate (static_cast<size_t> (4 * kFt8MaxCand), 0.0f);
  std::array<float, kFt8Nh1> sbase {};
  float syncmin = 0.0f;
  int imetric = 0;
  int lsubtract = 0;
  int run_pass = 0;
  ftx_ft8_prepare_pass_c (3, 1, ndecodes, &syncmin, &imetric, &lsubtract, &run_pass);
  (void) imetric;
  (void) lsubtract;
  if (run_pass == 0)
    {
      return 0;
    }

  int best_freq = 0;
  float best_score = 1.0e30f;
  int const window_budget = std::min (ft8_candidate_budget (request), 64);
  for (int ref = 0; ref < cq_count; ++ref)
    {
      float const base_freq = cq_freqs[static_cast<size_t> (ref)];
      int const window_ifa =
          std::max (ifa, static_cast<int> (std::floor (base_freq - 9.0f)));
      int const window_ifb =
          std::min (ifb, static_cast<int> (std::ceil (base_freq + 9.0f)));
      if (window_ifb <= window_ifa)
        {
          continue;
        }

      int ncand = 0;
      ftx_sync8_search_stage4_c (audio_state.dd.data (), kFt8NMax,
                                 static_cast<float> (window_ifa),
                                 static_cast<float> (window_ifb), syncmin, 0.0f,
                                 window_budget, 1, request.ncandthin,
                                 candidate.data (), &ncand, sbase.data ());

      for (int icand = 0; icand < ncand; ++icand)
        {
          float const freq = candidate[static_cast<size_t> (icand * 4)];
          float const dt = candidate[static_cast<size_t> (icand * 4 + 1)];
          float const sync = candidate[static_cast<size_t> (icand * 4 + 2)];
          float const cq_flag = candidate[static_cast<size_t> (icand * 4 + 3)];
          if (freq < static_cast<float> (window_ifa)
              || freq > static_cast<float> (window_ifb)
              || cq_flag < 1.5f
              || already_decoded_near (freq, 3.0f)
              || blocked_near (freq))
            {
              continue;
            }

          float const delta = std::fabs (freq - base_freq);
          if (delta < 3.5f || delta > 8.0f)
            {
              continue;
            }
          float const decoded_dt = cq_dts[static_cast<size_t> (ref)];
          if (std::fabs (dt - decoded_dt) < 0.75f)
            {
              continue;
            }
          float const score = std::fabs (delta - 5.0f)
                              - 0.20f * std::min (std::fabs (dt - decoded_dt), 2.5f)
                              - 0.10f * sync
                              + 0.02f * static_cast<float> (ref)
                              + 0.01f * static_cast<float> (icand);
          if (score < best_score)
            {
              best_score = score;
              best_freq = std::max (0, static_cast<int> (std::lround (freq)));
            }
        }
    }

  return best_score <= 0.35f ? best_freq : 0;
}

int ft8_utc_delta_seconds (int newer, int older);

int select_call_grid_focus_frequency (Ft8CallGridHistoryState const& history,
                                      Ft8Request const& request, int jseq,
                                      int ifa, int ifb, int ndecodes,
                                      Ft8Stage4State const& decode_state,
                                      std::array<int, 8> const& blocked,
                                      int blocked_count)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount || request.nzhsym < 50
      || request.ndepth < 3 || request.lft8apon == 0
      || !trim_fixed (request.hiscall).empty ())
    {
      return 0;
    }

  auto already_decoded_near = [&] (float freq) {
    int const limit = std::min (ndecodes, kFt8MaxEarly);
    for (int index = 0; index < limit; ++index)
      {
        if (std::fabs (freq - decode_state.f1_save[static_cast<size_t> (index)]) <= 3.0f)
          {
            return true;
          }
      }
    return false;
  };
  auto blocked_near = [&] (float freq) {
    int const limit = std::min (blocked_count, static_cast<int> (blocked.size ()));
    for (int index = 0; index < limit; ++index)
      {
        if (std::abs (static_cast<int> (std::lround (freq)) - blocked[static_cast<size_t> (index)]) <= 10)
          {
            return true;
          }
      }
    return false;
  };

  int best_freq = 0;
  float best_score = 1.0e30f;
  auto const& entries = history.entries[static_cast<size_t> (jseq)];
  for (Ft8CallGridEntry const& entry : entries)
    {
      if (!entry.valid || entry.nutc == request.nutc || entry.hits < 2)
        {
          continue;
        }
      int const age_seconds = ft8_utc_delta_seconds (request.nutc, entry.nutc);
      if (age_seconds <= 0 || age_seconds > kFt8CallGridMaxAge * 30 + 15)
        {
          continue;
        }
      if (entry.freq < static_cast<float> (ifa) || entry.freq > static_cast<float> (ifb)
          || already_decoded_near (entry.freq) || blocked_near (entry.freq))
        {
          continue;
        }

      float const score = static_cast<float> (age_seconds)
                          - 40.0f * static_cast<float> (std::min (entry.hits, 5))
                          + 0.002f * std::fabs (entry.freq - static_cast<float> (request.nfqso));
      if (score < best_score)
        {
          best_score = score;
          best_freq = std::max (0, static_cast<int> (std::lround (entry.freq)));
        }
    }
  return best_freq;
}

float ft8_cq_signature_score (float const* s8, int rows)
{
  if (!s8 || rows < 8)
    {
      return 0.0f;
    }

  auto strongest_tone = [s8, rows] (int symbol) {
    int best = 0;
    float best_value = s8[rows * symbol];
    for (int tone = 1; tone < 8; ++tone)
      {
        float const value = s8[tone + rows * symbol];
        if (value > best_value)
          {
            best_value = value;
            best = tone;
          }
      }
    return best;
  };

  float score = 0.0f;
  for (int symbol = 7; symbol <= 15; ++symbol)
    {
      int const tone = strongest_tone (symbol);
      if ((symbol < 15 && tone == 0) || (symbol == 15 && tone == 1))
        {
          score += 1.0f;
        }
    }
  int const tone17 = strongest_tone (16);
  int const tone27 = strongest_tone (26);
  int const tone33 = strongest_tone (32);
  if (tone17 == 0 || tone17 == 1) score += 0.5f;
  if (tone27 == 0 || tone27 == 1) score += 0.5f;
  if (tone33 == 2 || tone33 == 3) score += 0.5f;
  return score;
}

int ft8_utc_seconds (int nutc)
{
  int const sec = std::abs (nutc) % 100;
  int const min = (std::abs (nutc) / 100) % 100;
  int const hour = (std::abs (nutc) / 10000) % 100;
  return hour * 3600 + min * 60 + sec;
}

int ft8_utc_delta_seconds (int newer, int older)
{
  int delta = ft8_utc_seconds (newer) - ft8_utc_seconds (older);
  if (delta < 0)
    {
      delta += 24 * 3600;
    }
  return delta;
}

bool extract_ft8_call_grid (FixedChars<kFt8DecodedChars> const& decoded,
                            std::string& call, std::string& grid)
{
  call.clear ();
  grid.clear ();

  std::vector<std::string> const words = split_words (trim_fixed (decoded));
  if (words.size () < 3)
    {
      return false;
    }

  if (words[0] == "CQ")
    {
      size_t call_index = 1;
      if (words.size () >= 4 && is_cq_modifier (words[1]))
        {
          call_index = 2;
        }
      if (call_index + 1 >= words.size ()
          || !is_standard_call_word (words[call_index])
          || !is_grid4 (words[call_index + 1]))
        {
          return false;
        }
      call = words[call_index];
      grid = words[call_index + 1];
      return true;
    }

  if (words.size () == 3
      && is_standard_call_word (words[0])
      && is_standard_call_word (words[1])
      && is_grid4 (words[2]))
    {
      call = words[1];
      grid = words[2];
      return true;
    }

  return false;
}

void save_call_grid_history (Ft8CallGridHistoryState& state, Ft8Request const& request,
                             int jseq, float callback_dt, float freq,
                             FixedChars<kFt8DecodedChars> const& decoded)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount || request.ndepth < 3)
    {
      return;
    }

  std::string call;
  std::string grid;
  if (!extract_ft8_call_grid (decoded, call, grid))
    {
      return;
    }
  if (call == trim_fixed (request.mycall))
    {
      return;
    }
  if (!ft8sd_hint_is_cq (decoded))
    {
      return;
    }
  save_known_call_grid (known_call_grid_history (), call, grid, freq, callback_dt,
                        request.nutc);

  auto& entries = state.entries[static_cast<size_t> (jseq)];
  int replace_index = -1;
  float replace_score = -1.0e30f;
  for (int i = 0; i < static_cast<int> (entries.size ()); ++i)
    {
      Ft8CallGridEntry& entry = entries[static_cast<size_t> (i)];
      if (!entry.valid)
        {
          replace_index = i;
          break;
        }
      if (trim_fixed (entry.call) == call
          && trim_fixed (entry.grid) == grid
          && std::fabs (entry.freq - freq) <= 10.0f)
        {
          replace_index = i;
          break;
        }

      int const age_seconds = ft8_utc_delta_seconds (request.nutc, entry.nutc);
      float const score = static_cast<float> (age_seconds)
                          - 25.0f * static_cast<float> (std::min (entry.hits, 5));
      if (score > replace_score)
        {
          replace_score = score;
          replace_index = i;
        }
    }

  if (replace_index < 0)
    {
      return;
    }

  Ft8CallGridEntry& entry = entries[static_cast<size_t> (replace_index)];
  bool const same_entry =
      entry.valid
      && trim_fixed (entry.call) == call
      && trim_fixed (entry.grid) == grid
      && std::fabs (entry.freq - freq) <= 10.0f;
  entry.valid = true;
  entry.nutc = request.nutc;
  entry.dt = callback_dt;
  entry.freq = freq;
  entry.hits = same_entry ? std::min (entry.hits + 1, 99) : 1;
  entry.call = fixed_from_string<12> (call);
  entry.grid = fixed_from_string<4> (grid);
}

void save_known_call_grid (Ft8KnownCallGridState& state, std::string const& call,
                           std::string const& grid, float freq, float dt, int nutc)
{
  if (!is_standard_call_word (call) || !is_grid4 (grid) || grid == "RR73")
    {
      return;
    }

  int replace_index = -1;
  float replace_score = -1.0e30f;
  for (int i = 0; i < static_cast<int> (state.entries.size ()); ++i)
    {
      Ft8KnownCallGridEntry& entry = state.entries[static_cast<size_t> (i)];
      if (!entry.valid)
        {
          replace_index = i;
          break;
        }
      if (trim_fixed (entry.call) == call
          && trim_fixed (entry.grid) == grid)
        {
          replace_index = i;
          break;
        }

      int const age_seconds = nutc > 0 ? ft8_utc_delta_seconds (nutc, entry.nutc) : 0;
      float const score = static_cast<float> (age_seconds)
                          - 20.0f * static_cast<float> (std::min (entry.hits, 20));
      if (score > replace_score)
        {
          replace_score = score;
          replace_index = i;
        }
    }

  if (replace_index < 0)
    {
      return;
    }

  Ft8KnownCallGridEntry& entry = state.entries[static_cast<size_t> (replace_index)];
  bool const same_entry =
      entry.valid
      && trim_fixed (entry.call) == call
      && trim_fixed (entry.grid) == grid;
  entry.valid = true;
  entry.nutc = nutc;
  if (freq > 0.0f)
    {
      entry.freq = freq;
    }
  entry.dt = dt;
  entry.hits = same_entry ? std::min (entry.hits + 1, 999) : 1;
  entry.call = fixed_from_string<12> (call);
  entry.grid = fixed_from_string<4> (grid);
}

bool is_packable_cq_call_message (std::string const& call)
{
  if (call.empty () || call.size () > 12)
    {
      return false;
    }
  for (char const ch : call)
    {
      if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '/'))
        {
          return false;
        }
    }

  FixedChars<kFt8DecodedChars> const message =
      fixed_from_string<kFt8DecodedChars> ("CQ " + call);
  FixedChars<kFt8DecodedChars> msgsent = blank_fixed<kFt8DecodedChars> ();
  std::array<int, kFt8Nn> tones {};
  std::array<signed char, 174> codeword {};
  if (ftx_encode_ft8_candidate_c (message.data (), msgsent.data (),
                                  tones.data (), codeword.data ()) == 0)
    {
      return false;
    }
  return trim_fixed (msgsent) == "CQ " + call;
}

bool is_cq_call_only_message (FixedChars<kFt8DecodedChars> const& decoded)
{
  std::vector<std::string> const words = split_words (trim_fixed (decoded));
  return words.size () == 2
      && words[0] == "CQ"
      && is_packable_cq_call_message (words[1]);
}

void save_known_cq_call (Ft8KnownCqCallState& state, std::string const& call,
                         float freq, float dt, int nutc)
{
  if (!is_packable_cq_call_message (call) || freq <= 0.0f || nutc <= 0)
    {
      return;
    }

  int replace_index = -1;
  float replace_score = -1.0e30f;
  for (int i = 0; i < static_cast<int> (state.entries.size ()); ++i)
    {
      Ft8KnownCqCallEntry& entry = state.entries[static_cast<size_t> (i)];
      if (!entry.valid)
        {
          replace_index = i;
          break;
        }
      if (trim_fixed (entry.call) == call)
        {
          replace_index = i;
          break;
        }

      int const age_seconds = nutc > 0 ? ft8_utc_delta_seconds (nutc, entry.nutc) : 0;
      float const score = static_cast<float> (age_seconds)
                          - 18.0f * static_cast<float> (std::min (entry.hits, 20));
      if (score > replace_score)
        {
          replace_score = score;
          replace_index = i;
        }
    }

  if (replace_index < 0)
    {
      return;
    }

  Ft8KnownCqCallEntry& entry = state.entries[static_cast<size_t> (replace_index)];
  bool const same_entry = entry.valid && trim_fixed (entry.call) == call;
  entry.valid = true;
  entry.nutc = nutc;
  entry.freq = freq;
  entry.dt = dt;
  entry.hits = same_entry ? std::min (entry.hits + 1, 999) : 1;
  entry.call = fixed_from_string<12> (call);
}

Ft8CallGridEntry const* find_call_grid_cq_history (Ft8CallGridHistoryState const& state,
                                                   Ft8Request const& request,
                                                   int jseq, float freq,
                                                   float callback_dt,
                                                   float sync, int nsync,
                                                   float cq_score)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount
      || !ft8_history_replay_enabled (request)
      || request.lft8apon == 0
      || !trim_fixed (request.hiscall).empty ()
      || sync < 0.18f
      || nsync < 1)
    {
      return nullptr;
    }
  if (cq_score < 1.0f && nsync < 4)
    {
      return nullptr;
    }

  Ft8CallGridEntry const* best = nullptr;
  float best_score = 1.0e30f;
  auto const& entries = state.entries[static_cast<size_t> (jseq)];
  for (Ft8CallGridEntry const& entry : entries)
    {
      if (!entry.valid || entry.nutc == request.nutc)
        {
          continue;
        }
      int const age_seconds = ft8_utc_delta_seconds (request.nutc, entry.nutc);
      if (age_seconds <= 0 || age_seconds > kFt8CallGridMaxAge * 30 + 15)
        {
          continue;
        }

      float const freq_delta = std::fabs (entry.freq - freq);
      float const dt_delta = std::fabs (entry.dt - callback_dt);
      float const max_freq_delta = entry.hits >= 2 ? 3.0f : 1.5f;
      float const max_dt_delta = entry.hits >= 2 ? 0.18f : 0.10f;
      if (freq_delta > max_freq_delta || dt_delta > max_dt_delta)
        {
          continue;
        }

      float const score = freq_delta + 14.0f * dt_delta
                          + 0.01f * static_cast<float> (age_seconds)
                          - 0.20f * static_cast<float> (std::min (entry.hits, 5));
      if (score < best_score)
        {
          best = &entry;
          best_score = score;
        }
    }
  return best;
}

FixedChars<kFt8DecodedChars> call_grid_cq_message (Ft8CallGridEntry const& entry)
{
  return fixed_from_string<kFt8DecodedChars> (
      "CQ " + trim_fixed (entry.call) + " " + trim_fixed (entry.grid));
}

FixedChars<kFt8DecodedChars> known_call_grid_cq_message (Ft8KnownCallGridEntry const& entry)
{
  return fixed_from_string<kFt8DecodedChars> (
      "CQ " + trim_fixed (entry.call) + " " + trim_fixed (entry.grid));
}

FixedChars<kFt8DecodedChars> known_cq_call_message (Ft8KnownCqCallEntry const& entry)
{
  return fixed_from_string<kFt8DecodedChars> ("CQ " + trim_fixed (entry.call));
}

struct Ft8KnownCqReplayMatch
{
  Ft8KnownCallGridEntry const* entry {};
  float score {0.0f};
};

struct Ft8KnownCqCallReplayMatch
{
  Ft8KnownCqCallEntry const* entry {};
  float score {0.0f};
};

void insert_known_cq_replay_match (std::array<Ft8KnownCqReplayMatch, 12>& matches,
                                   int& count,
                                   Ft8KnownCallGridEntry const* entry,
                                   float score)
{
  if (!entry)
    {
      return;
    }
  for (int i = 0; i < count; ++i)
    {
      if (matches[static_cast<size_t> (i)].entry == entry)
        {
          if (score < matches[static_cast<size_t> (i)].score)
            {
              matches[static_cast<size_t> (i)].score = score;
            }
          return;
        }
    }

  if (count < static_cast<int> (matches.size ()))
    {
      matches[static_cast<size_t> (count++)] = {entry, score};
      return;
    }

  int worst = 0;
  for (int i = 1; i < count; ++i)
    {
      if (matches[static_cast<size_t> (i)].score
          > matches[static_cast<size_t> (worst)].score)
        {
          worst = i;
        }
    }
  if (score < matches[static_cast<size_t> (worst)].score)
    {
      matches[static_cast<size_t> (worst)] = {entry, score};
    }
}

void insert_known_cq_call_replay_match (
    std::array<Ft8KnownCqCallReplayMatch, 12>& matches,
    int& count,
    Ft8KnownCqCallEntry const* entry,
    float score)
{
  if (!entry)
    {
      return;
    }
  for (int i = 0; i < count; ++i)
    {
      if (matches[static_cast<size_t> (i)].entry == entry)
        {
          if (score < matches[static_cast<size_t> (i)].score)
            {
              matches[static_cast<size_t> (i)].score = score;
            }
          return;
        }
    }

  if (count < static_cast<int> (matches.size ()))
    {
      matches[static_cast<size_t> (count++)] = {entry, score};
      return;
    }

  int worst = 0;
  for (int i = 1; i < count; ++i)
    {
      if (matches[static_cast<size_t> (i)].score
          > matches[static_cast<size_t> (worst)].score)
        {
          worst = i;
        }
    }
  if (score < matches[static_cast<size_t> (worst)].score)
    {
      matches[static_cast<size_t> (worst)] = {entry, score};
    }
}

int collect_known_call_grid_cq_matches (Ft8KnownCallGridState const& state,
                                        Ft8Request const& request,
                                        float freq, float callback_dt,
                                        float cq_score, int nsync,
                                        std::array<Ft8KnownCqReplayMatch, 12>& matches,
                                        bool locked_known_cq_candidate = false)
{
  int count = 0;
  bool const known_cq_enabled = ft8_known_cq_replay_enabled (request);
  bool const lightweight_grid =
      !known_cq_enabled && ft8_lightweight_known_cq_grid_enabled (request);
  if ((!known_cq_enabled && !lightweight_grid)
      || (cq_score < 0.75f && nsync < 3))
    {
      return 0;
    }

  bool const history_replay = ft8_history_replay_enabled (request);
  if (!history_replay
      && !locked_known_cq_candidate
      && !lightweight_grid
      && (cq_score < kFt8KnownCallGridFastReplayMinCqScore
          || nsync < kFt8KnownCallGridFastReplayMinSync))
    {
      return 0;
    }
  if (lightweight_grid && nsync < 5)
    {
      return 0;
    }
  bool const low_cq_lightweight_grid = lightweight_grid && cq_score < 5.5f;

  bool const allow_wide_fast = false;

  for (Ft8KnownCallGridEntry const& entry : state.entries)
    {
      if (!entry.valid || entry.freq <= 0.0f || entry.nutc <= 0 || request.nutc <= 0)
        {
          continue;
        }
      if (!history_replay
          && !locked_known_cq_candidate
          && !lightweight_grid
          && entry.hits < kFt8KnownCallGridMinFastReplayHits
          && !allow_wide_fast)
        {
          continue;
        }
      if (lightweight_grid && entry.hits < kFt8KnownCallGridMinFastReplayHits)
        {
          continue;
        }
      if (low_cq_lightweight_grid && (entry.hits < 4 || nsync < 7))
        {
          continue;
        }
      int const age_seconds = ft8_utc_delta_seconds (request.nutc, entry.nutc);
      if (age_seconds <= 0 || age_seconds > kFt8KnownCallGridMaxAgeSeconds)
        {
          continue;
        }
      if (!history_replay
          && age_seconds > kFt8KnownCallGridFastReplayMaxAgeSeconds
          && !(allow_wide_fast && age_seconds <= 15 * 60))
        {
          continue;
        }

      float const freq_delta = std::fabs (entry.freq - freq);
      float const dt_delta = std::fabs (entry.dt - callback_dt);
      float const strict_freq_delta = history_replay
          ? 4.5f
          : (locked_known_cq_candidate || lightweight_grid
                 ? 4.5f
                 : kFt8KnownCallGridFastReplayMaxFreqDelta);
      float const strict_dt_delta = history_replay
          ? 0.55f
          : (locked_known_cq_candidate
                 ? 0.75f
                 : (lightweight_grid ? 0.20f : kFt8KnownCallGridFastReplayMaxDtDelta));
      bool const strict_match =
          freq_delta <= strict_freq_delta
          && dt_delta <= strict_dt_delta
          && (history_replay
              || locked_known_cq_candidate
              || entry.hits >= kFt8KnownCallGridMinFastReplayHits);
      bool const wide_match =
          allow_wide_fast
          && age_seconds <= 15 * 60
          && freq_delta <= 95.0f
          && dt_delta <= 0.24f
          && (entry.hits >= 2 || age_seconds <= 6 * 60);
      if (!strict_match && !wide_match)
        {
          continue;
        }

      float const score = (strict_match ? 0.0f : 25.0f)
                          + (strict_match ? freq_delta : 0.16f * freq_delta)
                          + 8.0f * dt_delta
                          + 0.0004f * static_cast<float> (age_seconds)
                          - 0.05f * static_cast<float> (std::min (entry.hits, 20));
      if (debug_known_cq_replay ())
        {
          std::cerr << "[KNOWNCQ] candidate call=" << trim_fixed (entry.call)
                    << " grid=" << trim_fixed (entry.grid)
                    << " entry_freq=" << entry.freq
                    << " cand_freq=" << freq
                    << " df=" << freq_delta
                    << " entry_dt=" << entry.dt
                    << " cand_dt=" << callback_dt
                    << " ddt=" << dt_delta
                    << " cq=" << cq_score
                    << " nsync=" << nsync
                    << " age=" << age_seconds
                    << " hits=" << entry.hits
                    << " wide=" << (wide_match && !strict_match ? 1 : 0)
                    << '\n';
        }
      insert_known_cq_replay_match (matches, count, &entry, score);
    }

  std::sort (matches.begin (), matches.begin () + count,
             [] (Ft8KnownCqReplayMatch const& lhs,
                 Ft8KnownCqReplayMatch const& rhs) {
               return lhs.score < rhs.score;
             });
  return count;
}

bool ft8_known_cq_call_replay_enabled (Ft8Request const& request)
{
  return request.nzhsym >= 50
         && request.ndepth >= 3
         && !request.supplemental
         && request.ncontest == 0
         && trim_fixed (request.hiscall).empty ();
}

int collect_known_cq_call_matches (Ft8KnownCqCallState const& state,
                                   Ft8Request const& request,
                                   float freq, float callback_dt,
                                   float cq_score, int nsync,
                                   std::array<Ft8KnownCqCallReplayMatch, 12>& matches)
{
  int count = 0;
  if (debug_known_cq_replay ())
    {
      int valid_entries = 0;
      for (Ft8KnownCqCallEntry const& entry : state.entries)
        {
          if (entry.valid)
            {
              if (valid_entries < 12)
                {
                  std::cerr << "[KNOWNCQCALL] entry call=" << trim_fixed (entry.call)
                            << " freq=" << entry.freq
                            << " dt=" << entry.dt
                            << " nutc=" << entry.nutc
                            << " hits=" << entry.hits << '\n';
                }
              ++valid_entries;
            }
        }
      std::cerr << "[KNOWNCQCALL] collect-enter valid=" << valid_entries
                << " score=" << cq_score
                << " nsync=" << nsync
                << " cand_freq=" << freq
                << " cand_dt=" << callback_dt
                << " nutc=" << request.nutc << '\n';
    }
  if (!ft8_known_cq_call_replay_enabled (request)
      || request.nutc <= 0
      || cq_score < 1.25f
      || nsync < 4)
    {
      return 0;
    }

  for (Ft8KnownCqCallEntry const& entry : state.entries)
    {
      if (!entry.valid || entry.freq <= 0.0f || entry.nutc <= 0)
        {
          continue;
        }
      int const age_seconds = ft8_utc_delta_seconds (request.nutc, entry.nutc);
      if (age_seconds <= 0 || age_seconds > kFt8KnownCallGridMaxAgeSeconds)
        {
          continue;
        }
      if (age_seconds > ft8_knowncq_fast_age ())
        {
          continue;
        }

      float const freq_delta = std::fabs (entry.freq - freq);
      float const dt_delta = std::fabs (entry.dt - callback_dt);
      bool const strong_cq_evidence = nsync >= 8 && cq_score >= 1.8f;
      float const freq_limit = strong_cq_evidence ? 4.0f : 2.5f;
      float const dt_limit = strong_cq_evidence ? 0.45f : 0.22f;
      if (freq_delta > freq_limit || dt_delta > dt_limit)
        {
          if (debug_known_cq_replay ())
            {
              std::cerr << "[KNOWNCQCALL] skip-window call=" << trim_fixed (entry.call)
                        << " df=" << freq_delta
                        << " ddt=" << dt_delta
                        << " freq_limit=" << freq_limit
                        << " dt_limit=" << dt_limit << '\n';
            }
          continue;
        }

      float const score = freq_delta
                          + 7.0f * dt_delta
                          + 0.00045f * static_cast<float> (age_seconds)
                          - 0.08f * static_cast<float> (std::min (entry.hits, 20));
      if (debug_known_cq_replay ())
        {
          std::cerr << "[KNOWNCQCALL] candidate call=" << trim_fixed (entry.call)
                    << " df=" << freq_delta
                    << " ddt=" << dt_delta
                    << " score=" << score
                    << " cq=" << cq_score
                    << " nsync=" << nsync << '\n';
        }
      insert_known_cq_call_replay_match (matches, count, &entry, score);
    }

  std::sort (matches.begin (), matches.begin () + count,
             [] (Ft8KnownCqCallReplayMatch const& lhs,
                 Ft8KnownCqCallReplayMatch const& rhs) {
               return lhs.score < rhs.score;
             });
  return count;
}

bool try_known_cq_call_replay (Ft8Request const& request, float const* s8,
                               int nsync, float f1, float xdt, float xbase,
                               float cq_score,
                               FixedChars<kFt8DecodedChars>& msg37,
                               float& xsnr, std::array<int, kFt8Nn>& itone,
                               std::array<signed char, kFt8Bits>& message77,
                               float const* llra, float const* llrb,
                               float const* llrc, float const* llrd)
{
  if (!s8)
    {
      return false;
    }

  std::array<Ft8KnownCqCallReplayMatch, 12> matches {};
  int const count =
      collect_known_cq_call_matches (known_cq_call_history (), request, f1,
                                     xdt - 0.5f, cq_score, nsync, matches);
  for (int index = 0; index < count; ++index)
    {
      Ft8KnownCqCallEntry const& entry =
          *matches[static_cast<size_t> (index)].entry;
      FixedChars<kFt8DecodedChars> const cq_message =
          known_cq_call_message (entry);
      float const callback_dt = xdt - 0.5f;
      float const freq_delta = std::fabs (entry.freq - f1);
      float const dt_delta = std::fabs (entry.dt - callback_dt);
      bool const allow_strict_metrics =
          nsync >= 12
          && cq_score >= 4.0f
          && freq_delta <= 1.0f
          && dt_delta <= 0.45f;
      bool const allow_repeated_tight_metrics =
          allow_strict_metrics
          && entry.hits >= kFt8KnownCallGridMinFastReplayHits
          && nsync >= 15
          && dt_delta <= 0.12f;
      int const hard_limit = allow_repeated_tight_metrics ? 62
          : (allow_strict_metrics ? 56 : -1);
      float const dmin_limit = allow_repeated_tight_metrics ? 140.0f
          : (allow_strict_metrics ? 180.0f : -1.0f);
      if (try_ft8sd_known_message (cq_message, request, s8, nsync, xbase,
                                   msg37, xsnr, itone, message77,
                                   llra, llrb, llrc, llrd, hard_limit, dmin_limit,
                                   allow_strict_metrics))
        {
          if (debug_known_cq_replay ())
            {
              std::cerr << "[KNOWNCQCALL] accept hint=" << trim_fixed (cq_message)
                        << " nsync=" << nsync << '\n';
            }
          return true;
        }
      if (debug_known_cq_replay ())
        {
          std::cerr << "[KNOWNCQCALL] reject hint=" << trim_fixed (cq_message)
                    << " nsync=" << nsync
                    << " strict_metrics=" << (allow_strict_metrics ? 1 : 0)
                    << " df=" << freq_delta
                    << " ddt=" << dt_delta << '\n';
        }
    }
  return false;
}

Ft8CqSignalEntry const* find_cq_signal_history (Ft8CqSignalHistoryState const& state,
                                                Ft8Request const& request,
                                                int jseq, float freq,
                                                float callback_dt,
                                                float cq_score)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount || request.ndepth < 3
      || cq_score < kFt8CqSignalRepeatScore)
    {
      return nullptr;
    }

  Ft8CqSignalEntry const* best = nullptr;
  float best_score = 1.0e30f;
  auto const& entries = state.entries[static_cast<size_t> (jseq)];
  for (Ft8CqSignalEntry const& entry : entries)
    {
      if (!entry.valid || entry.nutc == request.nutc)
        {
          continue;
        }
      int const age_seconds = ft8_utc_delta_seconds (request.nutc, entry.nutc);
      if (age_seconds <= 0 || age_seconds > kFt8CqSignalMaxAge * 30 + 15)
        {
          continue;
        }

      float const freq_delta = std::fabs (entry.freq - freq);
      float const dt_delta = std::fabs (entry.dt - callback_dt);
      if (freq_delta > 3.0f || dt_delta > 0.15f)
        {
          continue;
        }

      float const match_score = freq_delta + 14.0f * dt_delta
                                + 0.01f * static_cast<float> (age_seconds)
                                - 0.05f * entry.score
                                - 0.08f * static_cast<float> (std::min (entry.hits, 4));
      if (match_score < best_score)
        {
          best = &entry;
          best_score = match_score;
        }
    }
  return best;
}

void save_cq_signal_history (Ft8CqSignalHistoryState& state, Ft8Request const& request,
                             int jseq, float freq, float callback_dt,
                             float cq_score,
                             std::array<std::complex<float>, kFt8Nn * 8> const& cs,
                             bool force)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount || request.ndepth < 3
      || (!force && cq_score < kFt8CqSignalSaveScore))
    {
      return;
    }

  auto& entries = state.entries[static_cast<size_t> (jseq)];
  int replace_index = -1;
  float replace_score = -1.0e30f;
  for (int i = 0; i < static_cast<int> (entries.size ()); ++i)
    {
      Ft8CqSignalEntry& entry = entries[static_cast<size_t> (i)];
      if (!entry.valid)
        {
          replace_index = i;
          break;
        }
      int const age_seconds = ft8_utc_delta_seconds (request.nutc, entry.nutc);
      if (age_seconds >= 0
          && age_seconds <= kFt8CqSignalMaxAge * 30 + 15
          && std::fabs (entry.freq - freq) <= 3.0f
          && std::fabs (entry.dt - callback_dt) <= 0.15f)
        {
          replace_index = i;
          break;
        }

      float const score = static_cast<float> (age_seconds) - 3.0f * entry.score;
      if (score > replace_score)
        {
          replace_score = score;
          replace_index = i;
        }
    }

  if (replace_index < 0)
    {
      return;
    }

  Ft8CqSignalEntry& entry = entries[static_cast<size_t> (replace_index)];
  bool const same_signal =
      entry.valid
      && std::fabs (entry.freq - freq) <= 3.0f
      && std::fabs (entry.dt - callback_dt) <= 0.15f
      && ft8_utc_delta_seconds (request.nutc, entry.nutc) <= kFt8CqSignalMaxAge * 30 + 15;
  int const previous_hits = same_signal ? std::min (entry.hits, 4) : 0;
  float const previous_weight = static_cast<float> (previous_hits);
  float const total_weight = previous_weight + 1.0f;

  entry.valid = true;
  entry.nutc = request.nutc;
  entry.freq = same_signal ? (entry.freq * previous_weight + freq) / total_weight : freq;
  entry.dt = same_signal ? (entry.dt * previous_weight + callback_dt) / total_weight : callback_dt;
  entry.score = same_signal ? std::max (entry.score, cq_score) : cq_score;
  entry.hits = same_signal ? std::min (entry.hits + 1, 5) : 1;
  for (size_t index = 0; index < entry.cs.size (); ++index)
    {
      float const previous_mag = same_signal ? std::abs (entry.cs[index]) : 0.0f;
      float const current_mag = std::abs (cs[index]);
      float const merged_mag =
          same_signal ? (previous_mag * previous_weight + current_mag) / total_weight
                      : current_mag;
      entry.cs[index] = std::complex<float> {merged_mag, 0.0f};
    }
}

void downsample_ft8_stage4_candidate (float const* dd0, int* newdat, int decode_pass,
                                      bool use_var_downsample, float sync, float f1,
                                      std::array<std::complex<float>, kFt8A7DownsampleSize>& cd0)
{
  if (!use_var_downsample)
    {
      ftx_ft8_downsample_c (dd0, newdat, f1,
                            reinterpret_cast<fftwf_complex*> (cd0.data ()));
      return;
    }

  thread_local std::array<std::complex<float>, kFt8VarDownsampleSize> cd_var {};
  std::fill (cd_var.begin (), cd_var.end (), std::complex<float> {});

  int nqso = 1;
  int lhighsens =
      (sync < 1.9f
       || ((decode_pass == 2 || decode_pass == 4 || decode_pass == 6) && sync < 3.15f))
          ? 1
          : 0;
  int lsubtracted = 0;
  int npos = 0;
  ftx_ft8var_downsample_c (dd0, newdat, &f1, &nqso,
                           reinterpret_cast<fftwf_complex*> (cd_var.data ()),
                           nullptr, nullptr, &lhighsens, &lsubtracted, &npos, nullptr);

  for (int i = 0; i < kFt8A7DownsampleSize; ++i)
    {
      cd0[static_cast<size_t> (i)] =
          cd_var[static_cast<size_t> (kFt8VarDownsampleOffset + i)];
    }
}

void plan_ft8_ldpc_decode (Ft8Request const& request, float f1, float sync, int pass_iaptype,
                           int& Keff, int& maxosd, int& norder)
{
  norder = 2;
  maxosd = 2;
  if (request.ndepth == 1)
    {
      maxosd = -1;
    }
  else
    {
      int const overrideMaxOsd = stage4_ldpc_maxosd_override ().load (std::memory_order_relaxed);
      int const overrideNOrder = stage4_ldpc_norder_override ().load (std::memory_order_relaxed);
      if (overrideMaxOsd >= 0)
        {
          maxosd = overrideMaxOsd;
        }
      if (overrideNOrder > 0)
        {
          norder = overrideNOrder;
        }
    }
  if (request.nzhsym >= 50
      && request.ndepth <= 2
      && !request.supplemental
      && request.nft8cycles <= 1
      && request.nft8rxfsens <= 1)
    {
      // Live fast passes are latency guards, not the place for expensive OSD.
      // A single OSD-heavy LDPC attempt can ignore the outer Stage4 deadline
      // for seconds on slower Windows hosts, starving the next FT8 cycle.
      maxosd = -1;
    }
  if (request.ndepth >= 4
      && pass_iaptype > 0
	      && request.nzhsym < 47
      && !request.supplemental)
    {
      // JTDX gets many of its weakest live decodes through AP-assisted OSD.
      // Spend the extra saved BP state only on AP passes; applying maxosd=3
      // to every normal pass runs into the live deadline.
      maxosd = std::max (maxosd, 3);
      norder = std::max (norder, 3);
    }
  if (request.lft8lowth
      && pass_iaptype > 0
      && request.nzhsym >= 47
      && !request.supplemental)
    {
      maxosd = std::max (maxosd, 3);
      norder = std::max (norder, request.lft8subpass ? 3 : 2);
    }
  if (request.ndepth == 3
      && (std::fabs (static_cast<float> (request.nfqso) - f1) <= static_cast<float> (request.napwid)
          || std::fabs (static_cast<float> (request.nftx) - f1) <= static_cast<float> (request.napwid)
          || request.ncontest == 7))
    {
      maxosd = 2;
    }
  if (request.lft8subpass
      && request.nft8cycles >= 3
      && sync >= 5.0f
      && request.nzhsym >= 50
      && !request.supplemental)
    {
      maxosd = std::max (maxosd, 3);
      norder = std::max (norder, 3);
    }
  int const overrideMaxOsd = stage4_ldpc_maxosd_override ().load (std::memory_order_relaxed);
  int const overrideNOrder = stage4_ldpc_norder_override ().load (std::memory_order_relaxed);
  if (overrideMaxOsd >= 0)
    {
      maxosd = std::min (maxosd, overrideMaxOsd);
    }
  if (overrideNOrder > 0)
    {
      norder = std::min (norder, overrideNOrder);
    }
  Keff = 91;
}

bool decode_main_candidate_cpp (float* dd0, int* newdat, Ft8Request const& request,
                                int decode_pass, bool use_var_downsample,
                                bool equalized_pipeline, int imetric, int lsubtract,
                                std::array<int, 58> const& apsym,
                                std::array<int, 10> const& aph10,
                                float const* candidate_values, float candidate_cq_flag, float const* sbase,
                                int sbase_size, float& sync, float& f1, float& xdt,
                                float& xbase, int& nharderrors, float& dmin, int& nbadcrc,
                                int& ipass, int& iaptype,
                                FixedChars<kFt8DecodedChars>& msg37, float& xsnr,
                                std::array<int, kFt8Nn>& itone,
                                std::array<signed char, kFt8Bits>& message77,
                                Ft8A7Slot const* sd_hints,
                                Ft8CqSignalHistoryState* signal_history_state,
                                Ft8CallGridHistoryState* call_grid_history_state,
                                int jseq)
{
  sync = 0.0f;
  f1 = 0.0f;
  xdt = 0.0f;
  xbase = 0.0f;
  nharderrors = -1;
  dmin = 0.0f;
  nbadcrc = 1;
  ipass = 0;
  iaptype = 0;
  msg37 = blank_fixed<kFt8DecodedChars> ();
  xsnr = 0.0f;
  itone.fill (0);
  message77.fill (0);

  if (!dd0 || !newdat || !candidate_values || !sbase)
    {
      return false;
    }
  if (stage4_should_cancel ())
    {
      return false;
    }

  auto const mycall13 = widen_call_for_pack77 (request.mycall);
  auto const hiscall13 = widen_call_for_pack77 (request.hiscall);
  // NON resettare il context per ogni candidato: il reset azzerava la hash table
  // (call->hash dei call non-standard) prima di ogni candidato, impedendo qualsiasi
  // accumulo e lasciando ~15% di decode come "<...>". Il context e' thread_local
  // (un worker seriale) e l'unpack vi auto-salva i call completi decodificati, quindi
  // senza reset accumula attraverso candidati e slot come JTDX, risolvendo i call
  // hashati (compound/special). set_context aggiorna solo my/dx call senza azzerare.
  // Gli hash sono mappe immutabili (cap 4096), nessun rischio di stato stantio.
  legacy_pack77_set_context_c (mycall13.data (), hiscall13.data ());

  ftx_ft8_prepare_candidate_c (candidate_values[2], candidate_values[0], candidate_values[1],
                               sbase, sbase_size, &sync, &f1, &xdt, &xbase);
  bool const locked_known_cq_candidate = candidate_cq_flag >= 2.9f;

  std::array<std::complex<float>, kFt8A7DownsampleSize> cd0 {};
  std::array<float, 8 * kFt8Nn> s8 {};
  std::array<float, 174> llra {};
  std::array<float, 174> llrb {};
  std::array<float, 174> llrc {};
  std::array<float, 174> llrd {};
  std::array<float, 174> llre {};
  std::array<std::complex<float>, kFt8Nn * 8> current_cs {};

  int local_newdat = *newdat;
  downsample_ft8_stage4_candidate (dd0, &local_newdat, decode_pass,
                                   use_var_downsample, sync, f1, cd0);
  *newdat = local_newdat;
  if (stage4_should_cancel ())
    {
      return false;
    }

  int ibest = 0;
  float delfbest = 0.0f;
  ftx_ft8_a7_search_initial_c (cd0.data (), kFt8A7Np2, kFt8A7Fs2, xdt, &ibest, &delfbest);
  if (!locked_known_cq_candidate)
    {
      f1 += delfbest;
    }

  int second_pass_newdat = 0;
  downsample_ft8_stage4_candidate (dd0, &second_pass_newdat, decode_pass,
                                   use_var_downsample, sync, f1, cd0);
  if (stage4_should_cancel ())
    {
      return false;
    }

  ftx_ft8_a7_refine_search_c (cd0.data (), kFt8A7Np2, kFt8A7Fs2, ibest,
                              &ibest, &sync, &xdt);
  xdt += 0.5f;

  bool const live_weak_bitmetrics =
      request.nzhsym >= 50
      && request.ndepth >= 3
      && !request.supplemental
      && request.lft8lowth
      && (request.lft8subpass || request.nft8cycles >= 3 || request.nft8rxfsens >= 3);
  bool const deep_bitmetrics =
      (request.ndepth >= 4 && request.supplemental)
      || live_weak_bitmetrics;
  bool const try_equalized_metrics =
      !equalized_pipeline
      &&
      request.nzhsym >= 41
      && request.ndepth >= 3
      && !request.supplemental;
  int const metric_attempts = equalized_pipeline ? 1 : (try_equalized_metrics ? 2 : 1);
  for (int metric_attempt = 0; metric_attempt < metric_attempts; ++metric_attempt)
    {
      bool const equalized_metrics = equalized_pipeline || metric_attempt != 0;
      int nsync = 0;
      ftx_ft8_bitmetrics_capture_c (cd0.data (), kFt8A7Np2, ibest, imetric,
                                    kFt8BitMetricScale,
                                    deep_bitmetrics ? 1 : 0,
                                    equalized_metrics ? 1 : 0,
                                    nullptr, current_cs.data (),
                                    s8.data (), &nsync, llra.data (), llrb.data (),
                                    llrc.data (), llrd.data (), llre.data ());
      *newdat = local_newdat;

  float const cq_signature_score = ft8_cq_signature_score (s8.data (), 8);
  float const callback_dt_for_history = xdt - 0.5f;
  Ft8CqSignalEntry const* cq_history =
      signal_history_state
          ? find_cq_signal_history (*signal_history_state, request, jseq, f1,
                                    callback_dt_for_history, cq_signature_score)
          : nullptr;
  std::array<float, 8 * kFt8Nn> history_s8 {};
  std::array<float, 174> history_llra {};
  std::array<float, 174> history_llrb {};
  std::array<float, 174> history_llrc {};
  std::array<float, 174> history_llrd {};
  std::array<float, 174> history_llre {};
  int history_nsync = 0;
  bool const have_cq_history = cq_history != nullptr;
  if (have_cq_history)
    {
      ftx_ft8_bitmetrics_capture_c (cd0.data (), kFt8A7Np2, ibest, imetric,
                                    kFt8BitMetricScale,
                                    deep_bitmetrics ? 1 : 0,
                                    equalized_metrics ? 1 : 0,
                                    cq_history->cs.data (), nullptr,
                                    history_s8.data (), &history_nsync,
                                    history_llra.data (), history_llrb.data (),
                                    history_llrc.data (), history_llrd.data (),
                                    history_llre.data ());
    }
  int const sync_threshold = ft8_candidate_sync_threshold (imetric, request);
  float const cq_replay_score = std::max (cq_signature_score, candidate_cq_flag);
  Ft8A7Entry const* repeated_hint =
      find_ft8_repeated_hint (sd_hints, request, f1, xdt - 0.5f);
  Ft8ExpectedTarget const* trace_target =
      find_ft8_expected_target (request.nutc, f1, callback_dt_for_history);
  if (!trace_target)
    {
      trace_target = find_ft8_expected_target (request.nutc, candidate_values[0],
                                               candidate_values[1] - 0.5f);
    }
  bool const weak_repeated_hint_window =
      repeated_hint != nullptr
      && nsync >= 1
      && sync >= 0.20f;
	  bool const advanced_live_cq = ft8_live_advanced_history_enabled (request);
	  bool const cq_sync_override =
	      advanced_live_cq
	      && ((nsync == 4 && cq_signature_score >= 6.9f)
	          || (nsync == 5 && cq_signature_score >= 6.4f)
	          || (nsync == 6 && cq_signature_score >= 5.9f));
      if (trace_target)
        {
          trace_ft8_expected_target (*trace_target, "candidate-metrics",
                                     [&] (std::ostream& out) {
            out << " decode_pass=" << decode_pass
                << " metric_attempt=" << metric_attempt
                << " equalized_metrics=" << (equalized_metrics ? 1 : 0)
                << " candidate_freq=" << candidate_values[0]
                << " candidate_dt_raw=" << candidate_values[1]
                << " candidate_dt_cb=" << (candidate_values[1] - 0.5f)
                << " refined_freq=" << f1
                << " refined_dt=" << callback_dt_for_history
                << " sync=" << sync
                << " nsync=" << nsync
                << " threshold=" << sync_threshold
                << " cq_signature=" << cq_signature_score
                << " candidate_cq=" << candidate_cq_flag
                << " cq_override=" << (cq_sync_override ? 1 : 0)
                << " repeated_hint=" << (repeated_hint ? 1 : 0)
                << " use_var_downsample=" << (use_var_downsample ? 1 : 0)
                << " equalized_pipeline=" << (equalized_pipeline ? 1 : 0);
          });
        }

	  if (nsync <= sync_threshold && !cq_sync_override)
	    {
      bool replay_ok =
          try_call_grid_cq_replay (call_grid_history_state, request, s8.data (),
                                   nsync, sync, f1, xdt, xbase,
                                   cq_signature_score, msg37, xsnr, itone,
                                   message77, llra.data (), llrb.data (),
                                   llrc.data (), llrd.data (), jseq);
      if (!replay_ok)
        {
          replay_ok =
              try_known_cq_call_replay (request, s8.data (), nsync, f1, xdt,
                                        xbase, cq_replay_score, msg37,
                                        xsnr, itone, message77,
                                        llra.data (), llrb.data (), llrc.data (),
                                        llrd.data ());
        }
      if (!replay_ok)
        {
          std::array<Ft8KnownCqReplayMatch, 12> known_cq_matches {};
          int const known_cq_count =
              collect_known_call_grid_cq_matches (known_call_grid_history (), request, f1,
                                                  callback_dt_for_history,
                                                  cq_signature_score, nsync,
                                                  known_cq_matches,
                                                  locked_known_cq_candidate);
          bool const history_replay = ft8_history_replay_enabled (request);
          int const known_hard_limit = history_replay
              ? (nsync >= 5 ? 84 : 92)
              : (nsync >= 5 ? 78 : 82);
          float const known_dmin_limit = history_replay
              ? (nsync >= 5 ? 210.0f : 245.0f)
              : (nsync >= 5 ? 180.0f : 200.0f);
          for (int index = 0; index < known_cq_count && !replay_ok; ++index)
            {
              FixedChars<kFt8DecodedChars> const cq_message =
                  known_call_grid_cq_message (*known_cq_matches[static_cast<size_t> (index)].entry);
              replay_ok =
                  try_ft8sd_known_message (cq_message, request, s8.data (), nsync,
                                           xbase, msg37, xsnr, itone, message77,
                                           llra.data (), llrb.data (), llrc.data (),
                                           llrd.data (), known_hard_limit,
                                           known_dmin_limit);
            }
        }
      if (!replay_ok && weak_repeated_hint_window)
        {
          replay_ok =
              try_ft8sd_repeated_hint (sd_hints, request, s8.data (), nsync, f1, xdt,
                                       xbase, msg37, xsnr, itone, message77,
                                       llra.data (), llrb.data (), llrc.data (),
                                       llrd.data ());
        }
      if (!replay_ok)
        {
          if (trace_target)
            {
              trace_ft8_expected_target (*trace_target, "low-nsync-reject",
                                         [&] (std::ostream& out) {
                out << " decode_pass=" << decode_pass
                    << " metric_attempt=" << metric_attempt
                    << " nsync=" << nsync
                    << " threshold=" << sync_threshold
                    << " sync=" << sync
                    << " cq_signature=" << cq_signature_score
                    << " candidate_cq=" << candidate_cq_flag;
              });
            }
          continue;
        }
      if (trace_target)
        {
          trace_ft8_expected_target (*trace_target, "replay-success",
                                     [&] (std::ostream& out) {
            out << " decode_pass=" << decode_pass
                << " metric_attempt=" << metric_attempt
                << " msg=\"" << trim_fixed (msg37) << '"'
                << " expected_match="
                << (ft8_expected_message_matches (*trace_target, trim_fixed (msg37)) ? 1 : 0);
          });
        }
      nharderrors = kFt8StrictHardErrors;
      dmin = 0.0f;
      nbadcrc = 0;
      ipass = 0;
      iaptype = 0;
      if (signal_history_state && ft8sd_hint_is_cq (msg37))
        {
          save_cq_signal_history (*signal_history_state, request, jseq, f1,
                                  callback_dt_for_history, cq_signature_score,
                                  current_cs, true);
        }
      return true;
    }
  bool const cq_only_decode = cq_sync_override && nsync <= sync_threshold;

  bool const has_direct_ap_context =
      trim_length (request.hiscall.data (), 12) > 0
      || trim_length (request.hisgrid.data (), 6) > 0;
  bool const live_cq_ap_candidate =
      request.nzhsym >= 50
      && request.ndepth >= 3
      && request.lft8apon != 0
      && !request.supplemental
      && !has_direct_ap_context
      && request.ncontest == 0
      && nsync >= 10
      && sync >= 0.75f
      && (candidate_cq_flag >= 1.75f || cq_signature_score >= 4.8f);
  int const candidate_lft8apon =
      (has_direct_ap_context || live_cq_ap_candidate) ? request.lft8apon : 0;
  int npasses = ftx_ft8_select_npasses_c (candidate_lft8apon, request.lapcqonly,
                                          request.ncontest, request.nzhsym,
                                          request.nqsoprogress);
  bool const live_full_ap =
      request.nzhsym >= 47
      && request.ndepth >= 4
      && candidate_lft8apon != 0
      && !request.supplemental;
  if (live_full_ap || live_cq_ap_candidate)
    {
      // Questi rami fissano la finestra a otto passate. Con l'a priori storico
      // acceso servono anche gli slot in coda, dove stanno le ipotesi dalle
      // stazioni sentite: senza questo, quelle passate non vengono MAI
      // eseguite -- verificato, il tipo 7 non compariva mai fra quelli visti e
      // la misura dava "nessuna differenza" per il motivo sbagliato.
      npasses = 8 + ftx_ft8_ap_storico_passate_c ();
    }
  int pass_first = 1;
  int pass_last = std::max (0, npasses);
  ftx_ft8_plan_pass_window_c (0, npasses, &pass_first, &pass_last);

  std::array<float, 174> llrz {};
  std::array<int, 174> apmask {};
  std::array<signed char, 174> apmask_bits {};
  std::array<signed char, 91> message91 {};
  std::array<signed char, 174> cw {};

  int const llr_attempts = have_cq_history ? 2 : 1;
  for (int llr_attempt = 0; llr_attempt < llr_attempts; ++llr_attempt)
    {
      float const* active_llra = llr_attempt == 0 ? llra.data () : history_llra.data ();
      float const* active_llrb = llr_attempt == 0 ? llrb.data () : history_llrb.data ();
      float const* active_llrc = llr_attempt == 0 ? llrc.data () : history_llrc.data ();
      float const* active_llrd = llr_attempt == 0 ? llrd.data () : history_llrd.data ();
      float const* active_llre = llr_attempt == 0 ? llre.data () : history_llre.data ();

      // ---- Precalcolo a BLOCCHI delle passate di questo tentativo LLR.
      //
      // Il decoder vettorizzato lavora su sedici parole per volta, una per
      // corsia del registro. Decodificando una passata alla volta quindici
      // corsie girano a vuoto: in FT8 misurati 604 ms per passata contro i 382
      // del decoder originale, cioe' il contrario di FT2, dove la via a blocchi
      // c'e' gia' e i tempi crollano di due ordini di grandezza.
      //
      // La preparazione di una passata non dipende dall'esito delle altre,
      // quindi si possono preparare tutte in anticipo e decodificarle insieme.
      // Le terne (Keff, maxosd, norder) pero' cambiano da passata a passata e
      // la chiamata a blocchi ne accetta una sola: le passate vengono percio'
      // raggruppate per terna, un blocco per gruppo.
      //
      // Il ciclo sotto resta identico: se trova il risultato gia' pronto lo usa,
      // altrimenti decodifica come prima. Le passate oltre la prima riuscita
      // vengono decodificate senza essere usate -- lavoro che prima si
      // risparmiava, ma che a corsie piene non costa piu' di una sola parola.
      struct PassoPronto
      {
        int pass_index {0};
        std::array<signed char, 91> message91 {};
        std::array<signed char, 174> cw {};
        int ntype {0};
        int nharderrors {-1};
        float dmin {0.0f};
      };
      std::vector<PassoPronto> passi_pronti;
      if (ft8_use_fastldpc () && ft8_batch_passes () && !g_ft8_forza_classico)
        {
          struct PassoDaFare
          {
            int pass_index;
            int Keff, maxosd, norder;
            std::array<float, 174> llrz;
            std::array<signed char, 174> apmask_bits;
          };
          std::vector<PassoDaFare> da_fare;
          for (int pi = pass_first; pi <= pass_last; ++pi)
            {
              std::array<float, 174> pre_llrz {};
              std::array<int, 174> pre_apmask {};
              int pre_iaptype = 0;
              int pre_ready = 0;
              bool const pre_generic = live_cq_ap_candidate && pi == 7;
              if ((live_full_ap || live_cq_ap_candidate) && pi >= 6 && pi <= 8)
                {
                  if (pre_generic)
                    pre_ready = ftx_ft8_prepare_decode_pass_c (
                        pi, request.nqsoprogress, request.lapcqonly, request.ncontest,
                        request.nfqso, request.nftx, f1, request.napwid, apsym.data (),
                        aph10.data (), active_llra, active_llrb, active_llrc, active_llrd,
                        active_llre, pre_llrz.data (), pre_apmask.data (), &pre_iaptype);
                  else
                    pre_ready = ftx_ft8_prepare_cq_ap_pass_c (
                        pi, request.nqsoprogress, request.lapcqonly, request.ncontest,
                        request.nfqso, request.nftx, f1, request.napwid, apsym.data (),
                        aph10.data (), active_llra, active_llrb, active_llrc,
                        pre_llrz.data (), pre_apmask.data (), &pre_iaptype);
                }
              else
                {
                  pre_ready = ftx_ft8_prepare_decode_pass_c (
                      pi, request.nqsoprogress, request.lapcqonly, request.ncontest,
                      request.nfqso, request.nftx, f1, request.napwid, apsym.data (),
                      aph10.data (), active_llra, active_llrb, active_llrc, active_llrd,
                      active_llre, pre_llrz.data (), pre_apmask.data (), &pre_iaptype);
                }
              if (pre_ready == 0) continue;
              if (cq_only_decode && pre_iaptype != 1) continue;
              if (request.nzhsym >= 50 && pre_iaptype == 1 && !request.supplemental
                  && cq_signature_score < 3.1f) continue;

              PassoDaFare voce;
              voce.pass_index = pi;
              voce.Keff = 91; voce.maxosd = 2; voce.norder = 2;
              plan_ft8_ldpc_decode (request, f1, sync, pre_iaptype,
                                    voce.Keff, voce.maxosd, voce.norder);
              voce.llrz = pre_llrz;
              std::transform (pre_apmask.begin (), pre_apmask.end (),
                              voce.apmask_bits.begin (), [] (int value) {
                                return static_cast<signed char> (value != 0 ? 1 : 0);
                              });
              da_fare.push_back (voce);
            }

          std::vector<bool> fatto (da_fare.size (), false);
          for (size_t i = 0; i < da_fare.size (); ++i)
            {
              if (fatto[i]) continue;
              std::vector<size_t> gruppo;
              for (size_t j = i; j < da_fare.size (); ++j)
                {
                  if (fatto[j]) continue;
                  if (da_fare[j].Keff == da_fare[i].Keff
                      && da_fare[j].maxosd == da_fare[i].maxosd
                      && da_fare[j].norder == da_fare[i].norder)
                    {
                      gruppo.push_back (j);
                      fatto[j] = true;
                    }
                }
              int const n = static_cast<int> (gruppo.size ());
              std::vector<float> blocco_llr (static_cast<size_t> (n) * 174);
              std::vector<signed char> blocco_ap (static_cast<size_t> (n) * 174);
              for (int k = 0; k < n; ++k)
                {
                  PassoDaFare const& v = da_fare[gruppo[static_cast<size_t> (k)]];
                  std::copy (v.llrz.begin (), v.llrz.end (),
                             blocco_llr.begin () + static_cast<size_t> (k) * 174);
                  std::copy (v.apmask_bits.begin (), v.apmask_bits.end (),
                             blocco_ap.begin () + static_cast<size_t> (k) * 174);
                }
              std::vector<signed char> blocco_msg (static_cast<size_t> (n) * 91);
              std::vector<signed char> blocco_cw (static_cast<size_t> (n) * 174);
              std::vector<int> blocco_ntype (static_cast<size_t> (n));
              std::vector<int> blocco_hard (static_cast<size_t> (n));
              std::vector<float> blocco_dmin (static_cast<size_t> (n));
              fastldpc_set_ft8_mode_c (1);
              fastldpc_decode174_91_batch_c (n, blocco_llr.data (), blocco_ap.data (),
                                             da_fare[i].Keff, da_fare[i].maxosd,
                                             da_fare[i].norder, blocco_msg.data (),
                                             blocco_cw.data (), blocco_ntype.data (),
                                             blocco_hard.data (), blocco_dmin.data ());
              for (int k = 0; k < n; ++k)
                {
                  PassoPronto out;
                  out.pass_index = da_fare[gruppo[static_cast<size_t> (k)]].pass_index;
                  std::copy_n (blocco_msg.begin () + static_cast<size_t> (k) * 91, 91,
                               out.message91.begin ());
                  std::copy_n (blocco_cw.begin () + static_cast<size_t> (k) * 174, 174,
                               out.cw.begin ());
                  out.ntype = blocco_ntype[static_cast<size_t> (k)];
                  out.nharderrors = blocco_hard[static_cast<size_t> (k)];
                  out.dmin = blocco_dmin[static_cast<size_t> (k)];
                  passi_pronti.push_back (out);
                }
            }
        }

      for (int pass_index = pass_first; pass_index <= pass_last; ++pass_index)
        {
          if (stage4_should_cancel ())
            {
              return false;
            }
          int pass_iaptype = 0;
          int pass_ready = 0;
          bool const use_live_cq_generic_ap =
              live_cq_ap_candidate && pass_index == 7;
          if ((live_full_ap || live_cq_ap_candidate)
              && pass_index >= 6 && pass_index <= 8)
            {
              if (use_live_cq_generic_ap)
                {
                  pass_ready = ftx_ft8_prepare_decode_pass_c (
                      pass_index, request.nqsoprogress, request.lapcqonly,
                      request.ncontest, request.nfqso, request.nftx, f1,
                      request.napwid, apsym.data (), aph10.data (),
                      active_llra, active_llrb, active_llrc, active_llrd,
                      active_llre, llrz.data (), apmask.data (), &pass_iaptype);
                }
              else
                {
                  pass_ready = ftx_ft8_prepare_cq_ap_pass_c (pass_index, request.nqsoprogress,
                                                             request.lapcqonly, request.ncontest,
                                                             request.nfqso, request.nftx, f1,
                                                             request.napwid, apsym.data (), aph10.data (),
                                                             active_llra, active_llrb, active_llrc,
                                                             llrz.data (), apmask.data (), &pass_iaptype);
                }
            }
          else
            {
              pass_ready = ftx_ft8_prepare_decode_pass_c (pass_index, request.nqsoprogress,
                                                          request.lapcqonly, request.ncontest,
                                                          request.nfqso, request.nftx, f1,
                                                          request.napwid, apsym.data (), aph10.data (),
                                                          active_llra, active_llrb, active_llrc,
                                                          active_llrd, active_llre, llrz.data (),
                                                          apmask.data (), &pass_iaptype);
            }
          if (pass_ready == 0)
            {
              if (trace_target)
                {
                  trace_ft8_expected_target (*trace_target, "ap-pass-skip",
                                             [&] (std::ostream& out) {
                    out << " decode_pass=" << decode_pass
                        << " metric_attempt=" << metric_attempt
                        << " pass_index=" << pass_index
                        << " reason=not_ready";
                  });
                }
              continue;
            }
          if (cq_only_decode && pass_iaptype != 1)
            {
              if (trace_target)
                {
                  trace_ft8_expected_target (*trace_target, "ap-pass-skip",
                                             [&] (std::ostream& out) {
                    out << " decode_pass=" << decode_pass
                        << " metric_attempt=" << metric_attempt
                        << " pass_index=" << pass_index
                        << " iaptype=" << pass_iaptype
                        << " reason=cq_only";
                  });
                }
              continue;
            }
          if (request.nzhsym >= 50
              && pass_iaptype == 1
              && !request.supplemental
              && cq_signature_score < 3.1f)
            {
              if (trace_target)
                {
                  trace_ft8_expected_target (*trace_target, "ap-pass-skip",
                                             [&] (std::ostream& out) {
                    out << " decode_pass=" << decode_pass
                        << " metric_attempt=" << metric_attempt
                        << " pass_index=" << pass_index
                        << " iaptype=" << pass_iaptype
                        << " reason=cq_signature"
                        << " cq_signature=" << cq_signature_score;
                  });
                }
              continue;
            }

          std::transform (apmask.begin (), apmask.end (), apmask_bits.begin (),
                          [] (int value) {
                            return static_cast<signed char> (value != 0 ? 1 : 0);
                          });

          int Keff = 91;
          int maxosd = 2;
          int norder = 2;
          plan_ft8_ldpc_decode (request, f1, sync, pass_iaptype, Keff, maxosd, norder);

          int ntype = 0;
          int pass_nharderrors = -1;
          float pass_dmin = 0.0f;
          PassoPronto const* pronto = nullptr;
          for (PassoPronto const& pp : passi_pronti)
            {
              if (pp.pass_index == pass_index) { pronto = &pp; break; }
            }
          if (pronto != nullptr)
            {
              // Gia' decodificata nel blocco: nessuna chiamata al decoder.
              message91 = pronto->message91;
              cw = pronto->cw;
              ntype = pronto->ntype;
              pass_nharderrors = pronto->nharderrors;
              pass_dmin = pronto->dmin;
            }
          else
            {
              ft8_ldpc_decode (llrz.data (), Keff, maxosd, norder, apmask_bits.data (),
                                  message91.data (), cw.data (), &ntype,
                                  &pass_nharderrors, &pass_dmin);
            }
          bool const can_retry_local_cq_history_osd =
              (pass_nharderrors < 0 || pass_nharderrors > kFt8MaxHardErrors)
              && call_grid_history_state
              && decode_pass <= 2
              && pass_iaptype == 0
              && request.nzhsym >= 50
              && request.ndepth >= 3
              && !request.supplemental
              && nsync >= 8
              && sync >= 0.75f
              && stage4_remaining_ms () >= 900
              && (maxosd < 3 || norder < 4)
              && find_call_grid_cq_history (*call_grid_history_state, request, jseq,
                                            f1, callback_dt_for_history, sync,
                                            nsync, cq_signature_score) != nullptr;
          if (can_retry_local_cq_history_osd)
            {
              ft8_ldpc_decode (llrz.data (), Keff, 3, 4, apmask_bits.data (),
                                  message91.data (), cw.data (), &ntype,
                                  &pass_nharderrors, &pass_dmin);
            }
          bool const can_retry_strong_cq_osd =
              decode_pass == 1
              && pass_index == 3
              && request.nzhsym >= 50
              && request.ndepth >= 3
              && !request.supplemental
              && pass_iaptype == 0
              && candidate_values[2] >= 1.0f
              && candidate_cq_flag >= 1.5f
              && cq_signature_score >= 3.4f
              && nsync >= 15;
          if ((pass_nharderrors < 0 || pass_nharderrors > kFt8MaxHardErrors)
              && can_retry_strong_cq_osd
              && (maxosd < 3 || norder < 4))
            {
              // A narrow CQ-only OSD rescue recovers strong sync-search CQ
              // candidates that BP misses, without making the full pass OSD-heavy.
              ft8_ldpc_decode (llrz.data (), Keff, 3, 4, apmask_bits.data (),
                                  message91.data (), cw.data (), &ntype,
                                  &pass_nharderrors, &pass_dmin);
            }
          bool const can_retry_live_cq_ap_osd =
              live_cq_ap_candidate
              && decode_pass <= 2
              && pass_index >= 6
              && pass_index <= 8
              && pass_iaptype == 1
              && nsync >= 14
              && candidate_cq_flag >= 1.75f
              && sync >= 2.5f
              && (maxosd < 3 || norder < 4);
          if ((pass_nharderrors < 0 || pass_nharderrors > kFt8MaxHardErrors)
              && can_retry_live_cq_ap_osd)
            {
              std::array<float, 174> generic_llrz {};
              std::array<int, 174> generic_apmask {};
              int generic_iaptype = 0;
              int const generic_ready =
                  ftx_ft8_prepare_decode_pass_c (pass_index, request.nqsoprogress,
                                                 request.lapcqonly, request.ncontest,
                                                 request.nfqso, request.nftx, f1,
                                                 request.napwid, apsym.data (), aph10.data (),
                                                 active_llra, active_llrb, active_llrc,
                                                 active_llrd, active_llre,
                                                 generic_llrz.data (),
                                                 generic_apmask.data (), &generic_iaptype);
              if (generic_ready != 0 && generic_iaptype == 1)
                {
                  std::array<signed char, 174> generic_apmask_bits {};
                  std::transform (generic_apmask.begin (), generic_apmask.end (),
                                  generic_apmask_bits.begin (),
                                  [] (int value) {
                                    return static_cast<signed char> (value != 0 ? 1 : 0);
                                  });
                  std::array<signed char, 91> generic_message91 {};
                  std::array<signed char, 174> generic_cw {};
                  int generic_ntype = 0;
                  int generic_hard = -1;
                  float generic_dmin = 0.0f;
                  ft8_ldpc_decode (generic_llrz.data (), Keff, 3, 4,
                                      generic_apmask_bits.data (),
                                      generic_message91.data (), generic_cw.data (),
                                      &generic_ntype, &generic_hard, &generic_dmin);
                  if (generic_hard >= 0 && generic_hard <= kFt8MaxHardErrors)
                    {
                      llrz = generic_llrz;
                      apmask_bits = generic_apmask_bits;
                      message91 = generic_message91;
                      cw = generic_cw;
                      ntype = generic_ntype;
                      pass_nharderrors = generic_hard;
                      pass_dmin = generic_dmin;
                    }
                }
            }
          if (stage4_should_cancel ())
            {
              return false;
            }
          if (trace_target)
            {
              trace_ft8_expected_target (*trace_target, "ldpc-result",
                                         [&] (std::ostream& out) {
                out << " decode_pass=" << decode_pass
                    << " metric_attempt=" << metric_attempt
                    << " pass_index=" << pass_index
                    << " iaptype=" << pass_iaptype
                    << " hard=" << pass_nharderrors
                    << " dmin=" << pass_dmin
                    << " maxosd=" << maxosd
                    << " norder=" << norder
                    << " ntype=" << ntype;
              });
            }
          if (pass_nharderrors < 0 || pass_nharderrors > kFt8MaxHardErrors)
            {
              continue;
            }

      std::copy_n (message91.begin (), kFt8Bits, message77.begin ());

      FixedChars<kFt8DecodedChars> candidate_msg = blank_fixed<kFt8DecodedChars> ();
      int quirky = 0;
      int const unpack_ok = legacy_pack77_unpack77bits_c (message77.data (), 1,
                                                          candidate_msg.data (), &quirky);
      candidate_msg = normalize_resolved_hash_call_tokens (candidate_msg);
      if (has_unresolved_hash_placeholder (candidate_msg))
        {
          apply_pack77_hash_external_seed_cache ();
          FixedChars<kFt8DecodedChars> retry_msg = blank_fixed<kFt8DecodedChars> ();
          int retry_quirky = 0;
          int const retry_ok = legacy_pack77_unpack77bits_c (message77.data (), 1,
                                                             retry_msg.data (),
                                                             &retry_quirky);
          if (retry_ok != 0)
            {
              candidate_msg = normalize_resolved_hash_call_tokens (retry_msg);
              quirky = retry_quirky;
            }
        }
	      if (ftx_ft8_validate_candidate_meta_c (message77.data (), cw.data (),
	                                             pass_nharderrors, unpack_ok, quirky,
	                                             request.ncontest) == 0)
	        {
              if (trace_target)
                {
                  trace_ft8_expected_target (*trace_target, "post-ldpc-reject",
                                             [&] (std::ostream& out) {
                    out << " decode_pass=" << decode_pass
                        << " metric_attempt=" << metric_attempt
                        << " pass_index=" << pass_index
                        << " reason=meta"
                        << " unpack_ok=" << unpack_ok
                        << " quirky=" << quirky
                        << " msg=\"" << trim_fixed (candidate_msg) << '"';
                  });
                }
	          continue;
	        }
	      if (pass_iaptype == 1 && !is_strict_standard_ft8_message (candidate_msg))
	        {
              if (trace_target)
                {
                  trace_ft8_expected_target (*trace_target, "post-ldpc-reject",
                                             [&] (std::ostream& out) {
                    out << " decode_pass=" << decode_pass
                        << " metric_attempt=" << metric_attempt
                        << " pass_index=" << pass_index
                        << " reason=ap_nonstandard"
                        << " msg=\"" << trim_fixed (candidate_msg) << '"';
                  });
                }
	          continue;
	        }
      int const candidate_n3 = 4 * int (message77[71]) + 2 * int (message77[72]) + int (message77[73]);
      int const candidate_i3 = 4 * int (message77[74]) + 2 * int (message77[75]) + int (message77[76]);
      bool const candidate_risky =
          pass_iaptype > 0
          || (static_cast<float> (pass_nharderrors) + pass_dmin) > 36.6f;
      if (candidate_risky)
        {
          std::array<char, kFt8DecodedChars> checked_msg {};
          std::copy_n (candidate_msg.begin (), kFt8DecodedChars, checked_msg.begin ());
          int checked_badcrc = 0;
          int const lcall1hash = !checked_msg.empty () && checked_msg[0] == '<' ? 1 : 0;
          std::array<char, 4> hisgrid4 {};
          std::copy_n (request.hisgrid.begin (), hisgrid4.size (), hisgrid4.begin ());
          ftx_ft8var_chkfalse8_c (checked_msg.data (), &candidate_i3, &candidate_n3,
                                  &checked_badcrc, &pass_iaptype, &lcall1hash,
                                  request.mycall.data (), request.hiscall.data (),
                                  hisgrid4.data ());
	          if (checked_badcrc != 0)
	            {
                  if (trace_target)
                    {
                      trace_ft8_expected_target (*trace_target, "post-ldpc-reject",
                                                 [&] (std::ostream& out) {
                        out << " decode_pass=" << decode_pass
                            << " metric_attempt=" << metric_attempt
                            << " pass_index=" << pass_index
                            << " reason=false8"
                            << " iaptype=" << pass_iaptype
                            << " msg=\"" << trim_fixed (candidate_msg) << '"';
                      });
                    }
	              continue;
	            }
          candidate_msg = fixed_from_chars<kFt8DecodedChars> (checked_msg.data ());
          candidate_msg = normalize_resolved_hash_call_tokens (candidate_msg);
        }
	      // Il nominativo malformato si scarta sempre, quale che sia il numero
	      // di bit ribaltati: e' una forma impossibile, non un segnale debole.
	      if (has_malformed_call_word (candidate_msg)
	          || (pass_nharderrors > kFt8StrictHardErrors
	              && !is_strict_standard_ft8_message (candidate_msg)))
	        {
              if (trace_target)
                {
                  trace_ft8_expected_target (*trace_target, "post-ldpc-reject",
                                             [&] (std::ostream& out) {
                    out << " decode_pass=" << decode_pass
                        << " metric_attempt=" << metric_attempt
                        << " pass_index=" << pass_index
                        << " reason=hard_nonstandard"
                        << " hard=" << pass_nharderrors
                        << " msg=\"" << trim_fixed (candidate_msg) << '"';
                  });
                }
	          continue;
	        }
	      if (!is_plausible_ft8_message_for_emit (candidate_msg))
	        {
              if (trace_target)
                {
                  trace_ft8_expected_target (*trace_target, "post-ldpc-reject",
                                             [&] (std::ostream& out) {
                    out << " decode_pass=" << decode_pass
                        << " metric_attempt=" << metric_attempt
                        << " pass_index=" << pass_index
                        << " reason=plausible"
                        << " msg=\"" << trim_fixed (candidate_msg) << '"';
                  });
                }
	          continue;
	        }

      std::array<int, kFt8Nn> candidate_itone {};
      if (ftx_ft8_message77_to_itone_c (message77.data (), candidate_itone.data ()) == 0)
        {
          if (trace_target)
            {
              trace_ft8_expected_target (*trace_target, "post-ldpc-reject",
                                         [&] (std::ostream& out) {
                out << " decode_pass=" << decode_pass
                    << " metric_attempt=" << metric_attempt
                    << " pass_index=" << pass_index
                    << " reason=itone"
                    << " msg=\"" << trim_fixed (candidate_msg) << '"';
              });
            }
          continue;
        }

	      if (lsubtract != 0)
	        {
	          // 1.0.298 (Quick-win A — parita' FT8 vs JTDX) — su decode profondo
	          // (ndepth>=4, stesso gate opt-in di fix #1/#2 "Deep Search") raffina il DT
	          // del segnale forte PRIMA di sottrarlo: ftx_subtract_ft8_c con lrefinedt=1
	          // fa un fit parabolico (idt -90/0/+90) per allineare il riferimento al
	          // sub-campione, lasciando un residuo piu' pulito. Cosi' i segnali
	          // SOVRAPPOSTI deboli (cluster a pochi Hz) emergono meglio nelle passate di
	          // subtract-research successive — e' la radice del gap su banda affollata,
	          // come JTDX. Costo: 3x subtract-eval solo sui forti in deep (trascurabile,
	          // protetto dal deadline per-candidato). Vedi NOTA_UPSTREAM_FT8_PARITA_JTDX.md.
	          int const refine_dt = (request.ndepth >= 3) ? 1 : 0;
	          ftx_subtract_ft8_c (dd0, candidate_itone.data (), f1, xdt, refine_dt);
	        }

	      float pass_xsnr = 0.0f;
	      if (ftx_ft8_compute_snr_c (s8.data (), 8, kFt8Nn, candidate_itone.data (),
	                                 xbase, request.nagain, nsync, &pass_xsnr) == 0)
	        {
              if (trace_target)
                {
                  trace_ft8_expected_target (*trace_target, "post-ldpc-reject",
                                             [&] (std::ostream& out) {
                    out << " decode_pass=" << decode_pass
                        << " metric_attempt=" << metric_attempt
                        << " pass_index=" << pass_index
                        << " reason=snr"
                        << " msg=\"" << trim_fixed (candidate_msg) << '"';
                  });
                }
	          continue;
	        }

      int selected_pass = 0;
      int selected_iaptype = 0;
      if (ftx_ft8_finalize_decode_pass_c (0, pass_index, pass_iaptype,
                                          &selected_pass, &selected_iaptype) == 0)
        {
          if (trace_target)
            {
              trace_ft8_expected_target (*trace_target, "post-ldpc-reject",
                                         [&] (std::ostream& out) {
                out << " decode_pass=" << decode_pass
                    << " metric_attempt=" << metric_attempt
                    << " pass_index=" << pass_index
                    << " reason=finalize"
                    << " msg=\"" << trim_fixed (candidate_msg) << '"';
              });
            }
          continue;
        }

      if (trace_target)
        {
          trace_ft8_expected_target (*trace_target, "decode-success",
                                     [&] (std::ostream& out) {
            out << " decode_pass=" << decode_pass
                << " metric_attempt=" << metric_attempt
                << " pass_index=" << pass_index
                << " selected_pass=" << selected_pass
                << " iaptype=" << selected_iaptype
                << " hard=" << pass_nharderrors
                << " dmin=" << pass_dmin
                << " snr=" << pass_xsnr
                << " msg=\"" << trim_fixed (candidate_msg) << '"'
                << " expected_match="
                << (ft8_expected_message_matches (*trace_target, trim_fixed (candidate_msg)) ? 1 : 0);
          });
        }
      nharderrors = pass_nharderrors;
      dmin = pass_dmin;
      nbadcrc = 0;
      ipass = selected_pass;
      iaptype = selected_iaptype;
      msg37 = candidate_msg;
      xsnr = pass_xsnr;
      itone = candidate_itone;
      if (signal_history_state && ft8sd_hint_is_cq (candidate_msg))
        {
          save_cq_signal_history (*signal_history_state, request, jseq, f1,
                                  callback_dt_for_history, cq_signature_score,
                                  current_cs, true);
        }
      return true;
    }
    }

  if (try_call_grid_cq_replay (call_grid_history_state, request, s8.data (),
                               nsync, sync, f1, xdt, xbase,
                               cq_signature_score, msg37, xsnr, itone,
                               message77, llra.data (), llrb.data (),
                               llrc.data (), llrd.data (), jseq))
    {
      nharderrors = kFt8StrictHardErrors;
      dmin = 0.0f;
      nbadcrc = 0;
      ipass = 0;
      iaptype = 0;
      if (signal_history_state && ft8sd_hint_is_cq (msg37))
        {
          save_cq_signal_history (*signal_history_state, request, jseq, f1,
                                  callback_dt_for_history, cq_signature_score,
                                  current_cs, true);
        }
      return true;
    }
  if (try_known_cq_call_replay (request, s8.data (), nsync, f1, xdt, xbase,
                                cq_replay_score, msg37, xsnr, itone,
                                message77, llra.data (), llrb.data (),
                                llrc.data (), llrd.data ()))
    {
      nharderrors = kFt8StrictHardErrors;
      dmin = 0.0f;
      nbadcrc = 0;
      ipass = 0;
      iaptype = 0;
      if (signal_history_state && ft8sd_hint_is_cq (msg37))
        {
          save_cq_signal_history (*signal_history_state, request, jseq, f1,
                                  callback_dt_for_history, cq_signature_score,
                                  current_cs, true);
        }
      return true;
    }
  std::array<Ft8KnownCqReplayMatch, 12> known_cq_matches {};
  int const known_cq_count =
      collect_known_call_grid_cq_matches (known_call_grid_history (), request, f1,
                                          callback_dt_for_history,
                                          cq_signature_score, nsync,
                                          known_cq_matches,
                                          locked_known_cq_candidate);
  if (known_cq_count > 0)
    {
      bool const history_replay = ft8_history_replay_enabled (request);
      int const known_hard_limit = history_replay
          ? (nsync >= 5 ? 84 : 92)
          : (nsync >= 5 ? 78 : 82);
      float const known_dmin_limit = history_replay
          ? (nsync >= 5 ? 210.0f : 245.0f)
          : (nsync >= 5 ? 180.0f : 200.0f);
      bool known_cq_ok = false;
      for (int index = 0; index < known_cq_count && !known_cq_ok; ++index)
        {
          FixedChars<kFt8DecodedChars> const cq_message =
              known_call_grid_cq_message (*known_cq_matches[static_cast<size_t> (index)].entry);
          known_cq_ok =
              try_ft8sd_known_message (cq_message, request, s8.data (), nsync, xbase,
                                       msg37, xsnr, itone, message77,
                                       llra.data (), llrb.data (), llrc.data (),
                                       llrd.data (), known_hard_limit,
                                       known_dmin_limit);
        }
      if (known_cq_ok)
        {
          nharderrors = kFt8StrictHardErrors;
          dmin = 0.0f;
          nbadcrc = 0;
          ipass = 0;
          iaptype = 0;
          if (signal_history_state && ft8sd_hint_is_cq (msg37))
            {
              save_cq_signal_history (*signal_history_state, request, jseq, f1,
                                      callback_dt_for_history, cq_signature_score,
                                      current_cs, true);
            }
          return true;
        }
    }
  if (try_ft8sd_repeated_hint (sd_hints, request, s8.data (), nsync, f1, xdt,
                               xbase, msg37, xsnr, itone, message77,
                               llra.data (), llrb.data (), llrc.data (),
                               llrd.data ()))
    {
      nharderrors = kFt8StrictHardErrors;
      dmin = 0.0f;
      nbadcrc = 0;
      ipass = 0;
      iaptype = 0;
      if (signal_history_state && ft8sd_hint_is_cq (msg37))
        {
          save_cq_signal_history (*signal_history_state, request, jseq, f1,
                                  callback_dt_for_history, cq_signature_score,
                                  current_cs, true);
        }
      return true;
    }
  if (decode_pass == 1
      && sync >= 1.45f
      && candidate_cq_flag >= 1.5f
      && try_reverse_terminal_pair_rescue (
          request, jseq, s8.data (), nsync, xbase, msg37, xsnr, itone,
          message77, llra.data (), llrb.data (), llrc.data (), llrd.data ()))
    {
      nharderrors = kFt8StrictHardErrors;
      dmin = 0.0f;
      nbadcrc = 0;
      ipass = 0;
      iaptype = 0;
      return true;
    }
  if (!equalized_metrics && signal_history_state)
    {
      save_cq_signal_history (*signal_history_state, request, jseq, f1,
                              callback_dt_for_history, cq_signature_score,
                              current_cs, false);
    }
    }

  return false;
}

extern "C" void ftx_ft8_a7_reset_c ()
{
  global_a7_history ().reset ();
}

extern "C" void ftx_ft8_a7_prepare_tables_c (int nutc, int nzhsym, int jseq)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount)
    {
      return;
    }

  Ft8A7HistoryState& history = global_a7_history ();
  prepare_a7_tables (history.a7slots, history.nutc0, nutc, nzhsym, jseq);
}

extern "C" int ftx_ft8_a7_previous_count_c (int jseq)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount)
    {
      return 0;
    }

  Ft8A7Slot const& slot = global_a7_history ().a7slots[static_cast<size_t> (jseq)];
  return std::max (0, std::min (slot.previous_count, kFt8MaxEarly));
}

extern "C" int ftx_ft8_a7_get_previous_entry_c (int jseq, int index, float* dt_out,
                                                 float* freq_out, char* msg37_out)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount || index <= 0 || !dt_out || !freq_out || !msg37_out)
    {
      return 0;
    }

  Ft8A7Slot const& slot = global_a7_history ().a7slots[static_cast<size_t> (jseq)];
  if (index > slot.previous_count || index > kFt8MaxEarly)
    {
      return 0;
    }

  Ft8A7Entry const& entry = slot.previous[static_cast<size_t> (index - 1)];
  *dt_out = entry.dt;
  *freq_out = entry.freq;
  std::copy (entry.message.begin (), entry.message.end (), msg37_out);
  return 1;
}

extern "C" void ftx_ft8_a7_save_c (int jseq, float dt, float freq, char const* msg37)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount || !msg37)
    {
      return;
    }

  save_a7_entry (global_a7_history ().a7slots, jseq, dt, freq,
                 fixed_from_chars<kFt8DecodedChars> (msg37));
  save_a7_entry (stage4_state ().a7, jseq, dt, freq,
                 fixed_from_chars<kFt8DecodedChars> (msg37));
}

extern "C" void ftx_ft8_decode_candidate_stage4_c (
    float* dd0, int* newdat, int* nQSOProgress, int* nfqso, int* nftx, int* ndepth,
    int* nzhsym, int* lapon, int* lapcqonly, int* napwid, int* lsubtract,
    int* nagain, int* ncontest, int* imetric, char const* mycall12,
    char const* hiscall12, float const* candidate_values, float const* sbase,
    int* sbase_size, int const* apsym, int const* aph10, float* sync, float* f1,
    float* xdt, float* xbase, int* nharderrors, float* dmin, int* nbadcrc,
    int* ipass, int* iaptype, char* msg37, float* xsnr, int* itone,
    signed char* message77_out)
{
  if (!dd0 || !newdat || !nQSOProgress || !nfqso || !nftx || !ndepth || !nzhsym
      || !lapon || !lapcqonly || !napwid || !lsubtract || !nagain || !ncontest
      || !imetric || !mycall12 || !hiscall12 || !candidate_values || !sbase
      || !sbase_size || !apsym || !aph10 || !sync || !f1 || !xdt || !xbase
      || !nharderrors || !dmin || !nbadcrc || !ipass || !iaptype || !msg37
      || !xsnr || !itone || !message77_out)
    {
      return;
    }

  Ft8Request request;
  request.nqsoprogress = *nQSOProgress;
  request.nfqso = *nfqso;
  request.nftx = *nftx;
  request.nutc = 0;
  request.nfa = 0;
  request.nfb = 0;
  request.nzhsym = *nzhsym;
  request.ndepth = *ndepth;
  request.emedelay = 0.0f;
  request.ncontest = *ncontest;
  request.nagain = *nagain;
  request.lft8apon = *lapon;
  request.lapcqonly = *lapcqonly;
  request.napwid = *napwid;
  request.ldiskdat = 0;
  request.ncandthin = stage4_candidate_thin ().load (std::memory_order_relaxed);
  request.nft8cycles = stage4_decode_cycles ().load (std::memory_order_relaxed);
  request.nft8rxfsens = stage4_rx_freq_sensitivity ().load (std::memory_order_relaxed);
  request.lft8lowth = stage4_low_threshold_requested ().load (std::memory_order_relaxed);
  request.lft8subpass = stage4_subpass_requested ().load (std::memory_order_relaxed);
  request.supplemental = stage4_supplemental_requested ().load (std::memory_order_relaxed);
  request.mycall = fixed_from_chars<12> (mycall12);
  request.hiscall = fixed_from_chars<12> (hiscall12);
  request.hisgrid = blank_fixed<6> ();

  std::array<int, 58> apsym_array {};
  std::array<int, 10> aph10_array {};
  std::copy_n (apsym, apsym_array.size (), apsym_array.data ());
  std::copy_n (aph10, aph10_array.size (), aph10_array.data ());

  FixedChars<kFt8DecodedChars> msg37_array = blank_fixed<kFt8DecodedChars> ();
  std::array<int, kFt8Nn> itone_array {};
  std::array<signed char, kFt8Bits> message77_array {};
  float sync_out = 0.0f;
  float f1_out = 0.0f;
  float xdt_out = 0.0f;
  float xbase_out = 0.0f;
  int nharderrors_out = -1;
  float dmin_out = 0.0f;
  int nbadcrc_out = 1;
  int ipass_out = 0;
  int iaptype_out = 0;
  float xsnr_out = 0.0f;

  decode_main_candidate_cpp (dd0, newdat, request, 1, false, false, *imetric, *lsubtract,
                             apsym_array, aph10_array, candidate_values, 0.0f, sbase,
                             *sbase_size, sync_out, f1_out, xdt_out, xbase_out,
                             nharderrors_out, dmin_out, nbadcrc_out, ipass_out,
                             iaptype_out, msg37_array, xsnr_out, itone_array,
                             message77_array, nullptr, nullptr, nullptr, -1);

  *sync = sync_out;
  *f1 = f1_out;
  *xdt = xdt_out;
  *xbase = xbase_out;
  *nharderrors = nharderrors_out;
  *dmin = dmin_out;
  *nbadcrc = nbadcrc_out;
  *ipass = ipass_out;
  *iaptype = iaptype_out;
  *xsnr = xsnr_out;
  std::copy (msg37_array.begin (), msg37_array.end (), msg37);
  std::copy (itone_array.begin (), itone_array.end (), itone);
  std::copy (message77_array.begin (), message77_array.end (), message77_out);
}

void run_fast_a7_repeated_hints (Ft8Stage4State& state, Ft8Request const& request,
                                 int jseq,
                                 std::array<float, kFt8Nh1> const& sbase,
                                 int ifa, int ifb,
                                 float const* support_candidates,
                                 int support_count,
                                 AsyncCollector& collector);

void run_main_passes (Ft8Stage4State& state, Ft8Request const& request, int jseq,
                      std::array<int, 58> const& apsym, std::array<int, 10> const& aph10,
                      int ifa, int ifb, int& ndecodes, std::array<float, kFt8Nh1>& sbase,
                      AsyncCollector& collector, bool equalized_pipeline)
{
  std::vector<float> candidate (static_cast<size_t> (4 * kFt8MaxCand), 0.0f);
  std::array<int, kFt8Nn> itone {};
  std::array<signed char, kFt8Bits> message77 {};
  int npass = 5;
  if (request.ndepth <= 2)
    {
      npass = 3;
    }
  if (request.ndepth == 1)
    {
      npass = 2;
    }
  if (request.ndepth >= 3)
    {
      if (request.nft8cycles >= 2)
        {
          npass = std::max (npass, 6);
        }
      if (request.nft8cycles >= 3)
        {
          npass = std::max (npass, 9);
        }
      if (request.lft8subpass)
        {
          npass = std::max (npass, 8);
        }
    }
  bool const wide_full_band = (ifb - ifa) >= 3500;
  if (wide_full_band && request.ndepth >= 4 && !request.lft8subpass
      && request.nft8cycles <= 1 && request.nft8rxfsens <= 1)
    {
      npass = std::min (npass, 3);
    }

  std::vector<float> dd_cycle_base;
  auto copy_state_dd_to = [&state] (std::vector<float>& target)
    {
      target.resize (state.dd.size ());
      std::copy (state.dd.begin (), state.dd.end (), target.begin ());
    };
  bool const needs_shifted_cycle_base =
      request.nzhsym >= 50 && request.ndepth >= 4 && npass >= 7;
  if (needs_shifted_cycle_base)
    {
      copy_state_dd_to (dd_cycle_base);
    }
  long long const main_start_ms = debug_ft8_focus_replay () ? steady_clock_ms () : 0;
  int subpass_zero_streak = 0;  // Fase 1a accelerazione: early-terminate del subpass

  for (int ipass = 1; ipass <= npass; ++ipass)
    {
      if (stage4_should_cancel ())
        {
          if (debug_ft8_focus_replay ())
            {
              std::cerr << "[FT8MAIN] utc=" << request.nutc
                        << " pass=" << ipass
                        << " canceled=1 before=1 ndecodes=" << ndecodes
                        << " elapsed_ms=" << (steady_clock_ms () - main_start_ms)
                        << '\n';
            }
          return;
        }
      int const pass_start_decodes = ndecodes;
      long long const pass_start_ms = debug_ft8_focus_replay () ? steady_clock_ms () : 0;
      std::vector<float> dd_before_shift;
      bool shifted_pass = false;
      if (request.nzhsym >= 50 && request.ndepth >= 4 && (ipass == 4 || ipass == 7))
        {
          copy_state_dd_to (dd_before_shift);
          float const* shift_source =
              needs_shifted_cycle_base ? dd_cycle_base.data () : dd_before_shift.data ();
          if (ipass == 7)
            {
              state.dd[0] = shift_source[0];
              for (int i = 1; i < kFt8NMax; ++i)
                {
                  state.dd[static_cast<size_t> (i)] =
                      0.5f * (shift_source[i - 1] + shift_source[i]);
                }
            }
          else
            {
              for (int i = 0; i < kFt8NMax - 1; ++i)
                {
                  state.dd[static_cast<size_t> (i)] =
                      0.5f * (shift_source[i] + shift_source[i + 1]);
                }
            }
          shifted_pass = true;
        }
      float syncmin = 0.0f;
      int imetric = 0;
      int lsubtract = 0;
      int run_pass = 0;
      ftx_ft8_prepare_pass_c (request.ndepth, ipass, ndecodes,
                              &syncmin, &imetric, &lsubtract, &run_pass);
      if (request.lft8lowth)
        {
          float threshold_scale = request.lft8subpass ? 0.82f : 0.90f;
          if (request.nft8rxfsens >= 3)
            {
              threshold_scale *= 0.95f;
            }
          if (ipass >= 4)
            {
              threshold_scale *= request.lft8subpass ? 0.90f : 0.95f;
            }
          syncmin *= threshold_scale;
        }
      syncmin *= ft8_syncmin_scale_override ();
      // Traccia diagnostica: soglia effettiva e candidati trovati, per capire
      // se abbassarla fa davvero entrare piu' segnali. DECODIUM_FT8_CAND_LOG=1.
      static bool const cand_log = [] {
        char const* raw = std::getenv ("DECODIUM_FT8_CAND_LOG");
        return raw && raw[0] != '0' && raw[0] != 0;
      }();
      if (run_pass == 0)
        {
          if (shifted_pass)
            {
              std::copy (dd_before_shift.begin (), dd_before_shift.end (), state.dd.begin ());
            }
          continue;
        }

      int ncand = 0;
      ftx_sync8_search_stage4_c (state.dd.data (), kFt8NMax,
                                 static_cast<float> (ifa), static_cast<float> (ifb),
                                 syncmin, static_cast<float> (request.nfqso),
                                 ft8_main_candidate_budget (request, ifa, ifb, ipass),
                                 ipass, request.ncandthin, candidate.data (), &ncand,
                                 sbase.data ());
      if (cand_log)
        {
          std::fprintf (stderr, "[CAND] ipass=%d syncmin=%.3f ncand=%d\n",
                        ipass, static_cast<double> (syncmin), ncand);
          std::fflush (stderr);
        }
      append_ft8_hint_candidates (&state.a7[static_cast<size_t> (jseq)], request,
                                  ifa, ifb, candidate.data (), ncand);
      append_known_cq_candidates (known_call_grid_history (), request, ifa, ifb,
                                  candidate.data (), ncand);
      if (ipass == 1
          && request.nzhsym >= 50
          && request.ndepth >= 3
          && request.lft8apon != 0
          && !request.supplemental
          && !equalized_pipeline)
        {
          run_fast_a7_repeated_hints (state, request, jseq, sbase,
                                      ifa, ifb, candidate.data (), ncand,
                                      collector);
          if (stage4_should_cancel ())
            {
              return;
            }
        }

      std::vector<int> freqpart_order;
      std::vector<int> freqpart_binid;
      if (ft8_freqpart_bins () > 0 && ncand > 1)
        {
          int const fp_bins = ft8_freqpart_bins ();
          int const fp_span = std::max (1, ifb - ifa);
          freqpart_order.reserve (static_cast<size_t> (ncand));
          freqpart_binid.reserve (static_cast<size_t> (ncand));
          for (int fp_b = 0; fp_b < fp_bins; ++fp_b)
            for (int fp_i = 0; fp_i < ncand; ++fp_i)
              {
                int fp_bin = static_cast<int> ((candidate[static_cast<size_t> (fp_i * 4)] - ifa) * fp_bins / fp_span);
                fp_bin = std::max (0, std::min (fp_bins - 1, fp_bin));
                if (fp_bin == fp_b) { freqpart_order.push_back (fp_i); freqpart_binid.push_back (fp_b); }
              }
        }
      bool const fp_isolate = ft8_freqpart_isolate () && !freqpart_order.empty () && !shifted_pass;
      trace_ft8_expected_candidate_list (request.nutc, ipass, ifa, ifb, syncmin,
                                         imetric, lsubtract, run_pass,
                                         candidate.data (), ncand, shifted_pass,
                                         fp_isolate,
                                         fp_isolate ? ft8_freqpart_bins () : 0);
      std::vector<float> fp_dd_snapshot, fp_dd_accum;
      if (fp_isolate)
        {
          copy_state_dd_to (fp_dd_snapshot);
          fp_dd_accum = fp_dd_snapshot;
        }
      int const fp_nbins = fp_isolate ? ft8_freqpart_bins () : 1;
      std::vector<std::vector<float>> fp_dd_bins (static_cast<size_t> (fp_isolate ? fp_nbins : 0));
      std::mutex fp_commit_mtx;
      auto fp_process_bin = [&] (int fp_b)
        {
          std::vector<float> fp_dd_bin;
          if (fp_isolate) fp_dd_bin = fp_dd_snapshot;
          float* const fp_dd = fp_isolate ? fp_dd_bin.data () : state.dd.data ();
          int pass_newdat = 1;
          Ft8CqSignalHistoryState fp_cq_hist;
          Ft8CallGridHistoryState fp_cg_hist;
          Ft8CqSignalHistoryState& fp_cq_ref = fp_isolate ? fp_cq_hist : cq_signal_history ();
          Ft8CallGridHistoryState& fp_cg_ref = fp_isolate ? fp_cg_hist : call_grid_history ();
          std::array<Ft8A7Slot, kFt8SequenceCount> fp_a7_arr = state.a7;
          std::array<Ft8A7Slot, kFt8SequenceCount>& fp_a7_ref = fp_isolate ? fp_a7_arr : state.a7;
          int subtract_rescue_used = 0;
          int cq_companion_rescue_used = 0;
          std::unique_ptr<Ft8KnownCallGridState> fp_known_cg;
          if (fp_isolate)
            {
              fp_known_cg = std::make_unique<Ft8KnownCallGridState> (known_call_grid_history ());
              known_call_grid_override () = fp_known_cg.get ();
            }
          struct KnownCgOverrideGuard
          {
            bool active;
            ~KnownCgOverrideGuard () { if (active) { known_call_grid_override () = nullptr; } }
          } fp_known_cg_guard {fp_isolate};
          for (int fp_k = 0; fp_k < ncand; ++fp_k)
            {
              int const icand = freqpart_order.empty () ? fp_k : freqpart_order[static_cast<size_t> (fp_k)];
              if (fp_isolate && freqpart_binid[static_cast<size_t> (fp_k)] != fp_b) continue;
          if (stage4_should_cancel ())
            {
              if (shifted_pass)
                {
                  std::copy (dd_before_shift.begin (), dd_before_shift.end (), state.dd.begin ());
                }
              if (debug_ft8_focus_replay ())
                {
                  std::cerr << "[FT8MAIN] utc=" << request.nutc
                            << " pass=" << ipass
                            << " canceled=1 icand=" << icand
                            << " ncand=" << ncand
                            << " added=" << (ndecodes - pass_start_decodes)
                            << " ndecodes=" << ndecodes
                            << " elapsed_ms=" << (steady_clock_ms () - main_start_ms)
                            << '\n';
                }
              return;
            }
          float sync = 0.0f;
          float f1 = 0.0f;
          float xdt = 0.0f;
          float xbase = 0.0f;
          float dmin = 0.0f;
          float xsnr = 0.0f;
          int nharderrors = -1;
          int nbadcrc = 1;
          int candidate_pass = 0;
          int iaptype = 0;
          FixedChars<kFt8DecodedChars> msg37 = blank_fixed<kFt8DecodedChars> ();

          bool const locked_known_cq_candidate =
              candidate[static_cast<size_t> (icand * 4 + 3)] >= 2.9f;
          Ft8Request targeted_candidate_request {};
          Ft8Request const* active_candidate_request = &request;
          if (locked_known_cq_candidate && ft8_targeted_low_subpass_enabled (request))
            {
              targeted_candidate_request = ft8_targeted_low_subpass_request (request);
              active_candidate_request = &targeted_candidate_request;
            }
          bool const use_var_downsample =
              active_candidate_request->lft8lowth
              || active_candidate_request->lft8subpass
              || active_candidate_request->nft8cycles > 1
              || active_candidate_request->nft8rxfsens > 1;
          decode_main_candidate_cpp (fp_dd, &pass_newdat,
                                     *active_candidate_request, ipass,
                                     use_var_downsample, equalized_pipeline, imetric, lsubtract, apsym, aph10,
                                     candidate.data () + icand * 4,
                                     candidate[static_cast<size_t> (icand * 4 + 3)],
                                     sbase.data (),
                                     kFt8Nh1, sync, f1, xdt, xbase, nharderrors,
                                     dmin, nbadcrc, candidate_pass, iaptype,
                                     msg37, xsnr, itone, message77,
                                     &fp_a7_ref[static_cast<size_t> (jseq)],
                                     &fp_cq_ref, &fp_cg_ref, jseq);
          if (stage4_should_cancel ())
            {
              if (shifted_pass)
                {
                  std::copy (dd_before_shift.begin (), dd_before_shift.end (), state.dd.begin ());
                }
              return;
            }

          xdt -= 0.5f;

          int nsnr = 0;
          float callback_dt = 0.0f;
          float qual = 0.0f;
          ftx_ft8_finalize_main_result_c (xsnr, xdt, request.emedelay, nharderrors,
                                          dmin, &nsnr, &callback_dt, &qual);
          Ft8ExpectedTarget const* const attempt_target =
              find_ft8_expected_target (request.nutc, f1, callback_dt)
              ? find_ft8_expected_target (request.nutc, f1, callback_dt)
              : find_ft8_expected_target (
                    request.nutc,
                    candidate[static_cast<size_t> (icand * 4 + 0)],
                    candidate[static_cast<size_t> (icand * 4 + 1)] - 0.5f);
          if (attempt_target)
            {
              trace_ft8_expected_target (*attempt_target, "candidate-attempt-exit",
                                         [&] (std::ostream& out) {
                out << " pass=" << ipass
                    << " icand=" << icand
                    << " ncand=" << ncand
                    << " f1=" << f1
                    << " callback_dt=" << callback_dt
                    << " nsnr=" << nsnr
                    << " qual=" << qual
                    << " nbadcrc=" << nbadcrc
                    << " hard=" << nharderrors
                    << " dmin=" << dmin
                    << " candidate_pass=" << candidate_pass
                    << " iaptype=" << iaptype
                    << " msg=\"" << trim_fixed (msg37) << '"'
                    << " expected_match="
                    << (ft8_expected_message_matches (*attempt_target, trim_fixed (msg37)) ? 1 : 0);
              });
            }

          // Seconda passata di recupero col decoder classico.
          //
          // I due decoder non si battono sempre allo stesso modo, misurato su
          // registrazioni off-air il 29/08/2026, 19 slot per banda:
          //   40 m, banda piena : fastldpc 250 messaggi distinti, classico 198.
          //     Il classico impiega ~16 s per slot contro una scadenza di 8,
          //     viene troncato a meta' della lista dei candidati e perde cio'
          //     che non ha raggiunto.
          //   80 m, banda scarica : fastldpc 52, classico 56. Qui il tempo
          //     basta a entrambi, e la propagazione ESATTA del classico batte
          //     l'approssimazione min-sum sui segnali marginali. Le quattro in
          //     piu' erano stazioni vere (DO8JB/YU1LD, PE1NAO/M7XRI,
          //     RA3VME/CT3MD, W3UCA/DA6IT), e il classico non perdeva nulla di
          //     quanto trovava fastldpc: era un sovrainsieme.
          //
          // Non c'e' un criterio a priori per scegliere: dipende da quante
          // stazioni ci sono da trovare, e non si sa prima di cercarle. Il
          // numero di candidati non discrimina (mediana 717 in 40 m contro 751
          // in 80 m: la banda scarica ne produce di piu', perche' il sync
          // aggancia rumore).
          //
          // Quindi si prendono entrambi: fastldpc arriva in fondo alla lista e
          // garantisce di non perdere nulla per scadenza, e sul candidato che
          // non ha dato nulla si spende il tempo risparmiato per un secondo
          // tentativo col BP esatto. Il tetto per ciclo evita che una banda
          // piena di candidati sterili consumi il margine.
          if (nbadcrc != 0
              && ft8_use_fastldpc ()
              && ft8_classic_rescue_budget () > 0
              && ft8_classic_rescue_used () < ft8_classic_rescue_budget ()
              && stage4_remaining_ms () >= 1200
              && !stage4_should_cancel ())
            {
              ++ft8_classic_rescue_used ();
              g_ft8_forza_classico = true;
              decode_main_candidate_cpp (fp_dd, &pass_newdat,
                                         *active_candidate_request, ipass,
                                         use_var_downsample, equalized_pipeline, imetric, lsubtract, apsym, aph10,
                                         candidate.data () + icand * 4,
                                         candidate[static_cast<size_t> (icand * 4 + 3)],
                                         sbase.data (),
                                         kFt8Nh1, sync, f1, xdt, xbase, nharderrors,
                                         dmin, nbadcrc, candidate_pass, iaptype,
                                         msg37, xsnr, itone, message77,
                                         &fp_a7_ref[static_cast<size_t> (jseq)],
                                         &fp_cq_ref, &fp_cg_ref, jseq);
              g_ft8_forza_classico = false;
              if (nbadcrc == 0)
                {
                  xdt -= 0.5f;
                  ftx_ft8_finalize_main_result_c (xsnr, xdt, request.emedelay, nharderrors,
                                                  dmin, &nsnr, &callback_dt, &qual);
                }
            }

          bool const can_retry_cq_companion =
              nbadcrc != 0
              && cq_companion_rescue_used < 8
              && ipass <= 3
              && request.nzhsym >= 50
              && request.ndepth >= 3
              && !request.supplemental
              && request.lft8apon != 0
              && request.ncontest == 0
              && trim_fixed (request.hiscall).empty ()
              && candidate[static_cast<size_t> (icand * 4 + 2)] >= (ipass >= 3 ? 1.6f : 2.5f)
              && candidate[static_cast<size_t> (icand * 4 + 3)] >= 1.75f;
          if (can_retry_cq_companion)
            {
              for (float const offset : {3.125f, -3.125f})
                {
                  float companion_values[4] {
                    candidate[static_cast<size_t> (icand * 4 + 0)] + offset,
                    candidate[static_cast<size_t> (icand * 4 + 1)],
                    candidate[static_cast<size_t> (icand * 4 + 2)] * 0.92f,
                    candidate[static_cast<size_t> (icand * 4 + 3)]
                  };
                  if (companion_values[0] < static_cast<float> (ifa)
                      || companion_values[0] > static_cast<float> (ifb))
                    {
                      continue;
                    }
                  float companion_sync = 0.0f;
                  float companion_f1 = 0.0f;
                  float companion_xdt = 0.0f;
                  float companion_xbase = 0.0f;
                  float companion_dmin = 0.0f;
                  float companion_xsnr = 0.0f;
                  int companion_nharderrors = -1;
                  int companion_nbadcrc = 1;
                  int companion_pass = 0;
                  int companion_iaptype = 0;
                  FixedChars<kFt8DecodedChars> companion_msg37 =
                      blank_fixed<kFt8DecodedChars> ();
                  std::array<int, kFt8Nn> companion_itone {};
                  std::array<signed char, kFt8Bits> companion_message77 {};
                  int companion_newdat = pass_newdat;
                  decode_main_candidate_cpp (
                      fp_dd, &companion_newdat,
                      *active_candidate_request, ipass,
                      use_var_downsample, equalized_pipeline, imetric, lsubtract,
                      apsym, aph10, companion_values,
                      companion_values[3], sbase.data (), kFt8Nh1,
                      companion_sync, companion_f1, companion_xdt,
                      companion_xbase, companion_nharderrors, companion_dmin,
                      companion_nbadcrc, companion_pass, companion_iaptype,
                      companion_msg37, companion_xsnr, companion_itone,
                      companion_message77, &fp_a7_ref[static_cast<size_t> (jseq)],
                      &fp_cq_ref, &fp_cg_ref, jseq);
                  if (stage4_should_cancel ())
                    {
                      if (shifted_pass)
                        {
                          std::copy (dd_before_shift.begin (), dd_before_shift.end (),
                                     state.dd.begin ());
                        }
                      return;
                    }
                  if (companion_nbadcrc != 0)
                    {
                      continue;
                    }
                  companion_xdt -= 0.5f;
                  ftx_ft8_finalize_main_result_c (
                      companion_xsnr, companion_xdt, request.emedelay,
                      companion_nharderrors, companion_dmin, &nsnr,
                      &callback_dt, &qual);
                  sync = companion_sync;
                  f1 = companion_f1;
                  xdt = companion_xdt;
                  xbase = companion_xbase;
                  dmin = companion_dmin;
                  xsnr = companion_xsnr;
                  nharderrors = companion_nharderrors;
                  nbadcrc = companion_nbadcrc;
                  candidate_pass = companion_pass;
                  iaptype = companion_iaptype;
                  msg37 = companion_msg37;
                  itone = companion_itone;
                  message77 = companion_message77;
                  ++cq_companion_rescue_used;
                  break;
                }
            }

          if (nbadcrc == 0)
            {
              msg37 = normalize_resolved_hash_call_tokens (msg37);
              bool const replay_decode = candidate_pass == 0 && iaptype == 0;
              bool const replay_call_only_cq =
                  replay_decode && is_cq_call_only_message (msg37);
              if (!replay_call_only_cq && ft8sd_hint_is_cq (msg37) && nsnr <= -24)
                {
                  continue;
                }
              bool duplicate = false;
              {
                std::lock_guard<std::mutex> fp_lk (fp_commit_mtx);
              for (int id = 0; id < ndecodes; ++id)
                {
                  if (messages_equal (msg37, state.allmessages[static_cast<size_t> (id)]))
                    {
                      duplicate = true;
                      break;
                    }
                }

		              if (!duplicate)
		                {
			                  int const saved_slot =
			                      ftx_ft8_store_saved_decode_c (ndecodes, kFt8MaxEarly, nsnr, f1, xdt + 0.5f,
			                                                   itone.data (), kFt8Nn, state.allsnrs.data (),
                                                   state.f1_save.data (), state.xdt_save.data (),
                                                   state.itone_save.data ());
                  if (saved_slot == 0)
                    {
                      continue;
                    }
                  ndecodes = saved_slot;
                  state.allmessages[static_cast<size_t> (ndecodes - 1)] = msg37;
                }
              }

              if (!duplicate)
                {
			                  { std::lock_guard<std::mutex> fp_lk (fp_commit_mtx); collector.append (sync, nsnr, callback_dt, f1, msg37, iaptype, qual,
			                                    message77.data ()); }
                  if (!replay_decode)
	                    {
		                    save_a7_entry (fp_a7_ref, jseq, callback_dt, f1, msg37);
                      save_call_grid_history (fp_cg_ref, request, jseq,
                                              callback_dt, f1, msg37);
                      seed_pack77_hashes_from_message (msg37);
                    }

                  bool const can_rescue_after_subtract =
                      lsubtract != 0
                      && request.ndepth >= 3
                      && request.nzhsym >= 50
                      && subtract_rescue_used < 24;
                  if (can_rescue_after_subtract)
                    {
                      Ft8Request targeted_rescue_request {};
                      Ft8Request const* active_rescue_request = &request;
                      if (ft8_targeted_low_subpass_enabled (request))
                        {
                          targeted_rescue_request = ft8_targeted_low_subpass_request (request);
                          active_rescue_request = &targeted_rescue_request;
                        }
                      apply_pack77_hash_external_seed_cache ();
                      std::array<float, 4 * 96> rescue_candidate {};
                      std::array<float, kFt8Nh1> rescue_sbase {};
                      int rescue_count = 0;
                      int local_crowded_candidates = 0;
                      float local_best_cq = 0.0f;
                      for (int local_index = 0; local_index < ncand; ++local_index)
                        {
                          float const local_freq =
                              candidate[static_cast<size_t> (local_index * 4 + 0)];
                          float const local_delta = std::fabs (local_freq - f1);
                          if (local_delta <= 2.0f || local_delta > 45.0f)
                            {
                              continue;
                            }
                          float const local_sync =
                              candidate[static_cast<size_t> (local_index * 4 + 2)];
                          float const local_cq =
                              candidate[static_cast<size_t> (local_index * 4 + 3)];
                          if (local_sync >= 1.0f || local_cq >= 1.5f)
                            {
                              ++local_crowded_candidates;
                              local_best_cq = std::max (local_best_cq, local_cq);
                            }
                        }
                      bool const use_late_cluster_rescue =
                          ipass == 1
                          && request.nzhsym >= 50
                          && request.ndepth >= 3
                          && !request.supplemental
                          && (local_crowded_candidates >= 3 || local_best_cq >= 2.0f);
                      int const rescue_decode_pass =
                          use_late_cluster_rescue ? std::min (5, npass)
                                                  : std::min (ipass + 1, npass);
                      float rescue_syncmin = 0.0f;
                      int rescue_imetric = 0;
                      int rescue_lsubtract = 0;
                      int rescue_run_pass = 0;
                      ftx_ft8_prepare_pass_c (request.ndepth, rescue_decode_pass, ndecodes,
                                              &rescue_syncmin, &rescue_imetric,
                                              &rescue_lsubtract, &rescue_run_pass);
                      if (active_rescue_request->lft8lowth)
                        {
                          float threshold_scale =
                              active_rescue_request->lft8subpass ? 0.82f : 0.90f;
                          if (active_rescue_request->nft8rxfsens >= 3)
                            {
                              threshold_scale *= 0.95f;
                            }
                          if (rescue_decode_pass >= 4)
                            {
                              threshold_scale *=
                                  active_rescue_request->lft8subpass ? 0.90f : 0.95f;
                            }
                          rescue_syncmin *= threshold_scale;
                        }
                      int const rescue_ifa =
                          std::max (ifa, static_cast<int> (std::floor (f1 - 45.0f)));
                      int const rescue_ifb =
                          std::min (ifb, static_cast<int> (std::ceil (f1 + 45.0f)));
                      if (rescue_run_pass != 0 && rescue_ifb > rescue_ifa)
                        {
                          ftx_sync8_search_stage4_c (
                              fp_dd, kFt8NMax,
                              static_cast<float> (rescue_ifa),
                              static_cast<float> (rescue_ifb), rescue_syncmin,
                              static_cast<float> (request.nfqso), 96, rescue_decode_pass,
                              request.ncandthin, rescue_candidate.data (), &rescue_count,
                              rescue_sbase.data ());
                        }

                      int const rescue_candidate_limit =
                          use_late_cluster_rescue ? 4 : 8;
                      int const rescue_limit =
                          std::min ({rescue_count, rescue_candidate_limit,
                                      24 - subtract_rescue_used});
                      for (int rescue_index = 0; rescue_index < rescue_limit; ++rescue_index)
                        {
                          if (stage4_should_cancel ())
                            {
                              if (shifted_pass)
                                {
                                  std::copy (dd_before_shift.begin (), dd_before_shift.end (),
                                             state.dd.begin ());
                                }
                              if (debug_ft8_focus_replay ())
                                {
                                  std::cerr << "[FT8MAIN] utc=" << request.nutc
                                            << " pass=" << ipass
                                            << " canceled=1 rescue=1"
                                            << " rescue_index=" << rescue_index
                                            << " ncand=" << ncand
                                            << " added=" << (ndecodes - pass_start_decodes)
                                            << " ndecodes=" << ndecodes
                                            << " elapsed_ms="
                                            << (steady_clock_ms () - main_start_ms)
                                            << '\n';
                                }
                              return;
                            }
	                          float const rescue_freq =
	                              rescue_candidate[static_cast<size_t> (rescue_index * 4 + 0)];
	                          if (std::fabs (rescue_freq - f1) <= 2.0f)
	                            {
	                              continue;
	                            }

                          float rescue_sync = 0.0f;
                          float rescue_f1 = 0.0f;
                          float rescue_xdt = 0.0f;
                          float rescue_xbase = 0.0f;
                          float rescue_dmin = 0.0f;
                          float rescue_xsnr = 0.0f;
                          int rescue_nharderrors = -1;
                          int rescue_nbadcrc = 1;
                          int rescue_candidate_pass = 0;
                          int rescue_iaptype = 0;
                          FixedChars<kFt8DecodedChars> rescue_msg37 =
                              blank_fixed<kFt8DecodedChars> ();
                          std::array<int, kFt8Nn> rescue_itone {};
                          std::array<signed char, kFt8Bits> rescue_message77 {};
                          int rescue_newdat = 1;
                          bool const rescue_use_var_downsample =
                              active_rescue_request->lft8lowth
                              || active_rescue_request->lft8subpass
                              || active_rescue_request->nft8cycles > 1
                              || active_rescue_request->nft8rxfsens > 1;
                          decode_main_candidate_cpp (
                              fp_dd, &rescue_newdat, *active_rescue_request,
                              rescue_decode_pass, rescue_use_var_downsample,
                              equalized_pipeline, rescue_imetric,
                              rescue_lsubtract,
                              apsym, aph10,
                              rescue_candidate.data () + rescue_index * 4,
                              rescue_candidate[static_cast<size_t> (rescue_index * 4 + 3)],
                              rescue_sbase.data (), kFt8Nh1, rescue_sync, rescue_f1,
                              rescue_xdt, rescue_xbase, rescue_nharderrors,
                              rescue_dmin, rescue_nbadcrc, rescue_candidate_pass,
                              rescue_iaptype, rescue_msg37, rescue_xsnr,
                              rescue_itone, rescue_message77,
                              &fp_a7_ref[static_cast<size_t> (jseq)],
                              &fp_cq_ref, &fp_cg_ref, jseq);
                          if (rescue_nbadcrc != 0)
                            {
                              continue;
                            }

                          rescue_xdt -= 0.5f;
                          int rescue_nsnr = 0;
                          float rescue_callback_dt = 0.0f;
                          float rescue_qual = 0.0f;
                          ftx_ft8_finalize_main_result_c (
                              rescue_xsnr, rescue_xdt, request.emedelay,
                              rescue_nharderrors, rescue_dmin, &rescue_nsnr,
                              &rescue_callback_dt, &rescue_qual);
                          rescue_msg37 = normalize_resolved_hash_call_tokens (rescue_msg37);
                          bool const rescue_replay_decode =
                              rescue_candidate_pass == 0 && rescue_iaptype == 0;
                          bool const rescue_replay_call_only_cq =
                              rescue_replay_decode
                              && is_cq_call_only_message (rescue_msg37);
                          if (!rescue_replay_call_only_cq
                              && ft8sd_hint_is_cq (rescue_msg37)
                              && rescue_nsnr <= -24)
                            {
                              continue;
                            }

                          bool rescue_duplicate = false;
                          {
                            std::lock_guard<std::mutex> fp_lk (fp_commit_mtx);
                          for (int id = 0; id < ndecodes; ++id)
                            {
                              if (messages_equal (
                                      rescue_msg37,
                                      state.allmessages[static_cast<size_t> (id)]))
                                {
                                  rescue_duplicate = true;
                                  break;
                                }
                            }
                          if (rescue_duplicate)
                            {
                              continue;
                            }

                          int const rescue_saved_slot =
                              ftx_ft8_store_saved_decode_c (
                                  ndecodes, kFt8MaxEarly, rescue_nsnr,
                                  rescue_f1, rescue_xdt + 0.5f, rescue_itone.data (),
                                  kFt8Nn, state.allsnrs.data (), state.f1_save.data (),
                                  state.xdt_save.data (), state.itone_save.data ());
                          if (rescue_saved_slot == 0)
                            {
                              continue;
                            }
                          ndecodes = rescue_saved_slot;
                          state.allmessages[static_cast<size_t> (ndecodes - 1)] =
                              rescue_msg37;
                          }
                          { std::lock_guard<std::mutex> fp_lk (fp_commit_mtx); collector.append (rescue_sync, rescue_nsnr,
                                            rescue_callback_dt, rescue_f1,
                                            rescue_msg37, rescue_iaptype, rescue_qual,
                                            rescue_message77.data ()); }
                          if (!rescue_replay_decode)
                            {
                              save_a7_entry (fp_a7_ref, jseq, rescue_callback_dt,
                                             rescue_f1, rescue_msg37);
                              save_call_grid_history (fp_cg_ref, request, jseq,
                                                      rescue_callback_dt, rescue_f1,
                                                      rescue_msg37);
                              seed_pack77_hashes_from_message (rescue_msg37);
                            }
	                          ++subtract_rescue_used;
	                        }
	                    }
	                }
            }

          if (request.nzhsym == 41
              && ftx_ft8_should_bail_by_tseq_c (request.ldiskdat, current_sequence_seconds (), 13.4) != 0)
            {
              if (shifted_pass)
                {
                  std::copy (dd_before_shift.begin (), dd_before_shift.end (), state.dd.begin ());
                }
              return;
            }
        }
          if (fp_isolate)
            {
              fp_dd_bins[static_cast<size_t> (fp_b)] = std::move (fp_dd_bin);
            }
        };
      [[maybe_unused]] int const fp_threads = std::max (1, ft8_thread_budget ().load ());
#pragma omp parallel for schedule (dynamic) if (fp_isolate) num_threads (fp_threads)
      for (int fp_b = 0; fp_b < fp_nbins; ++fp_b)
        fp_process_bin (fp_b);
      if (fp_isolate)
        {
          freqpart_bins_used ().store (fp_nbins, std::memory_order_relaxed);
          for (int fp_b = 0; fp_b < fp_nbins; ++fp_b)
            {
              std::vector<float> const& fp_bin = fp_dd_bins[static_cast<size_t> (fp_b)];
              if (fp_bin.size () != fp_dd_accum.size ()) continue;
              for (size_t fp_s = 0; fp_s < fp_dd_accum.size (); ++fp_s)
                fp_dd_accum[fp_s] += fp_bin[fp_s] - fp_dd_snapshot[fp_s];
            }
          std::copy (fp_dd_accum.begin (), fp_dd_accum.end (), state.dd.begin ());
        }
      if (shifted_pass)
        {
          std::copy (dd_before_shift.begin (), dd_before_shift.end (), state.dd.begin ());
        }
      if (debug_ft8_focus_replay ())
        {
          std::cerr << "[FT8MAIN] utc=" << request.nutc
                    << " pass=" << ipass
                    << " ncand=" << ncand
                    << " added=" << (ndecodes - pass_start_decodes)
                    << " ndecodes=" << ndecodes
                    << " elapsed_ms=" << (steady_clock_ms () - main_start_ms)
	                    << " pass_ms=" << (steady_clock_ms () - pass_start_ms)
	                    << '\n';
	        }
      // Fase 1a (accelerazione): early-terminate del subpass a yield esaurito.
      // Profiling: ~94% dei decode nei primi 2 pass; pass 3-8 = ~74% tempo per ~6% yield.
      // Stop dopo 4 pass consecutivi a zero-add (era 2): i deboli arrivano a pass 5+
      // dopo zeri a 3-4; con freqpart il dig piu' lungo resta nel budget. SOLO subpass.
      if (request.lft8subpass)
        {
          if (ndecodes - pass_start_decodes == 0) ++subpass_zero_streak;
          else subpass_zero_streak = 0;
          if (subpass_zero_streak >= 4)
            {
              if (debug_ft8_focus_replay ())
                std::cerr << "[FT8MAIN] utc=" << request.nutc
                          << " subpass_early_terminate pass=" << ipass
                          << " ndecodes=" << ndecodes
                          << " elapsed_ms=" << (steady_clock_ms () - main_start_ms) << std::endl;
              break;
            }
        }
      if (ipass == 2
          && request.nzhsym >= 50
          && !request.supplemental
          && !equalized_pipeline)
        {
          run_fast_a7_repeated_hints (state, request, jseq, sbase,
                                      ifa, ifb, candidate.data (), ncand,
                                      collector);
          if (stage4_should_cancel ())
            {
              return;
            }
        }
	    }
	}

void run_ap_passes (Ft8Stage4State& state, Ft8Request const& request, int jseq,
                    std::array<float, kFt8Nh1> const& sbase, AsyncCollector& collector)
{
  constexpr float kSyncAp = 10.0f;
  std::array<signed char, kFt8Bits> zero_bits {};

  bool la8 = true;
  Ft8A7Slot& slot = state.a7[static_cast<size_t> (jseq)];

  if (ftx_ft8_should_run_a7_c (request.lft8apon, request.ncontest,
                               request.nzhsym, slot.previous_count) != 0)
    {
      int newdat_a7 = 1;
      int previous_limit = std::min (slot.previous_count, kFt8MaxEarly);
      if (request.nzhsym < 50)
        {
          previous_limit = std::min (previous_limit, 64);
        }
      for (int i = 0; i < previous_limit; ++i)
        {
          if (slot.previous[static_cast<size_t> (i)].freq == -99.0f)
            {
              previous_limit = i;
              break;
            }
        }

      std::array<int, kFt8MaxEarly> previous_order {};
      for (int i = 0; i < previous_limit; ++i)
        {
          previous_order[static_cast<size_t> (i)] = i;
        }
      auto a7_priority = [&] (int index) {
        Ft8A7Entry const& hint = slot.previous[static_cast<size_t> (index)];
        if (!ft8_hint_matches_context (hint, request))
          {
            return 100000 + index;
          }
        FixedChars<kFt8DecodedChars> const message = ft8_a7_replay_message (hint);
        int score = std::max (0, hint.age) * 1000;
        score -= std::min (std::max (hint.hits, 0), 16) * 100;
        if (is_directed_pair_only_message (message))
          {
            score += 80;
          }
        if (ft8sd_hint_is_cq (message))
          {
            score += 40;
          }
        return score;
      };
      std::stable_sort (previous_order.begin (),
                        previous_order.begin () + previous_limit,
                        [&] (int lhs, int rhs) {
                          int const lhs_score = a7_priority (lhs);
                          int const rhs_score = a7_priority (rhs);
                          if (lhs_score != rhs_score)
                            {
                              return lhs_score < rhs_score;
                          }
                          return lhs < rhs;
                        });
      if (debug_ft8_focus_replay ())
        {
          int const debug_limit = std::min (previous_limit, 16);
          for (int debug_index = 0; debug_index < debug_limit; ++debug_index)
            {
              int const hint_index = previous_order[static_cast<size_t> (debug_index)];
              Ft8A7Entry const& hint = slot.previous[static_cast<size_t> (hint_index)];
              std::cerr << "[FT8A7] utc=" << request.nutc
                        << " order=" << debug_index
                        << " index=" << hint_index
                        << " score=" << a7_priority (hint_index)
                        << " hits=" << hint.hits
                        << " age=" << hint.age
                        << " freq=" << hint.freq
                        << " dt=" << hint.dt
                        << " msg=" << trim_fixed (ft8_a7_replay_message (hint))
                        << '\n';
            }
        }

      for (int order_index = 0; order_index < previous_limit; ++order_index)
        {
          if (stage4_should_cancel ())
            {
              return;
            }
          int const i = previous_order[static_cast<size_t> (order_index)];
          Ft8A7Entry const& previous = slot.previous[static_cast<size_t> (i)];

          float f1 = 0.0f;
          float xdt = 0.0f;
          float xbase = 0.0f;
          FixedChars<12> call_1 = blank_fixed<12> ();
          FixedChars<12> call_2 = blank_fixed<12> ();
          FixedChars<4> grid4 = blank_fixed<4> ();

          int const request_status =
              ftx_ft8_prepare_a7_request_c (previous.freq, previous.dt, previous.message.data (),
                                            sbase.data (), kFt8Nh1, &f1, &xdt, &xbase,
                                            call_1.data (), call_2.data (), grid4.data ());
          if (request_status == 2)
            {
              break;
            }
          if (request_status == 0)
            {
              continue;
            }
          if (!ft8_hint_matches_context (previous, request))
            {
              continue;
            }

          std::array<int, kFt8Nn> fast_itone {};
          std::array<signed char, kFt8Bits> fast_message77 {};
          int nharderrors = -1;
          float dmin = 0.0f;
          float xsnr = 0.0f;
          FixedChars<kFt8DecodedChars> msg37 = blank_fixed<kFt8DecodedChars> ();
          if (try_ft8_a7_fast_repeated_hint (state, request, previous,
                                             newdat_a7, sbase, msg37, xsnr,
                                             xdt, f1, fast_itone,
                                             fast_message77, false))
            {
              if (stage4_should_cancel ())
                {
                  return;
                }
              msg37 = normalize_resolved_hash_call_tokens (msg37);
              if (is_directed_pair_only_message (previous.message)
                  && is_directed_pair_only_message (msg37)
                  && trim_fixed (previous.message) == trim_fixed (msg37))
                {
                  continue;
                }
              int nsnr = 0;
              int iaptype = 0;
              float qual = 0.0f;
              ftx_ft8_finalize_a7_result_c (xsnr, &nsnr, &iaptype, &qual);
              if (ftx_ft8_should_keep_a8_after_a7_c (msg37.data (), request.hiscall.data ()) == 0)
                {
                  la8 = false;
                }
              collector.append (kSyncAp, nsnr, xdt, f1, msg37, iaptype, qual,
                                fast_message77.data ());
              save_a7_entry (state.a7, jseq, xdt, f1, msg37);
              save_call_grid_history (call_grid_history (), request, jseq, xdt, f1, msg37);
              seed_pack77_hashes_from_message (msg37);
              continue;
            }
          if (stage4_should_cancel ())
            {
              return;
            }
          ftx_ft8_a7d_c (state.dd.data (), &newdat_a7, call_1.data (), call_2.data (), grid4.data (),
                         &xdt, &f1, &xbase, &nharderrors, &dmin, msg37.data (), &xsnr);
          if (stage4_should_cancel ())
            {
              return;
            }

          if (nharderrors >= 0)
            {
              msg37 = normalize_resolved_hash_call_tokens (msg37);
              if (is_directed_pair_only_message (previous.message)
                  && is_directed_pair_only_message (msg37)
                  && trim_fixed (previous.message) == trim_fixed (msg37))
                {
                  continue;
                }
              int nsnr = 0;
              int iaptype = 0;
              float qual = 0.0f;
              ftx_ft8_finalize_a7_result_c (xsnr, &nsnr, &iaptype, &qual);
              if (ftx_ft8_should_keep_a8_after_a7_c (msg37.data (), request.hiscall.data ()) == 0)
                {
                  la8 = false;
                }
              collector.append (kSyncAp, nsnr, xdt, f1, msg37, iaptype, qual,
                                zero_bits.data ());
              save_a7_entry (state.a7, jseq, xdt, f1, msg37);
              save_call_grid_history (call_grid_history (), request, jseq, xdt, f1, msg37);
              seed_pack77_hashes_from_message (msg37);
            }
        }
    }

  if (ftx_ft8_should_run_a8_c (request.lft8apon, request.ncontest, request.nzhsym,
                               la8 ? 1 : 0,
                               trim_length (request.hiscall.data (), 12),
                               trim_length (request.hisgrid.data (), 6),
                               request.ltry_a8) == 0)
    {
      return;
    }
  if (stage4_should_cancel ())
    {
      return;
    }

  float f1 = static_cast<float> (request.nfqso);
  float xdt = 0.0f;
  float fbest = 0.0f;
  float xsnr = 0.0f;
  float plog = 0.0f;
  FixedChars<kFt8DecodedChars> msg37 = blank_fixed<kFt8DecodedChars> ();
  ftx_ft8_a8d_c (state.dd.data (), request.mycall.data (), request.hiscall.data (),
                 request.hisgrid.data (), &f1, &xdt, &fbest, &xsnr, &plog, msg37.data ());
  if (stage4_should_cancel ())
    {
      return;
    }

  if (msg37[0] == ' ')
    {
      return;
    }
  msg37 = normalize_resolved_hash_call_tokens (msg37);

  int nsnr = 0;
  int iaptype = 0;
  float qual = 0.0f;
  float save_freq = 0.0f;
  ftx_ft8_finalize_a8_result_c (plog, xsnr, fbest, &nsnr, &iaptype, &qual, &save_freq);
  collector.append (kSyncAp, nsnr, xdt, fbest, msg37, iaptype, qual, zero_bits.data ());
  save_a7_entry (state.a7, jseq, xdt, save_freq, msg37);
  save_call_grid_history (call_grid_history (), request, jseq, xdt, save_freq, msg37);
  seed_pack77_hashes_from_message (msg37);
}

void run_fast_a7_repeated_hints (Ft8Stage4State& state, Ft8Request const& request,
                                 int jseq,
                                 std::array<float, kFt8Nh1> const& sbase,
                                 int ifa, int ifb,
                                 float const* support_candidates,
                                 int support_count,
                                 AsyncCollector& collector)
{
  constexpr float kSyncAp = 10.0f;
  constexpr int kMaxFastA7Attempts = 8;

  if (debug_ft8_focus_replay ())
    {
      std::cerr << "[FT8A7FAST] utc=" << request.nutc
                << " enter=1 lft8apon=" << request.lft8apon
                << " ncontest=" << request.ncontest
                << " nzhsym=" << request.nzhsym
                << " previous_count="
                << state.a7[static_cast<size_t> (jseq)].previous_count
                << '\n';
    }

  if (ftx_ft8_should_run_a7_c (request.lft8apon, request.ncontest,
                               request.nzhsym,
                               state.a7[static_cast<size_t> (jseq)].previous_count) == 0)
    {
      return;
    }

  Ft8A7Slot& slot = state.a7[static_cast<size_t> (jseq)];
  int previous_limit = std::min (slot.previous_count, kFt8MaxEarly);
  for (int i = 0; i < previous_limit; ++i)
    {
      if (slot.previous[static_cast<size_t> (i)].freq == -99.0f)
        {
          previous_limit = i;
          break;
        }
    }

  std::array<int, kFt8MaxEarly> previous_order {};
  for (int i = 0; i < previous_limit; ++i)
    {
      previous_order[static_cast<size_t> (i)] = i;
    }
	  auto direct_report_replay_tail = [] (FixedChars<kFt8DecodedChars> const& message) {
	    std::vector<std::string> const words = split_words (trim_fixed (message));
	    return words.size () == 3
	           && words[0].find ('<') == std::string::npos
	           && words[1].find ('<') == std::string::npos
           && is_report_token (words[2])
           && words[2] != "73"
	           && words[2] != "RRR"
	           && words[2] != "RR73";
	  };
  auto local_support_score = [&] (Ft8A7Entry const& hint) {
    if (!support_candidates || support_count <= 0)
      {
        return 1.0e30f;
      }
    float best_score = 1.0e30f;
    int const limit = std::min (support_count, kFt8MaxCand);
    for (int index = 0; index < limit; ++index)
      {
        float const candidate_freq =
            support_candidates[static_cast<size_t> (index * 4 + 0)];
        float const candidate_dt =
            support_candidates[static_cast<size_t> (index * 4 + 1)];
        float const candidate_sync =
            support_candidates[static_cast<size_t> (index * 4 + 2)];
        float const candidate_cq =
            support_candidates[static_cast<size_t> (index * 4 + 3)];
        float const freq_delta = std::fabs (candidate_freq - hint.freq);
        float const dt_delta = std::fabs (candidate_dt - hint.dt);
        if (freq_delta > 5.5f || dt_delta > 0.35f
            || (candidate_sync < 0.80f && candidate_cq < 1.0f))
          {
            continue;
          }
        float const score =
            freq_delta + 10.0f * dt_delta
            - 0.12f * std::min (candidate_sync, 10.0f)
            - 0.25f * std::min (candidate_cq, 3.0f);
        best_score = std::min (best_score, score);
      }
    return best_score;
  };
  auto a7_priority = [&] (int index) {
    Ft8A7Entry const& hint = slot.previous[static_cast<size_t> (index)];
    if (!ft8_hint_matches_context (hint, request))
      {
        return 100000 + index;
      }
    FixedChars<kFt8DecodedChars> const message = ft8_a7_replay_message (hint);
    int score = std::max (0, hint.age) * 1000;
    score -= std::min (std::max (hint.hits, 0), 16) * 100;
    if (is_directed_pair_only_message (message))
      {
        score += 80;
      }
    if (ft8sd_hint_is_cq (message))
      {
        score += 40;
      }
    float const support = local_support_score (hint);
    if (support >= 1.0e20f)
      {
        score += 2000;
      }
    else
      {
        score += static_cast<int> (std::lround (support * 40.0f)) - 600;
      }
    if (!ft8sd_hint_is_cq (message)
        && !is_directed_pair_only_message (message)
        && hint.age <= 1
        && hint.hits >= 3
        && direct_report_replay_tail (message))
      {
        score -= 700;
      }
    if (support >= 1.0e20f
        && !ft8sd_hint_is_cq (message)
        && !is_directed_pair_only_message (message)
        && hint.age <= 1
        && direct_report_replay_tail (message))
      {
        score -= 1750;
      }
    return score;
  };
  std::stable_sort (previous_order.begin (), previous_order.begin () + previous_limit,
                    [&] (int lhs, int rhs) {
                      int const lhs_score = a7_priority (lhs);
                      int const rhs_score = a7_priority (rhs);
                      if (lhs_score != rhs_score)
                        {
                          return lhs_score < rhs_score;
                        }
                      return lhs < rhs;
                    });
  if (debug_ft8_focus_replay ())
    {
      int const debug_limit = std::min (previous_limit, 16);
      for (int debug_index = 0; debug_index < debug_limit; ++debug_index)
        {
          int const hint_index = previous_order[static_cast<size_t> (debug_index)];
          Ft8A7Entry const& hint = slot.previous[static_cast<size_t> (hint_index)];
          std::cerr << "[FT8A7FAST] utc=" << request.nutc
                    << " order=" << debug_index
                    << " index=" << hint_index
                    << " score=" << a7_priority (hint_index)
                    << " hits=" << hint.hits
                    << " age=" << hint.age
                    << " freq=" << hint.freq
                    << " dt=" << hint.dt
                    << " msg=" << trim_fixed (ft8_a7_replay_message (hint))
                    << '\n';
        }
    }

  int newdat_a7 = 1;
  int attempts = 0;
  for (int order_index = 0; order_index < previous_limit; ++order_index)
    {
      if (stage4_should_cancel () || attempts >= kMaxFastA7Attempts)
        {
          return;
        }
      int const hint_index = previous_order[static_cast<size_t> (order_index)];
      Ft8A7Entry const& previous = slot.previous[static_cast<size_t> (hint_index)];
      FixedChars<kFt8DecodedChars> const hint_message = ft8_a7_replay_message (previous);
      bool const pair_only = is_directed_pair_only_message (hint_message);
      bool const strict_message = is_strict_standard_ft8_message (hint_message);
      bool const cq_replay = ft8sd_hint_is_cq (hint_message);
      float const support = local_support_score (previous);
      bool const recent_supported_cq =
          cq_replay && previous.age <= 1 && support < 1.0e20f && support <= 2.5f;
      bool const recent_supported_direct =
          !cq_replay && !pair_only && previous.age <= 1
          && support < 1.0e20f
          && (support <= 2.5f
              || (previous.hits >= 2
                  && direct_report_replay_tail (hint_message)
                  && support <= 4.0f)
              || (previous.hits >= 3
                  && direct_report_replay_tail (hint_message)
                  && support <= 25.0f));
      bool const recent_unsupported_direct_report =
          !cq_replay && !pair_only && previous.age <= 1
          && support >= 1.0e20f && direct_report_replay_tail (hint_message);
      int const min_hits = cq_replay ? (recent_supported_cq ? 1 : 3)
                                     : ((recent_supported_direct
                                         || recent_unsupported_direct_report)
                                            ? 1 : 6);
      if (previous.freq < static_cast<float> (ifa) - 4.0f
          || previous.freq > static_cast<float> (ifb) + 4.0f
          || previous.hits < min_hits
          || pair_only
          || !strict_message
          || !ft8_hint_matches_context (previous, request))
        {
          continue;
        }

      float f1 = 0.0f;
      float xdt = 0.0f;
      float xsnr = 0.0f;
      FixedChars<kFt8DecodedChars> msg37 = blank_fixed<kFt8DecodedChars> ();
      ++attempts;
      std::array<int, kFt8Nn> fast_itone {};
      std::array<signed char, kFt8Bits> fast_message77 {};
      bool replay_ok =
          try_ft8_a7_fast_repeated_hint (state, request, previous,
                                         newdat_a7, sbase, msg37, xsnr,
                                         xdt, f1, fast_itone,
                                         fast_message77,
                                         recent_supported_cq
                                         || recent_supported_direct
                                         || recent_unsupported_direct_report,
                                         recent_unsupported_direct_report);
      if (!replay_ok
          && recent_supported_direct
          && direct_report_replay_tail (hint_message))
        {
          for (float const delta : {0.75f, -0.75f})
            {
              if (stage4_should_cancel ())
                {
                  return;
                }
              Ft8A7Entry adjusted = previous;
              adjusted.freq = previous.freq + delta;
              replay_ok =
                  try_ft8_a7_fast_repeated_hint (state, request, adjusted,
                                                 newdat_a7, sbase, msg37, xsnr,
                                                 xdt, f1, fast_itone,
                                                 fast_message77, true, false);
              if (replay_ok)
                {
                  break;
                }
            }
        }
      if (!replay_ok)
        {
          continue;
        }
      if (stage4_should_cancel ())
        {
          return;
        }

      msg37 = normalize_resolved_hash_call_tokens (msg37);
      int nsnr = 0;
      int iaptype = 0;
      float qual = 0.0f;
      ftx_ft8_finalize_a7_result_c (xsnr, &nsnr, &iaptype, &qual);
      collector.append (kSyncAp, nsnr, xdt, f1, msg37, iaptype, qual,
                        fast_message77.data ());
      int const replay_seed_hits = direct_report_replay_tail (msg37) ? 3 : 1;
      save_a7_entry (state.a7, jseq, xdt, f1, msg37, replay_seed_hits);
      save_call_grid_history (call_grid_history (), request, jseq, xdt, f1, msg37);
      seed_pack77_hashes_from_message (msg37);
    }
}

} // namespace

extern "C" void ftx_ft8_a7d_c (float* dd0, int* newdat, char const call_1[12],
                               char const call_2[12], char const grid4[4], float* xdt,
                               float* f1, float* xbase, int* nharderrors,
                               float* dmin, char msg37[37], float* xsnr)
{
  if (msg37)
    {
      std::fill_n (msg37, kFt8DecodedChars, ' ');
    }
  if (nharderrors)
    {
      *nharderrors = -1;
    }
  if (dmin)
    {
      *dmin = 1.0e30f;
    }
  if (xsnr)
    {
      *xsnr = -25.0f;
    }

  if (!dd0 || !xdt || !f1 || !xbase || !nharderrors || !dmin || !msg37 || !xsnr)
    {
      return;
    }
  if (stage4_should_cancel ())
    {
      return;
    }

  int newdat_local = newdat ? *newdat : 0;
  std::array<std::complex<float>, kFt8A7DownsampleSize> cd0 {};
  ftx_ft8_downsample_c (dd0, &newdat_local, *f1,
                        reinterpret_cast<fftwf_complex*> (cd0.data ()));
  if (newdat)
    {
      *newdat = newdat_local;
    }

  int ibest = 0;
  float delfbest = 0.0f;
  ftx_ft8_a7_search_initial_c (cd0.data (), kFt8A7Np2, kFt8A7Fs2, *xdt, &ibest, &delfbest);

  std::array<float, 5> tweak {};
  tweak[0] = -delfbest;
  if (std::fabs (delfbest) > 0.0f)
    {
      int const npts = kFt8A7Np2;
      float const fsample = kFt8A7Fs2;
      std::array<std::complex<float>, kFt8A7DownsampleSize> adjusted {};
      ftx_twkfreq1_c (cd0.data (), &npts, &fsample, tweak.data (), adjusted.data ());
      cd0 = adjusted;
    }
  *f1 += delfbest;

  int second_pass_newdat = 0;
  ftx_ft8_downsample_c (dd0, &second_pass_newdat, *f1,
                        reinterpret_cast<fftwf_complex*> (cd0.data ()));

  float sync = 0.0f;
  ftx_ft8_a7_refine_search_c (cd0.data (), kFt8A7Np2, kFt8A7Fs2, ibest, &ibest, &sync, xdt);

  std::array<float, 8 * kFt8Nn> s8 {};
  std::array<float, 174> llra {};
  std::array<float, 174> llrb {};
  std::array<float, 174> llrc {};
  std::array<float, 174> llrd {};
  std::array<float, 174> llre {};
  int nsync = 0;
  ftx_ft8_bitmetrics_scaled_c (cd0.data (), kFt8A7Np2, ibest, 1, kFt8BitMetricScale, s8.data (), &nsync,
                               llra.data (), llrb.data (), llrc.data (), llrd.data (), llre.data ());

  std::array<float, kFt8A7MaxMsg> dmm;
  dmm.fill (1.0e30f);
  float best_dmin = 1.0e30f;
  float pbest = 0.0f;
  int best_nhard = -1;
  FixedChars<kFt8DecodedChars> best_msg = blank_fixed<kFt8DecodedChars> ();
  std::array<int, kFt8Nn> best_itone {};

  for (int imsg = 1; imsg <= kFt8A7MaxMsg; ++imsg)
    {
      if (stage4_should_cancel ())
        {
          return;
        }
      FixedChars<kFt8DecodedChars> candidate_message = blank_fixed<kFt8DecodedChars> ();
      if (ftx_prepare_ft8_a7_candidate_c (imsg, call_1, call_2, grid4,
                                          candidate_message.data ()) == 0)
        {
          continue;
        }

      FixedChars<kFt8DecodedChars> msgsent = blank_fixed<kFt8DecodedChars> ();
      std::array<int, kFt8Nn> itone {};
      std::array<signed char, 174> cw {};
      if (ftx_encode_ft8_candidate_c (candidate_message.data (), msgsent.data (),
                                      itone.data (), cw.data ()) == 0)
        {
          continue;
        }

      float pow = 0.0f;
      float dm = 0.0f;
      int nhard = 0;
      ftx_ft8a7_measure_candidate_c (s8.data (), 8, kFt8Nn, itone.data (), cw.data (),
                                     llra.data (), llrb.data (), llrc.data (), llrd.data (),
                                     &pow, &dm, &nhard);
      dmm[static_cast<size_t> (imsg - 1)] = dm;
      if (dm < best_dmin)
        {
          best_dmin = dm;
          best_msg = msgsent;
          pbest = pow;
          best_nhard = nhard;
          best_itone = itone;
        }
    }

  float dmin2 = 1.0e30f;
  int const accept = ftx_ft8_a7_finalize_metrics_c (dmm.data (), kFt8A7MaxMsg, pbest, *xbase,
                                                     dmin, &dmin2, xsnr);
  if (accept != 0)
    {
      float snr_estimate = *xsnr;
      ftx_ft8_compute_snr_c (s8.data (), 8, kFt8Nn, best_itone.data (), *xbase, 0,
                             nsync, &snr_estimate);
      *xsnr = snr_estimate;
    }
  *nharderrors = accept != 0 ? best_nhard : -1;
  std::copy (best_msg.begin (), best_msg.end (), msg37);

  if (starts_with (best_msg, "CQ ") && is_standard_call (call_2, 12)
      && trim_length (grid4, 4) == 0)
    {
      *nharderrors = -1;
    }
  if (starts_with (best_msg, "QU1RK "))
    {
      *nharderrors = -1;
    }
}

extern "C" void ftx_ft8_a8d_c (float* dd, char const mycall[12], char const dxcall[12],
                               char const dxgrid[6], float* f1a, float* xdt, float* fbest,
                               float* xsnr, float* plog, char msgbest[37])
{
  if (msgbest)
    {
      std::fill_n (msgbest, kFt8DecodedChars, ' ');
    }
  if (xsnr)
    {
      *xsnr = -134.0f;
    }
  if (plog)
    {
      *plog = 0.0f;
    }
  if (fbest)
    {
      *fbest = 0.0f;
    }

  if (!dd || !mycall || !dxcall || !dxgrid || !f1a || !xdt || !fbest || !xsnr || !plog || !msgbest)
    {
      return;
    }
  if (stage4_should_cancel ())
    {
      return;
    }

  float const f1 = *f1a;
  int newdat = 1;
  std::array<std::complex<float>, kFt8A7DownsampleSize> cd {};
  ftx_ft8_downsample_c (dd, &newdat, f1, reinterpret_cast<fftwf_complex*> (cd.data ()));

  float sbest = 0.0f;
  float tbest = 0.0f;
  FixedChars<kFt8DecodedChars> best_message = blank_fixed<kFt8DecodedChars> ();
  std::array<int, kFt8Nn> best_itone {};
  std::array<float, kFt8A7DownsampleSize + 1> best_spectrum {};

  for (int imsg = 1; imsg <= kFt8A8MaxMsg; ++imsg)
    {
      if (stage4_should_cancel ())
        {
          return;
        }
      FixedChars<kFt8DecodedChars> candidate_message = blank_fixed<kFt8DecodedChars> ();
      if (ftx_prepare_ft8_a8_candidate_c (imsg, mycall, dxcall, dxgrid,
                                          candidate_message.data ()) == 0)
        {
          continue;
        }

      FixedChars<kFt8DecodedChars> msgsent = blank_fixed<kFt8DecodedChars> ();
      std::array<int, kFt8Nn> itone {};
      std::array<signed char, 174> codeword {};
      if (ftx_encode_ft8_candidate_c (candidate_message.data (), msgsent.data (),
                                      itone.data (), codeword.data ()) == 0)
        {
          continue;
        }

      std::array<std::complex<float>, kFt8A8Nwave> cwave {};
      generate_ft8_a8_waveform (itone.data (), 0.0f, cwave);

      float spk = 0.0f;
      float candidate_fbest = 0.0f;
      float candidate_tbest = 0.0f;
      std::array<float, kFt8A7DownsampleSize + 1> spectrum {};
      if (ftx_ft8_a8_search_candidate_c (cd.data (), cwave.data (), kFt8A7DownsampleSize,
                                         kFt8A8Nwave, f1, &spk, &candidate_fbest,
                                         &candidate_tbest, spectrum.data ()) == 0)
        {
          continue;
        }

      if (spk > sbest)
        {
          sbest = spk;
          *fbest = candidate_fbest;
          tbest = candidate_tbest;
          best_message = candidate_message;
          best_itone = itone;
          best_spectrum = spectrum;
        }
    }

  if (ftx_ft8_a8_finalize_search_c (best_spectrum.data (), kFt8A7DownsampleSize + 1,
                                    f1, *fbest, xsnr) == 0)
    {
      return;
    }

  std::array<float, 5> tweak {};
  tweak[0] = f1 - *fbest;
  int const npts = kFt8A7DownsampleSize;
  float const fsample = kFt8A7Fs2;
  std::array<std::complex<float>, kFt8A7DownsampleSize> adjusted {};
  ftx_twkfreq1_c (cd.data (), &npts, &fsample, tweak.data (), adjusted.data ());
  cd = adjusted;
  *xdt = tbest;

  int nhard = 0;
  float sigobig = 0.0f;
  ftx_ft8_a8_score_c (cd.data (), kFt8A7DownsampleSize, tbest, best_itone.data (),
                      plog, &nhard, &sigobig);
  if (ftx_ft8_a8_accept_score_c (nhard, *plog, sigobig) == 0)
    {
      return;
    }

  std::copy (best_message.begin (), best_message.end (), msgbest);
}

extern "C" void ftx_ft8_a7d_ref_c (float* dd0, int* newdat, char const call_1[12],
                                   char const call_2[12], char const grid4[4], float* xdt,
                                   float* f1, float* xbase, int* nharderrors,
                                   float* dmin, char msg37[37], float* xsnr)
{
  if (msg37)
    {
      std::fill_n (msg37, kFt8DecodedChars, ' ');
    }
  if (nharderrors)
    {
      *nharderrors = -1;
    }
  if (dmin)
    {
      *dmin = 1.0e30f;
    }
  if (xsnr)
    {
      *xsnr = -25.0f;
    }

  if (!dd0 || !call_1 || !call_2 || !grid4 || !xdt || !f1 || !xbase
      || !nharderrors || !dmin || !msg37 || !xsnr)
    {
      if (newdat)
        {
          *newdat = 0;
        }
      return;
    }

  int newdat_local = newdat ? (*newdat != 0 ? 1 : 0) : 0;
  FixedChars<12> call1_copy = fixed_from_chars<12> (call_1);
  FixedChars<12> call2_copy = fixed_from_chars<12> (call_2);
  FixedChars<4> grid4_copy = fixed_from_chars<4> (grid4);
  FixedChars<kFt8DecodedChars> msg_copy = blank_fixed<kFt8DecodedChars> ();

  __ft8_a7_MOD_ft8_a7d (dd0, &newdat_local, call1_copy.data (), call2_copy.data (),
                        grid4_copy.data (), xdt, f1, xbase, nharderrors, dmin,
                        msg_copy.data (), xsnr, call1_copy.size (), call2_copy.size (),
                        grid4_copy.size (), msg_copy.size ());

  if (newdat)
    {
      *newdat = newdat_local != 0 ? 1 : 0;
    }
  std::copy (msg_copy.begin (), msg_copy.end (), msg37);
}

extern "C" void ftx_ft8_a8d_ref_c (float* dd, char const mycall[12], char const dxcall[12],
                                   char const dxgrid[6], float* f1a, float* xdt, float* fbest,
                                   float* xsnr, float* plog, char msgbest[37])
{
  if (msgbest)
    {
      std::fill_n (msgbest, kFt8DecodedChars, ' ');
    }
  if (xsnr)
    {
      *xsnr = -134.0f;
    }
  if (plog)
    {
      *plog = 0.0f;
    }
  if (fbest)
    {
      *fbest = 0.0f;
    }

  if (!dd || !mycall || !dxcall || !dxgrid || !f1a || !xdt || !fbest || !xsnr || !plog || !msgbest)
    {
      return;
    }

  FixedChars<12> mycall_copy = fixed_from_chars<12> (mycall);
  FixedChars<12> dxcall_copy = fixed_from_chars<12> (dxcall);
  FixedChars<6> dxgrid_copy = fixed_from_chars<6> (dxgrid);
  FixedChars<kFt8DecodedChars> msg_copy = blank_fixed<kFt8DecodedChars> ();

  ft8_a8d_ (dd, mycall_copy.data (), dxcall_copy.data (), dxgrid_copy.data (),
            f1a, xdt, fbest, xsnr, plog, msg_copy.data (),
            mycall_copy.size (), dxcall_copy.size (), dxgrid_copy.size (), msg_copy.size ());

  std::copy (msg_copy.begin (), msg_copy.end (), msgbest);
}

extern "C" void ft8_a8d_ (float* dd, char* mycall, char* dxcall, char* dxgrid,
                          float* f1a, float* xdt, float* fbest, float* xsnr,
                          float* plog, char* msgbest,
                          size_t mycall_len, size_t dxcall_len,
                          size_t dxgrid_len, size_t msgbest_len)
{
  FixedChars<12> mycall_copy = blank_fixed<12> ();
  FixedChars<12> dxcall_copy = blank_fixed<12> ();
  FixedChars<6> dxgrid_copy = blank_fixed<6> ();
  FixedChars<kFt8DecodedChars> msg_copy = blank_fixed<kFt8DecodedChars> ();

  if (mycall)
    {
      std::copy_n (mycall, std::min (static_cast<int>(mycall_len), static_cast<int>(mycall_copy.size ())), mycall_copy.data ());
    }
  if (dxcall)
    {
      std::copy_n (dxcall, std::min (static_cast<int>(dxcall_len), static_cast<int>(dxcall_copy.size ())), dxcall_copy.data ());
    }
  if (dxgrid)
    {
      std::copy_n (dxgrid, std::min (static_cast<int>(dxgrid_len), static_cast<int>(dxgrid_copy.size ())), dxgrid_copy.data ());
    }

  ftx_ft8_a8d_c (dd, mycall_copy.data (), dxcall_copy.data (), dxgrid_copy.data (),
                 f1a, xdt, fbest, xsnr, plog, msg_copy.data ());

  if (msgbest)
    {
      size_t const copy_size = std::min (static_cast<int>(msgbest_len), static_cast<int>(msg_copy.size ()));
      std::fill_n (msgbest, msgbest_len, ' ');
      std::copy_n (msg_copy.data (), copy_size, msgbest);
    }
}

extern "C" void __ft8_a7_MOD_ft8_a7d (float* dd0, int* newdat, char* call_1, char* call_2,
                                      char* grid4, float* xdt, float* f1, float* xbase,
                                      int* nharderrors, float* dmin, char* msg37, float* xsnr,
                                      size_t call_1_len, size_t call_2_len,
                                      size_t grid4_len, size_t msg37_len)
{
  FixedChars<12> call1_copy = blank_fixed<12> ();
  FixedChars<12> call2_copy = blank_fixed<12> ();
  FixedChars<4> grid4_copy = blank_fixed<4> ();
  FixedChars<kFt8DecodedChars> msg_copy = blank_fixed<kFt8DecodedChars> ();

  if (call_1)
    {
      std::copy_n (call_1, std::min (static_cast<int>(call_1_len), static_cast<int>(call1_copy.size ())), call1_copy.data ());
    }
  if (call_2)
    {
      std::copy_n (call_2, std::min (static_cast<int>(call_2_len), static_cast<int>(call2_copy.size ())), call2_copy.data ());
    }
  if (grid4)
    {
      std::copy_n (grid4, std::min (static_cast<int>(grid4_len), static_cast<int>(grid4_copy.size ())), grid4_copy.data ());
    }

  ftx_ft8_a7d_c (dd0, newdat, call1_copy.data (), call2_copy.data (), grid4_copy.data (),
                 xdt, f1, xbase, nharderrors, dmin, msg_copy.data (), xsnr);

  if (msg37)
    {
      size_t const copy_size = std::min (static_cast<int>(msg37_len), static_cast<int>(msg_copy.size ()));
      std::fill_n (msg37, msg37_len, ' ');
      std::copy_n (msg_copy.data (), copy_size, msg37);
    }
}

extern "C" void ftx_ft8_stage4_reset_c ()
{
  stage4_state ().reset ();
  cq_signal_history ().reset ();
  call_grid_history ().reset ();
}

extern "C" int ftx_ft8_message_is_plausible_for_emit_c (char const msg37[37])
{
  if (!msg37)
    {
      return 0;
    }
  FixedChars<kFt8DecodedChars> const normalized =
      normalize_resolved_hash_call_tokens (fixed_from_chars<kFt8DecodedChars> (msg37));
  return is_plausible_ft8_message_for_emit (normalized) ? 1 : 0;
}

extern "C" void ftx_ft8_stage4_seed_known_cq_c (char const* call, char const* grid,
                                                float freq, float dt, int nutc)
{
  if (!call || !grid)
    {
      return;
    }
  std::string const call_string = trim_fixed (fixed_from_chars<12> (call));
  std::string const grid_string = trim_fixed (fixed_from_chars<4> (grid));
  save_known_call_grid (known_call_grid_history (), call_string, grid_string, freq, dt, nutc);
  if (debug_known_cq_replay ())
    {
      std::cerr << "[KNOWNCQ] seed call=" << call_string
                << " grid=" << grid_string
                << " freq=" << freq
                << " dt=" << dt
                << " nutc=" << nutc << '\n';
    }
  seed_pack77_hash_call (call_string, true, true);
}

extern "C" void ftx_ft8_stage4_seed_known_cq_call_c (char const* call,
                                                     float freq, float dt, int nutc)
{
  if (!call)
    {
      return;
    }
  std::string const call_string = trim_fixed (fixed_from_chars<12> (call));
  save_known_cq_call (known_cq_call_history (), call_string, freq, dt, nutc);
  if (debug_known_cq_replay ())
    {
      std::cerr << "[KNOWNCQ] seed-call-only call=" << call_string
                << " freq=" << freq
                << " dt=" << dt
                << " nutc=" << nutc << '\n';
    }
  seed_pack77_hash_call (call_string, true, true);
}

extern "C" void ftx_ft8_stage4_seed_hash_call_c (char const* call)
{
  if (!call)
    {
      return;
    }
  seed_pack77_hash_call (trim_block (call, 13), true, true);
}

extern "C" void ftx_ft8_stage4_apply_hash_seed_cache_c ()
{
  apply_pack77_hash_seed_cache ();
}

extern "C" void ftx_ft8_stage4_set_cancel_c (int cancel)
{
  stage4_cancel_requested ().store (cancel != 0, std::memory_order_relaxed);
}

extern "C" void ftx_ft8_stage4_set_deadline_ms_c (long long deadline_ms)
{
  stage4_deadline_ms ().store (deadline_ms, std::memory_order_relaxed);
}

extern "C" void ftx_ft8_stage4_set_ldpc_osd_c (int maxosd, int norder)
{
  stage4_ldpc_maxosd_override ().store (maxosd, std::memory_order_relaxed);
  stage4_ldpc_norder_override ().store (norder, std::memory_order_relaxed);
}

extern "C" void ftx_ft8_stage4_set_supplemental_c (int supplemental)
{
  stage4_supplemental_requested ().store (supplemental != 0, std::memory_order_relaxed);
}

extern "C" void ftx_ft8_stage4_set_superfox_options_c (int enabled, int ntol_hz)
{
  stage4_superfox_enabled ().store (enabled != 0, std::memory_order_relaxed);
  stage4_superfox_tolerance_hz ().store (std::max (1, std::min (ntol_hz, 200)),
                                         std::memory_order_relaxed);
}

extern "C" void ftx_ft8_stage4_set_force_fresh_slot_c (int force)
{
  stage4_force_fresh_slot ().store (force != 0, std::memory_order_relaxed);
}

extern "C" void ftx_ft8_stage4_set_freqpart_c (int bins)
{
  stage4_freqpart_request ().store (std::max (0, std::min (32, bins)), std::memory_order_relaxed);
}

extern "C" void ftx_ft8_stage4_set_syncmin_scale_c (float scale)
{
  stage4_syncmin_scale_request ().store ((scale > 0.0f && scale <= 2.0f) ? scale : 1.0f, std::memory_order_relaxed);
}

extern "C" void ftx_ft8_stage4_set_decode_syncmin_c (int gate)
{
  stage4_decode_syncmin_request ().store ((gate >= 0 && gate <= 10) ? gate : -1, std::memory_order_relaxed);
}

extern "C" void ftx_ft8_stage4_set_decode_options_c (int low_thresholds, int subpass,
                                                     int cycles, int rx_freq_sensitivity,
                                                     int candidate_thin)
{
  stage4_low_threshold_requested ().store (low_thresholds != 0, std::memory_order_relaxed);
  stage4_subpass_requested ().store (subpass != 0, std::memory_order_relaxed);
  stage4_decode_cycles ().store (std::max (1, std::min (cycles, 3)), std::memory_order_relaxed);
  stage4_rx_freq_sensitivity ().store (std::max (1, std::min (rx_freq_sensitivity, 3)),
                                       std::memory_order_relaxed);
  stage4_candidate_thin ().store (std::max (1, std::min (candidate_thin, 100)),
                                  std::memory_order_relaxed);
}

extern "C" int ftx_ft8_freqpart_bins_used_c ()
{
  return freqpart_bins_used ().load (std::memory_order_relaxed);
}

extern "C" void ftx_ft8_async_decode_stage4_c (short const* iwave,
                                               int* nqsoprogress, int* nfqso, int* nftx,
                                               int* nutc, int* nfa, int* nfb,
                                               int* nzhsym, int* ndepth, float* emedelay,
                                               int* ncontest, int* nagain, int* lft8apon,
                                               int* ltry_a8, int* lapcqonly, int* napwid,
                                               char const* mycall, char const* hiscall,
                                               char const* hisgrid, int* ldiskdat,
                                               float* syncs, int* snrs, float* dts, float* freqs,
                                               int* naps, float* quals, signed char* bits77,
                                               char* decodeds, int* nout)
{
  if (!iwave || !nqsoprogress || !nfqso || !nftx || !nutc || !nfa || !nfb || !nzhsym
      || !ndepth || !emedelay || !ncontest || !nagain || !lft8apon || !lapcqonly
      || !ltry_a8 || !napwid || !mycall || !hiscall || !hisgrid || !ldiskdat
      || !snrs || !dts || !freqs || !naps || !quals || !bits77 || !decodeds || !nout)
    {
      if (nout)
        {
          *nout = 0;
        }
      return;
    }

  freqpart_bins_used ().store (0, std::memory_order_relaxed);
  AsyncCollector collector;
  collector.syncs = syncs;
  collector.snrs = snrs;
  collector.dts = dts;
  collector.freqs = freqs;
  collector.naps = naps;
  collector.quals = quals;
  collector.bits77 = bits77;
  collector.decodeds = decodeds;
  collector.nout = nout;
  collector.reset ();
  if (stage4_should_cancel ())
    {
      return;
    }

  // 6 settembre 2026 -- SuperFox (Hound): il pacchetto QPC compresso del Fox
  // occupa l'intera sequenza pari (nutc%10==0) e non e' un segnale FT8
  // normale -- niente ricerca candidati in parallelo sullo stesso audio
  // (stesso intento del vecchio percorso sincrono in mainwindow.cpp, mai
  // collegato a questo worker asincrono). Il decoder QPC vero e proprio
  // (superfox_decode_lines_from_wave) e' verificato correttamente
  // funzionante dai tool standalone utils/sfrx.cpp/sfoxsim.cpp; qui viene
  // agganciato per la prima volta alla pipeline live, solo per il ruolo
  // Hound (ncontest==7 -- il Fox trasmette il pacchetto, non lo riceve).
  bool const superfox_wanted =
      (*ncontest == 7)
      && stage4_superfox_enabled ().load (std::memory_order_relaxed)
      && ((*nutc % 10) == 0);
  if (superfox_wanted)
    {
      if (*nzhsym < 50)
        {
          return;   // slot pari in corso, aspetta il pass finale
        }
      if (stage4_should_cancel ())
        {
          return;
        }

      std::vector<std::string> lines;
      int nsnr = 0;
      float freq = 0.0f;
      float dt = 0.0f;
      int const ntol = stage4_superfox_tolerance_hz ().load (std::memory_order_relaxed);

      if (superfox_decode_lines_from_wave (iwave, *nfqso, ntol, lines, nsnr, freq, dt))
        {
          for (std::string const& line : lines)
            {
              collector.append (0.0f, nsnr, dt, freq,
                                fixed_from_string<kFt8DecodedChars> (line),
                                0 /* nap */, 1.0f /* qual */, nullptr /* message77 */);
            }
        }
      return;
    }

  Ft8Request request;
  request.iwave = iwave;
  request.nqsoprogress = *nqsoprogress;
  request.nfqso = *nfqso;
  request.nftx = *nftx;
  request.nutc = *nutc;
  request.nfa = *nfa;
  request.nfb = *nfb;
  request.nzhsym = *nzhsym;
  request.ndepth = *ndepth;
  request.emedelay = *emedelay;
  request.ncontest = *ncontest;
  request.nagain = *nagain;
  request.lft8apon = *lft8apon;
  request.ltry_a8 = *ltry_a8;
  request.lapcqonly = *lapcqonly;
  request.napwid = *napwid;
  request.ldiskdat = *ldiskdat;
  request.ncandthin = stage4_candidate_thin ().load (std::memory_order_relaxed);
  request.nft8cycles = stage4_decode_cycles ().load (std::memory_order_relaxed);
  request.nft8rxfsens = stage4_rx_freq_sensitivity ().load (std::memory_order_relaxed);
  request.lft8lowth = stage4_low_threshold_requested ().load (std::memory_order_relaxed);
  request.lft8subpass = stage4_subpass_requested ().load (std::memory_order_relaxed);
  request.supplemental = stage4_supplemental_requested ().load (std::memory_order_relaxed);
  request.mycall = fixed_from_chars<12> (mycall);
  request.hiscall = fixed_from_chars<12> (hiscall);
  request.hisgrid = fixed_from_chars<6> (hisgrid);

  int const requested_ndepth = request.ndepth;
  bool const requested_supplemental = request.supplemental;
  if (requested_supplemental)
    {
      request.ndepth = std::min (request.ndepth, 3);
    }

  Ft8Stage4State& state = stage4_state ();
  if (request.nutc != state.early_nutc
      || stage4_force_fresh_slot ().load (std::memory_order_relaxed))
    {
      // Early/full FT8 passes may be interrupted by the live deadline. Never
      // carry saved early decodes, tones, or subtractions into the next UTC
      // slot; otherwise stale AP decodes are emitted with a fresh timestamp.
      state.resetEarlySlotState ();
      state.early_nutc = request.nutc;
    }
  int const jseq = sequence_index_for_utc (request.nutc);
  prepare_a7_tables (state.a7, state.nutc0, request.nutc, request.nzhsym, jseq);

  int stage_action = 0;
  int refine = 0;
  ftx_ft8_plan_decode_stage_c (request.ndepth, request.nzhsym, state.ndec_early,
                               request.nagain, &stage_action, &refine);
  if (stage_action == 1)
    {
      state.ndec_early = 0;
      return;
    }
  if (stage4_should_cancel ())
    {
      return;
    }

  if (request.nzhsym == 50 && stage_action != 4)
    {
      copy_audio_to_float (request.iwave, state.dd);
    }

  std::array<int, 58> apsym {};
  std::array<int, 10> aph10 {};
  ftx_prepare_ft8_ap_c (request.mycall.data (), request.hiscall.data (),
                        request.ncontest, apsym.data (), aph10.data ());

  if (request.nzhsym <= 47)
    {
      copy_audio_to_float (request.iwave, state.dd);
      state.dd1 = state.dd;
      state.early_audio_samples = std::max (0, std::min (request.nzhsym * 3456, kFt8NMax));
    }

  int ndecodes = request.nzhsym == 41 ? 0 : state.ndec_early;
  if (request.nzhsym == 41)
    {
      state.allmessages.fill (blank_fixed<kFt8DecodedChars> ());
      state.allsnrs.fill (0);
    }

  if (stage_action == 2)
    {
      state.dd1 = state.dd;
      state.ndec_early = 0;
      if (request.nzhsym < 50)
        {
          state.ndec_early = ndecodes;
        }
      if (stage4_should_cancel ())
        {
          return;
        }
	      std::array<float, kFt8Nh1> blank_sbase {};
	      auto const& ap_sbase = state.early_sbase_valid ? state.early_sbase : blank_sbase;
	      run_ap_passes (state, request, jseq, ap_sbase, collector);
	      collector.resolve_hash_placeholders ();
	      return;
	    }

  if (stage_action == 3)
    {
      if (stage4_should_cancel ())
        {
          return;
        }
      state.lsubtracted.fill (false);
      ftx_ft8_apply_saved_subtractions_c (state.dd.data (), state.itone_save.data (), kFt8Nn,
                                          state.ndec_early, state.f1_save.data (),
                                          state.xdt_save.data (), state.lsubtracted.data (),
                                          1, refine != 0 ? 1 : 0);
      state.dd1 = state.dd;
	      std::array<float, kFt8Nh1> blank_sbase {};
	      auto const& ap_sbase = state.early_sbase_valid ? state.early_sbase : blank_sbase;
	      run_ap_passes (state, request, jseq, ap_sbase, collector);
	      collector.resolve_hash_placeholders ();
	      return;
	    }

  if (stage_action == 4)
    {
      if (stage4_should_cancel ())
        {
          return;
        }
      restore_carried_audio (state.dd, state.dd1, request.iwave, state.early_audio_samples);
      ftx_ft8_apply_saved_subtractions_c (state.dd.data (), state.itone_save.data (), kFt8Nn,
                                          state.ndec_early, state.f1_save.data (),
                                          state.xdt_save.data (), state.lsubtracted.data (),
                                          0, 1);
    }

  int ifa = request.nfa;
  int ifb = request.nfb;
  if (request.nzhsym == 50 && request.nagain != 0)
    {
      copy_audio_to_float (request.iwave, state.dd);
      ifa = request.nfqso - 20;
      ifb = request.nfqso + 20;
    }

  std::array<float, kFt8Nh1> sbase {};
  if (stage4_should_cancel ())
    {
      return;
    }
  bool const primary_equalized_pipeline = request.nzhsym >= 41 && request.nzhsym < 50;
  bool const run_supplemental_rescue =
      requested_supplemental
      && request.nzhsym >= 50
      && requested_ndepth >= 3;
  Ft8Request primary_request = request;
  primary_request.supplemental = false;
  if (run_supplemental_rescue)
    {
      primary_request.ndepth = std::min (primary_request.ndepth, 3);
    }
  auto merge_saved_decodes = [&] (Ft8Stage4State const& source_state, int source_ndecodes) {
    int merged_ndecodes = ndecodes;
    int added = 0;
    for (int id = 0; id < source_ndecodes && id < kFt8MaxEarly; ++id)
      {
        FixedChars<kFt8DecodedChars> const& decoded =
            source_state.allmessages[static_cast<size_t> (id)];
        bool duplicate = false;
        for (int existing = 0; existing < merged_ndecodes; ++existing)
          {
            if (messages_equal (decoded, state.allmessages[static_cast<size_t> (existing)]))
              {
                duplicate = true;
                break;
              }
          }
        if (duplicate)
          {
            continue;
          }
        int const saved_slot =
            ftx_ft8_store_saved_decode_c (merged_ndecodes, kFt8MaxEarly,
                                          source_state.allsnrs[static_cast<size_t> (id)],
                                          source_state.f1_save[static_cast<size_t> (id)],
                                          source_state.xdt_save[static_cast<size_t> (id)],
                                          source_state.itone_save.data () + id * kFt8Nn,
                                          kFt8Nn, state.allsnrs.data (), state.f1_save.data (),
                                          state.xdt_save.data (), state.itone_save.data ());
        if (saved_slot == 0)
          {
            continue;
          }
        merged_ndecodes = saved_slot;
        state.allmessages[static_cast<size_t> (merged_ndecodes - 1)] = decoded;
        save_a7_entry (state.a7, jseq,
                       source_state.xdt_save[static_cast<size_t> (id)] - 0.5f,
                       source_state.f1_save[static_cast<size_t> (id)], decoded);
        ++added;
      }
    if (merged_ndecodes > ndecodes)
      {
        ndecodes = merged_ndecodes;
      }
	    return added;
	  };

  Ft8Stage4State state_before_main = state;
  run_main_passes (state, primary_request, jseq, apsym, aph10, ifa, ifb, ndecodes, sbase, collector,
                   primary_equalized_pipeline);
  if (stage4_should_cancel ())
    {
      return;
    }

  if (run_supplemental_rescue)
    {
      bool const wide_full_band = (ifb - ifa) >= 3500;
      if (!wide_full_band)
        {
          Ft8Stage4State rescue_state = state_before_main;
          Ft8Request rescue_request = request;
          rescue_request.supplemental = true;
          rescue_request.ndepth = std::max (requested_ndepth, 4);
          int rescue_ndecodes = state_before_main.ndec_early;
          std::array<float, kFt8Nh1> rescue_sbase {};
          run_main_passes (rescue_state, rescue_request, jseq, apsym, aph10, ifa, ifb,
                           rescue_ndecodes, rescue_sbase, collector, false);
          if (stage4_should_cancel ())
            {
              return;
            }
          merge_saved_decodes (rescue_state, rescue_ndecodes);
        }

      std::array<int, 8> blocked_focus {};
      int blocked_focus_count = 0;
      auto block_focus_frequency = [&] (int freq) {
        if (blocked_focus_count < static_cast<int> (blocked_focus.size ()))
          {
            blocked_focus[static_cast<size_t> (blocked_focus_count)] = freq;
            ++blocked_focus_count;
          }
      };
		      auto run_focused_window = [&] (int focused_nfqso, bool subpass_profile,
		                                     bool current_slot_state, bool deep_supplemental,
		                                     char const* focus_tag) {
	        long long const focus_start_ms = debug_ft8_focus_replay () ? steady_clock_ms () : 0;
	        Ft8Stage4State focused_state = current_slot_state ? state : state_before_main;
	        Ft8Request focused_request = primary_request;
	        focused_request.nfqso = focused_nfqso;
        if (deep_supplemental)
          {
            focused_request.ndepth = std::max (requested_ndepth, 4);
            focused_request.supplemental = true;
          }
        else
          {
            focused_request.ndepth = std::min (focused_request.ndepth, 3);
            focused_request.supplemental = false;
          }
        if (subpass_profile)
          {
            focused_request.lft8lowth = true;
            focused_request.lft8subpass = true;
            focused_request.nft8cycles = std::max (focused_request.nft8cycles, 2);
            focused_request.nft8rxfsens = std::max (focused_request.nft8rxfsens, 2);
          }
	        int focused_ndecodes = current_slot_state ? ndecodes : state_before_main.ndec_early;
        std::array<float, kFt8Nh1> focused_sbase {};
        int const focus_ifa = std::max (ifa, focused_nfqso - 30);
        int const focus_ifb = std::min (ifb, focused_nfqso + 30);
        run_main_passes (focused_state, focused_request, jseq, apsym, aph10,
                         focus_ifa, focus_ifb, focused_ndecodes, focused_sbase,
                         collector, false);
	        if (stage4_should_cancel ())
	          {
	            if (debug_ft8_focus_replay ())
	              {
	                std::cerr << "[FT8FOCUS] utc=" << request.nutc
	                          << " tag=" << focus_tag
	                          << " freq=" << focused_nfqso
	                          << " canceled=1 elapsed_ms="
	                          << (steady_clock_ms () - focus_start_ms) << '\n';
	              }
	            return -1;
	          }
	        int const added = merge_saved_decodes (focused_state, focused_ndecodes);
	        if (debug_ft8_focus_replay ())
	          {
	            std::cerr << "[FT8FOCUS] utc=" << request.nutc
	                      << " tag=" << focus_tag
	                      << " freq=" << focused_nfqso
	                      << " subpass=" << (subpass_profile ? 1 : 0)
		                      << " current=" << (current_slot_state ? 1 : 0)
		                      << " deep=" << (deep_supplemental ? 1 : 0)
			                      << " added=" << added
	                      << " elapsed_ms=" << (steady_clock_ms () - focus_start_ms)
	                      << '\n';
	          }
	        return added;
		      };

      for (int focus_attempt = 0; focus_attempt < 2; ++focus_attempt)
        {
          int const focused_nfqso =
              select_ft8_focus_frequency (state, primary_request, ifa, ifb, ndecodes,
                                          blocked_focus, blocked_focus_count, false);
          if (focused_nfqso <= 0)
            {
              break;
            }
		          int added = run_focused_window (focused_nfqso, false, false, false, "primary");
          if (stage4_should_cancel ())
            {
              return;
            }
          if (added == 0)
            {
		              added = run_focused_window (focused_nfqso, true, true, false, "primary-subpass");
              if (stage4_should_cancel ())
                {
                  return;
                }
            }
          if (added <= 0)
            {
              block_focus_frequency (focused_nfqso);
            }
        }

      for (int overlap_attempt = 0; overlap_attempt < 2; ++overlap_attempt)
        {
          int const focused_nfqso =
              select_ft8_focus_frequency (state, primary_request, ifa, ifb, ndecodes,
                                          blocked_focus, blocked_focus_count, true);
          if (focused_nfqso <= 0)
            {
              break;
            }
		          int added = run_focused_window (focused_nfqso, true, true, false, "overlap-subpass-current");
          if (stage4_should_cancel ())
            {
              return;
            }
          if (added <= 0)
            {
		              added = run_focused_window (focused_nfqso, true, false, false, "overlap-subpass-prior");
              if (stage4_should_cancel ())
                {
                  return;
                }
            }
          if (added <= 0)
            {
              block_focus_frequency (focused_nfqso);
            }
        }

      for (int close_attempt = 0; close_attempt < 3; ++close_attempt)
        {
          int const focused_nfqso =
              select_ft8_close_cq_focus_frequency (state_before_main, state,
                                                   primary_request, ifa, ifb,
                                                   ndecodes, blocked_focus,
                                                   blocked_focus_count);
          if (focused_nfqso <= 0)
            {
              break;
            }
		          int added = run_focused_window (focused_nfqso, true, false, false, "close-cq-subpass-prior");
          if (stage4_should_cancel ())
            {
              return;
            }
          if (added <= 0)
            {
		              added = run_focused_window (focused_nfqso, true, true, false, "close-cq-subpass-current");
              if (stage4_should_cancel ())
                {
                  return;
                }
            }
          if (added <= 0)
            {
		              added = run_focused_window (focused_nfqso, true, false, true, "close-cq-deep-prior");
              if (stage4_should_cancel ())
                {
                  return;
                }
            }
          if (added <= 0)
            {
              block_focus_frequency (focused_nfqso);
            }
        }

      for (int call_grid_attempt = 0; call_grid_attempt < 2; ++call_grid_attempt)
        {
          int const focused_nfqso =
              select_call_grid_focus_frequency (call_grid_history (), primary_request,
                                                jseq, ifa, ifb, ndecodes, state,
                                                blocked_focus, blocked_focus_count);
          if (focused_nfqso <= 0)
            {
              break;
            }
		          int added = run_focused_window (focused_nfqso, false, false, true, "call-grid-deep-prior");
          if (stage4_should_cancel ())
            {
              return;
            }
          if (added <= 0)
            {
              block_focus_frequency (focused_nfqso);
            }
        }
    }

  state.ndec_early = 0;
  if (request.nzhsym < 50)
    {
      state.ndec_early = ndecodes;
      state.early_sbase = sbase;
      state.early_sbase_valid = true;
    }

	  run_ap_passes (state, primary_request, jseq, sbase, collector);
	  collector.resolve_hash_placeholders ();
	}

extern "C" void ftx_ft8_emit_results_c (int* nutc, int* ncontest, int* nagain,
                                        int* b_superfox, char const* mycall,
                                        char const* mygrid, char const* temp_dir,
                                        float const* syncs, int const* snrs,
                                        float const* dts, float const* freqs,
                                        int const* naps, float const* quals,
                                        char const* decodeds, int* nout,
                                        int* decoded_count)
{
  if (!nutc || !ncontest || !nagain || !b_superfox || !mycall || !mygrid || !temp_dir
      || !syncs || !snrs || !dts || !freqs || !naps || !quals || !decodeds || !nout)
    {
      if (decoded_count)
        {
          *decoded_count = 0;
        }
      return;
    }

  int const count = std::max (0, std::min (*nout, kFt8MaxLines));
  if (decoded_count)
    {
      *decoded_count = count;
    }

  std::string const temp_dir_path = trim_block (temp_dir, 500);
  std::string const decoded_path = temp_dir_path.empty ()
      ? "decoded.txt"
      : temp_dir_path + "/decoded.txt";

  std::ofstream decoded_file {
      decoded_path,
      std::ios::out | (*nagain != 0 ? std::ios::app : std::ios::trunc)
  };

  FixedChars<12> const mycall_fixed = fixed_from_chars<12> (mycall);
  FixedChars<6> const mygrid_fixed = fixed_from_chars<6> (mygrid);
  std::string const mycall_trimmed = trim_fixed (mycall_fixed);
  bool const fox_mode = *ncontest == 6;
  Ft8EmitState& emit_state = ft8_emit_state ();
  std::string const hounds_path = temp_dir_path.empty ()
      ? "houndcallers.txt"
      : temp_dir_path + "/houndcallers.txt";

  if (fox_mode && !std::ifstream {hounds_path}.good ())
    {
      emit_state.reset ();
    }

  int current_n30 = 0;
  if (fox_mode)
    {
      current_n30 = update_ft8_n30_state (emit_state, *nutc);
    }

  for (int index = 0; index < count; ++index)
    {
      std::string decoded {decodeds + index * kFt8DecodedChars, kFt8DecodedChars};
      std::string const stdout_line =
          format_ft8_stdout_line (*nutc, syncs[index], snrs[index], dts[index],
                                  freqs[index], decoded, naps[index], quals[index]);
      std::cout << stdout_line << '\n';

      if (decoded_file.is_open ())
        {
          decoded_file << format_ft8_decoded_file_line (*nutc, syncs[index], snrs[index],
                                                        dts[index], freqs[index], decoded,
                                                        naps[index], quals[index])
                       << '\n';
        }

      if (fox_mode)
        {
          Ft8FoxEntry entry;
          if (should_collect_fox_entry (decoded, mycall_trimmed, *b_superfox != 0,
                                        static_cast<int> (std::lround (freqs[index])), entry))
            {
              entry.snr = snrs[index];
              entry.n30 = current_n30;
              emit_state.fox_entries.push_back (entry);
            }
        }
    }

  std::cout.flush ();
  if (decoded_file.is_open ())
    {
      decoded_file.flush ();
    }

  if (fox_mode)
    {
      write_houndcallers_file (hounds_path, emit_state, mygrid_fixed, current_n30);
    }
}

extern "C" int ftx_superfox_unpack_lines_c (unsigned char const* xdec,
                                            int use_otp,
                                            char* lines_out,
                                            int line_stride,
                                            int max_lines)
{
  if (!xdec)
    {
      return 0;
    }

  std::array<unsigned char, kSuperFoxPackedSymbols> payload {};
  std::copy_n (xdec, kSuperFoxPackedSymbols, payload.data ());
  std::vector<std::string> const lines = superfox_unpack_lines (payload, use_otp != 0);
  copy_superfox_lines (lines, lines_out, line_stride, max_lines);
  return static_cast<int> (lines.size ());
}

extern "C" int ftx_superfox_analytic_c (float const* dd,
                                        int npts,
                                        float* real_out,
                                        float* imag_out)
{
  if (!dd || !real_out || !imag_out || npts <= 0)
    {
      return 0;
    }

  std::vector<std::complex<float>> analytic (static_cast<size_t> (npts));
  if (!superfox_analytic_signal (dd, npts, analytic.data ()))
    {
      return 0;
    }

  for (int i = 0; i < npts; ++i)
    {
      real_out[i] = analytic[static_cast<size_t> (i)].real ();
      imag_out[i] = analytic[static_cast<size_t> (i)].imag ();
    }
  return 1;
}

extern "C" int ftx_superfox_remove_tone_c (float* real_io,
                                           float* imag_io,
                                           int npts,
                                           float fsync)
{
  if (!real_io || !imag_io || npts != kFt8NMax)
    {
      return 0;
    }

  std::vector<std::complex<float>> c0 (static_cast<size_t> (npts));
  for (int i = 0; i < npts; ++i)
    {
      c0[static_cast<size_t> (i)] =
          std::complex<float> {real_io[i], imag_io[i]};
    }

  if (!superfox_remove_tone (c0.data (), fsync))
    {
      return 0;
    }

  for (int i = 0; i < npts; ++i)
    {
      real_io[i] = c0[static_cast<size_t> (i)].real ();
      imag_io[i] = c0[static_cast<size_t> (i)].imag ();
  }
  return 1;
}

extern "C" int ftx_superfox_qpc_sync_c (float const* real_in,
                                        float const* imag_in,
                                        int npts,
                                        float fsync,
                                        float ftol,
                                        float* f2_out,
                                        float* t2_out,
                                        float* snrsync_out)
{
  if (!real_in || !imag_in || !f2_out || !t2_out || !snrsync_out || npts != kFt8NMax)
    {
      return 0;
    }

  std::vector<std::complex<float>> c0 (static_cast<size_t> (npts));
  for (int i = 0; i < npts; ++i)
    {
      c0[static_cast<size_t> (i)] = std::complex<float> {real_in[i], imag_in[i]};
    }

  float f2 = 0.0f;
  float t2 = 0.0f;
  float snrsync = 0.0f;
  if (!superfox_qpc_sync (c0.data (), 12000.0f, fsync, ftol, f2, t2, snrsync))
    {
      return 0;
    }

  *f2_out = f2;
  *t2_out = t2;
  *snrsync_out = snrsync;
  return 1;
}

extern "C" int ftx_superfox_qpc_likelihoods2_c (float const* s3,
                                                int rows,
                                                int cols,
                                                float EsNo,
                                                float No,
                                                float* py_out)
{
  if (!s3 || !py_out || rows <= 0 || cols <= 0)
    {
      return 0;
    }

  superfox_qpc_likelihoods (s3, rows, cols, EsNo, No, py_out);
  return 1;
}

extern "C" int ftx_superfox_qpc_snr_c (float const* s3,
                                       int rows,
                                       int cols,
                                       unsigned char const* y,
                                       float* snr_out)
{
  if (!s3 || !y || !snr_out || rows <= 0 || cols <= 0)
    {
      return 0;
    }

  *snr_out = superfox_qpc_snr (s3, rows, cols, y);
  return 1;
}

extern "C" int ftx_superfox_qpc_decode2_c (float const* real_in,
                                           float const* imag_in,
                                           int npts,
                                           float fsync,
                                           float ftol,
                                           int ndepth,
                                           float dth,
                                           float damp,
                                           unsigned char* xdec_out,
                                           int* crc_ok_out,
                                           float* snrsync_out,
                                           float* fbest_out,
                                           float* tbest_out,
                                           float* snr_out)
{
  if (!real_in || !imag_in || !xdec_out || !crc_ok_out
      || !snrsync_out || !fbest_out || !tbest_out || !snr_out
      || npts != kFt8NMax)
    {
      return 0;
    }

  std::vector<std::complex<float>> c0 (static_cast<size_t> (npts));
  for (int i = 0; i < npts; ++i)
    {
      c0[static_cast<size_t> (i)] = std::complex<float> {real_in[i], imag_in[i]};
    }

  SuperFoxQpcDecodeResult result;
  if (!superfox_qpc_decode2 (c0.data (), fsync, ftol, ndepth, dth, damp, result))
    {
      return 0;
    }

  std::copy (result.xdec.begin (), result.xdec.end (), xdec_out);
  *crc_ok_out = result.crc_ok ? 1 : 0;
  *snrsync_out = result.snrsync;
  *fbest_out = result.fbest;
  *tbest_out = result.tbest;
  *snr_out = result.snr;
  return 1;
}

extern "C" int ftx_superfox_decode_lines_c (short const* iwave,
                                            int nfqso,
                                            int ntol,
                                            char* lines_out,
                                            int line_stride,
                                            int max_lines,
                                            int* nsnr_out,
                                            float* freq_out,
                                            float* dt_out)
{
  std::vector<std::string> lines;
  int nsnr = 0;
  float freq = 0.0f;
  float dt = 0.0f;
  if (!superfox_decode_lines_from_wave (iwave, nfqso, ntol, lines, nsnr, freq, dt))
    {
      if (nsnr_out)
        {
          *nsnr_out = 0;
        }
      if (freq_out)
        {
          *freq_out = 0.0f;
        }
      if (dt_out)
        {
          *dt_out = 0.0f;
        }
      return 0;
    }

  copy_superfox_lines (lines, lines_out, line_stride, max_lines);
  if (nsnr_out)
    {
      *nsnr_out = nsnr;
    }
  if (freq_out)
    {
      *freq_out = freq;
    }
  if (dt_out)
    {
      *dt_out = dt;
    }
  return static_cast<int> (lines.size ());
}

extern "C" void sfox_ana_ (float* dd, int* npts, std::complex<float>* c0, int* npts2)
{
  if (!dd || !npts || !c0)
    {
      return;
    }
  int const count = *npts;
  if (npts2)
    {
      *npts2 = count;
    }
  superfox_analytic_signal (dd, count, c0);
}

extern "C" void sfox_remove_tone_ (std::complex<float>* c0, float* fsync)
{
  if (!c0 || !fsync)
    {
      return;
    }
  superfox_remove_tone (c0, *fsync);
}

extern "C" void qpc_sync_ (std::complex<float>* crcvd0, float* fsample, int* /*isync*/, float* fsync,
                           float* ftol, float* f2, float* t2, float* snrsync)
{
  if (!crcvd0 || !fsync || !ftol || !f2 || !t2 || !snrsync)
    {
      return;
    }

  float f2_local = 0.0f;
  float t2_local = 0.0f;
  float snr_local = 0.0f;
  if (!superfox_qpc_sync (crcvd0, fsample ? *fsample : 12000.0f, *fsync, *ftol,
                          f2_local, t2_local, snr_local))
    {
      return;
    }
  *f2 = f2_local;
  *t2 = t2_local;
  *snrsync = snr_local;
}

extern "C" void qpc_likelihoods2_ (float* py, float* s3, float* EsNo, float* No)
{
  if (!py || !s3 || !EsNo || !No)
    {
      return;
    }
  superfox_qpc_likelihoods (s3, kSuperFoxQpcRows, kSuperFoxQpcCols, *EsNo, *No, py);
}

extern "C" void qpc_snr_ (float* s3, signed char* y, float* snr)
{
  if (!s3 || !y || !snr)
    {
      return;
    }
  std::array<unsigned char, kSuperFoxQpcCols> symbols {};
  for (int i = 0; i < kSuperFoxQpcCols; ++i)
    {
      symbols[static_cast<size_t> (i)] = static_cast<unsigned char> (std::max<int> (0, y[i]));
    }
  *snr = superfox_qpc_snr (s3, kSuperFoxQpcRows, kSuperFoxQpcCols, symbols.data ());
}

extern "C" void qpc_decode2_ (std::complex<float>* c0, float* fsync, float* ftol,
                              signed char* xdec, int* ndepth, float* dth, float* damp,
                              int* crc_ok, float* snrsync, float* fbest, float* tbest, float* snr)
{
  if (!c0 || !fsync || !ftol || !xdec || !ndepth || !dth || !damp
      || !crc_ok || !snrsync || !fbest || !tbest || !snr)
    {
      return;
    }

  SuperFoxQpcDecodeResult result;
  if (!superfox_qpc_decode2 (c0, *fsync, *ftol, *ndepth, *dth, *damp, result))
    {
      *crc_ok = 0;
      return;
    }

  for (int i = 0; i < kSuperFoxPackedSymbols; ++i)
    {
      xdec[i] = static_cast<signed char> (result.xdec[static_cast<size_t> (i)]);
    }
  *crc_ok = result.crc_ok ? 1 : 0;
  *snrsync = result.snrsync;
  *fbest = result.fbest;
  *tbest = result.tbest;
  *snr = result.snr;
}

extern "C" void ftx_ft8_decode_and_emit_params_c (short const* iwave,
                                                  params_block_t const* params,
                                                  char const* temp_dir,
                                                  int* decoded_count)
{
  if (!iwave || !params || !temp_dir)
    {
      if (decoded_count)
        {
          *decoded_count = 0;
        }
      return;
    }

  int const ncontest = params->nexp_decode & 7;
  if (ncontest == 7 && params->b_superfox && params->b_even_seq)
    {
      if (params->nzhsym >= 50)
        {
          std::vector<std::string> lines;
          int nsnr = 0;
          float freq = 0.0f;
          float dt = 0.0f;
          if (superfox_decode_lines_from_wave (iwave, params->nfqso, params->ntol,
                                               lines, nsnr, freq, dt))
            {
              for (std::string const& line : lines)
                {
                  std::cout << format_superfox_stdout_line (params->nutc, nsnr, dt, freq, line)
                            << '\n';
                }
              std::cout.flush ();
            }
        }
      if (decoded_count)
        {
          *decoded_count = 0;
        }
      return;
    }

  int nqsoprogress = params->nQSOProgress;
  int nfqso = params->nfqso;
  int nftx = params->nftx;
  int nutc = params->nutc;
  int nfa = params->nfa;
  int nfb = params->nfb;
  int nzhsym = params->nzhsym;
  int ndepth = params->ndepth;
  float emedelay = params->emedelay;
  int nagain = params->nagain ? 1 : 0;
  int lft8apon = params->lft8apon ? 1 : 0;
  int ltry_a8 = (params->nzhsym == 41 || params->lmultift8) ? 1 : 0;
  int lapcqonly = params->lapcqonly ? 1 : 0;
  int napwid = params->napwid;
  int ldiskdat = params->ndiskdat ? 1 : 0;
  int b_superfox = params->b_superfox ? 1 : 0;
  int nout = 0;

  std::array<float, kFt8MaxLines> syncs {};
  std::array<int, kFt8MaxLines> snrs {};
  std::array<float, kFt8MaxLines> dts {};
  std::array<float, kFt8MaxLines> freqs {};
  std::array<int, kFt8MaxLines> naps {};
  std::array<float, kFt8MaxLines> quals {};
  std::array<signed char, kFt8Bits * kFt8MaxLines> bits77 {};
  std::array<char, kFt8DecodedChars * kFt8MaxLines> decodeds {};

  ftx_ft8_async_decode_stage4_c (iwave, &nqsoprogress, &nfqso, &nftx, &nutc, &nfa, &nfb,
                                 &nzhsym, &ndepth, &emedelay, const_cast<int*> (&ncontest),
                                 &nagain, &lft8apon, &ltry_a8, &lapcqonly, &napwid,
                                 params->mycall, params->hiscall, params->hisgrid, &ldiskdat,
                                 syncs.data (), snrs.data (), dts.data (), freqs.data (),
                                 naps.data (), quals.data (), bits77.data (), decodeds.data (),
                                 &nout);

  int emitted_count = 0;
  ftx_ft8_emit_results_c (&nutc, const_cast<int*> (&ncontest), &nagain, &b_superfox,
                          params->mycall, params->mygrid, temp_dir, syncs.data (),
                          snrs.data (), dts.data (), freqs.data (), naps.data (),
                          quals.data (), decodeds.data (), &nout, &emitted_count);

  if (decoded_count)
    {
      *decoded_count = emitted_count;
    }
}

extern "C" void ftx_q65_decode_and_emit_params_c (short const* iwave,
                                                  params_block_t const* params,
                                                  char const* temp_dir,
                                                  int* decoded_count);
extern "C" void ftx_fst4_decode_and_emit_params_c (short const* iwave,
                                                   params_block_t const* params,
                                                   char const* temp_dir,
                                                   int* decoded_count);

extern "C" void ftx_native_decode_and_emit_params_c (short const* iwave,
                                                     params_block_t const* params,
                                                     char const* temp_dir,
                                                     int* decoded_count)
{
  if (decoded_count)
    {
      *decoded_count = 0;
    }
  if (!iwave || !params || !temp_dir)
    {
      return;
    }

  switch (params->nmode)
    {
    case 2:
      ftx_ft2_decode_and_emit_params_c (iwave, params, temp_dir, decoded_count);
      break;
    case 8:
      ftx_ft8_decode_and_emit_params_c (iwave, params, temp_dir, decoded_count);
      break;
    case 5:
      ftx_ft4_decode_and_emit_params_c (iwave, params, temp_dir, decoded_count);
      break;
    case 66:
      ftx_q65_decode_and_emit_params_c (iwave, params, temp_dir, decoded_count);
      break;
    case 240:
    case 241:
    case 242:
      ftx_fst4_decode_and_emit_params_c (iwave, params, temp_dir, decoded_count);
      break;
    default:
      break;
    }
}
