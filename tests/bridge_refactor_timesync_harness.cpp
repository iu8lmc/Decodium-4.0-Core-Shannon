#include <QtCore>
#include <cstdio>
namespace decodium {static void beginActiveSettingsProfile(QSettings& s){s.beginGroup("test-profile");}}
static void bridgeLog(const QString&){} static double globalOffset=0;static void setGlobalNtpOffsetMs(double x){globalOffset=x;}
struct Clock {bool enabled=false,selfCal=false;int period=0;void setEnabled(bool v){enabled=v;}void setSelfCalibrationEnabled(bool v){selfCal=v;}void setSelfCalPeriodMs(int v){period=v;}};
struct Ntp {bool enabled=false;double offset=0,rtt=0;int refresh=0,syncs=0;QString server;void setCustomServer(const QString&v){server=v;}void setInitialOffset(double v){offset=v;}void setRefreshInterval(int v){refresh=v;}void setMaxRtt(double v){rtt=v;}void setEnabled(bool v){enabled=v;}void syncNow(){++syncs;}};
class DecodiumBridge {public:Clock clock,*m_decoSyncTime=&clock;Ntp ntp,*m_ntpClient=&ntp;bool m_ntpEnabled=false,m_ntpSynced=true;double m_ntpOffsetMs=0;QString m_ntpCustomServer,m_mode="FT8";int enabledChanges=0,offsetChanges=0,syncedChanges=0;
 void ntpEnabledChanged(){++enabledChanges;}void ntpOffsetMsChanged(){++offsetChanges;}void ntpSyncedChanged(){++syncedChanges;}
 void applyNtpSettings();void configureNtpClientForMode(const QString&);
};
#include "bridge_refactor_timesync.inc"
static void check(bool ok,const char* label){if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}puts(label);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir d;check(d.isValid(),"isolated");QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,d.path());DecodiumBridge b;
 b.applyNtpSettings();check(b.m_ntpEnabled&&b.clock.enabled&&!b.clock.selfCal&&b.ntp.syncs==1&&b.ntp.refresh==60000&&b.ntp.rtt==60&&b.clock.period==15000,"default-ft8");
 {QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);s.setValue("NTPEnabled",false);s.setValue("NTPCustomServer"," 127.0.0.1 ");s.setValue("NTPOffset_ms",42.5);s.setValue("DecoSyncSelfCalEnabled",true);}
 b.applyNtpSettings();check(!b.m_ntpEnabled&&!b.clock.enabled&&!b.clock.selfCal&&!b.ntp.enabled&&!b.m_ntpSynced&&globalOffset==0&&b.m_ntpOffsetMs==42.5&&b.ntp.server=="127.0.0.1","legacy-disable");
 {QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);s.setValue("DecoSyncTimeEnabled",true);}
 b.m_mode="ft2";b.applyNtpSettings();check(b.m_ntpEnabled&&b.clock.selfCal&&globalOffset==42.5&&b.ntp.rtt==55&&b.clock.period==2500&&b.ntp.syncs==2,"explicit-wins-ft2");
 b.configureNtpClientForMode(" FT4 ");check(b.ntp.rtt==65&&b.clock.period==7500,"ft4");b.configureNtpClientForMode("JT65");check(b.ntp.rtt==250&&b.ntp.refresh==300000&&b.clock.period==15000,"other-mode");
 b.m_ntpClient=nullptr;b.configureNtpClientForMode("FT8");b.applyNtpSettings();check(b.clock.enabled&&b.m_ntpEnabled,"missing-client");
}
