#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a):i=s.index(a);j=s.index('\n}',i);return s[i:j+2]+'\n'
s=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout;t=(R/'src/bridge/DecodeSignalReport.cpp').read_text();names=['static int decodeSnrOrDefault(', 'static QString formatSignalReport(', 'static QString normalizeSignalReportText('];a='\n'.join(sec(s,n) for n in names);b='\n'.join(sec(t,n.replace('static ','')) for n in names).replace('int decodeSnrOrDefault(QString const& text, int fallback)', 'static int decodeSnrOrDefault(QString const& text, int fallback = -10)').replace('QString formatSignalReport(', 'static QString formatSignalReport(').replace('QString normalizeSignalReportText(', 'static QString normalizeSignalReportText(')
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-signalreport-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_signalreport_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 (d/'bridge_refactor_under_test.inc').write_text('#include "DecodeSignalReport.h"\n');run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),str(R/'src/bridge/DecodeSignalReport.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if len(set(out))!=1 or out[0].splitlines()!=['report|-08|-08','report|R+09|+09','report|+99|+49','report|-99|-50','report|bad|-10','report||','snr|42|-50|-99','format|+00|+49|-50']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
