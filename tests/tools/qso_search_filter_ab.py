#!/usr/bin/env python3
"""Compare original and current QSO filtering on the same synthetic cache."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
path = 'src/bridge/DecodiumBridgeQsoCache.cpp'
original = subprocess.check_output(['git','show','HEAD:'+path],cwd=root,text=True)
current = (root/path).read_text()
prefix = (root/'tests/bridge_refactor_qso_cache_harness.cpp').read_text().split('int main(',1)[0]
prefix = prefix.replace('#include <iostream>', '#include <iostream>\n#include <QCryptographicHash>\n#include <QJsonDocument>')
main = r'''
int main(int argc,char**argv) {
 QCoreApplication app(argc,argv); QTemporaryDir dir; DecodiumBridge b;
 b.path=dir.filePath("log.adi"); QFile file(b.path); if(!file.open(QIODevice::WriteOnly))return 2; file.write("x");file.close();
 QFileInfo info(b.path); b.m_qsoSearchCacheReady=true; b.m_qsoSearchCachePath=b.path;
 b.m_qsoSearchCacheModified=info.lastModified(); b.m_qsoSearchCacheSize=info.size();
 QStringList calls={"k1abc"," IU8LMC ","Å1xyz","", "W9zzz"};
 QStringList grids={"FN42"," jn70 ","", "AA00"};
 QStringList bands={"20m","40M","", " 15m "};
 QStringList modes={"FT8"," ft4 ","FT2",""};
 QStringList dates={"2026-11-07 12:00","2025-01-02 08:30","", "bad-date"};
 for(int i=0;i<6000;++i) b.m_qsoSearchCacheRows.append(QVariantMap{
  {"id",i},{"call",calls[i%5]},{"grid",grids[(i/5)%4]}, {"band",bands[(i/20)%4]},
  {"mode",modes[(i/80)%4]},{"dateTime",dates[(i/320)%4]},
  {"comment",i%3==0?"  Alpha beta ":"other"}});
 QStringList needles={"","abc","ß","alpha","20m","2026","fn42","none"};
 QStringList bandFilters={"","20m","40M"," 15m "};
 QStringList modeFilters={"","ft8","FT4","FT2"};
 QStringList froms={"","2026-01-01","abc","20/25/01/02"};
 QStringList tos={"","2026-11-07","@@","2025.12.31"};
 QCryptographicHash hash(QCryptographicHash::Sha256); int cases=0; qint64 total=0;
 QElapsedTimer timer; timer.start();
 for(const auto& needle:needles) for(const auto& band:bandFilters)
  for(const auto& mode:modeFilters) for(int d=0;d<4;++d) {
   QVariantList rows=b.searchQsos(needle,band,mode,froms[d],tos[(d+cases)%4]);
   hash.addData(QJsonDocument::fromVariant(rows).toJson(QJsonDocument::Compact));
   hash.addData(QByteArray::number(rows.size())); total+=rows.size(); ++cases;
  }
 std::cout << cases << " " << total << " " << hash.result().toHex().constData() << "\n";
 std::cerr << "elapsed_ms=" << timer.elapsed() << "\n";
}
'''
with tempfile.TemporaryDirectory(prefix='decodium-qso-filter-') as tmp:
    tmpdir=Path(tmp)
    harness=tmpdir/'harness.cpp';harness.write_text(prefix+main)
    env=os.environ.copy()
    env['PKG_CONFIG_PATH']='/home/the-matrix/.local/opt/decodium-qt/6.11.0/gcc_64/lib/pkgconfig'
    flags=subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],env=env,text=True).split()
    outputs=[]
    for name, source in [('old',original),('current',current)]:
        body=source[source.index('int         DecodiumBridge::qsoCount()'):]
        body=body.replace('decodiumBridgeCountAdifRecords(', 'countAdifRecordsLightweight(').replace('decodiumBridgeLogbookLog(', 'bridgeLog(')
        (tmpdir/'bridge_refactor_qso_cache.inc').write_text(body)
        exe=tmpdir/name
        subprocess.run(['g++','-O2','-std=c++17','-I'+str(tmpdir),'-I'+str(root/'src/bridge'),
                        str(harness),*flags,'-Wl,-rpath,/home/the-matrix/.local/opt/decodium-qt/6.11.0/gcc_64/lib','-o',str(exe)],check=True)
    for name in ('old','current','current','old'):
        result=subprocess.run([str(tmpdir/name)],text=True,capture_output=True,check=True)
        outputs.append(result.stdout)
        print(name,result.stdout.strip(),result.stderr.strip(),flush=True)
    if len(set(outputs))!=1: raise RuntimeError('filtered output differs')
