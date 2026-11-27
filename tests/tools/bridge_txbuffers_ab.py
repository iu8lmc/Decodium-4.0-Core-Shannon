#!/usr/bin/env python3
"""Compare audio status, buffer sizing and precompute delay at slot boundaries."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/txbuffers_before.inc').read_text();new='QString audioStateToString('+(r/'src/bridge/DecodiumBridgeTxFormat.cpp').read_text().split('QString audioStateToString(',1)[1]
for a,b in [('static QString audioStateToString(', 'QString audioStateToString('),('static QString audioErrorToString(', 'QString audioErrorToString('),('static qsizetype txAudioBufferBytesForMode(', 'qsizetype txAudioBufferBytesForMode('),('static int txAudioPrecomputeRetryDelayMs(', 'int txAudioPrecomputeRetryDelayMs(')]:old=old.replace(a,b)
if old!=new:raise SystemExit('TX buffer/status extraction differs')
src=r'''
#include <QAudio>
#include <QAudioFormat>
#include <QString>
#include <iostream>
static qint64 nowMs=0;qint64 ntpCorrectedCurrentMSecsSinceEpoch(){return nowMs;}
#include "method.inc"
int main(){for(int state=0;state<=4;state++)std::cout<<audioStateToString(static_cast<QAudio::State>(state)).toStdString()<<'\n';for(int error=0;error<=5;error++)std::cout<<audioErrorToString(static_cast<QAudio::Error>(error)).toStdString()<<'\n';for(int channel:{1,2}){QAudioFormat f;f.setSampleRate(48000);f.setChannelCount(channel);f.setSampleFormat(QAudioFormat::Int16);for(QString mode:{QString("FT2"),QString("FT8")})for(bool tune:{false,true})for(bool conservative:{false,true})std::cout<<txAudioBufferBytesForMode(f,mode,tune,conservative)<<'\n';}for(QString mode:{QString("FT2"),QString("FT4"),QString("FT8"),QString("UNKNOWN")})for(qint64 now:{0LL,899LL,900LL,3300LL,3749LL,7500LL,14999LL}){nowMs=now;std::cout<<txAudioPrecomputeRetryDelayMs(mode,true)<<','<<txAudioPrecomputeRetryDelayMs(mode,false)<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Multimedia'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-txbuffers-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
if outputs[0]!=outputs[1]:raise SystemExit('TX buffer/status A/B mismatch')
lines=outputs[0].splitlines();assert len(lines)==55 and '0,0' in lines and any(x.startswith('900,') for x in lines),outputs[0]
print('TX buffer/status A/B OK: states, errors, buffer sizes and precompute boundaries')
