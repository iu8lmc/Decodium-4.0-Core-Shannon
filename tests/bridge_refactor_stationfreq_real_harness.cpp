#include <QCoreApplication>
#include <QMap>
#include <QRegularExpression>
#include <QVariant>
#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>
#include "Radio.hpp"
#include "models/Bands.hpp"
#include "models/StationList.hpp"
#include <QSettings>
#include <QTemporaryDir>
#include "bridge_refactor_stationfreq_parser.inc"
static void bridgeLog(const QString&){}
class DecodiumBridge{public:
 QString settingsPath;bool failSave=false;int errors=0;QString m_currentCatLogicalBand="2m";qint64 m_currentCatStationOffsetHz=0;double m_frequency=144074000;
 QVariant readSettingFromLegacyIni(const QString&k)const{QSettings s(settingsPath,QSettings::IniFormat);return s.value(k);}void syncSettingToLegacyIni(const QString&k,const QVariant&v){if(!failSave){QSettings s(settingsPath,QSettings::IniFormat);s.setValue(k,v);s.sync();}}void errorMessage(const QString&){++errors;}void statusMessage(const QString&){}void settingValueChanged(const QString&,const QVariant&){}double stationOffsetForFrequencyHz(double)const{for(auto const&s:readSettingFromLegacyIni("stations").value<StationList::Stations>())if(s.band_name_=="2m")return s.offset_;return 0;}
 QVariantList stationFrequencyRows()const;bool addStationFrequencyRow(const QString&,const QVariant&,const QString&);bool updateStationFrequencyRow(int,const QString&,const QVariant&,const QString&);bool deleteStationFrequencyRow(int);
};
#include "bridge_refactor_stationfreq.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};QTemporaryDir dir; qRegisterMetaType<StationList::Stations>(); DecodiumBridge b; b.settingsPath=dir.filePath("station.ini");check(!b.addStationFrequencyRow("bad","1MHz","")&&!b.addStationFrequencyRow("20m","broken",""),"invalid");check(b.addStationFrequencyRow("2m","−130 MHz"," antenna ")&&b.m_currentCatStationOffsetHz==-130000000,"add-offset");check(b.addStationFrequencyRow("20m","100 Hz","dipole"),"second");auto rows=b.stationFrequencyRows();check(rows.size()==2&&rows[0].toMap().value("band").toString()=="20m", "sorted");check(b.addStationFrequencyRow("2m","-129 MHz","new")&&b.stationFrequencyRows().size()==2,"replace");check(b.updateStationFrequencyRow(1,"2m","-128 MHz","new")&&b.m_currentCatStationOffsetHz==-128000000,"update");b.failSave=true;check(!b.deleteStationFrequencyRow(1),"write-failure");b.failSave=false;check(b.deleteStationFrequencyRow(1)&&b.m_currentCatStationOffsetHz==0&&b.stationFrequencyRows().size()==1,"delete");check(!b.updateStationFrequencyRow(9,"20m",0,"")&&!b.deleteStationFrequencyRow(9),"bad-index");}
