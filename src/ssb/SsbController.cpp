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
    connect(&devices,&QMediaDevices::audioInputsChanged,this,[this]{
        if(active) fail(tr("Audio devices changed; PTT released"));
        emit devicesChanged();
    });
    guard.setInterval(100);
    connect(&guard,&QTimer::timeout,this,[this]{
        if(!active) return;
        if(!hooks.valid || !hooks.valid()) fail(tr("Radio unavailable or mode changed; PTT released"));
        else if(lastAudio.elapsed()>1500) fail(tr("Microphone stopped; PTT released"));
        else if(clock.elapsed()>180000) fail(tr("Three-minute TX limit reached; release and press PTT again"));
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
void SsbController::fail(QString const& text){stop();message=text;emit changed();}
bool SsbController::start(){
    if(active)return true;
    QAudioDevice device;
    // Explicit selection: never silently substitute the radio RX input.
    for(const auto& d:QMediaDevices::audioInputs())if(d.id().toBase64()==deviceId.toLatin1())device=d;
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
    source=new QAudioSource(device,format,this);
    source->setBufferSize(format.bytesForDuration(40000));
    input=source->start();
    if(!input || source->error()!=QAudio::NoError){fail(tr("Cannot open microphone"));return false;}
    if(!hooks.begin || !hooks.begin()){fail(tr("PTT refused: check CAT, USB/LSB mode and TX audio output"));return false;}
    active=true;message=tr("SSB transmitting");peak=0;phase=0;dsp.reset();pending.clear();packet.clear();
    lead=hooks.leadMs ? std::clamp(hooks.leadMs(),0,1500) : 150;
    clock.start();lastAudio.start();guard.start();
    connect(input,&QIODevice::readyRead,this,&SsbController::capture);
    connect(source,&QAudioSource::stateChanged,this,[this](QAudio::State state){
        if(active && state==QAudio::StoppedState)fail(tr("Microphone error; PTT released"));
    });
    emit changed();return true;
}
void SsbController::stop(){
    bool was=active;active=false;guard.stop();
    if(source){source->disconnect(this);source->stop();source->deleteLater();source=nullptr;}
    input=nullptr;pending.clear();packet.clear();peak=0;
    if(was && hooks.end)hooks.end();
    if(was)message=tr("Receiving");
    emit changed();
}
void SsbController::capture(){
    if(!active || !input)return;
    if(!hooks.valid || !hooks.valid()){fail(tr("Radio unavailable; PTT released"));return;}
    pending+=input->readAll();lastAudio.restart();
    const int frame=format.bytesPerFrame();
    if(pending.size()>format.bytesForDuration(250000)){fail(tr("Microphone backlog; PTT released"));return;}
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
