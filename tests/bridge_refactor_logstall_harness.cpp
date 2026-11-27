#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QThread>
#include <QTimer>
#include <QStringList>
#include <QVector>
#include <algorithm>
#include <memory>
#include <utility>
#include <iostream>
static QStringList captured;static void bridgeLog(const QString&s){captured<<s;}
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QObject context;startLogUiStallProbe(&context,"SYNTHETIC");QTimer::singleShot(150,&context,[]{QThread::msleep(120);});QTimer::singleShot(4500,&app,&QCoreApplication::quit);app.exec();bool timer=false,events=false,detected=false;for(const auto&s:captured){timer|=s.contains("pausa massima")&&s.contains("SYNTHETIC");events|=s.contains("eventi piu' lunghi")&&s.contains("SYNTHETIC");detected|=s.contains("eventi piu' lunghi")&&!s.contains("nessun evento oltre 60 ms");}std::cout<<"reports|"<<timer<<"|"<<events<<"|"<<captured.size()<<"|"<<detected<<'\n';}
