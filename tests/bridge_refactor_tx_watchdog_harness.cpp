#include <QtCore>
#include <cstdio>
namespace decodium {static void beginActiveSettingsProfile(QSettings& s){s.beginGroup("test-profile");}}
struct Legacy {int minutes=-1,calls=0;void setTxWatchdogMinutes(int v){minutes=v;++calls;}};
class DecodiumBridge {public:
 int m_txWatchdogMode=0,m_txWatchdogTime=0,m_txWatchdogCount=1,m_txWatchdogTicks=5,m_autoCQPeriodsMissed=2;
 qint64 m_txWatchdogActiveSinceMs=5,m_txWatchdogLastProgressLogMs=5;bool m_txWatchdogElapsedActive=true,available=true;
 QElapsedTimer m_txWatchdogElapsed; Legacy backend,*m_legacyBackend=&backend;QMap<QString,int> legacySettings;int modeChanges=0,timeChanges=0,countChanges=0,settingChanges=0;
 void syncSettingToLegacyIni(const QString& k,int v){legacySettings[k]=v;} bool legacyBackendAvailable()const{return available;}
 void txWatchdogModeChanged(){++modeChanges;}void txWatchdogTimeChanged(){++timeChanges;}void txWatchdogCountChanged(){++countChanges;}
 void settingValueChanged(const QString&,int){++settingChanges;}
 void persistTxWatchdogSettings();bool txWatchdogLimitConfigured()const;
 void setTxWatchdogMode(int);void setTxWatchdogTime(int);void setTxWatchdogCount(int);
};
#include "bridge_refactor_tx_watchdog.inc"
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}puts(name);}
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);QTemporaryDir d;check(d.isValid(),"isolated-settings");QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,d.path());DecodiumBridge b;
 check(!b.txWatchdogLimitConfigured(),"disabled");b.setTxWatchdogTime(7);b.setTxWatchdogMode(1);
 check(b.txWatchdogLimitConfigured()&&b.backend.minutes==7&&b.legacySettings.value("TxWatchdog")==7&&b.legacySettings.value("TXWatchdog")==7,"time-mode-syncs-legacy");
 {QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);check(s.value("txWatchdogMode")==1&&s.value("txWatchdogTime")==7&&s.value("TxWatchdog")==7,"persist-time-mode");}
 check(b.m_txWatchdogTicks==0&&b.m_txWatchdogActiveSinceMs==0&&!b.m_txWatchdogElapsedActive&&b.m_autoCQPeriodsMissed==0,"change-resets-counters");
 int prev=b.modeChanges;b.setTxWatchdogMode(1);check(b.modeChanges==prev&&b.backend.minutes==7,"unchanged-still-persists");
 b.setTxWatchdogMode(99);b.setTxWatchdogCount(99);check(b.m_txWatchdogMode==2&&b.m_txWatchdogCount==50&&b.backend.minutes==0&&b.txWatchdogLimitConfigured(),"count-mode-and-max-clamp");
 b.setTxWatchdogCount(-1);check(b.m_txWatchdogCount==1,"count-min-clamp");
 b.setTxWatchdogMode(-1);b.setTxWatchdogTime(9999);check(b.m_txWatchdogMode==0&&b.m_txWatchdogTime==999&&!b.txWatchdogLimitConfigured(),"mode-and-time-max-clamp");
 b.setTxWatchdogMode(1);b.setTxWatchdogTime(-5);check(b.m_txWatchdogTime==0&&!b.txWatchdogLimitConfigured()&&b.backend.minutes==0,"time-min-disables");
 b.available=false;int calls=b.backend.calls;b.setTxWatchdogTime(4);check(b.backend.calls==calls&&b.legacySettings.value("TxWatchdog")==4,"backend-unavailable");
}
