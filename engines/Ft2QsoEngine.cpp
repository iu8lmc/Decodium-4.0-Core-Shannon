// Ft2QsoEngine.cpp — implementation. See Ft2QsoEngine.hpp for design notes.

#include "Ft2QsoEngine.hpp"

#include <QRegularExpression>
#include <QStringList>
#include <QTimer>
#include <algorithm>
#include <cstring>

namespace decodium::ft2 {

namespace {

// --------------------------------------------------------------------------
// Local helpers (file-private).
// --------------------------------------------------------------------------

QString upperBaseCall(QString const& raw) {
    QString s = raw.trimmed().toUpper();
    int slash = s.indexOf('/');
    if (slash > 0) s = s.left(slash);
    return s;
}

bool looksLikeCallsign(QString const& tok) {
    if (tok.size() < 3 || tok.size() > 11) return false;
    bool hasDigit = false, hasLetter = false;
    for (QChar c : tok) {
        if (c.isDigit()) hasDigit = true;
        else if (c.isLetter()) hasLetter = true;
        else if (c != '/') return false;
    }
    return hasDigit && hasLetter;
}

bool looksLikeGrid4(QString const& tok) {
    if (tok.size() != 4) return false;
    return tok[0].isLetter() && tok[1].isLetter()
        && tok[2].isDigit() && tok[3].isDigit();
}

bool looksLikeReport(QString const& tok) {
    if (tok.size() < 2 || tok.size() > 4) return false;
    QChar c0 = tok[0];
    int start = 0;
    if (c0 == '+' || c0 == '-') start = 1;
    else if (c0.toUpper() == 'R' && tok.size() >= 3
             && (tok[1] == '+' || tok[1] == '-')) start = 2;
    else return false;
    for (int i = start; i < tok.size(); ++i)
        if (!tok[i].isDigit()) return false;
    return true;
}

bool isSignoffToken(QString const& tok) {
    QString u = tok.toUpper();
    return u == "73" || u == "RR73" || u == "RRR";
}

bool isRogerToken(QString const& tok) {
    QString u = tok.toUpper();
    return u == "RR73" || u == "RRR" || (u.startsWith('R') && u.size() >= 3);
}

QString formatReportFromSnr(int snr) {
    return snr >= 0
        ? QStringLiteral("+%1").arg(snr, 2, 10, QChar('0'))
        : QStringLiteral("-%1").arg(-snr, 2, 10, QChar('0'));
}

quint64 fingerprintMessage(QString const& body) {
    // FNV-1a 64-bit on a normalized (collapsed-whitespace, upper-case) body.
    QString norm = body.simplified().toUpper();
    quint64 h = 0xcbf29ce484222325ULL;
    QByteArray ba = norm.toUtf8();
    for (auto c : ba) {
        h ^= static_cast<unsigned char>(c);
        h *= 0x100000001b3ULL;
    }
    return h;
}

} // namespace

// ==========================================================================
// Construction / config
// ==========================================================================

Ft2QsoEngine::Ft2QsoEngine(EngineConfig const& cfg, QObject* parent)
    : QObject(parent)
    , m_cfg(cfg)
{
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(std::chrono::duration_cast<std::chrono::milliseconds>(kEngineTickInterval).count());
    connect(m_tickTimer, &QTimer::timeout, this, &Ft2QsoEngine::tick);
    m_tickTimer->start();

    emit engineDiagnostic(QStringLiteral("[Ft2Engine] start myCall=%1 grid=%2 multiAnswer=%3")
                          .arg(m_cfg.myCall).arg(m_cfg.myGrid).arg(m_cfg.multiAnswerMode));
}

Ft2QsoEngine::~Ft2QsoEngine() = default;

void Ft2QsoEngine::setConfig(EngineConfig const& cfg) {
    bool callChanged = (cfg.myBaseCall != m_cfg.myBaseCall);
    m_cfg = cfg;
    if (callChanged) {
        // Identity changed mid-session — abort whatever QSO was in flight.
        abortQso(QStringLiteral("config: myCall changed"));
    }
    emit engineDiagnostic(QStringLiteral("[Ft2Engine] cfg update myCall=%1 grid=%2")
                          .arg(m_cfg.myCall).arg(m_cfg.myGrid));
}

// ==========================================================================
// Decode parsing
// ==========================================================================

std::optional<DecodeRow> Ft2QsoEngine::parseRawRow(QString const& raw) const {
    // Accept the canonical pipe-separated format produced by the Bridge:
    //   "time|snr|dt|df|message|aptype|quality|freq"
    // (Bridge::parseFt8Row outputs a QStringList; we ask the Bridge to join
    // with '|' before calling feed(). For tests we synthesize the same.)
    QStringList fields = raw.split(QChar('|'));
    if (fields.size() < 8) return std::nullopt;

    DecodeRow row;
    row.timeToken     = fields[0].trimmed();
    row.snr           = fields[1].trimmed().toInt();
    row.dt            = fields[2].trimmed().toDouble();
    row.df            = fields[3].trimmed().toInt();
    row.message       = fields[4].trimmed();
    row.aptype        = fields[5].trimmed();
    row.lowConfidence = (fields[6].trimmed() == QStringLiteral("?"));
    row.audioHz       = fields[7].trimmed().toInt();
    if (row.audioHz == 0) row.audioHz = row.df;
    row.receivedAt    = clockNow();
    if (row.message.isEmpty()) return std::nullopt;

    enrichDecodeMeta(row);
    return row;
}

void Ft2QsoEngine::enrichDecodeMeta(DecodeRow& row) const {
    // Tokenize the trimmed message body. WSJT-X FT8/FT4/FT2 messages are at
    // most 4 meaningful tokens: "<TO> <FROM> <PAYLOAD>" or "CQ <FROM> <GRID>".
    QStringList toks = row.message.simplified().split(' ', Qt::SkipEmptyParts);
    if (toks.isEmpty()) return;

    QString const myBase = m_cfg.myBaseCall.toUpper();

    // CQ detection — first token "CQ" (optionally "CQ DX" / "CQ <DIR>").
    if (toks[0].toUpper() == QStringLiteral("CQ")) {
        row.isCq = true;
        // Skip optional CQ direction qualifier (CQ DX, CQ EU, CQ NA, ...).
        int callIdx = 1;
        if (toks.size() >= 3 && toks[1].size() <= 3 && !looksLikeCallsign(toks[1])) {
            callIdx = 2;
        }
        if (callIdx < toks.size() && looksLikeCallsign(toks[callIdx])) {
            row.fromCall = upperBaseCall(toks[callIdx]);
        }
        if (callIdx + 1 < toks.size() && looksLikeGrid4(toks[callIdx + 1])) {
            row.grid = toks[callIdx + 1].toUpper();
        }
        // CQ messages are not directedToMe by definition.
        return;
    }

    // Standard form: "<TO> <FROM> <PAYLOAD...>".
    // Decodium 3.0 (widgets/mainwindow.cpp:14415-14424) accepts both the
    // canonical order and the reversed order "<FROM> <TO> <PAYLOAD>" — some
    // remote stations or non-standard message generators swap the two calls.
    // Mirror that permissive behavior so the engine engages the QSO either way.
    if (toks.size() >= 2) {
        QString tok0 = upperBaseCall(toks[0]);
        QString tok1 = upperBaseCall(toks[1]);
        QString toTok;
        QString fromTok;
        if (!myBase.isEmpty() && tok0 == myBase) {
            toTok   = tok0;
            fromTok = tok1;
            row.directedToMe = true;
        } else if (!myBase.isEmpty() && tok1 == myBase) {
            toTok   = tok1;     // reversed form: our call is in slot 1
            fromTok = tok0;
            row.directedToMe = true;
        } else {
            toTok   = tok0;
            fromTok = tok1;
            row.directedToMe = false;
        }
        row.fromCall = fromTok;

        // Payload analysis: grid, report or signoff.
        for (int i = 2; i < toks.size(); ++i) {
            QString const& t = toks[i];
            if (looksLikeGrid4(t)) {
                row.grid = t.toUpper();
            } else if (looksLikeReport(t)) {
                QString u = t.toUpper();
                if (u.startsWith('R')) {
                    row.hasRogerPrefix = true;
                    row.report = u.mid(1);
                } else {
                    row.report = u;
                }
            } else if (isSignoffToken(t)) {
                // Track via report field as a sentinel — used by state logic.
                row.report = t.toUpper();
            } else if (isRogerToken(t)) {
                row.hasRogerPrefix = true;
            }
        }
    }
}

DecodeKey Ft2QsoEngine::makeKey(DecodeRow const& row) const {
    DecodeKey k;
    k.fromCall        = row.fromCall;
    k.msgFingerprint  = fingerprintMessage(row.message);
    k.freqBucket      = row.audioHz / 20;
    return k;
}

bool Ft2QsoEngine::isDuplicate(DecodeKey const& key) {
    auto it = m_dedup.find(key);
    if (it == m_dedup.end()) {
        m_dedup.emplace(key, clockNow());
        return false;
    }
    // Within the freshness window the same physical decode is a duplicate.
    if ((clockNow() - it->second) < kFreshnessWindow) return true;
    it->second = clockNow();
    return false;
}

void Ft2QsoEngine::pruneStaleDedup() {
    TimePoint const now = clockNow();
    for (auto it = m_dedup.begin(); it != m_dedup.end(); ) {
        if ((now - it->second) > kFreshnessWindow * 2) it = m_dedup.erase(it);
        else ++it;
    }
}

// ==========================================================================
// Caller queue
// ==========================================================================

void Ft2QsoEngine::enqueueCaller(DecodeRow const& row, bool directReply) {
    if (row.fromCall.isEmpty()) return;
    if (inCooldown(row.fromCall)) return;
    if (row.fromCall == m_cfg.myBaseCall) return;

    // Replace any existing entry for the same call (refresh SNR / freq).
    // A direct reply upgrades a previously-CQ entry but never downgrades it.
    for (auto& e : m_callerQueue) {
        if (e.call == row.fromCall) {
            e.snr     = row.snr;
            e.dt      = row.dt;
            e.audioHz = row.audioHz;
            if (!row.grid.isEmpty()) e.grid = row.grid;
            e.enqueuedAt = clockNow();
            if (directReply) e.directReply = true;
            emit callerQueueChanged();
            return;
        }
    }

    if (m_callerQueue.size() >= kCallerQueueCapacity) {
        m_callerQueue.pop_front(); // drop oldest, FIFO eviction
    }

    CallerEntry entry;
    entry.call        = row.fromCall;
    entry.grid        = row.grid;
    entry.snr         = row.snr;
    entry.dt          = row.dt;
    entry.audioHz     = row.audioHz;
    entry.enqueuedAt  = clockNow();
    entry.directReply = directReply;
    m_callerQueue.push_back(entry);
    emit callerQueueChanged();

    emit engineDiagnostic(QStringLiteral("[Ft2Engine] queue+ %1 SNR=%2 direct=%3 (size=%4)")
                          .arg(entry.call).arg(entry.snr).arg(directReply ? 1 : 0).arg(m_callerQueue.size()));
}

std::optional<CallerEntry> Ft2QsoEngine::popBestCaller() {
    if (m_callerQueue.empty()) return std::nullopt;
    auto best = m_callerQueue.begin();
    for (auto it = m_callerQueue.begin() + 1; it != m_callerQueue.end(); ++it) {
        if (it->betterThan(*best)) best = it;
    }
    CallerEntry chosen = *best;
    m_callerQueue.erase(best);
    emit callerQueueChanged();
    return chosen;
}

void Ft2QsoEngine::purgeCaller(QString const& call) {
    auto before = m_callerQueue.size();
    m_callerQueue.erase(
        std::remove_if(m_callerQueue.begin(), m_callerQueue.end(),
                       [&](CallerEntry const& e) { return e.call == call; }),
        m_callerQueue.end());
    if (m_callerQueue.size() != before) emit callerQueueChanged();
}

// ==========================================================================
// Cooldown
// ==========================================================================

void Ft2QsoEngine::markCooldown(QString const& call) {
    if (call.isEmpty()) return;
    m_cooldown[call] = clockNow() + kCooldown73;
}

bool Ft2QsoEngine::inCooldown(QString const& call) const {
    auto it = m_cooldown.find(call);
    if (it == m_cooldown.end()) return false;
    return clockNow() < it->second;
}

// ==========================================================================
// Public inputs
// ==========================================================================

FeedOutcome Ft2QsoEngine::feed(QStringList const& rawRows) {
    ++m_eventCounter;
    pruneStaleDedup();

    FeedOutcome agg;
    for (QString const& raw : rawRows) {
        auto parsed = parseRawRow(raw);
        if (!parsed) continue;
        DecodeKey key = makeKey(*parsed);
        if (isDuplicate(key)) continue;
        bool advanced = dispatchDecodeToState(*parsed);
        agg.accepted = true;
        if (advanced) {
            agg.triggeredAdvance = true;
            agg.newCurrentTx = m_currentTx;
            agg.newDxCall    = dxCall();
        }
    }
    return agg;
}

FeedOutcome Ft2QsoEngine::feedParsed(std::vector<DecodeRow> const& rows) {
    ++m_eventCounter;
    pruneStaleDedup();

    FeedOutcome agg;
    for (DecodeRow row : rows) {
        if (!row.message.isEmpty()) enrichDecodeMeta(row);
        if (row.receivedAt == TimePoint{}) row.receivedAt = clockNow();
        DecodeKey key = makeKey(row);
        if (isDuplicate(key)) continue;
        bool advanced = dispatchDecodeToState(row);
        agg.accepted = true;
        if (advanced) {
            agg.triggeredAdvance = true;
            agg.newCurrentTx = m_currentTx;
            agg.newDxCall    = dxCall();
        }
    }
    return agg;
}

void Ft2QsoEngine::tick() {
    // Watchdog: stuck states fall back to Idle (or CallingCq if TX enabled).
    if (watchdogExpired()) {
        QString reason = QStringLiteral("watchdog: %1 exceeded budget").arg(stateLabel());
        if (m_txEnabled) {
            transitionTo(state::CallingCq{ clockNow(), 0 }, reason);
            QString tx = buildTxMessage(6, {}, {}, {}, {}, false);
            requestTx(6, tx);
        } else {
            transitionTo(state::Idle{}, reason);
        }
        return;
    }

    // Closing -> Idle: finalize log emission then transition.
    if (auto* closing = std::get_if<state::Closing>(&m_state)) {
        if (!closing->loggedQso) {
            emit qsoLogged(closing->partnerCall, closing->partnerGrid,
                           closing->sentReport, closing->receivedReport,
                           QStringLiteral("FT2"));
            markCooldown(closing->partnerCall);
            purgeCaller(closing->partnerCall);
            closing->loggedQso = true;
            emit engineDiagnostic(QStringLiteral("[Ft2Engine] qsoLogged %1")
                                  .arg(closing->partnerCall));
        }
        // Hold in Closing for at least one slot so the closing TX (TX4/TX5)
        // gets transmitted once before we switch the legacy backend to CQ.
        if ((clockNow() - closing->closedAt) < kCloseDrainAfter) return;
        transitionTo(state::Idle{}, QStringLiteral("closing complete"));
        // Drain next caller if any and TX still enabled.
        if (m_txEnabled && m_cfg.multiAnswerMode) {
            if (auto next = popBestCaller()) {
                state::ReplyingTx1 r;
                r.partnerCall = next->call;
                r.partnerGrid = next->grid;
                r.enteredAt   = clockNow();
                transitionTo(r, QStringLiteral("auto-drain caller queue"));
                requestTx(2, buildTxMessage(2, r.partnerCall, r.partnerGrid,
                                            QStringLiteral("%1").arg(next->snr, 0, 10), {}, false));
            } else {
                // Nothing else to call — go back to CQ.
                state::CallingCq cq{ clockNow(), 0 };
                transitionTo(cq, QStringLiteral("auto-CQ after QSO"));
                requestTx(6, buildTxMessage(6, {}, {}, {}, {}, false));
            }
        }
        return;
    }

    // CallingCq idle drain: if a caller arrived while we were CQing, engage.
    if (auto* cq = std::get_if<state::CallingCq>(&m_state)) {
        Q_UNUSED(cq);
        if (m_txEnabled && !m_callerQueue.empty()) {
            if (auto next = popBestCaller()) {
                state::ReplyingTx1 r;
                r.partnerCall = next->call;
                r.partnerGrid = next->grid;
                r.enteredAt   = clockNow();
                transitionTo(r, QStringLiteral("CQ engage caller %1").arg(next->call));
                requestTx(2, buildTxMessage(2, r.partnerCall, r.partnerGrid,
                                            QStringLiteral("%1").arg(next->snr, 0, 10), {}, false));
            }
        }
    }
}

void Ft2QsoEngine::doubleClick(QString const& fromCall,
                               QString const& grid,
                               int            audioHz,
                               int            snr,
                               int            requestedTx) {
    QString call = upperBaseCall(fromCall);
    if (call.isEmpty()) return;

    // Explicit user action implies TX-enable (matches WSJT-X behavior).
    if (!m_txEnabled) {
        m_txEnabled = true;
        emit engineDiagnostic(QStringLiteral("[Ft2Engine] enableTx=true (doubleClick)"));
    }

    purgeCaller(call); // explicit engagement supersedes the queue entry
    QString const partnerGrid = grid.toUpper();
    QString const sentReport = formatReportFromSnr(snr);
    int tx = (requestedTx >= 1 && requestedTx <= 5)
        ? requestedTx
        : 1;

    switch (tx) {
    case 1: {
        state::ReplyingTx1 r;
        r.partnerCall = call;
        r.partnerGrid = partnerGrid;
        r.enteredAt   = clockNow();
        transitionTo(r, QStringLiteral("doubleClick %1 TX1").arg(call));
        requestTx(1, buildTxMessage(1, r.partnerCall, r.partnerGrid, {}, {}, false));
        break;
    }
    case 2: {
        state::AwaitingReport r;
        r.partnerCall = call;
        r.partnerGrid = partnerGrid;
        r.sentReport  = sentReport;
        r.enteredAt   = clockNow();
        transitionTo(r, QStringLiteral("doubleClick %1 TX2").arg(call));
        requestTx(2, buildTxMessage(2, r.partnerCall, r.partnerGrid, r.sentReport, {}, false));
        break;
    }
    case 3: {
        state::AwaitingRRR r;
        r.partnerCall = call;
        r.sentReport  = sentReport;
        r.receivedReport = sentReport;
        r.enteredAt   = clockNow();
        transitionTo(r, QStringLiteral("doubleClick %1 TX3").arg(call));
        requestTx(3, buildTxMessage(3, r.partnerCall, {}, r.sentReport, r.receivedReport, false));
        break;
    }
    default: {
        state::Closing c;
        c.partnerCall = call;
        c.partnerGrid = partnerGrid;
        c.sentReport = sentReport;
        c.receivedReport = sentReport;
        c.closedAt = clockNow();
        transitionTo(c, QStringLiteral("doubleClick %1 TX%2").arg(call).arg(tx));
        requestTx(tx, buildTxMessage(tx, c.partnerCall, c.partnerGrid, c.sentReport, c.receivedReport, tx == 4));
        break;
    }
    }
    Q_UNUSED(audioHz);
}

void Ft2QsoEngine::enableTx(bool on) {
    m_txEnabled = on;
    emit engineDiagnostic(QStringLiteral("[Ft2Engine] enableTx=%1").arg(on));
    if (!on) {
        // Just stop transmitting; keep engine state.
        return;
    }
    // TX enabled from Idle → start CQ.
    if (std::holds_alternative<state::Idle>(m_state)) {
        state::CallingCq cq{ clockNow(), 0 };
        transitionTo(cq, QStringLiteral("enableTx -> CQ"));
        requestTx(6, buildTxMessage(6, {}, {}, {}, {}, false));
    }
}

void Ft2QsoEngine::abortQso(QString const& reason) {
    transitionTo(state::Idle{}, QStringLiteral("abort: %1").arg(reason));
}

QString Ft2QsoEngine::dxCall() const {
    return std::visit([](auto const& v) -> QString {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, state::ReplyingTx1>     ) return v.partnerCall;
        else if constexpr (std::is_same_v<T, state::AwaitingReport> ) return v.partnerCall;
        else if constexpr (std::is_same_v<T, state::AwaitingRRR>    ) return v.partnerCall;
        else if constexpr (std::is_same_v<T, state::AwaitingSignoff>) return v.partnerCall;
        else if constexpr (std::is_same_v<T, state::Closing>        ) return v.partnerCall;
        else                                                          return QString{};
    }, m_state);
}

// ==========================================================================
// State machine — single dispatcher driven by std::visit on the active state.
// ==========================================================================

bool Ft2QsoEngine::dispatchDecodeToState(DecodeRow const& row) {
    bool advanced = false;

    std::visit([&](auto& s) {
        using T = std::decay_t<decltype(s)>;

        // ----------------------------------------------------------------
        // Idle: collect callers; engage on directedToMe Tx1.
        // ----------------------------------------------------------------
        if constexpr (std::is_same_v<T, state::Idle>) {
            if (row.isCq) {
                enqueueCaller(row);
                return;
            }
            if (row.directedToMe && !row.fromCall.isEmpty()) {
                if (inCooldown(row.fromCall)) return;
                state::AwaitingRRR next;
                if (!row.report.isEmpty() && row.hasRogerPrefix) {
                    next.partnerCall    = row.fromCall;
                    next.receivedReport = row.report;
                    next.sentReport     = QStringLiteral("%1").arg(row.snr);
                    next.enteredAt      = clockNow();
                    transitionTo(next, QStringLiteral("Idle: caught R+report"));
                    requestTx(4, buildTxMessage(4, next.partnerCall, {}, next.sentReport, next.receivedReport, true));
                    advanced = true;
                    return;
                }
                if (!row.report.isEmpty()) {
                    state::AwaitingRRR n;
                    n.partnerCall    = row.fromCall;
                    n.receivedReport = row.report;
                    n.sentReport     = QStringLiteral("%1").arg(row.snr);
                    n.enteredAt      = clockNow();
                    transitionTo(n, QStringLiteral("Idle: caught report"));
                    requestTx(3, buildTxMessage(3, n.partnerCall, {}, n.sentReport, n.receivedReport, false));
                    advanced = true;
                    return;
                }
                // Grid only: TX1 from them — go ReplyingTx1 -> AwaitingReport.
                state::AwaitingReport ar;
                ar.partnerCall = row.fromCall;
                ar.partnerGrid = row.grid;
                ar.sentReport  = QStringLiteral("%1").arg(row.snr);
                ar.enteredAt   = clockNow();
                transitionTo(ar, QStringLiteral("Idle: engaged on grid"));
                requestTx(2, buildTxMessage(2, ar.partnerCall, ar.partnerGrid, ar.sentReport, {}, false));
                advanced = true;
            }
        }

        // ----------------------------------------------------------------
        // CallingCq: a caller responding to our CQ → engage.
        // ----------------------------------------------------------------
        else if constexpr (std::is_same_v<T, state::CallingCq>) {
            if (row.directedToMe && !row.fromCall.isEmpty() && !inCooldown(row.fromCall)) {
                purgeCaller(row.fromCall);
                state::AwaitingReport ar;
                ar.partnerCall = row.fromCall;
                ar.partnerGrid = row.grid;
                ar.sentReport  = QStringLiteral("%1").arg(row.snr);
                ar.enteredAt   = clockNow();
                transitionTo(ar, QStringLiteral("CQ answered by %1").arg(row.fromCall));
                requestTx(2, buildTxMessage(2, ar.partnerCall, ar.partnerGrid, ar.sentReport, {}, false));
                advanced = true;
                return;
            }
            // Other directedTo-others CQ answers go to caller queue.
            if (row.isCq || (m_cfg.multiAnswerMode && !row.directedToMe)) {
                if (row.isCq) enqueueCaller(row);
            }
        }

        // ----------------------------------------------------------------
        // ReplyingTx1: waiting for partner's report.
        // ----------------------------------------------------------------
        else if constexpr (std::is_same_v<T, state::ReplyingTx1>) {
            if (!row.directedToMe || row.fromCall != s.partnerCall) {
                // Other directed callers ARE candidates for the next QSO —
                // queue them as direct replies so they outrank generic CQs.
                if (row.directedToMe && !row.fromCall.isEmpty()
                    && row.fromCall != s.partnerCall) {
                    enqueueCaller(row, /*directReply=*/true);
                } else if (row.isCq) {
                    enqueueCaller(row);
                }
                return;
            }
            if (!row.report.isEmpty()) {
                state::AwaitingRRR n;
                n.partnerCall    = s.partnerCall;
                n.receivedReport = row.report;
                n.sentReport     = QStringLiteral("%1").arg(row.snr);
                n.enteredAt      = clockNow();
                transitionTo(n, QStringLiteral("got report from %1").arg(s.partnerCall));
                requestTx(3, buildTxMessage(3, n.partnerCall, {}, n.sentReport, n.receivedReport, false));
                advanced = true;
            }
        }

        // ----------------------------------------------------------------
        // AwaitingReport: we sent TX2; expect partner's R+report (TX3).
        // ----------------------------------------------------------------
        else if constexpr (std::is_same_v<T, state::AwaitingReport>) {
            if (!row.directedToMe || row.fromCall != s.partnerCall) {
                if (row.directedToMe && !row.fromCall.isEmpty()
                    && row.fromCall != s.partnerCall) {
                    enqueueCaller(row, /*directReply=*/true);
                } else if (row.isCq) {
                    enqueueCaller(row);
                }
                return;
            }
            if (row.hasRogerPrefix && !row.report.isEmpty()) {
                // Standard WSJT-X behavior: TX4 (RR73) is the CQer's last TX of the QSO.
                // Log immediately; tick() finalizes Closing -> CQ. We do NOT linger in
                // AwaitingSignoff waiting for partner's 73 (it would re-flood RR73).
                state::Closing c;
                c.partnerCall    = s.partnerCall;
                c.partnerGrid    = s.partnerGrid;
                c.sentReport     = s.sentReport;
                c.receivedReport = row.report;
                c.closedAt       = clockNow();
                transitionTo(c, QStringLiteral("got R+report from %1, sending RR73 once").arg(s.partnerCall));
                requestTx(4, buildTxMessage(4, c.partnerCall, {}, c.sentReport, c.receivedReport, true));
                advanced = true;
                return;
            }
            // Bare report (no R) — partner still acknowledging; resend TX2.
            if (!row.report.isEmpty()) {
                ++s.retries;
                requestTx(2, buildTxMessage(2, s.partnerCall, s.partnerGrid, s.sentReport, {}, false));
            }
        }

        // ----------------------------------------------------------------
        // AwaitingRRR: we sent TX3; expect TX4 (RR73/RRR/73) from partner.
        // ----------------------------------------------------------------
        else if constexpr (std::is_same_v<T, state::AwaitingRRR>) {
            if (!row.directedToMe || row.fromCall != s.partnerCall) {
                if (row.directedToMe && !row.fromCall.isEmpty()
                    && row.fromCall != s.partnerCall) {
                    enqueueCaller(row, /*directReply=*/true);
                } else if (row.isCq) {
                    enqueueCaller(row);
                }
                return;
            }
            QString u = row.report.toUpper();
            if (isSignoffToken(u) || row.hasRogerPrefix) {
                state::Closing c;
                c.partnerCall    = s.partnerCall;
                c.sentReport     = s.sentReport;
                c.receivedReport = s.receivedReport;
                c.closedAt       = clockNow();
                transitionTo(c, QStringLiteral("got signoff/roger from %1").arg(s.partnerCall));
                requestTx(5, buildTxMessage(5, c.partnerCall, {}, c.sentReport, c.receivedReport, false));
                advanced = true;
            }
        }

        // ----------------------------------------------------------------
        // AwaitingSignoff: we sent TX4; expect TX5 (73) or another RR73.
        // ----------------------------------------------------------------
        else if constexpr (std::is_same_v<T, state::AwaitingSignoff>) {
            // Vestigial: no longer entered by normal flow (AwaitingReport now
            // closes directly). If reached via legacy path, just drain to
            // Closing on any partner reply — never resend RR73 (caused 1.0.72
            // regression "ripete troppe volte il 73").
            if (!row.directedToMe || row.fromCall != s.partnerCall) {
                if (row.directedToMe && !row.fromCall.isEmpty()
                    && row.fromCall != s.partnerCall) {
                    enqueueCaller(row, /*directReply=*/true);
                } else if (row.isCq) {
                    enqueueCaller(row);
                }
                return;
            }
            state::Closing c;
            c.partnerCall = s.partnerCall;
            c.closedAt    = clockNow();
            transitionTo(c, QStringLiteral("AwaitingSignoff drain on %1").arg(s.partnerCall));
            advanced = true;
        }

        // ----------------------------------------------------------------
        // Closing: ignore — the tick() routine will finalize.
        // ----------------------------------------------------------------
        else if constexpr (std::is_same_v<T, state::Closing>) {
            if (row.directedToMe && !row.fromCall.isEmpty()
                && row.fromCall != s.partnerCall) {
                enqueueCaller(row, /*directReply=*/true);
            } else if (row.isCq) {
                enqueueCaller(row);
            }
        }
    }, m_state);

    return advanced;
}

void Ft2QsoEngine::transitionTo(StateVariant next, QString const& reason) {
    char const* prevName = stateName(m_state);
    m_state = std::move(next);
    char const* nextName = stateName(m_state);
    emit engineDiagnostic(QStringLiteral("[Ft2Engine] %1 -> %2 (%3)")
                          .arg(prevName).arg(nextName).arg(reason));
    emit dxCallChanged(dxCall());
}

void Ft2QsoEngine::requestTx(int txNum, QString const& formatted) {
    m_currentTx = txNum;
    emit currentTxChanged(txNum);
    if (m_txEnabled) {
        emit txMessageRequested(txNum, formatted);
    }
}

QString Ft2QsoEngine::buildTxMessage(int txNum,
                                     QString const& partnerCall,
                                     QString const& partnerGrid,
                                     QString const& sentReport,
                                     QString const& recvReport,
                                     bool rrr) const {
    Q_UNUSED(partnerGrid);
    QString const me   = m_cfg.myBaseCall;
    QString const grid = m_cfg.myGrid;

    auto fmtReport = [](QString r) -> QString {
        r = r.trimmed();
        if (r.isEmpty()) return QStringLiteral("-15");
        if (r.startsWith('R')) r = r.mid(1);
        if (!r.startsWith('+') && !r.startsWith('-')) r = QStringLiteral("-") + r;
        return r;
    };

    switch (txNum) {
    case 1: // <DX> <ME> <MYGRID>
        return QStringLiteral("%1 %2 %3").arg(partnerCall, me, grid);
    case 2: // <DX> <ME> <REPORT>
        return QStringLiteral("%1 %2 %3").arg(partnerCall, me, fmtReport(sentReport));
    case 3: // <DX> <ME> R<REPORT>
        return QStringLiteral("%1 %2 R%3").arg(partnerCall, me, fmtReport(recvReport));
    case 4: // <DX> <ME> RR73 or RRR
        return QStringLiteral("%1 %2 %3").arg(partnerCall, me, rrr ? QStringLiteral("RR73") : QStringLiteral("RRR"));
    case 5: // <DX> <ME> 73
        return QStringLiteral("%1 %2 73").arg(partnerCall, me);
    case 6: // CQ <ME> <MYGRID>
        return QStringLiteral("CQ %1 %2").arg(me, grid);
    default:
        return QString{};
    }
}

bool Ft2QsoEngine::watchdogExpired() const {
    TimePoint entered{};
    Duration  budget = kWatchdogPerStateMax;
    std::visit([&](auto const& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, state::ReplyingTx1>     ) {
            entered = s.enteredAt;
            // ReplyingTx1 is the slowest state: we are waiting on a remote
            // operator to even acknowledge our call, which can legitimately
            // take 15+ slots in poor propagation. Use a longer budget to
            // avoid aborting QSOs ~1s before the partner finally answers.
            budget  = kWatchdogReplyingTx1;
        }
        else if constexpr (std::is_same_v<T, state::AwaitingReport> ) entered = s.enteredAt;
        else if constexpr (std::is_same_v<T, state::AwaitingRRR>    ) entered = s.enteredAt;
        else if constexpr (std::is_same_v<T, state::AwaitingSignoff>) entered = s.enteredAt;
        // Idle / CallingCq / Closing have no watchdog.
    }, m_state);

    if (entered == TimePoint{}) return false;
    return (clockNow() - entered) > budget;
}

} // namespace decodium::ft2
