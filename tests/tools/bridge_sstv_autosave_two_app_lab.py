#!/usr/bin/env python3
"""Prove SSTV automatic image save via two isolated BlackHole sessions.

Temporarily instruments both sessions' Main.qml. No CAT or RF is used. The
image comparison reports its actual errors with permissive limits because this
probe tests storage, not image fidelity; normal SSTV lab defaults are unchanged.
"""
import argparse,os,re,sqlite3,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
 p=argparse.ArgumentParser();p.add_argument('pattern',choices=('a','b'));p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
 if original.count(marker)!=1:raise RuntimeError('unexpected Main.qml')
 probe='''\n    Timer { interval: 500; running: true; repeat: true; property bool armed:false; onTriggered: {
        if (!armed && bridge.sstvStorageReady) { bridge.sstvRxAutoSaveEnabled=true; armed=bridge.sstvRxAutoSaveEnabled; }
        if (armed && bridge.sstvRxSaveState==="saved") { console.log("[SSTV-AUTOSAVE-LAB] saved"); stop(); }
    } }\n'''
 try:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_AVT_BINARY=str(a.binary.resolve()), DECODIUM_LAB_AVT_TX_MS='20000', DECODIUM_LAB_AVT_EXTRA_MS='25000', DECODIUM_LAB_AVT_PLATFORM='cocoa', DECODIUM_LAB_AVT_NO_CAPTURE='1', DECODIUM_LAB_VERIFY_SSTV_AUTOSAVE='1', DECODIUM_LAB_AVT_MAX_MAE='255', DECODIUM_LAB_AVT_MAX_P95='255', DECODIUM_DISABLE_QML_CACHE='1')
  result=subprocess.run([sys.executable,str(ROOT/'tests/sstv/tools/run_avt_two_app_lab.py'),'robot-c12',a.pattern,'pattern'],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=140)
  print(result.stdout,end='')
  if result.returncode:raise RuntimeError(f'autosave lab failed (exit {result.returncode})')
  match=re.search(r'artifacts (\S+)',result.stdout)
  if not match:raise RuntimeError('missing lab artifacts path')
  rxlog=(Path(match.group(1))/'rx.log').read_text(errors='replace')
  identity=re.search(r'Application identity: (Decodium - AVTrx[0-9a-f]+ - test)',rxlog)
  if not identity:raise RuntimeError('missing unique RX test identity')
  storage=Path.home()/'.qttest/Library/Application Support/IU8LMC'/identity.group(1)/'sstv'
  database=storage/'sstv-index.sqlite3'
  if not database.is_file():raise RuntimeError('SSTV gallery database missing')
  with sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True) as db:
   count=db.execute("select count(*) from sstv_images where category=1 and mode='robot-c12' and complete=1").fetchone()[0]
  images=list((storage/'images/received').rglob('*.png'))
  if count<1 or not images:raise RuntimeError('SSTV complete received Robot C12 image or record missing')
  print(f'gallery verification: {count} record(s), {len(images)} received PNG(s)')
 finally:qml.write_text(original)
if __name__=='__main__':main()
