#!/usr/bin/env python3
"""Exercise the Bridge's local web server with a temporary test profile."""
import argparse,json,os,secrets,socket,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args()
qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
# Reserve two neighbouring ports briefly; the server and its websocket use both.
for attempt in range(100):
 port=22000+secrets.randbelow(30000)
 try:
  with socket.socket() as x, socket.socket() as y:
   x.bind(('127.0.0.1',port));y.bind(('127.0.0.1',port+1))
  break
 except OSError: pass
else:raise RuntimeError('no free local port pair')
probe='''
 property int probeUnauth: -1
 property int probeAuth: -1
 function testWebRequest(path, done) {
  var xhr = new XMLHttpRequest(); xhr.onreadystatechange=function(){if(xhr.readyState===XMLHttpRequest.DONE)done(xhr.status)};
  xhr.open("GET", "http://127.0.0.1:__PORT__"+path);xhr.send();
 }
 Timer {interval:10000;running:true;repeat:false;onTriggered: {
  var started=bridge.startWebServer(__PORT__);var running=bridge.webServerRunning();
  var url=bridge.webServerUrl();var qr=bridge.webServerQrUrl();var query=url.substring(url.indexOf("?"));
  testWebRequest("/api/state",function(status){probeUnauth=status;testWebRequest("/api/state"+query,function(status2){
   probeAuth=status2;bridge.stopWebServer();var stopped=!bridge.webServerRunning();var restarted=bridge.startWebServer(__PORT__);var tokenReused=bridge.webServerUrl().substring(bridge.webServerUrl().indexOf("?"))===query;bridge.stopWebServer();console.log("[WEB-PROBE] "+JSON.stringify({started:started,running:running,url:url.indexOf("?token=")>=0,qr:qr.indexOf("/qr?token=")>=0,unauth:probeUnauth,auth:probeAuth,stopped:stopped,restarted:restarted,tokenReused:tokenReused,stoppedAgain:!bridge.webServerRunning()}));
  });});
 }}
'''.replace('__PORT__',str(port))
try:
 with tempfile.TemporaryDirectory(prefix='decodium-web-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','WebProbe'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  values=[json.loads(line.split('[WEB-PROBE] ',1)[1]) for line in result.stdout.splitlines() if '[WEB-PROBE] ' in line]
  expected={'started':True,'running':True,'url':True,'qr':True,'unauth':401,'auth':200,'stopped':True,'restarted':True,'tokenReused':True,'stoppedAgain':True}
  if result.returncode or values!=[expected] or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-12000:]);raise RuntimeError('web server probe failed')
  print(json.dumps(values[0],sort_keys=True))
finally:qml.write_text(original)
