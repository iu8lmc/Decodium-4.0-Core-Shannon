#!/usr/bin/env python3
import os,re,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
a=(R/'tests/bridge_refactor_fixtures/legacy_mirror_before.inc').read_text().strip();new=(R/'src/bridge/DecodiumBridgeLegacyMirror.cpp').read_text();start='QVariantList DecodiumBridge::mirrorLegacyDecodeLines(';b=new[new.index(start):].strip().replace('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey(').replace('decodiumBridgeMirrorSemanticKey(', 'decodeMirrorSemanticKey(').replace('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')
if a!=b:raise RuntimeError('legacy mirror differs from pre-extraction snapshot')
if re.match(r'^(\d{4}|\d{6})\s+(-?\d+)\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+\S+\s+(.*)$', '1200 -10  0.1 1500~   CQ W1AW FN31'):raise RuntimeError('fallback fixture unexpectedly matches fast path')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core','Qt6Network',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-legacymirror-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_legacymirror_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_legacymirror.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-include',str(R/'build/wsjtx_config.h'),'-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),str(d/'test.cpp'),str(R/'src/bridge/DecodeMessageNormalization.cpp'),str(R/'src/bridge/DecodeUtcTime.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp'),str(R/'Decoder/decodedtext.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or 'first|count=4|enriched=4' not in out[0] or 'reused|count=4|enriched=4' not in out[0] or 'fallback=120000|-10|0.1|1500' not in out[0] or 'window=384' not in out[0]:raise RuntimeError('unexpected legacy mirror output '+repr(out[0][:1500]))
 print('Legacy mirror A/B: RX/TX parsing, enrichment reuse, filters and 384-row window match; real DecodedText parser, controlled bridge collaborators')
