#include <QtCore>
#include <cstdio>
class DecodiumBridge { public:
QVariantMap readTextFile(const QString&,int) const;
QVariantMap writeTextFile(const QString&,const QString&) const;
QVariantMap readFileBytes(const QString&,int) const;
QVariantMap writeFileBytes(const QString&,const QString&) const;
};
#include "bridge_refactor_under_test.inc"
static void check(bool condition, const char* name) { if(!condition) { fprintf(stderr,"FAIL %s\n",name); exit(1); } puts(name); }
int main() {
 QTemporaryDir dir; check(dir.isValid(),"temp-dir"); DecodiumBridge b;
 auto e=b.readTextFile("",10); check(!e["ok"].toBool() && e["error"].toString()=="Missing file path","text-empty-path");
 auto p=dir.filePath("a/sub/data.txt"); auto w=b.writeTextFile(p,QString::fromUtf8("Ciao π")); check(w["ok"].toBool() && w["bytes"].toInt()==7,"text-write-nested-utf8");
 auto r=b.readTextFile(QUrl::fromLocalFile(p).toString(),100); check(r["ok"].toBool() && r["text"].toString()==QString::fromUtf8("Ciao π") && !r["truncated"].toBool(),"text-read-url");
 r=b.readTextFile(p,4); check(r["text"].toString()=="Ciao" && r["truncated"].toBool() && r["fileSize"].toInt()==7,"text-truncate");
 r=b.readTextFile(p,0); check(r["bytes"].toInt()==1 && r["truncated"].toBool(),"text-min-limit");
 check(!b.readTextFile(dir.filePath("missing"),5)["ok"].toBool(),"text-missing");
 check(!b.writeTextFile("","x")["ok"].toBool(),"text-write-empty");
 auto bp=dir.filePath("b/raw.bin"); QByteArray bytes=QByteArray::fromHex("0001ff800a");
 w=b.writeFileBytes(QUrl::fromLocalFile(bp).toString(),QString::fromLatin1(bytes.toBase64())); check(w["ok"].toBool() && w["bytes"].toInt()==5,"bytes-write-url");
 r=b.readFileBytes(bp,100); check(r["ok"].toBool() && QByteArray::fromBase64(r["base64"].toString().toLatin1())==bytes && !r["truncated"].toBool(),"bytes-roundtrip");
 r=b.readFileBytes(bp,2); check(r["bytes"].toInt()==2 && r["truncated"].toBool() && r["fileSize"].toInt()==5,"bytes-truncate");
 check(!b.readFileBytes("",1)["ok"].toBool(),"bytes-empty-path");
 check(!b.readFileBytes(dir.filePath("missing"),1)["ok"].toBool(),"bytes-missing");
 check(!b.writeFileBytes("",QStringLiteral("YQ=="))["ok"].toBool(),"bytes-write-empty-path");
 check(!b.writeFileBytes(bp,"")["ok"].toBool(),"bytes-write-empty-data");
 r=b.readFileBytes(bp,100); check(QByteArray::fromBase64(r["base64"].toString().toLatin1())==bytes,"failed-write-preserves-data");
 auto big=dir.filePath("big.txt"); check(b.writeTextFile(big,QString(70000,QLatin1Char('x')))["ok"].toBool(),"large-write");
 r=b.readTextFile(big,100000); check(r["bytes"].toInt()==65536 && r["fileSize"].toInt()==70000 && r["truncated"].toBool(),"text-max-limit");
 r=b.readFileBytes(big,100000); check(r["bytes"].toInt()==65536 && r["truncated"].toBool(),"bytes-max-limit");
 check(b.writeTextFile(p,"")["ok"].toBool() && b.readTextFile(p,100)["bytes"].toInt()==0,"text-overwrite-empty");
}
