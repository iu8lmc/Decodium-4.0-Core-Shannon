#!/usr/bin/env python3
"""Compare actual ordered asynchronous writes and settings thread pool configuration."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/async_logs_before.inc').read_text()
new=(root/'src/bridge/DecodiumBridgeAsyncLogs.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QThreadPool>
#include <QTemporaryDir>
#include <QString>
#include <utility>
#include "helpers.inc"
static QByteArray readFile(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;QString a=dir.path()+"/one/a.txt", b=dir.path()+"/two/b.txt", same=dir.path()+"/alias/file.txt";appendDecodeTextBatchesAsync(a,"A",b,"B");appendDecodeTextBatchesAsync(a,"C",b,"D");appendDecodeTextBatchesAsync(same,"X",same,"Y");appendDecodeTextBatchesAsync(same,"Z",same,"");appendDecodeTextBatchesAsync("","", "","");bool done=decodeTextLogThreadPool().waitForDone(5000);bool separate=readFile(a)=="AC"&&readFile(b)=="BD";bool alias=readFile(same)=="XYZ";bool pool=settingsWriteThreadPool().maxThreadCount()==1&&decodeTextLogThreadPool().maxThreadCount()==1;QTextStream(stdout)<<"done="<<done<<" separate="<<separate<<" alias="<<alias<<" pool="<<pool<<Qt::endl;return done&&separate&&alias&&pool?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-async-logs-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outputs=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outputs.append(result.stdout)
 assert outputs[0]==outputs[1]
