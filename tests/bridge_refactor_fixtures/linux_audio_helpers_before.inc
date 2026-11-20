#ifdef Q_OS_LINUX
static constexpr char kLinuxPulseSourcePrefix[] = "Pulse/PipeWire: ";
static constexpr char kLinuxPulseMonitorSourcePrefix[] = "Pulse/PipeWire monitor: ";
static constexpr char kLinuxPulseSinkPrefix[] = "Pulse/PipeWire: ";

static QStringList linuxAlsaCardsForLog()
{
    QFile file(QStringLiteral("/proc/asound/cards"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QStringList lines;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }
    return lines;
}

static QString linuxPulseSourceNameFromShortLine(const QString& line)
{
    const QStringList fields = line.split(QLatin1Char('\t'));
    if (fields.size() < 2) {
        return {};
    }

    // PipeWire exposes the audio playing on a sink as a PulseAudio source
    // whose name ends in ".monitor". It is a valid capture source and is
    // precisely what browser WebSDR/KiwiSDR users need, so do not discard it.
    return fields.at(1).trimmed();
}

static bool linuxPulseSourceIsMonitor(const QString& sourceName)
{
    return sourceName.trimmed().endsWith(QStringLiteral(".monitor"), Qt::CaseInsensitive);
}

static QString linuxPulseSourceDisplayName(const QString& sourceName)
{
    const QString source = sourceName.trimmed();
    if (source.isEmpty()) {
        return {};
    }
    const char* const prefix = linuxPulseSourceIsMonitor(source)
        ? kLinuxPulseMonitorSourcePrefix
        : kLinuxPulseSourcePrefix;
    return QString::fromLatin1(prefix) + source;
}

static QString linuxPulseSinkNameFromShortLine(const QString& line)
{
    const QStringList fields = line.split(QLatin1Char('\t'));
    if (fields.size() < 2) {
        return {};
    }

    const QString name = fields.at(1).trimmed();
    return name.isEmpty() ? QString() : name;
}

static QStringList linuxPulseSourcesForLog()
{
    QStringList lines;
    QProcess process;
    process.setProgram(QStringLiteral("pactl"));
    process.setArguments({QStringLiteral("list"), QStringLiteral("short"), QStringLiteral("sources")});
    process.start();
    if (!process.waitForFinished(1500)) {
        process.kill();
        process.waitForFinished(500);
        return {QStringLiteral("pactl list short sources timed out or is unavailable")};
    }
    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    const QString error = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    for (const QString& rawLine : output.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }
    if (lines.isEmpty()) {
        lines.append(error.isEmpty()
                         ? QStringLiteral("pactl returned no PulseAudio/PipeWire sources")
                         : QStringLiteral("pactl error: %1").arg(error));
    }
    return lines;
}

static QStringList linuxPulseSinksForLog()
{
    QStringList lines;
    QProcess process;
    process.setProgram(QStringLiteral("pactl"));
    process.setArguments({QStringLiteral("list"), QStringLiteral("short"), QStringLiteral("sinks")});
    process.start();
    if (!process.waitForFinished(1500)) {
        process.kill();
        process.waitForFinished(500);
        return {QStringLiteral("pactl list short sinks timed out or is unavailable")};
    }
    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    const QString error = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    for (const QString& rawLine : output.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }
    if (lines.isEmpty()) {
        lines.append(error.isEmpty()
                         ? QStringLiteral("pactl returned no PulseAudio/PipeWire sinks")
                         : QStringLiteral("pactl error: %1").arg(error));
    }
    return lines;
}

static QString linuxPulseSourceNameFromDisplay(QString value)
{
    value = value.trimmed();
    const QString prefix = QString::fromLatin1(kLinuxPulseSourcePrefix);
    const QString monitorPrefix = QString::fromLatin1(kLinuxPulseMonitorSourcePrefix);
    if (value.startsWith(monitorPrefix, Qt::CaseInsensitive)) {
        return value.mid(monitorPrefix.size()).trimmed();
    }
    if (value.startsWith(prefix, Qt::CaseInsensitive)) {
        return value.mid(prefix.size()).trimmed();
    }
    return {};
}

static QString linuxPulseSinkNameFromDisplay(QString value)
{
    value = value.trimmed();
    const QString prefix = QString::fromLatin1(kLinuxPulseSinkPrefix);
    return value.startsWith(prefix, Qt::CaseInsensitive)
        ? value.mid(prefix.size()).trimmed()
        : QString();
}

static bool linuxSetDefaultPulseSource(const QString& sourceName, QString* errorOut = nullptr)
{
    const QString source = sourceName.trimmed();
    if (source.isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("empty Pulse/PipeWire source");
        return false;
    }

    QProcess process;
    process.setProgram(QStringLiteral("pactl"));
    process.setArguments({QStringLiteral("set-default-source"), source});
    process.start();
    if (!process.waitForFinished(1500)) {
        process.kill();
        process.waitForFinished(500);
        if (errorOut) *errorOut = QStringLiteral("pactl set-default-source timed out");
        return false;
    }

    const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorOut) {
            *errorOut = stderrText.isEmpty()
                ? QStringLiteral("pactl set-default-source failed with exit code %1").arg(process.exitCode())
                : stderrText;
        }
        return false;
    }

    return true;
}

QString decodiumBridgeLinuxPulseSourceNameFromDisplay(QString value)
{
    return linuxPulseSourceNameFromDisplay(value);
}

bool decodiumBridgeLinuxSetDefaultPulseSource(const QString& name, QString* error)
{
    return linuxSetDefaultPulseSource(name, error);
}

static bool linuxSetDefaultPulseSink(const QString& sinkName, QString* errorOut = nullptr)
{
    const QString sink = sinkName.trimmed();
    if (sink.isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("empty Pulse/PipeWire sink");
        return false;
    }

    QProcess process;
    process.setProgram(QStringLiteral("pactl"));
    process.setArguments({QStringLiteral("set-default-sink"), sink});
    process.start();
    if (!process.waitForFinished(1500)) {
        process.kill();
        process.waitForFinished(500);
        if (errorOut) *errorOut = QStringLiteral("pactl set-default-sink timed out");
        return false;
    }

    const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorOut) {
            *errorOut = stderrText.isEmpty()
                ? QStringLiteral("pactl set-default-sink failed with exit code %1").arg(process.exitCode())
                : stderrText;
        }
        return false;
    }

    return true;
}

QString decodiumBridgeLinuxPulseSinkNameFromDisplay(QString value) { return linuxPulseSinkNameFromDisplay(value); }
bool decodiumBridgeLinuxSetDefaultPulseSink(const QString& name, QString* error) { return linuxSetDefaultPulseSink(name, error); }

static QString linuxMatchingPulseSinkForSource(const QString& sourceName)
{
    QString sinkName = sourceName.trimmed();
    if (sinkName.isEmpty()) {
        return {};
    }

    if (sinkName.startsWith(QStringLiteral("alsa_input."))) {
        sinkName.replace(0, QStringLiteral("alsa_input.").size(), QStringLiteral("alsa_output."));
    }

    const QStringList sinks = linuxPulseSinksForLog();
    for (const QString& line : sinks) {
        const QString candidate = linuxPulseSinkNameFromShortLine(line);
        if (candidate.compare(sinkName, Qt::CaseInsensitive) == 0) {
            return candidate;
        }
    }
    return {};
}

bool decodiumBridgeLinuxPulseSourceIsMonitor(const QString& name) { return linuxPulseSourceIsMonitor(name); }
QString decodiumBridgeLinuxMatchingPulseSinkForSource(const QString& name) { return linuxMatchingPulseSinkForSource(name); }

static bool audioDeviceLooksUsb(const QString& description)
{
    return description.contains(QStringLiteral("USB"), Qt::CaseInsensitive)
        || description.contains(QStringLiteral("CODEC"), Qt::CaseInsensitive)
        || description.contains(QStringLiteral("IC-"), Qt::CaseInsensitive)
        || description.contains(QStringLiteral("radio"), Qt::CaseInsensitive);
}
#endif

#ifdef Q_OS_LINUX
QStringList decodiumCachePulseSources() { return linuxPulseSourcesForLog(); }
QStringList decodiumCachePulseSinks() { return linuxPulseSinksForLog(); }
QStringList decodiumCacheAlsaCards() { return linuxAlsaCardsForLog(); }
bool decodiumCacheLooksUsb(const QString& s) { return audioDeviceLooksUsb(s); }
QString decodiumCacheSourceName(const QString& s) { return linuxPulseSourceNameFromShortLine(s); }
QString decodiumCacheSourceDisplay(const QString& s) { return linuxPulseSourceDisplayName(s); }
QString decodiumCacheSinkName(const QString& s) { return linuxPulseSinkNameFromShortLine(s); }
const char* decodiumCacheSinkPrefix() { return kLinuxPulseSinkPrefix; }
#endif

