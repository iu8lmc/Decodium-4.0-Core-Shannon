#!/usr/bin/env python3
"""A/B actual app OTP callbacks against an isolated loopback HTTP server."""
import http.server,json,os,re,secrets,shutil,socket,subprocess,tempfile,threading
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';mod=r/'src/bridge/DecodiumBridgeFoxVerifier.cpp';qml=r/'build/qml/decodium/Main.qml';m,n,q=(p.read_text() for p in (main,mod,qml)); fixture=(r/'tests/bridge_refactor_fixtures/verify_otp_before.inc').read_text();header='#include "DecodiumBridge.h"\n#include "Network/FoxVerifier.hpp"\n#include <QDateTime>\n\n';assert n==header+fixture
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  self.server.paths.append(self.path);good=self.path.endswith('/good.text');self.send_response(200 if good else 404);self.end_headers();self.wfile.write(b'accepted' if good else b'failed')
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);server.paths=[];thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start();port=server.server_port
modified=fixture.replace('FOXVERIFIER_DEFAULT_BASE_URL,',f'QStringLiteral("http://127.0.0.1:{port}"),');assert modified!=fixture
probe='''\n Connections {target:bridge; function onStatusMessage(msg) {if(msg.indexOf("Fox OTP ")===0) console.log("[OTP-LAB] "+msg)}}\n Timer {interval:9000;running:true;repeat:false;onTriggered:{bridge.verifyOtp("", "good", 0);bridge.verifyOtp("K1ABC", "good", 0);bridge.verifyOtp("W1AW", "bad", 1200);}}\n Timer {interval:15000;running:true;repeat:false;onTriggered:Qt.quit()}\n'''
rigs=[];results=[]
try:
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_UTC_TIMER_INSERTION_POINT';assert m.count(point)==1;main.write_text(m.replace(point,modified+'\n'+point));mod.write_text(header)
  else:main.write_text(m);mod.write_text(header+modified)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='OtpLab'+secrets.token_hex(5);rigs.append(rig);start=len(server.paths)
  with tempfile.TemporaryDirectory(prefix='decodium-otp-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   socks=[]
   for p in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',p));socks.append(sock)
   policy=f'(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))(allow network-outbound (remote ip "localhost:{port}"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];p=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   lines=sorted(line.split('[OTP-LAB] ',1)[1] for line in p.stdout.splitlines() if '[OTP-LAB] ' in line);paths=server.paths[start:]
   if p.returncode or len(lines)!=2 or len(paths)!=2 or not any('Fox OTP OK: K1ABC' in x and 'accepted' in x for x in lines) or not any('Fox OTP FAIL: W1AW' in x for x in lines):raise RuntimeError(p.stdout[-9000:]+str(paths))
   results.append([re.sub(r"/\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z/", "/TIMESTAMP/", line) for line in lines]);print(label,json.dumps(lines),flush=True)
 assert results[0]==results[1]
finally:
 for p,v in ((main,m),(mod,n),(qml,q)):p.write_text(v)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 server.shutdown();server.server_close();subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
