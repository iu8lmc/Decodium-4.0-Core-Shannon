#!/usr/bin/env python3
"""Compare the moved update logic and exercise validation and local HTTP requests."""
import os, shlex, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
old=(r/'tests/bridge_refactor_fixtures/localupdate_before.inc').read_text()
new=(r/'src/bridge/DecodiumBridgeLocalUpdate.cpp').read_text().split('static QStringList ctyDatDownloadUrls()',1)[1]
new='static QStringList ctyDatDownloadUrls()'+new
new=new.replace('decodiumBridgeLocalUpdateLog(', 'bridgeLog(').replace('decodiumBridgeEnsureParentDirectoryForFile(path, &parentError)', 'ensureParentDirectoryForFile(path, &parentError)')
if old != new: raise SystemExit('Local update implementation differs from the original')
helpers=old.split('void DecodiumBridge::checkCtyDatUpdate(',1)[0]
program=r'''
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <iostream>
void bridgeLog(const QString&) {}
#include "helpers.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);
 auto c=[](QByteArray b){QString why;bool ok=ctyDatPayloadLooksValid(b,&why);std::cout<<ok<<':'<<why.toStdString()<<'\n';};
 auto k=[](QByteArray b){QString why;bool ok=isProbablyCall3TxtPayload(b,&why);std::cout<<ok<<':'<<why.toStdString()<<'\n';};
 c(QByteArray(19999,'x')); c(QByteArray("<html>")+QByteArray(20000,'x')); c(QByteArray(20000,'x'));c(QByteArray("A:B:C:D:E:F:G:H:\nA:B:C:D:E:F:G:H:\n")+QByteArray(20000,'x'));
 k(QByteArray(1023,'x')); k(QByteArray("<html>")+QByteArray(1100,'x'));k(QByteArray(1100,'x'));k(QByteArray("A,B\n")+QByteArray(1100,'x'));
 QTcpServer server;if(!server.listen(QHostAddress::LocalHost,0))return 2;
 QNetworkAccessManager nam;nam.setProxy(QNetworkProxy::NoProxy);QString base=QString("http://127.0.0.1:%1/").arg(server.serverPort());nam.setProperty("ctyUrls",QStringList{base+"one",base+"two"});nam.setProperty("ctyUrlIndex",0);
 for(int n=0;n<2;n++){std::cout<<issueNextCtyDatRequest(&nam)<<':'<<nam.property("ctyUrlIndex").toInt()<<'\n';QElapsedTimer timer;timer.start();while(!server.hasPendingConnections()&&timer.elapsed()<3000)app.processEvents();auto *socket=server.nextPendingConnection();if(!socket)return 3;while(!socket->canReadLine()&&timer.elapsed()<3000)app.processEvents();QByteArray req=socket->readAll();while(!req.contains("\r\n\r\n")&&timer.elapsed()<3000){app.processEvents();req+=socket->readAll();}std::cout<<(req.contains(n?"GET /two ":"GET /one "))<<':'<<req.contains("User-Agent: Decodium/4.0 cty.dat updater")<<':'<<req.contains("Accept-Language: en-US,en;q=0.8")<<'\n';socket->write("HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");socket->flush();socket->disconnectFromHost();app.processEvents();}
 std::cout<<issueNextCtyDatRequest(&nam)<<':'<<nam.property("ctyUrlIndex").toInt()<<'\n'; }
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Network'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-localupdate-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(program)
 for text in (helpers,new.split('void DecodiumBridge::checkCtyDatUpdate(',1)[0]):
  (p/'helpers.inc').write_text(text);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
if out[0]!=out[1]:raise SystemExit('local update helper A/B mismatch')
assert out[0].count('1:1:1')==2 and out[0].endswith('0:2\n'),out[0]
print('local update A/B: source identical; payload validation and local HTTP fallback passed')
