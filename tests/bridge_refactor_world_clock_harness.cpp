#include <QtCore>
#include <algorithm>
#include <cmath>
#include <utility>
#include <cstdio>
static QTimeZone decodiumUtcTimeZone(){return QTimeZone(QByteArrayLiteral("UTC"));}
class DecodiumBridge { public:
 QVariantMap worldClockSnapshot(const QString&) const;
 QVariantList worldClockCityOptions(const QString&,int) const;
};
#include "bridge_refactor_under_test.inc"
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);} puts(name);}
static bool containsZone(const QVariantList& list,const QString& zone){for(auto const& v:list) if(v.toMap().value("zoneId").toString()==zone) return true;return false;}
int main(){
 DecodiumBridge b; auto u=b.worldClockSnapshot("UTC");
 auto now=QDateTime::currentDateTimeUtc();
 check(u.value("timeZoneId")=="UTC" && u.value("offsetSeconds").toInt()==0,"utc-zone");
 check(u.value("hours").toInt()==now.time().hour() && u.value("date").toString()==now.date().toString("dd/MM/yyyy"),"utc-clock");
 auto invalid=b.worldClockSnapshot("Invalid/NotAZone"); check(invalid.value("timeZoneId")=="UTC" && invalid.value("offsetSeconds").toInt()==0,"invalid-falls-back-utc");
 auto blank=b.worldClockSnapshot("   ");check(blank.value("timeZoneId")=="UTC","blank-falls-back-utc");
 QTimeZone rome("Europe/Rome");if(rome.isValid()) {auto r=b.worldClockSnapshot(" Europe/Rome ");check(r.value("timeZoneId")=="Europe/Rome" && r.value("offsetSeconds").toInt()==rome.offsetFromUtc(now),"rome-offset");}
 auto defaults=b.worldClockCityOptions("",200); check(!defaults.isEmpty() && defaults.first().toMap().value("zoneId")=="UTC","default-list");
 check(b.worldClockCityOptions("",0).size()==1 && b.worldClockCityOptions("",1).size()==1,"minimum-limit");
 check(b.worldClockCityOptions("zzzz_unlikely_city_zzzz",20).isEmpty(),"missing-city");
 if(rome.isValid()){auto it=b.worldClockCityOptions(" roma ",20);check(containsZone(it,"Europe/Rome"),"rome-italian-alias");auto en=b.worldClockCityOptions("RoMe",20);check(containsZone(en,"Europe/Rome"),"rome-english-alias-case");}
 if(QTimeZone("Europe/London").isValid())check(containsZone(b.worldClockCityOptions("Londra",20),"Europe/London"),"london-alias");
 auto limited=b.worldClockCityOptions("America",3);check(limited.size()<=3,"maximum-requested");
 QSet<QString> seen; for(auto const& v:defaults){auto z=v.toMap().value("zoneId").toString();check(!z.isEmpty() && !seen.contains(z),"unique-default-zone");seen.insert(z);}
}
