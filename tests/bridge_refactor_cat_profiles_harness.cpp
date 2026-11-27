#include <QtCore>
#include <algorithm>
#include <cstdio>
namespace decodium { static void beginActiveSettingsProfile(QSettings& s){ s.beginGroup(QStringLiteral("test-profile")); } }
struct FakeCat { int saves=0, disconnects=0; void saveSettings(){++saves;} void disconnectRig(){++disconnects;} };
static QString lastLog;
static void bridgeLog(const QString& msg){lastLog=msg;}
static void decodiumBridgeCatProfileLog(const QString& msg){lastLog=msg;}
class DecodiumBridge { public:
 FakeCat cat4, hamlib, omni; FakeCat *m_cat4OmCat=&cat4,*m_hamlibCat=&hamlib,*m_omniRigCat=&omni;
 QObject radio; QString m_catBackend="hamlib",m_audioInputDevice="Mic A",m_audioOutputDevice="Speaker A",m_catRigName,m_catMode;
 int m_audioInputChannel=1,m_audioOutputChannel=0, notifications=0, halts=0, reloads=0, cancellations=0; bool m_catConnected=false, transmitting=false;
 QString error,status; QObject* catManagerObj() const {return const_cast<QObject*>(&radio);} QString catBackendForPersistence()const{return m_catBackend;}
 bool sstvTxActive()const{return transmitting;} void cancelSstvTx(){++cancellations;}
 static QString tr(const char* s){return QString::fromUtf8(s);} void halt(){++halts;}
 void refreshPskReporterLocalStation(){} void updateRigTelemetry(double,double,double){}
 void reloadBridgeSettingsFromPersistentStore(){++reloads;} void errorMessage(QString s){error=s;} void statusMessage(QString s){status=s;}
 void catProfilesChanged(){++notifications;} void activeCatProfileChanged(){++notifications;}
 void catConnectedChanged(){} void catRigNameChanged(){} void catModeChanged(){}
 QString activeCatProfile()const; QStringList catProfileList()const; QString suggestedCatProfileName()const;
 bool saveCatProfile(const QString&); bool applyCatProfileSnapshotToSettings(const QString&,bool);
 void noteActiveCatProfileAtStartup(); bool loadCatProfile(const QString&); bool deleteCatProfile(const QString&);
 bool renameCatProfile(const QString&,const QString&);
};
#include "bridge_refactor_cat_helpers.inc"
#include "bridge_refactor_cat_methods.inc"
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);} puts(name);}
int main(int argc,char** argv){
 QCoreApplication app(argc,argv); QTemporaryDir dir; check(dir.isValid(),"isolated-settings");
 QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());
 DecodiumBridge b; b.radio.setProperty("rigName","IC-7300");
 check(b.suggestedCatProfileName()=="IC-7300 Hamlib","suggested-name");
 check(!b.saveCatProfile("  ") && b.error.contains("name"),"reject-empty-name");
 {QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);s.setValue("CATMode","USB");s.setValue("unrelated","keep");s.beginGroup("Transceiver");s.setValue("port","synthetic-port");s.endGroup();s.sync();}
 check(b.saveCatProfile("  Main   station  ") && b.activeCatProfile()=="Main station" && b.catProfileList()==QStringList{"Main station"},"save-normalized");
 check(b.cat4.saves==1 && b.hamlib.saves==1 && b.omni.saves==1,"save-all-managers");
 {QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);s.setValue("catBackend","tci");s.setValue("CATMode","DATA-U");s.beginGroup("Transceiver");s.setValue("port","changed-port");s.endGroup();s.sync();}
 b.noteActiveCatProfileAtStartup();
 {QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);check(s.value("catBackend")=="tci" && s.value("Transceiver/port")=="changed-port" && lastLog.contains("snapshot"),"startup-preserves-live-settings");}
 b.transmitting=true;check(!b.loadCatProfile("Main station") && b.cancellations==1 && b.halts==0,"load-cancels-active-sstv");
 b.transmitting=false;check(!b.loadCatProfile("missing"),"load-missing");
 check(b.loadCatProfile("main STATION") && b.halts==1 && b.reloads==1 && b.hamlib.disconnects==1 && b.cat4.disconnects==1 && b.omni.disconnects==1,"load-existing");
 {QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);check(s.value("catBackend")=="hamlib" && s.value("CATMode")=="USB" && s.value("Transceiver/port")=="synthetic-port" && s.value("unrelated")=="keep","load-restores-snapshot");}
 check(b.renameCatProfile("main station","Second") && b.activeCatProfile()=="Second" && b.catProfileList()==QStringList{"Second"},"rename-active");
 check(b.renameCatProfile("SECOND","second"),"rename-case-noop");
 check(b.saveCatProfile("Other") && !b.renameCatProfile("Second","OTHER"),"rename-conflict");
 check(!b.deleteCatProfile("missing"),"delete-missing");
 check(b.deleteCatProfile("other") && b.activeCatProfile().isEmpty() && b.catProfileList()==QStringList{"Second"},"delete-active");
 check(b.deleteCatProfile("Second") && b.catProfileList().isEmpty(),"delete-final");
 b.m_catBackend="native"; check(b.saveCatProfile("Native") && b.applyCatProfileSnapshotToSettings("native", false),"native-profile-save-and-apply");
 {QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);check(s.value("catBackend")=="hamlib","unsupported-backend-normalized");}
 b.m_catBackend="hamlib"; QString longName(100, QLatin1Char('x')); check(b.saveCatProfile(longName) && b.activeCatProfile().size()==80,"profile-name-length-limit");
 check(!b.renameCatProfile("", "Target") && !b.deleteCatProfile(" "),"reject-invalid-profile-operations");
}
