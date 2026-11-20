#pragma once
#include <QStringList>
#include <QVector>
struct HashResolveAllTxtCandidate { QString timeToken; int audioHz {-1}; QString message; QString source; };
bool parseHashResolveAllTxtLine(QString const&, HashResolveAllTxtCandidate*);
QStringList hashResolveLogCandidatePaths(QString const&);
QString resolveHashMessageFromCandidates(QString const&, int, QString const&, QVector<HashResolveAllTxtCandidate> const&, QString*);
