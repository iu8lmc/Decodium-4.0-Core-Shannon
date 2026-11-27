#!/usr/bin/env python3
"""Run original and moved update code against a localhost fixture in isolated profiles.

Builds temporary instrumented variants that differ only in their update URLs, then
restores the production source and executable, including after a failed test.
"""
import argparse, hashlib, http.server, json, os, secrets, shutil, socket, socketserver, subprocess, tempfile, threading
from pathlib import Path
root=Path(__file__).resolve().parents[2];source=root/'src/bridge/DecodiumBridgeLocalUpdate.cpp';qml=root/'build/qml/decodium/Main.qml';exe=root/'build/decodium'; original=source.read_text();original_qml=qml.read_text();marker='static QStringList ctyDatDownloadUrls()';head=original.split(marker,1)[0];body=marker+original.split(marker,1)[1]
old=(root/'tests/bridge_refactor_fixtures/localupdate_before.inc').read_text().replace('bridgeLog(', 'decodiumBridgeLocalUpdateLog(').replace('ensureParentDirectoryForFile(path, &parentError)', 'decodiumBridgeEnsureParentDirectoryForFile(path, &parentError)')
assert body==old
cty=(root/'resources/runtime/cty.dat').read_bytes();call=(root/'resources/runtime/CALL3.TXT').read_bytes();requests=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  requests.append(self.path)
  if self.path=='/first': self.send_response(503);self.end_headers();return
  data=cty if self.path=='/second' else call if self.path=='/call3' else None
  if data is None:self.send_response(404);self.end_headers();return
  self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
 def log_message(self,*args):pass
class Server(socketserver.ThreadingMixIn,http.server.HTTPServer):daemon_threads=True
server=Server(('127.0.0.1',0),Handler);port=server.server_port;threading.Thread(target=server.serve_forever,daemon=True).start()
urls={'https://www.country-files.com/bigcty/cty.dat':f'http://127.0.0.1:{port}/first','https://www.country-files.com/cty/cty.dat':f'http://127.0.0.1:{port}/second','https://sourceforge.net/projects/wsjt-x-improved/files/Additional%20Files/CALL3.TXT/download':f'http://127.0.0.1:{port}/call3'}
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.checkCtyDatUpdate(true);bridge.downloadCall3Txt();watch.start();}}\n Timer {id:watch;interval:500;repeat:true;property int ticks:0;onTriggered:{var c=bridge.ctyDatState;var d=bridge.call3TxtState;if((c.status!=="Update in progress..." && d.status!=="Update in progress..." && c.rowCount>0 && d.rowCount>0)||++ticks>80){console.log('[UPDATE-SUCCESS] '+JSON.stringify({cty:c,call3:d,cat:bridge.catConnected,tx:bridge.transmitting}));stop();Qt.quit();}}}\n'''
rigs=[];obs=[]
try:
 for label,variant in [('before',old),('after',body)]:
  instrumented=head+variant
  for oldurl,newurl in urls.items():assert instrumented.count(oldurl)==1;instrumented=instrumented.replace(oldurl,newurl)
  source.write_text(instrumented)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=root,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(original_qml.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='UpdateSuccess'+secrets.token_hex(5);rigs.append(rig);requests.clear()
  with tempfile.TemporaryDirectory(prefix='decodium-update-success-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software',no_proxy='127.0.0.1,localhost',NO_PROXY='127.0.0.1,localhost')
   sockets=[]
   for p in (2237,2239):s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',p));sockets.append(s)
   policy=f'(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:{port}"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(exe),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','60000']
   result=subprocess.run(cmd,cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=75)
   rows=[line.split('[UPDATE-SUCCESS] ',1)[1] for line in result.stdout.splitlines() if '[UPDATE-SUCCESS] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-9000:])
   x=json.loads(rows[0]);assert not x['cat'] and not x['tx'] and not x['cty']['error'] and not x['call3']['error'] and x['cty']['rowCount']>0 and x['call3']['rowCount']>0,x
   for key,expected in [('cty',cty),('call3',call)]:
    path=Path(x[key]['localPath']);assert path.is_file() and hashlib.sha256(path.read_bytes()).digest()==hashlib.sha256(expected).digest(),str(path)
   assert '/first' in requests and '/second' in requests and '/call3' in requests,requests
   summary={'ctyRows':x['cty']['rowCount'],'call3Rows':x['call3']['rowCount'],'ctyStatus':x['cty']['status'],'call3Status':x['call3']['status'],'ctyHash':hashlib.sha256(cty).hexdigest(),'call3Hash':hashlib.sha256(call).hexdigest(),'fallback':requests.index('/first')<requests.index('/second'),'cat':x['cat'],'tx':x['tx']};obs.append(summary);print(label,json.dumps(summary,sort_keys=True),flush=True)
 assert obs[0]==obs[1],obs
finally:
 source.write_text(original);qml.write_text(original_qml);server.shutdown()
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   path=parent/('Decodium - '+rig+' - test')
   if path.is_dir():shutil.rmtree(path)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=root,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
