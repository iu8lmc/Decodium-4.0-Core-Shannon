#!/usr/bin/env python3
"""Compare CALL3 loading, locator caching and geographical fallback."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
b=b.replace(sec(b,'static QStringList worldMapCall3CandidatePaths()', 'static WorldMapCall3LoadResult loadWorldMapCall3CacheFromFiles('),'QStringList worldMapCall3CandidatePaths();\n\n')
old=sec(b,'static QString maidenheadFromLonLat(','static int decodeSnrOrDefault(')+sec(b,'struct WorldMapCall3LoadResult','/* step A3:')+sec(b,'void DecodiumBridge::rememberWorldMapGrid(','void DecodiumBridge::markWorldMapQsoClosed(')
new=sec((R/'src/bridge/DecodiumBridgeWorldMapGrid.cpp').read_text(),'static QString maidenheadFromLonLat(')
for x,y in [('decodiumBridgeWorldMapCallKey(','worldMapCallKey('),('decodiumBridgeNormalizeWorldMapCall(','normalizeWorldMapCall('),('decodiumBridgeAudioLevelLog(','bridgeLog(')]:new=new.replace(x,y)
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-worldmapgrid-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_worldmapgrid_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 if results[0].splitlines()!=['parse|2|FN42AA|JN70','geo|JM75|JM75','async|1|2|FN31AB|JN70|1','approx|JM75|1']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
