// -*- Mode: C++ -*-
#include "Detector/WSPRDecodeWorker.hpp"

#include <QRegularExpression>
#include <QVector>
#include <QMutexLocker>

#include "Detector/FortranRuntimeGuard.hpp"

extern "C"
{
#include "lib/wsprd/wsprd_embedded.h"
}

namespace
{
  struct OutputCollector
  {
    QStringList rows;
    QStringList diagnostics;
  };

  bool looks_like_wspr_decode (QString const& line)
  {
    auto const fields = line.split (QRegularExpression {"\\s+"}, Qt::SkipEmptyParts);
    return fields.size () == 7 || fields.size () == 8;
  }

  void collect_output_line (QString const& raw, bool is_error, OutputCollector& collector)
  {
    auto normalized = raw;
    normalized.replace ("\r\n", "\n");
    normalized.replace ('\r', '\n');
    auto const fragments = normalized.split ('\n', Qt::SkipEmptyParts);
    for (auto const& fragment : fragments)
      {
        auto line = fragment.trimmed ();
        if (line.isEmpty ())
          {
            continue;
          }

        if (is_error)
          {
            collector.diagnostics << line;
            continue;
          }

        if (line == "<DecodeFinished>" || looks_like_wspr_decode (line))
          {
            collector.rows << line;
          }
        else
          {
            collector.diagnostics << line;
          }
      }
  }

  void wsprd_worker_callback (void * context, char const * text, int is_error)
  {
    if (!context || !text)
      {
        return;
      }

    auto& collector = *static_cast<OutputCollector *> (context);
    collect_output_line (QString::fromLocal8Bit (text), is_error != 0, collector);
  }
}

namespace decodium
{
namespace wspr
{

WSPRDecodeWorker::WSPRDecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void WSPRDecodeWorker::decode (DecodeRequest const& request)
{
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};

  OutputCollector collector;
  QVector<QByteArray> argv_storage;
  argv_storage.reserve (request.arguments.size () + 1);
  argv_storage << QByteArray {"wsprd"};
  for (auto const& argument : request.arguments)
    {
      argv_storage << argument.toLocal8Bit ();
    }

  QVector<char *> argv;
  argv.reserve (argv_storage.size ());
  for (auto& item : argv_storage)
    {
      argv << item.data ();
    }

  wsprd_set_output_callback (&wsprd_worker_callback, &collector);
  int const exitCode = wsprd_run (argv.size (), argv.data ());
  wsprd_set_output_callback (nullptr, nullptr);

  Q_EMIT decodeReady (request.serial, collector.rows, collector.diagnostics, exitCode);
}

}
}
