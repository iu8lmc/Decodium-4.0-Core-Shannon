#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <iostream>
static QString localRoot,globalRoot;
struct FakeStandardPaths{enum StandardLocation {AppLocalDataLocation,AppDataLocation};static QString writableLocation(StandardLocation l){return l==AppLocalDataLocation?localRoot:globalRoot;}};
#define QStandardPaths FakeStandardPaths
class DecodiumBridge{public:QStringList satelliteOptions()const;QStringList satModeOptions()const;};
#include "bridge_refactor_satellite_options.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;localRoot=dir.filePath("local");globalRoot=dir.filePath("global");QDir().mkpath(localRoot);QDir().mkpath(globalRoot);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};QFile local(localRoot+"/sat.dat");check(local.open(QIODevice::WriteOnly),"fixture");local.write("AO91|Fox-1B\nAO91|duplicate\nX1|\n|bad\nmalformed\n\n");local.close();QFile global(globalRoot+"/sat.dat");check(global.open(QIODevice::WriteOnly),"fallback-fixture");global.write("FO29|Fuji OSCAR 29\n");global.close();DecodiumBridge b;check(b.satelliteOptions()==QStringList({"","AO91 - Fox-1B","X1 - X1"}),"parse-dedupe-first");check(local.open(QIODevice::WriteOnly),"oversize-open");check(local.resize(4*1024*1024+1),"oversize-fixture");local.close();check(b.satelliteOptions()==QStringList({"","FO29 - Fuji OSCAR 29"}),"oversize-fallback");auto modes=b.satModeOptions();check(modes.size()==20&&modes.first().isEmpty()&&modes.contains("UV")&&modes.contains("VU"),"modes");}
