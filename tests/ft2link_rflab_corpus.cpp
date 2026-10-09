#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextStream>
#include <QVariantList>
#include <QVariantMap>

#include <cmath>

#include "controllers/FT2LinkQmlAdapter.hpp"

namespace
{
struct CorpusCase
{
  QString name;
  QString suite;
  QString profile;
  QString text;
  QVariantMap options;
};

QString safeStem (QString text)
{
  text = text.trimmed ().toLower ();
  text.replace (QRegularExpression (QStringLiteral ("[^a-z0-9]+")),
                QStringLiteral ("-"));
  text.replace (QRegularExpression (QStringLiteral ("^-+|-+$")),
                QString {});
  return text.isEmpty () ? QStringLiteral ("case") : text;
}

QVariantMap baseOptions (QString const& frameType = QStringLiteral ("DATA"))
{
  QVariantMap options;
  options.insert (QStringLiteral ("sampleRate"), 48000);
  options.insert (QStringLiteral ("frameType"), frameType);
  options.insert (QStringLiteral ("leadMs"), 500);
  options.insert (QStringLiteral ("tailMs"), 900);
  return options;
}

QVariantMap withRateMode (int mode)
{
  QVariantMap options = baseOptions ();
  options.insert (QStringLiteral ("w2300RateMode"), mode);
  return options;
}

QString signedLabel (double value, QString const& suffix)
{
  QString const sign = value < 0.0 ? QStringLiteral ("minus") : QStringLiteral ("plus");
  return QStringLiteral ("%1%2%3")
      .arg (sign)
      .arg (std::fabs (value), 0, 'f', 0)
      .arg (suffix);
}

void addCase (QVector<CorpusCase>* cases,
              QString const& name,
              QString const& profile,
              QString const& text,
              QVariantMap options,
              QString const& suite = QStringLiteral ("stress"))
{
  if (!cases)
    {
      return;
    }
  cases->push_back ({name, suite, profile, text, options});
}

QVector<CorpusCase> makeCases ()
{
  QVector<CorpusCase> cases;
  QString const payload = QStringLiteral (
      "RFLAB corpus payload for FT2-Link acquisition, CFO, drift and weak-mode testing.");
  QString const operational = QStringLiteral (
      "Operational FT2-Link W2300 corpus payload for rate-mode threshold testing.");

  for (double snr : {12.0, 9.0})
    {
      QVariantMap options = withRateMode (0);
      options.insert (QStringLiteral ("snrDb"), snr);
      addCase (&cases,
               QStringLiteral ("operational-fast-awgn-%1db").arg (snr, 0, 'f', 0),
               QStringLiteral ("W2300"), operational, options,
               QStringLiteral ("operational"));
    }
  {
    QVariantMap options = withRateMode (1);
    options.insert (QStringLiteral ("snrDb"), 6.0);
    addCase (&cases, QStringLiteral ("operational-robust-awgn-6db"),
             QStringLiteral ("W2300"), operational, options,
             QStringLiteral ("operational"));
  }
  {
    QVariantMap options = withRateMode (2);
    options.insert (QStringLiteral ("snrDb"), 3.0);
    addCase (&cases, QStringLiteral ("operational-weak-awgn-3db"),
             QStringLiteral ("W2300"), operational, options,
             QStringLiteral ("operational"));
  }
  {
    QVariantMap options = withRateMode (3);
    options.insert (QStringLiteral ("snrDb"), 0.0);
    addCase (&cases, QStringLiteral ("operational-deep-awgn-0db"),
             QStringLiteral ("W2300"), operational, options,
             QStringLiteral ("operational"));
  }
  {
    QVariantMap options = withRateMode (4);
    options.insert (QStringLiteral ("snrDb"), -3.0);
    addCase (&cases, QStringLiteral ("operational-ultra-awgn-minus3db"),
             QStringLiteral ("W2300"), operational, options,
             QStringLiteral ("operational"));
  }

  addCase (&cases, QStringLiteral ("w2300-fast-clean"),
           QStringLiteral ("W2300"), payload, withRateMode (0));

  for (double snr : {18.0, 12.0, 9.0, 6.0})
    {
      QVariantMap options = withRateMode (0);
      options.insert (QStringLiteral ("snrDb"), snr);
      addCase (&cases,
               QStringLiteral ("w2300-fast-awgn-%1db").arg (snr, 0, 'f', 0),
               QStringLiteral ("W2300"), payload, options);
    }

  for (double offset : {-50.0, -25.0, -10.0, -5.0, 5.0, 10.0, 25.0, 50.0})
    {
      QVariantMap options = withRateMode (0);
      options.insert (QStringLiteral ("centerHz"), 1500.0 + offset);
      options.insert (QStringLiteral ("simulatedCenterOffsetHz"), offset);
      addCase (&cases,
               QStringLiteral ("w2300-fast-offset-%1").arg (
                   signedLabel (offset, QStringLiteral ("hz"))),
               QStringLiteral ("W2300"), payload, options);
    }

  for (double drift : {5.0, 10.0, 25.0})
    {
      QVariantMap options = withRateMode (0);
      options.insert (QStringLiteral ("driftHz"), drift);
      addCase (&cases,
               QStringLiteral ("w2300-fast-drift-%1hz").arg (drift, 0, 'f', 0),
               QStringLiteral ("W2300"), payload, options);
    }

  for (double snr : {9.0, 6.0, 3.0, 0.0})
    {
      QVariantMap options = withRateMode (1);
      options.insert (QStringLiteral ("snrDb"), snr);
      addCase (&cases,
               QStringLiteral ("w2300-robust-awgn-%1db").arg (snr, 0, 'f', 0),
               QStringLiteral ("W2300"), payload, options);
    }

  for (double snr : {9.0, 6.0, 3.0, 0.0})
    {
      QVariantMap options = withRateMode (2);
      options.insert (QStringLiteral ("snrDb"), snr);
      addCase (&cases,
               QStringLiteral ("w2300-weak-awgn-%1db").arg (snr, 0, 'f', 0),
               QStringLiteral ("W2300"), payload, options);
    }

  for (double snr : {6.0, 3.0, 0.0, -3.0})
    {
      QVariantMap options = withRateMode (3);
      options.insert (QStringLiteral ("snrDb"), snr);
      addCase (&cases,
               QStringLiteral ("w2300-deep-awgn-%1db").arg (snr, 0, 'f', 0),
               QStringLiteral ("W2300"), payload, options);
    }

  for (double snr : {0.0, -3.0})
    {
      QVariantMap options = withRateMode (4);
      options.insert (QStringLiteral ("snrDb"), snr);
      addCase (&cases,
               QStringLiteral ("w2300-ultra-awgn-%1db").arg (snr, 0, 'f', 0),
               QStringLiteral ("W2300"), payload, options);
    }

  {
    QVariantMap options = withRateMode (2);
    options.insert (QStringLiteral ("snrDb"), 6.0);
    options.insert (QStringLiteral ("centerHz"), 1512.0);
    options.insert (QStringLiteral ("simulatedCenterOffsetHz"), 12.0);
    options.insert (QStringLiteral ("driftHz"), 4.0);
    options.insert (QStringLiteral ("fadeDepthDb"), 5.0);
    options.insert (QStringLiteral ("fadeHz"), 0.22);
    options.insert (QStringLiteral ("clipLevel"), 0.96);
    options.insert (QStringLiteral ("filter"), QStringLiteral ("WIDE"));
    options.insert (QStringLiteral ("sampleRatePpm"), 25.0);
    options.insert (QStringLiteral ("burstDelayMs"), 250);
    addCase (&cases, QStringLiteral ("w2300-weak-realistic-mixed"),
             QStringLiteral ("W2300"), payload, options);
  }

  {
    QVariantMap options = withRateMode (1);
    options.insert (QStringLiteral ("snrDb"), 9.0);
    options.insert (QStringLiteral ("centerHz"), 1492.0);
    options.insert (QStringLiteral ("simulatedCenterOffsetHz"), -8.0);
    options.insert (QStringLiteral ("driftHz"), 3.0);
    options.insert (QStringLiteral ("fadeDepthDb"), 4.0);
    options.insert (QStringLiteral ("fadeHz"), 0.18);
    options.insert (QStringLiteral ("sampleRatePpm"), -20.0);
    addCase (&cases, QStringLiteral ("w2300-robust-realistic-mixed"),
             QStringLiteral ("W2300"), payload, options);
  }

  return cases;
}

bool writeJsonFile (QString const& path, QVariantMap const& map, QString* error)
{
  QFile file {path};
  if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate))
    {
      if (error)
        {
          *error = file.errorString ();
        }
      return false;
    }
  file.write (QJsonDocument::fromVariant (map).toJson (QJsonDocument::Indented));
  file.write ("\n");
  return true;
}
}

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};
  QString const outputDir = app.arguments ().size () > 1
      ? QFileInfo {app.arguments ().at (1)}.absoluteFilePath ()
      : QFileInfo {QStringLiteral ("tmp/ft2link_rflab_corpus")}.absoluteFilePath ();

  if (!QDir ().mkpath (outputDir))
    {
      QTextStream {stderr} << "Cannot create output directory: " << outputDir << '\n';
      return 2;
    }

  FT2LinkQmlAdapter adapter;
  QVector<CorpusCase> const cases = makeCases ();
  QVariantList caseReports;
  int generated = 0;
  int decoded = 0;
  int failedGeneration = 0;

  for (int i = 0; i < cases.size (); ++i)
    {
      CorpusCase const& one = cases.at (i);
      QString const path = QDir {outputDir}.filePath (
          QStringLiteral ("%1-%2.wav")
              .arg (i + 1, 2, 10, QLatin1Char ('0'))
              .arg (safeStem (one.name)));
      QVariantMap generatedReport = adapter.generateRfLabWav (
          path, one.profile, one.text, one.options);
      QVariantMap replayOptions;
      replayOptions.insert (QStringLiteral ("applyToModel"), false);
      QVariantMap replayReport = generatedReport.value (QStringLiteral ("ok")).toBool ()
          ? adapter.replayRfLabWav (path, replayOptions)
          : QVariantMap {};

      QVariantMap item;
      item.insert (QStringLiteral ("name"), one.name);
      item.insert (QStringLiteral ("suite"), one.suite);
      item.insert (QStringLiteral ("profile"), one.profile);
      item.insert (QStringLiteral ("path"), path);
      item.insert (QStringLiteral ("options"), one.options);
      item.insert (QStringLiteral ("generated"), generatedReport);
      item.insert (QStringLiteral ("replay"), replayReport);
      caseReports.push_back (item);

      if (generatedReport.value (QStringLiteral ("ok")).toBool ())
        {
          ++generated;
        }
      else
        {
          ++failedGeneration;
        }
      if (replayReport.value (QStringLiteral ("decodedCount")).toInt () > 0)
        {
          ++decoded;
        }
    }

  QVariantMap manifest;
  manifest.insert (QStringLiteral ("ok"), failedGeneration == 0);
  manifest.insert (QStringLiteral ("outputDir"), outputDir);
  manifest.insert (QStringLiteral ("total"), cases.size ());
  manifest.insert (QStringLiteral ("generated"), generated);
  manifest.insert (QStringLiteral ("decoded"), decoded);
  manifest.insert (QStringLiteral ("generationFailed"), failedGeneration);
  manifest.insert (QStringLiteral ("cases"), caseReports);

  QString error;
  QString const manifestPath = QDir {outputDir}.filePath (
      QStringLiteral ("manifest.json"));
  if (!writeJsonFile (manifestPath, manifest, &error))
    {
      QTextStream {stderr} << "Cannot write manifest: " << error << '\n';
      return 3;
    }

  QTextStream out {stdout};
  out << "FT2-Link RFLAB corpus\n";
  out << "dir: " << outputDir << '\n';
  out << "generated: " << generated << "/" << cases.size () << '\n';
  out << "decoded: " << decoded << "/" << cases.size () << '\n';
  out << "manifest: " << manifestPath << '\n';
  return failedGeneration == 0 ? 0 : 4;
}
