// -*- Mode: C++ -*-
#include "wsjtx_config.h"
#include "commons.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtEndian>

#include "SharedMemorySegment.hpp"
#include "Detector/FST4DecodeWorker.hpp"
#include "Detector/FT2DecodeWorker.hpp"
#include "Detector/FT4DecodeWorker.hpp"
#include "Detector/FT8DecodeWorker.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"
#include "Detector/MSK144DecodeWorker.hpp"
#include "Detector/Q65DecodeWorker.hpp"
#include "revision_utils.hpp"

extern "C"
{
  void symspec_ (dec_data_t* shared_data, int* k, double* trperiod, int* nsps,
                 int* ingain, signed char* low_sidelobes, int* nminw,
                 float* pxdb, float* spectrum, float* df3, int* ihsym,
                 int* npts8, float* pxdbmax, float* npct);
}

namespace
{
  constexpr int kFt8Samples {50 * 3456};
  constexpr int kFt4Samples {21 * 3456};
  constexpr int kShmemWaitMs {10};
  constexpr int kShmemRetryCount {200};
  constexpr int kJt9Nsps {6912};
  constexpr int kJt9KStep {kJt9Nsps / 2};
  constexpr int kJt9SpectrumSize {184 * NSMAX};

  struct WavData
  {
    int sampleRate {};
    int channels {};
    int bitsPerSample {};
    QVector<short> samples;
  };

  struct Options
  {
    bool readFiles {true};
    QString shmKey;
    double trPeriod {60.0};
    QString exeDir {QStringLiteral (".")};
    QString dataDir {QStringLiteral (".")};
    QString tempDir {QStringLiteral (".")};
    int flow {200};
    int fsplit {2700};
    int fhigh {4000};
    int rxFrequency {1500};
    int tolerance {20};
    bool haveTolerance {false};
    int patience {1};
    int fftThreads {1};
    bool multithreadFt8 {false};
    int mtCycles {3};
    bool hideFt8Dupes {false};
    int mtRxFreqSensitivity {3};
    int mtNumThreads {0};
    int mtDecodeSensitivity {3};
    int mtStartTime {3};
    bool wideDxSearch {true};
    int mode {0};
    bool quiet {false};
    int qsoProgress {0};
    int subMode {0};
    int depth {1};
    bool txJt9 {false};
    QString myCall {QStringLiteral ("K1ABC")};
    QString myGrid;
    QString hisCall {QStringLiteral ("W9XYZ")};
    QString hisGrid {QStringLiteral ("EN37")};
    int experienceDecode {0};
    bool experienceDecodeSet {false};
    QStringList files;
  };

  struct LegacySpectra
  {
    QVector<float> values;
    int npts8 {0};
    int nzhsym {0};
  };

  [[noreturn]] void fail (QString const& message)
  {
    throw std::runtime_error {message.toStdString ()};
  }

  QByteArray to_fortran_field (QByteArray value, int width)
  {
    value = value.left (width);
    if (value.size () < width)
      {
        value.append (QByteArray (width - value.size (), ' '));
      }
    return value;
  }

  void emit_finished (int decodedCount)
  {
    QString const line = QStringLiteral ("<DecodeFinished>%1%2%3")
        .arg (0, 4)
        .arg (decodedCount, 4)
        .arg (0, 9);
    std::cout << line.toStdString () << '\n';
    std::cout.flush ();
  }

  void emit_rows (QStringList const& rows, bool quiet)
  {
    for (auto const& row : rows)
      {
        if (!row.trimmed ().isEmpty ())
          {
            std::cout << row.toStdString () << '\n';
          }
      }
    if (!quiet)
      {
        emit_finished (rows.size ());
      }
    else
      {
        std::cout.flush ();
      }
  }

  int input_sample_count (int mode, double trPeriod)
  {
    if (mode == 8)
      {
        return kFt8Samples;
      }
    if (mode == 5)
      {
        return kFt4Samples;
      }
    int const ntr = std::max (1, static_cast<int> (trPeriod));
    return ntr * RX_SAMPLE_RATE;
  }

  int extract_utc_from_filename (QString const& fileName)
  {
    QRegularExpression const re {R"((?:_|)(\d{4,6})\.(?:wav|WAV)$)"};
    auto const match = re.match (QFileInfo {fileName}.fileName ());
    if (!match.hasMatch ())
      {
        return 0;
      }
    bool ok = false;
    int const value = match.captured (1).toInt (&ok);
    return ok ? value : 0;
  }

  WavData read_wav_file (QString const& fileName)
  {
    QFile file {fileName};
    if (!file.open (QIODevice::ReadOnly))
      {
        fail (QStringLiteral ("cannot open WAV file \"%1\": %2")
                  .arg (fileName, file.errorString ()));
      }

    QByteArray const blob = file.readAll ();
    if (blob.size () < 12)
      {
        fail (QStringLiteral ("WAV file \"%1\" is too short").arg (fileName));
      }
    if (blob.mid (0, 4) != "RIFF" || blob.mid (8, 4) != "WAVE")
      {
        fail (QStringLiteral ("WAV file \"%1\" is not RIFF/WAVE").arg (fileName));
      }
    quint32 const riffSize =
        qFromLittleEndian<quint32> (reinterpret_cast<uchar const*> (blob.constData () + 4));
    int const riffEnd = std::min<int> (blob.size (), static_cast<int> (riffSize) + 8);

    bool haveFmt = false;
    bool haveData = false;
    quint16 audioFormat = 0;
    quint16 channels = 0;
    quint32 sampleRate = 0;
    quint16 bitsPerSample = 0;
    QByteArray dataChunk;

    int pos = 12;
    while (pos + 8 <= riffEnd)
      {
        QByteArray const chunkId = blob.mid (pos, 4);
        quint32 const chunkSize = qFromLittleEndian<quint32> (
            reinterpret_cast<uchar const*> (blob.constData () + pos + 4));
        pos += 8;
        if (pos + static_cast<int> (chunkSize) > riffEnd)
          {
            fail (QStringLiteral ("WAV file \"%1\" has a truncated %2 chunk")
                      .arg (fileName, QString::fromLatin1 (chunkId)));
          }

        if (chunkId == "fmt ")
          {
            if (chunkSize < 16)
              {
                fail (QStringLiteral ("WAV file \"%1\" has an invalid fmt chunk").arg (fileName));
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

    if (!haveFmt || !haveData)
      {
        fail (QStringLiteral ("WAV file \"%1\" is missing fmt or data chunks").arg (fileName));
      }
    if (audioFormat != 1u || channels != 1u || bitsPerSample != 16u)
      {
        fail (QStringLiteral ("WAV file \"%1\" must be PCM mono 16-bit").arg (fileName));
      }

    int const sampleCount = dataChunk.size () / 2;
    QVector<short> samples (sampleCount);
    auto const* raw = reinterpret_cast<uchar const*> (dataChunk.constData ());
    for (int i = 0; i < sampleCount; ++i)
      {
        samples[i] = static_cast<short> (qFromLittleEndian<qint16> (raw + 2 * i));
      }

    WavData result;
    result.sampleRate = static_cast<int> (sampleRate);
    result.channels = static_cast<int> (channels);
    result.bitsPerSample = static_cast<int> (bitsPerSample);
    result.samples = std::move (samples);
    return result;
  }

  QVector<short> fit_audio (QVector<short> const& source, int requiredSamples)
  {
    QVector<short> audio (std::max (0, requiredSamples));
    int const copyCount = std::min (audio.size (), source.size ());
    if (copyCount > 0)
      {
        std::copy_n (source.constBegin (), copyCount, audio.begin ());
      }
    return audio;
  }

  LegacySpectra compute_jt9_spectra (QVector<short> const& audio, double trPeriod)
  {
    std::unique_ptr<dec_data_t> shared {new dec_data_t {}};
    shared->params.ndiskdat = true;

    int const copyCount = std::min<int> (audio.size (), NTMAX * RX_SAMPLE_RATE);
    if (copyCount > 0)
      {
        std::copy_n (audio.constBegin (), copyCount, shared->d2);
      }

    signed char lowSidelobes = 0;
    int ingain = 0;
    int nminw = 1;
    int nsps = kJt9Nsps;
    float pxdb = 0.0f;
    QVector<float> spectrum (NSMAX);
    float df3 = 0.0f;
    int ihsym = 0;
    int npts8 = 0;
    float pxdbmax = 0.0f;
    float npct = 0.0f;
    int nhsym = 0;
    int nhsym0 = -999;
    int const npts = input_sample_count (9, trPeriod);

    for (int block = 1; block <= npts / kJt9KStep; ++block)
      {
        int k = block * kJt9KStep;
        symspec_ (shared.get (), &k, &trPeriod, &nsps,
                  &ingain, &lowSidelobes, &nminw, &pxdb, spectrum.data (),
                  &df3, &ihsym, &npts8, &pxdbmax, &npct);
        nhsym = (k - 2048) / kJt9KStep;
        if (nhsym >= 1 && nhsym != nhsym0)
          {
            nhsym0 = nhsym;
            if (nhsym >= 181)
              {
                break;
              }
          }
      }

    LegacySpectra result;
    result.values.resize (kJt9SpectrumSize);
    std::copy_n (shared->ss, kJt9SpectrumSize, result.values.begin ());
    result.npts8 = npts8;
    result.nzhsym = std::max (0, nhsym0);
    return result;
  }

  params_block_t make_params (Options const& options, int mode, int nutc)
  {
    params_block_t params {};
    params.nutc = nutc;
    params.ndiskdat = true;
    params.ntrperiod = std::max (1, static_cast<int> (options.trPeriod));
    params.nQSOProgress = options.qsoProgress;
    params.nfqso = options.rxFrequency;
    params.newdat = true;
    params.npts8 = 74736;
    params.nfa = options.flow;
    params.nfSplit = options.fsplit;
    params.nfb = options.fhigh;
    params.ntol = options.tolerance;
    params.kin = mode == 240 ? 720000 : 64800;
    params.nzhsym = mode == 8 ? 50 : 0;
    params.nsubmode = options.subMode;
    params.nagain = false;
    params.ndepth = options.depth;
    params.lft8apon = true;
    params.lapcqonly = false;
    params.ljt65apon = true;
    params.napwid = 75;
    params.ntxmode = options.txJt9 ? 9 : 65;
    params.nmode = mode == 0 ? 74 : mode;
    params.nclearave = false;
    params.minSync = 0;
    params.minw = 0;
    params.emedelay = (mode == 66 && params.ntrperiod == 60) ? 2.5f : 0.0f;
    params.dttol = 3.0f;
    params.n2pass = 2;
    params.nranera = 6;
    params.naggressive = 0;
    params.nrobust = false;
    params.nexp_decode = options.experienceDecode;
    params.max_drift = 0;
    params.lmultift8 = options.multithreadFt8;
    params.lhideft8dupes = options.hideFt8Dupes;
    params.nft8cycles = options.mtCycles;
    params.nmt = options.mtNumThreads;
    params.nft8rxfsens = options.mtRxFreqSensitivity;
    params.ndecoderstart = options.mtStartTime;
    params.lwidedxcsearch = options.wideDxSearch;

    auto const myCall = to_fortran_field (options.myCall.toLatin1 (), 12);
    auto const myGrid = to_fortran_field (options.myGrid.toLatin1 (), 6);
    auto const hisCall = to_fortran_field (options.hisCall.toLatin1 (), 12);
    auto const hisGrid = to_fortran_field (options.hisGrid.toLatin1 (), 6);
    std::copy_n (myCall.constData (), 12, params.mycall);
    std::copy_n (myGrid.constData (), 6, params.mygrid);
    std::copy_n (hisCall.constData (), 12, params.hiscall);
    std::copy_n (hisGrid.constData (), 6, params.hisgrid);
    return params;
  }

  void finalize_options (Options& options)
  {
    if (options.myCall == QStringLiteral ("b"))
      {
        options.myCall.clear ();
      }
    if (options.hisCall == QStringLiteral ("b"))
      {
        options.hisCall.clear ();
        options.hisGrid.clear ();
      }

    if (options.mode == 241 || options.mode == 242)
      {
        options.tolerance = std::min (options.tolerance, 100);
      }
    else if (options.mode == 74 && !options.haveTolerance)
      {
        options.tolerance = 20;
      }
    else if (options.mode == 66 && !options.haveTolerance)
      {
        options.tolerance = 10;
      }
    else
      {
        options.tolerance = std::min (options.tolerance, 1000);
      }

    if (!options.experienceDecodeSet
        && (options.mode == 240 || options.mode == 241 || options.mode == 242))
      {
        options.experienceDecode = 3 * 256;
      }
  }

  Options parse_options (QCoreApplication& app)
  {
    QCommandLineParser parser;
    parser.setApplicationDescription (QStringLiteral ("Native C++ decoder frontend"));
    parser.addHelpOption ();
    parser.addVersionOption ();

    QCommandLineOption shmemOption ({QStringLiteral ("s"), QStringLiteral ("shmem")},
                                    QStringLiteral ("Use shared memory key"), QStringLiteral ("KEY"));
    QCommandLineOption trPeriodOption ({QStringLiteral ("p"), QStringLiteral ("tr-period")},
                                       QStringLiteral ("Tx/Rx period in seconds"),
                                       QStringLiteral ("SECONDS"),
                                       QStringLiteral ("60"));
    QCommandLineOption exePathOption ({QStringLiteral ("e"), QStringLiteral ("executable-path")},
                                      QStringLiteral ("Location of subordinate executables"),
                                      QStringLiteral ("PATH"),
                                      QStringLiteral ("."));
    QCommandLineOption dataPathOption ({QStringLiteral ("a"), QStringLiteral ("data-path")},
                                       QStringLiteral ("Location of writable data files"),
                                       QStringLiteral ("PATH"),
                                       QStringLiteral ("."));
    QCommandLineOption tempPathOption ({QStringLiteral ("t"), QStringLiteral ("temp-path")},
                                       QStringLiteral ("Temporary files path"),
                                       QStringLiteral ("PATH"),
                                       QStringLiteral ("."));
    QCommandLineOption lowestOption ({QStringLiteral ("L"), QStringLiteral ("lowest")},
                                     QStringLiteral ("Lowest decoded frequency"),
                                     QStringLiteral ("HERTZ"),
                                     QStringLiteral ("200"));
    QCommandLineOption highestOption ({QStringLiteral ("H"), QStringLiteral ("highest")},
                                      QStringLiteral ("Highest decoded frequency"),
                                      QStringLiteral ("HERTZ"),
                                      QStringLiteral ("4000"));
    QCommandLineOption splitOption ({QStringLiteral ("S"), QStringLiteral ("split")},
                                    QStringLiteral ("JT9 split frequency"),
                                    QStringLiteral ("HERTZ"),
                                    QStringLiteral ("2700"));
    QCommandLineOption rxFreqOption ({QStringLiteral ("f"), QStringLiteral ("rx-frequency")},
                                     QStringLiteral ("Receive frequency offset"),
                                     QStringLiteral ("HERTZ"),
                                     QStringLiteral ("1500"));
    QCommandLineOption tolOption ({QStringLiteral ("F"), QStringLiteral ("freq-tolerance")},
                                  QStringLiteral ("Frequency tolerance"),
                                  QStringLiteral ("HERTZ"),
                                  QStringLiteral ("20"));
    QCommandLineOption patienceOption ({QStringLiteral ("w"), QStringLiteral ("patience")},
                                       QStringLiteral ("FFTW patience"),
                                       QStringLiteral ("PATIENCE"),
                                       QStringLiteral ("1"));
    QCommandLineOption fftThreadsOption ({QStringLiteral ("m"), QStringLiteral ("fft-threads")},
                                         QStringLiteral ("FFT worker threads"),
                                         QStringLiteral ("THREADS"),
                                         QStringLiteral ("1"));
    QCommandLineOption multift8Option ({QStringLiteral ("M"), QStringLiteral ("multithreadft8")},
                                       QStringLiteral ("Use multithread FT8 decoder"));
    QCommandLineOption mtCyclesOption ({QStringLiteral ("C"), QStringLiteral ("MTft8-cycles")},
                                       QStringLiteral ("MT FT8 cycles"),
                                       QStringLiteral ("CYCLES"),
                                       QStringLiteral ("3"));
    QCommandLineOption hideDupesOption ({QStringLiteral ("U"), QStringLiteral ("MTft8-hidedupes")},
                                        QStringLiteral ("Hide MT FT8 duplicates"));
    QCommandLineOption rxSensOption ({QStringLiteral ("R"), QStringLiteral ("MTft8-RxFreqSens")},
                                     QStringLiteral ("MT FT8 RX frequency sensitivity"),
                                     QStringLiteral ("RXFREQSENS"),
                                     QStringLiteral ("3"));
    QCommandLineOption mtThreadsOption ({QStringLiteral ("N"), QStringLiteral ("MTft8-NumThreads")},
                                        QStringLiteral ("MT FT8 thread count"),
                                        QStringLiteral ("NUMTHREADS"),
                                        QStringLiteral ("0"));
    QCommandLineOption mtDecodeSensOption ({QStringLiteral ("E"), QStringLiteral ("MTft8-DecSens")},
                                           QStringLiteral ("MT FT8 decode sensitivity"),
                                           QStringLiteral ("SENSITIVITY"),
                                           QStringLiteral ("3"));
    QCommandLineOption mtStartOption ({QStringLiteral ("D"), QStringLiteral ("MTft8-StartTime")},
                                      QStringLiteral ("MT FT8 start time"),
                                      QStringLiteral ("START_TIME"),
                                      QStringLiteral ("3"));
    QCommandLineOption skipWideDxOption ({QStringLiteral ("Z"), QStringLiteral ("Skip-MTft8-WideDxCallSearch")},
                                         QStringLiteral ("Disable MT FT8 wide DX call search"));
    QCommandLineOption q65Option ({QStringLiteral ("3"), QStringLiteral ("q65")},
                                  QStringLiteral ("Q65 mode"));
    QCommandLineOption jt4Option ({QStringLiteral ("4"), QStringLiteral ("jt4")},
                                  QStringLiteral ("JT4 mode"));
    QCommandLineOption ft4Option ({QStringLiteral ("5"), QStringLiteral ("ft4")},
                                  QStringLiteral ("FT4 mode"));
    QCommandLineOption jt65Option ({QStringLiteral ("6"), QStringLiteral ("jt65")},
                                   QStringLiteral ("JT65 mode"));
    QCommandLineOption fst4Option ({QStringLiteral ("7"), QStringLiteral ("fst4")},
                                   QStringLiteral ("FST4 mode"));
    QCommandLineOption fst4wOption ({QStringLiteral ("W"), QStringLiteral ("fst4w")},
                                    QStringLiteral ("FST4W mode"));
    QCommandLineOption fst4wHashOption ({QStringLiteral ("Y"), QStringLiteral ("fst4w-hash22")},
                                        QStringLiteral ("FST4W mode with hash22 output"));
    QCommandLineOption ft8Option ({QStringLiteral ("8"), QStringLiteral ("ft8")},
                                  QStringLiteral ("FT8 mode"));
    QCommandLineOption jt9Option ({QStringLiteral ("9"), QStringLiteral ("jt9")},
                                  QStringLiteral ("JT9 mode"));
    QCommandLineOption quietOption ({QStringLiteral ("q"), QStringLiteral ("quiet")},
                                    QStringLiteral ("Suppress <DecodeFinished> marker"));
    QCommandLineOption mskOption ({QStringLiteral ("k"), QStringLiteral ("msk144")},
                                  QStringLiteral ("MSK144 mode"));
    QCommandLineOption qsoOption ({QStringLiteral ("Q"), QStringLiteral ("QSOprog")},
                                  QStringLiteral ("QSO progress"),
                                  QStringLiteral ("PROGRESS"),
                                  QStringLiteral ("0"));
    QCommandLineOption subModeOption ({QStringLiteral ("b"), QStringLiteral ("sub-mode")},
                                      QStringLiteral ("Sub-mode letter"),
                                      QStringLiteral ("A"),
                                      QStringLiteral ("A"));
    QCommandLineOption depthOption ({QStringLiteral ("d"), QStringLiteral ("depth")},
                                    QStringLiteral ("Decoding depth"),
                                    QStringLiteral ("DEPTH"),
                                    QStringLiteral ("1"));
    QCommandLineOption txJt9Option ({QStringLiteral ("T"), QStringLiteral ("tx-jt9")},
                                    QStringLiteral ("TX mode is JT9"));
    QCommandLineOption myCallOption ({QStringLiteral ("c"), QStringLiteral ("my-call")},
                                     QStringLiteral ("My callsign"),
                                     QStringLiteral ("CALL"),
                                     QStringLiteral ("K1ABC"));
    QCommandLineOption myGridOption ({QStringLiteral ("G"), QStringLiteral ("my-grid")},
                                     QStringLiteral ("My grid"),
                                     QStringLiteral ("GRID"));
    QCommandLineOption hisCallOption ({QStringLiteral ("x"), QStringLiteral ("his-call")},
                                      QStringLiteral ("His callsign"),
                                      QStringLiteral ("CALL"),
                                      QStringLiteral ("W9XYZ"));
    QCommandLineOption hisGridOption ({QStringLiteral ("g"), QStringLiteral ("his-grid")},
                                      QStringLiteral ("His grid"),
                                      QStringLiteral ("GRID"),
                                      QStringLiteral ("EN37"));
    QCommandLineOption expDecodeOption ({QStringLiteral ("X"), QStringLiteral ("experience-decode")},
                                        QStringLiteral ("Experience-based decode flags"),
                                        QStringLiteral ("FLAGS"),
                                        QStringLiteral ("0"));

    QList<QCommandLineOption> const optionsList {
      shmemOption, trPeriodOption, exePathOption, dataPathOption, tempPathOption,
      lowestOption, highestOption, splitOption, rxFreqOption, tolOption,
      patienceOption, fftThreadsOption, multift8Option, mtCyclesOption,
      hideDupesOption, rxSensOption, mtThreadsOption, mtDecodeSensOption,
      mtStartOption, skipWideDxOption, q65Option, jt4Option, ft4Option,
      jt65Option, fst4Option, fst4wOption, fst4wHashOption, ft8Option,
      jt9Option, quietOption, mskOption, qsoOption, subModeOption,
      depthOption, txJt9Option, myCallOption, myGridOption, hisCallOption,
      hisGridOption, expDecodeOption
    };

    for (auto const& option : optionsList)
      {
        parser.addOption (option);
      }

    parser.addPositionalArgument (QStringLiteral ("file"), QStringLiteral ("Input WAV file(s)"));
    parser.process (app);

    Options parsed;
    parsed.readFiles = !parser.isSet (shmemOption);
    parsed.shmKey = parser.value (shmemOption);
    parsed.trPeriod = parser.value (trPeriodOption).toDouble ();
    parsed.exeDir = parser.value (exePathOption);
    parsed.dataDir = parser.value (dataPathOption);
    parsed.tempDir = parser.value (tempPathOption);
    parsed.flow = parser.value (lowestOption).toInt ();
    parsed.fhigh = parser.value (highestOption).toInt ();
    parsed.fsplit = parser.value (splitOption).toInt ();
    parsed.rxFrequency = parser.value (rxFreqOption).toInt ();
    parsed.tolerance = parser.value (tolOption).toInt ();
    parsed.haveTolerance = parser.isSet (tolOption);
    parsed.patience = parser.value (patienceOption).toInt ();
    parsed.fftThreads = parser.value (fftThreadsOption).toInt ();
    parsed.multithreadFt8 = parser.isSet (multift8Option);
    parsed.mtCycles = parser.value (mtCyclesOption).toInt ();
    parsed.hideFt8Dupes = parser.isSet (hideDupesOption);
    parsed.mtRxFreqSensitivity = parser.value (rxSensOption).toInt ();
    parsed.mtNumThreads = parser.value (mtThreadsOption).toInt ();
    parsed.mtDecodeSensitivity = parser.value (mtDecodeSensOption).toInt ();
    parsed.mtStartTime = parser.value (mtStartOption).toInt ();
    parsed.wideDxSearch = !parser.isSet (skipWideDxOption);
    parsed.quiet = parser.isSet (quietOption);
    parsed.qsoProgress = parser.value (qsoOption).toInt ();
    parsed.depth = parser.value (depthOption).toInt ();
    parsed.txJt9 = parser.isSet (txJt9Option);
    parsed.myCall = parser.value (myCallOption);
    parsed.myGrid = parser.value (myGridOption);
    parsed.hisCall = parser.value (hisCallOption);
    parsed.hisGrid = parser.value (hisGridOption);
    parsed.experienceDecode = parser.value (expDecodeOption).toInt ();
    parsed.experienceDecodeSet = parser.isSet (expDecodeOption);

    QString const subModeValue = parser.value (subModeOption).trimmed ();
    if (!subModeValue.isEmpty ())
      {
        parsed.subMode = std::max (0, subModeValue.at (0).toLatin1 () - 'A');
      }

    QList<int> requestedModes;
    auto addMode = [&requestedModes] (bool enabled, int value) {
      if (enabled)
        {
          requestedModes.push_back (value);
        }
    };
    addMode (parser.isSet (q65Option), 66);
    addMode (parser.isSet (jt4Option), 4);
    addMode (parser.isSet (ft4Option), 5);
    addMode (parser.isSet (jt65Option), 65);
    addMode (parser.isSet (fst4Option), 240);
    addMode (parser.isSet (fst4wOption), 241);
    addMode (parser.isSet (fst4wHashOption), 242);
    addMode (parser.isSet (ft8Option), 8);
    addMode (parser.isSet (jt9Option), 9);
    addMode (parser.isSet (mskOption), 144);

    if (parser.isSet (jt65Option) && parser.isSet (jt9Option))
      {
        requestedModes.removeAll (65);
        requestedModes.removeAll (9);
        requestedModes.push_back (74);
      }

    if (requestedModes.size () > 1)
      {
        fail (QStringLiteral ("conflicting mode flags selected"));
      }
    if (!requestedModes.isEmpty ())
      {
        parsed.mode = requestedModes.front ();
      }

    finalize_options (parsed);

    QStringList const positional = parser.positionalArguments ();
    parsed.files = positional;
    if (parsed.readFiles)
      {
        if (positional.isEmpty ())
          {
            fail (QStringLiteral ("no input WAV files provided"));
          }
      }
    else
      {
        if (parsed.shmKey.isEmpty ())
          {
            fail (QStringLiteral ("shared memory key is required with -s"));
          }
        if (!positional.isEmpty ())
          {
            fail (QStringLiteral ("positional WAV files are not allowed with -s"));
          }
      }

    return parsed;
  }

  template <typename Worker, typename Signal, typename Request>
  QStringList run_worker (Worker& worker, Signal signal, Request const& request)
  {
    QStringList rows;
    quint64 const serial = request.serial;
    auto const connection = QObject::connect (&worker, signal, &worker,
                                              [&rows, &serial] (quint64 readySerial,
                                                                QStringList const& readyRows) {
      if (readySerial == serial)
        {
          rows = readyRows;
        }
    });
    worker.decode (request);
    QObject::disconnect (connection);
    return rows;
  }

  QStringList decode_legacy_mode (params_block_t const& params, QVector<short> const& audio,
                                  QVector<float> const& spectra, QString const& tempDir, int mode)
  {
    decodium::legacyjt::LegacyJtDecodeWorker worker;
    decodium::legacyjt::DecodeRequest request;
    request.serial = 1;
    request.mode = mode == 4 ? QStringLiteral ("JT4")
                             : mode == 65 ? QStringLiteral ("JT65")
                                          : QStringLiteral ("JT9");
    request.audio = audio;
    request.ss = spectra;
    request.npts8 = params.npts8;
    request.nzhsym = params.nzhsym;
    request.nutc = params.nutc;
    request.nfqso = params.nfqso;
    request.ntol = params.ntol;
    request.ndepth = params.ndepth;
    request.nfa = params.nfa;
    request.nfb = params.nfb;
    request.nfsplit = params.nfSplit;
    request.nsubmode = params.nsubmode;
    request.nclearave = params.nclearave ? 1 : 0;
    request.minsync = params.minSync;
    request.minw = params.minw;
    request.emedelay = params.emedelay;
    request.dttol = params.dttol;
    request.newdat = params.newdat ? 1 : 0;
    request.nagain = params.nagain ? 1 : 0;
    request.n2pass = params.n2pass;
    request.nrobust = params.nrobust ? 1 : 0;
    request.ntrials = 1000;
    request.naggressive = params.naggressive;
    request.nexp_decode = params.nexp_decode;
    request.nqsoprogress = params.nQSOProgress;
    request.ljt65apon = params.ljt65apon ? 1 : 0;
    request.mycall = QByteArray {params.mycall, 12};
    request.hiscall = QByteArray {params.hiscall, 12};
    request.hisgrid = QByteArray {params.hisgrid, 6};
    request.tempDir = tempDir.toLocal8Bit ();
    return run_worker (worker, &decodium::legacyjt::LegacyJtDecodeWorker::decodeReady, request);
  }

  QStringList decode_dual_jt65_jt9 (params_block_t const& params, QVector<short> const& audio,
                                    QVector<float> const& spectra, QString const& tempDir)
  {
    QStringList rows;
    int const firstMode = params.ntxmode == 9 ? 9 : 65;
    int const secondMode = firstMode == 9 ? 65 : 9;
    rows << decode_legacy_mode (params, audio, firstMode == 9 ? spectra : QVector<float> {}, tempDir, firstMode);
    rows << decode_legacy_mode (params, audio, secondMode == 9 ? spectra : QVector<float> {}, tempDir, secondMode);
    return rows;
  }

	  QStringList decode_q65_mode (params_block_t const& params, QVector<short> const& audio)
  {
    decodium::q65::Q65DecodeWorker worker;
    decodium::q65::DecodeRequest request;
    request.serial = 1;
    request.audio = audio;
    request.nqd0 = 1;
    request.nutc = params.nutc;
    request.ntrperiod = params.ntrperiod;
    request.nsubmode = params.nsubmode;
    request.nfqso = params.nfqso;
    request.ntol = params.ntol;
    request.ndepth = params.ndepth;
    request.nfa = params.nfa;
    request.nfb = params.nfb;
    request.nclearave = params.nclearave ? 1 : 0;
    request.single_decode = (params.nexp_decode & 32) ? 1 : 0;
    request.nagain = params.nagain ? 1 : 0;
    request.max_drift = params.max_drift;
    request.lnewdat = params.newdat ? 1 : 0;
    request.emedelay = params.emedelay;
    request.mycall = QByteArray {params.mycall, 12};
    request.hiscall = QByteArray {params.hiscall, 12};
    request.hisgrid = QByteArray {params.hisgrid, 6};
    request.nqsoprogress = params.nQSOProgress;
    request.ncontest = params.nexp_decode & 7;
    request.lapcqonly = params.lapcqonly ? 1 : 0;
	    return run_worker (worker, &decodium::q65::Q65DecodeWorker::decodeReady, request);
	  }

	  QStringList decode_ft2_mode (params_block_t const& params, QVector<short> const& audio)
	  {
	    decodium::ft2::FT2DecodeWorker worker;
	    decodium::ft2::DecodeRequest request;
	    request.serial = 1;
	    request.audio = audio;
	    request.nutc = qMax (0, int (params.nutc));
	    request.nqsoprogress = qBound (0, int (params.nQSOProgress), 6);
	    request.nfqso = qBound (0, int (params.nfqso), 5000);
	    request.nfa = qBound (0, int (params.nfa), 5000);
	    request.nfb = qMax (request.nfa + 50, qBound (0, int (params.nfb), 5000));
	    request.ndepth = qMax (1, int (params.ndepth));
	    request.ncontest = qBound (0, int (params.nexp_decode & 7), 16);
	    request.mycall = QByteArray {params.mycall, 12};
	    request.hiscall = QByteArray {params.hiscall, 12};
	    worker.markLatestDecodeSerial (request.serial);
	    return run_worker (worker, &decodium::ft2::FT2DecodeWorker::decodeReady, request);
	  }

	  QStringList decode_ft4_mode (params_block_t const& params, QVector<short> const& audio)
	  {
	    decodium::ft4::FT4DecodeWorker worker;
	    decodium::ft4::DecodeRequest request;
	    request.serial = 1;
	    request.audio = audio;
	    request.nutc = qMax (0, int (params.nutc));
	    request.nqsoprogress = qBound (0, int (params.nQSOProgress), 6);
	    request.nfqso = qBound (0, int (params.nfqso), 5000);
	    request.nfa = qBound (0, int (params.nfa), 5000);
	    request.nfb = qMax (request.nfa + 50, qBound (0, int (params.nfb), 5000));
	    request.ndepth = qBound (1, int (params.ndepth), 4);
	    request.ncontest = qBound (0, int (params.nexp_decode & 7), 16);
	    request.mycall = QByteArray {params.mycall, 12};
	    request.hiscall = QByteArray {params.hiscall, 12};
	    return run_worker (worker, &decodium::ft4::FT4DecodeWorker::decodeReady, request);
	  }

	  QStringList decode_ft8_mode (params_block_t const& params, QVector<short> const& audio)
	  {
	    decodium::ft8::FT8DecodeWorker worker;
	    decodium::ft8::DecodeRequest request;
	    request.serial = 1;
	    request.audio = audio;
	    request.nqsoprogress = qBound (0, int (params.nQSOProgress), 6);
	    request.nfqso = qBound (0, int (params.nfqso), 5000);
	    request.nftx = qBound (0, int (params.nftx), 5000);
	    request.nutc = qMax (0, int (params.nutc));
	    request.nfa = qBound (0, int (params.nfa), 5000);
	    request.nfb = qMax (request.nfa + 50, qBound (0, int (params.nfb), 5000));
	    request.nzhsym = qBound (41, int (params.nzhsym), 50);
	    request.ndepth = qBound (1, int (params.ndepth), 4);
	    request.emedelay = params.emedelay;
	    request.ncontest = qBound (0, int (params.nexp_decode & 7), 16);
	    request.nagain = params.nagain ? 1 : 0;
	    request.lft8apon = params.lft8apon ? 1 : 0;
	    request.lapcqonly = params.lapcqonly ? 1 : 0;
	    request.napwid = qBound (0, int (params.napwid), 200);
	    request.ldiskdat = params.ndiskdat ? 1 : 0;
	    request.mycall = QByteArray {params.mycall, 12};
	    request.hiscall = QByteArray {params.hiscall, 12};
	    request.hisgrid = QByteArray {params.hisgrid, 6};
	    return run_worker (worker, &decodium::ft8::FT8DecodeWorker::decodeReady, request);
	  }

	  QStringList decode_fst4_mode (params_block_t const& params, QVector<short> const& audio)
	  {
    decodium::fst4::FST4DecodeWorker worker;
    decodium::fst4::DecodeRequest request;
    request.serial = 1;
    request.mode = params.nmode == 240 ? QStringLiteral ("FST4") : QStringLiteral ("FST4W");
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
    request.mycall = QByteArray {params.mycall, 12};
    request.hiscall = QByteArray {params.hiscall, 12};
    request.iwspr = params.nmode == 241 || params.nmode == 242;
    request.lprinthash22 = params.nmode == 242;
    return run_worker (worker, &decodium::fst4::FST4DecodeWorker::decodeReady, request);
  }

  QStringList decode_msk144_mode (params_block_t const& params, QVector<short> const& audio)
  {
    decodium::msk144::MSK144DecodeWorker worker;
    decodium::msk144::DecodeRequest request;
    request.serial = 1;
    request.audio = audio;
    request.nutc = params.nutc;
    request.kdone = audio.size ();
    request.nsubmode = params.nsubmode;
    request.newdat = params.newdat ? 1 : 0;
    request.minsync = params.minSync;
    request.npick = 0;
    request.t0_ms = 0;
    request.t1_ms = static_cast<int> (audio.size () * 1000.0 / RX_SAMPLE_RATE);
    request.maxlines = 50;
    request.rxfreq = params.nfqso;
    request.ftol = params.ntol;
    request.aggressive = params.naggressive;
    request.trperiod = params.ntrperiod;
    request.mycall = QByteArray {params.mycall, 12};
    request.hiscall = QByteArray {params.hiscall, 12};
    return run_worker (worker, &decodium::msk144::MSK144DecodeWorker::decodeReady, request);
  }

	  int run_native_ftx (params_block_t params, QVector<short> const& audio,
	                      QString const& tempDir, bool quiet)
	  {
	    Q_UNUSED (tempDir);
	    QStringList rows;

	    if (params.nmode == 8 && params.ndiskdat && !params.nagain)
	      {
	        QVector<short> early = audio;
	        for (int nearly : {41, 47})
          {
            params_block_t earlyParams = params;
            earlyParams.nzhsym = nearly;
            int const keep = nearly * 3456;
	            if (keep < early.size ())
	              {
	                std::fill (early.begin () + keep, early.end (), 0);
	              }
	            rows << decode_ft8_mode (earlyParams, early);
	            early = audio;
	          }
	      }

	    switch (params.nmode)
	      {
	      case 2:
	        rows << decode_ft2_mode (params, audio);
	        break;
	      case 5:
	        rows << decode_ft4_mode (params, audio);
	        break;
	      case 8:
	        rows << decode_ft8_mode (params, audio);
	        break;
	      case 66:
	        rows << decode_q65_mode (params, audio);
	        break;
	      default:
	        fail (QStringLiteral ("unsupported native FTX mode %1").arg (params.nmode));
	      }
	    emit_rows (rows, quiet);
	    return rows.size ();
	  }

  QStringList decode_via_workers (params_block_t const& params, QVector<short> const& audio,
                                  QVector<float> const& spectra, QString const& tempDir)
  {
    switch (params.nmode)
      {
      case 4:
      case 65:
      case 9:
        return decode_legacy_mode (params, audio, spectra, tempDir, params.nmode);
      case 74:
        return decode_dual_jt65_jt9 (params, audio, spectra, tempDir);
      case 66:
        return decode_q65_mode (params, audio);
      case 240:
      case 241:
      case 242:
        return decode_fst4_mode (params, audio);
      case 144:
        return decode_msk144_mode (params, audio);
      default:
        fail (QStringLiteral ("unsupported mode %1").arg (params.nmode));
      }
    return {};
  }

  int decode_from_params (params_block_t const& params, QVector<short> const& audio,
                          QVector<float> const& spectra, QString const& tempDir, bool quiet)
  {
    if (params.nmode == 5 || params.nmode == 8 || params.nmode == 66)
      {
        return run_native_ftx (params, audio, tempDir, quiet);
      }

    QStringList const rows = decode_via_workers (params, audio, spectra, tempDir);
    emit_rows (rows, quiet);
    return rows.size ();
  }

  params_block_t make_snapshot_params (params_block_t const& rawParams)
  {
    params_block_t params = rawParams;
    if (params.nmode == 0)
      {
        params.nmode = 74;
      }
    return params;
  }

  bool wait_for_lock (SharedMemorySegment& segment)
  {
    for (int attempt = 0; attempt < kShmemRetryCount; ++attempt)
      {
        if (segment.lock ())
          {
            return true;
          }
        std::this_thread::sleep_for (std::chrono::milliseconds (kShmemWaitMs));
      }
    return false;
  }

  void run_shared_memory_mode (Options const& options)
  {
    SharedMemorySegment segment {options.shmKey};
    for (int attempt = 0; attempt < kShmemRetryCount; ++attempt)
      {
        if (segment.attach ())
          {
            break;
          }
        std::this_thread::sleep_for (std::chrono::milliseconds (kShmemWaitMs));
        if (attempt + 1 == kShmemRetryCount)
          {
            fail (QStringLiteral ("shared-memory attach failed for key \"%1\": %2")
                      .arg (options.shmKey, segment.errorString ()));
          }
      }

    while (true)
      {
        if (!wait_for_lock (segment))
          {
            fail (QStringLiteral ("shared-memory lock failed while waiting for work: %1")
                      .arg (segment.errorString ()));
          }

        auto* shared = reinterpret_cast<dec_data_t*> (segment.data ());
        if (!shared)
          {
            segment.unlock ();
            fail (QStringLiteral ("shared memory is not mapped"));
          }

        if (shared->ipc[1] == 999)
          {
            segment.unlock ();
            segment.detach ();
            return;
          }

        if (shared->ipc[1] != 1)
          {
            segment.unlock ();
            std::this_thread::sleep_for (std::chrono::milliseconds (kShmemWaitMs));
            continue;
          }

        params_block_t const params = make_snapshot_params (shared->params);
        int const requiredSamples = input_sample_count (params.nmode, params.ntrperiod);
        QVector<short> audio (requiredSamples);
        if (requiredSamples > 0)
          {
            std::copy_n (shared->d2, requiredSamples, audio.begin ());
          }

        QVector<float> spectra;
        if (params.nmode == 9 || params.nmode == 74)
          {
            spectra.resize (kJt9SpectrumSize);
            std::copy_n (shared->ss, kJt9SpectrumSize, spectra.begin ());
          }

        shared->ipc[1] = 0;
        if (!segment.unlock ())
          {
            fail (QStringLiteral ("shared-memory unlock failed after parameter copy: %1")
                      .arg (segment.errorString ()));
          }

        decode_from_params (params, audio, spectra, options.tempDir, options.quiet);

        while (true)
          {
            if (!wait_for_lock (segment))
              {
                fail (QStringLiteral ("shared-memory lock failed while waiting for acknowledgement: %1")
                          .arg (segment.errorString ()));
              }
            auto* ackShared = reinterpret_cast<dec_data_t*> (segment.data ());
            if (ackShared && ackShared->ipc[2] == 1)
              {
                ackShared->ipc[2] = 0;
                segment.unlock ();
                break;
              }
            segment.unlock ();
            std::this_thread::sleep_for (std::chrono::milliseconds (kShmemWaitMs));
          }
      }
  }

  void run_file_mode (Options const& options, QStringList const& files)
  {
    for (auto const& fileName : files)
      {
        WavData const wav = read_wav_file (fileName);
        int const selectedMode = options.mode;
        int const effectiveMode = selectedMode == 0 ? 74 : selectedMode;
        int const requiredSamples = input_sample_count (effectiveMode, options.trPeriod);
        QVector<short> const audio = fit_audio (wav.samples, requiredSamples);
        int const nutc = extract_utc_from_filename (fileName);
        params_block_t params = make_params (options, effectiveMode, nutc);

        QVector<float> spectra;
        if (effectiveMode == 9 || effectiveMode == 74)
          {
            LegacySpectra const computed = compute_jt9_spectra (audio, options.trPeriod);
            spectra = computed.values;
            params.npts8 = computed.npts8;
            params.nzhsym = computed.nzhsym;
          }

        decode_from_params (params, audio, spectra, options.tempDir, options.quiet);
      }
  }
}

int main (int argc, char * argv[])
try
{
  QCoreApplication app {argc, argv};
  app.setApplicationName (QStringLiteral ("jt9"));
  app.setApplicationVersion (fork_release_version ());

  Options const options = parse_options (app);
  if (options.readFiles)
    {
      run_file_mode (options, options.files);
    }
  else
    {
      run_shared_memory_mode (options);
    }
  return 0;
}
catch (std::exception const& error)
{
  std::cerr << error.what () << '\n';
  return 1;
}
