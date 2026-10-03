#include "app/MacroModel.h"

#include <QDateTime>

namespace decortty::app {

namespace {

// The default set covers an ordinary QSO and a contest exchange. CR+LF ends
// each over because that is what a mechanical teleprinter expected and what
// every RTTY program still shows correctly.
QVector<QPair<QString, QString>> defaultMacros()
{
    return {
        {QStringLiteral("CQ"),
         QStringLiteral("CQ CQ CQ DE %MYCALL% %MYCALL% %MYCALL% PSE K\r\n")},
        {QStringLiteral("Answer"),
         QStringLiteral("%HISCALL% DE %MYCALL% %MYCALL% K\r\n")},
        {QStringLiteral("Report"),
         QStringLiteral("%HISCALL% DE %MYCALL% UR RST %RST% %RST% NAME %NAME% %NAME% QTH %QTH% %QTH% HW? %HISCALL% DE %MYCALL% K\r\n")},
        {QStringLiteral("73"),
         QStringLiteral("%HISCALL% DE %MYCALL% TNX FER NICE QSO 73 ES GL SK\r\n")},
        {QStringLiteral("Contest"),
         QStringLiteral("%HISCALL% %RST% %SERIAL% %SERIAL% %HISCALL% DE %MYCALL% K\r\n")},
        {QStringLiteral("Again"),
         QStringLiteral("PSE AGN AGN UR CALL? DE %MYCALL% K\r\n")},
        {QStringLiteral("RYRY"),
         QStringLiteral("RYRYRYRYRYRYRYRYRYRYRYRY DE %MYCALL%\r\n")},
        {QStringLiteral("My call"),
         QStringLiteral("DE %MYCALL% %MYCALL% ")},
    };
}

} // namespace

MacroModel::MacroModel(QObject* parent)
    : QAbstractListModel(parent)
{
    resetToDefaults();
}

int MacroModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_macros.size();
}

QVariant MacroModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_macros.size())
        return {};

    const Macro& macro = m_macros.at(index.row());
    switch (role) {
    case LabelRole:    return macro.label;
    case TemplateRole: return macro.text;
    case ExpandedRole: return expand(macro.text);
    default:           return {};
    }
}

QHash<int, QByteArray> MacroModel::roleNames() const
{
    return {
        {LabelRole,    "label"},
        {TemplateRole, "macroTemplate"},
        {ExpandedRole, "expanded"},
    };
}

void MacroModel::refreshExpanded()
{
    if (m_macros.isEmpty())
        return;
    emit dataChanged(index(0), index(m_macros.size() - 1), {ExpandedRole});
}

void MacroModel::setMyCall(const QString& call)
{
    const QString upper = call.toUpper().trimmed();
    if (m_myCall == upper)
        return;
    m_myCall = upper;
    refreshExpanded();
    emit qsoChanged();
}

void MacroModel::setHisCall(const QString& call)
{
    const QString upper = call.toUpper().trimmed();
    if (m_hisCall == upper)
        return;
    m_hisCall = upper;
    refreshExpanded();
    emit qsoChanged();
}

void MacroModel::setRstSent(const QString& rst)
{
    if (m_rstSent == rst)
        return;
    m_rstSent = rst;
    refreshExpanded();
    emit qsoChanged();
}

void MacroModel::setMyName(const QString& name)
{
    const QString upper = name.toUpper().trimmed();
    if (m_myName == upper)
        return;
    m_myName = upper;
    refreshExpanded();
    emit qsoChanged();
}

void MacroModel::setMyQth(const QString& qth)
{
    const QString upper = qth.toUpper().trimmed();
    if (m_myQth == upper)
        return;
    m_myQth = upper;
    refreshExpanded();
    emit qsoChanged();
}

void MacroModel::setStationProfile(const QString& call, const QString& name,
                                   const QString& qth)
{
    const QString normalizedCall = call.toUpper().trimmed();
    const QString normalizedName = name.toUpper().trimmed();
    const QString normalizedQth = qth.toUpper().trimmed();
    if (m_myCall == normalizedCall
        && m_myName == normalizedName
        && m_myQth == normalizedQth) {
        return;
    }

    m_myCall = normalizedCall;
    m_myName = normalizedName;
    m_myQth = normalizedQth;
    refreshExpanded();
    emit qsoChanged();
}

void MacroModel::setSerialNumber(int serial)
{
    serial = qMax(1, serial);
    if (m_serial == serial)
        return;
    m_serial = serial;
    refreshExpanded();
    emit qsoChanged();
}

void MacroModel::incrementSerial()
{
    setSerialNumber(m_serial + 1);
}

QString MacroModel::expand(const QString& templateText) const
{
    QString out = templateText;
    const QDateTime now = QDateTime::currentDateTimeUtc();

    out.replace(QLatin1String("%MYCALL%"),  m_myCall.isEmpty() ? QStringLiteral("NOCALL") : m_myCall);
    out.replace(QLatin1String("%HISCALL%"), m_hisCall);
    out.replace(QLatin1String("%RST%"),     m_rstSent);
    out.replace(QLatin1String("%NAME%"),    m_myName);
    out.replace(QLatin1String("%QTH%"),     m_myQth);
    out.replace(QLatin1String("%SERIAL%"),  QStringLiteral("%1").arg(m_serial, 3, 10, QLatin1Char('0')));
    out.replace(QLatin1String("%TIME%"),    now.toString(QStringLiteral("HHmm")) + QStringLiteral("Z"));
    out.replace(QLatin1String("%DATE%"),    now.toString(QStringLiteral("dd MMM yyyy")).toUpper());

    return out;
}

QString MacroModel::expandAt(int row) const
{
    if (row < 0 || row >= m_macros.size())
        return {};
    return expand(m_macros.at(row).text);
}

void MacroModel::setMacro(int row, const QString& label, const QString& templateText)
{
    if (row < 0 || row >= m_macros.size())
        return;
    m_macros[row].label = label;
    m_macros[row].text  = templateText;
    emit dataChanged(index(row), index(row), {LabelRole, TemplateRole, ExpandedRole});
}

void MacroModel::resetToDefaults()
{
    beginResetModel();
    m_macros.clear();
    for (const auto& [label, text] : defaultMacros())
        m_macros.append(Macro{label, text});
    endResetModel();
}

void MacroModel::load(QSettings& settings)
{
    m_myCall  = settings.value(QStringLiteral("station/call")).toString();
    m_myName  = settings.value(QStringLiteral("station/name")).toString();
    m_myQth   = settings.value(QStringLiteral("station/qth")).toString();
    m_rstSent = settings.value(QStringLiteral("station/rst"), QStringLiteral("599")).toString();
    m_serial  = settings.value(QStringLiteral("station/serial"), 1).toInt();

    const int count = settings.beginReadArray(QStringLiteral("macros"));
    if (count > 0) {
        beginResetModel();
        m_macros.clear();
        for (int i = 0; i < count; ++i) {
            settings.setArrayIndex(i);
            m_macros.append(Macro{settings.value(QStringLiteral("label")).toString(),
                                  settings.value(QStringLiteral("text")).toString()});
        }
        endResetModel();
    }
    settings.endArray();
    emit qsoChanged();
}

void MacroModel::save(QSettings& settings) const
{
    settings.setValue(QStringLiteral("station/call"), m_myCall);
    settings.setValue(QStringLiteral("station/name"), m_myName);
    settings.setValue(QStringLiteral("station/qth"), m_myQth);
    settings.setValue(QStringLiteral("station/rst"), m_rstSent);
    settings.setValue(QStringLiteral("station/serial"), m_serial);

    settings.beginWriteArray(QStringLiteral("macros"), m_macros.size());
    for (int i = 0; i < m_macros.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("label"), m_macros.at(i).label);
        settings.setValue(QStringLiteral("text"), m_macros.at(i).text);
    }
    settings.endArray();
}

} // namespace decortty::app
