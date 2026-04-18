#include <iostream>
#include <exception>
#include <stdexcept>
#include <string>
#include <iterator>
#include <algorithm>
#include <ios>
#include <cstdlib>
#include <locale>
#include <fftw3.h>

#include <QApplication>
#include <QStyleFactory>
#include <QProcessEnvironment>
#include <QTemporaryFile>
#include <QDateTime>
#include <QLocale>
#include <QTranslator>
#include <QRegularExpression>
#include <QObject>
#include <QSettings>
#include <QSysInfo>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QStringList>
#include <QLockFile>
#include <QSplashScreen>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QByteArray>
#include <QBitArray>
#include <QMetaType>
#include <QElapsedTimer>
#include <QThread>

#include "ExceptionCatchingApplication.hpp"
#include "Logger.hpp"
#include "revision_utils.hpp"
#include "MetaDataRegistry.hpp"
#include "qt_helpers.hpp"
#include "L10nLoader.hpp"
#include "SettingsGroup.hpp"
//#include "TraceFile.hpp"
#include "DecodiumLogging.hpp"
#include "MultiSettings.hpp"
#include "widgets/mainwindow.h"
#include "commons.h"
#include "lib/init_random_seed.h"
#include "Radio.hpp"
#include "models/FrequencyList.hpp"
#include "widgets/SplashScreen.hpp"
#include "widgets/MessageBox.hpp"       // last to avoid nasty MS macro definitions

extern "C" {
  // Fortran procedures we need
  void four2a_(_Complex float *, int * nfft, int * ndim, int * isign, int * iform, int len);
}

namespace
{
#if QT_VERSION < QT_VERSION_CHECK (5, 15, 0)
  struct RNGSetup
  {
    RNGSetup ()
    {
      // one time seed of pseudo RNGs from current time
      auto seed = QDateTime::currentMSecsSinceEpoch ();
      std::srand (static_cast<unsigned int>(seed)); // seed std::rand fallback paths
    }
  } seeding;
#endif

  void safe_stream_QVariant (boost::log::record_ostream& os, QVariant const& v)
  {
    switch (static_cast<QMetaType::Type> (v.type ()))
      {
      case QMetaType::QByteArray:
        os << "0x"
#if QT_VERSION >= QT_VERSION_CHECK (5, 9, 0)
           << v.toByteArray ().toHex (':').toStdString ()
#else
           << v.toByteArray ().toHex ().toStdString ()
#endif
          ;
        break;

      case QMetaType::QBitArray:
        {
          auto const& bits = v.toBitArray ();
          os << "0b";
          for (int i = 0; i < bits.size (); ++ i)
            {
              os << (bits[i] ? '1' : '0');
            }
        }
        break;

      default:
        os << v.toString ();
      }
  }

  bool is_sensitive_setting_key (QString const& key)
  {
    static QRegularExpression const sensitive_pattern {
      R"((password|passwd|secret|token|api[_-]?key|apikey|cloudlog|lotw.*(pass|password)))",
      QRegularExpression::CaseInsensitiveOption};
    return key.contains (sensitive_pattern);
  }

}

int main(int argc, char *argv[])
{
  init_random_seed ();

  // make the Qt type magic happen
  Radio::register_types ();
  register_types ();

  // Read optional file to disable highDPI scaling
  QFile f("DisableHighDpiScaling");
  if (!f.exists()) QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

  auto const env = QProcessEnvironment::systemEnvironment ();

  ExceptionCatchingApplication a(argc, argv);
  QObject::connect(&a, &QCoreApplication::aboutToQuit, []() {
    if (auto *instance = QCoreApplication::instance()) {
      instance->setProperty("decodiumShuttingDown", true);
    }
  });
  QApplication::setStyle(QStyleFactory::create("Fusion"));
  try
    {
      // LOG_INfO ("+++++++++++++++++++++++++++ Resources ++++++++++++++++++++++++++++");
      // {
      //   QDirIterator resources_iter {":/", QDirIterator::Subdirectories};
      //   while (resources_iter.hasNext ())
      //     {
      //       LOG_INFO (resources_iter.next ());
      //     }
      // }
      // LOG_INFO ("--------------------------- Resources ----------------------------");

      QLocale locale;              // get the current system locale

      // reset the C+ & C global locales to the classic C locale
      std::locale::global (std::locale::classic ());

      // Override programs executable basename as application name.
      // Keep a dedicated FT2 profile namespace (settings/data/temp) to avoid
      // collisions with WSJT-X profiles and preserve ft2.ini behavior.
      a.setApplicationName ("ft2");
      a.setApplicationVersion (version ());

      QCommandLineParser parser;
      parser.setApplicationDescription ("\n" PROJECT_DESCRIPTION);
      auto help_option = parser.addHelpOption ();
      auto version_option = parser.addVersionOption ();

      // support for multiple instances running from a single installation
      QCommandLineOption rig_option (QStringList {} << "r" << "rig-name"
                                     , "Where <rig-name> is for multi-instance support."
                                     , "rig-name");
      parser.addOption (rig_option);

      // support for start up configuration
      QCommandLineOption cfg_option (QStringList {} << "c" << "config"
                                     , "Where <configuration> is an existing one."
                                     , "configuration");
      parser.addOption (cfg_option);

      // support for UI language override (useful on Windows)
      QCommandLineOption lang_option (QStringList {} << "l" << "language"
                                     , "Where <language> is <lang-code>[-<country-code>]."
                                     , "language");
      parser.addOption (lang_option);

      QCommandLineOption test_option (QStringList {} << "test-mode"
                                      , "Writable files in test location.  Use with caution, for testing only.");
      parser.addOption (test_option);

      if (!parser.parse (a.arguments ()))
        {
          MessageBox::critical_message (nullptr, "Command line error", parser.errorText ());
          return -1;
        }
      else
        {
          if (parser.isSet (help_option))
            {
              MessageBox::information_message (nullptr, "Command line help", parser.helpText ());
              return 0;
            }
          else if (parser.isSet (version_option))
            {
              MessageBox::information_message (nullptr, "Application version", a.applicationVersion ());
              return 0;
            }
        }

      QStandardPaths::setTestModeEnabled (parser.isSet (test_option));

      // support for multiple instances running from a single installation
      bool multiple {false};
      if (parser.isSet (rig_option) || parser.isSet (test_option))
        {
          auto temp_name = parser.value (rig_option);
          if (!temp_name.isEmpty ())
            {
              if (temp_name.contains (QRegularExpression {R"([\\/,])"}))
                {
                  std::cerr << "Invalid rig name - \\ & / not allowed" << std::endl;
                  parser.showHelp (-1);
                }
                
              a.setApplicationName (a.applicationName () + " - " + temp_name);
            }

          if (parser.isSet (test_option))
            {
              a.setApplicationName (a.applicationName () + " - test");
            }

          multiple = true;
        }

      // now we have the application name we can open the logging and settings
      DecodiumLogging lg;
      LOG_INFO (program_title (revision ()) << " - Program startup");
      MultiSettings multi_settings {parser.value (cfg_option)};

      // find the temporary files path
      QDir temp_dir {QStandardPaths::writableLocation (QStandardPaths::TempLocation)};
      Q_ASSERT (temp_dir.exists ()); // sanity check

      // disallow multiple instances with same instance key
      QLockFile instance_lock {temp_dir.absoluteFilePath (a.applicationName () + ".lock")};
      instance_lock.setStaleLockTime (0);
      while (!instance_lock.tryLock (250))
        {
          if (QLockFile::LockFailedError == instance_lock.error ())
            {
              auto button = MessageBox::query_message (nullptr
                                                       , "Another instance may be running"
                                                       , "Another instance is still locking this rig profile. Retry?"
                                                       , QString {}
                                                       , MessageBox::Retry | MessageBox::No
                                                       , MessageBox::Retry);
              switch (button)
                {
                case MessageBox::Retry:
                  break;

                default:
                  throw std::runtime_error {"Multiple instances must have unique rig names"};
                }
            }
          else
            {
              throw std::runtime_error {"Failed to access lock file"};
            }
        }

      // load UI translations — prefer QSettings override, then CLI, then locale
      QString language_override = parser.value (lang_option);
      if (language_override.isEmpty ()) {
        language_override = multi_settings.settings ()->value ("UILanguage").toString ();
      }
      L10nLoader l10n {&a, locale, language_override};

      // Create a unique writeable temporary directory in a suitable location
      bool temp_ok {false};
      QString unique_directory {ExceptionCatchingApplication::applicationName ()};
      do
        {
          if (!temp_dir.mkpath (unique_directory)
              || !temp_dir.cd (unique_directory))
            {
              MessageBox::critical_message (nullptr,
                                            a.translate ("main", "Failed to create a temporary directory"),
                                            a.translate ("main", "Path: \"%1\"").arg (temp_dir.absolutePath ()));
              throw std::runtime_error {"Failed to create a temporary directory"};
            }
          if (!temp_dir.isReadable () || !(temp_ok = QTemporaryFile {temp_dir.absoluteFilePath ("test")}.open ()))
            {
              auto button =  MessageBox::critical_message (nullptr,
                                                           a.translate ("main", "Failed to create a usable temporary directory"),
                                                           a.translate ("main", "Another application may be locking the directory"),
                                                           a.translate ("main", "Path: \"%1\"").arg (temp_dir.absolutePath ()),
                                                           MessageBox::Retry | MessageBox::Cancel);
              if (MessageBox::Cancel == button)
                {
                  throw std::runtime_error {"Failed to create a usable temporary directory"};
                }
              temp_dir.cdUp ();  // revert to parent as this one is no good
            }
        }
      while (!temp_ok);

      // create writeable data directory if not already there
      auto writeable_data_dir = QDir {QStandardPaths::writableLocation (QStandardPaths::AppLocalDataLocation)};
      if (!writeable_data_dir.mkpath ("."))
        {
          MessageBox::critical_message (nullptr, a.translate ("main", "Failed to create data directory"),
                                        a.translate ("main", "path: \"%1\"").arg (writeable_data_dir.absolutePath ()));
          throw std::runtime_error {"Failed to create data directory"};
        }

      // set up SQLite database
      if (!QSqlDatabase::drivers ().contains ("QSQLITE"))
        {
          throw std::runtime_error {"Failed to find SQLite Qt driver"};
        }
      auto db = QSqlDatabase::addDatabase ("QSQLITE");
      db.setDatabaseName (writeable_data_dir.absoluteFilePath ("db.sqlite"));
      if (!db.open ())
        {
          throw std::runtime_error {("Database Error: " + db.lastError ().text ()).toStdString ()};
        }

      auto apply_pragma = [&db] (char const * pragma) {
        auto query = db.exec (pragma);
        if (query.lastError ().isValid ())
          {
            throw std::runtime_error {("Database Error: " + query.lastError ().text ()).toStdString ()};
          }
      };
      apply_pragma ("PRAGMA journal_mode=WAL");
      apply_pragma ("PRAGMA synchronous=NORMAL");
      apply_pragma ("PRAGMA busy_timeout=5000");

      int result;
      auto const& original_style_sheet = a.styleSheet ();
      do
        {
          // dump settings
          auto sys_lg = sys::get ();
          if (auto rec = sys_lg.open_record
              (
               boost::log::keywords::severity = boost::log::trivial::trace)
              )
            {
              boost::log::record_ostream strm (rec);
              strm << "++++++++++++++++++++++++++++ Settings ++++++++++++++++++++++++++++\n";
              for (auto const& key: multi_settings.settings ()->allKeys ())
                {
                  if (!key.contains (QRegularExpression {"^MultiSettings/[^/]*/"}))
                    {
                      auto const& value = multi_settings.settings ()->value (key);
                      if (value.canConvert<QVariantList> ())
                        {
                          strm << key << ":\n";
                          if (is_sensitive_setting_key (key))
                            {
                              strm << "\t<redacted>\n";
                            }
                          else
                            {
                              auto const sequence = value.toList ();
                              for (auto const& item: sequence)
                                {
                                  strm << "\t";
                                  safe_stream_QVariant (strm, item);
                                  strm << '\n';
                                }
                            }
                        }
                      else
                        {
                          strm << key << ": ";
                          if (is_sensitive_setting_key (key))
                            {
                              strm << "<redacted>";
                            }
                          else
                            {
                              safe_stream_QVariant (strm, value);
                            }
                          strm << '\n';
                        }
                    }
                }
              strm << "---------------------------- Settings ----------------------------\n";
              strm.flush ();
              sys_lg.push_record (boost::move (rec));
            }

          unsigned downSampleFactor;
          {
            SettingsGroup {multi_settings.settings (), "Tune"};

            // deal with Windows Vista and earlier input audio rate
            // converter problems
            downSampleFactor = multi_settings.settings ()->value ("Audio/DisableInputResampling",
#if defined (Q_OS_WIN)
                                                                  // default to true for
                                                                  // Windows Vista and older
                                                                  false // Vista-era resampling workaround no longer needed on Win10+
#else
                                                                  false
#endif
                                                                  ).toBool () ? 1u : 4u;
          }

          QDir::setCurrent(qApp->applicationDirPath()); //This helps to find the SF executables

          // run the application UI
          MainWindow w(temp_dir, multiple, &multi_settings, downSampleFactor, nullptr, env);
          w.show();
          QObject::connect (&a, SIGNAL (lastWindowClosed()), &a, SLOT (quit()));
          result = a.exec();

          // ensure config switches start with the right style sheet
          a.setStyleSheet (original_style_sheet);
        }
      while (!result && !multi_settings.exit ());

      // clean up lazily initialized resources
      {
        int nfft {-1};
        int ndim {1};
        int isign {1};
        int iform {1};
        // free FFT plan resources
        four2a_ (nullptr, &nfft, &ndim, &isign, &iform, 0);
      }
      // Do not call global FFTW cleanup here. Several decode paths keep
      // thread_local FFTW plans whose destructors run later during TLS
      // finalization at process exit; tearing down FFTW first can crash on
      // macOS when those destructors call fftwf_destroy_plan().

      temp_dir.removeRecursively (); // clean up temp files
      return result;
    }
  catch (std::exception const& e)
    {
      MessageBox::critical_message (nullptr, "Fatal error", e.what ());
      std::cerr << "Error: " << e.what () << '\n';
    }
  catch (...)
    {
      MessageBox::critical_message (nullptr, "Unexpected fatal error");
      std::cerr << "Unexpected fatal error\n";
      throw;			// hoping the runtime might tell us more about the exception
    }
  return -1;
}
