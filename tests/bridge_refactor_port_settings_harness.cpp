#include <QCoreApplication>
#include <QSettings>
#include <QString>
#include <iostream>
#include "DecodiumProfileSettings.h"
void bridgeLog(QString const&){}
class DecodiumBridge {public: bool decoPortAutoStart()const; bool ft2DriftSearchEnabled()const; void setFt2DriftSearchEnabled(bool);void setDecoPortAutoStart(bool);int decoPortConfiguredPort()const;};
#include "method.inc"
void check(bool x){if(!x)abort();}
int main(int n,char**v){QCoreApplication app(n,v);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,QString::fromLocal8Bit(qgetenv("PORT_LAB_SETTINGS")));QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");s.clear();s.sync();DecodiumBridge b;
 check(!b.decoPortAutoStart()&&b.ft2DriftSearchEnabled()&&b.decoPortConfiguredPort()==5559);b.setDecoPortAutoStart(true);b.setFt2DriftSearchEnabled(false);check(b.decoPortAutoStart()&&!b.ft2DriftSearchEnabled());s.setValue("DecoPortPort",1023);s.sync();check(b.decoPortConfiguredPort()==5559);s.setValue("DecoPortPort",65535);s.sync();check(b.decoPortConfiguredPort()==65535);s.setValue("DecoPortPort",65536);s.sync();check(b.decoPortConfiguredPort()==5559);
 app.setProperty("decodiumConfigName","LAB");b.setDecoPortAutoStart(false);b.setFt2DriftSearchEnabled(true);check(!b.decoPortAutoStart()&&b.ft2DriftSearchEnabled());app.setProperty("decodiumConfigName",QString());check(b.decoPortAutoStart()&&!b.ft2DriftSearchEnabled());std::cout<<"defaults, port bounds, root and profile persistence passed\n";}
