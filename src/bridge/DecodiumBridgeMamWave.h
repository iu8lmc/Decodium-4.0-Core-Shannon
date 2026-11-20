#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

QVector<float> generateMultiStreamFtxWave(QString const& mode, QStringList const& messages, QVector<int> const& frequencies, QString* errorOut = nullptr, QVector<float> const& gains = {});
