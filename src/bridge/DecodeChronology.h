#pragma once
#include <QDateTime>
#include <QVariant>
qint64 decodiumBridgeDecodeDisplaySortMsecs(QVariantMap const&, QDateTime const&);
void sortDecodeEntriesChronologically(QVariantList&);
void decodiumBridgeNormalizeDecodeEntries(QVariantList&, int, QString const&);
