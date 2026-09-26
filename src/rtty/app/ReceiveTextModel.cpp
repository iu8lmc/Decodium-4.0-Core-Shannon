#include "app/ReceiveTextModel.h"

#include <algorithm>

namespace decortty::app {

ReceiveTextModel::ReceiveTextModel(QObject* parent)
    : QAbstractListModel(parent)
{
    startLine(false);
}

int ReceiveTextModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_lines.size();
}

QVariant ReceiveTextModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size())
        return {};

    const Line& line = m_lines.at(index.row());
    switch (role) {
    case TextRole:        return line.text;
    case QualityRole:     return line.quality();
    case CorrectedRole:   return line.corrected;
    case TimestampRole:   return line.timestamp;
    case TransmittedRole: return line.transmitted;
    default:              return {};
    }
}

QHash<int, QByteArray> ReceiveTextModel::roleNames() const
{
    return {
        {TextRole,        "lineText"},
        {QualityRole,     "quality"},
        {CorrectedRole,   "corrected"},
        {TimestampRole,   "timestamp"},
        {TransmittedRole, "transmitted"},
    };
}

void ReceiveTextModel::setMaxLines(int lines)
{
    lines = std::clamp(lines, 100, 100000);
    if (m_maxLines == lines)
        return;
    m_maxLines = lines;
    trim();
    emit maxLinesChanged();
}

void ReceiveTextModel::setWrapColumn(int column)
{
    column = std::clamp(column, 32, 250);
    if (m_wrapColumn == column)
        return;
    m_wrapColumn = column;
    emit wrapColumnChanged();
}

void ReceiveTextModel::startLine(bool transmitted)
{
    beginInsertRows({}, m_lines.size(), m_lines.size());
    Line line;
    line.transmitted = transmitted;
    line.timestamp   = QDateTime::currentDateTimeUtc();
    m_lines.append(line);
    endInsertRows();
    trim();
    emit lineAdded();
}

void ReceiveTextModel::touchLast()
{
    const int row = m_lines.size() - 1;
    if (row < 0)
        return;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {TextRole, QualityRole, CorrectedRole});
}

void ReceiveTextModel::trim()
{
    if (m_lines.size() <= m_maxLines)
        return;
    const int excess = m_lines.size() - m_maxLines;
    beginRemoveRows({}, 0, excess - 1);
    m_lines.remove(0, excess);
    endRemoveRows();
}

void ReceiveTextModel::appendCharacter(QChar c, double quality, bool corrected)
{
    if (m_lines.isEmpty() || m_lines.last().transmitted)
        startLine(false);

    // CR and LF both terminate a line. A station sending CR+LF would otherwise
    // leave an empty row between every pair of lines.
    if (c == QLatin1Char('\r') || c == QLatin1Char('\n')) {
        if (!m_lines.last().text.isEmpty())
            startLine(false);
        return;
    }
    if (c == QChar(0x07))   // BELL — audible in the original, silent here
        return;

    Line& line = m_lines.last();
    line.text.append(c);
    line.qualitySum += quality;
    ++line.qualityCount;
    line.corrected = line.corrected || corrected;

    if (line.text.size() >= m_wrapColumn) {
        touchLast();
        startLine(false);
        return;
    }
    touchLast();
}

void ReceiveTextModel::appendTransmitted(QChar c)
{
    if (m_lines.isEmpty() || !m_lines.last().transmitted)
        startLine(true);

    if (c == QLatin1Char('\r') || c == QLatin1Char('\n')) {
        if (!m_lines.last().text.isEmpty())
            startLine(true);
        return;
    }

    Line& line = m_lines.last();
    line.text.append(c);
    if (line.text.size() >= m_wrapColumn) {
        touchLast();
        startLine(true);
        return;
    }
    touchLast();
}

void ReceiveTextModel::clear()
{
    beginResetModel();
    m_lines.clear();
    endResetModel();
    startLine(false);
}

QString ReceiveTextModel::plainText() const
{
    QStringList out;
    out.reserve(m_lines.size());
    for (const Line& line : m_lines)
        out.append(line.text);
    return out.join(QLatin1Char('\n'));
}

} // namespace decortty::app
