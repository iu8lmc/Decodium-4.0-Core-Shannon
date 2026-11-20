#include <QtCore>
#include <cstdio>
namespace decodium {static void beginActiveSettingsProfile(QSettings& s){s.beginGroup("profile");}}
static void bridgeLog(const QString&){}
class DecodiumBridge { public:
 bool m_ft2Conservative=false,m_applyingReadyProfile=false;QString m_activeReadyProfile;QMap<QString,QVariant> values;
 int notifications=0,legacyCalls=0,saves=0,resets=0;int m_autoCqMaxCycles=0,m_autoCqPauseSec=0,m_autoCqBurstCalls=0,m_autoCqListenCycles=0; qint64 m_autoCqGenericListenUntilMs=123;
 void ft2ConservativeChanged(){++notifications;}void activeReadyProfileChanged(){++notifications;}
 void autoCqMaxCyclesChanged(){++notifications;}void autoCqPauseSecChanged(){++notifications;}
 void autoCqBurstCallsChanged(){++notifications;}void autoCqListenCyclesChanged(){++notifications;}
 void applyAutoCqBurstCadenceToLegacyBackend(){++legacyCalls;}void saveSettingsAsync(){++saves;}void resetAutoCqBurstCadence(const QString&){++resets;}
#define SETTER(name,type) void name(type v){values[QStringLiteral(#name)]=v; if(!m_applyingReadyProfile) qFatal("profile guard missing");}
 SETTER(setFt2FullDecodeInAutoCq,bool) SETTER(setFt2QuickGiveUpStrong,bool) SETTER(setFt2AdaptiveDecode,bool)
 SETTER(setFt2ApHashCache,bool) SETTER(setFt2AsyncSkipRedundantSyncDecode,bool) SETTER(setMamMaxStreams,int)
 SETTER(setMamMultiStream,bool) SETTER(setFt2PartnerMemoryEnabled,bool) SETTER(setFt2Tx2ResendOnStall,bool)
 SETTER(setSmoothDecodeFlow,bool) SETTER(setMaxCallerRetries,int)
#undef SETTER
 void setFt2Conservative(bool);void applyReadyProfile(const QString&);void clearActiveReadyProfileOnManualChange();
 void setAutoCqMaxCycles(int);void setAutoCqPauseSec(int);void setAutoCqBurstCalls(int);void setAutoCqListenCycles(int);
};
#include "bridge_refactor_ftx_profiles.inc"
static void check(bool ok,const char* label){if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}puts(label);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir d;check(d.isValid(),"isolated");QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,d.path());DecodiumBridge b;
 b.applyReadyProfile("weak");check(b.m_activeReadyProfile=="weak"&&b.m_ft2Conservative&&b.values["setMaxCallerRetries"]==7&&!b.values["setFt2AdaptiveDecode"].toBool()&&!b.m_applyingReadyProfile,"weak");
 b.applyReadyProfile("contest");check(b.m_activeReadyProfile=="contest"&&!b.m_ft2Conservative&&b.values["setMamMultiStream"].toBool()&&b.values["setMamMaxStreams"]==2&&b.values["setMaxCallerRetries"]==3,"contest");
 b.applyReadyProfile("cpu");check(b.m_activeReadyProfile=="cpu"&&!b.values["setFt2FullDecodeInAutoCq"].toBool()&&b.values["setMaxCallerRetries"]==10,"cpu");
 b.applyReadyProfile("bad");check(b.m_activeReadyProfile=="balanced"&&b.values["setMaxCallerRetries"]==5,"fallback-balanced");
 b.m_applyingReadyProfile=true;b.clearActiveReadyProfileOnManualChange();check(b.m_activeReadyProfile=="balanced","profile-guard");b.m_applyingReadyProfile=false;b.clearActiveReadyProfileOnManualChange();check(b.m_activeReadyProfile.isEmpty(),"manual-clear");
 b.setAutoCqMaxCycles(9999);b.setAutoCqPauseSec(9999);b.setAutoCqBurstCalls(9999);b.setAutoCqListenCycles(9999);check(b.m_autoCqMaxCycles==999&&b.m_autoCqPauseSec==300&&b.m_autoCqBurstCalls==999&&b.m_autoCqListenCycles==999&&b.m_autoCqGenericListenUntilMs==0&&b.legacyCalls==4&&b.resets==2&&b.saves==2,"auto-max-and-legacy");
 int changes=b.notifications;b.setAutoCqBurstCalls(9999);check(b.notifications==changes&&b.resets==2,"unchanged");b.setAutoCqMaxCycles(-1);b.setAutoCqPauseSec(-1);b.setAutoCqBurstCalls(-1);b.setAutoCqListenCycles(-1);check(!b.m_autoCqMaxCycles&&!b.m_autoCqPauseSec&&!b.m_autoCqBurstCalls&&!b.m_autoCqListenCycles,"auto-min");
 {QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);check(s.value("ActiveReadyProfile").toString().isEmpty()&&s.value("AutoCqBurstCalls").toInt()==0&&s.value("AutoCqListenCycles").toInt()==0,"persist");}
}
