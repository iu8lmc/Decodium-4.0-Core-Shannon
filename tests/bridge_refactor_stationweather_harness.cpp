#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QVariant>
#include <functional>
#include <iostream>
#include "StationTelemetryCodec.hpp"
static QString lastLog;static int requests=0;static QUrl lastUrl;static QByteArray response;static bool networkError=false;static std::function<void()> finished;
static void bridgeLog(const QString&s){lastLog=s;}
static bool grid2deg(const QString&s,double&lon,double&lat){if(s!="JM75")return false;lon=14.5;lat=35.9;return true;}
#define QNetworkReply SyntheticNetworkReply
#define QNetworkAccessManager SyntheticNetworkAccessManager
class QNetworkReply {public:enum NetworkError{NoError,Failed};void finished(){}void deleteLater(){}NetworkError error()const{return networkError?Failed:NoError;}QString errorString()const{return "synthetic error";}QByteArray readAll(){return response;}};
class QNetworkAccessManager {public:explicit QNetworkAccessManager(QObject*){}void deleteLater(){}QNetworkReply*get(const QNetworkRequest&r){++requests;lastUrl=r.url();return new QNetworkReply;}};
class DecodiumBridge:public QObject {public:
 bool off=false,enabled=false;QString m_grid="JM75",status;int m_weatherTempC=0,m_weatherWindKmh=0,m_weatherWindDirDeg=0;qint64 m_weatherFetchedAtMs=0;decodium::telemetry::SkyCondition m_weatherSky=decodium::telemetry::SkyCondition::Unknown;
 bool offlineMode()const{return off;}QVariant getSetting(const QString&,const QVariant&)const{return enabled;}void statusMessage(const QString&s){status=s;}
 template<class A,class B,class C,class F>void connect(A,B,C,F f){finished=f;}
 void fetchWeatherForGrid();QVariantMap currentWeatherPreview()const;
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;b.off=true;b.enabled=true;b.fetchWeatherForGrid();b.off=false;b.enabled=false;b.fetchWeatherForGrid();b.enabled=true;b.m_grid="bad";b.fetchWeatherForGrid();std::cout<<"guards|"<<requests<<"|"<<b.currentWeatherPreview().value("available").toBool()<<'\n';b.m_grid="JM75";b.fetchWeatherForGrid();QUrlQuery query(lastUrl);std::cout<<"request|"<<requests<<"|"<<(lastUrl.host()=="api.open-meteo.com")<<"|"<<(query.queryItemValue("latitude")=="35.9000")<<"|"<<(query.queryItemValue("longitude")=="14.5000")<<'\n';networkError=true;finished();std::cout<<"network-error|"<<b.m_weatherFetchedAtMs<<"|"<<lastLog.contains("errore rete")<<'\n';networkError=false;response="invalid";b.fetchWeatherForGrid();finished();std::cout<<"json-error|"<<b.m_weatherFetchedAtMs<<"|"<<lastLog.contains("JSON non valido")<<'\n';response=R"({"current":{"temperature_2m":19.2,"wind_speed_10m":22.4,"wind_direction_10m":90,"weather_code":95}})";b.fetchWeatherForGrid();finished();auto v=b.currentWeatherPreview();std::cout<<"weather|"<<v.value("available").toBool()<<"|"<<v.value("tempC").toInt()<<"|"<<v.value("windKmh").toInt()<<"|"<<(b.m_weatherSky==decodium::telemetry::SkyCondition::Thunderstorm)<<"|"<<!b.status.isEmpty()<<'\n';b.m_weatherFetchedAtMs-=31*60*1000;std::cout<<"stale|"<<b.currentWeatherPreview().value("available").toBool()<<'\n';}
