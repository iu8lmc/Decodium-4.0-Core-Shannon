static QString canonicalApplicationDecodeMode(QString mode)
{
    QString const upperMode = mode.trimmed().toUpper();
    if (upperMode == QStringLiteral("FT2-LINK")
        || upperMode == QStringLiteral("FT2LINK")
        || upperMode == QStringLiteral("D4LINK")
        || upperMode == QStringLiteral("D4 LINK")) {
        return QStringLiteral("FT2-Link");
    }
    if (upperMode == QStringLiteral("FT8")
        || upperMode == QStringLiteral("FT2")
        || upperMode == QStringLiteral("FT4")
        || upperMode == QStringLiteral("Q65")
        || upperMode == QStringLiteral("MSK144")
        || upperMode == QStringLiteral("JT65")
        || upperMode == QStringLiteral("JT9")
        || upperMode == QStringLiteral("JT4")
        || upperMode == QStringLiteral("FST4")
        || upperMode == QStringLiteral("FST4W")
        || upperMode == QStringLiteral("WSPR")
        || upperMode.startsWith(QStringLiteral("Q65-"))
        || upperMode.startsWith(QStringLiteral("FST4-"))
        || upperMode.startsWith(QStringLiteral("FST4W-"))) {
        return upperMode;
    }
    if (upperMode == QStringLiteral("ECHO")) {
        return QStringLiteral("Echo");
    }
    // RTTY e' un modo dell'applicazione come gli altri, e va riconosciuto qui
    // prima che isRadioOnlyModeLabel() lo scambi per il nome di un modo della
    // radio: quella lista serve a non far commutare l'applicazione quando la
    // radio riporta "RTTY" o "USB", ma quando e' l'operatore a sceglierlo dal
    // selettore dei modi la richiesta e' un'altra cosa. Le due strade passano
    // di qui in ordine, e questa viene prima.
    if (upperMode == QStringLiteral("RTTY")) {
        return QStringLiteral("RTTY");
    }
    // JTTY (WSJT-X 3.2): testo da tastiera a frame da 1,888 s, trasmissione
    // libera nel tempo. Modo dell'applicazione, non della radio.
    if (upperMode == QStringLiteral("JTTY")) {
        return QStringLiteral("JTTY");
    }
    // CW: il modulo CW (decodificatore, macro, manipolatore) e' un modo
    // dell'applicazione come JTTY, non solo un nome di modo della radio.
    if (upperMode == QStringLiteral("SSB")) return QStringLiteral("SSB");
    if (upperMode == QStringLiteral("CW")) {
        return QStringLiteral("CW");
    }
    return {};
}

QString decodiumBridgeCanonicalApplicationDecodeMode(QString mode) { return canonicalApplicationDecodeMode(mode); }

static bool isFt2LinkApplicationMode(QString mode)
{
    QString const upperMode = mode.trimmed().toUpper();
    return upperMode == QStringLiteral("FT2-LINK")
        || upperMode == QStringLiteral("FT2LINK")
        || upperMode == QStringLiteral("D4LINK")
        || upperMode == QStringLiteral("D4 LINK");
}

bool decodiumBridgeIsFt2LinkApplicationMode(QString mode) { return isFt2LinkApplicationMode(mode); }

// RTTY e JTTY non passano dai decodificatori a slot: hanno un proprio modem a
// flusso continuo, alimentato dalla cattura PCM nativa, e l'area di lavoro dei
// modi digitali (backend legacy, code di decodifica, timer di periodo) resta
// ferma finche' uno dei due e' il modo attivo.
static bool isStreamingKeyboardMode(QString const& mode)
{
    QString const m = mode.trimmed();
    return m.compare(QStringLiteral("RTTY"), Qt::CaseInsensitive) == 0
        || m.compare(QStringLiteral("JTTY"), Qt::CaseInsensitive) == 0
        || m.compare(QStringLiteral("SSB"), Qt::CaseInsensitive) == 0
        || m.compare(QStringLiteral("CW"), Qt::CaseInsensitive) == 0;
}

// JTTY e CW tengono la cattura PCM nativa mentre sono attivi. Uscendone serve
// un passaggio di consegne soltanto se il nuovo modo assegna davvero la
// periferica al backend legacy; il percorso moderno riusa lo stesso stream.
bool decodiumBridgeIsStreamingKeyboardMode(QString const& mode)
{
    return isStreamingKeyboardMode(mode);
}

static bool isNativePcmKeyboardMode(QString const& mode)
{
    QString const m = mode.trimmed();
    return m.compare(QStringLiteral("JTTY"), Qt::CaseInsensitive) == 0
        || m.compare(QStringLiteral("SSB"), Qt::CaseInsensitive) == 0
        || m.compare(QStringLiteral("CW"), Qt::CaseInsensitive) == 0;
}

bool decodiumBridgeIsNativePcmKeyboardMode(QString const& mode) { return isNativePcmKeyboardMode(mode); }

static bool isRadioOnlyModeLabel(QString mode)
{
    QString const upperMode = mode.trimmed().toUpper();
    return upperMode == QStringLiteral("DATA")
        || upperMode == QStringLiteral("DATA-U")
        || upperMode == QStringLiteral("DATA-L")
        || upperMode == QStringLiteral("DATA-USB")
        || upperMode == QStringLiteral("DATA-LSB")
        || upperMode == QStringLiteral("USB-D")
        || upperMode == QStringLiteral("DIGU")
        || upperMode == QStringLiteral("DIGL")
        || upperMode == QStringLiteral("PKT")
        || upperMode == QStringLiteral("PKT-U")
        || upperMode == QStringLiteral("PKT-L")
        || upperMode == QStringLiteral("USB")
        || upperMode == QStringLiteral("LSB")
        || upperMode == QStringLiteral("CW")
        || upperMode == QStringLiteral("CW-R")
        || upperMode == QStringLiteral("AM")
        || upperMode == QStringLiteral("FM")
        || upperMode == QStringLiteral("RTTY")
        || upperMode == QStringLiteral("RTTY-U")
        || upperMode == QStringLiteral("RTTY-L");
}

bool decodiumBridgeIsRadioOnlyModeLabel(QString mode) { return isRadioOnlyModeLabel(mode); }

