#ifndef DECODIUM_LOGGING_HPP__
#define DECODIUM_LOGGING_HPP__

#include <QtGlobal>
#include <QString>
#include <QStringList>

// Diagnostic categories for the Decodium diagnostic log
enum class DiagCategory
{
  INFO,
  WARNING,
  ERR,
  DEBUG,
  CAT,
  AUDIO,
  DECODE,
  TX,
  QML
};

//
// Class DecodiumLogging - wraps application specific logging
//
// The Boost.Log system (constructor/destructor) is preserved.
// The diagnostic system is ADDITIONAL and writes to its own file
// (decodium_diagnostic.log) using QFile/QMutex for thread safety.
//
class DecodiumLogging final
{
public:
  explicit DecodiumLogging ();
  ~DecodiumLogging ();

  DecodiumLogging (DecodiumLogging const&) = delete;
  DecodiumLogging& operator= (DecodiumLogging const&) = delete;

  // Singleton accessor
  static DecodiumLogging* instance ();

  // Core diagnostic write function
  static void diag (DiagCategory cat, const QString& msg);

  // Convenience functions
  static void diagInfo   (const QString& msg) { diag (DiagCategory::INFO,    msg); }
  static void diagWarn   (const QString& msg) { diag (DiagCategory::WARNING, msg); }
  static void diagError  (const QString& msg) { diag (DiagCategory::ERR,   msg); }
  static void diagDebug  (const QString& msg) { diag (DiagCategory::DEBUG,   msg); }
  static void diagCat    (const QString& msg) { diag (DiagCategory::CAT,     msg); }
  static void diagAudio  (const QString& msg) { diag (DiagCategory::AUDIO,   msg); }
  static void diagDecode (const QString& msg) { diag (DiagCategory::DECODE,  msg); }
  static void diagTx     (const QString& msg) { diag (DiagCategory::TX,      msg); }
  static void diagQml    (const QString& msg) { diag (DiagCategory::QML,     msg); }

  // Log startup diagnostics (OS, Qt, locale, screen, audio, etc.)
  void logStartupDiagnostics ();

  // Returns the full path to the diagnostic log file
  static QString diagnosticLogPath ();

  // Read the last N lines from the diagnostic log
  static QStringList readLastLines (int n = 200);

  // Category to string helper
  static QString categoryToString (DiagCategory cat);

  // Install crash/signal handlers
  static void installCrashHandler ();

private:
  QtMessageHandler previous_qt_message_handler_ {nullptr};
  static DecodiumLogging* s_instance;
};

// Convenience macros
#define DIAG_INFO(msg)    DecodiumLogging::diagInfo(msg)
#define DIAG_WARN(msg)    DecodiumLogging::diagWarn(msg)
#define DIAG_ERROR(msg)   DecodiumLogging::diagError(msg)
#define DIAG_CAT(msg)     DecodiumLogging::diagCat(msg)
#define DIAG_TX(msg)      DecodiumLogging::diagTx(msg)
#define DIAG_AUDIO(msg)   DecodiumLogging::diagAudio(msg)
#define DIAG_DECODE(msg)  DecodiumLogging::diagDecode(msg)
#define DIAG_QML(msg)     DecodiumLogging::diagQml(msg)

#endif
