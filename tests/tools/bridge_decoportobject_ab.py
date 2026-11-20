#!/usr/bin/env python3
"""Compare gateway hook routing with synthetic rig and meters, without radio."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/decoportobject_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeDecoPortObject.cpp').read_text();new=s[s.index('QObject* DecodiumBridge::decoPortGatewayObject() const'):];assert old==new
code=r'''
#include <QObject>
#include <QString>
#include <QVector>
#include <functional>
#include <iostream>
#include <utility>
#define connect(...) ((void)0)
struct DecoPortRigDriver {bool opened=false,tx=false;double hz=1;QString mode="USB";bool isOpen(){return opened;}double frequencyHz(){return hz;}QString modeName(){return mode;}bool ptt(){return tx;}void setFrequencyHz(double x){hz=x;}void setModeName(const QString&x){mode=x;}};
struct Cat {bool tx=false,valid=true;double power=10,ros=2;bool pttActive()const{return tx;}bool strengthValid()const{return valid;}double strengthDb()const{return -73;}double powerWatts()const{return power;}double swr()const{return ros;}bool alcValid()const{return valid;}double alc()const{return 25;}bool drainVoltageValid()const{return valid;}double drainVoltage()const{return 13;}bool drainCurrentValid()const{return valid;}double drainCurrent()const{return 2;}bool paTemperatureValid()const{return valid;}double paTemperature()const{return 40;}bool compressionValid()const{return valid;}double compressionDb()const{return 3;}bool powerSettingValid()const{return valid;}double powerSettingPct()const{return 50;}};
class DecodiumDecoPortGateway:public QObject {public:struct RigHooks {
std::function<bool()> connected,pttActive,canTransmit;std::function<double()> frequencyHz;std::function<QString()> modeName;std::function<void(double)> setFrequencyHz;std::function<void(const QString&)> setModeName;std::function<void(bool)> setPtt;std::function<bool(double&)> sMeterDbm,forwardPowerW,swr,alcPct,drainVoltage,drainCurrent,paTemperature,compressionDb,powerSettingPct;};RigHooks hooks;QByteArray key;DecodiumDecoPortGateway(QObject*p):QObject(p){}void setRigHooks(RigHooks h){hooks=std::move(h);}void setAuthKey(QByteArray x){key=x;}void txAudioDue(const QVector<short>&){} };
class DecodiumBridge:public QObject {public:mutable DecodiumDecoPortGateway*m_decoPortGateway=nullptr;DecoPortRigDriver*m_decoPortRig=nullptr;Cat*m_hamlibCat=nullptr;bool m_transmitting=false,m_tuning=false,connected=false,keyed=false;double freq=100;QString mode="FT8";int qsys=0;bool catConnected(){return connected;}double frequency(){return freq;}QString catMode(){return mode;}bool transmitting(){return m_transmitting;}void requestRigFrequencyFromBridge(double x,const QString&){freq=x;qsys++;}void setMode(const QString& x){mode=x;}void decoPortKeyLocalRig(bool x){keyed=x;}void decoPortPlayTxAudio(const QVector<short>&){}QByteArray decoPortAuthKey()const{return "test";}QObject*decoPortGatewayObject()const;};
#include "methods.inc"
int main(){DecodiumBridge b;auto*g=static_cast<DecodiumDecoPortGateway*>(b.decoPortGatewayObject());auto&h=g->hooks;std::cout<<(b.decoPortGatewayObject()==g)<<':'<<(g->key=="test")<<':'<<h.connected()<<':'<<h.canTransmit()<<'\n';b.connected=true;h.setFrequencyHz(200);h.setModeName("FT4");h.setPtt(true);std::cout<<h.connected()<<':'<<h.canTransmit()<<':'<<h.frequencyHz()<<':'<<h.modeName().toStdString()<<':'<<b.qsys<<':'<<b.keyed<<'\n';b.m_transmitting=true;std::cout<<h.canTransmit()<<':'<<h.pttActive()<<'\n';b.m_transmitting=false;b.m_tuning=true;std::cout<<h.canTransmit()<<'\n';b.m_tuning=false;DecoPortRigDriver rig;rig.opened=true;rig.tx=true;b.m_decoPortRig=&rig;h.setFrequencyHz(300);h.setModeName("LSB");std::cout<<h.connected()<<':'<<h.canTransmit()<<':'<<h.frequencyHz()<<':'<<h.modeName().toStdString()<<':'<<h.pttActive()<<':'<<b.qsys<<'\n';double v=0;std::cout<<h.sMeterDbm(v)<<':'<<h.forwardPowerW(v)<<':'<<h.swr(v)<<':'<<h.alcPct(v)<<':'<<h.drainVoltage(v)<<'\n';Cat cat;b.m_hamlibCat=&cat;std::cout<<h.sMeterDbm(v)<<':'<<v<<':'<<h.forwardPowerW(v)<<':'<<h.swr(v)<<':'<<h.alcPct(v)<<'\n';cat.tx=true;std::cout<<h.forwardPowerW(v)<<':'<<v<<':'<<h.swr(v)<<':'<<v<<':'<<h.alcPct(v)<<':'<<v<<'\n';cat.power=0;cat.ros=.5;cat.valid=false;std::cout<<h.sMeterDbm(v)<<':'<<h.forwardPowerW(v)<<':'<<h.swr(v)<<':'<<h.alcPct(v)<<':'<<h.drainVoltage(v)<<':'<<h.drainCurrent(v)<<':'<<h.paTemperature(v)<<':'<<h.compressionDb(v)<<':'<<h.powerSettingPct(v)<<'\n';cat.valid=true;std::cout<<h.drainVoltage(v)<<':'<<v<<':'<<h.drainCurrent(v)<<':'<<v<<':'<<h.paTemperature(v)<<':'<<v<<':'<<h.compressionDb(v)<<':'<<v<<':'<<h.powerSettingPct(v)<<':'<<v<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-decoport-object-') as d:
 p=Path(d);(p/'main.cpp').write_text(code);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'lab')],check=True);outputs.append(subprocess.check_output([str(p/'lab')],text=True))
assert outputs[0]==outputs[1];assert outputs[0]== '1:1:0:0\n1:1:200:FT4:1:1\n0:1\n0\n1:1:300:LSB:1:1\n0:0:0:0:0\n1:-73:0:0:0\n1:10:1:2:1:25\n0:0:0:0:0:0:0:0:0\n1:13:1:2:1:40:1:3:1:50\n',outputs
print('DecoPort object A/B passed: lazy singleton, auth, routing, PTT guards and meter validity')
