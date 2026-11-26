// -*- Mode: C++ -*-
#ifndef DETECTOR_HASH_SEED_OWN_DIRS_HPP
#define DETECTOR_HASH_SEED_OWN_DIRS_HPP

// Cartelle dati di questa istanza per i semi hash dei worker FT8 e FT4.
//
// I worker cercavano il log vivo del backend embedded solo sotto
// GenericData/IU8LMC/Decodium/embedded-ft2. Su Linux e macOS e' giusto, ma su
// Windows l'organizzazione e' "Decodium" (DecodiumStorageMigration) e il log
// sta in AppLocalData/embedded-ft2 (embeddedLegacyPrivateDataDirPath): non
// entrava fra le fonti e l'ALL.TXT dell'app non riceveva la sua priorita'.

#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

namespace decodium
{
  namespace hash_seed
  {
    // AppLocalData e AppData di questa istanza (con --rig-name il nome cambia).
    inline QStringList own_data_dirs ()
    {
      QStringList dirs;
      for (auto const location : {QStandardPaths::AppLocalDataLocation,
                                  QStandardPaths::AppDataLocation})
        {
          QString const raw = QStandardPaths::writableLocation (location);
          if (raw.isEmpty ())
            {
              continue;
            }
          QString const dir = QDir::cleanPath (raw);
          if (!dirs.contains (dir, Qt::CaseInsensitive))
            {
              dirs.append (dir);
            }
        }
      return dirs;
    }

    // `upperCleanPath` e' QDir::cleanPath (path).toUpper (). Con `embedded`
    // vale solo per la sottocartella embedded-ft2.
    inline bool path_in_own_data_dir (QString const& upperCleanPath, bool embedded)
    {
      for (QString const& dir : own_data_dirs ())
        {
          QString prefix = dir.toUpper () + QLatin1Char ('/');
          if (embedded)
            {
              prefix += QStringLiteral ("EMBEDDED-FT2/");
            }
          if (upperCleanPath.startsWith (prefix))
            {
              return true;
            }
        }
      return false;
    }
  }
}

#endif
