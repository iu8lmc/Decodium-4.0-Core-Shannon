#!/usr/bin/env python3
"""Compare single-mode encoder dispatch across all supported mode families."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/txwave_before.inc').read_text();new='QVector<float> buildTxWaveformForMessage('+(r/'src/bridge/DecodiumBridgeTxWave.cpp').read_text().split('QVector<float> buildTxWaveformForMessage(',1)[1];new=new.replace('decodiumBridgeFst4SamplesPerSymbol(', 'fst4SamplesPerSymbolAt48k(').replace('decodiumBridgeQ65SamplesPerSymbol(', 'q65SamplesPerSymbolAt48k(').replace('decodiumBridgeQ65SubmodeIndex(', 'q65SubmodeIndexFromModeLabel(');old=old.replace('static QVector<float> buildTxWaveformForMessage(', 'QVector<float> buildTxWaveformForMessage(',1).replace('QString* errorOut = nullptr)', 'QString* errorOut)',1)
if old!=new:raise SystemExit('TX waveform dispatch differs')
src=r'''
#include <QString>
#include <QStringList>
#include <QVector>
#include <iostream>
namespace decodium{namespace txmsg{struct E{bool ok;QVector<int> tones;};E make(QString s){return {s!="BAD",QVector<int>(220,1)};}
#define ENCODER(name) E name(QString s){return make(s);}
ENCODER(encodeFt2) ENCODER(encodeFt4) ENCODER(encodeFt8) ENCODER(encodeJt65) ENCODER(encodeJt4) ENCODER(encodeJt9) ENCODER(encodeMsk144) ENCODER(encodeWspr) ENCODER(encodeQ65)
E encodeFst4WithHint(QString s,bool,bool){return make(s);}
} namespace txwave{
#define WAVE(name,id) template<typename... A> QVector<float> name(A...){return {id};}
WAVE(generateFt2Wave,2) WAVE(generateFt4Wave,4) WAVE(generateFt8Wave,8) WAVE(generateJt65Wave,65) WAVE(generateJt4Wave,14) WAVE(generateToneWave,99) WAVE(generateMsk144Wave,144) WAVE(generateFst4Wave,44)
}}
int fst4SamplesPerSymbolAt48k(QString const&,double){return 2880;}int q65SamplesPerSymbolAt48k(QString const&){return 5760;}int q65SubmodeIndexFromModeLabel(QString const&){return 1;}
#include "method.inc"
int main(){QStringList modes{"FT2","FT4","FT8","","JT65","JT4","JT9","MSK144","WSPR","FST4","FST4W-120","Q65-30B","UNKNOWN"};for(auto const& mode:modes){QString err;auto w=buildTxWaveformForMessage(mode,"GOOD",1500,15,&err);std::cout<<(w.isEmpty()?0:int(w.first()))<<':'<<(!err.isEmpty())<<'\n';}QString err;auto w=buildTxWaveformForMessage("FT8","BAD",1500,15,&err);std::cout<<w.isEmpty()<<':'<<(!err.isEmpty())<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-txwave-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
expected='2:0\n4:0\n8:0\n8:0\n65:0\n14:0\n99:0\n144:0\n99:0\n44:0\n44:0\n99:0\n0:1\n1:1\n'
if outputs!=[expected,expected]:raise SystemExit('TX waveform dispatch A/B mismatch '+repr(outputs))
print('TX waveform A/B OK: FT2 FT4 FT8 JT65 JT4 JT9 MSK144 WSPR FST4 FST4W Q65 and invalid inputs')
