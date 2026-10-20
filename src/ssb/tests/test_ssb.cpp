#include <QtTest>
#include "SsbController.h"
#include "SsbRxAgc.h"
#include <limits>
using namespace decodium::ssb;
class TestSsb: public QObject {
    Q_OBJECT
private slots:
    void limiterAndFinite(){SpeechDsp dsp;dsp.gainDb=30;for(int i=0;i<48000;++i){const float y=dsp.process(std::sin(i*.4)*100);QVERIFY(std::isfinite(y));QVERIFY(std::abs(y)<=.951);}QVERIFY(std::isfinite(dsp.process(std::numeric_limits<float>::quiet_NaN())));}
    void rejectsDc(){SpeechDsp dsp;dsp.automatic=false;float y=0;for(int i=0;i<48000;++i)y=dsp.process(.5);QVERIFY(std::abs(y)<.00001);}
    void voiceBand(){auto rms=[](double hz){SpeechDsp dsp;dsp.automatic=false;double sum=0;for(int i=0;i<48000;++i){double y=dsp.process(.2*std::sin(2*3.141592653589793*hz*i/48000));if(i>24000)sum+=y*y;}return std::sqrt(sum/24000);};QVERIFY(rms(1000)>rms(8000)*20);QVERIFY(rms(1000)>rms(30)*3);}
    void noImplicitMicrophone(){SsbController c;bool keyed=false;SsbController::Hooks h;h.begin=[&]{keyed=true;return true;};c.setHooks(h);c.setMicrophone("not-an-audio-device");QVERIFY(!c.start());QVERIFY(!keyed);QVERIFY(!c.transmitting());}
    void stopIsSafeBeforeAndAfterFailedCapture(){SsbController c;c.stop();c.setMicrophone("not-an-audio-device");QVERIFY(!c.start());c.stop();QVERIFY(!c.transmitting());}
    void rxAgcLevelsSignals(){
        RxAgc agc;agc.configure(12000);agc.setMode(RxAgc::Medium);agc.setThresholdDb(-90);
        auto settle=[&](double amp){float peak=0;for(int b=0;b<60;++b){float buf[1200];for(int i=0;i<1200;++i)buf[i]=float(amp*std::sin(2*3.14159265*700*i/12000.));agc.process(buf,1200);if(b>=50)for(float v:buf)peak=std::max(peak,std::abs(v));}return peak;};
        const float weak=settle(.002),strong=settle(.5);
        QVERIFY(weak>.15f);                 // il segnale debole viene portato su
        QVERIFY(strong<.6f && strong>.15f); // quello forte non sfonda
        QVERIFY(std::abs(20*std::log10(weak/strong))<12.f); // dinamica compressa
    }
    void rxAgcOffIsManualGain(){
        RxAgc agc;agc.configure(12000);agc.setMode(RxAgc::Off);agc.setManualGainDb(6);
        float buf[4]={.1f,-.1f,.2f,0.f};agc.process(buf,4);
        QVERIFY(std::abs(buf[0]-.1f*1.9953f)<.001f);
    }
    void rxAgcAutoDoesNotPumpNoise(){
        RxAgc agc;agc.configure(12000);agc.setMode(RxAgc::Auto);
        unsigned seed=12345;float maxOut=0;
        for(int b=0;b<200;++b){float buf[1200];for(int i=0;i<1200;++i){seed=seed*1664525u+1013904223u;buf[i]=((seed>>9)/float(1<<23)-.5f)*.01f;}agc.process(buf,1200);if(b>150)for(float v:buf)maxOut=std::max(maxOut,std::abs(v));}
        QVERIFY(std::isfinite(maxOut));
        QVERIFY(maxOut<.9f);                // il rumore non diventa un muro
    }
};
QTEST_GUILESS_MAIN(TestSsb)
#include "test_ssb.moc"
