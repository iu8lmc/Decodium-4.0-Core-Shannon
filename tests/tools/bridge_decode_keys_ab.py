#!/usr/bin/env python3
"""Compare decode deduplication keys."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/decode_keys_before.inc').read_text()
new=(root/'src/bridge/DecodiumBridgeDecodeKeys.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QString>
#include <QTextStream>
#include "DecodeUtcTime.h"
#include "DecodeMessageNormalization.h"
using decodium::decode_message::canonicalDecodeMessage;
#include "helpers.inc"
int main(){bool norm=decodeDedupKey("120000"," 01500 ","CQ K1ABC FN20")==decodeDedupKey("120000","1500","CQ K1ABC FN20");bool jitter=decodeDedupKeyFt2Async("120000","1501","CQ K1ABC FN20")==decodeDedupKeyFt2Async("120000","1519","CQ K1ABC FN20");bool bucket=decodeDedupKeyFt2Async("120000","1501","CQ K1ABC FN20")!=decodeDedupKeyFt2Async("120000","1520","CQ K1ABC FN20");bool slot=decodeDedupKeyFt2Async("120000","1501","CQ K1ABC FN20",3750)!=decodeDedupKeyFt2Async("120004","1501","CQ K1ABC FN20",7500);QTextStream(stdout)<<"norm="<<norm<<" jitter="<<jitter<<" bucket="<<bucket<<" slot="<<slot<<Qt::endl;return norm&&jitter&&bucket&&slot?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-tx-wave-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outputs=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(d),'-I'+str(root/'src/bridge'),str(d/'main.cpp'),str(root/'src/bridge/DecodeUtcTime.cpp'),str(root/'src/bridge/DecodeMessageNormalization.cpp'),*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outputs.append(result.stdout)
 assert outputs[0]==outputs[1]
