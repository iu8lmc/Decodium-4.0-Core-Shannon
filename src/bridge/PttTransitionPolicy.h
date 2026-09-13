#pragma once

#include <QString>

namespace decodium {
namespace tx {

enum class PttConfirmationMode
{
    RigFeedback,
    CommandDispatch,
    AudioActivity
};

inline PttConfirmationMode pttConfirmationMode(const QString& pttMethod,
                                                bool catCanPtt)
{
    Q_UNUSED(catCanPtt)
    const QString method = pttMethod.trimmed().toUpper();
    if (method == QStringLiteral("VOX")) {
        return PttConfirmationMode::AudioActivity;
    }
    if (method == QStringLiteral("DTR") || method == QStringLiteral("RTS")) {
        return PttConfirmationMode::CommandDispatch;
    }
    return PttConfirmationMode::RigFeedback;
}

inline bool legacyReportedTxIsAuthoritative(bool bridgeManagedAudioPath)
{
    return !bridgeManagedAudioPath;
}

inline int pttFeedbackTimeoutMs()
{
    return 650;
}

} // namespace tx
} // namespace decodium
