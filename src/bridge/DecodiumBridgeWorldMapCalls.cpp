#include "Sequencer/MessageTokenRules.hpp"
#include "Radio.hpp"
#include <QString>

using decodium::seq::normalizedUsableCallToken;

static QString normalizeWorldMapCall(QString call)
{
    return normalizedUsableCallToken(call);
}

static QString worldMapCallKey(QString call)
{
    QString const normalized = normalizeWorldMapCall(call);
    if (normalized.isEmpty()) {
        return {};
    }

    QString const base = Radio::base_callsign(normalized).trimmed().toUpper();
    return base.isEmpty() ? QString {} : base;
}

QString decodiumBridgeNormalizeWorldMapCall(QString call) { return normalizeWorldMapCall(call); }
QString decodiumBridgeWorldMapCallKey(QString call) { return worldMapCallKey(call); }

