#include <QString>
#include <QtMath>
#include <cmath>
#include <cassert>
#include <iostream>
void bridgeLog(QString const&){}
static qreal txAttenuationFromSlider(double v){return qBound<qreal>(0.0,v/10.0,45.0);}
static float rxInputGainFromLevel(double v){v=qBound(0.0,v,100.0);return v<=50.0?float(v/50.0):float(1.0+((v-50.0)/50.0)*3.0);}
static qreal txGainFromSlider(double v){return 0.9*std::pow(10.0,-txAttenuationFromSlider(v)/20.0);}
static int legacyTxAttenuationFromLevel(double v){return qRound(qBound(0.0,v,450.0));}
struct Legacy {int rx=-1,tx=-1;void setRxInputLevel(int v){rx=v;}void setTxOutputAttenuation(int v){tx=v;}};
struct Input {float gain=-1;void setInputGain(float v){gain=v;}};
struct Output {qreal attenuation=-1;void setAttenuation(qreal v){attenuation=v;}};
struct Sink {float volume=-1;void setVolume(float v){volume=v;}};
class DecodiumBridge {public:
 double m_audioLevel=0.3,m_sMeter=2,m_rxInputLevel=30,m_txOutputLevel=10;bool m_autoRxInputLevel=false,legacy=false;long long m_autoRxLevelLowStartMs=1,m_autoRxLevelLastAdjustMs=1,m_autoRxLevelManualHoldUntilMs=1;
 Legacy* m_legacyBackend=nullptr;Input* m_soundInput=nullptr;Output* m_soundOutput=nullptr;Sink* m_txAudioSink=nullptr;
 int rxSignals=0,txSignals=0,autoSignals=0;bool usingLegacyBackendForTx()const{return legacy;}void autoRxInputLevelChanged(){++autoSignals;}void rxInputLevelChanged(){++rxSignals;}void txOutputLevelChanged(){++txSignals;}
 double audioLevel()const;double sMeter()const;void setRxInputLevel(double);void setAutoRxInputLevel(bool);void applyRxInputLevel(double,bool);void setTxOutputLevel(double);
};
#include "bridge_refactor_under_test.inc"
int main(){DecodiumBridge b;Legacy legacy;Input input;Output output;Sink sink;b.m_legacyBackend=&legacy;b.m_soundInput=&input;b.m_soundOutput=&output;b.m_txAudioSink=&sink;
 assert(b.audioLevel()==.3&&b.sMeter()==2);b.setAutoRxInputLevel(true);assert(b.m_autoRxInputLevel&&b.autoSignals==1&&b.m_autoRxLevelLowStartMs==0);
 b.applyRxInputLevel(90,true);assert(b.m_autoRxInputLevel&&b.m_rxInputLevel==90&&input.gain>3.3);b.setRxInputLevel(110);assert(!b.m_autoRxInputLevel&&b.m_rxInputLevel==100&&b.autoSignals==2&&input.gain==4);
 b.setRxInputLevel(110);assert(b.rxSignals==2);b.legacy=true;b.setRxInputLevel(-1);assert(legacy.rx==0&&input.gain==0);
 b.setTxOutputLevel(500);assert(b.m_txOutputLevel==450&&legacy.tx==450&&output.attenuation==45&&sink.volume>0);b.setTxOutputLevel(-1);assert(b.m_txOutputLevel==0&&legacy.tx==0&&output.attenuation==0&&sink.volume>.89);assert(b.txSignals==2);
 std::cout<<"RX/TX bounds, manual auto-level cancellation, legacy and audio sinks: OK\n";}
