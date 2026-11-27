#include "DecodiumBridge.h"

QString DecodiumBridge::configuredCatRigMode() const
{
    QVariant const raw = getSetting(QStringLiteral("CATMode"), 0);
    QString const rawText = raw.toString().trimmed();

    if (rawText.compare(QStringLiteral("None"), Qt::CaseInsensitive) == 0) {
        return {};
    }
    if (rawText.compare(QStringLiteral("Data/Pkt LSB"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("DATA-L"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("PKTLSB"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("DIGL"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("DATA-L");
    }
    if (rawText.compare(QStringLiteral("Data/Pkt"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("DATA-U"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("USB-D"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("DIGU"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("DATA-U");
    }
    if (rawText.compare(QStringLiteral("USB"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("USB");
    }

    bool ok = false;
    int const index = raw.toInt(&ok);
    if (!ok) {
        return QStringLiteral("USB");
    }
    if (index == 1) {
        return QStringLiteral("DATA-U");
    }
    if (index == 2) {
        return {};
    }
    if (index == 3) {
        return QStringLiteral("DATA-L");
    }
    return QStringLiteral("USB");
}

bool DecodiumBridge::configuredCatRigModeRequestsDataPacket() const
{
    QVariant const raw = getSetting(QStringLiteral("CATMode"), 0);
    QString const rawText = raw.toString().trimmed();

    if (rawText.compare(QStringLiteral("Data/Pkt LSB"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("DATA-L"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("PKTLSB"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("DIGL"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("Data/Pkt"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("DATA-U"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("DATA-USB"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("USB-D"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("DIGU"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("PKT"), Qt::CaseInsensitive) == 0
        || rawText.compare(QStringLiteral("PKT-U"), Qt::CaseInsensitive) == 0) {
        return true;
    }

    bool ok = false;
    int const index = raw.toInt(&ok);
    return ok && (index == 1 || index == 3);
}

QString DecodiumBridge::rttyRigMode() const
{
    // Il modo dell'RTTY vero, non quello dei dati. Su una FT-991A la differenza
    // conta: in DATA-U l'apparato apre il passabanda dei modi digitali, in
    // RTTY-U stringe attorno ai due toni ed e' quello che si vuole quando si
    // copia RTTY. Chi trasmette in AFSK dall'audio deve invece restare sui
    // dati, perche' nei modi RTTY l'apparato aspetta il tasto FSK e l'audio in
    // ingresso non modula: per questo la scelta e' un'impostazione e non una
    // decisione presa qui.
    QVariant const raw = getSetting(QStringLiteral("RttyRigMode"),
                                    QStringLiteral("RTTY-U"));
    QString const testo = raw.toString().trimmed();
    if (testo.compare(QStringLiteral("None"), Qt::CaseInsensitive) == 0)
        return {};
    return testo;
}

