#include "RotorSpots.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace decodium::rotor {

namespace {
double nowSeconds() { return QDateTime::currentMSecsSinceEpoch() / 1000.0; }

const QSet<QString>& notGrid()
{
    static const QSet<QString> s {QStringLiteral("RR73")};
    return s;
}
const QSet<QString>& salutes()
{
    static const QSet<QString> s {QStringLiteral("RRR"), QStringLiteral("RR73"), QStringLiteral("73"),
                                  QStringLiteral("R"), QStringLiteral("TU"), QStringLiteral("TNX")};
    return s;
}
const QSet<QString>& notCall()
{
    static const QSet<QString> s {QStringLiteral("CQ"), QStringLiteral("DE"), QStringLiteral("QRZ"),
                                  QStringLiteral("DX"), QStringLiteral("TEST"), QStringLiteral("RR73"),
                                  QStringLiteral("RRR"), QStringLiteral("73"), QStringLiteral("NA"),
                                  QStringLiteral("EU"), QStringLiteral("AS"), QStringLiteral("SA"),
                                  QStringLiteral("AF"), QStringLiteral("OC")};
    return s;
}

bool plausibleCall(const QString& call)
{
    static const QRegularExpression re(QStringLiteral("^[A-Z0-9][A-Z0-9/]{1,11}$"));
    if (notCall().contains(call) || !re.match(call).hasMatch())
        return false;
    for (QChar c : call) {
        if (c.isDigit())
            return true;
    }
    return false;
}
}  // namespace

QVariantMap Spot::toMap(double now) const
{
    auto round1 = [](double v) { return std::round(v * 10.0) / 10.0; };
    auto round4 = [](double v) { return std::round(v * 10000.0) / 10000.0; };
    QVariantMap m;
    m.insert(QStringLiteral("call"), call);
    m.insert(QStringLiteral("grid"), grid);
    m.insert(QStringLiteral("lat"), round4(latitude));
    m.insert(QStringLiteral("lon"), round4(longitude));
    m.insert(QStringLiteral("az"), round1(azimuth));
    m.insert(QStringLiteral("km"), static_cast<qint64>(std::llround(distanceKm)));
    m.insert(QStringLiteral("snr"), snr ? QVariant(*snr) : QVariant());
    m.insert(QStringLiteral("mode"), mode);
    m.insert(QStringLiteral("freq"), frequencyHz);
    m.insert(QStringLiteral("source"), source);
    m.insert(QStringLiteral("entity"), entity);
    m.insert(QStringLiteral("comment"), comment);
    m.insert(QStringLiteral("ts"), round1(lastSeen));
    m.insert(QStringLiteral("age"), round1(now - lastSeen));
    m.insert(QStringLiteral("count"), count);
    return m;
}

bool SpotBook::stationFromMessage(const QString& text, QString* call, QString* grid)
{
    QString cleaned = text;
    cleaned.replace(QLatin1Char('<'), QLatin1Char(' ')).replace(QLatin1Char('>'), QLatin1Char(' '));
    QStringList tokens = cleaned.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (call) call->clear();
    if (grid) grid->clear();
    if (tokens.size() < 2)
        return false;

    static const QRegularExpression gridRe(QStringLiteral("^[A-R]{2}[0-9]{2}$"));
    static const QRegularExpression reportRe(QStringLiteral("^R?[+-][0-9]{1,2}$"));

    QString foundGrid;
    const QString last = tokens.last().toUpper();
    if (!notGrid().contains(last) && gridRe.match(last).hasMatch()) {
        foundGrid = last;
        tokens.removeLast();
    }
    // Rapporti e saluti chiudono il messaggio: tolti quelli, in fondo resta
    // sempre il nominativo di chi ha trasmesso.
    while (!tokens.isEmpty()
           && (salutes().contains(tokens.last().toUpper()) || reportRe.match(tokens.last().toUpper()).hasMatch()))
        tokens.removeLast();
    if (tokens.size() < 2)
        return false;

    const QString who = tokens.last().toUpper();
    if (!plausibleCall(who))
        return false;
    if (call) *call = who;
    if (grid) *grid = foundGrid;
    return true;
}

void SpotBook::configure(double ttlSeconds, int limit)
{
    m_ttl = ttlSeconds;
    m_limit = limit;
}

void SpotBook::setStation(const QString& callsign, const QString& locator)
{
    m_callsign = callsign.trimmed().toUpper();
    m_locator = locator;
    home();   // ricalcola subito rotta e distanza di tutte le stazioni
}

std::optional<Position> SpotBook::home()
{
    if (m_locator != m_originLocator) {
        Position p;
        m_origin = locatorToPosition(m_locator, &p) ? std::optional<Position>(p) : std::nullopt;
        m_originLocator = m_locator;
        if (m_origin) {
            for (Spot& s : m_spots) {
                const Bearing b = bearingBetween(*m_origin, Position{s.latitude, s.longitude});
                s.azimuth = b.shortPath;
                s.distanceKm = b.distanceKm;
            }
        }
        ++m_version;
    }
    return m_origin;
}

void SpotBook::aim(Spot& spot)
{
    const std::optional<Position> origin = home();
    if (!origin)
        return;
    const Bearing b = bearingBetween(*origin, Position{spot.latitude, spot.longitude});
    spot.azimuth = b.shortPath;
    spot.distanceKm = b.distanceKm;
}

void SpotBook::note(const QString& callIn, const QString& gridIn, const QString& source,
                    std::optional<int> snr, const QString& mode, qint64 frequencyHz,
                    std::optional<Position> position, const QString& entity, const QString& comment)
{
    const QString call = callIn.trimmed().toUpper();
    if (call.isEmpty())
        return;
    // La propria stazione compare in ogni QSO ma non e' un bersaglio.
    if (!m_callsign.isEmpty() && call == m_callsign)
        return;
    const QString grid = gridIn.trimmed().toUpper();

    auto it = m_spots.find(call);
    if (it == m_spots.end()) {
        Position where;
        QString label;
        if (!position) {
            if (grid.isEmpty() || !locatorToPosition(grid, &where))
                return;   // senza posizione non c'e' niente da mettere sulla mappa
            label = grid;
        } else {
            where = *position;
            // Il cluster da' il paese, non il riquadro: si scrive quello che si
            // ricava, cosi' l'interfaccia ha sempre un riferimento.
            label = positionToLocator(where, 4);
        }
        Spot spot;
        spot.call = call;
        spot.grid = label;
        spot.latitude = where.latitude;
        spot.longitude = where.longitude;
        spot.firstSeen = nowSeconds();
        it = m_spots.insert(call, spot);
    } else if (!grid.isEmpty() && grid != it->grid) {
        Position where;
        if (locatorToPosition(grid, &where)) {
            it->grid = grid;
            it->latitude = where.latitude;
            it->longitude = where.longitude;
        }
    }

    Spot& spot = it.value();
    spot.lastSeen = nowSeconds();
    ++spot.count;
    spot.source = source;
    if (snr)
        spot.snr = *snr;
    if (!mode.isEmpty())
        spot.mode = mode;
    if (frequencyHz)
        spot.frequencyHz = frequencyHz;
    if (!entity.isEmpty())
        spot.entity = entity;
    if (!comment.isEmpty())
        spot.comment = comment;

    aim(spot);
    purge(nowSeconds());
    ++m_version;
}

void SpotBook::purge(double now)
{
    for (auto it = m_spots.begin(); it != m_spots.end();) {
        if (now - it->lastSeen > m_ttl)
            it = m_spots.erase(it);
        else
            ++it;
    }
    if (m_spots.size() > m_limit) {
        QList<Spot> sorted = m_spots.values();
        std::sort(sorted.begin(), sorted.end(), [](const Spot& a, const Spot& b) { return a.lastSeen < b.lastSeen; });
        const int drop = static_cast<int>(m_spots.size()) - m_limit;
        for (int i = 0; i < drop; ++i)
            m_spots.remove(sorted.at(i).call);
    }
}

QVariantList SpotBook::entries(int limit)
{
    const double now = nowSeconds();
    purge(now);
    QList<Spot> spots = m_spots.values();
    std::sort(spots.begin(), spots.end(), [](const Spot& a, const Spot& b) { return a.lastSeen > b.lastSeen; });
    if (limit >= 0 && spots.size() > limit)
        spots = spots.mid(0, limit);
    QVariantList out;
    for (const Spot& s : spots)
        out.append(s.toMap(now));
    return out;
}

QVariantMap SpotBook::find(const QString& call)
{
    const auto it = m_spots.constFind(call.toUpper());
    if (it == m_spots.constEnd())
        return {};
    return it->toMap(nowSeconds());
}

void SpotBook::clear()
{
    m_spots.clear();
    ++m_version;
}

}  // namespace decodium::rotor
