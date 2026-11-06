// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecoPortRadioLibrary.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QRegularExpression>

#include <memory>

namespace {

QMutex& libraryMutex()
{
    static QMutex m;
    return m;
}

std::unique_ptr<DecoPortRadioLibrary>& libraryInstance()
{
    static std::unique_ptr<DecoPortRadioLibrary> lib;
    return lib;
}

QString& userFilePath()
{
    static QString path = qEnvironmentVariable("DECOPORT_RADIOS");
    return path;
}

// Il confronto dei nomi ignora tutto tranne lettere e cifre: "FT-991",
// "ft 991" e "FT991" sono la stessa radio scritta da tre persone diverse.
QString normalised(const QString& s)
{
    QString out;
    out.reserve(s.size());
    for (const QChar c : s)
        if (c.isLetterOrNumber())
            out.append(c.toUpper());
    return out;
}

// "0x94", "148", "0A4" -> numero. Le tabelle CI-V si scrivono in esadecimale.
int parseNumber(const QJsonValue& v)
{
    if (v.isDouble())
        return v.toInt();
    QString s = v.toString().trimmed();
    bool ok = false;
    int n = 0;
    if (s.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        n = s.mid(2).toInt(&ok, 16);
    else
        n = s.toInt(&ok, 10);
    return ok ? n : 0;
}

QVector<int> intList(const QJsonValue& v)
{
    QVector<int> out;
    for (const QJsonValue& e : v.toArray())
        if (const int n = parseNumber(e))
            out.append(n);
    return out;
}

// Le tabelle si uniscono mettendo davanti chi conta di piu', senza ripetere.
void mergeList(QVector<int>& into, const QVector<int>& from, bool prepend)
{
    QVector<int> out = prepend ? from : into;
    for (int n : (prepend ? into : from))
        if (!out.contains(n))
            out.append(n);
    into = out;
}

} // namespace

// ── istanza ─────────────────────────────────────────────────────────────────

const DecoPortRadioLibrary& DecoPortRadioLibrary::instance()
{
    QMutexLocker lock(&libraryMutex());
    auto& lib = libraryInstance();
    if (!lib) {
        lib.reset(new DecoPortRadioLibrary);
        lib->load(userFilePath());
    }
    return *lib;
}

void DecoPortRadioLibrary::setUserFile(const QString& path)
{
    QMutexLocker lock(&libraryMutex());
    userFilePath() = path;
}

void DecoPortRadioLibrary::reload()
{
    QMutexLocker lock(&libraryMutex());
    auto& lib = libraryInstance();
    lib.reset(new DecoPortRadioLibrary);
    lib->load(userFilePath());
}

DecoPortRadioLibrary::DecoPortRadioLibrary() = default;

void DecoPortRadioLibrary::load(const QString& userFile)
{
    QStringList sources;
    // Dal meno al piu' importante: ogni strato puo' correggere quello sotto.
    const struct {
        const char* path;
        bool prepend;
    } builtin[] = {
        {":/decoport/radios-hamlib.json", false},
        {":/decoport/radios-decoport.json", true},
    };
    for (const auto& b : builtin) {
        QFile f(QString::fromLatin1(b.path));
        if (!f.open(QIODevice::ReadOnly)) {
            m_warnings.append(QStringLiteral("manca %1 dentro il programma").arg(f.fileName()));
            continue;
        }
        if (merge(f.readAll(), f.fileName(), b.prepend))
            sources.append(m_source);
    }
    if (!userFile.isEmpty()) {
        QFile f(userFile);
        if (!f.open(QIODevice::ReadOnly))
            m_warnings.append(QStringLiteral("non si apre %1: %2").arg(userFile, f.errorString()));
        else if (merge(f.readAll(), userFile, true))
            sources.append(m_source);
    }
    sources.removeDuplicates();
    m_source = sources.join(QLatin1String(" + "));
}

bool DecoPortRadioLibrary::merge(const QByteArray& json, const QString& origin, bool prepend)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (doc.isNull() || !doc.isObject()) {
        m_warnings.append(QStringLiteral("%1: %2").arg(origin, err.errorString()));
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value(QLatin1String("format")).toInt() != 1) {
        m_warnings.append(QStringLiteral("%1: formato sconosciuto").arg(origin));
        return false;
    }
    m_source = root.value(QLatin1String("source")).toString(origin);

    for (const QJsonValue& v : root.value(QLatin1String("models")).toArray()) {
        const QJsonObject o = v.toObject();
        Model m;
        m.model     = o.value(QLatin1String("model")).toInt();
        if (m.model <= 0)
            continue;
        // Una voce che c'e' gia' si corregge campo per campo: il file
        // dell'utente puo' dire solo "questa radio va a 9600".
        const int existing = m_byNumber.value(m.model, -1);
        if (existing >= 0)
            m = m_models.at(existing);
        auto str = [&o](const char* key, QString& into) {
            const QJsonValue x = o.value(QLatin1String(key));
            if (x.isString())
                into = x.toString();
        };
        auto num = [&o](const char* key, int& into) {
            const QJsonValue x = o.value(QLatin1String(key));
            if (x.isDouble())
                into = x.toInt();
        };
        str("symbol", m.symbol);
        str("mfg", m.mfg);
        str("name", m.name);
        str("status", m.status);
        str("backend", m.backend);
        str("protocol", m.protocol);
        str("port", m.port);
        str("handshake", m.handshake);
        num("baudMin", m.baudMin);
        num("baudMax", m.baudMax);
        num("dataBits", m.dataBits);
        num("stopBits", m.stopBits);
        const QString parity = o.value(QLatin1String("parity")).toString();
        if (!parity.isEmpty())
            m.parity = parity.at(0).toUpper();
        if (existing >= 0) {
            m_models[existing] = m;
        } else {
            m_byNumber.insert(m.model, m_models.size());
            m_models.append(m);
        }
    }

    const QJsonObject id = root.value(QLatin1String("identify")).toObject();
    for (const QJsonValue& v : id.value(QLatin1String("kenwood")).toArray()) {
        const QJsonObject o = v.toObject();
        mergeList(m_kenwoodIds[o.value(QLatin1String("id")).toString()],
                  intList(o.value(QLatin1String("models"))), prepend);
    }
    for (const QJsonValue& v : id.value(QLatin1String("yaesu")).toArray()) {
        const QJsonObject o = v.toObject();
        mergeList(m_yaesuIds[o.value(QLatin1String("id")).toString()],
                  intList(o.value(QLatin1String("models"))), prepend);
    }
    for (const QJsonValue& v : id.value(QLatin1String("civ")).toArray()) {
        const QJsonObject o = v.toObject();
        const int addr = parseNumber(o.value(QLatin1String("civ")));
        if (addr > 0)
            mergeList(m_civ[addr], intList(o.value(QLatin1String("models"))), prepend);
    }
    for (const QJsonValue& v : id.value(QLatin1String("elecraftIds")).toArray())
        if (!m_elecraftIds.contains(v.toString()))
            m_elecraftIds.append(v.toString());

    QVector<UsbSignature> usb;
    for (const QJsonValue& v : root.value(QLatin1String("usb")).toArray()) {
        const QJsonObject o = v.toObject();
        UsbSignature s;
        bool okV = false, okP = false;
        s.vid = static_cast<quint16>(o.value(QLatin1String("vid")).toString().toUInt(&okV, 16));
        s.pid = static_cast<quint16>(o.value(QLatin1String("pid")).toString().toUInt(&okP, 16));
        if (!okV || !okP)
            continue;
        s.label      = o.value(QLatin1String("label")).toString();
        s.token      = o.value(QLatin1String("token")).toString();
        s.catHint    = o.value(QLatin1String("catHint")).toString();
        s.protocol   = o.value(QLatin1String("protocol")).toString();
        s.evidence   = o.value(QLatin1String("evidence")).toString();
        s.baud       = o.value(QLatin1String("baud")).toInt();
        s.confidence = o.value(QLatin1String("confidence")).toInt();
        usb.append(s);
    }
    m_usb = prepend ? usb + m_usb : m_usb + usb;

    QVector<NameToken> names;
    for (const QJsonValue& v : root.value(QLatin1String("names")).toArray()) {
        const QJsonObject o = v.toObject();
        NameToken t;
        t.token = o.value(QLatin1String("token")).toString();
        t.label = o.value(QLatin1String("label")).toString();
        t.baud  = o.value(QLatin1String("baud")).toInt();
        t.civ   = parseNumber(o.value(QLatin1String("civ")));
        if (!t.token.isEmpty())
            names.append(t);
    }
    m_names = prepend ? names + m_names : m_names + names;

    QVector<InterfaceToken> ifaces;
    for (const QJsonValue& v : root.value(QLatin1String("interfaces")).toArray()) {
        const QJsonObject o = v.toObject();
        InterfaceToken t{o.value(QLatin1String("token")).toString(),
                         o.value(QLatin1String("label")).toString()};
        if (!t.token.isEmpty())
            ifaces.append(t);
    }
    m_interfaces = prepend ? ifaces + m_interfaces : m_interfaces + ifaces;
    return true;
}

// ── domande ─────────────────────────────────────────────────────────────────

const DecoPortRadioLibrary::Model* DecoPortRadioLibrary::model(int number) const
{
    const int i = m_byNumber.value(number, -1);
    return i >= 0 ? &m_models.at(i) : nullptr;
}

QVector<const DecoPortRadioLibrary::Model*> DecoPortRadioLibrary::search(const QString& text) const
{
    const QString needle = normalised(text);
    QVector<const Model*> out;
    for (const Model& m : m_models) {
        if (needle.isEmpty()
            || normalised(m.mfg + m.name).contains(needle)
            || normalised(m.symbol).contains(needle)
            || QString::number(m.model) == needle)
            out.append(&m);
    }
    return out;
}

int DecoPortRadioLibrary::resolve(const QString& text, QString* why) const
{
    const QString t = text.trimmed();
    bool isNumber = false;
    const int n = t.toInt(&isNumber);
    if (isNumber) {
        if (model(n))
            return n;
        if (why)
            *why = QStringLiteral("il modello %1 non e' nella libreria").arg(n);
        return 0;
    }

    const QString needle = normalised(t);
    if (needle.isEmpty()) {
        if (why)
            *why = QStringLiteral("nome vuoto");
        return 0;
    }
    // Prima l'uguaglianza piena: "FT-991" e' l'FT-991 anche se esiste
    // l'FT-991A in un'altra libreria.
    QVector<const Model*> exact;
    for (const Model& m : m_models) {
        if (normalised(m.name) == needle || normalised(m.mfg + m.name) == needle
            || normalised(m.symbol) == needle)
            exact.append(&m);
    }
    if (exact.size() == 1)
        return exact.first()->model;

    const QVector<const Model*> partial = exact.isEmpty() ? search(t) : exact;
    if (partial.size() == 1)
        return partial.first()->model;
    if (why) {
        if (partial.isEmpty()) {
            *why = QStringLiteral("nessuna radio si chiama \"%1\"").arg(t);
        } else {
            QStringList names;
            for (int i = 0; i < partial.size() && i < 8; ++i)
                names.append(QStringLiteral("%1 (%2)").arg(partial.at(i)->label())
                                  .arg(partial.at(i)->model));
            if (partial.size() > 8)
                names.append(QStringLiteral("altre %1").arg(partial.size() - 8));
            *why = QStringLiteral("\"%1\" e' ambiguo: %2").arg(t, names.join(QLatin1String(", ")));
        }
    }
    return 0;
}

int DecoPortRadioLibrary::defaultCivAddress(int number) const
{
    for (auto it = m_civ.constBegin(); it != m_civ.constEnd(); ++it)
        if (it.value().contains(number))
            return it.key();
    return 0;
}

QString DecoPortRadioLibrary::protocolOf(int number) const
{
    const Model* m = model(number);
    return m ? m->protocol : QString();
}
