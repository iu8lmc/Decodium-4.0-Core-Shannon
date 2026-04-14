#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace
{
[[noreturn]] void fail (QString const& message)
{
  throw std::runtime_error (message.toStdString ());
}

void print_usage ()
{
  std::cout << "Usage:   test_q65        \"msg\"       A-D depth freq  DT fDop  f1 Stp TRp Q nfiles SNR\n"
            << "Example: test_q65 \"K1ABC W9XYZ EN37\"  A    3   1500 0.0  5.0 0.0  1   60 3  100   -20\n"
            << "Use SNR = 0 to loop over all relevant SNRs\n"
            << "Use MyCall=K1ABC, HisCall=W9XYZ, HisGrid=\"EN37\" for AP decodes\n"
            << "Option Q sets QSOprogress (0-5) for AP decoding.\n"
            << "Add 16 to requested depth to enable message averaging.\n";
}

int nsps_for_period (int ntrperiod)
{
  switch (ntrperiod)
    {
    case 15: return 1800;
    case 30: return 3600;
    case 60: return 7200;
    case 120: return 16000;
    case 300: return 41472;
    default: return 0;
    }
}

QString find_executable (QString const& name)
{
  QStringList candidates;
  QString const app_dir = QCoreApplication::applicationDirPath ();
  candidates << QDir (app_dir).absoluteFilePath (name)
             << QDir (app_dir).absoluteFilePath (QStringLiteral ("../build/%1").arg (name))
             << QDir (app_dir).absoluteFilePath (QStringLiteral ("../build-utils/%1").arg (name));
  QString const path_hit = QStandardPaths::findExecutable (name);
  if (!path_hit.isEmpty ())
    {
      candidates << path_hit;
    }

  for (QString const& candidate : candidates)
    {
      QFileInfo const info (candidate);
      if (info.exists () && info.isFile () && info.isExecutable ())
        {
          return info.absoluteFilePath ();
        }
    }
  return QString {};
}

QString run_process (QString const& program, QStringList const& args, int* exit_code = nullptr)
{
  QProcess process;
  process.setProgram (program);
  process.setArguments (args);
  process.setProcessChannelMode (QProcess::MergedChannels);
  process.start ();
  if (!process.waitForStarted ())
    {
      fail (QStringLiteral ("failed to start %1").arg (program));
    }
  if (!process.waitForFinished (-1))
    {
      fail (QStringLiteral ("timeout while running %1").arg (program));
    }
  if (exit_code)
    {
      *exit_code = process.exitCode ();
    }
  return QString::fromLocal8Bit (process.readAll ());
}

void remove_generated_wavs ()
{
  QDir dir (QDir::currentPath ());
  QStringList const files = dir.entryList (QStringList () << QStringLiteral ("*.wav"), QDir::Files,
                                           QDir::Name);
  for (QString const& file : files)
    {
      dir.remove (file);
    }
}

struct ParsedLine
{
  bool ok {false};
  float snr {0.0f};
  float dt {0.0f};
  int freq {0};
  bool decok {false};
  int idec {-1};
  int iavg {0};
};

ParsedLine parse_line (QString const& line, QString const& message)
{
  ParsedLine parsed;
  if (line.startsWith (QStringLiteral ("<Decode")) || line.trimmed ().isEmpty ())
    {
      return parsed;
    }

  QStringList const fields = line.split (QRegularExpression (QStringLiteral ("\\s+")),
                                         Qt::SkipEmptyParts);
  if (fields.size () < 4)
    {
      return parsed;
    }

  bool ok_snr = false;
  bool ok_dt = false;
  bool ok_freq = false;
  float const snr = fields.at (1).toFloat (&ok_snr);
  float const xdt = fields.at (2).toFloat (&ok_dt);
  int const freq = fields.at (3).toInt (&ok_freq);
  if (!ok_snr || !ok_dt || !ok_freq)
    {
      return parsed;
    }

  QRegularExpression const rx (QStringLiteral ("\\bq(\\d)(\\*|\\d)"));
  QRegularExpressionMatch const match = rx.match (line);
  if (!match.hasMatch ())
    {
      return parsed;
    }

  parsed.ok = true;
  parsed.snr = snr;
  parsed.dt = xdt;
  parsed.freq = freq;
  parsed.decok = line.contains (message);
  parsed.idec = match.captured (1).toInt ();
  parsed.iavg = match.captured (2) == QStringLiteral ("*") ? 10 : match.captured (2).toInt ();
  return parsed;
}

void append_threshold_line (int ntrperiod, QChar csubmode, int ndepth, int nqsoprogress,
                            int nfiles, float fdop, float f1, int nstp, int nfalse,
                            float snr_thresh, QString const& message)
{
  QFile file (QStringLiteral ("snr_thresh.out"));
  if (!file.open (QIODevice::Append | QIODevice::Text))
    {
      return;
    }
  QTextStream out (&file);
  out.setRealNumberNotation (QTextStream::FixedNotation);
  out.setRealNumberPrecision (1);
  out << QStringLiteral ("%1%2%3%4%5%6%7%8%9  %10\n")
           .arg (ntrperiod, 3)
           .arg (csubmode)
           .arg (ndepth, 3)
           .arg (nqsoprogress, 3)
           .arg (nfiles, 5)
           .arg (fdop, 7, 'f', 1)
           .arg (f1, 7, 'f', 1)
           .arg (nstp, 3)
           .arg (nfalse, 3)
           .arg (QString::number (snr_thresh, 'f', 1) + QStringLiteral (" ") + message.trimmed ());
}
}

int main (int argc, char* argv[])
{
  try
    {
      QCoreApplication app (argc, argv);
      if (argc != 13)
        {
          print_usage ();
          return 0;
        }

      QString const message = QString::fromLocal8Bit (argv[1]).trimmed ();
      QString const submode = QString::fromLocal8Bit (argv[2]).trimmed ().toUpper ();
      if (submode.size () != 1)
        {
          fail (QStringLiteral ("invalid Q65 submode"));
        }
      QChar const csubmode = submode.at (0);
      int const ndepth = QString::fromLocal8Bit (argv[3]).toInt ();
      int const nf0 = QString::fromLocal8Bit (argv[4]).toInt ();
      float const dt = QString::fromLocal8Bit (argv[5]).toFloat ();
      float const fdop = QString::fromLocal8Bit (argv[6]).toFloat ();
      float const f1 = QString::fromLocal8Bit (argv[7]).toFloat ();
      int const nstp = QString::fromLocal8Bit (argv[8]).toInt ();
      int const ntrperiod = QString::fromLocal8Bit (argv[9]).toInt ();
      int const nqsoprogress = QString::fromLocal8Bit (argv[10]).toInt ();
      int const nfiles = QString::fromLocal8Bit (argv[11]).toInt ();
      float const snr = QString::fromLocal8Bit (argv[12]).toFloat ();

      int const nsps = nsps_for_period (ntrperiod);
      if (nsps == 0)
        {
          fail (QStringLiteral ("Invalid TR period"));
        }

      float i50 = 0.0f;
      if (ntrperiod == 15) i50 = -23.0f;
      if (ntrperiod == 30) i50 = -26.0f;
      if (ntrperiod == 60) i50 = -29.0f;
      if (ntrperiod == 120) i50 = -31.0f;
      if (ntrperiod == 300) i50 = -35.0f;
      i50 += 8.0f * std::log (1.0f + fdop) / std::log (240.0f);

      int ia = static_cast<int> (i50) + 7;
      int ib = static_cast<int> (i50) - 10;
      if (snr != 0.0f)
        {
          ia = 99;
          ib = 99;
        }

      float const baud = 12000.0f / static_cast<float> (nsps);
      float const tsym = 1.0f / baud;
      float const dterr = tsym / 4.0f;
      int const nferr = std::max (1, std::max (static_cast<int> (std::lround (0.5f * baud)),
                                                static_cast<int> (std::lround (fdop / 3.0f))));

      QString const q65sim = find_executable (QStringLiteral ("q65sim"));
      QString const jt9 = find_executable (QStringLiteral ("jt9"));
      if (q65sim.isEmpty () || jt9.isEmpty ())
        {
          fail (QStringLiteral ("unable to locate q65sim/jt9 executables"));
        }

      std::cout << "Mode:" << std::setw (4) << ntrperiod << csubmode.toLatin1 ()
                << "  Depth:" << std::setw (3) << ndepth
                << "  fDop:" << std::setw (6) << std::fixed << std::setprecision (1) << fdop
                << "  Drift:" << std::setw (8) << f1
                << "  Steps:" << std::setw (3) << nstp << '\n';
      std::cout << " SNR Sync Avg Dec  Bad";
      for (int j = 0; j <= 5; ++j)
        {
          std::cout << std::setw (4) << j;
        }
      std::cout << "  tdec   avg  rms\n" << std::string (64, '-') << '\n';

      int ndec1z = nfiles;
      for (int nsnr = ia; nsnr >= ib; --nsnr)
        {
          float snr1 = (ia == 99) ? snr : static_cast<float> (nsnr);

          remove_generated_wavs ();
          int q65sim_exit = 0;
          run_process (q65sim,
                       QStringList ()
                         << message
                         << QString (csubmode)
                         << QString::number (nf0)
                         << QString::number (fdop, 'f', 1)
                         << QString::number (dt, 'f', 2)
                         << QString::number (std::lround (f1))
                         << QString::number (nstp)
                         << QString::number (ntrperiod)
                         << QStringLiteral ("1")
                         << QString::number (nfiles)
                         << QString::number (snr1, 'f', 1),
                       &q65sim_exit);
          if (q65sim_exit != 0)
            {
              fail (QStringLiteral ("q65sim failed"));
            }

          QDir dir (QDir::currentPath ());
          QStringList wavs = dir.entryList (QStringList () << QStringLiteral ("*.wav"), QDir::Files,
                                            QDir::Name);
          if (wavs.isEmpty ())
            {
              fail (QStringLiteral ("q65sim produced no wav files"));
            }

          QStringList jt9_args;
          jt9_args << QStringLiteral ("-3")
                   << QStringLiteral ("-p") << QString::number (ntrperiod)
                   << QStringLiteral ("-L") << QStringLiteral ("300")
                   << QStringLiteral ("-H") << QStringLiteral ("3000")
                   << QStringLiteral ("-d") << QString::number (ndepth)
                   << QStringLiteral ("-b") << QString (csubmode)
                   << QStringLiteral ("-Q") << QString::number (nqsoprogress)
                   << QStringLiteral ("-f") << QString::number (nf0)
                   << QStringLiteral ("-X") << QStringLiteral ("32");
          for (QString const& wav : wavs)
            {
              jt9_args << dir.absoluteFilePath (wav);
            }

          QElapsedTimer timer;
          timer.start ();
          int jt9_exit = 0;
          QString const output = run_process (jt9, jt9_args, &jt9_exit);
          double const tdec = static_cast<double> (timer.elapsed ()) / 1000.0;
          if (jt9_exit != 0)
            {
              fail (QStringLiteral ("jt9 failed"));
            }

          int nsync = 0;
          int ndecn = 0;
          int ndec1 = 0;
          int nfalse = 0;
          std::array<int, 6> naptype {};
          double snrsum = 0.0;
          double snrsq = 0.0;
          int nsum = 0;

          QStringList const lines = output.split (QRegularExpression (QStringLiteral ("\\r?\\n")),
                                                  Qt::SkipEmptyParts);
          for (QString const& line : lines)
            {
              ParsedLine const parsed = parse_line (line, message);
              if (!parsed.ok)
                {
                  continue;
                }

              if (parsed.decok)
                {
                  snrsum += parsed.snr;
                  snrsq += parsed.snr * parsed.snr;
                  ++nsum;
                }
              if ((std::fabs (parsed.dt - dt) <= dterr && std::abs (parsed.freq - nf0) <= nferr)
                  || parsed.decok)
                {
                  ++nsync;
                }
              if (parsed.idec < 0 || parsed.idec >= static_cast<int> (naptype.size ()))
                {
                  continue;
                }
              if (parsed.decok)
                {
                  ++ndecn;
                  if (parsed.iavg <= 1)
                    {
                      ++ndec1;
                    }
                  ++naptype[static_cast<size_t> (parsed.idec)];
                }
              else
                {
                  ++nfalse;
                  std::cout << "False: " << line.toStdString () << '\n';
                }
            }

          double snr_avg = 0.0;
          double snr_rms = 0.0;
          if (nsum >= 1)
            {
              snr_avg = snrsum / nsum;
              snr_rms = std::sqrt (std::max (0.0, snrsq / nsum - snr_avg * snr_avg));
            }

          std::cout << std::fixed << std::setprecision (1)
                    << std::setw (5) << snr1
                    << std::setw (4) << nsync
                    << std::setw (4) << ndecn
                    << std::setw (4) << ndec1
                    << std::setw (5) << nfalse;
          for (int value : naptype)
            {
              std::cout << std::setw (4) << value;
            }
          std::cout << std::setw (6) << std::setprecision (2) << (tdec / nfiles)
                    << std::setw (6) << std::setprecision (1) << snr_avg
                    << std::setw (5) << std::setprecision (1) << snr_rms << '\n';

          if (ndec1 < nfiles / 2 && ndec1z >= nfiles / 2)
            {
              float const snr_thresh =
                snr1 + static_cast<float> (nfiles / 2 - ndec1) / static_cast<float> (ndec1z - ndec1);
              append_threshold_line (ntrperiod, csubmode, ndepth, nqsoprogress, nfiles, fdop, f1,
                                     nstp, nfalse, snr_thresh, message);
            }
          if (ndec1 == 0 && ndecn == 0)
            {
              break;
            }
          ndec1z = ndec1;
        }

      return 0;
    }
  catch (std::exception const& error)
    {
      std::cerr << error.what () << '\n';
      return 1;
    }
}
