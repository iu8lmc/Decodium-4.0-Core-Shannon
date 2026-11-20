#include <QCoreApplication>
#include <QEventLoop>
#include <QList>
#include <QObject>
#include <QString>
#include <QTextStream>
#include <QTimer>
class QAudioDevice { QString name; public: QAudioDevice(QString n={}):name(n){} QString description()const{return name;} };
class QMediaDevices {public: static QAudioDevice defaultAudioInput(){return QAudioDevice("system-in");}static QAudioDevice defaultAudioOutput(){return QAudioDevice("system-out");}};
class DecodiumBridge:public QObject { public:
 bool m_shuttingDown=false,m_pendingAudioDeviceRefreshVerbose=false,m_audioDeviceCacheValid=false,m_audioDeviceCacheDirty=false;QTimer* m_audioDeviceRefreshTimer=nullptr;QList<QAudioDevice> m_cachedAudioInputs,m_cachedAudioOutputs;QAudioDevice m_cachedDefaultAudioInput,m_cachedDefaultAudioOutput;int refreshes=0;QString last;bool lastVerbose=false,lastSignals=false;
 void refreshAudioDeviceCache(const QString&r,bool v,bool s){++refreshes;last=r;lastVerbose=v;lastSignals=s;m_audioDeviceCacheValid=true;m_audioDeviceCacheDirty=false;}
 void scheduleAudioDeviceRefresh(int,bool);QList<QAudioDevice> cachedAudioInputs(const QString&,bool);QList<QAudioDevice> cachedAudioOutputs(const QString&,bool);QAudioDevice cachedDefaultAudioInput(const QString&,bool);QAudioDevice cachedDefaultAudioOutput(const QString&,bool);void enumerateAudioDevices();
};
#include "bridge_refactor_under_test.inc"
static void wait(int ms){QEventLoop e;QTimer::singleShot(ms,&e,&QEventLoop::quit);e.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);
 {DecodiumBridge b;b.m_cachedAudioInputs.append(QAudioDevice("cached-in"));auto a=b.cachedAudioInputs("sync",true);o<<"sync|"<<b.refreshes<<'|'<<b.last<<'|'<<b.lastSignals<<'|'<<a.first().description()<<'\n';}
 {DecodiumBridge b;b.m_cachedAudioOutputs.append(QAudioDevice("cached-out"));auto a=b.cachedAudioOutputs("async",false);o<<"scheduled|"<<b.refreshes<<'|'<<a.first().description()<<'\n';wait(800);o<<"fired|"<<b.refreshes<<'|'<<b.last<<'|'<<b.lastVerbose<<'\n';}
 {DecodiumBridge b;b.m_audioDeviceCacheValid=true;b.m_cachedDefaultAudioInput=QAudioDevice("preferred-in");o<<"default|"<<b.cachedDefaultAudioInput("x",false).description()<<'|'<<b.cachedDefaultAudioOutput("x",false).description()<<'|'<<b.refreshes<<'\n';}
 {DecodiumBridge b;b.scheduleAudioDeviceRefresh(0,false);b.scheduleAudioDeviceRefresh(0,true);wait(20);o<<"verbose|"<<b.refreshes<<'|'<<b.lastVerbose<<'\n';}
 {DecodiumBridge b;QTimer timer;b.m_audioDeviceRefreshTimer=&timer;b.scheduleAudioDeviceRefresh(-5,true);o<<"timer|"<<timer.isActive()<<'|'<<timer.interval()<<'|'<<b.m_pendingAudioDeviceRefreshVerbose<<'\n';}
 {DecodiumBridge b;b.m_shuttingDown=true;b.scheduleAudioDeviceRefresh(0,true);wait(10);o<<"shutdown|"<<b.refreshes<<'|'<<b.m_pendingAudioDeviceRefreshVerbose<<'\n';}
 {DecodiumBridge b;b.enumerateAudioDevices();o<<"enumerate|"<<b.refreshes<<'|'<<b.last<<'|'<<b.lastVerbose<<'|'<<b.lastSignals<<'\n';}
}
