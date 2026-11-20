#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>
#include "profile_under_test.h"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");s.setValue("GlobalValue",17);s.setValue("CW/monitorVolume",35);s.sync();app.setProperty("decodiumConfigName","First");decodium::ensureSettingsProfileInitialized("First");s.setValue("First/OnlyFirst",99);s.sync();app.setProperty("decodiumConfigName","Second");decodium::ensureSettingsProfileInitialized("Second");s.sync();std::cout<<"global|"<<s.value("Second/GlobalValue").toInt()<<"|"<<s.value("Second/CW/monitorVolume").toInt()<<'\n';std::cout<<"copied-other-profile|"<<s.contains("Second/First/OnlyFirst")<<'\n';std::cout<<"first-intact|"<<s.value("First/OnlyFirst").toInt()<<'\n';}
