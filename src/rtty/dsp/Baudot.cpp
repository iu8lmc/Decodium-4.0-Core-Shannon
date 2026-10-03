#include "dsp/Baudot.h"

#include <cctype>

namespace decortty::dsp {

namespace {

// Codes are indexed by their numeric value, where bit 0 is the first bit on
// the wire. '\0' marks a code that prints nothing.
constexpr char kLetters[32] = {
    0,    'E',  '\n', 'A',  ' ',  'S',  'I',  'U',
    '\r', 'D',  'R',  'J',  'N',  'F',  'C',  'K',
    'T',  'Z',  'L',  'W',  'H',  'Y',  'P',  'Q',
    'O',  'B',  'G',  0,    'M',  'X',  'V',  0
};

// US amateur figures case: BELL on 00101, apostrophe on 01011.
constexpr char kFiguresUs[32] = {
    0,    '3',  '\n', '-',  ' ',  0x07, '8',  '7',
    '\r', '$',  '4',  '\'', ',',  '!',  ':',  '(',
    '5',  '"',  ')',  '2',  '#',  '6',  '0',  '1',
    '9',  '?',  '&',  0,    '.',  '/',  ';',  0
};

// International ITA2: apostrophe on 00101, BELL on 01011, WRU (ENQ) on 01001.
constexpr char kFiguresIta2[32] = {
    0,    '3',  '\n', '-',  ' ',  '\'', '8',  '7',
    '\r', 0x05, '4',  0x07, ',',  '!',  ':',  '(',
    '5',  '"',  ')',  '2',  0,    '6',  '0',  '1',
    '9',  '?',  '&',  0,    '.',  '/',  ';',  0
};

const char* figuresTable(FiguresSet set)
{
    return set == FiguresSet::Ita2 ? kFiguresIta2 : kFiguresUs;
}

} // namespace

char baudotToChar(uint8_t code, Shift shift, FiguresSet set)
{
    code &= 0x1F;
    return shift == Shift::Letters ? kLetters[code] : figuresTable(set)[code];
}

bool charToBaudot(char c, uint8_t& code, Shift& shift, FiguresSet set)
{
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    for (uint8_t i = 0; i < 32; ++i) {
        if (kLetters[i] != 0 && kLetters[i] == upper) {
            code  = i;
            shift = Shift::Letters;
            return true;
        }
    }
    const char* figs = figuresTable(set);
    for (uint8_t i = 0; i < 32; ++i) {
        if (figs[i] != 0 && figs[i] == upper) {
            code  = i;
            shift = Shift::Figures;
            return true;
        }
    }
    return false;
}

char ShiftState::feed(uint8_t code, FiguresSet set)
{
    code &= 0x1F;

    if (code == kBaudotLtrs) {
        m_shift = Shift::Letters;
        return 0;
    }
    if (code == kBaudotFigs) {
        m_shift = Shift::Figures;
        return 0;
    }
    if (code == kBaudotSpace && m_usos)
        m_shift = Shift::Letters;

    return baudotToChar(code, m_shift, set);
}

std::vector<uint8_t> encodeBaudot(const std::string& text,
                                  Shift  startShift,
                                  Shift& endShift,
                                  FiguresSet set)
{
    std::vector<uint8_t> out;
    out.reserve(text.size());
    Shift current = startShift;

    for (const char c : text) {
        uint8_t code = 0;
        Shift   need = Shift::Letters;
        if (!charToBaudot(c, code, need, set))
            continue;

        // SPACE and the two line codes exist in both cases, so they never
        // justify a shift character of their own.
        const bool caseNeutral = (code == kBaudotSpace || code == kBaudotCr || code == kBaudotLf);

        if (!caseNeutral && need != current) {
            out.push_back(need == Shift::Figures ? kBaudotFigs : kBaudotLtrs);
            current = need;
        }
        out.push_back(code);

        // A receiver running unshift-on-space drops to letters here, so the
        // encoder must track the same assumption or the next figure group
        // would go out without its FIGS.
        if (code == kBaudotSpace)
            current = Shift::Letters;
    }

    endShift = current;
    return out;
}

} // namespace decortty::dsp
