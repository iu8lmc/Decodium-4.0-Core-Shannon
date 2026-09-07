#include "StationTelemetryCodec.hpp"

#include <QRegularExpression>

#include <array>
#include <algorithm>
#include <cmath>

namespace decodium
{
namespace telemetry
{

namespace
{

constexpr int kTotalBits = 71;
constexpr quint8 kMagicByte = 0xC3; // bit piu' alto = 1: vedi nota in .hpp

// Layout campi: {offset, larghezza in bit}.
constexpr int kOffMagic = 0, kWidMagic = 8;
constexpr int kOffGrid = 8, kWidGrid = 15;
constexpr int kOffTemp = 23, kWidTemp = 7;
constexpr int kOffWind = 30, kWidWind = 8;
constexpr int kOffWindDir = 38, kWidWindDir = 4;
constexpr int kOffSky = 42, kWidSky = 3;
constexpr int kOffPower = 45, kWidPower = 10;
constexpr int kOffRadio = 55, kWidRadio = 8;
constexpr int kOffAntenna = 63, kWidAntenna = 6;
// bit 69-70: riservati, sempre 0.

using BitArray = std::array<int, kTotalBits>;

void writeFieldBits (BitArray& bits, int offset, int width, quint64 value)
{
    for (int i = 0; i < width; ++i)
    {
        int const shift = width - 1 - i;
        bits[static_cast<std::size_t> (offset + i)] = static_cast<int> ((value >> shift) & 1ULL);
    }
}

quint64 readFieldBits (BitArray const& bits, int offset, int width)
{
    quint64 value = 0;
    for (int i = 0; i < width; ++i)
    {
        value = (value << 1) | static_cast<quint64> (bits[static_cast<std::size_t> (offset + i)] & 1);
    }
    return value;
}

int clampInt (int value, int lo, int hi)
{
    return std::max (lo, std::min (hi, value));
}

QString hexWordFromBits (BitArray const& bits, int offset, int width)
{
    quint64 const value = readFieldBits (bits, offset, width);
    return QStringLiteral ("%1").arg (value, 6, 16, QLatin1Char ('0')).toUpper ();
}

// Riempie una BitArray dai 3 blocchi esadecimali da 6 caratteri (formato
// esatto atteso/prodotto da pack_telemetry_cpp/unpack77_cpp).
bool bitsFromHex18 (QString const& hex18, BitArray& bits)
{
    if (hex18.size () != 18)
    {
        return false;
    }
    bool ok0 = false, ok1 = false, ok2 = false;
    quint64 const w0 = hex18.mid (0, 6).toULongLong (&ok0, 16);
    quint64 const w1 = hex18.mid (6, 6).toULongLong (&ok1, 16);
    quint64 const w2 = hex18.mid (12, 6).toULongLong (&ok2, 16);
    if (!ok0 || !ok1 || !ok2 || w0 >= (1ULL << 23))
    {
        return false;
    }
    writeFieldBits (bits, 0, 23, w0);
    writeFieldBits (bits, 23, 24, w1);
    writeFieldBits (bits, 47, 24, w2);
    return true;
}

// Locatore Maidenhead a 4 caratteri <-> 15 bit, stesso schema dei messaggi
// FT8 standard (v. Modulator/FtxMessageEncoder.cpp pack_grid4/unpack_grid4).
bool isGrid4 (QString const& g)
{
    if (g.size () != 4) return false;
    QChar const c0 = g[0].toUpper (), c1 = g[1].toUpper ();
    if (c0 < QLatin1Char ('A') || c0 > QLatin1Char ('R')) return false;
    if (c1 < QLatin1Char ('A') || c1 > QLatin1Char ('R')) return false;
    if (!g[2].isDigit () || !g[3].isDigit ()) return false;
    return true;
}

int packGrid4 (QString const& grid)
{
    if (!isGrid4 (grid))
    {
        return 0;
    }
    QString const g = grid.toUpper ();
    return (g[0].unicode () - 'A') * 1800
         + (g[1].unicode () - 'A') * 100
         + (g[2].unicode () - '0') * 10
         + (g[3].unicode () - '0');
}

QString unpackGrid4 (int value)
{
    if (value < 0 || value >= 32400)
    {
        return QString ();
    }
    int const a = value / 1800;
    int rem = value % 1800;
    int const b = rem / 100;
    rem = rem % 100;
    int const c = rem / 10;
    int const d = rem % 10;
    return QStringLiteral ("%1%2%3%4")
        .arg (QChar (static_cast<ushort> ('A' + a)))
        .arg (QChar (static_cast<ushort> ('A' + b)))
        .arg (c)
        .arg (d);
}

} // namespace

QStringList const& stationRadioModelNames ()
{
    static QStringList const names {
        QStringLiteral ("Non specificato"),
        QStringLiteral ("Icom IC-7300"), QStringLiteral ("Icom IC-7100"),
        QStringLiteral ("Icom IC-7610"), QStringLiteral ("Icom IC-9700"),
        QStringLiteral ("Icom IC-705"), QStringLiteral ("Icom IC-7851"),
        QStringLiteral ("Icom IC-7700"), QStringLiteral ("Icom IC-706MKIIG"),
        QStringLiteral ("Icom IC-718"),
        QStringLiteral ("Yaesu FT-991A"), QStringLiteral ("Yaesu FT-891"),
        QStringLiteral ("Yaesu FT-857D"), QStringLiteral ("Yaesu FTDX10"),
        QStringLiteral ("Yaesu FTDX101D"), QStringLiteral ("Yaesu FTDX101MP"),
        QStringLiteral ("Yaesu FT-450D"), QStringLiteral ("Yaesu FT-2000"),
        QStringLiteral ("Yaesu FT-1000MP"), QStringLiteral ("Yaesu FTDX3000"),
        QStringLiteral ("Yaesu FTDX5000"), QStringLiteral ("Yaesu FT-817ND"),
        QStringLiteral ("Yaesu FT-818"),
        QStringLiteral ("Kenwood TS-590SG"), QStringLiteral ("Kenwood TS-890S"),
        QStringLiteral ("Kenwood TS-990S"), QStringLiteral ("Kenwood TS-2000"),
        QStringLiteral ("Kenwood TS-480SAT"), QStringLiteral ("Kenwood TS-570D"),
        QStringLiteral ("Elecraft K3"), QStringLiteral ("Elecraft K3S"),
        QStringLiteral ("Elecraft K4"), QStringLiteral ("Elecraft KX2"),
        QStringLiteral ("Elecraft KX3"),
        QStringLiteral ("Xiegu X6100"), QStringLiteral ("Xiegu G90"),
        QStringLiteral ("Xiegu X5105"),
        QStringLiteral ("FlexRadio 6400"), QStringLiteral ("FlexRadio 6600"),
        QStringLiteral ("FlexRadio 6700"),
        QStringLiteral ("SDR / homebrew"),
        QStringLiteral ("Altro"),
    };
    return names;
}

QStringList const& stationAntennaTypeNames ()
{
    static QStringList const names {
        QStringLiteral ("Non specificata"),
        QStringLiteral ("Dipolo"), QStringLiteral ("Inverted V"),
        QStringLiteral ("Verticale"), QStringLiteral ("Ground plane"),
        QStringLiteral ("Yagi / beam"), QStringLiteral ("Verticale multibanda"),
        QStringLiteral ("End-fed"), QStringLiteral ("Loop magnetico"),
        QStringLiteral ("Delta loop"), QStringLiteral ("Filo lungo"),
        QStringLiteral ("Windom"), QStringLiteral ("Loop skywire"),
        QStringLiteral ("Stilo mobile"), QStringLiteral ("Log periodica"),
        QStringLiteral ("Quad"), QStringLiteral ("Array verticali"),
        QStringLiteral ("Altra"),
    };
    return names;
}

QStringList const& stationSkyConditionNames ()
{
    static QStringList const names {
        QStringLiteral ("Sereno"), QStringLiteral ("Poco nuvoloso"),
        QStringLiteral ("Nuvoloso"), QStringLiteral ("Pioggia"),
        QStringLiteral ("Neve"), QStringLiteral ("Temporale"),
        QStringLiteral ("Nebbia"), QStringLiteral ("Sconosciuto"),
    };
    return names;
}

int windDirDegToIndex16 (int degrees)
{
    int const normalized = ((degrees % 360) + 360) % 360;
    long const rounded = std::lround (static_cast<double> (normalized) / 22.5);
    return static_cast<int> (rounded % 16);
}

QString windDirIndex16ToLabel (int index16)
{
    static QStringList const labels {
        QStringLiteral ("N"), QStringLiteral ("NNE"), QStringLiteral ("NE"), QStringLiteral ("ENE"),
        QStringLiteral ("E"), QStringLiteral ("ESE"), QStringLiteral ("SE"), QStringLiteral ("SSE"),
        QStringLiteral ("S"), QStringLiteral ("SSW"), QStringLiteral ("SW"), QStringLiteral ("WSW"),
        QStringLiteral ("W"), QStringLiteral ("WNW"), QStringLiteral ("NW"), QStringLiteral ("NNW"),
    };
    int const idx = clampInt (index16, 0, 15);
    return labels[idx];
}

QString encodeStationTelemetryHex (StationTelemetryFields const& fields)
{
    BitArray bits {};
    bits.fill (0);

    writeFieldBits (bits, kOffMagic, kWidMagic, kMagicByte);
    writeFieldBits (bits, kOffGrid, kWidGrid,
                     static_cast<quint64> (packGrid4 (fields.grid4)));

    int const tempEncoded = (fields.tempC <= kTempUnknown)
        ? 127 // 7 bit tutti a 1 = "sconosciuto" (fuori dal range reale -50..+77)
        : clampInt (fields.tempC + 50, 0, 126);
    writeFieldBits (bits, kOffTemp, kWidTemp, static_cast<quint64> (tempEncoded));

    writeFieldBits (bits, kOffWind, kWidWind,
                     static_cast<quint64> (clampInt (fields.windSpeedKmh, 0, 255)));
    writeFieldBits (bits, kOffWindDir, kWidWindDir,
                     static_cast<quint64> (clampInt (fields.windDirDeg16, 0, 15)));
    writeFieldBits (bits, kOffSky, kWidSky,
                     static_cast<quint64> (clampInt (static_cast<int> (fields.sky), 0, 7)));
    writeFieldBits (bits, kOffPower, kWidPower,
                     static_cast<quint64> (clampInt (fields.powerWatts, 0, 1023)));
    writeFieldBits (bits, kOffRadio, kWidRadio,
                     static_cast<quint64> (clampInt (fields.radioModelId, 0,
                         stationRadioModelNames ().size () - 1)));
    writeFieldBits (bits, kOffAntenna, kWidAntenna,
                     static_cast<quint64> (clampInt (fields.antennaTypeId, 0,
                         stationAntennaTypeNames ().size () - 1)));

    return hexWordFromBits (bits, 0, 23)
         + hexWordFromBits (bits, 23, 24)
         + hexWordFromBits (bits, 47, 24);
}

bool decodeStationTelemetryHex (QString const& hex, StationTelemetryFields& out)
{
    QString const upper = hex.trimmed ().toUpper ();
    static QRegularExpression const hexPattern (QStringLiteral ("^[0-9A-F]{18}$"));
    if (!hexPattern.match (upper).hasMatch ())
    {
        return false;
    }

    BitArray bits {};
    bits.fill (0);
    if (!bitsFromHex18 (upper, bits))
    {
        return false;
    }

    if (readFieldBits (bits, kOffMagic, kWidMagic) != kMagicByte)
    {
        return false;
    }

    StationTelemetryFields fields;
    fields.grid4 = unpackGrid4 (static_cast<int> (readFieldBits (bits, kOffGrid, kWidGrid)));
    int const tempRaw = static_cast<int> (readFieldBits (bits, kOffTemp, kWidTemp));
    fields.tempC = (tempRaw >= 127) ? kTempUnknown : (tempRaw - 50);
    fields.windSpeedKmh = static_cast<int> (readFieldBits (bits, kOffWind, kWidWind));
    fields.windDirDeg16 = static_cast<int> (readFieldBits (bits, kOffWindDir, kWidWindDir));
    fields.sky = static_cast<SkyCondition> (readFieldBits (bits, kOffSky, kWidSky));
    fields.powerWatts = static_cast<int> (readFieldBits (bits, kOffPower, kWidPower));
    fields.radioModelId = static_cast<int> (readFieldBits (bits, kOffRadio, kWidRadio));
    fields.antennaTypeId = static_cast<int> (readFieldBits (bits, kOffAntenna, kWidAntenna));

    out = fields;
    return true;
}

}
}
