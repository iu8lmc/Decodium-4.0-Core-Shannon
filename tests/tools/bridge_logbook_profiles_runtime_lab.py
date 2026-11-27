#!/usr/bin/env python3
"""Compare logbook profile operations and restart in unique Qt test storage.

Temporarily instruments Main.qml; do not run another app from the same bundle.
"""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
 p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
 if original.count(marker)!=1:raise RuntimeError('unexpected Main.qml')
 rig='BridgeLogbook'+secrets.token_hex(5)
 try:
  with tempfile.TemporaryDirectory(prefix='decodium-logbook-runtime-') as tmp:
   for phase in ('write','restart'):
    create='if (!bridge.createLogbook(alpha,false) || !bridge.createLogbook(beta,false)) failed.push("create");' if phase=='write' else ''
    action='''
          if (!bridge.switchLogbook(pa) || !bridge.backupActiveLogbook()) failed.push("switch-backup");
          if (!bridge.deleteLogbook(pb,true)) failed.push("delete-inactive");
          if (!bridge.deleteLogbook(pa,true)) failed.push("delete-active");
          var remain=bridge.logbookProfiles();for(var j=0;j<remain.length;++j) if(remain[j].path===pa || remain[j].path===pb) failed.push("removed");
''' if phase=='restart' else 'if (!bridge.exportAdif(__EXPORT__)) failed.push("export");'
    probe='''\n    Timer { interval: 12000; running: true; repeat: false; onTriggered: {
        var failed=[], alpha="TestAlpha__RIG__", beta="TestBeta__RIG__";
        __CREATE__
        var list=bridge.logbookProfiles(), pa="", pb="";
        for(var i=0;i<list.length;++i) {if(list[i].name===alpha) pa=list[i].path; if(list[i].name===beta) pb=list[i].path;}
        // Only touch the two fresh profiles inside this unique Qt test-mode directory.
        if (pa.indexOf("/.qttest/")<0 || pb.indexOf("/.qttest/")<0 || pa.indexOf("__RIG__")<0 || pb.indexOf("__RIG__")<0 || pa===pb) failed.push("unsafe-or-missing-path");
        else { __ACTION__ }
        console.log("[LOGBOOK-RUNTIME-__PHASE__] " + (failed.length ? "FAIL " + failed.join(",") : "PASS"));
    } }\n'''.replace('__RIG__',rig).replace('__CREATE__',create).replace('__ACTION__',action).replace('__PHASE__',phase).replace('__EXPORT__',json.dumps(str(Path(tmp)/'export.adi')))
    qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software',DECODIUM_FORCE_STANDALONE_UDP='1')
    cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','27000']
    result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)']+cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=110)
    if result.returncode or f'[LOGBOOK-RUNTIME-{phase}] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
     print(result.stdout[-16000:]);raise RuntimeError(f'{phase}: logbook probe failed (exit {result.returncode})')
    if phase=='write' and b'<EOH>' not in (Path(tmp)/'export.adi').read_bytes().upper(): raise RuntimeError('export content is not ADIF')
    print(f'{phase}: isolated logbook profiles and file operations passed; CAT disabled',flush=True)
 finally:
  qml.write_text(original)
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   path=parent/('Decodium - '+rig+' - test')
   if path.is_dir():shutil.rmtree(path)
if __name__=='__main__':main()
