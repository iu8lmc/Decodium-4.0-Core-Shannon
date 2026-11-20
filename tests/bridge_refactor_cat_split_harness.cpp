#include <QObject>
#include <QString>
#include <QVariant>
#include <iostream>
bool isHamlibFamilyBackend(QString const& s){return s=="hamlib" || s=="tci";}QString normalizedCatSplitMode(QString const&s){return s.trimmed().toLower();}QString canonicalApplicationDecodeMode(QString s){return s.trimmed().toUpper();}
struct Doppler {bool active=false;bool applied()const{return active;}double txDialHz()const{return 144123456.0;}};
class DecodiumBridge {public:QString m_catBackend="hamlib",m_mode="FT8";bool m_vhfUhfFeatures=false;QObject cat;Doppler*d=nullptr;Doppler *m_q65Doppler=nullptr;int m_txFrequency=2300;double m_frequency=14074000;QObject *catManagerObj()const{return const_cast<QObject*>(&cat);}bool catSplitOperationActiveForMode()const;int catSplitXitHzForTxFrequency(int)const;int effectiveTxAudioFrequencyHz()const;double catSplitTxDialFrequencyHz()const;};
#include "method.inc"
void check(bool x){if(!x)abort();}
int main(){DecodiumBridge b;b.cat.setProperty("splitMode","rig");check(b.catSplitOperationActiveForMode());check(b.catSplitXitHzForTxFrequency(2300)==500 && b.effectiveTxAudioFrequencyHz()==1800 && b.catSplitTxDialFrequencyHz()==14074500);b.cat.setProperty("splitMode","none");check(!b.catSplitOperationActiveForMode()&&b.catSplitTxDialFrequencyHz()==0);b.cat.setProperty("splitMode","emulate");b.m_vhfUhfFeatures=true;b.m_mode="JT65";check(!b.catSplitOperationActiveForMode());b.m_mode="FST4-120";check(b.catSplitOperationActiveForMode());b.m_catBackend="none";check(!b.catSplitOperationActiveForMode());Doppler d;d.active=true;b.m_q65Doppler=&d;check(b.catSplitXitHzForTxFrequency(2300)==0 && b.catSplitTxDialFrequencyHz()==144123456.0);b.m_txFrequency=-3;check(b.effectiveTxAudioFrequencyHz()==0);std::cout<<"split modes, XIT, dial, VHF, disabled CAT and Doppler passed\n";}
