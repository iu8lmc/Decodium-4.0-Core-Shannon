#!/usr/bin/env python3
"""A/B startup ordering and legacy failure/shutdown using real Qt timers."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*args,**kw):return subprocess.run(args,check=True,text=True,**kw)
baseline=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
current=(R/'src/bridge/DecodiumBridge.cpp').read_text()
def extract(s,old):
 marker='// Su Mac, crea il legacy backend' if old else '// The timer starts during construction'
 a=s.index('QTimer::singleShot(3000, this, [this]() {',s.index(marker)) if old else s.index('const auto initializeLegacy =',s.index(marker))
 b=s.index('\n        });',a)+len('\n        });')
 return s[a:b].replace('singleShot(3000,','singleShot(30,').replace('singleShot(500,','singleShot(5,')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout);libexec=run('pkg-config','--variable=libexecdir','Qt6Core',stdout=subprocess.PIPE).stdout.strip()
with tempfile.TemporaryDirectory(prefix='decodium-startup-order-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_startup_order_harness.cpp').read_text());run(str(Path(libexec)/'moc'),str(d/'test.cpp'),'-o',str(d/'moc_test.cpp'));results=[]
 for code in (extract(baseline,True),extract(current,False)):
  (d/'bridge_startup_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout.splitlines())
 expected=[['before-ready|1','after-ready|1','ready|1','fallback|1|0|1','shutdown|1'],['before-ready|0','after-ready|1','ready|1','fallback|1|0|1','shutdown|0']]
 if results!=expected:raise RuntimeError(str(results))
 print('baseline:',', '.join(results[0]));print('current:',', '.join(results[1]))
