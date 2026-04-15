#include "DecodiumLogging.hpp"

#include <string>
#include <exception>
#include <sstream>
#include <atomic>

#include <boost/version.hpp>
#include <boost/log/core.hpp>
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/async_frontend.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/expressions/predicates/channel_severity_filter.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/greg_day.hpp>
#include <boost/container/flat_map.hpp>

#include <QtGlobal>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QMessageLogContext>

#include "Logger.hpp"
#include "qt_helpers.hpp"

namespace logging = boost::log;
namespace trivial = logging::trivial;
namespace keywords = logging::keywords;
namespace expr = logging::expressions;
namespace sinks = logging::sinks;
namespace posix_time = boost::posix_time;
namespace gregorian = boost::gregorian;
namespace container = boost::container;

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", trivial::severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)

namespace
{
  // Top level exception handler that gets exceptions from filters and
  // formatters.
  struct exception_handler
  {
    typedef void result;

    void operator () (std::runtime_error const& e) const
    {
      std::cout << "std::runtime_error: " << e.what () << std::endl;
    }
    void operator () (std::logic_error const& e) const
    {
      std::cout << "std::logic_error: " << e.what () << std::endl;
      throw;
    }
  };

  // Reroute Qt messages to the system logger
  void qt_log_handler (QtMsgType type, QMessageLogContext const& context, QString const& msg)
  {
#if defined (Q_OS_LINUX)
    static std::atomic<bool> qsocketnotifier_warning_logged {false};
    static bool const trace_qsocketnotifier_warning =
      qEnvironmentVariableIsSet ("FT2_TRACE_QSOCKETNOTIFIER");
    if (!trace_qsocketnotifier_warning
        && type == QtWarningMsg
        && msg == QStringLiteral ("QSocketNotifier: Can only be used with threads started with QThread"))
      {
        if (!qsocketnotifier_warning_logged.exchange (true))
          {
            auto log = sys::get ();
            BOOST_LOG_SEV (log, trivial::debug)
              << boost::log::add_value ("Line", context.line)
              << boost::log::add_value ("File", context.file ? std::string {context.file} : std::string {})
              << boost::log::add_value ("Function", context.function ? std::string {context.function} : std::string {})
              << "Suppressed Linux startup warning: "
              << msg.toStdString ()
              << " (set FT2_TRACE_QSOCKETNOTIFIER=1 to show it)";
          }
        return;
      }
#endif

    // Convert Qt message types to logger severities
    auto severity = trivial::trace;
    switch (type)
      {
      case QtDebugMsg: severity = trivial::debug; break;
      case QtInfoMsg: severity = trivial::info; break;
      case QtWarningMsg: severity = trivial::warning; break;
      case QtCriticalMsg: severity = trivial::error; break;
      case QtFatalMsg: severity = trivial::fatal; break;
      }
    // Map non-default Qt categories to logger channels, Qt logger
    // context is mapped to the appropriate logger attributes.
    auto log = sys::get ();
    std::string file;
    std::string function;
    if (context.file)
      {
        file = context.file;
      }
    if (context.function)
      {
        function = context.function;
      }
    if (!context.category || !qstrcmp (context.category, "default"))
      {
        BOOST_LOG_SEV (log, severity)
          << boost::log::add_value ("Line", context.line)
          << boost::log::add_value ("File", file)
          << boost::log::add_value ("Function", function)
          << msg.toStdString ();
      }
    else
      {
        BOOST_LOG_SEV (log, severity)
          << boost::log::add_value ("Line", context.line)
          << boost::log::add_value ("File", file)
          << boost::log::add_value ("Function", function)
          << context.category << ": " << msg.toStdString ();
      }
  }

  void default_log_config ()
  {
    auto core = logging::core::get ();

    //
    // Define sinks, filters, and formatters using expression
    // templates for efficiency.
    //

    // Default log file location.
    QDir app_data {QStandardPaths::writableLocation (QStandardPaths::AppLocalDataLocation)};
    Logger::init ();          // Basic setup of attributes

    //
    // Sink intended for general use that passes everything above
    // selected severity levels per channel. Log file is appended
    // between sessions and rotated to limit storage space usage.
    //
    auto sys_sink = boost::make_shared<sinks::asynchronous_sink<sinks::text_file_backend>>
      (
       keywords::auto_flush = false
#if BOOST_VERSION / 100 >= 1070
       , keywords::file_name = app_data.absoluteFilePath ("wsjtx_syslog.log").toStdWString ()
       , keywords::target_file_name =
#else
       , keywords::file_name =
#endif
       app_data.absoluteFilePath ("logs/wsjtx_syslog_%Y-%m.log").toStdWString ()
       , keywords::time_based_rotation = sinks::file::rotation_at_time_point (gregorian::greg_day (1), 0, 0, 0)
       , keywords::open_mode = std::ios_base::out | std::ios_base::app
#if BOOST_VERSION / 100 >= 1063
       , keywords::enable_final_rotation = false
#endif
       );

    sys_sink->locked_backend ()->set_file_collector
      (
       sinks::file::make_collector
       (
        keywords::max_size = 5 * 1024 * 1024
        , keywords::min_free_space = 1024 * 1024 * 1024
        , keywords::max_files = 12
        , keywords::target = app_data.absoluteFilePath ("logs").toStdWString ()
        )
       );
    sys_sink->locked_backend ()->scan_for_files ();

    // Per channel severity level filter
    using min_severity_filter = expr::channel_severity_filter_actor<std::string, trivial::severity_level>;
    min_severity_filter min_severity = expr::channel_severity_filter (channel, severity);
    min_severity["SYSLOG"] = trivial::error;
    min_severity["RIGCTRL"] = trivial::warning;
    min_severity["DATALOG"] = trivial::error;
    sys_sink->set_filter (min_severity || severity >= trivial::fatal);

    sys_sink->set_formatter
      (
       expr::stream
       << "[" << channel
       << "][" << expr::format_date_time<posix_time::ptime> ("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
       << "][" << expr::format_date_time<posix_time::time_duration> ("Uptime", "%O:%M:%S.%f")
       << "][" << trivial::severity
       << "] " << expr::message
       );

    core->add_sink (sys_sink);

    // Indicate start of logging
    LOG_INFO ("Log Start");
  }
}

DecodiumLogging::DecodiumLogging ()
{
  s_instance = this;
  auto core = logging::core::get ();
  // Catch relevant exceptions from logging.
  core->set_exception_handler
    (
     logging::make_exception_handler<std::runtime_error, std::logic_error> (exception_handler {})
     );
 
  // Check for a user-defined logging configuration settings file.
  QFile log_config {QStandardPaths::locate (QStandardPaths::ConfigLocation, "wsjtx_log_config.ini")};
  if (log_config.exists () && log_config.open (QFile::ReadOnly) && log_config.isReadable ())
    {
      QTextStream ts {&log_config};
      auto config = ts.readAll ();

      // Substitution variables.
      container::flat_map<QString, QString> replacements =
        {
         {"DesktopLocation", QStandardPaths::writableLocation (QStandardPaths::DesktopLocation)},
         {"DocumentsLocation", QStandardPaths::writableLocation (QStandardPaths::DocumentsLocation)},
         {"TempLocation", QStandardPaths::writableLocation (QStandardPaths::TempLocation)},
         {"HomeLocation", QStandardPaths::writableLocation (QStandardPaths::HomeLocation)},
         {"CacheLocation", QStandardPaths::writableLocation (QStandardPaths::CacheLocation)},
         {"GenericCacheLocation", QStandardPaths::writableLocation (QStandardPaths::GenericCacheLocation)},
         {"GenericDataLocation", QStandardPaths::writableLocation (QStandardPaths::GenericDataLocation)},
         {"AppDataLocation", QStandardPaths::writableLocation (QStandardPaths::AppDataLocation)},
         {"AppLocalDataLocation", QStandardPaths::writableLocation (QStandardPaths::AppLocalDataLocation)},
        };
      // Parse the configration settings substituting the variable if found.
      QString new_config;
      int pos {0};
      QRegularExpression subst_vars {R"(\${([^}]+)})"};
      auto iter = subst_vars.globalMatch (config);
      while (iter.hasNext ())
        {
          auto match = iter.next ();
          auto const& name = match.captured (1);
          auto repl_iter = replacements.find (name);
          auto repl = repl_iter != replacements.end () ? repl_iter->second : "${" + name + "}";
          new_config += config.mid (pos, match.capturedStart (1) - 2 - pos) + repl;
          pos = match.capturedEnd (0);
        }
      new_config += config.mid (pos);
      std::wstringbuf buffer {new_config.toStdWString (), std::ios_base::in};
      std::wistream stream {&buffer};
      try
        {
          Logger::init_from_config (stream);
          LOG_INFO ("Read logging configuration file: " << log_config.fileName ());
        }
      catch (std::exception const& e)
        {
          default_log_config ();
          LOG_ERROR ("Reading logging configuration file: " << log_config.fileName () << " - " << e.what ());
          LOG_INFO ("Reverting to default logging configuration");
        }
    }
  else                          // Default setup
    {
      default_log_config ();
    }

  previous_qt_message_handler_ = ::qInstallMessageHandler (&qt_log_handler);
}

DecodiumLogging::~DecodiumLogging ()
{
  ::qInstallMessageHandler (previous_qt_message_handler_);
  previous_qt_message_handler_ = nullptr;
  LOG_INFO ("Log Finish");
  auto core = logging::core::get ();
  core->flush ();
  core->remove_all_sinks ();
  s_instance = nullptr;
}

// ── Diagnostic log system ───────────────────────────────────────────────────
#include <QMutex>
#include <QDateTime>
#include <QSysInfo>
#include <QTimeZone>
#include <QGuiApplication>
#include <QScreen>
#include <QMediaDevices>
#include <QAudioDevice>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <csignal>

DecodiumLogging* DecodiumLogging::s_instance = nullptr;
static QMutex g_diagMutex;
static QFile* g_diagFile = nullptr;

static QString diagDir() { return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation); }
static QString diagPath() { return QDir(diagDir()).absoluteFilePath("decodium_diagnostic.log"); }

DecodiumLogging* DecodiumLogging::instance() { return s_instance; }
QString DecodiumLogging::diagnosticLogPath() { return diagPath(); }

QString DecodiumLogging::categoryToString(DiagCategory cat) {
    switch(cat) {
    case DiagCategory::INFO: return "INFO"; case DiagCategory::WARNING: return "WARN";
    case DiagCategory::ERR: return "ERROR"; case DiagCategory::DEBUG: return "DEBUG";
    case DiagCategory::CAT: return "CAT"; case DiagCategory::AUDIO: return "AUDIO";
    case DiagCategory::DECODE: return "DECODE"; case DiagCategory::TX: return "TX";
    case DiagCategory::QML: return "QML"; default: return "???";
    }
}

void DecodiumLogging::diag(DiagCategory cat, const QString& message) {
    QMutexLocker lock(&g_diagMutex);
    if (!g_diagFile) {
        QDir().mkpath(diagDir());
        g_diagFile = new QFile(diagPath());
        if (!g_diagFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            delete g_diagFile; g_diagFile = nullptr; return;
        }
    }
    if (g_diagFile->size() > 5*1024*1024) {
        g_diagFile->close(); delete g_diagFile; g_diagFile = nullptr;
        QDir d(diagDir());
        QFile::remove(d.absoluteFilePath("decodium_diagnostic.3.log"));
        QFile::rename(d.absoluteFilePath("decodium_diagnostic.2.log"), d.absoluteFilePath("decodium_diagnostic.3.log"));
        QFile::rename(d.absoluteFilePath("decodium_diagnostic.1.log"), d.absoluteFilePath("decodium_diagnostic.2.log"));
        QFile::rename(diagPath(), d.absoluteFilePath("decodium_diagnostic.1.log"));
        g_diagFile = new QFile(diagPath());
        if (!g_diagFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            delete g_diagFile; g_diagFile = nullptr; return;
        }
    }
    QString line = QString("[%1] [%2] %3\n").arg(
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"),
        DecodiumLogging::categoryToString(cat), message);
    g_diagFile->write(line.toUtf8());
    g_diagFile->flush();
}

QStringList DecodiumLogging::readLastLines(int n) {
    QFile f(diagPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QStringList all; QTextStream in(&f);
    while (!in.atEnd()) all.append(in.readLine());
    return all.mid(qMax(0, all.size() - n));
}

void DecodiumLogging::logStartupDiagnostics() {
    diagInfo("========== DECODIUM STARTUP ==========");
    diagInfo(QString("Qt: %1 | OS: %2 | CPU: %3").arg(qVersion(), QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture()));
    diagInfo(QString("Locale: %1 | TZ: %2").arg(QLocale::system().name(), QString(QTimeZone::systemTimeZone().id())));
    if (auto* s = QGuiApplication::primaryScreen())
        diagInfo(QString("Screen: %1x%2 @%3dpi").arg(s->size().width()).arg(s->size().height()).arg(s->logicalDotsPerInch()));
    for (auto const& d : QMediaDevices::audioInputs()) diagInfo("AudioIn: " + d.description());
    for (auto const& d : QMediaDevices::audioOutputs()) diagInfo("AudioOut: " + d.description());
#ifdef Q_OS_WIN
    MEMORYSTATUSEX m; m.dwLength = sizeof(m); GlobalMemoryStatusEx(&m);
    diagInfo(QString("RAM: %1 MB free / %2 MB total").arg(m.ullAvailPhys/1048576).arg(m.ullTotalPhys/1048576));
#endif
    diagInfo("======================================");
}

#ifdef Q_OS_WIN
static LONG WINAPI crashHandler(EXCEPTION_POINTERS* ep) {
    DecodiumLogging::diag(DiagCategory::ERR, QString("CRASH: code=0x%1").arg(ep->ExceptionRecord->ExceptionCode, 8, 16, QChar('0')));
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
static void signalHandler(int sig) { DecodiumLogging::diag(DiagCategory::ERR, QString("SIGNAL %1").arg(sig)); signal(sig, SIG_DFL); raise(sig); }
#endif

void DecodiumLogging::installCrashHandler() {
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(crashHandler);
#else
    signal(SIGSEGV, signalHandler); signal(SIGABRT, signalHandler);
#endif
    std::set_terminate([]() { DecodiumLogging::diag(DiagCategory::ERR, "std::terminate()"); std::abort(); });
}
