#include <QtCore>
#include <cstdio>
#include "AdifDocumentInternal.h"
#include "AdifExportSanitizer.h"
#ifdef ADIF_BASELINE
#include "bridge_refactor_adif_document.inc"
#else
using namespace decodium::adif_document;
#endif
static void check(bool ok,const char* label){if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}puts(label);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir d;check(d.isValid(),"isolated");
 auto missing=loadAdifDocument(d.filePath("missing.adi"));check(!missing.loaded&&missing.records.isEmpty()&&missing.header.contains("<EOH>"),"missing");
 QByteArray input("header <eoh>\n<call:6>iu8lmc <comment:5>hello <eor>\n<CALL:5>AB1CD<EOR>");auto records=parseAdifRecordsBytes(input);check(records.size()==2&&records[0].fields.value("CALL")=="iu8lmc"&&records[1].fields.value("CALL")=="AB1CD","two-records");
 auto truncated=parseAdifRecordsBytes("<CALL:5>AB1CD <COMMENT:100>short");check(truncated.size()==1&&truncated[0].fields.value("CALL")=="AB1CD"&&!truncated[0].fields.contains("COMMENT"),"truncated-field");
 QByteArray wide="<CALL:5>AB1CD <COMMENT:65537>"+QByteArray(65537,'x')+"<MODE:3>FT8<EOR>";auto large=parseAdifRecordsBytes(wide);check(large.size()==1&&!large[0].fields.contains("COMMENT")&&large[0].fields.value("MODE")=="FT8","oversized-skipped");
 QByteArray many;for(int i=0;i<130;++i)many+="<F"+QByteArray::number(i)+":1>x";many+="<EOR>";auto capped=parseAdifRecordsBytes(many);check(capped.size()==1&&capped[0].fields.size()==128,"field-cap");
 ParsedAdifDocument doc;doc.header="Test <EOH>\n";doc.loaded=true;ParsedAdifRecord r;r.fields.insert("CALL","IU8LMC");r.fields.insert("COMMENT",QString::fromUtf8("città"));r.fields.insert("MY_IOTA","NONE");doc.records.append(r);QString path=d.filePath("nested/out.adi");check(writeAdifDocument(path,doc),"write");auto loaded=loadAdifDocument(path);check(loaded.loaded&&loaded.records.size()==1&&loaded.records[0].fields.value("COMMENT")==QString::fromUtf8("città")&&!loaded.records[0].fields.contains("MY_IOTA"),"read-back-utf8-and-sanitize");
 QFile bom(d.filePath("bom.adi"));check(bom.open(QIODevice::WriteOnly),"bom-file");bom.write("\xEF\xBB\xBF<EOH><CALL:5>AB1CD<EOR>");bom.close();auto fromBom=loadAdifDocument(bom.fileName());check(fromBom.loaded&&fromBom.records.size()==1&&fromBom.records[0].fields.value("CALL")=="AB1CD","bom");
}
