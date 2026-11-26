#pragma once
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QStringList>
#include <QVector>
struct HashResolveAllTxtCandidate { QString timeToken; int audioHz {-1}; QString message; QString source; };
bool parseHashResolveAllTxtLine(QString const&, HashResolveAllTxtCandidate*);
QStringList hashResolveLogCandidatePaths(QString const&);
QString resolveHashMessageFromCandidates(QString const&, int, QString const&, QVector<HashResolveAllTxtCandidate> const&, QString*);

// Righe recenti dei log ALL.TXT, tenute in RAM per risolvere i nominativi <...>.
//
// La risoluzione cerca una riga dello stesso slot (+-2 s, +-5 Hz) scritta da un
// altro decoder. Prima, a ogni lotto, si rileggevano gli ultimi 8 MB di fino a
// 20 log (oltre 40 MB sul PC di sviluppo) sul thread della UI, e per non
// bloccarla la risoluzione veniva rinviata quasi sempre. L'indice legge dal
// disco solo i byte aggiunti dall'ultima volta e tiene solo le righe degli
// ultimi kKeepSeconds secondi (secondo la data e l'ora scritte nella riga):
// le righe di altri giorni alla stessa ora non possono piu' dare risoluzioni
// sbagliate o ambigue. L'ultima riga senza '\n' (scrittura in corso) si
// legge al giro dopo, intera. Un file piu' corto, con gli ultimi byte gia'
// letti diversi o cresciuto di oltre kMaxAppendBytes riparte dalla coda.
class RecentAllTxtHashIndex
{
public:
    static constexpr qint64 kKeepSeconds = 15 * 60;
    static constexpr qint64 kFutureToleranceSeconds = 120;
    static constexpr qint64 kInitialTailBytes = 128 * 1024;
    static constexpr qint64 kOldFileTailBytes = 4 * 1024;
    static constexpr qint64 kMaxAppendBytes = 1024 * 1024;

    struct Stats
    {
        int files {0};
        int restarts {0};
        qint64 readBytes {0};
        int candidates {0};
    };

    QVector<HashResolveAllTxtCandidate> refresh(QStringList const& paths,
                                                QDateTime const& nowUtc,
                                                Stats* stats = nullptr);

private:
    struct Entry
    {
        QDateTime when;
        HashResolveAllTxtCandidate candidate;
    };
    struct File
    {
        bool valid {false};
        qint64 committed {0};
        QByteArray fingerprint;
        QVector<Entry> entries;
    };

    QMutex mutex_;
    QHash<QString, File> files_;
};
