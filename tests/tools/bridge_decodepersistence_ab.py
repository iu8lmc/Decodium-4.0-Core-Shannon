#!/usr/bin/env python3
"""Compare persistence routing using real Qt queued delivery and fake worker."""
import os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=ROOT,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::appendDecodeMapToList(', 'void DecodiumBridge::startPersistenceWorker()')+sec(b,'void DecodiumBridge::enqueuePersistDecode(', 'void DecodiumBridge::stopPersistenceWorker()')
newfull=sec((ROOT/'src/bridge/DecodiumBridgeDecodePersistence.cpp').read_text(),'void DecodiumBridge::appendDecodeMapToList(')
for x,y in [('decodiumBridgeIsStreamingKeyboardMode(', 'isStreamingKeyboardMode('),('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey('),('decodiumBridgeUnresolvedGridOnlyDecode(', 'messageIsUnresolvedGridOnlyDecode('),('decodiumBridgeFst4wUnresolvedHashDecode(', 'isFst4wUnresolvedHashDecode('),('decodiumBridgeDecodePersistenceLog(', 'bridgeLog(')]:newfull=newfull.replace(x,y)
new=sec(newfull,'void DecodiumBridge::appendDecodeMapToList(', 'void DecodiumBridge::startPersistenceWorker()')+sec(newfull,'void DecodiumBridge::enqueuePersistDecode(', 'void DecodiumBridge::stopPersistenceWorker()')
if old!=new:raise RuntimeError('persistence routing source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout);moc=run('qtpaths','--query','QT_INSTALL_LIBEXECS',stdout=subprocess.PIPE).stdout.strip()+'/moc'
with tempfile.TemporaryDirectory(prefix='decodium-persist-ab-') as t:
 d=Path(t);h=(ROOT/'tests/bridge_refactor_persist_harness.cpp').read_text();out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);(d/'test.cpp').write_text(h);run(moc,str(d/'test.cpp'),'-o',str(d/'persist_harness.moc'))
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('persistence outputs differ')
 print(out[1],end='')
