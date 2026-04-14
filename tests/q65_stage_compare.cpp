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
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QtEndian>

#include "commons.h"
#include "Detector/Q65DecodeWorker.hpp"

extern "C"
{
  void ftx_q65_decode_and_emit_params_c (short const* iwave,
                                         params_block_t const* params,
                                         char const* temp_dir,
                                         int* decoded_count);
}

namespace
{
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
    QString decoded;
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

  WavData read_wav_file (QString const& file_name, int expected_samples)
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
    if (audio_format != 1u || channels != 1u || sample_rate != 12000u || bits_per_sample != 16u)
      {
        fail (QStringLiteral ("WAV file \"%1\" must be mono 12000 Hz 16-bit PCM").arg (file_name));
      }

    int const sample_count = sample_bytes.size () / static_cast<int> (sizeof (qint16));
    WavData wav;
    wav.sample_rate = static_cast<int> (sample_rate);
    wav.channels = static_cast<int> (channels);
    wav.bits_per_sample = static_cast<int> (bits_per_sample);
    wav.source_samples = sample_count;
    wav.padded = sample_count < expected_samples;
    wav.truncated = sample_count > expected_samples;
    wav.samples.assign (static_cast<size_t> (expected_samples), 0);

    int const copy_count = std::min (sample_count, expected_samples);
    uchar const* input = reinterpret_cast<uchar const*> (sample_bytes.constData ());
    for (int i = 0; i < copy_count; ++i)
      {
        wav.samples[static_cast<size_t> (i)] =
            qFromLittleEndian<qint16> (input + i * static_cast<int> (sizeof (qint16)));
      }
    return wav;
  }

  DecodeLine parse_worker_row (QString const& row)
  {
    static QRegularExpression const pattern {
        QStringLiteral (R"(^\s*\d{4,6}\s+(-?\d+)\s+(-?\d+\.\d)\s+(-?\d+)\s+:\s+(.*?)\s+q\d(?:\d)?\s*$)")
    };
    auto const match = pattern.match (row);
    if (!match.hasMatch ())
      {
        fail (QStringLiteral ("cannot parse worker row \"%1\"").arg (row));
      }

    DecodeLine line;
    line.snr = match.captured (1).toInt ();
    line.dt = match.captured (2).toFloat ();
    line.freq = match.captured (3).toFloat ();
    line.decoded = match.captured (4).trimmed ();
    return line;
  }

  QList<DecodeLine> run_worker_decode (std::vector<short> const& samples,
                                       int nutc, int ntrperiod, int nsubmode,
                                       int nfqso, int ntol, int ndepth,
                                       int nfa, int nfb)
  {
    decodium::q65::Q65DecodeWorker worker;
    decodium::q65::DecodeRequest request;
    request.serial = 1;
    request.audio = QVector<short> (samples.begin (), samples.end ());
    request.nqd0 = 1;
    request.nutc = nutc;
    request.ntrperiod = ntrperiod;
    request.nsubmode = nsubmode;
    request.nfqso = nfqso;
    request.ntol = ntol;
    request.ndepth = ndepth;
    request.nfa = nfa;
    request.nfb = nfb;
    request.lnewdat = 1;
    request.mycall = QByteArrayLiteral ("K1ABC");
    request.hiscall = QByteArrayLiteral ("W9XYZ");
    request.hisgrid = QByteArrayLiteral ("EN37");

    QList<DecodeLine> lines;
    QObject::connect (&worker, &decodium::q65::Q65DecodeWorker::decodeReady,
                      [&lines] (quint64, QStringList const& rows)
                      {
                        for (auto const& row : rows)
                          {
                            lines.append (parse_worker_row (row));
                          }
                      });
    worker.decode (request);
    return lines;
  }

  QList<DecodeLine> parse_decoded_file (QString const& path)
  {
    QFile file {path};
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
      {
        fail (QStringLiteral ("cannot open decoded file \"%1\": %2")
                  .arg (path, file.errorString ()));
      }

    QList<DecodeLine> lines;
    while (!file.atEnd ())
      {
        QString line = QString::fromUtf8 (file.readLine ()).trimmed ();
        if (line.isEmpty ())
          {
            continue;
          }
        QStringList const fields = line.split (QRegularExpression {QStringLiteral ("\\s+")},
                                               Qt::SkipEmptyParts);
        if (fields.size () < 8 || fields.constLast () != QStringLiteral ("Q65"))
          {
            fail (QStringLiteral ("cannot parse decoded.txt line \"%1\"").arg (line));
          }
        DecodeLine decoded;
        decoded.snr = fields.at (2).toInt ();
        decoded.dt = fields.at (3).toFloat ();
        decoded.freq = fields.at (4).toFloat ();
        QStringList message_fields = fields.mid (6, fields.size () - 7);
        decoded.decoded = message_fields.join (QLatin1Char {' '}).trimmed ();
        lines.append (decoded);
      }
    return lines;
  }

  QList<DecodeLine> run_native_decode (std::vector<short> const& samples,
                                       int nutc, int ntrperiod, int nsubmode,
                                       int nfqso, int ntol, int ndepth,
                                       int nfa, int nfb)
  {
    params_block_t params {};
    params.nmode = 66;
    params.nutc = nutc;
    params.ndiskdat = true;
    params.ntrperiod = ntrperiod;
    params.nQSOProgress = 0;
    params.nfqso = nfqso;
    params.newdat = true;
    params.nfa = nfa;
    params.nfb = nfb;
    params.ntol = ntol;
    params.nsubmode = nsubmode;
    params.ndepth = ndepth;
    params.emedelay = (ntrperiod == 60) ? 2.5f : 0.0f;
    std::copy_n (to_fortran_field (QByteArrayLiteral ("K1ABC"), 12).constData (), 12, params.mycall);
    std::copy_n (to_fortran_field (QByteArrayLiteral ("W9XYZ"), 12).constData (), 12, params.hiscall);
    std::copy_n (to_fortran_field (QByteArrayLiteral ("EN37"), 6).constData (), 6, params.hisgrid);

    QTemporaryDir temp_dir;
    if (!temp_dir.isValid ())
      {
        fail (QStringLiteral ("cannot create temporary directory"));
      }

    QByteArray temp_path = temp_dir.path ().toLocal8Bit ();
    int decoded_count = 0;
    ftx_q65_decode_and_emit_params_c (samples.data (), &params, temp_path.constData (), &decoded_count);
    Q_UNUSED (decoded_count);
    return parse_decoded_file (temp_dir.filePath (QStringLiteral ("decoded.txt")));
  }

  QString line_key (DecodeLine const& line)
  {
    return QStringLiteral ("%1|%2")
        .arg (qRound (line.freq), 5)
        .arg (line.decoded);
  }
}

int main (int argc, char* argv[])
{
  QCoreApplication app {argc, argv};
  QCoreApplication::setApplicationName (QStringLiteral ("q65_stage_compare"));

  QCommandLineParser parser;
  parser.setApplicationDescription (QStringLiteral ("Compare Q65 worker output with native dispatcher output"));
  parser.addHelpOption ();

  QCommandLineOption trPeriodOption ({QStringLiteral ("p"), QStringLiteral ("tr-period")},
                                     QStringLiteral ("Tx/Rx period in seconds"),
                                     QStringLiteral ("SECONDS"),
                                     QStringLiteral ("30"));
  QCommandLineOption submodeOption ({QStringLiteral ("b"), QStringLiteral ("submode")},
                                    QStringLiteral ("Q65 submode index 0..4"),
                                    QStringLiteral ("INDEX"),
                                    QStringLiteral ("0"));
  QCommandLineOption depthOption ({QStringLiteral ("d"), QStringLiteral ("depth")},
                                  QStringLiteral ("Decode depth"),
                                  QStringLiteral ("DEPTH"),
                                  QStringLiteral ("3"));
  QCommandLineOption freqOption ({QStringLiteral ("f"), QStringLiteral ("rx-frequency")},
                                 QStringLiteral ("Target frequency"),
                                 QStringLiteral ("HERTZ"),
                                 QStringLiteral ("1000"));
  QCommandLineOption tolOption ({QStringLiteral ("t"), QStringLiteral ("tolerance")},
                                QStringLiteral ("Frequency tolerance"),
                                QStringLiteral ("HERTZ"),
                                QStringLiteral ("10"));
  QCommandLineOption lowOption ({QStringLiteral ("L"), QStringLiteral ("lowest")},
                                QStringLiteral ("Low decode bound"),
                                QStringLiteral ("HERTZ"),
                                QStringLiteral ("200"));
  QCommandLineOption highOption ({QStringLiteral ("H"), QStringLiteral ("highest")},
                                 QStringLiteral ("High decode bound"),
                                 QStringLiteral ("HERTZ"),
                                 QStringLiteral ("4000"));

  parser.addOption (trPeriodOption);
  parser.addOption (submodeOption);
  parser.addOption (depthOption);
  parser.addOption (freqOption);
  parser.addOption (tolOption);
  parser.addOption (lowOption);
  parser.addOption (highOption);
  parser.addPositionalArgument (QStringLiteral ("wav"), QStringLiteral ("Input 12000 Hz mono WAV file"));
  parser.process (app);

  if (parser.positionalArguments ().size () != 1)
    {
      parser.showHelp (1);
    }

  try
    {
      int const ntrperiod = parse_int_option (parser, trPeriodOption, QStringLiteral ("tr-period"));
      int const nsubmode = parse_int_option (parser, submodeOption, QStringLiteral ("submode"));
      int const ndepth = parse_int_option (parser, depthOption, QStringLiteral ("depth"));
      int const nfqso = parse_int_option (parser, freqOption, QStringLiteral ("rx-frequency"));
      int const ntol = parse_int_option (parser, tolOption, QStringLiteral ("tolerance"));
      int const nfa = parse_int_option (parser, lowOption, QStringLiteral ("lowest"));
      int const nfb = parse_int_option (parser, highOption, QStringLiteral ("highest"));
      if (nsubmode < 0 || nsubmode > 4)
        {
          fail (QStringLiteral ("submode must be in 0..4"));
        }

      QString const wav_path = parser.positionalArguments ().constFirst ();
      WavData const wav = read_wav_file (wav_path, ntrperiod * RX_SAMPLE_RATE);
      int const nutc = QFileInfo {wav_path}.completeBaseName ().right (6).toInt ();

      QList<DecodeLine> const worker = run_worker_decode (wav.samples, nutc, ntrperiod, nsubmode,
                                                          nfqso, ntol, ndepth, nfa, nfb);
      QList<DecodeLine> const native = run_native_decode (wav.samples, nutc, ntrperiod, nsubmode,
                                                          nfqso, ntol, ndepth, nfa, nfb);

      QHash<QString, DecodeLine> worker_by_key;
      for (auto const& line : worker)
        {
          worker_by_key.insert (line_key (line), line);
        }
      QHash<QString, DecodeLine> native_by_key;
      for (auto const& line : native)
        {
          native_by_key.insert (line_key (line), line);
        }

      QSet<QString> const worker_keys = QSet<QString> (worker_by_key.keyBegin (), worker_by_key.keyEnd ());
      QSet<QString> const native_keys = QSet<QString> (native_by_key.keyBegin (), native_by_key.keyEnd ());
      QSet<QString> const common = worker_keys & native_keys;
      QSet<QString> const only_worker = worker_keys - native_keys;
      QSet<QString> const only_native = native_keys - worker_keys;

      std::cout << "worker=" << worker.size ()
                << " native=" << native.size ()
                << " common=" << common.size ()
                << " only_worker=" << only_worker.size ()
                << " only_native=" << only_native.size () << '\n';

      if (!only_worker.isEmpty () || !only_native.isEmpty ())
        {
          for (auto const& key : only_worker)
            {
              auto const& line = worker_by_key.value (key);
              std::cout << "only_worker: snr=" << line.snr
                        << " dt=" << line.dt
                        << " freq=" << qRound (line.freq)
                        << " decoded=\"" << line.decoded.toStdString () << "\"\n";
            }
          for (auto const& key : only_native)
            {
              auto const& line = native_by_key.value (key);
              std::cout << "only_native: snr=" << line.snr
                        << " dt=" << line.dt
                        << " freq=" << qRound (line.freq)
                        << " decoded=\"" << line.decoded.toStdString () << "\"\n";
            }
          return 1;
        }
      return 0;
    }
  catch (std::exception const& error)
    {
      std::cerr << "q65_stage_compare: " << error.what () << '\n';
      return 1;
    }
}
