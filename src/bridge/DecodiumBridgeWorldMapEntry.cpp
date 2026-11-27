#include "DecodiumBridge.h"
#include "DecodiumBridgeTraceScope.h"
#include "MapIntelligenceService.h"
#include "DxccLookup.h"
#include "Sequencer/MessageTokenRules.hpp"

using decodium::seq::isGridTokenStrict;
using decodium::seq::normalizedMessageTokens;
using decodium::seq::isDirectedCqModifierToken;
QString decodiumBridgeNormalizeWorldMapCall(QString);
QString decodiumBridgeWorldMapCallKey(QString);
bool decodiumBridgeLooksLikeCallsignToken(const QString&);
bool decodiumBridgeUnresolvedGridOnlyDecode(const QString&, QString*);
bool decodiumBridgeFst4wUnresolvedHashDecode(QString const&, QString const&);

void DecodiumBridge::replayWorldMapEntry(const QVariantMap& entry,
                                         bool skipClearedFeedEntry,
                                         int maxAgeSeconds)
{
    MainThreadTraceScope trace(QStringLiteral("replay_world_map_entry"),
                               QStringLiteral("skip_cleared=%1 msg=[%2]")
                                   .arg(skipClearedFeedEntry ? 1 : 0)
                                   .arg(entry.value(QStringLiteral("message")).toString().trimmed()),
                               30);
    if (!worldMapFeedEnabled() || !worldMapConsumerReady()) {
        return;
    }
    if (skipClearedFeedEntry) {
        QString const key = worldMapFeedEntryKey(entry);
        if (!key.isEmpty() && m_worldMapClearedDecodeKeys.contains(key)) {
            return;
        }
    }
    if (entry.value(QStringLiteral("isTx")).toBool()) {
        return;
    }
    if (m_hideGhostDecodes && looksLikeGhostDecode(entry)) {
        return;
    }
    if (!worldMapEntryFreshEnough(entry, maxAgeSeconds)) {
        return;
    }

    QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
    if (message.isEmpty()) {
        return;
    }
    QString const mode = entry.value(QStringLiteral("mode"), m_mode).toString().trimmed();
    if (entry.value(QStringLiteral("partialDecode")).toBool()
        || entry.value(QStringLiteral("unresolvedHash")).toBool()
        || entry.value(QStringLiteral("hasUnresolvedGridOnly")).toBool()
        || decodiumBridgeUnresolvedGridOnlyDecode(message, nullptr)
        || decodiumBridgeFst4wUnresolvedHashDecode(mode, message)) {
        return;
    }
    if (m_mapIntelligenceService) {
        QVariantMap filterEntry = entry;
        QString mapCall = filterEntry.value(QStringLiteral("fromCall"))
                              .toString().trimmed().toUpper();
        if (mapCall.isEmpty()) {
            mapCall = extractDecodedCallsign(message, true).trimmed().toUpper();
        }
        if (!mapCall.isEmpty() && m_dxccLookup && m_dxccLookup->isLoaded()) {
            DxccEntity const entity = m_dxccLookup->lookup(mapCall);
            if (entity.isValid()) {
                filterEntry.insert(QStringLiteral("dxcc"), entity.name);
                filterEntry.insert(QStringLiteral("continent"), entity.continent);
                filterEntry.insert(QStringLiteral("cqZone"), entity.cqZone);
                filterEntry.insert(QStringLiteral("ituZone"), entity.ituZone);
            }
        }
        QString band;
        if (m_bandManager) {
            band = m_bandManager->currentBandLambda().trimmed();
        }
        if (!m_mapIntelligenceService->liveEntryMatchesCurrentFilters(
                filterEntry, static_cast<qint64>(m_frequency), band)) {
            return;
        }
    }
    if (visualFeedsDeferredForTx()) {
        deferWorldMapEntryForTx(entry, skipClearedFeedEntry);
        return;
    }

    QString myGrid = m_grid.trimmed().toUpper();
    if (!isGridTokenStrict(myGrid)) {
        return;
    }
    myGrid = myGrid.left(qMin(6, myGrid.size()));

    QStringList const words = normalizedMessageTokens(message);
    if (words.isEmpty()) {
        return;
    }

    QString const myCallKey = decodiumBridgeWorldMapCallKey(m_callsign);
    auto isMine = [&myCallKey](QString const& call) {
        QString const key = decodiumBridgeWorldMapCallKey(call);
        return !key.isEmpty() && !myCallKey.isEmpty() && key == myCallKey;
    };

    bool const isCqLike = words.first() == QStringLiteral("CQ")
        || words.first() == QStringLiteral("QRZ")
        || words.first() == QStringLiteral("DE");
    bool const isEndOfQso = words.contains(QStringLiteral("73"))
        || words.contains(QStringLiteral("RR73"));

    QString entryGrid = entry.value(QStringLiteral("dxGrid")).toString().trimmed().toUpper();
    if (!isGridTokenStrict(entryGrid)) {
        entryGrid = extractDecodedGrid(message);
    }
    if (isGridTokenStrict(entryGrid)) {
        entryGrid = entryGrid.left(qMin(6, entryGrid.size()));
    } else {
        entryGrid.clear();
    }

    QStringList calls;
    calls.reserve(words.size());
    for (QString const& word : words) {
        if (word == QStringLiteral("CQ") || word == QStringLiteral("QRZ") || word == QStringLiteral("DE")
            || isDirectedCqModifierToken(word)) {
            continue;
        }
        if (decodiumBridgeLooksLikeCallsignToken(word)) {
            QString const call = decodiumBridgeNormalizeWorldMapCall(word);
            if (!call.isEmpty()) {
                calls << call;
            }
        }
    }

    QString entryCall = decodiumBridgeNormalizeWorldMapCall(entry.value(QStringLiteral("fromCall")).toString());
    if (entryCall.isEmpty()) {
        entryCall = decodiumBridgeNormalizeWorldMapCall(entry.value(QStringLiteral("dxCallsign")).toString());
    }
    if (entryCall.isEmpty() && !calls.isEmpty()) {
        entryCall = isCqLike ? calls.first() : calls.last();
    }

    QString gridOwnerCall;
    if (!entryGrid.isEmpty()) {
        if (isCqLike && !calls.isEmpty()) {
            gridOwnerCall = calls.first();
        } else if (!calls.isEmpty()) {
            // Standard directed messages are TO FROM GRID; the locator belongs
            // to the last callsign before the locator, not to the addressee.
            gridOwnerCall = calls.last();
        } else {
            gridOwnerCall = entryCall;
        }
    }

    if (!entryGrid.isEmpty()) {
        if (!gridOwnerCall.isEmpty()) {
            rememberWorldMapGrid(gridOwnerCall, entryGrid);
        }
    }

    auto lookupDxGridFor = [this](QString const& call) {
        QString locator = lookupWorldMapGrid(call);
        if (locator.isEmpty()
            && decodiumBridgeWorldMapCallKey(call) == decodiumBridgeWorldMapCallKey(m_dxCall)
            && isGridTokenStrict(m_dxGrid.trimmed().toUpper())) {
            locator = m_dxGrid.trimmed().toUpper().left(qMin(6, m_dxGrid.trimmed().size()));
        }
        return locator;
    };

    auto gridForMapCall = [&entryGrid, &gridOwnerCall, &lookupDxGridFor](QString const& call) {
        if (!entryGrid.isEmpty()
            && !gridOwnerCall.isEmpty()
            && decodiumBridgeWorldMapCallKey(call) == decodiumBridgeWorldMapCallKey(gridOwnerCall)) {
            return entryGrid;
        }
        return lookupDxGridFor(call);
    };

    static constexpr int roleIncomingToMe = 1;
    static constexpr int roleOutgoingFromMe = 2;
    static constexpr int roleBandOnly = 3;

    auto emitApproximateContact = [this, &myGrid](QString const& call, int role) {
        QString const mapCall = decodiumBridgeNormalizeWorldMapCall(call);
        QString const approxGrid = approximateWorldMapGridForCall(mapCall);
        if (mapCall.isEmpty() || !isGridTokenStrict(approxGrid)) {
            return false;
        }

        if (role == roleOutgoingFromMe) {
            emit worldMapContactAdded(mapCall, myGrid, approxGrid, role);
        } else if (role == roleIncomingToMe) {
            emit worldMapContactAdded(mapCall, approxGrid, myGrid, role);
        } else {
            emit worldMapContactAdded(mapCall, approxGrid, approxGrid, roleBandOnly);
        }
        return true;
    };

    QString remoteCall;
    QString remoteGrid;
    int role = roleBandOnly;

    bool const outgoingFromMe = !isCqLike
        && calls.size() >= 2
        && isMine(calls.at(1))
        && !isMine(calls.at(0));
    bool const incomingToMe = !isCqLike
        && calls.size() >= 2
        && isMine(calls.at(0))
        && !isMine(calls.at(1));

    if (outgoingFromMe) {
        remoteCall = calls.at(0);
        remoteGrid = gridForMapCall(remoteCall);
        role = roleOutgoingFromMe;
        if (isEndOfQso && !remoteCall.isEmpty()) {
            markWorldMapQsoClosed(remoteCall);
            role = roleBandOnly;
        }
        if (role != roleBandOnly && worldMapQsoPathSuppressed(remoteCall)) {
            role = roleBandOnly;
        }
        if (isGridTokenStrict(remoteGrid)) {
            emit worldMapContactAdded(remoteCall,
                                      role == roleOutgoingFromMe ? myGrid : remoteGrid,
                                      remoteGrid,
                                      role);
        } else {
            emitApproximateContact(remoteCall, role);
        }
        return;
    }

    if (incomingToMe) {
        remoteCall = calls.at(1);
        remoteGrid = gridForMapCall(remoteCall);
        role = roleIncomingToMe;
        if (isEndOfQso && !remoteCall.isEmpty()) {
            markWorldMapQsoClosed(remoteCall);
            role = roleBandOnly;
        }
        if (role != roleBandOnly && worldMapQsoPathSuppressed(remoteCall)) {
            role = roleBandOnly;
        }
        if (isGridTokenStrict(remoteGrid)) {
            emit worldMapContactAdded(remoteCall,
                                      remoteGrid,
                                      role == roleIncomingToMe ? myGrid : remoteGrid,
                                      role);
            return;
        }
        emitApproximateContact(remoteCall, role);
        return;
    }

    bool const mapAllMessages = getSetting(QStringLiteral("MapAllMessages"), false).toBool();
    if (!isCqLike && !mapAllMessages) {
        return;
    }

    if (isCqLike && !calls.isEmpty()) {
        remoteCall = calls.first();
        remoteGrid = gridForMapCall(remoteCall);
    } else if (!gridOwnerCall.isEmpty() && !isMine(gridOwnerCall)) {
        remoteCall = gridOwnerCall;
        remoteGrid = gridForMapCall(remoteCall);
    } else if (!entryCall.isEmpty() && !isMine(entryCall)) {
        remoteCall = entryCall;
        remoteGrid = gridForMapCall(remoteCall);
    } else if (calls.size() >= 2 && !isMine(calls.at(1))) {
        remoteCall = calls.at(1);
        remoteGrid = gridForMapCall(remoteCall);
    } else if (!calls.isEmpty() && !isMine(calls.first())) {
        remoteCall = calls.first();
        remoteGrid = gridForMapCall(remoteCall);
    }

    if (isEndOfQso && !remoteCall.isEmpty()) {
        markWorldMapQsoClosed(remoteCall);
    }
    if (remoteCall.isEmpty()) {
        return;
    }
    if (isGridTokenStrict(remoteGrid)) {
        emit worldMapContactAdded(remoteCall, remoteGrid, remoteGrid, roleBandOnly);
        return;
    }
    if (mapAllMessages) {
        emitApproximateContact(remoteCall, roleBandOnly);
    }
}

