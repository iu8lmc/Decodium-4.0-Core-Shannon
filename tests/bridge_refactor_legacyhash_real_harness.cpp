#include <QtCore>
#include "LegacyHashResolve.h"
#ifdef INCLUDE_EXTRACT
#include "bridge_refactor_under_test.inc"
#endif
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);HashResolveAllTxtCandidate a,b;bool valid=parseHashResolveAllTxtLine("261107_120000 14.074 Rx FT8 -12 0.1 1500 K1ABC K2XYZ RR73",&a);bool tx=parseHashResolveAllTxtLine("261107_120000 14.074 Tx FT8 -12 0.1 1500 K1ABC K2XYZ RR73",&b);a.source="fixture";QString source;auto resolved=resolveHashMessageFromCandidates("120001",1503,"<...> K2XYZ RR73",{a},&source);o<<"parse|"<<valid<<'|'<<tx<<'|'<<a.timeToken<<'|'<<a.audioHz<<'|'<<a.message<<'\n';o<<"match|"<<resolved<<'|'<<source<<'\n';b=a;b.message="K9XYZ K2XYZ RR73";source.clear();o<<"ambiguous|"<<resolveHashMessageFromCandidates("120001",1500,"<...> K2XYZ RR73",{a,b},&source)<<'|'<<source.isEmpty()<<'\n';o<<"outside|"<<resolveHashMessageFromCandidates("120010",1500,"<...> K2XYZ RR73",{a},nullptr)<<'\n';QTemporaryDir d;QString p=d.filePath("ALL.TXT");auto paths=hashResolveLogCandidatePaths(p);o<<"paths|"<<(!paths.isEmpty() && paths.first()==p)<<'|'<<(paths.count(p)==1)<<'\n';}
