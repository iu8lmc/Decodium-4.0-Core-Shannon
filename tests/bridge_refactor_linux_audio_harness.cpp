#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QStringList>
#include <QTextStream>
#include "linux_audio_helpers.inc"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QString error;
    const QString source = linuxPulseSourceNameFromShortLine(QStringLiteral("7\talsa_input.test\tmodule"));
    const QString monitor = linuxPulseSourceDisplayName(QStringLiteral("alsa_output.test.monitor"));
    const QString sink = linuxMatchingPulseSinkForSource(source);
    bool sourceSet = linuxSetDefaultPulseSource(source, &error);
    bool sinkSet = linuxSetDefaultPulseSink(sink, &error);
    bool emptyRejected = !linuxSetDefaultPulseSource(QString(), &error) && !error.isEmpty();
    const bool malformed = linuxPulseSourceNameFromShortLine(QStringLiteral("bad-line")).isEmpty()
        && linuxPulseSinkNameFromShortLine(QStringLiteral("bad-line")).isEmpty();
    const bool deniedSource = !linuxSetDefaultPulseSource(QStringLiteral("denied"), &error)
        && error == QStringLiteral("synthetic failure");
    const bool deniedSink = !linuxSetDefaultPulseSink(QStringLiteral("denied"), &error)
        && error == QStringLiteral("synthetic failure");
    const bool timeout = !linuxSetDefaultPulseSource(QStringLiteral("slow"), &error)
        && error.contains(QStringLiteral("timed out"));
    const bool emptySink = !linuxSetDefaultPulseSink(QString(), &error) && !error.isEmpty();
    qputenv("LAB_PACTL_EMPTY", "1");
    const bool emptyList = linuxPulseSourcesForLog() == QStringList{QStringLiteral("pactl returned no PulseAudio/PipeWire sources")}
        && linuxPulseSinksForLog() == QStringList{QStringLiteral("pactl returned no PulseAudio/PipeWire sinks")};
    qunsetenv("LAB_PACTL_EMPTY");
    const bool pass = source == QStringLiteral("alsa_input.test")
        && monitor == QStringLiteral("Pulse/PipeWire monitor: alsa_output.test.monitor")
        && linuxPulseSourceNameFromDisplay(monitor) == QStringLiteral("alsa_output.test.monitor")
        && linuxPulseSinkNameFromDisplay(QStringLiteral("Pulse/PipeWire: alsa_output.test")) == QStringLiteral("alsa_output.test")
        && sink == QStringLiteral("alsa_output.test") && sourceSet && sinkSet && emptyRejected
        && linuxPulseSourcesForLog().size() == 2 && audioDeviceLooksUsb(QStringLiteral("USB CODEC"))
        && malformed && deniedSource && deniedSink && timeout && emptySink && emptyList;
    QTextStream(stdout) << "source=" << source << " monitor=" << monitor << " sink=" << sink
                        << " sourceSet=" << sourceSet << " sinkSet=" << sinkSet
                        << " emptyRejected=" << emptyRejected << " malformed=" << malformed
                        << " deniedSource=" << deniedSource << " deniedSink=" << deniedSink
                        << " timeout=" << timeout << " emptySink=" << emptySink << " emptyList=" << emptyList << " pass=" << pass << '\n';
    return pass ? 0 : 1;
}
