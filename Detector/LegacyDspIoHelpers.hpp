#ifndef LEGACY_DSP_IO_HELPERS_HPP
#define LEGACY_DSP_IO_HELPERS_HPP

#include "commons.h"

#include <QByteArray>
#include <QString>
#include <array>
#include <vector>

namespace decodium
{
namespace legacy
{

struct CalibrationSolution
{
  int iz {0};
  double a {0.0};
  double b {0.0};
  double rms {0.0};
  double sigmaa {0.0};
  double sigmab {0.0};
  int irc {0};
};

struct EchoAnalysisResult
{
  int nqual {0};
  float xlevel {0.0f};
  float sigdb {0.0f};
  float db_err {0.0f};
  float dfreq {0.0f};
  float width {0.0f};
  float xdt {0.0f};
  QString rxcall;
};

struct SpectrumPlotState
{
  std::array<float, NSMAX> syellow {};
  std::array<float, 3457> ref {};
  std::array<float, 3457> filter {};
};

struct EchoPlotState
{
  int nclearave {0};
  int nsum {0};
  std::array<float, 4096> blue {};
  std::array<float, 4096> red {};
};

struct Jt65SymspecResult
{
  int nqsym {0};
  int nhsym {0};
  std::vector<float> ss;
  std::vector<float> savg;
  std::vector<float> ref;
  bool ok {false};
};

struct Jt65SyncCandidate
{
  float freq {0.0f};
  float dt {0.0f};
  float sync {0.0f};
  float flip {0.0f};
};

struct Jt65Demod64aResult
{
  std::array<int, 63> mrsym;
  std::array<int, 63> mrprob;
  std::array<int, 63> mr2sym;
  std::array<int, 63> mr2prob;
  int ntest {0};
  int nlow {0};
};

struct Jt65ExtractResult
{
  int ncount {-99};
  int nhist {0};
  QByteArray decoded;
  bool ltext {false};
  int nft {0};
  float qual {0.0f};
  std::array<int, 63> correct {};
  std::array<int, 10> param {};
  std::array<int, 63> mrs {};
  std::array<int, 63> mrs2 {};
  std::array<float, 64 * 63> s3a {};
};

struct Jt65Decode65bResult
{
  int nft {0};
  float qual {0.0f};
  int nhist {0};
  QByteArray decoded;
};

struct Jt65Decode65aResult
{
  float sync2 {0.0f};
  std::array<float, 5> a {};
  float dt {0.0f};
  int nft {0};
  int nspecial {0};
  float qual {0.0f};
  int nhist {0};
  int nsmo {0};
  QByteArray decoded;
};

struct Jt65SharedStateView
{
  int const* param {nullptr};
  int const* mrs {nullptr};
  int const* mrs2 {nullptr};
  int const* mdat {nullptr};
  int const* mref {nullptr};
  int const* mdat2 {nullptr};
  int const* mref2 {nullptr};
  float const* pr {nullptr};
  float const* s3a {nullptr};
  float width {0.0f};
  int const* correct {nullptr};
  float sync_threshold {0.0f};
  float refspec_dfref {0.0f};
  float const* refspec_ref {nullptr};
  float const* sync_ss {nullptr};
};

void wav12_inplace (short* d2_io, int* npts_io, short sample_bits);
QString freqcal_line (short const* id2, int k, int nkhz, int noffset, int ntol);
CalibrationSolution calibrate_freqcal_directory (QString const& data_dir);
SpectrumPlotState& spectrum_plot_state ();
void clear_spectrum_plot_state ();
EchoPlotState& echo_plot_state ();
void clear_echo_plot_state ();
void reset_refspectrum_state ();
void refspectrum_update (short* id2, bool clear_reference, bool accumulate_reference,
                         bool use_reference, QString const& file_path);
void wspr_downsample_update (short const* id2, int k);
int savec2_file (QString const& file_path, int tr_seconds, double f0m1500);
void degrade_snr_inplace (short* d2, int npts, float db, float bandwidth);
Jt65SymspecResult symspec65_compute (float const* dd, int npts);
std::vector<Jt65SyncCandidate> sync65_compute (int nfa, int nfb, int ntol, int nqsym,
                                               bool robust, bool vhf, float thresh0);
void graycode65_inplace (int* dat, int n, int idir);
void interleave63_inplace (int* data, int idir);
void smooth121_inplace (float* x, int nz);
void lorentzian_fit (float const* y, int npts, std::array<float, 5>* a_out);
void subtract65_inplace (float* dd, int npts, float f0, float dt);
void set_jt65_data_dir_hint (QByteArray const& path);
void jt65_initialize_tables ();
Jt65SharedStateView jt65_shared_state ();
void jt65_set_mode_width (float width);
float jt65_mode_width ();
void jt65_set_sync_threshold (float thresh0);
float jt65_sync_threshold ();
void jt65_set_decode_smoothing (int nsmo);
int jt65_decode_smoothing ();
void jt65_store_symspec_state (Jt65SymspecResult const& result);
void jt65_store_extract_state (Jt65ExtractResult const& result);
Jt65Demod64aResult demod64a_compute (float const* s3, int nadd, float afac1);
Jt65ExtractResult extract_compute (float const* s3, int nadd, int mode65, int ntrials,
                                   int naggressive, int ndepth, int nflip,
                                   QByteArray const& mycall_12, QByteArray const& hiscall_12,
                                   QByteArray const& hisgrid_6, int nQSOProgress,
                                   bool ljt65apon);
Jt65Decode65bResult decode65b_compute (float const* s2, int nflip, int nadd, int mode65,
                                       int ntrials, int naggressive, int ndepth,
                                       QByteArray const& mycall_12, QByteArray const& hiscall_12,
                                       QByteArray const& hisgrid_6, int nQSOProgress,
                                       bool ljt65apon);
Jt65Decode65aResult decode65a_compute (float const* dd, int npts, int newdat, int nqd, float f0,
                                       int nflip, int mode65, int ntrials, int naggressive,
                                       int ndepth, int ntol, QByteArray const& mycall_12,
                                       QByteArray const& hiscall_12, QByteArray const& hisgrid_6,
                                       int nQSOProgress, bool ljt65apon, bool vhf,
                                       float dt_hint = 0.0f);
void jt65_store_last_s1 (float const* src);
void jt65_load_last_s1 (float* dst);
void symspec_update (dec_data_t* shared_data, int k, int nsps, int ingain,
                     bool low_sidelobes, int minw, float* pxdb, float* s, float* df3,
                     int* ihsym, int* npts8, float* pxdbmax);
void hspec_update (short const* id2, int k, int ntrpdepth, int ingain,
                   float* green, float* s, int* jh, float* pxmax, float* db_no_gain);
void save_echo_params_inplace (int ndop_total, int ndop_audio, int nfrit, float f1, float fspread,
                               int tone_spacing, int const itone[6], short id2[15]);
void load_echo_params (short const id2[15], int* ndop_total, int* ndop_audio, int* nfrit,
                       float* f1, float* fspread, int* tone_spacing, int itone[6]);
EchoAnalysisResult avecho_update (short const* id2, int ndop, int nfrit, int nauto, int ndf,
                                  int navg, float f1, float width, bool disk_data,
                                  bool echo_call, QString const& txcall,
                                  int nclearave = 0, float fspread_self = 0.0f,
                                  float fspread_dx = 0.0f);

}
}

#endif // LEGACY_DSP_IO_HELPERS_HPP
