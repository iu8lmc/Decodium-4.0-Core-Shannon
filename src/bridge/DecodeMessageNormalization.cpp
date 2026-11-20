#include "DecodeMessageNormalization.h"
#include <QRegularExpression>

namespace decodium::decode_message {
QString stripLegacyDecodeAppendage(QString line)
{
    line = line.trimmed();
    if (line.startsWith(QStringLiteral("TU; "))) {
        line = line.mid(4).trimmed();
    }

    int const nbPos = line.indexOf(QChar::Nbsp);
    if (nbPos >= 0) {
        return line.left(nbPos).trimmed();
    }

    static const QRegularExpression decodeHeaderPattern {
        R"(^(\d{4}|\d{6})\s+[-+]?\d+\s+[-+]?\d+(?:\.\d+)?\s+\d+\s+\S\s+)"
    };
    auto const match = decodeHeaderPattern.match(line);
    if (match.hasMatch()) {
        int const payloadStart = match.capturedEnd();
        QString payload = line.mid(payloadStart);
        static const QRegularExpression annotationPattern {R"(\s{2,})"};
        int const annotationPos = payload.indexOf(annotationPattern);
        if (annotationPos >= 0) {
            payload = payload.left(annotationPos);
        }
        return (line.left(payloadStart) + payload.trimmed()).trimmed();
    }

    return line;
}

QString stripDecodeApAnnotation(QString message, QString* aptype)
{
    static const QRegularExpression apSuffixPattern {
        QStringLiteral(R"(\s+a([0-9]+)\s*$)")
    };

    QRegularExpressionMatch const match = apSuffixPattern.match(message);
    if (match.hasMatch()) {
        if (aptype && aptype->trimmed().isEmpty()) {
            *aptype = match.captured(1).trimmed();
        }
        message.truncate(match.capturedStart());
    }
    return message.trimmed();
}

QString canonicalDecodeMessage(QString message)
{
    return stripDecodeApAnnotation(message).simplified();
}

} // namespace decodium::decode_message
