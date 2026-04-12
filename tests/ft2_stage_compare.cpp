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
  void ft2_async_decode_(short iwave[], int* nqsoprogress, int* nfqso, int* nfa, int* nfb,
                         int* ndepth, int* ncontest, char mycall[], char hiscall[],
                         int snrs[], float dts[], float freqs[], int naps[], float quals[],
                         signed char bits77[], char decodeds[], int* nout,
                         size_t, size_t, size_t);
  void ftx_ft2_cpp_dsp_rollout_stage_override_c (int stage);
  void ftx_ft2_cpp_dsp_rollout_stage_reset_c ();
  void ftx_ft2_stage7_clravg_c ();
}

namespace
{
  constexpr int kFt2SampleCount {45000};
  constexpr int kFt2MaxLines {100};
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
        if (!ok || stage < 0 || stage > 7)
          {
            fail (QStringLiteral ("invalid FT2 stage \"%1\"; expected values in 0..7").arg (part));
          }
        if (!seen.contains (stage))
          {
            seen.insert (stage);
            stages.append (stage);
          }
      }
    if (stages.isEmpty ())
      {
        fail (QStringLiteral ("no FT2 stages selected"));
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
    wav.padded = sample_count < kFt2SampleCount;
    wav.truncated = sample_count > kFt2SampleCount;
    wav.samples.assign (static_cast<size_t> (kFt2SampleCount), 0);

    int const copy_count = std::min (sample_count, kFt2SampleCount);
    uchar const* input = reinterpret_cast<uchar const*> (sample_bytes.constData ());
    for (int i = 0; i < copy_count; ++i)
      {
        wav.samples[static_cast<size_t> (i)] =
            qFromLittleEndian<qint16> (input + i * static_cast<int> (sizeof (qint16)));
      }
    return wav;
  }

  DecodeResult run_decode (std::vector<short> const& samples, int stage, int nqsoprogress,
                           int nfqso, int nfa, int nfb, int ndepth, int ncontest,
                           QByteArray const& mycall, QByteArray const& hiscall)
  {
    std::vector<short> iwave = samples;
    std::array<int, kFt2MaxLines> snrs {};
    std::array<float, kFt2MaxLines> dts {};
    std::array<float, kFt2MaxLines> freqs {};
    std::array<int, kFt2MaxLines> naps {};
    std::array<float, kFt2MaxLines> quals {};
    std::array<signed char, kBitsPerMessage * kFt2MaxLines> bits77 {};
    std::array<char, kFt2MaxLines * kDecodedChars> decodeds {};
    int nout = 0;
    QByteArray mycall_field = to_fortran_field (mycall, 12);
    QByteArray hiscall_field = to_fortran_field (hiscall, 12);

    QMutexLocker locker {&decodium::fortran::runtime_mutex ()};
    if (stage >= 7)
      {
        ftx_ft2_stage7_clravg_c ();
      }
    ftx_ft2_cpp_dsp_rollout_stage_override_c (stage);
    ft2_async_decode_ (iwave.data (), &nqsoprogress, &nfqso, &nfa, &nfb, &ndepth, &ncontest,
                       mycall_field.data (), hiscall_field.data (), snrs.data (), dts.data (),
                       freqs.data (), naps.data (), quals.data (), bits77.data (),
                       decodeds.data (), &nout, static_cast<size_t> (mycall_field.size ()),
                       static_cast<size_t> (hiscall_field.size ()),
                       static_cast<size_t> (decodeds.size ()));
    ftx_ft2_cpp_dsp_rollout_stage_reset_c ();
    if (stage >= 7)
      {
        ftx_ft2_stage7_clravg_c ();
      }
    locker.unlock ();

    DecodeResult result;
    result.stage = stage;
    result.lines.reserve (static_cast<size_t> (std::max (0, nout)));
    for (int i = 0; i < std::max (0, nout) && i < kFt2MaxLines; ++i)
      {
        DecodeLine line;
        line.snr = snrs[i];
        line.dt = dts[i];
        line.freq = freqs[i];
        line.nap = naps[i];
        line.qual = quals[i];
        line.decoded = QString::fromLatin1 (
            trim_fortran_field (decodeds.data () + i * kDecodedChars, kDecodedChars));
        result.lines.push_back (line);
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
      QCoreApplication::setApplicationName (QStringLiteral ("ft2_stage_compare"));
      QCoreApplication::setApplicationVersion (QStringLiteral ("1.0"));

      QCommandLineParser parser;
      parser.setApplicationDescription (
          QStringLiteral ("Compare FT2 decode outputs across DSP rollout stages on the same WAV file."));
      parser.addHelpOption ();
      parser.addVersionOption ();

      QCommandLineOption const stages_option {
          QStringList {QStringLiteral ("s"), QStringLiteral ("stages")},
          QStringLiteral ("Comma-separated FT2 stages to compare."),
          QStringLiteral ("stages"),
          QStringLiteral ("0,7")
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
          QStringLiteral ("5000")
      };
      QCommandLineOption const ndepth_option {
          QStringLiteral ("ndepth"),
          QStringLiteral ("FT2 decode depth."),
          QStringLiteral ("depth"),
          QStringLiteral ("3")
      };
      QCommandLineOption const ncontest_option {
          QStringLiteral ("ncontest"),
          QStringLiteral ("Contest mode hint."),
          QStringLiteral ("value"),
          QStringLiteral ("0")
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

      parser.addOption (stages_option);
      parser.addOption (nqsoprogress_option);
      parser.addOption (nfqso_option);
      parser.addOption (nfa_option);
      parser.addOption (nfb_option);
      parser.addOption (ndepth_option);
      parser.addOption (ncontest_option);
      parser.addOption (mycall_option);
      parser.addOption (hiscall_option);
      parser.addPositionalArgument (QStringLiteral ("wav-file"),
                                    QStringLiteral ("Path to a 12000 Hz mono 16-bit FT2 WAV file."));

      parser.process (app);

      QStringList const positional = parser.positionalArguments ();
      if (positional.size () != 1)
        {
          fail (QStringLiteral ("expected exactly one WAV file path"));
        }

      QList<int> const stages = parse_stages (parser.value (stages_option));
      int const nqsoprogress = parse_int_option (parser, nqsoprogress_option, QStringLiteral ("nqsoprogress"));
      int const nfqso = parse_int_option (parser, nfqso_option, QStringLiteral ("nfqso"));
      int const nfa = parse_int_option (parser, nfa_option, QStringLiteral ("nfa"));
      int const nfb = parse_int_option (parser, nfb_option, QStringLiteral ("nfb"));
      int const ndepth = parse_int_option (parser, ndepth_option, QStringLiteral ("ndepth"));
      int const ncontest = parse_int_option (parser, ncontest_option, QStringLiteral ("ncontest"));
      QByteArray const mycall = parser.value (mycall_option).trimmed ().toLatin1 ();
      QByteArray const hiscall = parser.value (hiscall_option).trimmed ().toLatin1 ();

      WavData const wav = read_wav_file (positional.constFirst ());

      QTextStream out {stdout};
      out << "file: " << QFileInfo (positional.constFirst ()).absoluteFilePath () << '\n';
      out << "samples: source=" << wav.source_samples
          << " compare=" << wav.samples.size ()
          << " sample_rate=" << wav.sample_rate
          << " channels=" << wav.channels
          << " bits=" << wav.bits_per_sample;
      if (wav.padded) out << " padded";
      if (wav.truncated) out << " truncated";
      out << '\n';
      out << "decoder params: nqsoprogress=" << nqsoprogress
          << " nfqso=" << nfqso
          << " nfa=" << nfa
          << " nfb=" << nfb
          << " ndepth=" << ndepth
          << " ncontest=" << ncontest
          << '\n' << '\n';

      std::vector<DecodeResult> results;
      results.reserve (static_cast<size_t> (stages.size ()));
      for (int stage : stages)
        {
          auto result = run_decode (wav.samples, stage, nqsoprogress, nfqso, nfa, nfb, ndepth,
                                    ncontest, mycall, hiscall);
          print_result (out, result);
          out << '\n';
          results.push_back (std::move (result));
        }

      for (size_t i = 0; i < results.size (); ++i)
        {
          for (size_t j = i + 1; j < results.size (); ++j)
            {
              print_comparison (out, results[i], results[j]);
            }
        }

      out.flush ();
      return 0;
    }
  catch (std::exception const& error)
    {
      QTextStream err {stderr};
      err << error.what () << '\n';
      err.flush ();
      return 1;
    }
}
