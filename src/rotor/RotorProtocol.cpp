#include "RotorProtocol.h"

#include <QRegularExpression>

#include <cmath>

namespace decodium::rotor {

const QList<Model>& models()
{
    static const QList<Model> table {
        {QStringLiteral("d_az"),   QStringLiteral("Control box D - azimuth only"),
         kIdAzimuth, 0, 1},
        {QStringLiteral("d_el"),   QStringLiteral("Control box D - elevation only"),
         0, kIdElevation, 1},
        {QStringLiteral("d_azel"), QStringLiteral("Control box D - azimuth + elevation"),
         kIdAzimuth, kIdElevation, 1},
        {QStringLiteral("combi"),  QStringLiteral("Combi-Track / Big-RAS (angles x10)"),
         kIdAzimuth, kIdSecondUnit, 10},
    };
    return table;
}

const Model* modelByKey(const QString& key)
{
    for (const Model& m : models()) {
        if (m.key == key)
            return &m;
    }
    return nullptr;
}

QByteArray encode(char axisId, char verb, const QString& argument)
{
    QByteArray frame;
    frame.append(kStx);
    frame.append(static_cast<char>(QChar(QLatin1Char(axisId)).toUpper().toLatin1()));
    frame.append(verb);
    frame.append(argument.toLatin1());
    frame.append(kCr);
    return frame;
}

QByteArray queryPosition(char axisId)
{
    return encode(axisId, '?');
}

QByteArray gotoFrame(char axisId, double degrees, int multiplier)
{
    // Arrotondamento al pari, come fa Python: la stessa traccia di prima.
    long raw = std::lround(std::nearbyint(degrees * multiplier));
    if (raw < 0)
        return {};
    // 997/999 (e 9777/9999 sui Combi) sono riservati agli stop: un goto su quel
    // valore esatto fermerebbe il rotore invece di muoverlo.
    if (raw == kStopSoft || raw == kStopFast || raw == 9777 || raw == 9999)
        raw -= 1;
    return encode(axisId, 'G', QString::number(raw));
}

QByteArray stopFrame(char axisId, bool fast, int multiplier)
{
    int code;
    if (multiplier == 10)
        code = fast ? 9999 : 9777;
    else
        code = fast ? kStopFast : kStopSoft;
    return encode(axisId, 'G', QString::number(code));
}

QByteArray disableCpm(char axisId)
{
    return encode(axisId, 'S');
}

bool decodeReply(const QByteArray& frame, int multiplier, Reply* reply, QString* error)
{
    QByteArray payload = frame;
    while (!payload.isEmpty() && (payload.front() == kStx || payload.front() == kCr
                                  || payload.front() == ' ' || payload.front() == '\n'))
        payload.remove(0, 1);
    while (!payload.isEmpty() && (payload.back() == kStx || payload.back() == kCr
                                  || payload.back() == ' ' || payload.back() == '\n'))
        payload.chop(1);
    for (char c : payload) {
        if (static_cast<unsigned char>(c) > 0x7F) {
            if (error)
                *error = QStringLiteral("non-ASCII frame");
            return false;
        }
    }
    static const QRegularExpression re(
        QStringLiteral("^([A-Z]),([^,]),(-?\\d+(?:\\.\\d+)?),([A-Z])$"));
    const QRegularExpressionMatch match = re.match(QString::fromLatin1(payload));
    if (!match.hasMatch()) {
        if (error)
            *error = QStringLiteral("unrecognised frame: ") + QString::fromLatin1(payload);
        return false;
    }
    if (reply) {
        reply->axisId = match.captured(1).at(0);
        reply->verb = match.captured(2).at(0);
        reply->value = match.captured(3).toDouble() / (multiplier > 0 ? multiplier : 1);
        reply->status = match.captured(4).at(0);
    }
    return true;
}

}  // namespace decodium::rotor
