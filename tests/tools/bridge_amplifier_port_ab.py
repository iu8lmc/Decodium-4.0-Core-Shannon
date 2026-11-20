#!/usr/bin/env python3
"""Compare the real amplifier/CAT collision helper with isolated fake ports."""
from pathlib import Path
import os,shlex,subprocess,tempfile
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/amplifier_port_before.inc').read_text();source=(r/'src/bridge/DecodiumBridgeAmplifierConfig.cpp').read_text();start=source.index('static bool amplifierPortClashesWithCat(');end=source.index('QVariantMap DecodiumBridge::cercaAmplificatore()',start);new=source[start:end];assert old.strip()==new.strip()
harness=r'''
#include <QString>
#include <iostream>
struct DecodiumTransceiverManager {QString serialPort() const{return " /dev/cat ";} QString pttPort() const{return "/dev/ptt";}};
#include "method.inc"
int main(){DecodiumTransceiverManager c;for(auto s:{"", " /DEV/CAT ","/dev/ptt","/dev/amp"})std::cout<<decodiumBridgeAmplifierPortClashes(&c,QString::fromUtf8(s));std::cout<<decodiumBridgeAmplifierPortClashes(nullptr,"/dev/cat")<<"\n";}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-amp-port-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text(harness)
 for label,body in [('before',old),('after',new)]:
  (d/'method.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test')],check=True);out=subprocess.check_output([str(d/'test')],text=True);outputs.append(out);print(label,out.strip())
assert outputs==['01100\n','01100\n'],outputs
