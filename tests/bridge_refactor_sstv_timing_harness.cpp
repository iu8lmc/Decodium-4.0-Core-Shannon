#include <QtCore>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <cstdio>
namespace decodium::sstv { struct SstvTxTimingConfig {std::uint64_t pttLeadDelayMs=0,pttTailDelayMs=0,pttReleaseRetryMs=0,voxPreKeyMs=0,voxHangMs=0;double voxToneFrequencyHz=0,voxToneLevel=0;}; }
struct Coordinator {bool accept=true;int calls=0;decodium::sstv::SstvTxTimingConfig last;bool updateTimingConfig(const decodium::sstv::SstvTxTimingConfig& c){++calls;last=c;return accept;}};
class DecodiumBridge {public:
 QMap<QString,QVariant> settings; bool active=false;int changed=0;QString status;Coordinator coord,*m_sstvTxCoordinator=&coord;
 QVariant getSetting(const QString& k,QVariant d)const{return settings.value(k,d);}void setSetting(const QString& k,QVariant v){settings[k]=v;}
 bool sstvTxActive()const{return active;}static QString tr(const char*s){return QString::fromUtf8(s);}void statusMessage(QString s){status=s;}void sstvTxTimingChanged(){++changed;}
 int sstvPttLeadMs()const;void setSstvPttLeadMs(int);int sstvPttTailMs()const;void setSstvPttTailMs(int);int sstvPttReleaseRetryMs()const;void setSstvPttReleaseRetryMs(int);
 int sstvVoxPreKeyMs()const;void setSstvVoxPreKeyMs(int);int sstvVoxHangMs()const;void setSstvVoxHangMs(int);
 double sstvVoxToneFrequencyHz()const;void setSstvVoxToneFrequencyHz(double);double sstvVoxToneLevel()const;void setSstvVoxToneLevel(double);bool applySstvTxTimingSettings();
};
#include "bridge_refactor_sstv_timing.inc"
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}puts(name);}
int main(){DecodiumBridge b;
 check(b.sstvPttLeadMs()==200&&b.sstvPttTailMs()==500&&b.sstvPttReleaseRetryMs()==500&&b.sstvVoxPreKeyMs()==750&&b.sstvVoxHangMs()==500&&b.sstvVoxToneFrequencyHz()==1900&&b.sstvVoxToneLevel()==0.5,"defaults");
 b.settings["TxDelay"]=0.35;check(b.sstvPttLeadMs()==350,"legacy-delay-fallback");
 b.setSstvPttLeadMs(9999);b.setSstvPttTailMs(-1);b.setSstvPttReleaseRetryMs(1);b.setSstvVoxPreKeyMs(9999);b.setSstvVoxHangMs(9999);b.setSstvVoxToneFrequencyHz(100);b.setSstvVoxToneLevel(5);
 check(b.sstvPttLeadMs()==5000&&b.sstvPttTailMs()==0&&b.sstvPttReleaseRetryMs()==100&&b.sstvVoxPreKeyMs()==4000&&b.sstvVoxHangMs()==5000&&b.sstvVoxToneFrequencyHz()==300&&b.sstvVoxToneLevel()==1.0&&b.changed==7,"bounds-all-setters");
 int c=b.changed;b.setSstvPttLeadMs(6000);b.setSstvPttTailMs(-50);b.setSstvPttReleaseRetryMs(0);b.setSstvVoxPreKeyMs(5000);b.setSstvVoxHangMs(6000);b.setSstvVoxToneFrequencyHz(10);b.setSstvVoxToneLevel(2);check(b.changed==c,"unchanged-after-clamping");
 b.active=true;b.setSstvPttLeadMs(1);b.setSstvPttTailMs(2);b.setSstvPttReleaseRetryMs(200);b.setSstvVoxPreKeyMs(200);b.setSstvVoxHangMs(200);b.setSstvVoxToneFrequencyHz(1000);b.setSstvVoxToneLevel(0.4);
 check(b.changed==c&&b.sstvPttLeadMs()==5000&&b.sstvPttTailMs()==0&&b.sstvPttReleaseRetryMs()==100&&b.sstvVoxPreKeyMs()==4000&&b.sstvVoxHangMs()==5000&&b.sstvVoxToneFrequencyHz()==300&&b.sstvVoxToneLevel()==1.0&&b.status.contains("Stop SSTV"),"active-tx-rejects-all");
 b.active=false;
#if DECODIUM_HAS_SSTV
 check(b.coord.calls==7&&b.coord.last.pttLeadDelayMs==5000&&b.coord.last.voxToneLevel==1.0,"coordinator-updated");
 b.coord.accept=false;b.setSstvPttLeadMs(1);b.setSstvPttTailMs(2);b.setSstvPttReleaseRetryMs(200);b.setSstvVoxPreKeyMs(200);b.setSstvVoxHangMs(200);b.setSstvVoxToneFrequencyHz(1000);b.setSstvVoxToneLevel(0.4);
 check(b.changed==c&&b.sstvPttLeadMs()==5000&&b.sstvPttTailMs()==0&&b.sstvPttReleaseRetryMs()==100&&b.sstvVoxPreKeyMs()==4000&&b.sstvVoxHangMs()==5000&&b.sstvVoxToneFrequencyHz()==300&&b.sstvVoxToneLevel()==1.0&&b.status.contains("rejected"),"rejected-rollback-all");
 b.m_sstvTxCoordinator=nullptr;check(b.applySstvTxTimingSettings(),"null-coordinator");
#else
 check(b.applySstvTxTimingSettings(),"sstv-disabled");
#endif
 b.setSstvVoxToneFrequencyHz(std::numeric_limits<double>::quiet_NaN());b.setSstvVoxToneLevel(std::numeric_limits<double>::quiet_NaN());check(b.sstvVoxToneFrequencyHz()==1900&&b.sstvVoxToneLevel()==0.5,"nan-fallback");
}
