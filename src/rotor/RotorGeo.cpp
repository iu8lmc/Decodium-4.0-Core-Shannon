#include "RotorGeo.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace decodium::rotor {

namespace {
constexpr double kPi = 3.14159265358979323846;
double rad(double d) { return d * kPi / 180.0; }
double deg(double r) { return r * 180.0 / kPi; }
double wrap360(double d)
{
    double r = std::fmod(d, 360.0);
    return r < 0 ? r + 360.0 : r;
}
}  // namespace

QVariantMap Bearing::toMap() const
{
    auto round1 = [](double v) { return std::round(v * 10.0) / 10.0; };
    auto round4 = [](double v) { return std::round(v * 10000.0) / 10000.0; };
    return {
        {QStringLiteral("short_path"), round1(shortPath)},
        {QStringLiteral("long_path"), round1(longPath)},
        {QStringLiteral("distance_km"), static_cast<qint64>(std::llround(distanceKm))},
        {QStringLiteral("lat"), round4(target.latitude)},
        {QStringLiteral("lon"), round4(target.longitude)},
    };
}

bool isValidLocator(const QString& locator)
{
    static const QRegularExpression re(
        QStringLiteral("^[A-R]{2}[0-9]{2}([A-X]{2}([0-9]{2})?)?$"),
        QRegularExpression::CaseInsensitiveOption);
    return re.match(locator.trimmed()).hasMatch();
}

bool locatorToPosition(const QString& locator, Position* position)
{
    const QString text = locator.trimmed().toUpper();
    if (!isValidLocator(text))
        return false;

    double lon = (text[0].unicode() - 'A') * 20.0 - 180.0;
    double lat = (text[1].unicode() - 'A') * 10.0 - 90.0;
    lon += (text[2].unicode() - '0') * 2.0;
    lat += (text[3].unicode() - '0') * 1.0;

    // Ampiezza del riquadro raggiunto: serve a centrarlo alla fine.
    double lonSpan = 2.0, latSpan = 1.0;
    if (text.size() >= 6) {
        lon += (text[4].unicode() - 'A') * (2.0 / 24.0);
        lat += (text[5].unicode() - 'A') * (1.0 / 24.0);
        lonSpan = 2.0 / 24.0;
        latSpan = 1.0 / 24.0;
    }
    if (text.size() == 8) {
        lon += (text[6].unicode() - '0') * (lonSpan / 10.0);
        lat += (text[7].unicode() - '0') * (latSpan / 10.0);
        lonSpan /= 10.0;
        latSpan /= 10.0;
    }
    if (position) {
        position->latitude = lat + latSpan / 2.0;
        position->longitude = lon + lonSpan / 2.0;
    }
    return true;
}

QString positionToLocator(const Position& position, int precision)
{
    double lon = std::fmod(position.longitude + 180.0, 360.0);
    if (lon < 0) lon += 360.0;
    double lat = position.latitude + 90.0;

    auto divmod = [](double value, double step, double* rest) {
        const double q = std::floor(value / step);
        *rest = value - q * step;
        return static_cast<int>(q);
    };
    double restLon, restLat;
    const int fieldLon = divmod(lon, 20.0, &restLon);
    const int fieldLat = divmod(lat, 10.0, &restLat);
    const int squareLon = divmod(restLon, 2.0, &restLon);
    const int squareLat = divmod(restLat, 1.0, &restLat);

    QString out;
    out += QChar('A' + std::clamp(fieldLon, 0, 17));
    out += QChar('A' + std::clamp(fieldLat, 0, 17));
    out += QChar('0' + std::clamp(squareLon, 0, 9));
    out += QChar('0' + std::clamp(squareLat, 0, 9));
    if (precision >= 6) {
        const int subLon = divmod(restLon, 2.0 / 24.0, &restLon);
        const int subLat = divmod(restLat, 1.0 / 24.0, &restLat);
        out += QChar('A' + std::clamp(subLon, 0, 23));
        out += QChar('A' + std::clamp(subLat, 0, 23));
    }
    if (precision >= 8) {
        out += QChar('0' + std::clamp(static_cast<int>(restLon / (2.0 / 240.0)), 0, 9));
        out += QChar('0' + std::clamp(static_cast<int>(restLat / (1.0 / 240.0)), 0, 9));
    }
    return out;
}

Bearing bearingBetween(const Position& origin, const Position& target)
{
    const double lat1 = rad(origin.latitude), lon1 = rad(origin.longitude);
    const double lat2 = rad(target.latitude), lon2 = rad(target.longitude);
    const double dLon = lon2 - lon1;

    const double y = std::sin(dLon) * std::cos(lat2);
    const double x = std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(dLon);
    const double shortPath = wrap360(deg(std::atan2(y, x)));

    const double cosine = std::sin(lat1) * std::sin(lat2) + std::cos(lat1) * std::cos(lat2) * std::cos(dLon);
    const double central = std::acos(std::min(1.0, std::max(-1.0, cosine)));

    Bearing b;
    b.shortPath = shortPath;
    b.longPath = wrap360(shortPath + 180.0);
    b.distanceKm = central * kEarthRadiusKm;
    b.target = target;
    return b;
}

bool bearingToLocator(const QString& myLocator, const QString& targetLocator, Bearing* bearing)
{
    Position from, to;
    if (!locatorToPosition(myLocator, &from) || !locatorToPosition(targetLocator, &to))
        return false;
    if (bearing)
        *bearing = bearingBetween(from, to);
    return true;
}

}  // namespace decodium::rotor
