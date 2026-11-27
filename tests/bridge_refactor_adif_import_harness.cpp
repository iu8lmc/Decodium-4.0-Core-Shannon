#include <QtCore>
#include <cstdio>
#include "AdifDocumentInternal.h"
using decodium::adif_document::loadAdifDocument;
using decodium::adif_document::writeAdifDocument;
using decodium::adif_document::adifRecordDedupeKey;
using decodium::adif_document::normalizeImportedAdifRecord;
using decodium::adif_document::adifFrequencyHzFromFields;
using decodium::adif_document::normalizeAdifModeForDisplay;
#ifdef ADIF_BASELINE
#include "bridge_refactor_adif_import.inc"
#else
using decodium::adif_document::importAdifDocumentInBackground;
using decodium::adif_document::importConfirmedAdifDocumentInBackground;
#endif
static void check(bool ok,const char* label){if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}puts(label);}
static void fixture(const QString& path,const QByteArray& data){QFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size())qFatal("fixture failed");}
static QByteArray bytes(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir d;check(d.isValid(),"isolated");
 const QByteArray first="<CALL:5>AB1CD <QSO_DATE:8>20261107 <TIME_ON:6>120000 <BAND:3>20M <MODE:3>FT8 <EOR>\n";
 const QByteArray second="<CALL:6>IU8LMC <QSO_DATE:8>20261107 <TIME_ON:6>130000 <BAND:3>20M <MODE:3>FT8 <EOR>\n";
 const QByteArray header="fixture <EOH>\n";QString src=d.filePath("source.adi"),dst=d.filePath("destination.adi");fixture(src,header+first+second);fixture(dst,header+first);QList<int> progress;
 auto result=importAdifDocumentInBackground(src,dst,[&](int p,const QString&){progress<<p;});QString backup=result.value("backupPath").toString();check(result.value("ok").toBool()&&result.value("imported")==1&&result.value("skipped")==1&&result.value("total")==2&&progress.last()==100,"merge");check(!backup.isEmpty()&&bytes(backup)==header+first&&loadAdifDocument(dst).records.size()==2,"backup-original");
 result=importAdifDocumentInBackground(src,dst,{});check(result.value("ok").toBool()&&result.value("imported")==0&&result.value("skipped")==2&&result.value("backupPath").toString().isEmpty(),"duplicate-no-backup");
 check(!importAdifDocumentInBackground(dst,dst,{}).value("ok").toBool(),"same-path");check(!importAdifDocumentInBackground(d.filePath("missing"),dst,{}).value("ok").toBool(),"missing-source");QString empty=d.filePath("empty.adi");fixture(empty,header);check(!importAdifDocumentInBackground(empty,dst,{}).value("ok").toBool(),"empty-source");
 QString changingSource=d.filePath("changing-source.adi"),safeDest=d.filePath("safe-dest.adi");fixture(changingSource,header+second);fixture(safeDest,header+first);bool altered=false;result=importAdifDocumentInBackground(changingSource,safeDest,[&](int p,const QString&){if(p>=20&&!altered){QFile f(changingSource);f.open(QIODevice::Append);f.write("x");altered=true;}});check(!result.value("ok").toBool()&&bytes(safeDest)==header+first,"source-race");
 fixture(changingSource,header+second);fixture(safeDest,header+first);altered=false;result=importAdifDocumentInBackground(changingSource,safeDest,[&](int p,const QString&){if(p>=35&&!altered){QFile f(safeDest);f.open(QIODevice::Append);f.write("x");altered=true;}});check(!result.value("ok").toBool()&&bytes(safeDest)==header+first+"x","destination-race");
 for(const QString& provider:{QStringLiteral("lotw_confirmed"),QStringLiteral("qrz_confirmed"),QStringLiteral("eqsl")}){QString target=d.filePath(provider+".adi");fixture(target,header+first);result=importConfirmedAdifDocumentInBackground(src,target,provider);auto data=loadAdifDocument(target);QString flag=provider=="lotw_confirmed"?"LOTW_QSL_RCVD":(provider=="qrz_confirmed"?"QSL_RCVD":"EQSL_QSL_RCVD");check(result.value("ok").toBool()&&result.value("imported")==1&&result.value("updated")==1&&data.records.size()==2&&data.records[0].fields.value(flag)=="Y","confirmed-provider");}
}
