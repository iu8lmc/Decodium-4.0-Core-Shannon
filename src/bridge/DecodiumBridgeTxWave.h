#pragma once
#include <QString>
#include <QVector>

QVector<float> buildTxWaveformForMessage(QString const&, QString const&, int, double, QString* = nullptr);
