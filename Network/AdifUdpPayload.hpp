#pragma once
#include <QByteArray>

namespace decodium::adif {
// Change only BAND/BAND_RX values. Respect lengths so text containing an
// apparent ADIF tag is never interpreted as another field; retain formatting.
inline QByteArray udpPayload(QByteArray input)
{
    int cursor = 0;
    while (cursor < input.size()) {
        int start = input.indexOf('<', cursor);
        if (start < 0) break;
        int end = input.indexOf('>', start + 1);
        if (end < 0) break;
        auto parts = input.mid(start + 1, end - start - 1).split(':');
        cursor = end + 1;
        if (parts.size() < 2) continue; // EOH/EOR
        bool ok = false;
        int length = parts[1].trimmed().toInt(&ok);
        if (!ok || length < 0 || length > input.size() - cursor) break;
        auto name = parts[0].trimmed().toUpper();
        if (name == "BAND" || name == "BAND_RX") {
            for (int i = cursor; i < cursor + length; ++i) {
                char ch = input.at(i);
                if (ch >= 'A' && ch <= 'Z') input[i] = char(ch + ('a' - 'A'));
            }
        }
        cursor += length;
    }
    return input;
}
}
