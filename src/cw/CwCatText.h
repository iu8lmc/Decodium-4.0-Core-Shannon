#pragma once

#include <QString>

namespace decodium::cw {

// FT-991/A text memory holds 50 characters, including the end marker.
// Always send at least two characters: Hamlib treats one character as a
// memory-playback request, rather than text (including single typed digits).
inline constexpr int ft991TextLimit = 49;

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
