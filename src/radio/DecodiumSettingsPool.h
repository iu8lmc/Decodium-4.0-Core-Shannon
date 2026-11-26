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
};

inline std::map<QString, std::vector<std::unique_ptr<Slot>>>& slotTable()
{
    // Lasciata allocata fino all'uscita: i QSettings non vanno distrutti
    // nei distruttori statici, dopo QCoreApplication.
    static auto* pool = new std::map<QString, std::vector<std::unique_ptr<Slot>>>;
    return *pool;
}

inline bool onMainThread()
{
    auto const* app = QCoreApplication::instance();
    return app && QThread::currentThread() == app->thread();
}
}  // namespace settings_pool_detail

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
        if (m_slot) {
            while (!m_slot->settings->group().isEmpty()) {
                m_slot->settings->endGroup();
            }
            m_slot->inUse = false;
        }
    }

    PooledSettings(PooledSettings const&) = delete;
    PooledSettings& operator=(PooledSettings const&) = delete;

    QSettings& operator*() { return *m_settings; }
    QSettings* operator->() { return m_settings; }

private:
    template <typename Create>
    PooledSettings(QString const& key, Create create)
    {
        if (!settings_pool_detail::onMainThread()) {
            m_owned.reset(create());
            m_settings = m_owned.get();
            return;
        }
        auto& list = settings_pool_detail::slotTable()[key];
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
