// Included by DecodiumBridge.cpp: shares the existing CAT/audio routing.
bool DecodiumBridge::ssbValid() const
{
    if(m_mode != QStringLiteral("SSB") || !m_catConnected || rtlSdrEnabled())return false;
    const QString mode=m_catMode.toUpper();
    if(mode!=m_ssbRadioMode && !(m_ssbRadioMode=="DIGU" && (mode=="PKTUSB" || mode=="DATA-U"))
       && !(m_ssbRadioMode=="DIGL" && (mode=="PKTLSB" || mode=="DATA-L")))return false;
    if(m_ssbTxActive && m_ssbRemoteTx!=m_decoPortUseRemote)return false;
    if(m_decoPortUseRemote){
        auto* link=remoteLinkIfAny();
        return link && link->isLinked() && link->canTransmit() && link->txBlockedReason().isEmpty();
    }
    if(m_ssbTxActive && !m_decoPortRemoteKeyed)return false;
    return true;
}
bool DecodiumBridge::ssbBegin()
{
    if(m_ssbTxActive)return true;
    if(!ssbValid() || m_ssbControls.value("busy").toBool() || m_transmitting || m_tuning || sstvTxActive() || m_rttyTxActive
       || m_cwRemoteKeying || m_decoPortRemoteKeyed || activeCatReportsPttActive())return false;
    if(!m_decoPortUseRemote) {
        bool found=false;auto device=resolveTxOutputDevice(&found);
        if(!found || device.isNull() || !activeCatCanPtt(this,m_hamlibCat,m_catBackend,m_omniRigCat,m_legacyBackend,m_cat4OmCat))return false;
    }
    m_ssbRemoteTx=m_decoPortUseRemote;
    if(m_ssbRemoteTx)remoteLinkIfAny()->setPtt(true,0);
    else {
        keySharedAudioTransmitter(true,true);
        if(!m_decoPortRemoteKeyed)return false;
    }
    m_ssbRestoreMonitor=m_decoPortMonitor;
    if(m_ssbRestoreMonitor)setDecoPortMonitor(false);
    m_ssbTxActive=true;emit ssbStateChanged();return true;
}
void DecodiumBridge::ssbStop()
{
    if(!m_ssbTxActive)return;
    m_ssbTxActive=false;
    if(m_ssbRemoteTx){if(auto* link=remoteLinkIfAny())link->setPtt(false,0);}
    else keySharedAudioTransmitter(false,false);
    if(m_decoPortTxOut){QPointer<RtlSdrAudioOutput> out(m_decoPortTxOut);
        QMetaObject::invokeMethod(out,[out]{if(out)out->stop(QStringLiteral("ssb-ended"));},Qt::QueuedConnection);}
    m_decoPortTxOutRate=0;
    if(m_ssbRestoreMonitor && m_mode=="SSB" && m_catConnected)setDecoPortMonitor(true);
    m_ssbRestoreMonitor=false;emit ssbStateChanged();
}
void DecodiumBridge::ssbSend(const QVector<short>& samples)
{
    if(!m_ssbTxActive)return;
    if(!ssbValid()){ssbStop();return;}
    if(m_ssbRemoteTx){if(auto* link=remoteLinkIfAny())link->sendTxAudio(samples,0);}
    else decoPortPlayTxAudio(samples,false);
}
void DecodiumBridge::ssbSetRadioMode(const QString& mode)
{
    if(m_mode!="SSB" || m_ssbTxActive || m_transmitting || m_tuning)return;
    if(mode!="USB" && mode!="LSB" && mode!="DIGU" && mode!="DIGL")return;
    m_ssbRadioMode=mode;
    applyConfiguredCatRigMode(QStringLiteral("ssb-mode"));emit ssbStateChanged();
}
void DecodiumBridge::ssbRefreshControls()
{
    if(!m_ssbVoiceConnected && m_hamlibCat){
        m_ssbVoiceConnected=true;
        connect(m_hamlibCat,&DecodiumTransceiverManager::voiceControlsReady,this,[this](const QVariantMap& v){
            if(!m_decoPortUseRemote){m_ssbControls=v;emit ssbStateChanged();}
        });
        connect(this,&DecodiumBridge::catConnectedChanged,this,[this]{m_ssbControls.clear();if(!m_catConnected)ssbStop();emit ssbStateChanged();});
    }
    if(m_ssbTxActive || m_ssbControls.value("busy").toBool())return;
    m_ssbControls.clear();emit ssbStateChanged();
    if(m_decoPortUseRemote && m_remoteIsDecolink) {
        auto* link=qobject_cast<DecolinkLink*>(remoteLinkIfAny());
        if(link) {
            disconnect(link,&DecolinkLink::voiceControlsReady,this,nullptr);
            connect(link,&DecolinkLink::voiceControlsReady,this,[this,link](const QVariantMap& v){
                if(m_mode=="SSB" && m_decoPortUseRemote && remoteLinkIfAny()==link){m_ssbControls=v;emit ssbStateChanged();}
            });
            link->requestVoiceControl();return;
        }
    }
    if(m_decoPortUseRemote || m_catBackend!="hamlib" || !m_catConnected){
        m_ssbControls["error"]=tr("Advanced CAT controls unavailable on this connection");emit ssbStateChanged();return;
    }
    m_ssbControls["busy"]=true;emit ssbStateChanged();
    m_hamlibCat->requestVoiceControl();
}
void DecodiumBridge::ssbSetControl(const QString& key,double value)
{
    if(m_mode!="SSB" || m_ssbTxActive || m_transmitting || m_tuning || !m_catConnected || activeCatReportsPttActive())return;
    if(m_decoPortUseRemote) {
        if(m_remoteIsDecolink)if(auto* link=qobject_cast<DecolinkLink*>(remoteLinkIfAny()))link->requestVoiceControl(key,value,true);
        return;
    }
    if(m_catBackend!="hamlib")return;
    if(m_ssbControls.value("busy").toBool() || !m_ssbControls.value(key+"Supported").toBool())return;
    m_ssbControls["busy"]=true;emit ssbStateChanged();
    m_hamlibCat->requestVoiceControl(key,value,true);
}

void DecodiumBridge::ssbMonitorAudio(const QVector<short>& samples)
{
    if (!m_ssbTxActive && m_decoPortMonitor && m_decoPortMonitorOut && m_decoPortMonitorRate > 0) {
        // Se la scheda non accetta i 12 kHz si va a 48 ripetendo ogni campione
        // quattro volte: per un ascolto di controllo e' un compromesso onesto,
        // e non tocca affatto quello che entra nel decoder.
        QVector<short> out_samples;
        if (m_decoPortMonitorRate == SAMPLE_RATE) {
            out_samples = samples;
        } else {
            int const factor = m_decoPortMonitorRate / SAMPLE_RATE;
            out_samples.reserve(samples.size() * factor);
            for (short s : samples)
                for (int i = 0; i < factor; ++i)
                    out_samples.append(s);
        }
        // Volume affects the local speaker copy only; decoder PCM stays untouched.
        for (short& sample : out_samples)
            sample = static_cast<short>(std::lround(sample * m_remoteRxGain));
        QPointer<RtlSdrAudioOutput> out(m_decoPortMonitorOut);
        int const rate = m_decoPortMonitorRate;
        QMetaObject::invokeMethod(m_decoPortMonitorOut, [out, out_samples, rate]() {
            if (out) out->enqueueSamples(out_samples, rate);
        }, Qt::QueuedConnection);
    }
}
