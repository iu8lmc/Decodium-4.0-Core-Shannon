#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>

QString DecodiumBridge::launchSecondInstance(const QString& profileName,
                                             bool useSharedCat)
{
    QString const name = profileName.trimmed();
    if (name.isEmpty())
        return tr("Give the second instance a name.");
    // Gli stessi caratteri che rifiuta --rig-name: finirebbero in un nome di
    // gruppo delle impostazioni, e li' una barra o una virgola fanno danni.
    if (name.contains(QRegularExpression(QStringLiteral(R"([\\/,])"))))
        return tr("The name cannot contain \\ / or ,");
    if (name.compare(decodium::activeSettingsProfileName(), Qt::CaseInsensitive) == 0)
        return tr("This instance is already using that name.");

    // Il profilo nuovo nasce copiando la radice: eredita anche la porta
    // seriale e la scheda audio di questa istanza, che pero' sono gia'
    // occupate. Quello che non puo' essere condiviso va tolto subito, prima
    // che la seconda istanza ci provi e si trovi un errore all'avvio.
    decodium::ensureSettingsProfileInitialized(name);

    int sharePort = 4533;
    if (useSharedCat) {
        sharePort = decodium::profiledSettingsValue(QString(),
                                                    QStringLiteral("CatSharePort"),
                                                    4533).toInt();
        if (sharePort < 1024 || sharePort > 65535)
            sharePort = 4533;
        if (!m_catShare || !m_catShare->listening()) {
            // Chiedere la CAT condivisa e non trovarla accesa e' il caso
            // normale la prima volta: si accende, senza toccare il permesso
            // di trasmettere, che ha un interruttore suo.
            bool const allowPtt = decodium::profiledSettingsValue(
                                      QString(), QStringLiteral("CatShareAllowPtt"), false).toBool();
            configureCatShare(true, sharePort, true, allowPtt);
        }
        if (!m_catShare || !m_catShare->listening()) {
            return tr("Shared CAT is not listening on port %1: %2")
                .arg(sharePort)
                .arg(m_catShare ? m_catShare->status() : tr("not available"));
        }
    }

    {
        QSettings s(QSettings::IniFormat, QSettings::UserScope,
                    QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        s.beginGroup(QStringLiteral("MultiSettings"));
        s.beginGroup(name);
        // Due programmi non condividono una porta: la seconda istanza non
        // ricondivide, altrimenti all'avvio trova la porta occupata dalla
        // prima e mostra un errore che non e' un errore.
        s.setValue(QStringLiteral("CatShareEnabled"), false);
        s.beginGroup(QStringLiteral("Transceiver"));
        // La seriale ce l'ha la prima istanza. Lasciarla scritta qui vorrebbe
        // dire farla aprire alla seconda e fallire.
        s.setValue(QStringLiteral("serialPort"), QString());
        if (useSharedCat) {
            s.setValue(QStringLiteral("rigName"), QStringLiteral("Hamlib NET rigctl"));
            s.setValue(QStringLiteral("networkPort"),
                       QStringLiteral("127.0.0.1:%1").arg(sharePort));
        }
        s.endGroup();
        if (useSharedCat)
            s.setValue(QStringLiteral("catBackend"), QStringLiteral("hamlib"));
        s.endGroup();
        s.endGroup();
        s.sync();
    }

    QString const program = QCoreApplication::applicationFilePath();
    if (!QProcess::startDetached(program, {QStringLiteral("--rig-name"), name})) {
        return tr("Could not start %1").arg(program);
    }
    qInfo().noquote() << "[MULTI] seconda istanza avviata con profilo" << name
                      << (useSharedCat ? "su CAT condivisa" : "con CAT propria");
    return QString();
}

