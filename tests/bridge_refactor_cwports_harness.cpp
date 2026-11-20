#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QString>
#include <iostream>
namespace decodium::ports {struct SerialPortRegistry{static QString owner;static QString ownerForPort(const QString&){return owner;}};QString SerialPortRegistry::owner;}
struct Cat {QString transport="serial",port="COM5",ptt="COM7",method="DTR";bool morse=true;QString portType()const{return transport;}QString serialPort()const{return port;}QString pttPort()const{return ptt;}QString pttMethod()const{return method;}bool rigMorseAvailable()const{return morse;}};
class DecodiumBridge:public QObject {public:QString m_mode="CW",m_catBackend="hamlib";bool m_decoPortUseRemote=false;Cat*m_hamlibCat=nullptr;bool cwRigKeyerAvailable()const;QString cwSerialKeyerPortReservation(const QString&)const;};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;Cat c;b.m_hamlibCat=&c;std::cout<<"keyer|"<<b.cwRigKeyerAvailable();b.m_decoPortUseRemote=true;std::cout<<"|"<<b.cwRigKeyerAvailable()<<'\n';b.m_decoPortUseRemote=false;decodium::ports::SerialPortRegistry::owner="synthetic owner";std::cout<<"lease|"<<b.cwSerialKeyerPortReservation("COM5").contains("synthetic owner")<<'\n';decodium::ports::SerialPortRegistry::owner.clear();std::cout<<"cat|"<<b.cwSerialKeyerPortReservation("  com5 ").contains("CAT port")<<'\n';std::cout<<"ptt|"<<b.cwSerialKeyerPortReservation("COM7").contains("PTT port")<<'\n';c.method="CAT";std::cout<<"ptt-cat|"<<b.cwSerialKeyerPortReservation("COM7").isEmpty()<<'\n';c.transport="network";std::cout<<"network|"<<b.cwSerialKeyerPortReservation("COM5").isEmpty()<<'\n';c.transport="serial";QTemporaryDir tmp;QString real=tmp.filePath("tty"),alias=tmp.filePath("alias");QFile f(real);if(!f.open(QIODevice::WriteOnly))qFatal("fixture");f.close();if(!QFile::link(real,alias))qFatal("link");c.port=real;std::cout<<"alias|"<<!b.cwSerialKeyerPortReservation(alias).isEmpty()<<'\n';}
