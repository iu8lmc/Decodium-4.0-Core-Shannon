#include <QGuiApplication>
#include <QScreen>
#include <QTextStream>
#include <QUrl>
namespace DecodiumLogging { QString diagnosticLogPath(){return "/tmp/test diagnostic.log";} }
struct DesktopMock { static QList<QUrl> opened; static bool openUrl(const QUrl& u){opened.append(u);return true;} };
QList<QUrl> DesktopMock::opened;
#define QDesktopServices DesktopMock
#define FORK_RELEASE_VERSION "1.0.737"
class DecodiumBridge { public: QString diagnosticLogPath() const; void openDiagnosticLog() const; void openHamlibUpdatePage() const; QVariantMap primaryScreenAvailableGeometry() const; QString version() const; };
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QGuiApplication app(argc,argv);DecodiumBridge b;auto g=b.primaryScreenAvailableGeometry();b.openDiagnosticLog();b.openHamlibUpdatePage();QTextStream o(stdout);o<<"log|"<<(b.diagnosticLogPath()=="/tmp/test diagnostic.log")<<'|'<<(DesktopMock::opened[0].toLocalFile()==b.diagnosticLogPath())<<'\n';o<<"link|"<<(DesktopMock::opened[1].toString()=="https://github.com/Hamlib/Hamlib/releases")<<'\n';o<<"screen|"<<(g.value("width").toInt()>0)<<'|'<<(g.value("height").toInt()>0)<<'|'<<g.contains("name")<<'\n';o<<"version|"<<b.version()<<'\n';}
