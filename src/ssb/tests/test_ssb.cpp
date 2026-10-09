#include <QtTest>
#include "SsbController.h"
#include <limits>
using namespace decodium::ssb;
class TestSsb: public QObject {
    Q_OBJECT
private slots:
    void limiterAndFinite(){SpeechDsp dsp;dsp.gainDb=30;for(int i=0;i<48000;++i){const float y=dsp.process(std::sin(i*.4)*100);QVERIFY(std::isfinite(y));QVERIFY(std::abs(y)<=.951);}QVERIFY(std::isfinite(dsp.process(std::numeric_limits<float>::quiet_NaN())));}
    void rejectsDc(){SpeechDsp dsp;dsp.automatic=false;float y=0;for(int i=0;i<48000;++i)y=dsp.process(.5);QVERIFY(std::abs(y)<.00001);}
    void voiceBand(){auto rms=[](double hz){SpeechDsp dsp;dsp.automatic=false;double sum=0;for(int i=0;i<48000;++i){double y=dsp.process(.2*std::sin(2*3.141592653589793*hz*i/48000));if(i>24000)sum+=y*y;}return std::sqrt(sum/24000);};QVERIFY(rms(1000)>rms(8000)*20);QVERIFY(rms(1000)>rms(30)*3);}
    void noImplicitMicrophone(){SsbController c;bool keyed=false;SsbController::Hooks h;h.begin=[&]{keyed=true;return true;};c.setHooks(h);c.setMicrophone("not-an-audio-device");QVERIFY(!c.start());QVERIFY(!keyed);QVERIFY(!c.transmitting());}
};
QTEST_GUILESS_MAIN(TestSsb)
#include "test_ssb.moc"
