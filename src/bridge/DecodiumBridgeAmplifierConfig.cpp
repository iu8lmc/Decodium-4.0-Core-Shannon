#include "DecodiumBridge.h"
#include <QDebug>

void decodiumBridgeAmplifierLog(const QString& message);
static bool amplifierPortClashesWithCat(DecodiumTransceiverManager* cat, const QString& port)
{
    if (!cat || port.trimmed().isEmpty())
        return false;
    QString const wanted = port.trimmed().toUpper();
    return wanted == cat->serialPort().trimmed().toUpper()
        || wanted == cat->pttPort().trimmed().toUpper();
}

bool decodiumBridgeAmplifierPortClashes(DecodiumTransceiverManager* cat, const QString& port)
{
    return amplifierPortClashesWithCat(cat, port);
}


QVariantMap DecodiumBridge::cercaAmplificatore()
{
    QVariantMap esito;

    // Le porte da non toccare: quelle che il CAT dell'applicazione tiene
    // aperte, e quella del PTT se e' su una seriale a parte.
    QStringList escluse;
    if (m_hamlibCat) {
        escluse << m_hamlibCat->serialPort().trimmed();
        escluse << m_hamlibCat->pttPort().trimmed();
    }
    escluse.removeAll(QString());

    decodiumBridgeAmplifierLog(QStringLiteral("[AMP] ricerca avviata; porte escluse: %1")
                  .arg(escluse.isEmpty() ? QStringLiteral("nessuna") : escluse.join(QStringLiteral(", "))));

    auto const trovati = DecodiumAmplifier::cerca(escluse);
    if (trovati.isEmpty()) {
        esito.insert(QStringLiteral("trovato"), false);
        esito.insert(QStringLiteral("escluse"), escluse);
        return esito;
    }

    auto const& t = trovati.first();
    esito.insert(QStringLiteral("trovato"), true);
    esito.insert(QStringLiteral("porta"), t.porta);
    esito.insert(QStringLiteral("modello"), t.modello);
    esito.insert(QStringLiteral("baud"), t.baud);
    esito.insert(QStringLiteral("descrizione"), t.descrizione);

    // Si configura da se': una ricerca che trova e poi lascia all'operatore il
    // compito di ricopiare a mano quello che ha appena letto non ha fatto meta'
    // del lavoro. In interrogazione attiva, non in ascolto passivo: l'apparato
    // parla solo se interrogato, e il passivo sarebbe un'attesa senza fine.
    configureAmplifier(true, t.porta, t.baud, false, 500);
    decodiumBridgeAmplifierLog(QStringLiteral("[AMP] configurato da se': %1 su %2 a %3 baud")
                  .arg(t.modello, t.porta).arg(t.baud));
    return esito;
}

void DecodiumBridge::configureAmplifier(bool enabled, const QString& port,
                                        int baud, bool passive, int pollMs)
{
    if (enabled && decodiumBridgeAmplifierPortClashes(m_hamlibCat, port)) {
        qWarning().noquote()
            << "[AMP] rifiutata la porta" << port
            << ": e' quella del CAT. Indicare la porta dell'amplificatore.";
        enabled = false;
    }
    setSetting(QStringLiteral("AmpEnabled"), enabled);
    setSetting(QStringLiteral("AmpPort"), port);
    setSetting(QStringLiteral("AmpBaud"), baud);
    setSetting(QStringLiteral("AmpPassive"), passive);
    setSetting(QStringLiteral("AmpPollMs"), pollMs);
    if (m_amplifier)
        m_amplifier->configure(enabled, port, baud, passive, pollMs);
}

void DecodiumBridge::configureAmplifierManufacturer(const QString& manufacturer)
{
    QString value = manufacturer.trimmed();
    if (value.isEmpty())
        value = QStringLiteral("SPE");
    setSetting(QStringLiteral("AmpManufacturer"), value);
    if (m_amplifier)
        m_amplifier->setManufacturer(value);
}

