#!/usr/bin/env python3
"""Compare startup migration using isolated real INI settings."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2];old=(root/'tests/bridge_refactor_fixtures/cat_migration_startup_before.inc').read_text();new=(root/'src/bridge/DecodiumBridgeCatMigrationStartup.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#include <QDebug>
#include "src/radio/DecodiumProfileSettings.h"
#include "src/radio/CatNativeMigration.h"
#include "helpers.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());{QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");if(!s.fileName().startsWith(dir.path()+"/"))return 2;s.clear();s.setValue("catBackend","native");s.setValue("CAT_Native/serialPort","TEST_PORT");s.sync();}migrateRemovedNativeCatBackend();QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");s.sync();bool migrated=s.value("catBackend").toString()=="hamlib"&&s.value("Transceiver/serialPort").toString()=="TEST_PORT";s.setValue("Transceiver/serialPort","UPDATED");s.sync();migrateRemovedNativeCatBackend();s.sync();bool stable=s.value("Transceiver/serialPort").toString()=="UPDATED";QTextStream(stdout)<<"migrated="<<migrated<<" stable="<<stable<<Qt::endl;return migrated&&stable?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-cat-migrate-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);out=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label;subprocess.run(['c++','-std=c++17','-I'+str(d),'-I'+str(root),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True);v=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,v.stdout.strip(),flush=True);out.append(v.stdout)
 assert out[0]==out[1]
