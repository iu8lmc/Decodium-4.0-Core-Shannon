QObject* DecodiumBridge::decoPortGatewayObject() const
{
    if (!m_decoPortGateway) {
        auto* self = const_cast<DecodiumBridge*>(this);
        m_decoPortGateway = new DecodiumDecoPortGateway(self);
        DecodiumDecoPortGateway::RigHooks hooks;
        // I ganci guardano prima al driver: se il gateway e' riuscito ad aprire
        // la radio per conto suo, quella e' la verita' — la frequenza viene dal
        // bus, non da quello che l'applicazione crede. Solo se non ce l'ha
        // fatta si torna a chiedere all'applicazione.
        auto rigOpen = [self]() -> DecoPortRigDriver* {
            return (self->m_decoPortRig && self->m_decoPortRig->isOpen())
                       ? self->m_decoPortRig : nullptr;
        };
        hooks.connected      = [self, rigOpen]() {
            return rigOpen() != nullptr || self->catConnected();
        };
        hooks.frequencyHz    = [self, rigOpen]() {
            if (auto* r = rigOpen()) return r->frequencyHz();
            return self->frequency();
        };
        hooks.modeName       = [self, rigOpen]() {
            if (auto* r = rigOpen()) return r->modeName();
            return self->catMode();
        };
        hooks.pttActive      = [self, rigOpen]() {
            if (auto* r = rigOpen()) return r->ptt();
            return self->transmitting();
        };
        hooks.setFrequencyHz = [self, rigOpen](double hz) {
            if (auto* r = rigOpen()) { r->setFrequencyHz(hz); return; }
            // If the direct DecoPort Hamlib driver could not claim the port
            // because this application's CAT already owns it, updating only
            // the bridge property is not enough: the real radio would stay at
            // the old frequency.  Reuse the normal CAT QSY path so Hamlib,
            // TCI, OmniRig and legacy fallbacks all receive the command.
            self->requestRigFrequencyFromBridge(hz,
                                                QStringLiteral("DecoPort remote client"));
        };
        hooks.setModeName    = [self, rigOpen](const QString& m) {
            if (auto* r = rigOpen()) { r->setModeName(m); return; }
            self->setMode(m);
        };
        // Il PTT ora c'e', ma solo se questa stazione puo' davvero trasmettere:
        // serve il CAT per alzarlo e una radio che non stia gia' trasmettendo
        // per conto suo. Quando manca, il gateway dichiara StateCanTransmit=0 e
        // i client lo vedono invece di provarci e basta.
        hooks.canTransmit    = [self, rigOpen]() {
            if (self->m_transmitting || self->m_tuning)
                return false;
            return rigOpen() != nullptr || self->catConnected();
        };
        hooks.setPtt         = [self](bool on) { self->decoPortKeyLocalRig(on); };

        // Gli strumenti, con le stesse regole di validita' della CAT condivisa:
        // sono la stessa verita' detta da un'altra porta, e due porte che
        // rispondono diverso sulla stessa radio sono un difetto, non una scelta.
        //
        // I misuratori di trasmissione valgono solo mentre si trasmette: a
        // riposo uno zero direbbe «nessuna potenza, ROS perfetto», che somiglia
        // a una stazione che va benissimo. L'S-meter e' l'opposto — vale solo in
        // ricezione — e la sua validita' se la calcola gia' il gestore.
        auto const inTx = [self]() {
            return self->m_hamlibCat && self->m_hamlibCat->pttActive();
        };
        hooks.sMeterDbm     = [self](double& v) {
            auto const* rig = self->m_hamlibCat;
            if (!rig || !rig->strengthValid()) return false;
            v = rig->strengthDb();
            return true;
        };
        hooks.forwardPowerW = [self, inTx](double& v) {
            auto const* rig = self->m_hamlibCat;
            if (!rig || !inTx()) return false;
            double const w = rig->powerWatts();
            if (w <= 0.0) return false;
            v = w;
            return true;
        };
        hooks.swr           = [self, inTx](double& v) {
            auto const* rig = self->m_hamlibCat;
            if (!rig || !inTx()) return false;
            double const s = rig->swr();
            if (s < 1.0) return false;   // sotto 1 non e' un ROS, e' un errore
            v = s;
            return true;
        };
        hooks.alcPct        = [self, inTx](double& v) {
            auto const* rig = self->m_hamlibCat;
            if (!rig || !inTx() || !rig->alcValid()) return false;
            v = rig->alc();              // il gestore lo tiene gia' in percento
            return true;
        };
        // Gli strumenti del finale. Non passano dal filtro "solo in
        // trasmissione": la temperatura scende DOPO, ed e' proprio dopo che si
        // guarda, mentre tensione e corrente a riposo raccontano lo stato
        // dell'alimentazione. Ognuno ha gia' il suo "valido" dal gestore, che
        // e' l'unica cosa che distingue una radio senza sensore da una che in
        // questo istante non ha risposto.
        hooks.drainVoltage    = [self](double& v) {
            auto const* rig = self->m_hamlibCat;
            if (!rig || !rig->drainVoltageValid()) return false;
            v = rig->drainVoltage();
            return true;
        };
        hooks.drainCurrent    = [self](double& v) {
            auto const* rig = self->m_hamlibCat;
            if (!rig || !rig->drainCurrentValid()) return false;
            v = rig->drainCurrent();
            return true;
        };
        hooks.paTemperature   = [self](double& v) {
            auto const* rig = self->m_hamlibCat;
            if (!rig || !rig->paTemperatureValid()) return false;
            v = rig->paTemperature();
            return true;
        };
        hooks.compressionDb   = [self](double& v) {
            auto const* rig = self->m_hamlibCat;
            if (!rig || !rig->compressionValid()) return false;
            v = rig->compressionDb();
            return true;
        };
        // La manopola della potenza: l'unica delle dieci che si legge a
        // trasmettitore fermo, ed e' li' che si guarda — prima di premere.
        hooks.powerSettingPct = [self](double& v) {
            auto const* rig = self->m_hamlibCat;
            if (!rig || !rig->powerSettingValid()) return false;
            v = rig->powerSettingPct();
            return true;
        };

        m_decoPortGateway->setRigHooks(std::move(hooks));
        connect(m_decoPortGateway, &DecodiumDecoPortGateway::txAudioDue,
                self, [self](const QVector<short>& samples) {
                    self->decoPortPlayTxAudio(samples);
                });
        m_decoPortGateway->setAuthKey(decoPortAuthKey());
    }
    return m_decoPortGateway;
}

