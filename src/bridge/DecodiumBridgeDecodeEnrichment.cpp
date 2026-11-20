#include "DecodiumBridge.h"
#include "AdifDocumentInternal.h"
#include "DecodiumBridgeWorkedHistory.h"
#include "DecodeMessageNormalization.h"
#include "DecodeUnresolved.h"
#include "DecodeTokenClassifier.h"
#include "Sequencer/MessageTokenRules.hpp"
#include "DxccLookup.h"
#include "UsStateDataManager.h"

using namespace decodium::adif_document;
using namespace decodium::decode_message;
using namespace decodium::decode_unresolved;
using namespace decodium::decode_tokens;
using namespace decodium::seq;
QString decodiumBridgeExtractRightCallsign(const QString&);

QString DecodiumBridge::lookupUsStateForDecode(const QString& call, const QString& gridHint) const
{
    if (!m_showUsState || !m_usStateData || !m_usStateData->ready()) {
        return {};
    }
    return m_usStateData->stateForCall(call, gridHint);
}

void DecodiumBridge::enrichDecodeEntry(QVariantMap& entry, bool countAsReceived) const
{
    if (countAsReceived)
        noteDecodeReceived();  // 1.0.233 DevOverlay counter (no-op se overlay off)
    QString msg = entry.value("message").toString();
    QString aptype = entry.value(QStringLiteral("aptype")).toString().trimmed();
    msg = stripDecodeApAnnotation(msg, &aptype);
    entry[QStringLiteral("message")] = msg;
    entry[QStringLiteral("aptype")] = aptype;

    bool isCQ = entry.value("isCQ").toBool();
    if (!isCQ) {
        isCQ = msg.startsWith(QStringLiteral("CQ "), Qt::CaseInsensitive)
            || msg.compare(QStringLiteral("CQ"), Qt::CaseInsensitive) == 0
            || msg.startsWith(QStringLiteral("QRZ "), Qt::CaseInsensitive);
        entry[QStringLiteral("isCQ")] = isCQ;
    }
    bool const localTxEntry = entry.value(QStringLiteral("isTx")).toBool()
        || entry.value(QStringLiteral("db")).toString().trimmed().compare(QStringLiteral("TX"), Qt::CaseInsensitive) == 0;
    // Every decode must carry an explicit provenance.  Feed/map rows already
    // provide source/provider; legacy decoder rows do not, so give them a
    // stable, human-readable origin instead of leaving the UI ambiguous.
    QString provenance = entry.value(QStringLiteral("provenance")).toString().trimmed();
    if (provenance.isEmpty()) {
        provenance = entry.value(QStringLiteral("source")).toString().trimmed();
    }
    if (provenance.isEmpty()) {
        provenance = localTxEntry ? QStringLiteral("Local TX")
                                  : QStringLiteral("Local decoder");
    }
    entry.insert(QStringLiteral("source"), provenance);
    entry.insert(QStringLiteral("provenance"), provenance);
    entry[QStringLiteral("displayMessage")] =
        displayMessageWithCallsignsActorFirst(msg, localTxEntry, m_callsign);
    QString fromCall = entry.value("fromCall").toString().trimmed().toUpper();
    if (fromCall.isEmpty())
        fromCall = extractDecodedCallsign(msg, isCQ);

    QString const myBase = normalizedBaseCall(m_callsign);
    auto sameBaseCall = [&myBase](QString const& call) {
        QString const base = normalizedBaseCall(call);
        return !myBase.isEmpty() && !base.isEmpty() && base == myBase;
    };
    QString curMode = entry.value(QStringLiteral("mode")).toString().trimmed().toUpper();
    if (curMode.isEmpty()) {
        curMode = m_mode.trimmed().toUpper();
    }
    QString unresolvedGridOnlyLocator;
    bool const unresolvedGridOnlyDecode =
        !localTxEntry && messageIsUnresolvedGridOnlyDecode(msg, &unresolvedGridOnlyLocator);
    bool const unresolvedFst4wHash = isFst4wUnresolvedHashDecode(curMode, msg);
    bool const unresolvedNonOperationalDecode = unresolvedGridOnlyDecode || unresolvedFst4wHash;
    if (unresolvedNonOperationalDecode) {
        fromCall.clear();
        entry[QStringLiteral("hasUnresolvedPeer")] = true;
        entry[QStringLiteral("unresolvedHash")] = true;
        entry[QStringLiteral("partialDecode")] = true;
    }

    // DXCC lookup: usa il callsign di DESTRA nel messaggio (il secondo call).
    // Se il peer e' il placeholder FT "<...>", non ripiegare sul nostro call:
    // mostrerebbe Malta/9H1SR come DX e inquinerebbe worked-before/map/logica UI.
    QString rightCall = unresolvedNonOperationalDecode ? QString() : decodiumBridgeExtractRightCallsign(msg);
    bool const unresolvedPeerPlaceholder =
        !localTxEntry
        && messageHasUnresolvedPeerPlaceholder(msg, m_callsign);
    if (unresolvedPeerPlaceholder && sameBaseCall(rightCall)) {
        rightCall.clear();
    }
    // Never substitute the left-hand station when the semantic DX token on
    // the right is unknown. A blank DXCC is correct; showing the sender's
    // entity would attribute the row to the wrong country.

    // Keep worked-before/LotW state aligned with the DX call shown in the row.
    // For third-party traffic like "YC1FMV 7Q9PTP RR73", fromCall may be the
    // addressee/other station while dxCallsign/rightCall is the visible DX.
    QString const dxStatusCall = rightCall.trimmed().toUpper();
    bool const isWorkedEver = !dxStatusCall.isEmpty() && m_workedCalls.contains(dxStatusCall);
    bool isLotw = m_lotwEnabled && !dxStatusCall.isEmpty() && m_lotwUsers.contains(dxStatusCall);

    QString firstCall;
    QStringList const tokens = normalizedMessageTokens(msg);
    for (QString const& token : tokens) {
        if (token == QStringLiteral("CQ")
            || token == QStringLiteral("QRZ")
            || token == QStringLiteral("DE")
            || isDirectedCqModifierToken(token)) {
            continue;
        }
        if (looksLikeCallsignToken(token)) {
            if (isGridToken(token)) {
                continue;
            }
            firstCall = token;
            break;
        }
    }
    bool const selfEntry = localTxEntry || sameBaseCall(fromCall) || sameBaseCall(firstCall);
    QString const curBand = freqHzToBandToken(m_frequency);
    bool isB4 = false;
    if (!selfEntry && !dxStatusCall.isEmpty()) {
        const QString bandModeKey = workedCallBandModeKey(curBand, curMode, dxStatusCall);
        if (!bandModeKey.isEmpty()) {
            isB4 = m_worked.callByBandMode.contains(bandModeKey);
        }
    }
    QString const callBandKey = workedCallBandKey(curBand, dxStatusCall);
    bool const dxIsWorkedBand = !selfEntry
        && !callBandKey.isEmpty()
        && m_worked.callByBand.contains(callBandKey);
    // 1.0.584: match on the BASE call. m_worked.callToday & friends are keyed
    // the same way, so IK8OLM/P is recognised as IK8OLM and vice versa.
    QString const dxStatusBaseCall = normalizedBaseCall(dxStatusCall);
    QString const baseCallBandKey = workedCallBandKey(curBand, dxStatusBaseCall);
    bool const dxIsWorkedToday = !selfEntry
        && !dxStatusBaseCall.isEmpty()
        && m_worked.callToday.contains(dxStatusBaseCall);
    bool const dxIsWorkedYesterday = !selfEntry
        && !dxStatusBaseCall.isEmpty()
        && m_worked.callYesterday.contains(dxStatusBaseCall);
    bool const dxIsWorkedEver = !selfEntry
        && !dxStatusBaseCall.isEmpty()
        && m_worked.callEver.contains(dxStatusBaseCall);
    bool const dxIsWorkedTodayBand = !selfEntry
        && !baseCallBandKey.isEmpty()
        && m_worked.callTodayByBand.contains(baseCallBandKey);
    bool const dxIsNewCallBand = !selfEntry
        && !dxStatusCall.isEmpty()
        && !curBand.isEmpty()
        && !dxIsWorkedBand;

    QString dxCountry;
    QString dxContinent;
    QString dxPrefix;
    int     dxCqZone = 0;
    int     dxItuZone = 0;
    bool dxIsNewCountry      = false;
    bool dxIsNewDxcc         = false;
    bool dxIsNewDxccBand     = false;
    bool dxIsNewContinent    = false;
    bool dxIsNewContinentBand= false;
    bool dxIsNewCqZone       = false;
    bool dxIsNewCqZoneBand   = false;
    bool dxIsNewItuZone      = false;
    bool dxIsNewItuZoneBand  = false;
    if (m_dxccLookup && m_dxccLookup->isLoaded() && !rightCall.isEmpty()) {
        DxccEntity ent = m_dxccLookup->lookup(rightCall);
        if (ent.isValid()) {
            dxCountry   = ent.name;
            dxContinent = ent.continent;
            dxPrefix    = ent.prefix;
            dxCqZone    = ent.cqZone;
            dxItuZone   = ent.ituZone;
            if (!selfEntry) {
                dxIsNewDxcc      = !m_worked.dxccEver.contains(ent.name);
                dxIsNewContinent = !m_worked.continentEver.contains(ent.continent);
                if (ent.cqZone  > 0) dxIsNewCqZone  = !m_worked.cqZoneEver.contains(ent.cqZone);
                if (ent.ituZone > 0) dxIsNewItuZone = !m_worked.ituZoneEver.contains(ent.ituZone);
                if (!curBand.isEmpty()) {
                    dxIsNewDxccBand      = !m_worked.dxccByBand.contains(curBand + QLatin1Char('|') + ent.name);
                    dxIsNewContinentBand = !m_worked.continentByBand.contains(curBand + QLatin1Char('|') + ent.continent);
                    if (ent.cqZone  > 0) dxIsNewCqZoneBand  = !m_worked.cqZoneByBand.contains(curBand + QLatin1Char('|') + QString::number(ent.cqZone));
                    if (ent.ituZone > 0) dxIsNewItuZoneBand = !m_worked.ituZoneByBand.contains(curBand + QLatin1Char('|') + QString::number(ent.ituZone));
                }
            }
        }
    }

    QString dxGridExtracted = unresolvedNonOperationalDecode ? QString() : extractDecodedGrid(msg);
    QString const usState = unresolvedNonOperationalDecode
        ? QString()
        : lookupUsStateForDecode(rightCall, dxGridExtracted);
    double distKm = -1.0;
    double bearing = -1.0;
    if (!selfEntry && !dxGridExtracted.isEmpty() && !m_grid.isEmpty()) {
        bearing = calcBearing(m_grid, dxGridExtracted);
        distKm = calcDistance(m_grid, dxGridExtracted);
    }

    bool dxIsNewGrid     = false;
    bool dxIsNewGridBand = false;
    if (!selfEntry && !dxGridExtracted.isEmpty()) {
        QString const grid4 = dxGridExtracted.left(4).toUpper();
        dxIsNewGrid = !m_worked.gridEver.contains(grid4);
        if (!curBand.isEmpty()) {
            dxIsNewGridBand = !m_worked.gridByBand.contains(curBand + QLatin1Char('|') + grid4);
        }
    }

    entry["fromCall"] = fromCall;
    entry["isB4"] = isB4;
    entry["isLotw"] = isLotw;
    entry["dxCountry"] = dxCountry;
    entry["dxCallsign"] = rightCall;
    entry["usState"] = usState;
    // 1.0.132: flag per highlight del nominativo inserito nel campo DxCall
    // (l'utente vuole vedere subito dove sta trasmettendo nel Full Spectrum).
    {
        QString const dxCallTrimmed = m_dxCall.trimmed().toUpper();
        bool const matchesDxCall = !dxCallTrimmed.isEmpty()
            && ((!rightCall.isEmpty() && rightCall.toUpper() == dxCallTrimmed)
                || (!fromCall.isEmpty() && fromCall.toUpper() == dxCallTrimmed));
        entry["matchesDxCall"] = matchesDxCall;
    }
    entry["dxContinent"] = dxContinent;
    entry["dxPrefix"] = dxPrefix;
    entry["dxCqZone"]  = dxCqZone;
    entry["dxItuZone"] = dxItuZone;
    entry["dxIsWorked"] = isB4;
    entry["dxIsWorkedEver"] = isWorkedEver && !selfEntry;
    entry["dxIsWorkedBand"] = dxIsWorkedBand;
    entry["dxIsWorkedToday"] = dxIsWorkedToday;
    entry["dxIsWorkedYesterday"] = dxIsWorkedYesterday;
    entry["dxIsWorkedEver"] = dxIsWorkedEver;
    entry["dxIsWorkedTodayBand"] = dxIsWorkedTodayBand;
    entry["dxIsNewBand"] = entry.value("dxIsNewBand", false);
    entry["dxIsNewCountry"] = dxIsNewCountry;
    entry["dxIsNewDxcc"]          = dxIsNewDxcc;
    entry["dxIsNewDxccBand"]      = dxIsNewDxccBand;
    entry["dxIsNewContinent"]     = dxIsNewContinent;
    entry["dxIsNewContinentBand"] = dxIsNewContinentBand;
    entry["dxIsNewCqZone"]        = dxIsNewCqZone;
    entry["dxIsNewCqZoneBand"]    = dxIsNewCqZoneBand;
    entry["dxIsNewItuZone"]       = dxIsNewItuZone;
    entry["dxIsNewItuZoneBand"]   = dxIsNewItuZoneBand;
    entry["dxIsNewGrid"]          = dxIsNewGrid;
    entry["dxIsNewGridBand"]      = dxIsNewGridBand;
    entry["dxIsNewCall"]          = !isWorkedEver && !selfEntry && !dxStatusCall.isEmpty();
    entry["dxIsNewCallBand"]      = dxIsNewCallBand;
    entry["dxIsMostWanted"] = entry.value("dxIsMostWanted", false);
    entry["dxBearing"] = bearing;
    entry["dxDistance"] = distKm;
    entry["dxGrid"] = dxGridExtracted;
    entry[QStringLiteral("isPartialHashDecode")] = unresolvedGridOnlyDecode || unresolvedFst4wHash;

    QString const directedPeer = directedPeerTokenFromMessage(msg,
                                                              m_callsign.trimmed().toUpper(),
                                                              myBase);
    if (directedDecodePeerHasInvalidDxcc(directedPeer,
                                         msg,
                                         QStringLiteral("enrich"),
                                         false)) {
        entry[QStringLiteral("directedInvalidDxcc")] = true;
        entry[QStringLiteral("isMyCall")] = false;
    } else {
        entry[QStringLiteral("directedInvalidDxcc")] = false;
    }

    // 1.0.138: pre-calcola la cascata WSJT-X UNA volta qui, quando il decode
    // entra. Prima Waterfall.qml::refreshDecodeLabels() chiamava
    // bridge.decodeHighlightBg(d) per ogni call ad ogni decodeListChanged
    // signal (FT2 ~5-20Hz × 30 decode max = 150-600 chiamate/sec a una
    // funzione C++ con lookup DXCC/worked/LotW). CPU/GPU spike confermato
    // dagli utenti dalla 1.0.131 in poi. Adesso il QML legge d.highlightBg.
    entry["highlightBg"] = decodeHighlightBg(entry);

    // 1.0.144: pre-calcola flag aggregato isHighlighted (bold rendering nei
    // delegate decode). Sostituisce binding QML composto a 5 lookup:
    //   font.bold: modelData.isTx||isCQ||isMyCall||dxIsNewCountry||dxIsMostWanted
    // con singolo lookup model.isHighlighted → meno costo per delegate paint
    // (= meno carico GPU su PC vecchi).
    bool const isHl = (entry.value(QStringLiteral("isTx")).toBool()
                       && decodeColorEnabled(QStringLiteral("colorTxMessage"))
                       && decodeColorBold(QStringLiteral("colorTxMessage")))
                   || (entry.value(QStringLiteral("isCQ")).toBool()
                       && decodeColorEnabled(QStringLiteral("colorCQ"))
                       && decodeColorBold(QStringLiteral("colorCQ")))
                   || (entry.value(QStringLiteral("isMyCall")).toBool()
                       && decodeColorEnabled(QStringLiteral("colorMyCall"))
                       && decodeColorBold(QStringLiteral("colorMyCall")))
                   || (dxIsNewCountry
                       && decodeColorEnabled(QStringLiteral("colorDXEntity"))
                       && decodeColorBold(QStringLiteral("colorDXEntity")))
                   || (entry.value(QStringLiteral("dxIsMostWanted")).toBool()
                       && decodeColorEnabled(QStringLiteral("colorDXEntity"))
                       && decodeColorBold(QStringLiteral("colorDXEntity")));
    entry["isHighlighted"] = isHl;

    // 1.0.232 (Sprint 1, Phase 4.2.1) — pre-compute 3 display strings/colors
    // here, instead of recomputing them on every delegate paint in the
    // ListView. Eliminates JS calls + parseInt + ternary cascades from the
    // hot painting path for FT8/FT2/JT9/JT65/Q65/WSPR entries (all builders
    // converge in enrichDecodeEntry).
    //
    // (a) formattedTime: replicates decodePanel.formatUtcForDisplay (Main.qml
    //     :4810). Take any "time" string, strip non-digits, format as
    //     hh:mm:ss / hh:mm / passthrough.
    {
        QString const timeStr = entry.value(QStringLiteral("time")).toString();
        QString digits;
        digits.reserve(timeStr.size());
        for (QChar const c : timeStr) {
            if (c.isDigit()) digits.append(c);
        }
        QString formatted;
        if (digits.size() >= 6) {
            formatted = digits.left(2) + QLatin1Char(':')
                      + digits.mid(2, 2) + QLatin1Char(':')
                      + digits.mid(4, 2);
        } else if (digits.size() == 4) {
            formatted = digits.left(2) + QLatin1Char(':') + digits.mid(2, 2);
        } else {
            formatted = timeStr;
        }
        entry[QStringLiteral("formattedTime")] = formatted;
    }

    // (b) snrColor: replicates the db-column ternary cascade used in Main.qml
    //     delegates (lines 5642 / 6193 / 9742 / 10238):
    //       isTx → "#f1c40f"
    //       db > -5  → accentGreen (theme)
    //       db > -15 → secondaryCyan (theme)
    //       else     → textSecondary (theme)
    //     Colors are sampled from the live theme palette so Ocean Blue and
    //     Stellar Light both render correctly. NB: when the user switches
    //     theme, already-enriched rows keep stale colors until they get
    //     re-enriched (refreshDecodeListDxcc / next decode append).
    {
        QString snrHex;
        bool const isTxEntry = entry.value(QStringLiteral("isTx")).toBool();
        if (isTxEntry) {
            snrHex = QStringLiteral("#f1c40f");
        } else if (m_themeManager) {
            QString const dbStr = entry.value(QStringLiteral("db")).toString();
            bool parseOk = false;
            int const dbVal = dbStr.toInt(&parseOk);
            int const effective = parseOk ? dbVal : 0;
            QColor pick;
            if (effective > -5) {
                pick = m_themeManager->accentColor();
            } else if (effective > -15) {
                pick = m_themeManager->secondaryColor();
            } else {
                pick = m_themeManager->textSecondary();
            }
            snrHex = pick.name(QColor::HexRgb);
        }
        entry[QStringLiteral("snrColor")] = snrHex;
    }

    // (c) bgColorHex: pre-compute the static fragment of the delegate
    //     background (Main.qml :9720). Does NOT include highlightFill /
    //     period-separator handling because those depend on per-frame state
    //     in QML. The delegate is expected to keep its full ternary and use
    //     bgColorHex only as the final fallback (isCQ vs default text-fade).
    {
        bool const isCQEntry = entry.value(QStringLiteral("isCQ")).toBool();
        QString bgHex;
        if (m_themeManager) {
            QColor const base = isCQEntry
                ? m_themeManager->accentColor()
                : m_themeManager->textPrimary();
            // 0.15 alpha for CQ tint, 0.05 alpha for default — match
            // Qt.rgba(...,0.15) / Qt.rgba(...,0.05) in QML.
            QColor tinted = base;
            tinted.setAlphaF(isCQEntry ? 0.15 : 0.05);
            // 1.0.254 fix: skip bgColorHex se base color invalido o alpha
            // quasi-zero (theme non ancora pronto -> #00000000 truthy ma
            // visivamente trasparente -> Rectangle nero in QML).
            if (base.isValid() && tinted.alphaF() > 0.02) {
                bgHex = tinted.name(QColor::HexArgb);
            }
        }
        entry[QStringLiteral("bgColorHex")] = bgHex;
    }
}


// === PRIVATE SLOTS ===
