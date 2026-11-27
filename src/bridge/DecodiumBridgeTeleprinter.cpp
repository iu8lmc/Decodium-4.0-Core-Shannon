#include "DecodiumBridge.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QtMath>

void DecodiumBridge::aggiungiRigaRtty (QString const& testo, double qualita,
                                      double frequenzaHz)
{
    if (testo.trimmed ().isEmpty ())
        return;

    QDateTime const adesso = QDateTime::currentDateTimeUtc ();
    QVariantMap entry;
    entry.insert (QStringLiteral ("mode"), QStringLiteral ("RTTY"));
    entry.insert (QStringLiteral ("time"), adesso.toString (QStringLiteral ("hhmmss")));
    entry.insert (QStringLiteral ("utc"), adesso.toString (QStringLiteral ("hhmm")));
    entry.insert (QStringLiteral ("timestamp"), adesso.toMSecsSinceEpoch ());
    entry.insert (QStringLiteral ("message"), testo);
    entry.insert (QStringLiteral ("displayMessage"), testo);
    entry.insert (QStringLiteral ("freq"), qRound (frequenzaHz));
    entry.insert (QStringLiteral ("isTx"), false);
    // La certezza del decodificatore, da 0 a 1, diventa la colonna del
    // rapporto: in RTTY non esiste un SNR come in FT8, e mostrare quanto il
    // Viterbi ci creda dice all'operatore la stessa cosa utile.
    entry.insert (QStringLiteral ("db"), QString::number (qRound (qualita * 100.0)));
    entry.insert (QStringLiteral ("dt"), QStringLiteral ("0.0"));

    appendDecodeMapToList (entry);
}

void DecodiumBridge::aggiungiRigaJtty (QString const& testo, double frequenzaHz,
                                      QDateTime const& inizioUtc)
{
    if (testo.trimmed ().isEmpty ())
        return;
    QDateTime const quando = inizioUtc.isValid () ? inizioUtc.toUTC ()
                                                  : QDateTime::currentDateTimeUtc ();
    QVariantMap entry;
    entry.insert (QStringLiteral ("mode"), QStringLiteral ("JTTY"));
    entry.insert (QStringLiteral ("time"), quando.toString (QStringLiteral ("hhmmss")));
    entry.insert (QStringLiteral ("utc"), quando.toString (QStringLiteral ("hhmm")));
    entry.insert (QStringLiteral ("timestamp"), quando.toMSecsSinceEpoch ());
    entry.insert (QStringLiteral ("message"), testo);
    entry.insert (QStringLiteral ("displayMessage"), testo);
    entry.insert (QStringLiteral ("freq"), qRound (frequenzaHz));
    entry.insert (QStringLiteral ("isTx"), false);
    entry.insert (QStringLiteral ("db"), QString ());
    entry.insert (QStringLiteral ("dt"), QStringLiteral ("0.0"));
    appendDecodeMapToList (entry);
}

// ALL.TXT come lo scrive WSJT-X 3.2 per JTTY: niente SNR ne' DT, la
// frequenza audio della riga e il testo del messaggio completo.
void DecodiumBridge::appendJttyAllTxt (bool trasmesso, int frequenzaAudio,
                                      QString const& testo, QDateTime const& quando) const
{
    if (testo.trimmed ().isEmpty ())
        return;
    QString const path = legacyAllTxtPath ();
    QDir ().mkpath (QFileInfo (path).absolutePath ());
    QFile file (path);
    if (!file.open (QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
        return;
    QDateTime const utc = quando.isValid () ? quando.toUTC () : QDateTime::currentDateTimeUtc ();
    QTextStream out (&file);
    out << QStringLiteral ("%1 %2  %3  JTTY  %4  %5 %6  %7")
              .arg (utc.toString (QStringLiteral ("yyMMdd_hhmmss")))
              .arg (m_frequency / 1e6, 10, 'f', 3)
              .arg (trasmesso ? QStringLiteral ("Tx") : QStringLiteral ("Rx"))
              .arg (QStringLiteral ("0"), 4)
              .arg (QStringLiteral ("0.0"), 5)
              .arg (frequenzaAudio, 5)
              .arg (testo.trimmed ())
        << '\n';
}

