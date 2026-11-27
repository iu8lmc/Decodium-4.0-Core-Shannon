#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <QTextStream>
#include <algorithm>
#include <cmath>
static void bridgeLog(const QString&){}
class DecodiumBridge{public:double m_localCatFrequencyTargetHz=0,m_localCatFrequencyPreviousHz=0;qint64 m_localCatFrequencyGuardUntilMs=0,m_localCatFrequencyGuardMaxMs=0,m_lastIgnoredCatFrequencyLogMs=0;int m_localTxFrequencyTargetHz=-1,m_localRxFrequencyTargetHz=-1;qint64 m_localTxFrequencyGuardUntilMs=0,m_localRxFrequencyGuardUntilMs=0;bool shouldIgnoreCatFrequencyDuringLocalQsy(double,const QString&);bool shouldIgnoreLegacyAudioFrequencyDuringLocalQsy(int,bool);};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);DecodiumBridge b;auto now=QDateTime::currentMSecsSinceEpoch();o<<"inactive|"<<b.shouldIgnoreCatFrequencyDuringLocalQsy(14000000,"mock")<<'\n';b.m_localCatFrequencyTargetHz=14074000;b.m_localCatFrequencyPreviousHz=14070000;b.m_localCatFrequencyGuardUntilMs=now+1000;b.m_localCatFrequencyGuardMaxMs=now+6000;o<<"stale|"<<b.shouldIgnoreCatFrequencyDuringLocalQsy(14070000,"mock")<<'|'<<(b.m_localCatFrequencyGuardUntilMs>now+1000)<<'\n';o<<"target|"<<b.shouldIgnoreCatFrequencyDuringLocalQsy(14074000,"mock")<<'|'<<b.m_localCatFrequencyTargetHz<<'\n';b.m_localCatFrequencyTargetHz=14074000;b.m_localCatFrequencyPreviousHz=14070000;b.m_localCatFrequencyGuardUntilMs=now+3000;o<<"manual|"<<b.shouldIgnoreCatFrequencyDuringLocalQsy(14100000,"mock")<<'|'<<b.m_localCatFrequencyTargetHz<<'\n';b.m_localRxFrequencyTargetHz=1200;b.m_localRxFrequencyGuardUntilMs=now+3000;o<<"rx|"<<b.shouldIgnoreLegacyAudioFrequencyDuringLocalQsy(1000,false)<<'|'<<b.shouldIgnoreLegacyAudioFrequencyDuringLocalQsy(1200,false)<<'|'<<b.m_localRxFrequencyTargetHz<<'\n';b.m_localTxFrequencyTargetHz=1500;b.m_localTxFrequencyGuardUntilMs=now-1;o<<"expired|"<<b.shouldIgnoreLegacyAudioFrequencyDuringLocalQsy(1000,true)<<'|'<<b.m_localTxFrequencyTargetHz<<'\n';}
