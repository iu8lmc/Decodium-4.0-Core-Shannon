#include <QCoreApplication>
#include <QDebug>
#include <QUrl>
#include <iostream>
#ifdef Q_OS_LINUX
#undef Q_OS_LINUX
#endif
struct QDesktopServices {static bool result;static int opens;static QUrl last;static bool openUrl(const QUrl&u){last=u;++opens;return result;}};
bool QDesktopServices::result=true;int QDesktopServices::opens=0;QUrl QDesktopServices::last;
class DecodiumBridge:public QObject{public:int errors=0,status=0;void errorMessage(const QString&){++errors;}void statusMessage(const QString&){++status;}bool openExternalUrl(const QString&);};
#include "bridge_refactor_external_url.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};DecodiumBridge b;check(!b.openExternalUrl("  ")&&b.errors==1,"empty");check(!b.openExternalUrl("javascript://example.com/x")&&QDesktopServices::opens==0,"scheme-block");check(b.openExternalUrl(" https://example.invalid/test ")&&QDesktopServices::last.scheme()=="https","https");check(b.openExternalUrl("mailto:user@example.invalid")&&QDesktopServices::last.scheme()=="mailto","mailto");check(b.openExternalUrl("/tmp/synthetic-image.png")&&QDesktopServices::last.isLocalFile(),"local-file");QDesktopServices::result=false;check(!b.openExternalUrl("https://example.invalid/fail")&&b.errors==3,"opener-failure");check(b.status==3,"status-count");}
