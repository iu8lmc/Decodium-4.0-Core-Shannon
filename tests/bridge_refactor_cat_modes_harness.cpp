#include <QtCore>
#include <cstdio>
class DecodiumBridge {public:
 QMap<QString,QVariant> values; QVariant getSetting(const QString& k,QVariant d)const{return values.value(k,d);}
 QString configuredCatRigMode()const;bool configuredCatRigModeRequestsDataPacket()const;QString rttyRigMode()const;
};
#include "bridge_refactor_cat_modes.inc"
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}puts(name);}
int main(){DecodiumBridge b;
 check(b.configuredCatRigMode()=="USB"&&!b.configuredCatRigModeRequestsDataPacket()&&b.rttyRigMode()=="RTTY-U","defaults");
 for(QString alias:{"Data/Pkt","data-u","USB-D"," DIGU "}){b.values["CATMode"]=alias;check(b.configuredCatRigMode()=="DATA-U"&&b.configuredCatRigModeRequestsDataPacket(),"data-alias");}
 for(QString alias:{"DATA-USB","PKT","PKT-U"}){b.values["CATMode"]=alias;check(b.configuredCatRigMode()=="USB"&&b.configuredCatRigModeRequestsDataPacket(),"packet-only-alias");}
 b.values["CATMode"]=" None ";check(b.configuredCatRigMode().isEmpty()&&!b.configuredCatRigModeRequestsDataPacket(),"none");
 b.values["CATMode"]=1;check(b.configuredCatRigMode()=="DATA-U"&&b.configuredCatRigModeRequestsDataPacket(),"numeric-data");
 b.values["CATMode"]=2;check(b.configuredCatRigMode().isEmpty()&&!b.configuredCatRigModeRequestsDataPacket(),"numeric-none");
 b.values["CATMode"]="unknown";check(b.configuredCatRigMode()=="USB"&&!b.configuredCatRigModeRequestsDataPacket(),"invalid-fallback");
 b.values["RttyRigMode"]=" None ";check(b.rttyRigMode().isEmpty(),"rtty-none");
 b.values["RttyRigMode"]=" RTTY-L ";check(b.rttyRigMode()=="RTTY-L","rtty-custom");
}
