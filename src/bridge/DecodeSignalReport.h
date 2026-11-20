#pragma once
#include <QString>
int decodeSnrOrDefault(QString const&, int fallback = -10);
QString formatSignalReport(int);
QString normalizeSignalReportText(QString const&);
