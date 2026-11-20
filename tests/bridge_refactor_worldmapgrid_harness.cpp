#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QRunnable>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThreadPool>
#include <QTimer>
#include <cmath>
#include <utility>
static QStringList candidates;static QString worldMapCallKey(QString s){return s.trimmed().toUpper().section('/',0,0);}static QString normalizeWorldMapCall(QString s){return s.trimmed().toUpper();}static bool isGridTokenStrict(const QString&s){return s.size()>=4&&s.at(0).isLetter()&&s.at(1).isLetter()&&s.at(2).isDigit()&&s.at(3).isDigit();}static void bridgeLog(const QString&){}namespace Radio{QString base_callsign(QString s){return s.section('/',0,0);}}
struct DxccEntity{double lat=0,lon=0;bool valid=false;bool isValid()const{return valid;}};class Lookup{public:bool loaded=true;bool isLoaded(){return loaded;}DxccEntity lookup(const QString&s){return {35.0,-14.0,s=="K1ABC"};}};
class DecodiumBridge:public QObject{public:bool m_worldMapCall3Loaded=false,m_worldMapCall3Loading=false;int m_worldMapCall3RecordCount=0,events=0;QHash<QString,QString>m_worldMapGridByCall;Lookup*m_dxccLookup=nullptr;void localDataStateChanged(){++events;}void loadDxccLookupAsync(){}void rememberWorldMapGrid(const QString&,const QString&);void loadWorldMapCall3Cache();QString lookupWorldMapGrid(const QString&);QString approximateWorldMapGridForCall(const QString&);};
#include "bridge_refactor_under_test.inc"
QStringList worldMapCall3CandidatePaths(){return candidates;}
static void wait(int ms){QEventLoop e;QTimer::singleShot(ms,&e,&QEventLoop::quit);e.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);QTemporaryDir dir;QString file=dir.filePath("CALL3.TXT");QFile f(file);f.open(QIODevice::WriteOnly);f.write("# comment\nK1ABC,FN42aa\nK2ABC,JN70\nbad,??\n");f.close();candidates={file,file};auto parsed=loadWorldMapCall3CacheFromFiles(candidates);o<<"parse|"<<parsed.loaded<<'|'<<parsed.grids.value("K1ABC")<<'|'<<parsed.grids.value("K2ABC")<<'\n';o<<"geo|"<<maidenheadFromLonLat(14,35)<<'|'<<maidenheadFromLonLat(374,35)<<'\n';DecodiumBridge b;Lookup lookup;b.m_dxccLookup=&lookup;b.rememberWorldMapGrid("K1ABC","FN31ab");b.loadWorldMapCall3Cache();wait(150);o<<"async|"<<b.m_worldMapCall3Loaded<<'|'<<b.m_worldMapCall3RecordCount<<'|'<<b.lookupWorldMapGrid("K1ABC")<<'|'<<b.lookupWorldMapGrid("K2ABC")<<'|'<<b.events<<'\n';o<<"approx|"<<b.approximateWorldMapGridForCall("K1ABC")<<'|'<<b.approximateWorldMapGridForCall("UNKNOWN").isEmpty()<<'\n';}
