// CallsignHash28.h — 1.0.293/294 (fork iu8lmc), AP hashed-callsign cache
//
// Hash28 + estrazione call CONDIVISI tra il bridge (seed della cache) e il decoder
// FT2 (confirm AP). CRITICO: seed e confirm DEVONO usare le STESSE funzioni qui,
// altrimenti gli hash non coincidono mai e la cache è silenziosamente inerte.
// Per questo l'estrazione+normalizzazione+hash di un intero messaggio è incapsulata
// in ft2MessageCallHashes(), chiamata IDENTICAMENTE dai due lati.
//
// Contratto: callsign in MAIUSCOLO, trimmato, suffisso dopo '/' rimosso, campo fisso
// 13 char (pad spazi, troncato), nhash2 seed 146 mascherato a 28 bit (pattern
// verificato in Detector/FtxFt8Stage4.cpp:1540).
#pragma once

#include <cstdint>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QRegularExpression>

extern "C" uint32_t nhash2(void const* key, uint64_t length, uint32_t initval);

namespace decodium {

inline constexpr uint32_t kFt2HashSeed   = 146u;
inline constexpr uint32_t kFt2Hash28Mask = 0x0FFFFFFFu;  // 28 bit
inline constexpr int      kFt2HashField  = 13;

// Core: la call passata DEVE essere già MAIUSCOLA, trimmata, senza suffisso '/'.
inline quint32 ft2CallsignHash28Raw(const char* call, int len) noexcept
{
    char field[kFt2HashField];
    for (int i = 0; i < kFt2HashField; ++i)
        field[i] = (i < len) ? call[i] : ' ';
    return nhash2(field, static_cast<uint64_t>(kFt2HashField), kFt2HashSeed) & kFt2Hash28Mask;
}

// Wrapper: normalizza (upper + trim + strip suffisso '/') prima di hashare.
inline quint32 ft2CallsignHash28(const QString& call)
{
    QString c = call.trimmed().toUpper();
    int const slash = c.indexOf(QLatin1Char('/'));
    if (slash > 0) c = c.left(slash);
    QByteArray const b = c.toLatin1();
    return ft2CallsignHash28Raw(b.constData(), b.size());
}

// Estrae gli hash28 dei callsign STANDARD presenti in un messaggio decodificato.
// USATA SIA dal seed (bridge) SIA dal confirm (Stage7) → estrazione+hash identici
// per costruzione. Esclude CQ/DE, grid, report e call nonstandard <...>.
inline QVector<quint32> ft2MessageCallHashes(const QString& message)
{
    // Regex standard-callsign (stesso pattern dello strict-call gate del bridge).
    static const QRegularExpression callRe(
        QStringLiteral("^([A-Z][0-9]?|[0-9A-Z][A-Z])[0-9][A-Z]{0,3}$"));
    static const QRegularExpression wsRe(QStringLiteral("\\s+"));
    QVector<quint32> out;
    const QStringList tokens = message.toUpper().split(wsRe, Qt::SkipEmptyParts);
    for (QString tok : tokens) {
        if (tok == QLatin1String("CQ") || tok == QLatin1String("DE")) continue;
        if (tok.contains(QLatin1Char('<'))) continue;  // nonstandard <...> escluse
        int const slash = tok.indexOf(QLatin1Char('/'));
        if (slash > 0) tok = tok.left(slash);
        if (!callRe.match(tok).hasMatch()) continue;
        out.append(ft2CallsignHash28(tok));
    }
    return out;
}

} // namespace decodium
