#include "CwMacros.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace decodium::cw {

QString expand(const QString& text, const Context& context)
{
    QString out = text;
    auto put = [&out](const QString& token, const QString& value) {
        out.replace(QStringLiteral("{") + token + QStringLiteral("}"), value, Qt::CaseInsensitive);
    };
    put(QStringLiteral("MYCALL"), context.myCall.toUpper());
    put(QStringLiteral("CALL"), context.call.toUpper());
    put(QStringLiteral("RST"), context.rst);
    put(QStringLiteral("NR"), context.nr);
    put(QStringLiteral("EXCH"), context.exch);
    put(QStringLiteral("NAME"), context.name);
    static const QRegularExpression leftovers(QStringLiteral("\\{[A-Za-z#]+\\}"));
    out.remove(leftovers);
    return out.toUpper().simplified();
}

QList<Macro> defaultMacros()
{
    return {
        {QStringLiteral("F1 CQ"),        QStringLiteral("CQ TEST {MYCALL} {MYCALL} TEST")},
        {QStringLiteral("F2 Call"),      QStringLiteral("{CALL}")},
        {QStringLiteral("F3 Exch"),      QStringLiteral("{CALL} 5NN {NR}")},
        {QStringLiteral("F4 TU"),        QStringLiteral("TU {MYCALL} TEST")},
        {QStringLiteral("F5 ?"),         QStringLiteral("?")},
        {QStringLiteral("F6 AGN"),       QStringLiteral("AGN")},
        {QStringLiteral("F7 NR?"),       QStringLiteral("NR?")},
        {QStringLiteral("F8 73"),        QStringLiteral("73 GL")},
        // F9-F12: quelle che servono in S&P e con l'ESM.
        {QStringLiteral("F9 My call"),   QStringLiteral("{MYCALL}")},
        {QStringLiteral("F10 S&P Exch"), QStringLiteral("5NN {NR}")},
        {QStringLiteral("F11 QRZ?"),     QStringLiteral("QRZ?")},
        {QStringLiteral("F12 QRL?"),     QStringLiteral("QRL?")},
    };
}

QString macrosToJson(const QList<Macro>& macros)
{
    QJsonArray array;
    for (const Macro& m : macros)
        array.append(QJsonObject{{QStringLiteral("label"), m.label}, {QStringLiteral("text"), m.text}});
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QList<Macro> macrosFromJson(const QString& json)
{
    const QJsonArray array = QJsonDocument::fromJson(json.toUtf8()).array();
    QList<Macro> out;
    for (const QJsonValue& value : array) {
        if (out.size() >= kMaxMacros)
            break;
        const QJsonObject o = value.toObject();
        out.append({o.value(QStringLiteral("label")).toString(), o.value(QStringLiteral("text")).toString()});
    }
    return out.isEmpty() ? defaultMacros() : out;
}

}  // namespace decodium::cw
