#include <QtCore>
#include "bridge_refactor_under_test.inc"
int main(){QTextStream o(stdout);for(QString s:{"-08","R+09","+99","-99","bad",""})o<<"report|"<<s<<'|'<<normalizeSignalReportText(s)<<'\n';o<<"snr|"<<decodeSnrOrDefault("42")<<'|'<<decodeSnrOrDefault("-90")<<'|'<<decodeSnrOrDefault("bad",-99)<<'\n';o<<"format|"<<formatSignalReport(0)<<'|'<<formatSignalReport(99)<<'|'<<formatSignalReport(-99)<<'\n';}
