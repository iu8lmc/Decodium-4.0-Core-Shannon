#include <QCoreApplication>
#include <QMap>
#include <QRegularExpression>
#include <QVariant>
#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>
namespace Radio {using FrequencyDelta=qint64;QString pretty_frequency_MHz_string(FrequencyDelta hz){return QString::number(hz/1000000.0,'f',6);}}
struct Bands{int find(const QString&band)const{if(band.compare("20m",Qt::CaseInsensitive)==0)return 0;if(band.compare("2m",Qt::CaseInsensitive)==0)return 1;return -1;}};
struct StationList {struct Station{QString band_name_;Radio::FrequencyDelta offset_;QString antenna_description_;bool operator==(const Station&rhs)const{return band_name_==rhs.band_name_&&offset_==rhs.offset_&&antenna_description_==rhs.antenna_description_;}};using Stations=QList<Station>;};
Q_DECLARE_METATYPE(StationList::Stations)
#include "bridge_refactor_stationfreq_parser.inc"
static void bridgeLog(const QString&){}
class DecodiumBridge{public:
 QVariant data;bool failSave=false;int errors=0;QString m_currentCatLogicalBand="2m";qint64 m_currentCatStationOffsetHz=0;double m_frequency=144074000;
 QVariant readSettingFromLegacyIni(const QString&)const{return data;}void syncSettingToLegacyIni(const QString&,const QVariant&v){if(!failSave)data=v;}void errorMessage(const QString&){++errors;}void statusMessage(const QString&){}void settingValueChanged(const QString&,const QVariant&){}double stationOffsetForFrequencyHz(double)const{for(auto const&s:data.value<StationList::Stations>())if(s.band_name_=="2m")return s.offset_;return 0;}
 QVariantList stationFrequencyRows()const;bool addStationFrequencyRow(const QString&,const QVariant&,const QString&);bool updateStationFrequencyRow(int,const QString&,const QVariant&,const QString&);bool deleteStationFrequencyRow(int);
};
#include "bridge_refactor_stationfreq.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};DecodiumBridge b;check(!b.addStationFrequencyRow("bad","1MHz","")&&!b.addStationFrequencyRow("20m","broken",""),"invalid");check(b.addStationFrequencyRow("2m","−130 MHz"," antenna ")&&b.m_currentCatStationOffsetHz==-130000000,"add-offset");check(b.addStationFrequencyRow("20m","100 Hz","dipole"),"second");auto rows=b.stationFrequencyRows();check(rows.size()==2&&rows[0].toMap().value("band").toString()=="20m", "sorted");check(b.addStationFrequencyRow("2m","-129 MHz","new")&&b.stationFrequencyRows().size()==2,"replace");check(b.updateStationFrequencyRow(1,"2m","-128 MHz","new")&&b.m_currentCatStationOffsetHz==-128000000,"update");b.failSave=true;check(!b.deleteStationFrequencyRow(1),"write-failure");b.failSave=false;check(b.deleteStationFrequencyRow(1)&&b.m_currentCatStationOffsetHz==0&&b.stationFrequencyRows().size()==1,"delete");check(!b.updateStationFrequencyRow(9,"20m",0,"")&&!b.deleteStationFrequencyRow(9),"bad-index");}
