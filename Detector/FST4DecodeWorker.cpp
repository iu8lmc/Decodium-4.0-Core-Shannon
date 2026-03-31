// -*- Mode: C++ -*-
#include "wsjtx_config.h"
#include "Detector/FST4DecodeWorker.hpp"
#include "Detector/FtxFst4Ldpc.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <vector>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QTextStream>

#include "commons.h"
#include "Detector/FortranRuntimeGuard.hpp"

extern "C"
{
  void four2a_ (std::complex<float> a[], int* nfft, int* ndim, int* isign, int* iform);
  void blanker_ (short iwave[], int* nz, int* ndropmax, int* npct, std::complex<float> c_bigfft[]);
  void pctile_ (float x[], int* npts, int* npct, float* xpct);
  void polyfit_ (double x[], double y[], double sigmay[], int* npts, int* nterms, int* mode,
                 double a[], double* chisqr);
  void get_fst4_bitmetrics_ (std::complex<float> cd[], int* nss, float bitmetrics[], float s4[],
                             int* nsync_qual, int* badsync);
  void ftx_pack77_reset_context_ ();
  void ftx_pack77_set_context_ (char const* mycall, char const* hiscall,
                                fortran_charlen_t, fortran_charlen_t);
  void ftx_pack77_pack_ (char const* msg0, int* i3, int* n3, char* c77,
                         char* msgsent, int* success, int* received,
                         fortran_charlen_t, fortran_charlen_t, fortran_charlen_t);
  void ftx_pack77_unpack_ (char const* c77, int* received, char* msgsent, int* success,
                           fortran_charlen_t, fortran_charlen_t);
  void get_fst4_tones_from_bits_ (signed char* msgbits, int* i4tone, int* iwspr);
}

namespace
{
  using Complex = std::complex<float>;

  constexpr int kFst4MaxLines {200};
  constexpr int kDecodedChars {37};
  constexpr int kSyncSymbols {40};
  constexpr int kDataSymbols {120};
  constexpr int kTotalSymbols {kSyncSymbols + kDataSymbols};
  constexpr int kMaxCandidates {200};
  constexpr int kMaxWCalls {100};

  struct RuntimeConfig
  {
    int nsps {720};
    int nmax {15 * RX_SAMPLE_RATE};
    int ndown {18};
    int nfft1 {15 * RX_SAMPLE_RATE};
    int nfft2 {0};
    int nss {40};
    int nspsec {0};
    int nblock {4};
    int jittermax {0};
    bool doK50Decode {false};
    float fs {RX_SAMPLE_RATE};
    float fs2 {0.0f};
    float dt2 {0.0f};
    float baud {0.0f};
    float sigbw {0.0f};
  };

  struct Candidate
  {
    float fc0 {0.0f};
    float detmet {0.0f};
    float fcSynced {-1.0f};
    int isbest {0};
    float base {0.0f};
  };

  struct DecodedLine
  {
    int snr {0};
    float dt {0.0f};
    float freq {0.0f};
    int nap {0};
    float qual {0.0f};
    QByteArray decoded;
    std::array<signed char, 77> bits77 {};
    float fmid {-999.0f};
    float w50 {0.0f};
  };

  struct PersistentState
  {
    bool initialized {false};
    std::array<int, 29> mcq {};
    std::array<int, 19> mrrr {};
    std::array<int, 19> m73 {};
    std::array<int, 19> mrr73 {};
    std::array<int, 77> rvec {};
    std::array<int, 6> nappasses {};
    std::array<std::array<int, 4>, 6> naptypes {};
    std::array<int, 240> apbits {};
    QByteArray mycall0;
    QByteArray hiscall0;
    QStringList wcalls;
    QString wcallsDir;
  };

  struct SyncCache
  {
    std::vector<Complex> csync1;
    std::vector<Complex> csync2;
    std::vector<Complex> csynct1;
    std::vector<Complex> csynct2;
    int nss0 {-1};
    int ntr0 {-1};
    float f0save {std::numeric_limits<float>::quiet_NaN ()};
    float dt {0.0f};
    float fac {0.0f};
  };

  PersistentState& persistent_state ()
  {
    static PersistentState state;
    return state;
  }

  SyncCache& sync_cache ()
  {
    static SyncCache cache;
    return cache;
  }

  QByteArray fixed_field (QByteArray value, int width)
  {
    int const nul = value.indexOf ('\0');
    if (nul >= 0)
      {
        value.truncate (nul);
      }
    value = value.left (width);
    if (value.size () < width)
      {
        value.append (QByteArray (width - value.size (), ' '));
      }
    return value;
  }

  QByteArray trimmed_field (QByteArray value)
  {
    int const nul = value.indexOf ('\0');
    if (nul >= 0)
      {
        value.truncate (nul);
      }
    return value.trimmed ();
  }

  QString format_decode_utc (int nutc)
  {
    if (nutc <= 0)
      {
        return QString {};
      }
    int const width = nutc > 9999 ? 6 : 4;
    return QString::number (nutc).rightJustified (width, QLatin1Char {'0'});
  }

  QString decode_text (QByteArray value)
  {
    value = fixed_field (std::move (value), kDecodedChars);
    return QString::fromLatin1 (value.constData (), value.size ());
  }

  QString build_row (QString const& utcPrefix, DecodedLine const& line, int ntrperiod)
  {
    QString decoded = decode_text (line.decoded);
    if (line.nap != 0 && line.qual < 0.17f && decoded.size () >= kDecodedChars)
      {
        decoded[kDecodedChars - 1] = QLatin1Char {'?'};
      }

    QString const annot = line.nap != 0
        ? QStringLiteral ("a%1").arg (line.nap).leftJustified (2, QLatin1Char {' '})
        : QStringLiteral ("  ");

    QString row = QStringLiteral ("%1%2%3 `  %4 %5")
        .arg (line.snr, 4)
        .arg (line.dt, 5, 'f', 1)
        .arg (qRound (line.freq), 5)
        .arg (decoded)
        .arg (annot);
    if (ntrperiod >= 60 && line.fmid != -999.0f)
      {
        row += QStringLiteral ("%1").arg (line.w50, 6, 'f', line.w50 < 0.95f ? 3 : 2);
      }
    if (!utcPrefix.isEmpty ())
      {
        row.prepend (utcPrefix);
      }
    return row;
  }

  QStringList build_rows (QString const& utcPrefix, std::vector<DecodedLine> const& lines, int ntrperiod)
  {
    QStringList rows;
    rows.reserve (static_cast<int> (lines.size ()));
    for (auto const& line : lines)
      {
        rows << build_row (utcPrefix, line, ntrperiod);
      }
    return rows;
  }

  QString trim_temp_dir (char const* temp_dir)
  {
    if (!temp_dir)
      {
        return {};
      }
    size_t length = 0;
    while (length < 500 && temp_dir[length] != '\0')
      {
        ++length;
      }
    return QString::fromLocal8Bit (temp_dir, static_cast<int> (length));
  }

  QString format_fst4_decoded_file_line (int nutc, DecodedLine const& line)
  {
    int const utc_width = nutc > 9999 ? 6 : 4;
    return QStringLiteral ("%1%2%3%4%5%6   %7 FST4")
        .arg (nutc, utc_width, 10, QLatin1Char {'0'})
        .arg (0, 4)
        .arg (line.snr, 5)
        .arg (line.dt, 6, 'f', 1)
        .arg (line.freq, 8, 'f', 0)
        .arg (0, 4)
        .arg (decode_text (line.decoded));
  }

  int emit_fst4_results (int nutc, int ntrperiod, int nagain,
                         char const* temp_dir, std::vector<DecodedLine> const& lines)
  {
    QString const utcPrefix = format_decode_utc (nutc);
    QString const tempDirPath = trim_temp_dir (temp_dir);
    QString const decodedPath = tempDirPath.isEmpty ()
        ? QStringLiteral ("decoded.txt")
        : QDir {tempDirPath}.filePath (QStringLiteral ("decoded.txt"));

    QFile decodedFile {decodedPath};
    if (decodedFile.open (QIODevice::WriteOnly | QIODevice::Text
                          | (nagain != 0 ? QIODevice::Append : QIODevice::Truncate)))
      {
        QTextStream decodedStream {&decodedFile};
        QTextStream stdoutStream {stdout, QIODevice::WriteOnly};
        for (auto const& line : lines)
          {
            stdoutStream << build_row (utcPrefix, line, ntrperiod) << '\n';
            decodedStream << format_fst4_decoded_file_line (nutc, line) << '\n';
          }
        stdoutStream.flush ();
        decodedStream.flush ();
      }
    else
      {
        QTextStream stdoutStream {stdout, QIODevice::WriteOnly};
        for (auto const& line : lines)
          {
            stdoutStream << build_row (utcPrefix, line, ntrperiod) << '\n';
          }
        stdoutStream.flush ();
      }

    return static_cast<int> (lines.size ());
  }

  RuntimeConfig config_for_period (int ntrperiod, int ndepth)
  {
    RuntimeConfig cfg;
    if (ntrperiod == 15)
      {
        cfg.nsps = 720;
        cfg.nmax = 15 * RX_SAMPLE_RATE;
        cfg.ndown = 18;
        cfg.nfft1 = (cfg.nmax / cfg.ndown) * cfg.ndown;
      }
    else if (ntrperiod == 30)
      {
        cfg.nsps = 1680;
        cfg.nmax = 30 * RX_SAMPLE_RATE;
        cfg.ndown = 42;
        cfg.nfft1 = 359856;
      }
    else if (ntrperiod == 60)
      {
        cfg.nsps = 3888;
        cfg.nmax = 60 * RX_SAMPLE_RATE;
        cfg.ndown = 108;
        cfg.nfft1 = 7500 * 96;
      }
    else if (ntrperiod == 120)
      {
        cfg.nsps = 8200;
        cfg.nmax = 120 * RX_SAMPLE_RATE;
        cfg.ndown = 205;
        cfg.nfft1 = 7200 * 200;
      }
    else if (ntrperiod == 300)
      {
        cfg.nsps = 21504;
        cfg.nmax = 300 * RX_SAMPLE_RATE;
        cfg.ndown = 512;
        cfg.nfft1 = 7020 * 512;
      }
    else if (ntrperiod == 900)
      {
        cfg.nsps = 66560;
        cfg.nmax = 900 * RX_SAMPLE_RATE;
        cfg.ndown = 1664;
        cfg.nfft1 = 6480 * 1664;
      }
    else
      {
        cfg.nsps = 134400;
        cfg.nmax = 1800 * RX_SAMPLE_RATE;
        cfg.ndown = 3360;
        cfg.nfft1 = 6426 * 3360;
      }

    cfg.nfft2 = cfg.nfft1 / cfg.ndown;
    cfg.nfft1 = cfg.nfft2 * cfg.ndown;
    cfg.nss = cfg.nsps / cfg.ndown;
    cfg.fs2 = static_cast<float> (RX_SAMPLE_RATE) / cfg.ndown;
    cfg.nspsec = static_cast<int> (cfg.fs2);
    cfg.dt2 = 1.0f / cfg.fs2;
    cfg.baud = static_cast<float> (RX_SAMPLE_RATE) / cfg.nsps;
    cfg.sigbw = 4.0f * cfg.baud;
    cfg.nblock = 4;
    if (ndepth >= 3)
      {
        cfg.jittermax = 2;
        cfg.doK50Decode = true;
      }
    else if (ndepth == 2)
      {
        cfg.jittermax = 2;
        cfg.doK50Decode = false;
      }
    else
      {
        cfg.jittermax = 0;
        cfg.doK50Decode = false;
      }
    return cfg;
  }

  void initialize_state (PersistentState& state)
  {
    if (state.initialized)
      {
        return;
      }

    state.rvec = {{
      0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0,
      1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1,
      0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1
    }};

    std::array<int, 29> mcqBase {{
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0
    }};
    std::array<int, 19> mrrrBase {{
      0,1,1,1,1,1,1,0,1,0,0,1,0,0,1,0,0,0,1
    }};
    std::array<int, 19> m73Base {{
      0,1,1,1,1,1,1,0,1,0,0,1,0,1,0,0,0,0,1
    }};
    std::array<int, 19> mrr73Base {{
      0,1,1,1,1,1,1,0,0,1,1,1,0,1,0,1,0,0,1
    }};

    for (int i = 0; i < 29; ++i)
      {
        state.mcq[i] = 2 * ((mcqBase[i] + state.rvec[i]) & 1) - 1;
      }
    for (int i = 0; i < 19; ++i)
      {
        state.mrrr[i] = 2 * ((mrrrBase[i] + state.rvec[i + 58]) & 1) - 1;
        state.m73[i] = 2 * ((m73Base[i] + state.rvec[i + 58]) & 1) - 1;
        state.mrr73[i] = 2 * ((mrr73Base[i] + state.rvec[i + 58]) & 1) - 1;
      }

    state.nappasses = {{2, 2, 2, 2, 2, 3}};
    state.naptypes[0] = {{1, 2, 0, 0}};
    state.naptypes[1] = {{2, 3, 0, 0}};
    state.naptypes[2] = {{2, 3, 0, 0}};
    state.naptypes[3] = {{3, 6, 0, 0}};
    state.naptypes[4] = {{3, 6, 0, 0}};
    state.naptypes[5] = {{3, 1, 2, 0}};
    state.apbits.fill (0);
    state.apbits[0] = 99;
    state.apbits[29] = 99;
    state.initialized = true;
  }

  QString resolved_data_dir (QByteArray const& requested)
  {
    QString dir = QString::fromLocal8Bit (trimmed_field (requested));
    if (dir.isEmpty ())
      {
        dir = QDir::currentPath ();
      }
    return dir;
  }

  QString fst4w_calls_path (QString const& dataDir)
  {
    return QDir {dataDir}.filePath (QStringLiteral ("fst4w_calls.txt"));
  }

  void load_wcalls (PersistentState& state, QString const& dataDir)
  {
    if (state.wcallsDir == dataDir)
      {
        return;
      }

    state.wcallsDir = dataDir;
    state.wcalls.clear ();

    QFile file {fst4w_calls_path (dataDir)};
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
      {
        return;
      }

    QTextStream in {&file};
    while (!in.atEnd () && state.wcalls.size () < kMaxWCalls)
      {
        QString line = in.readLine ().trimmed ();
        if (!line.isEmpty ())
          {
            state.wcalls << line.left (20);
          }
      }
  }

  void save_wcalls (PersistentState const& state, QString const& dataDir)
  {
    QFile file {fst4w_calls_path (dataDir)};
    if (!file.open (QIODevice::WriteOnly | QIODevice::Text))
      {
        return;
      }

    QTextStream out {&file};
    for (auto const& line : state.wcalls)
      {
        out << line.left (20) << '\n';
      }
  }

  QByteArray first_two_words (QByteArray const& message)
  {
    QList<QByteArray> const rawWords = trimmed_field (message).split (' ');
    QList<QByteArray> words;
    words.reserve (rawWords.size ());
    for (auto const& word : rawWords)
      {
        if (!word.isEmpty ())
          {
            words << word;
          }
      }
    if (words.size () < 2)
      {
        return {};
      }
    return (words.at (0) + ' ' + words.at (1)).left (20);
  }

  bool contains_known_fst4w_call (PersistentState const& state, QByteArray const& message)
  {
    QByteArray const trimmed = trimmed_field (message);
    for (auto const& call : state.wcalls)
      {
        if (trimmed.contains (call.toLatin1 ()))
          {
            return true;
          }
      }
    return false;
  }

  void maybe_save_fst4w_call (PersistentState& state, QByteArray const& message, bool* changed)
  {
    QByteArray wpart = first_two_words (message);
    if (wpart.isEmpty () || wpart.contains ('/') || wpart.contains ('<'))
      {
        return;
      }
    QString const entry = QString::fromLatin1 (wpart);
    if (state.wcalls.contains (entry))
      {
        return;
      }
    if (state.wcalls.size () < kMaxWCalls)
      {
        state.wcalls << entry;
      }
    else
      {
        state.wcalls.pop_front ();
        state.wcalls << entry;
      }
    if (changed)
      {
        *changed = true;
      }
  }

  bool pack77_bits (QString const& message, int received, int* i3Out, int* n3Out,
                    std::array<int, 77>* bitsOut, QByteArray* msgsentOut)
  {
    QByteArray const msg0 = fixed_field (message.toLatin1 (), 37);
    std::array<char, 77> c77 {};
    std::array<char, 37> msgsent {};
    int i3 = 0;
    int n3 = 0;
    int success = 0;
    int receivedValue = received;
    ftx_pack77_pack_ (msg0.constData (), &i3, &n3, c77.data (), msgsent.data (), &success, &receivedValue,
                      static_cast<fortran_charlen_t> (37),
                      static_cast<fortran_charlen_t> (77),
                      static_cast<fortran_charlen_t> (37));
    if (i3Out) *i3Out = i3;
    if (n3Out) *n3Out = n3;
    if (msgsentOut) *msgsentOut = QByteArray {msgsent.data (), 37};
    if (!success)
      {
        return false;
      }
    if (bitsOut)
      {
        for (int i = 0; i < 77; ++i)
          {
            (*bitsOut)[i] = c77[i] == '1' ? 1 : 0;
          }
      }
    return true;
  }

  void update_ap_bits (PersistentState& state, QByteArray const& mycallRaw, QByteArray const& hiscallRaw)
  {
    initialize_state (state);
    QByteArray const mycall = trimmed_field (mycallRaw);
    QByteArray const hiscall = trimmed_field (hiscallRaw);
    if (mycall == state.mycall0 && hiscall == state.hiscall0)
      {
        return;
      }

    state.apbits.fill (0);
    state.apbits[0] = 99;
    state.apbits[29] = 99;

    if (mycall.size () >= 3)
      {
        QByteArray hiscall0 = hiscall;
        bool nohiscall = false;
        if (hiscall0.size () < 3)
          {
            hiscall0 = mycall;
            nohiscall = true;
          }

        QString const message = QStringLiteral ("%1 %2 RR73")
            .arg (QString::fromLatin1 (mycall), QString::fromLatin1 (hiscall0));
        int i3 = 0;
        int n3 = 0;
        std::array<int, 77> bits {};
        QByteArray msgsent;
        if (pack77_bits (message, 0, &i3, &n3, &bits, &msgsent)
            && i3 == 1
            && msgsent == fixed_field (message.toLatin1 (), 37))
          {
            for (int i = 0; i < 77; ++i)
              {
                int const bit = (bits[i] + state.rvec[i]) & 1;
                state.apbits[i] = 2 * bit - 1;
              }
            if (nohiscall)
              {
                state.apbits[29] = 99;
              }
          }
      }

    state.mycall0 = mycall;
    state.hiscall0 = hiscall;
  }

  float snr_cal_factor (int ntrperiod)
  {
    switch (ntrperiod)
      {
      case 15: return 800.0f;
      case 30: return 600.0f;
      case 60: return 430.0f;
      case 120: return 390.0f;
      case 300: return 340.0f;
      case 900: return 320.0f;
      case 1800: return 320.0f;
      default: return 430.0f;
      }
  }

  bool fst4_baseline_cpp (std::vector<float> const& s, int np, int ia, int ib, int npct, std::vector<float>& sbase)
  {
    if (np <= 0 || ia < 1 || ib > np || ib < ia)
      {
        return false;
      }

    int const nseg = 8;
    int const nlen = (ib - ia + 1) / nseg;
    if (nlen <= 0)
      {
        return false;
      }
    int const i0 = (ib - ia + 1) / 2;

    std::vector<float> sw (static_cast<size_t> (np), 0.0f);
    for (int i = ia; i <= ib; ++i)
      {
        sw[static_cast<size_t> (i - 1)] = 10.0f * std::log10 (std::max (s[static_cast<size_t> (i - 1)], 1.0e-30f));
      }

    std::vector<double> x;
    std::vector<double> y;
    x.reserve (1000);
    y.reserve (1000);
    for (int n = 1; n <= nseg; ++n)
      {
        int const ja = ia + (n - 1) * nlen;
        int const jb = ja + nlen - 1;
        if (jb > ib)
          {
            break;
          }

        std::vector<float> segment;
        segment.reserve (static_cast<size_t> (nlen));
        for (int i = ja; i <= jb; ++i)
          {
            segment.push_back (sw[static_cast<size_t> (i - 1)]);
          }
        int npts = nlen;
        float base = 0.0f;
        pctile_ (segment.data (), &npts, &npct, &base);
        for (int i = ja; i <= jb; ++i)
          {
            if (sw[static_cast<size_t> (i - 1)] <= base && x.size () < 1000)
              {
                x.push_back (static_cast<double> (i - i0));
                y.push_back (sw[static_cast<size_t> (i - 1)]);
              }
          }
      }

    if (x.size () < 3)
      {
        return false;
      }
    std::vector<double> sigmay = y;
    std::array<double, 5> coeffs {};
    int kz = static_cast<int> (x.size ());
    int nterms = 3;
    int mode = 0;
    double chisqr = 0.0;
    polyfit_ (x.data (), y.data (), sigmay.data (), &kz, &nterms, &mode, coeffs.data (), &chisqr);

    sbase.assign (static_cast<size_t> (np), 1.0f);
    for (int i = ia; i <= ib; ++i)
      {
        double const t = static_cast<double> (i - i0);
        double const db = coeffs[0] + t * (coeffs[1] + t * coeffs[2]) + 0.2;
        sbase[static_cast<size_t> (i - 1)] = static_cast<float> (std::pow (10.0, db / 10.0));
      }
    return true;
  }

  std::array<signed char, 77> fst4w_bits77 (std::array<signed char, 74> const& message74)
  {
    std::array<signed char, 77> bits {};
    for (int i = 0; i < 50; ++i)
      {
        bits[static_cast<size_t> (i)] = message74[static_cast<size_t> (i)];
      }
    static constexpr std::array<signed char, 27> suffix {{
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0
    }};
    std::copy (suffix.begin (), suffix.end (), bits.begin () + 50);
    return bits;
  }

  std::vector<Candidate> get_candidates_fst4_cpp (std::vector<Complex> const& bigfft,
                                                   RuntimeConfig const& cfg,
                                                   int hmod, float fa, float fb,
                                                   int nfa, int nfb, float minsync)
  {
    int const nh1 = cfg.nfft1 / 2;
    float const df1 = cfg.fs / cfg.nfft1;
    float const df2 = cfg.baud / 2.0f;
    int const nd = static_cast<int> (df2 / df1);
    int const ndh = nd / 2;
    int ia = qRound (std::max (100.0f, fa) / df2);
    int ib = qRound (std::min (4800.0f, fb) / df2);
    int ina = qRound (std::max (100.0f, static_cast<float> (nfa)) / df2);
    int inb = qRound (std::min (4800.0f, static_cast<float> (nfb)) / df2);
    if (ia < ina) ia = ina;
    if (ib > inb) ib = inb;

    int const nnw = qRound (48000.0f * cfg.nsps * 2.0f / cfg.fs);
    std::vector<float> s (std::max (nnw, 1), 0.0f);
    for (int i = ina; i <= inb; ++i)
      {
        int const j0 = qRound (i * df2 / df1);
        float sum = 0.0f;
        for (int j = j0 - ndh; j <= j0 + ndh; ++j)
          {
            if (j >= 0 && j <= nh1)
              {
                sum += std::norm (bigfft[static_cast<size_t> (j)]);
              }
          }
        s[static_cast<size_t> (i - 1)] = sum;
      }

    ina = std::max (ina, 1 + 3 * hmod);
    inb = std::min (inb, nnw - 3 * hmod);
    std::vector<float> s2 (std::max (nnw, 1), 0.0f);
    std::vector<float> sbase (std::max (nnw, 1), 0.0f);
    for (int i = ina; i <= inb; ++i)
      {
        s2[static_cast<size_t> (i - 1)] =
            s[static_cast<size_t> (i - 1 - 3 * hmod)] +
            s[static_cast<size_t> (i - 1 - hmod)] +
            s[static_cast<size_t> (i - 1 + hmod)] +
            s[static_cast<size_t> (i - 1 + 3 * hmod)];
      }

    int npctile = 30;
    int baselineStart = ina + 3 * hmod;
    int baselineEnd = inb - 3 * hmod;
    if (!fst4_baseline_cpp (s2, nnw, baselineStart, baselineEnd, npctile, sbase))
      {
        return {};
      }
    for (int i = ina; i <= inb; ++i)
      {
        if (sbase[static_cast<size_t> (i - 1)] <= 0.0f)
          {
            return {};
          }
      }
    for (int i = ina; i <= inb; ++i)
      {
        s2[static_cast<size_t> (i - 1)] /= sbase[static_cast<size_t> (i - 1)];
      }

    std::vector<Candidate> candidates;
    candidates.reserve (kMaxCandidates);
    if (ia < 3) ia = 3;
    if (ib > nnw - 2) ib = nnw - 2;

    static constexpr std::array<float, 7> xdb {{0.25f, 0.50f, 0.75f, 1.0f, 0.75f, 0.50f, 0.25f}};
    while (static_cast<int> (candidates.size ()) < kMaxCandidates)
      {
        int iploc = ia;
        float pval = -1.0f;
        for (int i = ia; i <= ib; ++i)
          {
            float const value = s2[static_cast<size_t> (i - 1)];
            if (value > pval)
              {
                pval = value;
                iploc = i;
              }
          }
        if (pval < minsync)
          {
            break;
          }
        for (int offset = -3; offset <= 3; ++offset)
          {
            int const k = iploc + 2 * hmod * offset;
            if (k >= ia && k <= ib)
              {
                float& slot = s2[static_cast<size_t> (k - 1)];
                slot = std::max (0.0f, slot - 0.9f * pval * xdb[static_cast<size_t> (offset + 3)]);
              }
          }
        Candidate candidate;
        candidate.fc0 = df2 * iploc;
        candidate.detmet = pval;
        candidate.base = sbase[static_cast<size_t> (iploc - 1)];
        candidates.push_back (candidate);
      }

    return candidates;
  }

  void fst4_downsample_cpp (std::vector<Complex> const& bigfft, RuntimeConfig const& cfg,
                            float f0, std::vector<Complex>& out)
  {
    int const nh1 = cfg.nfft1 / 2;
    float const df = cfg.fs / cfg.nfft1;
    int const i0 = qRound (f0 / df);
    int const ih = qRound ((f0 + 1.3f * cfg.sigbw / 2.0f) / df);
    int const nbw = ih - i0 + 1;

    out.assign (static_cast<size_t> (cfg.nfft2), Complex {});
    if (i0 >= 0 && i0 <= nh1)
      {
        out[0] = bigfft[static_cast<size_t> (i0)];
      }
    for (int i = 1; i <= nbw; ++i)
      {
        if (i0 + i <= nh1 && i < cfg.nfft2)
          {
            out[static_cast<size_t> (i)] = bigfft[static_cast<size_t> (i0 + i)];
          }
        if (i0 - i >= 0 && cfg.nfft2 - i >= 0)
          {
            out[static_cast<size_t> (cfg.nfft2 - i)] = bigfft[static_cast<size_t> (i0 - i)];
          }
      }
    float const scale = 1.0f / cfg.nfft2;
    for (auto& value : out)
      {
        value *= scale;
      }
    int nfft = cfg.nfft2;
    int ndim = 1;
    int isign = 1;
    int iform = 1;
    four2a_ (out.data (), &nfft, &ndim, &isign, &iform);
  }

  Complex correlate (std::vector<Complex> const& data, int dataOffset,
                     std::vector<Complex> const& sync, int syncOffset, int count)
  {
    Complex sum {};
    for (int i = 0; i < count; ++i)
      {
        sum += data[static_cast<size_t> (dataOffset + i)] * std::conj (sync[static_cast<size_t> (syncOffset + i)]);
      }
    return sum;
  }

  float sync_fst4_cpp (std::vector<Complex> const& cd0, int i0, float f0, int hmod,
                       int ncoh, int np, int nss, int ntr, float fs)
  {
    static constexpr std::array<int, 8> isyncword1 {{0,1,3,2,1,0,2,3}};
    static constexpr std::array<int, 8> isyncword2 {{2,3,1,0,3,2,0,1}};

    SyncCache& cache = sync_cache ();
    int const nz = 8 * nss;
    if (cache.nss0 != nss || cache.ntr0 != ntr)
      {
        cache.csync1.assign (static_cast<size_t> (nz), Complex {});
        cache.csync2.assign (static_cast<size_t> (nz), Complex {});
        cache.csynct1.assign (static_cast<size_t> (nz), Complex {});
        cache.csynct2.assign (static_cast<size_t> (nz), Complex {});
        float const twopi = 8.0f * std::atan (1.0f);
        cache.dt = 1.0f / fs;
        float phi1 = 0.0f;
        float phi2 = 0.0f;
        int k = 0;
        for (int i = 0; i < 8; ++i)
          {
            float const dphi1 = twopi * hmod * (isyncword1[static_cast<size_t> (i)] - 1.5f) / nss;
            float const dphi2 = twopi * hmod * (isyncword2[static_cast<size_t> (i)] - 1.5f) / nss;
            for (int j = 0; j < nss; ++j, ++k)
              {
                cache.csync1[static_cast<size_t> (k)] = Complex {std::cos (phi1), std::sin (phi1)};
                cache.csync2[static_cast<size_t> (k)] = Complex {std::cos (phi2), std::sin (phi2)};
                phi1 = std::fmod (phi1 + dphi1, twopi);
                phi2 = std::fmod (phi2 + dphi2, twopi);
              }
          }
        cache.fac = 1.0f / (8.0f * nss);
        cache.nss0 = nss;
        cache.ntr0 = ntr;
        cache.f0save = std::numeric_limits<float>::quiet_NaN ();
      }

    if (!(f0 == cache.f0save))
      {
        float const twopi = 8.0f * std::atan (1.0f);
        float const dphi = twopi * f0 * cache.dt;
        float phi = 0.0f;
        for (int i = 0; i < nz; ++i)
          {
            Complex const ctwk {std::cos (phi), std::sin (phi)};
            cache.csynct1[static_cast<size_t> (i)] = ctwk * cache.csync1[static_cast<size_t> (i)];
            cache.csynct2[static_cast<size_t> (i)] = ctwk * cache.csync2[static_cast<size_t> (i)];
            phi = std::fmod (phi + dphi, twopi);
          }
        cache.f0save = f0;
      }

    int const i1 = i0;
    int const i2 = i0 + 38 * nss;
    int const i3 = i0 + 76 * nss;
    int const i4 = i0 + 114 * nss;
    int const i5 = i0 + 152 * nss;
    float s1 = 0.0f;
    float s2 = 0.0f;
    float s3 = 0.0f;
    float s4 = 0.0f;
    float s5 = 0.0f;

    if (ncoh > 0)
      {
        int const nsec = 8 / ncoh;
        int const span = ncoh * nss;
        for (int i = 0; i < nsec; ++i)
          {
            int const is = i * span;
            Complex z1 {};
            if (i1 + is >= 1 && i1 + is + span - 1 < np)
              {
                z1 = correlate (cd0, i1 + is, cache.csynct1, is, span);
              }
            Complex const z2 = correlate (cd0, i2 + is, cache.csynct2, is, span);
            Complex const z3 = correlate (cd0, i3 + is, cache.csynct1, is, span);
            Complex const z4 = correlate (cd0, i4 + is, cache.csynct2, is, span);
            Complex z5 {};
            if (i5 + is + span - 1 < np)
              {
                z5 = correlate (cd0, i5 + is, cache.csynct1, is, span);
              }
            s1 += std::abs (z1) / nz;
            s2 += std::abs (z2) / nz;
            s3 += std::abs (z3) / nz;
            s4 += std::abs (z4) / nz;
            s5 += std::abs (z5) / nz;
          }
      }
    else
      {
        int const nsub = -ncoh;
        int const nps = nss / nsub;
        for (int i = 0; i < 8; ++i)
          {
            for (int isub = 0; isub < nsub; ++isub)
              {
                int const is = i * nss + isub * nps;
                Complex z1 {};
                if (i1 + is >= 1 && i1 + is + nps - 1 < np)
                  {
                    z1 = correlate (cd0, i1 + is, cache.csynct1, is, nps);
                  }
                Complex const z2 = correlate (cd0, i2 + is, cache.csynct2, is, nps);
                Complex const z3 = correlate (cd0, i3 + is, cache.csynct1, is, nps);
                Complex const z4 = correlate (cd0, i4 + is, cache.csynct2, is, nps);
                Complex z5 {};
                if (i5 + is + nps - 1 < np)
                  {
                    z5 = correlate (cd0, i5 + is, cache.csynct1, is, nps);
                  }
                s1 += std::abs (z1) / (8.0f * nss);
                s2 += std::abs (z2) / (8.0f * nss);
                s3 += std::abs (z3) / (8.0f * nss);
                s4 += std::abs (z4) / (8.0f * nss);
                s5 += std::abs (z5) / (8.0f * nss);
              }
          }
      }

    return s1 + s2 + s3 + s4 + s5;
  }

  void fst4_sync_search_cpp (std::vector<Complex> const& c2, RuntimeConfig const& cfg,
                             int hmod, int ntrperiod, float emedelay,
                             float& sbest, float& fcbest, int& isbest)
  {
    int is0 = 0;
    int ishw = 0;
    if (emedelay < 0.1f)
      {
        is0 = qRound (1.5f * cfg.nspsec);
        ishw = qRound (1.5f * cfg.nspsec);
      }
    else
      {
        is0 = qRound ((emedelay + 1.0f) * cfg.nspsec);
        ishw = qRound (1.5f * cfg.nspsec);
      }

    float fc1 = 0.0f;
    sbest = -1.0e30f;
    for (int ifreq = -12; ifreq <= 12; ++ifreq)
      {
        float const fc = fc1 + 0.1f * cfg.baud * ifreq;
        for (int istart = std::max (1, is0 - ishw); istart <= is0 + ishw; istart += 4 * hmod)
          {
            float const sync = sync_fst4_cpp (c2, istart, fc, hmod, 8, cfg.nfft2, cfg.nss, ntrperiod, cfg.fs2);
            if (sync > sbest)
              {
                sbest = sync;
                fcbest = fc;
                isbest = istart;
              }
          }
      }

    fc1 = fcbest;
    is0 = isbest;
    ishw = 4 * hmod;
    sbest = 0.0f;
    for (int ifreq = -7; ifreq <= 7; ++ifreq)
      {
        float const fc = fc1 + 0.02f * cfg.baud * ifreq;
        for (int istart = std::max (1, is0 - ishw); istart <= is0 + ishw; istart += hmod)
          {
            float const sync = sync_fst4_cpp (c2, istart, fc, hmod, 8, cfg.nfft2, cfg.nss, ntrperiod, cfg.fs2);
            if (sync > sbest)
              {
                sbest = sync;
                fcbest = fc;
                isbest = istart;
              }
          }
      }
  }

  QByteArray unpack_77bits (std::array<signed char, 77> const& message77, bool* okOut = nullptr)
  {
    std::array<char, 77> c77 {};
    for (int i = 0; i < 77; ++i)
      {
        c77[static_cast<size_t> (i)] = message77[static_cast<size_t> (i)] != 0 ? '1' : '0';
      }

    int received = 1;
    int success = 0;
    std::array<char, 37> msgsent {};
    ftx_pack77_unpack_ (c77.data (), &received, msgsent.data (), &success,
                        static_cast<fortran_charlen_t> (77),
                        static_cast<fortran_charlen_t> (37));
    if (okOut) *okOut = success != 0;
    return success != 0 ? QByteArray {msgsent.data (), 37} : QByteArray {};
  }

  QByteArray unpack_fst4w_50bit (std::array<signed char, 74> const& message74, bool printHash22, bool* okOut = nullptr)
  {
    std::array<char, 77> c77 {};
    for (int i = 0; i < 50; ++i)
      {
        c77[static_cast<size_t> (i)] = message74[static_cast<size_t> (i)] != 0 ? '1' : '0';
      }
    static constexpr char suffix[] {"000000000000000000000110000"};
    std::copy_n (suffix, 27, c77.data () + 50);

    int received = 1;
    int success = 0;
    std::array<char, 37> msgsent {};
    ftx_pack77_unpack_ (c77.data (), &received, msgsent.data (), &success,
                        static_cast<fortran_charlen_t> (77),
                        static_cast<fortran_charlen_t> (37));
    if (!success)
      {
        if (okOut) *okOut = false;
        return {};
      }

    QByteArray msg {msgsent.data (), 37};
    if (printHash22 && msg.contains ("<...>"))
      {
        int n22tmp = 0;
        for (int i = 0; i < 22; ++i)
          {
            n22tmp = (n22tmp << 1) | (c77[static_cast<size_t> (i)] == '1' ? 1 : 0);
          }
        int const firstSpace = msg.indexOf (' ');
        QByteArray tail = firstSpace >= 0 ? trimmed_field (msg.mid (firstSpace + 1)) : QByteArray {};
        QByteArray replacement = QByteArrayLiteral ("<") + QByteArray::number (n22tmp).rightJustified (7, '0')
            + QByteArrayLiteral (">");
        if (!tail.isEmpty ())
          {
            replacement += ' ';
            replacement += tail;
          }
        msg = fixed_field (replacement, 37);
      }

    if (okOut) *okOut = true;
    return msg;
  }

  float s4_value (std::array<float, 4 * kTotalSymbols> const& s4, int tone, int symbolIndex1Based)
  {
    return s4[static_cast<size_t> (tone + 4 * (symbolIndex1Based - 1))];
  }

  void append_unique_line (std::vector<DecodedLine>& lines, std::vector<QByteArray>& seen, DecodedLine line)
  {
    line.decoded = fixed_field (line.decoded, 37);
    auto const found = std::find (seen.begin (), seen.end (), line.decoded);
    if (found != seen.end ())
      {
        size_t const index = static_cast<size_t> (std::distance (seen.begin (), found));
        if (index < lines.size ())
          {
            DecodedLine& existing = lines[index];
            bool const replace = line.snr > existing.snr
                || (line.snr == existing.snr && line.qual > existing.qual)
                || (line.snr == existing.snr && line.qual == existing.qual && line.w50 < existing.w50);
            if (replace)
              {
                existing = std::move (line);
              }
          }
        return;
      }
    seen.push_back (line.decoded);
    if (static_cast<int> (lines.size ()) < kFst4MaxLines)
      {
        lines.push_back (std::move (line));
      }
  }
}

namespace decodium
{
namespace fst4
{

std::vector<DecodedLine> decodeFst4Lines (DecodeRequest const& request)
{
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};

  RuntimeConfig const cfg = config_for_period (qBound (15, request.ntrperiod, 1800),
                                               qBound (1, request.ndepth, 3));
  QVector<short> localAudio (cfg.nmax);
  int const copyCount = std::min (request.audio.size (), cfg.nmax);
  if (copyCount > 0)
    {
      std::copy_n (request.audio.constBegin (), copyCount, localAudio.begin ());
    }

  QByteArray const mycall = fixed_field (request.mycall, 12);
  QByteArray const hiscall = fixed_field (request.hiscall, 12);
  ftx_pack77_reset_context_ ();
  ftx_pack77_set_context_ (mycall.constData (), hiscall.constData (),
                           static_cast<fortran_charlen_t> (12),
                           static_cast<fortran_charlen_t> (12));

  PersistentState& state = persistent_state ();
  initialize_state (state);
  update_ap_bits (state, mycall, hiscall);

  QString const dataDir = resolved_data_dir (request.dataDir);
  if (request.iwspr != 0)
    {
      load_wcalls (state, dataDir);
    }

  bool const singleDecode = (request.nexp_decode & 32) != 0;
  int const nb = request.nexp_decode / 256 - 3;
  int npct = nb >= 0 ? nb : 0;
  int inb1 = 20;
  int inb2 = 5;
  if (nb == -1)
    {
      inb2 = 5;
    }
  else if (nb == -2)
    {
      inb2 = 2;
    }
  else if (nb == -3)
    {
      inb2 = 1;
    }
  else
    {
      inb1 = 0;
    }

  int nfa = qBound (0, request.nfa, 5000);
  int nfb = qMax (nfa + 50, qBound (0, request.nfb, 5000));
  int const nfqso = qBound (0, request.nfqso, 5000);
  int const ntol = qMax (0, request.ntol);
  int const hmod = 1;

  float fa = 0.0f;
  float fb = 0.0f;
  if (request.iwspr != 0)
    {
      nfa = std::max (100, nfqso - ntol - 100);
      nfb = std::min (4800, nfqso + ntol + 100);
      fa = std::max (100.0f, nfqso + 1.5f * cfg.baud - ntol);
      fb = std::min (4800.0f, nfqso + 1.5f * cfg.baud + ntol);
    }
  else
    {
      fa = std::max (100.0f, nfa + 1.5f * cfg.baud);
      fb = std::min (4800.0f, nfb + 1.5f * cfg.baud);
      nfa = std::max (100, nfa - 100);
      nfb = std::min (4800, nfb + 100);
      if (singleDecode)
        {
          fa = std::max (100.0f, nfa + 1.5f * cfg.baud);
          fb = std::min (4800.0f, nfb + 1.5f * cfg.baud);
        }
    }

  std::vector<DecodedLine> rows;
  std::vector<QByteArray> seen;
  bool newCallsign = false;
  float const minsync = request.ntrperiod == 15 ? 1.15f : 1.20f;

  std::vector<Complex> bigfft (static_cast<size_t> (cfg.nfft1 / 2 + 1));
  std::vector<Complex> c2;
  std::vector<Complex> cframe (static_cast<size_t> (kTotalSymbols * cfg.nss));

  for (int inb = 0; inb <= inb1; inb += inb2)
    {
      if (nb < 0)
        {
          npct = inb;
        }

      int nz = cfg.nfft1;
      int ndropmax = 1;
      blanker_ (localAudio.data (), &nz, &ndropmax, &npct, bigfft.data ());
      int nfft = cfg.nfft1;
      int ndim = 1;
      int isign = -1;
      int iform = 0;
      four2a_ (bigfft.data (), &nfft, &ndim, &isign, &iform);

      auto candidates = get_candidates_fst4_cpp (bigfft, cfg, hmod, fa, fb, nfa, nfb, minsync);
      for (auto& candidate : candidates)
        {
          if (request.iwspr == 0 && nb < 0 && npct != 0
              && std::abs (candidate.fc0 - (nfqso + 1.5f * cfg.baud)) > ntol)
            {
              continue;
            }

          float sbest = 0.0f;
          float fcbest = 0.0f;
          int isbest = 0;
          fst4_downsample_cpp (bigfft, cfg, candidate.fc0, c2);
          fst4_sync_search_cpp (c2, cfg, hmod, request.ntrperiod, request.emedelay,
                                sbest, fcbest, isbest);
          candidate.fcSynced = candidate.fc0 + fcbest;
          candidate.isbest = isbest;
        }

      for (size_t i = 0; i < candidates.size (); ++i)
        {
          if (candidates[i].fcSynced <= 0.0f)
            {
              continue;
            }
          for (size_t j = i + 1; j < candidates.size (); ++j)
            {
              if (candidates[j].fcSynced > 0.0f
                  && std::abs (candidates[j].fcSynced - candidates[i].fcSynced) < 0.10f * cfg.baud
                  && std::abs (candidates[j].isbest - candidates[i].isbest) <= 2)
                {
                  candidates[j].fcSynced = -1.0f;
                }
            }
        }

      std::vector<Candidate> deduped;
      deduped.reserve (candidates.size ());
      for (auto const& candidate : candidates)
        {
          if (candidate.fcSynced > 0.0f)
            {
              deduped.push_back (candidate);
            }
        }

      std::vector<Candidate> ordered;
      ordered.reserve (deduped.size ());
      if (request.iwspr == 0 && !singleDecode)
        {
          for (auto const& candidate : deduped)
            {
              if (std::abs (candidate.fcSynced - (nfqso + 1.5f * cfg.baud)) <= 20.0f)
                {
                  ordered.push_back (candidate);
                }
            }
          for (auto const& candidate : deduped)
            {
              if (std::abs (candidate.fcSynced - (nfqso + 1.5f * cfg.baud)) > 20.0f)
                {
                  ordered.push_back (candidate);
                }
            }
        }
      else
        {
          ordered = std::move (deduped);
        }

      for (auto const& candidate : ordered)
        {
          bool candidateDone = false;
          fst4_downsample_cpp (bigfft, cfg, candidate.fcSynced, c2);
          for (int jitterIndex = 0; jitterIndex <= cfg.jittermax && !candidateDone; ++jitterIndex)
            {
              int ioffset = 0;
              if (jitterIndex == 1) ioffset = 1;
              if (jitterIndex == 2) ioffset = -1;
              int const is0 = candidate.isbest + ioffset;
              int const iend = is0 + kTotalSymbols * cfg.nss - 1;
              if (is0 < 0 || iend >= cfg.nfft2)
                {
                  continue;
                }
              std::copy_n (c2.begin () + is0, kTotalSymbols * cfg.nss, cframe.begin ());

              std::array<float, 320 * 4> bitmetrics {};
              std::array<float, 4 * kTotalSymbols> s4 {};
              int nsyncQual = 0;
              int badsync = 0;
              int nss = cfg.nss;
              get_fst4_bitmetrics_ (cframe.data (), &nss, bitmetrics.data (), s4.data (),
                                    &nsyncQual, &badsync);
              if (badsync != 0)
                {
                  continue;
                }

              std::array<float, 240 * 4> llrs {};
              auto bm = [&bitmetrics] (int bitIndex1, int block1) -> float {
                return bitmetrics[static_cast<size_t> ((bitIndex1 - 1) + 320 * (block1 - 1))];
              };
              for (int il = 0; il < 4; ++il)
                {
                  for (int i = 0; i < 60; ++i)
                    {
                      llrs[static_cast<size_t> (i + 240 * il)] = bm (17 + i, il + 1);
                      llrs[static_cast<size_t> (60 + i + 240 * il)] = bm (93 + i, il + 1);
                      llrs[static_cast<size_t> (120 + i + 240 * il)] = bm (169 + i, il + 1);
                      llrs[static_cast<size_t> (180 + i + 240 * il)] = bm (245 + i, il + 1);
                    }
                }

              float apmag = 0.0f;
              for (int i = 0; i < 240; ++i)
                {
                  apmag = std::max (apmag, std::abs (llrs[static_cast<size_t> (i + 240 * 3)]));
                }
              apmag *= 1.1f;

              int ntmax = cfg.nblock + state.nappasses[static_cast<size_t> (qBound (0, request.nqsoprogress, 5))];
              if (request.lapcqonly != 0) ntmax = cfg.nblock + 1;
              if (request.ndepth == 1) ntmax = cfg.nblock;
              if (request.iwspr != 0) ntmax = cfg.nblock;

              for (int itry = 1; itry <= ntmax && !candidateDone; ++itry)
                {
                  std::array<float, 240> llr {};
                  std::array<signed char, 240> apmask {};
                  int iaptype = 0;

                  if (itry <= cfg.nblock)
                    {
                      std::copy_n (llrs.begin () + 240 * (itry - 1), 240, llr.begin ());
                    }
                  else
                    {
                      std::copy_n (llrs.begin () + 240 * (cfg.nblock - 1), 240, llr.begin ());
                      iaptype = state.naptypes[static_cast<size_t> (qBound (0, request.nqsoprogress, 5))]
                                              [static_cast<size_t> (itry - cfg.nblock - 1)];
                      if (request.lapcqonly != 0)
                        {
                          iaptype = 1;
                        }
                      if (iaptype >= 2 && state.apbits[0] > 1)
                        {
                          continue;
                        }
                      if (iaptype >= 3 && state.apbits[29] > 1)
                        {
                          continue;
                        }
                      if (iaptype == 1)
                        {
                          std::fill_n (apmask.begin (), 29, static_cast<signed char> (1));
                          for (int i = 0; i < 29; ++i)
                            {
                              llr[static_cast<size_t> (i)] = apmag * state.mcq[static_cast<size_t> (i)];
                            }
                        }
                      else if (iaptype == 2)
                        {
                          std::fill_n (apmask.begin (), 29, static_cast<signed char> (1));
                          for (int i = 0; i < 29; ++i)
                            {
                              llr[static_cast<size_t> (i)] = apmag * state.apbits[static_cast<size_t> (i)];
                            }
                        }
                      else if (iaptype == 3)
                        {
                          std::fill_n (apmask.begin (), 58, static_cast<signed char> (1));
                          for (int i = 0; i < 58; ++i)
                            {
                              llr[static_cast<size_t> (i)] = apmag * state.apbits[static_cast<size_t> (i)];
                            }
                        }
                      else if (iaptype == 4 || iaptype == 5 || iaptype == 6)
                        {
                          std::fill_n (apmask.begin (), 77, static_cast<signed char> (1));
                          for (int i = 0; i < 58; ++i)
                            {
                              llr[static_cast<size_t> (i)] = apmag * state.apbits[static_cast<size_t> (i)];
                            }
                          for (int i = 0; i < 19; ++i)
                            {
                              int const source = iaptype == 4 ? state.mrrr[static_cast<size_t> (i)]
                                  : (iaptype == 5 ? state.m73[static_cast<size_t> (i)] : state.mrr73[static_cast<size_t> (i)]);
                              llr[static_cast<size_t> (58 + i)] = apmag * source;
                            }
                        }
                    }

                  float dmin = 0.0f;
                  int nharderrors = -1;
                  int ntype = 0;
                  bool unpackOk = false;
                  QByteArray msg;
                  std::array<signed char, 240> cw {};

                  if (request.iwspr == 0)
                    {
                      int keff = 91;
                      int maxosd = 2;
                      int norder = 3;
                      std::array<signed char, 101> message101 {};
                      bool const haveCodeword = decode240_101_native (
                          llr, keff, maxosd, norder, apmask, message101, cw, ntype, nharderrors, dmin);
                      if (!haveCodeword)
                        {
                          continue;
                        }

                      std::array<signed char, 77> message77 {};
                      for (int i = 0; i < 77; ++i)
                        {
                          message77[static_cast<size_t> (i)] = static_cast<signed char> ((message101[static_cast<size_t> (i)] + state.rvec[static_cast<size_t> (i)]) & 1);
                        }
                      msg = unpack_77bits (message77, &unpackOk);
                      if (nharderrors >= 0 && unpackOk)
                        {
                          std::array<int, kTotalSymbols> itone {};
                          int iwsprFlag = 0;
                          get_fst4_tones_from_bits_ (message101.data (), itone.data (), &iwsprFlag);
                          float fmid = -999.0f;
                          float w50 = 0.0f;
                          if (QFileInfo::exists (QStringLiteral ("plotspec")))
                            {
                              int nsps = cfg.nsps;
                              int nmax = cfg.nmax;
                              int ndown = cfg.ndown;
                              int hmodArg = hmod;
                              int i0arg = candidate.isbest + ioffset;
                              float fcArg = candidate.fcSynced;
                              dopspread_native (itone, std::vector<short> (localAudio.constBegin (), localAudio.constEnd ()),
                                                nsps, nmax, ndown, hmodArg, i0arg, fcArg, fmid, w50);
                            }

                          float xsig = 0.0f;
                          for (int i = 0; i < kTotalSymbols; ++i)
                            {
                              xsig += s4_value (s4, itone[static_cast<size_t> (i)], i + 1);
                            }
                          float const arg = candidate.base > 0.0f
                              ? snr_cal_factor (request.ntrperiod) * xsig / candidate.base - 1.0f
                              : -1.0f;
                          float const xsnr = arg > 0.0f
                              ? 10.0f * std::log10 (arg) + 10.0f * std::log10 (1.46f / 2500.0f)
                                  + 10.0f * std::log10 (8200.0f / cfg.nsps)
                              : -99.9f;
                          float xdt = (candidate.isbest + ioffset - cfg.nspsec) / cfg.fs2;
                          if (request.ntrperiod == 15)
                            {
                              xdt = (candidate.isbest + ioffset - static_cast<float> (cfg.nspsec) / 2.0f) / cfg.fs2;
                            }
                          float const fsig = candidate.fcSynced - 1.5f * cfg.baud;

                          DecodedLine line;
                          line.snr = qRound (xsnr);
                          line.dt = xdt;
                          line.freq = fsig;
                          line.nap = iaptype;
                          line.qual = 0.0f;
                          line.decoded = msg;
                          line.bits77 = message77;
                          line.fmid = fmid;
                          line.w50 = w50;
                          append_unique_line (rows, seen, std::move (line));
                          candidateDone = true;
                        }
                    }
                  else
                    {
                      std::array<signed char, 74> message74 {};
                      int keff = 66;
                      int maxosd = 2;
                      int norder = 3;
                      bool haveCodeword = decode240_74_native (
                          llr, keff, maxosd, norder, apmask, message74, cw, ntype, nharderrors, dmin);
                      if (nharderrors >= 0)
                        {
                          if (haveCodeword)
                            {
                              msg = unpack_fst4w_50bit (message74, request.lprinthash22 != 0, &unpackOk);
                            }
                          else
                            {
                              nharderrors = -1;
                            }
                        }

                      if (unpackOk && cfg.doK50Decode)
                        {
                          maybe_save_fst4w_call (state, msg, &newCallsign);
                        }

                      if (!unpackOk && cfg.doK50Decode)
                        {
                          maxosd = 1;
                          keff = 50;
                          norder = 4;
                          haveCodeword = decode240_74_native (
                              llr, keff, maxosd, norder, apmask, message74, cw, ntype, nharderrors, dmin);
                          if (!haveCodeword)
                            {
                              nharderrors = -1;
                            }
                          else
                            {
                              msg = unpack_fst4w_50bit (message74, request.lprinthash22 != 0, &unpackOk);
                              if (unpackOk && !contains_known_fst4w_call (state, msg))
                                {
                                  unpackOk = false;
                                }
                            }
                        }

                      if (nharderrors >= 0 && unpackOk)
                        {
                          std::array<int, kTotalSymbols> itone {};
                          int iwsprFlag = 1;
                          get_fst4_tones_from_bits_ (message74.data (), itone.data (), &iwsprFlag);
                          float fmid = -999.0f;
                          float w50 = 0.0f;
                          if (QFileInfo::exists (QStringLiteral ("plotspec")))
                            {
                              int nsps = cfg.nsps;
                              int nmax = cfg.nmax;
                              int ndown = cfg.ndown;
                              int hmodArg = hmod;
                              int i0arg = candidate.isbest + ioffset;
                              float fcArg = candidate.fcSynced;
                              dopspread_native (itone, std::vector<short> (localAudio.constBegin (), localAudio.constEnd ()),
                                                nsps, nmax, ndown, hmodArg, i0arg, fcArg, fmid, w50);
                            }

                          float xsig = 0.0f;
                          for (int i = 0; i < kTotalSymbols; ++i)
                            {
                              xsig += s4_value (s4, itone[static_cast<size_t> (i)], i + 1);
                            }
                          float const arg = candidate.base > 0.0f
                              ? snr_cal_factor (request.ntrperiod) * xsig / candidate.base - 1.0f
                              : -1.0f;
                          float const xsnr = arg > 0.0f
                              ? 10.0f * std::log10 (arg) + 10.0f * std::log10 (1.46f / 2500.0f)
                                  + 10.0f * std::log10 (8200.0f / cfg.nsps)
                              : -99.9f;
                          float xdt = (candidate.isbest + ioffset - cfg.nspsec) / cfg.fs2;
                          if (request.ntrperiod == 15)
                            {
                              xdt = (candidate.isbest + ioffset - static_cast<float> (cfg.nspsec) / 2.0f) / cfg.fs2;
                            }
                          float const fsig = candidate.fcSynced - 1.5f * cfg.baud;

                          DecodedLine line;
                          line.snr = qRound (xsnr);
                          line.dt = xdt;
                          line.freq = fsig;
                          line.nap = iaptype;
                          line.qual = 0.0f;
                          line.decoded = msg;
                          line.bits77 = fst4w_bits77 (message74);
                          line.fmid = fmid;
                          line.w50 = w50;
                          append_unique_line (rows, seen, std::move (line));
                          candidateDone = true;
                        }
                    }
                }
            }
        }
    }

  if (newCallsign && cfg.doK50Decode && request.iwspr != 0)
    {
      save_wcalls (state, dataDir);
    }

  return rows;
}

QStringList decodeFst4Rows (DecodeRequest const& request)
{
  QString const utcPrefix = format_decode_utc (request.nutc);
  return build_rows (utcPrefix, decodeFst4Lines (request), request.ntrperiod);
}

FST4DecodeWorker::FST4DecodeWorker (QObject* parent)
  : QObject {parent}
{
}

void FST4DecodeWorker::decode (DecodeRequest const& request)
{
  Q_EMIT decodeReady (request.serial, decodeFst4Rows (request));
}

}
}

extern "C" void ftx_fst4_reset_runtime_state_c ()
{
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};
  persistent_state () = PersistentState {};
  sync_cache () = SyncCache {};
  ftx_pack77_reset_context_ ();
}

extern "C" void fst4_async_decode_ (short iwave[], int* nutc, int* nqsoprogress, int* nfa, int* nfb,
                                     int* nfqso, int* ndepth, int* ntrperiod, int* nexp_decode,
                                     int* ntol, float* emedelay, int* nagain, int* lapcqonly,
                                     char* mycall, char* hiscall, int* iwspr, int* lprinthash22,
                                     int* snrs, float* dts, float* freqs, int* naps, float* quals,
                                     signed char* bits77, char* decodeds, float* fmids, float* w50s,
                                     int* nout, fortran_charlen_t mycall_len, fortran_charlen_t hiscall_len,
                                     fortran_charlen_t /*decodeds_len*/)
{
  if (nout) *nout = 0;
  if (!iwave || !nutc || !nqsoprogress || !nfa || !nfb || !nfqso || !ndepth || !ntrperiod
      || !nexp_decode || !ntol || !emedelay || !nagain || !lapcqonly || !mycall || !hiscall
      || !iwspr || !lprinthash22 || !snrs || !dts || !freqs || !naps || !quals || !bits77
      || !decodeds || !fmids || !w50s)
    {
      return;
    }

  constexpr int kMaxLines = 200;
  constexpr int kDecodedChars = 37;
  std::fill_n (snrs, kMaxLines, 0);
  std::fill_n (dts, kMaxLines, 0.0f);
  std::fill_n (freqs, kMaxLines, 0.0f);
  std::fill_n (naps, kMaxLines, 0);
  std::fill_n (quals, kMaxLines, 0.0f);
  std::fill_n (bits77, 77 * kMaxLines, static_cast<signed char> (0));
  std::fill_n (decodeds, kDecodedChars * kMaxLines, ' ');
  std::fill_n (fmids, kMaxLines, -999.0f);
  std::fill_n (w50s, kMaxLines, 0.0f);

  decodium::fst4::DecodeRequest request;
  request.serial = 0;
  request.mode = *iwspr != 0 ? QStringLiteral ("FST4W") : QStringLiteral ("FST4");
  RuntimeConfig const cfg = config_for_period (qBound (15, *ntrperiod, 1800),
                                               qBound (1, *ndepth, 3));
  request.audio = QVector<short> (cfg.nmax);
  std::copy_n (iwave, cfg.nmax, request.audio.data ());
  request.dataDir = QDir::currentPath ().toLocal8Bit ();
  request.nutc = *nutc;
  request.nqsoprogress = *nqsoprogress;
  request.nfa = *nfa;
  request.nfb = *nfb;
  request.nfqso = *nfqso;
  request.ndepth = *ndepth;
  request.ntrperiod = *ntrperiod;
  request.nexp_decode = *nexp_decode;
  request.ntol = *ntol;
  request.emedelay = *emedelay;
  request.nagain = *nagain;
  request.lapcqonly = *lapcqonly;
  request.mycall = QByteArray {mycall, static_cast<int> (mycall_len)};
  request.hiscall = QByteArray {hiscall, static_cast<int> (hiscall_len)};
  request.iwspr = *iwspr;
  request.lprinthash22 = *lprinthash22;

  auto const lines = decodium::fst4::decodeFst4Lines (request);
  int const count = std::min (static_cast<int> (lines.size ()), kMaxLines);
  if (nout) *nout = count;
  for (int i = 0; i < count; ++i)
    {
      auto const& line = lines[static_cast<size_t> (i)];
      snrs[i] = line.snr;
      dts[i] = line.dt;
      freqs[i] = line.freq;
      naps[i] = line.nap;
      quals[i] = line.qual;
      std::copy (line.bits77.begin (), line.bits77.end (), bits77 + 77 * i);
      QByteArray const decoded = fixed_field (line.decoded, kDecodedChars);
      std::copy_n (decoded.constData (), kDecodedChars, decodeds + kDecodedChars * i);
      fmids[i] = line.fmid;
      w50s[i] = line.w50;
    }
}

extern "C" void fst4_baseline_ (float s[], int* np, int* ia, int* ib, int* npct, float sbase[])
{
  if (!s || !np || !ia || !ib || !npct || !sbase || *np <= 0)
    {
      return;
    }

  std::vector<float> spectrum (s, s + *np);
  std::vector<float> baseline;
  if (!fst4_baseline_cpp (spectrum, *np, *ia, *ib, *npct, baseline))
    {
      std::fill_n (sbase, *np, 0.0f);
      return;
    }
  std::copy (baseline.begin (), baseline.end (), sbase);
}

extern "C" void ftx_fst4_decode_and_emit_params_c (short const* iwave,
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

  int const depth = std::max (1, params->ndepth & 3);
  RuntimeConfig const cfg = config_for_period (qBound (15, params->ntrperiod, 1800), depth);

  decodium::fst4::DecodeRequest request;
  request.serial = 0;
  request.mode = params->nmode == 240 ? QStringLiteral ("FST4") : QStringLiteral ("FST4W");
  request.audio = QVector<short> (cfg.nmax);
  std::copy_n (iwave, cfg.nmax, request.audio.data ());
  request.dataDir = QDir::currentPath ().toLocal8Bit ();
  request.nutc = params->nutc;
  request.nqsoprogress = params->nQSOProgress;
  request.nfa = params->nfa;
  request.nfb = params->nfb;
  request.nfqso = params->nfqso;
  request.ndepth = depth;
  request.ntrperiod = params->ntrperiod;
  request.nexp_decode = params->nexp_decode;
  request.ntol = params->ntol;
  request.emedelay = params->emedelay;
  request.nagain = params->nagain ? 1 : 0;
  request.lapcqonly = params->lapcqonly ? 1 : 0;
  request.mycall = QByteArray {params->mycall, static_cast<int> (sizeof (params->mycall))};
  request.hiscall = QByteArray {params->hiscall, static_cast<int> (sizeof (params->hiscall))};
  request.iwspr = params->nmode == 240 ? 0 : 1;
  request.lprinthash22 = params->nmode == 242 ? 1 : 0;

  auto const lines = decodeFst4Lines (request);
  if (decoded_count)
    {
      *decoded_count = emit_fst4_results (request.nutc, request.ntrperiod,
                                          request.nagain, temp_dir, lines);
    }
  else
    {
      emit_fst4_results (request.nutc, request.ntrperiod, request.nagain, temp_dir, lines);
    }
}

extern "C" int ftx_fst4_get_candidates_c (std::complex<float> const* bigfft,
                                           int nfft1, int nsps, float fs,
                                           float fa, float fb, int nfa, int nfb,
                                           float minsync, float* candidates_out,
                                           int max_candidates)
{
  if (!bigfft || !candidates_out || nfft1 <= 0 || nsps <= 0 || max_candidates <= 0)
    {
      return 0;
    }

  RuntimeConfig cfg;
  cfg.nfft1 = nfft1;
  cfg.nsps = nsps;
  cfg.fs = fs;
  cfg.baud = fs / nsps;

  std::vector<Complex> bigfft_vec (bigfft, bigfft + (nfft1 / 2 + 1));
  auto candidates = get_candidates_fst4_cpp (bigfft_vec, cfg, 1, fa, fb, nfa, nfb, minsync);
  int const count = std::min (static_cast<int> (candidates.size ()), max_candidates);
  for (int i = 0; i < count; ++i)
    {
      candidates_out[5 * i + 0] = candidates[static_cast<size_t> (i)].fc0;
      candidates_out[5 * i + 1] = candidates[static_cast<size_t> (i)].detmet;
      candidates_out[5 * i + 2] = candidates[static_cast<size_t> (i)].fcSynced;
      candidates_out[5 * i + 3] = static_cast<float> (candidates[static_cast<size_t> (i)].isbest);
      candidates_out[5 * i + 4] = candidates[static_cast<size_t> (i)].base;
    }
  return count;
}

extern "C" int ftx_fst4_downsample_c (std::complex<float> const* bigfft,
                                      int nfft1, int ndown, int nsps, float fs,
                                      float f0, std::complex<float>* out,
                                      int out_size)
{
  if (!bigfft || !out || nfft1 <= 0 || ndown <= 0 || nsps <= 0)
    {
      return 0;
    }
  int const nfft2 = nfft1 / ndown;
  if (out_size < nfft2)
    {
      return 0;
    }

  RuntimeConfig cfg;
  cfg.nfft1 = nfft1;
  cfg.nfft2 = nfft2;
  cfg.ndown = ndown;
  cfg.nsps = nsps;
  cfg.fs = fs;
  cfg.baud = fs / nsps;
  cfg.sigbw = 4.0f * cfg.baud;

  std::vector<Complex> bigfft_vec (bigfft, bigfft + (nfft1 / 2 + 1));
  std::vector<Complex> out_vec;
  fst4_downsample_cpp (bigfft_vec, cfg, f0, out_vec);
  std::copy (out_vec.begin (), out_vec.end (), out);
  return nfft2;
}

extern "C" int ftx_fst4_sync_search_c (std::complex<float> const* c2, int nfft2,
                                       int nsps, int ndown, int ntrperiod,
                                       float emedelay, float* sbest_out,
                                       float* fcbest_out, int* isbest_out)
{
  if (!c2 || !sbest_out || !fcbest_out || !isbest_out || nfft2 <= 0 || nsps <= 0 || ndown <= 0)
    {
      return 0;
    }

  RuntimeConfig cfg;
  cfg.nsps = nsps;
  cfg.ndown = ndown;
  cfg.nfft2 = nfft2;
  cfg.nss = nsps / ndown;
  cfg.fs2 = static_cast<float> (RX_SAMPLE_RATE) / ndown;
  cfg.nspsec = static_cast<int> (cfg.fs2);
  cfg.baud = static_cast<float> (RX_SAMPLE_RATE) / nsps;

  std::vector<Complex> c2_vec (c2, c2 + nfft2);
  float sbest = 0.0f;
  float fcbest = 0.0f;
  int isbest = 0;
  fst4_sync_search_cpp (c2_vec, cfg, 1, ntrperiod, emedelay, sbest, fcbest, isbest);
  *sbest_out = sbest;
  *fcbest_out = fcbest;
  *isbest_out = isbest;
  return 1;
}

extern "C" void __fst4_decode_MOD_get_candidates_fst4 (std::complex<float> c_bigfft[], int* nfft1,
                                                        int* nsps, int* hmod, float* fs, float* fa, float* fb,
                                                        int* nfa, int* nfb, float* minsync,
                                                        int* ncand, float candidates[])
{
  if (ncand) *ncand = 0;
  if (!c_bigfft || !nfft1 || !nsps || !hmod || !fs || !fa || !fb || !nfa || !nfb || !minsync || !candidates)
    {
      return;
    }

  RuntimeConfig cfg;
  cfg.nfft1 = *nfft1;
  cfg.nsps = *nsps;
  cfg.fs = *fs;
  cfg.baud = *fs / *nsps;

  std::vector<Complex> bigfft_vec (c_bigfft, c_bigfft + (*nfft1 / 2 + 1));
  auto found = get_candidates_fst4_cpp (bigfft_vec, cfg, *hmod, *fa, *fb, *nfa, *nfb, *minsync);
  int const count = std::min (static_cast<int> (found.size ()), kMaxCandidates);
  for (int i = 0; i < count; ++i)
    {
      candidates[i] = found[static_cast<size_t> (i)].fc0;
      candidates[i + kMaxCandidates] = found[static_cast<size_t> (i)].detmet;
      candidates[i + 2 * kMaxCandidates] = found[static_cast<size_t> (i)].fcSynced;
      candidates[i + 3 * kMaxCandidates] = static_cast<float> (found[static_cast<size_t> (i)].isbest);
      candidates[i + 4 * kMaxCandidates] = found[static_cast<size_t> (i)].base;
    }
  if (ncand)
    {
      *ncand = count;
    }
}

extern "C" void __fst4_decode_MOD_fst4_downsample (std::complex<float> c_bigfft[], int* nfft1,
                                                    int* ndown, float* f0, float* sigbw,
                                                    std::complex<float> c1[])
{
  if (!c_bigfft || !nfft1 || !ndown || !f0 || !sigbw || !c1 || *nfft1 <= 0 || *ndown <= 0)
    {
      return;
    }

  RuntimeConfig cfg;
  cfg.nfft1 = *nfft1;
  cfg.nfft2 = *nfft1 / *ndown;
  cfg.ndown = *ndown;
  cfg.fs = RX_SAMPLE_RATE;
  cfg.sigbw = *sigbw;

  std::vector<Complex> bigfft_vec (c_bigfft, c_bigfft + (*nfft1 / 2 + 1));
  std::vector<Complex> out_vec;
  fst4_downsample_cpp (bigfft_vec, cfg, *f0, out_vec);
  std::copy (out_vec.begin (), out_vec.end (), c1);
}

extern "C" void __fst4_decode_MOD_fst4_sync_search (std::complex<float> c2[], int* nfft2,
                                                     int* hmod, float* fs2, int* nss, int* ntrperiod,
                                                     int* nsyncoh, float* emedelay,
                                                     float* sbest, float* fcbest, int* isbest)
{
  if (!c2 || !nfft2 || !hmod || !fs2 || !nss || !ntrperiod || !nsyncoh || !emedelay
      || !sbest || !fcbest || !isbest || *nfft2 <= 0 || *nss <= 0)
    {
      return;
    }

  RuntimeConfig cfg;
  cfg.nfft2 = *nfft2;
  cfg.nss = *nss;
  cfg.fs2 = *fs2;
  cfg.nspsec = static_cast<int> (cfg.fs2);
  cfg.baud = cfg.fs2 / *nss;

  std::vector<Complex> c2_vec (c2, c2 + *nfft2);
  fst4_sync_search_cpp (c2_vec, cfg, *hmod, *ntrperiod, *emedelay, *sbest, *fcbest, *isbest);
  (void) nsyncoh;
}

extern "C" int ftx_fst4_debug_first_candidate_c (short const* audio, int audio_size,
                                                 int ntrperiod, int ndepth,
                                                 int nfa_in, int nfb_in, int nfqso,
                                                 int ntol, float emedelay,
                                                 char const* mycall_in, char const* hiscall_in,
                                                 int* stage_out, int* badsync_out,
                                                 int* nharderrors_out, int* iaptype_out,
                                                 char msg_out[37])
{
  if (stage_out) *stage_out = 0;
  if (badsync_out) *badsync_out = 0;
  if (nharderrors_out) *nharderrors_out = -999;
  if (iaptype_out) *iaptype_out = 0;
  if (msg_out) std::fill_n (msg_out, 37, ' ');
  if (!audio || audio_size <= 0 || !mycall_in || !hiscall_in)
    {
      return 0;
    }
  (void) nfqso;
  (void) ntol;

  RuntimeConfig const cfg = config_for_period (qBound (15, ntrperiod, 1800), qBound (1, ndepth, 3));
  QVector<short> localAudio (cfg.nmax);
  int const copyCount = std::min (audio_size, cfg.nmax);
  if (copyCount > 0)
    {
      std::copy_n (audio, copyCount, localAudio.begin ());
    }

  QByteArray const mycall = fixed_field (QByteArray {mycall_in, 12}, 12);
  QByteArray const hiscall = fixed_field (QByteArray {hiscall_in, 12}, 12);
  ftx_pack77_reset_context_ ();
  ftx_pack77_set_context_ (mycall.constData (), hiscall.constData (),
                           static_cast<fortran_charlen_t> (12),
                           static_cast<fortran_charlen_t> (12));

  PersistentState& state = persistent_state ();
  initialize_state (state);
  update_ap_bits (state, mycall, hiscall);

  int nfa = qBound (0, nfa_in, 5000);
  int nfb = qMax (nfa + 50, qBound (0, nfb_in, 5000));
  float fa = std::max (100.0f, nfa + 1.5f * cfg.baud);
  float fb = std::min (4800.0f, nfb + 1.5f * cfg.baud);
  nfa = std::max (100, nfa - 100);
  nfb = std::min (4800, nfb + 100);
  float const minsync = ntrperiod == 15 ? 1.15f : 1.20f;
  int const hmod = 1;

  std::vector<Complex> bigfft (static_cast<size_t> (cfg.nfft1 / 2 + 1));
  int nz = cfg.nfft1;
  int ndropmax = 1;
  int npct = 0;
  blanker_ (localAudio.data (), &nz, &ndropmax, &npct, bigfft.data ());
  int nfft = cfg.nfft1;
  int ndim = 1;
  int isign = -1;
  int iform = 0;
  four2a_ (bigfft.data (), &nfft, &ndim, &isign, &iform);

  auto candidates = get_candidates_fst4_cpp (bigfft, cfg, hmod, fa, fb, nfa, nfb, minsync);
  if (candidates.empty ())
    {
      if (stage_out) *stage_out = 1;
      return 0;
    }

  Candidate candidate = candidates.front ();
  std::vector<Complex> c2;
  fst4_downsample_cpp (bigfft, cfg, candidate.fc0, c2);
  float sbest = 0.0f;
  float fcbest = 0.0f;
  int isbest = 0;
  fst4_sync_search_cpp (c2, cfg, hmod, ntrperiod, emedelay, sbest, fcbest, isbest);
  candidate.fcSynced = candidate.fc0 + fcbest;
  candidate.isbest = isbest;
  if (stage_out) *stage_out = 2;

  fst4_downsample_cpp (bigfft, cfg, candidate.fcSynced, c2);
  std::vector<Complex> cframe (static_cast<size_t> (kTotalSymbols * cfg.nss));
  for (int jitterIndex = 0; jitterIndex <= cfg.jittermax; ++jitterIndex)
    {
      int ioffset = 0;
      if (jitterIndex == 1) ioffset = 1;
      if (jitterIndex == 2) ioffset = -1;
      int const is0 = candidate.isbest + ioffset;
      int const iend = is0 + kTotalSymbols * cfg.nss - 1;
      if (is0 < 0 || iend >= cfg.nfft2)
        {
          continue;
        }
      std::copy_n (c2.begin () + is0, kTotalSymbols * cfg.nss, cframe.begin ());

      std::array<float, 320 * 4> bitmetrics {};
      std::array<float, 4 * kTotalSymbols> s4 {};
      int nsyncQual = 0;
      int badsync = 0;
      int nss = cfg.nss;
      get_fst4_bitmetrics_ (cframe.data (), &nss, bitmetrics.data (), s4.data (), &nsyncQual, &badsync);
      if (badsync_out) *badsync_out = badsync;
      if (badsync != 0)
        {
          if (stage_out) *stage_out = 3;
          continue;
        }

      std::array<float, 240 * 4> llrs {};
      auto bm = [&bitmetrics] (int bitIndex1, int block1) -> float {
        return bitmetrics[static_cast<size_t> ((bitIndex1 - 1) + 320 * (block1 - 1))];
      };
      for (int il = 0; il < 4; ++il)
        {
          for (int i = 0; i < 60; ++i)
            {
              llrs[static_cast<size_t> (i + 240 * il)] = bm (17 + i, il + 1);
              llrs[static_cast<size_t> (60 + i + 240 * il)] = bm (93 + i, il + 1);
              llrs[static_cast<size_t> (120 + i + 240 * il)] = bm (169 + i, il + 1);
              llrs[static_cast<size_t> (180 + i + 240 * il)] = bm (245 + i, il + 1);
            }
        }

      float apmag = 0.0f;
      for (int i = 0; i < 240; ++i)
        {
          apmag = std::max (apmag, std::abs (llrs[static_cast<size_t> (i + 240 * 3)]));
        }
      apmag *= 1.1f;

      int ntmax = cfg.nblock + state.nappasses[0];
      if (ndepth == 1) ntmax = cfg.nblock;
      for (int itry = 1; itry <= ntmax; ++itry)
        {
          std::array<float, 240> llr {};
          std::array<signed char, 240> apmask {};
          int iaptype = 0;
          if (itry <= cfg.nblock)
            {
              std::copy_n (llrs.begin () + 240 * (itry - 1), 240, llr.begin ());
            }
          else
            {
              std::copy_n (llrs.begin () + 240 * (cfg.nblock - 1), 240, llr.begin ());
              iaptype = state.naptypes[0][static_cast<size_t> (itry - cfg.nblock - 1)];
              if (iaptype >= 2 && state.apbits[0] > 1)
                {
                  continue;
                }
              if (iaptype >= 3 && state.apbits[29] > 1)
                {
                  continue;
                }
              if (iaptype == 1)
                {
                  std::fill_n (apmask.begin (), 29, static_cast<signed char> (1));
                  for (int i = 0; i < 29; ++i)
                    {
                      llr[static_cast<size_t> (i)] = apmag * state.mcq[static_cast<size_t> (i)];
                    }
                }
              else if (iaptype == 2)
                {
                  std::fill_n (apmask.begin (), 29, static_cast<signed char> (1));
                  for (int i = 0; i < 29; ++i)
                    {
                      llr[static_cast<size_t> (i)] = apmag * state.apbits[static_cast<size_t> (i)];
                    }
                }
            }

          float dmin = 0.0f;
          int nharderrors = -1;
          int ntype = 0;
          std::array<signed char, 240> cw {};
          std::array<signed char, 101> message101 {};
          int keff = 91;
          int maxosd = 2;
          int norder = 3;
          bool const haveCodeword = decodium::fst4::decode240_101_native (
              llr, keff, maxosd, norder, apmask, message101, cw, ntype, nharderrors, dmin);
          if (nharderrors_out) *nharderrors_out = nharderrors;
          if (iaptype_out) *iaptype_out = iaptype;
          if (stage_out) *stage_out = 4;

          if (!haveCodeword)
            {
              continue;
            }

          std::array<signed char, 77> message77 {};
          for (int i = 0; i < 77; ++i)
            {
              message77[static_cast<size_t> (i)] = static_cast<signed char> ((message101[static_cast<size_t> (i)] + state.rvec[static_cast<size_t> (i)]) & 1);
            }
          bool unpackOk = false;
          QByteArray msg = unpack_77bits (message77, &unpackOk);
          if (unpackOk)
            {
              if (stage_out) *stage_out = 5;
              if (msg_out)
                {
                  std::fill_n (msg_out, 37, ' ');
                  std::copy_n (msg.constData (), std::min (37, msg.size ()), msg_out);
                }
            }
          if (nharderrors >= 0 && unpackOk)
            {
              if (stage_out) *stage_out = 6;
              return 1;
            }
        }
    }

  return 0;
}
