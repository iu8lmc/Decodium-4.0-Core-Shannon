#include <QtCore>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <cstdio>
#ifdef BRIDGE_REFERENCE
#include "bridge_refactor_under_test.inc"
#else
#include "Ft2LinkAccessCrypto.h"
using namespace decodium::ft2_access;
#endif
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);} puts(name);}
int main() {
#ifdef SYNTHETIC_CONFIG
 check(ft2LinkConfiguredSalt()==QByteArray("salt") && ft2LinkConfiguredHash()==QByteArray("hash") && ft2LinkConfiguredIterations()==12345,"configured-values");
#else
 check(ft2LinkConfiguredSalt().isEmpty() && ft2LinkConfiguredHash().isEmpty() && ft2LinkConfiguredIterations()==160000,"default-values");
#endif
 const QByteArray salt("salt");
 check(ft2LinkPbkdf2Sha256("password",salt,1,32).toHex()=="120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b","pbkdf2-round-1");
 check(ft2LinkPbkdf2Sha256("password",salt,2,32).toHex()=="ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43","pbkdf2-round-2");
 check(ft2LinkPbkdf2Sha256("password",salt,4096,40).toHex()=="c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134af7ad98c1b458ce3f","pbkdf2-multiblock");
 check(ft2LinkPbkdf2Sha256(QString::fromUtf8("parolà"),QByteArray::fromHex("0073616c74ff"),3,48).toHex()=="67c3b35ba99f43219b724fc69efc83afec8c6e8d91c50c798fe3f5520a663cf340f8a864d3347a8544484a98868dde18","pbkdf2-utf8-binary-salt");
 check(ft2LinkPbkdf2Sha256("password",salt,2,0).isEmpty(),"pbkdf2-zero-length");
 check(constantTimeEquals("abc","abc") && constantTimeEquals(QByteArray(),QByteArray()),"equal");
 check(!constantTimeEquals("abc","abd") && !constantTimeEquals("abc","ab"),"different");
}
