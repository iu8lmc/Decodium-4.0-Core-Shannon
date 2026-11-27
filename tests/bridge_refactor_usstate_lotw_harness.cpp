#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QSet>
#include <iostream>
namespace decodium{void beginActiveSettingsProfile(QSettings&){}}
struct Manager{bool loaded=false,update=false;int loads=0,updates=0;bool ready()const{return loaded;}bool updating()const{return update;}int gridCount()const{return 42;}int stateCount()const{return 2;}void ensureLoadedAsync(){++loads;}void updateNow(){++updates;}};
class DecodiumBridge{public:
 bool m_showUsState=false,m_lotwEnabled=false,offline=false;Manager* m_usStateData=nullptr;QSet<QString> m_lotwUsers;int refreshes=0,notifications=0,reloads=0;bool forced=false;bool offlineMode()const{return offline;}void refreshDecodeListDxcc(){++refreshes;}void showUsStateChanged(){++notifications;}void lotwEnabledChanged(){++notifications;}void statusMessage(const QString&){}void reloadLotwUsers(bool force){++reloads;forced=force;}
 void setShowUsState(bool);bool usStateDataReady()const;bool usStateDataUpdating()const;int usStateGridCount()const;int usStateLocatorCount()const;void updateUsStateData();bool isLotwUser(const QString&)const;void setLotwEnabled(bool);void updateLotwUsers();void forceUpdateLotwUsers();
};
#include "bridge_refactor_usstate_lotw.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};DecodiumBridge b;check(!b.usStateDataReady()&&!b.usStateDataUpdating()&&b.usStateGridCount()==0,"missing-manager");Manager m;b.m_usStateData=&m;b.setShowUsState(true);QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");check(m.loads==1&&b.refreshes==1&&s.value("showUsState").toBool(),"enable-and-persist");b.setShowUsState(true);check(m.loads==1&&b.notifications==1,"duplicate");m.loaded=true;m.update=true;check(b.usStateDataReady()&&b.usStateDataUpdating()&&b.usStateGridCount()==42&&b.usStateLocatorCount()==2,"state-getters");b.offline=true;b.updateUsStateData();check(m.updates==0,"offline");b.offline=false;b.updateUsStateData();check(m.updates==1,"update");b.m_lotwUsers.insert("K1ABC");check(b.isLotwUser("k1abc")&&!b.isLotwUser("unknown"),"callsign");b.setLotwEnabled(true);check(b.reloads==1&&!b.forced,"lotw-enable");b.forceUpdateLotwUsers();check(b.reloads==2&&b.forced,"forced");b.setLotwEnabled(false);check(b.refreshes==2,"disable-refresh");}
