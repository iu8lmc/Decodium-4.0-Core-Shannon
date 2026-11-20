#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <algorithm>
#include <iostream>
namespace decodium { void beginActiveSettingsProfile(QSettings& s) { s.beginGroup("test-profile"); } }
static constexpr int kMaxFtDecodeThreads=24;
static int ftAutoUiReserveForCores(int cores){return cores>1?1:0;}
static int autoFtThreadCountForCores(int cores){return qBound(1,cores-ftAutoUiReserveForCores(cores),kMaxFtDecodeThreads);}
static void bridgeLog(const QString&) {}
class DecodiumBridge : public QObject {
public:
 int m_ftThreads=4, applyCount=0, threadSignals=0, lowSignals=0, endSignals=0, graphicsSignals=0;
 bool m_ftThreadsAuto=false,m_lowCpuModeEnabled=false,m_lowEndMode=false,m_resetSafeGraphicsNextLaunch=false;
 QString m_uiQuality="High"; QStringList messages;
 void applyLowCpuRuntimeProfile(const QString&) {++applyCount;}
 void setProcessPriority(int) {} void applyReadyProfile(const QString&) {} void setUiQuality(const QString& q){m_uiQuality=q;}
 void ftThreadsChanged(){++threadSignals;} void lowCpuModeEnabledChanged(){++lowSignals;} void lowEndModeChanged(){++endSignals;} void resetSafeGraphicsNextLaunchChanged(){++graphicsSignals;}
 void settingValueChanged(const QString&,bool){} void statusMessage(const QString&s){messages<<s;}
 void setFtThreads(int); void setFtThreadsAuto(bool); void cycleFtThreads(); void setLowCpuModeEnabled(bool); bool looksLikeWeakComputer() const; void setLowEndMode(bool); void setResetSafeGraphicsNextLaunch(bool);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char** argv){
 QCoreApplication app(argc,argv); QCoreApplication::setOrganizationName("BridgeTest"); QCoreApplication::setApplicationName("CpuAB"); QTemporaryDir tmp; if(!tmp.isValid())return 1;
 QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,tmp.path());
 DecodiumBridge b; b.setFtThreads(1000); if(b.m_ftThreads!=24||b.threadSignals!=1)return 2;
 b.cycleFtThreads(); if(b.m_ftThreads!=1||b.threadSignals!=2)return 3;
 b.setFtThreadsAuto(true); if(!b.m_ftThreadsAuto||b.m_ftThreads<1||b.m_ftThreads>24)return 4;
 b.setFtThreadsAuto(false); if(b.m_ftThreadsAuto)return 5;
 b.setLowCpuModeEnabled(true);b.setLowCpuModeEnabled(true); if(!b.m_lowCpuModeEnabled||b.lowSignals!=1)return 6;
 b.setLowEndMode(true); if(!b.m_lowEndMode||b.m_uiQuality!="Low"||b.m_ftThreads>4)return 7;
 b.setLowEndMode(false); if(b.m_lowEndMode||b.m_lowCpuModeEnabled||b.m_uiQuality!="High")return 8;
 b.setResetSafeGraphicsNextLaunch(true); b.setResetSafeGraphicsNextLaunch(false); if(b.graphicsSignals!=2||b.m_resetSafeGraphicsNextLaunch)return 9;
 (void)b.looksLikeWeakComputer();
 QSettings settings(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");settings.beginGroup("test-profile"); if(settings.value("LowCpuMode").toBool()||settings.value("lowCpuModeEnabled").toBool())return 10;
 std::cout << "thread bounds/cycle/auto; low CPU/low-end restoration; graphics reset; weak-computer probe: OK\n";
}
