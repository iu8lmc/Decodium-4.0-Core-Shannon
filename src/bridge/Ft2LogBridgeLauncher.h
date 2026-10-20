#pragma once

// FT2 Log Bridge: il client di community.ft2.it che riceve da Decodium i QSO
// loggati (UDP stile WSJT-X, porta 2237) e li carica nel Log Online.
//
// E' un programma separato (Go, icona nel tray) che ogni operatore scarica
// dalla Dashboard del sito gia' configurato con la propria chiave. Qui c'e'
// solo quello che serve a Decodium per avviarlo da solo:
//   - riconoscere una copia CONFIGURATA (config.json con una api_key vera);
//   - trovarla nei posti dove finisce di solito (Download, Documenti, Desktop);
//   - sapere se sta gia' girando: una seconda copia non puo' aprire la porta
//     UDP e mostra una finestra d'errore, quindi non va mai avviata due volte.

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

#include <deque>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace decodium
{
namespace logbridge
{

inline QString executableName ()
{
#ifdef Q_OS_WIN
  return QStringLiteral ("ft2logbridge.exe");
#else
  return QStringLiteral ("ft2logbridge");
#endif
}

inline QJsonObject readConfig (QString const& exePath)
{
  QFile file {QFileInfo {exePath}.dir ().filePath (QStringLiteral ("config.json"))};
  if (!file.open (QIODevice::ReadOnly))
    {
      return {};
    }
  QByteArray data = file.readAll ();
  if (data.startsWith ("\xEF\xBB\xBF"))
    {
      data.remove (0, 3);   // il client stesso tollera il BOM di Blocco note
    }
  return QJsonDocument::fromJson (data).object ();
}

// Configurata = accanto all'eseguibile c'e' config.json con una chiave vera.
// Senza, il client si ferma con una finestra d'avviso: avviarlo non serve.
inline bool isConfigured (QString const& exePath)
{
  if (!QFileInfo {exePath}.isFile ())
    {
      return false;
    }
  QString const key = readConfig (exePath).value (QStringLiteral ("api_key")).toString ().trimmed ();
  return !key.isEmpty () && key != QStringLiteral ("INCOLLA-QUI-LA-TUA-API-KEY");
}

inline int configuredUdpPort (QString const& exePath)
{
  int const port = readConfig (exePath).value (QStringLiteral ("udp_port")).toInt ();
  return port > 0 && port < 65536 ? port : 2237;
}

inline bool isRunning ()
{
#ifdef Q_OS_WIN
  HANDLE const snapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
    {
      return false;
    }
  PROCESSENTRY32W entry {};
  entry.dwSize = sizeof (entry);
  bool found = false;
  for (BOOL ok = Process32FirstW (snapshot, &entry); ok; ok = Process32NextW (snapshot, &entry))
    {
      if (QString::fromWCharArray (entry.szExeFile).compare (executableName (), Qt::CaseInsensitive) == 0)
        {
          found = true;
          break;
        }
    }
  CloseHandle (snapshot);
  return found;
#else
  QDir const proc {QStringLiteral ("/proc")};
  for (QString const& pid : proc.entryList (QDir::Dirs | QDir::NoDotAndDotDot))
    {
      bool numeric = false;
      pid.toLongLong (&numeric);
      if (!numeric)
        {
          continue;
        }
      QFile comm {proc.filePath (pid + QStringLiteral ("/comm"))};
      if (comm.open (QIODevice::ReadOnly) && comm.readAll ().trimmed () == "ft2logbridge")
        {
          return true;
        }
    }
  return false;
#endif
}

// Cerca la copia configurata piu' recente. La Dashboard la fa scaricare come
// .zip, che finisce quasi sempre in Download (o nella cartella di Telegram, se
// passa da li'); si scende al massimo di tre livelli e si visita un numero
// limitato di cartelle, perche' gira all'avvio di Decodium.
//
// In AMPIEZZA, livello per livello: una cartella Download vera ha poche
// centinaia di sottocartelle ai primi due livelli e migliaia al terzo (misurato:
// 219, 539, 7270). In profondita' il limite si esauriva dentro il primo
// albero di sorgenti e la copia in "Telegram Desktop\..." non veniva mai vista.
inline QString findConfiguredCopy ()
{
  QStringList roots;
  for (auto const location : {QStandardPaths::DownloadLocation,
                              QStandardPaths::DocumentsLocation,
                              QStandardPaths::DesktopLocation})
    {
      QString const path = QStandardPaths::writableLocation (location);
      if (!path.isEmpty () && !roots.contains (path))
        {
          roots << path;
        }
    }

  QString best;
  QDateTime bestTime;
  constexpr int kMaxDepth = 3;
  constexpr int kMaxDirsPerRoot = 6000;
  QStringList const skip {QStringLiteral (".git"), QStringLiteral ("node_modules"), QStringLiteral ("__pycache__")};
  for (QString const& root : roots)
    {
      std::deque<std::pair<QString, int>> pending {{root, 0}};
      int visited = 0;
      while (!pending.empty () && visited < kMaxDirsPerRoot)
        {
          auto const [dirPath, depth] = pending.front ();
          pending.pop_front ();
          ++visited;
          QDir const dir {dirPath};
          QString const candidate = dir.filePath (executableName ());
          if (QFileInfo::exists (candidate) && isConfigured (candidate))
            {
              QDateTime const when = QFileInfo {dir.filePath (QStringLiteral ("config.json"))}.lastModified ();
              if (best.isEmpty () || when > bestTime)
                {
                  best = QDir::toNativeSeparators (QFileInfo {candidate}.absoluteFilePath ());
                  bestTime = when;
                }
            }
          if (depth < kMaxDepth)
            {
              for (QString const& sub : dir.entryList (QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks))
                {
                  if (!skip.contains (sub, Qt::CaseInsensitive))
                    {
                      pending.emplace_back (dir.filePath (sub), depth + 1);
                    }
                }
            }
        }
    }
  return best;
}

struct StartResult
{
  bool ok {false};
  bool alreadyRunning {false};
  qint64 pid {0};
  QString message;
};

inline StartResult start (QString const& exePath)
{
  StartResult result;
  if (isRunning ())
    {
      result.ok = true;
      result.alreadyRunning = true;
      result.message = QStringLiteral ("already running");
      return result;
    }
  QFileInfo const exe {exePath};
  if (!exe.isFile ())
    {
      result.message = QStringLiteral ("executable not found: %1").arg (QDir::toNativeSeparators (exePath));
      return result;
    }
  if (!isConfigured (exe.absoluteFilePath ()))
    {
      result.message = QStringLiteral ("config.json with an API key is missing next to %1")
                           .arg (QDir::toNativeSeparators (exe.absoluteFilePath ()));
      return result;
    }
  // La cartella di lavoro e' quella dell'eseguibile: il client cerca li'
  // config.json e scrive li' ft2logbridge.log.
  result.ok = QProcess::startDetached (exe.absoluteFilePath (), {}, exe.absolutePath (), &result.pid);
  result.message = result.ok ? QStringLiteral ("started")
                             : QStringLiteral ("could not start %1").arg (QDir::toNativeSeparators (exe.absoluteFilePath ()));
  return result;
}

}  // namespace logbridge
}  // namespace decodium
