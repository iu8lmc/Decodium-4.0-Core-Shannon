#!/usr/bin/env python3
"""Compare ALL.TXT seed output and timing using an isolated synthetic log."""
import argparse, datetime, json, os, random, shlex, subprocess, tempfile
from pathlib import Path

root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--before',type=Path,required=True);p.add_argument('--after',type=Path,default=root/'src/bridge/DecodiumBridgeKnownCq.cpp');p.add_argument('--output',type=Path,required=True);p.add_argument('--other-mode-percent',type=int,default=9);a=p.parse_args()
if not 0<=a.other_mode_percent<=100:raise ValueError('invalid other-mode percentage')
env=os.environ.copy();env['QT_HASH_SEED']='0'
def run(args, **kwargs):return subprocess.run(args,env=env,check=True,text=True,**kwargs)
flags=shlex.split(run(['pkg-config','--cflags','--libs','Qt6Core'],stdout=subprocess.PIPE).stdout)
preamble=(root/'tests/bridge_refactor_knowncq_harness.cpp').read_text().split('int main(',1)[0]
main='''
#include <QElapsedTimer>
#include <QCryptographicHash>
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv);DecodiumBridge bridge;bridge.path=QString::fromLocal8Bit(argv[1]);
 if (QString::fromLocal8Bit(argv[2]) == "enabled") qputenv("DECODIUM_FT8_SEED_KNOWN_CQ_REPLAY", "1");
 else qunsetenv("DECODIUM_FT8_SEED_KNOWN_CQ_REPLAY");
 QElapsedTimer timer;timer.start();bridge.seedFt8KnownCqCacheFromAllTxt();qint64 elapsed=timer.nsecsElapsed()/1000;
 auto digest=[](QStringList const& items){return QCryptographicHash::hash(items.join("\\n").toUtf8(),QCryptographicHash::Sha256).toHex();};
 std::cout << "RESULT " << elapsed << " " << hashes.size() << " " << digest(hashes).constData() << " " << known.size() << " " << digest(known).constData() << "\\n";
}
'''
def extracted(path):
 s=path.read_text();i=s.index('void DecodiumBridge::moveLegacyAllTxtCursorToEnd()');return s[i:].replace('decodiumBridgeKnownCqLog(', 'bridgeLog(')
with tempfile.TemporaryDirectory(prefix='decodium-knowncq-perf-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text(preamble+main)
 executables=[]
 for name,source in [('before',a.before),('after',a.after)]:
  (d/'bridge_refactor_under_test.inc').write_text(extracted(source));exe=d/name
  run(shlex.split(os.environ.get('CXX','c++'))+['-O2','-std=c++17',str(d/'test.cpp'),*flags,'-o',str(exe)],stdout=subprocess.PIPE,stderr=subprocess.PIPE);executables.append(exe)
 now=datetime.datetime.now(datetime.timezone.utc).strftime('%y%m%d_%H%M%S')
 paths=[d/'ALL.TXT',d/'JTDX-ALL.TXT']
 with paths[0].open('w') as f:
  for i in range(160000):
   if i%100<a.other_mode_percent:
    other_call='K1FT8' if i%20==0 else 'K1ABC'
    f.write(f'{now} 14.074 Rx FT4 -08 0.1 1200 CQ {other_call} FN42\n')
   elif i%7==0:
    f.write(f'{now} 14.074 Rx FT8 -08 0.1 1200 <K1ABC> K2DEF RR73\n')
   else:
    f.write(f'{now} 14.074 Rx FT8 -10 0.2 {400+i%2400} CQ K{i%10}AB{i%10000:04d} FN42\n')
 paths[1].write_bytes(paths[0].read_bytes())
 edge=d/'EDGE-ALL.TXT';rng=random.Random(312901)
 examples=['FN42','RR73','R-12','+05','K1ABC','k1abc/p','<K2DEF>','A1','A123456789012','A1234567890123','ſ1abc','ß1abc','É1ABC','A١BC','...','K1ABC;','<;K1ABC>,','QRZ','CQ','TU','AA00','RR99','SS99','A/1','1/2','\u00a0K1ABC\u00a0','ﬀ1abc','ǰ1abc','\u03451abc']
 alphabet='ABCDEFRSTXYZ0123456789/+-._<>;,éßſ١'
 examples += [''.join(rng.choice(alphabet) for _ in range(rng.randrange(1,16))) for _ in range(3000)]
 with edge.open('w',encoding='utf-8') as f:
  for i,token in enumerate(examples):
   mode=['fT8','FT4','FT8'][i%3]
   f.write(f'{now} 14.074 Rx {mode} -10 0.2 1500 CQ {token}\n')
 report=[]
 for file,mode in [(paths[0],'disabled'),(paths[1],'disabled'),(paths[1],'enabled'),(paths[0],'enabled'),(edge,'disabled')]:
  values={'file':file.name,'replay':mode,'bytes':file.stat().st_size,'otherModePercent':a.other_mode_percent}
  for idx in (0,1,1,0):
   proc=run([str(executables[idx]),str(file),mode],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
   lines=[line.split() for line in proc.stdout.splitlines() if line.startswith('RESULT ')]
   if len(lines)!=1:raise RuntimeError('missing result '+proc.stdout)
   _,elapsed,count,hashsum,knowncount,knownsum=lines[0];key=('before','after')[idx]
   values.setdefault(key,[]).append({'elapsedUs':int(elapsed),'hashCount':int(count),'hashDigest':hashsum,'knownCount':int(knowncount),'knownDigest':knownsum})
  correctness=lambda r:(r['hashCount'],r['hashDigest'],r['knownCount'],r['knownDigest'])
  if len({correctness(r) for k in ('before','after') for r in values[k]})!=1:raise RuntimeError('seed output differs: '+repr(values))
  report.append(values)
 a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
