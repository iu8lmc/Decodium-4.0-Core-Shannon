#!/usr/bin/env python3
"""A/B test actual SMTP client with a synthetic local SMTP server."""
import json,os,secrets,shutil,socket,subprocess,tempfile,threading
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';mod=r/'src/bridge/DecodiumBridgeFt2SmtpSend.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();n=mod.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/ft2smtp_send_before.inc').read_text();mark='QVariantMap DecodiumBridge::sendFt2LinkGatewayEmail(';prefix=n[:n.index('static QString ft2LinkCleanEmailAddress(')];helpers=n[n.index('static QString ft2LinkCleanEmailAddress('):n.index(mark)];assert n[n.index(mark):]==fixture+'#undef ft2LinkSmtpSecretKey\n#undef ft2LinkSmtpConfigString\n'
probe='''
 property var smtpStates: []
 Connections {target:bridge; function onFt2LinkEmailGatewayStatus(id,box,state,detail){smtpStates.push(state);}}
 Timer {interval: 10000; running:true;repeat:false;onTriggered:{
  var empty=bridge.sendFt2LinkGatewayEmail(42,{},"x");
  var bad=bridge.sendFt2LinkGatewayEmail(42,{host:"127.0.0.1",fromEmail:"a@example.test",auth:false},"x");
  var cfg={host:"127.0.0.1",port:25252,security:"NONE",auth:false,fromEmail:"Sender <a@example.test>",toEmail:"b@example.test"};
  var sent=bridge.sendFt2LinkGatewayEmail(42,cfg,"Subject: synthetic\\n\\n.line\\nend");
  console.log("[SMTP-START] "+JSON.stringify({empty:empty.error,bad:bad.error,queued:sent.ok,box:sent.mailboxId}));
 }}
 Timer {interval:15000;running:true;repeat:false;onTriggered:{console.log("[SMTP-END] "+JSON.stringify(smtpStates));Qt.quit();}}
'''
def serve(sock,records,errors):
 try:
  sock.settimeout(30);c,_=sock.accept();c.settimeout(10)
  with c, c.makefile('rb') as f:
   c.sendall(b'220 synthetic ESMTP\r\n'); data=False
   while True:
    line=f.readline()
    if not line:break
    records.append(line.decode('utf-8').rstrip('\r\n'))
    if data:
     if line==b'.\r\n':data=False;c.sendall(b'250 accepted\r\n')
    elif line.startswith(b'EHLO'):c.sendall(b'250 synthetic\r\n')
    elif line.startswith((b'MAIL FROM:',b'RCPT TO:')):c.sendall(b'250 ok\r\n')
    elif line==b'DATA\r\n':data=True;c.sendall(b'354 send data\r\n')
    elif line==b'QUIT\r\n':c.sendall(b'221 bye\r\n');break
    else:raise RuntimeError(repr(line))
 except Exception as e:errors.append(str(e))
rigs=[];out=[]
try:
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_SHUTDOWN_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,helpers+fixture+point));mod.write_text(prefix)
  else:main.write_text(m);mod.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='SmtpSend'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-smtp-send-') as tmp, socket.socket() as server:
   server.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);server.bind(('127.0.0.1',25252));server.listen(1);records=[];errors=[];thread=threading.Thread(target=serve,args=(server,records,errors),daemon=True);thread.start();env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:*"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45);thread.join(12)
   rows={tag:[x.split(tag+' ',1)[1] for x in result.stdout.splitlines() if tag+' ' in x] for tag in ('[SMTP-START]','[SMTP-END]')}
   if result.returncode or any(len(v)!=1 for v in rows.values()) or errors or thread.is_alive():raise RuntimeError(str((rows,records,errors,result.stdout[-5000:])))
   start=json.loads(rows['[SMTP-START]'][0]);states=json.loads(rows['[SMTP-END]'][0]);assert start=={'empty':'Missing SMTP host','bad':'Missing SMTP recipient email','queued':True,'box':42},start
   assert 'Sent' in states and any(x=='..line' for x in records) and any(x=='MAIL FROM:<a@example.test>' for x in records) and any(x=='RCPT TO:<b@example.test>' for x in records),(states,records)
   normalized=[x for x in records if not x.startswith('EHLO ')];out.append((start,states,normalized));print(label,json.dumps(out[-1]),flush=True)
 assert out[0]==out[1]
finally:
 main.write_text(m);mod.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
