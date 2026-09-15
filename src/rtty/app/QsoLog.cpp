#include "app/QsoLog.h"

#include <QDir>
#include <QTimeZone>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

namespace decortty::app {

QsoLog::QsoLog(QObject* parent)
    : QAbstractListModel(parent)
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    m_path = dir + QStringLiteral("/decortty.adi");

    // Riprende il log della sessione precedente, se c'è: chiudere il programma
    // non deve significare ricominciare da zero.
    if (QFile::exists(m_path))
        importAdif(m_path);
}

int QsoLog::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

QVariant QsoLog::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size())
        return {};

    const QsoRecord& q = m_records.at(index.row());
    switch (role) {
    case StartRole:       return q.start;
    case CallRole:        return q.call;
    case RstSentRole:     return q.rstSent;
    case RstReceivedRole: return q.rstReceived;
    case NameRole:        return q.name;
    case FrequencyRole:   return q.frequencyMhz;
    case ModeRole:        return q.mode;
    case DurationRole:    return q.start.secsTo(q.end);
    default:              return {};
    }
}

QHash<int, QByteArray> QsoLog::roleNames() const
{
    return {
        {StartRole, "start"},        {CallRole, "call"},
        {RstSentRole, "rstSent"},    {RstReceivedRole, "rstReceived"},
        {NameRole, "name"},          {FrequencyRole, "frequencyMhz"},
        {ModeRole, "mode"},          {DurationRole, "durationSeconds"},
    };
}

void QsoLog::setAutoSavePath(const QString& path)
{
    if (path.isEmpty() || path == m_path)
        return;
    m_path = path;
    emit logPathChanged();
}

bool QsoLog::addQso(const QString& call, const QString& rstSent,
                    const QString& rstReceived, const QString& name,
                    const QString& qth, double frequencyMhz,
                    const QString& exchangeSent, const QString& exchangeReceived,
                    const QString& comment)
{
    const QString cleanCall = call.trimmed().toUpper();
    if (cleanCall.isEmpty()) {
        emit errorOccurred(tr("A callsign is the least a contact needs to be logged"));
        return false;
    }

    QsoRecord q;
    q.start            = QDateTime::currentDateTimeUtc();
    q.end              = q.start;
    q.call             = cleanCall;
    q.rstSent          = rstSent.trimmed();
    q.rstReceived      = rstReceived.trimmed();
    q.name             = name.trimmed();
    q.qth              = qth.trimmed();
    q.exchangeSent     = exchangeSent.trimmed();
    q.exchangeReceived = exchangeReceived.trimmed();
    q.frequencyMhz     = frequencyMhz;
    q.comment          = comment.trimmed();

    beginInsertRows({}, m_records.size(), m_records.size());
    m_records.append(q);
    endInsertRows();

    autoSave();
    emit countChanged();
    emit qsoLogged(cleanCall);
    return true;
}

void QsoLog::removeQso(int row)
{
    if (row < 0 || row >= m_records.size())
        return;
    beginRemoveRows({}, row, row);
    m_records.remove(row);
    endRemoveRows();
    autoSave();
    emit countChanged();
}

void QsoLog::clear()
{
    beginResetModel();
    m_records.clear();
    endResetModel();
    autoSave();
    emit countChanged();
}

int QsoLog::timesWorked(const QString& call) const
{
    const QString wanted = call.trimmed().toUpper();
    if (wanted.isEmpty())
        return 0;
    int n = 0;
    for (const QsoRecord& q : m_records)
        if (q.call == wanted)
            ++n;
    return n;
}

QString QsoLog::lastWorked(const QString& call) const
{
    const QString wanted = call.trimmed().toUpper();
    QDateTime latest;
    for (const QsoRecord& q : m_records)
        if (q.call == wanted && (!latest.isValid() || q.start > latest))
            latest = q.start;
    return latest.isValid() ? latest.toString(QStringLiteral("dd MMM yyyy HH:mm")) + QStringLiteral("Z")
                            : QString();
}

// ── ADIF ────────────────────────────────────────────────────────────────────

QString QsoLog::adifField(const QString& name, const QString& value)
{
    if (value.isEmpty())
        return {};
    // In ADIF ogni campo dichiara la propria lunghezza: <CALL:6>IU8LMC. È così
    // che il formato tollera valori con spazi e caratteri qualsiasi senza
    // bisogno di virgolette.
    return QStringLiteral("<%1:%2>%3 ").arg(name).arg(value.length()).arg(value);
}

bool QsoLog::exportAdif(const QString& path) const
{
    const QString target = path.isEmpty() ? m_path : path;
    QFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "DecoRTTY log\n";
    out << "<ADIF_VER:5>3.1.4 ";
    out << adifField(QStringLiteral("PROGRAMID"), QStringLiteral("DecoRTTY"));
    out << adifField(QStringLiteral("PROGRAMVERSION"), QStringLiteral(DECORTTY_VERSION));
    out << "<EOH>\n";

    for (const QsoRecord& q : m_records) {
        out << adifField(QStringLiteral("QSO_DATE"), q.start.toString(QStringLiteral("yyyyMMdd")));
        out << adifField(QStringLiteral("TIME_ON"),  q.start.toString(QStringLiteral("hhmmss")));
        out << adifField(QStringLiteral("TIME_OFF"), q.end.toString(QStringLiteral("hhmmss")));
        out << adifField(QStringLiteral("CALL"), q.call);
        out << adifField(QStringLiteral("MODE"), q.mode);
        if (q.frequencyMhz > 0.0)
            out << adifField(QStringLiteral("FREQ"), QString::number(q.frequencyMhz, 'f', 6));
        out << adifField(QStringLiteral("RST_SENT"), q.rstSent);
        out << adifField(QStringLiteral("RST_RCVD"), q.rstReceived);
        out << adifField(QStringLiteral("NAME"), q.name);
        out << adifField(QStringLiteral("QTH"), q.qth);
        out << adifField(QStringLiteral("STX_STRING"), q.exchangeSent);
        out << adifField(QStringLiteral("SRX_STRING"), q.exchangeReceived);
        out << adifField(QStringLiteral("COMMENT"), q.comment);
        out << "<EOR>\n";
    }
    return true;
}

bool QsoLog::importAdif(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const QString text = QString::fromUtf8(file.readAll());
    // Salta l'intestazione: tutto ciò che precede <EOH> è metadata del file.
    const int headerEnd = text.indexOf(QStringLiteral("<EOH>"), 0, Qt::CaseInsensitive);
    const QString body = headerEnd >= 0 ? text.mid(headerEnd + 5) : text;

    static const QRegularExpression fieldRe(
        QStringLiteral("<([A-Za-z_0-9]+):(\\d+)(?::[A-Za-z])?>"),
        QRegularExpression::CaseInsensitiveOption);

    QVector<QsoRecord> loaded;
    QsoRecord current;
    QString dateText, timeOnText, timeOffText;
    bool any = false;

    int pos = 0;
    while (pos < body.length()) {
        const QRegularExpressionMatch m = fieldRe.match(body, pos);
        if (!m.hasMatch())
            break;

        const QString name = m.captured(1).toUpper();
        const int length   = m.captured(2).toInt();
        const int valueAt  = m.capturedEnd(0);
        if (valueAt + length > body.length())
            break;
        const QString value = body.mid(valueAt, length);
        pos = valueAt + length;

        if (name == QLatin1String("QSO_DATE"))      dateText = value;
        else if (name == QLatin1String("TIME_ON"))  timeOnText = value;
        else if (name == QLatin1String("TIME_OFF")) timeOffText = value;
        else if (name == QLatin1String("CALL"))     { current.call = value.toUpper(); any = true; }
        else if (name == QLatin1String("MODE"))     current.mode = value;
        else if (name == QLatin1String("FREQ"))     current.frequencyMhz = value.toDouble();
        else if (name == QLatin1String("RST_SENT")) current.rstSent = value;
        else if (name == QLatin1String("RST_RCVD")) current.rstReceived = value;
        else if (name == QLatin1String("NAME"))     current.name = value;
        else if (name == QLatin1String("QTH"))      current.qth = value;
        else if (name == QLatin1String("STX_STRING")) current.exchangeSent = value;
        else if (name == QLatin1String("SRX_STRING")) current.exchangeReceived = value;
        else if (name == QLatin1String("COMMENT"))  current.comment = value;

        // <EOR> chiude il collegamento; non è un campo con lunghezza, quindi si
        // cerca a parte.
        const int eor = body.indexOf(QStringLiteral("<EOR>"), pos, Qt::CaseInsensitive);
        const QRegularExpressionMatch next = fieldRe.match(body, pos);
        if (eor >= 0 && (!next.hasMatch() || eor < next.capturedStart(0))) {
            if (any && !current.call.isEmpty()) {
                const QString stamp = dateText + timeOnText;
                current.start = QDateTime::fromString(stamp, QStringLiteral("yyyyMMddhhmmss"));
                current.start.setTimeZone(QTimeZone::UTC);
                current.end = timeOffText.isEmpty()
                                  ? current.start
                                  : QDateTime::fromString(dateText + timeOffText,
                                                          QStringLiteral("yyyyMMddhhmmss"));
                current.end.setTimeZone(QTimeZone::UTC);
                loaded.append(current);
            }
            current = QsoRecord{};
            dateText.clear(); timeOnText.clear(); timeOffText.clear();
            any = false;
            pos = eor + 5;
        }
    }

    beginResetModel();
    m_records = loaded;
    endResetModel();
    emit countChanged();
    return true;
}

void QsoLog::autoSave()
{
    if (!m_path.isEmpty() && !exportAdif(m_path))
        emit errorOccurred(tr("Cannot write the log in %1").arg(m_path));
}

} // namespace decortty::app
