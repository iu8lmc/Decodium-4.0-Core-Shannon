#include "DecodiumBridge.h"
#include "DecodeUserFilterConfig.h"
#include "DecodeMessageNormalization.h"
#include "LegacyDecodeText.h"
#include <QFileInfo>
#include <QRegularExpression>

using decodium::decode_message::stripDecodeApAnnotation;
void decodiumBridgeDecodeFlowLog(const QString&);
QString decodiumBridgeMirrorEntryKey(const QVariantMap&);
QString decodeMirrorLooseKey(const QVariantMap&);
QString decodiumBridgeNormalizeRemoteActivityTime(QString);

QVariantList DecodiumBridge::augmentLegacyMirrorWithAllTxt(QVariantList const& mirroredEntries,
                                                           bool rxPane) const
{
    if (!isTimeSyncDecodeMode(m_mode)) {
        return mirroredEntries;
    }

    QString const path = legacyAllTxtPath();
    if (path.trimmed().isEmpty()) {
        return mirroredEntries;
    }

    QFileInfo const fileInfo(path);
    if (!fileInfo.exists()) {
        m_legacyAllTxtConsumedPath = path;
        m_legacyAllTxtConsumedSize = -1;
        return mirroredEntries;
    }

    QString const absolutePath = fileInfo.absoluteFilePath();
    qint64 const fileSize = fileInfo.size();
    if (m_legacyAllTxtConsumedPath != absolutePath
        || m_legacyAllTxtConsumedSize < 0
        || fileSize < m_legacyAllTxtConsumedSize) {
        m_legacyAllTxtConsumedPath = absolutePath;
        m_legacyAllTxtConsumedSize = fileSize;
        decodiumBridgeDecodeFlowLog(QStringLiteral("legacy-alltxt prime %1: size=%2 source=%3")
                      .arg(rxPane ? QStringLiteral("rx") : QStringLiteral("band"))
                      .arg(fileSize)
                      .arg(absolutePath));
        return mirroredEntries;
    }

    // Su slot molto affollati FT8/FT4 il legacy ALL.TXT puo crescere di diverse
    // centinaia di righe in meno di un minuto. Un budget troppo piccolo fa si che
    // il mirror di D4 veda solo la coda finale del file e perda CQ valide che D3
    // ha gia mostrato a schermo. Manteniamo una finestra piu ampia e dinamica.
    int const desiredLineBudget = qMax(4096, mirroredEntries.size() * 8);
    QStringList const tailLines = readTextLinesFromOffset(path, m_legacyAllTxtConsumedSize, desiredLineBudget);
    if (tailLines.isEmpty()) {
        m_legacyAllTxtConsumedSize = fileSize;
        return mirroredEntries;
    }

    static const QRegularExpression allTxtPattern {
        R"(^(\d{6})_(\d{6})\s+([0-9]+(?:\.[0-9]+)?)\s+(Rx|Tx|Ck)\s+([A-Z0-9_+\-]+)\s+(.+)$)",
        QRegularExpression::CaseInsensitiveOption
    };
    static const QRegularExpression payloadPattern {
        R"(^(-?\d+)\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+(.*)$)"
    };

    QVariantList augmented = mirroredEntries;
    QHash<QString, int> exactIndexByKey;
    QMultiHash<QString, int> looseIndexByKey;
    for (int i = 0; i < augmented.size(); ++i) {
        QVariantMap const entry = augmented.at(i).toMap();
        exactIndexByKey.insert(decodiumBridgeMirrorEntryKey(entry), i);
        looseIndexByKey.insert(decodeMirrorLooseKey(entry), i);
    }

    QString const normalizedMode = m_mode.trimmed().toUpper();
    int upgradedTimes = 0;
    int appended = 0;
    DecodeUserFilterConfig const userFilterConfig = readDecodeUserFilterConfig();
    for (QString const& rawLine : tailLines) {
        QRegularExpressionMatch const lineMatch = allTxtPattern.match(rawLine.trimmed());
        if (!lineMatch.hasMatch()) {
            continue;
        }

        QString const txRx = lineMatch.captured(4).trimmed().toUpper();
        if (txRx != QStringLiteral("RX")) {
            continue;
        }

        QString const entryMode = lineMatch.captured(5).trimmed().toUpper();
        if (entryMode != normalizedMode) {
            continue;
        }

        QRegularExpressionMatch const payloadMatch = payloadPattern.match(lineMatch.captured(6).trimmed());
        if (!payloadMatch.hasMatch()) {
            continue;
        }

        QVariantMap entry;
        entry[QStringLiteral("time")] = decodiumBridgeNormalizeRemoteActivityTime(lineMatch.captured(2));
        entry[QStringLiteral("db")] = payloadMatch.captured(1).trimmed();
        entry[QStringLiteral("dt")] = payloadMatch.captured(2).trimmed();
        entry[QStringLiteral("freq")] = payloadMatch.captured(3).trimmed();
        QString aptype;
        QString const message = stripDecodeApAnnotation(payloadMatch.captured(4).trimmed(), &aptype);
        if (message.isEmpty()) {
            continue;
        }
        QString semanticRejectReason;
        if (!shouldAcceptDecodedMessage(message, &semanticRejectReason)) {
            decodiumBridgeDecodeFlowLog(QStringLiteral("semantic decode filter legacy-alltxt: %1 msg='%2'")
                          .arg(semanticRejectReason, message));
            continue;
        }

        bool const isCQ = message.startsWith(QStringLiteral("CQ "), Qt::CaseInsensitive)
                       || message == QStringLiteral("CQ")
                       || message.startsWith(QStringLiteral("QRZ "), Qt::CaseInsensitive);
        entry[QStringLiteral("message")] = message;
        entry[QStringLiteral("aptype")] = aptype;
        entry[QStringLiteral("quality")] = QString {};
        entry[QStringLiteral("mode")] = normalizedMode;
        entry[QStringLiteral("isTx")] = false;
        entry[QStringLiteral("isCQ")] = isCQ;
        entry[QStringLiteral("isMyCall")] = messageMentionsLocalCall(message);
        entry[QStringLiteral("fromCall")] = extractDecodedCallsign(message, isCQ);
        entry[QStringLiteral("forceRxPane")] = rxPane;
        enrichDecodeEntry(entry);
        if (m_hideGhostDecodes && looksLikeGhostDecode(entry)) {
            decodiumBridgeDecodeFlowLog(QStringLiteral("ghost decode filter legacy-alltxt: msg='%1'")
                          .arg(message));
            continue;
        }
        QString userFilterReason;
        if (!shouldAcceptDecodeEntryByUserFilters(entry, userFilterConfig, false, &userFilterReason)) {
            decodiumBridgeDecodeFlowLog(QStringLiteral("user decode filter legacy-alltxt: %1 msg='%2'")
                          .arg(userFilterReason, message));
            continue;
        }

        QString const exactKey = decodiumBridgeMirrorEntryKey(entry);
        if (exactIndexByKey.contains(exactKey)) {
            continue;
        }

        QString const looseKey = decodeMirrorLooseKey(entry);
        auto const indexes = looseIndexByKey.values(looseKey);
        bool upgradedExisting = false;
        for (int const index : indexes) {
            QVariantMap existing = augmented.at(index).toMap();
            QString const existingTime = decodiumBridgeNormalizeRemoteActivityTime(existing.value(QStringLiteral("time")).toString());
            bool const existingMskZeroTime =
                normalizedMode == QStringLiteral("MSK144")
                && existingTime == QStringLiteral("000000");
            if (existing.value(QStringLiteral("isTx")).toBool()) {
                continue;
            }
            if (existingTime.size() >= 6 && !existingMskZeroTime) {
                continue;
            }
            if (!existingMskZeroTime
                && !existingTime.isEmpty()
                && !entry.value(QStringLiteral("time")).toString().startsWith(existingTime)) {
                continue;
            }
            existing[QStringLiteral("time")] = entry.value(QStringLiteral("time")).toString();
            augmented[index] = existing;
            exactIndexByKey.insert(decodiumBridgeMirrorEntryKey(existing), index);
            ++upgradedTimes;
            upgradedExisting = true;
            break;
        }

        if (upgradedExisting) {
            continue;
        }

        if (rxPane) {
            continue;
        }

        int const newIndex = augmented.size();
        augmented.append(entry);
        exactIndexByKey.insert(exactKey, newIndex);
        looseIndexByKey.insert(looseKey, newIndex);
        ++appended;
    }

    if (upgradedTimes > 0 || appended > 0) {
        decodiumBridgeDecodeFlowLog(QStringLiteral("legacy-alltxt augment %1: upgraded=%2 appended=%3 source=%4")
                      .arg(rxPane ? QStringLiteral("rx") : QStringLiteral("band"))
                      .arg(upgradedTimes)
                      .arg(appended)
                      .arg(path));
    }

    m_legacyAllTxtConsumedSize = fileSize;

    return augmented;
}

