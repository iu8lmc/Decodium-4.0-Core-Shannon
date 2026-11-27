#include <QtCore>
namespace decodium { void beginActiveSettingsProfile(QSettings& s){s.beginGroup("test");} }
using DWORD=unsigned long;using HANDLE=int;
constexpr DWORD NORMAL_PRIORITY_CLASS=32,ABOVE_NORMAL_PRIORITY_CLASS=32768,HIGH_PRIORITY_CLASS=128,REALTIME_PRIORITY_CLASS=256;
DWORD lastClass=0;int calls=0;bool accepted=true;HANDLE GetCurrentProcess(){return 1;}bool SetPriorityClass(HANDLE,DWORD c){lastClass=c;++calls;return accepted;}DWORD GetLastError(){return 5;}
void bridgeLog(const QString&){}
class DecodiumBridge { public:int m_processPriority=1,notifications=0;void processPriorityChanged(){++notifications;}void applyProcessPriority();void setProcessPriority(int); };
#define Q_OS_WIN
#define emit
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());DecodiumBridge b;QTextStream o(stdout);for(int v:{-10,1,2,3,9}){b.setProcessPriority(v);o<<b.m_processPriority<<'|'<<lastClass<<'|'<<b.notifications<<'|'<<calls<<'\n';}accepted=false;b.setProcessPriority(2);QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");s.beginGroup("test");o<<"failure|"<<b.m_processPriority<<'|'<<lastClass<<'|'<<b.notifications<<'|'<<s.value("ProcessPriority").toInt()<<'\n';}
