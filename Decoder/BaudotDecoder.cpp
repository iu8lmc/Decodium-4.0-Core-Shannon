#include "BaudotDecoder.hpp"

namespace decodium {
namespace rtty {

// ITA2 Character Matrix (5-bit index 0-31)
// Indexes map directly to 5-bit binary representations
const char BaudotHandler::LTRS_TABLE[32] = {
    '\0', 'E', '\n', 'A', ' ', 'S', 'I', 'U', 
    '\r', 'D', 'R', 'J', 'N', 'F', 'C', 'K', 
    'T', 'Z', 'L', 'W', 'H', 'Y', 'P', 'Q', 
    'O', 'B', 'G', '\0', 'M', 'X', 'V', '\0'
};

const char BaudotHandler::FIGS_TABLE[32] = {
    '\0', '3', '\n', '-', ' ', '\'', '8', '7', 
    '\r', '\a', '4', '\a', ',', '\a', ':', '(', 
    '5', '+', ')', '2', '#', '6', '0', '1', 
    '9', '?', '\a', '\0', '.', '/', '=', '\0'
};

BaudotHandler::BaudotHandler() 
    : current_rx_state_(ShiftState::Letters),
      current_tx_state_(ShiftState::Letters) 
{
}

void BaudotHandler::reset() {
    current_rx_state_ = ShiftState::Letters;
    current_tx_state_ = ShiftState::Letters;
}

QChar BaudotHandler::decodeSymbol(uint8_t bits) {
    // Mask to just 5 bits
    bits &= 0x1F;

    if (bits == CODE_LTRS) {
        current_rx_state_ = ShiftState::Letters;
        return QChar('\0');
    }
    if (bits == CODE_FIGS) {
        current_rx_state_ = ShiftState::Figures;
        return QChar('\0');
    }

    char c = '\0';
    if (current_rx_state_ == ShiftState::Letters) {
        c = LTRS_TABLE[bits];
    } else {
        c = FIGS_TABLE[bits];
    }
    
    // Ignore internal bells or unmapped
    if (c == '\a') return QChar('\0');

    return QChar(c);
}

QByteArray BaudotHandler::encodeString(const QString& text) {
    QByteArray out;
    QString upperText = text.toUpper();

    for (int i = 0; i < upperText.length(); ++i) {
        char c = upperText[i].toLatin1();
        uint8_t symbol = 0xFF;
        ShiftState requiredShift = current_tx_state_;

        // Special universal control codes
        if (c == ' ') { symbol = CODE_SPC; }
        else if (c == '\r') { symbol = CODE_CR; }
        else if (c == '\n') { symbol = CODE_LF; }
        else {
            // Find in LTRS table
            for (int k = 1; k < 32; ++k) {
                if (k == CODE_LTRS || k == CODE_FIGS) continue;
                if (LTRS_TABLE[k] == c) {
                    symbol = k;
                    requiredShift = ShiftState::Letters;
                    break;
                }
            }
            // If not found, try FIGS table
            if (symbol == 0xFF) {
                for (int k = 1; k < 32; ++k) {
                    if (k == CODE_LTRS || k == CODE_FIGS) continue;
                    if (FIGS_TABLE[k] == c) {
                        symbol = k;
                        requiredShift = ShiftState::Figures;
                        break;
                    }
                }
            }
        }

        if (symbol != 0xFF) {
            if (requiredShift != current_tx_state_) {
                out.append(static_cast<char>(requiredShift == ShiftState::Letters ? CODE_LTRS : CODE_FIGS));
                current_tx_state_ = requiredShift;
            }
            out.append(static_cast<char>(symbol));
        }
    }
    return out;
}

} // namespace rtty
} // namespace decodium
