#!/usr/bin/env python3
"""Compare actual ADIF import/export on an isolated synthetic logbook.

Temporarily instruments the selected bundle's Main.qml; avoid concurrent use of
that bundle. Rebuild its QML sync target after an unclean exit.
"""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
 p=argparse.ArgumentParser();p.add_argument('--variant',choices=('a','b'),default='a');p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
 if original.count(marker)!=1:raise RuntimeError('unexpected Main.qml')
 rig='BridgeAdif'+secrets.token_hex(5)
 try:
  with tempfile.TemporaryDirectory(prefix='decodium-adif-roundtrip-') as tmp:
   source=Path(tmp)/'fixture.adi';out=Path(tmp)/'export.adi'
   call='IU8LMC' if a.variant=='a' else 'AB1CD'
   comment='ALPHA' if a.variant=='a' else 'città'
   source.write_text(f'Synthetic ADIF\n<EOH>\n<CALL:{len(call)}>{call} <COMMENT:{len(comment.encode("utf-8"))}>{comment} <QSO_DATE:8>20261107 <TIME_ON:6>120000 <BAND:3>20M <MODE:3>FT8 <FREQ:6>14.074 <EOR>\n')
   probe='''\n    Timer { interval: 12000; running: true; repeat: true; property int step: 0; property int ticks: 0; property string logPath: ""; onTriggered: {
      var failed=[];
      if (step===0) {
        var name="Synthetic__RIG__";
        if (!bridge.createLogbook(name,false)) failed.push("create");
        var list=bridge.logbookProfiles();for(var i=0;i<list.length;++i) if(list[i].name===name) logPath=list[i].path;
        if(logPath.indexOf("/.qttest/")<0 || logPath.indexOf("__RIG__")<0) failed.push("unsafe-path");
        if(bridge.importFromAdif(__MISSING__)!==0 || bridge.adifImportInProgress || bridge.adifImportProgress!==0 || bridge.adifImportStatus.indexOf("non trovato")<0) failed.push("missing-file");
        if(!failed.length && !bridge.importFromAdif(__SOURCE__)) failed.push("import-start");
        if(failed.length) { console.log("[ADIF-ROUNDTRIP] FAIL " + failed.join(",")); stop(); return; }
        step=1;interval=250;return;
      }
      if (bridge.adifImportInProgress) { if(++ticks<48)return;failed.push("timeout"); }
      if (bridge.adifImportProgress!==100 || bridge.qsoCount!==1) failed.push("import-result:"+bridge.adifImportStatus);
      if (!failed.length && step===1) {
        if (!bridge.importFromAdif(__SOURCE__)) failed.push("duplicate-start");
        else { step=2;ticks=0;return; }
      }
      if (!failed.length && !bridge.exportAdif(__OUTPUT__)) failed.push("export");
      if (!bridge.deleteLogbook(logPath,true)) failed.push("cleanup");
      console.log("[ADIF-ROUNDTRIP] " + (failed.length ? "FAIL " + failed.join(",") : "PASS"));stop();
    } }\n'''.replace('__RIG__',rig).replace('__SOURCE__',json.dumps(str(source))).replace('__OUTPUT__',json.dumps(str(out))).replace('__MISSING__',json.dumps(str(Path(tmp)/'missing.adi')))
   qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
   cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','30000']
   result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=110)
   if result.returncode or '[ADIF-ROUNDTRIP] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
    print(result.stdout[-16000:]);raise RuntimeError(f'ADIF probe failed (exit {result.returncode})')
   data=out.read_bytes().upper()
   if data.count(b'<EOR>') != 1:raise RuntimeError('duplicate import changed record count')
   for token in (call.encode(),comment.encode('utf-8').upper(),b'20261107',b'120000',b'14.074',b'<EOR>'):
    if token not in data:raise RuntimeError(f'missing exported field {token!r}')
   print(f'{a.variant}: missing-file rejection, compatibility import and progress reached 100; synthetic ADIF imported twice and exported with one matching QSO; CAT disabled')
 finally:
  qml.write_text(original)
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   path=parent/('Decodium - '+rig+' - test')
   if path.is_dir():shutil.rmtree(path)
if __name__=='__main__':main()
