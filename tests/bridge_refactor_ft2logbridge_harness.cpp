#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QRunnable>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThreadPool>
#include <QTimer>
#include <QVariant>
#define qApp QCoreApplication::instance()
static void bridgeLog(const QString&){}
namespace decodium::logbridge {struct StartResult{bool alreadyRunning=false,ok=false;qint64 pid=0;QString message;};static QString found;static int starts=0;static bool isRunning(){return false;}static QString findConfiguredCopy(){return found;}static bool isConfigured(const QString&p){return !p.isEmpty();}static StartResult start(const QString&){++starts;return {false,true,42,"ok"};}static int configuredUdpPort(const QString&){return 2237;}}
class DecodiumBridge:public QObject{public:QString m_ft2LogBridgeStatus;QMap<QString,QVariant>settings;int events=0;QVariant getSetting(const QString&k,const QVariant&v={})const{return settings.value(k,v);}void setSetting(const QString&k,const QVariant&v){settings[k]=v;}void ft2LogBridgeStatusChanged(){++events;}void setFt2LogBridgeStatus(const QString&);bool ft2LogBridgeRunning()const;void startFt2LogBridgeNow();void detectFt2LogBridgePath();void launchFt2LogBridge(bool);};
#include "bridge_refactor_under_test.inc"
static void wait(int ms){QEventLoop e;QTimer::singleShot(ms,&e,&QEventLoop::quit);e.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);DecodiumBridge b;b.setFt2LogBridgeStatus("ready");b.setFt2LogBridgeStatus("ready");o<<"status|"<<b.events<<'|'<<b.ft2LogBridgeRunning()<<'\n';b.settings["FT2LogBridgeAutoStart"]=false;b.launchFt2LogBridge(true);o<<"disabled|"<<decodium::logbridge::starts<<'\n';b.detectFt2LogBridgePath();wait(100);o<<"missing|"<<b.m_ft2LogBridgeStatus.startsWith("No configured")<<'\n';QTemporaryDir dir;QString p=dir.filePath("fake");QFile f(p);if(!f.open(QIODevice::WriteOnly))return 2;f.close();decodium::logbridge::found=p;b.detectFt2LogBridgePath();wait(100);o<<"found|"<<(b.settings.value("FT2LogBridgePath").toString()==p)<<'\n';b.settings["UDPServer"]="192.0.2.1";b.startFt2LogBridgeNow();wait(100);o<<"started|"<<decodium::logbridge::starts<<'|'<<b.m_ft2LogBridgeStatus.contains("Warning:")<<'\n';b.settings["UDPServer"]="127.0.0.1";b.settings["UDPPrimarySendQso"]=false;b.startFt2LogBridgeNow();wait(100);o<<"disabled-qso|"<<decodium::logbridge::starts<<'|'<<b.m_ft2LogBridgeStatus.contains("QSO logged")<<'\n';}
