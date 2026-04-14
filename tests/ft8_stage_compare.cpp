#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutexLocker>
#include <QSet>
#include <QStringList>
#include <QTextStream>
#include <QtEndian>

#include "Detector/FortranRuntimeGuard.hpp"

extern "C"
{
  void ftx_ft8_async_decode_stage4_c (short const* iwave, int* nqsoprogress, int* nfqso, int* nftx,
                                      int* nutc, int* nfa, int* nfb, int* nzhsym, int* ndepth,
                                      float* emedelay, int* ncontest, int* nagain,
                                      int* lft8apon, int* ltry_a8, int* lapcqonly, int* napwid,
                                      char const* mycall, char const* hiscall,
                                      char const* hisgrid, int* ldiskdat, float* syncs, int* snrs,
                                      float* dts, float* freqs, int* naps, float* quals,
                                      signed char* bits77, char* decodeds, int* nout);
  void ftx_ft8_cpp_dsp_rollout_stage_override_c (int stage);
  void ftx_ft8_cpp_dsp_rollout_stage_reset_c ();
  void ftx_ft8_stage4_reset_c ();
}

namespace
{
  constexpr int kFt8SampleCount {180000};
  constexpr int kFt8MaxLines {200};
  constexpr int kBitsPerMessage {77};
  constexpr int kDecodedChars {37};

  struct WavData
  {
    int sample_rate {};
    int channels {};
    int bits_per_sample {};
    int source_samples {};
    bool padded {};
    bool truncated {};
    std::vector<short> samples;
  };

  struct DecodeLine
  {
    int snr {};
    float dt {};
    float freq {};
    int nap {};
    float qual {};
    QString decoded;
  };

  struct DecodeResult
  {
    int stage {};
    std::vector<DecodeLine> lines;
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

  QByteArray trim_fortran_field (char const* data, int width)
  {
    QByteArray field {data, width};
    while (!field.isEmpty () && (field.back () == ' ' || field.back () == '\0'))
      {
        field.chop (1);
      }
    return field;
  }

  QString decode_key (DecodeLine const& line)
  {
    return QStringLiteral ("%1|%2")
        .arg (qRound (line.freq), 5)
        .arg (line.decoded.trimmed ());
  }

  QString message_key (DecodeLine const& line)
  {
    return line.decoded.trimmed ();
  }

  QString describe_line (DecodeLine const& line)
  {
    return QStringLiteral ("snr=%1 dt=%2 freq=%3 nap=%4 qual=%5 decoded=\"%6\"")
        .arg (line.snr, 4)
        .arg (line.dt, 0, 'f', 1)
        .arg (qRound (line.freq), 5)
        .arg (line.nap, 2)
        .arg (line.qual, 0, 'f', 3)
        .arg (line.decoded.trimmed ());
  }

  QList<int> parse_stages (QString const& raw)
  {
    QList<int> stages;
    QSet<int> seen;
    for (QString part : raw.split (QLatin1Char {','}, Qt::SkipEmptyParts))
      {
        part = part.trimmed ();
        bool ok = false;
        int const stage = part.toInt (&ok);
        if (!ok || stage < 0 || stage > 4)
          {
            fail (QStringLiteral ("invalid FT8 stage \"%1\"; expected values in 0..4").arg (part));
          }
        if (!seen.contains (stage))
          {
            seen.insert (stage);
            stages.append (stage);
          }
      }
    if (stages.isEmpty ())
      {
        fail (QStringLiteral ("no FT8 stages selected"));
      }
    return stages;
  }

  int parse_int_option (QCommandLineParser const& parser, QCommandLineOption const& option,
                        QString const& name)
  {
    bool ok = false;
    int const value = parser.value (option).toInt (&ok);
    if (!ok)
      {
        fail (QStringLiteral ("invalid --%1 value").arg (name));
      }
    return value;
  }

  float parse_float_option (QCommandLineParser const& parser, QCommandLineOption const& option,
                            QString const& name)
  {
    bool ok = false;
    float const value = parser.value (option).toFloat (&ok);
    if (!ok)
      {
        fail (QStringLiteral ("invalid --%1 value").arg (name));
      }
    return value;
  }

  WavData read_wav_file (QString const& file_name)
  {
    QFile file {file_name};
    if (!file.open (QIODevice::ReadOnly))
      {
        fail (QStringLiteral ("cannot open WAV file \"%1\": %2")
                  .arg (file_name, file.errorString ()));
      }

    QByteArray const blob = file.readAll ();
    if (blob.size () < 12)
      {
        fail (QStringLiteral ("WAV file \"%1\" is too short").arg (file_name));
      }
    if (blob.mid (0, 4) != "RIFF" || blob.mid (8, 4) != "WAVE")
      {
        fail (QStringLiteral ("WAV file \"%1\" is not RIFF/WAVE").arg (file_name));
      }

    bool have_fmt = false;
    bool have_data = false;
    quint16 audio_format = 0;
    quint16 channels = 0;
    quint32 sample_rate = 0;
    quint16 bits_per_sample = 0;
    QByteArray sample_bytes;

    int pos = 12;
    while (pos + 8 <= blob.size ())
      {
        QByteArray const chunk_id = blob.mid (pos, 4);
        quint32 const chunk_size = qFromLittleEndian<quint32> (
            reinterpret_cast<uchar const*> (blob.constData () + pos + 4));
        pos += 8;
        if (pos + static_cast<int> (chunk_size) > blob.size ())
          {
            fail (QStringLiteral ("WAV file \"%1\" has a truncated %2 chunk")
                      .arg (file_name, QString::fromLatin1 (chunk_id)));
          }

        if (chunk_id == "fmt ")
          {
            if (chunk_size < 16)
              {
                fail (QStringLiteral ("WAV file \"%1\" has an invalid fmt chunk").arg (file_name));
              }
            uchar const* fmt = reinterpret_cast<uchar const*> (blob.constData () + pos);
            audio_format = qFromLittleEndian<quint16> (fmt);
            channels = qFromLittleEndian<quint16> (fmt + 2);
            sample_rate = qFromLittleEndian<quint32> (fmt + 4);
            bits_per_sample = qFromLittleEndian<quint16> (fmt + 14);
            have_fmt = true;
          }
        else if (chunk_id == "data")
          {
            sample_bytes = blob.mid (pos, static_cast<int> (chunk_size));
            have_data = true;
          }

        pos += static_cast<int> ((chunk_size + 1u) & ~1u);
      }

    if (!have_fmt || !have_data)
      {
        fail (QStringLiteral ("WAV file \"%1\" is missing fmt or data chunks").arg (file_name));
      }
    if (audio_format != 1u)
      {
        fail (QStringLiteral ("WAV file \"%1\" is not PCM (format=%2)")
                  .arg (file_name)
                  .arg (audio_format));
      }
    if (channels != 1u)
      {
        fail (QStringLiteral ("WAV file \"%1\" must be mono; got %2 channels")
                  .arg (file_name)
                  .arg (channels));
      }
    if (sample_rate != 12000u)
      {
        fail (QStringLiteral ("WAV file \"%1\" must be 12000 Hz; got %2 Hz")
                  .arg (file_name)
                  .arg (sample_rate));
      }
    if (bits_per_sample != 16u)
      {
        fail (QStringLiteral ("WAV file \"%1\" must be 16-bit PCM; got %2 bits")
                  .arg (file_name)
                  .arg (bits_per_sample));
      }

    int const sample_count = sample_bytes.size () / static_cast<int> (sizeof (qint16));
    WavData wav;
    wav.sample_rate = static_cast<int> (sample_rate);
    wav.channels = static_cast<int> (channels);
    wav.bits_per_sample = static_cast<int> (bits_per_sample);
    wav.source_samples = sample_count;
    wav.padded = sample_count < kFt8SampleCount;
    wav.truncated = sample_count > kFt8SampleCount;
    wav.samples.assign (static_cast<size_t> (kFt8SampleCount), 0);

    int const copy_count = std::min (sample_count, kFt8SampleCount);
    uchar const* input = reinterpret_cast<uchar const*> (sample_bytes.constData ());
    for (int i = 0; i < copy_count; ++i)
      {
        wav.samples[static_cast<size_t> (i)] =
            qFromLittleEndian<qint16> (input + i * static_cast<int> (sizeof (qint16)));
      }
    return wav;
  }

  DecodeResult run_decode (std::vector<short> const& samples, int stage, int nqsoprogress,
                           int nfqso, int nftx, int nutc, int nfa, int nfb, int nzhsym,
                           int ndepth, float emedelay, int ncontest, int nagain,
                           int lft8apon, int lapcqonly, int napwid, int ldiskdat,
                           QByteArray const& mycall, QByteArray const& hiscall,
                           QByteArray const& hisgrid)
  {
    std::vector<short> iwave = samples;
    std::array<int, kFt8MaxLines> snrs {};
    std::array<float, kFt8MaxLines> syncs {};
    std::array<float, kFt8MaxLines> dts {};
    std::array<float, kFt8MaxLines> freqs {};
    std::array<int, kFt8MaxLines> naps {};
    std::array<float, kFt8MaxLines> quals {};
    std::array<signed char, kBitsPerMessage * kFt8MaxLines> bits77 {};
    std::array<char, kFt8MaxLines * kDecodedChars> decodeds {};
    int nout = 0;
    QByteArray mycall_field = to_fortran_field (mycall, 12);
    QByteArray hiscall_field = to_fortran_field (hiscall, 12);
    QByteArray hisgrid_field = to_fortran_field (hisgrid, 6);
    DecodeResult result;
    result.stage = stage;
    result.lines.reserve (static_cast<size_t> (kFt8MaxLines));
    QSet<QString> seen_keys;

    auto collect_step_lines = [&] (int step_nout) {
      for (int i = 0; i < std::max (0, step_nout) && i < kFt8MaxLines; ++i)
        {
          DecodeLine line;
          line.snr = snrs[i];
          line.dt = dts[i];
          line.freq = freqs[i];
          line.nap = naps[i];
          line.qual = quals[i];
          line.decoded = QString::fromLatin1 (
              trim_fortran_field (decodeds.data () + i * kDecodedChars, kDecodedChars));
          QString const key = decode_key (line);
          if (seen_keys.contains (key))
            {
              continue;
            }
          seen_keys.insert (key);
          result.lines.push_back (line);
        }
    };

    auto invoke_decode = [&] (int nzhsym_step) {
      int step_nqsoprogress = nqsoprogress;
      int step_nfqso = nfqso;
      int step_nftx = nftx;
      int step_nutc = nutc;
      int step_nfa = nfa;
      int step_nfb = nfb;
      int step_nzhsym = nzhsym_step;
      int step_ndepth = ndepth;
      float step_emedelay = emedelay;
      int step_ncontest = ncontest;
      int step_nagain = nagain;
      int step_lft8apon = lft8apon;
      int step_ltry_a8 = nzhsym_step == 41 ? 1 : 0;
      int step_lapcqonly = lapcqonly;
      int step_napwid = napwid;
      int step_ldiskdat = ldiskdat;
      int step_nout = 0;

      ftx_ft8_async_decode_stage4_c (iwave.data (), &step_nqsoprogress, &step_nfqso,
                                     &step_nftx, &step_nutc, &step_nfa, &step_nfb,
                                     &step_nzhsym, &step_ndepth, &step_emedelay,
                                     &step_ncontest, &step_nagain, &step_lft8apon,
                                     &step_ltry_a8, &step_lapcqonly, &step_napwid,
                                     mycall_field.constData (),
                                     hiscall_field.constData (), hisgrid_field.constData (),
                                     &step_ldiskdat, syncs.data (), snrs.data (), dts.data (),
                                     freqs.data (), naps.data (), quals.data (),
                                     bits77.data (), decodeds.data (), &step_nout);
      collect_step_lines (step_nout);
      return step_nout;
    };

    QMutexLocker locker {&decodium::fortran::runtime_mutex ()};
    ftx_ft8_stage4_reset_c ();
    if (nzhsym >= 50)
      {
        invoke_decode (41);
        invoke_decode (47);
      }
    nout = invoke_decode (nzhsym);
    ftx_ft8_stage4_reset_c ();
    locker.unlock ();

    if (result.lines.empty () && nout > 0)
      {
        collect_step_lines (nout);
      }
    return result;
  }

  QHash<QString, QString> line_map (DecodeResult const& result)
  {
    QHash<QString, QString> map;
    for (DecodeLine const& line : result.lines)
      {
        QString const key = decode_key (line);
        if (!map.contains (key))
          {
            map.insert (key, describe_line (line));
          }
      }
    return map;
  }

  QHash<QString, QString> message_map (DecodeResult const& result)
  {
    QHash<QString, QString> map;
    for (DecodeLine const& line : result.lines)
      {
        QString const key = message_key (line);
        if (!map.contains (key))
          {
            map.insert (key, describe_line (line));
          }
      }
    return map;
  }

  void print_result (QTextStream& out, DecodeResult const& result)
  {
    out << "stage " << result.stage << ": nout=" << result.lines.size () << '\n';
    if (result.lines.empty ())
      {
        out << "  (no decodes)\n";
        return;
      }
    for (DecodeLine const& line : result.lines)
      {
        out << "  - " << describe_line (line) << '\n';
      }
  }

  void print_comparison (QTextStream& out, DecodeResult const& lhs, DecodeResult const& rhs)
  {
    QHash<QString, QString> const left_map = line_map (lhs);
    QHash<QString, QString> const right_map = line_map (rhs);
    QSet<QString> const left_keys = QSet<QString> (left_map.keyBegin (), left_map.keyEnd ());
    QSet<QString> const right_keys = QSet<QString> (right_map.keyBegin (), right_map.keyEnd ());
    QSet<QString> const common = left_keys & right_keys;
    QSet<QString> const only_left = left_keys - right_keys;
    QSet<QString> const only_right = right_keys - left_keys;
    QStringList only_left_sorted = QStringList (only_left.values ());
    QStringList only_right_sorted = QStringList (only_right.values ());
    std::sort (only_left_sorted.begin (), only_left_sorted.end ());
    std::sort (only_right_sorted.begin (), only_right_sorted.end ());

    out << "stage " << lhs.stage << " vs stage " << rhs.stage
        << ": common=" << common.size ()
        << " only_" << lhs.stage << '=' << only_left.size ()
        << " only_" << rhs.stage << '=' << only_right.size ()
        << '\n';

    for (QString const& key : only_left_sorted)
      {
        out << "  missing in stage " << rhs.stage << ": " << left_map.value (key) << '\n';
      }
    for (QString const& key : only_right_sorted)
      {
        out << "  extra in stage " << rhs.stage << ": " << right_map.value (key) << '\n';
      }

    QHash<QString, QString> const left_message_map = message_map (lhs);
    QHash<QString, QString> const right_message_map = message_map (rhs);
    QSet<QString> const left_message_keys = QSet<QString> (left_message_map.keyBegin (),
                                                           left_message_map.keyEnd ());
    QSet<QString> const right_message_keys = QSet<QString> (right_message_map.keyBegin (),
                                                            right_message_map.keyEnd ());
    QSet<QString> const common_messages = left_message_keys & right_message_keys;
    QSet<QString> const only_left_messages = left_message_keys - right_message_keys;
    QSet<QString> const only_right_messages = right_message_keys - left_message_keys;
    QStringList only_left_messages_sorted = QStringList (only_left_messages.values ());
    QStringList only_right_messages_sorted = QStringList (only_right_messages.values ());
    std::sort (only_left_messages_sorted.begin (), only_left_messages_sorted.end ());
    std::sort (only_right_messages_sorted.begin (), only_right_messages_sorted.end ());

    out << "message parity stage " << lhs.stage << " vs stage " << rhs.stage
        << ": common=" << common_messages.size ()
        << " only_" << lhs.stage << '=' << only_left_messages.size ()
        << " only_" << rhs.stage << '=' << only_right_messages.size ()
        << '\n';

    for (QString const& key : only_left_messages_sorted)
      {
        out << "  message missing in stage " << rhs.stage << ": "
            << left_message_map.value (key) << '\n';
      }
    for (QString const& key : only_right_messages_sorted)
      {
        out << "  message extra in stage " << rhs.stage << ": "
            << right_message_map.value (key) << '\n';
      }
  }
}

int main (int argc, char * argv[])
{
  try
    {
      QCoreApplication app {argc, argv};
      QCoreApplication::setApplicationName (QStringLiteral ("ft8_stage_compare"));
      QCoreApplication::setApplicationVersion (QStringLiteral ("1.0"));

      QCommandLineParser parser;
      parser.setApplicationDescription (
          QStringLiteral ("Compare FT8 decode outputs across DSP rollout stages on the same WAV file."));
      parser.addHelpOption ();
      parser.addVersionOption ();

      QCommandLineOption const stages_option {
          QStringList {QStringLiteral ("s"), QStringLiteral ("stages")},
          QStringLiteral ("Comma-separated FT8 stages to compare."),
          QStringLiteral ("stages"),
          QStringLiteral ("0,3")
      };
      QCommandLineOption const nqsoprogress_option {
          QStringLiteral ("nqsoprogress"),
          QStringLiteral ("QSO progress hint."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const nfqso_option {
          QStringLiteral ("nfqso"),
          QStringLiteral ("Expected QSO audio frequency in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("1000")
      };
      QCommandLineOption const nftx_option {
          QStringLiteral ("nftx"),
          QStringLiteral ("Transmit audio frequency hint in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("0")
      };
      QCommandLineOption const nutc_option {
          QStringLiteral ("nutc"),
          QStringLiteral ("UTC time hint as HHMM or HHMMSS integer."),
          QStringLiteral ("time"),
          QStringLiteral ("0")
      };
      QCommandLineOption const nfa_option {
          QStringLiteral ("nfa"),
          QStringLiteral ("Low decode limit in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("200")
      };
      QCommandLineOption const nfb_option {
          QStringLiteral ("nfb"),
          QStringLiteral ("High decode limit in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("4000")
      };
      QCommandLineOption const nzhsym_option {
          QStringLiteral ("nzhsym"),
          QStringLiteral ("FT8 half-symbol count (41..50)."),
          QStringLiteral ("count"),
          QStringLiteral ("50")
      };
      QCommandLineOption const depth_option {
          QStringLiteral ("depth"),
          QStringLiteral ("FT8 decode depth."),
          QStringLiteral ("depth"),
          QStringLiteral ("3")
      };
      QCommandLineOption const emedelay_option {
          QStringLiteral ("emedelay"),
          QStringLiteral ("EME delay hint."),
          QStringLiteral ("seconds"),
          QStringLiteral ("0")
      };
      QCommandLineOption const ncontest_option {
          QStringLiteral ("ncontest"),
          QStringLiteral ("Contest mode hint."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const nagain_option {
          QStringLiteral ("nagain"),
          QStringLiteral ("Again flag (0/1)."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const lft8apon_option {
          QStringLiteral ("lft8apon"),
          QStringLiteral ("FT8 AP enabled flag (0/1)."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const lapcqonly_option {
          QStringLiteral ("lapcqonly"),
          QStringLiteral ("CQ-only AP flag (0/1)."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
      };
      QCommandLineOption const napwid_option {
          QStringLiteral ("napwid"),
          QStringLiteral ("AP frequency window in Hz."),
          QStringLiteral ("hz"),
          QStringLiteral ("50")
      };
      QCommandLineOption const ldiskdat_option {
          QStringLiteral ("ldiskdat"),
          QStringLiteral ("Disk-data flag (0/1)."),
          QStringLiteral ("value"),
          QStringLiteral ("1")
      };
      QCommandLineOption const mycall_option {
          QStringLiteral ("mycall"),
          QStringLiteral ("Optional mycall override."),
          QStringLiteral ("call"),
          QString {}
      };
      QCommandLineOption const hiscall_option {
          QStringLiteral ("hiscall"),
          QStringLiteral ("Optional hiscall override."),
          QStringLiteral ("call"),
          QString {}
      };
      QCommandLineOption const hisgrid_option {
          QStringLiteral ("hisgrid"),
          QStringLiteral ("Optional hisgrid override."),
          QStringLiteral ("grid"),
          QString {}
      };

      parser.addOption (stages_option);
      parser.addOption (nqsoprogress_option);
      parser.addOption (nfqso_option);
      parser.addOption (nftx_option);
      parser.addOption (nutc_option);
      parser.addOption (nfa_option);
      parser.addOption (nfb_option);
      parser.addOption (nzhsym_option);
      parser.addOption (depth_option);
      parser.addOption (emedelay_option);
      parser.addOption (ncontest_option);
      parser.addOption (nagain_option);
      parser.addOption (lft8apon_option);
      parser.addOption (lapcqonly_option);
      parser.addOption (napwid_option);
      parser.addOption (ldiskdat_option);
      parser.addOption (mycall_option);
      parser.addOption (hiscall_option);
      parser.addOption (hisgrid_option);
      parser.addPositionalArgument (QStringLiteral ("wav-file"),
                                    QStringLiteral ("Path to a 12000 Hz mono 16-bit FT8 WAV file."));
      parser.process (app);

      QStringList const positional = parser.positionalArguments ();
      if (positional.size () != 1)
        {
          parser.showHelp (2);
        }

      QList<int> const stages = parse_stages (parser.value (stages_option));
      int const nqsoprogress = parse_int_option (parser, nqsoprogress_option, QStringLiteral ("nqsoprogress"));
      int const nfqso = parse_int_option (parser, nfqso_option, QStringLiteral ("nfqso"));
      int const nftx = parse_int_option (parser, nftx_option, QStringLiteral ("nftx"));
      int const nutc = parse_int_option (parser, nutc_option, QStringLiteral ("nutc"));
      int const nfa = parse_int_option (parser, nfa_option, QStringLiteral ("nfa"));
      int const nfb = parse_int_option (parser, nfb_option, QStringLiteral ("nfb"));
      int const nzhsym = parse_int_option (parser, nzhsym_option, QStringLiteral ("nzhsym"));
      int const depth = parse_int_option (parser, depth_option, QStringLiteral ("depth"));
      float const emedelay = parse_float_option (parser, emedelay_option, QStringLiteral ("emedelay"));
      int const ncontest = parse_int_option (parser, ncontest_option, QStringLiteral ("ncontest"));
      int const nagain = parse_int_option (parser, nagain_option, QStringLiteral ("nagain"));
      int const lft8apon = parse_int_option (parser, lft8apon_option, QStringLiteral ("lft8apon"));
      int const lapcqonly = parse_int_option (parser, lapcqonly_option, QStringLiteral ("lapcqonly"));
      int const napwid = parse_int_option (parser, napwid_option, QStringLiteral ("napwid"));
      int const ldiskdat = parse_int_option (parser, ldiskdat_option, QStringLiteral ("ldiskdat"));

      QString const wav_path = QFileInfo {positional.constFirst ()}.absoluteFilePath ();
      WavData const wav = read_wav_file (wav_path);

      QTextStream out {stdout};
      out << "file: " << wav_path << '\n';
      out << "samples: source=" << wav.source_samples
          << " compare=" << wav.samples.size ()
          << " sample_rate=" << wav.sample_rate
          << " channels=" << wav.channels
          << " bits=" << wav.bits_per_sample
          << '\n';
      if (wav.padded)
        {
          out << "note: source WAV is shorter than FT8 compare buffer; zero-padding tail samples\n";
        }
      if (wav.truncated)
        {
          out << "note: source WAV is longer than FT8 compare buffer; truncating to first "
              << kFt8SampleCount << " samples\n";
        }
      out << "decoder params: nqsoprogress=" << nqsoprogress
          << " nfqso=" << nfqso
          << " nftx=" << nftx
          << " nutc=" << nutc
          << " nfa=" << nfa
          << " nfb=" << nfb
          << " nzhsym=" << nzhsym
          << " depth=" << depth
          << " emedelay=" << emedelay
          << " ncontest=" << ncontest
          << " nagain=" << nagain
          << " lft8apon=" << lft8apon
          << " lapcqonly=" << lapcqonly
          << " napwid=" << napwid
          << " ldiskdat=" << ldiskdat
          << '\n';

      std::vector<DecodeResult> results;
      results.reserve (static_cast<size_t> (stages.size ()));
      for (int stage : stages)
        {
          results.push_back (run_decode (wav.samples, stage, nqsoprogress, nfqso, nftx, nutc,
                                         nfa, nfb, nzhsym, depth, emedelay, ncontest,
                                         nagain, lft8apon, lapcqonly, napwid, ldiskdat,
                                         parser.value (mycall_option).toLatin1 (),
                                         parser.value (hiscall_option).toLatin1 (),
                                         parser.value (hisgrid_option).toLatin1 ()));
        }

      out << '\n';
      for (DecodeResult const& result : results)
        {
          print_result (out, result);
          out << '\n';
        }

      for (size_t i = 1; i < results.size (); ++i)
        {
          print_comparison (out, results.front (), results[i]);
        }

      return EXIT_SUCCESS;
    }
  catch (std::exception const& ex)
    {
      std::cerr << ex.what () << '\n';
      return EXIT_FAILURE;
    }
}
