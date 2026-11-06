#include "DecodiumSelfCheck.hpp"

#include <QCoreApplication>
#include <algorithm>
#include <cmath>

namespace {

QString tr_(const char* text)
{
    return QCoreApplication::translate("DecodiumSelfCheck", text);
}

// Durata dello slot T/R: serve per giudicare l'offset dell'orologio in modo
// proporzionato. 372 ms sono irrilevanti in FT8 (slot 15 s) ma disastrosi in
// FT2 (slot 3,75 s, payload 2,52 s) — caso reale IZ1KGY.
double slotSecondsForMode(const QString& mode)
{
    const QString m = mode.toUpper();
    if (m.startsWith(QLatin1String("FT2")))  return 3.75;
    if (m.startsWith(QLatin1String("FT4")))  return 7.5;
    if (m.startsWith(QLatin1String("Q65")))  return 30.0;
    if (m.startsWith(QLatin1String("MSK")))  return 15.0;
    return 15.0;  // FT8 e resto
}

bool isFtxMode(const QString& mode)
{
    const QString m = mode.toUpper();
    return m.startsWith(QLatin1String("FT2")) || m.startsWith(QLatin1String("FT4"))
           || m.startsWith(QLatin1String("FT8"));
}

}  // namespace

namespace DecodiumSelfCheck {

QList<Finding> run(const State& s)
{
    QList<Finding> out;

    // ── 1. Versione vecchia ───────────────────────────────────────────────
    // La causa numero uno in assoluto: i tester segnalano bug gia' corretti da
    // release fa (visti report da 1.0.262/348/368 quando l'attuale era 1.0.370).
    // Va per primo: se e' vecchia, tutto il resto e' rumore.
    if (s.updateAvailable && !s.latestVersion.isEmpty()) {
        out.push_back({Severity::Problem,
                       tr_("You are running an outdated version"),
                       tr_("You have %1, the latest release is %2.")
                           .arg(s.appVersion, s.latestVersion),
                       tr_("Update BEFORE reporting: the vast majority of \"QSOs do not "
                           "complete\" problems have already been fixed in releases newer "
                           "than yours."),
                       QStringLiteral("install")});
    }

    // ── 2. Orologio fuori sincronia ───────────────────────────────────────
    // Giudicato in proporzione allo slot del modo attivo. Caso reale: 372 ms su
    // slot FT2 da 3,75 s = disallineamento serio, i decode saltano.
    const double slot = slotSecondsForMode(s.mode);
    const double warnMs    = slot * 1000.0 * 0.04;   // 4% dello slot
    const double problemMs = slot * 1000.0 * 0.08;   // 8% dello slot
    const double off = std::abs(s.ntpOffsetMs);

    if (!s.ntpEnabled) {
        out.push_back({Severity::Warning,
                       tr_("Time synchronisation is turned off"),
                       tr_("Without synchronisation there is no way to tell whether your "
                           "clock is aligned, and digital modes depend on exact time."),
                       tr_("Turn time synchronisation on in the settings, or make sure the "
                           "Windows Time service (W32Time) is running."),
                       QStringLiteral("rx")});
    } else if (s.ntpSynced && off >= warnMs) {
        const bool serious = off >= problemMs;
        out.push_back({serious ? Severity::Problem : Severity::Warning,
                       tr_("Your PC clock is out of sync"),
                       tr_("Measured offset: %1 ms. In %2 a slot lasts %3 s, so this offset "
                           "pushes your transmissions outside the usable window.")
                           .arg(QString::number(s.ntpOffsetMs, 'f', 0), s.mode,
                                QString::number(slot, 'f', 2)),
                       tr_("Fix the clock: start the Windows Time service (W32Time) and force "
                           "a resync. This is not a Decodium problem — no digital mode "
                           "software can decode with the wrong time."),
                       QStringLiteral("rx")});
    }

    // ── 3. MAM multi-stream acceso in FT2 ─────────────────────────────────
    // Sperimentale e MAI validato in FT2: i chiamanti rispondono al CQ ma non
    // completano (retry limit). Caso reale ik7imk/II7WWA: sembrava una
    // regressione del sequencer, era questo toggle.
    if (s.mamMultiStream && s.mode.toUpper().startsWith(QLatin1String("FT2"))) {
        out.push_back({Severity::Problem,
                       tr_("MAM multi-stream is enabled in FT2"),
                       tr_("Multi-stream is experimental and has never been validated in "
                           "FT2: stations answer your CQ but cannot complete the contact."),
                       tr_("Turn multi-stream off and work the pileup one station at a time. "
                           "This is not a fault of the FT2 sequencer."),
                       QStringLiteral("tx")});
    }

    // ── 4. Troppi thread di decodifica ────────────────────────────────────
    // Oltre un certo punto i thread non accelerano la decodifica ma rubano CPU
    // all'audio: si sentono underrun e si perdono slot. Misurato: su 32 core
    // l'ottimo e' 12, oltre non migliora.
    if (s.ftThreads > 0 && s.cpuCores > 0) {
        const int reasonable = std::min(12, std::max(2, s.cpuCores - 2));
        if (s.ftThreads > reasonable) {
            out.push_back({Severity::Warning,
                           tr_("Too many decoder threads"),
                           tr_("You have %1 threads on %2 cores. Beyond about %3 decoding "
                               "does not get faster, but CPU is taken away from audio.")
                               .arg(s.ftThreads).arg(s.cpuCores).arg(reasonable),
                           tr_("Lower the threads to %1: if you hear choppy audio or lose "
                               "slots, this is the most likely cause.").arg(reasonable),
                           QStringLiteral("audio")});
        }
    }

    // ── 5. Nessun decode: banda morta o audio scollegato ──────────────────
    // Caso reale ZL1BW: 48 TX su 48 erano CQ, nessuna risposta, zero QSO. Non
    // era il sequencer: era la banda vuota. Chiediamo almeno 3 minuti di
    // ascolto prima di dirlo, altrimenti e' solo impazienza.
    if (s.monitoring && s.monitoringSeconds >= 180 && s.totalDecodesReceived == 0
        && isFtxMode(s.mode)) {
        out.push_back({Severity::Problem,
                       tr_("Nothing has been decoded"),
                       tr_("You have been receiving on %2 for %1 minutes and nothing has "
                           "been decoded.")
                           .arg(s.monitoringSeconds / 60).arg(s.mode),
                       tr_("This almost always means the band is closed, or audio is not "
                           "reaching Decodium. Check that the waterfall shows the noise from "
                           "your radio: if it is flat, it is the audio input device; if it "
                           "moves but nobody is transmitting, the band is closed."),
                       QStringLiteral("rx")});
    }

    // ── 6. Nominativo non impostato ───────────────────────────────────────
    if (s.callsign.trimmed().isEmpty()) {
        out.push_back({Severity::Problem,
                       tr_("Your callsign is not set"),
                       tr_("Without a callsign you cannot transmit or log contacts."),
                       tr_("Enter your callsign in the settings."),
                       QStringLiteral("other")});
    }

    std::stable_sort(out.begin(), out.end(), [](const Finding& a, const Finding& b) {
        return static_cast<int>(a.severity) > static_cast<int>(b.severity);
    });
    return out;
}

QVariantList toVariantList(const QList<Finding>& findings)
{
    QVariantList list;
    list.reserve(findings.size());
    for (const Finding& f : findings) {
        QVariantMap m;
        m.insert(QStringLiteral("severity"), static_cast<int>(f.severity));
        m.insert(QStringLiteral("severityText"),
                 f.severity == Severity::Problem ? tr_("Problem")
                 : f.severity == Severity::Warning ? tr_("Warning")
                                                   : tr_("Ok"));
        m.insert(QStringLiteral("title"), f.title);
        m.insert(QStringLiteral("detail"), f.detail);
        m.insert(QStringLiteral("action"), f.action);
        m.insert(QStringLiteral("category"), f.category);
        list.append(m);
    }
    return list;
}

}  // namespace DecodiumSelfCheck
