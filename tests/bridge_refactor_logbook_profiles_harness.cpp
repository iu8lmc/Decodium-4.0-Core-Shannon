#include <QtCore>
#include <cstdio>
#include "AdifDocumentInternal.h"
static bool writeAdifDocument(const QString& path,const ParsedAdifDocument& doc){QFile f(path);if(!f.open(QIODevice::WriteOnly))return false;return f.write(doc.header.toUtf8())>=0;}
static int countAdifRecordsLightweight(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll().count("<EOR>"):0;}
class DecodiumBridge {public:
 QStringList m_logbookProfileNames,m_logbookProfilePaths;QString m_adifLogPath,m_activeLogbookName,m_callsign="9H1SR",dir;int saved=0,reloads=0,errors=0,notices=0;
 QString effectiveAdifLogPath()const{return m_adifLogPath;}QString ensureAdifLogPath(){return m_adifLogPath;}QString logbookDirectoryPath()const{return dir;}
 QString sanitizedLogbookFileStem(const QString&n)const{QString x=n;x.replace(' ','_');return x;}
 void ensureLogbookProfile(const QString&n,const QString&p){int i=m_logbookProfilePaths.indexOf(p);if(i<0){m_logbookProfileNames<<n;m_logbookProfilePaths<<p;}else m_logbookProfileNames[i]=n;}
 void saveLogbookSettings(){++saved;}void reloadActiveLogbookState(const QString&){++reloads;}
 void statusMessage(const QString&){++notices;}void errorMessage(const QString&){++errors;}void activeLogbookChanged(){++notices;}
 QVariantList logbookProfiles()const;bool backupActiveLogbook();bool createLogbook(const QString&,bool);bool addLogbook(const QString&,const QString&);bool switchLogbook(const QString&);bool deleteLogbook(const QString&,bool);
};
#include "bridge_refactor_logbook_profiles.inc"
static void check(bool ok,const char* label){if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}puts(label);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir tmp;check(tmp.isValid(),"isolated");DecodiumBridge b;b.dir=tmp.path();
 check(b.backupActiveLogbook(),"backup-missing");check(b.createLogbook(" Alpha ",false),"create");QString first=b.m_adifLogPath;check(QFileInfo::exists(first)&&b.m_activeLogbookName=="Alpha"&&b.logbookProfiles().size()==1,"created-state");
 check(b.backupActiveLogbook(),"backup-existing");check(b.createLogbook("Alpha",false)&&b.m_adifLogPath!=first&&b.logbookProfiles().size()==2,"create-collision");QString second=b.m_adifLogPath;
 check(b.switchLogbook(QUrl::fromLocalFile(first).toString())&&b.m_adifLogPath==first,"switch-url");QString third=tmp.filePath("third.adi");check(b.addLogbook(third," third ")&&b.m_adifLogPath==third&&QFileInfo::exists(third),"add-missing");
 check(!b.addLogbook(" ",""),"add-empty");check(!b.deleteLogbook("",true)&&!b.deleteLogbook(tmp.filePath("unknown"),true),"invalid-delete");
 check(b.deleteLogbook(second,true)&&!QFileInfo::exists(second)&&b.m_adifLogPath==third,"delete-inactive");check(b.deleteLogbook(third,false)&&QFileInfo::exists(third)&&b.m_adifLogPath==first,"remove-active-keep-file");
 check(b.deleteLogbook(first,true)&&!QFileInfo::exists(first)&&b.m_logbookProfilePaths.size()==1&&QFileInfo::exists(b.m_adifLogPath)&&b.m_adifLogPath!=first,"delete-last-replacement");
 check(b.saved>=5&&b.reloads>=4,"save-and-reload");
}
