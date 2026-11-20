#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QStringList>
#include <iostream>
static QString latestLog;
static void bridgeLog(const QString& s) { latestLog=s; }
class DecodiumBridge:public QObject {
public:
 QString error;
 void errorMessage(const QString& msg) { error=msg; }
 bool decodiumRxTerminalAvailable() const;
 void openDecodiumRxTerminal();
};
#include "bridge_refactor_under_test.inc"
static void put(const QString& p,const QByteArray& b) { QFile f(p); if(!f.open(QIODevice::WriteOnly)) qFatal("write failed"); f.write(b); f.close(); f.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner); }
int main(int argc,char**argv) {
 QCoreApplication app(argc,argv);QString dir=QCoreApplication::applicationDirPath(), exe=dir+"/decodium-rx", terminal=dir+"/gnome-terminal", output=dir+"/arguments";
 qputenv("PATH",dir.toUtf8());DecodiumBridge bridge;
 std::cout<<"missing|"<<bridge.decodiumRxTerminalAvailable()<<'\n'; bridge.openDecodiumRxTerminal(); std::cout<<"missing-error|"<<bridge.error.contains("was not found")<<'\n';
 put(exe,"#!/bin/sh\nexit 0\n");bridge.error.clear(); bridge.openDecodiumRxTerminal(); std::cout<<"no-terminal|"<<bridge.decodiumRxTerminalAvailable()<<"|"<<bridge.error.contains("needs a terminal")<<'\n';
 put(terminal,"#!/bin/sh\nprintf '%s\\n' \"$1\" \"$2\" > \"$(dirname \"$0\")/arguments\"\n");
 // Avoid a dependency on external dirname after isolating PATH.
 put(terminal,QString("#!/bin/sh\nprintf '%%s\\n' \"$1\" \"$2\" > '%1'\n").arg(output).replace("%%s","%s").toUtf8());
 bridge.error.clear();bridge.openDecodiumRxTerminal();for(int i=0;i<100&&!QFileInfo::exists(output);++i)QThread::msleep(10);
 QFile f(output);if(!f.open(QIODevice::ReadOnly))qFatal("terminal stub did not run");QStringList args=QString::fromUtf8(f.readAll()).split('\n');
 std::cout<<"launch|"<<bridge.error.isEmpty()<<"|"<<(args.value(0)=="--")<<"|"<<(args.value(1)==exe)<<"|"<<latestLog.contains("aperto dal menu")<<'\n';
 f.close(); QFile::remove(output); QFile::remove(terminal); put(dir+"/xterm",QString("#!/bin/sh\nprintf '%%s\\n' \"$1\" \"$2\" > '%1'\n").arg(output).replace("%%s","%s").toUtf8());
 bridge.openDecodiumRxTerminal();for(int i=0;i<100&&!QFileInfo::exists(output);++i)QThread::msleep(10);
 QFile second(output);if(!second.open(QIODevice::ReadOnly))qFatal("fallback terminal did not run");QStringList args2=QString::fromUtf8(second.readAll()).split('\n');
 std::cout<<"fallback|"<<(args2.value(0)=="-e")<<"|"<<(args2.value(1)==exe)<<'\n';
}
