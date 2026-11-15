#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,n):i=s.index(n);return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout;new=(R/'src/bridge/DecodiumBridgeDecodeSemantic.cpp').read_text();names=['bool DecodiumBridge::contestDecodeExchangesEnabled()', 'bool DecodiumBridge::shouldAcceptDecodedMessage('];a='\n'.join(sec(old,n) for n in names);b='\n'.join(sec(new,n).replace('decodiumBridgeDecodeTelemetryOnly(', 'decodeListModel_isTelemetryOnlyMessage(') for n in names)
guard=b[b.index('    // FST4W carries beacon reports'):b.index('    bool const ft2StrictSemanticGate')]
if 'isGridTokenStrict' not in guard or a!=b.replace(guard,''):raise RuntimeError('unexpected decode semantic source change')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-semantic-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_semantic_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_semantic.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),str(d/'test.cpp'),str(R/'src/bridge/DecodeValidationRules.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or '|CQ K1ABC FN42|1|' not in out[0] or '|CQ IU8LMC JN70|0|self CQ decoded on RX' not in out[0] or '|CQ X9 FN42|0|CQ from invalid callsign (ghost)' not in out[0] or 'FT2 standalone telemetry/hex payload' not in out[0]:raise RuntimeError('unexpected semantic output')
 print(f'Decode semantic A/B: {len(out[0].splitlines())} mode, contest, QSO and message combinations match; production telemetry; hash context controlled')
 harness=(R/'tests/bridge_refactor_semantic_harness.cpp').read_text().split('int main(',1)[0]
 harness+=r'''int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;b.m_mode="FST4W-120";for(auto const& msg:QStringList{"K1ABC FN42 33","K1ABC/P 30","<...> FN42 33","<1234567> FN42 33"}){QString reason;if(!b.shouldAcceptDecodedMessage(msg,&reason,true)){qCritical()<<"rejected"<<msg<<reason;return 1;}}b.m_mode="FT8";QString reason;if(b.shouldAcceptDecodedMessage("K1ABC FN42 33",&reason,true)){qCritical()<<"FT8 guard changed";return 2;}qInfo()<<"FST4W beacon formats accepted; FT8 remains unchanged";}
'''
 (d/'test.cpp').write_text(harness);(d/'bridge_refactor_semantic.inc').write_text(b)
 run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),str(d/'test.cpp'),str(R/'src/bridge/DecodeValidationRules.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp'),*flags,'-o',str(d/'test'))
 run(str(d/'test'))
