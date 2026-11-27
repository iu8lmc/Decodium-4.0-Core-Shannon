#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>
namespace decodium {void beginActiveSettingsProfile(QSettings&) {}}
static void bridgeLog(const QString&) {}
class DecodiumBridge {public:
 QString m_targetCallSign;int m_targetCallMaxRetries=0,m_targetCallTimeoutS=10,m_targetCallPeriod=0,m_targetCallPauseS=0;bool m_armedWatchEnabled=false,m_armedReArm=false;int changes=0;
 void targetCallSignChanged(){++changes;}void targetCallMaxRetriesChanged(){++changes;}void targetCallTimeoutSChanged(){++changes;}void targetCallPeriodChanged(){++changes;}void targetCallPauseSChanged(){++changes;}void armedWatchEnabledChanged(){++changes;}void armedReArmChanged(){++changes;}
 void setTargetCallSign(const QString&);void setTargetCallMaxRetries(int);void setTargetCallTimeoutS(int);void setTargetCallPeriod(int);void setTargetCallPauseS(int);void setArmedWatchEnabled(bool);void setArmedReArm(bool);
};
#include "bridge_refactor_targetcall.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());DecodiumBridge b;auto check=[](bool ok,const char* name){if(!ok){std::cerr<<name<<" failed\n";std::exit(1);}std::cout<<name<<'\n';};
 b.setTargetCallSign("  k1abc  ");check(b.m_targetCallSign=="K1ABC","callsign");b.setTargetCallMaxRetries(3000);b.setTargetCallTimeoutS(0);b.setTargetCallPeriod(7);b.setTargetCallPauseS(999);b.setArmedWatchEnabled(true);b.setArmedReArm(true);check(b.m_targetCallMaxRetries==999&&b.m_targetCallTimeoutS==10&&b.m_targetCallPeriod==2&&b.m_targetCallPauseS==300,"upper-and-lower");int previous=b.changes;b.setTargetCallSign("K1ABC");b.setArmedWatchEnabled(true);check(b.changes==previous,"no-duplicate-notify");QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");check(s.value("CallFeature/TargetCallSign").toString()=="K1ABC"&&s.value("CallFeature/MaxRetries").toInt()==999&&s.value("CallFeature/Period").toInt()==2&&s.value("CallFeature/PauseS").toInt()==300&&s.value("CallFeature/Armed").toBool()&&s.value("CallFeature/ReArm").toBool(),"persistence");b.setTargetCallMaxRetries(-3);b.setTargetCallTimeoutS(900);b.setTargetCallPeriod(-1);b.setTargetCallPauseS(-4);check(b.m_targetCallMaxRetries==0&&b.m_targetCallTimeoutS==600&&b.m_targetCallPeriod==0&&b.m_targetCallPauseS==0,"opposite-bounds");}
