#!/usr/bin/env python3
"""Compare TX-row coalescing with real UTC slot helpers."""
from pathlib import Path
import shlex, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
fixture=(root/'tests/bridge_refactor_fixtures/visual_tx_coalesce_before.inc').read_text()
module=(root/'src/bridge/DecodiumBridgeVisualTxCoalesce.cpp').read_text()
start=module.index('static bool shouldCoalesceVisualTxRows(')
end=module.index('QString decodiumBridgeNormalizedVisualTxTimeToken(',start)
assert module[start:end]==fixture
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include "DecodeUtcTime.h"
#include <QCoreApplication>
#include <QHash>
#include <QTextStream>
#include <QVariantList>
#include <QVariantMap>
#include "coalesce.inc"
QVariantMap row(QString t,bool tx,bool local,QString message){return {{"time",t},{"isTx",tx},{"localTxMirror",local},{"message",message}};}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QVariantList rows{row("100001",true,false,"old"),row("100014",true,true,"local"),row("100014",false,false,"rx"),row("100015",true,false,"next")};coalesceRxPaneTxRows(rows,"FT8");bool merged=rows.size()==3&&rows[0].toMap().value("message")=="local"&&rows[1].toMap().value("message")=="rx"&&rows[2].toMap().value("message")=="next";QVariantList jt{row("100001",true,false,"a"),row("100001",true,true,"b")};coalesceRxPaneTxRows(jt,"JT65");bool unsupported=jt.size()==2;bool ft4=normalizedVisualTxTimeToken("100007","FT4")=="100007";bool ft2=normalizedVisualTxTimeToken("100003","FT2")=="100003";QVariantMap empty=row("",true,false,"x");bool noTime=!rxPaneVisualTxSlotKey(empty,"FT8").isEmpty();QVariantList duplicates{row("235959",true,false,"a"),row("235959",true,true,"b")};coalesceRxPaneTxRows(duplicates,"FT8");bool midnight=duplicates.size()==1&&duplicates[0].toMap().value("message")=="b";bool ok=merged&&unsupported&&ft4&&ft2&&noTime&&midnight;QTextStream(stdout)<<"merged="<<merged<<" unsupported="<<unsupported<<" ft4="<<ft4<<" ft2="<<ft2<<" noTime="<<noTime<<" midnight="<<midnight<<Qt::endl;return ok?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-visual-coalesce-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outs=[]
 for label,body in [('before',fixture),('after',module[start:end])]:
  (d/'coalesce.inc').write_text(body);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(d),'-I'+str(root/'src/bridge'),str(d/'main.cpp'),str(root/'src/bridge/DecodeUtcTime.cpp'),*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True);print(label,result.stdout.strip(),flush=True);assert result.returncode==0;outs.append(result.stdout)
 assert outs[0]==outs[1]
