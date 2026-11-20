#include <QString>
#include <QtMath>
#include <cassert>
#include <iostream>
void bridgeLog(QString const&){}
static float rxInputGainFromLevel(double v){v=qBound(0.0,v,100.0);return v<=50.0?float(v/50.0):float(1.0+((v-50.0)/50.0)*3.0);}
static double rxInputLevelFromGain(double g){g=qBound(0.0,g,4.0);return g<=1.0?g*50.0:50.0+((g-1.0)/3.0)*50.0;}
static constexpr double kAutoRxInputMaxLevel=100.0;
class DecodiumBridge {public:bool m_autoRxInputLevel=false;double m_rxInputLevel=50;qint64 m_autoRxLevelLowStartMs=0,m_autoRxLevelLastAdjustMs=0,m_autoRxLevelManualHoldUntilMs=0;int changes=0;void applyRxInputLevel(double v,bool automatic){assert(automatic);m_rxInputLevel=v;++changes;}void updateAutoRxInputLevel(double,double,int,int,qint64);};
#include "bridge_refactor_under_test.inc"
int main(){DecodiumBridge b;b.m_autoRxLevelLowStartMs=123;b.updateAutoRxInputLevel(.02,.3,0,1000,10000);assert(b.changes==0&&b.m_autoRxLevelLowStartMs==0);
 b.m_autoRxInputLevel=true;b.m_autoRxLevelManualHoldUntilMs=11000;b.updateAutoRxInputLevel(.02,.3,0,1000,10000);assert(b.changes==0&&b.m_autoRxLevelLowStartMs==0);b.m_autoRxLevelManualHoldUntilMs=0;
 b.updateAutoRxInputLevel(.05,.99,4,1000,12000);assert(b.changes==1&&b.m_rxInputLevel<50);double down=b.m_rxInputLevel;b.updateAutoRxInputLevel(.05,.99,4,1000,12100);assert(b.changes==1&&b.m_rxInputLevel==down);
 b.updateAutoRxInputLevel(.00001,.001,0,1000,15000);assert(b.m_autoRxLevelLowStartMs==0);b.updateAutoRxInputLevel(.01,.2,0,1000,16000);assert(b.m_autoRxLevelLowStartMs==16000);b.updateAutoRxInputLevel(.01,.2,0,1000,18000);assert(b.changes==1);b.updateAutoRxInputLevel(.01,.2,0,1000,19100);assert(b.changes==2&&b.m_rxInputLevel>down);
 b.m_rxInputLevel=110;b.updateAutoRxInputLevel(.05,.5,0,1000,22000);assert(b.m_rxInputLevel==100&&b.changes==3);b.m_autoRxLevelLowStartMs=1;b.updateAutoRxInputLevel(.01,.2,0,32,25000);assert(b.m_autoRxLevelLowStartMs==0);
 std::cout<<"auto RX clipping, cooldown, hold, silence, low-signal rise and limits: OK\n";}
