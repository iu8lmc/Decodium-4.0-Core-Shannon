#pragma once
#include <QString>
#include <QStringList>

namespace decodium::decode_colors {
const QStringList& decodeColorPropertyNames();
bool isDecodeColorProperty(const QString&);
QString decodeColorEnabledSettingKey(const QString&);
QString decodeColorBoldSettingKey(const QString&);
bool decodeColorBoldDefault(const QString&);
}
