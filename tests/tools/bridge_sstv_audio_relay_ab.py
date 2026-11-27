#!/usr/bin/env python3
"""Compare SSTV audio relay ingress and draining with fake receivers."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2];old=(root/'tests/bridge_refactor_fixtures/sstv_audio_relay_before.inc').read_text();new=(root/'src/bridge/DecodiumSstvAudioRelay.h').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QVector>
#include <QDebug>
#include <QTextStream>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
namespace decodium::sstv {
struct SstvRxRouteToken {bool ok=true;bool valid()const{return ok;}};
class SstvSourceClockMapper{qint64 last=-1;public:void reset(){last=-1;}std::optional<qint64> map(qint64 now,qint64){if(last>=0&&now<last)return {};last=now;return now;}};
class SstvRxRuntime{public:std::atomic<int> calls{0};std::atomic<bool> blocking{false},entered{false},release{false};bool badClock=false;static qint64 localMonotonicNowNs(){return 1000;}bool enqueuePcm16At(QVector<short>,int,SstvRxRouteToken,qint64,bool,bool invalid=false){++calls;badClock=invalid;if(blocking){entered=true;while(!release)std::this_thread::yield();}return true;}};
namespace hamdrm::waveform {class HamDrmNativeRxBackend{public:int calls=0;bool submitPcm16(const std::int16_t*,std::size_t,std::uint32_t){++calls;return true;}};}
}
#include "helpers.inc"
int main(){using namespace decodium::sstv;DecodiumSstvAudioRelay relay;SstvRxRuntime rx;SstvRxRouteToken token;
 bool disabled=!relay.submit({1},48000,token);
#if DECODIUM_HAS_HAMDRM
 auto ham=std::make_shared<hamdrm::waveform::HamDrmNativeRxBackend>();relay.enable(&rx,ham);
#else
 relay.enable(&rx);
#endif
 bool normal=relay.submit({1,2},48000,token,100)&&rx.calls==1&&!relay.submit({},48000,token,101);bool regression=relay.submit({3},48000,token,90)&&rx.badClock;
#if DECODIUM_HAS_HAMDRM
 bool digital=ham->calls==2;
#else
 bool digital=true;
#endif
 rx.blocking=true;std::thread producer([&](){relay.submit({4},48000,token,200);});auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!rx.entered&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();bool entered=rx.entered;std::atomic<bool> drained{false};std::thread stopper([&](){relay.disableAndDrain();drained=true;});std::this_thread::sleep_for(std::chrono::milliseconds(30));bool held=entered&&!drained;rx.release=true;producer.join();stopper.join();bool stopped=drained&&!relay.submit({5},48000,token);QTextStream(stdout)<<"disabled="<<disabled<<" normal="<<normal<<" regression="<<regression<<" digital="<<digital<<" held="<<held<<" stopped="<<stopped<<Qt::endl;return disabled&&normal&&regression&&digital&&held&&stopped?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-sstv-relay-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness)
 for digital in (0,1):
  out=[]
  for label,body in [('before',old),('after',new[-len(old):])]:
   (d/'helpers.inc').write_text(body);exe=d/label;subprocess.run(['c++','-std=c++17','-pthread','-DDECODIUM_HAS_SSTV=1',f'-DDECODIUM_HAS_HAMDRM={digital}','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True);v=subprocess.run([str(exe)],capture_output=True,text=True,check=True,timeout=10);print(label,'hamdrm='+str(digital),v.stdout.strip(),flush=True);out.append(v.stdout)
  assert out[0]==out[1]
