#pragma once
#include <QString>
int q65PeriodSecondsFromModeLabel(QString const&);
int q65SubmodeIndexFromModeLabel(QString const&);
int q65SamplesPerSymbolAt48k(QString const&);
int fst4PeriodSecondsForMode(QString const&, double);
int fst4SamplesPerSymbolAt12k(int);
int fst4SamplesPerSymbolAt48k(QString const&, double);
int decodiumBridgeFst4SamplesPerSymbol(QString const&, double);
int decodiumBridgeQ65SamplesPerSymbol(QString const&);
int decodiumBridgeQ65SubmodeIndex(QString const&);
