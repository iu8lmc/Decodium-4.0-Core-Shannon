static RemoteRadioLink* g_decoPortCatLink = nullptr;

RemoteRadioLink*& decodiumBridgeDecoPortCatLinkRef()
{
    return g_decoPortCatLink;
}

RemoteRadioLink* decodiumBridgeDecoPortCatLink()
{
    return g_decoPortCatLink;
}

static inline bool decoPortIsCat(const QString& backend)
{
    return backend == QLatin1String("decoport");
}

bool decodiumBridgeDecoPortIsCat(const QString& backend) { return decoPortIsCat(backend); }

static inline DecodiumCat4OmManager* activeCat4OmManager(
    QObject const* owner, DecodiumCat4OmManager* explicitManager = nullptr)
{
    if (explicitManager) return explicitManager;
    return owner
        ? owner->findChild<DecodiumCat4OmManager*>(QString(), Qt::FindDirectChildrenOnly)
        : nullptr;
}

static inline bool activeCatCanPtt(QObject const* owner, DecodiumTransceiverManager* h, const QString& b,
                                   DecodiumOmniRigManager* o = nullptr, DecodiumLegacyBackend* legacy = nullptr,
                                   DecodiumCat4OmManager* c = nullptr)
{
    // Il PTT la radio remota lo sa alzare: c'e' un comando apposta nel
    // protocollo, e non serve ne' VOX ne' una linea seriale.
    if (decoPortIsCat(b))
        return g_decoPortCatLink && g_decoPortCatLink->isLinked();
    c = activeCat4OmManager(owner, c);
    auto const isVox = [](QString const& method) {
        return method.trimmed().compare(QStringLiteral("VOX"), Qt::CaseInsensitive) == 0;
    };
    if (b == QStringLiteral("omnirig") && o && isVox(o->pttMethod())) return false;
    if (b != QStringLiteral("cat4om")
        && b != QStringLiteral("omnirig") && h && isVox(h->pttMethod())) return false;
    if (decodiumBridgeLegacyRigControlFallback(legacy, b)) return true;
    if (b=="cat4om") return c && c->canPtt();
    if (b=="omnirig" && o) return o->canPtt();
    return h->canPtt();
}

bool decodiumBridgeSharedCatCanPtt(QObject const* owner, DecodiumTransceiverManager* manager,
                                   const QString& backend, DecodiumOmniRigManager* omni,
                                   DecodiumLegacyBackend* legacy)
{
    return activeCatCanPtt(owner, manager, backend, omni, legacy);
}

static inline bool activeCatUsesVoxPtt(QObject const* owner, DecodiumTransceiverManager* h, const QString& b,
                                       DecodiumOmniRigManager* o = nullptr,
                                       DecodiumCat4OmManager* c = nullptr)
{
    // Mai VOX verso la radio remota: il PTT si dice, non si fa indovinare.
    if (decoPortIsCat(b))
        return false;
    c = activeCat4OmManager(owner, c);
    auto const isVox = [](QString const& method) {
        return method.trimmed().compare(QStringLiteral("VOX"), Qt::CaseInsensitive) == 0;
    };
    if (b == QStringLiteral("cat4om")) return c && isVox(c->pttMethod());
    if (b == QStringLiteral("omnirig")) return o && isVox(o->pttMethod());
    return h && isVox(h->pttMethod());
}

static inline void activeCatSetPtt(QObject const* owner, DecodiumTransceiverManager* h, const QString& b, bool on,
                                   DecodiumOmniRigManager* o = nullptr, DecodiumLegacyBackend* legacy = nullptr,
                                   DecodiumCat4OmManager* c = nullptr)
{
    if (decoPortIsCat(b)) {
        if (g_decoPortCatLink) g_decoPortCatLink->setPtt(on, 0);
        return;
    }
    c = activeCat4OmManager(owner, c);
    if (decodiumBridgeLegacyRigControlFallback(legacy, b)) {
        legacy->setRigPtt(on);
        return;
    }
    if (b=="cat4om") { if (c) c->setRigPtt(on); }
    else if (b=="omnirig" && o) o->setRigPtt(on);
    else h->setRigPtt(on);
}

void decodiumBridgeSharedActiveCatSetPtt(QObject const* owner, DecodiumTransceiverManager* h,
                                         QString const& backend, bool on,
                                         DecodiumOmniRigManager* omni, DecodiumLegacyBackend* legacy)
{
    activeCatSetPtt(owner, h, backend, on, omni, legacy);
}

static inline void activeCatSetTxPtt(QObject const* owner, DecodiumTransceiverManager* h, const QString& b,
                                     bool on, double txDialHz,
                                     DecodiumOmniRigManager* o = nullptr, DecodiumLegacyBackend* legacy = nullptr,
                                     DecodiumCat4OmManager* c = nullptr)
{
    if (decoPortIsCat(b)) {
        activeCatSetPtt(owner, h, b, on, o, legacy, c);
        return;
    }
    c = activeCat4OmManager(owner, c);
    if (b == QStringLiteral("cat4om") && c && !decodiumBridgeLegacyRigControlFallback(legacy, b)) {
        c->setRigTxFrequencyAndPttAsync(txDialHz, on);
        return;
    }
    if (on && txDialHz > 0.0 && decodiumBridgeIsHamlibFamilyBackend(b) && h
        && !decodiumBridgeLegacyRigControlFallback(legacy, b)) {
        qDebug().noquote()
            << "[CATDBG] Hamlib Fake-It/Split PTT"
            << "backend=" << b.trimmed().toLower()
            << "on=" << on
            << "txDialHz=" << QString::number(txDialHz, 'f', 0);
        h->setRigTxFrequencyAndPtt(txDialHz, true);
        return;
    }

    activeCatSetPtt(owner, h, b, on, o, legacy, c);
}

static inline bool activeCatSetTxPttAsync(QObject const* owner, DecodiumTransceiverManager* h, const QString& b,
                                          bool on, double txDialHz,
                                          DecodiumOmniRigManager* o = nullptr, DecodiumLegacyBackend* legacy = nullptr,
                                          DecodiumCat4OmManager* c = nullptr)
{
    if (decoPortIsCat(b)) {
        activeCatSetPtt(owner, h, b, on, o, legacy, c);
        return false;   // niente asincrono: il comando e' gia' partito
    }
    c = activeCat4OmManager(owner, c);
    if (b == QStringLiteral("cat4om") && c && !decodiumBridgeLegacyRigControlFallback(legacy, b)) {
        c->setRigTxFrequencyAndPttAsync(txDialHz, on);
        return true;
    }
    if (on && txDialHz > 0.0 && decodiumBridgeIsHamlibFamilyBackend(b) && h
        && !decodiumBridgeLegacyRigControlFallback(legacy, b)) {
        qDebug().noquote()
            << "[CATDBG] Hamlib async Fake-It/Split PTT"
            << "backend=" << b.trimmed().toLower()
            << "on=" << on
            << "txDialHz=" << QString::number(txDialHz, 'f', 0);
        h->setRigTxFrequencyAndPttAsync(txDialHz, true);
        return true;
    }

    Q_UNUSED(o)
    Q_UNUSED(legacy)
    Q_UNUSED(c)
    return false;
}

bool decodiumBridgeSharedCatUsesVox(QObject const* owner, DecodiumTransceiverManager* manager,
                                    QString const& backend, DecodiumOmniRigManager* omni)
{
    return activeCatUsesVoxPtt(owner, manager, backend, omni);
}
void decodiumBridgeSharedCatSetTxPtt(QObject const* owner, DecodiumTransceiverManager* manager,
                                    QString const& backend, bool on, double hz,
                                    DecodiumOmniRigManager* omni, DecodiumLegacyBackend* legacy)
{
    activeCatSetTxPtt(owner, manager, backend, on, hz, omni, legacy);
}
bool decodiumBridgeSharedCatSetTxPttAsync(QObject const* owner, DecodiumTransceiverManager* manager,
                                         QString const& backend, bool on, double hz,
                                         DecodiumOmniRigManager* omni, DecodiumLegacyBackend* legacy)
{
    return activeCatSetTxPttAsync(owner, manager, backend, on, hz, omni, legacy);
}

static inline void activeCatSetFreq(QObject const* owner, DecodiumTransceiverManager* h, const QString& b, double hz,
                                    DecodiumOmniRigManager* o = nullptr, DecodiumLegacyBackend* legacy = nullptr,
                                    DecodiumCat4OmManager* c = nullptr)
{
    if (decoPortIsCat(b)) {
        if (g_decoPortCatLink && hz > 0.0) g_decoPortCatLink->tune(hz);
        return;
    }
    c = activeCat4OmManager(owner, c);
    if (decodiumBridgeLegacyRigControlFallback(legacy, b)) {
        legacy->setDialFrequency(hz);
        return;
    }
    if (b=="cat4om") { if (c) c->setRigFrequency(hz); }
    else if (b=="omnirig" && o) o->setRigFrequency(hz);
    else h->setRigFrequency(hz);
}

void decodiumBridgeCwSetCatFrequency(QObject const* owner, DecodiumTransceiverManager* hamlib,
                                     const QString& backend, double hz,
                                     DecodiumOmniRigManager* omni, DecodiumLegacyBackend* legacy)
{
    activeCatSetFreq(owner, hamlib, backend, hz, omni, legacy);
}

static inline void activeCatSetTxFreq(QObject const* owner, DecodiumTransceiverManager* h, const QString& b, double hz,
                                      DecodiumOmniRigManager* o = nullptr,
                                      DecodiumCat4OmManager* c = nullptr)
{
    // Niente split verso la radio remota: DecoPort ha una frequenza sola, e
    // fingere di averne due vorrebbe dire spostare quella di ricezione.
    if (decoPortIsCat(b))
        return;
    c = activeCat4OmManager(owner, c);
    if (b=="cat4om") { if (c) c->setRigTxFrequency(hz); }
    else if (b=="omnirig" && o) o->setRigTxFrequency(hz);
    else h->setRigTxFrequency(hz);
}

void decodiumBridgeSetCatTxFrequency(const QObject* owner, DecodiumTransceiverManager* hamlib,
                                      const QString& backend, double hz, DecodiumOmniRigManager* omni)
{
    activeCatSetTxFreq(owner, hamlib, backend, hz, omni);
}


