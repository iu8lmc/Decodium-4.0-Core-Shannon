#ifndef DECODIUM_ADIF_EXPORT_SANITIZER_H
#define DECODIUM_ADIF_EXPORT_SANITIZER_H

#include <QByteArray>
#include <QString>

namespace decodium::adif {

// Optional ADIF fields must be omitted when unset.  TQSL rejects the
// sentinel value "NONE" for MY_IOTA.
bool isInvalidUnsetValue(const QString& fieldName, const QString& value);

// Remove invalid legacy MY_IOTA sentinels while preserving all other ADIF
// content and formatting.  The active logbook is never modified by this
// function.
QByteArray sanitizeExport(const QByteArray& input);

} // namespace decodium::adif

#endif // DECODIUM_ADIF_EXPORT_SANITIZER_H
