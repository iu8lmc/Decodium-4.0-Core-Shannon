#!/usr/bin/env python3
"""Compare precomputed-wave copy, zero fill and mode-sensitive clearing."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2];old=(root/'tests/bridge_refactor_fixtures/precomputed_wave_store_before.inc').read_text();new=(root/'src/bridge/DecodiumBridgePrecomputedWave.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QString>
#include <QVector>
#include <QReadWriteLock>
#include <QTextStream>
#include <algorithm>
constexpr int FOXCOM_WAVE_SIZE=8;
struct Block {float* wave=nullptr;} foxcom_;
bool foxcom_ensure_wave(Block* b){static float buf[8]={};b->wave=buf;return true;}
QReadWriteLock& fox_wave_lock(){static QReadWriteLock l;return l;}
QString& precomputed_tx_wave_mode(){static QString s;return s;}
QVector<float>& precomputed_tx_wave_buffer(){static QVector<float> v;return v;}
#include "helpers.inc"
int main(){storeBridgePrecomputedTxWave("FT8",{1.f,-2.f,3.f});bool copy=foxcom_.wave[0]==1&&foxcom_.wave[1]==-2&&foxcom_.wave[2]==3&&foxcom_.wave[3]==0&&foxcom_.wave[7]==0&&precomputed_tx_wave_buffer().size()==3;storeBridgePrecomputedTxWave("FT4",{});bool other=precomputed_tx_wave_mode()=="FT8"&&foxcom_.wave[0]==1;storeBridgePrecomputedTxWave("FT8",{});bool clear=precomputed_tx_wave_mode().isEmpty()&&precomputed_tx_wave_buffer().isEmpty()&&foxcom_.wave[0]==0;storeBridgePrecomputedTxWave("FT2",QVector<float>(12,0.5f));bool limit=precomputed_tx_wave_buffer().size()==8&&foxcom_.wave[7]==0.5f;QTextStream(stdout)<<"copy="<<copy<<" other="<<other<<" clear="<<clear<<" limit="<<limit<<Qt::endl;return copy&&other&&clear&&limit?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-wave-store-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);out=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label;subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True);v=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,v.stdout.strip(),flush=True);out.append(v.stdout)
 assert out[0]==out[1]
