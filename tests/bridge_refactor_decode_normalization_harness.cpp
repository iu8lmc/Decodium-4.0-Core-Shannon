#include <QtCore>
#include <cstdio>
#ifdef BRIDGE_REFERENCE
#include "bridge_refactor_decode_under_test.inc"
#else
#include "DecodeMessageNormalization.h"
using namespace decodium::decode_message;
#endif
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);} puts(name);}
int main(){
 check(stripLegacyDecodeAppendage("  CQ IU8LMC JM75  ")=="CQ IU8LMC JM75","trim");
 check(stripLegacyDecodeAppendage(" TU; CQ IU8LMC JM75 ")=="CQ IU8LMC JM75","remove-tu");
 check(stripLegacyDecodeAppendage(QString::fromUtf8("CQ K1ABC FN31\u00a0extra"))=="CQ K1ABC FN31","nbsp-cut");
 check(stripLegacyDecodeAppendage("123456 -12 0.4 1000 ~ CQ K1ABC FN31  extra") == "123456 -12 0.4 1000 ~ CQ K1ABC FN31","header-annotation");
 check(stripLegacyDecodeAppendage("1234 +05 -0.2 500 # K1ABC W1XYZ RR73  trailing") == "1234 +05 -0.2 500 # K1ABC W1XYZ RR73","short-header");
 check(stripLegacyDecodeAppendage("plain  double  spaces")=="plain  double  spaces","plain-preserved");
 QString a; check(stripDecodeApAnnotation(" CQ K1ABC FN31 a12 ",&a)=="CQ K1ABC FN31" && a=="12","ap-captured");
 a="42";check(stripDecodeApAnnotation("text a3",&a)=="text" && a=="42","ap-existing-preserved");
 check(stripDecodeApAnnotation("text aX")=="text aX" && stripDecodeApAnnotation("text a5 trailing")=="text a5 trailing","non-suffix-preserved");
 check(canonicalDecodeMessage("  CQ   K1ABC    FN31  a1   ")=="CQ K1ABC FN31","canonical-annotation");
 check(canonicalDecodeMessage("").isEmpty() && stripDecodeApAnnotation("").isEmpty(),"empty");
}
