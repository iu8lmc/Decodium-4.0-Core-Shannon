#include "DecoPortPacket.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QMessageAuthenticationCode>
#include <QPasswordDigestor>
#include <QtEndian>

#include <cmath>

namespace decoport {
namespace {

// Scrittori big-endian su QByteArray. Deliberatamente espliciti: il formato del
// filo non deve dipendere dall'ordine dei byte della macchina che compila.
void putU8(QByteArray& b, quint8 v)   { b.append(static_cast<char>(v)); }
void putU16(QByteArray& b, quint16 v) { char t[2]; qToBigEndian(v, t); b.append(t, 2); }
void putU32(QByteArray& b, quint32 v) { char t[4]; qToBigEndian(v, t); b.append(t, 4); }
void putI64(QByteArray& b, qint64 v)  { char t[8]; qToBigEndian(v, t); b.append(t, 8); }

bool takeU8(const QByteArray& b, int& off, quint8* out)
{
    if (off + 1 > b.size()) return false;
    *out = static_cast<quint8>(b.at(off));
    off += 1;
    return true;
}
// I byte si compongono a mano invece di passare da qFromBigEndian su un
// puntatore dentro il buffer: al compilatore quel puntatore non dice niente
// sulla lunghezza e -Werror=array-bounds rifiuta la lettura. Cosi' i limiti
// sono verificati sopra e la lettura e' byte per byte, senza ambiguita'.
inline quint8 byteAt(const QByteArray& b, int i)
{
    return static_cast<quint8>(b.at(i));
}
bool takeU16(const QByteArray& b, int& off, quint16* out)
{
    if (off + 2 > b.size()) return false;
    *out = static_cast<quint16>((static_cast<quint16>(byteAt(b, off)) << 8)
                                | static_cast<quint16>(byteAt(b, off + 1)));
    off += 2;
    return true;
}
bool takeU32(const QByteArray& b, int& off, quint32* out)
{
    if (off + 4 > b.size()) return false;
    quint32 v = 0;
    for (int i = 0; i < 4; ++i)
        v = (v << 8) | static_cast<quint32>(byteAt(b, off + i));
    *out = v;
    off += 4;
    return true;
}
bool takeI64(const QByteArray& b, int& off, qint64* out)
{
    if (off + 8 > b.size()) return false;
    quint64 v = 0;
    for (int i = 0; i < 8; ++i)
        v = (v << 8) | static_cast<quint64>(byteAt(b, off + i));
    *out = static_cast<qint64>(v);
    off += 8;
    return true;
}

} // namespace

QString modeToString(Mode m)
{
    switch (m) {
    case Mode::Usb:   return QStringLiteral("USB");
    case Mode::Lsb:   return QStringLiteral("LSB");
    case Mode::Cw:    return QStringLiteral("CW");
    case Mode::Cwr:   return QStringLiteral("CWR");
    case Mode::Am:    return QStringLiteral("AM");
    case Mode::Fm:    return QStringLiteral("FM");
    case Mode::Digu:  return QStringLiteral("DIGU");
    case Mode::Digl:  return QStringLiteral("DIGL");
    case Mode::Rtty:  return QStringLiteral("RTTY");
    case Mode::Rttyr: return QStringLiteral("RTTYR");
    case Mode::PktFm: return QStringLiteral("PKTFM");
    case Mode::Unknown: break;
    }
    return QStringLiteral("UNKNOWN");
}

Mode modeFromString(const QString& s)
{
    QString const u = s.trimmed().toUpper();
    if (u == QLatin1String("USB"))   return Mode::Usb;
    if (u == QLatin1String("LSB"))   return Mode::Lsb;
    if (u == QLatin1String("CW"))    return Mode::Cw;
    if (u == QLatin1String("CWR"))   return Mode::Cwr;
    if (u == QLatin1String("AM"))    return Mode::Am;
    if (u == QLatin1String("FM"))    return Mode::Fm;
    if (u == QLatin1String("RTTY"))  return Mode::Rtty;
    if (u == QLatin1String("RTTYR")) return Mode::Rttyr;
    if (u == QLatin1String("PKTFM")) return Mode::PktFm;
    // I nomi che le radio danno al "codec USB dentro il modulatore" sono tutti
    // diversi; qui rientrano nei due modi neutri.
    if (u == QLatin1String("DIGU") || u == QLatin1String("DATA-USB")
        || u == QLatin1String("DATA-U") || u == QLatin1String("PKT-U")
        || u == QLatin1String("PKTUSB") || u == QLatin1String("USB-D")
        || u == QLatin1String("DIG")   || u == QLatin1String("DATA")) {
        return Mode::Digu;
    }
    if (u == QLatin1String("DIGL") || u == QLatin1String("DATA-LSB")
        || u == QLatin1String("DATA-L") || u == QLatin1String("PKT-L")
        || u == QLatin1String("PKTLSB") || u == QLatin1String("LSB-D")) {
        return Mode::Digl;
    }
    return Mode::Unknown;
}

void Context::setSMeterDbm(double dbm)
{
    double const clamped = std::isfinite(dbm) ? qBound(-200.0, dbm, 100.0) : 0.0;
    sMeterDbmTenths = static_cast<qint16>(std::lround(clamped * 10.0));
    mask |= FieldSMeter;
}

// Gli strumenti, tutti con la stessa forma: limita, arrotonda, alza il bit.
//
// Il limite non e' pignoleria sul tipo. Una lettura CAT sbagliata — una riga
// arrivata a meta', un byte perso — puo' dare numeri assurdi, e un ago mandato
// a fondo scala da un errore di trasmissione e' peggio di un ago fermo: chi
// guarda crede di avere un guasto in antenna e stacca.
namespace {
template <typename T>
T scalato(double valore, double scala, double minimo, double massimo)
{
    double const v = std::isfinite(valore) ? qBound(minimo, valore, massimo) : minimo;
    return static_cast<T>(std::lround(v * scala));
}
}  // namespace

void Context::setForwardPowerW(double watts)
{
    forwardPowerTenthW = scalato<quint16>(watts, 10.0, 0.0, 6553.0);
    mask |= FieldForwardPower;
}

void Context::setSwr(double ratio)
{
    // Sotto 1.00 il ROS non esiste: se la radio lo dice, mente lei o mente la
    // conversione, e in nessuno dei due casi va mostrato com'e'.
    swrHundredths = scalato<quint16>(ratio, 100.0, 1.0, 99.99);
    mask |= FieldSwr;
}

void Context::setAlcPct(double pct)
{
    alcTenthPct = scalato<qint16>(pct, 10.0, -100.0, 200.0);
    mask |= FieldAlc;
}

void Context::setDrainVoltage(double volts)
{
    drainVoltHundredths = scalato<quint16>(volts, 100.0, 0.0, 655.0);
    mask |= FieldDrainVoltage;
}

void Context::setDrainCurrent(double amps)
{
    drainAmpHundredths = scalato<quint16>(amps, 100.0, 0.0, 655.0);
    mask |= FieldDrainCurrent;
}

void Context::setPaTemperature(double celsius)
{
    paTempTenthC = scalato<qint16>(celsius, 10.0, -50.0, 200.0);
    mask |= FieldPaTemperature;
}

void Context::setCompressionDb(double db)
{
    compressionTenthDb = scalato<quint16>(db, 10.0, 0.0, 60.0);
    mask |= FieldCompression;
}

void Context::setPowerSettingPct(double pct)
{
    powerSetTenthPct = scalato<quint16>(pct, 10.0, 0.0, 100.0);
    mask |= FieldPowerSetting;
}

QByteArray encodeContextPayload(const Context& ctx)
{
    QByteArray out;
    out.reserve(64);
    putU32(out, ctx.mask);
    // I campi seguono l'ordine dei bit: cosi' il decodificatore e' una sequenza
    // di if, senza tag ne' lunghezze per campo.
    if (ctx.has(FieldFrequency))     putI64(out, ctx.frequencyHz);
    if (ctx.has(FieldMode))          putU8(out, static_cast<quint8>(ctx.mode));
    if (ctx.has(FieldPtt))           putU8(out, ctx.ptt ? 1 : 0);
    if (ctx.has(FieldSMeter))        putU16(out, static_cast<quint16>(ctx.sMeterDbmTenths));
    if (ctx.has(FieldSampleRate))    putU32(out, ctx.sampleRate);
    if (ctx.has(FieldChannels))      putU8(out, ctx.channels);
    if (ctx.has(FieldBandwidth))     putU32(out, ctx.bandwidthHz);
    if (ctx.has(FieldRigLabel)) {
        QByteArray const utf8 = ctx.rigLabel.toUtf8().left(255);
        putU8(out, static_cast<quint8>(utf8.size()));
        out.append(utf8);
    }
    if (ctx.has(FieldStateFlags))    putU32(out, ctx.stateFlags);
    if (ctx.has(FieldTxAudioLeadMs)) putU16(out, ctx.txAudioLeadMs);
    if (ctx.has(FieldSessionPort))   putU16(out, ctx.sessionPort);
    if (ctx.has(FieldForwardPower))  putU16(out, ctx.forwardPowerTenthW);
    if (ctx.has(FieldSwr))           putU16(out, ctx.swrHundredths);
    if (ctx.has(FieldAlc))           putU16(out, static_cast<quint16>(ctx.alcTenthPct));
    if (ctx.has(FieldDrainVoltage))  putU16(out, ctx.drainVoltHundredths);
    if (ctx.has(FieldDrainCurrent))  putU16(out, ctx.drainAmpHundredths);
    if (ctx.has(FieldPaTemperature)) putU16(out, static_cast<quint16>(ctx.paTempTenthC));
    if (ctx.has(FieldCompression))   putU16(out, ctx.compressionTenthDb);
    if (ctx.has(FieldPowerSetting))  putU16(out, ctx.powerSetTenthPct);
    return out;
}

bool decodeContextPayload(const QByteArray& payload, Context* out)
{
    if (!out) return false;
    int off = 0;
    Context ctx;
    if (!takeU32(payload, off, &ctx.mask)) return false;

    if (ctx.has(FieldFrequency) && !takeI64(payload, off, &ctx.frequencyHz)) return false;
    if (ctx.has(FieldMode)) {
        quint8 v = 0;
        if (!takeU8(payload, off, &v)) return false;
        ctx.mode = (v <= static_cast<quint8>(Mode::PktFm)) ? static_cast<Mode>(v) : Mode::Unknown;
    }
    if (ctx.has(FieldPtt)) {
        quint8 v = 0;
        if (!takeU8(payload, off, &v)) return false;
        ctx.ptt = (v != 0);
    }
    if (ctx.has(FieldSMeter)) {
        quint16 v = 0;
        if (!takeU16(payload, off, &v)) return false;
        ctx.sMeterDbmTenths = static_cast<qint16>(v);
    }
    if (ctx.has(FieldSampleRate) && !takeU32(payload, off, &ctx.sampleRate)) return false;
    if (ctx.has(FieldChannels)   && !takeU8(payload, off, &ctx.channels))    return false;
    if (ctx.has(FieldBandwidth)  && !takeU32(payload, off, &ctx.bandwidthHz))return false;
    if (ctx.has(FieldRigLabel)) {
        quint8 len = 0;
        if (!takeU8(payload, off, &len)) return false;
        if (off + len > payload.size()) return false;
        ctx.rigLabel = QString::fromUtf8(payload.constData() + off, len);
        off += len;
    }
    if (ctx.has(FieldStateFlags)    && !takeU32(payload, off, &ctx.stateFlags))    return false;
    if (ctx.has(FieldTxAudioLeadMs) && !takeU16(payload, off, &ctx.txAudioLeadMs)) return false;
    if (ctx.has(FieldSessionPort)   && !takeU16(payload, off, &ctx.sessionPort))   return false;

    if (ctx.has(FieldForwardPower)  && !takeU16(payload, off, &ctx.forwardPowerTenthW))  return false;
    if (ctx.has(FieldSwr)           && !takeU16(payload, off, &ctx.swrHundredths))       return false;
    if (ctx.has(FieldAlc)) {
        quint16 v = 0;
        if (!takeU16(payload, off, &v)) return false;
        ctx.alcTenthPct = static_cast<qint16>(v);
    }
    if (ctx.has(FieldDrainVoltage)  && !takeU16(payload, off, &ctx.drainVoltHundredths)) return false;
    if (ctx.has(FieldDrainCurrent)  && !takeU16(payload, off, &ctx.drainAmpHundredths))  return false;
    if (ctx.has(FieldPaTemperature)) {
        quint16 v = 0;
        if (!takeU16(payload, off, &v)) return false;
        ctx.paTempTenthC = static_cast<qint16>(v);
    }
    if (ctx.has(FieldCompression)   && !takeU16(payload, off, &ctx.compressionTenthDb))  return false;
    if (ctx.has(FieldPowerSetting)  && !takeU16(payload, off, &ctx.powerSetTenthPct))    return false;

    *out = ctx;
    return true;
}

namespace {

QByteArray authTag(const QByteArray& key, const QByteArray& signedBytes)
{
    QMessageAuthenticationCode mac(QCryptographicHash::Sha256, key);
    mac.addData(signedBytes);
    return mac.result().left(kAuthTagBytes);
}

// Confronto a tempo costante: con un confronto normale il tempo di risposta
// racconta quanti byte iniziali erano giusti, e la firma si indovina un byte
// alla volta.
bool tagsEqual(const QByteArray& a, const QByteArray& b)
{
    if (a.size() != b.size())
        return false;
    quint8 diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<quint8>(a[i]) ^ static_cast<quint8>(b[i]);
    return diff == 0;
}

} // namespace

QByteArray deriveKeyFromPassword(const QString& password)
{
    QString const trimmed = password.trimmed();
    if (trimmed.isEmpty())
        return QByteArray();
    // Sale fisso: la chiave deve venire uguale sulle due macchine. E' il
    // compromesso di un segreto condiviso — si compensa con le iterazioni.
    static const QByteArray salt = QByteArrayLiteral("Decodium-DecoPort-v1");
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
                                              trimmed.toUtf8(), salt,
                                              200000, 32);
}

bool timestampAcceptable(const Header& h, qint64 windowSeconds)
{
    if (!(h.flags & FlagHasTimestamp))
        return false;
    qint64 const nowSec = QDateTime::currentSecsSinceEpoch();
    qint64 const delta = nowSec - static_cast<qint64>(h.tsSeconds);
    return delta <= windowSeconds && delta >= -windowSeconds;
}

QByteArray buildPacket(Type type, quint32 streamId, quint32 sequence,
                       quint64 tsNs, const QByteArray& payload,
                       const QByteArray& authKey)
{
    quint16 flags = tsNs > 0 ? static_cast<quint16>(FlagHasTimestamp)
                             : static_cast<quint16>(FlagNone);
    if (!authKey.isEmpty())
        flags |= static_cast<quint16>(FlagAuthenticated);

    QByteArray pkt;
    pkt.reserve(kHeaderBytes + payload.size() + kAuthTagBytes);
    putU32(pkt, kMagic);
    putU8(pkt, kVersion);
    putU8(pkt, static_cast<quint8>(type));
    putU16(pkt, flags);
    putU32(pkt, streamId);
    putU32(pkt, sequence);
    putU32(pkt, static_cast<quint32>(tsNs / 1000000000ull));
    putU32(pkt, static_cast<quint32>(tsNs % 1000000000ull));
    putU16(pkt, static_cast<quint16>(qMin<int>(payload.size(), 0xFFFF)));
    putU16(pkt, 0);
    pkt.append(payload);
    if (!authKey.isEmpty())
        pkt.append(authTag(authKey, pkt));   // firma su header + payload
    return pkt;
}

bool parsePacket(const QByteArray& datagram, Header* header, QByteArray* payload,
                 const QByteArray& authKey, bool* authenticated)
{
    if (authenticated)
        *authenticated = false;
    if (!header || datagram.size() < kHeaderBytes) return false;

    int off = 0;
    quint32 magic = 0;
    if (!takeU32(datagram, off, &magic) || magic != kMagic) return false;

    Header h;
    quint8 rawType = 0;
    if (!takeU8(datagram, off, &h.version)) return false;
    if (h.version != kVersion) return false;
    if (!takeU8(datagram, off, &rawType)) return false;
    if (rawType < static_cast<quint8>(Type::Announce) || rawType > static_cast<quint8>(Type::Status))
        return false;
    h.type = static_cast<Type>(rawType);
    if (!takeU16(datagram, off, &h.flags))     return false;
    if (!takeU32(datagram, off, &h.streamId))  return false;
    if (!takeU32(datagram, off, &h.sequence))  return false;
    if (!takeU32(datagram, off, &h.tsSeconds)) return false;
    if (!takeU32(datagram, off, &h.tsNanos))   return false;
    if (!takeU16(datagram, off, &h.payloadLength)) return false;
    quint16 reserved = 0;
    if (!takeU16(datagram, off, &reserved)) return false;

    // La lunghezza dichiarata deve stare dentro il datagramma: un pacchetto
    // troncato o costruito male non deve poter far leggere oltre il buffer.
    if (off + static_cast<int>(h.payloadLength) > datagram.size()) return false;

    // La firma sta dopo il payload e non e' contata in payloadLength.
    if (h.flags & FlagAuthenticated) {
        int const signedLen = off + static_cast<int>(h.payloadLength);
        if (datagram.size() < signedLen + kAuthTagBytes)
            return false;
        if (!authKey.isEmpty()) {
            QByteArray const expected = authTag(authKey, datagram.left(signedLen));
            QByteArray const got = datagram.mid(signedLen, kAuthTagBytes);
            if (authenticated)
                *authenticated = tagsEqual(expected, got);
        }
    }

    *header = h;
    if (payload)
        *payload = datagram.mid(off, h.payloadLength);
    return true;
}

quint64 nowUnixNs()
{
    return static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1000000ull;
}

} // namespace decoport
