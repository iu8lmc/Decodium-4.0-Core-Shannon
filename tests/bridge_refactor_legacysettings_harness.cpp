#include <QCoreApplication>
#include <QDir>
#include <QSet>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <iostream>
static QString fakeRoot,profileName;
struct FakeStandardPaths {enum StandardLocation {ConfigLocation};static QString writableLocation(StandardLocation){return fakeRoot;}};
#define QStandardPaths FakeStandardPaths
namespace decodium {QString activeSettingsProfileName(){return profileName;}void beginActiveSettingsProfile(QSettings&s){if(!profileName.isEmpty())s.beginGroup(profileName);}}
static void bridgeLog(const QString&){}
static QString aliasedBridgeSettingKey(const QString& key){return key=="ShowGreyline"?QStringLiteral("MapShowGreyline"):key;}
class DecodiumBridge {public:QString legacyIniPath()const;QString legacyConfigGroupName()const;bool isLegacySyncKey(const QString&)const;void syncSettingToLegacyIni(const QString&,const QVariant&);QVariant readSettingFromLegacyIni(const QString&)const;};
#include "bridge_refactor_legacysettings.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;fakeRoot=dir.path();QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};DecodiumBridge b;check(b.legacyIniPath()==dir.filePath("decodium4.ini")&&b.legacyConfigGroupName()=="Default","default-path-group");check(b.isLegacySyncKey("UDPServer")&&!b.isLegacySyncKey("DefinitelyNotASetting"),"key-membership");{QSettings ini(b.legacyIniPath(),QSettings::IniFormat);ini.setValue("MultiSettings/CurrentName","RigA");ini.setValue("RigA/foo","group");ini.setValue("Configuration/bar","configuration");ini.setValue("Default/baz","default");ini.sync();}check(b.legacyConfigGroupName()=="RigA","active-group");check(b.readSettingFromLegacyIni("foo").toString()=="group"&&b.readSettingFromLegacyIni("bar").toString()=="configuration"&&b.readSettingFromLegacyIni("baz").toString()=="default","group-fallback");b.syncSettingToLegacyIni("foo","root");{QSettings ini(b.legacyIniPath(),QSettings::IniFormat);check(ini.value("foo").toString()=="root"&&ini.value("RigA/foo").toString()=="root"&&ini.value("Configuration/foo").toString()=="root"&&ini.value("Default/foo").toString()=="root","write-groups");}profileName="Isolated";{QSettings profile(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(profile);profile.setValue("foo","profile");profile.setValue("MapShowGreyline",true);profile.sync();}check(b.readSettingFromLegacyIni("foo").toString()=="profile"&&b.readSettingFromLegacyIni("ShowGreyline").toBool(),"profile-and-alias");check(!b.readSettingFromLegacyIni("missing").isValid(),"missing");}
