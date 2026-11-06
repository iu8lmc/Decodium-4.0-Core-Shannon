// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

// La libreria delle radio: cosa sono, come parlano, come si riconoscono.
//
// Tre strati, nell'ordine in cui contano:
//
//   1. il file dell'utente, se c'e' (DECOPORT_RADIOS o setUserFile): corregge
//      e aggiunge, e vince su tutto il resto;
//   2. quello che sa DecoPort e Hamlib no (data/radios-decoport.json): come si
//      presentano le radio al sistema operativo, e le lacune delle tabelle;
//   3. quello che sa Hamlib (data/radios-hamlib.json), ricavato dai suoi
//      sorgenti da tools/build_radio_library.py: ogni modello, il suo
//      dialetto CAT, i parametri seriali, e le tabelle con cui Hamlib stessa
//      riconosce una radio dalla risposta a "ID;" o dall'indirizzo CI-V.
//
// I due file di DecoPort sono dentro il programma: la libreria c'e' anche dove
// Hamlib non e' installata, e non dipende da quale versione lo sia.
class DecoPortRadioLibrary
{
public:
    struct Model {
        int     model {0};
        QString symbol;      // FT991
        QString mfg;         // Yaesu
        QString name;        // FT-991
        QString status;      // Stable, Beta, Alpha, Untested, Buggy
        QString backend;     // yaesu, icom, kenwood, ...
        // Il dialetto CAT: newcat, kenwood, civ, yaesu5, binary. E' cio' che
        // dice al multiplexer dove finisce un comando e a chi va una risposta.
        QString protocol;
        QString port;        // serial, network, usb, none, other
        int     baudMin {0};
        int     baudMax {0};
        int     dataBits {8};
        int     stopBits {1};
        QChar   parity {QLatin1Char('N')};
        QString handshake;   // none, hardware, xonxoff

        QString label() const { return (mfg + QLatin1Char(' ') + name).trimmed(); }
    };

    struct UsbSignature {
        quint16 vid {0};
        quint16 pid {0};     // 0 = qualunque prodotto di quel costruttore
        QString label;
        QString token;
        QString catHint;
        QString protocol;
        QString evidence;
        int     baud {0};
        int     confidence {0};
    };

    struct NameToken {
        QString token;
        QString label;
        int     baud {0};
        int     civ {0};
    };

    struct InterfaceToken {
        QString token;
        QString label;
    };

    // La libreria condivisa, caricata alla prima richiesta.
    static const DecoPortRadioLibrary& instance();
    // Il file dell'utente da sovrapporre. Va chiamata prima di instance(), o
    // seguita da reload().
    static void setUserFile(const QString& path);
    static void reload();

    // "Hamlib 4.7.0 + DecoPort (+ file utente)": da dove viene quello che sa.
    QString source() const { return m_source; }
    // Problemi di caricamento, da mostrare e non da nascondere.
    QStringList warnings() const { return m_warnings; }

    const QVector<Model>& models() const { return m_models; }
    const Model* model(int number) const;

    // Le voci il cui nome, costruttore o simbolo contiene il testo.
    QVector<const Model*> search(const QString& text) const;
    // Un modello da come lo scrive una persona: "1035", "FT-991", "Yaesu
    // FT-991", "ft991". 0 se non c'e' o se e' ambiguo, e `why` dice quale dei
    // due e fra chi.
    int resolve(const QString& text, QString* why = nullptr) const;

    // Le tabelle di riconoscimento. Una risposta puo' valere piu' modelli: il
    // primo e' quello che sceglierebbe Hamlib.
    QVector<int> modelsForKenwoodId(const QString& id) const { return m_kenwoodIds.value(id); }
    QVector<int> modelsForYaesuId(const QString& id) const { return m_yaesuIds.value(id); }
    QVector<int> modelsForCivAddress(int addr) const { return m_civ.value(addr); }
    // Vero se a quella risposta "ID;" puo' esserci dietro anche un Elecraft.
    bool idSharedWithElecraft(const QString& id) const { return m_elecraftIds.contains(id); }
    // L'indirizzo CI-V predefinito di un modello Icom, 0 se non e' noto.
    int defaultCivAddress(int model) const;

    // Il dialetto di un modello, vuoto se il modello non c'e'.
    QString protocolOf(int model) const;

    const QVector<UsbSignature>&   usbSignatures() const { return m_usb; }
    const QVector<NameToken>&      nameTokens() const { return m_names; }
    const QVector<InterfaceToken>& interfaceTokens() const { return m_interfaces; }

private:
    DecoPortRadioLibrary();
    void load(const QString& userFile);
    bool merge(const QByteArray& json, const QString& origin, bool prepend);

    QString     m_source;
    QStringList m_warnings;
    QVector<Model> m_models;
    QHash<int, int> m_byNumber;   // modello -> indice in m_models
    QHash<QString, QVector<int>> m_kenwoodIds;
    QHash<QString, QVector<int>> m_yaesuIds;
    QHash<int, QVector<int>>     m_civ;
    QStringList m_elecraftIds;
    QVector<UsbSignature>   m_usb;
    QVector<NameToken>      m_names;
    QVector<InterfaceToken> m_interfaces;
};
