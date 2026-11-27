#include <QtCore>
#include <cstdio>
#include "AdifDocumentInternal.h"
#ifdef ADIF_BASELINE
#include "bridge_refactor_adif_normalization.inc"
#else
using namespace decodium::adif_document;
#endif
static void check(bool ok,const char* label){if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}puts(label);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);
 check(adifDigitsOnly("2x0-26")=="2026"&&normalizedAdifDateToken("2026-11-07")=="20261107"&&normalizedAdifDateToken("2026").isEmpty(),"dates");
 check(normalizedAdifTimeToken("12:34")=="123400"&&normalizedAdifTimeToken("").isEmpty(),"time");
 QMap<QString,QString> f{{"CALL"," iu8lmc "},{"QSO_DATE","2026-11-07"},{"TIME_ON","12:34"},{"MODE","mfsk"},{"SUBMODE","ft8"},{"FREQ","14,074"},{"GRIDSQUARE","jm75"}};
 check(normalizeAdifModeForDisplay(f)=="FT8"&&normalizedAdifDateTime(f)=="2026-11-07 12:34:00","mode-and-datetime");
 check(adifFrequencyHzFromFields(f)==14074000&&adifBandFromFields(f)=="20M"&&freqHzToBandToken(14074000)=="20m","frequency-band");
 ParsedAdifRecord r;r.fields=f;auto normalized=normalizeImportedAdifRecord(r);check(normalized.fields.value("CALL")=="IU8LMC"&&normalized.fields.value("GRIDSQUARE")=="JM75"&&normalized.fields.value("TIME_ON")=="123400"&&normalized.fields.value("BAND")=="20M","normalize-record");
 check(adifRecordDedupeKey(f)==adifRecordDedupeKey(normalized.fields)&&!adifRecordDedupeKey(f).isEmpty(),"dedupe-normalized");f["CALL"]=" ";check(adifRecordDedupeKey(f).isEmpty(),"empty-call");f["CALL"]="AB1CD";f["QSO_DATE"]="";check(adifRecordDedupeKey(f).contains("|RAW|"),"raw-fallback");
 f["FREQ"]="bad";f["FREQ_RX"]="14.074";check(adifFrequencyHzFromFields(f)==0,"invalid-primary-not-fallback");f.remove("FREQ");check(adifFrequencyHzFromFields(f)==14074000,"rx-frequency-fallback");
}
