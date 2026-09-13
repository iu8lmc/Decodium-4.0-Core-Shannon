#include "DecodiumRigDetector.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QMap>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>

namespace {

// ── Come si presenta una porta al sistema ─────────────────────────────────────
struct PortEntry
{
    QString name;          // COM5, /dev/ttyUSB0, ...
    QString location;      // \\.\COM5
    QString description;   // quella di Qt: spesso il nome del chip, non della radio
    QString manufacturer;
    QString serialNumber;  // uguale per tutte le porte dello stesso apparato
    QString friendlyName;  // solo Windows: e' QUI che compare "Enhanced"/"Standard"
    quint16 vid {0};
    quint16 pid {0};
    bool    hasIds {false};
};

// ── Firme note ───────────────────────────────────────────────────────────────
// Volutamente poche e solide. Una firma sbagliata e' peggio di nessuna firma:
// farebbe scegliere all'utente una radio che non ha. Quando non si puo' dire il
// modello si dichiara solo cio' che si sa (l'interfaccia) con fiducia bassa.
struct RigSignature
{
    quint16     vid;
    quint16     pid;          // 0 = qualunque prodotto di quel costruttore
    const char* rigLabel;
    const char* rigToken;     // vuoto se il modello non e' deducibile dalla sola USB
    const char* catPortHint;  // testo che nel nome di sistema marca la porta CAT
    int         baudRate;
    int         confidence;
    const char* evidence;
};

const RigSignature kSignatures[] = {
    // Yaesu FT-991/FT-991A/FT-DX10 espongono un CP2105 a DUE porte: la
    // "Enhanced" e' il CAT, la "Standard" e' l'altra. E' proprio la scelta su
    // cui sbaglia chi configura a mano.
    { 0x10C4, 0xEA70,
      "Yaesu FT-991 / FT-991A / FT-DX10", "FT-991", "enhanced", 38400, 75,
      "CP2105 a doppia porta (VID 10C4, PID EA70): tipico di questi Yaesu" },

    // Icom con interfaccia USB propria.
    { 0x0C26, 0x0000,
      "Icom (interfaccia USB propria)", "", "", 19200, 60,
      "costruttore USB dichiarato Icom (VID 0C26)" },
};

// Modelli riconoscibili dal testo, quando il sistema lo espone davvero.
// Il confronto avviene sul nome di sistema, non sul chip.
struct ModelToken
{
    const char* token;
    const char* rigLabel;
    int         baudRate;
    int         civAddress;
};

const ModelToken kModelTokens[] = {
    { "IC-7300",  "Icom IC-7300",     19200, 0x94 },
    { "IC-7610",  "Icom IC-7610",     19200, 0x98 },
    { "IC-9700",  "Icom IC-9700",     19200, 0xA2 },
    { "IC-705",   "Icom IC-705",      19200, 0xA4 },
    { "IC-7600",  "Icom IC-7600",     19200, 0x7A },
    { "FT-991",   "Yaesu FT-991",     38400, 0 },
    { "FTDX10",   "Yaesu FT-DX10",    38400, 0 },
    { "FT-DX10",  "Yaesu FT-DX10",    38400, 0 },
    { "FTDX101",  "Yaesu FT-DX101D",  38400, 0 },
    { "FT-DX101", "Yaesu FT-DX101D",  38400, 0 },
    { "TS-590",   "Kenwood TS-590S",  57600, 0 },
    { "TS-890",   "Kenwood TS-890S", 115200, 0 },
    { "TS-2000",  "Kenwood TS-2000",  57600, 0 },
};

// Interfacce audio/CAT di terze parti diffuse: non dicono quale radio ci sia
// dietro, ma riconoscerle evita di spacciarle per una radio.
struct InterfaceToken
{
    const char* token;
    const char* label;
};

const InterfaceToken kInterfaceTokens[] = {
    { "digirig",    "Digirig" },
    { "rigblaster", "RIGblaster" },
    { "signalink",  "SignaLink" },
    { "ch340",      "adattatore USB-seriale CH340" },
    { "cp210",      "adattatore USB-seriale Silicon Labs" },
    { "ft232",      "adattatore USB-seriale FTDI" },
    { "prolific",   "adattatore USB-seriale Prolific" },
};

// Schede audio integrate nelle radio o nelle interfacce.
const char* const kAudioTokens[] = {
    "USB Audio CODEC",
    "USB AUDIO CODEC",
    "USB PnP Sound Device",
    "USB Audio Device",
    "SignaLink",
    "Digirig",
};

// Schede virtuali: utilissime per le prove, ma non sono la radio. Proporle
// come "audio del rig" manderebbe l'utente fuori strada.
const char* const kVirtualAudioTokens[] = {
    "VB-Audio",
    "CABLE",
    "Virtual Audio Cable",
    "Virtual Cable",
    "Virtual Mixer",
    "WO Mic",
    "Voicemeeter",
};

bool containsToken(const QString& haystack, const char* needle)
{
    return haystack.contains(QString::fromLatin1(needle), Qt::CaseInsensitive);
}

bool isVirtualAudio(const QString& description)
{
    for (const char* token : kVirtualAudioTokens) {
        if (containsToken(description, token))
            return true;
    }
    return false;
}

// ── Windows: il nome che distingue le porte gemelle ──────────────────────────
// Qt riporta la stessa descrizione per entrambe le porte di un CP2105 ("CP2105
// Dual USB to UART Bridge Controller"), quindi da sola non basta. Il nome che
// contiene "Enhanced"/"Standard" sta nel registro, ed e' quello che serve.
QMap<QString, QString> windowsFriendlyNamesByPort()
{
    QMap<QString, QString> byPort;
#if defined(Q_OS_WIN)
    QSettings usbTree(QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Enum\\USB"),
                      QSettings::NativeFormat);
    const QStringList devices = usbTree.childGroups();
    for (const QString& device : devices) {
        usbTree.beginGroup(device);
        const QStringList instances = usbTree.childGroups();
        for (const QString& instance : instances) {
            usbTree.beginGroup(instance);
            const QString port = usbTree.value(QStringLiteral("Device Parameters/PortName")).toString().trimmed();
            const QString friendly = usbTree.value(QStringLiteral("FriendlyName")).toString().trimmed();
            if (!port.isEmpty() && !friendly.isEmpty())
                byPort.insert(port.toUpper(), friendly);
            usbTree.endGroup();
        }
        usbTree.endGroup();
    }
#endif
    return byPort;
}

QList<PortEntry> collectPorts()
{
    const QMap<QString, QString> friendlyNames = windowsFriendlyNamesByPort();

    QList<PortEntry> entries;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        PortEntry entry;
        entry.name         = info.portName();
        entry.location     = info.systemLocation();
        entry.description  = info.description().trimmed();
        entry.manufacturer = info.manufacturer().trimmed();
        entry.serialNumber = info.serialNumber().trimmed();
        entry.hasIds       = info.hasVendorIdentifier() && info.hasProductIdentifier();
        if (entry.hasIds) {
            entry.vid = info.vendorIdentifier();
            entry.pid = info.productIdentifier();
        }
        entry.friendlyName = friendlyNames.value(entry.name.toUpper());
        entries.append(entry);
    }
    return entries;
}

// Tutto il testo con cui il sistema descrive la porta, per le ricerche.
QString searchableText(const PortEntry& entry)
{
    return QStringList{entry.friendlyName, entry.description, entry.manufacturer}
        .join(QLatin1Char(' '));
}

// Le porte dello stesso apparato condividono numero di serie e identita' USB.
QString deviceKey(const PortEntry& entry)
{
    if (entry.hasIds) {
        return QStringLiteral("%1:%2:%3")
            .arg(entry.vid, 4, 16, QLatin1Char('0'))
            .arg(entry.pid, 4, 16, QLatin1Char('0'))
            .arg(entry.serialNumber);
    }
    return QStringLiteral("porta:") + entry.name;
}

// Ordina le porte di un apparato: prima quella indicata dalla firma come CAT.
void sortGroupByCatHint(QList<PortEntry>& group, const QString& hint)
{
    if (hint.isEmpty())
        return;
    std::stable_sort(group.begin(), group.end(),
                     [&hint](const PortEntry& a, const PortEntry& b) {
        const bool aHit = a.friendlyName.contains(hint, Qt::CaseInsensitive);
        const bool bHit = b.friendlyName.contains(hint, Qt::CaseInsensitive);
        return aHit && !bHit;
    });
}

struct AudioPair
{
    QString input;
    QString output;
};

// Cerca ingresso e uscita che appartengano alla stessa scheda del rig.
AudioPair findRigAudio()
{
    AudioPair pair;
    for (const char* token : kAudioTokens) {
        QString foundIn;
        QString foundOut;
        for (const QAudioDevice& device : QMediaDevices::audioInputs()) {
            const QString description = device.description().trimmed();
            if (isVirtualAudio(description))
                continue;
            if (containsToken(description, token)) {
                foundIn = description;
                break;
            }
        }
        for (const QAudioDevice& device : QMediaDevices::audioOutputs()) {
            const QString description = device.description().trimmed();
            if (isVirtualAudio(description))
                continue;
            if (containsToken(description, token)) {
                foundOut = description;
                break;
            }
        }
        if (!foundIn.isEmpty() || !foundOut.isEmpty()) {
            pair.input  = foundIn;
            pair.output = foundOut;
            break;
        }
    }
    return pair;
}

} // namespace

namespace DecodiumRigDetector {

QVariantList detect()
{
    const QList<PortEntry> ports = collectPorts();

    // Raggruppa le porte per apparato, conservando l'ordine di enumerazione.
    QStringList order;
    QMap<QString, QList<PortEntry>> groups;
    for (const PortEntry& entry : ports) {
        const QString key = deviceKey(entry);
        if (!groups.contains(key))
            order.append(key);
        groups[key].append(entry);
    }

    const AudioPair audio = findRigAudio();

    QVariantList candidates;
    for (const QString& key : order) {
        QList<PortEntry> group = groups.value(key);
        if (group.isEmpty())
            continue;

        const QString text = searchableText(group.first());

        QString rigLabel;
        QString rigToken;
        QString catHint;
        QString evidence;
        int baudRate   = 0;
        int civAddress = 0;
        int confidence = 0;

        // 1) Il modello scritto per esteso e' la prova piu' forte.
        for (const ModelToken& model : kModelTokens) {
            if (containsToken(text, model.token)) {
                rigLabel   = QString::fromLatin1(model.rigLabel);
                rigToken   = QString::fromLatin1(model.token);
                baudRate   = model.baudRate;
                civAddress = model.civAddress;
                confidence = 90;
                evidence   = QStringLiteral("il sistema nomina esplicitamente %1")
                                 .arg(QString::fromLatin1(model.token));
                break;
            }
        }

        // 2) Altrimenti la firma USB, che identifica la famiglia.
        if (rigLabel.isEmpty() && group.first().hasIds) {
            for (const RigSignature& signature : kSignatures) {
                const bool vidHit = signature.vid == group.first().vid;
                const bool pidHit = signature.pid == 0 || signature.pid == group.first().pid;
                if (vidHit && pidHit) {
                    rigLabel   = QString::fromLatin1(signature.rigLabel);
                    rigToken   = QString::fromLatin1(signature.rigToken);
                    catHint    = QString::fromLatin1(signature.catPortHint);
                    baudRate   = signature.baudRate;
                    confidence = signature.confidence;
                    evidence   = QString::fromLatin1(signature.evidence);
                    break;
                }
            }
        }

        // 3) Altrimenti almeno il tipo di interfaccia, dichiarandolo per quello
        //    che e': non sappiamo quale radio ci sia dietro.
        if (rigLabel.isEmpty()) {
            for (const InterfaceToken& iface : kInterfaceTokens) {
                if (containsToken(text, iface.token)) {
                    rigLabel   = QString::fromLatin1(iface.label);
                    confidence = 35;
                    evidence   = QStringLiteral(
                        "riconosciuta l'interfaccia, non la radio: il modello va scelto a mano");
                    break;
                }
            }
        }

        if (rigLabel.isEmpty()) {
            rigLabel   = QStringLiteral("Porta seriale");
            confidence = 15;
            evidence   = QStringLiteral("nessun segno riconoscibile: potrebbe non essere una radio");
        }

        sortGroupByCatHint(group, catHint);

        QStringList otherPorts;
        for (int i = 1; i < group.size(); ++i)
            otherPorts.append(group.at(i).name);

        // Quando la firma indica quale porta e' il CAT e la troviamo davvero,
        // il riconoscimento vale di piu': e' la scelta che l'utente sbaglia.
        if (!catHint.isEmpty()
            && group.first().friendlyName.contains(catHint, Qt::CaseInsensitive)) {
            confidence = std::min(95, confidence + 15);
            evidence += QStringLiteral("; la porta %1 e' quella marcata come CAT")
                            .arg(group.first().name);
        }

        QVariantMap candidate;
        candidate.insert(QStringLiteral("rigLabel"),    rigLabel);
        candidate.insert(QStringLiteral("rigToken"),    rigToken);
        candidate.insert(QStringLiteral("catPort"),     group.first().name);
        candidate.insert(QStringLiteral("otherPorts"),  otherPorts);
        candidate.insert(QStringLiteral("baudRate"),    baudRate);
        candidate.insert(QStringLiteral("civAddress"),  civAddress);
        candidate.insert(QStringLiteral("audioInput"),  audio.input);
        candidate.insert(QStringLiteral("audioOutput"), audio.output);
        candidate.insert(QStringLiteral("confidence"),  confidence);
        candidate.insert(QStringLiteral("evidence"),    evidence);
        candidate.insert(QStringLiteral("systemName"),
                         group.first().friendlyName.isEmpty() ? group.first().description
                                                              : group.first().friendlyName);
        candidates.append(candidate);
    }

    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const QVariant& a, const QVariant& b) {
        return a.toMap().value(QStringLiteral("confidence")).toInt()
             > b.toMap().value(QStringLiteral("confidence")).toInt();
    });

    return candidates;
}

} // namespace DecodiumRigDetector
