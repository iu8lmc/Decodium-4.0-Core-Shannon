#include "RotorConfig.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace decodium::rotor {

namespace {
const char kGroup[] = "Rotor";

QString presetsToJson(const QList<Preset>& presets)
{
    QJsonArray array;
    for (const Preset& p : presets) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), p.name);
        o.insert(QStringLiteral("az"), p.az);
        if (p.el)
            o.insert(QStringLiteral("el"), *p.el);
        array.append(o);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QList<Preset> presetsFromJson(const QString& json)
{
    QList<Preset> out;
    const QJsonArray array = QJsonDocument::fromJson(json.toUtf8()).array();
    for (const QJsonValue& v : array) {
        const QJsonObject o = v.toObject();
        Preset p;
        p.name = o.value(QStringLiteral("name")).toString();
        if (p.name.isEmpty())
            continue;
        p.az = o.value(QStringLiteral("az")).toDouble();
        if (o.contains(QStringLiteral("el")) && !o.value(QStringLiteral("el")).isNull())
            p.el = o.value(QStringLiteral("el")).toDouble();
        out.append(p);
    }
    return out;
}
}  // namespace

QVariantList presetsToVariant(const QList<Preset>& presets)
{
    QVariantList out;
    for (const Preset& p : presets) {
        QVariantMap m;
        m.insert(QStringLiteral("name"), p.name);
        m.insert(QStringLiteral("az"), p.az);
        m.insert(QStringLiteral("el"), p.el ? QVariant(*p.el) : QVariant());
        out.append(m);
    }
    return out;
}

void Config::load(QSettings& s)
{
    s.beginGroup(QLatin1String(kGroup));
    auto get = [&s](const char* key, const QVariant& def) { return s.value(QLatin1String(key), def); };
    enabled = get("enabled", enabled).toBool();
    port = get("port", port).toString();
    baudrate = get("baudrate", baudrate).toInt();
    timeout = get("timeout", timeout).toDouble();
    retries = get("retries", retries).toInt();
    model = get("model", model).toString();
    pollInterval = get("pollInterval", pollInterval).toDouble();
    simulate = get("simulate", simulate).toBool();
    networkEnabled = get("networkEnabled", networkEnabled).toBool();
    bind = get("bind", bind).toString();
    wsPort = get("wsPort", wsPort).toInt();
    httpPort = get("httpPort", httpPort).toInt();
    rotctldPort = get("rotctldPort", rotctldPort).toInt();
    token = get("token", token).toString();
    limits.azMin = get("azMin", limits.azMin).toDouble();
    limits.azMax = get("azMax", limits.azMax).toDouble();
    limits.elMin = get("elMin", limits.elMin).toDouble();
    limits.elMax = get("elMax", limits.elMax).toDouble();
    parkAz = get("parkAz", parkAz).toDouble();
    parkEl = get("parkEl", parkEl).toDouble();
    stallTimeout = get("stallTimeout", stallTimeout).toDouble();
    stopOnClientLoss = get("stopOnClientLoss", stopOnClientLoss).toBool();
    tolerance = get("tolerance", tolerance).toDouble();
    beamwidth = get("beamwidth", beamwidth).toDouble();
    presets = presetsFromJson(get("presets", QString()).toString());
    spotsEnabled = get("spotsEnabled", spotsEnabled).toBool();
    spotTtl = get("spotTtl", spotTtl).toDouble();
    spotLimit = get("spotLimit", spotLimit).toInt();
    tileUrl = get("tileUrl", tileUrl).toString();
    tileAttribution = get("tileAttribution", tileAttribution).toString();
    tileCacheDays = get("tileCacheDays", tileCacheDays).toDouble();
    tileCacheMb = get("tileCacheMb", tileCacheMb).toDouble();
    darkTheme = get("darkTheme", darkTheme).toBool();
    s.endGroup();

    // Valori fuori misura tornano ai limiti: una configurazione rovinata non
    // deve poter rendere inservibile il controllo.
    baudrate = std::clamp(baudrate, 300, 115200);
    retries = std::clamp(retries, 1, 10);
    timeout = std::clamp(timeout, 0.2, 30.0);
    pollInterval = std::clamp(pollInterval, 0.05, 5.0);
    wsPort = std::clamp(wsPort, 1, 65535);
    httpPort = std::clamp(httpPort, 1, 65535);
    rotctldPort = std::clamp(rotctldPort, 1, 65535);
    tolerance = std::clamp(tolerance, 0.1, 20.0);
    stallTimeout = std::clamp(stallTimeout, 1.0, 600.0);
    if (limits.azMax <= limits.azMin) { limits.azMin = 0.0; limits.azMax = 360.0; }
    if (limits.elMax <= limits.elMin) { limits.elMin = 0.0; limits.elMax = 90.0; }
    historySize = std::clamp(historySize, 10, 100000);
    trafficSize = std::clamp(trafficSize, 10, 100000);
}

void Config::save(QSettings& s) const
{
    s.beginGroup(QLatin1String(kGroup));
    auto set = [&s](const char* key, const QVariant& v) { s.setValue(QLatin1String(key), v); };
    set("enabled", enabled);
    set("port", port);
    set("baudrate", baudrate);
    set("timeout", timeout);
    set("retries", retries);
    set("model", model);
    set("pollInterval", pollInterval);
    set("simulate", simulate);
    set("networkEnabled", networkEnabled);
    set("bind", bind);
    set("wsPort", wsPort);
    set("httpPort", httpPort);
    set("rotctldPort", rotctldPort);
    set("token", token);
    set("azMin", limits.azMin);
    set("azMax", limits.azMax);
    set("elMin", limits.elMin);
    set("elMax", limits.elMax);
    set("parkAz", parkAz);
    set("parkEl", parkEl);
    set("stallTimeout", stallTimeout);
    set("stopOnClientLoss", stopOnClientLoss);
    set("tolerance", tolerance);
    set("beamwidth", beamwidth);
    set("presets", presetsToJson(presets));
    set("spotsEnabled", spotsEnabled);
    set("spotTtl", spotTtl);
    set("spotLimit", spotLimit);
    set("tileUrl", tileUrl);
    set("tileAttribution", tileAttribution);
    set("tileCacheDays", tileCacheDays);
    set("tileCacheMb", tileCacheMb);
    set("darkTheme", darkTheme);
    s.endGroup();
}

double Config::clampAz(double degrees) const
{
    return std::min(std::max(degrees, limits.azMin), limits.azMax);
}

double Config::clampEl(double degrees) const
{
    return std::min(std::max(degrees, limits.elMin), limits.elMax);
}

}  // namespace decodium::rotor
