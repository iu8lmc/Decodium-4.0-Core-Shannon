#include "LegacyDecodeText.h"
#include <QFile>
#include <QRegularExpression>

QStringList readTextLinesFromOffset(QString const& path, qint64 startOffset, int maxLines)
{
    if (maxLines <= 0 || startOffset < 0) {
        return {};
    }

    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    qint64 const size = file.size();
    if (startOffset >= size) {
        file.close();
        return {};
    }

    file.seek(startOffset);
    QByteArray data = file.readAll();
    file.close();

    QStringList lines = QString::fromUtf8(data).split(QRegularExpression {R"([\r\n]+)"},
                                                      Qt::SkipEmptyParts);
    if (lines.size() > maxLines) {
        lines = lines.mid(lines.size() - maxLines);
    }
    return lines;
}

QStringList readTextTailLines(QString const& path, qint64 tailBytes, int maxLines)
{
    if (path.trimmed().isEmpty() || tailBytes <= 0 || maxLines <= 0) {
        return {};
    }

    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    qint64 const size = file.size();
    qint64 const startOffset = qMax<qint64>(0, size - tailBytes);
    if (!file.seek(startOffset)) {
        return {};
    }
    if (startOffset > 0) {
        file.readLine();
    }

    QByteArray const data = file.readAll();
    QStringList lines = QString::fromUtf8(data).split(QRegularExpression {R"([\r\n]+)"},
                                                      Qt::SkipEmptyParts);
    if (lines.size() > maxLines) {
        lines = lines.mid(lines.size() - maxLines);
    }
    return lines;
}

