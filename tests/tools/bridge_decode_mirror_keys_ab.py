#!/usr/bin/env python3
"""Exercise mirror keys and set differences with real message/time helpers."""
from pathlib import Path
import shlex, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
fixture=(root/'tests/bridge_refactor_fixtures/decode_mirror_keys_before.inc').read_text()
module=(root/'src/bridge/DecodiumBridgeDecodeMirrorKeys.cpp').read_text()
assert module.endswith(fixture)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include "DecodeMessageNormalization.h"
#include <QCoreApplication>
#include <QSet>
#include <QTextStream>
#include <QVariantList>
#include <QVariantMap>
using decodium::decode_message::canonicalDecodeMessage;
QString decodiumBridgeNormalizedVisualTxTimeToken(const QString&,const QString&);
#include "keys.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QVariantMap a{{"time","100001"},{"mode","FT8"},{"freq","1500"},{"message","CQ K1ABC FN42"},{"isTx",true},{"db","-10"},{"dt","0.1"}};auto b=a;b["time"]="100014";bool same=decodeMirrorEntryKey(a)==decodeMirrorEntryKey(b);b["message"]="CQ K2XYZ FN31";bool changed=decodeMirrorEntryKey(a)!=decodeMirrorEntryKey(b)&&decodeMirrorSemanticKey(a)!=decodeMirrorSemanticKey(b);QVariantList added=decodeEntriesNotInPrevious({a,b},{a});bool diff=added.size()==1&&added[0].toMap().value("message")==b.value("message");auto missing=a;missing["time"]="";missing["timestamp"]=qulonglong(123);bool clear=decodeRxClearEntryKey(missing).endsWith("|ts=123");bool loose=decodeMirrorLooseKey(a).contains("-10|0.1|1500|");bool ok=same&&changed&&diff&&clear&&loose;QTextStream(stdout)<<"same="<<same<<" changed="<<changed<<" diff="<<diff<<" clear="<<clear<<" loose="<<loose<<Qt::endl;return ok?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-mirror-keys-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outs=[]
 for label,body in [('before',fixture),('after',module[-len(fixture):])]:
  (d/'keys.inc').write_text(body);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(d),'-I'+str(root/'src/bridge'),str(d/'main.cpp'),str(root/'src/bridge/DecodeMessageNormalization.cpp'),str(root/'src/bridge/DecodeUtcTime.cpp'),str(root/'src/bridge/DecodiumBridgeVisualTxCoalesce.cpp'),*flags,'-o',str(exe)],check=True,stderr=subprocess.DEVNULL)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outs.append(result.stdout)
 assert outs[0]==outs[1]
