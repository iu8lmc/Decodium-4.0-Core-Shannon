#include <QtCore>
#include <cstdio>
#include "DecodeUserFilterConfig.h"
namespace decodium {static QString active;QString activeSettingsProfileName(){return active;}void beginActiveSettingsProfile(QSettings& s){if(!active.isEmpty())s.beginGroup(active);}}
static void beginConfiguredBridgeSettingsGroup(QSettings&s){decodium::beginActiveSettingsProfile(s);}
static QString aliasedBridgeSettingKey(QString const&k){return k=="Blacklist1"?"OldBlacklist1":k;}
class DecodiumBridge {public:QString path;bool bypass=false;bool filtersBypassed()const{return bypass;}QString legacyIniPath()const{return path;}DecodeUserFilterConfig readDecodeUserFilterConfig()const;};
#include "bridge_refactor_filtersettings.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir d;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,d.path());DecodiumBridge b;b.path=d.filePath("legacy.ini");
 {QSettings s(b.path,QSettings::IniFormat);s.setValue("MultiSettings/CurrentName","Field");s.setValue("Field/Blacklisted",true);s.setValue("Field/OldBlacklist1"," k1abc ");s.setValue("Field/Blacklist2","K1ABC");s.setValue("Field/Territory1","North-America");s.setValue("Field/Territory2","EUROPA");s.sync();}
 for(int scenario=0;scenario<4;++scenario){decodium::active=scenario==2?"Profile":"";b.bypass=scenario==3;if(scenario==2){QSettings p(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");p.beginGroup("Profile");p.setValue("Blacklist1"," 9h1sr ");p.setValue("Whitelisted",true);p.setValue("Whitelist1"," IU8LMC ");p.sync();}auto f=b.readDecodeUserFilterConfig();printf("%d|%d|%d|%s|%s|%s\n",scenario,f.blacklistEnabled,f.whitelistEnabled,f.blacklistTerms.join(',').toUtf8().constData(),f.whitelistTerms.join(',').toUtf8().constData(),f.territoryTerms.join(',').toUtf8().constData());}
}
