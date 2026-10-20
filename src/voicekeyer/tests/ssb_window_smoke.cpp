// Harness della finestra SSB: carica SsbWindow.qml con oggetti reali per il voice
// keyer, il riconoscimento, l'assistente e i pacchetti, e con oggetti finti per la
// radio. Nessun hook TX e' collegato: niente puo' trasmettere. Salva uno
// screenshot e conta gli avvisi QML.
#include "AssistantChat.h"
#include "PackManager.h"
#include "SpeechToText.h"
#include "VoiceKeyer.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>

namespace {
int gWarnings = 0;
void handler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        ++gWarnings;
        std::fprintf(stderr, "QML WARN: %s\n", qPrintable(msg));
    }
}
}  // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    // argv[1] = PNG di uscita, argv[2] = cartella qml/decodium del progetto
    if (argc < 3) return 2;
    const QString qmlRoot = QString::fromLocal8Bit(argv[2]);
    qInstallMessageHandler(handler);

    QTemporaryDir dir;   // nessun pacchetto installato: la prova non dipende dalla macchina
    decodium::voicekeyer::PackManager packs(dir.filePath(QStringLiteral("pack")));
    decodium::voicekeyer::VoiceKeyer keyer(dir.filePath(QStringLiteral("keyer")), &packs);
    decodium::voicekeyer::SpeechToText speech(&packs);
    decodium::voicekeyer::AssistantChat assistant(&packs);

    // Radio finta: solo i campi che la finestra legge. Nessuna funzione di trasmissione.
    QVariantMap bridge {
        {QStringLiteral("mode"), QStringLiteral("SSB")},
        {QStringLiteral("catConnected"), false},
        {QStringLiteral("catMode"), QStringLiteral("USB")},
        {QStringLiteral("decoPortUseRemote"), false},
        {QStringLiteral("ssbControls"), QVariantMap{{QStringLiteral("busy"), false}}},
        {QStringLiteral("frequency"), 14074000},
        {QStringLiteral("ssbRadioMode"), QStringLiteral("USB")},
        {QStringLiteral("ssbRxAgcMode"), 0},
        {QStringLiteral("ssbRxAgcThreshold"), -80},
        {QStringLiteral("ssbRxAgcGainDb"), 0.0},
        {QStringLiteral("remoteRxVolume"), 70},
        {QStringLiteral("remoteRxDevice"), QString()},
        {QStringLiteral("decoPortMonitor"), false},
        {QStringLiteral("rigAlcValid"), false},
        {QStringLiteral("rigPowerWatts"), 0.0},
        {QStringLiteral("rigSwr"), 0.0},
        {QStringLiteral("rigAlc"), 0.0},
        {QStringLiteral("hamlibCat"), QVariant()},
    };
    QVariantMap voice {
        {QStringLiteral("microphones"), QVariantList()},
        {QStringLiteral("microphone"), QString()},
        {QStringLiteral("gainDb"), 0.0},
        {QStringLiteral("automatic"), true},
        {QStringLiteral("lowCut"), 200},
        {QStringLiteral("highCut"), 2800},
        {QStringLiteral("micPeak"), 0.0},
        {QStringLiteral("transmitting"), false},
        {QStringLiteral("status"), QString()},
    };

    QQmlApplicationEngine engine;
    engine.addImportPath(qmlRoot);
    engine.rootContext()->setContextProperty(QStringLiteral("appEngine"), bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("ssbModule"), voice);
    engine.rootContext()->setContextProperty(QStringLiteral("voiceKeyer"), &keyer);
    engine.rootContext()->setContextProperty(QStringLiteral("speech"), &speech);
    engine.rootContext()->setContextProperty(QStringLiteral("assistant"), &assistant);
    engine.rootContext()->setContextProperty(QStringLiteral("packs"), &packs);
    engine.load(QUrl::fromLocalFile(qmlRoot + QStringLiteral("/components/SsbWindow.qml")));
    if (engine.rootObjects().isEmpty()) {
        std::fprintf(stderr, "SsbWindow.qml non caricata\n");
        return 3;
    }
    auto* win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!win) return 4;
    win->show();

    QTimer::singleShot(3000, [&] {
        // Tutto il resto resta com'e': nessun clic su pulsanti che trasmettono.
        win->grabWindow().save(argv[1]);
        app.quit();
    });
    const int rc = app.exec();
    std::fprintf(stderr, "avvisi QML: %d\n", gWarnings);
    return rc;
}
