#include <QCoreApplication>
#include <QDebug>
#include <QPointer>
#include <QTextStream>
class PanadapterItem:public QObject{};
class DecodiumBridge{public:QList<QPointer<PanadapterItem>>m_panadapterItems;void registerPanadapterItem(PanadapterItem*);void unregisterPanadapterItem(PanadapterItem*);};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);DecodiumBridge b;auto*a=new PanadapterItem;auto*c=new PanadapterItem;b.registerPanadapterItem(nullptr);b.registerPanadapterItem(a);b.registerPanadapterItem(a);b.registerPanadapterItem(c);o<<"register|"<<b.m_panadapterItems.size()<<'\n';delete a;o<<"destroyed|"<<(b.m_panadapterItems.first().isNull())<<'\n';b.registerPanadapterItem(c);o<<"pruned|"<<b.m_panadapterItems.size()<<'\n';b.unregisterPanadapterItem(c);o<<"removed|"<<b.m_panadapterItems.size()<<'\n';delete c;}
