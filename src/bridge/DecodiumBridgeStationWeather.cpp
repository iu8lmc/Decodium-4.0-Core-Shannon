#include "DecodiumBridge.h"
#include "helper_functions.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::fetchWeatherForGrid()
{
    if (offlineMode()) {
        return;
    }
    if (!getSetting(QStringLiteral("WeatherApiEnable"), false).toBool()) {
        return;
    }
    QString const grid = m_grid.trimmed().toUpper();
    double lon = 0.0, lat = 0.0;
    if (!grid2deg(grid, lon, lat)) {
        decodiumBridgeAudioLevelLog(QStringLiteral("fetchWeatherForGrid: locatore non valido [%1]").arg(grid));
        return;
    }

    QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("latitude"), QString::number(lat, 'f', 4));
    query.addQueryItem(QStringLiteral("longitude"), QString::number(lon, 'f', 4));
    query.addQueryItem(QStringLiteral("current"),
                        QStringLiteral("temperature_2m,wind_speed_10m,wind_direction_10m,weather_code"));
    query.addQueryItem(QStringLiteral("timezone"), QStringLiteral("UTC"));
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Decodium/4.0");

    QNetworkReply* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            decodiumBridgeAudioLevelLog(QStringLiteral("fetchWeatherForGrid: errore rete: %1").arg(reply->errorString()));
            return;
        }
        QJsonParseError parseError{};
        QJsonDocument const doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            decodiumBridgeAudioLevelLog(QStringLiteral("fetchWeatherForGrid: JSON non valido"));
            return;
        }
        QJsonObject const current = doc.object().value(QStringLiteral("current")).toObject();
        if (current.isEmpty()) {
            return;
        }
        m_weatherTempC = qRound(current.value(QStringLiteral("temperature_2m")).toDouble());
        m_weatherWindKmh = qRound(current.value(QStringLiteral("wind_speed_10m")).toDouble());
        m_weatherWindDirDeg = qRound(current.value(QStringLiteral("wind_direction_10m")).toDouble());
        int const wmo = current.value(QStringLiteral("weather_code")).toInt(-1);
        using decodium::telemetry::SkyCondition;
        if (wmo == 0)                              m_weatherSky = SkyCondition::Clear;
        else if (wmo == 1 || wmo == 2)              m_weatherSky = SkyCondition::PartlyCloudy;
        else if (wmo == 3)                          m_weatherSky = SkyCondition::Cloudy;
        else if (wmo == 45 || wmo == 48)            m_weatherSky = SkyCondition::Fog;
        else if ((wmo >= 51 && wmo <= 67) || (wmo >= 80 && wmo <= 82))
                                                     m_weatherSky = SkyCondition::Rain;
        else if ((wmo >= 71 && wmo <= 77) || wmo == 85 || wmo == 86)
                                                     m_weatherSky = SkyCondition::Snow;
        else if (wmo == 95 || wmo == 96 || wmo == 99) m_weatherSky = SkyCondition::Thunderstorm;
        else                                         m_weatherSky = SkyCondition::Unknown;
        m_weatherFetchedAtMs = QDateTime::currentMSecsSinceEpoch();
        decodiumBridgeAudioLevelLog(QStringLiteral("fetchWeatherForGrid: tempC=%1 windKmh=%2 windDeg=%3 wmo=%4")
                      .arg(m_weatherTempC).arg(m_weatherWindKmh).arg(m_weatherWindDirDeg).arg(wmo));
        emit statusMessage(QCoreApplication::translate("StationWeather", "Now: %1°C, wind %2 km/h %3, %4")
                                .arg(m_weatherTempC).arg(m_weatherWindKmh)
                                .arg(decodium::telemetry::windDirIndex16ToLabel(
                                    decodium::telemetry::windDirDegToIndex16(m_weatherWindDirDeg)))
                                .arg(decodium::telemetry::stationSkyConditionNames().value(
                                    static_cast<int>(m_weatherSky))));
    });
}

QVariantMap DecodiumBridge::currentWeatherPreview() const
{
    QVariantMap out;
    qint64 const ageMs = QDateTime::currentMSecsSinceEpoch() - m_weatherFetchedAtMs;
    bool const fresh = getSetting(QStringLiteral("WeatherApiEnable"), false).toBool()
        && m_weatherFetchedAtMs > 0 && ageMs < (30 * 60 * 1000);
    out[QStringLiteral("available")] = fresh;
    if (fresh) {
        out[QStringLiteral("tempC")] = m_weatherTempC;
        out[QStringLiteral("windKmh")] = m_weatherWindKmh;
        out[QStringLiteral("windDirLabel")] = decodium::telemetry::windDirIndex16ToLabel(
            decodium::telemetry::windDirDegToIndex16(m_weatherWindDirDeg));
        out[QStringLiteral("sky")] = decodium::telemetry::stationSkyConditionNames().value(
            static_cast<int>(m_weatherSky), QStringLiteral("?"));
    }
    return out;
}

