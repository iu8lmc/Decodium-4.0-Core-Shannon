#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>
#include <iostream>
#include "bridge_refactor_cabrillo_helpers.inc"
class DecodiumBridge{public:
 QString m_callsign="K1ABC",m_contestExchange,logPath;int errors=0,status=0;QString logAllTxtPath()const{return logPath;}void errorMessage(const QString&){++errors;}void statusMessage(const QString&){++status;}bool exportCabrillo(const QString&);
};
#include "bridge_refactor_cabrillo.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};DecodiumBridge b;b.logPath=dir.filePath("input.txt");check(!b.exportCabrillo("")&&!b.exportCabrillo(dir.filePath("missing.log")),"missing-input");QFile input(b.logPath);check(input.open(QIODevice::WriteOnly),"fixture");input.write("20261107_120000 K1ABC IU8LMC JN70 14.074000MHz FT8\ninvalid line\n20261108_130000 K1ABC W1AW FN31 7.074000MHz FT4\n");input.close();QString output=dir.filePath("nested/test.log");check(b.exportCabrillo(output),"export");QFile f(output);check(f.open(QIODevice::ReadOnly),"read");QByteArray data=f.readAll();check(data.contains("START-OF-LOG: 3.0")&&data.contains("CONTEST: GENERIC-FT")&&data.count("QSO: ")==2&&data.contains("IU8LMC")&&data.contains("W1AW")&&data.contains("END-OF-LOG:"),"content");b.m_contestExchange="TEST-CONTEST";QString second=dir.filePath("second.log");check(b.exportCabrillo(second),"custom-export");QFile c(second);check(c.open(QIODevice::ReadOnly)&&c.readAll().contains("CONTEST: TEST-CONTEST"),"custom-contest");check(!b.exportCabrillo(dir.filePath("input.txt/child.log")),"parent-error");std::cout<<"sha256 "<<QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex()).toStdString()<<'\n';}
