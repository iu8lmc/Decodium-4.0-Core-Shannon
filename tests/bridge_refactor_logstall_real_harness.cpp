#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QStringList>
#include <iostream>
static QStringList captured;
void decodiumBridgeAudioLevelLog(const QString&s){captured<<s;}
void decodiumBridgeStartLogUiStallProbe(QObject*,const QString&);
int main(int argc,char**argv){QCoreApplication app(argc,argv);QObject context;decodiumBridgeStartLogUiStallProbe(&context,"SYNTHETIC");QTimer::singleShot(150,&context,[]{QThread::msleep(120);});QTimer::singleShot(4500,&app,&QCoreApplication::quit);app.exec();bool timer=false,events=false,detected=false;for(const auto&s:captured){timer|=s.contains("pausa massima")&&s.contains("SYNTHETIC");events|=s.contains("eventi piu' lunghi")&&s.contains("SYNTHETIC");detected|=s.contains("eventi piu' lunghi")&&!s.contains("nessun evento oltre 60 ms");}std::cout<<"reports|"<<timer<<"|"<<events<<"|"<<captured.size()<<"|"<<detected<<'\n';}
