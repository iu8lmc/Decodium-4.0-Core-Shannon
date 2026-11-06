#include "SsbController.h"
#include <QSettings>
#include <cstring>
namespace decodium::ssb {
SsbController::SsbController(QObject* p):QObject(p) {
    QSettings s;
    deviceId=s.value("SSB/microphone").toString();
    dsp.gainDb=std::clamp(s.value("SSB/gainDb",0).toDouble(),-20.,30.);
    dsp.automatic=s.value("SSB/automatic",true).toBool();
    dsp.lowHz=std::clamp(s.value("SSB/lowCut",200).toInt(),80,500);
    dsp.highHz=std::clamp(s.value("SSB/highCut",2800).toInt(),1800,3500);
    connect(&devices,&QMediaDevices::audioInputsChanged,
            this,&SsbController::handleAudioInputsChanged);
    guard.setInterval(100);
    connect(&guard,&QTimer::timeout,this,[this]{
        if(!active) return;
        if(!hooks.valid || !hooks.valid()) fail(tr("Radio unavailable or mode changed; PTT released"));
        else if(clock.elapsed()>180000) fail(tr("Three-minute TX limit reached; release and press PTT again"));
        else if(!microphoneRecoveryPending && lastAudio.elapsed()>1500)
            recoverMicrophone(tr("Microphone stream stopped delivering audio"));
    });
}
SsbController::~SsbController(){ stop(); }
QVariantList SsbController::microphones() const {
    QVariantList out;
    for(const auto& d:QMediaDevices::audioInputs())
        out.append(QVariantMap{{"name",d.description()},{"id",QString::fromLatin1(d.id().toBase64())}});
    return out;
}
void SsbController::save(){QSettings s;s.setValue("SSB/microphone",deviceId);s.setValue("SSB/gainDb",dsp.gainDb);s.setValue("SSB/automatic",dsp.automatic);s.setValue("SSB/lowCut",dsp.lowHz);s.setValue("SSB/highCut",dsp.highHz);emit changed();}
void SsbController::setMicrophone(QString const& id){if(active)return;deviceId=id;save();}
void SsbController::setGainDb(double v){if(!std::isfinite(v))return;dsp.gainDb=std::clamp(v,-20.,30.);save();}
void SsbController::setAutomatic(bool v){dsp.automatic=v;save();}
void SsbController::setLowCut(int v){dsp.lowHz=std::clamp(v,80,500);save();}
void SsbController::setHighCut(int v){dsp.highHz=std::clamp(v,1800,3500);save();}
void SsbController::fail(QString const& text){
    qWarning().noquote() << "[SSB] failure:" << text;
    stop();message=text;emit changed();
}
QAudioDevice SsbController::selectedMicrophoneDevice() const
{
    for(const auto& device:QMediaDevices::audioInputs())
        if(device.id().toBase64()==deviceId.toLatin1())return device;
    return {};
}

void SsbController::handleAudioInputsChanged()
{
    emit devicesChanged();
    if(!active)return;

    // PipeWire/PulseAudio and WASAPI may rebuild their graph while the input
    // stream is being opened.  The device-list signal is therefore not, by
    // itself, evidence that the selected microphone was unplugged.  Keep a
    // healthy stream alive; if the selected endpoint really disappeared or
    // the stream stopped, use the same single bounded recovery as any other
    // transient capture failure.
    const QAudioDevice selected=selectedMicrophoneDevice();
    if(!selected.isNull() && source && source->state()!=QAudio::StoppedState)return;
    recoverMicrophone(selected.isNull()
        ? tr("Selected microphone temporarily unavailable")
        : tr("Microphone stream stopped during an audio-device refresh"));
}

bool SsbController::openMicrophone()
{
    const QAudioDevice device=selectedMicrophoneDevice();
    if(device.isNull())return false;

    source=new QAudioSource(device,format,this);
    source->setBufferSize(format.bytesForDuration(40000));
    input=source->start();
    if(!input || source->error()!=QAudio::NoError){
        qWarning().noquote() << "[SSB] microphone open failed"
                             << "device=" << device.description()
                             << "state=" << int(source->state())
                             << "error=" << int(source->error());
        source->stop();source->deleteLater();source=nullptr;input=nullptr;
        return false;
    }
    qInfo().noquote() << "[SSB] microphone opened"
                      << "device=" << device.description()
                      << "rate=" << format.sampleRate()
                      << "channels=" << format.channelCount()
                      << "format=" << int(format.sampleFormat());
    connect(input,&QIODevice::readyRead,this,&SsbController::capture);
    connect(source,&QAudioSource::stateChanged,this,[this](QAudio::State state){
        if(active && state==QAudio::StoppedState)
            recoverMicrophone(tr("Microphone capture stream stopped"));
    });
    return true;
}

void SsbController::recoverMicrophone(const QString& reason)
{
    if(!active || microphoneRecoveryPending)return;
    if(microphoneRestartAttempts++ >= 1){
        fail(tr("Microphone error; PTT released"));
        return;
    }
    microphoneRecoveryPending=true;
    const quint64 generation=++microphoneGeneration;
    if(source){source->disconnect(this);source->stop();source->deleteLater();source=nullptr;}
    input=nullptr;
    // PipeWire/WASAPI devices may briefly report Stopped while the first
    // capture stream is negotiating with the USB radio output. One bounded
    // reopen keeps a held SSB PTT alive without masking a real microphone
    // fault indefinitely.
    qWarning().noquote() << "[SSB] transient microphone interruption; bounded reopen"
                         << "reason=" << reason;
    message=tr("Reopening microphone…");emit changed();
    QTimer::singleShot(250,this,[this,generation]{
        if(!active || generation!=microphoneGeneration)return;
        if(!openMicrophone()){
            microphoneRecoveryPending=false;
            fail(tr("Cannot reopen microphone; PTT released"));return;
        }
        microphoneRecoveryPending=false;
        lastAudio.restart();message=tr("SSB transmitting");emit changed();
    });
}

bool SsbController::start(){
    if(active)return true;
    QAudioDevice device;
    // Explicit selection: never silently substitute the radio RX input.
    device=selectedMicrophoneDevice();
    if(device.isNull()){fail(tr("Select a PC microphone"));return false;}
    format.setSampleRate(48000);
    bool supported=false;
    for(auto sf:{QAudioFormat::Float,QAudioFormat::Int16}) {
        for(int channels:{1,2}) {
            format.setSampleFormat(sf);format.setChannelCount(channels);
            if(device.isFormatSupported(format)){supported=true;break;}
        }
        if(supported)break;
    }
    if(!supported){fail(tr("Microphone must support 48 kHz PCM"));return false;}
    microphoneRestartAttempts=0;microphoneRecoveryPending=false;++microphoneGeneration;
    if(!openMicrophone()){fail(tr("Cannot open microphone"));return false;}
    if(!hooks.begin || !hooks.begin()){fail(tr("PTT refused: check CAT, USB/LSB mode and TX audio output"));return false;}
    active=true;message=tr("SSB transmitting");peak=0;phase=0;dsp.reset();pending.clear();packet.clear();
    lead=hooks.leadMs ? std::clamp(hooks.leadMs(),0,1500) : 150;
    clock.start();lastAudio.start();guard.start();
    qInfo().noquote() << "[SSB] PTT session started lead_ms=" << lead;
    const quint64 generation=microphoneGeneration;
    // A backend can emit Stopped synchronously inside QAudioSource::start(),
    // before active becomes true. Verify once after activation so that early
    // notification cannot leave a keyed but dead stream waiting for timeout.
    QTimer::singleShot(250,this,[this,generation]{
        if(active && generation==microphoneGeneration
            && (!source || source->state()==QAudio::StoppedState))
            recoverMicrophone(tr("Microphone did not remain active after startup"));
    });
    emit changed();return true;
}
void SsbController::stop(){
    if(active)qInfo().noquote() << "[SSB] PTT session stopped"
        << "elapsed_ms=" << clock.elapsed()
        << "last_audio_age_ms=" << lastAudio.elapsed()
        << "input_state=" << (source ? int(source->state()) : -1)
        << "input_error=" << (source ? int(source->error()) : -1);
    bool was=active;active=false;microphoneRecoveryPending=false;++microphoneGeneration;guard.stop();
    if(source){source->disconnect(this);source->stop();source->deleteLater();source=nullptr;}
    input=nullptr;pending.clear();packet.clear();peak=0;
    if(was && hooks.end)hooks.end();
    if(was)message=tr("Receiving");
    emit changed();
}
void SsbController::capture(){
    if(!active || !input)return;
    if(!hooks.valid || !hooks.valid()){fail(tr("Radio unavailable; PTT released"));return;}
    const QByteArray captured=input->readAll();
    // A readyRead notification with no PCM is not a healthy microphone
    // heartbeat: keep the existing no-audio watchdog effective.
    if(captured.isEmpty())return;
    pending+=captured;lastAudio.restart();
    const int frame=format.bytesPerFrame();
    if(pending.size()>format.bytesForDuration(250000)){
        // Capture notifications can be batched while CAT/audio startup or
        // the GUI event loop is busy. A backlog of valid PCM is not a device
        // failure. Discard old *whole frames*, keeping the newest 40 ms plus
        // any partial frame for the next read. Never replay the stale burst
        // or mix a pre-gap TX packet/filter history into the fresh audio.
        const qsizetype discard=(pending.size()/frame)*frame-format.bytesForDuration(40000);
        pending.remove(0,discard);
        packet.clear();phase=0;dsp.reset();peak=0;
        qWarning().noquote() << "[SSB] microphone backlog resynchronised; PTT retained"
                             << "discarded_ms=" << format.durationForBytes(discard)/1000
                             << "retained_bytes=" << pending.size();
    }
    const int count=pending.size()/frame;double blockPeak=0;
    for(int i=0;i<count;++i){
        float sample=0;
        for(int c=0;c<format.channelCount();++c){
            const char* p=pending.constData()+i*frame+c*format.bytesPerSample();
            if(format.sampleFormat()==QAudioFormat::Float){float f;std::memcpy(&f,p,4);sample+=std::isfinite(f)?f:0;}
            else{qint16 n;std::memcpy(&n,p,2);sample+=n/32768.f;}
        }
        sample=dsp.process(sample/format.channelCount());blockPeak=std::max(blockPeak,double(std::abs(sample)));
        if(++phase==4){phase=0;packet.append(short(std::lround(sample*32767)));}
        if(packet.size()==480){
            // Do not transmit audio captured before the PTT settling period.
            if(clock.elapsed()>=lead && hooks.send)hooks.send(packet);
            // The transport may synchronously abort and clear pending audio.
            if(!active)return;
            packet.clear();
        }
    }
    pending.remove(0,count*frame);peak=std::max(blockPeak,peak*.8);emit changed();
}
}
