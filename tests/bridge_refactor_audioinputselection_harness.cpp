#include <QCoreApplication>
#include <QDebug>
#include <QList>
#include <QString>
#include <QTextStream>
#ifdef SIMULATE_LINUX
#define Q_OS_LINUX 1
#endif
class QAudioDevice { QString name; public: QAudioDevice(QString n={}):name(n){} QString description()const{return name;} };
class QMediaDevices {public:static QAudioDevice defaultAudioInput(){return QAudioDevice("pulse-default");}};
static bool pulseWorks=false;
static QString linuxPulseSourceNameFromDisplay(QString s){return s.startsWith("pulse:")?s.mid(6):QString();}
static bool linuxSetDefaultPulseSource(const QString&,QString*error){if(!pulseWorks)*error="failure";return pulseWorks;}
static void bridgeLog(const QString&){}
static bool tryResolveAudioDeviceByIdentity(const QList<QAudioDevice>& ds,const QString& name,const QString& id,QAudioDevice*out,QString*why){for(const auto&d:ds)if(d.description()==name||(!id.isEmpty()&&d.description()==id)){*out=d;*why=id.isEmpty()?"name":"id";return true;}*why="not-found";return false;}
class DecodiumBridge {public: QList<QAudioDevice> inputs;QAudioDevice def;QList<QAudioDevice> cachedAudioInputs(const QString&,bool){return inputs;}QAudioDevice cachedDefaultAudioInput(const QString&,bool){return def;}QAudioDevice resolveRxInputDevice(const QString&,const QString&,bool*,QString*);};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);DecodiumBridge b;b.inputs={QAudioDevice("one"),QAudioDevice("two")};b.def=QAudioDevice("default");bool found=false;QString why;auto test=[&](QString n,QString id){auto d=b.resolveRxInputDevice(n,id,&found,&why);o<<d.description()<<'|'<<found<<'|'<<why<<'\n';};test(" two ","");test("missing","one");test("missing","");test("","");b.def=QAudioDevice();test("missing","");
#ifdef SIMULATE_LINUX
 pulseWorks=true;test("pulse:synthetic","");pulseWorks=false;test("pulse:synthetic","");
#endif
 b.resolveRxInputDevice("missing","",nullptr,nullptr);o<<"null-ok\n";}
