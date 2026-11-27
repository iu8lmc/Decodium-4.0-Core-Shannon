#include <QtCore>
#include <QtNetwork>
#include <algorithm>
#include <cstdio>
namespace decodium {void beginActiveSettingsProfile(QSettings&s){s.beginGroup("test-profile");}}
struct Server {bool running=false;quint16 port=0;bool isRunning()const{return running;}quint16 wsPort()const{return port;}};
class DecodiumBridge {public:Server*m_remoteServer=nullptr;QVariant legacy;int restarts=0;QVariant readSettingFromLegacyIni(const QString&)const{return legacy;}void syncSettingToLegacyIni(const QString&,QVariant v){legacy=v;}void scheduleUdpMessageClientRestart(){++restarts;}
 int remoteWebSocketPort()const;QStringList networkInterfaceNames()const;QString udpInterfaceName()const;void setUdpInterfaceName(const QString&);
};
#include "bridge_refactor_network_prefs.inc"
static void check(bool v,const char*s){if(!v){fprintf(stderr,"FAIL %s\n",s);exit(1);}puts(s);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir d;check(d.isValid(),"isolated");QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,d.path());DecodiumBridge b;
 check(b.remoteWebSocketPort()==19090,"default-port");{QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);s.setValue("RemoteHttpPort",24001);s.sync();}check(b.remoteWebSocketPort()==24000,"http-fallback");{QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);s.setValue("RemoteWsPort",25000);s.sync();}check(b.remoteWebSocketPort()==25000,"configured-port");Server server;server.running=true;server.port=26000;b.m_remoteServer=&server;check(b.remoteWebSocketPort()==26000,"active-server");
 auto names=b.networkInterfaceNames();QStringList copy=names;copy.removeDuplicates();check(names==copy,"interfaces-unique");for(int i=1;i<names.size();++i)check(names[i-1].compare(names[i],Qt::CaseInsensitive)<=0,"interfaces-sorted");
 b.legacy=QStringList{"legacy"};check(b.udpInterfaceName()=="legacy","legacy-fallback");b.setUdpInterfaceName("  synthetic0 ");check(b.udpInterfaceName()=="synthetic0"&&b.legacy.toStringList()==QStringList{"synthetic0"}&&b.restarts==1,"set");b.setUdpInterfaceName("");check(b.udpInterfaceName().isEmpty()&&b.restarts==2,"clear");}
