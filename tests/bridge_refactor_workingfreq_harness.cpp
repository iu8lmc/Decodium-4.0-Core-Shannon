#include <QCoreApplication>
#include <QBuffer>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <iostream>
#include "Radio.hpp"
#include "models/Bands.hpp"
#include "models/FrequencyList.hpp"
#include "models/IARURegions.hpp"
#include "models/Modes.hpp"
static void bridgeLog(const QString&){}
static QString canonicalApplicationDecodeMode(QString mode){return mode.trimmed().toUpper();}
static bool isRadioOnlyModeLabel(QString mode){return mode.trimmed().toUpper()=="USB";}
#include "bridge_refactor_workingfreq_helpers.inc"
class DecodiumBridge:public QObject{public:
 QMap<QString,QVariant> settings;QString m_mode="FT8";double qsyHz=0;int errors=0;QVariant readSettingFromLegacyIni(const QString&k)const{return settings.value(k);}void syncSettingToLegacyIni(const QString&k,const QVariant&v){settings[k]=v;}void errorMessage(const QString&){++errors;}void statusMessage(const QString&){}void settingValueChanged(const QString&,const QVariant&){}QString startupModeForFrequency(double)const{return "FT8";}void requestRigFrequencyFromBridge(double f,const QString&){qsyHz=f;}void setMode(const QString&m){m_mode=m;}
 QVariantList workingFrequencyRows()const;QStringList frequencyRegionOptions()const;QStringList frequencyModeOptions()const;QStringList frequencyBandOptions()const;double workingFrequencyForBandMode(const QString&,const QString&)const;bool addWorkingFrequencyRow(const QString&,const QString&,const QVariant&,const QString&,const QString&,const QString&,bool);bool updateWorkingFrequencyRow(int,const QString&,const QString&,const QVariant&,const QString&,const QString&,const QString&,bool);bool deleteWorkingFrequencyRow(int);bool setWorkingFrequencyPreferred(int,bool);void resetWorkingFrequenciesToDefaults();bool loadWorkingFrequenciesFile(const QString&,bool);bool saveWorkingFrequenciesFile(const QString&);void qsyToWorkingFrequency(int);
};
#include "bridge_refactor_workingfreq_methods.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};DecodiumBridge b;auto initial=b.workingFrequencyRows();check(!initial.isEmpty()&&!b.frequencyRegionOptions().isEmpty()&&b.frequencyModeOptions().contains("FT8")&&!b.frequencyBandOptions().isEmpty(),"defaults-and-options");check(!b.addWorkingFrequencyRow("All","FT8","not-frequency","bad","","",false),"invalid-input");auto first=initial.first().toMap();QString region=first.value("region").toString(),mode="FT8";check(b.addWorkingFrequencyRow(region,mode,"14.123456 MHz","SYNTHETIC-ROW","","",true),"add");auto rows=b.workingFrequencyRows();int idx=-1;for(int i=0;i<rows.size();++i)if(rows[i].toMap().value("description")=="SYNTHETIC-ROW")idx=i;check(idx>=0&&rows[idx].toMap().value("frequencyHz").toLongLong()==14123456,"read-row");b.qsyToWorkingFrequency(idx);check(b.qsyHz==14123456,"qsy-simulated");check(b.updateWorkingFrequencyRow(idx,region,mode,"14.123457 MHz","SYNTHETIC-EDIT","","",false),"update");rows=b.workingFrequencyRows();idx=-1;for(int i=0;i<rows.size();++i)if(rows[i].toMap().value("description")=="SYNTHETIC-EDIT")idx=i;check(idx>=0&&b.setWorkingFrequencyPreferred(idx,true),"preferred");QString file=dir.filePath("frequencies.qrg");check(b.saveWorkingFrequenciesFile(file)&&QFileInfo(file).size()>0,"export-qrg");DecodiumBridge c;check(c.loadWorkingFrequenciesFile(file,false),"import-qrg");bool found=false;for(auto v:c.workingFrequencyRows())if(v.toMap().value("description")=="SYNTHETIC-EDIT")found=true;check(found,"roundtrip");QString json=dir.filePath("frequencies.qrg.json");check(b.saveWorkingFrequenciesFile(json),"export-json");DecodiumBridge d;check(d.loadWorkingFrequenciesFile(json,false),"import-json");QFile corrupt(dir.filePath("bad.qrg"));check(corrupt.open(QIODevice::WriteOnly),"corrupt-fixture");corrupt.write("not-a-qrg");corrupt.close();check(!d.loadWorkingFrequenciesFile(corrupt.fileName(),false),"reject-corrupt");check(b.deleteWorkingFrequencyRow(idx),"delete");check(!b.deleteWorkingFrequencyRow(-1),"invalid-index");b.resetWorkingFrequenciesToDefaults();check(!b.workingFrequencyRows().isEmpty(),"reset");}
