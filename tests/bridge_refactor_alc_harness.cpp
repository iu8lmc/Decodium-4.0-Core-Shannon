#include <QCoreApplication>
#include <QDateTime>
#include <QSettings>
#include <QTimer>
#include <QString>
#include <iostream>
#include "AlcCalibrationMath.h"
#define emit
void bridgeLog(const QString&){}
namespace decodium {void beginActiveSettingsProfile(QSettings&) {}}
struct Cat {bool online=false;bool connected()const{return online;}};
class DecodiumBridge:public QObject {public:
 int m_alcTarget=20,m_alcConvergeCount=0,m_alcStuckCount=0;bool m_alcCalibrating=false,m_transmitting=false,m_tuning=false,m_manualTxHold=false,m_rigAlcValid=false;QString m_catBackend="none",m_alcCalStatus;Cat *m_hamlibCat=nullptr;double m_txOutputLevel=30,m_alcCalStartLevel=0,m_alcEma=0,m_rigAlc=0;qint64 m_alcCalStartMs=0;QTimer *m_alcCalTimer=nullptr;int tunes=0,stops=0,notices=0;
 void alcTargetChanged(){++notices;}void alcCalibrationStatusChanged(){++notices;}void alcCalibratingChanged(){++notices;}void startTune(){++tunes;}void stopTune(){++stops;}void setTxOutputLevel(double d){m_txOutputLevel=d;}
 void setAlcTarget(int);void startAlcCalibration();void onAlcCalibrationTick();void finishAlcCalibration(bool,const QString&);void cancelAlcCalibration();};
#include "method.inc"
void check(bool v){if(!v)abort();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,QString::fromLocal8Bit(qgetenv("ALC_LAB_SETTINGS")));DecodiumBridge b;Cat cat;b.m_hamlibCat=&cat;
 b.setAlcTarget(-10);check(b.m_alcTarget==5);b.setAlcTarget(90);check(b.m_alcTarget==60);b.setAlcTarget(20);
 b.startAlcCalibration();check(!b.m_alcCalibrating && b.tunes==0 && b.m_alcCalStatus.contains("not available"));
 b.m_catBackend="hamlib";cat.online=true;b.m_transmitting=true;b.startAlcCalibration();check(!b.m_alcCalibrating && b.tunes==0);b.m_transmitting=false;
 b.startAlcCalibration();check(b.m_alcCalibrating && b.tunes==1);b.m_rigAlcValid=true;b.m_rigAlc=20;b.onAlcCalibrationTick();b.onAlcCalibrationTick();check(!b.m_alcCalibrating && b.stops==1 && b.m_alcCalStatus.startsWith("Calibration done"));
 b.m_txOutputLevel=30;b.startAlcCalibration();b.m_txOutputLevel=10;b.cancelAlcCalibration();check(!b.m_alcCalibrating && b.m_txOutputLevel==30 && b.m_alcCalStatus.contains("cancelled"));
 b.startAlcCalibration();b.m_alcCalStartMs=QDateTime::currentMSecsSinceEpoch()-13000;b.onAlcCalibrationTick();check(!b.m_alcCalibrating && b.m_alcCalStatus.contains("timeout"));
 b.m_rigAlcValid=false;b.startAlcCalibration();b.m_alcCalStartMs=QDateTime::currentMSecsSinceEpoch()-3000;b.onAlcCalibrationTick();check(!b.m_alcCalibrating && b.m_alcCalStatus.contains("not reported"));
 b.onAlcCalibrationTick();check(!b.m_alcCalibrating);std::cout<<"ALC guards, target limits, convergence, cancel, timeout and missing telemetry passed\n";}
