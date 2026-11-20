#include <QAudioDevice>
#include <QCoreApplication>
#include <QMediaDevices>
#include <QSettings>
#include <QTemporaryDir>
#include <QVariant>
#include <iostream>
struct Sidetone {int clears=0;double volume=-1;QByteArray device;void clear(){++clears;}void setVolume(double v){volume=v;}void setDeviceId(QByteArray d){device=d;}};
class DecodiumBridge : public QObject {
public:
 Sidetone* m_cwSidetone=nullptr;int changes=0;
 void cwMonitorChanged(){++changes;}
 bool cwMonitorEnabled() const;int cwMonitorVolume() const;QString cwMonitorDevice() const;
 void setCwMonitorEnabled(bool);void setCwMonitorVolume(int);void setCwMonitorDevice(const QString&);QVariantList cwMonitorOutputs() const;
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);QTemporaryDir tmp;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,tmp.path());QCoreApplication::setOrganizationName("cwtest");QCoreApplication::setApplicationName("isolate");
 DecodiumBridge b;Sidetone tone;b.m_cwSidetone=&tone;
 std::cout<<"default|"<<b.cwMonitorEnabled()<<"|"<<b.cwMonitorVolume()<<"|"<<b.cwMonitorDevice().isEmpty()<<'\n';
 b.setCwMonitorEnabled(false); b.setCwMonitorVolume(180);b.setCwMonitorDevice(QString::fromLatin1(QByteArray("device-a").toBase64()));
 std::cout<<"set|"<<b.cwMonitorEnabled()<<"|"<<b.cwMonitorVolume()<<"|"<<tone.clears<<"|"<<tone.volume<<"|"<<(tone.device=="device-a")<<"|"<<b.changes<<'\n';
 b.setCwMonitorVolume(-5);b.setCwMonitorEnabled(true);QSettings settings;settings.sync();
 auto out=b.cwMonitorOutputs(); auto first=out.value(0).toMap();
 std::cout<<"persist|"<<settings.value("CW/monitorVolume").toInt()<<"|"<<settings.value("CW/monitorEnabled").toBool()<<"|"<<(tone.volume==0)<<"|"<<b.changes<<'\n';
 std::cout<<"outputs|"<<(!out.isEmpty())<<"|"<<first.value("id").toString().isEmpty()<<"|"<<(!first.value("label").toString().isEmpty())<<'\n';
}
