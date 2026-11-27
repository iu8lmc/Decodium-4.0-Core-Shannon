#pragma once

#include <QString>

namespace decodium::cw {

// FT-991/A text memory holds 50 characters, including the end marker.
// Always send at least two characters: Hamlib treats one character as a
// memory-playback request, rather than text (including single typed digits).
inline constexpr int ft991TextLimit = 49;

// CI-V command 0x17 accepts at most 30 characters.  Also pace these chunks
// in CwController: Hamlib's asynchronous FIFO acknowledgement does not mean
// the radio has room for the remainder of a long message yet (issue #109).
inline constexpr int icomTextLimit = 30;

inline int catMorseTextLimit(const QString& manufacturer, bool isFt991)
{
    if (isFt991)
        return ft991TextLimit;
    if (manufacturer.trimmed().compare(QStringLiteral("Icom"), Qt::CaseInsensitive) == 0)
        return icomTextLimit;
    return 0; // Preserve other backends, including NET rigctl, unchanged.
}

inline QString ft991MorsePayload(QString text)
{
    if (text.endsWith(QLatin1Char('}')))
        text.chop(1);
    if (text.trimmed().isEmpty() || text.size() > ft991TextLimit
        || text.contains(QLatin1Char(';')) || text.contains(QLatin1Char('}')))
        return {};
    return text.toUpper() + QLatin1Char('}');
}

} // namespace decodium::cw
