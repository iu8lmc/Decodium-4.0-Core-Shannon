#include <QCoreApplication>
#include <QDateTime>
#include <QVariantMap>
#include <QVariantList>
#include <QString>
#include <iostream>
#include <cmath>
#define emit
void bridgeLog(QString const&){} struct DecodeUserFilterConfig {};
namespace Radio { QString base_callsign(QString s){return s.toUpper().split('/').first();} }
QString normalizedBaseCall(QString s){return Radio::base_callsign(s);}QString decodeMirrorEntryKey(QVariantMap const&e){return e.value("key").toString();}int secondsFromUtcDisplayToken(QString s){return s=="old"?0:-1;}
class DecodiumBridge {public:
 bool m_waitPounceActive=false,m_transmitting=false,m_tuning=false,m_autoCqRepeat=false,m_manualTxHold=false,m_txEnabled=false,feature=true,accept=true,userAccept=true,duplicate=false;QString m_callsign="ME1AA",m_dxCall,m_mode="FT8",m_lastWaitPounceKey;int m_specialOperationActivity=0,m_qsoProgress=0,m_currentTx=6;double m_frequency=14074000;qint64 m_lastWaitPounceMs=0;int clicks=0;
 QVariant getSetting(QString const&,bool def){return feature?def:false;}bool shouldAcceptDecodedMessage(QString const&,QString*){return accept;}DecodeUserFilterConfig readDecodeUserFilterConfig(){return {};}bool shouldAcceptDecodeEntryByUserFilters(QVariantMap const&,DecodeUserFilterConfig const&,bool,QString*){return userAccept;}int periodMsForMode(QString const&){return 15000;}QString extractDecodedCallsign(QString const&,bool){return "K1ABC";}bool isRecentAutoCqDuplicate(QString const&,double,QString const&){return duplicate;}void statusMessage(QString const&){}void processDecodeDoubleClick(QString const&,QString const&,QString const&,int){++clicks;}
 bool tryStartWaitPounceFromEntry(QVariantMap const&,QVariantList const&,QString const&);};
constexpr int kSpecialOpFox=6,kSpecialOpHound=7;
#include "method.inc"
void check(bool x){if(!x)abort();}
int main(int n,char**v){QCoreApplication app(n,v);DecodiumBridge b;QVariantMap e{{"message","CQ K1ABC FN42"},{"fromCall","K1ABC"},{"key","one"},{"freq","1500"}};
 check(!b.tryStartWaitPounceFromEntry(e,{},"test"));b.m_waitPounceActive=true;check(!b.tryStartWaitPounceFromEntry(e,{},"test")&&b.clicks==0);b.m_txEnabled=true;b.m_transmitting=true;check(!b.tryStartWaitPounceFromEntry(e,{},"test"));b.m_transmitting=false;b.userAccept=false;check(!b.tryStartWaitPounceFromEntry(e,{},"test"));b.userAccept=true;check(!b.tryStartWaitPounceFromEntry(e,{e},"test"));b.duplicate=true;check(!b.tryStartWaitPounceFromEntry(e,{},"test"));b.duplicate=false;check(b.tryStartWaitPounceFromEntry(e,{},"test")&&b.clicks==1);check(!b.tryStartWaitPounceFromEntry(e,{},"test")&&b.clicks==1);
 b.m_lastWaitPounceKey.clear();b.m_txEnabled=false;app.setProperty("decodiumLabPassiveWaitPounce",true);check(b.tryStartWaitPounceFromEntry(e,{},"test")&&b.clicks==2);std::cout<<"disabled, unarmed, TX, filter, duplicate, throttle and passive lab guards passed\n";}
