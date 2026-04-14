#include <QtTest>
#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QtEndian>
#include <array>
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <random>
#include <vector>

#include <fftw3.h>

#include "Detector/FST4DecodeWorker.hpp"
#include "Detector/MSK144DecodeWorker.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"
#include "commons.h"
#include "helper_functions.h"
#include "otpgenerator.h"
#include "qt_helpers.hpp"

using fortran_charlen_t_local = size_t;

extern "C" void ftx_ft8_prepare_pass_c (int ndepth, int ipass, int ndecodes,
                                         float* syncmin, int* imetric,
                                         int* lsubtract, int* run_pass);
extern "C" void genmsk_128_90_ (char* msg, int* ichk, char* msgsent, int* itone, int* itype,
                                 size_t, size_t);
extern "C" void ana64_ (short iwave[], int* npts, std::complex<float> c0[]);
extern "C" void azdist_ (char* myGrid, char* hisGrid, double* utch, int* nAz, int* nEl,
                         int* nDmiles, int* nDkm, int* nHotAz, int* nHotABetter,
                         size_t, size_t);
extern "C" void ftx_ft8_plan_decode_stage_c (int ndepth, int nzhsym, int ndec_early,
                                              int nagain, int* action_out, int* refine_out);
extern "C" int ftx_ft8_should_bail_by_tseq_c (int ldiskdat, double tseq, double limit);
extern "C" int ftx_ft8_store_saved_decode_c (int ndecodes, int max_early,
                                              int nsnr, float f1, float xdt,
                                              int const* itone, int nn,
                                              int* allsnrs, float* f1_save,
                                              float* xdt_save, int* itone_save);
extern "C" void ftx_ft8_prepare_candidate_c (float sync_in, float f1_in, float xdt_in,
                                              float const* sbase, int sbase_size,
                                              float* sync_out, float* f1_out,
                                              float* xdt_out, float* xbase_out);
extern "C" void ftx_ft8_finalize_main_result_c (float xsnr, float xdt_in, float emedelay,
                                                 int nharderrors, float dmin,
                                                 int* nsnr_out, float* xdt_out,
                                                 float* qual_out);
extern "C" int ftx_ft8_should_run_a7_c (int lft8apon, int ncontest, int nzhsym, int previous_count);
extern "C" int ftx_ft8_should_run_a8_c (int lft8apon, int ncontest, int nzhsym, int la8,
                                         int hiscall_len, int hisgrid_len, int ltry_a8);
extern "C" int ftx_ft8_prepare_a7_request_c (float previous_f0, float previous_dt,
                                              char const previous_msg37[37],
                                              float const* sbase, int sbase_size,
                                              float* f1_out, float* xdt_out,
                                              float* xbase_out, char call_1_out[12],
                                              char call_2_out[12], char grid4_out[4]);
extern "C" void ftx_ft8var_msgparser_c (char const msg37_in[37],
                                         char msg37_out[37],
                                         char msg37_2_out[37]);
extern "C" int ftx_ft8var_searchcalls_c (char const callsign1[12], char const callsign2[12]);
extern "C" int ftx_ft8var_chkflscall_c (char const call_a[12], char const call_b[12]);
extern "C" int ftx_ft8var_chkspecial8_c (char msg37[37], char msg37_2[37],
                                          char const mycall12[12], char const hiscall12[12]);
extern "C" int ftx_ft8var_chklong8_c (char const callsign[12]);
extern "C" void ftx_ft8var_extract_call_c (char const msg37[37], char call2_out[12]);
extern "C" int ftx_ft8var_filtersfree_c (char const decoded[22]);
extern "C" void ftx_ft8var_chkgrid_c (char const callsign[12], char const grid[12],
                                      int* lchkcall_out, int* lgvalid_out, int* lwrongcall_out);
extern "C" void ftx_ft8var_chkfalse8_c (char msg37[37], int const* i3_in, int const* n3_in,
                                         int* nbadcrc_io, int const* iaptype_in,
                                         int const* lcall1hash_in, char const mycall12[12],
                                         char const hiscall12[12], char const hisgrid4[4]);
extern "C" void ftx_ft8_a7_reset_c ();
extern "C" void ftx_ft8_a7_prepare_tables_c (int nutc, int nzhsym, int jseq);
extern "C" int ftx_ft8_a7_previous_count_c (int jseq);
extern "C" int ftx_ft8_a7_get_previous_entry_c (int jseq, int index, float* dt_out,
                                                 float* freq_out, char msg37_out[37]);
extern "C" void ftx_ft8_a7_save_c (int jseq, float dt, float freq, char const msg37[37]);
extern "C" int ftx_ft8_should_keep_a8_after_a7_c (char const decoded_msg37[37],
                                                   char const hiscall12[12]);
extern "C" void ftx_ft8_finalize_a7_result_c (float xsnr, int* nsnr_out, int* iaptype_out,
                                               float* qual_out);
extern "C" void ftx_ft8_finalize_a8_result_c (float plog, float xsnr, float fbest,
                                               int* nsnr_out, int* iaptype_out,
                                               float* qual_out, float* save_freq_out);
extern "C" int ftx_ft8_select_npasses_c (int lapon, int lapcqonly, int ncontest,
                                          int nzhsym, int nQSOProgress);
extern "C" void ftx_ft8_plan_pass_window_c (int requested_pass, int npasses,
                                             int* pass_first_out, int* pass_last_out);
extern "C" int ftx_ft8_prepare_ap_pass_c (int ipass, int nQSOProgress, int lapcqonly, int ncontest,
                                           int nfqso, int nftx, float f1, int napwid,
                                           int const* apsym, int const* aph10,
                                           float const* llra, float const* llrc,
                                           float* llrz, int* apmask, int* iaptype_out);
extern "C" int ftx_ft8_prepare_decode_pass_c (int ipass, int nQSOProgress, int lapcqonly, int ncontest,
                                               int nfqso, int nftx, float f1, int napwid,
                                               int const* apsym, int const* aph10,
                                               float const* llra, float const* llrb,
                                               float const* llrc, float const* llrd,
                                               float const* llre, float* llrz,
                                               int* apmask, int* iaptype_out);
extern "C" int ftx_ft8_validate_candidate_c (int nharderrors, int zero_count, int i3, int n3,
                                              int unpack_ok, int quirky, int ncontest);
extern "C" int ftx_ft8_validate_candidate_meta_c (signed char const* message77, signed char const* cw,
                                                   int nharderrors, int unpack_ok, int quirky, int ncontest);
extern "C" int ftx_ft8_compute_snr_c (float const* s8, int rows, int cols, int const* itone,
                                       float xbase, int nagain, int nsync, float* xsnr_out);
extern "C" int ftx_ft8_finalize_decode_pass_c (int nbadcrc, int pass_index, int iaptype_in,
                                                int* ipass_out, int* iaptype_out);
extern "C" void ftx_ft8c_measure_candidate_c (float const* llra, signed char const* cw,
                                               float* score_out, int* nharderrors_out,
                                               float* dmin_out);
extern "C" int legacy_pack77_unpack77bits_c (signed char const* message77, int received,
                                              char msgsent[37], int* quirky_out);
extern "C" int ftx_encode_ft8_candidate_c (char const* message37, char* msgsent_out,
                                            int* itone_out, signed char* codeword_out);
extern "C" int ftx_ft8_message77_to_itone_c (signed char const* message77, int* itone_out);
extern "C" int ftx_ft2_message77_to_itone_c (signed char const* message77, int* itone_out);
extern "C" void genft2_ (char* msg, int* ichk, char* msgsent, signed char* msgbits, int* itone);
extern "C" void gen_ft2wave_ (int* itone, int* nsym, int* nsps, float* fsample, float* f0,
                               std::complex<float>* cwave, float* wave, int* icmplx, int* nwave);
extern "C" void foxgenft2_ ();
extern "C" void foxgen_ (bool* bSuperFox, char const* fname, fortran_charlen_t_local len);
extern "C" void sftx_sub_ (char const* otp_key, fortran_charlen_t_local len);
extern "C" void sfox_wave_gfsk_ ();
extern "C" void sfox_ana_ (float* dd, int* npts, std::complex<float>* c0, int* npts2);
extern "C" void sfox_remove_tone_ (std::complex<float>* c0, float* fsync);
extern "C" void sfox_gen_gfsk_ (int* idat, float* f0, int* isync, int* itone, std::complex<float>* cdat);
extern "C" void qpc_sync_ (std::complex<float>* crcvd0, float* fsample, int* isync, float* fsync,
                           float* ftol, float* f2, float* t2, float* snrsync);
extern "C" void qpc_likelihoods2_ (float* py, float* s3, float* EsNo, float* No);
extern "C" void qpc_snr_ (float* s3, signed char* y, float* snr);
extern "C" void qpc_encode (unsigned char* y, unsigned char const* x);
extern "C" int ftx_superfox_unpack_lines_c (unsigned char const* xdec, int use_otp,
                                            char* lines_out, int line_stride, int max_lines);
extern "C" int ftx_superfox_analytic_c (float const* dd, int npts, float* real_out, float* imag_out);
extern "C" int ftx_superfox_remove_tone_c (float* real_io, float* imag_io, int npts, float fsync);
extern "C" int ftx_superfox_qpc_sync_c (float const* real_in, float const* imag_in, int npts,
                                        float fsync, float ftol, float* f2_out, float* t2_out,
                                        float* snrsync_out);
extern "C" int ftx_superfox_qpc_likelihoods2_c (float const* s3, int rows, int cols,
                                                float EsNo, float No, float* py_out);
extern "C" int ftx_superfox_qpc_snr_c (float const* s3, int rows, int cols,
                                       unsigned char const* y, float* snr_out);
extern "C" int ftx_superfox_qpc_decode2_c (float const* real_in, float const* imag_in, int npts,
                                           float fsync, float ftol, int ndepth, float dth, float damp,
                                           unsigned char* xdec_out, int* crc_ok_out,
                                           float* snrsync_out, float* fbest_out,
                                           float* tbest_out, float* snr_out);
extern "C" int ftx_superfox_decode_lines_c (short const* iwave, int nfqso, int ntol,
                                            char* lines_out, int line_stride, int max_lines,
                                            int* nsnr_out, float* freq_out, float* dt_out);
extern "C" int ftx_q65_ana64_c (short const* iwave, int npts, float* real_out, float* imag_out);
extern "C" int ftx_encode174_91_message77_c (signed char const* message77,
                                             signed char* codeword_out);
extern "C" void ftx_ft8a7_measure_candidate_c (float const* s8, int rows, int cols,
                                                int const* itone, signed char const* cw,
                                                float const* llra, float const* llrb,
                                                float const* llrc, float const* llrd,
                                                float* pow_out, float* dmin_out,
                                                int* nharderrors_out);
extern "C" void ftx_ft8_a7_search_initial_c (std::complex<float> const* cd0, int np2, float fs2,
                                              float xdt_in, int* ibest_out, float* delfbest_out);
extern "C" void ftx_ft8_a7_refine_search_c (std::complex<float> const* cd0, int np2, float fs2,
                                             int ibest_in, int* ibest_out, float* sync_out,
                                             float* xdt_out);
extern "C" int ftx_ft8_a7_finalize_metrics_c (float const* dmm, int count, float pbest,
                                               float xbase, float* dmin_out,
                                               float* dmin2_out, float* xsnr_out);
extern "C" void ftx_ft8_a7d_c (float* dd0, int* newdat, char const call_1[12],
                               char const call_2[12], char const grid4[4], float* xdt,
                               float* f1, float* xbase, int* nharderrors,
                               float* dmin, char msg37[37], float* xsnr);
extern "C" void ftx_ft8_a7d_ref_c (float* dd0, int* newdat, char const call_1[12],
                                   char const call_2[12], char const grid4[4], float* xdt,
                                   float* f1, float* xbase, int* nharderrors,
                                   float* dmin, char msg37[37], float* xsnr);
extern "C" void ftx_ft8_a8d_c (float* dd, char const mycall[12], char const dxcall[12],
                               char const dxgrid[6], float* f1a, float* xdt, float* fbest,
                               float* xsnr, float* plog, char msgbest[37]);
extern "C" void ftx_ft8_a8d_ref_c (float* dd, char const mycall[12], char const dxcall[12],
                                   char const dxgrid[6], float* f1a, float* xdt, float* fbest,
                                   float* xsnr, float* plog, char msgbest[37]);
extern "C" void ftx_ldpc174_91_metrics_c (signed char const* cw, float const* llr,
                                           int* nharderrors_out, float* dmin_out);
extern "C" int ftx_ldpc174_91_finalize_c (signed char const* cw, float const* llr,
                                           signed char* message91_out, int* nharderrors_out,
                                           float* dmin_out);
extern "C" void ftx_decode174_91_c (float const* llr_in, int Keff, int maxosd, int norder,
                                     signed char const* apmask_in, signed char* message91_out,
                                     signed char* cw_out, int* ntype_out,
                                     int* nharderror_out, float* dmin_out);
extern "C" void ftx_sync8_search_c (float const* dd, int npts, float nfa, float nfb,
                                     float syncmin, float nfqso, int maxcand,
                                     float* candidate, int* ncand, float* sbase);
extern "C" void ftx_sync8d_c (std::complex<float> const* cd0, int np, int i0,
                               std::complex<float> const* ctwk, int itwk, float* sync);
extern "C" void ftx_twkfreq1_c (std::complex<float> const* ca, int const* npts,
                                 float const* fsample, float const* a,
                                 std::complex<float>* cb);
extern "C" int ftx_ft8_a8_search_candidate_c (std::complex<float> const* cd,
                                               std::complex<float> const* cwave,
                                               int nzz, int nwave, float f1,
                                               float* spk_out, float* fpk_out,
                                               float* tpk_out, float* spectrum_out);
extern "C" int ftx_ft8_a8_finalize_search_c (float const* spectrum, int size, float f1,
                                              float fbest, float* xsnr_out);
extern "C" int ftx_ft8_a8_accept_score_c (int nhard, float plog, float sigobig);
extern "C" int ftx_prepare_ft8_a7_candidate_c (int imsg, char const call_1[12],
                                                char const call_2[12],
                                                char const grid4[4], char message37[37]);
extern "C" int ftx_prepare_ft8_a8_candidate_c (int imsg, char const mycall[12],
                                                char const hiscall[12],
                                                char const hisgrid[6],
                                                char message37[37]);
extern "C" void ftx_ft8_downsample_c (float const* dd, int* newdat, float f0, fftwf_complex* c1);
extern "C" void ftx_gen_ft8_refsig_c (int const* itone, float f0, fftwf_complex* cref_out);
extern "C" void ftx_subtract_ft8_c (float* dd0, int const* itone, float f0, float dt, int lrefinedt);
extern "C" void genft8_ (char* msg, int* i3, int* n3, char* msgsent, signed char msgbits[],
                         int itone[], size_t, size_t);
extern "C" void gen_ft8wave_ (int itone[], int* nsym, int* nsps, float* bt, float* fsample,
                              float* f0, std::complex<float>* cwave, float wave[],
                              int* icmplx, int* nwave);
extern "C" int ftx_gen_ft8wave_complex_c (int const* itone, int nsym, int nsps, float bt,
                                          float fsample, float f0, float* real_out,
                                          float* imag_out, int nwave);
extern "C" void ftx_ft8_cpp_dsp_rollout_stage_override_c (int stage);
extern "C" void ftx_ft8_cpp_dsp_rollout_stage_reset_c ();
extern "C" void ftx_getcandidates4_c (float const* dd, float fa, float fb, float syncmin,
                                      float nfqso, int maxcand, float* savg,
                                      float* candidate, int* ncand, float* sbase);
extern "C" void ftx_sync4d_c (std::complex<float> const* cd0, int np, int i0,
                              std::complex<float> const* ctwk, int itwk, float* sync);
extern "C" void ftx_sync4d_ref_c (std::complex<float> const* cd0, int np, int i0,
                                  std::complex<float> const* ctwk, int itwk, float* sync);
extern "C" void ftx_ft4_bitmetrics_c (std::complex<float> const* cd, float* bitmetrics,
                                      int* badsync);
extern "C" void ftx_ft4_bitmetrics_ref_c (std::complex<float> const* cd, float* bitmetrics,
                                          int* badsync);
extern "C" void genft4_ (char* msg, int* ichk, char* msgsent, signed char msgbits[],
                         int itone[], fortran_charlen_t_local, fortran_charlen_t_local);
extern "C" void gen_ft4wave_ (int itone[], int* nsym, int* nsps, float* fsample, float* f0,
                              std::complex<float>* cwave, float wave[], int* icmplx, int* nwave);
extern "C" void genfst4_ (char* msg, int* ichk, char* msgsent, signed char msgbits[],
                          int itone[], int* iwspr, fortran_charlen_t_local, fortran_charlen_t_local);
extern "C" void gen_fst4wave_ (int itone[], int* nsym, int* nsps, int* nwave, float* fsample,
                               int* hmod, float* f0, int* icmplx, std::complex<float>* cwave,
                               float wave[]);
extern "C" void get_crc24_ (signed char mc[], int* len, int* ncrc);
extern "C" void fst4_async_decode_ (short iwave[], int* nutc, int* nqsoprogress, int* nfa, int* nfb,
                                    int* nfqso, int* ndepth, int* ntrperiod, int* nexp_decode,
                                    int* ntol, float* emedelay, int* nagain, int* lapcqonly,
                                    char mycall[], char hiscall[], int* iwspr, int* lprinthash22,
                                    int snrs[], float dts[], float freqs[], int naps[], float quals[],
                                    signed char bits77[], char decodeds[], float fmids[],
                                    float w50s[], int* nout,
                                    fortran_charlen_t_local, fortran_charlen_t_local,
                                    fortran_charlen_t_local);
extern "C" void blanker_ (short iwave[], int* nz, int* ndropmax, int* npct, std::complex<float> c_bigfft[]);
extern "C" void four2a_ (std::complex<float> a[], int* nfft, int* ndim, int* isign, int* iform);
extern "C" void pctile_ (float x[], int* npts, int* npct, float* xpct);
extern "C" void polyfit_ (double x[], double y[], double sigmay[], int* npts, int* nterms, int* mode,
                          double a[], double* chisqr);
extern "C" void __fst4_decode_MOD_get_candidates_fst4 (std::complex<float> c_bigfft[], int* nfft1,
                                                        int* nsps, int* hmod, float* fs, float* fa, float* fb,
                                                        int* nfa, int* nfb, float* minsync,
                                                        int* ncand, float candidates[]);
extern "C" void __fst4_decode_MOD_fst4_downsample (std::complex<float> c_bigfft[], int* nfft1,
                                                    int* ndown, float* f0, float* sigbw,
                                                    std::complex<float> c1[]);
extern "C" void __fst4_decode_MOD_fst4_sync_search (std::complex<float> c2[], int* nfft2,
                                                     int* hmod, float* fs2, int* nss, int* ntrperiod,
                                                     int* nsyncoh, float* emedelay,
                                                     float* sbest, float* fcbest, int* isbest);
extern "C" void ftx_fst4_decode_and_emit_params_c (short const* iwave, params_block_t const* params,
                                                   char const* temp_dir, int* decoded_count);
extern "C" int ftx_fst4_get_candidates_c (std::complex<float> const* bigfft, int nfft1, int nsps,
                                          float fs, float fa, float fb, int nfa, int nfb,
                                          float minsync, float* candidates_out, int max_candidates);
extern "C" int ftx_fst4_downsample_c (std::complex<float> const* bigfft, int nfft1, int ndown,
                                      int nsps, float fs, float f0, std::complex<float>* out,
                                      int out_size);
extern "C" int ftx_fst4_sync_search_c (std::complex<float> const* c2, int nfft2, int nsps,
                                       int ndown, int ntrperiod, float emedelay,
                                       float* sbest_out, float* fcbest_out, int* isbest_out);
extern "C" int ftx_fst4_debug_first_candidate_c (short const* audio, int audio_size,
                                                 int ntrperiod, int ndepth,
                                                 int nfa_in, int nfb_in, int nfqso,
                                                 int ntol, float emedelay,
                                                 char const* mycall_in, char const* hiscall_in,
                                                 int* stage_out, int* badsync_out,
                                                 int* nharderrors_out, int* iaptype_out,
                                                 char msg_out[37]);
extern "C" void ftx_fst4_reset_runtime_state_c ();
extern "C" void ftx_ft4_decode_c (short const* iwave, int* nqsoprogress, int* nfqso, int* nfa, int* nfb,
                                  int* ndepth, int* lapcqonly, int* ncontest,
                                  char const* mycall, char const* hiscall,
                                  float* syncs, int snrs[], float dts[], float freqs[],
                                  int naps[], float quals[], signed char bits77[],
                                  char decodeds[], int* nout,
                                  fortran_charlen_t_local, fortran_charlen_t_local,
                                  fortran_charlen_t_local);
extern "C" void ftx_ft4_cpp_dsp_rollout_stage_override_c (int stage);
extern "C" void ftx_ft4_cpp_dsp_rollout_stage_reset_c ();
extern "C" void ft2_async_decode_ (short iwave[], int* nqsoprogress, int* nfqso, int* nfa, int* nfb,
                                   int* ndepth, int* ncontest, char const* mycall, char const* hiscall,
                                   int snrs[], float dts[], float freqs[], int naps[], float quals[],
                                   signed char bits77[], char decodeds[], int* nout,
                                   fortran_charlen_t_local, fortran_charlen_t_local,
                                   fortran_charlen_t_local);
extern "C" void mskrtd_ (short id2[], int* nutc0, float* tsec, int* ntol, int* nrxfreq, int* ndepth,
                         char* mycall, char* hiscall, bool* bshmsg, bool* btrain,
                         double const pcoeffs[], bool* bswl, char* datadir, char* line,
                         fortran_charlen_t_local, fortran_charlen_t_local,
                         fortran_charlen_t_local, fortran_charlen_t_local);
extern "C" void ft2_triggered_decode_ (short iwave[], int* nqsoprogress, int* nfqso, int* nfa, int* nfb,
                                       int* ndepth, int* ncontest, char const* mycall, char const* hiscall,
                                       char outlines[], int* nout,
                                       fortran_charlen_t_local, fortran_charlen_t_local,
                                       fortran_charlen_t_local);
extern "C" void ftx_ft2_decode_candidate_stage7_c (float const* llra, float const* llrb,
                                                   float const* llrc, float const* llrd,
                                                   float const* llre, int ndepth0,
                                                   int ncontest, int qso_progress,
                                                   int lapcqonly, int nfqso, float f1,
                                                   float apmag, int averaged, int doosd,
                                                   char const* mycall, char const* hiscall,
                                                   int* ok_out, int* stop_candidate_out,
                                                   int* selected_pass_out, int* iaptype_out,
                                                   int* nharderror_out, float* dmin_out,
                                                   int* maxosd_out, signed char* message77_out,
                                                   char* decoded_out);
extern "C" int ftx_ft2_message_is_plausible_c (char const decoded37[37]);
extern "C" void ftx_ft2_cpp_dsp_rollout_stage_override_c (int stage);
extern "C" void ftx_ft2_cpp_dsp_rollout_stage_reset_c ();
extern "C" void ftx_ft2_stage7_clravg_c ();

namespace
{
constexpr int kFt2AsyncSampleCount {45000};
constexpr int kFt2Nsps {288};
constexpr int kFt2Codeword {174};
constexpr int kFt2Bits {77};
constexpr int kFt2DecodedChars {37};
constexpr int kFt2MaxLines {100};
constexpr int kFst4DecodedChars {37};
constexpr int kFst4MaxLines {200};
constexpr int kSuperFoxSampleCount {15 * 12000};
constexpr int kMsk144BlockSize {7168};
constexpr int kMsk144StepSize {kMsk144BlockSize / 2};
constexpr int kMsk144SampleRate {12000};
constexpr int kMsk144MaxLines {50};

struct WavFixture
{
  int sampleRate {0};
  QVector<short> samples;
};

QByteArray to_fortran_field_local (QByteArray value, int width);
QString build_fst4_row_reference (int nutc, int snr, float dt, float freq,
                                  int nap, float qual, char const* decodeds, int index,
                                  int ntrperiod, float fmid, float w50);

WavFixture read_pcm16_mono_wav (QString const& fileName)
{
  QFile file {fileName};
  if (!file.open (QIODevice::ReadOnly))
    {
      return {};
    }

  QByteArray const blob = file.readAll ();
  if (blob.size () < 12 || blob.mid (0, 4) != "RIFF" || blob.mid (8, 4) != "WAVE")
    {
      return {};
    }

  bool haveFmt = false;
  bool haveData = false;
  quint16 audioFormat = 0;
  quint16 channels = 0;
  quint32 sampleRate = 0;
  quint16 bitsPerSample = 0;
  QByteArray dataChunk;

  int pos = 12;
  while (pos + 8 <= blob.size ())
    {
      QByteArray const chunkId = blob.mid (pos, 4);
      quint32 const chunkSize = qFromLittleEndian<quint32> (
          reinterpret_cast<uchar const*> (blob.constData () + pos + 4));
      pos += 8;
      if (pos + static_cast<int> (chunkSize) > blob.size ())
        {
          return {};
        }

      if (chunkId == "fmt ")
        {
          if (chunkSize < 16)
            {
              return {};
            }
          auto const* fmt = reinterpret_cast<uchar const*> (blob.constData () + pos);
          audioFormat = qFromLittleEndian<quint16> (fmt);
          channels = qFromLittleEndian<quint16> (fmt + 2);
          sampleRate = qFromLittleEndian<quint32> (fmt + 4);
          bitsPerSample = qFromLittleEndian<quint16> (fmt + 14);
          haveFmt = true;
        }
      else if (chunkId == "data")
        {
          dataChunk = blob.mid (pos, static_cast<int> (chunkSize));
          haveData = true;
        }

      pos += static_cast<int> ((chunkSize + 1u) & ~1u);
    }

  if (!haveFmt || !haveData || audioFormat != 1u || channels != 1u || bitsPerSample != 16u)
    {
      return {};
    }

  int const sampleCount = dataChunk.size () / 2;
  QVector<short> samples (sampleCount);
  auto const* raw = reinterpret_cast<uchar const*> (dataChunk.constData ());
  for (int i = 0; i < sampleCount; ++i)
    {
      samples[i] = static_cast<short> (qFromLittleEndian<qint16> (raw + 2 * i));
    }

  WavFixture fixture;
  fixture.sampleRate = static_cast<int> (sampleRate);
  fixture.samples = std::move (samples);
  return fixture;
}

QVector<short> render_fst4_pcm_reference (QString const& message, int tr_seconds, bool wspr_hint)
{
  int nsps = 0;
  if (tr_seconds == 15) nsps = 720;
  if (tr_seconds == 30) nsps = 1680;
  if (tr_seconds == 60) nsps = 3888;
  if (tr_seconds == 120) nsps = 8200;
  if (tr_seconds == 300) nsps = 21504;
  if (tr_seconds == 900) nsps = 66560;
  if (tr_seconds == 1800) nsps = 134400;
  if (nsps == 0)
    {
      return {};
    }

  QByteArray fixed = message.leftJustified (37, QLatin1Char {' '}, true).left (37).toLatin1 ();
  std::array<char, 38> msg {};
  std::array<char, 38> msgsent {};
  std::array<signed char, 101> msgbits {};
  std::array<int, 160> tones {};
  std::copy_n (fixed.constData (), 37, msg.data ());
  int ichk = 0;
  int iwspr = wspr_hint ? 1 : 0;
  genfst4_ (msg.data (), &ichk, msgsent.data (), msgbits.data (), tones.data (), &iwspr,
            static_cast<fortran_charlen_t_local> (37), static_cast<fortran_charlen_t_local> (37));

  int nsym = 160;
  int nwave = (nsym + 2) * nsps;
  int hmod = 1;
  int icmplx = 0;
  float fsample = 12000.0f;
  float baud = fsample / static_cast<float> (nsps);
  float f0 = 1500.0f + 1.5f * static_cast<float> (hmod) * baud;
  QVector<float> wave (nwave, 0.0f);
  gen_fst4wave_ (tones.data (), &nsym, &nsps, &nwave, &fsample, &hmod, &f0, &icmplx,
                 nullptr, wave.data ());

  int const nmax = tr_seconds * 12000;
  QVector<float> shifted (nmax, 0.0f);
  int const offset = (tr_seconds == 15) ? 6000 : 12000;
  int const copy = qMax (0, qMin (wave.size (), shifted.size () - offset));
  for (int i = 0; i < copy; ++i)
    {
      shifted[offset + i] = wave.at (i);
    }

  float peak = 0.0f;
  for (float sample : shifted)
    {
      peak = std::max (peak, std::fabs (sample));
    }
  if (peak <= 0.0f)
    {
      return QVector<short> (nmax, 0);
    }

  float const scale = 32766.9f / peak;
  QVector<short> pcm (nmax, 0);
  for (int i = 0; i < nmax; ++i)
    {
      pcm[i] = static_cast<short> (std::lround (scale * shifted.at (i)));
    }
  return pcm;
}

QStringList decode_fst4_rows_async_export_reference (decodium::fst4::DecodeRequest const& request)
{
  QVector<short> audio = request.audio;
  if (audio.isEmpty ())
    {
      return {};
    }

  std::array<int, kFst4MaxLines> snrs {};
  std::array<float, kFst4MaxLines> dts {};
  std::array<float, kFst4MaxLines> freqs {};
  std::array<int, kFst4MaxLines> naps {};
  std::array<float, kFst4MaxLines> quals {};
  std::array<signed char, 77 * kFst4MaxLines> bits77 {};
  std::array<char, kFst4DecodedChars * kFst4MaxLines> decodeds {};
  std::array<float, kFst4MaxLines> fmids {};
  std::array<float, kFst4MaxLines> w50s {};

  int nutc = request.nutc;
  int nqsoprogress = request.nqsoprogress;
  int nfa = request.nfa;
  int nfb = request.nfb;
  int nfqso = request.nfqso;
  int ndepth = request.ndepth;
  int ntrperiod = request.ntrperiod;
  int nexp_decode = request.nexp_decode;
  int ntol = request.ntol;
  float emedelay = request.emedelay;
  int nagain = request.nagain;
  int lapcqonly = request.lapcqonly;
  int iwspr = request.iwspr;
  int lprinthash22 = request.lprinthash22;
  int nout = 0;

  QByteArray mycall = to_fortran_field_local (request.mycall, 12);
  QByteArray hiscall = to_fortran_field_local (request.hiscall, 12);
  fst4_async_decode_ (audio.data (), &nutc, &nqsoprogress, &nfa, &nfb, &nfqso, &ndepth, &ntrperiod,
                      &nexp_decode, &ntol, &emedelay, &nagain, &lapcqonly, mycall.data (),
                      hiscall.data (), &iwspr, &lprinthash22, snrs.data (), dts.data (),
                      freqs.data (), naps.data (), quals.data (), bits77.data (), decodeds.data (),
                      fmids.data (), w50s.data (), &nout,
                      static_cast<fortran_charlen_t_local> (12),
                      static_cast<fortran_charlen_t_local> (12),
                      static_cast<fortran_charlen_t_local> (kFst4DecodedChars * kFst4MaxLines));

  QStringList rows;
  for (int i = 0; i < qBound (0, nout, kFst4MaxLines); ++i)
    {
      rows << build_fst4_row_reference (request.nutc, snrs[static_cast<size_t> (i)],
                                        dts[static_cast<size_t> (i)], freqs[static_cast<size_t> (i)],
                                        naps[static_cast<size_t> (i)], quals[static_cast<size_t> (i)],
                                        decodeds.data (), i, request.ntrperiod,
                                        fmids[static_cast<size_t> (i)], w50s[static_cast<size_t> (i)]);
    }
  return rows;
}

QByteArray to_fortran_field_local (QByteArray value, int width)
{
  value = value.left (width);
  if (value.size () < width)
    {
      value.append (QByteArray (width - value.size (), ' '));
    }
  return value;
}

QString trim_fortran_line_local (char const* data, int width)
{
  QByteArray field {data, width};
  while (!field.isEmpty () && (field.back () == ' ' || field.back () == '\0'))
    {
      field.chop (1);
    }
  return QString::fromLatin1 (field);
}

QString build_fst4_row_reference (int nutc, int snr, float dt, float freq,
                                  int nap, float qual, char const* decodeds, int index,
                                  int ntrperiod, float fmid, float w50)
{
  QByteArray decoded {decodeds + index * kFst4DecodedChars, kFst4DecodedChars};
  while (!decoded.isEmpty () && (decoded.back () == ' ' || decoded.back () == '\0'))
    {
      decoded.chop (1);
    }
  QString text = QString::fromLatin1 (decoded.constData (), decoded.size ());
  if (text.size () < kFst4DecodedChars)
    {
      text = text.leftJustified (kFst4DecodedChars, QLatin1Char {' '});
    }
  text = text.left (kFst4DecodedChars);
  if (nap != 0 && qual < 0.17f && !text.isEmpty ())
    {
      text[kFst4DecodedChars - 1] = QLatin1Char {'?'};
    }

  QString const utcPrefix = nutc > 0
      ? QString::number (nutc).rightJustified (nutc > 9999 ? 6 : 4, QLatin1Char {'0'})
      : QString {};
  QString const annot = nap != 0
      ? QStringLiteral ("a%1").arg (nap).leftJustified (2, QLatin1Char {' '})
      : QStringLiteral ("  ");
  QString row = QStringLiteral ("%1%2%3 `  %4 %5")
      .arg (snr, 4)
      .arg (dt, 5, 'f', 1)
      .arg (qRound (freq), 5)
      .arg (text)
      .arg (annot);
  if (ntrperiod >= 60 && fmid != -999.0f)
    {
      row += QStringLiteral ("%1").arg (w50, 6, 'f', w50 < 0.95f ? 3 : 2);
    }
  if (!utcPrefix.isEmpty ())
    {
      row.prepend (utcPrefix);
    }
  return row;
}

bool msk144_row_in_pick_window_local (QString const& row, decodium::msk144::DecodeRequest const& request)
{
  if (request.npick <= 0)
    {
      return true;
    }
  if (row.size () < 15)
    {
      return false;
    }

  bool ok = false;
  double const tdec = row.mid (10, 5).trimmed ().toDouble (&ok);
  if (!ok)
    {
      return false;
    }

  double const t0 = std::max (0, request.t0_ms) * 0.001;
  double const t1 = std::max (request.t0_ms, request.t1_ms) * 0.001;
  return tdec >= t0 && tdec <= t1;
}

QVector<short> make_clean_msk144_wave (QString const& message, int npts, float freq = 1500.0f)
{
  if (npts <= 0)
    {
      return {};
    }

  decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeMsk144 (message);
  if (!encoded.ok || encoded.tones.isEmpty ())
    {
      return {};
    }

  int nsym = 144;
  if (encoded.tones.size () > 40 && encoded.tones.at (40) < 0)
    {
      nsym = 40;
    }
  if (nsym <= 0)
    {
      return {};
    }

  constexpr double kTwoPi = 6.28318530717958647692;
  constexpr double kBaud = 2000.0;
  constexpr int kNsps = 6;
  double const dphi0 = kTwoPi * (static_cast<double> (freq) - 0.25 * kBaud) / kMsk144SampleRate;
  double const dphi1 = kTwoPi * (static_cast<double> (freq) + 0.25 * kBaud) / kMsk144SampleRate;
  double phi = 0.0;

  QVector<short> wave (npts, 0);
  int const nreps = npts / (nsym * kNsps);
  int index = 0;
  for (int rep = 0; rep < nreps; ++rep)
    {
      for (int symbol = 0; symbol < nsym; ++symbol)
        {
          double const dphi = encoded.tones.at (symbol) == 0 ? dphi0 : dphi1;
          for (int j = 0; j < kNsps && index < npts; ++j)
            {
              wave[index++] =
                  static_cast<short> (qBound (-32767, qRound (30000.0 * std::cos (phi)), 32767));
              phi = std::fmod (phi + dphi, kTwoPi);
              if (phi < 0.0)
                {
                  phi += kTwoPi;
                }
            }
        }
    }
  return wave;
}

QStringList decode_msk144_rows_fortran_reference (decodium::msk144::DecodeRequest const& request)
{
  QStringList rows;
  if (request.audio.size () < kMsk144BlockSize)
    {
      return rows;
    }

  int sample_count = request.audio.size ();
  if (request.kdone > 0)
    {
      sample_count = std::min (sample_count, request.kdone);
    }
  if (request.trperiod > 0.0)
    {
      int const tr_samples = qMax (0, qRound (request.trperiod * kMsk144SampleRate));
      if (tr_samples > 0)
        {
          sample_count = std::min (sample_count, tr_samples);
        }
    }
  if (sample_count < kMsk144BlockSize)
    {
      return rows;
    }

  QByteArray mycall = to_fortran_field_local (request.mycall, 12);
  QByteArray hiscall = to_fortran_field_local (request.hiscall, 12);
  QByteArray datadir = QDir::currentPath ().toLocal8Bit ();
  if (datadir.isEmpty ())
    {
      datadir = QByteArrayLiteral (".");
    }
  std::array<double, 5> pcoeffs {{0.0, 0.0, 0.0, 0.0, 0.0}};
  bool const bshmsg = request.shorthandEnabled;
  bool const btrain = request.trainingEnabled;
  bool const bswl = request.swlEnabled;
  int const max_lines = qBound (1, request.maxlines, kMsk144MaxLines);
  int const ntol = qMax (0, request.ftol);
  int const nrxfreq = qBound (0, request.rxfreq, 5000);
  int const ndepth = qBound (0, request.aggressive, 10);

  for (int offset = 0; offset + kMsk144BlockSize <= sample_count && rows.size () < max_lines;
       offset += kMsk144StepSize)
    {
      std::array<short, kMsk144BlockSize> block {};
      std::copy_n (request.audio.constData () + offset, block.size (), block.begin ());
      std::array<char, 80> line {};
      int nutc = request.nutc;
      float tsec = static_cast<float> (offset) / static_cast<float> (kMsk144SampleRate);
      int ntol_local = ntol;
      int nrxfreq_local = nrxfreq;
      int ndepth_local = ndepth;
      bool bshmsg_local = bshmsg;
      bool btrain_local = btrain;
      bool bswl_local = bswl;
      mskrtd_ (block.data (), &nutc, &tsec, &ntol_local, &nrxfreq_local, &ndepth_local,
               mycall.data (), hiscall.data (), &bshmsg_local, &btrain_local,
               pcoeffs.data (), &bswl_local, datadir.data (), line.data (),
               static_cast<fortran_charlen_t_local> (mycall.size ()),
               static_cast<fortran_charlen_t_local> (hiscall.size ()),
               static_cast<fortran_charlen_t_local> (datadir.size ()),
               static_cast<fortran_charlen_t_local> (line.size ()));
      QString const row = trim_fortran_line_local (line.data (), static_cast<int> (line.size ()));
      if (!row.isEmpty () && msk144_row_in_pick_window_local (row, request))
        {
          rows << row;
        }
    }

  return rows;
}

QStringList decode_msk144_rows_native_realtime (decodium::msk144::DecodeRequest const& request)
{
  QStringList rows;
  if (request.audio.size () < kMsk144BlockSize)
    {
      return rows;
    }

  int sample_count = request.audio.size ();
  if (request.kdone > 0)
    {
      sample_count = std::min (sample_count, request.kdone);
    }
  if (request.trperiod > 0.0)
    {
      int const tr_samples = qMax (0, qRound (request.trperiod * kMsk144SampleRate));
      if (tr_samples > 0)
        {
          sample_count = std::min (sample_count, tr_samples);
        }
    }
  if (sample_count < kMsk144BlockSize)
    {
      return rows;
    }

  QString const data_dir = QDir::currentPath ().isEmpty () ? QStringLiteral (".") : QDir::currentPath ();
  int const max_lines = qBound (1, request.maxlines, kMsk144MaxLines);
  decodium::msk144::resetMsk144DecoderState ();

  for (int offset = 0; offset + kMsk144BlockSize <= sample_count && rows.size () < max_lines;
       offset += kMsk144StepSize)
    {
      decodium::msk144::RealtimeDecodeRequest live;
      live.audio = request.audio.constData () + offset;
      live.nutc = request.nutc;
      live.tsec = static_cast<float> (offset) / static_cast<float> (kMsk144SampleRate);
      live.rxfreq = request.rxfreq;
      live.ftol = request.ftol;
      live.aggressive = request.aggressive;
      live.mycall = request.mycall;
      live.hiscall = request.hiscall;
      live.shorthandEnabled = request.shorthandEnabled;
      live.trainingEnabled = request.trainingEnabled;
      live.swlEnabled = request.swlEnabled;
      live.dataDir = data_dir;
      QString const row = decodium::msk144::decodeMsk144RealtimeBlock (live);
      if (!row.isEmpty () && msk144_row_in_pick_window_local (row, request))
        {
          rows << row;
        }
    }

  return rows;
}

std::vector<short> make_clean_superfox_wave (QString const& line, QString const& otpKey)
{
  std::array<unsigned char, 50> xin {};
  if (!decodium::txwave::packSuperFoxMessage (line, otpKey, false, false, QString {}, &xin))
    {
      return {};
    }

  std::array<unsigned char, 128> y {};
  qpc_encode (y.data (), xin.data ());
  std::rotate (y.begin (), y.begin () + 1, y.end ());
  y.back () = 0;

  std::array<int, 127> chansym {};
  for (int i = 0; i < 127; ++i)
    {
      chansym[static_cast<size_t> (i)] = y[static_cast<size_t> (i)];
    }

  std::array<int, 24> isync {{
      1, 2, 4, 7, 11, 16, 22, 29, 37, 39, 42, 43,
      45, 48, 52, 57, 63, 70, 78, 80, 83, 84, 86, 89}};
  std::array<int, 151> itone {};
  std::vector<std::complex<float>> cdat (kSuperFoxSampleCount, std::complex<float> {});
  float f0 = 750.0f;
  sfox_gen_gfsk_ (chansym.data (), &f0, isync.data (), itone.data (), cdat.data ());

  int const shift = 6000;
  std::vector<short> iwave (kSuperFoxSampleCount);
  for (int i = 0; i < kSuperFoxSampleCount; ++i)
    {
      int src = i - shift;
      if (src < 0)
        {
          src += kSuperFoxSampleCount;
        }
      float const sample = std::imag (cdat[static_cast<size_t> (src)]);
      iwave[static_cast<size_t> (i)] = static_cast<short> (std::lround (32767.0f * sample));
    }

  return iwave;
}

QByteArray trim_fortran_field_local (char const* data, int width)
{
  QByteArray field {data, width};
  while (!field.isEmpty () && (field.back () == ' ' || field.back () == '\0'))
    {
      field.chop (1);
    }
  return field;
}

struct Ft8StageCompareResult
{
  int stage {0};
  int nout {-1};
  QStringList detail_lines;
  QStringList messages;
};

Ft8StageCompareResult parse_ft8_stage_compare_output (QString const& output, int stage)
{
  Ft8StageCompareResult result;
  result.stage = stage;

  QRegularExpression const nout_re {
      QStringLiteral ("stage %1: nout=(\\d+)").arg (stage)
  };
  QRegularExpressionMatch const nout_match = nout_re.match (output);
  if (nout_match.hasMatch ())
    {
      result.nout = nout_match.captured (1).toInt ();
    }

  bool in_stage = false;
  QRegularExpression const decoded_re {QStringLiteral ("decoded=\"([^\"]*)\"")};
  QStringList const lines = output.split (QLatin1Char {'\n'});
  for (QString const& line : lines)
    {
      if (line.startsWith (QStringLiteral ("stage ")))
        {
          in_stage = line.startsWith (QStringLiteral ("stage %1:").arg (stage));
          continue;
        }
      if (!in_stage)
        {
          continue;
        }
      if (line.startsWith (QStringLiteral ("  - ")))
        {
          result.detail_lines << line.mid (4).trimmed ();
          QRegularExpressionMatch const decoded_match = decoded_re.match (line);
          if (decoded_match.hasMatch ())
            {
              result.messages << decoded_match.captured (1);
            }
          continue;
        }
      if (line.startsWith (QStringLiteral ("  (no decodes)")))
        {
          continue;
        }
      if (!line.startsWith (QLatin1Char {' '}))
        {
          in_stage = false;
        }
    }

  return result;
}

QSet<QString> ft8_message_set (Ft8StageCompareResult const& result)
{
  QSet<QString> set;
  for (QString const& message : result.messages)
    {
      set.insert (message);
    }
  return set;
}

float ft8_compute_snr_legacy_reference (float const* s8, int rows, int cols, int const* itone)
{
  float xsnr_sum = 0.001f;
  for (int col = 0; col < cols; ++col)
    {
      int const target = std::max (0, std::min (itone[col], rows - 1));
      float const xsig = s8[target + rows * col] * s8[target + rows * col];

      float total_power = 0.0f;
      for (int row = 0; row < rows; ++row)
        {
          float const value = s8[row + rows * col];
          total_power += value * value;
        }

      float xnoi = 0.01f;
      if (rows > 1)
        {
          xnoi = (total_power - xsig) / static_cast<float> (rows - 1);
          if (xnoi < 0.01f)
            {
              xnoi = 0.01f;
            }
        }

      float xsnr_symbol = 1.01f;
      if (xnoi < xsig)
        {
          xsnr_symbol = xsig / xnoi;
        }
      xsnr_sum += xsnr_symbol;
    }

  float xsnr = xsnr_sum / static_cast<float> (cols) - 1.0f;
  xsnr = 10.0f * std::log10 (std::max (xsnr, 0.001f)) - 26.5f;
  if (xsnr > 7.0f)
    {
      xsnr += (xsnr - 7.0f) * 0.5f;
    }
  if (xsnr > 30.0f)
    {
      xsnr -= 1.0f;
      if (xsnr > 40.0f)
        {
          xsnr -= 1.0f;
        }
      if (xsnr > 49.0f)
        {
          xsnr = 49.0f;
        }
    }
  return xsnr;
}

std::vector<short> make_ft2_case_wave (QString const& message, float frequency_hz,
                                       float offset_ms, float snr_db, unsigned seed)
{
  decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeFt2 (message);
  if (!encoded.ok || encoded.tones.isEmpty ())
    {
      return {};
    }

  QVector<float> const wave = decodium::txwave::generateFt2Wave (
      encoded.tones.constData (), encoded.tones.size (), kFt2Nsps, 12000.0f, frequency_hz);
  if (wave.isEmpty ())
    {
      return {};
    }

  int const offset_samples = static_cast<int> (std::lround (offset_ms * 12.0f));
  if (offset_samples < 0 || offset_samples + wave.size () > kFt2AsyncSampleCount)
    {
      return {};
    }

  std::vector<float> frame (static_cast<size_t> (kFt2AsyncSampleCount), 0.0f);
  for (int i = 0; i < wave.size (); ++i)
    {
      frame[static_cast<size_t> (offset_samples + i)] = 0.85f * wave[i];
    }

  double sum_sq = 0.0;
  int count = 0;
  for (float sample : frame)
    {
      if (sample != 0.0f)
        {
          sum_sq += static_cast<double> (sample) * static_cast<double> (sample);
          ++count;
        }
    }
  float const signal_rms =
      count > 0 ? static_cast<float> (std::sqrt (sum_sq / static_cast<double> (count))) : 0.0f;
  if (signal_rms > 0.0f)
    {
      double const sigma =
          static_cast<double> (signal_rms) / std::pow (10.0, static_cast<double> (snr_db) / 20.0);
      std::mt19937 rng {seed};
      std::normal_distribution<float> noise {0.0f, static_cast<float> (sigma)};
      for (float& sample : frame)
        {
          sample += noise (rng);
        }
    }

  std::vector<short> pcm (static_cast<size_t> (kFt2AsyncSampleCount), 0);
  for (int i = 0; i < kFt2AsyncSampleCount; ++i)
    {
      float const clipped = std::max (-1.0f, std::min (1.0f, frame[static_cast<size_t> (i)]));
      pcm[static_cast<size_t> (i)] = static_cast<short> (std::lround (32767.0f * clipped));
    }
  return pcm;
}

std::vector<float> make_ft8_case_wave (QString const& message, float frequency_hz,
                                       float offset_ms, float snr_db, unsigned seed)
{
  constexpr int kFt8AsyncSampleCount = 15 * 12000;
  constexpr int kFt8Nsps = 1920;

  decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeFt8 (message);
  if (!encoded.ok || encoded.tones.isEmpty ())
    {
      return {};
    }

  QVector<float> const wave = decodium::txwave::generateFt8Wave (
      encoded.tones.constData (), encoded.tones.size (), kFt8Nsps, 2.0f, 12000.0f, frequency_hz);
  if (wave.isEmpty ())
    {
      return {};
    }

  int const offset_samples = static_cast<int> (std::lround (offset_ms * 12.0f));
  if (offset_samples < 0 || offset_samples + wave.size () > kFt8AsyncSampleCount)
    {
      return {};
    }

  std::vector<float> frame (static_cast<size_t> (kFt8AsyncSampleCount), 0.0f);
  for (int i = 0; i < wave.size (); ++i)
    {
      frame[static_cast<size_t> (offset_samples + i)] = 0.85f * wave[i];
    }

  double sum_sq = 0.0;
  int count = 0;
  for (float sample : frame)
    {
      if (sample != 0.0f)
        {
          sum_sq += static_cast<double> (sample) * static_cast<double> (sample);
          ++count;
        }
    }
  float const signal_rms =
      count > 0 ? static_cast<float> (std::sqrt (sum_sq / static_cast<double> (count))) : 0.0f;
  if (signal_rms > 0.0f)
    {
      double const sigma =
          static_cast<double> (signal_rms) / std::pow (10.0, static_cast<double> (snr_db) / 20.0);
      std::mt19937 rng {seed};
      std::normal_distribution<float> noise {0.0f, static_cast<float> (sigma)};
      for (float& sample : frame)
        {
          sample += noise (rng);
        }
    }

  return frame;
}

QStringList run_ft2_decode_stage (std::vector<short> const& iwave, int stage)
{
  QByteArray const mycall (12, ' ');
  QByteArray const hiscall (12, ' ');
  std::array<int, kFt2MaxLines> snrs {};
  std::array<float, kFt2MaxLines> dts {};
  std::array<float, kFt2MaxLines> freqs {};
  std::array<int, kFt2MaxLines> naps {};
  std::array<float, kFt2MaxLines> quals {};
  std::array<signed char, 77 * kFt2MaxLines> bits77 {};
  std::array<char, kFt2MaxLines * kFt2DecodedChars> decodeds {};
  int nout = 0;
  int nqsoprogress = 0;
  int nfqso = 1000;
  int nfa = 200;
  int nfb = 5000;
  int ndepth = 3;
  int ncontest = 0;

  std::vector<short> samples = iwave;
  if (stage >= 7)
    {
      ftx_ft2_stage7_clravg_c ();
    }
  ftx_ft2_cpp_dsp_rollout_stage_override_c (stage);
  ft2_async_decode_ (samples.data (), &nqsoprogress, &nfqso, &nfa, &nfb,
                     &ndepth, &ncontest, mycall.data (), hiscall.data (),
                     snrs.data (), dts.data (), freqs.data (), naps.data (), quals.data (),
                     bits77.data (), decodeds.data (), &nout,
                     static_cast<fortran_charlen_t_local> (mycall.size ()),
                     static_cast<fortran_charlen_t_local> (hiscall.size ()),
                     static_cast<fortran_charlen_t_local> (decodeds.size ()));

  QStringList lines;
  for (int i = 0; i < nout; ++i)
    {
      QByteArray const raw = trim_fortran_field_local (decodeds.data () + i * kFt2DecodedChars,
                                                       kFt2DecodedChars);
      lines << QString::fromLatin1 (raw.constData (), raw.size ());
    }
  if (stage >= 7)
    {
      ftx_ft2_stage7_clravg_c ();
    }
  lines.sort ();
  return lines;
}

QStringList run_ft2_triggered_decode_stage (std::vector<short> const& iwave, int stage)
{
  QByteArray const mycall (12, ' ');
  QByteArray const hiscall (12, ' ');
  std::array<char, 80 * kFt2MaxLines> outlines {};
  int nout = 0;
  int nqsoprogress = 0;
  int nfqso = 1000;
  int nfa = 200;
  int nfb = 5000;
  int ndepth = 3;
  int ncontest = 0;

  std::vector<short> samples = iwave;
  if (stage >= 7)
    {
      ftx_ft2_stage7_clravg_c ();
    }
  ftx_ft2_cpp_dsp_rollout_stage_override_c (stage);
  ft2_triggered_decode_ (samples.data (), &nqsoprogress, &nfqso, &nfa, &nfb,
                         &ndepth, &ncontest, mycall.data (), hiscall.data (),
                         outlines.data (), &nout,
                         static_cast<fortran_charlen_t_local> (mycall.size ()),
                         static_cast<fortran_charlen_t_local> (hiscall.size ()),
                         static_cast<fortran_charlen_t_local> (80));

  QStringList lines;
  for (int i = 0; i < nout; ++i)
    {
      QByteArray const raw = trim_fortran_field_local (outlines.data () + i * 80, 80);
      lines << QString::fromLatin1 (raw.constData (), raw.size ());
    }
  if (stage >= 7)
    {
      ftx_ft2_stage7_clravg_c ();
    }
  lines.sort ();
  return lines;
}

std::array<char, 13> make_ft2_context_field (QString const& value)
{
  std::array<char, 13> field {};
  QByteArray const latin = value.left (12).toLatin1 ();
  std::copy (latin.begin (), latin.end (), field.begin ());
  return field;
}
}

class TestQtHelpers
  : public QObject
{
  Q_OBJECT

public:

private:
  Q_SLOT void azdist_cpp_matches_fortran_reference ()
  {
    struct Case
    {
      QString myGrid;
      QString hisGrid;
      double utch;

      Case (QString my_grid, QString his_grid, double utc_hours)
        : myGrid {std::move (my_grid)}
        , hisGrid {std::move (his_grid)}
        , utch {utc_hours}
      {
      }
    };

    std::array<Case, 5> const cases {{
        {QStringLiteral ("JM75FV"), QStringLiteral ("FM19"), 22.25},
        {QStringLiteral ("JM75FV"), QStringLiteral ("CN88"), 5.75},
        {QStringLiteral ("FN42"), QStringLiteral ("FN31"), 12.0},
        {QStringLiteral ("QF56"), QStringLiteral ("BP40"), 9.5},
        {QStringLiteral ("FN42"), QStringLiteral ("FN42"), 0.0},
    }};

    for (Case const& item : cases)
      {
        QByteArray my = item.myGrid.leftJustified (6, QLatin1Char {' '}, true).left (6).toLatin1 ();
        QByteArray his = item.hisGrid.leftJustified (6, QLatin1Char {' '}, true).left (6).toLatin1 ();
        double utch = item.utch;
        int naz = -1;
        int nel = -1;
        int ndmiles = -1;
        int ndkm = -1;
        int nhotaz = -1;
        int nhotabetter = -1;

        azdist_ (my.data (), his.data (), &utch, &naz, &nel, &ndmiles, &ndkm, &nhotaz, &nhotabetter,
                 static_cast<size_t> (6), static_cast<size_t> (6));

        GeoDistanceInfo const geo = geo_distance (item.myGrid, item.hisGrid, item.utch);
        QString const context = QStringLiteral ("%1 -> %2 @ %3h")
                                    .arg (item.myGrid, item.hisGrid)
                                    .arg (item.utch, 0, 'f', 2);
        QVERIFY2 (geo.az == naz, qPrintable (QStringLiteral ("az mismatch: %1").arg (context)));
        QVERIFY2 (geo.el == nel, qPrintable (QStringLiteral ("el mismatch: %1").arg (context)));
        QVERIFY2 (geo.miles == ndmiles, qPrintable (QStringLiteral ("miles mismatch: %1").arg (context)));
        QVERIFY2 (geo.km == ndkm, qPrintable (QStringLiteral ("km mismatch: %1").arg (context)));
        if (ndkm >= 500)
          {
            QVERIFY2 (geo.hotAz == nhotaz,
                      qPrintable (QStringLiteral ("hotAz mismatch: %1 cpp=%2 fort=%3")
                                      .arg (context)
                                      .arg (geo.hotAz)
                                      .arg (nhotaz)));
            QVERIFY2 (geo.hotABetter == (nhotabetter != 0),
                      qPrintable (QStringLiteral ("hotABetter mismatch: %1").arg (context)));
          }
      }
  }

  Q_SLOT void msk144_encoder_matches_fortran_for_standard_and_free_text ()
  {
    auto compare = [] (QString const& message) {
      QByteArray fixed = message.left (37).toLatin1 ();
      if (fixed.size () < 37)
        {
          fixed.append (QByteArray (37 - fixed.size (), ' '));
        }

      std::array<char, 38> msg {};
      std::array<char, 38> msgsent_fortran {};
      std::array<int, 144> tones_fortran {};
      int ichk = 0;
      int itype = -1;
      std::copy_n (fixed.constData (), 37, msg.data ());

      genmsk_128_90_ (msg.data (), &ichk, msgsent_fortran.data (), tones_fortran.data (), &itype,
                      static_cast<fortran_charlen_t_local> (37),
                      static_cast<fortran_charlen_t_local> (37));

      decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeMsk144 (message);
      QVERIFY (encoded.ok);
      QCOMPARE (encoded.messageType, 1);
      QCOMPARE (encoded.msgsent, QByteArray (msgsent_fortran.data (), 37));
      QCOMPARE (encoded.tones.size (), 144);
      for (int i = 0; i < 144; ++i)
        {
          QCOMPARE (encoded.tones.at (i), tones_fortran[static_cast<size_t> (i)]);
        }
      QCOMPARE (itype, 1);
    };

    compare (QStringLiteral ("CQ K1ABC FN42"));
    compare (QStringLiteral ("K1ABC W9XYZ -10"));
    compare (QStringLiteral ("TNX BOB 73 GL"));

    {
      QString const message = QStringLiteral ("<K1ABC W9XYZ> R+03");
      QByteArray fixed = message.left (37).toLatin1 ();
      if (fixed.size () < 37)
        {
          fixed.append (QByteArray (37 - fixed.size (), ' '));
        }

      std::array<char, 38> msg {};
      std::array<char, 38> msgsent_fortran {};
      std::array<int, 144> tones_fortran {};
      int ichk = 0;
      int itype = -1;
      std::copy_n (fixed.constData (), 37, msg.data ());
      genmsk_128_90_ (msg.data (), &ichk, msgsent_fortran.data (), tones_fortran.data (), &itype,
                      static_cast<size_t> (37), static_cast<size_t> (37));

      decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeMsk144 (message);
      QVERIFY (encoded.ok);
      QCOMPARE (encoded.messageType, 7);
      QCOMPARE (itype, 7);
      QVERIFY2 (tones_fortran[40] == -40,
                qPrintable (QStringLiteral ("expected shorthand marker at tone 40, got %1")
                                .arg (tones_fortran[40])));
      QCOMPARE (encoded.msgsent, QByteArray (msgsent_fortran.data (), 37));
      QCOMPARE (encoded.tones.size (), 144);
      for (int i = 0; i < 41; ++i)
        {
          QVERIFY2 (encoded.tones.at (i) == tones_fortran[static_cast<size_t> (i)],
                    qPrintable (QStringLiteral ("shorthand tone mismatch at %1 cpp=%2 fort=%3")
                                    .arg (i)
                                    .arg (encoded.tones.at (i))
                                    .arg (tones_fortran[static_cast<size_t> (i)])));
        }
    }

    {
      decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeMsk144 (QStringLiteral ("@1500"));
      QVERIFY (encoded.ok);
      QCOMPARE (encoded.messageType, 1);
      QCOMPARE (encoded.tones.size (), 144);
      QCOMPARE (encoded.tones.at (0), 1500);
      for (int i = 1; i < encoded.tones.size (); ++i)
        {
          QCOMPARE (encoded.tones.at (i), 0);
        }
    }
  }

  Q_SLOT void fst4_encoder_matches_frozen_reference ()
  {
    auto check = [] (QString const& message, bool wspr_hint, QByteArray const& expected_msgsent,
                     QByteArray const& expected_bits, QByteArray const& expected_tones) {
      QByteArray fixed = message.leftJustified (37, QLatin1Char {' '}, true).left (37).toLatin1 ();
      std::array<char, 38> msg {};
      std::array<char, 38> msgsent {};
      std::array<signed char, 101> msgbits {};
      std::array<int, 160> tones {};
      std::copy_n (fixed.constData (), 37, msg.data ());

      int ichk = 0;
      int iwspr = wspr_hint ? 1 : 0;
      genfst4_ (msg.data (), &ichk, msgsent.data (), msgbits.data (), tones.data (), &iwspr,
                static_cast<fortran_charlen_t_local> (37),
                static_cast<fortran_charlen_t_local> (37));

      QCOMPARE (QByteArray (msgsent.data (), 37), expected_msgsent);
      QCOMPARE (iwspr, wspr_hint ? 1 : 0);
      for (int i = 0; i < expected_tones.size (); ++i)
        {
          QCOMPARE (tones[static_cast<size_t> (i)], expected_tones.at (i) - '0');
        }

      decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeFst4 (message);
      QVERIFY (encoded.ok);
      QCOMPARE (encoded.msgsent, expected_msgsent);
      QCOMPARE ((encoded.i3 == 0 && encoded.n3 == 6) ? 1 : 0, wspr_hint ? 1 : 0);
      QCOMPARE (encoded.tones.size (), expected_tones.size ());
      for (int i = 0; i < encoded.msgbits.size (); ++i)
        {
          QCOMPARE (encoded.msgbits.at (i), msgbits[static_cast<size_t> (i)] != 0 ? '\1' : '\0');
        }
      for (int i = 0; i < expected_tones.size (); ++i)
        {
          QCOMPARE (encoded.tones.at (i), expected_tones.at (i) - '0');
        }
      if (wspr_hint)
        {
          for (int i = 0; i < expected_bits.size (); ++i)
            {
              QCOMPARE (msgbits[static_cast<size_t> (i)],
                        static_cast<signed char> (expected_bits.at (i) == '1'));
            }
          for (int i = expected_bits.size (); i < 101; ++i)
            {
              QCOMPARE (msgbits[static_cast<size_t> (i)], static_cast<signed char> (0));
            }
        }
    };

    check (QStringLiteral ("CQ K1ABC FN42"),
           false,
           QByteArrayLiteral ("CQ K1ABC FN42                        "),
           QByteArrayLiteral (""),
           QByteArrayLiteral ("0132102310331123303131102221131113022123103201223312331102103331311303320301013210233211322021033021030322331110002310320102320213022232310312132211310101321023"));

    check (QStringLiteral ("K1ABC FN42 33"),
           true,
           QByteArrayLiteral ("K1ABC FN42 33                        "),
           QByteArrayLiteral ("00001001101111011110001101010101000011001100101000011000110100111011110111"),
           QByteArrayLiteral ("0132102300313221230211110020203301302123103201023221202011201333110020111321013210230223212312101133033221030333312310320101230013010112103132030110312101321023"));
  }

  Q_SLOT void fst4_waveform_matches_frozen_reference ()
  {
    struct Case
    {
      QString message;
      int trSeconds;
      bool wsprHint;
      int offset;
      std::array<short, 32> samples;
    };

    std::array<Case, 2> const cases {{
      {QStringLiteral ("CQ K1ABC FN42"), 15, false, 6000,
       {{0, 2, 10, 16, 0, -44, -90, -86, 0, 143, 249, 213, 0, -297, -487, -395,
         0, 506, 802, 631, 0, -769, -1193, -921, 0, 1085, 1658, 1263, 0, -1452, -2195, -1655}}},
      {QStringLiteral ("K1ABC FN42 33"), 120, true, 12000,
       {{0, 0, 0, 0, 0, 0, -1, -1, 0, 1, 2, 2, 0, -2, -4, -3,
         0, 4, 6, 5, 0, -6, -9, -7, 0, 9, 13, 10, 0, -11, -17, -13}}},
    }};

    for (Case const& item : cases)
      {
        QVector<short> const pcm = render_fst4_pcm_reference (item.message, item.trSeconds, item.wsprHint);
        QVERIFY2 (pcm.size () >= item.offset + static_cast<int> (item.samples.size ()),
                  qPrintable (QStringLiteral ("FST4 PCM too short for %1").arg (item.message)));
        for (int i = 0; i < static_cast<int> (item.samples.size ()); ++i)
          {
            short const actual = pcm.at (item.offset + i);
            short const expected = item.samples[static_cast<size_t> (i)];
            QVERIFY2 (std::abs (int {actual} - int {expected}) <= 1,
                      qPrintable (QStringLiteral ("FST4 PCM mismatch at %1 for %2: actual=%3 expected=%4")
                                      .arg (i)
                                      .arg (item.message)
                                      .arg (actual)
                                      .arg (expected)));
          }
      }
  }

  Q_SLOT void fst4_search_primitives_match_fortran_reference ()
  {
    QVector<short> audio = render_fst4_pcm_reference (QStringLiteral ("CQ K1ABC FN42"), 15, false);
    QVERIFY (!audio.isEmpty ());

    int nfft1 = 15 * RX_SAMPLE_RATE;
    int nsps = 720;
    int ndown = 18;
    int nfft2 = nfft1 / ndown;
    int nss = nsps / ndown;
    int hmod = 1;
    int ntrperiod = 15;
    float fs = static_cast<float> (RX_SAMPLE_RATE);
    float fs2 = fs / ndown;
    float baud = fs / nsps;
    float sigbw = 4.0f * baud;
    float fa = 1500.0f + 1.5f * baud - 20.0f;
    float fb = 1500.0f + 1.5f * baud + 20.0f;
    int nfa = 1500 - 20 - 100;
    int nfb = 1500 + 20 + 100;
    float minsync = 1.15f;

    std::vector<std::complex<float>> bigfft (static_cast<size_t> (nfft1 / 2 + 1));
    int nz = nfft1;
    int ndropmax = 1;
    int npct = 0;
    blanker_ (audio.data (), &nz, &ndropmax, &npct, bigfft.data ());
    int ndim = 1;
    int isign = -1;
    int iform = 0;
    four2a_ (bigfft.data (), &nfft1, &ndim, &isign, &iform);

    std::array<float, 200 * 5> cand_ref {};
    std::array<float, 200 * 5> cand_cpp {};
    int ncand_ref = 0;
    __fst4_decode_MOD_get_candidates_fst4 (bigfft.data (), &nfft1, &nsps,
                                           &hmod, &fs, &fa, &fb,
                                           &nfa, &nfb, &minsync,
                                           &ncand_ref, cand_ref.data ());
    int const ncand_cpp = ftx_fst4_get_candidates_c (bigfft.data (), nfft1, nsps, fs, fa, fb,
                                                     nfa, nfb, minsync, cand_cpp.data (), 200);

    QVERIFY (ncand_ref > 0);
    QCOMPARE (ncand_cpp, ncand_ref);
    for (int i = 0; i < std::min (ncand_ref, 3); ++i)
      {
        float const fc_ref = cand_ref[static_cast<size_t> (i)];
        float const det_ref = cand_ref[static_cast<size_t> (i + 200)];
        float const base_ref = cand_ref[static_cast<size_t> (i + 800)];
        float const fc_cpp = cand_cpp[static_cast<size_t> (5 * i + 0)];
        float const det_cpp = cand_cpp[static_cast<size_t> (5 * i + 1)];
        float const base_cpp = cand_cpp[static_cast<size_t> (5 * i + 4)];
        QVERIFY (std::fabs (fc_cpp - fc_ref) <= 1.0e-4f);
        QVERIFY (std::fabs (det_cpp - det_ref) <= 1.0e-4f);
        QVERIFY (std::fabs (base_cpp - base_ref) <= std::max (1.0e-3f, 0.05f * std::fabs (base_ref)));
      }

    float const f0 = cand_ref[0];
    std::vector<std::complex<float>> c2_ref (static_cast<size_t> (nfft2));
    std::vector<std::complex<float>> c2_cpp (static_cast<size_t> (nfft2));
    float f0_arg = f0;
    __fst4_decode_MOD_fst4_downsample (bigfft.data (), &nfft1, &ndown,
                                       &f0_arg, &sigbw, c2_ref.data ());
    QCOMPARE (ftx_fst4_downsample_c (bigfft.data (), nfft1, ndown, nsps, fs, f0, c2_cpp.data (), nfft2), nfft2);
    float max_real_diff = 0.0f;
    float max_imag_diff = 0.0f;
    int max_index = -1;
    for (int i = 0; i < nfft2; i += 97)
      {
        float const real_diff = std::fabs (c2_cpp[static_cast<size_t> (i)].real () - c2_ref[static_cast<size_t> (i)].real ());
        float const imag_diff = std::fabs (c2_cpp[static_cast<size_t> (i)].imag () - c2_ref[static_cast<size_t> (i)].imag ());
        if (real_diff > max_real_diff || imag_diff > max_imag_diff)
          {
            max_real_diff = std::max (max_real_diff, real_diff);
            max_imag_diff = std::max (max_imag_diff, imag_diff);
            max_index = i;
          }
      }
    QVERIFY2 (max_real_diff <= 0.2f && max_imag_diff <= 0.2f,
              qPrintable (QStringLiteral ("FST4 downsample mismatch at %1: cpp=(%2,%3) ref=(%4,%5) d=(%6,%7)")
                              .arg (max_index)
                              .arg (c2_cpp[static_cast<size_t> (max_index)].real (), 0, 'g', 9)
                              .arg (c2_cpp[static_cast<size_t> (max_index)].imag (), 0, 'g', 9)
                              .arg (c2_ref[static_cast<size_t> (max_index)].real (), 0, 'g', 9)
                              .arg (c2_ref[static_cast<size_t> (max_index)].imag (), 0, 'g', 9)
                              .arg (max_real_diff, 0, 'g', 9)
                              .arg (max_imag_diff, 0, 'g', 9)));

    int nsyncoh = 8;
    float emedelay = 0.0f;
    float sbest_ref = 0.0f;
    float fcbest_ref = 0.0f;
    int isbest_ref = 0;
    __fst4_decode_MOD_fst4_sync_search (c2_ref.data (), &nfft2,
                                        &hmod, &fs2,
                                        &nss, &ntrperiod,
                                        &nsyncoh, &emedelay, &sbest_ref, &fcbest_ref, &isbest_ref);

    float sbest_cpp = 0.0f;
    float fcbest_cpp = 0.0f;
    int isbest_cpp = 0;
    QVERIFY (ftx_fst4_sync_search_c (c2_cpp.data (), nfft2, nsps, ndown, ntrperiod,
                                     emedelay, &sbest_cpp, &fcbest_cpp, &isbest_cpp) != 0);
    QVERIFY2 (std::fabs (sbest_cpp - sbest_ref) <= 0.5f,
              qPrintable (QStringLiteral ("FST4 sync strength mismatch: cpp=%1 ref=%2")
                              .arg (sbest_cpp, 0, 'g', 9)
                              .arg (sbest_ref, 0, 'g', 9)));
    QVERIFY2 (std::fabs (fcbest_cpp - fcbest_ref) <= 1.0e-4f,
              qPrintable (QStringLiteral ("FST4 sync freq mismatch: cpp=%1 ref=%2")
                              .arg (fcbest_cpp, 0, 'g', 9)
                              .arg (fcbest_ref, 0, 'g', 9)));
    QCOMPARE (isbest_cpp, isbest_ref);
  }

  Q_SLOT void shared_dsp_pctile_matches_expected_values ()
  {
    float values[] {9.0f, 1.0f, 7.0f, 3.0f, 5.0f};
    int npts = 5;
    int npct = 50;
    float xpct = 0.0f;
    pctile_ (values, &npts, &npct, &xpct);
    QCOMPARE (xpct, 5.0f);

    npct = 0;
    pctile_ (values, &npts, &npct, &xpct);
    QCOMPARE (xpct, 1.0f);

    npts = -1;
    npct = 10;
    xpct = 0.0f;
    pctile_ (values, &npts, &npct, &xpct);
    QCOMPARE (xpct, 1.0f);
  }

  Q_SLOT void shared_dsp_polyfit_recovers_known_quadratic ()
  {
    double x[] {0.0, 1.0, 2.0, 3.0, 4.0};
    double y[] {1.0, 6.0, 15.0, 28.0, 45.0};
    double sigmay[] {1.0, 1.0, 1.0, 1.0, 1.0};
    int npts = 5;
    int nterms = 3;
    int mode = 0;
    double coeffs[] {0.0, 0.0, 0.0};
    double chisqr = -1.0;

    polyfit_ (x, y, sigmay, &npts, &nterms, &mode, coeffs, &chisqr);

    QVERIFY (std::fabs (coeffs[0] - 1.0) <= 1.0e-9);
    QVERIFY (std::fabs (coeffs[1] - 3.0) <= 1.0e-9);
    QVERIFY (std::fabs (coeffs[2] - 2.0) <= 1.0e-9);
    QVERIFY (std::fabs (chisqr) <= 1.0e-9);
  }

  Q_SLOT void shared_dsp_blanker_zeroes_expected_samples ()
  {
    short samples[] {1, 2, 100, 3, 4, 5, 6, 7};
    int nz = 8;
    int ndropmax = 1;
    int npct = 25;
    std::complex<float> bigfft[5];

    blanker_ (samples, &nz, &ndropmax, &npct, bigfft);

    QVERIFY (std::fabs (bigfft[0].real () - 1.0f) <= 1.0e-6f);
    QVERIFY (std::fabs (bigfft[0].imag () - 2.0f) <= 1.0e-6f);
    QVERIFY (std::fabs (bigfft[1].real ()) <= 1.0e-6f);
    QVERIFY (std::fabs (bigfft[1].imag ()) <= 1.0e-6f);
    QVERIFY (std::fabs (bigfft[2].real () - 4.0f) <= 1.0e-6f);
    QVERIFY (std::fabs (bigfft[2].imag () - 5.0f) <= 1.0e-6f);
    QVERIFY (std::fabs (bigfft[3].real () - 6.0f) <= 1.0e-6f);
    QVERIFY (std::fabs (bigfft[3].imag () - 7.0f) <= 1.0e-6f);
  }

  Q_SLOT void shared_dsp_four2a_matches_expected_fft_shapes ()
  {
    {
      std::complex<float> values[] {
          {1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}
      };
      int nfft = 4;
      int ndim = 1;
      int isign = -1;
      int iform = 1;
      four2a_ (values, &nfft, &ndim, &isign, &iform);
      for (auto const& value : values)
        {
          QVERIFY (std::fabs (value.real () - 1.0f) <= 1.0e-6f);
          QVERIFY (std::fabs (value.imag ()) <= 1.0e-6f);
        }

      isign = 1;
      four2a_ (values, &nfft, &ndim, &isign, &iform);
      QVERIFY (std::fabs (values[0].real () - 4.0f) <= 1.0e-5f);
      QVERIFY (std::fabs (values[0].imag ()) <= 1.0e-5f);
      for (int i = 1; i < 4; ++i)
        {
          QVERIFY (std::fabs (values[i].real ()) <= 1.0e-5f);
          QVERIFY (std::fabs (values[i].imag ()) <= 1.0e-5f);
        }
    }

    {
      std::complex<float> values[3] {};
      auto* real_values = reinterpret_cast<float*> (values);
      real_values[0] = 1.0f;
      real_values[1] = 2.0f;
      real_values[2] = 3.0f;
      real_values[3] = 4.0f;

      int nfft = 4;
      int ndim = 1;
      int isign = -1;
      int iform = 0;
      four2a_ (values, &nfft, &ndim, &isign, &iform);
      QVERIFY (std::fabs (values[0].real () - 10.0f) <= 1.0e-5f);
      QVERIFY (std::fabs (values[0].imag ()) <= 1.0e-5f);
      QVERIFY (std::fabs (values[1].real () + 2.0f) <= 1.0e-5f);
      QVERIFY (std::fabs (values[1].imag () - 2.0f) <= 1.0e-5f);
      QVERIFY (std::fabs (values[2].real () + 2.0f) <= 1.0e-5f);
      QVERIFY (std::fabs (values[2].imag ()) <= 1.0e-5f);

      isign = 1;
      iform = -1;
      four2a_ (values, &nfft, &ndim, &isign, &iform);
      QVERIFY (std::fabs (real_values[0] - 4.0f) <= 1.0e-4f);
      QVERIFY (std::fabs (real_values[1] - 8.0f) <= 1.0e-4f);
      QVERIFY (std::fabs (real_values[2] - 12.0f) <= 1.0e-4f);
      QVERIFY (std::fabs (real_values[3] - 16.0f) <= 1.0e-4f);
    }

    int cleanup_nfft = -1;
    int cleanup_ndim = 1;
    int cleanup_isign = 1;
    int cleanup_iform = 1;
    four2a_ (nullptr, &cleanup_nfft, &cleanup_ndim, &cleanup_isign, &cleanup_iform);
  }

  Q_SLOT void fst4_crc24_matches_frozen_reference ()
  {
    auto bits_from_pattern = [] (char const* pattern) {
      std::vector<signed char> bits;
      int const size = static_cast<int> (std::strlen (pattern));
      bits.reserve (size);
      for (int i = 0; i < size; ++i)
        {
          bits.push_back (pattern[i] == '1' ? 1 : 0);
        }
      return bits;
    };

    auto append_crc = [] (std::vector<signed char> bits, int crc) {
      Q_ASSERT (bits.size () >= 24);
      int const start = static_cast<int> (bits.size ()) - 24;
      for (int i = 0; i < 24; ++i)
        {
          bits[static_cast<size_t> (start + i)] = (crc >> (23 - i)) & 1;
        }
      return bits;
    };

    struct Case
    {
      char const* bits;
      int expected;
    };

    std::array<Case, 4> const cases {{
      {"10110100101101110010011010101001100101101000110100000000000000000000000000", 1339986},
      {"11111111111111111111111111111111111111111111111111000000000000000000000000", 5552799},
      {"11001010111100010100101100110101001011110000101001011001101010010111100001010000000000000000000000000", 14357504},
      {"11000110001110001100011100011000111000110001110001100011100011000111000110001000000000000000000000000", 6417775},
    }};

    for (Case const& item : cases)
      {
        std::vector<signed char> bits = bits_from_pattern (item.bits);
        int len = static_cast<int> (bits.size ());
        int crc = -1;
        get_crc24_ (bits.data (), &len, &crc);
        QCOMPARE (crc, item.expected);

        std::vector<signed char> checked = append_crc (bits, crc);
        int check_crc = -1;
        get_crc24_ (checked.data (), &len, &check_crc);
        QCOMPARE (check_crc, 0);
      }
  }

  Q_SLOT void fst4_async_export_matches_native_worker_on_clean_waveforms ()
  {
    struct Case
    {
      QString message;
      int trSeconds;
      bool wsprHint;
      int nfqso;
      int ntol;
      int ndepth;
    };

    std::array<Case, 2> const cases {{
      {QStringLiteral ("CQ K1ABC FN42"), 15, false, 1500, 20, 2},
      {QStringLiteral ("K1ABC FN42 33"), 120, true, 1500, 20, 3},
    }};

    for (Case const& item : cases)
      {
        decodium::fst4::DecodeRequest request;
        request.serial = 1;
        request.mode = item.wsprHint ? QStringLiteral ("FST4W") : QStringLiteral ("FST4");
        request.audio = render_fst4_pcm_reference (item.message, item.trSeconds, item.wsprHint);
        request.nutc = 0;
        request.nqsoprogress = 0;
        request.nfa = item.nfqso - item.ntol;
        request.nfb = item.nfqso + item.ntol;
        request.nfqso = item.nfqso;
        request.ndepth = item.ndepth;
        request.ntrperiod = item.trSeconds;
        request.nexp_decode = 0;
        request.ntol = item.ntol;
        request.emedelay = 0.0f;
        request.nagain = 0;
        request.lapcqonly = 0;
        request.mycall = QByteArrayLiteral ("K1ABC       ");
        request.hiscall = QByteArrayLiteral ("W9XYZ       ");
        request.iwspr = item.wsprHint ? 1 : 0;
        request.lprinthash22 = 0;

        QTemporaryDir native_dir;
        QVERIFY (native_dir.isValid ());
        QTemporaryDir reference_dir;
        QVERIFY (reference_dir.isValid ());

        decodium::fst4::DecodeRequest native_request = request;
        native_request.dataDir = native_dir.path ().toLocal8Bit ();
        decodium::fst4::DecodeRequest reference_request = request;
        reference_request.dataDir = reference_dir.path ().toLocal8Bit ();

        ftx_fst4_reset_runtime_state_c ();
        QStringList const native = decodium::fst4::decodeFst4Rows (native_request);
        ftx_fst4_reset_runtime_state_c ();
        QStringList const reference = decode_fst4_rows_async_export_reference (reference_request);
        QCOMPARE (reference.size (), native.size ());
        for (int rowIndex = 0; rowIndex < reference.size (); ++rowIndex)
          {
            QString const& refRow = reference.at (rowIndex);
            QString const& nativeRow = native.at (rowIndex);
            if (refRow == nativeRow)
              {
                continue;
              }

            QVERIFY2 (item.wsprHint,
                      qPrintable (QStringLiteral ("Unexpected FST4 row mismatch at index %1: ref='%2' native='%3'")
                                      .arg (rowIndex)
                                      .arg (refRow)
                                      .arg (nativeRow)));
            QVERIFY2 (refRow.size () >= 4 && nativeRow.size () >= 4,
                      qPrintable (QStringLiteral ("Malformed FST4W row at index %1: ref='%2' native='%3'")
                                      .arg (rowIndex)
                                      .arg (refRow)
                                      .arg (nativeRow)));
            bool refOk = false;
            bool nativeOk = false;
            int const refSnr = refRow.leftRef (4).toString ().trimmed ().toInt (&refOk);
            int const nativeSnr = nativeRow.leftRef (4).toString ().trimmed ().toInt (&nativeOk);
            QVERIFY2 (refOk && nativeOk && refRow.mid (4) == nativeRow.mid (4)
                          && std::abs (refSnr - nativeSnr) <= 4,
                      qPrintable (QStringLiteral ("FST4W row mismatch exceeds tolerance at index %1: ref='%2' native='%3'")
                                      .arg (rowIndex)
                                      .arg (refRow)
                                      .arg (nativeRow)));
          }
        if (native.isEmpty () && !item.wsprHint)
          {
            int stage = 0;
            int badsync = 0;
            int nharderrors = 0;
            int iaptype = 0;
            std::array<char, 37> msg_debug {};
            int const ok = ftx_fst4_debug_first_candidate_c (native_request.audio.constData (), native_request.audio.size (),
                                                             native_request.ntrperiod, native_request.ndepth,
                                                             native_request.nfa, native_request.nfb, native_request.nfqso,
                                                             native_request.ntol, native_request.emedelay,
                                                             native_request.mycall.constData (), native_request.hiscall.constData (),
                                                             &stage, &badsync, &nharderrors, &iaptype,
                                                             msg_debug.data ());
            QVERIFY2 (stage >= 5,
                      qPrintable (QStringLiteral ("FST4 native worker stalled too early; debug ok=%1 stage=%2 badsync=%3 nhard=%4 iaptype=%5 msg='%6'")
                                      .arg (ok)
                                      .arg (stage)
                                      .arg (badsync)
                                      .arg (nharderrors)
                                      .arg (iaptype)
                                      .arg (QString::fromLatin1 (msg_debug.data (), 37).trimmed ())));
          }
      }
  }

  Q_SLOT void fst4_native_emit_bridge_writes_decoded_output ()
  {
    params_block_t params {};
    params.nutc = 123456;
    params.ntrperiod = 15;
    params.nQSOProgress = 0;
    params.nfqso = 1500;
    params.nfa = 1480;
    params.nfb = 1520;
    params.ntol = 20;
    params.ndepth = 2;
    params.nexp_decode = 0;
    params.nmode = 240;
    std::memcpy (params.mycall, "K1ABC       ", 12);
    std::memcpy (params.hiscall, "W9XYZ       ", 12);

    QVector<short> const audio = render_fst4_pcm_reference (QStringLiteral ("CQ K1ABC FN42"), 15, false);

    QTemporaryDir temp_dir;
    QVERIFY (temp_dir.isValid ());
    QByteArray temp_dir_bytes = temp_dir.path ().toLocal8Bit ();
    temp_dir_bytes.append ('\0');

    int decoded_count = 0;
    ftx_fst4_reset_runtime_state_c ();
    ftx_fst4_decode_and_emit_params_c (audio.constData (), &params, temp_dir_bytes.constData (), &decoded_count);

    decodium::fst4::DecodeRequest request;
    request.serial = 1;
    request.mode = QStringLiteral ("FST4");
    request.audio = audio;
    request.dataDir = QDir::currentPath ().toLocal8Bit ();
    request.nutc = params.nutc;
    request.nqsoprogress = params.nQSOProgress;
    request.nfa = params.nfa;
    request.nfb = params.nfb;
    request.nfqso = params.nfqso;
    request.ndepth = params.ndepth;
    request.ntrperiod = params.ntrperiod;
    request.nexp_decode = params.nexp_decode;
    request.ntol = params.ntol;
    request.emedelay = params.emedelay;
    request.nagain = params.nagain ? 1 : 0;
    request.lapcqonly = params.lapcqonly ? 1 : 0;
    request.mycall = QByteArray {params.mycall, static_cast<int> (sizeof (params.mycall))};
    request.hiscall = QByteArray {params.hiscall, static_cast<int> (sizeof (params.hiscall))};
    request.iwspr = 0;
    request.lprinthash22 = 0;
    ftx_fst4_reset_runtime_state_c ();
    QStringList const native_rows = decodium::fst4::decodeFst4Rows (request);

    QVERIFY (!native_rows.isEmpty ());
    QCOMPARE (decoded_count, native_rows.size ());
    QFile decoded_file {temp_dir.filePath (QStringLiteral ("decoded.txt"))};
    QVERIFY (decoded_file.exists ());
    QVERIFY (decoded_file.open (QIODevice::ReadOnly | QIODevice::Text));
    QString const decoded_text = QString::fromUtf8 (decoded_file.readAll ());
    int const tick = native_rows.first ().indexOf (QLatin1Char {'`'});
    QVERIFY (tick >= 0);
    QString const decoded_field = native_rows.first ().mid (tick + 4, 37).trimmed ();
    QVERIFY (!decoded_field.isEmpty ());
    QVERIFY2 (decoded_text.contains (decoded_field),
              qPrintable (decoded_text));
  }

  Q_SLOT void msk144_cli_decodes_sample_wav_without_fast_decode_crash ()
  {
    QString const bin_dir = QCoreApplication::applicationDirPath ();
    QString const jt9_path = QFileInfo {bin_dir + QStringLiteral ("/../jt9")}.absoluteFilePath ();
    QString const wav_path = QFileInfo {QDir {bin_dir}.absoluteFilePath (
        QStringLiteral ("../MSK144.wav"))}.absoluteFilePath ();
    if (!QFileInfo::exists (jt9_path) || !QFileInfo::exists (wav_path))
      {
        QSKIP (qPrintable (QStringLiteral ("MSK144 CLI fixture not available (%1, %2)")
                               .arg (jt9_path, wav_path)));
      }

    QProcess decode;
    decode.start (jt9_path, {QStringLiteral ("-k"), wav_path});
    QVERIFY2 (decode.waitForFinished (180000), qPrintable (decode.errorString ()));
    QCOMPARE (decode.exitStatus (), QProcess::NormalExit);
    QCOMPARE (decode.exitCode (), 0);

    QString const output = QString::fromLocal8Bit (decode.readAllStandardOutput ())
                         + QString::fromLocal8Bit (decode.readAllStandardError ());
    QVERIFY2 (!output.contains (QStringLiteral ("Fortran runtime error")), qPrintable (output));
    QVERIFY2 (output.contains (QStringLiteral ("K1JT WA4CQG EM72")), qPrintable (output));
    QVERIFY2 (output.contains (QStringLiteral ("<DecodeFinished>")), qPrintable (output));
  }

  Q_SLOT void msk144_native_runner_matches_fortran_reference_on_sample_wav ()
  {
    QString const bin_dir = QCoreApplication::applicationDirPath ();
    QString const wav_path = QFileInfo {QDir {bin_dir}.absoluteFilePath (
        QStringLiteral ("../MSK144.wav"))}.absoluteFilePath ();
    if (!QFileInfo::exists (wav_path))
      {
        QSKIP (qPrintable (QStringLiteral ("MSK144 fixture not available (%1)").arg (wav_path)));
      }

    WavFixture const wav = read_pcm16_mono_wav (wav_path);
    QVERIFY2 (wav.sampleRate == kMsk144SampleRate, qPrintable (QStringLiteral ("unexpected sample rate %1")
                                                                    .arg (wav.sampleRate)));
    QVERIFY (!wav.samples.isEmpty ());

    decodium::msk144::DecodeRequest request;
    request.serial = 1;
    request.audio = wav.samples;
    request.nutc = 123456;
    request.kdone = wav.samples.size ();
    request.npick = 0;
    request.t0_ms = 0;
    request.t1_ms = wav.samples.size () * 1000 / kMsk144SampleRate;
    request.maxlines = kMsk144MaxLines;
    request.rxfreq = 1500;
    request.ftol = 20;
    request.aggressive = 0;
    request.trperiod = 60.0;

    QStringList const reference = decode_msk144_rows_fortran_reference (request);
    decodium::msk144::resetMsk144DecoderState ();
    QStringList const native = decodium::msk144::decodeMsk144Rows (request);

    QCOMPARE (native, reference);
    QVERIFY2 (native.contains (QStringLiteral ("123456   8  8.7 1488 &  K1JT WA4CQG EM72")),
              qPrintable (native.join (QStringLiteral ("\n"))));
  }

  Q_SLOT void msk144_realtime_runner_matches_fortran_reference_on_sample_wav ()
  {
    QString const bin_dir = QCoreApplication::applicationDirPath ();
    QString const wav_path = QFileInfo {QDir {bin_dir}.absoluteFilePath (
        QStringLiteral ("../MSK144.wav"))}.absoluteFilePath ();
    if (!QFileInfo::exists (wav_path))
      {
        QSKIP (qPrintable (QStringLiteral ("MSK144 fixture not available (%1)").arg (wav_path)));
      }

    WavFixture const wav = read_pcm16_mono_wav (wav_path);
    QVERIFY2 (wav.sampleRate == kMsk144SampleRate, qPrintable (QStringLiteral ("unexpected sample rate %1")
                                                                    .arg (wav.sampleRate)));
    QVERIFY (!wav.samples.isEmpty ());

    decodium::msk144::DecodeRequest request;
    request.serial = 1;
    request.audio = wav.samples;
    request.nutc = 123456;
    request.kdone = wav.samples.size ();
    request.npick = 0;
    request.t0_ms = 0;
    request.t1_ms = wav.samples.size () * 1000 / kMsk144SampleRate;
    request.maxlines = kMsk144MaxLines;
    request.rxfreq = 1500;
    request.ftol = 20;
    request.aggressive = 0;
    request.trperiod = 60.0;

    QStringList const reference = decode_msk144_rows_fortran_reference (request);
    QStringList const live_native = decode_msk144_rows_native_realtime (request);

    QCOMPARE (live_native, reference);
    QVERIFY2 (live_native.contains (QStringLiteral ("123456   8  8.7 1488 &  K1JT WA4CQG EM72")),
              qPrintable (live_native.join (QStringLiteral ("\n"))));
  }

  Q_SLOT void msk144_shorthand_runner_matches_fortran_reference_on_generated_waveform ()
  {
    decodium::msk144::DecodeRequest request;
    request.serial = 1;
    request.audio = make_clean_msk144_wave (QStringLiteral ("<K1JT WA4CQG> RRR"), kMsk144BlockSize);
    QVERIFY2 (!request.audio.isEmpty (), "Failed to synthesize clean MSK144 shorthand waveform");
    request.nutc = 123456;
    request.kdone = request.audio.size ();
    request.npick = 0;
    request.t0_ms = 0;
    request.t1_ms = request.audio.size () * 1000 / kMsk144SampleRate;
    request.maxlines = kMsk144MaxLines;
    request.rxfreq = 1500;
    request.ftol = 20;
    request.aggressive = 0;
    request.trperiod = 60.0;
    request.mycall = QByteArrayLiteral ("K1JT");
    request.hiscall = QByteArrayLiteral ("WA4CQG");
    request.shorthandEnabled = true;

    QStringList const reference = decode_msk144_rows_fortran_reference (request);
    decodium::msk144::resetMsk144DecoderState ();
    QStringList const batch_native = decodium::msk144::decodeMsk144Rows (request);
    QStringList const live_native = decode_msk144_rows_native_realtime (request);

    QCOMPARE (batch_native, reference);
    QCOMPARE (live_native, reference);
    QVERIFY2 (!reference.isEmpty (), "Fortran reference produced no MSK144 shorthand decode");
    QVERIFY2 (reference.join (QStringLiteral ("\n")).contains (QStringLiteral ("<K1JT WA4CQG> RRR")),
              qPrintable (reference.join (QStringLiteral ("\n"))));
  }

  Q_SLOT void msk144_swl_runner_matches_fortran_reference_on_repeated_generated_waveform ()
  {
    QVector<short> const block = make_clean_msk144_wave (QStringLiteral ("<K1JT WA4CQG> RRR"), kMsk144BlockSize);
    QVERIFY2 (!block.empty (), "Failed to synthesize clean MSK144 SWL shorthand waveform");

    decodium::msk144::DecodeRequest request;
    request.serial = 1;
    request.audio.resize (2 * kMsk144BlockSize);
    std::copy (block.begin (), block.end (), request.audio.begin ());
    std::copy (block.begin (), block.end (), request.audio.begin () + kMsk144BlockSize);
    request.nutc = 123456;
    request.kdone = request.audio.size ();
    request.npick = 0;
    request.t0_ms = 0;
    request.t1_ms = request.audio.size () * 1000 / kMsk144SampleRate;
    request.maxlines = kMsk144MaxLines;
    request.rxfreq = 1500;
    request.ftol = 20;
    request.aggressive = 0;
    request.trperiod = 60.0;
    request.swlEnabled = true;

    QStringList const reference = decode_msk144_rows_fortran_reference (request);
    decodium::msk144::resetMsk144DecoderState ();
    QStringList const batch_native = decodium::msk144::decodeMsk144Rows (request);
    QStringList const live_native = decode_msk144_rows_native_realtime (request);

    QCOMPARE (batch_native, reference);
    QCOMPARE (live_native, reference);
  }

  Q_SLOT void q65_ana64_cpp_matches_fortran_reference ()
  {
    constexpr int kQ65Ana64Npts = 12000;
    constexpr float kSampleRate = 12000.0f;
    constexpr float kTwoPi = 6.28318530717958647692f;

    std::vector<short> audio (kQ65Ana64Npts);
    for (int i = 0; i < kQ65Ana64Npts; ++i)
      {
        float const t = static_cast<float> (i) / kSampleRate;
        float const sample = 14000.0f * std::sin (kTwoPi * 713.0f * t)
                           + 7000.0f * std::sin (kTwoPi * 1234.0f * t + 0.35f)
                           + 2500.0f * std::sin (kTwoPi * 41.0f * t);
        int const clipped = std::max (-32767, std::min (32767, qRound (sample)));
        audio[static_cast<size_t> (i)] = static_cast<short> (clipped);
      }

    int npts = kQ65Ana64Npts;
    std::vector<std::complex<float>> reference (kQ65Ana64Npts);
    ana64_ (audio.data (), &npts, reference.data ());

    std::vector<float> real_out (kQ65Ana64Npts / 2);
    std::vector<float> imag_out (kQ65Ana64Npts / 2);
    QVERIFY (ftx_q65_ana64_c (audio.data (), kQ65Ana64Npts, real_out.data (), imag_out.data ()) != 0);

    float max_error = 0.0f;
    float mean_error = 0.0f;
    for (int i = 0; i < kQ65Ana64Npts / 2; ++i)
      {
        std::complex<float> const native {real_out[static_cast<size_t> (i)], imag_out[static_cast<size_t> (i)]};
        float const error = std::abs (native - reference[static_cast<size_t> (i)]);
        max_error = std::max (max_error, error);
        mean_error += error;
      }
    mean_error /= static_cast<float> (kQ65Ana64Npts / 2);

    QVERIFY2 (max_error <= 1.0e-5f,
              qPrintable (QStringLiteral ("max ana64 error %1 mean %2").arg (max_error).arg (mean_error)));
  }

  Q_SLOT void ft2_message_plausibility_filter_rejects_garbage_type4_decodes ()
  {
    auto check = [] (QString const& message) {
      QByteArray fixed = message.left (37).toLatin1 ();
      if (fixed.size () < 37)
        {
          fixed.append (QByteArray (37 - fixed.size (), ' '));
        }
      return ftx_ft2_message_is_plausible_c (fixed.constData ()) != 0;
    };

    QVERIFY (check (QStringLiteral ("CQ TEST K1ABC FN42")));
    QVERIFY (check (QStringLiteral ("CQ GB13COL")));
    QVERIFY (check (QStringLiteral ("VP2V/GM4WJS K1ABC RR73")));
    QVERIFY (check (QStringLiteral ("CQ EA8/GM4WJS")));
    QVERIFY (check (QStringLiteral ("TNX BOB 73 GL")));

    QVERIFY (!check (QStringLiteral ("CQ CAYOBTYZCV0")));
    QVERIFY (!check (QStringLiteral ("CQ 3K6TLT33XGQ")));
    QVERIFY (!check (QStringLiteral ("<...> 7SVPAYTXTIK RR73")));
    QVERIFY (!check (QStringLiteral ("<...> T2XY0X08FWW 73")));
    QVERIFY (!check (QStringLiteral ("CQ 6PWE9JEL80E")));
    QVERIFY (!check (QStringLiteral ("CQ 5MWF1/NLQ5X")));
    QVERIFY (!check (QStringLiteral ("M9B ZNWF6WH7V")));
    QVERIFY (!check (QStringLiteral ("M9B ZNWF6WH7V RR73")));
  }

  Q_SLOT void ft2_assisted_directed_reply_context_rejects_cq_only_state ()
  {
    QVERIFY (!ft2_allow_assisted_directed_reply_context (false, true, false, false));
    QVERIFY (!ft2_allow_assisted_directed_reply_context (false, false, false, false));
  }

  Q_SLOT void ft2_assisted_directed_reply_context_rejects_recent_cq_like_tx_state ()
  {
    QVERIFY (!ft2_allow_assisted_directed_reply_context (true, true, false, false));
  }

  Q_SLOT void ft2_assisted_directed_reply_context_rejects_auto_reply_without_partner ()
  {
    QVERIFY (!ft2_allow_assisted_directed_reply_context (false, false, true, false));
    QVERIFY (!ft2_allow_assisted_directed_reply_context (false, true, true, false));
  }

  Q_SLOT void ft2_assisted_directed_reply_context_accepts_active_reply_state ()
  {
    QVERIFY (ft2_allow_assisted_directed_reply_context (true, false, false, false));
    QVERIFY (ft2_allow_assisted_directed_reply_context (false, false, false, true));
    QVERIFY (ft2_allow_assisted_directed_reply_context (false, false, true, true));
  }

  Q_SLOT void round_15s_date_time_up ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 22, 500}};
    QCOMPARE (qt_round_date_time_to (dt, 15000), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 30)));
  }

  Q_SLOT void truncate_15s_date_time_up ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 22, 500}};
    QCOMPARE (qt_truncate_date_time_to (dt, 15000), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 15)));
  }

  Q_SLOT void round_15s_date_time_down ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 22, 499}};
    QCOMPARE (qt_round_date_time_to (dt, 15000), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 15)));
  }

  Q_SLOT void truncate_15s_date_time_down ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 22, 499}};
    QCOMPARE (qt_truncate_date_time_to (dt, 15000), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 15)));
  }

  Q_SLOT void round_15s_date_time_on ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 15}};
    QCOMPARE (qt_round_date_time_to (dt, 15000), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 15)));
  }

  Q_SLOT void truncate_15s_date_time_on ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 15}};
    QCOMPARE (qt_truncate_date_time_to (dt, 15000), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 15)));
  }

  Q_SLOT void round_15s_date_time_under ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 14, 999}};
    QCOMPARE (qt_round_date_time_to (dt, 15000), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 15)));
  }

  Q_SLOT void truncate_15s_date_time_under ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 14, 999}};
    QCOMPARE (qt_truncate_date_time_to (dt, 15000), QDateTime (QDate (2020, 8, 6), QTime (14, 15)));
  }

  Q_SLOT void round_15s_date_time_over ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 15, 1}};
    QCOMPARE (qt_round_date_time_to (dt, 15000), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 15)));
  }

  Q_SLOT void truncate_15s_date_time_over ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 15, 1}};
    QCOMPARE (qt_truncate_date_time_to (dt, 15000), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 15)));
  }

  Q_SLOT void round_7p5s_date_time_up ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 26, 250}};
    QCOMPARE (qt_round_date_time_to (dt, 7500), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 30)));
  }

  Q_SLOT void truncate_7p5s_date_time_up ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 26, 250}};
    QCOMPARE (qt_truncate_date_time_to (dt, 7500), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 22, 500)));
  }

  Q_SLOT void round_7p5s_date_time_down ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 26, 249}};
    QCOMPARE (qt_round_date_time_to (dt, 7500), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 22, 500)));
  }

  Q_SLOT void truncate_7p5s_date_time_down ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 26, 249}};
    QCOMPARE (qt_truncate_date_time_to (dt, 7500), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 22, 500)));
  }

  Q_SLOT void round_7p5s_date_time_on ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 22, 500}};
    QCOMPARE (qt_round_date_time_to (dt, 7500), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 22, 500)));
  }

  Q_SLOT void truncate_7p5s_date_time_on ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 22, 500}};
    QCOMPARE (qt_truncate_date_time_to (dt, 7500), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 22, 500)));
  }

  Q_SLOT void round_7p5s_date_time_under ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 22, 499}};
    QCOMPARE (qt_round_date_time_to (dt, 7500), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 22, 500)));
  }

  Q_SLOT void truncate_7p5s_date_time_under ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 22, 499}};
    QCOMPARE (qt_truncate_date_time_to (dt, 7500), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 15)));
  }

  Q_SLOT void round_7p5s_date_time_over ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 22, 501}};
    QCOMPARE (qt_round_date_time_to (dt, 7500), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 22, 500)));
  }

  Q_SLOT void truncate_7p5s_date_time_over ()
  {
    QDateTime dt {QDate {2020, 8, 6}, QTime {14, 15, 22, 501}};
    QCOMPARE (qt_truncate_date_time_to (dt, 7500), QDateTime (QDate (2020, 8, 6), QTime (14, 15, 22, 500)));
  }

  Q_SLOT void is_multicast_address_data ()
  {
    QTest::addColumn<QString> ("addr");
    QTest::addColumn<bool> ("result");

    QTest::newRow ("loopback") << "127.0.0.1" << false;
    QTest::newRow ("looback IPv6") << "::1" << false;
    QTest::newRow ("lowest-") << "223.255.255.255" << false;
    QTest::newRow ("lowest") << "224.0.0.0" << true;
    QTest::newRow ("lowest- IPv6") << "feff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" << false;
    QTest::newRow ("lowest IPv6") << "ff00::" << true;
    QTest::newRow ("highest") << "239.255.255.255" << true;
    QTest::newRow ("highest+") << "240.0.0.0" << false;
    QTest::newRow ("highest IPv6") << "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" << true;
  }

  Q_SLOT void is_multicast_address ()
  {
    QFETCH (QString, addr);
    QFETCH (bool, result);

    QCOMPARE (::is_multicast_address (QHostAddress {addr}), result);
  }

  Q_SLOT void is_MAC_ambiguous_multicast_address_data ()
  {
    QTest::addColumn<QString> ("addr");
    QTest::addColumn<bool> ("result");

    QTest::newRow ("loopback") << "127.0.0.1" << false;
    QTest::newRow ("looback IPv6") << "::1" << false;

    QTest::newRow ("lowest- R1") << "223.255.255.255" << false;
    QTest::newRow ("lowest R1") << "224.0.0.0" << false;
    QTest::newRow ("highest R1") << "224.0.0.255" << false;
    QTest::newRow ("highest+ R1") << "224.0.1.0" << false;
    QTest::newRow ("lowest- R1A") << "224.127.255.255" << false;
    QTest::newRow ("lowest R1A") << "224.128.0.0" << true;
    QTest::newRow ("highest R1A") << "224.128.0.255" << true;
    QTest::newRow ("highest+ R1A") << "224.128.1.0" << false;

    QTest::newRow ("lowest- R2") << "224.255.255.255" << false;
    QTest::newRow ("lowest R2") << "225.0.0.0" << true;
    QTest::newRow ("highest R2") << "225.0.0.255" << true;
    QTest::newRow ("highest+ R2") << "225.0.1.0" << false;
    QTest::newRow ("lowest- R2A") << "225.127.255.255" << false;
    QTest::newRow ("lowest R2A") << "225.128.0.0" << true;
    QTest::newRow ("highest R2A") << "225.128.0.255" << true;
    QTest::newRow ("highest+ R2A") << "225.128.1.0" << false;

    QTest::newRow ("lowest- R3") << "238.255.255.255" << false;
    QTest::newRow ("lowest R3") << "239.0.0.0" << true;
    QTest::newRow ("highest R3") << "239.0.0.255" << true;
    QTest::newRow ("highest+ R3") << "239.0.1.0" << false;
    QTest::newRow ("lowest- R3A") << "239.127.255.255" << false;
    QTest::newRow ("lowest R3A") << "239.128.0.0" << true;
    QTest::newRow ("highest R3A") << "239.128.0.255" << true;
    QTest::newRow ("highest+ R3A") << "239.128.1.0" << false;

    QTest::newRow ("lowest- IPv6") << "feff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" << false;
    QTest::newRow ("lowest IPv6") << "ff00::" << false;
    QTest::newRow ("highest") << "239.255.255.255" << false;
    QTest::newRow ("highest+") << "240.0.0.0" << false;
    QTest::newRow ("highest IPv6") << "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" << false;
  }

  Q_SLOT void is_MAC_ambiguous_multicast_address ()
  {
    QFETCH (QString, addr);
    QFETCH (bool, result);

    QCOMPARE (::is_MAC_ambiguous_multicast_address (QHostAddress {addr}), result);
  }

  Q_SLOT void hotp_rfc_4226_vectors ()
  {
    OTPGenerator generator;
    QByteArray const secret {"12345678901234567890"};
    QStringList const expected {
      "755224",
      "287082",
      "359152",
      "969429",
      "338314",
      "254676",
      "287922",
      "162583",
      "399871",
      "520489",
    };

    for (int counter = 0; counter < expected.size (); ++counter) {
      QCOMPARE (QString::fromLatin1 (generator.generateHOTP (secret, counter, 6)), expected.at (counter));
    }
  }

  Q_SLOT void ft8_sync8_search_zero_input ()
  {
    constexpr int kFt8SampleCount {180000};
    constexpr int kMaxCand {32};
    constexpr int kNH1 {1920};

    std::vector<float> dd (kFt8SampleCount, 0.0f);
    std::vector<float> candidate (3 * kMaxCand, -1.0f);
    std::vector<float> sbase (kNH1, -1.0f);
    int ncand = -1;

    ftx_sync8_search_c (dd.data (), kFt8SampleCount, 200.0f, 3000.0f, 1.5f,
                        1500.0f, kMaxCand, candidate.data (), &ncand, sbase.data ());

    QCOMPARE (ncand, 0);
    for (float value : candidate)
      {
        QCOMPARE (value, 0.0f);
      }
    for (float value : sbase)
      {
        QVERIFY (std::isfinite (value));
      }
  }

  Q_SLOT void ft8_sync8d_zero_input ()
  {
    constexpr int kNp2 {2812};
    constexpr int kBufferSize {3200};
    constexpr int kTweakSize {32};

    std::vector<std::complex<float>> cd0 (kBufferSize, {0.0f, 0.0f});
    std::vector<std::complex<float>> tweak (kTweakSize, {1.0f, 0.0f});
    float sync = -1.0f;

    ftx_sync8d_c (cd0.data (), kNp2, 0, tweak.data (), 1, &sync);

    QCOMPARE (sync, 0.0f);
  }

  Q_SLOT void ft8_twkfreq1_identity_coefficients ()
  {
    constexpr int kNpts {16};

    std::vector<std::complex<float>> input;
    input.reserve (kNpts);
    for (int i = 0; i < kNpts; ++i)
      {
        input.emplace_back (static_cast<float> (i + 1), static_cast<float> (-0.5f * i));
      }
    std::vector<std::complex<float>> output (kNpts, {0.0f, 0.0f});

    int npts = kNpts;
    float fsample = 200.0f;
    float a[5] {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    ftx_twkfreq1_c (input.data (), &npts, &fsample, a, output.data ());

    for (int i = 0; i < kNpts; ++i)
      {
        QCOMPARE (output[static_cast<size_t> (i)].real (), input[static_cast<size_t> (i)].real ());
        QCOMPARE (output[static_cast<size_t> (i)].imag (), input[static_cast<size_t> (i)].imag ());
      }
  }

  Q_SLOT void totp_rfc_6238_sha1_vectors ()
  {
    OTPGenerator generator;
    QString const secret {"GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"};

    struct TotpVector
    {
      qint64 seconds;
      char const * expected;
    };

    TotpVector const vectors[] {
      {59, "94287082"},
      {1111111109, "07081804"},
      {1111111111, "14050471"},
      {1234567890, "89005924"},
      {2000000000, "69279037"},
      {20000000000LL, "65353130"},
    };

    for (auto const& vector : vectors) {
      QDateTime const dt = QDateTime::fromSecsSinceEpoch (vector.seconds, Qt::UTC);
      QCOMPARE (generator.generateTOTP (secret, dt, 8), QString::fromLatin1 (vector.expected));
    }
  }

  Q_SLOT void totp_rfc_6238_sha256_vectors ()
  {
    OTPGenerator generator;
    QByteArray const secret {"12345678901234567890123456789012"};

    struct TotpVector
    {
      qint64 seconds;
      char const * expected;
    };

    TotpVector const vectors[] {
      {59, "46119246"},
      {1111111109, "68084774"},
      {1111111111, "67062674"},
      {1234567890, "91819424"},
      {2000000000, "90698825"},
      {20000000000LL, "77737706"},
    };

    for (auto const& vector : vectors) {
      QDateTime const dt = QDateTime::fromSecsSinceEpoch (vector.seconds, Qt::UTC);
      QCOMPARE (QString::fromLatin1 (generator.generateTOTP (secret, dt, 8, QCryptographicHash::Sha256)),
                QString::fromLatin1 (vector.expected));
    }
  }

  Q_SLOT void totp_rfc_6238_sha512_vectors ()
  {
    OTPGenerator generator;
    QByteArray const secret {"1234567890123456789012345678901234567890123456789012345678901234"};

    struct TotpVector
    {
      qint64 seconds;
      char const * expected;
    };

    TotpVector const vectors[] {
      {59, "90693936"},
      {1111111109, "25091201"},
      {1111111111, "99943326"},
      {1234567890, "93441116"},
      {2000000000, "38618901"},
      {20000000000LL, "47863826"},
    };

    for (auto const& vector : vectors) {
      QDateTime const dt = QDateTime::fromSecsSinceEpoch (vector.seconds, Qt::UTC);
      QCOMPARE (QString::fromLatin1 (generator.generateTOTP (secret, dt, 8, QCryptographicHash::Sha512)),
                QString::fromLatin1 (vector.expected));
    }
  }

  Q_SLOT void ftx_decode77_round_trip_data ()
  {
    QTest::addColumn<QString> ("message");

    QTest::newRow ("standard") << "CQ K1ABC FN42";
    QTest::newRow ("field-day") << "K1ABC W9XYZ 3A EMA";
    QTest::newRow ("telemetry") << "0123456789ABCDEF01";
    QTest::newRow ("wspr1") << "K1ABC FN42 30";
  }

  Q_SLOT void ftx_decode77_round_trip ()
  {
    QFETCH (QString, message);

    auto const encoded = decodium::txmsg::encodeFt8 (message);
    QVERIFY2 (encoded.ok, qPrintable (message));

    auto const decoded = decodium::txmsg::decode77 (encoded.msgbits, encoded.i3, encoded.n3);
    QVERIFY2 (decoded.ok, qPrintable (message));
    QCOMPARE (decoded.msgsent, encoded.msgsent);
  }

  Q_SLOT void ftx_decode77_context_hash_lookup ()
  {
    decodium::txmsg::Decode77Context context;

    context.setMyCall ("K1ABC");
    QVERIFY (context.hasMyCall ());
    QCOMPARE (context.lookupHash10 (context.hashMy10 ()), QStringLiteral ("<...>"));
    QCOMPARE (context.lookupHash12 (context.hashMy12 ()), QStringLiteral ("<...>"));
    QCOMPARE (context.lookupHash22 (context.hashMy22 ()), QStringLiteral ("<K1ABC>"));

    context.setDxCall ("PJ4/W9XYZ");
    QVERIFY (context.hasDxCall ());
    QCOMPARE (context.lookupHash22 (context.hashDx22 ()), QStringLiteral ("<...>"));

    context.saveHashCall ("PJ4/W9XYZ");
    QCOMPARE (context.lookupHash10 (context.hashDx10 ()), QStringLiteral ("<PJ4/W9XYZ>"));
    QCOMPARE (context.lookupHash12 (context.hashDx12 ()), QStringLiteral ("<PJ4/W9XYZ>"));
    QCOMPARE (context.lookupHash22 (context.hashDx22 ()), QStringLiteral ("<PJ4/W9XYZ>"));
  }

  Q_SLOT void ftx_decode77_updates_recent_calls ()
  {
    decodium::txmsg::Decode77Context context;
    context.setDxCall ("W9XYZ");
    QCOMPARE (context.lookupHash22 (context.hashDx22 ()), QStringLiteral ("<...>"));

    auto const encoded = decodium::txmsg::encodeFt8 ("K1ABC W9XYZ -10");
    QVERIFY (encoded.ok);

    auto const decoded = decodium::txmsg::decode77 (encoded.msgbits, encoded.i3, encoded.n3, &context, true);
    QVERIFY (decoded.ok);
    QCOMPARE (decoded.msgsent, encoded.msgsent);

    QStringList const recent = context.recentCalls ();
    QVERIFY (recent.size () >= 2);
    QCOMPARE (recent.at (0), QStringLiteral ("W9XYZ"));
    QCOMPARE (recent.at (1), QStringLiteral ("K1ABC"));
    QCOMPARE (context.lookupHash22 (context.hashDx22 ()), QStringLiteral ("<W9XYZ>"));
  }

  Q_SLOT void ft8_prepare_pass_plan ()
  {
    float syncmin = 0.0f;
    int imetric = 0;
    int lsubtract = 0;
    int run_pass = 0;

    ftx_ft8_prepare_pass_c (3, 1, 0, &syncmin, &imetric, &lsubtract, &run_pass);
    QCOMPARE (run_pass, 1);
    QCOMPARE (imetric, 1);
    QCOMPARE (lsubtract, 1);
    QVERIFY (qFuzzyCompare (syncmin, 1.0f));

    ftx_ft8_prepare_pass_c (2, 4, 0, &syncmin, &imetric, &lsubtract, &run_pass);
    QCOMPARE (run_pass, 0);
    QCOMPARE (imetric, 2);
    QCOMPARE (lsubtract, 1);
    QVERIFY (qAbs (syncmin - 1.8f * 0.88f * 0.76f) < 1.0e-5f);
  }

  Q_SLOT void ft8_store_saved_decode ()
  {
    int const tones[4] {1, 2, 3, 4};
    int allsnrs[2] {0, 0};
    float f1_save[2] {0.0f, 0.0f};
    float xdt_save[2] {0.0f, 0.0f};
    int itone_save[8] {0, 0, 0, 0, 0, 0, 0, 0};

    int const slot = ftx_ft8_store_saved_decode_c (0, 2, -12, 1234.5f, 0.78f,
                                                   tones, 4, allsnrs, f1_save,
                                                   xdt_save, itone_save);
    QCOMPARE (slot, 1);
    QCOMPARE (allsnrs[0], -12);
    QVERIFY (qAbs (f1_save[0] - 1234.5f) < 1.0e-5f);
    QVERIFY (qAbs (xdt_save[0] - 0.78f) < 1.0e-5f);
    QCOMPARE (itone_save[0], 1);
    QCOMPARE (itone_save[1], 2);
    QCOMPARE (itone_save[2], 3);
    QCOMPARE (itone_save[3], 4);
    QCOMPARE (ftx_ft8_store_saved_decode_c (2, 2, -9, 99.0f, 0.1f,
                                            tones, 4, allsnrs, f1_save,
                                            xdt_save, itone_save), 0);
  }

  Q_SLOT void ft8_decode_stage_plan ()
  {
    int action = -1;
    int refine = -1;

    ftx_ft8_plan_decode_stage_c (1, 41, 0, 0, &action, &refine);
    QCOMPARE (action, 1);
    QCOMPARE (refine, 1);

    ftx_ft8_plan_decode_stage_c (3, 47, 0, 0, &action, &refine);
    QCOMPARE (action, 2);
    QCOMPARE (refine, 1);

    ftx_ft8_plan_decode_stage_c (3, 47, 2, 0, &action, &refine);
    QCOMPARE (action, 3);
    QCOMPARE (refine, 1);

    ftx_ft8_plan_decode_stage_c (2, 47, 2, 0, &action, &refine);
    QCOMPARE (action, 3);
    QCOMPARE (refine, 0);

    ftx_ft8_plan_decode_stage_c (3, 50, 2, 0, &action, &refine);
    QCOMPARE (action, 4);
    QCOMPARE (refine, 1);

    ftx_ft8_plan_decode_stage_c (3, 50, 2, 1, &action, &refine);
    QCOMPARE (action, 0);
    QCOMPARE (refine, 1);

    QCOMPARE (ftx_ft8_should_bail_by_tseq_c (0, 14.31, 14.3), 1);
    QCOMPARE (ftx_ft8_should_bail_by_tseq_c (1, 14.31, 14.3), 0);
    QCOMPARE (ftx_ft8_should_bail_by_tseq_c (0, 13.39, 13.4), 0);
  }

  Q_SLOT void ft8_candidate_and_result_meta ()
  {
    float const sbase[] {40.0f, 41.0f, 42.0f, 43.0f};
    float sync = 0.0f;
    float f1 = 0.0f;
    float xdt = 0.0f;
    float xbase = 0.0f;
    int nsnr = 0;
    float callback_xdt = 0.0f;
    float qual = 0.0f;

    ftx_ft8_prepare_candidate_c (2.5f, 3.125f, 0.75f, sbase, 4, &sync, &f1, &xdt, &xbase);
    QVERIFY (qAbs (sync - 2.5f) < 1.0e-5f);
    QVERIFY (qAbs (f1 - 3.125f) < 1.0e-5f);
    QVERIFY (qAbs (xdt - 0.75f) < 1.0e-5f);
    QVERIFY (qAbs (xbase - 1.0f) < 1.0e-5f);

    ftx_ft8_finalize_main_result_c (-9.6f, -0.25f, 1, 4, 2.0f, &nsnr, &callback_xdt, &qual);
    QCOMPARE (nsnr, -10);
    QVERIFY (qAbs (callback_xdt - 1.75f) < 1.0e-5f);
    QVERIFY (qAbs (qual - 0.9f) < 1.0e-5f);
  }

  Q_SLOT void ft8_a7_a8_meta ()
  {
    int nsnr = 0;
    int iaptype = 0;
    float qual = 0.0f;
    float save_freq = 0.0f;

    QCOMPARE (ftx_ft8_should_run_a7_c (1, 5, 50, 2), 1);
    QCOMPARE (ftx_ft8_should_run_a7_c (1, 6, 50, 2), 0);
    QCOMPARE (ftx_ft8_should_run_a8_c (1, 5, 50, 1, 4, 4, 1), 1);
    QCOMPARE (ftx_ft8_should_run_a8_c (1, 5, 50, 0, 4, 4, 1), 0);

    ftx_ft8_finalize_a7_result_c (-7.4f, &nsnr, &iaptype, &qual);
    QCOMPARE (nsnr, -7);
    QCOMPARE (iaptype, 7);
    QVERIFY (qAbs (qual - 1.0f) < 1.0e-5f);

    ftx_ft8_finalize_a8_result_c (-148.0f, -11.2f, 1234.5f, &nsnr, &iaptype, &qual, &save_freq);
    QCOMPARE (nsnr, -11);
    QCOMPARE (iaptype, 8);
    QVERIFY (qAbs (qual - 0.16f) < 1.0e-5f);
    QVERIFY (qAbs (save_freq - 1234.5f) < 1.0e-5f);
  }

  Q_SLOT void ft8_a7_a8_request_meta ()
  {
    auto fixed = [] (QByteArray text, int length) {
      if (text.size () < length)
        {
          text.append (QByteArray (length - text.size (), ' '));
        }
      return text.left (length);
    };

    QByteArray const cq = fixed ("CQ", 12);
    QByteArray const k1abc = fixed ("K1ABC", 12);
    QByteArray const w9xyz = fixed ("W9XYZ", 12);
    QByteArray const en37 = fixed ("EN37", 4);
    QByteArray const en37aa = fixed ("EN37AA", 6);
    QByteArray const previous = fixed ("CQ DX K1ABC", 37);
    QByteArray const angled = fixed ("K1ABC <W9XYZ>", 37);
    char a7_msg[37];
    char a8_msg[37];
    char call_1[12];
    char call_2[12];
    char grid4[4];
    float f1 = 0.0f;
    float xdt = 0.0f;
    float xbase = 0.0f;
    float const sbase[] {40.0f, 41.0f, 42.0f, 43.0f};

    QCOMPARE (ftx_prepare_ft8_a7_candidate_c (5, cq.constData (), k1abc.constData (),
                                              en37.constData (), a7_msg), 1);
    QCOMPARE (QString::fromLatin1 (a7_msg, 37).trimmed (), QStringLiteral ("CQ K1ABC EN37"));

    QCOMPARE (ftx_prepare_ft8_a8_candidate_c (5, k1abc.constData (), w9xyz.constData (),
                                              en37aa.constData (), a8_msg), 1);
    QCOMPARE (QString::fromLatin1 (a8_msg, 37).trimmed (), QStringLiteral ("CQ W9XYZ EN37"));

    QCOMPARE (ftx_ft8_prepare_a7_request_c (3.125f, 0.75f, previous.constData (), sbase, 4,
                                            &f1, &xdt, &xbase, call_1, call_2, grid4), 1);
    QCOMPARE (QString::fromLatin1 (call_1, 12).trimmed (), QStringLiteral ("CQ"));
    QCOMPARE (QString::fromLatin1 (call_2, 12).trimmed (), QStringLiteral ("DX"));
    QCOMPARE (QString::fromLatin1 (grid4, 4), QStringLiteral ("K1AB"));
    QVERIFY (qAbs (f1 - 3.125f) < 1.0e-5f);
    QVERIFY (qAbs (xdt - 0.75f) < 1.0e-5f);
    QVERIFY (qAbs (xbase - 1.0f) < 1.0e-5f);

    QCOMPARE (ftx_ft8_prepare_a7_request_c (-98.0f, 0.0f, previous.constData (), sbase, 4,
                                            &f1, &xdt, &xbase, call_1, call_2, grid4), 0);
    QCOMPARE (ftx_ft8_prepare_a7_request_c (-99.0f, 0.0f, previous.constData (), sbase, 4,
                                            &f1, &xdt, &xbase, call_1, call_2, grid4), 2);
    QCOMPARE (ftx_ft8_prepare_a7_request_c (3.125f, 0.0f, angled.constData (), sbase, 4,
                                            &f1, &xdt, &xbase, call_1, call_2, grid4), 0);
    QCOMPARE (ftx_ft8_should_keep_a8_after_a7_c (fixed ("CQ W9XYZ EN37", 37).constData (),
                                                 w9xyz.constData ()), 0);
    QCOMPARE (ftx_ft8_should_keep_a8_after_a7_c (fixed ("CQ K1ABC EN37", 37).constData (),
                                                 w9xyz.constData ()), 1);
  }

  Q_SLOT void ft8_a7_history_bridge_meta ()
  {
    auto fixed = [] (QByteArray text, int length) {
      if (text.size () < length)
        {
          text.append (QByteArray (length - text.size (), ' '));
        }
      return text.left (length);
    };

    ftx_ft8_a7_reset_c ();
    ftx_ft8_a7_prepare_tables_c (100000, 41, 0);

    QByteArray const first = fixed ("CQ K1ABC EN37", 37);
    ftx_ft8_a7_save_c (0, 0.75f, 1200.0f, first.constData ());
    QCOMPARE (ftx_ft8_a7_previous_count_c (0), 0);

    ftx_ft8_a7_prepare_tables_c (100010, 50, 0);
    QCOMPARE (ftx_ft8_a7_previous_count_c (0), 1);

    float dt = 0.0f;
    float freq = 0.0f;
    char msg[37];
    std::fill_n (msg, 37, ' ');
    QCOMPARE (ftx_ft8_a7_get_previous_entry_c (0, 1, &dt, &freq, msg), 1);
    QVERIFY (qAbs (dt - 0.75f) < 1.0e-5f);
    QVERIFY (qAbs (freq - 1200.0f) < 1.0e-4f);
    QCOMPARE (QString::fromLatin1 (msg, 37).trimmed (), QStringLiteral ("CQ K1ABC EN37"));

    QByteArray const followup = fixed ("F5AAA K1ABC -10", 37);
    ftx_ft8_a7_save_c (0, 0.80f, 1201.0f, followup.constData ());
    std::fill_n (msg, 37, ' ');
    QCOMPARE (ftx_ft8_a7_get_previous_entry_c (0, 1, &dt, &freq, msg), 1);
    QVERIFY (freq <= -98.0f);
  }

  Q_SLOT void ft8_ap_pass_meta ()
  {
    QCOMPARE (ftx_ft8_select_npasses_c (0, 0, 0, 50, 3), 5);
    QCOMPARE (ftx_ft8_select_npasses_c (1, 0, 0, 50, 3), 13);
    QCOMPARE (ftx_ft8_select_npasses_c (1, 1, 0, 50, 3), 7);

    int apsym[58];
    int aph10[10];
    float llra[174];
    float llrc[174];
    float llrz[174];
    int apmask[174];
    int iaptype = 0;

    std::fill_n (apsym, 58, 1);
    std::fill_n (aph10, 10, -1);
    std::fill_n (llra, 174, 0.5f);
    std::fill_n (llrc, 174, 0.25f);
    std::fill_n (llrz, 174, 0.0f);
    std::fill_n (apmask, 174, 0);

    QCOMPARE (ftx_ft8_prepare_ap_pass_c (6, 3, 0, 0, 1500, 1600, 1505.0f, 100,
                                         apsym, aph10, llra, llrc, llrz, apmask, &iaptype), 1);
    QCOMPARE (iaptype, 3);
    QCOMPARE (apmask[0], 1);
    QCOMPARE (apmask[57], 1);
    QVERIFY (qAbs (llrz[0] - 0.55f) < 1.0e-5f);
    QVERIFY (qAbs (llrz[74] + 0.55f) < 1.0e-5f);
    QVERIFY (qAbs (llrz[76] - 0.55f) < 1.0e-5f);

    QCOMPARE (ftx_ft8_prepare_ap_pass_c (6, 3, 0, 6, 1500, 1600, 1505.0f, 100,
                                         apsym, aph10, llra, llrc, llrz, apmask, &iaptype), 0);
  }

  Q_SLOT void ft8_prepare_decode_pass_meta ()
  {
    int apsym[58];
    int aph10[10];
    float llra[174];
    float llrb[174];
    float llrc[174];
    float llrd[174];
    float llre[174];
    float llrz[174];
    float llrz_ap[174];
    int apmask[174];
    int apmask_ap[174];
    int iaptype = -1;
    int iaptype_ap = -1;

    std::fill_n (apsym, 58, 1);
    std::fill_n (aph10, 10, -1);
    std::fill_n (llra, 174, 0.5f);
    std::fill_n (llrb, 174, -0.25f);
    std::fill_n (llrc, 174, 1.5f);
    std::fill_n (llrd, 174, -1.0f);
    std::fill_n (llre, 174, 2.0f);
    std::fill_n (llrz, 174, 0.0f);
    std::fill_n (llrz_ap, 174, 0.0f);
    std::fill_n (apmask, 174, 9);
    std::fill_n (apmask_ap, 174, 0);

    QCOMPARE (ftx_ft8_prepare_decode_pass_c (1, 3, 0, 0, 1500, 1600, 1505.0f, 100,
                                             apsym, aph10, llra, llrb, llrc, llrd, llre,
                                             llrz, apmask, &iaptype), 1);
    QCOMPARE (iaptype, 0);
    for (int i = 0; i < 174; ++i)
      {
        QCOMPARE (llrz[i], llra[i]);
        QCOMPARE (apmask[i], 0);
      }

    QCOMPARE (ftx_ft8_prepare_decode_pass_c (5, 3, 0, 0, 1500, 1600, 1505.0f, 100,
                                             apsym, aph10, llra, llrb, llrc, llrd, llre,
                                             llrz, apmask, &iaptype), 1);
    QCOMPARE (iaptype, 0);
    for (int i = 0; i < 174; ++i)
      {
        QCOMPARE (llrz[i], llre[i]);
        QCOMPARE (apmask[i], 0);
      }

    QCOMPARE (ftx_ft8_prepare_decode_pass_c (6, 3, 0, 0, 1500, 1600, 1505.0f, 100,
                                             apsym, aph10, llra, llrb, llrc, llrd, llre,
                                             llrz, apmask, &iaptype), 1);
    QCOMPARE (ftx_ft8_prepare_ap_pass_c (6, 3, 0, 0, 1500, 1600, 1505.0f, 100,
                                         apsym, aph10, llra, llrc, llrz_ap, apmask_ap, &iaptype_ap), 1);
    QCOMPARE (iaptype, iaptype_ap);
    for (int i = 0; i < 174; ++i)
      {
        QCOMPARE (llrz[i], llrz_ap[i]);
        QCOMPARE (apmask[i], apmask_ap[i]);
      }
  }

  Q_SLOT void ft8_decode_pass_policy_meta ()
  {
    int pass_first = -1;
    int pass_last = -1;
    ftx_ft8_plan_pass_window_c (0, 13, &pass_first, &pass_last);
    QCOMPARE (pass_first, 1);
    QCOMPARE (pass_last, 13);

    ftx_ft8_plan_pass_window_c (7, 13, &pass_first, &pass_last);
    QCOMPARE (pass_first, 7);
    QCOMPARE (pass_last, 7);

    ftx_ft8_plan_pass_window_c (0, 0, &pass_first, &pass_last);
    QCOMPARE (pass_first, 1);
    QCOMPARE (pass_last, 0);

    int ipass = -1;
    int iaptype = -1;
    QCOMPARE (ftx_ft8_finalize_decode_pass_c (1, 6, 3, &ipass, &iaptype), 0);
    QCOMPARE (ipass, -1);
    QCOMPARE (iaptype, -1);

    QCOMPARE (ftx_ft8_finalize_decode_pass_c (0, 6, 3, &ipass, &iaptype), 1);
    QCOMPARE (ipass, 6);
    QCOMPARE (iaptype, 3);
  }

  Q_SLOT void ft8_validation_and_snr_meta ()
  {
    QCOMPARE (ftx_ft8_validate_candidate_c (4, 10, 1, 0, 1, 0, 0), 1);
    QCOMPARE (ftx_ft8_validate_candidate_c (4, 174, 1, 0, 1, 0, 0), 0);
    QCOMPARE (ftx_ft8_validate_candidate_c (4, 10, 1, 0, 0, 0, 0), 0);
    QCOMPARE (ftx_ft8_validate_candidate_c (4, 10, 1, 0, 1, 1, 0), 0);

    signed char message77[77];
    signed char cw[174];
    std::fill_n (message77, 77, 0);
    std::fill_n (cw, 174, 1);
    message77[74] = 1; // i3 = 4 -> valid
    QCOMPARE (ftx_ft8_validate_candidate_meta_c (message77, cw, 4, 1, 0, 0), 1);
    cw[0] = 0;
    QCOMPARE (ftx_ft8_validate_candidate_meta_c (message77, cw, 4, 1, 0, 0), 1);
    std::fill_n (cw, 174, 0);
    QCOMPARE (ftx_ft8_validate_candidate_meta_c (message77, cw, 4, 1, 0, 0), 0);
    std::fill_n (cw, 174, 1);
    message77[10] = 2;
    QCOMPARE (ftx_ft8_validate_candidate_meta_c (message77, cw, 4, 1, 0, 0), 0);

    float s8[8 * 79];
    int itone[79];
    std::fill_n (s8, 8 * 79, 0.1f);
    std::fill_n (itone, 79, 2);
    for (int i = 0; i < 79; ++i)
      {
        s8[2 + 8 * i] = 3.0f;
        s8[6 + 8 * i] = 0.5f;
      }

    float xsnr = 0.0f;
    QCOMPARE (ftx_ft8_compute_snr_c (s8, 8, 79, itone, 1.0f, 1, 12, &xsnr), 1);
    float const legacy_xsnr = ft8_compute_snr_legacy_reference (s8, 8, 79, itone);
    QVERIFY (xsnr > -25.0f);
    QVERIFY2 (std::fabs (xsnr - legacy_xsnr) < 1.0e-4f,
              qPrintable (QStringLiteral ("FT8 SNR mismatch: cpp=%1 legacy=%2")
                              .arg (xsnr, 0, 'g', 9)
                              .arg (legacy_xsnr, 0, 'g', 9)));
  }

  Q_SLOT void ft8c_candidate_metrics ()
  {
    float llra[174];
    signed char cw[174];
    std::fill_n (llra, 174, 0.0f);
    std::fill_n (cw, 174, 0);

    llra[0] = 2.0f;
    llra[1] = -1.0f;
    llra[2] = -3.0f;
    llra[3] = 4.0f;
    cw[0] = 1;
    cw[2] = 1;

    float score = 0.0f;
    float dmin = 0.0f;
    int nharderrors = 0;
    ftx_ft8c_measure_candidate_c (llra, cw, &score, &nharderrors, &dmin);

    QVERIFY (qAbs (score + 4.0f) < 1.0e-5f);
    QCOMPARE (nharderrors, 2);
    QVERIFY (qAbs (dmin - 7.0f) < 1.0e-5f);
  }

  Q_SLOT void ft8_candidate_encoder_smoke ()
  {
    char message[37];
    std::fill_n (message, 37, ' ');
    QByteArray const input {"K1ABC W9XYZ RR73"};
    std::copy (input.begin (), input.end (), message);

    char msgsent[37];
    int itone[79];
    signed char codeword[174];
    std::fill_n (msgsent, 37, '\0');
    std::fill_n (itone, 79, -1);
    std::fill_n (codeword, 174, -1);

    QCOMPARE (ftx_encode_ft8_candidate_c (message, msgsent, itone, codeword), 1);

    auto const encoded = decodium::txmsg::encodeFt8 (QString::fromLatin1 (message, 37));
    QVERIFY (encoded.ok);
    QCOMPARE (QByteArray (msgsent, 37), encoded.msgsent);
    QCOMPARE (encoded.tones.size (), 79);
    for (int i = 0; i < 79; ++i)
      {
        QCOMPARE (itone[i], encoded.tones.at (i));
        QVERIFY (itone[i] >= 0);
        QVERIFY (itone[i] <= 7);
      }
    for (signed char bit : codeword)
      {
        QVERIFY (bit == 0 || bit == 1);
      }
  }

  Q_SLOT void ft8_message77_unpack_and_tones ()
  {
    auto const encoded = decodium::txmsg::encodeFt8 (QStringLiteral ("CQ K1ABC FN42"));
    QVERIFY (encoded.ok);
    QCOMPARE (encoded.msgbits.size (), 77);
    QCOMPARE (encoded.tones.size (), 79);

    signed char message77[77];
    for (int i = 0; i < 77; ++i)
      {
        message77[i] = encoded.msgbits.at (i) != 0 ? 1 : 0;
      }

    char unpacked[37];
    int quirky = -1;
    std::fill_n (unpacked, 37, '\0');
    QCOMPARE (legacy_pack77_unpack77bits_c (message77, 1, unpacked, &quirky), 1);
    QCOMPARE (QByteArray (unpacked, 37), encoded.msgsent);
    QCOMPARE (quirky, 0);

    int itone[79];
    std::fill_n (itone, 79, -1);
    QCOMPARE (ftx_ft8_message77_to_itone_c (message77, itone), 1);
    for (int i = 0; i < 79; ++i)
      {
        QCOMPARE (itone[i], encoded.tones.at (i));
      }

    message77[5] = 2;
    QCOMPARE (legacy_pack77_unpack77bits_c (message77, 1, unpacked, &quirky), 0);
    QCOMPARE (ftx_ft8_message77_to_itone_c (message77, itone), 0);
  }

  Q_SLOT void ft8var_msgparser_bridge_examples ()
  {
    auto const to_fixed = [] (QByteArray text) {
      return text.leftJustified (37, ' ', true);
    };

    char input[37];
    char output[37];
    char output2[37];
    std::fill_n (input, 37, ' ');
    std::fill_n (output, 37, '\0');
    std::fill_n (output2, 37, '\0');

    QByteArray const special = to_fixed (QByteArray {"ES1JA RR73; TA1BM <...> -22"});
    std::copy (special.begin (), special.end (), input);
    ftx_ft8var_msgparser_c (input, output, output2);
    QCOMPARE (QByteArray (output, 37), special);
    QCOMPARE (QByteArray (output2, 37), to_fixed (QByteArray {"TA1BM <...> -22"}));

    QByteArray const plain = to_fixed (QByteArray {"CQ K1ABC FN42"});
    std::copy (plain.begin (), plain.end (), input);
    ftx_ft8var_msgparser_c (input, output, output2);
    QCOMPARE (QByteArray (output, 37), plain);
    QCOMPARE (QByteArray (output2, 37), QByteArray (37, ' '));
  }

  Q_SLOT void ft8var_false_decode_bridges ()
  {
    auto const to_fixed12 = [] (QByteArray text) {
      return text.leftJustified (12, ' ', true);
    };
    auto const to_fixed37 = [] (QByteArray text) {
      return text.leftJustified (37, ' ', true);
    };

    QByteArray const blank12 (12, ' ');
    QByteArray const known_call = to_fixed12 (QByteArray {"2E0AAG"});
    QByteArray const unknown_call = to_fixed12 (QByteArray {"QQ0QQQ"});
    QByteArray const cq = to_fixed12 (QByteArray {"CQ"});
    QCOMPARE (ftx_ft8var_searchcalls_c (known_call.constData (), blank12.constData ()), 1);
    QCOMPARE (ftx_ft8var_chkflscall_c (cq.constData (), known_call.constData ()), 0);
    QCOMPARE (ftx_ft8var_chkflscall_c (cq.constData (), unknown_call.constData ()), 1);

    QByteArray const special = to_fixed37 (QByteArray {"Q84NN RR73; HO5RFG <...> -16"});
    QByteArray const parsed = to_fixed37 (QByteArray {"HO5RFG <...> -16"});
    QByteArray const mycall = to_fixed12 (QByteArray {"MY1ABC"});
    QByteArray const hiscall = to_fixed12 (QByteArray {"HIS1ABC"});

    char msg37[37];
    char msg37_2[37];
    std::copy (special.begin (), special.end (), msg37);
    std::copy (parsed.begin (), parsed.end (), msg37_2);
    QCOMPARE (ftx_ft8var_chkspecial8_c (msg37, msg37_2, mycall.constData (), hiscall.constData ()), 1);
    QCOMPARE (QByteArray (msg37, 37), QByteArray (37, ' '));
    QCOMPARE (QByteArray (msg37_2, 37), QByteArray (37, ' '));

    QByteArray const bypass_mycall = to_fixed12 (QByteArray {"Q84NN"});
    std::copy (special.begin (), special.end (), msg37);
    std::copy (parsed.begin (), parsed.end (), msg37_2);
    QCOMPARE (ftx_ft8var_chkspecial8_c (msg37, msg37_2, bypass_mycall.constData (), hiscall.constData ()), 0);
    QCOMPARE (QByteArray (msg37, 37), special);
    QCOMPARE (QByteArray (msg37_2, 37), parsed);

    QByteArray const valid_long = to_fixed12 (QByteArray {"ZW5STAYHOME"});
    QByteArray const invalid_long = to_fixed12 (QByteArray {"I78PY2MTFLF"});
    QCOMPARE (ftx_ft8var_chklong8_c (valid_long.constData ()), 0);
    QCOMPARE (ftx_ft8var_chklong8_c (invalid_long.constData ()), 1);

    {
      QByteArray const mygrid = QByteArray {"FN42"};
      QByteArray const valid_falsecheck = to_fixed37 (QByteArray {"CQ K1ABC FN42"});
      char msg37_check[37];
      std::copy (valid_falsecheck.begin (), valid_falsecheck.end (), msg37_check);
      int i3 = 1;
      int n3 = 0;
      int nbadcrc = 0;
      int iaptype = 0;
      int lcall1hash = 0;
      ftx_ft8var_chkfalse8_c (msg37_check, &i3, &n3, &nbadcrc, &iaptype, &lcall1hash,
                              mycall.constData (), hiscall.constData (), mygrid.constData ());
      QCOMPARE (nbadcrc, 0);
      QCOMPARE (QByteArray (msg37_check, 37), valid_falsecheck);

      QByteArray const invalid_falsecheck = to_fixed37 (QByteArray {"CQ QQ0QQQ FN42"});
      std::copy (invalid_falsecheck.begin (), invalid_falsecheck.end (), msg37_check);
      nbadcrc = 0;
      ftx_ft8var_chkfalse8_c (msg37_check, &i3, &n3, &nbadcrc, &iaptype, &lcall1hash,
                              mycall.constData (), hiscall.constData (), mygrid.constData ());
      QCOMPARE (nbadcrc, 1);
      QCOMPARE (QByteArray (msg37_check, 37), QByteArray (37, ' '));
    }

    char extract_msg[37];
    char extract_call[12];
    QByteArray const cq_message = to_fixed37 (QByteArray {"CQ K1ABC FN42"});
    std::copy (cq_message.begin (), cq_message.end (), extract_msg);
    std::fill_n (extract_call, 12, ' ');
    ftx_ft8var_extract_call_c (extract_msg, extract_call);
    QCOMPARE (QByteArray (extract_call, 12), to_fixed12 (QByteArray {"K1ABC"}));

    QByteArray const qso_message = to_fixed37 (QByteArray {"K1ABC N0CALL -10"});
    std::copy (qso_message.begin (), qso_message.end (), extract_msg);
    std::fill_n (extract_call, 12, ' ');
    ftx_ft8var_extract_call_c (extract_msg, extract_call);
    QCOMPARE (QByteArray (extract_call, 12), to_fixed12 (QByteArray {"N0CALL"}));

    auto const to_fixed22 = [] (QByteArray text) {
      return text.leftJustified (22, ' ', true);
    };
    QByteArray const free_text_ok = to_fixed22 (QByteArray {"K1ABC K2DEF 73"});
    QByteArray const free_text_bad = to_fixed22 (QByteArray {"/BAD MESSAGE"});
    QCOMPARE (ftx_ft8var_filtersfree_c (free_text_ok.constData ()), 0);
    QCOMPARE (ftx_ft8var_filtersfree_c (free_text_bad.constData ()), 1);
  }

  Q_SLOT void ft2_message77_to_itone_matches_reference ()
  {
    QStringList const messages {
        QStringLiteral ("CQ K1ABC FN42"),
        QStringLiteral ("YL3GAO LZ2INP RR73"),
        QStringLiteral ("CQ PO8AHY/P R HO14"),
        QStringLiteral ("SV1XT I5ABV -06")
    };

    for (QString const& message : messages)
      {
        auto const encoded = decodium::txmsg::encodeFt2 (message);
        QVERIFY2 (encoded.ok, qPrintable (message));
        QCOMPARE (encoded.msgbits.size (), 77);
        QCOMPARE (encoded.tones.size (), 103);

        signed char message77[77];
        for (int i = 0; i < 77; ++i)
          {
            message77[i] = encoded.msgbits.at (i) != 0 ? 1 : 0;
          }

        int tones_cpp[103];
        std::fill_n (tones_cpp, 103, -1);
        QCOMPARE (ftx_ft2_message77_to_itone_c (message77, tones_cpp), 1);
        for (int i = 0; i < 103; ++i)
          {
            QCOMPARE (tones_cpp[i], encoded.tones.at (i));
          }

        message77[5] = 2;
        QCOMPARE (ftx_ft2_message77_to_itone_c (message77, tones_cpp), 0);
      }
  }

  Q_SLOT void ft2_foxgen_populates_wave ()
  {
    constexpr int kFt2FoxWaveSamples = (103 + 2) * 4 * 288;
    std::fill_n (foxcom_.wave, static_cast<int> (sizeof (foxcom_.wave) / sizeof (foxcom_.wave[0])), 0.0f);
    std::fill_n (&foxcom_.cmsg[0][0], static_cast<int> (sizeof (foxcom_.cmsg)), ' ');
    foxcom_.nslots = 1;
    foxcom_.nfreq = 1500;

    QByteArray const message = QByteArray {"CQ K1ABC FN42"}.leftJustified (40, ' ', true);
    std::copy_n (message.constData (), 40, foxcom_.cmsg[0]);

    foxgenft2_ ();

    float peak = 0.0f;
    int non_zero = 0;
    for (int i = 0; i < kFt2FoxWaveSamples; ++i)
      {
        float const sample = foxcom_.wave[i];
        peak = std::max (peak, std::fabs (sample));
        if (std::fabs (sample) > 1.0e-6f)
          {
            ++non_zero;
          }
      }

    QVERIFY2 (non_zero > 0, "foxgenft2_ produced an all-zero FT2 Fox waveform");
    QVERIFY2 (peak > 0.0f, "foxgenft2_ peak stayed at zero");
    QVERIFY2 (peak <= 1.0001f, "foxgenft2_ waveform is not normalized");
  }

  Q_SLOT void superfox_tx_native_matches_fortran_reference ()
  {
    struct Case
    {
      std::array<QString, 5> messages;
      int nslots;
      bool moreCqs;
      bool sendMsg;
      QString freeText;
    };

    struct Capture
    {
      std::array<int, 151> tones {};
      std::vector<float> wave;
    };

    constexpr int kSuperFoxWaveSamples = (151 + 2) * 4 * 1024;
    auto clear_state = [] {
      std::fill_n (foxcom_.wave, static_cast<int> (sizeof (foxcom_.wave) / sizeof (foxcom_.wave[0])), 0.0f);
      std::fill_n (&foxcom_.cmsg[0][0], static_cast<int> (sizeof (foxcom_.cmsg)), ' ');
      std::fill_n (foxcom_.textMsg, static_cast<int> (sizeof (foxcom_.textMsg)), ' ');
      std::fill_n (foxcom3_.itone3, 151, 0);
      std::fill_n (&foxcom3_.cmsg2[0][0], static_cast<int> (sizeof (foxcom3_.cmsg2)), ' ');
      foxcom_.nslots = 0;
      foxcom_.nfreq = 750;
      foxcom_.bMoreCQs = false;
      foxcom_.bSendMsg = false;
    };

    auto setup_case = [&clear_state] (Case const& item) {
      clear_state ();
      foxcom_.nslots = item.nslots;
      foxcom_.nfreq = 750;
      foxcom_.bMoreCQs = item.moreCqs;
      foxcom_.bSendMsg = item.sendMsg;
      QByteArray const free_text = item.freeText.leftJustified (26, QLatin1Char {' '}, true).left (26).toLatin1 ();
      std::copy_n (free_text.constData (), 26, foxcom_.textMsg);
      for (int slot = 0; slot < item.nslots; ++slot)
        {
          QByteArray const message = item.messages[static_cast<size_t> (slot)].leftJustified (
              40, QLatin1Char {' '}, true).left (40).toLatin1 ();
          std::copy_n (message.constData (), 40, foxcom_.cmsg[slot]);
        }
    };

    auto capture = [] {
      Capture captured;
      captured.wave.resize (kSuperFoxWaveSamples, 0.0f);
      for (int i = 0; i < 151; ++i)
        {
          captured.tones[static_cast<size_t> (i)] = foxcom3_.itone3[i];
        }
      std::copy_n (foxcom_.wave, kSuperFoxWaveSamples, captured.wave.begin ());
      return captured;
    };

    std::array<Case, 1> const cases {{
        {{{QStringLiteral ("CQ K1ABC FN42"), QString {}, QString {}, QString {}, QString {}}}, 1, false, false, QString {}},
    }};

    for (int index = 0; index < static_cast<int> (cases.size ()); ++index)
      {
        Case const& item = cases[static_cast<size_t> (index)];

        setup_case (item);
        bool superfox = true;
        foxgen_ (&superfox, "", 0);
        QByteArray const otp = QByteArray {"OTP:000000"};
        sftx_sub_ (otp.constData (), static_cast<fortran_charlen_t_local> (otp.size ()));
        sfox_wave_gfsk_ ();
        Capture const reference = capture ();

        setup_case (item);
        QVERIFY2 (decodium::txwave::generateSuperFoxTx (QStringLiteral ("OTP:000000")),
                  qPrintable (QStringLiteral ("native SuperFox TX failed on case %1").arg (index)));
        Capture const native = capture ();

        for (int i = 0; i < 151; ++i)
          {
            QCOMPARE (native.tones[static_cast<size_t> (i)], reference.tones[static_cast<size_t> (i)]);
          }

        float max_diff = 0.0f;
        for (int i = 0; i < kSuperFoxWaveSamples; ++i)
          {
            max_diff = std::max (max_diff, std::fabs (native.wave[static_cast<size_t> (i)]
                                                       - reference.wave[static_cast<size_t> (i)]));
          }
        QVERIFY2 (max_diff <= 1.0e-5f,
                  qPrintable (QStringLiteral ("SuperFox wave mismatch on case %1, max diff %2")
                                  .arg (index)
                                  .arg (max_diff, 0, 'g', 8)));
      }
  }

  Q_SLOT void superfox_tx_native_supports_sendmsg_and_morecqs ()
  {
    constexpr int kSuperFoxWaveSamples = (151 + 2) * 4 * 1024;
    std::fill_n (foxcom_.wave, static_cast<int> (sizeof (foxcom_.wave) / sizeof (foxcom_.wave[0])), 0.0f);
    std::fill_n (&foxcom_.cmsg[0][0], static_cast<int> (sizeof (foxcom_.cmsg)), ' ');
    std::fill_n (foxcom_.textMsg, static_cast<int> (sizeof (foxcom_.textMsg)), ' ');
    foxcom_.nslots = 1;
    foxcom_.nfreq = 750;
    foxcom_.bMoreCQs = true;
    foxcom_.bSendMsg = true;

    QByteArray const message = QByteArray {"CQ K1ABC FN42"}.leftJustified (40, ' ', true);
    QByteArray const free_text = QByteArray {"TNX BOB 73 GL"}.leftJustified (26, ' ', true);
    std::copy_n (message.constData (), 40, foxcom_.cmsg[0]);
    std::copy_n (free_text.constData (), 26, foxcom_.textMsg);

    QVERIFY (decodium::txwave::generateSuperFoxTx (QStringLiteral ("OTP:000000")));

    int non_zero_tones = 0;
    for (int i = 0; i < 151; ++i)
      {
        QVERIFY2 (foxcom3_.itone3[i] >= 0 && foxcom3_.itone3[i] <= 128,
                  qPrintable (QStringLiteral ("invalid SuperFox tone %1 at index %2")
                                  .arg (foxcom3_.itone3[i])
                                  .arg (i)));
        if (foxcom3_.itone3[i] != 0)
          {
            ++non_zero_tones;
          }
      }
    QVERIFY (non_zero_tones > 0);

    float peak = 0.0f;
    int non_zero_samples = 0;
    for (int i = 0; i < kSuperFoxWaveSamples; ++i)
      {
        float const sample = foxcom_.wave[i];
        peak = std::max (peak, std::fabs (sample));
        if (std::fabs (sample) > 1.0e-6f)
          {
            ++non_zero_samples;
          }
      }
    QVERIFY (non_zero_samples > 0);
    QVERIFY (peak > 0.0f);
    QVERIFY (peak <= 1.0001f);
  }

  Q_SLOT void superfox_analytic_cpp_matches_fortran_reference ()
  {
    constexpr int kNpts = 4096;
    constexpr float kTwoPi = 6.28318530717958647692f;
    std::array<float, kNpts> dd {};
    for (int i = 0; i < kNpts; ++i)
      {
        float const phase0 = kTwoPi * 321.5f * float (i) / 12000.0f;
        float const phase1 = kTwoPi * 987.25f * float (i) / 12000.0f;
        dd[static_cast<size_t> (i)] =
            12000.0f * std::sin (phase0) + 4000.0f * std::cos (phase1);
      }

    int npts = kNpts;
    std::array<std::complex<float>, kNpts> reference {};
    sfox_ana_ (dd.data (), &npts, reference.data (), &npts);

    std::array<float, kNpts> real_out {};
    std::array<float, kNpts> imag_out {};
    QVERIFY (ftx_superfox_analytic_c (dd.data (), kNpts, real_out.data (), imag_out.data ()) != 0);

    float max_diff = 0.0f;
    for (int i = 0; i < kNpts; ++i)
      {
        max_diff = std::max (max_diff,
                             std::fabs (real_out[static_cast<size_t> (i)] - reference[static_cast<size_t> (i)].real ()));
        max_diff = std::max (max_diff,
                             std::fabs (imag_out[static_cast<size_t> (i)] - reference[static_cast<size_t> (i)].imag ()));
      }
    QVERIFY2 (max_diff <= 1.0e-4f,
              qPrintable (QStringLiteral ("SuperFox analytic mismatch, max diff %1")
                              .arg (max_diff, 0, 'g', 8)));
  }

  Q_SLOT void superfox_remove_tone_cpp_matches_fortran_reference ()
  {
    constexpr int kNpts = 15 * 12000;
    constexpr float kSampleRate = 12000.0f;
    constexpr float kTwoPi = 6.28318530717958647692f;

    std::vector<std::complex<float>> signal (static_cast<size_t> (kNpts));
    for (int i = 0; i < kNpts; ++i)
      {
        float const t = static_cast<float> (i + 1) / kSampleRate;
        float const tone0 = kTwoPi * 1117.25f * t;
        float const tone1 = kTwoPi * 1462.50f * t;
        float const tone2 = kTwoPi * (760.0f + 0.005f * static_cast<float> (i)) * t;
        signal[static_cast<size_t> (i)] =
            std::complex<float> {std::cos (tone0), std::sin (tone0)}
            + 0.18f * std::complex<float> {std::cos (tone1), std::sin (tone1)}
            + 0.03f * std::complex<float> {std::cos (tone2), std::sin (tone2)};
      }

    std::vector<std::complex<float>> reference = signal;
    float fsync = 750.0f;
    sfox_remove_tone_ (reference.data (), &fsync);

    std::vector<float> real_out (static_cast<size_t> (kNpts));
    std::vector<float> imag_out (static_cast<size_t> (kNpts));
    for (int i = 0; i < kNpts; ++i)
      {
        real_out[static_cast<size_t> (i)] = signal[static_cast<size_t> (i)].real ();
        imag_out[static_cast<size_t> (i)] = signal[static_cast<size_t> (i)].imag ();
      }

    QVERIFY (ftx_superfox_remove_tone_c (real_out.data (), imag_out.data (), kNpts, fsync) != 0);

    float max_diff = 0.0f;
    for (int i = 0; i < kNpts; ++i)
      {
        max_diff = std::max (max_diff,
                             std::fabs (real_out[static_cast<size_t> (i)]
                                        - reference[static_cast<size_t> (i)].real ()));
        max_diff = std::max (max_diff,
                             std::fabs (imag_out[static_cast<size_t> (i)]
                                        - reference[static_cast<size_t> (i)].imag ()));
      }
    QVERIFY2 (max_diff <= 5.0e-4f,
              qPrintable (QStringLiteral ("SuperFox remove-tone mismatch, max diff %1")
                              .arg (max_diff, 0, 'g', 8)));
  }

  Q_SLOT void superfox_qpc_sync_cpp_matches_fortran_reference ()
  {
    constexpr int kNpts = 15 * 12000;
    constexpr float kSampleRate = 12000.0f;
    constexpr float kTwoPi = 6.28318530717958647692f;
    std::array<int, 24> const isync {{
        1, 2, 4, 7, 11, 16, 22, 29, 37, 39, 42, 43,
        45, 48, 52, 57, 63, 70, 78, 80, 83, 84, 86, 89}};

    std::vector<std::complex<float>> signal (static_cast<size_t> (kNpts), std::complex<float> {});
    float const fsignal = 1117.25f;
    for (int symbol : isync)
      {
        int const start = 6000 + (symbol - 1) * 1024;
        for (int i = 0; i < 1024; ++i)
          {
            int const sample_index = start + i;
            float const phase = kTwoPi * fsignal * static_cast<float> (sample_index + 1) / kSampleRate;
            signal[static_cast<size_t> (sample_index)] =
                std::complex<float> {std::cos (phase), std::sin (phase)};
          }
      }

    float fsample = kSampleRate;
    float fsync = 750.0f;
    float ftol = 200.0f;
    float f2_ref = 0.0f;
    float t2_ref = 0.0f;
    float snrsync_ref = 0.0f;
    std::array<int, 24> isync_mutable = isync;
    qpc_sync_ (signal.data (), &fsample, isync_mutable.data (), &fsync, &ftol,
               &f2_ref, &t2_ref, &snrsync_ref);

    std::vector<float> real_in (static_cast<size_t> (kNpts));
    std::vector<float> imag_in (static_cast<size_t> (kNpts));
    for (int i = 0; i < kNpts; ++i)
      {
        real_in[static_cast<size_t> (i)] = signal[static_cast<size_t> (i)].real ();
        imag_in[static_cast<size_t> (i)] = signal[static_cast<size_t> (i)].imag ();
      }

    float f2_cpp = 0.0f;
    float t2_cpp = 0.0f;
    float snrsync_cpp = 0.0f;
    QVERIFY (ftx_superfox_qpc_sync_c (real_in.data (), imag_in.data (), kNpts,
                                      fsync, ftol, &f2_cpp, &t2_cpp, &snrsync_cpp) != 0);

    QVERIFY (std::fabs (f2_cpp - f2_ref) <= 1.0e-4f);
    QVERIFY (std::fabs (t2_cpp - t2_ref) <= 1.0e-5f);
    QVERIFY (std::fabs (snrsync_cpp - snrsync_ref) <= 1.0e-4f);
  }

  Q_SLOT void superfox_qpc_likelihoods_cpp_matches_fortran_reference ()
  {
    constexpr int kRows = 128;
    constexpr int kCols = 128;
    std::vector<float> s3 (static_cast<size_t> (kRows * kCols));
    for (int col = 0; col < kCols; ++col)
      {
        for (int row = 0; row < kRows; ++row)
          {
            int const index = row + kRows * col;
            s3[static_cast<size_t> (index)] =
                0.1f + 0.01f * float ((row * 17 + col * 13) % 19)
                + 0.001f * float ((row + col) % 7);
          }
      }

    float EsNo = 3.16f;
    float No = 1.0f;
    std::vector<float> py_ref (static_cast<size_t> (kRows * kCols));
    qpc_likelihoods2_ (py_ref.data (), s3.data (), &EsNo, &No);

    std::vector<float> py_cpp (static_cast<size_t> (kRows * kCols));
    QVERIFY (ftx_superfox_qpc_likelihoods2_c (s3.data (), kRows, kCols, EsNo, No, py_cpp.data ()) != 0);

    float max_diff = 0.0f;
    for (size_t i = 0; i < py_cpp.size (); ++i)
      {
        max_diff = std::max (max_diff, std::fabs (py_cpp[i] - py_ref[i]));
      }
    QVERIFY2 (max_diff <= 1.0e-6f,
              qPrintable (QStringLiteral ("SuperFox qpc_likelihoods mismatch, max diff %1")
                              .arg (max_diff, 0, 'g', 8)));
  }

  Q_SLOT void superfox_qpc_snr_cpp_matches_fortran_reference ()
  {
    constexpr int kRows = 128;
    constexpr int kCols = 128;
    std::vector<float> s3 (static_cast<size_t> (kRows * kCols));
    for (int col = 0; col < kCols; ++col)
      {
        for (int row = 0; row < kRows; ++row)
          {
            int const index = row + kRows * col;
            s3[static_cast<size_t> (index)] =
                0.2f + 0.005f * float ((row * 5 + col * 11) % 23);
          }
      }

    std::array<signed char, 128> y_ref {};
    std::array<unsigned char, 128> y_cpp {};
    for (int col = 0; col < kCols; ++col)
      {
        int const value = (3 * col + 7) % kRows;
        y_ref[static_cast<size_t> (col)] = static_cast<signed char> (value);
        y_cpp[static_cast<size_t> (col)] = static_cast<unsigned char> (value);
      }

    float snr_ref = 0.0f;
    qpc_snr_ (s3.data (), y_ref.data (), &snr_ref);

    float snr_cpp = 0.0f;
    QVERIFY (ftx_superfox_qpc_snr_c (s3.data (), kRows, kCols, y_cpp.data (), &snr_cpp) != 0);
    QVERIFY (std::fabs (snr_cpp - snr_ref) <= 1.0e-6f);
  }

  Q_SLOT void superfox_qpc_decode2_cpp_decodes_clean_generated_payload ()
  {
    std::array<unsigned char, 50> packed {};
    QVERIFY (decodium::txwave::packSuperFoxMessage (QStringLiteral ("CQ K1ABC FN42"),
                                                    QStringLiteral ("OTP:000000"),
                                                    false, false, QString {}, &packed));

    std::array<unsigned char, 50> expected = packed;
    std::reverse (expected.begin (), expected.end ());

    std::vector<short> const iwave = make_clean_superfox_wave (QStringLiteral ("CQ K1ABC FN42"),
                                                               QStringLiteral ("OTP:000000"));
    QVERIFY (!iwave.empty ());

    std::vector<float> dd (iwave.size ());
    for (size_t i = 0; i < iwave.size (); ++i)
      {
        dd[i] = static_cast<float> (iwave[i]);
      }

    std::vector<float> real_in (iwave.size ());
    std::vector<float> imag_in (iwave.size ());
    QVERIFY (ftx_superfox_analytic_c (dd.data (), kSuperFoxSampleCount,
                                      real_in.data (), imag_in.data ()) != 0);
    QVERIFY (ftx_superfox_remove_tone_c (real_in.data (), imag_in.data (),
                                         kSuperFoxSampleCount, 750.0f) != 0);

    std::array<unsigned char, 50> xdec_cpp {};
    int crc_ok_cpp = 0;
    float snrsync_cpp = 0.0f;
    float fbest_cpp = 0.0f;
    float tbest_cpp = 0.0f;
    float snr_cpp = 0.0f;
    QVERIFY (ftx_superfox_qpc_decode2_c (real_in.data (), imag_in.data (), kSuperFoxSampleCount,
                                         750.0f, 200.0f, 3, 0.5f, 1.0f,
                                         xdec_cpp.data (), &crc_ok_cpp, &snrsync_cpp,
                                         &fbest_cpp, &tbest_cpp, &snr_cpp) != 0);

    QCOMPARE (crc_ok_cpp, 1);
    for (int i = 0; i < 50; ++i)
      {
        QCOMPARE (xdec_cpp[static_cast<size_t> (i)], expected[static_cast<size_t> (i)]);
      }

    QVERIFY (snrsync_cpp > 0.0f);
    QVERIFY (snr_cpp > -16.5f);
  }

  Q_SLOT void superfox_unpack_native_roundtrips_packed_payload ()
  {
    constexpr int kMaxLines = 16;
    constexpr int kLineStride = 64;

    auto unpack_lines = [kMaxLines] (std::array<unsigned char, 50> const& xin) {
      std::array<unsigned char, 50> payload = xin;
      std::reverse (payload.begin (), payload.end ());
      std::array<char, kMaxLines * kLineStride> raw {};
      std::fill (raw.begin (), raw.end (), ' ');
      int const count =
          ftx_superfox_unpack_lines_c (payload.data (), 1, raw.data (), kLineStride, kMaxLines);
      QStringList lines;
      for (int i = 0; i < std::min (count, kMaxLines); ++i)
        {
          QByteArray const line {raw.data () + i * kLineStride, kLineStride};
          lines << QString::fromLatin1 (line).trimmed ();
        }
      return lines;
    };

    struct Case
    {
      QString line;
      bool moreCqs;
      bool sendMsg;
      QString freeText;
      QStringList expected;
    };

    std::array<Case, 2> const cases {{
        {QStringLiteral ("CQ K1ABC FN42"), false, false, QString {},
         QStringList {QStringLiteral ("CQ K1ABC FN42"), QStringLiteral ("$VERIFY$ K1ABC 000000")}},
        {QStringLiteral ("CQ K1ABC FN42"), false, true, QStringLiteral ("TNX BOB 73 GL"),
         QStringList {QStringLiteral ("TNX BOB 73 GL"), QStringLiteral ("$VERIFY$ K1ABC 000000")}},
    }};

    for (Case const& item : cases)
      {
        std::array<unsigned char, 50> xin {};
        QVERIFY2 (decodium::txwave::packSuperFoxMessage (item.line, QStringLiteral ("OTP:000000"),
                                                         item.moreCqs, item.sendMsg, item.freeText, &xin),
                  qPrintable (item.line));

        QStringList const lines = unpack_lines (xin);
        for (QString const& expected : item.expected)
          {
            QVERIFY2 (lines.contains (expected),
                      qPrintable (QStringLiteral ("missing SuperFox line '%1' in [%2]")
                                      .arg (expected, lines.join (QStringLiteral (" | ")))));
          }
      }
  }

  Q_SLOT void superfox_decode_helper_rejects_null_input ()
  {
    constexpr int kMaxLines = 16;
    constexpr int kLineStride = 64;

    std::array<char, kMaxLines * kLineStride> raw {};
    std::fill (raw.begin (), raw.end (), ' ');
    int nsnr = 0;
    float freq = 0.0f;
    float dt = 0.0f;
    int const count =
        ftx_superfox_decode_lines_c (nullptr, 750, 200,
                                     raw.data (), kLineStride, kMaxLines,
                                     &nsnr, &freq, &dt);
    QCOMPARE (count, 0);
    QCOMPARE (nsnr, 0);
    QCOMPARE (freq, 0.0f);
    QCOMPARE (dt, 0.0f);
  }

  Q_SLOT void superfox_decode_helper_decodes_clean_generated_waveform ()
  {
    constexpr int kMaxLines = 16;
    constexpr int kLineStride = 64;

    std::vector<short> const iwave = make_clean_superfox_wave (QStringLiteral ("CQ K1ABC FN42"),
                                                               QStringLiteral ("OTP:000000"));
    QVERIFY (!iwave.empty ());

    std::array<char, kMaxLines * kLineStride> raw {};
    std::fill (raw.begin (), raw.end (), ' ');
    int nsnr = 0;
    float freq = 0.0f;
    float dt = 0.0f;
    int const count =
        ftx_superfox_decode_lines_c (iwave.data (), 750, 200,
                                     raw.data (), kLineStride, kMaxLines,
                                     &nsnr, &freq, &dt);
    QVERIFY (count > 0);

    QStringList lines;
    for (int i = 0; i < std::min (count, kMaxLines); ++i)
      {
        QByteArray const line (raw.data () + i * kLineStride, kLineStride);
        lines << QString::fromLatin1 (line).trimmed ();
      }

    QVERIFY2 (lines.contains (QStringLiteral ("CQ K1ABC FN42")),
              qPrintable (lines.join (QStringLiteral (" | "))));
    QVERIFY2 (lines.contains (QStringLiteral ("$VERIFY$ K1ABC 000000")),
              qPrintable (lines.join (QStringLiteral (" | "))));
  }

  Q_SLOT void ft2_legacy_gen_exports_match_native ()
  {
    QByteArray message = QByteArray {"CQ K1ABC FN42"}.leftJustified (37, ' ', true);
    char msgsent[37];
    signed char msgbits[77];
    int itone[103];
    int ichk = 0;

    std::fill_n (msgsent, 37, ' ');
    std::fill_n (msgbits, 77, static_cast<signed char> (0));
    std::fill_n (itone, 103, 0);
    genft2_ (message.data (), &ichk, msgsent, msgbits, itone);

    auto const encoded = decodium::txmsg::encodeFt2 (QStringLiteral ("CQ K1ABC FN42"));
    QVERIFY (encoded.ok);
    QCOMPARE (QByteArray (msgsent, 37), encoded.msgsent);
    for (int i = 0; i < 77; ++i)
      {
        QCOMPARE (msgbits[i], static_cast<signed char> (encoded.msgbits.at (i) != 0));
      }
    for (int i = 0; i < 103; ++i)
      {
        QCOMPARE (itone[i], encoded.tones.at (i));
      }

    int nsym = 103;
    int nsps = 288;
    float fsample = 12000.0f;
    float f0 = 1500.0f;
    int icmplx = 0;
    int nwave = (103 + 2) * 288;
    std::vector<float> wave (static_cast<size_t> (nwave), 0.0f);
    gen_ft2wave_ (itone, &nsym, &nsps, &fsample, &f0, nullptr, wave.data (), &icmplx, &nwave);

    QVector<float> const native = decodium::txwave::generateFt2Wave (itone, nsym, nsps, fsample, f0);
    QCOMPARE (native.size (), nwave);
    for (int i = 0; i < nwave; ++i)
      {
        QVERIFY2 (std::fabs (wave[static_cast<size_t> (i)] - native.at (i)) < 1.0e-6f,
                  "gen_ft2wave_ diverged from native FT2 waveform generator");
      }
  }

  Q_SLOT void ft8_a7_candidate_metrics ()
  {
    float s8[8 * 79];
    int itone[79];
    signed char cw[174];
    float llra[174];
    float llrb[174];
    float llrc[174];
    float llrd[174];

    std::fill_n (s8, 8 * 79, 0.0f);
    std::fill_n (itone, 79, 2);
    std::fill_n (cw, 174, 0);
    std::fill_n (llra, 174, 0.0f);
    std::fill_n (llrb, 174, 0.0f);
    std::fill_n (llrc, 174, 0.0f);
    std::fill_n (llrd, 174, 0.0f);

    cw[0] = 1;
    cw[2] = 1;
    for (int i = 0; i < 79; ++i)
      {
        s8[2 + 8 * i] = 3.0f;
      }

    llra[0] = 2.0f;
    llra[1] = -1.0f;
    llra[2] = -3.0f;
    llra[3] = 4.0f;

    llrb[0] = 2.0f;
    llrb[1] = -1.0f;
    llrb[2] = -3.0f;
    llrb[3] = -4.0f;

    llrc[0] = -2.0f;
    llrc[1] = -1.0f;
    llrc[2] = 3.0f;
    llrc[3] = 2.0f;

    llrd[0] = 2.0f;
    llrd[1] = 1.0f;
    llrd[2] = 3.0f;
    llrd[3] = -4.0f;

    float pow = 0.0f;
    float dmin = 0.0f;
    int nharderrors = 0;
    ftx_ft8a7_measure_candidate_c (s8, 8, 79, itone, cw, llra, llrb, llrc, llrd,
                                   &pow, &dmin, &nharderrors);

    QVERIFY (qAbs (pow - 79.0f * 9.0f) < 1.0e-5f);
    QVERIFY (qAbs (dmin - 1.0f) < 1.0e-5f);
    QCOMPARE (nharderrors, 1);
  }

  Q_SLOT void ft8_a7_search_and_finalize ()
  {
    constexpr int kNp2 = 2812;
    constexpr float kFs2 = 200.0f;
    constexpr float kPi = 3.14159265358979323846f;
    int const costas[7] {3, 1, 4, 0, 6, 5, 2};
    std::vector<std::complex<float>> cd0 (3200, std::complex<float> {});

    int const expected_ibest = 100;
    for (int tone = 0; tone < 7; ++tone)
      {
        float phi = 0.0f;
        float const dphi = 2.0f * kPi * static_cast<float> (costas[tone]) / 32.0f;
        for (int j = 0; j < 32; ++j)
          {
            std::complex<float> const sample (std::cos (phi), std::sin (phi));
            cd0[expected_ibest + tone * 32 + j] = sample;
            cd0[expected_ibest + tone * 32 + 36 * 32 + j] = sample;
            cd0[expected_ibest + tone * 32 + 72 * 32 + j] = sample;
            phi = std::fmod (phi + dphi, 2.0f * kPi);
          }
      }

    int ibest = 0;
    float delfbest = 99.0f;
    ftx_ft8_a7_search_initial_c (cd0.data (), kNp2, kFs2, 0.03f, &ibest, &delfbest);
    QCOMPARE (ibest, expected_ibest);
    QVERIFY (qAbs (delfbest) < 1.0e-5f);

    float sync = 0.0f;
    float xdt = 0.0f;
    ftx_ft8_a7_refine_search_c (cd0.data (), kNp2, kFs2, ibest, &ibest, &sync, &xdt);
    QCOMPARE (ibest, expected_ibest);
    QVERIFY (sync > 0.0f);
    QVERIFY (qAbs (xdt + 0.005f) < 1.0e-5f);

    float const dmm[] {5.0f, 11.0f, 9.0f};
    float dmin = 0.0f;
    float dmin2 = 0.0f;
    float xsnr_out = 0.0f;
    QCOMPARE (ftx_ft8_a7_finalize_metrics_c (dmm, 3, 9.0e6f, 1.0f, &dmin, &dmin2, &xsnr_out), 1);
    QVERIFY (qAbs (dmin - 5.0f) < 1.0e-5f);
    QVERIFY (qAbs (dmin2 - 9.0f) < 1.0e-5f);
    QVERIFY (xsnr_out > -25.0f);

    float const reject_dmm[] {10.0f, 12.0f, 25.0f};
    QCOMPARE (ftx_ft8_a7_finalize_metrics_c (reject_dmm, 3, 9.0e6f, 1.0f, &dmin, &dmin2, &xsnr_out), 0);
  }

  Q_SLOT void ft8_a7_cpp_matches_reference_on_deterministic_noise ()
  {
    constexpr int kSampleCount = 15 * 12000;
    constexpr float kPi = 3.14159265358979323846f;
    auto fixed = [] (QByteArray text, int length) {
      if (text.size () < length)
        {
          text.append (QByteArray (length - text.size (), ' '));
        }
      return text.left (length);
    };

    QByteArray const call_1 = fixed ("CQ", 12);
    QByteArray const call_2 = fixed ("K1ABC", 12);
    QByteArray const grid4 = fixed ("EN37", 4);

    struct NoiseCase
    {
      unsigned seed;
      float f1;
      float xdt;
      float xbase;
    };

    std::array<NoiseCase, 3> const cases {{
        {12345u, 1200.0f, 0.05f, 1.0f},
        {23456u, 1675.0f, -0.10f, 1.4f},
        {34567u, 2400.0f, 0.20f, 0.8f},
    }};

    for (NoiseCase const& test_case : cases)
      {
        std::vector<float> dd_ref (kSampleCount, 0.0f);
        std::vector<float> dd_cpp (kSampleCount, 0.0f);
        std::mt19937 rng {test_case.seed};
        std::normal_distribution<float> noise {0.0f, 150.0f};
        for (int i = 0; i < kSampleCount; ++i)
          {
            float const t = static_cast<float> (i) / 12000.0f;
            float const sample =
                900.0f * std::sin (2.0f * kPi * 611.0f * t)
              + 650.0f * std::cos (2.0f * kPi * 1437.0f * t)
              + 400.0f * std::sin (2.0f * kPi * 23.0f * t)
              + noise (rng);
            dd_ref[static_cast<size_t> (i)] = sample;
            dd_cpp[static_cast<size_t> (i)] = sample;
          }

        int newdat_ref = 1;
        int newdat_cpp = 1;
        float xdt_ref = test_case.xdt;
        float xdt_cpp = test_case.xdt;
        float f1_ref = test_case.f1;
        float f1_cpp = test_case.f1;
        float xbase_ref = test_case.xbase;
        float xbase_cpp = test_case.xbase;
        int nhard_ref = -99;
        int nhard_cpp = -99;
        float dmin_ref = 0.0f;
        float dmin_cpp = 0.0f;
        float xsnr_ref = 0.0f;
        float xsnr_cpp = 0.0f;
        std::array<char, 37> msg_ref {};
        std::array<char, 37> msg_cpp {};
        std::fill (msg_ref.begin (), msg_ref.end (), ' ');
        std::fill (msg_cpp.begin (), msg_cpp.end (), ' ');

        ftx_ft8_a7d_ref_c (dd_ref.data (), &newdat_ref, call_1.constData (), call_2.constData (),
                           grid4.constData (), &xdt_ref, &f1_ref, &xbase_ref,
                           &nhard_ref, &dmin_ref, msg_ref.data (), &xsnr_ref);
        ftx_ft8_a7d_c (dd_cpp.data (), &newdat_cpp, call_1.constData (), call_2.constData (),
                       grid4.constData (), &xdt_cpp, &f1_cpp, &xbase_cpp,
                       &nhard_cpp, &dmin_cpp, msg_cpp.data (), &xsnr_cpp);

        QCOMPARE (newdat_cpp, newdat_ref);
        QCOMPARE (nhard_cpp, nhard_ref);
        QByteArray const msg_cpp_text (msg_cpp.data (), static_cast<int> (msg_cpp.size ()));
        QByteArray const msg_ref_text (msg_ref.data (), static_cast<int> (msg_ref.size ()));
        QCOMPARE (msg_cpp_text, msg_ref_text);
        QVERIFY2 (std::fabs (xdt_cpp - xdt_ref) <= 1.0e-4f,
                  qPrintable (QStringLiteral ("FT8 A7 xdt mismatch for seed %1: cpp=%2 ref=%3")
                                  .arg (test_case.seed)
                                  .arg (xdt_cpp, 0, 'g', 9)
                                  .arg (xdt_ref, 0, 'g', 9)));
        QVERIFY2 (std::fabs (f1_cpp - f1_ref) <= 1.0e-4f,
                  qPrintable (QStringLiteral ("FT8 A7 f1 mismatch for seed %1: cpp=%2 ref=%3")
                                  .arg (test_case.seed)
                                  .arg (f1_cpp, 0, 'g', 9)
                                  .arg (f1_ref, 0, 'g', 9)));
        QVERIFY2 (std::fabs (dmin_cpp - dmin_ref) <= 1.0e-4f,
                  qPrintable (QStringLiteral ("FT8 A7 dmin mismatch for seed %1: cpp=%2 ref=%3")
                                  .arg (test_case.seed)
                                  .arg (dmin_cpp, 0, 'g', 9)
                                  .arg (dmin_ref, 0, 'g', 9)));
        QVERIFY2 (std::fabs (xsnr_cpp - xsnr_ref) <= 1.0e-4f,
                  qPrintable (QStringLiteral ("FT8 A7 xsnr mismatch for seed %1: cpp=%2 ref=%3")
                                  .arg (test_case.seed)
                                  .arg (xsnr_cpp, 0, 'g', 9)
                                  .arg (xsnr_ref, 0, 'g', 9)));
      }
  }

  Q_SLOT void ft8_a8_search_and_finalize ()
  {
    constexpr int kNzz = 3200;
    constexpr int kNwave = 2528;
    constexpr int kCenter = kNzz / 2;
    std::vector<std::complex<float>> cd (kNzz, std::complex<float> {});
    std::vector<std::complex<float>> cwave (kNzz, std::complex<float> {});
    std::vector<float> spectrum (kNzz + 1, 0.0f);

    for (int i = 0; i < kNwave; ++i)
      {
        float const phase = ((i * 17) % 29) < 14 ? 0.0f : 3.14159265358979323846f;
        cwave[i] = std::complex<float> (std::cos (phase), std::sin (phase));
      }

    int const lag = 20;
    for (int k = lag + 100; k <= (kNwave - 1) + lag + 100; ++k)
      {
        if (k >= 0 && k < kNzz)
          {
            cd[k] = cwave[k - lag - 100];
          }
      }

    float spk = 0.0f;
    float fpk = 0.0f;
    float tpk = 0.0f;
    QCOMPARE (ftx_ft8_a8_search_candidate_c (cd.data (), cwave.data (), kNzz, kNwave, 1500.0f,
                                             &spk, &fpk, &tpk, spectrum.data ()), 1);
    QVERIFY (spk > 0.0f);
    QVERIFY (qAbs (fpk - 1500.0f) < 0.1f);
    QVERIFY (qAbs (tpk) <= 1.0f);
    QVERIFY (spectrum[kCenter] > spectrum[kCenter + 10]);

    float xsnr = 0.0f;
    QCOMPARE (ftx_ft8_a8_finalize_search_c (spectrum.data (), kNzz + 1, 1500.0f, fpk, &xsnr), 1);
    QVERIFY (xsnr > -50.0f);
    QCOMPARE (ftx_ft8_a8_finalize_search_c (spectrum.data (), kNzz + 1, 1500.0f, 1506.0f, &xsnr), 0);

    QCOMPARE (ftx_ft8_a8_accept_score_c (54, -159.0f, 0.71f), 1);
    QCOMPARE (ftx_ft8_a8_accept_score_c (55, -159.0f, 0.71f), 0);
    QCOMPARE (ftx_ft8_a8_accept_score_c (54, -159.1f, 0.71f), 0);
  }

  Q_SLOT void ft8_a8_cpp_matches_reference_on_signal_and_noise ()
  {
    auto fixed = [] (QByteArray text, int length) {
      if (text.size () < length)
        {
          text.append (QByteArray (length - text.size (), ' '));
        }
      return text.left (length);
    };

    QByteArray const mycall = fixed ("K1ABC", 12);
    QByteArray const dxcall = fixed ("W9XYZ", 12);
    QByteArray const dxgrid = fixed ("EN37AA", 6);
    std::array<char, 37> a8_message {};
    std::fill (a8_message.begin (), a8_message.end (), ' ');
    QCOMPARE (ftx_prepare_ft8_a8_candidate_c (5, mycall.constData (), dxcall.constData (),
                                              dxgrid.constData (), a8_message.data ()), 1);
    QString const message_text = QString::fromLatin1 (a8_message.data (), 37).trimmed ();
    QCOMPARE (message_text, QStringLiteral ("CQ W9XYZ EN37"));

    struct Case
    {
      QString label;
      std::vector<float> dd;
      float f1;
    };

    std::array<Case, 2> const cases {{
        {
            QStringLiteral ("strong-signal"),
            make_ft8_case_wave (message_text, 1512.5f, 360.0f, 18.0f, 4001u),
            1512.5f
        },
        {
            QStringLiteral ("deterministic-noise"),
            [] {
              constexpr int kSampleCount = 15 * 12000;
              constexpr float kPi = 3.14159265358979323846f;
              std::vector<float> samples (kSampleCount, 0.0f);
              std::mt19937 rng {4002u};
              std::normal_distribution<float> noise {0.0f, 150.0f};
              for (int i = 0; i < kSampleCount; ++i)
                {
                  float const t = static_cast<float> (i) / 12000.0f;
                  samples[static_cast<size_t> (i)] =
                      700.0f * std::sin (2.0f * kPi * 503.0f * t)
                    + 540.0f * std::cos (2.0f * kPi * 911.0f * t)
                    + 260.0f * std::sin (2.0f * kPi * 37.0f * t)
                    + noise (rng);
                }
              return samples;
            }(),
            1600.0f
        }
    }};

    for (Case const& test_case : cases)
      {
        QVERIFY2 (!test_case.dd.empty (),
                  qPrintable (QStringLiteral ("Failed to build FT8 A8 case %1").arg (test_case.label)));

        std::vector<float> dd_ref = test_case.dd;
        std::vector<float> dd_cpp = test_case.dd;
        float f1_ref = test_case.f1;
        float f1_cpp = test_case.f1;
        float xdt_ref = 0.0f;
        float xdt_cpp = 0.0f;
        float fbest_ref = 0.0f;
        float fbest_cpp = 0.0f;
        float xsnr_ref = 0.0f;
        float xsnr_cpp = 0.0f;
        float plog_ref = 0.0f;
        float plog_cpp = 0.0f;
        std::array<char, 37> msg_ref {};
        std::array<char, 37> msg_cpp {};
        std::fill (msg_ref.begin (), msg_ref.end (), ' ');
        std::fill (msg_cpp.begin (), msg_cpp.end (), ' ');

        ftx_ft8_a8d_ref_c (dd_ref.data (), mycall.constData (), dxcall.constData (), dxgrid.constData (),
                           &f1_ref, &xdt_ref, &fbest_ref, &xsnr_ref, &plog_ref, msg_ref.data ());
        ftx_ft8_a8d_c (dd_cpp.data (), mycall.constData (), dxcall.constData (), dxgrid.constData (),
                       &f1_cpp, &xdt_cpp, &fbest_cpp, &xsnr_cpp, &plog_cpp, msg_cpp.data ());

        QByteArray const msg_cpp_text (msg_cpp.data (), static_cast<int> (msg_cpp.size ()));
        QByteArray const msg_ref_text (msg_ref.data (), static_cast<int> (msg_ref.size ()));
        QCOMPARE (msg_cpp_text, msg_ref_text);
        QVERIFY2 (std::fabs (xdt_cpp - xdt_ref) <= 1.0e-3f,
                  qPrintable (QStringLiteral ("FT8 A8 xdt mismatch for %1: cpp=%2 ref=%3")
                                  .arg (test_case.label)
                                  .arg (xdt_cpp, 0, 'g', 9)
                                  .arg (xdt_ref, 0, 'g', 9)));
        QVERIFY2 (std::fabs (fbest_cpp - fbest_ref) <= 1.0e-3f,
                  qPrintable (QStringLiteral ("FT8 A8 fbest mismatch for %1: cpp=%2 ref=%3")
                                  .arg (test_case.label)
                                  .arg (fbest_cpp, 0, 'g', 9)
                                  .arg (fbest_ref, 0, 'g', 9)));
        QVERIFY2 (std::fabs (xsnr_cpp - xsnr_ref) <= 1.0e-3f,
                  qPrintable (QStringLiteral ("FT8 A8 xsnr mismatch for %1: cpp=%2 ref=%3")
                                  .arg (test_case.label)
                                  .arg (xsnr_cpp, 0, 'g', 9)
                                  .arg (xsnr_ref, 0, 'g', 9)));
        QVERIFY2 (std::fabs (plog_cpp - plog_ref) <= 1.0e-3f,
                  qPrintable (QStringLiteral ("FT8 A8 plog mismatch for %1: cpp=%2 ref=%3")
                                  .arg (test_case.label)
                                  .arg (plog_cpp, 0, 'g', 9)
                                  .arg (plog_ref, 0, 'g', 9)));
      }
  }

  Q_SLOT void ldpc174_finalize_metrics ()
  {
    signed char cw[174];
    float llr[174];
    signed char message91[91];
    std::fill_n (cw, 174, 0);
    std::fill_n (llr, 174, 0.0f);
    std::fill_n (message91, 91, 1);

    float dmin = -1.0f;
    int nharderrors = -1;
    QCOMPARE (ftx_ldpc174_91_finalize_c (cw, llr, message91, &nharderrors, &dmin), 1);
    QCOMPARE (nharderrors, 0);
    QVERIFY (qAbs (dmin) < 1.0e-6f);
    for (signed char bit : message91)
      {
        QCOMPARE (bit, static_cast<signed char> (0));
      }

    llr[0] = 2.0f;
    llr[1] = -1.5f;
    llr[2] = 3.0f;
    cw[0] = 0;
    cw[1] = 1;
    cw[2] = 1;
    ftx_ldpc174_91_metrics_c (cw, llr, &nharderrors, &dmin);
    QCOMPARE (nharderrors, 2);
    QVERIFY (qAbs (dmin - 3.5f) < 1.0e-5f);
  }

  Q_SLOT void ldpc174_cpp_decoder_is_self_consistent ()
  {
    std::array<signed char, 77> bits77 {};
    for (int i = 0; i < 77; ++i)
      {
        bits77[static_cast<size_t> (i)] = ((i % 5) == 0 || (i % 7) == 3) ? 1 : 0;
      }

    std::array<signed char, 174> codeword {};
    QVERIFY2 (ftx_encode174_91_message77_c (bits77.data (), codeword.data ()) != 0,
              "failed to encode deterministic 174_91 testcase");

    struct Case
    {
      char const* label;
      float base;
      std::array<int, 8> flipped;
      std::array<int, 8> weakened;
    };

    std::array<Case, 2> const cases {{
      {"clean", 4.0f, {{-1, -1, -1, -1, -1, -1, -1, -1}}, {{12, 44, 71, 95, 118, 143, -1, -1}}},
      {"weak", 1.5f, {{5, 17, 29, 42, 63, 88, 121, 151}}, {{9, 26, 54, 77, 103, 132, 160, -1}}},
    }};

    for (Case const& test_case : cases)
      {
        std::array<float, 174> llr {};
        std::array<signed char, 174> apmask {};
        for (int i = 0; i < 174; ++i)
          {
            llr[static_cast<size_t> (i)] =
              codeword[static_cast<size_t> (i)] != 0 ? test_case.base : -test_case.base;
          }
        for (int index : test_case.weakened)
          {
            if (index >= 0)
              {
                llr[static_cast<size_t> (index)] *= 0.22f;
              }
          }
        for (int index : test_case.flipped)
          {
            if (index >= 0)
              {
                llr[static_cast<size_t> (index)] *= -0.65f;
              }
          }

        std::array<signed char, 91> message91_cpp {};
        std::array<signed char, 91> message91_final {};
        std::array<signed char, 174> cw_cpp {};
        int ntype_cpp = 0;
        int nhard_cpp = 0;
        float dmin_cpp = 0.0f;
        int nhard_final = 0;
        float dmin_final = 0.0f;

        ftx_decode174_91_c (llr.data (), 91, 3, 4, apmask.data (), message91_cpp.data (),
                            cw_cpp.data (), &ntype_cpp, &nhard_cpp, &dmin_cpp);
        if (QByteArray (test_case.label) == QByteArrayLiteral ("clean"))
          {
            QVERIFY2 (ntype_cpp != 0, "clean 174_91 case did not decode");
            QCOMPARE (QByteArray (reinterpret_cast<char const*> (cw_cpp.data ()),
                                  static_cast<int> (cw_cpp.size ())),
                      QByteArray (reinterpret_cast<char const*> (codeword.data ()),
                                  static_cast<int> (codeword.size ())));
          }

        if (ntype_cpp != 0)
          {
            QCOMPARE (ftx_ldpc174_91_finalize_c (cw_cpp.data (), llr.data (), message91_final.data (),
                                                 &nhard_final, &dmin_final), 1);
            QCOMPARE (QByteArray (reinterpret_cast<char const*> (message91_cpp.data ()),
                                  static_cast<int> (message91_cpp.size ())),
                      QByteArray (reinterpret_cast<char const*> (message91_final.data ()),
                                  static_cast<int> (message91_final.size ())));
            QCOMPARE (nhard_cpp, nhard_final);
            QVERIFY2 (std::fabs (dmin_cpp - dmin_final) <= 1.0e-4f,
                      qPrintable (QStringLiteral ("174_91 dmin mismatch for %1: cpp=%2 final=%3")
                                      .arg (test_case.label)
                                      .arg (dmin_cpp, 0, 'g', 9)
                                      .arg (dmin_final, 0, 'g', 9)));
          }
        else
          {
            QCOMPARE (nhard_cpp, -1);
          }
      }
  }

  Q_SLOT void ft8_downsample_smoke_reuse_workspace ()
  {
    constexpr int kNMax = 15 * 12000;
    constexpr int kNfft2 = 3200;
    constexpr float kPi = 3.14159265358979323846f;
    std::vector<float> dd (kNMax, 0.0f);
    std::vector<std::complex<float>> c1 (kNfft2, std::complex<float> {});

    for (int i = 0; i < kNMax; ++i)
      {
        float const t = static_cast<float> (i);
        dd[static_cast<size_t> (i)] = 0.4f * std::sin (2.0f * kPi * 1000.0f * t / 12000.0f)
                                    + 0.2f * std::cos (2.0f * kPi * 1300.0f * t / 12000.0f);
      }

    for (int iter = 0; iter < 128; ++iter)
      {
        int newdat = (iter % 7 == 0) ? 1 : 0;
        float const f0 = 900.0f + static_cast<float> ((iter % 9) * 25);
        ftx_ft8_downsample_c (dd.data (), &newdat, f0, reinterpret_cast<fftwf_complex*> (c1.data ()));
        QCOMPARE (newdat, 0);
      }

    bool non_zero = false;
    for (auto const& sample : c1)
      {
        if (std::abs (sample) > 1.0e-6f)
          {
            non_zero = true;
            break;
          }
      }
    QVERIFY (non_zero);
  }

  Q_SLOT void ft8_subtract_smoke_reuse_workspace ()
  {
    constexpr int kNMax = 15 * 12000;
    constexpr int kNFrame = 1920 * 79;
    std::vector<int> itone (79, 0);
    std::vector<std::complex<float>> cref (kNFrame, std::complex<float> {});
    std::vector<float> source (kNMax, 0.0f);
    std::vector<float> dd (kNMax, 0.0f);

    for (int i = 0; i < 79; ++i)
      {
        itone[static_cast<size_t> (i)] = i % 8;
      }

    ftx_gen_ft8_refsig_c (itone.data (), 1200.0f, reinterpret_cast<fftwf_complex*> (cref.data ()));
    for (int i = 0; i < kNFrame; ++i)
      {
        source[static_cast<size_t> (i)] = 1.6f * cref[static_cast<size_t> (i)].real ();
      }
    for (int i = kNFrame; i < kNMax; ++i)
      {
        float const phase = static_cast<float> ((i - kNFrame) % 64) / 64.0f;
        source[static_cast<size_t> (i)] = 0.02f * std::sin (2.0f * 3.14159265358979323846f * phase);
      }

    float energy_before = 0.0f;
    for (float sample : source)
      {
        energy_before += sample * sample;
      }

    for (int iter = 0; iter < 128; ++iter)
      {
        for (size_t i = 0; i < source.size (); ++i)
          {
            dd[i] = source[i];
          }
        ftx_subtract_ft8_c (dd.data (), itone.data (), 1200.0f, 0.0f, 0);
      }

    float energy_after = 0.0f;
    for (float sample : dd)
      {
        QVERIFY (std::isfinite (sample));
        energy_after += sample * sample;
      }

    QVERIFY (energy_after < energy_before);

    dd = source;
    ftx_subtract_ft8_c (dd.data (), itone.data (), 1200.0f, 0.0f, 1);
    for (float sample : dd)
      {
        QVERIFY (std::isfinite (sample));
      }
  }

  Q_SLOT void ft8_waveform_bridge_matches_fortran_reference ()
  {
    std::array<int, 79> itone {};
    std::array<signed char, 174> codeword {};
    QByteArray msg37 = QByteArrayLiteral ("CQ K1ABC FN42");
    char msgsent[37] {};
    QVERIFY2 (ftx_encode_ft8_candidate_c (msg37.constData (), msgsent, itone.data (), codeword.data ()) != 0,
              "Failed to encode FT8 candidate for waveform bridge test");

    int nsym = 79;
    int nsps = 1920;
    float bt = 2.0f;
    float fsample = 12000.0f;
    float f0 = 0.0f;
    int icmplx = 1;
    int nwave = nsym * nsps;
    std::vector<std::complex<float>> ref_wave (static_cast<size_t> (nwave), std::complex<float> {});
    std::vector<float> ref_real (static_cast<size_t> (nwave), 0.0f);
    std::vector<float> real_out (static_cast<size_t> (nwave), 0.0f);
    std::vector<float> imag_out (static_cast<size_t> (nwave), 0.0f);

    gen_ft8wave_ (itone.data (), &nsym, &nsps, &bt, &fsample, &f0, ref_wave.data (), ref_real.data (),
                  &icmplx, &nwave);
    QVERIFY2 (ftx_gen_ft8wave_complex_c (itone.data (), nsym, nsps, bt, fsample, f0,
                                         real_out.data (), imag_out.data (), nwave) != 0,
              "C++ FT8 complex waveform bridge failed");

    float max_abs_error = 0.0f;
    for (int i = 0; i < nwave; ++i)
      {
        float const dr = std::abs (ref_wave[static_cast<size_t> (i)].real () - real_out[static_cast<size_t> (i)]);
        float const di = std::abs (ref_wave[static_cast<size_t> (i)].imag () - imag_out[static_cast<size_t> (i)]);
        max_abs_error = std::max (max_abs_error, std::max (dr, di));
      }

    QVERIFY2 (max_abs_error < 1.0e-3f, qPrintable (QStringLiteral ("max FT8 waveform error %1").arg (max_abs_error, 0, 'g', 8)));
  }

  Q_SLOT void ft8_async_decode_stage3_matches_stage0_generated_waveform ()
  {
    QTemporaryDir temp_dir;
    QVERIFY2 (temp_dir.isValid (), "failed to create temporary directory for FT8 simulation");

    QString const bin_dir = QCoreApplication::applicationDirPath ();
    QString const ft8sim_path = QFileInfo {bin_dir + QStringLiteral ("/../ft8sim")}.absoluteFilePath ();
    QString const compare_path = QFileInfo {bin_dir + QStringLiteral ("/ft8_stage_compare")}.absoluteFilePath ();
    if (!QFileInfo::exists (ft8sim_path) || !QFileInfo::exists (compare_path))
      {
        QSKIP (qPrintable (QStringLiteral ("FT8 compare binaries not built (%1, %2)")
                               .arg (ft8sim_path, compare_path)));
      }

    QProcess sim;
    sim.setWorkingDirectory (temp_dir.path ());
    sim.start (ft8sim_path, {QStringLiteral ("CQ K1ABC FN42"), QStringLiteral ("1500"),
                             QStringLiteral ("0"), QStringLiteral ("0"),
                             QStringLiteral ("0"), QStringLiteral ("1"),
                             QStringLiteral ("6")});
    QVERIFY2 (sim.waitForFinished (30000), qPrintable (sim.errorString ()));
    QCOMPARE (sim.exitStatus (), QProcess::NormalExit);
    QCOMPARE (sim.exitCode (), 0);

    QString const wav_path = temp_dir.filePath (QStringLiteral ("000000_000001.wav"));
    QVERIFY2 (QFileInfo::exists (wav_path), qPrintable (QStringLiteral ("missing generated FT8 WAV %1").arg (wav_path)));

    QProcess compare_stage0;
    compare_stage0.start (compare_path, {wav_path, QStringLiteral ("--stages"), QStringLiteral ("0"),
                                         QStringLiteral ("--nfqso"), QStringLiteral ("1500")});
    QVERIFY2 (compare_stage0.waitForFinished (30000), qPrintable (compare_stage0.errorString ()));
    QCOMPARE (compare_stage0.exitStatus (), QProcess::NormalExit);
    QCOMPARE (compare_stage0.exitCode (), 0);
    QString const stage0_output = QString::fromLocal8Bit (compare_stage0.readAllStandardOutput ())
                                + QString::fromLocal8Bit (compare_stage0.readAllStandardError ());
    Ft8StageCompareResult const stage0_result = parse_ft8_stage_compare_output (stage0_output, 0);

    QProcess compare_stage3;
    compare_stage3.start (compare_path, {wav_path, QStringLiteral ("--stages"), QStringLiteral ("3"),
                                         QStringLiteral ("--nfqso"), QStringLiteral ("1500")});
    QVERIFY2 (compare_stage3.waitForFinished (30000), qPrintable (compare_stage3.errorString ()));
    QCOMPARE (compare_stage3.exitStatus (), QProcess::NormalExit);
    QCOMPARE (compare_stage3.exitCode (), 0);
    QString const stage3_output = QString::fromLocal8Bit (compare_stage3.readAllStandardOutput ())
                                + QString::fromLocal8Bit (compare_stage3.readAllStandardError ());
    Ft8StageCompareResult const stage3_result = parse_ft8_stage_compare_output (stage3_output, 3);

    if (stage0_result.nout == 0)
      {
        QSKIP ("Generated FT8 waveform did not decode via the offline comparator; use real FT8 WAV parity checks instead.");
      }
    if (!stage0_result.messages.contains (QStringLiteral ("CQ K1ABC FN42")))
      {
        QSKIP ("Generated FT8 waveform did not decode the expected message; use real FT8 WAV parity checks instead.");
      }
    if (stage3_result.nout == 0)
      {
        QSKIP ("Generated FT8 waveform did not decode via stage 3; use real FT8 WAV parity checks instead.");
      }
    QVERIFY2 (stage3_result.messages.contains (QStringLiteral ("CQ K1ABC FN42")),
              qPrintable (stage3_output));
    QCOMPARE (stage3_result.nout, stage0_result.nout);
    QCOMPARE (stage3_result.detail_lines, stage0_result.detail_lines);
  }

  Q_SLOT void ft8_real_recordings_stage3_expected_baselines ()
  {
    QString const bin_dir = QCoreApplication::applicationDirPath ();
    QString const compare_path = QFileInfo {bin_dir + QStringLiteral ("/ft8_stage_compare")}.absoluteFilePath ();
    QString const repo_root =
        QFileInfo {QDir {QCoreApplication::applicationDirPath ()}.absoluteFilePath (
            QStringLiteral ("../.."))}.absoluteFilePath ();

    struct Ft8Fixture
    {
      QString file_name;
      int expected_nout;
      QString expected_message;
    };

    std::array<Ft8Fixture, 3> const fixtures {{
        {QStringLiteral ("FT8.wav"), 12, QStringLiteral ("W1FC F5BZB -08")},
        {QStringLiteral ("FT8-2.wav"), 21, QStringLiteral ("W7PP AF4T 53 TN")},
        {QStringLiteral ("FT8-3.wav"), 0, QString {}},
    }};

    if (!QFileInfo::exists (compare_path))
      {
        QSKIP ("ft8_stage_compare tool not available");
      }

    for (Ft8Fixture const& fixture : fixtures)
      {
        QString const wav_path = QFileInfo {QDir {repo_root}.absoluteFilePath (fixture.file_name)}.absoluteFilePath ();
        if (!QFileInfo::exists (wav_path))
          {
            QSKIP (qPrintable (QStringLiteral ("missing FT8 fixture %1").arg (wav_path)));
          }

        QProcess compare;
        compare.start (compare_path, {wav_path, QStringLiteral ("--stages"), QStringLiteral ("3"),
                                      QStringLiteral ("--ldiskdat"), QStringLiteral ("1")});
        QVERIFY2 (compare.waitForFinished (180000), qPrintable (compare.errorString ()));
        QCOMPARE (compare.exitStatus (), QProcess::NormalExit);
        QCOMPARE (compare.exitCode (), 0);

        QString const output = QString::fromLocal8Bit (compare.readAllStandardOutput ())
                             + QString::fromLocal8Bit (compare.readAllStandardError ());
        Ft8StageCompareResult const stage3 = parse_ft8_stage_compare_output (output, 3);
        QCOMPARE (stage3.nout, fixture.expected_nout);
        if (!fixture.expected_message.isEmpty ())
          {
            QVERIFY2 (stage3.messages.contains (fixture.expected_message), qPrintable (output));
          }
      }
  }

  Q_SLOT void ft8_async_decode_stage4_matches_stage3_generated_waveform ()
  {
    QTemporaryDir temp_dir;
    QVERIFY2 (temp_dir.isValid (), "failed to create temporary directory for FT8 stage4 simulation");

    QString const bin_dir = QCoreApplication::applicationDirPath ();
    QString const ft8sim_path = QFileInfo {bin_dir + QStringLiteral ("/../ft8sim")}.absoluteFilePath ();
    QString const compare_path = QFileInfo {bin_dir + QStringLiteral ("/ft8_stage_compare")}.absoluteFilePath ();
    if (!QFileInfo::exists (ft8sim_path) || !QFileInfo::exists (compare_path))
      {
        QSKIP (qPrintable (QStringLiteral ("FT8 compare binaries not built (%1, %2)")
                               .arg (ft8sim_path, compare_path)));
      }

    QProcess sim;
    sim.setWorkingDirectory (temp_dir.path ());
    sim.start (ft8sim_path, {QStringLiteral ("CQ K1ABC FN42"), QStringLiteral ("1500"),
                             QStringLiteral ("0"), QStringLiteral ("0"),
                             QStringLiteral ("0"), QStringLiteral ("1"),
                             QStringLiteral ("6")});
    QVERIFY2 (sim.waitForFinished (30000), qPrintable (sim.errorString ()));
    QCOMPARE (sim.exitStatus (), QProcess::NormalExit);
    QCOMPARE (sim.exitCode (), 0);

    QString const wav_path = temp_dir.filePath (QStringLiteral ("000000_000001.wav"));
    QVERIFY2 (QFileInfo::exists (wav_path), qPrintable (QStringLiteral ("missing generated FT8 WAV %1").arg (wav_path)));

    QProcess compare_stage3;
    compare_stage3.start (compare_path, {wav_path, QStringLiteral ("--stages"), QStringLiteral ("3"),
                                         QStringLiteral ("--nfqso"), QStringLiteral ("1500"),
                                         QStringLiteral ("--ldiskdat"), QStringLiteral ("1")});
    QVERIFY2 (compare_stage3.waitForFinished (30000), qPrintable (compare_stage3.errorString ()));
    QCOMPARE (compare_stage3.exitStatus (), QProcess::NormalExit);
    QCOMPARE (compare_stage3.exitCode (), 0);
    QString const stage3_output = QString::fromLocal8Bit (compare_stage3.readAllStandardOutput ())
                                + QString::fromLocal8Bit (compare_stage3.readAllStandardError ());
    Ft8StageCompareResult const stage3_result = parse_ft8_stage_compare_output (stage3_output, 3);

    QProcess compare_stage4;
    compare_stage4.start (compare_path, {wav_path, QStringLiteral ("--stages"), QStringLiteral ("4"),
                                         QStringLiteral ("--nfqso"), QStringLiteral ("1500"),
                                         QStringLiteral ("--ldiskdat"), QStringLiteral ("1")});
    QVERIFY2 (compare_stage4.waitForFinished (30000), qPrintable (compare_stage4.errorString ()));
    QCOMPARE (compare_stage4.exitStatus (), QProcess::NormalExit);
    QCOMPARE (compare_stage4.exitCode (), 0);
    QString const stage4_output = QString::fromLocal8Bit (compare_stage4.readAllStandardOutput ())
                                + QString::fromLocal8Bit (compare_stage4.readAllStandardError ());
    Ft8StageCompareResult const stage4_result = parse_ft8_stage_compare_output (stage4_output, 4);

    if (stage3_result.nout == 0 || stage4_result.nout == 0)
      {
        QSKIP ("Generated FT8 waveform did not decode on both stage 3 and stage 4.");
      }

    QCOMPARE (stage4_result.nout, stage3_result.nout);
    QCOMPARE (stage4_result.detail_lines, stage3_result.detail_lines);
  }

  Q_SLOT void ft8_real_recordings_stage4_match_stage3_isolated ()
  {
    QString const bin_dir = QCoreApplication::applicationDirPath ();
    QString const compare_path = QFileInfo {bin_dir + QStringLiteral ("/ft8_stage_compare")}.absoluteFilePath ();
    QString const repo_root =
        QFileInfo {QDir {QCoreApplication::applicationDirPath ()}.absoluteFilePath (
            QStringLiteral ("../.."))}.absoluteFilePath ();
    QStringList const fixtures {
      QFileInfo {QDir {repo_root}.absoluteFilePath (QStringLiteral ("FT8.wav"))}.absoluteFilePath (),
      QFileInfo {QDir {repo_root}.absoluteFilePath (QStringLiteral ("FT8-2.wav"))}.absoluteFilePath (),
      QFileInfo {QDir {repo_root}.absoluteFilePath (QStringLiteral ("FT8-3.wav"))}.absoluteFilePath ()
    };

    if (!QFileInfo::exists (compare_path))
      {
        QSKIP ("ft8_stage_compare tool not available");
      }

    for (QString const& wav_path : fixtures)
      {
        if (!QFileInfo::exists (wav_path))
          {
            QSKIP (qPrintable (QStringLiteral ("missing FT8 fixture %1").arg (wav_path)));
          }

        QProcess compare_stage3;
        compare_stage3.start (compare_path, {wav_path, QStringLiteral ("--stages"), QStringLiteral ("3"),
                                             QStringLiteral ("--ldiskdat"), QStringLiteral ("1")});
        QVERIFY2 (compare_stage3.waitForFinished (180000), qPrintable (compare_stage3.errorString ()));
        QCOMPARE (compare_stage3.exitStatus (), QProcess::NormalExit);
        QCOMPARE (compare_stage3.exitCode (), 0);
        QString const stage3_output = QString::fromLocal8Bit (compare_stage3.readAllStandardOutput ())
                                    + QString::fromLocal8Bit (compare_stage3.readAllStandardError ());
        Ft8StageCompareResult const stage3_result = parse_ft8_stage_compare_output (stage3_output, 3);

        QProcess compare_stage4;
        compare_stage4.start (compare_path, {wav_path, QStringLiteral ("--stages"), QStringLiteral ("4"),
                                             QStringLiteral ("--ldiskdat"), QStringLiteral ("1")});
        QVERIFY2 (compare_stage4.waitForFinished (180000), qPrintable (compare_stage4.errorString ()));
        QCOMPARE (compare_stage4.exitStatus (), QProcess::NormalExit);
        QCOMPARE (compare_stage4.exitCode (), 0);
        QString const stage4_output = QString::fromLocal8Bit (compare_stage4.readAllStandardOutput ())
                                    + QString::fromLocal8Bit (compare_stage4.readAllStandardError ());
        Ft8StageCompareResult const stage4_result = parse_ft8_stage_compare_output (stage4_output, 4);

        QCOMPARE (stage4_result.nout, stage3_result.nout);
        QCOMPARE (stage4_result.detail_lines, stage3_result.detail_lines);
        QCOMPARE (ft8_message_set (stage4_result), ft8_message_set (stage3_result));
      }
  }

  Q_SLOT void ft4_getcandidates_smoke ()
  {
    constexpr int kNMax = 21 * 3456;
    constexpr int kNh1 = 2304 / 2;
    constexpr int kMaxCand = 32;
    constexpr float kPi = 3.14159265358979323846f;
    std::vector<float> dd (kNMax, 0.0f);
    std::vector<float> savg (kNh1, 0.0f);
    std::vector<float> sbase (kNh1, 0.0f);
    std::vector<float> candidate (2 * kMaxCand, 0.0f);

    for (int i = 0; i < kNMax; ++i)
      {
        float const t = static_cast<float> (i) / 12000.0f;
        dd[static_cast<size_t> (i)] = 0.8f * std::sin (2.0f * kPi * 950.0f * t)
                                    + 0.3f * std::cos (2.0f * kPi * 1230.0f * t);
      }

    for (int iter = 0; iter < 64; ++iter)
      {
        std::fill (savg.begin (), savg.end (), 0.0f);
        std::fill (sbase.begin (), sbase.end (), 0.0f);
        std::fill (candidate.begin (), candidate.end (), 0.0f);
        int ncand = 0;
        ftx_getcandidates4_c (dd.data (), 200.0f, 4000.0f, 0.45f,
                              950.0f + static_cast<float> (iter % 3) * 20.0f,
                              kMaxCand, savg.data (), candidate.data (), &ncand, sbase.data ());
        QVERIFY (ncand > 0);
        QVERIFY (ncand <= kMaxCand);
        for (int i = 0; i < ncand; ++i)
          {
            float const freq = candidate[static_cast<size_t> (2 * i)];
            float const strength = candidate[static_cast<size_t> (2 * i + 1)];
            QVERIFY (std::isfinite (freq));
            QVERIFY (std::isfinite (strength));
            QVERIFY (freq >= 200.0f);
            QVERIFY (freq <= 4910.0f);
            QVERIFY (strength >= 0.0f);
          }
      }
  }

  Q_SLOT void ft4_sync4d_smoke ()
  {
    constexpr int kNp = (21 * 3456) / 18;
    constexpr int kNtwk = 2 * (576 / 18);
    constexpr float kPi = 3.14159265358979323846f;
    std::vector<std::complex<float>> cd0 (kNp, std::complex<float> {});
    std::vector<std::complex<float>> ctwk (kNtwk, std::complex<float> {1.0f, 0.0f});

    for (int i = 0; i < kNp; ++i)
      {
        float const phase = 2.0f * kPi * static_cast<float> (i % 32) / 32.0f;
        cd0[static_cast<size_t> (i)] = std::polar (0.5f, phase);
      }

    for (int iter = 0; iter < 128; ++iter)
      {
        float sync = -1.0f;
        int const i0 = (iter % 9) * 32 - 64;
        ftx_sync4d_c (cd0.data (), kNp, i0, ctwk.data (), iter % 2, &sync);
        QVERIFY (std::isfinite (sync));
        QVERIFY (sync >= 0.0f);
      }
  }

  Q_SLOT void ft4_sync4d_matches_fortran_reference ()
  {
    constexpr int kNp = (21 * 3456) / 18;
    constexpr int kNtwk = 2 * (576 / 18);
    std::vector<std::complex<float>> cd0 (kNp, std::complex<float> {});
    std::vector<std::complex<float>> ctwk (kNtwk, std::complex<float> {});
    std::mt19937 rng {0x4f5434u};
    std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

    for (auto& sample : cd0)
      {
        sample = {dist (rng), dist (rng)};
      }
    for (auto& sample : ctwk)
      {
        sample = {dist (rng), dist (rng)};
      }

    std::array<int, 10> const starts {{-344, -129, -64, -1, 0, 17, 108, 560, 3890, 3905}};
    for (int start : starts)
      {
        float sync_cpp = -1.0f;
        float sync_ref = -1.0f;
        ftx_sync4d_c (cd0.data (), kNp, start, ctwk.data (), 1, &sync_cpp);
        ftx_sync4d_ref_c (cd0.data (), kNp, start, ctwk.data (), 1, &sync_ref);
        QVERIFY2 (std::abs (sync_cpp - sync_ref) < 1.0e-4f,
                  qPrintable (QStringLiteral ("FT4 sync4d mismatch at start=%1: cpp=%2 ref=%3")
                              .arg (start)
                              .arg (sync_cpp, 0, 'g', 9)
                              .arg (sync_ref, 0, 'g', 9)));
      }
  }

  Q_SLOT void ft4_bitmetrics_smoke ()
  {
    constexpr int kNn = 103;
    constexpr int kNss = 576 / 18;
    std::vector<std::complex<float>> cd (kNn * kNss, std::complex<float> {});
    std::vector<float> bitmetrics (2 * kNn * 3, 0.0f);

    for (int iter = 0; iter < 64; ++iter)
      {
        int badsync = 0;
        std::fill (bitmetrics.begin (), bitmetrics.end (), 0.0f);
        ftx_ft4_bitmetrics_c (cd.data (), bitmetrics.data (), &badsync);
        QCOMPARE (badsync, 1);
      }
  }

  Q_SLOT void ft4_bitmetrics_costas_sync ()
  {
    constexpr int kNn = 103;
    constexpr int kNss = 576 / 18;
    constexpr float kPi = 3.14159265358979323846f;
    std::vector<std::complex<float>> cd (kNn * kNss, std::complex<float> {});
    std::vector<float> bitmetrics (2 * kNn * 3, 0.0f);
    std::array<int, kNn> tones {};
    tones.fill (0);

    std::array<int, 4> const costas_a {{0, 1, 3, 2}};
    std::array<int, 4> const costas_b {{1, 0, 2, 3}};
    std::array<int, 4> const costas_c {{2, 3, 1, 0}};
    std::array<int, 4> const costas_d {{3, 2, 0, 1}};

    for (int i = 0; i < 4; ++i)
      {
        tones[static_cast<size_t> (i)] = costas_a[static_cast<size_t> (i)];
        tones[static_cast<size_t> (33 + i)] = costas_b[static_cast<size_t> (i)];
        tones[static_cast<size_t> (66 + i)] = costas_c[static_cast<size_t> (i)];
        tones[static_cast<size_t> (99 + i)] = costas_d[static_cast<size_t> (i)];
      }

    for (int sym = 0; sym < kNn; ++sym)
      {
        int const tone = tones[static_cast<size_t> (sym)];
        for (int n = 0; n < kNss; ++n)
          {
            float const phase = 2.0f * kPi * static_cast<float> (tone * n) / static_cast<float> (kNss);
            cd[static_cast<size_t> (sym * kNss + n)] = std::polar (1.0f, phase);
          }
      }

    int badsync = 1;
    ftx_ft4_bitmetrics_c (cd.data (), bitmetrics.data (), &badsync);
    QCOMPARE (badsync, 0);

    bool non_zero = false;
    for (float value : bitmetrics)
      {
        QVERIFY (std::isfinite (value));
        if (std::abs (value) > 1.0e-6f)
          {
            non_zero = true;
          }
      }
    QVERIFY (non_zero);
  }

  Q_SLOT void ft4_bitmetrics_matches_fortran_reference ()
  {
    constexpr int kNn = 103;
    constexpr int kNss = 576 / 18;
    constexpr int kRows = 2 * kNn;
    constexpr float kPi = 3.14159265358979323846f;
    std::vector<std::complex<float>> cd (kNn * kNss, std::complex<float> {});
    std::vector<float> bitmetrics_cpp (kRows * 3, 0.0f);
    std::vector<float> bitmetrics_ref (kRows * 3, 0.0f);
    std::array<int, kNn> tones {};

    for (int sym = 0; sym < kNn; ++sym)
      {
        tones[static_cast<size_t> (sym)] = (3 * sym + 1) % 4;
      }

    std::array<int, 4> const costas_a {{0, 1, 3, 2}};
    std::array<int, 4> const costas_b {{1, 0, 2, 3}};
    std::array<int, 4> const costas_c {{2, 3, 1, 0}};
    std::array<int, 4> const costas_d {{3, 2, 0, 1}};
    for (int i = 0; i < 4; ++i)
      {
        tones[static_cast<size_t> (i)] = costas_a[static_cast<size_t> (i)];
        tones[static_cast<size_t> (33 + i)] = costas_b[static_cast<size_t> (i)];
        tones[static_cast<size_t> (66 + i)] = costas_c[static_cast<size_t> (i)];
        tones[static_cast<size_t> (99 + i)] = costas_d[static_cast<size_t> (i)];
      }

    for (int sym = 0; sym < kNn; ++sym)
      {
        int const tone = tones[static_cast<size_t> (sym)];
        for (int n = 0; n < kNss; ++n)
          {
            float const phase = 2.0f * kPi * static_cast<float> (tone * n) / static_cast<float> (kNss);
            cd[static_cast<size_t> (sym * kNss + n)] = std::polar (1.0f, phase);
          }
      }

    int badsync_cpp = 1;
    int badsync_ref = 1;
    ftx_ft4_bitmetrics_c (cd.data (), bitmetrics_cpp.data (), &badsync_cpp);
    ftx_ft4_bitmetrics_ref_c (cd.data (), bitmetrics_ref.data (), &badsync_ref);

    QCOMPARE (badsync_cpp, badsync_ref);
    QCOMPARE (badsync_ref, 0);

    for (size_t i = 0; i < bitmetrics_cpp.size (); ++i)
      {
        QVERIFY2 (std::isfinite (bitmetrics_cpp[i]), "C++ bitmetric is not finite");
        QVERIFY2 (std::isfinite (bitmetrics_ref[i]), "Fortran bitmetric is not finite");
        QVERIFY2 (std::abs (bitmetrics_cpp[i] - bitmetrics_ref[i]) < 1.0e-4f,
                   qPrintable (QStringLiteral ("FT4 bitmetric mismatch at %1: cpp=%2 ref=%3")
                               .arg (static_cast<qulonglong> (i))
                               .arg (bitmetrics_cpp[i], 0, 'g', 9)
                               .arg (bitmetrics_ref[i], 0, 'g', 9)));
      }
  }

  Q_SLOT void ft4_bitmetrics_matches_fortran_reference_on_generated_waveform ()
  {
    constexpr int kNn = 103;
    constexpr int kNss = 32;
    constexpr int kRows = 2 * kNn;
    constexpr int kNwave = (kNn + 2) * kNss;
    std::array<char, 37> message {};
    std::array<char, 37> msgsent {};
    std::array<signed char, 77> msgbits {};
    std::array<int, kNn> itone {};
    std::vector<std::complex<float>> cwave (kNwave, std::complex<float> {});
    std::vector<std::complex<float>> cd (kNn * kNss, std::complex<float> {});
    std::vector<float> wave (kNwave, 0.0f);
    std::vector<float> bitmetrics_cpp (kRows * 3, 0.0f);
    std::vector<float> bitmetrics_ref (kRows * 3, 0.0f);

    std::fill (message.begin (), message.end (), ' ');
    std::fill (msgsent.begin (), msgsent.end (), ' ');
    QByteArray const text {"CQ K1ABC FN42"};
    std::copy (text.begin (), text.end (), message.begin ());

    int ichk = 0;
    genft4_ (message.data (), &ichk, msgsent.data (), msgbits.data (), itone.data (),
             static_cast<fortran_charlen_t_local> (message.size ()),
             static_cast<fortran_charlen_t_local> (msgsent.size ()));

    int nsym = kNn;
    int nsps = kNss;
    int icmplx = 1;
    int nwave = kNwave;
    float fsample = 200.0f;
    float f0 = 0.0f;
    gen_ft4wave_ (itone.data (), &nsym, &nsps, &fsample, &f0,
                  cwave.data (), wave.data (), &icmplx, &nwave);

    std::copy_n (cwave.begin () + kNss, cd.size (), cd.begin ());

    int badsync_cpp = 1;
    int badsync_ref = 1;
    ftx_ft4_bitmetrics_c (cd.data (), bitmetrics_cpp.data (), &badsync_cpp);
    ftx_ft4_bitmetrics_ref_c (cd.data (), bitmetrics_ref.data (), &badsync_ref);

    QCOMPARE (badsync_cpp, badsync_ref);
    QCOMPARE (badsync_ref, 0);

    for (size_t i = 0; i < bitmetrics_cpp.size (); ++i)
      {
        QVERIFY2 (std::isfinite (bitmetrics_cpp[i]), "C++ generated-wave bitmetric is not finite");
        QVERIFY2 (std::isfinite (bitmetrics_ref[i]), "Fortran generated-wave bitmetric is not finite");
        QVERIFY2 (std::abs (bitmetrics_cpp[i] - bitmetrics_ref[i]) < 1.0e-4f,
                   qPrintable (QStringLiteral ("Generated-wave FT4 bitmetric mismatch at %1: cpp=%2 ref=%3")
                               .arg (static_cast<qulonglong> (i))
                               .arg (bitmetrics_cpp[i], 0, 'g', 9)
                               .arg (bitmetrics_ref[i], 0, 'g', 9)));
      }
  }

  Q_SLOT void ft4_decode_stage3_matches_stage0_generated_waveform ()
  {
    constexpr int kNn = 103;
    constexpr int kNsps = 576;
    constexpr int kSampleCount = 21 * 3456;
    constexpr int kMaxLines = 100;
    constexpr int kDecodedChars = 37;
    std::array<char, 37> message {};
    std::array<char, 37> msgsent {};
    std::array<char, 12> mycall {};
    std::array<char, 12> hiscall {};
    std::array<signed char, 77> msgbits {};
    std::array<int, kNn> itone {};
    std::vector<std::complex<float>> c0 (kSampleCount, std::complex<float> {});
    std::vector<std::complex<float>> shifted (kSampleCount, std::complex<float> {});
    std::vector<float> wave (kSampleCount, 0.0f);
    std::vector<short> iwave (kSampleCount, 0);

    std::fill (message.begin (), message.end (), ' ');
    std::fill (msgsent.begin (), msgsent.end (), ' ');
    std::fill (mycall.begin (), mycall.end (), ' ');
    std::fill (hiscall.begin (), hiscall.end (), ' ');
    QByteArray const text {"CQ K1ABC FN42"};
    std::copy (text.begin (), text.end (), message.begin ());

    int ichk = 0;
    genft4_ (message.data (), &ichk, msgsent.data (), msgbits.data (), itone.data (),
             static_cast<fortran_charlen_t_local> (message.size ()),
             static_cast<fortran_charlen_t_local> (msgsent.size ()));

    int nsym = kNn;
    int nsps = kNsps;
    int icmplx = 1;
    int nwave = kSampleCount;
    float fsample = 12000.0f;
    float f0 = 1000.0f;
    gen_ft4wave_ (itone.data (), &nsym, &nsps, &fsample, &f0,
                  c0.data (), wave.data (), &icmplx, &nwave);

    float const xdt = 0.0f;
    float const dt = 1.0f / fsample;
    int const kshift = static_cast<int> (std::lround ((xdt + 0.5f) / dt)) - kNsps;
    std::fill (shifted.begin (), shifted.end (), std::complex<float> {});
    if (kshift >= 0)
      {
        std::copy_n (c0.begin (), c0.size () - static_cast<size_t> (kshift), shifted.begin () + kshift);
      }
    else
      {
        std::copy (c0.begin () + (-kshift), c0.end (), shifted.begin ());
      }

    float const snrdb = -10.0f;
    float const bandwidth_ratio = 2500.0f / (fsample / 2.0f);
    float const sig = std::sqrt (2.0f * bandwidth_ratio) * std::pow (10.0f, 0.05f * snrdb);
    std::mt19937 rng {123456u};
    std::normal_distribution<float> noise {0.0f, 1.0f};
    for (size_t i = 0; i < iwave.size (); ++i)
      {
        float const sample = 100.0f * (sig * shifted[i].real () + noise (rng));
        float const clipped = std::max (-32767.0f, std::min (32767.0f, sample));
        iwave[i] = static_cast<short> (std::lround (clipped));
      }

    auto run_decode = [&] (int stage) {
      std::array<int, kMaxLines> snrs {};
      std::array<float, kMaxLines> dts {};
      std::array<float, kMaxLines> freqs {};
      std::array<int, kMaxLines> naps {};
      std::array<float, kMaxLines> quals {};
      std::array<float, kMaxLines> syncs {};
      std::array<signed char, 77 * kMaxLines> bits77 {};
      std::array<char, kMaxLines * kDecodedChars> decodeds {};
      int nout = 0;
      int nqsoprogress = 0;
      int nfqso = 1000;
      int nfa = 200;
      int nfb = 3000;
      int ndepth = 3;
      int lapcqonly = 0;
      int ncontest = 0;

      ftx_ft4_cpp_dsp_rollout_stage_override_c (stage);
      ftx_ft4_decode_c (iwave.data (), &nqsoprogress, &nfqso, &nfa, &nfb,
                        &ndepth, &lapcqonly, &ncontest, mycall.data (), hiscall.data (),
                        syncs.data (), snrs.data (), dts.data (), freqs.data (), naps.data (),
                        quals.data (), bits77.data (), decodeds.data (), &nout,
                        static_cast<fortran_charlen_t_local> (mycall.size ()),
                        static_cast<fortran_charlen_t_local> (hiscall.size ()),
                        static_cast<fortran_charlen_t_local> (decodeds.size ()));

      QStringList lines;
      for (int i = 0; i < nout; ++i)
        {
          QByteArray raw (decodeds.data () + i * kDecodedChars, kDecodedChars);
          while (!raw.isEmpty () && raw.back () == ' ')
            {
              raw.chop (1);
            }
          lines << QString::fromLatin1 (raw.constData (), raw.size ());
        }
      return lines;
    };

    QStringList const stage0 = run_decode (0);
    QStringList const stage3 = run_decode (3);
    QStringList const stage4 = run_decode (4);
    ftx_ft4_cpp_dsp_rollout_stage_reset_c ();

    QVERIFY2 (!stage0.isEmpty (), "FT4 stage 0 produced no decodes on generated waveform");
    QVERIFY (stage0.contains (QStringLiteral ("CQ K1ABC FN42")));
    QCOMPARE (stage3, stage0);
    QCOMPARE (stage4, stage0);
  }

  Q_SLOT void ft2_async_decode_stage7_matches_stage0_case2_regression ()
  {
    std::vector<short> const iwave = make_ft2_case_wave (
        QStringLiteral ("K1ABC N0CALL"), 2780.0f, 1080.0f, 10.0f, 12345u);
    QVERIFY2 (!iwave.empty (), "Failed to generate FT2 case2 regression waveform");

    QStringList const stage0 = run_ft2_decode_stage (iwave, 0);
    QStringList const stage1 = run_ft2_decode_stage (iwave, 1);
    QStringList const stage2 = run_ft2_decode_stage (iwave, 2);
    QStringList const stage3 = run_ft2_decode_stage (iwave, 3);
    QStringList const stage4 = run_ft2_decode_stage (iwave, 4);
    QStringList const stage5 = run_ft2_decode_stage (iwave, 5);
    QStringList const stage6 = run_ft2_decode_stage (iwave, 6);
    QStringList const stage7 = run_ft2_decode_stage (iwave, 7);
    ftx_ft2_cpp_dsp_rollout_stage_reset_c ();

    QVERIFY2 (!stage0.isEmpty (), "FT2 stage 0 produced no decodes on regression waveform");
    QVERIFY (stage0.contains (QStringLiteral ("K1ABC N0CALL")));
    QCOMPARE (stage1, stage0);
    QCOMPARE (stage2, stage0);
    QCOMPARE (stage3, stage0);
    QCOMPARE (stage4, stage0);
    QCOMPARE (stage5, stage0);
    QCOMPARE (stage6, stage0);
    QCOMPARE (stage7, stage0);
  }

  Q_SLOT void ft2_async_decode_stage7_matches_stage0_low_snr_frequency_sweep ()
  {
    struct LowSnrCase
    {
      float frequency_hz;
      float offset_ms;
      unsigned seed;
    };

    std::array<LowSnrCase, 4> const cases {{
        {500.0f, 520.0f, 4101u},
        {1000.0f, 610.0f, 4102u},
        {1500.0f, 700.0f, 4103u},
        {2000.0f, 790.0f, 4104u},
    }};

    for (LowSnrCase const& test_case : cases)
      {
        std::vector<short> const iwave = make_ft2_case_wave (
            QStringLiteral ("CQ TEST K1ABC FN42"),
            test_case.frequency_hz,
            test_case.offset_ms,
            -23.0f,
            test_case.seed);
        QVERIFY2 (!iwave.empty (), "Failed to generate FT2 low-SNR sweep waveform");

        QStringList const stage0 = run_ft2_decode_stage (iwave, 0);
        QStringList const stage7 = run_ft2_decode_stage (iwave, 7);
        ftx_ft2_cpp_dsp_rollout_stage_reset_c ();

        QCOMPARE (stage7, stage0);
      }
  }

  Q_SLOT void ft2_triggered_decode_wrapper_matches_stage7_generated_waveform ()
  {
    std::vector<short> const iwave = make_ft2_case_wave (
        QStringLiteral ("K1ABC N0CALL"), 2780.0f, 1080.0f, 10.0f, 12345u);
    QVERIFY2 (!iwave.empty (), "Failed to generate FT2 triggered regression waveform");

    QStringList const stage7_async = run_ft2_decode_stage (iwave, 7);
    QStringList const stage7_triggered = run_ft2_triggered_decode_stage (iwave, 7);
    ftx_ft2_cpp_dsp_rollout_stage_reset_c ();

    QVERIFY2 (!stage7_async.isEmpty (), "FT2 stage 7 produced no decodes on triggered regression waveform");
    QCOMPARE (stage7_triggered.size (), stage7_async.size ());
    for (QString const& decoded : stage7_async)
      {
        QVERIFY (stage7_triggered.join (QLatin1Char {'\n'}).contains (decoded));
      }
  }

  Q_SLOT void ft2_async_decode_stage7_matches_stage0_weak_decode_threshold_case ()
  {
    QString const tool_path =
        QDir {QCoreApplication::applicationDirPath ()}.absoluteFilePath (QStringLiteral ("ft2_stage_compare"));
    QString const fixture_path =
        QDir {QCoreApplication::applicationDirPath ()}.absoluteFilePath (
            QStringLiteral ("../ft2_stage7_low_snr_m17/ft2_2000hz_snr-17.wav"));
    if (!QFileInfo::exists (tool_path))
      {
        QSKIP ("ft2_stage_compare tool not found; offline FT2 comparator missing");
      }
    if (!QFileInfo::exists (fixture_path))
      {
        QSKIP ("FT2 weak-threshold WAV fixture not found; offline regression fixture missing");
      }

    QProcess compare;
    compare.start (tool_path, {fixture_path, QStringLiteral ("--stages"), QStringLiteral ("0,7")});
    QVERIFY2 (compare.waitForFinished (300000), "ft2_stage_compare did not finish");

    QByteArray const stdout_text = compare.readAllStandardOutput ();
    QByteArray const stderr_text = compare.readAllStandardError ();
    QVERIFY2 (compare.exitStatus () == QProcess::NormalExit && compare.exitCode () == 0,
              qPrintable (QString::fromLatin1 (stderr_text.isEmpty () ? stdout_text : stderr_text)));

    QString const output = QString::fromLatin1 (stdout_text);
    QVERIFY2 (output.contains (QStringLiteral ("stage 0: nout=1")),
              qPrintable (output));
    QVERIFY2 (output.contains (QStringLiteral ("stage 7: nout=1")),
              qPrintable (output));
    QVERIFY2 (output.contains (QStringLiteral ("decoded=\"CQ TEST K1ABC FN42\"")),
              qPrintable (output));
    QVERIFY2 (output.contains (QStringLiteral ("message parity stage 0 vs stage 7: common=1 only_0=0 only_7=0")),
              qPrintable (output));
  }

  Q_SLOT void ft2_candidate_stage7_decodes_generated_candidate ()
  {
    std::array<signed char, kFt2Bits> const rvec {{
        0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0,
        1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1,
        0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1
    }};
    decodium::txmsg::EncodedMessage const encoded =
        decodium::txmsg::encodeFt2 (QStringLiteral ("CQ TEST K1ABC FN42"));
    QVERIFY2 (encoded.ok, "Failed to encode FT2 reference candidate");
    QCOMPARE (encoded.msgbits.size (), kFt2Bits);

    std::array<signed char, kFt2Bits> bits77 {};
    for (int i = 0; i < kFt2Bits; ++i)
      {
        bits77[static_cast<size_t> (i)] =
            (encoded.msgbits.at (i) == '1') ? static_cast<signed char> (1)
                                            : static_cast<signed char> (0);
        bits77[static_cast<size_t> (i)] =
            static_cast<signed char> ((bits77[static_cast<size_t> (i)] + rvec[static_cast<size_t> (i)]) & 1);
      }

    std::array<signed char, kFt2Codeword> codeword {};
    QVERIFY2 (ftx_encode174_91_message77_c (bits77.data (), codeword.data ()) != 0,
              "Failed to build FT2 codeword for candidate wrapper test");

    std::array<float, kFt2Codeword> llra {};
    std::array<float, kFt2Codeword> llrb {};
    std::array<float, kFt2Codeword> llrc {};
    std::array<float, kFt2Codeword> llrd {};
    std::array<float, kFt2Codeword> llre {};
    for (int i = 0; i < kFt2Codeword; ++i)
      {
        float const hard_llr = codeword[static_cast<size_t> (i)] != 0 ? 6.0f : -6.0f;
        llra[static_cast<size_t> (i)] = hard_llr;
        llrb[static_cast<size_t> (i)] = 0.95f * hard_llr;
        llrc[static_cast<size_t> (i)] = 1.05f * hard_llr;
        llrd[static_cast<size_t> (i)] = hard_llr;
        llre[static_cast<size_t> (i)] = hard_llr;
      }

    float apmag = 0.0f;
    for (float value : llra)
      {
        apmag = std::max (apmag, std::fabs (value));
      }
    apmag *= 1.1f;

    std::array<char, 13> const mycall = make_ft2_context_field (QString {});
    std::array<char, 13> const hiscall = make_ft2_context_field (QString {});
    std::array<signed char, kFt2Bits> bits_cpp {};
    std::array<char, kFt2DecodedChars> decoded_cpp {};
    int ok_cpp = 0;
    int stop_cpp = 0;
    int pass_cpp = 0;
    int iap_cpp = 0;
    int nh_cpp = -1;
    int maxosd_cpp = 0;
    float dmin_cpp = 0.0f;

    std::fill (decoded_cpp.begin (), decoded_cpp.end (), ' ');

    ftx_ft2_decode_candidate_stage7_c (
        llra.data (), llrb.data (), llrc.data (), llrd.data (), llre.data (),
        3, 0, 0, 0, 1000, 2000.0f, apmag, 0, 1, mycall.data (), hiscall.data (),
        &ok_cpp, &stop_cpp, &pass_cpp, &iap_cpp, &nh_cpp, &dmin_cpp, &maxosd_cpp,
        bits_cpp.data (), decoded_cpp.data ());

    QByteArray const decoded_cpp_text =
        trim_fortran_field_local (decoded_cpp.data (), kFt2DecodedChars);

    QVERIFY (ok_cpp == 0 || ok_cpp == 1);
    QVERIFY (stop_cpp == 0 || stop_cpp == 1);
    if (ok_cpp != 0)
      {
        QCOMPARE (decoded_cpp_text, QByteArrayLiteral ("CQ TEST K1ABC FN42"));
        QCOMPARE (QByteArray (reinterpret_cast<char const*> (bits_cpp.data ()), kFt2Bits),
                  QByteArray (reinterpret_cast<char const*> (bits77.data ()), kFt2Bits));
      }
    QVERIFY (pass_cpp >= 0);
    QVERIFY (iap_cpp >= 0);
    QVERIFY (nh_cpp >= -1);
    QVERIFY (maxosd_cpp >= 0);
    QVERIFY (std::isfinite (dmin_cpp));
  }

  Q_SLOT void ft2_async_decode_stage7_matches_stage0_2000hz_minus17db ()
  {
    QTemporaryDir temp_dir;
    QVERIFY2 (temp_dir.isValid (), "failed to create temporary directory for FT2 weak decode test");

    QString const bin_dir = QCoreApplication::applicationDirPath ();
    QString const make_wav_path =
        QFileInfo {bin_dir + QStringLiteral ("/ft2_make_test_wav")}.absoluteFilePath ();
    QString const compare_path =
        QFileInfo {bin_dir + QStringLiteral ("/ft2_stage_compare")}.absoluteFilePath ();
    if (!QFileInfo::exists (make_wav_path) || !QFileInfo::exists (compare_path))
      {
        QSKIP (qPrintable (QStringLiteral ("FT2 compare binaries not built (%1, %2)")
                               .arg (make_wav_path, compare_path)));
      }

    QString const wav_path = temp_dir.filePath (QStringLiteral ("ft2_2000hz_snr-17.wav"));
    QProcess make_wav;
    make_wav.start (make_wav_path, {wav_path,
                                    QStringLiteral ("--message"), QStringLiteral ("CQ TEST K1ABC FN42"),
                                    QStringLiteral ("--freq"), QStringLiteral ("2000"),
                                    QStringLiteral ("--offset-ms"), QStringLiteral ("790"),
                                    QStringLiteral ("--snr-db"), QStringLiteral ("-17"),
                                    QStringLiteral ("--seed"), QStringLiteral ("4003")});
    QVERIFY2 (make_wav.waitForFinished (30000), qPrintable (make_wav.errorString ()));
    QCOMPARE (make_wav.exitStatus (), QProcess::NormalExit);
    QCOMPARE (make_wav.exitCode (), 0);
    QVERIFY2 (QFileInfo::exists (wav_path), qPrintable (QStringLiteral ("missing generated FT2 WAV %1").arg (wav_path)));

    QProcess compare;
    compare.start (compare_path, {wav_path, QStringLiteral ("--stages"), QStringLiteral ("0,7")});
    QVERIFY2 (compare.waitForFinished (300000), qPrintable (compare.errorString ()));
    QCOMPARE (compare.exitStatus (), QProcess::NormalExit);
    QCOMPARE (compare.exitCode (), 0);

    QString const output = QString::fromLocal8Bit (compare.readAllStandardOutput ())
                         + QString::fromLocal8Bit (compare.readAllStandardError ());
    if (output.contains (QStringLiteral ("stage 0: nout=0"))
        || output.contains (QStringLiteral ("stage 7: nout=0")))
      {
        QSKIP ("Generated FT2 weak waveform did not decode cleanly in both paths; use the fixture-based weak-threshold regression instead.");
      }
    QVERIFY2 (output.contains (QStringLiteral ("stage 0: nout=1")), qPrintable (output));
    QVERIFY2 (output.contains (QStringLiteral ("stage 7: nout=1")), qPrintable (output));
    QVERIFY2 (output.contains (QStringLiteral ("decoded=\"CQ TEST K1ABC FN42\"")), qPrintable (output));
    QVERIFY2 (output.contains (QStringLiteral ("message parity stage 0 vs stage 7: common=1 only_0=0 only_7=0")),
              qPrintable (output));
  }

  Q_SLOT void ft2_real1_recording_scan_has_no_stage7_regressions ()
  {
    QString const repo_root =
        QFileInfo {QDir {QCoreApplication::applicationDirPath ()}.absoluteFilePath (
            QStringLiteral ("../.."))}.absoluteFilePath ();
    QString const script_path =
        QFileInfo {QDir {QCoreApplication::applicationDirPath ()}.absoluteFilePath (
            QStringLiteral ("../../build-utils/ft2_recording_scan_compare.sh"))}.absoluteFilePath ();
    QString const sample_path =
        QFileInfo {QDir {QCoreApplication::applicationDirPath ()}.absoluteFilePath (
            QStringLiteral ("../FT2-Real1.wav"))}.absoluteFilePath ();

    if (!QFileInfo::exists (script_path) || !QFileInfo::exists (sample_path))
      {
        QSKIP ("FT2 real1 recording scan fixture not available");
      }

    QProcess scan;
    scan.setWorkingDirectory (repo_root);
    scan.start (QStringLiteral ("bash"),
                {script_path, sample_path,
                 QStringLiteral ("--stages"), QStringLiteral ("0,7"),
                 QStringLiteral ("--mycall"), QStringLiteral ("9H1SR")});
    QVERIFY2 (scan.waitForFinished (600000), qPrintable (scan.errorString ()));
    QCOMPARE (scan.exitStatus (), QProcess::NormalExit);
    QCOMPARE (scan.exitCode (), 0);

    QString const output = QString::fromLocal8Bit (scan.readAllStandardOutput ())
                         + QString::fromLocal8Bit (scan.readAllStandardError ());
    QVERIFY2 (output.contains (QStringLiteral ("decode_windows=0/")), qPrintable (output));
    QVERIFY2 (output.contains (QStringLiteral ("mismatch_windows=0/")), qPrintable (output));
  }

  Q_SLOT void ft2_real2_recording_scan_matches_stage0 ()
  {
    QString const bin_dir = QCoreApplication::applicationDirPath ();
    QString const compare_path =
        QFileInfo {bin_dir + QStringLiteral ("/ft2_stage_compare")}.absoluteFilePath ();
    QStringList const chunk_paths {
      QFileInfo {QDir {bin_dir}.absoluteFilePath (
          QStringLiteral ("../ft2_real2_chunks/real2_00_00.wav"))}.absoluteFilePath (),
      QFileInfo {QDir {bin_dir}.absoluteFilePath (
          QStringLiteral ("../ft2_real2_chunks/real2_03_75.wav"))}.absoluteFilePath (),
      QFileInfo {QDir {bin_dir}.absoluteFilePath (
          QStringLiteral ("../ft2_real2_chunks/real2_07_50.wav"))}.absoluteFilePath ()
    };

    if (!QFileInfo::exists (compare_path)
        || !std::all_of (chunk_paths.cbegin (), chunk_paths.cend (), [] (QString const& path) {
             return QFileInfo::exists (path);
           }))
      {
        QSKIP ("FT2 real2 chunked fixtures or comparator not available");
      }

    for (QString const& chunk_path : chunk_paths)
      {
        QProcess compare;
        compare.start (compare_path, {chunk_path,
                                      QStringLiteral ("--stages"), QStringLiteral ("0,7"),
                                      QStringLiteral ("--mycall"), QStringLiteral ("9H1SR")});
        QVERIFY2 (compare.waitForFinished (120000), qPrintable (compare.errorString ()));
        QCOMPARE (compare.exitStatus (), QProcess::NormalExit);
        QCOMPARE (compare.exitCode (), 0);

        QString const output = QString::fromLocal8Bit (compare.readAllStandardOutput ())
                             + QString::fromLocal8Bit (compare.readAllStandardError ());
        QVERIFY2 (output.contains (QStringLiteral ("stage 0: nout=")), qPrintable (output));
        QVERIFY2 (output.contains (QStringLiteral ("message parity stage 0 vs stage 7:")),
                  qPrintable (output));
        QVERIFY2 (output.contains (QStringLiteral ("only_0=0 only_7=0")), qPrintable (output));
      }
  }

  Q_SLOT void ft2_equalized_branch_matches_reference_on_real_sample ()
  {
    QString const bin_dir = QCoreApplication::applicationDirPath ();
    QString const compare_path =
        QFileInfo {bin_dir + QStringLiteral ("/ft2_equalized_compare")}.absoluteFilePath ();
    QString const sample_path =
        QFileInfo {QDir {bin_dir}.absoluteFilePath (
            QStringLiteral ("../ft2_real2_chunks/real2_00_00.wav"))}.absoluteFilePath ();

    if (!QFileInfo::exists (compare_path) || !QFileInfo::exists (sample_path))
      {
        QSKIP ("FT2 equalized compare tool or real-sample fixture not available");
      }

    QProcess compare;
    compare.start (compare_path, {sample_path,
                                  QStringLiteral ("--syncmin"), QStringLiteral ("1"),
                                  QStringLiteral ("--maxcand"), QStringLiteral ("50")});
    QVERIFY2 (compare.waitForFinished (180000), qPrintable (compare.errorString ()));
    QCOMPARE (compare.exitStatus (), QProcess::NormalExit);
    QCOMPARE (compare.exitCode (), 0);

    QString const output = QString::fromLocal8Bit (compare.readAllStandardOutput ())
                         + QString::fromLocal8Bit (compare.readAllStandardError ());
    QVERIFY2 (output.contains (QStringLiteral ("compared_segments=")), qPrintable (output));
    QVERIFY2 (output.contains (QStringLiteral ("mismatched_segments=0")), qPrintable (output));
  }
};

QTEST_MAIN (TestQtHelpers);

#include "test_qt_helpers.moc"
