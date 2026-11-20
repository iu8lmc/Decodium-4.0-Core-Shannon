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
   probe='''\n    Timer { interval:12000; running:true; repeat:true; property int step:0; property int ticks:0; property string logPath:""; property string when:""; onTriggered: {
      var f=[];
      if(step===0) { var name="Synthetic__RIG__"; if(!bridge.createLogbook(name,false)) f.push("create"); var list=bridge.logbookProfiles(); for(var i=0;i<list.length;i++) if(list[i].name===name) logPath=list[i].path; if(logPath.indexOf("/.qttest/")<0 || logPath.indexOf("__RIG__")<0) f.push("unsafe-path"); if(!f.length && !bridge.importAdif(__SOURCE__)) f.push("import"); if(f.length) {console.log("[ADIF-ROUNDTRIP] FAIL "+f.join(","));stop();return;} step=1;interval=250;return; }
      if(bridge.adifImportInProgress) {if(++ticks<48)return;f.push("timeout");}
      if(step===1 && !f.length) {var rows=bridge.searchQsos("","","","","");if(!rows.length && ++ticks<48)return;if(rows.length!==1)f.push("search");else {when=rows[0].dateTime;if(bridge.exportSelectedToAdif(logPath,rows)) f.push("overwrite"); var bad=[Object.assign({},rows[0],{exportFingerprint:"invalid"})];if(bridge.exportSelectedToAdif(__OUTPUT__,bad))f.push("stale");if(bridge.exportToAdif(""))f.push("empty-full-export");if(bridge.exportToAdif(__BAD_OUTPUT__))f.push("missing-parent-export");if(bridge.exportToAdif(__DIRECTORY__))f.push("directory-export");if(!bridge.exportToAdif(__OUTPUT__))f.push("full-export");if(!bridge.editQso(rows[0].call,when,{comment:"EDITED"}))f.push("edit");}step=2;ticks=0;if(!f.length)return;}
      if(step===2 && !f.length) {var changed=bridge.searchQsos("","","","","");if(!changed.length && ++ticks<48)return;if(changed.length!==1 || changed[0].comment!=="EDITED")f.push("edited-record");if(!bridge.deleteQso(__CALL__,when))f.push("delete");if(bridge.deleteQso(__CALL__,when))f.push("delete-again");if(bridge.qsoCount!==0)f.push("count");}
      if(!bridge.deleteLogbook(logPath,true))f.push("cleanup");console.log("[ADIF-ROUNDTRIP] "+(f.length?"FAIL "+f.join(","):"PASS"));stop();
    } }\n'''.replace('__RIG__',rig).replace('__SOURCE__',json.dumps(str(source))).replace('__OUTPUT__',json.dumps(str(out))).replace('__BAD_OUTPUT__',json.dumps(str(Path(tmp)/'missing-parent'/'export.adi'))).replace('__DIRECTORY__',json.dumps(tmp)).replace('__CALL__',json.dumps(call))
   qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
   cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','30000']
   result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=110)
   if result.returncode or '[ADIF-ROUNDTRIP] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
    print(result.stdout[-16000:]);raise RuntimeError(f'ADIF probe failed (exit {result.returncode})')
   data=out.read_bytes().upper()
   if data.count(b'<EOR>') != 1:raise RuntimeError('duplicate import changed record count')
   for token in (call.encode(),comment.encode('utf-8').upper(),b'20261107',b'120000',b'14.074',b'<EOR>'):
    if token not in data:raise RuntimeError(f'missing exported field {token!r}')
   print(f'{a.variant}: full export, empty/missing-parent/directory guards, edit and deletion passed on synthetic QSO; CAT disabled')
 finally:
  qml.write_text(original)
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   path=parent/('Decodium - '+rig+' - test')
   if path.is_dir():shutil.rmtree(path)
if __name__=='__main__':main()
