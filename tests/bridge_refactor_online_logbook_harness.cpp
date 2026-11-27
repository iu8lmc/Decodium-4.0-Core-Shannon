#include <QtCore>
#include <cstdio>
struct Cloud {
    bool enabled=false; QString url, key; int station=-1, tests=0;
    void setEnabled(bool v){enabled=v;} void setApiUrl(QString v){url=v;}
    void setApiKey(QString v){key=v;} void setStationId(int v){station=v;}
    void testApi(){++tests;}
};
struct Qrz {
    bool enabled=false, replace=false; QString key; int tests=0;
    void setEnabled(bool v){enabled=v;} void setApiKey(QString v){key=v;}
    void setReplaceDuplicates(bool v){replace=v;} void testApi(){++tests;}
};
struct Callsign { QString key; void setQrzApiKey(QString v){key=v;} };
class DecodiumBridge { public:
    bool m_cloudlogEnabled=false, m_qrzLogbookEnabled=false, m_qrzLogbookReplaceDuplicates=false;
    QString m_cloudlogUrl, m_cloudlogApiKey, m_qrzLogbookApiKey;
    Cloud* m_cloudlog=nullptr; Qrz* m_qrzLogbook=nullptr; Callsign* m_callsignIntelligence=nullptr;
    int changes=0, stationSetting=1; QString error, status;
    void cloudlogEnabledChanged(){++changes;} void cloudlogUrlChanged(){++changes;}
    void cloudlogApiKeyChanged(){++changes;} void qrzLogbookEnabledChanged(){++changes;}
    void qrzLogbookApiKeyChanged(){++changes;} void qrzLogbookReplaceDuplicatesChanged(){++changes;}
    void errorMessage(QString v){error=v;} void statusMessage(QString v){status=v;}
    QVariant getSetting(const QString&, int) {return stationSetting;}
    void setCloudlogEnabled(bool); void setCloudlogUrl(const QString&); void setCloudlogApiKey(const QString&);
    void testCloudlogApi(); void setQrzLogbookEnabled(bool); void setQrzLogbookApiKey(const QString&);
    void setQrzLogbookReplaceDuplicates(bool); void testQrzLogbookApi();
};
#include "bridge_refactor_under_test.inc"
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);} puts(name);}
int main(){
 DecodiumBridge b; Cloud cloud; Qrz qrz; Callsign callsign;
 b.testCloudlogApi(); check(b.error.contains("mancante"),"cloud-missing-credentials");
 b.setCloudlogUrl("https://example.invalid/api"); b.setCloudlogApiKey("synthetic");
 b.error.clear(); b.testCloudlogApi(); check(b.error.contains("non inizializzato"),"cloud-missing-client");
 int n=b.changes; b.setCloudlogUrl(b.m_cloudlogUrl); b.setCloudlogApiKey(b.m_cloudlogApiKey); b.setCloudlogEnabled(false);
 check(n==b.changes,"cloud-unchanged-no-notify");
 b.m_cloudlog=&cloud; b.setCloudlogEnabled(true); b.setCloudlogUrl("https://other.invalid/api"); b.setCloudlogApiKey("key2");
 check(cloud.enabled && cloud.url==b.m_cloudlogUrl && cloud.key=="key2" && b.changes==n+3,"cloud-settings-propagated");
 b.stationSetting=5000; b.testCloudlogApi(); check(cloud.station==999 && cloud.tests==1 && cloud.key=="key2","cloud-test-upper-bound");
 b.stationSetting=-10; b.testCloudlogApi(); check(cloud.station==0 && cloud.tests==2,"cloud-test-lower-bound");
 b.error.clear(); b.testQrzLogbookApi(); check(b.error.contains("mancante"),"qrz-missing-key");
 b.setQrzLogbookApiKey("key"); b.error.clear(); b.testQrzLogbookApi(); check(b.error.contains("non inizializzato"),"qrz-missing-client");
 b.m_qrzLogbook=&qrz; b.m_callsignIntelligence=&callsign; n=b.changes;
 b.setQrzLogbookEnabled(true); b.setQrzLogbookApiKey("key2"); b.setQrzLogbookReplaceDuplicates(true);
 check(qrz.enabled && qrz.replace && qrz.key=="key2" && callsign.key=="key2" && b.changes==n+3,"qrz-settings-propagated");
 n=b.changes; b.setQrzLogbookEnabled(true); b.setQrzLogbookApiKey("key2"); b.setQrzLogbookReplaceDuplicates(true);
 check(b.changes==n,"qrz-unchanged-no-notify");
 b.testQrzLogbookApi(); check(qrz.tests==1 && qrz.key=="key2" && b.status.contains("test in corso"),"qrz-test");
 b.setQrzLogbookApiKey("  "); b.error.clear(); b.testQrzLogbookApi(); check(b.error.contains("mancante") && qrz.tests==1,"qrz-whitespace-key");
}
