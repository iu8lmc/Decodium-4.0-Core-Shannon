// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/sstv/fax/SstvFax480.h"
#include "src/sstv/fax/SstvFax480Receiver.h"
#include "src/sstv/fax/SstvFax480AutoRx.h"
#include "src/sstv/dsp/SstvFrequencyDemodulator.h"
#include "src/sstv/dsp/SstvResampler.h"
#include "src/sstv/dsp/SstvPreprocessor.h"
#include <QtTest>
#include <QFile>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <vector>
using namespace decodium::sstv;
class TestFax480 : public QObject {
    Q_OBJECT
private slots:
    void retainedReceiverVisHeadersAB() {
        for (const char* variant : {"success", "missed"}) {
            QFile file(QStringLiteral(DECODIUM_SSTV_FIXTURES_DIR "/fax480-retained-vis-%1.s16")
                       .arg(QString::fromLatin1(variant)));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto bytes=file.readAll();
            QCOMPARE(bytes.size(),qsizetype(4*12'000*2));
            auto config=SstvFrequencyDemodulatorConfig::sstvDefaults();
            config.averagingSamples=3U;config.hopSamples=1U;
            SstvFrequencyDemodulator demodulator(config);
            SstvFax480AutoRx receiver(SstvFax480Profile::LibsstvVis480,12'000U);
            for (int offset=0;offset<4*12'000;offset+=1200) {
                std::vector<float> samples(1200);
                for (int i=0;i<1200;++i) {
                    const auto value=qFromLittleEndian<qint16>(
                        reinterpret_cast<const uchar*>(bytes.constData()+2*(offset+i)));
                    samples[i]=float(value)/32768.0f;
                }
                const auto observations=demodulator.consume(samples);
                receiver.consume(observations.data(),observations.size());
            }
            QVERIFY2(receiver.acquired(),variant);
        }
    }
    void profileTimingAndBounds() {
        for (auto profile : {SstvFax480Profile::Dayton480,SstvFax480Profile::Qsstv500}) {
            for (std::uint32_t rate : {12'000U,48'000U}) {
                SstvFax480Mapper m(profile,rate);
                const auto first=m.position(0);
                QCOMPARE(first.region,SstvFax480Region::Start);
                QCOMPARE(first.frequencyHz,profile==SstvFax480Profile::Dayton480 ? 2300.0 : 1500.0);
                QCOMPARE(m.height(),profile==SstvFax480Profile::Dayton480 ? 480U : 500U);
                const auto header=5'002'000'000ULL*rate/1'000'000'000ULL;
                if (profile==SstvFax480Profile::Dayton480) {
                    QCOMPARE(m.position(header).region,SstvFax480Region::PhasingSync);
                    QCOMPARE(m.position(header+std::uint64_t(rate)*6/1000).region,SstvFax480Region::PhasingWhite);
                    QCOMPARE(m.position(m.imageStart()).region,SstvFax480Region::Sync);
                } else QCOMPARE(m.position(m.imageStart()).region,SstvFax480Region::Pixel);
                for (std::uint32_t line=0;line<m.height();++line) {
                    const auto a=m.lineStart(line), b=m.lineStart(line+1);
                    QVERIFY(b>a);
                    QCOMPARE(m.position(a).line,line);
                    QCOMPARE(m.position(b-1U).line,line);
                    QCOMPARE(m.position(b-1U).region,profile==SstvFax480Profile::Dayton480
                             ? SstvFax480Region::Pixel : SstvFax480Region::Sync);
                }
                QCOMPARE(m.lineStart(m.height()),m.totalSamples());
                QCOMPARE(m.position(m.totalSamples()).region,SstvFax480Region::Complete);
                QVERIFY_THROWS_EXCEPTION(std::out_of_range,m.lineStart(m.height()+1));
            }
        }
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,SstvFax480Mapper(SstvFax480Profile::Dayton480,7999));
    }
    void vis85TimingAndPayload() {
        const SstvFax480Mapper mapper(SstvFax480Profile::LibsstvVis480,12000U);
        QCOMPARE(mapper.height(),480U);
        QCOMPARE(mapper.imageStart(),std::uint64_t(10'920));
        QCOMPARE(mapper.position(0).frequencyHz,1900.0);
        QCOMPARE(mapper.position(300U*12U).frequencyHz,1200.0);
        QCOMPARE(mapper.position(310U*12U).frequencyHz,1900.0);
        QCOMPARE(mapper.position(610U*12U).frequencyHz,1200.0);
        for (unsigned bit=0;bit<8U;++bit) {
            const auto sample=(640U+bit*30U+15U)*12U;
            const auto expected=bit<7U && ((85U>>bit)&1U) ? 1100.0 : 1300.0;
            QCOMPARE(mapper.position(sample).frequencyHz,expected);
        }
        QCOMPARE(mapper.position(mapper.imageStart()).region,SstvFax480Region::Sync);
        QCOMPARE(mapper.totalSamples(),std::uint64_t(1'550'360));
    }
    void vis85RejectsWrongModeAndParity() {
        constexpr std::uint32_t rate=12'000U;
        const SstvFax480Mapper mapper(SstvFax480Profile::LibsstvVis480,rate);
        for (unsigned scenario=0;scenario<4;++scenario) {
            SstvFax480AutoRx receiver(SstvFax480Profile::LibsstvVis480,rate);
            for (std::uint64_t sample=0;sample<rate*2U;sample+=6U) {
                const auto pos=mapper.position(sample);
                SstvFrequencyObservation obs;
                obs.centreSample=sample; obs.status=SstvFrequencyStatus::Valid; obs.confidence=1.0;
                obs.correctedFrequencyHz=pos.region==SstvFax480Region::Pixel ? 1700.0 : pos.frequencyHz;
                if ((scenario==1 || scenario==2) && sample>=640U*12U && sample<670U*12U) obs.correctedFrequencyHz=1300.0;
                if ((scenario==2 || scenario==3) && sample>=850U*12U && sample<880U*12U) obs.correctedFrequencyHz=1100.0;
                receiver.consume(&obs,1);
            }
            QCOMPARE(receiver.acquired(),scenario==0);
        }
    }
    void vis85ReacquiresAfterCarrierAndSilenceAB() {
        constexpr std::uint32_t rate=12'000U;
        const SstvFax480Mapper mapper(SstvFax480Profile::LibsstvVis480,rate);
        for (const auto leadMs : {300U, 600U}) {
            SstvFax480AutoRx receiver(SstvFax480Profile::LibsstvVis480,rate);
            const std::uint64_t gapStart=std::uint64_t(rate)*leadMs/1000U;
            const std::uint64_t headerStart=gapStart+rate/10U;
            for (std::uint64_t sample=0;sample<headerStart+rate*2U;sample+=6U) {
                SstvFrequencyObservation obs;
                obs.centreSample=sample; obs.status=SstvFrequencyStatus::Valid; obs.confidence=1.0;
                if (sample<gapStart) obs.correctedFrequencyHz=1900.0;
                else if (sample<headerStart) obs.status=SstvFrequencyStatus::LowConfidence;
                else {
                    const auto pos=mapper.position(sample-headerStart);
                    obs.correctedFrequencyHz=pos.region==SstvFax480Region::Pixel ? 1700.0 : pos.frequencyHz;
                }
                receiver.consume(&obs,1);
            }
            QVERIFY(receiver.acquired());
        }
    }
    void vis85AcceptsLongContiguousVoxAB() {
        constexpr std::uint32_t rate=12'000U;
        const SstvFax480Mapper mapper(SstvFax480Profile::LibsstvVis480,rate);
        for (const auto leadMs : {750U, 4000U}) {
            SstvFax480AutoRx receiver(SstvFax480Profile::LibsstvVis480,rate);
            const std::uint64_t headerStart=std::uint64_t(rate)*leadMs/1000U;
            for (std::uint64_t sample=0;sample<headerStart+rate*2U;sample+=6U) {
                SstvFrequencyObservation obs;
                obs.centreSample=sample; obs.status=SstvFrequencyStatus::Valid; obs.confidence=1.0;
                if (sample<headerStart) obs.correctedFrequencyHz=1900.0;
                else {
                    const auto pos=mapper.position(sample-headerStart);
                    obs.correctedFrequencyHz=pos.region==SstvFax480Region::Pixel ? 1700.0 : pos.frequencyHz;
                }
                receiver.consume(&obs,1);
            }
            QVERIFY2(receiver.acquired(), qPrintable(QStringLiteral("VOX %1 ms").arg(leadMs)));
        }
    }
    void vis85LeaderDropoutAndRecoveryAB() {
        constexpr std::uint32_t rate=12'000U;
        const SstvFax480Mapper mapper(SstvFax480Profile::LibsstvVis480,rate);
        for (const auto dropoutMs : {1U, 8U, 10U, 14U, 15U, 20U})
        for (const bool validUnclassified : {false, true})
        for (const std::uint64_t hop : {1U, 6U})
        for (const auto dropoutStartMs : {20U, 85U, 150U, 275U, 330U, 400U, 480U, 590U}) {
            SstvFax480AutoRx receiver(SstvFax480Profile::LibsstvVis480,rate);
            const auto secondHeader=std::uint64_t(rate)*3U;
            bool firstAcquired=false;
            for (std::uint64_t sample=0;sample<secondHeader+rate*2U;sample+=hop) {
                SstvFrequencyObservation obs;
                obs.centreSample=sample; obs.status=SstvFrequencyStatus::Valid; obs.confidence=1.0;
                if (sample>=rate*2U && sample<secondHeader) {
                    obs.status=SstvFrequencyStatus::LowConfidence;
                } else {
                    const auto relative=sample>=secondHeader ? sample-secondHeader : sample;
                    const auto pos=mapper.position(relative);
                    obs.correctedFrequencyHz=pos.region==SstvFax480Region::Pixel ? 1700.0 : pos.frequencyHz;
                    if (sample>=std::uint64_t(rate)*dropoutStartMs/1000U
                        && sample<std::uint64_t(rate)*(dropoutStartMs+dropoutMs)/1000U) {
                        if (validUnclassified) obs.correctedFrequencyHz=1700.0;
                        else obs.status=SstvFrequencyStatus::LowConfidence;
                    }
                }
                receiver.consume(&obs,1);
                if (sample<secondHeader && receiver.acquired()) firstAcquired=true;
            }
            // A 15–20 ms gap near the beginning resets acquisition, but the
            // remaining leader is still long enough to be valid.
            const bool expectedFirst = dropoutMs<15U
                || (dropoutStartMs==20U);
            QVERIFY2(firstAcquired == expectedFirst,
                     qPrintable(QStringLiteral("dropout=%1 ms start=%2 ms unclassified=%3 hop=%4 acquired=%5")
                        .arg(dropoutMs).arg(dropoutStartMs).arg(validUnclassified).arg(hop).arg(firstAcquired)));
            QVERIFY(receiver.acquired());
        }
    }
    void vis85DataDropoutAndInvalidBitsAB() {
        constexpr std::uint32_t rate=12'000U;
        const SstvFax480Mapper mapper(SstvFax480Profile::LibsstvVis480,rate);
        for (const unsigned bit : {0U,1U,2U,3U,4U,5U,6U})
        for (const unsigned gapMs : {8U,10U,16U,30U})
        for (const bool wrongBit : {false,true}) {
            SstvFax480AutoRx receiver(SstvFax480Profile::LibsstvVis480,rate);
            const auto bitStart=(640U+30U*bit)*12U;
            const auto gapStart=bitStart+(gapMs==30U ? 0U : 7U*12U);
            for (std::uint64_t sample=0;sample<rate*2U;sample+=6U) {
                const auto pos=mapper.position(sample);
                SstvFrequencyObservation obs;
                obs.centreSample=sample; obs.status=SstvFrequencyStatus::Valid; obs.confidence=1.0;
                obs.correctedFrequencyHz=pos.region==SstvFax480Region::Pixel ? 1700.0 : pos.frequencyHz;
                if (wrongBit && sample>=bitStart && sample<bitStart+30U*12U)
                    obs.correctedFrequencyHz=((85U >> bit) & 1U) ? 1300.0 : 1100.0;
                if (sample>=gapStart && sample<gapStart+gapMs*12U)
                    obs.status=SstvFrequencyStatus::LowConfidence;
                else if (sample>=gapStart-6U && sample<gapStart)
                    obs.correctedFrequencyHz=1200.0;
                receiver.consume(&obs,1);
            }
            QCOMPARE(receiver.acquired(), !wrongBit && gapMs<=10U);
        }
    }
    void vis85MissingControlToneIsRejectedAB() {
        constexpr std::uint32_t rate=12'000U;
        const SstvFax480Mapper mapper(SstvFax480Profile::LibsstvVis480,rate);
        for (const auto interval : {std::pair<unsigned,unsigned>{300U,310U}, {640U,670U}, {300U,311U}}) {
            SstvFax480AutoRx receiver(SstvFax480Profile::LibsstvVis480,rate);
            for (std::uint64_t sample=0;sample<rate*2U;sample+=6U) {
                const auto pos=mapper.position(sample);
                SstvFrequencyObservation obs;
                obs.centreSample=sample; obs.status=SstvFrequencyStatus::Valid; obs.confidence=1.0;
                obs.correctedFrequencyHz=pos.region==SstvFax480Region::Pixel ? 1700.0 : pos.frequencyHz;
                if (sample>=std::uint64_t(interval.first)*rate/1000U
                    && sample<std::uint64_t(interval.second)*rate/1000U)
                    obs.status=SstvFrequencyStatus::LowConfidence;
                // Two brief 1200 Hz spikes separated by a dropout must not
                // be combined into a continuously observed VIS break.
                if (interval.second==311U && ((sample>=300U*12U && sample<300U*12U+6U)
                    || (sample>=310U*12U && sample<310U*12U+6U))) {
                    obs.status=SstvFrequencyStatus::Valid;
                    obs.correctedFrequencyHz=1200.0;
                }
                receiver.consume(&obs,1);
            }
            QVERIFY(!receiver.acquired());
        }
    }
    void vis85LeaderSpikeAndDropoutAB() {
        constexpr std::uint32_t rate=12'000U;
        const SstvFax480Mapper mapper(SstvFax480Profile::LibsstvVis480,rate);
        for (const auto dropoutStartMs : {150U, 400U}) {
            SstvFax480AutoRx receiver(SstvFax480Profile::LibsstvVis480,rate);
            const auto start=std::uint64_t(rate)*dropoutStartMs/1000U;
            for (std::uint64_t sample=0;sample<rate*2U;sample+=1U) {
                const auto pos=mapper.position(sample);
                SstvFrequencyObservation obs;
                obs.centreSample=sample; obs.status=SstvFrequencyStatus::Valid; obs.confidence=1.0;
                obs.correctedFrequencyHz=pos.region==SstvFax480Region::Pixel ? 1700.0 : pos.frequencyHz;
                if (sample>=start && sample<start+6U) obs.correctedFrequencyHz=1200.0;
                else if (sample>=start+6U && sample<start+126U) obs.status=SstvFrequencyStatus::LowConfidence;
                receiver.consume(&obs,1);
            }
            QVERIFY(receiver.acquired());
        }
    }
    void capturedVis85LeaderDropoutsAB() {
        for (const auto variant : {'a','b'}) {
            QFile file(QStringLiteral(DECODIUM_SSTV_FIXTURES_DIR "/fax480-vis-dropout-%1.s16")
                       .arg(QLatin1Char(variant)));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QByteArray bytes=file.readAll();
            QCOMPARE(bytes.size(),qsizetype(5*48'000*2));
            for (const bool removeBreak : {false,true}) {
                SstvResampler sampler(48'000U);
                SstvPreprocessor preprocessor(SstvPreprocessorConfig::sstvDefaults());
                auto config=SstvFrequencyDemodulatorConfig::sstvDefaults();
                config.averagingSamples=3U;config.hopSamples=1U;
                SstvFrequencyDemodulator demodulator(config);
                SstvFax480AutoRx receiver(SstvFax480Profile::LibsstvVis480,12'000U);
                constexpr int chunk=4800;
                for (int offset=0;offset<5*48'000;offset+=chunk) {
                    std::vector<float> input(chunk);
                    for (int i=0;i<chunk;++i) {
                        const auto sample=offset+i;
                        const auto value=qFromLittleEndian<qint16>(
                            reinterpret_cast<const uchar*>(bytes.constData()+2*sample));
                        const int breakStart=variant=='a' ? 2600 : 2900;
                        input[i]=removeBreak && sample>=breakStart*48 && sample<(breakStart+80)*48
                            ? 0.0f : float(value)/32768.0f;
                    }
                    const auto reduced=sampler.process(input);
                    const auto filtered=preprocessor.process(reduced);
                    const auto observations=demodulator.consume(filtered);
                    receiver.consume(observations.data(),observations.size());
                }
                QCOMPARE(receiver.acquired(),!removeBreak);
            }
        }
    }
    void autoAcquisitionRejectsBrokenSyncAndSteadyTone() {
        constexpr std::uint32_t rate=12'000U;
        for (auto profile : {SstvFax480Profile::Dayton480,SstvFax480Profile::Qsstv500}) {
            SstvFax480Mapper mapper(profile,rate);
            SstvFax480AutoRx broken(profile,rate), steady(profile,rate);
            const auto firstSync=profile==SstvFax480Profile::Dayton480
                ? std::uint64_t(5'002U)*rate/1000U
                : mapper.lineStart(1U)-std::uint64_t(rate)*5U/1000U;
            for (std::uint64_t sample=0;sample<firstSync+rate/100U;sample+=6U) {
                SstvFrequencyObservation obs;
                obs.centreSample=sample; obs.status=SstvFrequencyStatus::Valid; obs.confidence=1.0;
                const auto pos=mapper.position(sample);
                obs.correctedFrequencyHz=pos.region==SstvFax480Region::Pixel ? 1900.0 : pos.frequencyHz;
                if ((pos.region==SstvFax480Region::Sync || pos.region==SstvFax480Region::PhasingSync)
                    && (sample/6U)%3U==0U) obs.status=SstvFrequencyStatus::LowConfidence;
                broken.consume(&obs,1);
                obs.status=SstvFrequencyStatus::Valid; obs.correctedFrequencyHz=1700.0;
                steady.consume(&obs,1);
            }
            QVERIFY(!broken.acquired());
            QVERIFY(!steady.acquired());
        }
    }
    void receiverRejectsInvalidInputAndLeavesMissingUncovered() {
        for (auto profile : {SstvFax480Profile::Dayton480,SstvFax480Profile::Qsstv500}) {
            SstvFax480Receiver receiver(profile,12'000U);
            const auto initial=receiver.coverage();
            QVERIFY(std::none_of(initial.begin(),initial.end(),[](std::uint8_t v){return v!=0;}));
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument,receiver.consume(nullptr,1));
            QVERIFY_THROWS_EXCEPTION(std::length_error,receiver.consume(nullptr,8193));
            SstvFax480Mapper mapper(profile,12'000U);
            SstvFrequencyObservation obs;
            obs.centreSample=mapper.lineStart(1)+100U;
            obs.status=SstvFrequencyStatus::Valid;
            obs.confidence=1.0;
            obs.correctedFrequencyHz=1900.0;
            receiver.consume(&obs,1);
            const auto seen=receiver.coverage();
            QCOMPARE(std::count(seen.begin(),seen.end(),std::uint8_t(1)),1);
            --obs.centreSample;
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument,receiver.consume(&obs,1));
            QVERIFY(receiver.coverage()==seen);
        }
    }
    void encoderABAndPcmFrequencyProof() {
        constexpr std::uint32_t rate=12'000;
        for (auto profile : {SstvFax480Profile::Dayton480,SstvFax480Profile::Qsstv500,SstvFax480Profile::LibsstvVis480}) {
            SstvFax480Mapper mapper(profile,rate);
            for (unsigned variant=0;variant<2;++variant) {
                const auto value=static_cast<std::uint8_t>(variant ? 192 : 64);
                std::vector<std::uint8_t> pixels(std::size_t(mapper.height())*512U,value);
                SstvFax480Encoder a(profile,rate,pixels), b(profile,rate,pixels);
                std::vector<std::int16_t> pcmA(mapper.totalSamples()),pcmB(mapper.totalSamples());
                auto run=[](SstvFax480Encoder& encoder,std::vector<std::int16_t>& pcm,std::size_t block) {
                    std::size_t cursor=0;
                    while (cursor<pcm.size()) {
                        const auto n=encoder.pull(pcm.data()+cursor,std::min(block,pcm.size()-cursor));
                        if (!n) break;
                        cursor+=n;
                    }
                    return cursor;
                };
                QCOMPARE(run(a,pcmA,509U),pcmA.size());
                QCOMPARE(run(b,pcmB,8192U),pcmB.size());
                QVERIFY(a.complete() && b.complete());
                QVERIFY(pcmA==pcmB);
                SstvFrequencyDemodulator demod(SstvFrequencyDemodulatorConfig::sstvDefaults());
                const auto start=mapper.lineStart(10), end=mapper.lineStart(12);
                std::vector<float> samples(end-start);
                for (std::size_t i=0;i<samples.size();++i) samples[i]=float(pcmA[start+i])/32768.0F;
                std::size_t good=0, total=0;
                for (std::size_t i=0;i<samples.size();i+=1024U) {
                    const auto obs=demod.consume(samples.data()+i,std::min(std::size_t(1024),samples.size()-i));
                    for (const auto& o:obs) {
                        const auto pos=mapper.position(start+o.centreSample);
                        if (o.valid() && pos.region==SstvFax480Region::Pixel && pos.pixel>32 && pos.pixel<480) {
                            ++total;
                            if (std::abs(o.correctedFrequencyHz-(1500.0+800.0*value/255.0))<35.0) ++good;
                        }
                    }
                }
                QVERIFY(total>100U);
                QVERIFY(double(good)/total>0.98);
                SstvFax480Receiver rxA(profile,rate), rxB(profile,rate);
                SstvFax480AutoRx autoA(profile,rate), autoB(profile,rate);
                SstvFrequencyDemodulator fullDemod(SstvFrequencyDemodulatorConfig::sstvDefaults());
                std::vector<float> input(4096);
                for (std::size_t offset=0;offset<pcmA.size();offset+=input.size()) {
                    const auto n=std::min(input.size(),pcmA.size()-offset);
                    for (std::size_t i=0;i<n;++i) input[i]=float(pcmA[offset+i])/32768.0F;
                    const auto obs=fullDemod.consume(input.data(),n);
                    rxA.consume(obs.data(),obs.size());
                    autoA.consume(obs.data(),obs.size());
                    for (std::size_t i=0;i<obs.size();i+=17U) {
                        const auto part=std::min(std::size_t(17),obs.size()-i);
                        rxB.consume(obs.data()+i,part);
                        autoB.consume(obs.data()+i,part);
                    }
                }
                QVERIFY2(autoA.acquired(),"FAX480 auto acquisition did not lock");
                QVERIFY(autoB.acquired());
                QCOMPARE(autoA.anchor(),autoB.anchor());
                QVERIFY(autoA.image()==autoB.image());
                const auto autoImage=autoA.image(), autoCoverage=autoA.coverage();
                std::size_t autoSeen=0; double autoError=0;
                for (std::size_t i=0;i<autoImage.size();++i) if (autoCoverage[i]) {
                    ++autoSeen; autoError+=std::abs(int(autoImage[i])-int(value));
                }
                QVERIFY(double(autoSeen)/autoImage.size()>0.90);
                QVERIFY(autoError/autoSeen<12.0);
                const auto result=rxA.image(), covered=rxA.coverage();
                QVERIFY(result==rxB.image());
                QVERIFY(covered==rxB.coverage());
                std::size_t seen=0; double sum=0;
                for (std::size_t i=0;i<result.size();++i) if (covered[i]) {
                    ++seen; sum+=std::abs(int(result[i])-int(value));
                }
                QVERIFY(double(seen)/result.size()>0.90);
                QVERIFY(sum/seen<12.0);
                a.reset();
                std::int16_t out[32] {};
                QCOMPARE(a.pull(out,32U),std::size_t(32));
                QVERIFY(std::equal(std::begin(out),std::end(out),pcmA.begin()));
                a.cancel(); QCOMPARE(a.pull(out,32U),std::size_t(0));
            }
        }
    }
};
QTEST_APPLESS_MAIN(TestFax480)
#include "test_sstv_fax480.moc"
