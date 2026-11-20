#include <QCoreApplication>
#include <QMap>
#include <QVariant>
#include <QtMath>
#include <cmath>
#include <limits>
#include <iostream>
namespace Radio {using Frequency=quint64;using FrequencyDelta=qint64;}
struct Bands {QString find(Radio::Frequency hz)const{if(hz>=14000000&&hz<14350000)return "20m";if(hz>=144000000&&hz<148000000)return "2m";return {};}};
struct StationList {struct Station{QString band_name_;Radio::FrequencyDelta offset_;};using Stations=QList<Station>;};
Q_DECLARE_METATYPE(StationList::Stations)
static void bridgeLog(const QString&){}
class DecodiumBridge {public:
 QMap<QString,QVariant> settings;QString m_currentCatLogicalBand;qint64 m_currentCatStationOffsetHz=0;
 QVariant readSettingFromLegacyIni(const QString&k)const{return settings.value(k);}void syncSettingToLegacyIni(const QString&k,const QVariant&v){settings[k]=v;}void settingValueChanged(const QString&,const QVariant&){}
 double frequencyCalibrationSlopePpm()const;double frequencyCalibrationInterceptHz()const;void setFrequencyCalibrationSlopePpm(double);void setFrequencyCalibrationInterceptHz(double);double stationOffsetForFrequencyHz(double)const;double logicalFrequencyFromCatDial(double,QString*,qint64*)const;double applyFrequencyCalibration(double)const;
};
#include "bridge_refactor_frequency_calibration.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};DecodiumBridge b;check(b.applyFrequencyCalibration(14074000)==14074000,"zero-calibration");b.setFrequencyCalibrationSlopePpm(999);b.setFrequencyCalibrationInterceptHz(-2000);check(b.frequencyCalibrationSlopePpm()==200&&b.frequencyCalibrationInterceptHz()==-1000,"clamp");b.setFrequencyCalibrationSlopePpm(10);b.setFrequencyCalibrationInterceptHz(20);StationList::Stations stations{{"2m",-130000000}};b.settings["stations"]=QVariant::fromValue(stations);check(b.stationOffsetForFrequencyHz(144074000)==-130000000&&b.stationOffsetForFrequencyHz(14074000)==0,"station-offset");double dial=b.applyFrequencyCalibration(144074000);QString band;qint64 offset=0;double logical=b.logicalFrequencyFromCatDial(dial,&band,&offset);check(std::abs(logical-144074000)<0.01&&band=="2m"&&offset==-130000000,"inferred-inverse");b.m_currentCatLogicalBand="2m";b.m_currentCatStationOffsetHz=-130000000;logical=b.logicalFrequencyFromCatDial(dial,&band,&offset);check(std::abs(logical-144074000)<0.01&&band=="2m","active-inverse");check(b.logicalFrequencyFromCatDial(-1,&band,&offset)==0&&band.isEmpty()&&offset==0,"invalid-input");b.settings.remove("stations");b.m_currentCatLogicalBand.clear();check(std::abs(b.logicalFrequencyFromCatDial(b.applyFrequencyCalibration(14074000),&band,&offset)-14074000)<0.01&&band=="20m","ordinary-inverse");}
