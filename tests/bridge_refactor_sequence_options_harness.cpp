#include <QCoreApplication>
#include <QSettings>
#include <QString>
#include <iostream>
#define emit
void bridgeLog(QString const&){}
namespace decodium {void beginActiveSettingsProfile(QSettings&) {}}
int accumulo=-1;extern "C" void ftx_ft2_set_accumulo_enabled_c(int v){accumulo=v;}
struct Legacy {bool a=false,h=false;int ac=0,hc=0;void setAutoSeq(bool x){a=x;++ac;}void setHoldTxFreq(bool x){h=x;++hc;}};
class DecodiumBridge {public: bool m_autoSeq=false,m_waitPounceActive=false,m_holdTxFreq=false,m_ft2AccumuloEnabled=false,legacyTx=true;QString m_lastWaitPounceKey="old";qint64 m_lastWaitPounceMs=9;Legacy *m_legacyBackend=nullptr;int autoNotices=0,waitNotices=0,holdNotices=0,accNotices=0;
 bool usingLegacyBackendForTx(){return legacyTx;}void autoSeqChanged(){++autoNotices;}void waitPounceActiveChanged(){++waitNotices;}void holdTxFreqChanged(){++holdNotices;}void ft2AccumuloEnabledChanged(){++accNotices;}void statusMessage(QString const&){}
 void setAutoSeq(bool);void setWaitPounceActive(bool);void setHoldTxFreq(bool);void setFt2AccumuloEnabled(bool);};
#include "method.inc"
void check(bool x){if(!x)abort();}
int main(int n,char**v){QCoreApplication app(n,v);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,QString::fromLocal8Bit(qgetenv("SEQ_LAB_SETTINGS")));DecodiumBridge b;Legacy l;b.m_legacyBackend=&l;
 b.setWaitPounceActive(true);check(b.m_waitPounceActive&&b.m_autoSeq&&l.a&&l.ac==1&&b.autoNotices==1&&b.waitNotices==1);b.setWaitPounceActive(true);check(b.waitNotices==1);b.setWaitPounceActive(false);check(b.m_lastWaitPounceKey.isEmpty()&&b.m_lastWaitPounceMs==0);
 b.setHoldTxFreq(true);b.setHoldTxFreq(true);check(b.m_holdTxFreq&&l.h&&l.hc==1&&b.holdNotices==1);QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");check(s.value("HoldTxFreq").toBool()&&s.value("holdTxFreq").toBool()&&!s.value("WaitPounceActive").toBool());
 b.setFt2AccumuloEnabled(true);check(accumulo==1&&b.accNotices==1);b.setFt2AccumuloEnabled(false);check(accumulo==0&&b.accNotices==2);b.legacyTx=false;b.setAutoSeq(false);check(l.ac==1&&!b.m_autoSeq);std::cout<<"autoseq, wait-pounce, legacy hold, persistence and FT2 toggle passed\n";}
