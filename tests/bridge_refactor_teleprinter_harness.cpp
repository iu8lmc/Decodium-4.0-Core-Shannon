#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVariantMap>
#include <QtMath>
#include <cassert>
#include <iostream>

class DecodiumBridge {
public:
    QVariantList rows;
    QString path;
    double m_frequency = 14074000;
    void appendDecodeMapToList(QVariantMap const& value) { rows.append(value); }
    QString legacyAllTxtPath() const { return path; }
    void aggiungiRigaRtty(QString const&, double, double);
    void aggiungiRigaJtty(QString const&, double, QDateTime const&);
    void appendJttyAllTxt(bool, int, QString const&, QDateTime const&) const;
};
#include "bridge_refactor_under_test.inc"
int main(int argc, char** argv) {
 QCoreApplication app(argc,argv); QTemporaryDir tmp; assert(tmp.isValid()); DecodiumBridge b; b.path=tmp.path()+"/nested/ALL.TXT";
 QDateTime date=QDateTime::fromString(QStringLiteral("2024-05-06T12:34:56+02:00"),Qt::ISODate);
 b.aggiungiRigaRtty("  ",.7,1234.6); b.aggiungiRigaJtty("\t",1234,date); assert(b.rows.empty());
 b.aggiungiRigaRtty("CQ RTTY",.73,1234.6); b.aggiungiRigaJtty("CQ JTTY",1450.3,date); assert(b.rows.size()==2);
 auto r=b.rows[0].toMap(); auto j=b.rows[1].toMap();
 assert(r["mode"]=="RTTY" && r["message"]=="CQ RTTY" && r["freq"].toInt()==1235 && r["db"]=="73" && !r["isTx"].toBool());
 assert(j["mode"]=="JTTY" && j["time"]=="103456" && j["utc"]=="1034" && j["freq"].toInt()==1450 && j["db"].toString().isEmpty() && j["timestamp"].toLongLong()==date.toMSecsSinceEpoch());
 b.appendJttyAllTxt(false,1450,"  CQ JTTY  ",date); b.appendJttyAllTxt(true,1500," TX JTTY ",date); b.appendJttyAllTxt(false,1,"  ",date);
 QFile f(b.path);assert(f.open(QIODevice::ReadOnly));QString contents=QString::fromUtf8(f.readAll());assert(contents.count('\n')==2 && contents.contains("240506_103456") && contents.contains("Rx") && contents.contains("Tx") && contents.contains("CQ JTTY") && contents.contains("TX JTTY"));
 std::cout << "RTTY/JTTY rows, UTC, rounded frequency, empty inputs and ALL.TXT RX/TX: OK\n";
}
