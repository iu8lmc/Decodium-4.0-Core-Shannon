#include <QtCore>
#include <cstdio>
namespace decodium { static void beginActiveSettingsProfile(QSettings& s){s.beginGroup(QStringLiteral("test-profile"));} }
static QThreadPool& settingsWriteThreadPool(){static QThreadPool p;p.setMaxThreadCount(1);return p;}
static QThreadPool& decodiumBridgeSettingsWriteThreadPool(){return settingsWriteThreadPool();}
static void bridgeLog(const QString&){} static void decodiumBridgeWindowStateLog(const QString&){}
class DecodiumBridge {public:
 int resets=0; void windowLayoutResetRequested(){++resets;}
 QVariantMap loadWindowState(const QString&)const;
 void saveWindowState(const QString&,int,int,int,int,bool,bool);
 void saveWindowStatesAsync(const QVariantMap&,const QVariantMap&);
 void resetWindowLayout();
};
#include "bridge_refactor_window_state.inc"
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}puts(name);}
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);QTemporaryDir dir;check(dir.isValid(),"isolated-settings");QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());DecodiumBridge b;
 check(b.loadWindowState(" ").isEmpty()&&b.loadWindowState("missing").isEmpty(),"empty-and-missing");
 b.saveWindowState("  waterfall ",-40,60,800,400,true,false); auto state=b.loadWindowState("waterfall");
 check(state.value("x")==-40&&state.value("y")==60&&state.value("width")==800&&state.value("height")==400&&state.value("detached").toBool()&&!state.value("minimized").toBool(),"sync-roundtrip");
 b.saveWindowState(" ",1,1,1,1,false,false);check(b.loadWindowState("").isEmpty(),"ignore-empty-key");
 QVariantMap partial{{"x",100},{"maximized",true},{"unrelated",99}};
 b.saveWindowStatesAsync(QVariantMap{{" waterfall ",partial},{" empty ",QVariantMap{}},{"  ",partial}},QVariantMap{{" layout ","test"},{" ",42}});
 check(settingsWriteThreadPool().waitForDone(5000),"async-finished");state=b.loadWindowState("waterfall");
 check(state.value("x")==100&&state.value("width")==800&&state.value("maximized").toBool()&&!state.contains("unrelated"),"async-partial-preserves-fields");
 {QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);check(s.value("layout")=="test" && !s.contains("WindowState/empty/x"),"layout-and-empty-ignored");}
 b.saveWindowStatesAsync({},{});check(settingsWriteThreadPool().waitForDone(5000),"empty-batch");
 b.resetWindowLayout();check(b.resets==1&&b.loadWindowState("waterfall").isEmpty(),"reset-removes-window-state");
 {QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);check(s.value("layout")=="test","reset-preserves-other-settings");}
}
