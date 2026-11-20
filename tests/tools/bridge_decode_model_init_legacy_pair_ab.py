#!/usr/bin/env python3
"""Compare model initialization with a real legacy RX over virtual audio."""
import json, subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridgeConstructor.cpp'
init=r/'src/bridge/DecodiumBridgeLegacyInit.cpp'
m=main.read_text(); original_init=init.read_text()
fixture=(r/'tests/bridge_refactor_fixtures/decode_model_init_before.inc').read_text()
module=(r/'src/bridge/DecodiumBridgeDecodeModelInit.cpp').read_text()
assert module.split('void DecodiumBridge::initialiseDecodeModels()\n{\n',1)[1]==fixture+'}\n'
point='    initialiseDecodeModels();\n'; assert m.count(point)==1
macro='#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled'; assert original_init.count(macro)==1
policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
try:
 init.write_text(original_init.replace(macro,'#define embeddedLegacyRigControlEnabledForBackend(...) false',1))
 outputs=[]
 for label in ('before','after'):
  declarations='#include "DecodiumBridgeDecodeUiFlags.h"\n#include "DecodeListModel.h"\n#include <utility>\n'
  main.write_text(m.replace('#include "DecodiumBridge.h"', '#include "DecodiumBridge.h"\n'+declarations,1).replace(point,fixture,1) if label=='before' else m)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL)
  result=subprocess.run(['sandbox-exec','-p',policy,'python3','tests/tools/bridge_legacydelta_pair_lab.py'],cwd=r,check=True,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=130)
  value=json.loads(result.stdout.strip().splitlines()[-1]);assert value=={'found':True,'legacy':True,'tx':False},value
  outputs.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert outputs[0]==outputs[1]
finally:
 main.write_text(m);init.write_text(original_init)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL)
