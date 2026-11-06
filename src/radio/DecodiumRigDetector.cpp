#include "DecodiumRigDetector.h"

#include "DecoPortRadioLibrary.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QMap>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
#endif

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
    // Solo Windows: l'apparato fisico a cui appartiene la porta, ricavato
    // dall'istanza PnP ("USB\VID_10C4&PID_EA70\8&B523DD2&0"). Esiste anche
    // quando il chip non ha numero di serie, ed e' cio' che tiene separate due
    // radio identiche attaccate allo stesso hub.
    QString physicalId;
    // La funzione USB della porta ("VID_10C4&PID_EA70&MI_01"): distingue le
    // porte di un adattatore multiplo senza dipendere dal numero COM.
    QString interfaceTag;
    // Porta creata da un driver di porte virtuali (com0com, VSPE...). Non e'
    // una radio, e se la proponessimo come tale DecoPort finirebbe per
    // condividere le proprie stesse porte.
    bool    isVirtual {false};
    quint16 vid {0};
    quint16 pid {0};
    bool    hasIds {false};
};

// ── Firme note ───────────────────────────────────────────────────────────────
// Stanno nella libreria radio (data/radios-decoport.json), non qui: firme USB,
// modelli riconoscibili dal nome, interfacce di terze parti. Volutamente poche
// e solide. Una firma sbagliata e' peggio di nessuna firma: farebbe scegliere
// all'utente una radio che non ha. Quando non si puo' dire il modello si
// dichiara solo cio' che si sa (l'interfaccia) con fiducia bassa.

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

// ── Windows: cosa sa il sistema di ogni porta presente ───────────────────────
// Qt riporta la stessa descrizione per entrambe le porte di un CP2105 ("CP2105
// Dual USB to UART Bridge Controller"), quindi da sola non basta. Il nome che
// contiene "Enhanced"/"Standard" e l'istanza PnP li sa il gestore dispositivi.
//
// Si chiedono a SetupAPI e non al ramo Enum del registro: il registro ricorda
// anche gli apparati che non ci sono piu', e su un hub ogni presa usata lascia
// la sua voce fantasma con lo stesso nome COM di quella vera. DIGCF_PRESENT
// risponde solo per cio' che e' attaccato adesso, su qualunque bus: USB,
// FTDIBUS, PCI per le schede multiporta.
struct SystemPort
{
    QString friendlyName;
    QString instanceId;   // USB\VID_10C4&PID_EA70&MI_00\8&B523DD2&0&0000
};

#if defined(Q_OS_WIN)
QString deviceText(HDEVINFO set, SP_DEVINFO_DATA& info, DWORD property)
{
    wchar_t buf[512] = {};
    DWORD type = 0;
    if (!SetupDiGetDeviceRegistryPropertyW(set, &info, property, &type,
                                           reinterpret_cast<PBYTE>(buf),
                                           sizeof(buf) - sizeof(wchar_t), nullptr)
        || type != REG_SZ)
        return {};
    return QString::fromWCharArray(buf).trimmed();
}
#endif

QMap<QString, SystemPort> windowsPresentPorts()
{
    QMap<QString, SystemPort> byPort;
#if defined(Q_OS_WIN)
    // Tutte le classi, non solo "Ports": una porta seriale e' qualunque
    // dispositivo con un PortName. L'estremo nascosto di com0com sta nella sua
    // classe CNCPorts, e alcune radio USB si registrano come modem; guardando
    // solo "Ports" il primo passerebbe per una radio e le seconde non si
    // vedrebbero.
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE)
        return byPort;

    SP_DEVINFO_DATA info;
    info.cbSize = sizeof(info);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &info); ++i) {
        HKEY key = SetupDiOpenDevRegKey(set, &info, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (key == INVALID_HANDLE_VALUE)
            continue;
        wchar_t name[64] = {};
        DWORD size = sizeof(name) - sizeof(wchar_t);
        DWORD type = 0;
        const LONG rc = RegQueryValueExW(key, L"PortName", nullptr, &type,
                                         reinterpret_cast<LPBYTE>(name), &size);
        RegCloseKey(key);
        if (rc != ERROR_SUCCESS || type != REG_SZ)
            continue;
        const QString port = QString::fromWCharArray(name).trimmed().toUpper();
        // Anche le parallele hanno un PortName.
        if (port.isEmpty() || port.startsWith(QLatin1String("LPT")))
            continue;

        SystemPort sp;
        sp.friendlyName = deviceText(set, info, SPDRP_FRIENDLYNAME);
        wchar_t instance[512] = {};
        if (SetupDiGetDeviceInstanceIdW(set, &info, instance, 511, nullptr))
            sp.instanceId = QString::fromWCharArray(instance);
        byPort.insert(port, sp);
    }
    SetupDiDestroyDeviceInfoList(set);
#endif
    return byPort;
}

// L'apparato fisico di un'istanza PnP. Le funzioni di un chip composito
// ("...&MI_00\8&B523DD2&0&0000", "...&MI_01\8&B523DD2&0&0001") hanno lo stesso
// padre: si toglie l'interfaccia dall'identita' hardware e il numero di
// funzione dall'istanza. Un dispositivo a funzione singola e' gia' se stesso.
QString physicalIdOf(const QString& instanceId)
{
    QStringList parts = instanceId.split(QLatin1Char('\\'));
    if (parts.size() != 3)
        return instanceId;
    static const QRegularExpression kInterface(QStringLiteral("&MI_[0-9A-F]{2}$"),
                                               QRegularExpression::CaseInsensitiveOption);
    if (!kInterface.match(parts[1]).hasMatch())
        return instanceId;
    parts[1].remove(kInterface);
    const int amp = parts[2].lastIndexOf(QLatin1Char('&'));
    if (amp > 0)
        parts[2].truncate(amp);
    return parts.join(QLatin1Char('\\'));
}

// Le porte dei driver virtuali stanno su enumeratori propri, non su un bus.
bool isVirtualEnumerator(const QString& instanceId)
{
    const QString bus = instanceId.section(QLatin1Char('\\'), 0, 0).toUpper();
    return bus == QLatin1String("ROOT") || bus == QLatin1String("COM0COM")
        || bus.contains(QLatin1String("VSP")) || bus.contains(QLatin1String("VIRTUAL"));
}

QList<PortEntry> collectPorts()
{
    const QMap<QString, SystemPort> system = windowsPresentPorts();

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
        const SystemPort sp = system.value(entry.name.toUpper());
        entry.friendlyName = sp.friendlyName;
        if (!sp.instanceId.isEmpty()) {
            entry.physicalId   = physicalIdOf(sp.instanceId);
            entry.interfaceTag = sp.instanceId.section(QLatin1Char('\\'), 1, 1);
            entry.isVirtual    = isVirtualEnumerator(sp.instanceId);
        }
        entries.append(entry);
    }
    return entries;
}

// Tutto il testo con cui il sistema descrive la porta, per le ricerche. C'e'
// anche il numero di serie: gli Icom ci scrivono il modello ("IC-7300
// 03001234"), ed e' spesso l'unico posto in cui la radio dice il proprio nome.
QString searchableText(const PortEntry& entry)
{
    return QStringList{entry.friendlyName, entry.description, entry.manufacturer,
                       entry.serialNumber}
        .join(QLatin1Char(' '));
}

// Le porte dello stesso apparato condividono numero di serie e identita' USB.
// Un numero di serie vuoto non identifica niente: i CH340, molti PL2303 e i
// CP2105 delle radio Yaesu non ne hanno, e raggruppare per VID:PID fonderebbe
// in un solo apparato tutte le radio uguali attaccate allo stesso hub. Li'
// decide l'istanza PnP, che Windows ricava dalla presa a cui sono attaccati.
QString deviceKey(const PortEntry& entry)
{
    if (entry.hasIds && !entry.serialNumber.isEmpty()) {
        return QStringLiteral("%1:%2:%3")
            .arg(entry.vid, 4, 16, QLatin1Char('0'))
            .arg(entry.pid, 4, 16, QLatin1Char('0'))
            .arg(entry.serialNumber);
    }
    if (!entry.physicalId.isEmpty())
        return QStringLiteral("pnp:") + entry.physicalId;
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
        if (entry.isVirtual)
            continue;
        const QString key = deviceKey(entry);
        if (!groups.contains(key))
            order.append(key);
        groups[key].append(entry);
    }

    const AudioPair audio = findRigAudio();
    const DecoPortRadioLibrary& library = DecoPortRadioLibrary::instance();

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
        // Vero quando sappiamo che le porte del gruppo sono UNA radio: modello o
        // firma. Solo allora le porte restano insieme.
        bool isRadio = false;
        // Il dialetto CAT, quando la firma lo dice: serve a condividere la
        // porta nativa prima ancora di aver risolto il modello.
        QString protocol;

        // 1) Il modello scritto per esteso e' la prova piu' forte.
        for (const auto& model : library.nameTokens()) {
            if (text.contains(model.token, Qt::CaseInsensitive)) {
                rigLabel   = model.label;
                rigToken   = model.token;
                baudRate   = model.baud;
                civAddress = model.civ;
                confidence = 90;
                evidence   = QStringLiteral("il sistema nomina esplicitamente %1").arg(model.token);
                isRadio    = true;
                break;
            }
        }

        // 2) Altrimenti la firma USB, che identifica la famiglia.
        if (rigLabel.isEmpty() && group.first().hasIds) {
            for (const auto& signature : library.usbSignatures()) {
                const bool vidHit = signature.vid == group.first().vid;
                const bool pidHit = signature.pid == 0 || signature.pid == group.first().pid;
                if (vidHit && pidHit) {
                    rigLabel   = signature.label;
                    rigToken   = signature.token;
                    catHint    = signature.catHint;
                    baudRate   = signature.baud;
                    confidence = signature.confidence;
                    evidence   = signature.evidence;
                    protocol   = signature.protocol;
                    isRadio    = true;
                    break;
                }
            }
        }

        // 3) Altrimenti almeno il tipo di interfaccia, dichiarandolo per quello
        //    che e': non sappiamo quale radio ci sia dietro.
        if (rigLabel.isEmpty()) {
            for (const auto& iface : library.interfaceTokens()) {
                if (text.contains(iface.token, Qt::CaseInsensitive)) {
                    rigLabel   = iface.label;
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

        // Quando la firma indica quale porta e' il CAT e la troviamo davvero,
        // il riconoscimento vale di piu': e' la scelta che l'utente sbaglia.
        if (!catHint.isEmpty()
            && group.first().friendlyName.contains(catHint, Qt::CaseInsensitive)) {
            confidence = std::min(95, confidence + 15);
            evidence += QStringLiteral("; la porta %1 e' quella marcata come CAT")
                            .arg(group.first().name);
        }

        auto emitCandidate = [&](const QList<PortEntry>& ports, const QString& id,
                                 const QString& why) {
            QStringList otherPorts;
            for (int i = 1; i < ports.size(); ++i)
                otherPorts.append(ports.at(i).name);

            QVariantMap candidate;
            // Chi deve ritrovare lo stesso apparato dopo averlo staccato usa
            // questo, non il numero COM: Windows lo cambia a ogni presa.
            candidate.insert(QStringLiteral("deviceKey"),   id);
            candidate.insert(QStringLiteral("isRadio"),     isRadio);
            candidate.insert(QStringLiteral("protocol"),    protocol);
            candidate.insert(QStringLiteral("rigLabel"),    rigLabel);
            candidate.insert(QStringLiteral("rigToken"),    rigToken);
            candidate.insert(QStringLiteral("catPort"),     ports.first().name);
            candidate.insert(QStringLiteral("otherPorts"),  otherPorts);
            candidate.insert(QStringLiteral("baudRate"),    baudRate);
            candidate.insert(QStringLiteral("civAddress"),  civAddress);
            candidate.insert(QStringLiteral("audioInput"),  audio.input);
            candidate.insert(QStringLiteral("audioOutput"), audio.output);
            candidate.insert(QStringLiteral("confidence"),  confidence);
            candidate.insert(QStringLiteral("evidence"),    evidence + why);
            candidate.insert(QStringLiteral("systemName"),
                             ports.first().friendlyName.isEmpty() ? ports.first().description
                                                                  : ports.first().friendlyName);
            candidates.append(candidate);
        };

        // Piu' porte che non sappiamo essere una radio sono un hub o un
        // adattatore multiplo (CP2108, FT4232, schede PCI): ogni presa puo'
        // avere dietro una radio diversa, quindi ognuna e' un apparato a se'.
        if (isRadio || group.size() == 1) {
            emitCandidate(group, key, QString());
        } else {
            for (const PortEntry& port : group) {
                const QString tag = port.interfaceTag.isEmpty() ? port.name : port.interfaceTag;
                emitCandidate({port}, key + QLatin1Char('|') + tag,
                              QStringLiteral("; presa di un adattatore a %1 porte, trattata da sola")
                                  .arg(group.size()));
            }
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const QVariant& a, const QVariant& b) {
        return a.toMap().value(QStringLiteral("confidence")).toInt()
             > b.toMap().value(QStringLiteral("confidence")).toInt();
    });

    return candidates;
}

} // namespace DecodiumRigDetector
