#pragma once
#include <QDateTime>
#include <QVariant>

struct QsoLogSnapshot
{
    QString path;
    QDateTime modified;
    qint64 size {-1};
    QVariantList rows;
    QVariantMap stats;
};

QVariantMap qsoStatsFromRows(QVariantList const& rows);
QsoLogSnapshot buildQsoLogSnapshot(QString const& path, QString const& myGrid);
