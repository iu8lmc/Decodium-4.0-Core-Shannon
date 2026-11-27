#!/usr/bin/env python3
"""Compare QSO journal selection and actual isolated writes."""
from pathlib import Path
import os,shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2];old=(root/'tests/bridge_refactor_fixtures/qso_journal_before.inc').read_text();new=(root/'src/bridge/DecodiumBridgeQsoJournal.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>
#include <QUuid>
#include "helpers.inc"
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);QCoreApplication::setOrganizationName("DecodiumJournalTest");QCoreApplication::setApplicationName(QUuid::createUuid().toString(QUuid::WithoutBraces));QStandardPaths::setTestModeEnabled(true);
 QString root=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);bool safe=root.contains(".qttest")&&root.contains("DecodiumJournalTest");QDir d(root+"/qso_journal");
 bool select=qsoJournalWanted("autoSeq hello")&&qsoJournalWanted("QSO complet")&&!qsoJournalWanted("unrelated audio status");QDate day=QDateTime::currentDateTimeUtc().date();
 QString expired=d.filePath("sequencer_"+day.addDays(-31).toString("yyyy-MM-dd")+".log");QString boundary=d.filePath("sequencer_"+day.addDays(-30).toString("yyyy-MM-dd")+".log");QString invalid=d.filePath("sequencer_not-a-date.log");
 bool prepared=safe&&QDir().mkpath(d.path());for(const QString& name:{expired,boundary,invalid}){QFile f(name);prepared=prepared&&f.open(QIODevice::WriteOnly);f.close();}
 if(prepared)qsoJournal("synthetic\nmessage");if(qEnvironmentVariable("DECODIUM_QSO_JOURNAL")=="0"){bool disabled=QFile::exists(expired)&&!QFile::exists(d.filePath("sequencer_"+day.toString("yyyy-MM-dd")+".log"));bool removed=safe&&QDir(root).removeRecursively();QTextStream(stdout)<<"disabled="<<disabled<<" removed="<<removed<<Qt::endl;return disabled&&removed?0:1;}QString today=d.filePath("sequencer_"+day.toString("yyyy-MM-dd")+".log");QFile f(today);bool written=f.open(QIODevice::ReadOnly)&&f.readAll().contains("synthetic message\n");f.close();
 bool retained=!QFile::exists(expired)&&QFile::exists(boundary)&&QFile::exists(invalid);bool removed=safe&&QDir(root).removeRecursively();QTextStream(stdout)<<"select="<<select<<" written="<<written<<" retention="<<retained<<" removed="<<removed<<Qt::endl;return select&&written&&retained&&removed?0:1;}

'''
with tempfile.TemporaryDirectory(prefix='decodium-journal-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);out=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label;subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True);v=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,v.stdout.strip(),flush=True);out.append(v.stdout);env=os.environ.copy();env["DECODIUM_QSO_JOURNAL"]="0";disabled=subprocess.run([str(exe)],capture_output=True,text=True,check=True,env=env);print(label,disabled.stdout.strip(),flush=True);out.append(disabled.stdout)
 assert out[0]==out[2] and out[1]==out[3]
