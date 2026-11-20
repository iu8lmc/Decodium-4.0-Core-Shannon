#pragma once
#include <QString>
namespace decodium::decode_message {
QString stripLegacyDecodeAppendage(QString line);
QString stripDecodeApAnnotation(QString message, QString* aptype = nullptr);
QString canonicalDecodeMessage(QString message);
} // namespace decodium::decode_message
