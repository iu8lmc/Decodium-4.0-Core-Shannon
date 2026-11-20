#!/usr/bin/env python3
"""Compare PSK XML parsing in isolated apps using only a local HTTP server."""
import http.server,json,os,secrets,shutil,socket,subprocess,tempfile,threading
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgePskQueries.cpp';qml=r/'build/qml/decodium/Main.qml'
a,b,q=(x.read_text() for x in (main,module,qml));sig='void DecodiumBridge::fetchPskReporterSnapshot(';pos=b.index(sig);head=b[:b.index('void DecodiumBridge::sendPskReporterNow()')];fixture=b[pos:]; assert fixture.count('QStringLiteral("https://retrieve.pskreporter.info/query")')==1
fixture=fixture.replace('QStringLiteral("https://retrieve.pskreporter.info/query")','qEnvironmentVariable("LAB_PSK_URL")')
xml=b'<receptionReports><receptionReport receiverCallsign="K1ABC" receiverLocator="FN42" sNR="-12" frequency="14074000"/><receptionReport receiverCallsign="K1ABC" receiverLocator="FN42" sNR="-3" frequency="14074000"/><receptionReport receiverCallsign="W1AW" receiverLocator="BAD!" sNR="5" frequency="7074000"/></receptionReports>'
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  self.server.paths.append(self.path);self.send_response(200);self.send_header('Content-Type','application/xml');self.send_header('Content-Length',str(len(xml)));self.end_headers();self.wfile.write(xml)
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);server.paths=[];threading.Thread(target=server.serve_forever,daemon=True).start()
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.callsign="9H1SR";bridge.grid="JM75";bridge.fetchPskHeardBy();watch.start();}}\n Timer {id:watch;interval:200;running:false;repeat:true;property int n:0;onTriggered:{if(bridge.pskHeardByFetching&&++n<50)return;var rows=bridge.pskHeardByList;console.log('[PSK-LOCAL] '+JSON.stringify({count:bridge.pskHeardByCount,rows:Array.from(rows).map(x=>({call:x.call,grid:x.grid,snr:x.snr,freq:x.freq})),fetching:bridge.pskHeardByFetching,cat:bridge.catConnected,tx:bridge.transmitting}));stop();Qt.quit();}}\n'''
rigs=[];results=[]
try:
 for label in ('before','after'):
  if label=='before':main.write_text(a+'\n'+head+fixture);module.write_text(b[:pos])
  else:main.write_text(a);module.write_text(b[:pos]+fixture)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='PskLocal'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-psk-local-') as tmp:
   env=os.environ.copy();env.update(LAB_PSK_URL=f'http://127.0.0.1:{server.server_port}/query',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy=f'(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))(allow network-outbound (remote ip "localhost:{server.server_port}"))'
   proc=subprocess.run(['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'],cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[PSK-LOCAL] ',1)[1] for line in proc.stdout.splitlines() if '[PSK-LOCAL] ' in line]
   if proc.returncode or len(rows)!=1:raise RuntimeError(proc.stdout[-5000:])
   value=json.loads(rows[0]);expected={'count':2,'rows':[{'call':'K1ABC','grid':'FN42','snr':-12,'freq':14074000},{'call':'W1AW','grid':'','snr':5,'freq':7074000}],'fetching':False,'cat':False,'tx':False};assert value==expected,value
   results.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
   for sock in socks:sock.close()
 assert results[0]==results[1] and len(server.paths)==4 and sorted(server.paths[:2])==sorted(server.paths[2:]),server.paths
 assert all('senderCallsign=9H1SR' in p and 'rptlimit=50' in p for p in server.paths),server.paths
finally:
 for p,t in ((main,a),(module,b),(qml,q)):p.write_text(t)
 server.shutdown();server.server_close()
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
