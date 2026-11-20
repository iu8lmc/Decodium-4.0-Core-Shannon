#include "DecodiumBridge.h"
#include "DecodeValidationRules.h"
#include "Detector/CallsignHash28.h"
#include "Sequencer/MessageTokenRules.hpp"
#include "DxccLookup.h"
#include "Radio.hpp"
#include "helper_functions.h"
#include <QRegularExpression>
#include <QSet>

using namespace decodium::seq;
using namespace decodium::decode_validation;
void decodiumBridgeGhostLog(QString const&);
bool decodiumBridgeIsStreamingKeyboardMode(QString const&);

bool DecodiumBridge::looksLikeGhostDecode(QVariantMap const& entry) const
{
    // RTTY porta testo libero, non messaggi strutturati: i criteri qui sotto
    // sono tarati sui nominativi dei modi digitali e il controllo sulla forma
    // ITU scarterebbe qualunque parola. Chi legge RTTY vede il testo com'e',
    // errori compresi, ed e' proprio quello che serve.
    // Lo stesso vale per JTTY: i suoi frame hanno gia' passato FEC, CRC e la
    // grammatica completa, e portano anche testo libero.
    if (decodiumBridgeIsStreamingKeyboardMode(entry.value(QStringLiteral("mode")).toString())) {
        return false;
    }

    if (entry.value(QStringLiteral("isTx")).toBool()) return false;
    if (entry.value(QStringLiteral("isB4")).toBool()) return false;
    if (entry.value(QStringLiteral("dxIsWorked")).toBool()) return false;

    // CONFERMA PER I SEGNALI ESTREMI.
    // Sotto i -23 dB il decodificatore prova moltissime ipotesi, e la CRC-14
    // ne lascia passare una ogni 16384 per puro caso: piu' candidati esamini,
    // piu' falsi accetti. E' il prezzo della fase profonda, non un difetto.
    // A quel livello si accetta un nominativo solo se e' gia' stato sentito
    // con segnale affidabile in questa sessione (o e' gia' lavorato, gestito
    // dalle uscite qui sopra): una stazione vera a -25 dB quasi sempre e'
    // gia' passata piu' forte, o si ripetera'; un falso da CRC compare una
    // volta sola. Misurato in aria: col solo decode normale i nominativi visti
    // una volta sola erano il 19%, con la fase profonda sono saliti al 44%.
    //
    // 10 settembre 2026: conferma anche per RIPETIZIONE. Con la sola conferma
    // "gia' sentito forte" una stazione DX che sale dal rumore resta invisibile
    // finche' non supera i -20 dB una volta: in una notte sui 40 m "CQ YB1BZV
    // OI42" e' stato scartato 190 volte, "CQ DV4ZAR PK13" e altri 1375 in
    // tutto, poi sentiti anche a -9. Il falso da CRC non ripete mai lo stesso
    // nominativo in uno slot diverso: se un nominativo debole torna entro dieci
    // minuti in un altro slot, e' una stazione, e da li' passa.
    {
        static QSet<QString> sentitiForte;
        static QHash<QString, QPair<QString, qint64>> sentitiDeboli;   // nominativo -> (slot, ms)
        bool snrLetto = false;
        int const snr = entry.value(QStringLiteral("db")).toString().trimmed().toInt(&snrLetto);
        QString testo = entry.value(QStringLiteral("displayMessage")).toString().trimmed();
        if (testo.isEmpty()) {
            testo = entry.value(QStringLiteral("message")).toString().trimmed();
        }
        const QStringList parole =
            testo.toUpper().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        auto sembraNominativo = [](QString const& w) {
            if (w.size() < 3 || w.size() > 11) return false;
            if (w == QStringLiteral("CQ") || w == QStringLiteral("DE")
                || w == QStringLiteral("73") || w == QStringLiteral("RR73")
                || w == QStringLiteral("RRR") || w == QStringLiteral("TU")) return false;
            static const QRegularExpression locatore {QStringLiteral("^[A-R]{2}[0-9]{2}$")};
            if (locatore.match(w).hasMatch()) return false;
            bool cifra = false, lettera = false;
            for (QChar const c : w) {
                if (c.isDigit()) cifra = true;
                else if (c.isLetter()) lettera = true;
                else if (c != QLatin1Char('/') && c != QLatin1Char('<')
                         && c != QLatin1Char('>')) return false;
            }
            return cifra && lettera;
        };
        if (snrLetto && snr >= -20) {
            // segnale affidabile: da qui in poi questi nominativi sono noti
            for (QString const& w : parole) {
                if (sembraNominativo(w)) sentitiForte.insert(w);
            }
        } else if (snrLetto && snr <= -23) {
            QString const slot = entry.value(QStringLiteral("time")).toString().trimmed();
            qint64 const adessoMs = QDateTime::currentMSecsSinceEpoch();
            static constexpr qint64 kRipetizioneMs = 10 * 60 * 1000;
            bool confermato = false;
            for (QString const& w : parole) {
                if (!sembraNominativo(w)) continue;
                if (sentitiForte.contains(w)) { confermato = true; break; }
                auto const it = sentitiDeboli.constFind(w);
                if (it != sentitiDeboli.constEnd()
                    && it.value().first != slot
                    && adessoMs - it.value().second <= kRipetizioneMs) {
                    // Da qui il nominativo e' noto come uno sentito forte: la
                    // lista viene riclassificata a ogni aggiornamento della UI
                    // e una riga accettata non deve sparire dopo dieci minuti.
                    sentitiForte.insert(w);
                    confermato = true;
                    break;
                }
            }
            // Avvistamento debole (primo o rinfrescato): serve alla conferma
            // del prossimo slot e tiene viva la finestra finche' la stazione
            // continua. Il limite tiene la tabella piccola anche in una notte
            // di fase profonda.
            if (sentitiDeboli.size() > 4096) sentitiDeboli.clear();
            for (QString const& w : parole) {
                if (sembraNominativo(w)) sentitiDeboli.insert(w, qMakePair(slot, adessoMs));
            }
            if (!confermato) return true;
        }
    }

    // NOMINATIVO STRUTTURALMENTE IMPOSSIBILE.
    // Un nominativo vero ha la forma ITU: prefisso, UN blocco di cifre, e un
    // suffisso di sole lettere. La spazzatura che la CRC-14 lascia passare per
    // caso ha invece cifre sparse ovunque (2YJZ88IZ5Q, RFCJ5R218, K89M/T29D4P).
    // E' un test di FORMA, non una soglia: non penalizza i segnali deboli.
    //
    // Misurato su 1062004 nominativi d'archivio: ne boccia 518, lo 0,049%, e
    // sono TUTTI falsi. Passano i prefissi con cifra (4L7T, 9A6NTK, 3B8GL) e i
    // nominativi speciali da evento (EN35UKR, LZ123RF, HB10GBT, R1996A), che
    // usano fino a quattro cifre. Una versione piu' stretta, provata prima,
    // bocciava il 4,7% dei veri: quella e' la trappola da evitare.
    {
        static const QRegularExpression formaItu {
            QStringLiteral("^(?:[A-Z]{1,2}|[0-9][A-Z]|[A-Z][0-9])[0-9]{1,4}[A-Z]{1,5}$")};
        QString testoForma =
            entry.value(QStringLiteral("displayMessage")).toString().trimmed();
        if (testoForma.isEmpty()) {
            testoForma = entry.value(QStringLiteral("message")).toString().trimmed();
        }
        static const QRegularExpression locatoreForma {
            QStringLiteral("^[A-R]{2}[0-9]{2}([A-X]{2})?$")};
        for (QString token : testoForma.toUpper().split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            if (token.size() < 3 || token.size() > 13) continue;
            if (token == QStringLiteral("CQ") || token == QStringLiteral("DE")
                || token == QStringLiteral("DX") || token == QStringLiteral("QRZ")
                || token == QStringLiteral("73") || token == QStringLiteral("RR73")
                || token == QStringLiteral("RRR") || token == QStringLiteral("TU")) continue;
            if (locatoreForma.match(token).hasMatch()) continue;
            if (token.startsWith(QStringLiteral("R-")) || token.startsWith(QStringLiteral("R+"))
                || token.startsWith(QLatin1Char('-')) || token.startsWith(QLatin1Char('+'))) continue;
            // senza cifre non e' un nominativo ma una parola di testo libero
            bool haCifra = false;
            for (QChar const c : token) { if (c.isDigit()) { haCifra = true; break; } }
            if (!haCifra) continue;

            token.remove(QLatin1Char('<')); token.remove(QLatin1Char('>'));
            int const barre = token.count(QLatin1Char('/'));
            if (barre > 1 || token.startsWith(QLatin1Char('/'))
                || token.endsWith(QLatin1Char('/'))) return true;
            QString base = token;
            if (barre == 1) {
                QString const a = token.section(QLatin1Char('/'), 0, 0);
                QString const b = token.section(QLatin1Char('/'), 1, 1);
                QString const affisso = a.size() >= b.size() ? b : a;
                base = a.size() >= b.size() ? a : b;
                if (affisso.isEmpty() || affisso.size() > 4) return true;
                for (QChar const c : affisso) { if (!c.isLetterOrNumber()) return true; }
            }
            if (!formaItu.match(base).hasMatch()) return true;
        }
    }

    // FT2: un singolo token esadecimale lungo (es. 31ED17795E04339455)
    // e' payload telemetry/hash non risolto, non una stazione radio. Se
    // arriva fino al modello UI trattalo come ghost strutturale, a prescindere
    // dallo SNR, per evitare righe "callsign" inesistenti e punti Live Map.
    QString const entryMode = entry.value(QStringLiteral("mode"), m_mode)
                                  .toString().trimmed().toUpper();
    QString msgText = entry.value(QStringLiteral("displayMessage")).toString().trimmed();
    if (msgText.isEmpty()) {
        msgText = entry.value(QStringLiteral("message")).toString().trimmed();
    }
    // 1.0.373 - ghost strutturale "R nuda + grid" (CALL CALL R GRID / CQ CALL R GRID):
    // formato non-standard 77-bit, solo decode LDPC corrotti lo generano. SNR-indipendente.
    if (looksLikeMalformedRGrid(msgText)) {
        return true;
    }
    if (entryMode == QStringLiteral("FT2")) {
        if (decodeListModel_isTelemetryOnlyMessage(msgText)) {
            return true;
        }
    }

    bool snrOk = false;
    int const db = entry.value(QStringLiteral("db")).toString().trimmed().toInt(&snrOk);
    if (!snrOk) return false;  // se SNR non parsa, non considerare ghost

    bool const isMyCall = entry.value(QStringLiteral("isMyCall")).toBool();

    // 1.0.148: il whitelist su isMyCall non e' piu' assoluto. Il decoder LDPC
    // a SNR debole genera false positive "MYCALL X RPT" plausibili (utente ha
    // visto "IU8LMC JA3FYC RR73" a -21 dB dal Giappone da una stazione mai
    // chiamata). Se NON siamo in QSO attivo con quella stazione (= dxCall
    // attuale), applichiamo un criterio aggressivo: SNR ≤ -14 dB + distanza
    // intercontinentale (>8000 km) = ghost. QSO attivi (partner == m_dxCall)
    // restano sempre passanti per non perdere conferme deboli reali.
    if (isMyCall) {
        QString const partner = entry.value(QStringLiteral("dxCallsign"))
                                     .toString().trimmed().toUpper();
        QString const fromCall = entry.value(QStringLiteral("fromCall"))
                                      .toString().trimmed().toUpper();
        QString const dx = m_dxCall.trimmed().toUpper();
        bool const isActiveQso = !dx.isEmpty()
            && (partner == dx || fromCall == dx);
        if (isActiveQso) return false;  // QSO attivo: mai filtrare
        if (db <= -14) {
            bool distOk = false;
            double const distKm = entry.value(QStringLiteral("dxDistance"))
                                      .toDouble(&distOk);
            if (distOk && distKm > 8000.0) return true;
        }
        // 1.0.222 — strict-callsign ghost filter su pattern myCall + partner
        // sintatticamente NON-callsign. Screenshot utente 2026-05-18 08:05
        // ha mostrato 17 decode "IU8LMC <token-rumore>" passare il filtro
        // SNR/distance perche' Radio::valid_callsign_regexp e' molto loose
        // (accetta qualunque alfanumerico con transizione alpha/digit).
        // Usiamo strict_standard_callsign_re per validare il partner: se la
        // base (no suffix /X /P /M ecc) non matcha "^([A-Z][0-9]?|
        // [0-9A-Z][A-Z])[0-9][A-Z]{0,3}$" e SNR <= -10, e' ghost.
        // Cutoff -10 e' generoso: i pattern screenshot sono -12..-22.
        // Compound calls validi (IU8LMC/P, OH8X/8) restano accettati.
        QString partnerCandidate = partner;
        if (partnerCandidate.isEmpty()) {
            partnerCandidate = fromCall;
        }
        if (partnerCandidate.isEmpty()) {
            // Fallback: estrai secondo token dal messaggio
            QString const msg = entry.value(QStringLiteral("displayMessage"))
                                     .toString().trimmed().toUpper();
            QString const fallback = entry.value(QStringLiteral("message"))
                                          .toString().trimmed().toUpper();
            QString const source = msg.isEmpty() ? fallback : msg;
            QStringList const tokens = source.simplified().split(QLatin1Char(' '),
                                                                  Qt::SkipEmptyParts);
            if (tokens.size() >= 2) {
                partnerCandidate = tokens.at(1);
                partnerCandidate.remove('<');
                partnerCandidate.remove('>');
            }
        }
        if (!partnerCandidate.isEmpty() && db <= -10) {
            QString partnerBase = partnerCandidate;
            int const slashIdx = partnerBase.indexOf(QLatin1Char('/'));
            if (slashIdx > 0) {
                partnerBase = partnerBase.left(slashIdx);
            }
            static QRegularExpression const strictCallRe(
                QStringLiteral("^([A-Z][0-9]?|[0-9A-Z][A-Z])[0-9][A-Z]{0,3}$"));
            if (!strictCallRe.match(partnerBase).hasMatch()
                && !isSpecialEventStyleCallsignToken(partnerCandidate)) {
                decodiumBridgeGhostLog(QStringLiteral("[GhostFilter] strict-call reject: partner=%1 base=%2 db=%3")
                              .arg(partnerCandidate, partnerBase, QString::number(db)));
                return true;
            }
            // 1.0.224 — Anche se il partner e' sintatticamente valido, il
            // terzo+ token puo' essere noise corrupted (es. "R..83", "AG3...").
            // Pattern signature decoder-garbage: dots consecutivi `\.\.`,
            // trailing dots `\.+$`, o char isolato + dots + digit `^[A-Z]\.+\d`.
            // I sentinel `<...>` per hashed callsign NON matchano (angolari
            // strippati prima del display). Decode validi non hanno mai dots
            // multipli dentro o trailing in payload normale (R+report, RR73,
            // 73, grid, ?). Cutoff db<=-10 invariato.
            QString const msgFull = entry.value(QStringLiteral("displayMessage"))
                                          .toString().trimmed().toUpper();
            QString const msgFallback = entry.value(QStringLiteral("message"))
                                              .toString().trimmed().toUpper();
            QString const msgText = msgFull.isEmpty() ? msgFallback : msgFull;
            QStringList const allTokens = msgText.simplified().split(QLatin1Char(' '),
                                                                      Qt::SkipEmptyParts);
            if (allTokens.size() >= 3) {
                static QRegularExpression const corruptedTokenRe(
                    QStringLiteral("(\\.\\.)|(\\.+$)|(^[A-Z]\\.+\\d)"));
                for (int idx = 2; idx < allTokens.size(); ++idx) {
                    QString const& tok = allTokens.at(idx);
                    if (tok.startsWith(QLatin1Char('<'))) continue;  // hashed sentinel
                    if (corruptedTokenRe.match(tok).hasMatch()) {
                        decodiumBridgeGhostLog(QStringLiteral("[GhostFilter] corrupted-token reject: partner=%1 token=%2 db=%3")
                                      .arg(partnerCandidate, tok, QString::number(db)));
                        return true;
                    }
                }
            }
        }
        // 1.0.291 — Directed false-call guard. A weak, low-confidence
        // message addressed to my call with a grid that is geographically
        // incompatible with the caller's DXCC is much more likely an LDPC
        // ghost than a real first call. Example reported:
        // "EA8DJR JE8TRM/P LO57 ?" at -26 dB while EA8DJR was only RX.
        // Keep active-QSO replies untouched via the isActiveQso return above.
        // 1.0.416: extend the same hard mismatch signature to high-confidence
        // weak first calls at <= -18 dB. Example: "9H1SR C38EPT/P NK14" at
        // -19 dB, where NK14 is thousands of km away from the C3 DXCC.
        QString const msgFull = entry.value(QStringLiteral("displayMessage"))
                                      .toString().trimmed().toUpper();
        QString const msgFallback = entry.value(QStringLiteral("message"))
                                          .toString().trimmed().toUpper();
        QString const msgText = msgFull.isEmpty() ? msgFallback : msgFull;
        QStringList const rawTokens = msgText.simplified().split(QLatin1Char(' '),
                                                                  Qt::SkipEmptyParts);
        bool const lowConfidence = !rawTokens.isEmpty()
            && rawTokens.constLast().trimmed() == QStringLiteral("?");
        QString grid = entry.value(QStringLiteral("dxGrid")).toString().trimmed().toUpper();
        if (!isGridTokenStrict(grid)) {
            grid = extractDecodedGrid(msgText);
        }
        if (db <= -16
            && (lowConfidence || db <= -18)
            && isGridTokenStrict(grid)
            && m_dxccLookup
            && m_dxccLookup->isLoaded()) {
            QString callForEntity = partnerCandidate.trimmed().toUpper();
            if (callForEntity.isEmpty()) {
                callForEntity = directedPeerTokenFromMessage(msgText,
                                                             m_callsign.trimmed().toUpper(),
                                                             normalizedBaseCall(m_callsign)).trimmed().toUpper();
            }
            if (!callForEntity.isEmpty()) {
                DxccEntity const ent = m_dxccLookup->lookup(callForEntity);
                if (ent.isValid()) {
                    double gridLon = 0.0;
                    double gridLat = 0.0;
                    if (grid2deg(grid, gridLon, gridLat)) {
                        double mismatchAz = 0.0;
                        double const entLonEast = -ent.lon;
                        double const mismatchKm = azdist(ent.lat, entLonEast,
                                                         gridLat, gridLon,
                                                         mismatchAz);
                        if (mismatchKm > 5000.0) {
                            decodiumBridgeGhostLog(QStringLiteral("[GhostFilter] directed call/grid mismatch reject: call=%1 dxcc=%2 grid=%3 mismatch=%4km db=%5 lowConfidence=%6")
                                          .arg(callForEntity,
                                               ent.name,
                                               grid,
                                               QString::number(qRound(mismatchKm)),
                                               QString::number(db),
                                               lowConfidence ? QStringLiteral("yes") : QStringLiteral("no")));
                            return true;
                        }
                    }
                }
            }
        }
        return false;  // isMyCall non attivo ma SNR/dist non da ghost
    }

    // 1.0.174 / 1.0.270: soglie solo-SNR rimosse. Restano i criteri
    // semantici sotto, altrimenti i veri segnali a -24/-26 spariscono
    // dall'interfaccia pur essendo stati decodificati.
    QString const partner = entry.value(QStringLiteral("dxCallsign"))
                                 .toString().trimmed().toUpper();
    QString const fromCall = entry.value(QStringLiteral("fromCall"))
                                  .toString().trimmed().toUpper();
    QString const dx = m_dxCall.trimmed().toUpper();
    bool const isPartnerOfActiveQso = !dx.isEmpty()
        && (partner == dx || fromCall == dx);
    QString const aptype = entry.value(QStringLiteral("aptype")).toString().trimmed();
    if (!aptype.isEmpty() && !isPartnerOfActiveQso && db <= -24) {
        QString const msgText = entry.value(QStringLiteral("displayMessage"))
                                    .toString().trimmed().toUpper();
        if (msgText.contains(QStringLiteral("<...>"))
            || msgText.contains(QStringLiteral("..."))) {
            return true;
        }
    }

    // 1.0.146: terzo criterio = SNR ≤ -19 + distanza estrema (>10000 km).
    // Ghost a SNR marginale spesso producono callsign-country mismatch (es.
    // "JX4KCI G5IOSZ/P NC21" interpretato come England 13495 km, ma 13495
    // è fantasioso — UK è 1500 km, Jan Mayen 5000 km). Distance/Country
    // estremi a SNR weak senza B4 sono firma ghost classica.
    if (db <= -19) {
        bool distOk = false;
        double const distKm = entry.value(QStringLiteral("dxDistance"))
                                  .toDouble(&distOk);
        if (distOk && distKm > 10000.0) return true;
    }

    // Stronger third-party ghost signature: a weak, non-CQ decode reports a
    // locator thousands of km away from the DXCC implied by the callsign.
    // Example seen in FT2: "HH8ZXU SP2CTC/P NC87" maps a Polish /P call to
    // Antarctica. Real portable activity should carry a matching prefix; this
    // pattern is much more likely an LDPC false decode and pollutes Signal RX
    // and Live Map.
    bool const isCQ = entry.value(QStringLiteral("isCQ")).toBool();
    if (!isCQ && db <= -16 && !isPartnerOfActiveQso && !m_grid.trimmed().isEmpty()) {
        QString grid = entry.value(QStringLiteral("dxGrid")).toString().trimmed().toUpper();
        if (isGridTokenStrict(grid) && m_dxccLookup && m_dxccLookup->isLoaded()) {
            QString callForEntity = entry.value(QStringLiteral("dxCallsign")).toString().trimmed().toUpper();
            if (callForEntity.isEmpty()) {
                callForEntity = entry.value(QStringLiteral("fromCall")).toString().trimmed().toUpper();
            }
            DxccEntity const ent = m_dxccLookup->lookup(callForEntity);
            if (ent.isValid()) {
                double gridLon = 0.0;
                double gridLat = 0.0;
                if (grid2deg(grid, gridLon, gridLat)) {
                    double mismatchAz = 0.0;
                    double const entLonEast = -ent.lon;
                    double const mismatchKm = azdist(ent.lat, entLonEast, gridLat, gridLon, mismatchAz);
                    bool distOk = false;
                    double const localDistKm = entry.value(QStringLiteral("dxDistance")).toDouble(&distOk);
                    if (mismatchKm > 5000.0 && (!distOk || localDistKm > 8000.0)) {
                        decodiumBridgeGhostLog(QStringLiteral("[GhostFilter] call/grid mismatch reject: call=%1 dxcc=%2 grid=%3 mismatch=%4km db=%5")
                                      .arg(callForEntity,
                                           ent.name,
                                           grid,
                                           QString::number(qRound(mismatchKm)),
                                           QString::number(db)));
                        return true;
                    }
                }
            }
        }
    }

    return false;
}


bool DecodiumBridge::directedDecodePeerHasInvalidDxcc(const QString& peerToken,
                                                      const QString& message,
                                                      const QString& context,
                                                      bool logReject) const
{
    if (!m_dxccLookup || !m_dxccLookup->isLoaded()) {
        return false;
    }

    QString const peer = normalizedUsableCallToken(peerToken);
    if (peer.isEmpty() || isPlaceholderCallToken(peer)) {
        return false;
    }

    QString const base = Radio::base_callsign(peer).trimmed().toUpper();
    if (base.isEmpty() || !Radio::is_callsign(base)) {
        return false;
    }

    auto const peerMatchesCall = [&peer, &base](QString const& call) {
        QString const normalized = normalizeCallToken(call).trimmed().toUpper();
        QString const callBase = normalizedBaseCall(normalized);
        return (!normalized.isEmpty() && normalized == peer)
            || (!callBase.isEmpty() && callBase == base);
    };
    if (peerMatchesCall(m_dxCall)
        || peerMatchesCall(m_autoCqLockedCall)
        || peerMatchesCall(inferredPartnerForAutolog())
        || messageContainsCallToken(buildCurrentTxMessage(), peer, base)
        || messageContainsCallToken(m_lastTransmittedMessage, peer, base)) {
        return false;
    }

    // I call chiaramente corrotti sono gia' gestiti dal filtro strutturale.
    // Qui colpiamo solo i falsi positivi apparentemente plausibili ma con
    // prefisso non risolto in CTY/DXCC, es. X49IKP/P.
    if (!isPlausibleDecodedCallsign(peer)) {
        return false;
    }

    DxccEntity const ent = m_dxccLookup->lookup(peer);
    if (ent.isValid()) {
        return false;
    }

    if (logReject) {
        decodiumBridgeGhostLog(QStringLiteral("[GhostFilter] directed invalid-DXCC reject context=%1 peer=%2 base=%3 msg=%4")
                      .arg(context,
                           peer,
                           base,
                           message.trimmed()));
    }
    return true;
}

bool DecodiumBridge::shouldSuppressDirectedGhostDecode(const QStringList& fields,
                                                       const QString& context) const
{
    if (fields.size() < 5) {
        return false;
    }

    QString const msg = fields.value(4).trimmed();
    if (msg.isEmpty()) {
        return false;
    }

    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);
    if (myCallUpper.isEmpty()
        || !messageContainsCallToken(msg, myCallUpper, myBaseUpper)) {
        return false;
    }

    QString const peerToken = directedPeerTokenFromMessage(msg, myCallUpper, myBaseUpper);
    QString const peer = normalizedUsableCallToken(peerToken);
    if (peer.isEmpty()) {
        return false;
    }
    QString const peerBase = normalizedBaseCall(peer);
    if (peerBase.isEmpty()) {
        return false;
    }

    auto const peerMatchesCall = [&peer, &peerBase](QString const& call) {
        QString const normalized = normalizeCallToken(call).trimmed().toUpper();
        QString const callBase = normalizedBaseCall(normalized);
        return (!normalized.isEmpty() && normalized == peer)
            || (!callBase.isEmpty() && callBase == peerBase);
    };
    bool const activeOrManualTarget =
        peerMatchesCall(m_dxCall)
        || peerMatchesCall(m_autoCqLockedCall)
        || peerMatchesCall(m_resumeTargetCall)
        || peerMatchesCall(m_pendingAutoSeqPartnerBase)
        || peerMatchesCall(inferredPartnerForAutolog())
        || messageContainsCallToken(buildCurrentTxMessage(), peer, peerBase)
        || messageContainsCallToken(m_lastTransmittedMessage, peer, peerBase)
        || messageContainsCallToken(m_pendingAutoSeqMessage, peer, peerBase);
    if (activeOrManualTarget) {
        return false;
    }

    // A retry-capped AutoCQ exchange retains a short, immutable snapshot of
    // its partner.  The partner's delayed 73 often has no grid and can be
    // weak; do not send it through the generic FT2 weak-first-contact gate
    // before autoSequenceStep gets the opportunity to recover and log it.
    QString const lateBase = normalizedBaseCall(m_lateAutoLogCall);
    QStringList const messageTokens = normalizedMessageTokens(msg);
    QString const finalToken = messageTokens.isEmpty()
        ? QString()
        : messageTokens.constLast();
    bool const lateFinalForSnapshot =
        m_lateAutoLogValid
        && m_lateAutoLogExpires.isValid()
        && QDateTime::currentDateTimeUtc() <= m_lateAutoLogExpires
        && !lateBase.isEmpty()
        && peerBase == lateBase
        && (finalToken == QStringLiteral("73")
            || finalToken == QStringLiteral("RR73")
            || finalToken == QStringLiteral("RRR"));
    if (lateFinalForSnapshot) {
        decodiumBridgeGhostLog(QStringLiteral("[GhostFilter] allow late final signoff for snapshot partner %1 context=%2 msg=%3")
                      .arg(peerBase, context, msg));
        return false;
    }

    if (directedPeerLooksStructurallyGhost(peerToken)) {
        decodiumBridgeGhostLog(QStringLiteral("[GhostFilter] directed ghost suppress context=%1 reason=structural peer=%2 msg=%3")
                      .arg(context, peerToken.trimmed(), msg));
        return true;
    }

    if (directedDecodePeerHasInvalidDxcc(peerToken, msg, context)) {
        return true;
    }

    QString grid = extractDecodedGrid(msg);
    // A standard directed first call with a locator that agrees with the
    // caller's DXCC has two independent integrity checks (FT message grammar
    // plus call/grid geography).  It is safe to let AutoCQ answer it on the
    // first decode rather than losing a weak caller while waiting for a
    // second repetition.
    bool trustedDirectedGrid = false;
    if (isGridTokenStrict(grid) && m_dxccLookup && m_dxccLookup->isLoaded()) {
        DxccEntity const ent = m_dxccLookup->lookup(peer);
        if (ent.isValid()) {
            double gridLon = 0.0;
            double gridLat = 0.0;
            if (grid2deg(grid, gridLon, gridLat)) {
                double mismatchAz = 0.0;
                double const entLonEast = -ent.lon;
                double const mismatchKm = azdist(ent.lat, entLonEast,
                                                 gridLat, gridLon,
                                                 mismatchAz);
                // 1.0.378: suppress low-confidence grid-mismatch directed decodes.
                // 1.0.416: also suppress high-confidence weak first-calls when the
                // mismatch is extreme. Active/manual targets returned earlier, so
                // QSO replies stay visible even with a corrupt grid.
                QStringList const ghostTokens = msg.simplified().split(QLatin1Char(' '),
                                                                        Qt::SkipEmptyParts);
                bool const ghostLowConfidence = !ghostTokens.isEmpty()
                    && ghostTokens.constLast().trimmed() == QStringLiteral("?");
                bool snrOk = false;
                int const ghostSnr = fields.value(1).trimmed().toInt(&snrOk);
                if (mismatchKm > 5000.0
                    && (ghostLowConfidence || (snrOk && ghostSnr <= -18))) {
                    // dedup: il filtro gira a ogni rebuild lista (signal-rx); una riga
                    // di log per messaggio invece di decine identiche.
                    if (!m_loggedDirectedGhostMsgs.contains(msg)) {
                        if (m_loggedDirectedGhostMsgs.size() > 256) {
                            m_loggedDirectedGhostMsgs.clear();
                        }
                        m_loggedDirectedGhostMsgs.insert(msg);
                        decodiumBridgeGhostLog(QStringLiteral("[GhostFilter] directed call/grid mismatch suppress context=%1 call=%2 dxcc=%3 grid=%4 mismatch=%5km msg=%6")
                                      .arg(context,
                                           peer,
                                           ent.name,
                                           grid,
                                           QString::number(qRound(mismatchKm)),
                                           msg));
                    }
                    return true;
                }
                if (mismatchKm <= 5000.0) {
                    trustedDirectedGrid = true;
                }
            }
        }
    }

    // Ghost-gate 4o livello (caso C58BTM, loopback 2026-06-11): i ghost LDPC
    // in formato strict superano structural+DXCC+grid. Un chiamante DEBOLE
    // (<=-10) MAI sentito prima deve confermarsi con un SECONDO decode (slot
    // diverso) entro 120s prima di raggiungere lista/auto-seq: i ghost sono
    // one-shot (token casuali ogni volta), una stazione reale ripete la
    // chiamata ogni slot. Esenzione: call gia' vista in banda (AP cache).
    // Solo FT2. Un call+grid coerente con la DXCC e' gia' validato sopra e
    // passa al primo decode; gli altri first-contact deboli restano soggetti
    // alla conferma sul secondo slot.
    // Do not apply the two-decode first-contact gate while rebuilding Signal RX:
    // at that point the row has already passed the Band Activity filters and is
    // visible in Full Spectrum. Signal RX is the QSO timeline, so a weak direct
    // caller must not disappear there while still being used by the sequencer.
    if (m_mode == QStringLiteral("FT2")
        && context != QStringLiteral("signal-rx")
        && !trustedDirectedGrid) {
        bool snrOk = false;
        int const snrVal = fields.value(1).trimmed().toInt(&snrOk);
        if (snrOk && snrVal <= -10) {
            qint64 const pendNowMs = QDateTime::currentMSecsSinceEpoch();
            bool seenInBand = false;
            if (m_ft2ApHashCache) {
                for (quint32 const h28 : decodium::ft2MessageCallHashes(peerBase)) {
                    if (m_hashedCallsignCache.containsHash28(h28, pendNowMs)) {
                        seenInBand = true;
                        break;
                    }
                }
            }
            if (!seenInBand) {
                QString const timeToken = fields.value(0).trimmed();
                auto const it = m_directedWeakPeerSeen.constFind(peerBase);
                bool const confirmed = it != m_directedWeakPeerSeen.constEnd()
                    && it.value().first != timeToken
                    && (pendNowMs - it.value().second) <= 120000;
                if (confirmed) {
                    m_directedWeakPeerSeen.remove(peerBase);
                    decodiumBridgeGhostLog(QStringLiteral("[GhostFilter] weak first-contact CONFERMATO al 2o decode context=%1 peer=%2 snr=%3")
                                  .arg(context, peerBase).arg(snrVal));
                } else {
                    if (it == m_directedWeakPeerSeen.constEnd()
                        || (pendNowMs - it.value().second) > 120000) {
                        if (m_directedWeakPeerSeen.size() > 64) {
                            m_directedWeakPeerSeen.clear();
                        }
                        m_directedWeakPeerSeen.insert(peerBase,
                                                      qMakePair(timeToken, pendNowMs));
                        decodiumBridgeGhostLog(QStringLiteral("[GhostFilter] weak first-contact: attendo conferma al 2o decode context=%1 peer=%2 snr=%3 msg=%4")
                                      .arg(context, peerBase).arg(snrVal).arg(msg));
                    }
                    return true;
                }
            }
        }
    }

    return false;
}

bool DecodiumBridge::shouldSuppressDirectedGhostDecode(const QVariantMap& entry,
                                                       const QString& context) const
{
    if (entry.value(QStringLiteral("isTx")).toBool()) {
        return false;
    }
    QString const msg = entry.value(QStringLiteral("message")).toString().trimmed();
    if (msg.isEmpty()) {
        return false;
    }
    QStringList fields;
    fields << entry.value(QStringLiteral("time")).toString()
           << entry.value(QStringLiteral("db")).toString()
           << entry.value(QStringLiteral("dt")).toString()
           << entry.value(QStringLiteral("freq")).toString()
           << msg
           << entry.value(QStringLiteral("aptype")).toString()
           << entry.value(QStringLiteral("quality")).toString()
           << entry.value(QStringLiteral("freq")).toString();
    return shouldSuppressDirectedGhostDecode(fields, context);
}
