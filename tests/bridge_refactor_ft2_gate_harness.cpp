#include <QtCore>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <cstdio>
#ifdef BRIDGE_REFERENCE
#include "bridge_refactor_under_test.inc"
#else
#include "Ft2LinkAccessCrypto.h"
#endif
static constexpr int kFt2LinkAccessSaltBytes=16, kFt2LinkAccessHashBytes=32;
static void bridgeLog(const QString&) {}
static void decodiumBridgeLogbookLog(const QString&) {}
class DecodiumBridge { public:
 bool m_ft2LinkAccessUnlocked=false; int changes=0;
 void ft2LinkAccessChanged(){++changes;}
 bool ft2LinkAccessUnlocked() const; bool ft2LinkAccessPasswordConfigured() const;
 bool verifyFt2LinkAccessPassword(const QString&); void lockFt2LinkAccess();
};
#include "bridge_refactor_gate_under_test.inc"
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);} puts(name);}
int main(){
 DecodiumBridge b;
#ifdef VALID_GATE
 check(b.ft2LinkAccessPasswordConfigured(),"configured");
 check(!b.verifyFt2LinkAccessPassword("wrong") && !b.ft2LinkAccessUnlocked() && b.changes==0,"wrong-password");
 check(b.verifyFt2LinkAccessPassword("synthetic-password") && b.ft2LinkAccessUnlocked() && b.changes==1,"correct-password");
 check(b.verifyFt2LinkAccessPassword("synthetic-password") && b.changes==1,"repeat-password");
 check(!b.verifyFt2LinkAccessPassword("wrong") && b.ft2LinkAccessUnlocked() && b.changes==1,"wrong-after-unlock");
 b.lockFt2LinkAccess(); check(!b.ft2LinkAccessUnlocked() && b.changes==2,"lock");
 b.lockFt2LinkAccess(); check(b.changes==2,"repeat-lock");
#else
 check(!b.ft2LinkAccessPasswordConfigured(),"not-configured");
 check(!b.verifyFt2LinkAccessPassword("synthetic-password") && !b.ft2LinkAccessUnlocked() && b.changes==0,"not-configured-rejects");
 b.lockFt2LinkAccess(); check(b.changes==0,"not-configured-lock");
#endif
}
