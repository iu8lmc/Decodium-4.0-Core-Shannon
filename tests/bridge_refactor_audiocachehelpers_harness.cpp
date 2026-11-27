#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>
class QAudioDevice{QString n,id;public:QAudioDevice(QString name={},QString value={}):n(name),id(value){}QString description()const{return n;}QString idForTest()const{return id;}};
static QString audioDeviceIdForLog(const QAudioDevice& d){return d.idForTest().isEmpty()?QStringLiteral("<empty>"):d.idForTest();}
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);QAudioDevice a(" Mic ","01"),b("mic","02"),c("Speaker","03"),empty(" ","");QList<QAudioDevice> inputs{a,b,empty};QList<QAudioDevice> outputs{c};o<<"names|"<<audioDeviceDescriptions(inputs).join(";")<<'|'<<audioDeviceDescriptions(outputs).join(";")<<'\n';o<<"signature|"<<audioDeviceCacheSignature(inputs,outputs,a,c).replace('\n',';')<<'\n';o<<"empty|"<<audioDeviceDescriptions({}).size()<<'|'<<audioDeviceCacheSignature({}, {}, empty, empty).replace('\n',';')<<'\n';}
