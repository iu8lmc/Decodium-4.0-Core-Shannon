#include <QtCore>
#include <cstdio>
static QTimeZone decodiumUtcTimeZone(){return QTimeZone::utc();}
static QString bandFromFreqHz(double f){return f>14000000&&f<14350000?"20m":"";}
class DecodiumBridge {public:
 QString m_callsign="9H1SR",m_grid="JM75",adif,alltxt,imported;int notices=0,errors=0;
 QString effectiveAdifLogPath()const{return adif;}QString logAllTxtPath()const{return alltxt;}
 void statusMessage(const QString&){++notices;}void errorMessage(const QString&){++errors;}
 bool importFromAdifAsync(const QString& f){imported=f;return !f.isEmpty();}
 bool exportAdif(const QString&);bool importAdif(const QString&);
};
#include "bridge_refactor_adif_export.inc"
static void check(bool ok,const char* label){if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}puts(label);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir d;check(d.isValid(),"isolated");DecodiumBridge b;b.adif=d.filePath("in.adi");b.alltxt=d.filePath("all.txt");QString out=d.filePath("out.adi");
 check(!b.exportAdif(""),"empty-path");QFile src(b.adif);check(src.open(QIODevice::WriteOnly),"open-source");src.write("<EOH>\n<CALL:5>AB1CD <EOR>\n");src.close();check(b.exportAdif(b.adif),"same-path");check(b.exportAdif(out),"copy");QFile copied(out);check(copied.open(QIODevice::ReadOnly)&&copied.readAll().contains("AB1CD"),"copy-content");copied.close();check(QFile::remove(b.adif),"remove-fixture");
 QFile txt(b.alltxt);check(txt.open(QIODevice::WriteOnly),"open-text");txt.write("20261107_120000 TX IU8LMC JM75 14.074MHz FT8\n");txt.close();check(b.exportAdif(out),"fallback-export");QFile converted(out);check(converted.open(QIODevice::ReadOnly),"open-export");QByteArray data=converted.readAll();check(data.contains("<CALL:6>IU8LMC")&&data.contains("<BAND:3>20m")&&data.contains("<MODE:3>FT8")&&data.contains("<EOR>"),"fallback-content");check(b.importAdif("test.adi")&&b.imported=="test.adi","import-delegation");
}
