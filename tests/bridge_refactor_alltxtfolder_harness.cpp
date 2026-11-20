#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <iostream>
class QDesktopServices {public:static QUrl last;static bool result;static bool openUrl(const QUrl&u){last=u;return result;}};QUrl QDesktopServices::last;bool QDesktopServices::result=true;
class DecodiumBridge:public QObject {public:QString selected;QString legacyAllTxtPath()const{return selected;}QString logAllTxtPath()const;bool openAllTxtFolder()const;};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QStandardPaths::setTestModeEnabled(true);QTemporaryDir temp;DecodiumBridge b;b.selected="  "+temp.filePath("ALL.TXT")+"  ";QFile f(temp.filePath("ALL.TXT"));if(!f.open(QIODevice::WriteOnly))qFatal("fixture open failed");f.write("synthetic");f.close();bool ok=b.openAllTxtFolder();std::cout<<"existing|"<<ok<<"|"<<(QDesktopServices::last.toLocalFile()==temp.path())<<"|"<<(b.logAllTxtPath()==temp.filePath("ALL.TXT"))<<'\n';b.selected=temp.filePath("missing/ALL.TXT");ok=b.openAllTxtFolder();std::cout<<"missing|"<<ok<<"|"<<(QDesktopServices::last.toLocalFile()==temp.path())<<'\n';QString unicodeDir=temp.filePath(QString::fromUtf8("prove radio/è spazio"));if(!QDir().mkpath(unicodeDir))qFatal("fixture directory failed");b.selected=QDir(unicodeDir).filePath("ALL.TXT");QFile unicodeFile(b.selected);if(!unicodeFile.open(QIODevice::WriteOnly))qFatal("unicode fixture failed");unicodeFile.close();ok=b.openAllTxtFolder();std::cout<<"unicode|"<<ok<<"|"<<(QDesktopServices::last.isLocalFile() && QDesktopServices::last.toLocalFile()==unicodeDir)<<'\n';QDesktopServices::result=false;b.selected=temp.filePath("ALL.TXT");std::cout<<"rejected|"<<b.openAllTxtFolder()<<'\n';b.selected.clear();std::cout<<"fallback|"<<b.logAllTxtPath().endsWith("/ALL.TXT")<<'\n';}
