#include <QtCore>
#include <cmath>
#include <limits>
#include <cstdio>
#include "Radio.hpp"
#include "models/Bands.hpp"
#include "AdifExportSanitizer.h"
#include "bridge_refactor_frequency.inc"
Radio::Frequency decodiumBridgeParseFrequencyHzValue(const QVariant& v,bool* ok){return parseFrequencyHzValue(v,ok);}
#ifdef BRIDGE_REFERENCE
#include "bridge_refactor_adif_record.inc"
#else
QString bridgeAdifRecordText(const QString&,const QString&,double,const QString&,const QDateTime&,const QDateTime&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,int,int);
#endif
static void check(bool v,const char*s){if(!v){fprintf(stderr,"FAIL %s\n",s);exit(1);}puts(s);}
int main(){QDateTime on(QDate(2026,11,7),QTime(12,30,0),Qt::UTC),off=on.addSecs(60);
 QString a=bridgeAdifRecordText("K1ABC","FN42",14074000,"FT4",on,off,"-10","-12","9H1SR","JM75","città","SAT","FO-29","U/V","145.9 MHz","Mario","Roma",15,28);
 check(a.contains("<CALL:5>K1ABC") && a.contains("<MODE:4>MFSK") && a.contains("<SUBMODE:3>FT4") && a.contains("<BAND:3>20M"),"basic");
 check(a.contains("<COMMENT:6>città") && a.contains("<sat_name:5>FO-29") && a.contains("<freq_rx:10>145.900000") && a.contains("<band_rx:2>2M"),"utf8-satellite");
 check(a.contains("<CQZ:2>15")&&a.contains("<ITUZ:2>28")&&a.contains("<TIME_OFF:6>123100"),"metadata");
 QString b=bridgeAdifRecordText("AB1CD","",0,"SSB",on,on.addSecs(-60),"","","9H1SR","","","","","","bad","","",0,0);
 check(b.contains("<MODE:3>SSB")&&b.contains("<BAND:3>OOB")&&b.contains("<RST_SENT:3>-10")&&!b.contains("freq_rx")&&b.contains("<TIME_OFF:6>123000"),"fallbacks");
}
