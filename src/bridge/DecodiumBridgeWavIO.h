#pragma once
#include <QString>
#include <QVector>

bool writeMono16WavFile(QString const&, QVector<float> const&, int);
bool writeMono16WavFile(QString const&, QVector<short> const&, int);
