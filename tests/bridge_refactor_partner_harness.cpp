#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>
namespace decodium { void beginActiveSettingsProfile(QSettings& s){s.beginGroup("test-profile");} }
static void bridgeLog(const QString&){}
class DecodiumBridge:public QObject {
public:
 struct PartnerMemoryEntry {QString callUpper; int lastTxNum=0,qsoProgress=0,lastSnrDb=127; qint64 lastSeenMs=0;QString lastTxPayload;};
 QHash<QString,PartnerMemoryEntry> m_partnerMemory;
 static constexpr qint64 kPartnerMemoryWindowMs=30000,kPartnerMemoryPruneMs=60000;
 qint64 m_partnerMemoryLastPruneMs=0;
 bool m_ft2PartnerMemoryEnabled=false,m_ft2Conservative=false,m_transmitting=false,m_resumeQsoOnReply=false,m_ft2Tx2ResendOnStall=false,allowed=true;
 int m_currentTx=0,m_qsoProgress=0,m_currentPartnerSnrDb=127,m_ft2Tx2ResendsThisQso=0,m_ft2LastForcedTxBefore=0,m_periodTicksMax=10,m_txWatchdogTicks=0,progressSignals=0,clearCalls=0;
 QString m_dxCall,m_mode,m_lastTransmittedMessage;
 void clearResumeQsoOnReplyArm(const QString&){++clearCalls;} void clearPendingAutoSeqTx(const QString&){++clearCalls;}
 bool selectCurrentTxIfAllowed(int n,const QString&){if(!allowed)return false;m_currentTx=n;return true;}
 void resumeQsoOnReplyChanged(){} void ft2PartnerMemoryEnabledChanged(){} void ft2Tx2ResendOnStallChanged(){} void qsoProgressChanged(){++progressSignals;}
 void setResumeQsoOnReply(bool); void rememberPartnerStateV2(QString const&); bool tryResumeFromPartnerMemoryV2(QString const&);void prunePartnerMemoryIfDueV2();void setFt2PartnerMemoryEnabled(bool);bool maybeForceTx2ResendOnStall();void setFt2Tx2ResendOnStall(bool);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir tmp;if(!tmp.isValid())return 1;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,tmp.path());DecodiumBridge b;
 b.m_dxCall=" k1abc ";b.m_mode="FT2";b.m_currentTx=3;b.m_qsoProgress=4;b.m_currentPartnerSnrDb=-12;b.rememberPartnerStateV2("off");if(!b.m_partnerMemory.isEmpty())return 2;
 b.setFt2PartnerMemoryEnabled(true);b.rememberPartnerStateV2("not conservative");if(!b.m_partnerMemory.isEmpty())return 3;
 b.m_ft2Conservative=true;b.rememberPartnerStateV2("ok");if(!b.m_partnerMemory.contains("K1ABC"))return 4;
 b.m_transmitting=true;if(b.tryResumeFromPartnerMemoryV2("K1ABC"))return 5;b.m_transmitting=false;
 b.m_qsoProgress=3;b.m_currentPartnerSnrDb=1;if(!b.tryResumeFromPartnerMemoryV2("k1abc")||b.m_qsoProgress!=4||b.m_currentPartnerSnrDb!=-12||b.m_currentTx!=3)return 6;
 b.m_partnerMemory["K1ABC"].lastSeenMs=QDateTime::currentMSecsSinceEpoch()-31000;if(b.tryResumeFromPartnerMemoryV2("K1ABC")||!b.m_partnerMemory.isEmpty())return 7;
 b.rememberPartnerStateV2("prune");b.m_partnerMemory["K1ABC"].lastSeenMs=QDateTime::currentMSecsSinceEpoch()-61000;b.prunePartnerMemoryIfDueV2();if(!b.m_partnerMemory.isEmpty())return 8;
 b.m_ft2Tx2ResendOnStall=true;b.m_qsoProgress=4;b.m_currentTx=3;b.m_txWatchdogTicks=19;if(b.maybeForceTx2ResendOnStall())return 9;b.m_txWatchdogTicks=20;if(!b.maybeForceTx2ResendOnStall()||b.m_currentTx!=2||b.m_qsoProgress!=3||b.m_txWatchdogTicks!=0)return 10;
 b.m_currentTx=3;b.m_qsoProgress=4;b.m_txWatchdogTicks=20;if(b.maybeForceTx2ResendOnStall())return 11;
 b.setResumeQsoOnReply(true);b.setResumeQsoOnReply(false);b.setFt2PartnerMemoryEnabled(false);b.setFt2Tx2ResendOnStall(false);if(b.clearCalls!=2)return 12;
 std::cout<<"partner gates, resume, TX guard, expiry/prune, one-shot resend and settings: OK\n";
}
