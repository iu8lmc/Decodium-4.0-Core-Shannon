#include <QtCore>
#include <utility>
#include <cstdio>
class DecodiumBridge {public:
 QString m_mode="FT8";int m_digiLastCycleMs=0,m_digiLastStreams=0,m_digiLastDeferred=0,m_txFrequency=1500,m_rxFrequency=1000;QVector<int>m_mamSlots,m_callerQueue;
 bool foxElasticActive()const{return false;}QVariantMap digiPileupStatus()const;int mamMinStreamGapHz()const;int mamStreamStepHz()const;QVector<int> mamPlanCqFrequencies(int,const QVector<int>&)const;
};
#include "bridge_refactor_mam_plan.inc"
static void check(bool v,const char*s){if(!v){fprintf(stderr,"FAIL %s\n",s);exit(1);}puts(s);}
int main(){DecodiumBridge b;check(b.digiPileupStatus().value("gapHz").toInt()==50 && b.mamPlanCqFrequencies(2,{1500})==QVector<int>({1560,1620}),"ft8-spacing");
 b.m_mode="FT2";check(b.mamMinStreamGapHz()==170 && b.mamStreamStepHz()==180 && b.mamPlanCqFrequencies(2,{1500,1680})==QVector<int>({1860,2040}),"ft2-spacing");
 b.m_mode="FT4";b.m_txFrequency=3980;check(b.mamMinStreamGapHz()==85 && b.mamPlanCqFrequencies(3,{})==QVector<int>({3980,3890,3800}),"ft4-upper-bound");
 b.m_txFrequency=100;b.m_rxFrequency=1000;check(b.mamPlanCqFrequencies(2,{})==QVector<int>({200,290}),"lower-bound");check(b.mamPlanCqFrequencies(0,{}).isEmpty(),"zero");}
