#!/usr/bin/env python3
"""Compare real SSTV logbook commit with temporary test-only QML entry point."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];main=root/'src/bridge/DecodiumBridge.cpp';sstv=root/'src/bridge/DecodiumBridgeSstv.cpp';qso=root/'src/bridge/DecodiumBridgeSstvQso.cpp';qml=root/'build/qml/decodium/Main.qml';oldmain=main.read_text();oldsstv=sstv.read_text();oldqso=qso.read_text();oldqml=qml.read_text();fixture=(root/'tests/bridge_refactor_fixtures/sstvqso_before.inc').read_text();assert oldqso.endswith(fixture)
marker='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert oldsstv.count(marker)==1
injection='''\n#if DECODIUM_HAS_SSTV\n    { decodium::sstv::SstvQsoLogRequest request; QString error; QString id;\n      if (!decodium::sstv::SstvQsoLog::requestFromVariantMap(values, &request, &error)) return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};\n      const bool ok=commitSstvNewQso(request,&id,&error);\n      return {{QStringLiteral("ok"),ok},{QStringLiteral("error"),error},{QStringLiteral("id"),id},{QStringLiteral("path"),effectiveAdifLogPath()}}; }\n#endif\n'''
probe='''\n Timer {id:qsoTest;interval:500;running:true;repeat:true;property int ticks:0;property bool created:false;property string id:"";onTriggered:{\n if(++ticks<20)return;bridge.callsign="IU8LMC";bridge.grid="JM89AE";var r=bridge.logSstvQso({imageRecordId:"2acb07fa-f7f6-46da-9f1f-38d5ab75f3e1",createNewQso:true,existingQsoId:"",remoteCallsign:"9H1TEST/P",remoteGrid:"JM75FV",frequencyHz:14230000,timeOnUtc:"2026-08-24T12:13:14.000Z",timeOffUtc:"2026-08-24T12:15:12Z",reportSent:"59",reportReceived:"57",comments:"portable test",imageMode:"Martin M1"});\n if(r.ok){if(created){console.log('[SSTVQSO] duplicate-created');Qt.quit();return;}created=true;id=r.id;}\n else if(created && r.error.indexOf('already exists')>=0){console.log('[SSTVQSO] '+JSON.stringify({created:created,id:id,duplicate:true,path:r.path,cat:bridge.catConnected,tx:bridge.transmitting}));Qt.quit();}\n else if(r.error.indexOf('index is loading')<0 || ticks>110){console.log('[SSTVQSO] unexpected '+JSON.stringify(r));Qt.quit();}\n }}\n'''
rigs=[];results=[]
try:
 sstv.write_text(oldsstv.replace(marker,marker+injection,1))
 for name in ('before','after'):
  if name=='before':
   anchor='// === Shannon FDR';assert oldmain.count(anchor)==1;main.write_text(oldmain.replace(anchor,fixture+anchor));qso.write_text(oldqso[:-len(fixture)])
  else:main.write_text(oldmain);qso.write_text(oldqso)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=root,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(oldqml.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1))
  rig='SstvQso'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-sstvqso-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',port));socks.append(s)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(root/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','70000'];r=subprocess.run(cmd,cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=85)
   lines=[l.split('[SSTVQSO] ',1)[1] for l in r.stdout.splitlines() if '[SSTVQSO] ' in l]
   if r.returncode or len(lines)!=1 or not lines[0].startswith('{'):raise RuntimeError(r.stdout[-9000:])
   x=json.loads(lines[0]);assert x['created'] and x['duplicate'] and not x['cat'] and not x['tx'],x
   content=Path(x['path']).read_text();assert content.upper().count('9H1TEST/P')==1 and '<MODE:4>SSTV' in content.upper(),content[-3000:]
   obs={'created':x['created'],'duplicate':x['duplicate'],'id':x['id'],'cat':x['cat'],'tx':x['tx'],'recordCount':content.upper().count('9H1TEST/P')};results.append(obs);print(name,json.dumps(obs,sort_keys=True),flush=True)
 assert results[0]==results[1],results
finally:
 main.write_text(oldmain);sstv.write_text(oldsstv);qso.write_text(oldqso);qml.write_text(oldqml)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=root,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
