#include <QtCore>
#include <cstdio>
static void bridgeLog(const QString&){}
class DecodiumBridge {public:
 QString m_adifLogPath,m_activeLogbookName,m_callsign="9H1SR";QStringList m_logbookProfileNames,m_logbookProfilePaths;
 QString effectiveAdifLogPath()const;QString defaultAdifLogPath()const;QString ensureAdifLogPath();QString logbookDirectoryPath()const;
 QString sanitizedLogbookFileStem(const QString&)const;void ensureLogbookProfile(const QString&,const QString&);void loadLogbookSettings();void saveLogbookSettings()const;
};
#include "bridge_refactor_logbook_settings.inc"
static void check(bool ok,const char* label){if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}puts(label);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QCoreApplication::setOrganizationName("bridge-refactor-test");QCoreApplication::setApplicationName("logbook-settings-test");QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;check(dir.isValid(),"isolated");QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());DecodiumBridge b;
 check(b.logbookDirectoryPath().endsWith("/logbooks"),"directory");check(b.sanitizedLogbookFileStem(" ..A b/?? ")=="A_b","sanitize");check(b.sanitizedLogbookFileStem("...___")=="Default","empty-stem");check(b.sanitizedLogbookFileStem(QString(60,'x')).size()==48,"stem-length");
 QString pathA=dir.filePath("a.adi"),pathB=dir.filePath("b.adi");b.ensureLogbookProfile("", " ");check(b.m_logbookProfilePaths.isEmpty(),"empty-profile");b.ensureLogbookProfile("Alpha",pathA);b.ensureLogbookProfile("Alpha",pathB);check(b.m_logbookProfileNames==QStringList({"Alpha","Alpha 2"}),"unique-names");b.ensureLogbookProfile("Renamed",pathA);check(b.m_logbookProfileNames.first()=="Renamed"&&b.m_logbookProfileNames.size()==2,"rename-existing");b.m_adifLogPath=pathB;b.m_activeLogbookName="Alpha 2";b.saveLogbookSettings();DecodiumBridge loaded;loaded.loadLogbookSettings();check(loaded.m_logbookProfilePaths==b.m_logbookProfilePaths&&loaded.m_activeLogbookName=="Alpha 2"&&loaded.effectiveAdifLogPath()==pathB,"save-reload");
 loaded.m_adifLogPath="";check(!loaded.ensureAdifLogPath().isEmpty()&&loaded.m_adifLogPath==loaded.effectiveAdifLogPath(),"ensure-default");
}
