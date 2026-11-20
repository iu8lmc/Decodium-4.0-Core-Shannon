#include <QApplication>
#include <QDateTime>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
static void bridgeLog(const QString&){}namespace decodium{void beginActiveSettingsProfile(QSettings&){}}
class Alert{public:QStringList played;void playAlert(const QString&s){played<<s;}};
class DecodiumBridge{public:bool m_alertSoundsEnabled=true,m_alertOnMyCall=true,m_alertOnCq=true;Alert*m_alertManager=nullptr;QString m_mode="FT8",m_lastWantedCall;qint64 m_lastMyCallAlertMs=0,m_lastCqAlertMs=0,m_lastWantedAlertMs=0;int periodMsForMode(const QString&)const{return 15000;}void maybePlayDecodeAlert(bool,bool,const QString&);void playAlert(const QString&);};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QApplication app(argc,argv);QTextStream o(stdout);QTemporaryDir temp;if(!temp.isValid())return 2;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());QSettings settings(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");settings.setValue("alertOnWantedCallsign",true);settings.setValue("wantedCallsigns","K1*; W2ABC");settings.sync();Alert a;DecodiumBridge b;b.m_alertManager=&a;b.maybePlayDecodeAlert(false,true,"IU8LMC K1ABC");b.maybePlayDecodeAlert(false,true,"IU8LMC K1ABC");o<<"mycall|"<<a.played.join(",")<<'\n';b.maybePlayDecodeAlert(true,false,"CQ K2ABC");b.maybePlayDecodeAlert(true,false,"CQ K2ABC");o<<"cq|"<<a.played.join(",")<<'\n';b.maybePlayDecodeAlert(false,false,"CQ <K1ABC>");b.maybePlayDecodeAlert(false,false,"CQ K1ABC");o<<"wanted|"<<a.played.join(",")<<'|'<<b.m_lastWantedCall<<'\n';b.maybePlayDecodeAlert(false,false,"CQ K1XYZ");o<<"different|"<<a.played.size()<<'|'<<b.m_lastWantedCall<<'\n';b.m_alertSoundsEnabled=false;b.playAlert("CQ");b.maybePlayDecodeAlert(true,true,"K1ABC");o<<"disabled|"<<a.played.size()<<'\n';}
