#ifndef DECODIUMSETTINGSPOOL_H
#define DECODIUMSETTINGSPOOL_H

// QSettings tenuti aperti e riusati sul thread principale.
//
// Il bridge costruiva un QSettings nuovo per quasi ogni lettura: getSetting()
// ne costruiva due (tre con un profilo attivo), activeSettingsProfileName()
// uno, e la lettura dei filtri di decodifica una cinquantina per gruppo di
// decodifiche. Ogni costruzione su Decodium3.ini costa ~140 us e 11 operazioni
// sul file system (controlla i quattro file INI utente/sistema, app/org),
// contro ~0,3 us per leggere un valore da un QSettings gia' aperto.
//
// PooledSettings da' un QSettings costruito come prima ma preso da una piccola
// riserva per file, riportato alla radice dei gruppi quando lo si lascia. Le
// letture restano le stesse:
//  - tutti i QSettings del processo sullo stesso file condividono gli stessi
//    dati in memoria (QConfFile di Qt): cio' che un'altra parte del programma
//    scrive e' visibile subito, anche prima che finisca sul disco;
//  - le modifiche fatte da fuori (seconda istanza, file modificato a mano)
//    arrivano con sync(), che il QSettings riusato fa al piu' ogni
//    kPooledSettingsResyncMs (e che ogni QSettings nuovo del processo fa
//    gia' costruendosi).
// Solo il thread principale usa la riserva; gli altri thread, e le chiamate
// prima che esista QCoreApplication, ricevono un QSettings nuovo come prima.
// Chiamate annidate sullo stesso file (getSetting -> profilo attivo) prendono
// oggetti diversi della riserva: nessuno vede i gruppi aperti da un altro.
//
// La riserva e' spenta finche' qualcuno non chiama enableSettingsPool(),
// quando i percorsi delle impostazioni sono definitivi. Un QSettings tenuto
// aperto resta legato al file scelto quando e' stato costruito: dopo un
// QSettings::setPath (modalita' laboratorio, test) leggerebbe ancora il file
// vecchio. Il programma la accende dopo l'ultimo setPath dell'avvio; i test e
// gli altri programmi che non la accendono costruiscono un QSettings nuovo a
// ogni uso, come prima. Richiamarla dopo un altro setPath scarta gli oggetti
// tenuti, cosi' le letture seguenti vanno al file nuovo.

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSettings>
#include <QString>
#include <QThread>

#include <map>
#include <memory>
#include <vector>

namespace decodium
{

constexpr qint64 kPooledSettingsResyncMs = 250;

namespace settings_pool_detail
{
struct Slot
{
    std::unique_ptr<QSettings> settings;
    QElapsedTimer sinceSync;
    bool inUse {false};
    bool retired {false};
};

struct Pool
{
    bool enabled {false};
    // Non "slots": e' una macro di Qt.
    std::map<QString, std::vector<std::unique_ptr<Slot>>> byFile;
    // Oggetti scartati mentre qualcuno li usava: distrutti al rilascio.
    std::vector<std::unique_ptr<Slot>> retired;
};

inline Pool& pool()
{
    // Lasciata allocata fino all'uscita: i QSettings non vanno distrutti
    // nei distruttori statici, dopo QCoreApplication.
    static auto* instance = new Pool;
    return *instance;
}

inline bool onMainThread()
{
    auto const* app = QCoreApplication::instance();
    return app && QThread::currentThread() == app->thread();
}

inline void discardSlots(Pool& p)
{
    for (auto& entry : p.byFile) {
        for (auto& slot : entry.second) {
            if (slot->inUse) {
                slot->retired = true;
                p.retired.push_back(std::move(slot));
            }
        }
    }
    p.byFile.clear();
}
}  // namespace settings_pool_detail

// Accende la riserva (o la svuota, se era gia' accesa). Da chiamare sul
// thread principale, dopo QCoreApplication e dopo l'ultimo QSettings::setPath.
inline void enableSettingsPool()
{
    if (!settings_pool_detail::onMainThread()) {
        return;
    }
    auto& p = settings_pool_detail::pool();
    settings_pool_detail::discardSlots(p);
    p.enabled = true;
}

// Spegne la riserva: da qui ogni uso costruisce un QSettings nuovo.
inline void disableSettingsPool()
{
    if (!settings_pool_detail::onMainThread()) {
        return;
    }
    auto& p = settings_pool_detail::pool();
    settings_pool_detail::discardSlots(p);
    p.enabled = false;
}

class PooledSettings
{
public:
    // Decodium3.ini: QSettings(IniFormat, UserScope, "Decodium", "Decodium3").
    PooledSettings()
        : PooledSettings(QStringLiteral("profile:Decodium/Decodium3"), [] {
              return new QSettings(QSettings::IniFormat, QSettings::UserScope,
                                   QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
          })
    {
    }

    // Un file INI per percorso: QSettings(iniPath, IniFormat).
    explicit PooledSettings(QString const& iniPath)
        : PooledSettings(QStringLiteral("file:") + iniPath, [&iniPath] {
              return new QSettings(iniPath, QSettings::IniFormat);
          })
    {
    }

    ~PooledSettings()
    {
        if (!m_slot) {
            return;
        }
        if (m_slot->retired) {
            auto& retired = settings_pool_detail::pool().retired;
            for (auto it = retired.begin(); it != retired.end(); ++it) {
                if (it->get() == m_slot) {
                    retired.erase(it);
                    break;
                }
            }
            return;
        }
        while (!m_slot->settings->group().isEmpty()) {
            m_slot->settings->endGroup();
        }
        m_slot->inUse = false;
    }

    PooledSettings(PooledSettings const&) = delete;
    PooledSettings& operator=(PooledSettings const&) = delete;

    QSettings& operator*() { return *m_settings; }
    QSettings* operator->() { return m_settings; }

private:
    template <typename Create>
    PooledSettings(QString const& key, Create create)
    {
        if (!settings_pool_detail::onMainThread() || !settings_pool_detail::pool().enabled) {
            m_owned.reset(create());
            m_settings = m_owned.get();
            return;
        }
        auto& list = settings_pool_detail::pool().byFile[key];
        for (auto& slot : list) {
            if (!slot->inUse) {
                m_slot = slot.get();
                break;
            }
        }
        if (!m_slot) {
            auto slot = std::make_unique<settings_pool_detail::Slot>();
            slot->settings.reset(create());
            slot->sinceSync.start();
            m_slot = slot.get();
            list.push_back(std::move(slot));
        } else if (m_slot->sinceSync.elapsed() >= kPooledSettingsResyncMs) {
            // Modifiche fatte da fuori dal processo; scrive anche quelle in
            // sospeso, come fa ogni QSettings nuovo costruendosi.
            m_slot->settings->sync();
            m_slot->sinceSync.restart();
        }
        m_slot->inUse = true;
        m_settings = m_slot->settings.get();
    }

    QSettings* m_settings {nullptr};
    std::unique_ptr<QSettings> m_owned;
    settings_pool_detail::Slot* m_slot {nullptr};
};

}  // namespace decodium

#endif  // DECODIUMSETTINGSPOOL_H
