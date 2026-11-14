// Standalone native-worker measurement; input is signed 16-bit little-endian 12 kHz mono PCM.
#include "Detector/FST4DecodeWorker.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "Detector/FtxFst4Ldpc.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>
#include <cstdio>
#include <thread>
#include <chrono>
#include "Detector/FortranRuntimeGuard.hpp"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    auto args = app.arguments();
    if (args.size() == 2 && (args[1] == "--roundtrip-fst4w" || args[1] == "--ldpc-fst4w")) {
        auto encoded = decodium::txmsg::encodeFst4WithHint(QStringLiteral("K1ABC FN42 33"), true);
        if (args[1] == "--ldpc-fst4w") {
            std::array<float,240> llr {};
            std::array<signed char,240> mask {}, cw {};
            std::array<signed char,74> message {};
            int starts[] = {8,46,84,122};
            int inverseGray[] = {0,1,3,2};
            int index=0;
            for (int start : starts) for (int j=0; j<30; ++j) {
                int value=inverseGray[encoded.tones[start+j]];
                llr[index++]=(value & 2) ? 10.0f : -10.0f;
                llr[index++]=(value & 1) ? 10.0f : -10.0f;
            }
            int type=0, errors=-1;
            float distance=0;
            bool ok=decodium::fst4::decode240_74_native(llr,66,2,3,mask,message,cw,type,errors,distance);
            int differing=0;
            for (int i=0;i<50;++i) differing += message[i]!=encoded.msgbits[i];
            auto result=QJsonDocument(QJsonObject{{"ok",ok},{"hardErrors",errors},{"payloadBitsDifferent",differing}}).toJson();
            fwrite(result.constData(),1,result.size(),stdout);
            return ok && errors==0 && differing==0 ? 0 : 5;
        }
        QByteArray bits(77, '\0');
        for (int i=0; i<50 && i<encoded.msgbits.size(); ++i) bits[i]=encoded.msgbits[i];
        QByteArray suffix("000000000000000000000110000");
        for (int i=0; i<27; ++i) bits[50+i]=suffix[i]=='1' ? 1 : 0;
        decodium::txmsg::Decode77Context context;
        auto decoded = decodium::txmsg::decode77(bits, &context, true);
        auto data = QJsonDocument(QJsonObject{{"encoded",QString::fromLatin1(encoded.msgsent).trimmed()},
                                            {"decoded",QString::fromLatin1(decoded.msgsent).trimmed()},
                                            {"ok",encoded.ok && decoded.ok}}).toJson();
        fwrite(data.constData(),1,data.size(),stdout);
        return encoded.ok && decoded.ok && decoded.msgsent.trimmed()=="K1ABC FN42 33" ? 0 : 4;
    }

    if (args.size() != 3 && !(args.size() == 4 && (args[3] == "--cancel" || args[3] == "--fst4w" || args[3] == "--fst4w-reference"))) return 2;
    bool ok = false;
    int period = args[2].toInt(&ok);
    if (!ok || !QList<int>{15,30,60,120,300,900,1800}.contains(period)) return 2;
    QFile file(args[1]);
    if (!file.open(QIODevice::ReadOnly)) return 2;
    QByteArray pcm = file.readAll();
    if (pcm.isEmpty() || pcm.size() % 2 || pcm.size() > period * 12000LL * 2) return 2;
    QTemporaryDir data;
    if (!data.isValid()) return 2;
    decodium::fst4::DecodeRequest request;
    request.mode = QStringLiteral("FST4");
    bool fst4w = args.size() == 4 && args[3].startsWith("--fst4w");
    bool reference = args.size() == 4 && args[3] == "--fst4w-reference";
    if (reference && period != 1800) return 2;
    request.iwspr = fst4w ? 1 : 0;
    request.mode = fst4w ? QStringLiteral("FST4W") : QStringLiteral("FST4");
    request.ntrperiod = period;
    request.ndepth = 3;
    request.nfa = 1000;
    request.nfb = 1400;
    request.nfqso = fst4w ? 1500 : 1331;
    request.ntol = reference ? 100 : 20;
    request.nexp_decode = reference ? 768 : 0; // NB=0 in the documented WSJT-X example.
    request.mycall = "K1ABC";
    request.hiscall = "W9XYZ";
    request.dataDir = data.path().toLocal8Bit();
    request.audio.resize(pcm.size()/2);
    for (qsizetype i=0; i<request.audio.size(); ++i)
        request.audio[i] = qFromLittleEndian<qint16>(reinterpret_cast<const uchar*>(pcm.constData()+2*i));
    QJsonObject cancellation;
    if (args.size() == 4 && args[3] == "--cancel") {
        decodium::fst4::FST4DecodeWorker worker;
        QStringList emitted;
        QObject::connect(&worker, &decodium::fst4::FST4DecodeWorker::decodeReady,
                         &worker, [&](quint64, QStringList rows) { emitted = rows; }, Qt::DirectConnection);
        request.serial = 1;
        std::thread active([&] { worker.decode(request); });
        auto& mutex = decodium::fortran::runtime_mutex();
        QElapsedTimer wait;
        wait.start();
        bool busy = false;
        while (wait.elapsed() < 2000) {
            if (!mutex.tryLock()) { busy = true; break; }
            mutex.unlock();
            std::this_thread::yield();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        QElapsedTimer stop;
        stop.start();
        worker.retireThrough(1);
        active.join();
        double residualMs = stop.nsecsElapsed()/1e6;
        bool staleEmpty = emitted.isEmpty();
        worker.retireThrough(0); // Must not move the retirement boundary backwards.
        worker.decode(request);
        bool queuedEmpty = emitted.isEmpty();
        request.serial = 2;
        worker.decode(request);
        bool recovered = false;
        for (auto const& row : emitted) recovered |= row.contains("CQ K9KFR EN71");
        cancellation = {{"observedRuntimeBusy",busy},{"residualMs",residualMs},
                        {"staleEmpty",staleEmpty},{"queuedEmpty",queuedEmpty},{"recovered",recovered}};
        if (!busy || !staleEmpty || !queuedEmpty || !recovered || residualMs > 500.0) return 3;
    }
    QJsonArray runs;
    for (int i=0; i<3; ++i) {
        request.serial = i+1;
        QElapsedTimer timer;
        timer.start();
        auto rows = decodium::fst4::decodeFst4Rows(request);
        double ms = timer.nsecsElapsed()/1e6;
        runs.append(QJsonObject{{"run",i+1},{"elapsedMs",ms},{"rows",QJsonArray::fromStringList(rows)}});
    }
    auto result = QJsonDocument(QJsonObject{{"period",period},{"fst4w",fst4w},{"runs",runs},{"cancellation",cancellation}}).toJson();
    fwrite(result.constData(), 1, result.size(), stdout);
    return 0;
}
