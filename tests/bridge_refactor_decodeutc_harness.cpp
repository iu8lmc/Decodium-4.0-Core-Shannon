#include <QtCore>
#include <utility>
#include "bridge_refactor_under_test.inc"
int main(){QTextStream o(stdout);for(QString s:{"12:34:56","1234","20261107_235959","25:99:99"})o<<"time|"<<s<<'|'<<normalizeUtcDisplayToken(s)<<'|'<<secondsFromUtcDisplayToken(s)<<'\n';o<<"delta|"<<signedUtcSecondDelta(86399,1)<<'|'<<signedUtcSecondDelta(1,86399)<<'|'<<signedUtcSecondDelta(-1,2)<<'\n';}
