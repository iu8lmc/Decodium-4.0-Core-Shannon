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
#include "bridge_refactor_lotw_mock_network.h"
#define QNetworkAccessManager LotwMockNetwork
#define QNetworkReply LotwMockReply
static void bridgeLog(const QString&){}
namespace {
#include "bridge_refactor_lotw_helpers.inc"
}
class DecodiumBridge:public QObject{public:
 bool m_lotwUpdating=false,m_shuttingDown=false,offline=true;QSet<QString> m_lotwUsers;int changed=0,refreshes=0,errors=0;bool offlineMode()const{return offline;}void statusMessage(const QString&){}void errorMessage(const QString&){++errors;}void lotwUpdatingChanged(){}void lotwUsersChanged(){++changed;}void refreshDecodeListDxcc(){++refreshes;}void reloadLotwUsers(bool);void startLotwUsersDownload(QString const&,bool);
};
#include "bridge_refactor_lotw_load.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QCoreApplication::setOrganizationName("TestOrg");QTemporaryDir dir;genericRoot=dir.filePath("generic");appRoot=dir.filePath("app");QDir().mkpath(genericRoot);QDir().mkpath(appRoot);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};DecodiumBridge b;b.offline=false;QString path=sharedLotwUsersCachePath();lotwMockFail=false;lotwMockData="CALLSIGN,DATE\nk1abc,2026\niu8lmc,2026\n";b.m_lotwUpdating=true;b.startLotwUsersDownload(path,true);QElapsedTimer t;t.start();while(b.m_lotwUpdating&&t.elapsed()<5000){app.processEvents();QThread::msleep(1);}check(!b.m_lotwUpdating&&b.m_lotwUsers.size()==2&&QFile::exists(path)&&b.changed==1,"network-success");lotwMockFail=true;b.m_lotwUpdating=true;b.startLotwUsersDownload(path,true);t.restart();while(b.m_lotwUpdating&&t.elapsed()<5000){app.processEvents();QThread::msleep(1);}check(!b.m_lotwUpdating&&b.m_lotwUsers.size()==2&&b.errors==0,"network-error-retains-cache");b.m_lotwUsers.clear();b.m_lotwUpdating=true;b.startLotwUsersDownload(path,true);t.restart();while(b.m_lotwUpdating&&t.elapsed()<5000){app.processEvents();QThread::msleep(1);}check(!b.m_lotwUpdating&&b.errors==1,"network-error-report");lotwMockFail=false;lotwMockData="CALLSIGN,DATE\n";b.m_lotwUpdating=true;b.startLotwUsersDownload(path,true);t.restart();while(b.m_lotwUpdating&&t.elapsed()<5000){app.processEvents();QThread::msleep(1);}check(!b.m_lotwUpdating&&b.m_lotwUsers.isEmpty(),"network-empty-csv");}
