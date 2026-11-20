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
   call='K9'+secrets.token_hex(3).upper()
   comment='ALPHA' if a.variant=='a' else 'città'
   source.write_text(f'Synthetic ADIF\n<EOH>\n<CALL:{len(call)}>{call} <COMMENT:{len(comment.encode("utf-8"))}>{comment} <QSO_DATE:8>20261107 <TIME_ON:6>120000 <BAND:3>20M <MODE:3>FT8 <FREQ:6>14.074 <EOR>\n')
   probe='''\n    Timer { interval:12000; running:true; repeat:true; property int step:0; property int ticks:0; property string logPath:""; property string when:""; onTriggered: {
      var f=[];
      if(step===0) { var name="Synthetic__RIG__"; if(!bridge.createLogbook(name,false)) f.push("create"); var list=bridge.logbookProfiles(); for(var i=0;i<list.length;i++) if(list[i].name===name) logPath=list[i].path; if(logPath.indexOf("/.qttest/")<0 || logPath.indexOf("__RIG__")<0) f.push("unsafe-path"); if(!f.length && !bridge.importAdif(__SOURCE__)) f.push("import"); if(f.length) {console.log("[ADIF-ROUNDTRIP] FAIL "+f.join(","));stop();return;} bridge.warmLogCacheAsync();step=1;interval=250;return; }
      if(bridge.adifImportInProgress) {if(++ticks<48)return;f.push("timeout");}
      if(step===1 && !f.length) {var rows=bridge.searchQsos("","","","","");if(!rows.length && ++ticks<48)return;if(rows.length!==1)f.push("search");else {if(bridge.qsoCount!==1)f.push("count-before");if(bridge.workedCallsigns().indexOf(__CALL__)<0)f.push("worked-call");if(bridge.getQsoStats().totalQsos!==1)f.push("stats");if(bridge.searchQsos(__CALL__,"20m","ft8","2026-11-07","2026-11-07").length!==1)f.push("filters");if(bridge.searchQsos(__CALL__,"40m","","","").length!==0)f.push("filter-miss");when=rows[0].dateTime;if(bridge.exportSelectedToAdif(logPath,rows)) f.push("overwrite"); var bad=[Object.assign({},rows[0],{exportFingerprint:"invalid"})];if(bridge.exportSelectedToAdif(__OUTPUT__,bad))f.push("stale");if(!bridge.exportSelectedToAdif(__OUTPUT__,rows))f.push("selected-export");if(!bridge.editQso(rows[0].call,when,{comment:"EDITED",mode:"FT4"}))f.push("edit");}step=2;ticks=0;if(!f.length)return;}
      if(step===2 && !f.length) {var changed=bridge.searchQsos("","","","","");if(!changed.length && ++ticks<48)return;if(changed.length!==1 || (changed[0].comment!=="EDITED" || changed[0].mode!=="FT4"))f.push("edited-record");if(!bridge.deleteQso(__CALL__,when))f.push("delete");if(bridge.deleteQso(__CALL__,when))f.push("delete-again");if(bridge.qsoCount!==0)f.push("count");}
      if(!bridge.deleteLogbook(logPath,true))f.push("cleanup");console.log("[ADIF-ROUNDTRIP] "+(f.length?"FAIL "+f.join(","):"PASS"));stop();
    } }\n'''.replace('__RIG__',rig).replace('__SOURCE__',json.dumps(str(source))).replace('__OUTPUT__',json.dumps(str(out))).replace('__CALL__',json.dumps(call))
   qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','30000']
   result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)']+cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=110)
   if result.returncode or '[ADIF-ROUNDTRIP] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
    print(result.stdout[-16000:]);raise RuntimeError(f'ADIF probe failed (exit {result.returncode})')
   data=out.read_bytes().upper()
   if data.count(b'<EOR>') != 1:raise RuntimeError('duplicate import changed record count')
   for token in (call.encode(),comment.encode('utf-8').upper(),b'20261107',b'120000',b'14.074',b'<EOR>'):
    if token not in data:raise RuntimeError(f'missing exported field {token!r}')
   print(f'{a.variant}: count, search filters, statistics, warmup, edit and deletion passed on synthetic QSO; CAT disabled')
 finally:
  qml.write_text(original)
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   path=parent/('Decodium - '+rig+' - test')
   if path.is_dir():shutil.rmtree(path)
if __name__=='__main__':main()
