#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>
#include <iostream>
#include <utility>
static QString genericRoot,appRoot;
struct FakeStandardPaths{enum StandardLocation{GenericDataLocation,AppDataLocation};static QString writableLocation(StandardLocation l){return l==GenericDataLocation?genericRoot:appRoot;}};
#define QStandardPaths FakeStandardPaths
static void bridgeLog(const QString&){}
namespace {
#include "bridge_refactor_lotw_helpers.inc"
}
class DecodiumBridge:public QObject{public:
 bool m_lotwUpdating=false,m_shuttingDown=false,offline=true;QSet<QString> m_lotwUsers;int changed=0,refreshes=0;bool offlineMode()const{return offline;}void statusMessage(const QString&){}void errorMessage(const QString&){}void lotwUpdatingChanged(){}void lotwUsersChanged(){++changed;}void refreshDecodeListDxcc(){++refreshes;}void reloadLotwUsers(bool);void startLotwUsersDownload(QString const&,bool);
};
#include "bridge_refactor_lotw_load.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QCoreApplication::setOrganizationName("TestOrg");QTemporaryDir dir;genericRoot=dir.filePath("generic");appRoot=dir.filePath("app");QDir().mkpath(genericRoot);QDir().mkpath(appRoot);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};auto parsed=parseLotwUsersCsv("CALLSIGN,DATE\nk1abc,2026\nK1ABC,2025\niu8lmc,2026\n");check(parsed.users.size()==2&&parsed.users.contains("K1ABC"),"parse-dedupe");check(parseLotwUsersCsv("CALLSIGN,DATE\n").users.isEmpty(),"empty-csv");DecodiumBridge b;b.reloadLotwUsers(false);check(!b.m_lotwUpdating&&b.m_lotwUsers.isEmpty(),"offline-no-cache");QString path=sharedLotwUsersCachePath();QDir().mkpath(QFileInfo(path).absolutePath());QFile f(path);check(f.open(QIODevice::WriteOnly),"fixture");f.write("CALLSIGN,DATE\nk1abc,2026\niu8lmc,2026\n");f.close();b.reloadLotwUsers(false);QElapsedTimer t;t.start();while(b.m_lotwUpdating&&t.elapsed()<5000){app.processEvents();QThread::msleep(1);}check(!b.m_lotwUpdating&&b.m_lotwUsers.size()==2&&b.changed==1&&b.refreshes==1,"async-offline-cache");b.reloadLotwUsers(true);check(!b.m_lotwUpdating&&b.m_lotwUsers.size()==2,"forced-offline");QFile::remove(path);QFile legacy(appRoot+"/lotw_users.txt");check(legacy.open(QIODevice::WriteOnly),"legacy-fixture");legacy.write("newcall,2026\n");legacy.close();b.reloadLotwUsers(false);t.restart();while(b.m_lotwUpdating&&t.elapsed()<5000){app.processEvents();QThread::msleep(1);}check(QFile::exists(path)&&b.m_lotwUsers.contains("NEWCALL"),"migration-and-load");}
