#include <QCoreApplication>
#include <QDateTime>
#include <QRegularExpression>
#include <QStringList>
#include <QtMath>
#include <iostream>
#include <utility>
#include "DecodeMessageNormalization.h"
using decodium::decode_message::stripDecodeApAnnotation;
class DecodiumBridge {
public:
    int m_nfa = 200;
    double m_frequency = 14095600;
    int m_rxFrequency = 1400;
    QStringList parseFt8Row(const QString&) const;
    QStringList parseWsprRow(const QString&) const;
    QStringList parseJt65Row(const QString&) const;
};
#include "bridge_refactor_decode_rows_helper.inc"
#include "bridge_refactor_decode_rows.inc"
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    DecodiumBridge b;
    auto check = [](const char* name, const QStringList& got, const QStringList& expected) {
        if (got != expected) { std::cerr << name << " mismatch: " << got.join('|').toStdString() << '\n'; std::exit(1); }
        std::cout << name << ": " << got.join('|').toStdString() << '\n';
    };
    check("ft8", b.parseFt8Row("1234 -12 0.3 1500 ~  CQ K1ABC FN42 a1"), {"1234","-12","0.3","1300","CQ K1ABC FN42","1","100","1500"});
    check("ft8-invalid", b.parseFt8Row("not a decode"), {});
    check("wspr", b.parseWsprRow("2256 -21 -0.3 14.097090 0 DU1MGA PK04 37"), {"2256","-21","-0.3","1290","DU1MGA PK04 37","0","100","1490"});
    check("wspr-invalid-call", b.parseWsprRow("2256 -21 -0.3 14.097090 0 INVALID PK04 37"), {});
    check("wspr-invalid-power", b.parseWsprRow("2256 -21 -0.3 14.097090 0 DU1MGA PK04 61"), {});
    check("wspr-finished", b.parseWsprRow("<DecodeFinished>"), {});
    b.m_frequency = 1000000;
    check("wspr-fallback", b.parseWsprRow("2256 -21 -0.3 14.097090 0 <K1ABC> FN42 20"), {"2256","-21","-0.3","1200","<K1ABC> FN42 20","0","100","1400"});
    const QString jt = QStringLiteral("1234%1%2%3  # CQ K1ABC FN42").arg(-12,4).arg(0.3,5,'f',1).arg(1500,5);
    check("jt65", b.parseJt65Row(jt), {"1234","-12","0.3","1300","CQ K1ABC FN42","","100","1500"});
    check("jt65-short", b.parseJt65Row("1234"), {});
}
