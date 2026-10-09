#pragma once
#include <QString>
#include <QStringList>

namespace decodium::radio {
// A temporary RTTY CAT override must not leak into the next decoder mode
// when the operator's normal CAT mode policy is "None".
class RttyRigModeState {
public:
    void clear() { *this = {}; }
    void enter(const QString& context, const QString& reported) {
        clear();
        const auto mode = reported.trimmed().toUpper();
        const QStringList supported {"USB", "LSB", "DATA-U", "DATA-L", "DIGU", "DIGL",
                                     "RTTY-U", "RTTY-L", "CW", "CW-R", "AM", "FM", "DATA-FM"};
        if (!context.isEmpty() && supported.contains(mode)) {
            context_ = context;
            previous_ = mode;
        }
    }
    void overrideRequested(const QString& context, const QString& mode) {
        if (context == context_ && !previous_.isEmpty()
            && mode.compare(previous_, Qt::CaseInsensitive) != 0)
            overridden_ = true;
    }
    void leave(qint64 now) { until_ = overridden_ ? now + 6000 : 0; }
    QString restoreTarget(const QString& context, const QString& configured, qint64 now) const {
        if (!configured.isEmpty() || context.isEmpty() || context != context_
            || !overridden_ || until_ == 0 || now >= until_)
            return {};
        return previous_;
    }
private:
    QString context_, previous_;
    bool overridden_ = false;
    qint64 until_ = 0;
};
}
