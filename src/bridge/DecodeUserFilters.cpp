#include "DecodeUserFilterConfig.h"
#include "DecodeMessageNormalization.h"
#include "Sequencer/MessageTokenRules.hpp"

using decodium::decode_message::canonicalDecodeMessage;
using decodium::seq::normalizedUsableCallToken;
using decodium::seq::normalizedBaseCall;
using decodium::seq::normalizedMessageTokens;
using decodium::seq::isDirectedCqModifierToken;
bool decodiumBridgeLooksLikeCallsignToken(const QString&);

static void addDecodeFilterSubject(QStringList& subjects, QString token)
{
    token = token.trimmed().toUpper();
    if (token.isEmpty()) {
        return;
    }

    auto addUnique = [&subjects](QString const& value) {
        QString const trimmed = value.trimmed().toUpper();
        if (!trimmed.isEmpty() && !subjects.contains(trimmed)) {
            subjects.append(trimmed);
        }
    };

    addUnique(token);
    QString const usable = normalizedUsableCallToken(token);
    addUnique(usable);
    addUnique(normalizedBaseCall(usable));
}

static QString legacyDecodeFilterWord2Subject(const QString& message)
{
    QStringList const words = normalizedMessageTokens(message);
    if (words.size() < 2) {
        return {};
    }

    int index = 1;
    QString const word2 = words.at(1);
    if (words.constFirst() == QStringLiteral("CQ")
        && isDirectedCqModifierToken(word2)
        && words.size() > 2) {
        index = 2;
    } else if (message.contains(QLatin1Char(';')) && words.size() > 3) {
        index = 3;
    }
    return index < words.size() ? words.at(index).toUpper() : QString {};
}

static QStringList decodeFilterCallingSubjects(const QVariantMap& entry)
{
    QStringList subjects;
    QString const message = canonicalDecodeMessage(entry.value(QStringLiteral("message")).toString());

    addDecodeFilterSubject(subjects, entry.value(QStringLiteral("fromCall")).toString());
    addDecodeFilterSubject(subjects, entry.value(QStringLiteral("dxCallsign")).toString());
    addDecodeFilterSubject(subjects, legacyDecodeFilterWord2Subject(message));

    QStringList const words = normalizedMessageTokens(message);
    for (QString const& word : words) {
        if (word == QStringLiteral("CQ")
            || word == QStringLiteral("QRZ")
            || word == QStringLiteral("DE")
            || isDirectedCqModifierToken(word)) {
            continue;
        }
        if (decodiumBridgeLooksLikeCallsignToken(word)) {
            addDecodeFilterSubject(subjects, word);
        }
    }

    return subjects;
}

static bool decodeFilterEntryMatchesTerms(const QVariantMap& entry,
                                          const QStringList& terms,
                                          bool callingOnly)
{
    if (terms.isEmpty()) {
        return false;
    }

    if (callingOnly) {
        QStringList const subjects = decodeFilterCallingSubjects(entry);
        for (QString const& subject : subjects) {
            for (QString const& term : terms) {
                if (subject.startsWith(term)) {
                    return true;
                }
            }
        }
        return false;
    }

    QString const message =
        canonicalDecodeMessage(entry.value(QStringLiteral("message")).toString()).toUpper();
    for (QString const& term : terms) {
        if (message.contains(term)) {
            return true;
        }
    }
    return false;
}

static QString normalizedDecodeFilterTerritoryName(QString country)
{
    country = country.trimmed();
    country.replace(QStringLiteral("Islands"), QStringLiteral("Is."));
    country.replace(QStringLiteral("Island"), QStringLiteral("Is."));
    country.replace(QStringLiteral("North "), QStringLiteral("N. "));
    country.replace(QStringLiteral("Northern "), QStringLiteral("N. "));
    country.replace(QStringLiteral("South "), QStringLiteral("S. "));
    country.replace(QStringLiteral("East "), QStringLiteral("E. "));
    country.replace(QStringLiteral("Eastern "), QStringLiteral("E. "));
    country.replace(QStringLiteral("West "), QStringLiteral("W. "));
    country.replace(QStringLiteral("Western "), QStringLiteral("W. "));
    country.replace(QStringLiteral("Central "), QStringLiteral("C. "));
    country.replace(QStringLiteral(" and "), QStringLiteral(" & "));
    country.replace(QStringLiteral("Republic"), QStringLiteral("Rep."));
    country.replace(QStringLiteral("United States of America"), QStringLiteral("U.S.A."));
    country.replace(QStringLiteral("United States"), QStringLiteral("U.S.A."));
    country.replace(QStringLiteral("Fed. Rep. of "), QString());
    country.replace(QStringLiteral("French "), QStringLiteral("Fr."));
    country.replace(QStringLiteral("Asiatic"), QStringLiteral("AS"));
    country.replace(QStringLiteral("European"), QStringLiteral("EU"));
    country.replace(QStringLiteral("African"), QStringLiteral("AF"));
    return country.toUpper();
}

static QStringList decodeFilterTerritoryAliases(QString territory)
{
    territory = normalizedDecodeFilterTerritoryName(territory).simplified();
    territory.replace(QLatin1Char('-'), QLatin1Char(' '));
    territory.replace(QLatin1Char('_'), QLatin1Char(' '));

    QStringList aliases;
    auto addAlias = [&aliases](QString value) {
        value = value.trimmed().toUpper();
        if (!value.isEmpty() && !aliases.contains(value)) {
            aliases.append(value);
        }
    };

    addAlias(territory);

    if (territory == QStringLiteral("EU") || territory == QStringLiteral("EUROPE") || territory == QStringLiteral("EUROPA")) {
        addAlias(QStringLiteral("EU"));
        addAlias(QStringLiteral("EUROPE"));
        addAlias(QStringLiteral("EUROPA"));
    } else if (territory == QStringLiteral("AF") || territory == QStringLiteral("AFRICA")) {
        addAlias(QStringLiteral("AF"));
        addAlias(QStringLiteral("AFRICA"));
    } else if (territory == QStringLiteral("OC") || territory == QStringLiteral("OCEANIA")) {
        addAlias(QStringLiteral("OC"));
        addAlias(QStringLiteral("OCEANIA"));
    } else if (territory == QStringLiteral("AS") || territory == QStringLiteral("ASIA")) {
        addAlias(QStringLiteral("AS"));
        addAlias(QStringLiteral("ASIA"));
    } else if (territory == QStringLiteral("NA")
               || territory == QStringLiteral("N AMERICA")
               || territory == QStringLiteral("N. AMERICA")
               || territory == QStringLiteral("NORTH AMERICA")) {
        addAlias(QStringLiteral("NA"));
        addAlias(QStringLiteral("NORTH AMERICA"));
        addAlias(QStringLiteral("N. AMERICA"));
    } else if (territory == QStringLiteral("SA")
               || territory == QStringLiteral("S AMERICA")
               || territory == QStringLiteral("S. AMERICA")
               || territory == QStringLiteral("SOUTH AMERICA")) {
        addAlias(QStringLiteral("SA"));
        addAlias(QStringLiteral("SOUTH AMERICA"));
        addAlias(QStringLiteral("S. AMERICA"));
    }

    return aliases;
}

static bool decodeFilterTerritoryCandidateMatches(QString candidate, QString territory)
{
    candidate = normalizedDecodeFilterTerritoryName(candidate).simplified();
    territory = normalizedDecodeFilterTerritoryName(territory).simplified();
    if (candidate.isEmpty() || territory.isEmpty()) {
        return false;
    }
    if (territory.size() <= 2) {
        return candidate == territory || candidate.startsWith(territory + QLatin1Char(' '));
    }
    return candidate.contains(territory);
}

static bool decodeFilterEntryMatchesTerritory(const QVariantMap& entry,
                                              const QStringList& territories)
{
    if (territories.isEmpty()) {
        return false;
    }

    QStringList candidates;
    QString const country = entry.value(QStringLiteral("dxCountry")).toString().trimmed();
    if (!country.isEmpty()) {
        candidates.append(country.toUpper());
        candidates.append(normalizedDecodeFilterTerritoryName(country));
    }
    QString const prefix = entry.value(QStringLiteral("dxPrefix")).toString().trimmed().toUpper();
    if (!prefix.isEmpty()) {
        candidates.append(prefix);
    }
    QString const continent = entry.value(QStringLiteral("dxContinent")).toString().trimmed().toUpper();
    if (!continent.isEmpty()) {
        candidates.append(continent);
    }
    candidates.removeDuplicates();

    QStringList territoryAliases;
    for (QString const& territory : territories) {
        territoryAliases.append(decodeFilterTerritoryAliases(territory));
    }
    territoryAliases.removeDuplicates();

    for (QString const& candidate : candidates) {
        for (QString const& territory : territoryAliases) {
            if (decodeFilterTerritoryCandidateMatches(candidate, territory)) {
                return true;
            }
        }
    }
    return false;
}

bool shouldAcceptDecodeEntryByUserFilters(const QVariantMap& entry,
                                                 const DecodeUserFilterConfig& filters,
                                                 bool waitPounceScope,
                                                 QString* reason)
{
    if (reason) {
        reason->clear();
    }

    bool const isTx = entry.value(QStringLiteral("isTx")).toBool()
        || entry.value(QStringLiteral("db")).toString().trimmed().compare(
               QStringLiteral("TX"), Qt::CaseInsensitive) == 0;
    if (isTx) {
        return true;
    }

    QString const message = canonicalDecodeMessage(entry.value(QStringLiteral("message")).toString());
    if (message.isEmpty()) {
        return true;
    }

    if (filters.waitPounceOnly && !waitPounceScope) {
        return true;
    }

    if (filters.alwaysPassEnabled
        && decodeFilterEntryMatchesTerms(entry, filters.passTerms, filters.callingOnly)) {
        return true;
    }

    if (filters.blacklistEnabled
        && decodeFilterEntryMatchesTerms(entry, filters.blacklistTerms, filters.callingOnly)) {
        if (reason) {
            *reason = QStringLiteral("blacklist");
        }
        return false;
    }

    if (filters.whitelistEnabled) {
        if (!decodeFilterEntryMatchesTerms(entry, filters.whitelistTerms, filters.callingOnly)) {
            if (reason) {
                *reason = QStringLiteral("not whitelisted");
            }
            return false;
        }
    }

    if (decodeFilterEntryMatchesTerritory(entry, filters.territoryTerms)) {
        if (reason) {
            *reason = QStringLiteral("territory");
        }
        return false;
    }

    return true;
}

