#include "AdifExportSanitizer.h"

namespace decodium::adif {

bool isInvalidUnsetValue(const QString& fieldName, const QString& value)
{
    return fieldName.trimmed().compare(QStringLiteral("MY_IOTA"),
                                       Qt::CaseInsensitive) == 0
        && value.trimmed().compare(QStringLiteral("NONE"),
                                   Qt::CaseInsensitive) == 0;
}

QByteArray sanitizeExport(const QByteArray& input)
{
    QByteArray output;
    output.reserve(input.size());

    int cursor = 0;
    while (cursor < input.size()) {
        const int tagStart = input.indexOf('<', cursor);
        if (tagStart < 0) {
            output.append(input.mid(cursor));
            break;
        }
        output.append(input.mid(cursor, tagStart - cursor));

        const int tagEnd = input.indexOf('>', tagStart + 1);
        if (tagEnd < 0) {
            output.append(input.mid(tagStart));
            break;
        }

        QByteArray const specification =
            input.mid(tagStart + 1, tagEnd - tagStart - 1).trimmed();
        const int separator = specification.indexOf(':');
        QByteArray fieldName;
        int valueLength = -1;
        if (separator > 0) {
            fieldName = specification.left(separator).trimmed().toUpper();
            QByteArray lengthText = specification.mid(separator + 1);
            const int typeSeparator = lengthText.indexOf(':');
            if (typeSeparator >= 0) {
                lengthText.truncate(typeSeparator);
            }
            bool ok = false;
            valueLength = lengthText.trimmed().toInt(&ok);
            if (!ok) {
                valueLength = -1;
            }
        }

        const int valueStart = tagEnd + 1;
        const bool completeValue = valueLength >= 0
            && valueLength <= input.size() - valueStart;
        const bool invalidMyIota = fieldName == QByteArrayLiteral("MY_IOTA")
            && completeValue
            && input.mid(valueStart, valueLength).trimmed()
                   .compare(QByteArrayLiteral("NONE"), Qt::CaseInsensitive) == 0;
        if (invalidMyIota) {
            // Leave separators/newlines following the value untouched.
            cursor = valueStart + valueLength;
            continue;
        }

        output.append(input.mid(tagStart, tagEnd - tagStart + 1));
        cursor = tagEnd + 1;
    }
    return output;
}

} // namespace decodium::adif
