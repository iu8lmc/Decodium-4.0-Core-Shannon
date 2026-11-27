#pragma once
#include <QStringList>

QStringList readTextLinesFromOffset(QString const& path, qint64 startOffset, int maxLines);
QStringList readTextTailLines(QString const& path, qint64 tailBytes, int maxLines);
